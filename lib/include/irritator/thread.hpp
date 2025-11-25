// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

namespace irt {

/** Provides SPSC Lock-Free.
 *
 * @tparam T A simple type (trivially object or nothrow).
 * @tparam Capacity A fixed size buffer.
 */
template<typename T, std::size_t Capacity>
class circular_buffer;

class ordered_task_list;
class unordered_task_list;
struct worker_stats;
class ordered_worker;
class unordered_worker;

template<std::size_t ordered, std::size_t unordered>
class task_manager;

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
 * The @c circular_buffer class provides SPSC Lock-Free.
 *
 * @tparam T A simple type (trivially object or nothrow).
 * @tparam Capacity A fixed size buffer.
 */
template<typename T, std::size_t Capacity>
class circular_buffer
{
public:
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be power of 2");
    static_assert(Capacity > 1, "Capacity must be > 1");

    constexpr circular_buffer() noexcept = default;

    constexpr ~circular_buffer() noexcept;

    circular_buffer(const circular_buffer&) noexcept            = delete;
    circular_buffer& operator=(const circular_buffer&) noexcept = delete;

    bool try_push(const T& value) noexcept;
    bool try_push(T&& value) noexcept;
    bool try_pop(T& value) noexcept;
    bool try_pop() noexcept;

    bool empty() const noexcept;

private:
    static constexpr sz m_mask = Capacity - 1;

    alignas(64) std::atomic<sz> m_head{ 0 };
    alignas(64) std::atomic<sz> m_tail{ 0 };
    alignas(64) T m_buffer[Capacity]{};
};

/**
 * The @c task is a function with a static buffer allowing 6 sizeof(u64) data in
 * lambda capture.
 *
 * Simplicity key to scalability:
 * – @c task has well defined input and output.
 * – Independent stateless, no stalls, always completes.
 * – @c task added to @c main_task_list.
 * – @c task fully independent
 */
using task =
  small_function<(sizeof(void*) * 2) + (sizeof(u64) * 6), void(void)>;

/** Stores thread-safe statistics per worker. */
struct alignas(64) worker_stats {
    std::atomic<u32> num_submitted_tasks{ 0 };
    std::atomic<u32> num_executed_tasks{ 0 };

    std::chrono::steady_clock::time_point start_time{
        std::chrono::steady_clock::now()
    };
};

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
    task              function;
    std::atomic<u32>* completion_counter = nullptr;

    job() noexcept = default;

    job(task t, std::atomic<u32>* cnt) noexcept
      : function(std::move(t))
      , completion_counter(cnt)
    {}
};

/**
 * Worker de base avec gestion du cycle de vie
 */
template<typename Derived>
class worker_base
{
public:
    enum class state : u8 { idle, starting, running, stopping, stopped };

    worker_base() noexcept = default;

    virtual ~worker_base() noexcept
    {
        if (current_state() != state::stopped) {
            shutdown();
            join();
        }
    }

    worker_base(const worker_base&)            = delete;
    worker_base& operator=(const worker_base&) = delete;
    worker_base(worker_base&&) noexcept        = delete;
    worker_base& operator=(worker_base&&)      = delete;

    void start() noexcept
    {
        state expected = state::idle;
        if (!state_.compare_exchange_strong(expected, state::starting))
            return;

        try {
            thread_ = std::thread([this] {
                state_.store(state::running, std::memory_order_release);
                static_cast<Derived*>(this)->run();
                state_.store(state::stopped, std::memory_order_release);
            });
        } catch (...) {
            state_.store(state::stopped, std::memory_order_release);
        }
    }

    void shutdown() noexcept
    {
        state_.store(state::stopping, std::memory_order_release);
    }

    void join() noexcept
    {
        if (thread_.joinable()) {
            try {
                thread_.join();
            } catch (...) {
            }
        }
    }

    state current_state() const noexcept
    {
        return state_.load(std::memory_order_acquire);
    }

