// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>
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
    using full_log_history_type = shared_buffer<
      vector<log_record>,
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
 * share data
 *
 * * * * */

/// @class request_buffer
///
/// @brief A lock-free asynchronous mailbox with a built-in anti-spam
/// mechanism.
///
/// @details Designed for a "Request-Fulfill" pattern between a single
/// high-frequency GUI thread (calling try_request() and try_take()) and
/// asynchronous worker threads (calling fulfill() or fail()). Only one task is
/// active at any given time, which prevents "Request Storms".
///
/// Ownership of the value follows the state:
/// - idle, ready: owned by the GUI thread;
/// - pending: owned by the worker thread.
///
/// The buffer must outlive the task: do not destroy it while a request is
/// pending.
///
/// @tparam T The type of data being transferred. Must be nothrow move
/// constructible.
///
/// @code
/// void update_gui() {
///     if (auto result = my_buffer->try_take())
///         this->data = std::move(*result);
///
///     if (not this->has_data() && my_buffer->try_request()) {
///         // Executed ONCE until fulfill() or fail() is called.
///         add_gui_task([buf = my_buffer]() {   // shared_ptr copy
///             try {
///                 buf->fulfill(expensive_calculation());
///             } catch (...) {
///                 buf->fail();
///             }
///         });
///     }
/// }
/// @endcode
template<typename T>
class request_buffer
{
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "T must be nothrow move constructible");

public:
    enum class state : u8 { idle, pending, ready };

    request_buffer() noexcept = default;

    request_buffer(const request_buffer&)            = delete;
    request_buffer& operator=(const request_buffer&) = delete;

    ~request_buffer() noexcept
    {
        debug::ensure(m_state.load(std::memory_order_relaxed) !=
                        state::pending and
                      "request_buffer destroyed while a task is in flight");
    }

    /// GUI thread. Atomically switches idle -> pending.
    /// @return true if the caller must now launch the task, false if a task is
    /// already running or a result is waiting to be taken.
    bool try_request() noexcept
    {
        state expected = state::idle;
        return m_state.compare_exchange_strong(expected, state::pending,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    }

    /// Worker thread. Delivers the result (pending -> ready).
    void fulfill(T&& data) noexcept
    {
        debug::ensure(m_state.load(std::memory_order_relaxed) ==
                      state::pending);

        m_value.emplace(std::move(data));
        m_state.store(state::ready, std::memory_order_release);
    }

    /// Worker thread. Gives up without delivering a result (pending -> idle),
    /// to be called when the task throws. A new request may then be issued.
    void fail() noexcept
    {
        debug::ensure(m_state.load(std::memory_order_relaxed) ==
                      state::pending);

        m_state.store(state::idle, std::memory_order_release);
    }

    /// GUI thread, never blocks. Consumes the result (ready -> idle).
    /// @return the data if ready, otherwise std::nullopt.
    std::optional<T> try_take() noexcept
    {
        if (m_state.load(std::memory_order_acquire) != state::ready)
            return std::nullopt;

        std::optional<T> out{ std::move(m_value) };
        m_value.reset();
        m_state.store(state::idle, std::memory_order_release);
        return out;
    }

    /// Get the current state (mainly used in unit test.
    state current_state() const noexcept
    {
        return m_state.load(std::memory_order_acquire);
    }

private:
    std::atomic<state> m_state{ state::idle };
    std::optional<T>   m_value;
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

/** Wake-up signal of the unordered workers.
 *
 * A monotonic epoch: a worker reads the epoch, scans the lists and, if it found
 * nothing, sleeps until the epoch changes. An event published between the read
 * and the sleep changes the epoch, so the sleep returns immediately: no wake-up
 * can be lost and no polling (`sleep_for`) is needed. Based on C++20
 * @c std::atomic::wait / @c notify_all (futex on Linux, WaitOnAddress on
 * Windows): latency of a few microseconds instead of one timer tick.
 */
class worker_signal
{
public:
    u32 load() const noexcept
    {
        return m_epoch.load(std::memory_order_acquire);
    }

    void notify() noexcept
    {
        m_epoch.fetch_add(1, std::memory_order_release);
        m_epoch.notify_all();
    }

    //! Returns as soon as the epoch differs from @c seen.
    void wait(u32 seen) const noexcept
    {
        m_epoch.wait(seen, std::memory_order_acquire);
    }

private:
    std::atomic<u32> m_epoch{ 0 };
};

/** Ordered FIFO list executed by a single worker.
 *
 * Counters are atomic: they can be read (ImGui statistics) from any thread
 * while the worker runs. They are only modified under the mutex.
 */
class ordered_task_list
{
public:
    constexpr static std::size_t task_max = 256;

    ordered_task_list() noexcept  = default;
    ~ordered_task_list() noexcept = default;

    //! Only valid before @c task_manager::start().
    ordered_task_list(ordered_task_list&& other) noexcept;

    /** Enqueue a task.
     *
     * @return @c false, and the task is neither run nor counted, if the list
     * is stopping or if the @c task_max slots of the ring buffer are used.
     */
    template<typename Fn>
    bool add(Fn&& fn) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping || !m_queue.emplace_enqueue(std::forward<Fn>(fn)))
                return false;

            m_tasks_submitted.fetch_add(1, std::memory_order_relaxed);
        }
        worker_cv.notify_one();
        return true;
    }

    bool pop(task& out) noexcept;
    void notify_done() noexcept;

    //! Wait until every accepted task is done, or until @c shutdown().
    void wait_empty() noexcept;

    //! Stop accepting tasks; queued tasks are still executed. Wakes the
    //! workers and the threads blocked in @c wait_empty().
    void shutdown() noexcept;
    bool stopping() const noexcept;

    u64 tasks_submitted() const noexcept
    {
        return m_tasks_submitted.load(std::memory_order_relaxed);
    }

    u64 tasks_completed() const noexcept
    {
        return m_tasks_completed.load(std::memory_order_relaxed);
    }

