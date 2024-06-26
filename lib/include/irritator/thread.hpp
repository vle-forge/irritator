// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>

namespace irt {

template<typename T>
struct worker_base;
struct worker_main;
struct worker_generic;
struct task_list;
struct unordered_task_list;
struct worker_stats;
class task_manager;

enum class main_task : i8 { simulation = 0, gui };

static inline constexpr int  unordered_task_list_tasks_number = 256;
static inline constexpr int  task_list_tasks_number           = 16;
static inline constexpr auto main_task_size                   = 2;
static inline constexpr auto max_temp_worker                  = 8;
static inline constexpr auto max_threads = main_task_size + max_temp_worker;

//! A `std::mutex` like based on `std::atomic_flag::wait` and
//! `std::atomic_flag::notify_one` standard functions.
class spin_mutex
{
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;

public:
    spin_mutex() noexcept  = default;
    ~spin_mutex() noexcept = default;

    spin_mutex(const spin_mutex& other) noexcept;

    void lock() noexcept;
    bool try_lock() noexcept;
    void unlock() noexcept;
};

//! A `std::mutex` like based on `std::atomic_flag` and
//! `std::thread::yied`.
class spin_yield_mutex_lock
{
    std::atomic_flag m_flag = ATOMIC_FLAG_INIT;

public:
    spin_yield_mutex_lock() noexcept;
    bool try_lock() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
};

template<typename T, int Size>
class thread_safe_ring_buffer
{
public:
    static_assert(Size >= 1);
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);
    static_assert(std::is_nothrow_copy_assignable_v<T> ||
                  std::is_trivially_copy_assignable_v<T> ||
                  std::is_nothrow_move_assignable_v<T> ||
                  std::is_trivially_move_assignable_v<T>);

    constexpr thread_safe_ring_buffer() noexcept = default;

    thread_safe_ring_buffer(const thread_safe_ring_buffer&) noexcept = delete;
    thread_safe_ring_buffer& operator=(
      const thread_safe_ring_buffer&) noexcept = delete;

    bool push(const T& value) noexcept
    {
        int head      = m_head.load(std::memory_order_relaxed);
        int next_head = next(head);
        if (next_head == m_tail.load(std::memory_order_acquire))
            return false;

        m_ring[head] = value;
        m_head.store(next_head, std::memory_order_release);

        return true;
    }

    bool pop(T& value) noexcept
    {
        int tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return false;

        else if constexpr (std::is_trivially_move_assignable_v<T> or
                           std::is_nothrow_move_assignable_v<T>)
            value = std::move(m_ring[tail]);
        else
            value = m_ring[tail];

        m_tail.store(next(tail), std::memory_order_release);

        return true;
    }

    constexpr auto capacity() noexcept -> int { return Size; }

private:
    constexpr auto next(int current) noexcept -> int
    {
        return (current + 1) % Size;
    }

    T m_ring[Size];

    std::atomic_int m_head, m_tail;
};

// Simplicity key to scalability
// – Task has well defined input and output
// – Independent stateless, no stalls, always completes
// – Task added to @c main_task_list
// – Task fully independent
using task =
  small_function<(sizeof(void*) * 2) + (sizeof(u64) * 4), void(void)>;

// Simplicity key to scalability
// - Job build by task for large parallelisable task
// – Job has well defined input and output
// – Independent stateless, no stalls, always completes
// – Job added to @c temp_task_list
// – Task fully independent
struct job {
    job() noexcept = default;

    job(const task& t, std::atomic_int* cnt) noexcept
      : function(t)
      , counter(cnt)
    {}

    task             function;
    std::atomic_int* counter = nullptr;
};

//! @brief Simple task list access by only one thread
struct task_list {
    worker_stats&                                         stats;
    thread_safe_ring_buffer<task, task_list_tasks_number> tasks;

    worker_main* worker = nullptr;

    //! number of task since task_list constructor
    std::atomic_int  task_number = 0;
    std::atomic_int  task_done   = 0;
    std::atomic_flag wakeup      = ATOMIC_FLAG_INIT;

    task_list(worker_stats& stats) noexcept;
    ~task_list() noexcept = default;
    task_list(task_list&& other) noexcept;

