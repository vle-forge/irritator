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

class spin_lock;
class scoped_spin_lock;

struct worker;

struct task;
struct task_list;
class task_manager;

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

// Simplicity key to scalability
// – Job has well defined input and output
// – Independent stateless, no stalls, always completes
// – Jobs added to job lists
// – Multiple job lists
// – Job lists fully independent
// – Simple synchronization of jobs within list through "signal" and
//   "synchronize" tokens

using task_function = void (*)(void*) noexcept;

struct task
{
    constexpr task() noexcept = default;
    constexpr task(task_function function_, void* parameter_) noexcept;

    task_function function  = nullptr;
    void*         parameter = nullptr;
};

//! @brief Simple task list access by only one thread
struct task_list
{
    ring_buffer<task, 256> tasks;
    spin_lock              spin;

    i32 task_number = 0;   // number of task since task_list constructor
    i8  priority    = 127; // task_list priority (-127 better than 127).

    task_list() noexcept = default;

    void add(task_function function, void* parameter) noexcept;
    void wait() noexcept;
};

struct worker
{
    void start() noexcept;
    void terminate() noexcept;

    worker() noexcept = default;

    worker(const worker& other) = delete;
    worker& operator=(const worker& other) = delete;

    void run() noexcept;
    void join() noexcept;

    std::jthread         thread;
    vector<task_list*>   task_lists;
    ring_buffer<task, 8> tasks;
    bool                 is_terminating = false;
};

struct task_manager_parameters
{
    i32 thread_number           = 3;
    i32 simple_task_list_number = 1;
    i32 multi_task_list_number  = 1;
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

/*****************************************************************************
 *
 * Implementation
 *
 ****************************************************************************/

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

constexpr task::task(task_function function_, void* parameter_) noexcept
  : function(function_)
  , parameter(parameter_)
{}

inline void task_list::add(task_function function, void* parameter) noexcept
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

inline void task_list::wait() noexcept
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

inline void worker::start() noexcept
{
    thread = std::jthread{ &worker::run, this };
}

inline void worker::terminate() noexcept { is_terminating = true; }

inline void worker::run() noexcept
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

inline void worker::join() noexcept { thread.join(); }

} // namespace irt

#endif