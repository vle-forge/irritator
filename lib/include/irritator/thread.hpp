// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace irt {

/* * * * *
 *
 * thread-safe log / journal
 *
 * * * * */

class journal_registry
{
private:
    journal_registry() noexcept
      : m_journals(8, reserve_tag)
    {}

public:
    static journal_registry& instance() noexcept
    {
        static journal_registry r;
        return r;
    }

    thread_journal& attach() noexcept
    {
        std::scoped_lock lock(m_mutex);

        m_journals.push_back(std::make_unique<thread_journal>());
        return *m_journals.back();
    }

    template<typename F>
    void for_each(F&& f) noexcept
    {
        std::scoped_lock lock(m_mutex);

        for (auto& j : m_journals)
            f(*j);
    }

private:
    spin_mutex                              m_mutex;
    vector<std::unique_ptr<thread_journal>> m_journals;
};

struct journal_scope {
    journal_scope()
    {
        current_journal = &journal_registry::instance().attach();
    }
};

/** Aggregation and diffusion journal log
 *
 *  A single append-only shared buffer. A collector writes (periodic drain of
 * thread buffers), any number of readers read without ever blocking each other
 * or the collector.
 */
class log_history
{
public:
    using full_log_history_type =
      shared_buffer<vector<log_record>,
                    append_only_merge_policy<vector<log_record>>>;

    /** Copy log from @c thread_journal into @c global_log_history.
     *
     * To be called periodically: once per frame on the ImGui side, or by a
     * dedicated thread on the CLI/cluster side.
     */
    void collect() noexcept;

    /** Reset the @c m_history @c shared_buffer. */
    void reset_history() { m_history.reset(); }

    /** Copy the history log span from @c m_history into output.
     *
     * Lock-free reading on the consumer side, with version tracking to only
     * process new lines since the last call (useful to avoid re-scanning the
     * entire log every ImGui frame).
     *
     * @param last_version @c shared_bbuffer version.
     * @param last_size The cursor into the buffer.
     */
    template<typename Fn>
    void read_log(u64& last_version, u64& last_size, Fn&& fn)
    {
        m_history.read([&](const vector<log_record>& buf,
                           std::uint64_t             ver) noexcept {
            if (ver == last_version)
                return;

            if (last_size > buf.size())
                last_size = 0;

            fn(std::span<const log_record>(buf.begin() + last_size, buf.end()));

            last_version = ver;
            last_size    = buf.size();
        });
    }

private:
    full_log_history_type m_history;
};

/* * * * *
 *
 * task system
 *
 * * * * */

using task = lambda_function<void(void), 64>;

class ordered_task_list;
class unordered_task_list;
class ordered_worker;
class unordered_worker;
class task_manager;

class ordered_task_list
{
public:
    ordered_task_list() noexcept  = default;
    ~ordered_task_list() noexcept = default;
    ordered_task_list(ordered_task_list&& other) noexcept;

    template<typename Fn>
    void add(Fn&& fn) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
                return;
            m_tasks_submitted += 1;
            m_queue.emplace_enqueue(std::forward<Fn>(fn));
        }
        worker_cv.notify_one();
    }

    bool pop(task& out) noexcept;
    void notify_done() noexcept;
    void wait_empty() noexcept;
    void shutdown() noexcept;
    bool stopping() const noexcept;

    u64 tasks_submitted() const noexcept { return m_tasks_submitted; }
    u64 tasks_completed() const noexcept { return m_tasks_completed; }

private:
    ring_buffer<task> m_queue{ 256 };

    mutable std::mutex      m_mutex;
    std::condition_variable worker_cv;   // for workers (work available)
    std::condition_variable producer_cv; // for producers (fully drained)

    u64 m_tasks_submitted{ 0 };
    u64 m_tasks_completed{ 0 };
    u64 m_tasks_running{ 0 };

    bool m_stopping{ false };
};

class ordered_worker
{
public:
    explicit ordered_worker(ordered_task_list& list) noexcept;
    ordered_worker(ordered_worker&& other) noexcept;
    ~ordered_worker() noexcept = default;

    void start() noexcept;
    void join() noexcept;

    u64 tasks_completed() const noexcept { return m_tasks_completed; }
    u64 execution_time_in_ms() const noexcept { return m_execution_time; }
    int initialized() const noexcept { return m_initialized; }

private:
    ordered_task_list* m_list;
    std::thread        m_thread;