    std::chrono::nanoseconds::rep execution_time() const noexcept
    {
        return exec_time_.load(std::memory_order_relaxed);
    }

protected:
    void add_execution_time(std::chrono::nanoseconds delta) noexcept
    {
        exec_time_.fetch_add(delta.count(), std::memory_order_relaxed);
    }

    std::atomic<state> state_{ state::idle };

private:
    std::thread thread_;

    std::atomic<std::chrono::nanoseconds::rep> exec_time_{ 0 };
};

class ordered_task_list
{
public:
    explicit ordered_task_list() noexcept = default;
    ~ordered_task_list() noexcept         = default;

    ordered_task_list(const ordered_task_list&)            = delete;
    ordered_task_list& operator=(const ordered_task_list&) = delete;
    ordered_task_list(ordered_task_list&&)                 = delete;
    ordered_task_list& operator=(ordered_task_list&&)      = delete;

    void set_stats(worker_stats* stats) noexcept { stats_ = stats; }

    template<typename Fn>
    bool try_add(Fn&& fn) noexcept
    {
        if (shutdown_.load(std::memory_order_acquire))
            return false;

        task t(std::forward<Fn>(fn));
        if (!buffer_.try_push(std::move(t)))
            return false;

        tasks_submitted_.fetch_add(1, std::memory_order_release);
        if (stats_)
            stats_->num_submitted_tasks.fetch_add(1, std::memory_order_relaxed);

        // Réveille le worker si endormi
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cv_worker_.notify_one();
        }
        return true;
    }

    template<typename Fn>
    void add(Fn&& fn) noexcept
    {
        // Boucle avec backoff si le buffer est plein
        sz           backoff     = 1;
        constexpr sz MAX_BACKOFF = 1024;

        while (!try_add(std::forward<Fn>(fn))) {
            notify_worker();
            for (sz i = 0; i < backoff; ++i) {
#if (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
                __builtin_ia32_pause();
#elif defined(__aarch64__)
                asm volatile("yield");
#else
                std::this_thread::yield();
#endif
            }
            backoff = std::min(backoff * 2, MAX_BACKOFF);
        }
    }

    void notify_worker() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        cv_worker_.notify_one();
    }

    void wait_completion() noexcept
    {
        const u32 expected = tasks_submitted_.load(std::memory_order_acquire);
        std::unique_lock<std::mutex> lock(mtx_);
        cv_producer_.wait(lock, [&] {
            return tasks_completed_.load(std::memory_order_acquire) >= expected;
        });

        if (stats_) {
            stats_->num_executed_tasks.store(
              tasks_completed_.load(std::memory_order_relaxed),
              std::memory_order_relaxed);
        }
    }

    void shutdown() noexcept
    {
        shutdown_.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cv_worker_.notify_all();
            cv_producer_.notify_all();
        }
    }

    // API worker
    bool try_pop(task& t) noexcept { return buffer_.try_pop(t); }

    void worker_wait() noexcept
    {
        std::unique_lock<std::mutex> lock(mtx_);
        worker_active_.store(false, std::memory_order_release);

        cv_worker_.wait(lock, [&] {
            return shutdown_.load(std::memory_order_acquire) ||
                   !buffer_.empty();
        });

        worker_active_.store(true, std::memory_order_relaxed);
    }

    void worker_notify_idle() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        cv_producer_.notify_one();
    }

    u32 tasks_submitted() const noexcept { return tasks_submitted_.load(); }
    u32 tasks_completed() const noexcept { return tasks_completed_.load(); }
    u32 pending_tasks() const noexcept
    {
        return tasks_submitted_.load() - tasks_completed_.load();
    }

    static constexpr sz BUFFER_SIZE = 256;

    worker_stats*                      stats_ = nullptr;
    circular_buffer<task, BUFFER_SIZE> buffer_;

    std::atomic<u32> tasks_submitted_{ 0 };
    std::atomic<u32> tasks_completed_{ 0 };

    std::atomic<bool> shutdown_{ false };
    std::atomic<bool> worker_active_{ true };

private:
    mutable std::mutex      mtx_;
    std::condition_variable cv_worker_;
    std::condition_variable cv_producer_;
};

