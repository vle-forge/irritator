// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <array>
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

    std::array<task, 256> task_buffer;
    ring_buffer<task>     tasks;

    std::mutex              mutex;
    std::condition_variable cond;

    //! number of task since task_list constructor
    std::atomic<int> task_number = 0;

    //! task_list priority (-127 better than 127).
    i8   priority       = 127;
    bool is_terminating = false;

    task_list() noexcept;
    ~task_list() noexcept = default;
    task_list(task_list&& other) noexcept;

    task_list(const task_list& other)            = delete;
    task_list& operator=(const task_list& other) = delete;
    task_list& operator=(task_list&& other)      = delete;

    void add(task_function function, void* parameter) noexcept;
    void submit() noexcept;
    void wait() noexcept;
    void terminate() noexcept;
};

constexpr int task_manager_worker_max     = 8;
constexpr int task_manager_list_max       = 4;
constexpr int task_manager_list_by_worker = 2;

struct worker
{
    worker() noexcept = default;
    ~worker() noexcept;
    worker(worker&& other) noexcept;

    worker(const worker& other)            = delete;
    worker& operator=(const worker& other) = delete;
    worker& operator=(worker&& other)      = delete;

    void start() noexcept;
    void run() noexcept;
    void join() noexcept;

    std::thread                                           thread;
    small_vector<task_list*, task_manager_list_by_worker> task_lists;
    bool                                                  is_running = false;
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
    small_vector<task_list, task_manager_list_max> task_lists;
    small_vector<worker, task_manager_worker_max>  workers;

    task_manager(const task_manager_parameters& params) noexcept;

    task_manager(task_manager&& params)          = delete;
    task_manager(const task_manager& params)     = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager& operator=(task_manager&&)      = delete;

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
{
}

/*
    task_list
 */

inline task_list::task_list() noexcept
  : tasks{ task_buffer.data(), length(task_buffer) }
  , task_number{ 0 }
{
}

inline task_list::task_list(task_list&& other) noexcept
{
    priority       = other.priority;
    is_terminating = other.is_terminating;
}

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

inline worker::worker(worker&& other) noexcept
  : thread{ std::move(other.thread) }
  , task_lists{ other.task_lists }
  , is_running{ std::move(other.is_running) }
{
    other.thread = std::thread();
    other.task_lists.clear();
    other.is_running = false;
}

inline worker::~worker() noexcept
{
    try {
        if (is_running && thread.joinable())
            thread.join();
    } catch (...) {
    }
}

inline void worker::start() noexcept
{
    thread     = std::thread{ &worker::run, this };
    is_running = true;
}

inline void worker::run() noexcept
{
    std::sort(task_lists.begin(),
              task_lists.end(),
              [](const task_list* left, const task_list* right) -> bool {
                  return left->priority < right->priority;
              });

    bool                running = true;
    std::array<task, 8> task_buffer;
    ring_buffer<task>   tasks(task_buffer.data(), length(task_buffer));

    while (running) {
        while (!tasks.empty()) {
            auto task = tasks.front();
            tasks.dequeue();
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
                    auto task = lst->tasks.front();
                    lst->tasks.dequeue();
                    tasks.enqueue(task);
                    --lst->task_number;
                }
            } else {
                for (;;) {
                    {
                        std::unique_lock<std::mutex> lock{ lst->mutex };
                        while (!tasks.full() && !lst->tasks.empty()) {
                            auto task = lst->tasks.front();
                            lst->tasks.dequeue();
                            tasks.enqueue(task);
                            --lst->task_number;
                        }
                    }

                    if (tasks.empty())
                        break;

                    while (!tasks.empty()) {
                        auto task = tasks.front();
                        tasks.dequeue();
                        task.function(task.parameter);
                    }
                }
            }
        }
    }
}

inline void worker::join() noexcept
{
    try {
        if (is_running && thread.joinable()) {
            thread.join();
        }
        is_running = false;
    } catch (...) {
    }
}

/*
    task_manager
 */

inline task_manager::task_manager(
  const task_manager_parameters& params) noexcept
{
    for (auto i = 0; i < params.thread_number; ++i)
        workers.emplace_back();

    for (auto i = 0; i < params.simple_task_list_number; ++i)
        task_lists.emplace_back();
}

inline status task_manager::start() noexcept
{
    for (auto i = 0, e = workers.ssize(); i != e; ++i)
        workers[i].start();

    return status::success;
}

inline void task_manager::finalize() noexcept
{
    for (auto i = 0, e = task_lists.ssize(); i != e; ++i)
        task_lists[i].terminate();

    for (auto i = 0, e = workers.ssize(); i != e; ++i)
        workers[i].join();

    task_lists.clear();
    workers.clear();
}

} // namespace irt

#endif