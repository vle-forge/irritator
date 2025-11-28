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

using task = lambda_function<void(void), sizeof(void*) * 4>;

class ordered_task_list;
class unordered_task_list;
class ordered_worker;
class unordered_worker;
class task_manager;

class ordered_task_list
{
public:
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

    void start() noexcept
    {
        m_thread = std::thread([this] {
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

private:
    ordered_task_list* m_list;
    std::thread        m_thread;

    u64                           m_tasks_completed{ 0 };
    std::chrono::nanoseconds::rep m_execution_time;
};

class unordered_task_list
{
public:
    unordered_task_list() noexcept { m_pending.reserve(1024); }

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
                    std::this_thread::yield();
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

private:
    std::span<unordered_task_list> m_lists;
    std::thread                    m_thread;

    u64                           m_tasks_completed{ 0 };
    std::chrono::nanoseconds::rep m_execution_time;
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
