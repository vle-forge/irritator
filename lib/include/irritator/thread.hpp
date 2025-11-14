// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_THREAD_HPP

#include <exception>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
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

    std::chrono::steady_clock::time_point start_time;

    worker_stats() noexcept
      : start_time(std::chrono::steady_clock::now())
    {}
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
    std::atomic_uint* completion_counter = nullptr;

    job() noexcept = default;

    job(task t, std::atomic_uint* cnt) noexcept
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
    enum class state : int { idle, starting, running, stopping, stopped };

    worker_base() noexcept = default;
    virtual ~worker_base() noexcept;

    worker_base(const worker_base&)            = delete;
    worker_base& operator=(const worker_base&) = delete;
    worker_base(worker_base&&) noexcept;
    worker_base& operator=(worker_base&&) = delete;

    void start() noexcept;
    void shutdown() noexcept;
    void join() noexcept;

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
    std::thread                                thread_;
    std::atomic<std::chrono::nanoseconds::rep> exec_time_{ 0 };
};

/**
 * Worker dédié pour les tâches ordonnées
 */
class ordered_worker final : public worker_base<ordered_worker>
{
public:
    ordered_worker() noexcept = default;

    void set_task_list(ordered_task_list* tl) noexcept { task_list_ = tl; }

    void run() noexcept;

private:
    ordered_task_list* task_list_ = nullptr;
};

/**
 * Worker générique pour les tâches non ordonnées (avec work stealing)
 */
class unordered_worker final : public worker_base<unordered_worker>
{
public:
    unordered_worker() noexcept = default;

    unordered_worker(unordered_worker&&) noexcept;

    void add_task_list(unordered_task_list* tl) noexcept
    {
        task_lists_.push_back(tl);
    }

    void run() noexcept;
    void wake() noexcept;
    void wait() noexcept;

private:
    std::vector<unordered_task_list*> task_lists_;
    std::atomic_flag                  wake_flag_ = ATOMIC_FLAG_INIT;
};

/**
 * Ordered task list with SPSC (Single Producer Single Consumer) behaviour.
 */
class ordered_task_list
{
public:
    explicit ordered_task_list() noexcept = default;
    ~ordered_task_list() noexcept         = default;

    ordered_task_list(const ordered_task_list&)            = delete;
    ordered_task_list& operator=(const ordered_task_list&) = delete;
    ordered_task_list(ordered_task_list&&)                 = delete;
    ordered_task_list& operator=(ordered_task_list&&)      = delete;

    // API for user

    void set_stats(worker_stats* stats) noexcept { stats_ = stats; }

    template<typename Fn>
    bool try_add(Fn&& fn) noexcept;

    template<typename Fn>
    void add(Fn&& fn) noexcept;

    void notify_worker() noexcept;
    void wait_completion() noexcept;
    void shutdown() noexcept;

    // API for worker

    bool try_pop(task& t) noexcept;
    void worker_wait() noexcept;
    void worker_notify_idle() noexcept;

    u32 tasks_submitted() const noexcept
    {
        return tasks_submitted_.load(std::memory_order_acquire);
    }

    u32 tasks_completed() const noexcept
    {
        return tasks_completed_.load(std::memory_order_acquire);
    }

    u32 pending_tasks() const noexcept
    {
        return tasks_submitted_.load(std::memory_order_acquire) -
               tasks_completed_.load(std::memory_order_acquire);
    }

    static constexpr sz BUFFER_SIZE = 256;

    worker_stats* stats_ = nullptr;

    circular_buffer<task, BUFFER_SIZE> buffer_;

    std::atomic<u32> tasks_submitted_{ 0 };
    std::atomic<u32> tasks_completed_{ 0 };

    std::atomic<bool> shutdown_{ false };
    std::atomic<bool> worker_active_{ true };

    std::atomic_flag wake_worker_   = ATOMIC_FLAG_INIT;
    std::atomic_flag wake_producer_ = ATOMIC_FLAG_INIT;
};

/**
 * Unordered task list with work-stealing task pool behavior.
 */
class unordered_task_list
{
public:
    explicit unordered_task_list() noexcept;
    ~unordered_task_list() noexcept = default;

    unordered_task_list(const unordered_task_list&)            = delete;
    unordered_task_list& operator=(const unordered_task_list&) = delete;
    unordered_task_list(unordered_task_list&&) noexcept;
    unordered_task_list& operator=(unordered_task_list&&) noexcept = delete;

    void set_stats(worker_stats* stats) noexcept;
    void set_workers(std::span<unordered_worker> workers) noexcept;

