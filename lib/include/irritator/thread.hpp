// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace irt {

using task = std::function<void()>;

class ordered_task_list;
class unordered_task_list;
class ordered_worker;
class unordered_worker;
class task_manager;

/* ========================== ORDERED QUEUE ========================== */

class ordered_task_list
{
public:
    void add(task t) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stopping_)
                return;
            tasks_submitted_ += 1;
            queue_.enqueue(std::move(t));
        }
        cv_.notify_one();
    }

    bool pop(task& out) noexcept
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty())
            return false;
        out = std::move(*queue_.head());
        queue_.pop_head();
        tasks_running_ += 1;
        return true;
    }

    void notify_done()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        --tasks_running_;
        ++tasks_completed_;
        if (queue_.empty() && tasks_running_ == 0 &&
            tasks_completed_ == tasks_submitted_) {
            cv_empty_.notify_all();
        }
    }

    void wait_empty() noexcept
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_empty_.wait(lock, [&] {
            return stopping_ || (queue_.empty() && tasks_running_ == 0 &&
                                 tasks_completed_ == tasks_submitted_);
        });
    }

    void shutdown() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopping_ = true;
        }
        cv_.notify_all();
    }

    bool stopping() const noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return stopping_;
    }

    u64 tasks_submitted() const noexcept { return tasks_submitted_; }
    u64 tasks_completed() const noexcept { return tasks_completed_; }

private:
    ring_buffer<task> queue_{ 256 };

    mutable std::mutex      mtx_;
    std::condition_variable cv_;       // for workers (work available)
    std::condition_variable cv_empty_; // for producers (fully drained)

    u64 tasks_submitted_{ 0 };
    u64 tasks_completed_{ 0 };
    u64 tasks_running_{ 0 };

    bool stopping_{ false };
};

class ordered_worker
{
public:
    explicit ordered_worker(ordered_task_list& list) noexcept
      : list_(&list)
    {}

    ordered_worker(ordered_worker&& other) noexcept
      : list_(other.list_)
      , thr_(std::move(other.thr_))
      , tasks_completed_(other.tasks_completed_)
      , execution_time_(other.execution_time_)
    {}

    void start() noexcept
    {
        thr_ = std::thread([this] {
            task t;
            while (list_->pop(t)) {
                try {
                    tasks_completed_ += 1;
                    const auto start = std::chrono::steady_clock::now();
                    t();
                    execution_time_ =
                      (std::chrono::steady_clock::now() - start).count();
                } catch (...) {
                }
                list_->notify_done();
            }
        });
    }

    void join() noexcept
    {
        if (thr_.joinable())
            thr_.join();
    }

    u64 tasks_completed() const noexcept { return tasks_completed_; }

private:
    ordered_task_list* list_;
    std::thread        thr_;

    u64                           tasks_completed_{ 0 };
    std::chrono::nanoseconds::rep execution_time_;
};

class unordered_task_list
{
public:
    unordered_task_list() noexcept { pending_.reserve(1024); }

    void add(task t) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_ || phase_ != phase::accepting)
            return;
        pending_.push_back(std::move(t));
        tasks_submitted_ += 1;
    }

    void submit() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stopping_ || phase_ != phase::accepting)
                return;
            batch_size_ = static_cast<u32>(pending_.size());
            next_index_ = 0;
            completed_  = 0;
            phase_ = (batch_size_ == 0) ? phase::accepting : phase::executing;
        }
    }

    bool try_steal(task& out) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_ || phase_ != phase::executing)
            return false;
        if (next_index_ >= batch_size_)
            return false;
        out = std::move(pending_[next_index_++]);
        tasks_completed_ += 1;
        return true;
    }

    void notify_done() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (phase_ != phase::executing)
            return;
        ++completed_;
        if (completed_ >= batch_size_) {
            phase_ = phase::accepting;
            pending_.clear();
            cv_producer_.notify_all();
        }
    }

    void wait_completion() noexcept
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_producer_.wait(lock, [&] {
            return stopping_ || phase_ == phase::accepting ||
                   (phase_ == phase::executing && completed_ >= batch_size_);
        });
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopping_ = true;
            phase_    = phase::shutting_down;
        }
        cv_producer_.notify_all();
    }

    bool stopping() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return stopping_;
    }

    u64 tasks_submitted() const noexcept { return tasks_submitted_; }
    u64 tasks_completed() const noexcept { return tasks_completed_; }