    task_list(const task_list& other)            = delete;
    task_list& operator=(const task_list& other) = delete;
    task_list& operator=(task_list&& other)      = delete;

    // Add a task into @c tasks ring (can wait until ring buffer has a place).
    template<typename Fn>
    void add(Fn&& fn) noexcept;
    void submit() noexcept;
    void wait() noexcept;
    void terminate() noexcept;
};

struct unordered_task_list {
    enum class phase { add, submit, run, stop };

    worker_stats&                                        stats;
    small_vector<task, unordered_task_list_tasks_number> tasks;

    vector<worker_generic*> workers;
    std::atomic_int         current_task = 0;
    std::atomic_int         phase        = ordinal(phase::add);

    unordered_task_list(worker_stats& stats) noexcept;
    ~unordered_task_list() noexcept = default;
    unordered_task_list(unordered_task_list&& other) noexcept;

    unordered_task_list(const unordered_task_list& other)            = delete;
    unordered_task_list& operator=(const unordered_task_list& other) = delete;
    unordered_task_list& operator=(unordered_task_list&& other)      = delete;

    // Add a task into @c tasks ring (can wait until ring buffer has a place).
    template<typename Fn>
    bool add(Fn&& fn) noexcept;
    void submit() noexcept;
    void wait() noexcept;
    void terminate() noexcept;
};

template<typename T>
struct worker_base {
    enum class phase { start, run, stop };

    worker_base() noexcept = default;
    ~worker_base() noexcept;
    worker_base(worker_base&& other) noexcept;

    worker_base(const worker_base& other)            = delete;
    worker_base& operator=(const worker_base& other) = delete;
    worker_base& operator=(worker_base&& other)      = delete;

    //! Initialize the encapsulated std::thread with the T::run() function.
    void start() noexcept;

    //! Send message to stop the std::thread T::run() loop.
    void terminate() noexcept;

    std::thread              thread;
    std::chrono::nanoseconds exec_time{};

    std::atomic_int phase = ordinal(phase::start);
};

struct worker_main : public worker_base<worker_main> {
    worker_main() noexcept  = default;
    ~worker_main() noexcept = default;
    worker_main(worker_main&& other) noexcept;

    void run() noexcept;

    task_list* tl = nullptr;
};

struct worker_generic : public worker_base<worker_generic> {
    worker_generic() noexcept  = default;
    ~worker_generic() noexcept = default;
    worker_generic(worker_generic&& other) noexcept;

    void run() noexcept;

    std::atomic_flag                 wakeup = ATOMIC_FLAG_INIT;
    thread_safe_ring_buffer<job, 16> tasks;
};

struct worker_stats {
    u32 num_submitted_tasks = 0u;
    u32 num_executed_tasks  = 0u;

    std::chrono::steady_clock::time_point start_time;
};

class task_manager
{
public:
    small_vector<task_list, main_task_size>   main_task_lists;
    small_vector<worker_main, main_task_size> main_workers;

    small_vector<unordered_task_list, main_task_size> temp_task_lists;
    small_vector<worker_generic, max_temp_worker>     temp_workers;

    worker_stats main_task_lists_stats[main_task_size];
    worker_stats temp_task_lists_stats[main_task_size];

    task_manager() noexcept;
    ~task_manager() noexcept;

    task_manager(task_manager&& params)          = delete;
    task_manager(const task_manager& params)     = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager& operator=(task_manager&&)      = delete;

    // creating task lists and spawning workers
    void start() noexcept;
    void finalize() noexcept;
};

/*****************************************************************************
 *
 * Implementation
 *
 ****************************************************************************/

//
// spin_lock
//

inline spin_mutex::spin_mutex(const spin_mutex& /*other*/) noexcept
  : spin_mutex()
{}

inline void spin_mutex::lock() noexcept
{
    while (m_flag.test_and_set(std::memory_order_acquire))
        m_flag.wait(true, std::memory_order_relaxed);
}

inline bool spin_mutex::try_lock() noexcept
{
    return not m_flag.test_and_set(std::memory_order_acquire);
}