class ordered_worker final : public worker_base<ordered_worker>
{
public:
    ordered_worker() = default;
    void set_task_list(ordered_task_list* tl) noexcept { task_list_ = tl; }

    void run() noexcept
    {
        debug::ensure(task_list_ != nullptr);

        while (state_.load(std::memory_order_acquire) == state::running) {
            task t;

            while (task_list_->try_pop(t)) {
                const auto start = std::chrono::steady_clock::now();
                try {
                    t();
                } catch (...) {
                }
                const auto elapsed = std::chrono::steady_clock::now() - start;
                add_execution_time(elapsed);

                task_list_->tasks_completed_.fetch_add(
                  1, std::memory_order_release);
            }

            task_list_->worker_notify_idle();
            task_list_->worker_wait();
        }
    }

private:
    ordered_task_list* task_list_ = nullptr;
};

class unordered_task_list
{
public:
    explicit unordered_task_list() noexcept
    {
        pending_tasks_.reserve(MAX_BATCH_SIZE);
    }
    ~unordered_task_list() noexcept = default;

    unordered_task_list(const unordered_task_list&)                = delete;
    unordered_task_list& operator=(const unordered_task_list&)     = delete;
    unordered_task_list(unordered_task_list&&) noexcept            = delete;
    unordered_task_list& operator=(unordered_task_list&&) noexcept = delete;

    void set_stats(worker_stats* stats) noexcept { stats_ = stats; }
    void set_workers(std::span<unordered_worker> workers) noexcept;

    /** Add a task in the current batch. */
    template<typename Fn>
    bool add(Fn&& fn) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (phase_ != phase::accepting)
            return false;
        if (pending_tasks_.size() >= MAX_BATCH_SIZE)
            return false;

        try {
            pending_tasks_.emplace_back(std::forward<Fn>(fn));
            if (stats_)
                stats_->num_submitted_tasks.fetch_add(
                  1, std::memory_order_relaxed);
            return true;
        } catch (...) {
            return false;
        }
    }

    /** Submit the current batch to the workers. */
    void submit() noexcept;

    /** Wait the current batch completion. */
    void wait_completion() noexcept
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_producer_.wait(lock, [&] {
            return phase_ != phase::executing ||
                   completed_tasks_ >= batch_size_;
        });

        // Si batch terminé, repasser en accepting et nettoyer
        if (phase_ == phase::executing && completed_tasks_ >= batch_size_) {
            phase_ = phase::accepting;
            pending_tasks_.clear();
            if (stats_) {
                stats_->num_executed_tasks.fetch_add(batch_size_,
                                                     std::memory_order_relaxed);
            }
        }
    }

    /** Shutdown the system. */
    void shutdown() noexcept;

    /** Worker interface. */
    bool try_steal_task(job& j) noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (phase_ != phase::executing)
            return false;
        if (next_task_index_ >= batch_size_)
            return false;

        j.function           = std::move(pending_tasks_[next_task_index_++]);
        j.completion_counter = &completed_tasks_atomic_proxy_;
        return true;
    }

    // Proxy atomic pour la complétion: on incrémente via notify_task_done
    void notify_task_done() noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        ++completed_tasks_;
        if (completed_tasks_ >= batch_size_) {
            cv_producer_.notify_one();
        }
    }

    // Accesseurs utilisés par le worker
    u32 tasks_completed() const noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return completed_tasks_;
    }

    sz pending_tasks() const noexcept
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (phase_ == phase::executing)
            return static_cast<sz>(batch_size_);
        return static_cast<sz>(pending_tasks_.size());
    }

private:
    friend class unordered_worker;

    enum class phase : int { accepting, executing, shutting_down };

    static constexpr sz MAX_BATCH_SIZE = 4096;

    worker_stats*               stats_ = nullptr;
    std::span<unordered_worker> workers_;

    std::vector<task> pending_tasks_;

    // Sync
    mutable std::mutex      mtx_;
    std::condition_variable cv_workers_;
    std::condition_variable cv_producer_;

    // État de batch
    phase phase_{ phase::accepting };
    u32   next_task_index_{ 0 };
    u32   completed_tasks_{ 0 };
    u32   batch_size_{ 0 };

    // Pour compatibiliser job::completion_counter (si utilisé): on fait pointer
    // sur une atomic<u32> factice qui n’est jamais lue directement; le worker
    // appelle notify_task_done().
    std::atomic<u32> completed_tasks_atomic_proxy_{ 0 };
};

