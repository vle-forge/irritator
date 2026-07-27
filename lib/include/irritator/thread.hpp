// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace irt {

/* * * * *
 *
 * thread-safe log / journal
 *
 * * * * */

struct log_record {
    constexpr static inline auto title_length = 63;
    constexpr static inline auto msg_length   = 254;

    u64                        ts;
    std::thread::id            tid;
    small_string<title_length> t;
    small_string<msg_length>   msg;
    log_level                  level;
};

using thread_journal = static_circular_buffer<log_record, 4096>;

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

inline thread_local thread_journal* current_journal = nullptr;

struct journal_scope {
    journal_scope()
    {
        current_journal = &journal_registry::instance().attach();
    }
};

inline u64 get_time_since_epoch() noexcept
{
    const auto timepoint = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
             timepoint.time_since_epoch())
      .count();
}

inline void log(log_level        lvl,
                std::string_view t,
                std::string_view msg = std::string_view{}) noexcept
{
    if (current_journal) [[likely]]
        current_journal->push(log_record{
          get_time_since_epoch(), std::this_thread::get_id(), t, msg, lvl });
}

template<typename Fn, typename... Args>
inline void log(log_level level, Fn&& fn, Args&&... args) noexcept
{
    if (current_journal) [[likely]] {
        current_journal->push([&](log_record& l) noexcept {
            l.ts    = get_time_since_epoch();
            l.level = level;
            l.tid   = std::this_thread::get_id();

            std::invoke(
              std::forward<Fn>(fn), l.t, l.msg, std::forward<Args>(args)...);
        });
    }
}

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
    void collect() noexcept
    {
        m_history.write([](vector<log_record>& dst) {
            journal_registry::instance().for_each([&](thread_journal& j) {
                log_record rec;
                while (j.pop(rec))
                    dst.push_back(std::move(rec));
            });
        });
    }

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
    ordered_task_list() noexcept = default;

    ~ordered_task_list() noexcept = default;

    ordered_task_list(ordered_task_list&& other) noexcept
      : m_queue(std::move(other.m_queue))
      , m_tasks_submitted(other.m_tasks_submitted)
      , m_tasks_completed(other.m_tasks_completed)
      , m_tasks_running(other.m_tasks_running)
      , m_stopping(other.m_stopping)
    {}

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

    bool pop(task& out) noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        worker_cv.wait(lock, [&] { return m_stopping || !m_queue.empty(); });
        if (m_stopping && m_queue.empty())
            return false;
        out = std::move(*m_queue.head());
        m_queue.pop_head();
        m_tasks_running += 1;
        return true;
    }

    void notify_done()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        --m_tasks_running;
        ++m_tasks_completed;
        if (m_queue.empty() && m_tasks_running == 0 &&
            m_tasks_completed == m_tasks_submitted) {
            producer_cv.notify_all();
        }
    }

    void wait_empty() noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        producer_cv.wait(lock, [&] {
            return m_stopping || (m_queue.empty() && m_tasks_running == 0 &&
                                  m_tasks_completed == m_tasks_submitted);
        });
    }

    void shutdown() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        worker_cv.notify_all();
    }

    bool stopping() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopping;
    }

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
    explicit ordered_worker(ordered_task_list& list) noexcept
      : m_list(&list)
    {}

    ordered_worker(ordered_worker&& other) noexcept
      : m_list(other.m_list)
      , m_thread(std::move(other.m_thread))
      , m_tasks_completed(other.m_tasks_completed)
      , m_execution_time(other.m_execution_time)
    {}

    ~ordered_worker() noexcept = default;

    void start() noexcept
    {
        m_thread = std::thread([this] {
            journal_scope scope;
            m_initialized = true;

            task t;
            while (m_list->pop(t)) {
                try {
                    m_tasks_completed += 1;
                    const auto start = std::chrono::steady_clock::now();
                    t();
                    m_execution_time =
                      (std::chrono::steady_clock::now() - start).count();
                } catch (...) {
                }
                m_list->notify_done();
            }
        });
    }

    void join() noexcept
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    u64 tasks_completed() const noexcept { return m_tasks_completed; }
    int initialized() const noexcept { return m_initialized; }

private:
    ordered_task_list* m_list;
    std::thread        m_thread;

    u64                           m_tasks_completed{ 0 };
    std::chrono::nanoseconds::rep m_execution_time;

    bool m_initialized = false;
};

class unordered_task_list
{
public:
    unordered_task_list() noexcept { m_pending.reserve(1024); }

    unordered_task_list(unordered_task_list&& other) noexcept
      : m_pending(std::move(other.m_pending))
      , m_tasks_submitted(other.m_tasks_submitted)
      , m_tasks_completed(other.m_tasks_completed)
      , m_batch_size(other.m_batch_size)
      , m_next_index(other.m_next_index)
      , m_completed(other.m_completed)
      , m_phase(other.m_phase)
      , m_stopping(other.m_stopping)
    {}

