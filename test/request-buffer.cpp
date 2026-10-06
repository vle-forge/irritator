// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <stop_token>

#include <boost/ut.hpp>

using namespace std::chrono_literals;

namespace irt {

using rb_int = request_buffer<int>;
using state  = rb_int::state;

template<typename Pred>
bool wait_until(Pred&& pred, std::chrono::milliseconds timeout = 5s)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::yield();
    }
    return true;
}

// Simple pool: the destructor drains the queue, so that no task (and no
// captured shared_ptr) is silently dropped.
class thread_pool
{
public:
    explicit thread_pool(unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            m_threads.emplace_back([this](std::stop_token st) { run(st); });
    }

    ~thread_pool()
    {
        for (auto& t : m_threads)
            t.request_stop();
        m_cv.notify_all();
        // the jthread destructors join
    }

    void submit(std::function<void()> task)
    {
        {
            std::lock_guard lock(m_mutex);
            m_queue.emplace_back(std::move(task));
        }
        m_cv.notify_one();
    }

private:
    void run(std::stop_token st)
    {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, st, [this] { return !m_queue.empty(); });
                if (m_queue.empty())
                    return; // stop requested and nothing left to drain
                task = std::move(m_queue.front());
                m_queue.pop_front();
            }
            task();
        }
    }

    std::mutex                        m_mutex;
    std::condition_variable_any       m_cv;
    std::deque<std::function<void()>> m_queue;
    std::vector<std::jthread>         m_threads; // last: joined first
};

// ---------------------------------------------------------------------------
// Compile-time API checks
// ---------------------------------------------------------------------------

static_assert(noexcept(std::declval<rb_int&>().try_request()));
static_assert(noexcept(std::declval<rb_int&>().try_take()));
static_assert(noexcept(std::declval<rb_int&>().fulfill(1)));
static_assert(noexcept(std::declval<rb_int&>().fail()));
static_assert(!std::is_copy_constructible_v<rb_int>);
static_assert(!std::is_copy_assignable_v<rb_int>);
static_assert(!std::is_move_constructible_v<rb_int>);
static_assert(!std::is_move_assignable_v<rb_int>);

// fulfill() only takes an rvalue: an lvalue must not compile (the
// requires-expression must be dependent, hence the concept).
template<typename B, typename A>
concept can_fulfill = requires(B& b, A&& a) { b.fulfill(std::forward<A>(a)); };

static_assert(!can_fulfill<rb_int, int&>);
static_assert(can_fulfill<rb_int, int>);

// ---------------------------------------------------------------------------
// Payload of the stress tests: self-checking, large enough to be torn.
// ---------------------------------------------------------------------------

struct payload {
    u64              seq = 0;
    std::vector<u64> data;
    u64              sum = 0;
};

static payload make_payload(u64 seq)
{
    payload p;
    p.seq = seq;
    p.data.resize(1 + (seq % 64) * 16);
    for (u64 i = 0; i < p.data.size(); ++i)
        p.data[i] = seq * 1000003u + i * i;
    p.sum = std::accumulate(p.data.begin(), p.data.end(), u64{ 0 });
    return p;
}

static bool payload_is_consistent(const payload& p)
{
    if (p.data.size() != 1 + (p.seq % 64) * 16)
        return false;
    for (u64 i = 0; i < p.data.size(); ++i)
        if (p.data[i] != p.seq * 1000003u + i * i)
            return false;
    return p.sum == std::accumulate(p.data.begin(), p.data.end(), u64{ 0 });
}

