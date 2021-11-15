// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
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
    small_vector<task, 256> submit_task;
    ring_buffer<task, 256>  tasks;

    std::mutex              mutex;
    std::condition_variable cond;

    //! number of task since task_list constructor
    std::atomic<int> task_number = 0;

    //! task_list priority (-127 better than 127).
    i8   priority       = 127;
    bool is_terminating = false;

    task_list() noexcept;
    task_list(const task_list& other) = delete;
    task_list& operator=(const task_list& other) = delete;

    void add(task_function function, void* parameter) noexcept;
    void submit() noexcept;
    void wait() noexcept;
    void terminate() noexcept;
};

struct worker
{
    worker() noexcept           = default;
    worker(const worker& other) = delete;
    worker& operator=(const worker& other) = delete;

    void start() noexcept;
    void run() noexcept;
    void join() noexcept;

    std::jthread       thread;
    vector<task_list*> task_lists;
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

//
// spin_lock
//

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

/*
    task
 */

constexpr task::task(task_function function_, void* parameter_) noexcept
  : function(function_)
  , parameter(parameter_)
{}

/*
    task_list
 */

inline task_list::task_list() noexcept
  : task_number{ 0 }
{}

inline void task_list::add(task_function function, void* parameter) noexcept
{
    irt_assert(is_terminating == false);
    submit_task.emplace_back(function, parameter);
}

inline void task_list::submit() noexcept
{
    irt_assert(is_terminating == false);

    {
        std::unique_lock lock{ mutex };

        for (auto& t : submit_task)
            tasks.emplace_enqueue(t);

        task_number += submit_task.ssize();
        submit_task.clear();
    }

    cond.notify_all();
}

inline void task_list::wait() noexcept
{
    irt_assert(is_terminating == false);

    using namespace std::chrono_literals;

    while (task_number > 0) {
        cond.notify_all();
        std::this_thread::sleep_for(10ms);
    }
}

inline void task_list::terminate() noexcept
{
    is_terminating = true;
    cond.notify_all();
}

/*
    worker
 */

inline void worker::start() noexcept
{
    thread = std::jthread{ &worker::run, this };
}

inline void worker::run() noexcept
{
    std::sort(task_lists.begin(),
              task_lists.end(),
              [](const task_list* left, const task_list* right) -> bool {
                  return left->priority < right->priority;
              });

    bool                 running = true;
    ring_buffer<task, 8> tasks;

    while (running) {
        while (!tasks.empty()) {
            auto task = tasks.dequeue();
            task.function(task.parameter);
        }

        running = false;

        for (auto& lst : task_lists) {
            if (!lst->is_terminating) {
                running = true;
                std::unique_lock<std::mutex> lock{ lst->mutex };
                lst->cond.wait(lock, [&] {
                    return lst->is_terminating || lst->task_number > 0;
                });

                while (!tasks.full() && !lst->tasks.empty()) {
                    tasks.enqueue(lst->tasks.dequeue());
                    --lst->task_number;
                }
            } else {
                for (;;) {
                    {
                        std::unique_lock<std::mutex> lock{ lst->mutex };
                        while (!tasks.full() && !lst->tasks.empty()) {
                            tasks.enqueue(lst->tasks.dequeue());
                            --lst->task_number;
                        }
                    }

                    if (tasks.empty())
                        break;

                    while (!tasks.empty()) {
                        auto task = tasks.dequeue();
                        task.function(task.parameter);
                    }
                }
            }
        }
    }
}

inline void worker::join() noexcept { thread.join(); }

/*
    task_manager
 */

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
    for (auto& tl : task_lists)
        tl.terminate();

    for (auto& e : workers)
        e.join();

    task_lists.clear();
    workers.clear();
}

} // namespace irt

#endif