private:
    ring_buffer<task> m_queue{ task_max };

    mutable std::mutex      m_mutex;
    std::condition_variable worker_cv;   // for workers (work available)
    std::condition_variable producer_cv; // for producers (fully drained)

    std::atomic<u64> m_tasks_submitted{ 0 };
    std::atomic<u64> m_tasks_completed{ 0 };
    u64              m_tasks_running{ 0 }; // protected by m_mutex

    bool m_stopping{ false }; // protected by m_mutex
};

class ordered_worker
{
public:
    explicit ordered_worker(ordered_task_list& list) noexcept;
    ordered_worker(ordered_worker&& other) noexcept;
    ~ordered_worker() noexcept = default;

    //! Idempotent. The destructor of @c task_manager joins the thread.
    void start() noexcept;
    void join() noexcept;

    u64 tasks_completed() const noexcept
    {
        return m_tasks_completed.load(std::memory_order_relaxed);
    }

    u64 execution_time_in_ns() const noexcept
    {
        return m_execution_time_ns.load(std::memory_order_relaxed);
    }

    u64 execution_time_in_ms() const noexcept
    {
        return execution_time_in_ns() / 1'000'000u;
    }

    bool initialized() const noexcept
    {
        return m_initialized.load(std::memory_order_acquire);
    }

private:
    ordered_task_list* m_list;
    std::thread        m_thread;

    std::atomic<u64>  m_tasks_completed{ 0 };
    std::atomic<u64>  m_execution_time_ns{ 0 };
    std::atomic<bool> m_initialized{ false };
};

/** Batch of independent tasks executed by all the unordered workers.
 *
 * Cycle: @c add()* -> @c submit() -> @c wait_completion(). A task added while a
 * batch executes is kept for the next batch (it is never dropped).
 */
class unordered_task_list
{
public:
    unordered_task_list() noexcept;

    //! Only valid before @c task_manager::start().
    unordered_task_list(unordered_task_list&& other) noexcept;

    /** Add a task to the next batch.
     *
     * @return @c false, and the task is not counted, if the list is stopping
     * or on allocation failure.
     */
    template<typename Fn>
    bool add(Fn&& fn) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
            return false;

        auto& dst = m_phase == phase::accepting ? m_pending : m_incoming;
        if (dst.emplace_back(std::forward<Fn>(fn)) == nullptr)
            return false;

        m_tasks_submitted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    //! Start the batch. Ignored if the previous batch is not finished.
    void submit() noexcept;
    bool try_steal(task& out) noexcept;
    void notify_done() noexcept;