class unordered_worker final : public worker_base<unordered_worker>
{
public:
    unordered_worker() noexcept = default;

    unordered_worker(unordered_worker&&) noexcept = delete;

    void add_task_list(unordered_task_list* tl) noexcept
    {
        task_lists_.push_back(tl);
    }

    void run() noexcept
    {
        using namespace std::chrono_literals;

        while (state_.load(std::memory_order_acquire) == state::running) {
            bool found_work    = false;
            bool any_executing = false;

            for (auto* tl : task_lists_) {
                {
                    // Lire l’état sous mutex
                    std::lock_guard<std::mutex> lock(tl->mtx_);
                    any_executing |=
                      (tl->phase_ == unordered_task_list::phase::executing) &&
                      (tl->completed_tasks_ < tl->batch_size_);
                }

                job j;
                while (tl->try_steal_task(j)) {
                    found_work = true;

                    const auto start = std::chrono::steady_clock::now();
                    try {
                        j.function();
                    } catch (...) { /* log */
                    }
                    const auto elapsed =
                      std::chrono::steady_clock::now() - start;
                    add_execution_time(elapsed);

                    // Signaler complétion (ne pas écrire directement sur proxy)
                    tl->notify_task_done();
                }
            }

            if (!found_work) {
                if (any_executing) {
                    // micro-yield en présence d’un batch
                    std::this_thread::yield();
                } else {
                    // Attendre un submit ou shutdown via une condition
                    // quelconque On choisit une liste pour attendre; toutes
                    // notifient cv_workers_ au submit/shutdown
                    if (!task_lists_.empty()) {
                        auto*                        tl = task_lists_.front();
                        std::unique_lock<std::mutex> lock(tl->mtx_);
                        tl->cv_workers_.wait_for(lock, 5ms, [&] {
                            return tl->phase_ ==
                                     unordered_task_list::phase::executing ||
                                   tl->phase_ == unordered_task_list::phase::
                                                   shutting_down ||
                                   state_.load(std::memory_order_acquire) !=
                                     state::running;
                        });
                    } else {
                        std::this_thread::sleep_for(5ms);
                    }
                }
            }
        }
    }

    void wake() noexcept
    {
        // Le wake réel est piloté par les listes via cv_workers_, mais on
        // conserve cette API pour compatibilité avec task_manager::shutdown()
        // et unordered_task_list::submit(). Aucun état local à notifier ici;
        // les listes notifient leurs cv_workers_.
    }

private:
    std::vector<unordered_task_list*> task_lists_;
};

template<std::size_t OrderedList = 4, std::size_t UnorderedList = 1>
class task_manager
{
public:
    static_assert(OrderedList > 0, "Need at least one ordered task list");
    static_assert(UnorderedList > 0, "Need at least one unordered task list");

    constexpr static inline sz ordered_list_number   = OrderedList;
    constexpr static inline sz ordered_worker_number = OrderedList;
    constexpr static inline sz unordered_list_number = UnorderedList;

    task_manager() noexcept;
    ~task_manager() noexcept;

    task_manager(const task_manager&)            = delete;
    task_manager& operator=(const task_manager&) = delete;
    task_manager(task_manager&&)                 = delete;
    task_manager& operator=(task_manager&&)      = delete;

    void start() noexcept;
    void shutdown() noexcept;

    // Accès aux listes de tâches
    ordered_task_list& get_ordered_list(sz index) noexcept
    {
        debug::ensure(index < OrderedList);
        return ordered_lists_[index];
    }

    unordered_task_list& get_unordered_list(sz index) noexcept
    {
        debug::ensure(index < UnorderedList);
        return unordered_lists_[index];
    }