private:
    enum class phase : uint8_t { accepting, executing, shutting_down };

    vector<task> pending_;

    u64 tasks_submitted_{ 0 };
    u64 tasks_completed_{ 0 };
    u32 batch_size_{ 0 };
    u32 next_index_{ 0 };
    u32 completed_{ 0 };

    mutable std::mutex      mtx_;
    std::condition_variable cv_producer_;

    phase phase_{ phase::accepting };
    bool  stopping_{ false };
};

class unordered_worker
{
public:
    explicit unordered_worker(std::span<unordered_task_list> lists) noexcept
      : lists_(lists)
    {}

    unordered_worker(unordered_worker&& other) noexcept
      : lists_(other.lists_)
      , thr_(std::move(other.thr_))
      , tasks_completed_(other.tasks_completed_)
      , execution_time_(other.execution_time_)
    {}

    void start() noexcept
    {
        thr_ = std::thread([this] {
            while (true) {
                bool found = false;
                for (auto& l : lists_) {
                    task t;
                    while (l.try_steal(t)) {
                        found = true;
                        try {
                            const auto start = std::chrono::steady_clock::now();
                            t();
                            execution_time_ =
                              (std::chrono::steady_clock::now() - start)
                                .count();
                            tasks_completed_ += 1;
                        } catch (...) {
                        }
                        l.notify_done();
                    }
                }
                if (!found) {
                    if (std::all_of(lists_.begin(),
                                    lists_.end(),
                                    [](const auto& l) { return l.stopping(); }))
                        break;
                    std::this_thread::yield();
                }
            }
        });
    }

    void join() noexcept
    {
        if (thr_.joinable())
            thr_.join();
    }

    u64 tasks_completed() const noexcept { return tasks_completed_; }

private:
    std::span<unordered_task_list> lists_;
    std::thread                    thr_;

    u64                           tasks_completed_{ 0 };
    std::chrono::nanoseconds::rep execution_time_;
};

/* ========================== TASK MANAGER ========================== */

class task_manager
{
public:
    task_manager(
      size_t ordered_count,
      size_t unordered_count,
      size_t unordered_worker_count = std::thread::hardware_concurrency())
      : ordered_lists_(ordered_count)
      , ordered_workers_(ordered_count, reserve_tag)
      , unordered_lists_(unordered_count)
      , unordered_workers_(unordered_worker_count == 0 ? 1
                                                       : unordered_worker_count,
                           reserve_tag)
    {
        for (auto& l : ordered_lists_)
            ordered_workers_.emplace_back(l);

        const auto span = std::span<unordered_task_list>(
          unordered_lists_.data(), unordered_lists_.size());

        for (sz i = 0, e = unordered_workers_.capacity(); i < e; ++i)
            unordered_workers_.emplace_back(span);
    }

    void start()
    {
        for (auto& w : ordered_workers_)
            w.start();
        for (auto& w : unordered_workers_)
            w.start();
    }

    void shutdown()
    {
        for (auto& l : ordered_lists_)
            l.shutdown();
        for (auto& l : unordered_lists_)
            l.shutdown();
        for (auto& w : ordered_workers_)
            w.join();
        for (auto& w : unordered_workers_)
            w.join();
    }

    ordered_task_list& ordered(std::integral auto i)
    {
        return ordered_lists_[i];
    }
    unordered_task_list& unordered(std::integral auto i)
    {
        return unordered_lists_[i];
    }

    size_t ordered_size() const noexcept { return ordered_lists_.size(); }
    size_t unordered_size() const noexcept { return unordered_lists_.size(); }
    size_t wordered_size() const noexcept { return ordered_workers_.size(); }
    size_t wunordered_size() const noexcept
    {
        return unordered_workers_.size();
    }

    u64 wordered_tasks_completed(std::integral auto i) const noexcept
    {
        return ordered_workers_[i].tasks_completed();
    }

    u64 wunordered_tasks_completed(std::integral auto i) const noexcept
    {
        return unordered_workers_[i].tasks_completed();
    }

    std::chrono::nanoseconds::rep wordered_execution_time(
      std::integral auto i) const noexcept
    {
        return ordered_workers_[i].tasks_completed();
    }

    std::chrono::nanoseconds::rep wunordered_execution_time(
      std::integral auto i) const noexcept
    {
        return unordered_workers_[i].tasks_completed();
    }

private:
    vector<ordered_task_list> ordered_lists_;
    vector<ordered_worker>    ordered_workers_;

    vector<unordered_task_list> unordered_lists_;
    vector<unordered_worker>    unordered_workers_;
};

} // namespace irt

#endif