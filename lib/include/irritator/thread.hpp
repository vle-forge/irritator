// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <shared_mutex>
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

template<std::size_t ordered, std::size_t unordered>
class task_manager;

static inline constexpr int unordered_task_list_tasks_number = 256;
static inline constexpr int task_list_tasks_number           = 16;

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

/** Run a function @a fn according to the test of the @a std::atomic_flag.
 * The function is called if and only if the @a std::atomic_flag is false. */
template<typename Fn, typename... Args>
constexpr void scoped_flag_run(std::atomic_flag& flag,
                               Fn&&              fn,
                               Args&&... args) noexcept
{
    if (not flag.test_and_set()) { /* If the flag is false, then set to true
                                      and call the function. */
        std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
        flag.clear();
    }
}

template<typename T, int Size>
class static_buffer
{
public:
    static_assert(Size > 0);

    using size_type  = small_storage_size_t<Size>;
    using index_type = std::make_signed_t<size_type>;

    static_buffer(const static_buffer& o) noexcept            = delete;
    static_buffer(static_buffer&& o) noexcept                 = delete;
    static_buffer& operator=(const static_buffer& o) noexcept = delete;
    static_buffer& operator=(static_buffer&& o) noexcept      = delete;

    T& operator[](std::integral auto position) noexcept
    {
        return *(static_cast<T*>(m_buffer.data()) + position);
    }

    const T& operator[](std::integral auto position) const noexcept
    {
        return *(static_cast<T*>(m_buffer.data()) + position);
    }

    size_type capacity() const noexcept { return Size; }
    bool      good() const noexcept { return true; }

protected:
    std::array<std::byte, sizeof(T) * Size> m_buffer;
};

template<typename T,
         int Size,
         typename A = allocator<new_delete_memory_resource>>
class dynamic_buffer
{
public:
    static_assert(Size > 0);

    using size_type  = small_storage_size_t<Size>;
    using index_type = std::make_signed_t<size_type>;

    dynamic_buffer() noexcept
      : m_buffer(static_cast<std::byte*>(A::allocate(sizeof(T) * Size)))
    {}

    dynamic_buffer(const dynamic_buffer& o) noexcept            = delete;
    dynamic_buffer(dynamic_buffer&& o) noexcept                 = delete;
    dynamic_buffer& operator=(const dynamic_buffer& o) noexcept = delete;
    dynamic_buffer& operator=(dynamic_buffer&& o) noexcept      = delete;

    ~dynamic_buffer() noexcept
    {
        if (m_buffer)
            A::deallocate(m_buffer, Size * sizeof(T));

        m_buffer = nullptr;
    }

    T& operator[](std::integral auto position) noexcept
    {
        return *(reinterpret_cast<T*>(m_buffer) + position);
    }

    const T& operator[](std::integral auto position) const noexcept
    {
        return *(reinterpret_cast<T*>(m_buffer) + position);
    }

    size_type capacity() const noexcept { return Size; }
    bool      good() const noexcept { return m_buffer != nullptr; }

protected:
    std::byte* m_buffer = nullptr;
};

/**
 * The @c circular_buffer_base class provides a single-writer/single-reader fifo
 * queue where pushing and popping is wait-free.
 *
 * @tparam T A simple type (trivially object or nothrow).
 * @tparam Memory A fixed size buffer in heap or in stack.
 */
template<typename T, typename Memory>
class circular_buffer_base
{
public:
    using underlying_memory_type = Memory;
    using size_type              = typename underlying_memory_type::size_type;
    using index_type             = typename underlying_memory_type::index_type;

    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T> ||
                  std::is_nothrow_copy_assignable_v<T> ||
                  std::is_trivially_copy_assignable_v<T> ||
                  std::is_nothrow_move_assignable_v<T> ||
                  std::is_trivially_move_assignable_v<T>);

    constexpr circular_buffer_base() noexcept = default;

    constexpr ~circular_buffer_base() noexcept
    {
        while (not empty()) {
            T value;
            if (not pop(value))
                break;
        }
    }

    circular_buffer_base(const circular_buffer_base&) noexcept = delete;
    circular_buffer_base& operator=(const circular_buffer_base&) noexcept =
      delete;

    bool push(const T& value) noexcept
    {
        int head      = m_head.load(std::memory_order_relaxed);
        int next_head = next(head);
        if (next_head == m_tail.load(std::memory_order_acquire))
            return false;

        std::construct_at(&m_buffer[head], value);
        m_head.store(next_head, std::memory_order_release);

        return true;
    }

    bool pop(T& value) noexcept
    {
        int tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return false;

        if constexpr (std::is_trivially_move_assignable_v<T> or
                      std::is_nothrow_move_assignable_v<T>) {
            value = std::move(m_buffer[tail]);
        } else {
            value = m_buffer[tail];
        }

        std::destroy_at(&m_buffer[tail]);
        m_tail.store(next(tail), std::memory_order_release);

        return true;
    }

    bool empty() const noexcept
    {
        int tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return true;

        return false;
    }

    constexpr auto capacity() noexcept { return m_buffer.capacity(); }