    template<typename Fn>
    void add(Fn&& fn) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_phase != phase::accepting)
            return;
        m_pending.emplace_back(std::forward<Fn>(fn));
        m_tasks_submitted += 1;
    }

    void submit() noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_phase != phase::accepting)
            return;
        m_batch_size = static_cast<u32>(m_pending.size());
        m_next_index = 0;
        m_completed  = 0;
        m_phase = (m_batch_size == 0) ? phase::accepting : phase::executing;
    }

    bool try_steal(task& out) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_phase != phase::executing)
            return false;
        if (m_next_index >= m_batch_size)
            return false;
        out = std::move(m_pending[m_next_index++]);
        m_tasks_completed += 1;
        return true;
    }

    void notify_done() noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_phase != phase::executing)
            return;
        ++m_completed;
        if (m_completed >= m_batch_size) {
            m_phase = phase::accepting;
            m_pending.clear();
            m_producer_cv.notify_all();
        }
    }

    void wait_completion() noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_producer_cv.wait(lock, [&] {
            return m_stopping || m_phase == phase::accepting ||
                   (m_phase == phase::executing && m_completed >= m_batch_size);
        });
    }

    void shutdown() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
            m_phase    = phase::shutting_down;
        }
        m_producer_cv.notify_all();
    }

    bool stopping() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopping;
    }

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
    explicit unordered_worker(std::span<unordered_task_list> lists) noexcept
      : m_lists(lists)
    {}

    unordered_worker(unordered_worker&& other) noexcept
      : m_lists(other.m_lists)
      , m_thread(std::move(other.m_thread))
      , m_tasks_completed(other.m_tasks_completed)
      , m_execution_time(other.m_execution_time)
    {}

    void start() noexcept
    {
        m_thread = std::thread([this] {
            journal_scope scope;
            m_initialized = true;

            while (true) {
                bool found = false;
                for (auto& l : m_lists) {
                    task t;
                    while (l.try_steal(t)) {
                        found = true;
                        try {
                            const auto start = std::chrono::steady_clock::now();
                            t();
                            m_execution_time =
                              (std::chrono::steady_clock::now() - start)
                                .count();
                            m_tasks_completed += 1;
                        } catch (...) {
                        }
                        l.notify_done();
                    }
                }
                if (!found) {
                    if (std::all_of(m_lists.begin(),
                                    m_lists.end(),
                                    [](const auto& l) { return l.stopping(); }))
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    // std::this_thread::yield();
                }
            }
        });
    }

    void join() noexcept
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    u64 tasks_completed() const noexcept { return m_tasks_completed; }
    int initialized() const noexcept { return m_initialized; }

private:
    std::span<unordered_task_list> m_lists;
    std::thread                    m_thread;

    u64                           m_tasks_completed{ 0 };
    std::chrono::nanoseconds::rep m_execution_time;

    bool m_initialized = false;
};

class task_manager
{
public:
    task_manager(
      size_t ordered_count,
      size_t unordered_count,
      size_t unordered_worker_count = std::thread::hardware_concurrency())
      : m_ordered_lists(ordered_count)
      , m_ordered_workers(ordered_count, reserve_tag)
      , m_unordered_lists(unordered_count)
      , m_unordered_workers(
          unordered_worker_count == 0 ? 1 : unordered_worker_count,
          reserve_tag)
    {
        for (auto& l : m_ordered_lists)
            m_ordered_workers.emplace_back(l);

        const auto span = std::span<unordered_task_list>(
          m_unordered_lists.data(), m_unordered_lists.size());

        for (sz i = 0, e = m_unordered_workers.capacity(); i < e; ++i)
            m_unordered_workers.emplace_back(span);
    }

    void start()
    {
        for (auto& w : m_ordered_workers)
            w.start();

        for (auto& w : m_unordered_workers)
            w.start();

        const auto nb = m_ordered_workers.size() + m_unordered_workers.size();
        auto       initialized = 0u;

        do {
            initialized = 0u;

            for (auto& w : m_ordered_workers)
                initialized += w.initialized();

            for (auto& w : m_unordered_workers)
                initialized += w.initialized();

            std::this_thread::yield();
        } while (nb != initialized);
    }

    void shutdown()
    {
        for (auto& l : m_ordered_lists)
            l.shutdown();
        for (auto& l : m_unordered_lists)
            l.shutdown();
        for (auto& w : m_ordered_workers)
            w.join();
        for (auto& w : m_unordered_workers)
            w.join();
    }

    ordered_task_list& ordered(std::integral auto i)
    {
        return m_ordered_lists[i];
    }
    unordered_task_list& unordered(std::integral auto i)
    {
        return m_unordered_lists[i];
    }

    size_t ordered_size() const noexcept { return m_ordered_lists.size(); }
    size_t unordered_size() const noexcept { return m_unordered_lists.size(); }
    size_t wordered_size() const noexcept { return m_ordered_workers.size(); }
    size_t wunordered_size() const noexcept
    {
        return m_unordered_workers.size();
    }

    u64 wordered_tasks_completed(std::integral auto i) const noexcept
    {
        return m_ordered_workers[i].tasks_completed();
    }

    u64 wunordered_tasks_completed(std::integral auto i) const noexcept
    {
        return m_unordered_workers[i].tasks_completed();
    }

    std::chrono::nanoseconds::rep wordered_execution_time(
      std::integral auto i) const noexcept
    {
        return m_ordered_workers[i].tasks_completed();
    }

    std::chrono::nanoseconds::rep wunordered_execution_time(
      std::integral auto i) const noexcept
    {
        return m_unordered_workers[i].tasks_completed();
    }

private:
    vector<ordered_task_list> m_ordered_lists;
    vector<ordered_worker>    m_ordered_workers;

    vector<unordered_task_list> m_unordered_lists;
    vector<unordered_worker>    m_unordered_workers;
};

} // namespace irt

#endif
