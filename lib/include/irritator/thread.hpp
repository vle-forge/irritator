// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <barrier>
#include <latch>
#include <thread>

namespace irt {

// - main thread (default) for gui
//
// affect chamge from other thread
//
//
//
// - modeling job list
//   - remove
//   - copy
//   - load irt
//   - save irt
//   - load desc
//   - save desc
//   - clear
//
// - simulation job list
//   - remove
//   - copy
//   - load irt
//   - save irt
//   - load desc
//   - save desc
//   - clear
//
//
//
// task = { void(function*)(void*); void* data; status s; int state };
// task_list = data_array<task, task_id> + hierarchy<task>;
//
//
// task = { enum task_type, void *data, status s, int state };
// ring-buffer = { vector<task> + begin + last}
// Ex. :
// for(x : ...)
//    tl.add(...)
// tl.submit
// tl.wait
//
// tl.add(modeling_copy_children, args)
// tl.add(sync, nullptr);
// tl.submit()
// tl.wait()
//
// for (child : selected_children)
//   tl.add(modeling_copy_child, child);
// tl.add(sync, nullptr);
// tl.submit()
// tl.wait()
//
// for (child : bag)
//   tl.add(simulation_make_transition, child);
// tl.submit()
// tl.wait()
//
// std::shared_mutex + std::shared_lock
// - multiple reader one writer
//   (shared_lock    unique_lock)

class spin_lock
{
    std::atomic_flag flag;

public:
    spin_lock() noexcept;

    bool try_lock() noexcept;
    void lock() noexcept;
    void unlock() noexcept;
};

class scoped_spin_lock
{
    spin_lock& spin;

public:
    scoped_spin_lock(spin_lock& spin_) noexcept;
    ~scoped_spin_lock() noexcept;
};

class latch
{
    std::atomic<i32> counter;

public:
    inline constexpr latch(i32 expected) noexcept
      : counter(expected)
    {}

    ~latch() noexcept            = default;
    latch(const latch&) noexcept = delete;
    latch& operator=(const latch&) noexcept = delete;
    latch(latch&&) noexcept                 = delete;
    latch& operator=(latch&&) noexcept = delete;

    void count_down(i32 update = 1) noexcept
    {
        const auto old = counter.fetch_sub(update, std::memory_order_release);
        if (old == update)
            counter.notify_all();
    }

    bool try_wait() const noexcept
    {
        return counter.load(std::memory_order_acquire) == 0;
    }

    void wait() const noexcept
    {
        for (;;) {
            const auto old = counter.load(std::memory_order_acquire);
            if (old == 0)
                break;

            counter.wait(old, std::memory_order_acquire);
        }
    }

    void arrive_and_wait(i32 update = 1) noexcept
    {
        count_down(update);
        wait();
    }
};

enum class task_type : i8
{
    sync,

    modeling_load_irt,
    modeling_save_irt,
    modeling_load_desc,
    modeling_save_desc,
    modeling_remove_childen,
    modeling_copy_children,
    modeling_clear,

    simulation_change_child,
    simulation_remove_childen,
    simulation_copy_children,
    simulation_clear,

    simulation_transition,
};

enum class task_state : i8
{
    undefined,
    todo,
    running,
    stalled,
    done,
};

// Simplicity key to scalability
// – Job has well defined input and output
// – Independent stateless, no stalls, always completes
// – Jobs added to job lists
// – Multiple job lists
// – Job lists fully independent
// – Simple synchronization of jobs within list through "signal" and
// "synchronize" tokens

using task_function = void (*)(void*) noexcept;

struct task
{
    task() noexcept = default;

    task(task_function function_, void* parameter_) noexcept
      : function(function_)
      , parameter(parameter_)
      , state(task_state::todo)
      , operation_status(status::success)
    {}

    task_function function         = nullptr;
    void*         parameter        = nullptr;
    task_state    state            = task_state::undefined;
    status        operation_status = status::success;
};

struct task;
struct task_list;
struct worker;
class task_manager;

static inline constexpr i32 max_task_list = 4;
static inline constexpr i32 max_threads   = 8;

//! @brief Simple task list access by only one thread
struct task_list
{
    ring_buffer<task, 256> tasks;
    worker*                worker = nullptr;
    spin_lock              spin;