private:
    constexpr auto next(int current) noexcept -> int
    {
        return (current + 1) % capacity();
    }

private:
    underlying_memory_type m_buffer;
    std::atomic_int        m_head;
    std::atomic_int        m_tail;
};

/**
 * The @c static_circular_buffer class provides a single-writer/single-reader
 * fifo queue where pushing and popping is wait-free and a stack allocated
 * underlying buffer.
 *
 * @tparam T A simple type (trivially object or nothrow).
 * @tparam Size The capacity of the underlying buffer.
 */
template<typename T, int Size>
using static_circular_buffer = circular_buffer_base<T, static_buffer<T, Size>>;

/**
 * The @c circular_buffer class provides a single-writer/single-reader fifo
 * queue where pushing and popping is wait-free and a heap allocated underlying
 * buffer.
 *
 * @tparam T A simple type (trivially object or nothrow).
 * @tparam Size The capacity of the underlying buffer.
 */
template<typename T, int Size>
using circular_buffer = circular_buffer_base<T, dynamic_buffer<T, Size>>;

/**
 * The @c task is a function with a static buffer allowing 4 sizeof(u64) data in
 * lambda capture.
 *
 * Simplicity key to scalability:
 * – @c task has well defined input and output.
 * – Independent stateless, no stalls, always completes.
 * – @c task added to @c main_task_list.
 * – @c task fully independent
 */
using task =
  small_function<(sizeof(void*) * 2) + (sizeof(u64) * 4), void(void)>;

/**
 * The @c task is a function with a static buffer allowing 4 sizeof(u64) data in
 * lambda capture.
 *
 * Simplicity key to scalability:
 * - @c job build by task for large parallelisable task.
 * – @c job has well defined input and output.
 * – Independent stateless, no stalls, always completes.
 * – @c job added to @c temp_task_list.
 * – @c job fully independent.
 */
struct job {
    job() noexcept = default;

    job(const task& t, std::atomic_int* cnt) noexcept
      : function(t)
      , counter(cnt)
    {}

    task             function;
    std::atomic_int* counter = nullptr;
};

/**
 * Simple task list access by only one thread.
 */
struct task_list {
    worker_stats&                                 stats;
    circular_buffer<task, task_list_tasks_number> tasks;

    worker_main* worker = nullptr;

    //! number of task since task_list constructor
    std::atomic_int  task_number = 0;
    std::atomic_int  task_done   = 0;
    std::atomic_flag wakeup      = ATOMIC_FLAG_INIT;

    explicit task_list(worker_stats& stats) noexcept;
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

    worker_stats& stats;
    vector<task>  tasks;

    vector<worker_generic*> workers;
    std::atomic_int         current_task = 0;
    std::atomic_int         phase        = ordinal(phase::add);

    explicit unordered_task_list(worker_stats& stats) noexcept;
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
    enum class phase { starting, start, run, stop, done };

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

    std::atomic_int phase = ordinal(phase::starting);
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

    std::atomic_flag         wakeup = ATOMIC_FLAG_INIT;
    circular_buffer<job, 16> tasks;
};

struct worker_stats {
    u32 num_submitted_tasks = 0u;
    u32 num_executed_tasks  = 0u;

    std::chrono::steady_clock::time_point start_time;
};

template<std::size_t ordered = 4, std::size_t unordered = 1>
class task_manager
{
public:
    static_assert(ordered > 0);
    static_assert(unordered > 0);

    enum class phase { starting, running, done };

    small_vector<task_list, ordered>   ordered_task_lists;
    small_vector<worker_main, ordered> ordered_task_workers;

    small_vector<unordered_task_list, unordered> unordered_task_lists;
    small_vector<worker_generic, unordered>      unordered_task_workers;

    worker_stats ordered_task_stats[ordered];
    worker_stats unordered_task_stats[unordered];

    std::atomic_int phase = ordinal(phase::starting);

    task_manager() noexcept;
    ~task_manager() noexcept;

    task_manager(task_manager&& params)          = delete;
    task_manager(const task_manager& params)     = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager& operator=(task_manager&&)      = delete;

    /** Creating task lists and spawning workers and wait for them so they can
     * start their run loops. */
    void start() noexcept;