    /** Add a task in the current batch. */
    template<typename Fn>
    bool add(Fn&& fn) noexcept;

    /** Submit the current batch to the workers. */
    void submit() noexcept;

    /** Wait the current batch completion. */
    void wait_completion() noexcept;

    /** Shutdown the system. */
    void shutdown() noexcept;

    /** Worker interface. */
    bool try_steal_task(job& j) noexcept;

    u32 tasks_completed() const noexcept
    {
        return completed_tasks_.load(std::memory_order_acquire);
    }

    sz pending_tasks() const noexcept { return pending_tasks_.size(); }

private:
    enum class phase : int {
        accepting, //<! Accepts @c add().
        executing, //<! Work in progress.
        shutting_down
    };

    static constexpr sz MAX_BATCH_SIZE = 4096;

    worker_stats*               stats_ = nullptr;
    std::span<unordered_worker> workers_;

    std::vector<task> pending_tasks_;

    std::atomic<phase> current_phase_{ phase::accepting };
    std::atomic<u32>   next_task_index_{ 0 };
    std::atomic<u32>   completed_tasks_{ 0 };
    u32                batch_size_{ 0 };
};

/**
 * Gestionnaire principal des threads et tâches
 */
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
    std::vector<unordered_worker>                     unordered_workers_;
};

/*****************************************************************************
 *
 * Implementation
 *
 ****************************************************************************/

template<typename Fn>
inline bool ordered_task_list::try_add(Fn&& fn) noexcept
{
    if (shutdown_.load(std::memory_order_acquire)) {
        return false;
    }

    task t(std::forward<Fn>(fn));

    if (!buffer_.try_push(std::move(t))) {
        return false;
    }

    tasks_submitted_.fetch_add(1, std::memory_order_release);
    stats_->num_submitted_tasks.fetch_add(1, std::memory_order_relaxed);

    return true;
}