    i32 task_number = 0;   // number of task since task_list constructor
    i8  priority    = 127; // task_list priority (-127 better than 127).

    task_list() noexcept = default;

    void add(task_function function, void* parameter) noexcept
    {
        for (;;) {
            {
                scoped_spin_lock lock(spin);
                if (!tasks.full()) {
                    tasks.emplace_enqueue(function, parameter);
                    ++task_number;
                    return;
                }
            }

            std::this_thread::yield();
        }
    }

    void wait() noexcept
    {
        for (;;) {
            {
                scoped_spin_lock lock(spin);
                if (tasks.empty()) {
                    tasks.clear();
                    return;
                }
            }

            std::this_thread::yield();
        }
    }
};

struct worker
{
    void start() noexcept { thread = std::jthread{ &worker::run, this }; }
    void terminate() noexcept { is_terminating = true; }

    worker() noexcept = default;

    worker(const worker& other) = delete;
    worker& operator=(const worker& other) = delete;

    void run() noexcept
    {
        std::sort(task_lists.begin(),
                  task_lists.end(),
                  [](const task_list* left, const task_list* right) -> bool {
                      return left->priority < right->priority;
                  });

        for (;;) {
            while (!tasks.empty()) {
                auto task = tasks.dequeue();
                task.function(task.parameter);
            }

            for (auto& lst : task_lists) {
                scoped_spin_lock lock(lst->spin);
                if (lst->tasks.empty())
                    continue;

                while (!tasks.full() && !lst->tasks.empty())
                    tasks.enqueue(lst->tasks.dequeue());
            }

            if (tasks.empty() && is_terminating) {
                bool all_empty = true;

                for (auto& lst : task_lists) {
                    scoped_spin_lock lock(lst->spin);
                    if (!lst->tasks.empty()) {
                        all_empty = false;
                        break;
                    }
                }

                if (all_empty)
                    return;
            }
        }
    }

    void join() noexcept { thread.join(); }

    std::jthread         thread;
    vector<task_list*>   task_lists;
    ring_buffer<task, 8> tasks;
    bool                 is_terminating = false;
};

struct task_manager_parameters
{
    int thread_number           = 3;
    int simple_task_list_number = 1;
    int multi_task_list_number  = 1;
};

class task_manager
{
public:
    vector<worker>    workers;
    vector<task_list> task_lists;

    task_manager() noexcept = default;

    task_manager(task_manager&& params)      = delete;
    task_manager(const task_manager& params) = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager& operator=(task_manager&&) = delete;

    status init(task_manager_parameters& params) noexcept;

    // creating task lists and spawning workers
    status start() noexcept;
    void   finalize() noexcept;
};

inline status task_manager::init(task_manager_parameters& params) noexcept
{
    irt_return_if_fail(params.thread_number > 0, status::gui_not_enough_memory);
    irt_return_if_fail(params.simple_task_list_number > 0,
                       status::gui_not_enough_memory);

    workers.resize(params.thread_number);
    task_lists.resize(params.simple_task_list_number);

    return status::success;
}

inline status task_manager::start() noexcept
{
    i32 worker_index = 0;
    for (auto& lst : task_lists) {
        lst.worker = &workers[worker_index++];
        worker_index %= workers.ssize();
    }

    for (auto& e : workers)
        e.start();

    return status::success;
}

inline void task_manager::finalize() noexcept
{
    for (auto& e : workers)
        e.terminate();

    for (auto& e : workers)
        e.join();

    task_lists.clear();
    workers.clear();
}

/*****************************************************************************
 *
 * Implementation
 *
 ****************************************************************************/

inline spin_lock::spin_lock() noexcept { flag.clear(); }

inline bool spin_lock::try_lock() noexcept
{
    return !flag.test_and_set(std::memory_order_acquire);
}

inline void spin_lock::lock() noexcept
{
    for (size_t i = 0; !try_lock(); ++i)
        if (i % 100 == 0)
            std::this_thread::yield();
}

inline void spin_lock::unlock() noexcept
{
    flag.clear(std::memory_order_release);
}

inline scoped_spin_lock::scoped_spin_lock(spin_lock& spin_) noexcept
  : spin(spin_)
{
    spin.lock();
}

inline scoped_spin_lock::~scoped_spin_lock() noexcept { spin.unlock(); }

} // namespace irt

#endif