// The test thread is the GUI: it polls try_take()/try_request() while a pool
// computes. Every id with id % 7 == 3 makes the task throw -> fail().
// Over the whole run:
//  - never more than one task active (no request storm);
//  - payloads are intact (TSan checks the happens-before edges of m_value);
//  - results arrive in launch order (a single task in flight at any time);
//  - launched == taken + failed (nothing lost, nothing duplicated).
//
// `gui_yields` paces the GUI loop with yield() instead of sleep_for(): a
// sleep costs one timer tick (1 to 15.6 ms) on Windows.
static void stress(unsigned workers, u64 target_launches, int gui_yields)
{
    namespace ut = boost::ut;
    using ut::eq;
    using ut::expect;

    request_buffer<payload> buf;
    std::atomic<int>        active{ 0 };
    std::atomic<int>        max_active{ 0 };
    std::atomic<u64>        failed{ 0 };

    u64  launched = 0;
    u64  taken    = 0;
    u64  last_seq = 0;
    bool intact   = true;
    bool in_order = true;

    {
        thread_pool pool(workers); // destroyed before buf and the counters

        while (launched < target_launches ||
               buf.current_state() != request_buffer<payload>::state::idle) {
            if (auto r = buf.try_take()) {
                intact   = intact && payload_is_consistent(*r);
                in_order = in_order && r->seq > last_seq;
                last_seq = r->seq;
                ++taken;
            }

            if (launched < target_launches && buf.try_request()) {
                const u64 id = ++launched;
                pool.submit([&buf, &active, &max_active, &failed, id] {
                    const int now  = active.fetch_add(1) + 1;
                    int       prev = max_active.load();
                    while (prev < now &&
                           !max_active.compare_exchange_weak(prev, now)) {
                    }

                    try {
                        if (id % 7 == 3)
                            throw std::runtime_error("simulated failure");
                        payload p = make_payload(id);
                        active.fetch_sub(1);
                        buf.fulfill(std::move(p));
                    } catch (...) {
                        active.fetch_sub(1);
                        failed.fetch_add(1);
                        buf.fail();
                    }
                });
            }

            for (int i = 0; i < gui_yields; ++i)
                std::this_thread::yield();
        }
    }

    u64 expected_failed = 0;
    for (u64 id = 1; id <= launched; ++id)
        if (id % 7 == 3)
            ++expected_failed;

    expect(eq(launched, target_launches));
    expect(eq(max_active.load(), 1)) << "more than one task in flight";
    expect(intact) << "torn or stale payload";
    expect(in_order) << "results out of order";
    expect(eq(failed.load(), expected_failed));
    expect(eq(taken + failed.load(), launched)) << "lost or duplicated result";
    expect(buf.current_state() == request_buffer<payload>::state::idle);
}

} // namespace