template<typename Fn>
inline void ordered_task_list::add(Fn&& fn) noexcept
{
    // Stratégie backoff exponentiel
    sz           backoff     = 1;
    constexpr sz MAX_BACKOFF = 1024;

    while (!try_add(std::forward<Fn>(fn))) {
        // Réveiller le worker pour qu'il consomme
        notify_worker();

        // Backoff exponentiel
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

    notify_worker();
}

inline void ordered_task_list::notify_worker() noexcept
{
    wake_worker_.test_and_set(std::memory_order_release);
    wake_worker_.notify_one();
}

inline void ordered_task_list::wait_completion() noexcept
{
    const u32 expected = tasks_submitted_.load(std::memory_order_acquire);

    while (tasks_completed_.load(std::memory_order_acquire) < expected) {
        notify_worker();

        if (tasks_completed_.load(std::memory_order_acquire) >= expected) {
            break;
        }

        wake_producer_.wait(false, std::memory_order_acquire);
        wake_producer_.clear(std::memory_order_relaxed);
    }

    stats_->num_executed_tasks.store(
      tasks_completed_.load(std::memory_order_relaxed),
      std::memory_order_relaxed);
}

inline void ordered_task_list::shutdown() noexcept
{
    shutdown_.store(true, std::memory_order_release);
    notify_worker();
}

inline bool ordered_task_list::try_pop(task& t) noexcept
{
    return buffer_.try_pop(t);
}

inline void ordered_task_list::worker_wait() noexcept
{
    worker_active_.store(false, std::memory_order_release);

    if (shutdown_.load(std::memory_order_acquire))
        return;

    if (!buffer_.empty()) {
        worker_active_.store(true, std::memory_order_relaxed);
        return;
    }

    wake_worker_.wait(false, std::memory_order_acquire);
    wake_worker_.clear(std::memory_order_relaxed);
    worker_active_.store(true, std::memory_order_relaxed);
}

inline void ordered_task_list::worker_notify_idle() noexcept
{
    wake_producer_.test_and_set(std::memory_order_release);
    wake_producer_.notify_one();
}

/*****************************************************************************
 *
 * Implémentation unordered_task_list
 *
 ****************************************************************************/

inline unordered_task_list::unordered_task_list() noexcept
{
    pending_tasks_.reserve(MAX_BATCH_SIZE);
}

inline unordered_task_list::unordered_task_list(unordered_task_list&&) noexcept
  : next_task_index_{ 0 }
  , completed_tasks_{ 0 }
{
    std::terminate();
}

inline void unordered_task_list::set_stats(worker_stats* stats) noexcept
{
    stats_ = stats;
}

inline void unordered_task_list::set_workers(
  std::span<unordered_worker> workers) noexcept
{
    workers_ = workers;
}

template<typename Fn>
inline bool unordered_task_list::add(Fn&& fn) noexcept
{
    if (current_phase_.load(std::memory_order_acquire) != phase::accepting) {
        return false;
    }

    if (pending_tasks_.size() >= MAX_BATCH_SIZE) {
        return false;
    }

    try {
        pending_tasks_.emplace_back(std::forward<Fn>(fn));
        stats_->num_submitted_tasks.fetch_add(1, std::memory_order_relaxed);
        return true;
    } catch (...) {
        return false;
    }
}

inline void unordered_task_list::submit() noexcept
{
    phase expected = phase::accepting;
    if (!current_phase_.compare_exchange_strong(
          expected, phase::executing, std::memory_order_acq_rel)) {
        return; // Déjà en exécution
    }

    batch_size_ = static_cast<u32>(pending_tasks_.size());
    next_task_index_.store(0, std::memory_order_release);
    completed_tasks_.store(0, std::memory_order_release);

    for (auto& w : workers_)
        w.wake();
}

inline void unordered_task_list::wait_completion() noexcept
{
    if (current_phase_.load(std::memory_order_acquire) != phase::executing) {
        return;
    }

    // Attente active avec yield
    while (completed_tasks_.load(std::memory_order_acquire) < batch_size_) {
        std::this_thread::yield();
    }

    // Réinitialiser pour le prochain batch
    pending_tasks_.clear();
    current_phase_.store(phase::accepting, std::memory_order_release);

    stats_->num_executed_tasks.fetch_add(batch_size_,
                                         std::memory_order_relaxed);
}

inline void unordered_task_list::shutdown() noexcept
{
    current_phase_.store(phase::shutting_down, std::memory_order_release);
}

inline bool unordered_task_list::try_steal_task(job& j) noexcept
{
    if (current_phase_.load(std::memory_order_acquire) != phase::executing) {
        return false;
    }

    const u32 index = next_task_index_.fetch_add(1, std::memory_order_acq_rel);

    if (index >= batch_size_) {
        return false;
    }

    j.function           = std::move(pending_tasks_[index]);
    j.completion_counter = &completed_tasks_;

    return true;
}

/*****************************************************************************
 *
 * Implémentation worker_base
 *
 ****************************************************************************/

template<typename Derived>
inline worker_base<Derived>::worker_base(worker_base&& other) noexcept
  : state_(other.state_.load(std::memory_order_acquire))
  , thread_(std::move(other.thread_))
  , exec_time_(other.exec_time_.load(std::memory_order_relaxed))
{
    other.state_.store(state::stopped, std::memory_order_release);
}

template<typename Derived>
inline worker_base<Derived>::~worker_base() noexcept
{
    if (current_state() != state::stopped) {
        shutdown();
        join();
    }
}

template<typename Derived>
inline void worker_base<Derived>::start() noexcept
{
    state expected = state::idle;
    if (!state_.compare_exchange_strong(
          expected, state::starting, std::memory_order_acq_rel)) {
        return;
    }

    try {
        thread_ = std::thread([this]() {
            state_.store(state::running, std::memory_order_release);
            static_cast<Derived*>(this)->run();
            state_.store(state::stopped, std::memory_order_release);
        });
    } catch (...) {
        state_.store(state::stopped, std::memory_order_release);
    }
}

template<typename Derived>
inline void worker_base<Derived>::shutdown() noexcept
{
    state_.store(state::stopping, std::memory_order_release);
}

template<typename Derived>
inline void worker_base<Derived>::join() noexcept
{
    if (thread_.joinable()) {
        try {
            thread_.join();
        } catch (...) {
        }
    }
}

/*****************************************************************************
 *
 * Implémentation ordered_worker
 *
 ****************************************************************************/

inline void ordered_worker::run() noexcept
{
    debug::ensure(task_list_ != nullptr);

    while (state_.load(std::memory_order_acquire) == state::running) {
        task t;
        bool found_work = false;

        // Traiter toutes les tâches disponibles
        while (task_list_->try_pop(t)) {
            found_work = true;

            const auto start = std::chrono::steady_clock::now();
            t();
            const auto elapsed = std::chrono::steady_clock::now() - start;
            add_execution_time(elapsed);

            task_list_->tasks_completed_.fetch_add(1,
                                                   std::memory_order_release);
        }

        if (found_work) {
            // Signaler au producteur qu'on est idle
            task_list_->worker_notify_idle();
        }

        // Attendre du travail
        task_list_->worker_wait();
    }
}

/*****************************************************************************
 *
 * Implémentation unordered_worker
 *
 ****************************************************************************/

inline unordered_worker::unordered_worker(unordered_worker&&) noexcept
  : wake_flag_{}
{
    std::terminate();
}

inline void unordered_worker::run() noexcept
{
    while (state_.load(std::memory_order_acquire) == state::running) {
        bool found_work = false;

        // Work stealing sur toutes les listes
        for (auto* tl : task_lists_) {
            job j;

            while (tl->try_steal_task(j)) {
                found_work = true;

                const auto start = std::chrono::steady_clock::now();
                j.function();
                const auto elapsed = std::chrono::steady_clock::now() - start;
                add_execution_time(elapsed);

                if (j.completion_counter)
                    j.completion_counter->fetch_add(1,
                                                    std::memory_order_release);
            }
        }

        if (!found_work) {
            // Attendre réveil
            wait();
        }
    }
}

inline void unordered_worker::wake() noexcept
{
    wake_flag_.test_and_set(std::memory_order_release);
    wake_flag_.notify_one();
}

inline void unordered_worker::wait() noexcept
{
    wake_flag_.wait(false, std::memory_order_acquire);
    wake_flag_.clear(std::memory_order_relaxed);
}

/*****************************************************************************
 *
 * Implémentation task_manager
 *
 ****************************************************************************/

template<std::size_t O, std::size_t U>
inline task_manager<O, U>::task_manager() noexcept
{
    const sz num_hw_threads = std::thread::hardware_concurrency();
    const sz num_unordered  = ordered_worker_number < num_hw_threads
                                ? num_hw_threads - ordered_worker_number
                                : 1;

    for (sz i = 0; i < ordered_lists_.size(); ++i)
        ordered_lists_[i].set_stats(&ordered_stats_[i]);

    for (sz i = 0; i < unordered_lists_.size(); ++i)
        unordered_lists_[i].set_stats(&unordered_stats_[i]);

    for (sz i = 0; i < ordered_lists_.size(); ++i)
        ordered_workers_[i].set_task_list(&ordered_lists_[i]);

    unordered_workers_.reserve(num_unordered);
    for (sz i = 0; i < num_unordered; ++i) {
        unordered_workers_.emplace_back();

        for (auto& list : unordered_lists_) {
            unordered_workers_[i].add_task_list(&list);
        }
    }

    for (auto& list : unordered_lists_) {
        list.set_workers(unordered_workers_);
    }
}

template<std::size_t O, std::size_t U>
inline task_manager<O, U>::~task_manager() noexcept
{
    if (state_.load(std::memory_order_acquire) != manager_state::stopped) {
        shutdown();
    }
}

template<std::size_t O, std::size_t U>
inline void task_manager<O, U>::start() noexcept
{
    manager_state expected = manager_state::constructed;
    if (!state_.compare_exchange_strong(
          expected, manager_state::running, std::memory_order_acq_rel)) {
        return;
    }

    // Démarrer tous les workers
    for (auto& w : ordered_workers_) {
        w.start();
    }

    for (auto& w : unordered_workers_) {
        w.start();
    }

    // Attendre que tous soient démarrés
    bool all_running = false;
    while (!all_running) {
        all_running = true;

        for (const auto& w : ordered_workers_) {
            if (w.current_state() != ordered_worker::state::running) {
                all_running = false;
                break;
            }
        }

        if (all_running) {
            for (const auto& w : unordered_workers_) {
                if (w.current_state() != unordered_worker::state::running) {
                    all_running = false;
                    break;
                }
            }
        }

        if (!all_running) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

template<std::size_t O, std::size_t U>
inline void task_manager<O, U>::shutdown() noexcept
{
    manager_state expected = manager_state::running;
    if (!state_.compare_exchange_strong(
          expected, manager_state::shutting_down, std::memory_order_acq_rel)) {
        return;
    }

    // Arrêter toutes les listes
    for (auto& list : ordered_lists_) {
        list.shutdown();
    }

    for (auto& list : unordered_lists_) {
        list.shutdown();
    }

    // Arrêter tous les workers
    for (auto& w : ordered_workers_) {
        w.shutdown();
    }

    for (auto& w : unordered_workers_) {
        w.shutdown();
        w.wake();
    }

    // Attendre la terminaison
    for (auto& w : ordered_workers_) {
        w.join();
    }

    for (auto& w : unordered_workers_) {
        w.join();
    }

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

} // namespace irt

#endif