    const worker_stats& get_ordered_stats(sz index) const noexcept
    {
        debug::ensure(index < OrderedList);
        return ordered_stats_[index];
    }

    const worker_stats& get_unordered_stats(sz index) const noexcept
    {
        debug::ensure(index < UnorderedList);
        return unordered_stats_[index];
    }

    auto ordered_workers_size() const noexcept
    {
        return ordered_workers_.size();
    }

    auto unordered_workers_size() const noexcept
    {
        return unordered_workers_.size();
    }

    std::span<const worker_stats> ordered_stats() const noexcept;
    std::span<const worker_stats> unordered_stats() const noexcept;

    std::span<const ordered_task_list>   ordered_lists() const noexcept;
    std::span<const unordered_task_list> unordered_lists() const noexcept;

    std::span<const ordered_worker>   ordered_workers() const noexcept;
    std::span<const unordered_worker> unordered_workers() const noexcept;

private:
    enum class manager_state : int {
        constructed,
        running,
        shutting_down,
        stopped
    };

    std::atomic<manager_state> state_{ manager_state::constructed };

    std::array<worker_stats, ordered_worker_number> ordered_stats_;
    std::array<worker_stats, unordered_list_number> unordered_stats_;

    std::array<ordered_task_list, ordered_list_number>     ordered_lists_;
    std::array<unordered_task_list, unordered_list_number> unordered_lists_;

    std::array<ordered_worker, ordered_worker_number> ordered_workers_;
    vector<unordered_worker>                          unordered_workers_;
};

//
// circular buffer impl
//

template<typename T, std::size_t Capacity>
constexpr circular_buffer<T, Capacity>::~circular_buffer() noexcept
{
    while (try_pop())
        ;
}

