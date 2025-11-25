// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace irt {

using task = std::function<void()>;

/* ========================== ORDERED QUEUE ========================== */

class ordered_task_list
{
public:
    void add(task t)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stopping_)
                return;
            queue_.push(std::move(t));
        }
        cv_.notify_one();
    }

    bool pop(task& out)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty())
            return false;
        out = std::move(queue_.front());
        queue_.pop();
        if (queue_.empty()) {
            cv_.notify_all();
        }
        return true;
    }

    void wait_empty()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&] { return stopping_ || queue_.empty(); });
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopping_ = true;
        }
        cv_.notify_all();
    }

    bool stopping() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return stopping_;
    }

private:
    std::queue<task>        queue_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    bool                    stopping_{ false };
};

class ordered_worker
{
public:
    explicit ordered_worker(ordered_task_list& list)
      : list_(&list)
    {}

    void start()
    {
        thr_ = std::thread([this] {
            task t;
            while (list_->pop(t)) {
                try {
                    t();
                } catch (...) {
                }
            }
        });
    }

    void join()
    {
        if (thr_.joinable())
            thr_.join();
    }

private:
    ordered_task_list* list_;
    std::thread        thr_;
};

class unordered_task_list
{
public:
    unordered_task_list() { pending_.reserve(1024); }

    void add(task t)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_ || phase_ != phase::accepting)
            return;
        pending_.push_back(std::move(t));
    }

    void submit()
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

    bool try_steal(task& out)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stopping_ || phase_ != phase::executing)
            return false;
        if (next_index_ >= batch_size_)
            return false;
        out = std::move(pending_[next_index_++]);
        return true;
    }

    void notify_done()
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

    void wait_completion()
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

private:
    enum class phase : uint8_t { accepting, executing, shutting_down };

    std::vector<task> pending_;
    u32               batch_size_{ 0 };
    u32               next_index_{ 0 };
    u32               completed_{ 0 };

    mutable std::mutex      mtx_;
    std::condition_variable cv_producer_;

    phase phase_{ phase::accepting };
    bool  stopping_{ false };
};

class unordered_worker
{
public:
    explicit unordered_worker(std::vector<unordered_task_list*>& lists)
      : lists_(lists)
    {}

    void start()
    {
        thr_ = std::thread([this] {
            while (true) {
                bool found = false;
                for (auto* l : lists_) {
                    task t;
                    while (l->try_steal(t)) {
                        found = true;
                        try {
                            t();
                        } catch (...) {
                        }
                        l->notify_done();
                    }
                }
                if (!found) {
                    if (std::all_of(lists_.begin(), lists_.end(), [](auto* l) {
                            return l->stopping();
                        }))
                        break;
                    std::this_thread::yield();
                }
            }
        });
    }

    void join()
    {
        if (thr_.joinable())
            thr_.join();
    }

private:
    std::vector<unordered_task_list*> lists_;
    std::thread                       thr_;
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
      , unordered_lists_(unordered_count)
    {
        if (unordered_worker_count == 0)
            unordered_worker_count = 1;

        for (auto& l : ordered_lists_)
            ordered_workers_.emplace_back(l);

        unordered_lists_ptrs_.reserve(unordered_count);
        for (auto& l : unordered_lists_)
            unordered_lists_ptrs_.push_back(&l);

        unordered_workers_.reserve(unordered_worker_count);
        for (size_t i = 0; i < unordered_worker_count; i++)
            unordered_workers_.emplace_back(unordered_lists_ptrs_);
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

    ordered_task_list&   ordered(size_t i) { return ordered_lists_[i]; }
    unordered_task_list& unordered(size_t i) { return unordered_lists_[i]; }

private:
    std::vector<ordered_task_list> ordered_lists_;
    std::vector<ordered_worker>    ordered_workers_;

    std::vector<unordered_task_list>  unordered_lists_;
    std::vector<unordered_task_list*> unordered_lists_ptrs_;
    std::vector<unordered_worker>     unordered_workers_;
};

} // namespace irt

#endif