int main()
{
    using namespace boost::ut;

    // -----------------------------------------------------------------------
    // Single-threaded protocol
    // -----------------------------------------------------------------------

    "initial state is idle"_test = [] {
        irt::request_buffer<int> buf;
        expect(buf.current_state() == irt::request_buffer<int>::state::idle);
        expect(!buf.try_take().has_value());
    };

    "try_request is exclusive"_test = [] {
        irt::request_buffer<int> buf;

        expect(buf.try_request());  // idle -> pending
        expect(!buf.try_request()); // already pending
        expect(!buf.try_request());
        expect(buf.current_state() == irt::request_buffer<int>::state::pending);

        buf.fulfill(1); // pending -> ready
        expect(buf.current_state() == irt::request_buffer<int>::state::ready);
        expect(!buf.try_request()) << "a result is waiting: no new request";

        auto r = buf.try_take(); // ready -> idle
        expect(r.has_value());
        expect(buf.current_state() == irt::request_buffer<int>::state::idle);
        expect(buf.try_request()) << "allowed again";
        buf.fail();
    };

    "try_take returns nullopt when idle or pending"_test = [] {
        irt::request_buffer<int> buf;
        expect(!buf.try_take().has_value()); // idle

        expect(buf.try_request());
        expect(!buf.try_take().has_value()); // pending
        expect(buf.current_state() == irt::request_buffer<int>::state::pending);

        buf.fulfill(5);
        auto r = buf.try_take();
        expect(r.has_value());
        expect(eq(r.value_or(0), 5));
    };

    "round trip and single delivery"_test = [] {
        irt::request_buffer<std::string> buf;
        const std::string big(1000, 'x'); // forces a heap allocation

        expect(buf.try_request());
        buf.fulfill(std::string(big));

        auto r = buf.try_take();
        expect(r.has_value());
        expect(r.has_value() and *r == big);

        expect(!buf.try_take().has_value()) << "delivered only once";
    };

    "move-only type"_test = [] {
        irt::request_buffer<std::unique_ptr<int>> buf;

        expect(buf.try_request());
        buf.fulfill(std::make_unique<int>(7));

        auto r = buf.try_take();
        expect(r.has_value());
        expect(r.has_value() and *r != nullptr and **r == 7);
    };

    "fail returns to idle and allows a new request"_test = [] {
        irt::request_buffer<std::string> buf;

        expect(buf.try_request());
        buf.fail(); // pending -> idle
        expect(buf.current_state() ==
               irt::request_buffer<std::string>::state::idle);
        expect(!buf.try_take().has_value()) << "nothing delivered";

        expect(buf.try_request()) << "can retry";
        buf.fulfill("ok");
        auto r = buf.try_take();
        expect(r.has_value() and *r == "ok");
    };

    "many cycles, single thread"_test = [] {
        irt::request_buffer<int> buf;
        int                      wrong = 0;
        for (int i = 0; i < 10000; ++i) {
            if (!buf.try_request())
                ++wrong;
            if (i % 5 == 0) {
                buf.fail();
            } else {
                buf.fulfill(int{ i });
                auto r = buf.try_take();
                wrong += !(r.has_value() and *r == i);
            }
        }
        expect(eq(wrong, 0));
        expect(buf.current_state() == irt::request_buffer<int>::state::idle);
    };

    // -----------------------------------------------------------------------
    // Multi-threaded
    // -----------------------------------------------------------------------

    // The GUI keeps polling while the worker is stalled: nothing blocks, and
    // the anti-spam guarantee holds the whole time.
    "GUI is never blocked by a stalled worker"_test = [] {
        irt::request_buffer<int> buf;
        std::atomic<bool>        started{ false };
        std::atomic<bool>        release{ false };

        expect(buf.try_request());
        std::thread worker([&] {
            started = true;
            while (!release.load(std::memory_order_acquire))
                std::this_thread::yield();
            buf.fulfill(42);
        });

        expect(irt::wait_until([&] { return started.load(); }));

        int leaks = 0;
        for (int i = 0; i < 100000; ++i)
            if (buf.try_take().has_value() || buf.try_request()) {
                ++leaks; // the worker has not delivered anything yet
                break;
            }
        expect(eq(leaks, 0));

        release.store(true, std::memory_order_release);
        const bool ready = irt::wait_until([&] {
            return buf.current_state() ==
                   irt::request_buffer<int>::state::ready;
        });
        worker.join();
        expect(ready);

        auto r = buf.try_take();
        expect(r.has_value());
        expect(eq(r.value_or(0), 42));
    };

    // The GUI can release its reference while a task is still pending, as long
    // as the task keeps the buffer alive (shared_ptr pattern). A gate keeps the
    // task pending until the GUI reference is really dropped: deterministic,
    // without any sleep.
    "buffer outlives the GUI owner through a shared_ptr"_test = [] {
        std::atomic<bool> go{ false };
        std::atomic<bool> delivered{ false };
        {
            irt::thread_pool pool(2);
            {
                auto buf = std::make_shared<irt::request_buffer<std::string>>();
                expect(buf->try_request());
                pool.submit([buf, &go, &delivered] {
                    while (!go.load(std::memory_order_acquire))
                        std::this_thread::yield();
                    buf->fulfill(std::string(256, 'y'));
                    delivered = true;
                });
                // the GUI reference is dropped here while the task is pending
            }
            go.store(true, std::memory_order_release);
            expect(irt::wait_until([&] { return delivered.load(); }));
        } // pool drained and joined: the last shared_ptr dies in a worker
        expect(delivered.load());
    };

    "stress: spinning GUI, 4 workers"_test = [] { irt::stress(4, 5000, 0); };

    "stress: spinning GUI, 1 worker"_test = [] { irt::stress(1, 3000, 0); };

    // A GUI at 60 Hz polls much less often than the tasks complete.
    "stress: paced GUI"_test = [] { irt::stress(4, 1000, 20); };
}