    /** Send message to thread to stop theirs run loops and wait for
     * termination. */
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
  : stats(stats_)
  , tasks(unordered_task_list_tasks_number, reserve_tag)
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
    debug::ensure(any_equal(phase, ordinal(phase::starting)));
    phase = ordinal(phase::start);

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

    phase = ordinal(phase::done);
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

    phase = ordinal(phase::done);
}

/*
    task_manager
 */

template<std::size_t ordered, std::size_t unordered>
inline task_manager<ordered, unordered>::task_manager() noexcept
{
    const auto hc = numeric_cast<sz>(std::thread::hardware_concurrency());
    const auto max_thread_ordered   = std::clamp(hc, sz{ 0 }, ordered);
    const auto max_thread_unordered = std::clamp(hc, sz{ 0 }, unordered);

    for (sz i = 0; i < max_thread_ordered; ++i)
        ordered_task_lists.emplace_back(ordered_task_stats[i]);

    for (sz i = 0; i < max_thread_ordered; ++i)
        ordered_task_workers.emplace_back();

    for (sz i = 0; i < max_thread_unordered; ++i)
        unordered_task_lists.emplace_back(unordered_task_stats[i]);

    for (sz i = 0; i < max_thread_unordered; ++i)
        unordered_task_workers.emplace_back();

    for (sz i = 0; i < max_thread_unordered; ++i)
        for (sz j = 0; j < max_thread_unordered; ++j)
            unordered_task_lists[i].workers.emplace_back(
              &unordered_task_workers[j]);

    for (sz i = 0; i < max_thread_ordered; ++i) {
        ordered_task_lists[i].worker = &ordered_task_workers[i];
        ordered_task_workers[i].tl   = &ordered_task_lists[i];
    }
}

template<std::size_t ordered, std::size_t unordered>
inline task_manager<ordered, unordered>::~task_manager() noexcept
{
    if (phase != ordinal(phase::done))
        finalize();
}

template<std::size_t ordered, std::size_t unordered>
inline void task_manager<ordered, unordered>::start() noexcept
{
    debug::ensure(phase == ordinal(phase::starting));

    for (auto i = 0; i < ordered_task_workers.ssize(); ++i)
        ordered_task_workers[i].start();

    for (auto i = 0; i < unordered_task_workers.ssize(); ++i)
        unordered_task_workers[i].start();

    int remaining;

    do {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });

        remaining =
          ordered_task_workers.ssize() + unordered_task_workers.ssize();
        for (auto i = 0; i < ordered_task_workers.ssize(); ++i)
            if (any_equal(ordered_task_workers[i].phase,
                          ordinal(worker_base<worker_main>::phase::start),
                          ordinal(worker_base<worker_main>::phase::run)))
                remaining--;

        for (auto i = 0; i < unordered_task_workers.ssize(); ++i)
            if (any_equal(unordered_task_workers[i].phase,
                          ordinal(worker_base<worker_main>::phase::start),
                          ordinal(worker_base<worker_main>::phase::run)))
                remaining--;
    } while (remaining > 0);

    phase = ordinal(phase::running);
}

template<std::size_t ordered, std::size_t unordered>
inline void task_manager<ordered, unordered>::finalize() noexcept
{
    debug::ensure(phase == ordinal(phase::running));

    for (auto i = 0, e = unordered_task_workers.ssize(); i != e; ++i)
        unordered_task_workers[i].terminate();

    for (auto i = 0, e = ordered_task_workers.ssize(); i != e; ++i)
        ordered_task_workers[i].terminate();

    for (auto i = 0, e = unordered_task_lists.ssize(); i != e; ++i)
        unordered_task_lists[i].terminate();

    for (auto i = 0, e = ordered_task_lists.ssize(); i != e; ++i)
        ordered_task_lists[i].terminate();

    int remaining;

    do {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });

        remaining =
          ordered_task_workers.ssize() + unordered_task_workers.ssize();

        for (auto i = 0; i < ordered_task_workers.ssize(); ++i)
            if (ordered_task_workers[i].phase ==
                ordinal(worker_base<worker_main>::phase::done))
                remaining--;

        for (auto i = 0; i < unordered_task_workers.ssize(); ++i)
            if (unordered_task_workers[i].phase ==
                ordinal(worker_base<worker_main>::phase::done))
                remaining--;
    } while (remaining > 0);

    ordered_task_lists.clear();
    ordered_task_workers.clear();
    unordered_task_lists.clear();
    unordered_task_workers.clear();

    phase = ordinal(phase::done);
}