inline void spin_mutex::unlock() noexcept
{
    m_flag.clear(std::memory_order_release);
    m_flag.notify_one();
}

//
// spin_bad_lock
//

inline spin_yield_mutex_lock::spin_yield_mutex_lock() noexcept
{
    m_flag.clear();
}

inline bool spin_yield_mutex_lock::try_lock() noexcept
{
    return !m_flag.test_and_set(std::memory_order_acquire);
}

inline void spin_yield_mutex_lock::lock() noexcept
{
    for (size_t i = 0; !try_lock(); ++i)
        if ((i % 100) == 0)
            std::this_thread::yield();
}

inline void spin_yield_mutex_lock::unlock() noexcept
{
    m_flag.clear(std::memory_order_release);
}

/*
    task_list
 */

inline task_list::task_list(worker_stats& stats_) noexcept
  : stats{ stats_ }
{
    stats.start_time = std::chrono::steady_clock::now();
}

inline task_list::task_list(task_list&& other) noexcept
  : stats{ other.stats }
{
    stats.start_time = std::chrono::steady_clock::now();
}

template<typename Fn>
void task_list::add(Fn&& fn) noexcept
{
    while (!tasks.push(task{ fn })) {
        wakeup.test_and_set();
        wakeup.notify_all();

        std::this_thread::yield();
    }

    ++stats.num_submitted_tasks;
    ++task_number;
}

inline void task_list::submit() noexcept
{
    wakeup.test_and_set();
    wakeup.notify_all();

    stats.num_executed_tasks = static_cast<u32>(task_done);
}

inline void task_list::wait() noexcept
{
    do {
        wakeup.test_and_set();
        wakeup.notify_all();

        wakeup.wait(true);
    } while (task_number < task_done);

    stats.num_executed_tasks = static_cast<u32>(task_done);

    wakeup.clear();
}

inline void task_list::terminate() noexcept
{
    wakeup.test_and_set();
    wakeup.notify_all();
}

inline unordered_task_list::unordered_task_list(worker_stats& stats_) noexcept
  : stats{ stats_ }
{
    stats.start_time = std::chrono::steady_clock::now();
}

template<typename Fn>
inline bool unordered_task_list::add(Fn&& fn) noexcept
{
    debug::ensure(any_equal(phase, ordinal(phase::add)));
    phase = ordinal(phase::add);

    if (tasks.full())
        return false;

    ++stats.num_submitted_tasks;

    tasks.emplace_back(fn);
    return true;
}

inline void unordered_task_list::submit() noexcept
{
    debug::ensure(any_equal(phase, ordinal(phase::add)));
    phase = ordinal(phase::submit);

    current_task = 0;
    int i = 0, e = tasks.ssize();

    while (i < e) {
        for (auto* w : workers) {
            const int old_i = i;

            while (i < e) {
                job j(tasks[i], &current_task);

                if (w->tasks.push(j)) {
                    ++i;
                } else {
                    break;
                }
            }

            if (old_i != i) {
                w->wakeup.test_and_set();
                w->wakeup.notify_all();
            }
        }
    }
}

inline void unordered_task_list::wait() noexcept
{
    debug::ensure(any_equal(phase, ordinal(phase::submit)));

    do {
        for (auto* w : workers) {
            w->wakeup.test_and_set();
            w->wakeup.notify_all();
        }

        for (auto* w : workers) {
            w->wakeup.wait(true);
        }
    } while (current_task < tasks.ssize());

    for (auto* w : workers) {
        w->wakeup.clear();
    }

    stats.num_executed_tasks += static_cast<u32>(tasks.size());

    current_task = 0;
    tasks.clear();
    phase = ordinal(phase::add);
}

inline void unordered_task_list::terminate() noexcept
{
    for (auto* w : workers) {
        w->wakeup.test_and_set();
        w->wakeup.notify_all();
    }

    phase = ordinal(phase::stop);
}

/*
    worker
 */

template<typename T>
inline worker_base<T>::worker_base(worker_base<T>&& other) noexcept
  : thread{ std::move(other.thread) }
  , phase{ other.phase.load() }
{
    other.thread = std::thread();
    other.phase  = ordinal(phase::stop);
}

