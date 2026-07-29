// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/thread.hpp>

#include <chrono>

namespace irt {

u64 get_time_since_epoch() noexcept
{
    const auto timepoint = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
             timepoint.time_since_epoch())
      .count();
}

void log_history::collect() noexcept
{
    m_history.write([](vector<log_record>& dst) {
        journal_registry::instance().for_each([&](thread_journal& j) {
            log_record rec;
            while (j.pop(rec))
                dst.push_back(std::move(rec));
        });
    });
}

ordered_task_list::ordered_task_list(ordered_task_list&& other) noexcept
  : m_queue(std::move(other.m_queue))
  , m_tasks_submitted(other.m_tasks_submitted)
  , m_tasks_completed(other.m_tasks_completed)
  , m_tasks_running(other.m_tasks_running)
  , m_stopping(other.m_stopping)
{}

bool ordered_task_list::pop(task& out) noexcept
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

void ordered_task_list::notify_done() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    --m_tasks_running;
    ++m_tasks_completed;
    if (m_queue.empty() && m_tasks_running == 0 &&
        m_tasks_completed == m_tasks_submitted) {
        producer_cv.notify_all();
    }
}

void ordered_task_list::wait_empty() noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    producer_cv.wait(lock, [&] {
        return m_stopping || (m_queue.empty() && m_tasks_running == 0 &&
                              m_tasks_completed == m_tasks_submitted);
    });
}

void ordered_task_list::shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    worker_cv.notify_all();
}

bool ordered_task_list::stopping() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping;
}

ordered_worker::ordered_worker(ordered_task_list& list) noexcept
  : m_list(&list)
{}

ordered_worker::ordered_worker(ordered_worker&& other) noexcept
  : m_list(other.m_list)
  , m_thread(std::move(other.m_thread))
  , m_tasks_completed(other.m_tasks_completed)
  , m_execution_time(other.m_execution_time)
{}

void ordered_worker::start() noexcept
{
    m_thread = std::thread([this] {
        journal_scope scope;
        m_initialized = true;

        task t;
        while (m_list->pop(t)) {
            try {
                const auto start = std::chrono::steady_clock::now();

                t();

                const auto duration_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);

                m_execution_time += static_cast<u64>(duration_ms.count());
                m_tasks_completed += 1;
            } catch (...) {
            }
            m_list->notify_done();
        }
    });
}

void ordered_worker::join() noexcept
{
    if (m_thread.joinable())
        m_thread.join();
}

unordered_task_list::unordered_task_list() noexcept { m_pending.reserve(1024); }

unordered_task_list::unordered_task_list(unordered_task_list&& other) noexcept
  : m_pending(std::move(other.m_pending))
  , m_tasks_submitted(other.m_tasks_submitted)
  , m_tasks_completed(other.m_tasks_completed)
  , m_batch_size(other.m_batch_size)
  , m_next_index(other.m_next_index)
  , m_completed(other.m_completed)
  , m_phase(other.m_phase)
  , m_stopping(other.m_stopping)
{}

void unordered_task_list::submit() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping || m_phase != phase::accepting)
        return;
    m_batch_size = static_cast<u32>(m_pending.size());
    m_next_index = 0;
    m_completed  = 0;
    m_phase      = (m_batch_size == 0) ? phase::accepting : phase::executing;
}

bool unordered_task_list::try_steal(task& out) noexcept
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

void unordered_task_list::notify_done() noexcept
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

void unordered_task_list::wait_completion() noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_producer_cv.wait(lock, [&] {
        return m_stopping || m_phase == phase::accepting ||
               (m_phase == phase::executing && m_completed >= m_batch_size);
    });
}

void unordered_task_list::shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        m_phase    = phase::shutting_down;
    }
    m_producer_cv.notify_all();
}

bool unordered_task_list::stopping() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping;
}

unordered_worker::unordered_worker(
  std::span<unordered_task_list> lists) noexcept
  : m_lists(lists)
{}

unordered_worker::unordered_worker(unordered_worker&& other) noexcept
  : m_lists(other.m_lists)
  , m_thread(std::move(other.m_thread))
  , m_tasks_completed(other.m_tasks_completed)
  , m_execution_time(other.m_execution_time)
{}

void unordered_worker::start() noexcept
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

                        const auto duration_ms =
                          std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start);

                        m_execution_time +=
                          static_cast<u64>(duration_ms.count());
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

void unordered_worker::join() noexcept
{
    if (m_thread.joinable())
        m_thread.join();
}

task_manager::task_manager(size_t ordered_count,
                           size_t unordered_count,
                           size_t unordered_worker_count) noexcept
  : m_ordered_lists(ordered_count)
  , m_ordered_workers(ordered_count, reserve_tag)
  , m_unordered_lists(unordered_count)
  , m_unordered_workers(unordered_worker_count == 0 ? 1
                                                    : unordered_worker_count,
                        reserve_tag)
{
    for (auto& l : m_ordered_lists)
        m_ordered_workers.emplace_back(l);

    const auto span = std::span<unordered_task_list>(m_unordered_lists.data(),
                                                     m_unordered_lists.size());

    for (sz i = 0, e = m_unordered_workers.capacity(); i < e; ++i)
        m_unordered_workers.emplace_back(span);
}

void task_manager::start() noexcept
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

void task_manager::shutdown() noexcept
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

}