/// A class to wrap object of type @a T with a @a shared_mutex.
///
/// Access to the underlying object @a T is done using @a read_only and @a
/// read_write functions with lambda function or using @a value and @a
/// read_only_value. The read-only functions use a @a shared_lock to shared
/// object. The read-write functions use a @a unique_lock.
template<typename T>
class locker
{
public:
    template<typename... Args>
    explicit locker(Args&&... args) noexcept
      : m_mutex()
      , m_value(std::forward<Args>(args)...)
    {}

    /// Get an access to the underlying value @c T in read-only mode.
    template<typename Fn, typename... Args>
    void read_only(Fn&& fn, Args&&... args) const noexcept
    {
        std::shared_lock lock(m_mutex);
        std::invoke(std::forward<Fn>(fn), m_value, std::forward<Args>(args)...);
    }

    /// Try to get an access to the underlying value @c T in read-only mode.
    template<typename Fn, typename... Args>
    bool try_read_only(Fn&& fn, Args&&... args) const noexcept
    {
        if (std::shared_lock lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            std::invoke(
              std::forward<Fn>(fn), m_value, std::forward<Args>(args)...);

            return true;
        }

        return false;
    }

    /// Get an access to the underlying value @c T in read-write mode.
    template<typename Fn, typename... Args>
    void read_write(Fn&& fn, Args&&... args) noexcept
    {
        std::unique_lock lock(m_mutex);
        std::invoke(std::forward<Fn>(fn), m_value, std::forward<Args>(args)...);
    }

private:
    mutable std::shared_mutex m_mutex;
    T                         m_value;
};

/// A class to wrap two objects of type @a T with two mutexes.
///
/// Access to the underlying object @a T is done using @a read_only and @a
/// read_write functions with lambda function or using @a value and @a
/// read_only_value. The read-only functions use a @a shared_lock to shared
/// object. The read-write functions use a @a unique_lock. The read-write
/// function copy the buffer, call the @c fn function and swap the buffer after
/// update.
///
/// Use this class when you often read the value and only sometimes write it and
/// when the cost of the copy is negligible compared to the modification time.
template<typename T>
class locker_2
{
    static_assert(std::is_default_constructible_v<T>,
                  "T muste be default-constructible for 2nd buffer");

    static_assert(std::is_nothrow_copy_assignable_v<T> or
                    std::is_trivially_copy_assignable_v<T>,
                  "T must be assignable");

    static_assert(std::is_swappable_v<T>, "T must be swappable");

public:
    template<typename... Args>
    explicit locker_2(Args&&... args) noexcept
      : m_mutex()
      , m_value(std::forward<Args>(args)...)
      , m_value_2nd()
    {}

    /// Get an access to the underlying value @c T in read-only mode.
    template<typename Fn, typename... Args>
    void read_only(Fn&& fn, Args&&... args) const noexcept
    {
        std::shared_lock lock(m_mutex);
        std::invoke(std::forward<Fn>(fn), m_value, std::forward<Args>(args)...);
    }

    /// Try to get an access to the underlying value @c T in read-only mode.
    template<typename Fn, typename... Args>
    bool try_read_only(Fn&& fn, Args&&... args) const noexcept
    {
        if (std::shared_lock lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            std::invoke(
              std::forward<Fn>(fn), m_value, std::forward<Args>(args)...);

            return true;
        }

        return false;
    }

    /// Get an access to the underlying value @c T in read-write mode.
    ///
    /// First, the function lock the mutex using a @c shared_lock to read the
    /// value and make a copy into the 2nd buffer after the lock of the 2nd
    /// mutex them the function unlock the first mutex. The user function @c fn
    /// is called and finally, the function lock the mutex using a @c
    /// unique_lock and the buffer are swapped.
    template<typename Fn, typename... Args>
    void read_write(Fn&& fn, Args&&... args) noexcept
    {
        // Lock the first mutex in read mode and the second mutex in write mode.
        std::shared_lock lock_copy(m_mutex);
        std::unique_lock lock_writer(m_mutex_2nd_buffer);
        m_value_2nd = m_value;

        // Unlock the first mutex to allow reader while writing into the 2nd
        // buffer.
        lock_copy.unlock();

        std::invoke(
          std::forward<Fn>(fn), m_value_2nd, std::forward<Args>(args)...);

        // Lock the first mutex in write to perform the swap between buffers.
        std::unique_lock lock_swap(m_mutex);

        using std::swap;
        swap(m_value, m_value_2nd);
    }

private:
    mutable std::shared_mutex m_mutex;
    mutable spin_mutex        m_mutex_2nd_buffer;

    T m_value;
    T m_value_2nd; //!< A second value to be used in a second thread during a
                   //!< writing process.
};

} // namespace irt

#endif