template<typename T>
inline worker_base<T>::~worker_base() noexcept
{
    terminate();

    try {
        if (thread.joinable()) {
            thread.join();
        }
    } catch (...) {
    }
}

template<typename T>
inline void worker_base<T>::start() noexcept
{
    debug::ensure(any_equal(phase, ordinal(phase::start)));

    try {
        thread = std::thread{ &T::run, static_cast<T*>(this) };
        phase  = ordinal(phase::run);
    } catch (...) {
    }
}

template<typename T>
inline void worker_base<T>::terminate() noexcept
{
    phase = ordinal(phase::stop);

    if constexpr (std::is_same_v<T, worker_generic>) {
        reinterpret_cast<T*>(this)->wakeup.test_and_set();
        reinterpret_cast<T*>(this)->wakeup.notify_all();
    }
}

inline worker_main::worker_main(worker_main&& other) noexcept
  : worker_base{ std::move(other) }
  , tl{ other.tl }
{
    other.tl = nullptr;
}

inline void worker_main::run() noexcept
{
    while (phase != ordinal(phase::stop)) {
        tl->wakeup.wait(false);

        task t;

        for (;;) {
            if (tl->tasks.pop(t)) {
                const auto start = std::chrono::steady_clock::now();
                t();
                exec_time += std::chrono::steady_clock::now() - start;
                ++tl->task_done;
            } else {
                tl->wakeup.clear();
                tl->wakeup.notify_all();
                break;
            }
        }
    }
}

inline worker_generic::worker_generic(worker_generic&& other) noexcept
  : worker_base{ std::move(other) }
{}

inline void worker_generic::run() noexcept
{
    while (phase != ordinal(phase::stop)) {
        wakeup.wait(false);

        job j;

        for (;;) {
            if (tasks.pop(j)) {
                const auto start = std::chrono::steady_clock::now();
                j.function();
                exec_time += std::chrono::steady_clock::now() - start;
                j.counter->fetch_add(1);
            } else {
                wakeup.clear();
                wakeup.notify_all();
                break;
            }
        }
    }
}

/*
    task_manager
 */

inline task_manager::task_manager() noexcept
{
    const auto hc         = std::thread::hardware_concurrency();
    const auto max_thread = hc <= 2u               ? 2u
                            : max_temp_worker < hc ? max_temp_worker
                                                   : hc - 2u;

    for (auto i = 0; i < main_task_size; ++i)
        main_task_lists.emplace_back(main_task_lists_stats[i]);

    for (auto i = 0; i < main_task_size; ++i)
        main_workers.emplace_back();

    for (auto i = 0; i < main_task_size; ++i)
        temp_task_lists.emplace_back(temp_task_lists_stats[i]);

    for (auto i = 0u; i < max_thread; ++i)
        temp_workers.emplace_back();

    for (auto i = 0; i < main_task_size; ++i)
        for (auto j = 0u; j < max_thread; ++j)
            temp_task_lists[i].workers.emplace_back(&temp_workers[j]);

    for (auto i = 0; i < main_task_size; ++i) {
        main_task_lists[i].worker = &main_workers[i];
        main_workers[i].tl        = &main_task_lists[i];
    }
}

inline task_manager::~task_manager() noexcept { finalize(); }

inline void task_manager::start() noexcept
{
    for (auto i = 0; i < main_task_size; ++i)
        main_workers[i].start();

    for (auto i = 0; i < temp_workers.ssize(); ++i)
        temp_workers[i].start();
}

inline void task_manager::finalize() noexcept
{
    for (auto i = 0, e = temp_workers.ssize(); i != e; ++i)
        temp_workers[i].terminate();

    for (auto i = 0, e = main_workers.ssize(); i != e; ++i)
        main_workers[i].terminate();

    for (auto i = 0, e = temp_task_lists.ssize(); i != e; ++i)
        temp_task_lists[i].terminate();

    for (auto i = 0, e = main_task_lists.ssize(); i != e; ++i)
        main_task_lists[i].terminate();

    main_task_lists.clear();
    main_workers.clear();
    temp_task_lists.clear();
    temp_workers.clear();
}

} // namespace irt

#endif