    u64 m_tasks_completed{ 0 };
    u64 m_execution_time{}; //!< stores in milliseconds

    bool m_initialized = false;
};

class unordered_task_list
{
public:
    unordered_task_list() noexcept;
    unordered_task_list(unordered_task_list&& other) noexcept;

    template<typename Fn>
    void add(Fn&& fn) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_phase != phase::accepting)
            return;
        m_pending.emplace_back(std::forward<Fn>(fn));
        m_tasks_submitted += 1;
    }

    void submit() noexcept;
    bool try_steal(task& out) noexcept;
    void notify_done() noexcept;
    void wait_completion() noexcept;
    void shutdown() noexcept;
    bool stopping() const;

    u64 tasks_submitted() const noexcept { return m_tasks_submitted; }
    u64 tasks_completed() const noexcept { return m_tasks_completed; }

private:
    enum class phase : uint8_t { accepting, executing, shutting_down };

    vector<task> m_pending;

    u64 m_tasks_submitted{ 0 };
    u64 m_tasks_completed{ 0 };
    u32 m_batch_size{ 0 };
    u32 m_next_index{ 0 };
    u32 m_completed{ 0 };

    mutable std::mutex      m_mutex;
    std::condition_variable m_producer_cv;

    phase m_phase{ phase::accepting };
    bool  m_stopping{ false };
};

class unordered_worker
{
public:
    explicit unordered_worker(std::span<unordered_task_list> lists) noexcept;
    unordered_worker(unordered_worker&& other) noexcept;

    void start() noexcept;
    void join() noexcept;

    u64 tasks_completed() const noexcept { return m_tasks_completed; }
    u64 execution_time_in_ms() const noexcept { return m_execution_time; }
    int initialized() const noexcept { return m_initialized; }

private:
    std::span<unordered_task_list> m_lists;
    std::thread                    m_thread;

    u64 m_tasks_completed{ 0 };
    u64 m_execution_time; //!< stores in milliseconds

    bool m_initialized = false;
};

class task_manager
{
public:
    task_manager(size_t ordered_count,
                 size_t unordered_count,
                 size_t unordered_worker_count =
                   std::thread::hardware_concurrency()) noexcept;

    void start() noexcept;
    void shutdown() noexcept;

    ordered_task_list&   ordered(std::integral auto i) noexcept;
    unordered_task_list& unordered(std::integral auto i) noexcept;

    size_t ordered_size() const noexcept;
    size_t unordered_size() const noexcept;
    size_t wordered_size() const noexcept;
    size_t wunordered_size() const noexcept;

    u64 wordered_tasks_completed(std::integral auto i) const noexcept;
    u64 wunordered_tasks_completed(std::integral auto i) const noexcept;

    u64 wordered_execution_time(std::integral auto i) const noexcept;
    u64 wunordered_execution_time(std::integral auto i) const noexcept;

private:
    vector<ordered_task_list> m_ordered_lists;
    vector<ordered_worker>    m_ordered_workers;

    vector<unordered_task_list> m_unordered_lists;
    vector<unordered_worker>    m_unordered_workers;
};

inline ordered_task_list& task_manager::ordered(std::integral auto i) noexcept
{
    return m_ordered_lists[i];
}

inline unordered_task_list& task_manager::unordered(
  std::integral auto i) noexcept
{
    return m_unordered_lists[i];
}

inline size_t task_manager::ordered_size() const noexcept
{
    return m_ordered_lists.size();
}

inline size_t task_manager::unordered_size() const noexcept
{
    return m_unordered_lists.size();
}

inline size_t task_manager::wordered_size() const noexcept
{
    return m_ordered_workers.size();
}

inline size_t task_manager::wunordered_size() const noexcept
{
    return m_unordered_workers.size();
}

inline u64 task_manager::wordered_tasks_completed(
  std::integral auto i) const noexcept
{
    return m_ordered_workers[i].tasks_completed();
}

inline u64 task_manager::wunordered_tasks_completed(
  std::integral auto i) const noexcept
{
    return m_unordered_workers[i].tasks_completed();
}

inline u64 task_manager::wordered_execution_time(
  std::integral auto i) const noexcept
{
    return m_ordered_workers[i].tasks_completed();
}

inline u64 task_manager::wunordered_execution_time(
  std::integral auto i) const noexcept
{
    return m_unordered_workers[i].tasks_completed();
}

} // namespace irt

#endif
