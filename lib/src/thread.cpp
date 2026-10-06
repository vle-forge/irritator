// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/thread.hpp>

#include <algorithm>
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
  , m_tasks_submitted(other.m_tasks_submitted.load())
  , m_tasks_completed(other.m_tasks_completed.load())
  , m_tasks_running(other.m_tasks_running)
  , m_stopping(other.m_stopping)
{}

bool ordered_task_list::pop(task& out) noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    worker_cv.wait(lock, [&] { return m_stopping || !m_queue.empty(); });
    if (m_queue.empty()) // only possible if stopping: the queue is drained
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
    const auto done = m_tasks_completed.fetch_add(1) + 1;
    if (m_tasks_running == 0 && m_queue.empty() &&
        done == m_tasks_submitted.load())
        producer_cv.notify_all();
}

void ordered_task_list::wait_empty() noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    producer_cv.wait(lock, [&] {
        return m_stopping ||
               (m_queue.empty() && m_tasks_running == 0 &&
                m_tasks_completed.load() == m_tasks_submitted.load());
    });
}

void ordered_task_list::shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    worker_cv.notify_all();
    producer_cv.notify_all(); // wait_empty() has `m_stopping` in its predicate
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
  , m_tasks_completed(other.m_tasks_completed.load())
  , m_execution_time_ns(other.m_execution_time_ns.load())
  , m_initialized(other.m_initialized.load())
{}

void ordered_worker::start() noexcept
{
    if (m_thread.joinable())
        return;

    m_thread = std::thread([this] {
        journal_scope scope;
        m_initialized.store(true, std::memory_order_release);

        while (true) {
            task t; // destroyed at each iteration: captures do not outlive it
            if (!m_list->pop(t))
                break;

            const auto start = std::chrono::steady_clock::now();
            try {
                t();
            } catch (...) {
            }
            const auto
              ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();

            // Statistics first: once wait_empty() returns they are up to date.
            m_execution_time_ns.fetch_add(static_cast<u64>(ns),
                                          std::memory_order_relaxed);
            m_tasks_completed.fetch_add(1, std::memory_order_relaxed);
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
  , m_incoming(std::move(other.m_incoming))
  , m_tasks_submitted(other.m_tasks_submitted.load())
  , m_tasks_completed(other.m_tasks_completed.load())
  , m_batch_size(other.m_batch_size)
  , m_next_index(other.m_next_index)
  , m_completed(other.m_completed)
  , m_signal(other.m_signal)
  , m_phase(other.m_phase)
  , m_stopping(other.m_stopping)
{}

void unordered_task_list::submit() noexcept
{
    bool started = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_phase != phase::accepting)
            return;

        m_batch_size = static_cast<u32>(m_pending.size());
        m_next_index = 0;
        m_completed  = 0;
        started      = m_batch_size != 0;
        m_phase      = started ? phase::executing : phase::accepting;
    }

    if (started && m_signal)
        m_signal->notify();
}

bool unordered_task_list::try_steal(task& out) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping || m_phase != phase::executing)
        return false;
    if (m_next_index >= m_batch_size)
        return false;
    out = std::move(m_pending[m_next_index++]);
    return true;
}

void unordered_task_list::notify_done() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tasks_completed.fetch_add(1);

    if (m_phase != phase::executing)
        return;

    if (++m_completed >= m_batch_size) {
        m_pending.clear();
        m_pending.swap(m_incoming); // tasks added meanwhile: the next batch
        m_phase = phase::accepting;
        m_producer_cv.notify_all();
    }
}

void unordered_task_list::wait_completion() noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_producer_cv.wait(
      lock, [&] { return m_stopping || m_phase != phase::executing; });
}

void unordered_task_list::shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        m_phase    = phase::shutting_down;
    }
    m_producer_cv.notify_all();

    if (m_signal)
        m_signal->notify();
}

bool unordered_task_list::stopping() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping;
}

unordered_worker::unordered_worker(std::span<unordered_task_list> lists,
                                   worker_signal& signal) noexcept
  : m_lists(lists)
  , m_signal(&signal)
{}

unordered_worker::unordered_worker(unordered_worker&& other) noexcept
  : m_lists(other.m_lists)
  , m_signal(other.m_signal)
  , m_thread(std::move(other.m_thread))
  , m_tasks_completed(other.m_tasks_completed.load())
  , m_execution_time_ns(other.m_execution_time_ns.load())
  , m_initialized(other.m_initialized.load())
{}

void unordered_worker::start() noexcept
{
    if (m_thread.joinable())
        return;

    m_thread = std::thread([this] {
        journal_scope scope;
        m_initialized.store(true, std::memory_order_release);

        while (true) {
            // The epoch is read BEFORE the scan: an event published after this
            // line changes it, and wait() below then returns immediately.
            const u32 seen  = m_signal->load();
            bool      found = false;

            for (auto& l : m_lists) {
                task t;
                while (l.try_steal(t)) {
                    found = true;

                    const auto start = std::chrono::steady_clock::now();
                    try {
                        t();
                    } catch (...) {
                    }
                    const auto
                      ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();

                    m_execution_time_ns.fetch_add(static_cast<u64>(ns),
                                                  std::memory_order_relaxed);
                    m_tasks_completed.fetch_add(1, std::memory_order_relaxed);
                    l.notify_done();
                }
            }

            if (!found) {
                if (std::all_of(m_lists.begin(), m_lists.end(),
                                [](const auto& l) { return l.stopping(); }))
                    break;

                m_signal->wait(seen);
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
  , m_unordered_workers(unordered_count == 0          ? 0
                        : unordered_worker_count == 0 ? 1
                                                      : unordered_worker_count,
                        reserve_tag)
{
    for (auto& l : m_ordered_lists)
        m_ordered_workers.emplace_back(l);

    for (auto& l : m_unordered_lists)
        l.m_signal = &m_signal;

    const auto span = std::span<unordered_task_list>(m_unordered_lists.data(),
                                                     m_unordered_lists.size());

    for (sz i = 0, e = m_unordered_workers.capacity(); i < e; ++i)
        m_unordered_workers.emplace_back(span, m_signal);
}

task_manager::~task_manager() noexcept { shutdown(); }

void task_manager::start() noexcept
{
    for (auto& w : m_ordered_workers)
        w.start();

    for (auto& w : m_unordered_workers)
        w.start();

    // Wait until every thread is running (journal registered).
    for (auto& w : m_ordered_workers)
        while (!w.initialized())
            std::this_thread::yield();

    for (auto& w : m_unordered_workers)
        while (!w.initialized())
            std::this_thread::yield();
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

} // namespace irt