    //! Wait for the end of the batch, or until @c shutdown().
    void wait_completion() noexcept;

    //! Tasks of the running batch not yet started are abandoned; tasks in
    //! progress are finished.
    void shutdown() noexcept;
    bool stopping() const noexcept;

    //! Number of tasks accepted by @c add().
    u64 tasks_submitted() const noexcept
    {
        return m_tasks_submitted.load(std::memory_order_relaxed);
    }

    //! Number of tasks whose execution is over.
    u64 tasks_completed() const noexcept
    {
        return m_tasks_completed.load(std::memory_order_relaxed);
    }

private:
    friend class task_manager;

    enum class phase : uint8_t { accepting, executing, shutting_down };

    vector<task> m_pending;  // the batch (accepting: being built)
    vector<task> m_incoming; // tasks added while the batch executes

    std::atomic<u64> m_tasks_submitted{ 0 };
    std::atomic<u64> m_tasks_completed{ 0 };
    u32              m_batch_size{ 0 };
    u32              m_next_index{ 0 };
    u32              m_completed{ 0 };

    mutable std::mutex      m_mutex;
    std::condition_variable m_producer_cv;

    worker_signal* m_signal{ nullptr }; // set by task_manager before start()

    phase m_phase{ phase::accepting };
    bool  m_stopping{ false };
};

class unordered_worker
{
public:
    unordered_worker(std::span<unordered_task_list> lists,
                     worker_signal&                 signal) noexcept;
    unordered_worker(unordered_worker&& other) noexcept;

    //! Idempotent. The destructor of @c task_manager joins the thread.
    void start() noexcept;
    void join() noexcept;

    u64 tasks_completed() const noexcept
    {
        return m_tasks_completed.load(std::memory_order_relaxed);
    }

    u64 execution_time_in_ns() const noexcept
    {
        return m_execution_time_ns.load(std::memory_order_relaxed);
    }

    u64 execution_time_in_ms() const noexcept
    {
        return execution_time_in_ns() / 1'000'000u;
    }

    bool initialized() const noexcept
    {
        return m_initialized.load(std::memory_order_acquire);
    }

private:
    std::span<unordered_task_list> m_lists;
    worker_signal*                 m_signal;
    std::thread                    m_thread;

    std::atomic<u64>  m_tasks_completed{ 0 };
    std::atomic<u64>  m_execution_time_ns{ 0 };
    std::atomic<bool> m_initialized{ false };
};

/** Owns the lists and the worker threads.
 *
 * The destructor calls @c shutdown(): forgetting it no longer ends in
 * @c std::terminate (joinable @c std::thread). Not copyable nor movable: the
 * threads reference the lists and the signal.
 *
 * @c shutdown() must not be called from a task.
 */
class task_manager
{
public:
    /**
     * @param unordered_worker_count Number of threads for the unordered
     * lists (0 means 1). No unordered thread is created if
     * @c unordered_count is 0.
     */
    task_manager(size_t ordered_count,
                 size_t unordered_count,
                 size_t unordered_worker_count =
                   std::thread::hardware_concurrency()) noexcept;

    ~task_manager() noexcept;

    task_manager(const task_manager&)            = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager(task_manager&&)                 = delete;
    task_manager& operator=(task_manager&&)      = delete;

    void start() noexcept;
    void shutdown() noexcept; //!< idempotent

    ordered_task_list&   ordered(std::integral auto i) noexcept;
    unordered_task_list& unordered(std::integral auto i) noexcept;

    size_t ordered_size() const noexcept;
    size_t unordered_size() const noexcept;
    size_t wordered_size() const noexcept;
    size_t wunordered_size() const noexcept;

    u64 wordered_tasks_completed(std::integral auto i) const noexcept;
    u64 wunordered_tasks_completed(std::integral auto i) const noexcept;

    //! Execution time in milliseconds.
    u64 wordered_execution_time(std::integral auto i) const noexcept;
    u64 wunordered_execution_time(std::integral auto i) const noexcept;

private:
    worker_signal m_signal; // before the lists and workers: used by both

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
    return m_ordered_workers[i].execution_time_in_ms();
}

inline u64 task_manager::wunordered_execution_time(
  std::integral auto i) const noexcept
{
    return m_unordered_workers[i].execution_time_in_ms();
}

} // namespace irt

#endif