template<typename T, std::size_t Capacity>
bool circular_buffer<T, Capacity>::try_push(const T& value) noexcept
{
    const sz head      = m_head.load(std::memory_order_relaxed);
    const sz next_head = (head + 1) & m_mask;

    if (next_head == m_tail.load(std::memory_order_acquire))
        return false; // Buffer is full.

    std::construct_at(std::addressof(m_buffer[head]), value);
    m_head.store(next_head, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool circular_buffer<T, Capacity>::try_push(T&& value) noexcept
{
    const sz head      = m_head.load(std::memory_order_relaxed);
    const sz next_head = (head + 1) & m_mask;

    if (next_head == m_tail.load(std::memory_order_acquire))
        return false; // Buffer is full.

    std::construct_at(std::addressof(m_buffer[head]), std::move(value));
    m_head.store(next_head, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool circular_buffer<T, Capacity>::try_pop(T& value) noexcept
{
    const sz tail = m_tail.load(std::memory_order_relaxed);

    if (tail == m_head.load(std::memory_order_acquire)) {
        return false; // Buffer is empty.
    }

    value = std::move(m_buffer[tail]);
    std::destroy_at(std::addressof(m_buffer[tail]));

    m_tail.store((tail + 1) & m_mask, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool circular_buffer<T, Capacity>::try_pop() noexcept
{
    const sz tail = m_tail.load(std::memory_order_relaxed);

    if (tail == m_head.load(std::memory_order_acquire))
        return false;

    std::destroy_at(std::addressof(m_buffer[tail]));
    m_tail.store((tail + 1) & m_mask, std::memory_order_release);
    return true;
}

template<typename T, std::size_t Capacity>
bool circular_buffer<T, Capacity>::empty() const noexcept
{
    return m_tail.load(std::memory_order_acquire) ==
           m_head.load(std::memory_order_acquire);
}

// unordered_task_list

inline void unordered_task_list::set_workers(
  std::span<unordered_worker> workers) noexcept
{
    workers_ = workers;
}

inline void unordered_task_list::submit() noexcept
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (phase_ != phase::accepting)
            return;

        batch_size_      = static_cast<u32>(pending_tasks_.size());
        next_task_index_ = 0;
        completed_tasks_ = 0;
        phase_           = phase::executing;
    }

    // Réveiller tous les workers
    for (auto& w : workers_)
        w.wake();

    cv_workers_.notify_all();
}

inline void unordered_task_list::shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        phase_ = phase::shutting_down;
    }

    cv_workers_.notify_all();
    cv_producer_.notify_all();
    for (auto& w : workers_)
        w.wake();
}

// task_manager

//
// task_manager impl
//

template<std::size_t O, std::size_t U>
inline task_manager<O, U>::task_manager() noexcept
  : unordered_workers_(
      ordered_worker_number < std::thread::hardware_concurrency()
        ? std::thread::hardware_concurrency() - ordered_worker_number
        : 1)
{
    for (sz i = 0; i < ordered_lists_.size(); ++i)
        ordered_lists_[i].set_stats(&ordered_stats_[i]);

    for (sz i = 0; i < unordered_lists_.size(); ++i)
        unordered_lists_[i].set_stats(&unordered_stats_[i]);

    for (sz i = 0; i < ordered_lists_.size(); ++i)
        ordered_workers_[i].set_task_list(&ordered_lists_[i]);

    for (sz i = 0; i < unordered_workers_.size(); ++i)
        for (auto& list : unordered_lists_)
            unordered_workers_[i].add_task_list(&list);

    std::span<unordered_worker> workers_span(unordered_workers_.data(),
                                             unordered_workers_.size());
    for (auto& list : unordered_lists_) {
        list.set_workers(workers_span);
    }
}

template<std::size_t O, std::size_t U>
inline task_manager<O, U>::~task_manager() noexcept
{
    if (state_.load(std::memory_order_acquire) != manager_state::stopped)
        shutdown();
}

template<std::size_t O, std::size_t U>
inline void task_manager<O, U>::start() noexcept
{
    manager_state expected = manager_state::constructed;
    if (!state_.compare_exchange_strong(expected, manager_state::running))
        return;

    for (auto& w : ordered_workers_)
        w.start();
    for (auto& w : unordered_workers_)
        w.start();
}

template<std::size_t O, std::size_t U>
inline void task_manager<O, U>::shutdown() noexcept
{
    manager_state expected = manager_state::running;
    if (!state_.compare_exchange_strong(expected, manager_state::shutting_down))
        return;

    for (auto& list : ordered_lists_)
        list.shutdown();
    for (auto& list : unordered_lists_)
        list.shutdown();

    for (auto& w : ordered_workers_)
        w.shutdown();
    for (auto& w : unordered_workers_)
        w.shutdown();

    for (auto& w : ordered_workers_)
        w.join();
    for (auto& w : unordered_workers_)
        w.join();

    state_.store(manager_state::stopped, std::memory_order_release);
}

template<std::size_t O, std::size_t U>
inline std::span<const worker_stats> task_manager<O, U>::ordered_stats()
  const noexcept
{
    return { ordered_stats_.data(), ordered_stats_.size() };
}

template<std::size_t O, std::size_t U>
inline std::span<const worker_stats> task_manager<O, U>::unordered_stats()
  const noexcept
{
    return { unordered_stats_.data(), unordered_stats_.size() };
}

template<std::size_t O, std::size_t U>
inline std::span<const ordered_task_list> task_manager<O, U>::ordered_lists()
  const noexcept
{
    return { ordered_lists_.data(), ordered_lists_.size() };
}

template<std::size_t O, std::size_t U>
inline std::span<const unordered_task_list>
task_manager<O, U>::unordered_lists() const noexcept
{
    return { unordered_lists_.data(), unordered_lists_.size() };
}

template<std::size_t O, std::size_t U>
inline std::span<const ordered_worker> task_manager<O, U>::ordered_workers()
  const noexcept
{
    return { ordered_workers_.data(), ordered_workers_.size() };
}

template<std::size_t O, std::size_t U>
inline std::span<const unordered_worker> task_manager<O, U>::unordered_workers()
  const noexcept
{
    return { unordered_workers_.cbegin(), unordered_workers_.size() };
}

} // namespace irt

#endif
