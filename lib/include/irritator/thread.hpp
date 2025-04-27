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

enum class main_task : i8 { simulation_0 = 0, simulation_1, simulation_2, gui };

static inline constexpr int  unordered_task_list_tasks_number = 256;
static inline constexpr int  task_list_tasks_number           = 16;
static inline constexpr auto ordered_task_worker_size         = 4;
static inline constexpr auto unordered_task_worker_size       = 4;
static inline constexpr auto task_work_size =
  ordered_task_worker_size + unordered_task_worker_size;

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

//! @brief Simple task list access by only one thread
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

    std::atomic_flag         wakeup = ATOMIC_FLAG_INIT;
    circular_buffer<job, 16> tasks;
};

struct worker_stats {
    u32 num_submitted_tasks = 0u;
    u32 num_executed_tasks  = 0u;

    std::chrono::steady_clock::time_point start_time;
};

class task_manager
{
public:
    small_vector<task_list, ordered_task_worker_size>   ordered_task_lists;
    small_vector<worker_main, ordered_task_worker_size> ordered_task_workers;

    small_vector<unordered_task_list, unordered_task_worker_size>
      unordered_task_lists;
    small_vector<worker_generic, unordered_task_worker_size>
      unordered_task_workers;

    worker_stats ordered_task_stats[ordered_task_worker_size];
    worker_stats unordered_task_stats[unordered_task_worker_size];

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
  : stats(stats_)
  , tasks(unordered_task_list_tasks_number, reserve_tag{})
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
    const auto max_thread = hc <= 2u ? 2u
                            : unordered_task_worker_size < hc
                              ? unordered_task_worker_size
                              : hc - 2u;

    for (auto i = 0; i < ordered_task_worker_size; ++i)
        ordered_task_lists.emplace_back(ordered_task_stats[i]);

    for (auto i = 0; i < ordered_task_worker_size; ++i)
        ordered_task_workers.emplace_back();

    for (auto i = 0; i < ordered_task_worker_size; ++i)
        unordered_task_lists.emplace_back(unordered_task_stats[i]);

    for (auto i = 0u; i < max_thread; ++i)
        unordered_task_workers.emplace_back();

    for (auto i = 0; i < ordered_task_worker_size; ++i)
        for (auto j = 0u; j < max_thread; ++j)
            unordered_task_lists[i].workers.emplace_back(
              &unordered_task_workers[j]);

    for (auto i = 0; i < ordered_task_worker_size; ++i) {
        ordered_task_lists[i].worker = &ordered_task_workers[i];
        ordered_task_workers[i].tl   = &ordered_task_lists[i];
    }
}

inline task_manager::~task_manager() noexcept { finalize(); }

inline void task_manager::start() noexcept
{
    for (auto i = 0; i < ordered_task_worker_size; ++i)
        ordered_task_workers[i].start();

    for (auto i = 0; i < unordered_task_workers.ssize(); ++i)
        unordered_task_workers[i].start();
}

inline void task_manager::finalize() noexcept
{
    for (auto i = 0, e = unordered_task_workers.ssize(); i != e; ++i)
        unordered_task_workers[i].terminate();

    for (auto i = 0, e = ordered_task_workers.ssize(); i != e; ++i)
        ordered_task_workers[i].terminate();

    for (auto i = 0, e = unordered_task_lists.ssize(); i != e; ++i)
        unordered_task_lists[i].terminate();

    for (auto i = 0, e = ordered_task_lists.ssize(); i != e; ++i)
        ordered_task_lists[i].terminate();

    ordered_task_lists.clear();
    ordered_task_workers.clear();
    unordered_task_lists.clear();
    unordered_task_workers.clear();
}

} // namespace irt

#endif
