// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <boost/ut.hpp>
#include <fmt/format.h>

using heap_mr = irt::allocator<irt::monotonic_small_buffer<256 * 256 * 16>>;

static void function_1(std::atomic_int& counter) noexcept
{
    counter.fetch_add(1, std::memory_order_acq_rel);
}

static void function_100(std::atomic_int& counter) noexcept
{
    counter.fetch_add(100, std::memory_order_acq_rel);
}

using data_task = irt::lambda_function<void(void)>;
enum class data_task_id : irt::u32;

using data_task_ref = irt::lambda_function<void(void)>;

using namespace std::chrono_literals;

template<typename Pred>
static bool wait_until(Pred&& pred, std::chrono::milliseconds timeout = 5s)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::yield();
    }
    return true;
}

using vec      = std::vector<int>;
using vec64    = std::vector<irt::u64>;
using sb_int   = irt::shared_buffer<int>;
using sb_vec   = irt::shared_buffer<vec>;
using sb_app   = irt::shared_buffer<vec, irt::append_only_merge_policy<vec>>;
using sb_app64 = irt::shared_buffer<vec64,
                                    irt::append_only_merge_policy<vec64>>;

template<typename B>
static auto snapshot(const B& b)
{
    return b.read([](const auto& value, irt::u64 version) {
        return std::pair{ value, version };
    });
}

// ---------------------------------------------------------------------------
// Compile-time API checks
// ---------------------------------------------------------------------------

namespace {

static_assert(std::is_copy_constructible_v<sb_int>);
static_assert(std::is_nothrow_copy_constructible_v<sb_int>);
static_assert(!std::is_copy_assignable_v<sb_int>);
static_assert(!std::is_move_assignable_v<sb_int>);
static_assert(noexcept(std::declval<sb_int&>().reset()));

// A callback returning a reference must not leak a reference to a slot (nor
// to a local): the result is a value.
using ref_cb = const std::string& (*)(const std::string&, irt::u64);
static_assert(
  std::is_same_v<decltype(std::declval<const irt::shared_buffer<std::string>&>()
                            .read(std::declval<ref_cb>())),
                 std::string>);

} // namespace

// ---------------------------------------------------------------------------
// Helpers shared by the tests
// ---------------------------------------------------------------------------

// Random sequence of operations compared against a plain std::vector. The
// append-only policy relies on a prefix invariant that write_only() and
// reset() break: this is the regression test of that defect.
template<typename B>
static void model_check(irt::u64 seed, int steps)
{
    namespace ut = boost::ut;

    using ut::eq;
    using ut::expect;

    B               b;
    vec             model;
    irt::u64        version = 0;
    std::mt19937_64 rng(seed);

    for (int i = 0; i < steps; ++i) {
        switch (rng() % 10) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: {
            const int x = static_cast<int>(rng() % 1000);
            model.push_back(x);
            b.write([x](vec& v) { v.push_back(x); });
            ++version;
            break;
        }
        case 6:
        case 7: {
            vec fresh(rng() % 20);
            for (auto& x : fresh)
                x = static_cast<int>(rng() % 1000);
            model = fresh;
            b.write_only([&fresh](vec& v) { v = fresh; });
            ++version;
            break;
        }
        case 8:
            model.clear();
            b.reset();
            ++version;
            break;
        default: {
            B copy(b);
            const auto [value, ver] = snapshot(copy);
            expect(value == model)
              << "copy differs from the model, seed" << seed;
            expect(eq(ver, version));
            break;
        }
        }

        const auto [value, ver] = snapshot(b);
        if (value != model || ver != version) {
            expect(value == model)
              << "content differs, seed" << seed << "step" << i;
            expect(eq(ver, version)) << "version differs, seed" << seed;
            return;
        }
    }
}

struct payload {
    irt::u64              seq = 0;
    std::vector<irt::u64> data;
};

static bool payload_ok(const payload& p)
{
    if (p.seq == 0)
        return p.data.empty();

    if (p.data.size() != 1 + p.seq % 97)
        return false;

    for (irt::u64 x : p.data)
        if (x != p.seq)
            return false;
    return true;
}

// Writer at full speed, readers checking every snapshot, optionally a thread
// calling reset(). Detects: torn snapshots, versions going backwards.
static void stress_payload(unsigned readers, irt::u64 writes, bool with_reset)
{
    namespace ut = boost::ut;

    using ut::eq;
    using ut::expect;
    using ut::ge;

    irt::shared_buffer<payload> buf;
    std::atomic<bool>           stop{ false };
    std::atomic<irt::u64>       bad{ 0 };
    std::atomic<irt::u64>       backwards{ 0 };
    std::atomic<irt::u64>       reads{ 0 };
    std::atomic<unsigned>       started{ 0 };

    std::vector<std::thread> threads;

    for (unsigned r = 0; r < readers; ++r)
        threads.emplace_back([&] {
            irt::u64 last = 0;
            ++started;
            while (!stop.load(std::memory_order_acquire)) {
                const auto [ok,
                            ver] = buf.read([](const payload& p, irt::u64 v) {
                    return std::pair{ payload_ok(p), v };
                });
                if (!ok)
                    ++bad;
                if (ver < last)
                    ++backwards;
                last = ver;
                ++reads;
            }
        });

    if (with_reset)
        threads.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                buf.reset();
                std::this_thread::sleep_for(200us);
            }
        });

    // The readers must really run while the writer works: wait for them, and
    // keep writing until they have performed a minimum number of reads.
    expect(wait_until([&] { return started.load() == readers; }))
      << "readers did not start";

    irt::u64 written = 0;
    while (written < writes || reads.load() < 5000) {
        buf.write([](payload& p) {
            ++p.seq;
            p.data.assign(1 + p.seq % 97, p.seq);
        });
        ++written;
    }

    stop = true;
    for (auto& t : threads)
        t.join();

    expect(eq(bad.load(), irt::u64{ 0 })) << "torn snapshots";
    expect(eq(backwards.load(), irt::u64{ 0 })) << "version went backwards";
    expect(ge(reads.load(), irt::u64{ 5000 }));
    if (!with_reset)
        expect(eq(snapshot(buf).second, written));
}

// ---------------------------------------------------------------------------

int main()
{
    using namespace boost::ut;

    // -----------------------------------------------------------------------
    // Single-threaded behaviour
    // -----------------------------------------------------------------------

    "initial state"_test = [] {
        sb_int b;
        const auto [value, version] = snapshot(b);
        expect(eq(value, 0));
        expect(eq(version, irt::u64{ 0 }));
    };

    "write publishes and counts versions"_test = [] {
        sb_int b;
        for (int i = 1; i <= 200; ++i) {
            const int r = b.write([i](int& x) {
                x = i;
                return x * 2;
            });
            expect(eq(r, 2 * i));

            const auto [value, version] = snapshot(b);
            expect(eq(value, i));
            expect(eq(version, static_cast<irt::u64>(i)));
        }

        b.write([](int& x, int delta) { x += delta; },
                5); // void callback + args
        expect(eq(snapshot(b).first, 205));
    };

    "write_only replaces the content"_test = [] {
        sb_vec b;
        b.write([](vec& v) { v.push_back(1); });
        b.write_only([](vec& v) { v.assign({ 7, 8 }); });

        {
            const auto [value, version] = snapshot(b);
            expect(value == vec{ 7, 8 });
            expect(eq(version, irt::u64{ 2 }));
        }

        const int r = b.write_only([](vec& v) {
            v.assign({ 3 });
            return 11;
        });
        expect(eq(r, 11));
        expect(snapshot(b).first == vec{ 3 });
    };

    "read forwards its arguments and returns values"_test = [] {
        sb_int b;
        b.write([](int& x) { x = 5; });

        expect(eq(
          b.read([](const int& x, irt::u64, int k) { return x + k; }, 10), 15));

        irt::shared_buffer<std::string> s;
        s.write([](std::string& v) { v = "hello"; });
        const std::string copy = s.read(
          [](const std::string& v, irt::u64) -> const std::string& {
              return v;
          });
        expect(eq(copy, std::string{ "hello" }));
    };

    "try_read tells whether the snapshot is still the latest"_test = [] {
        sb_int b;
        b.write([](int& x) { x = 1; });

        const auto ok = b.try_read([](const int& x, irt::u64) { return x; });
        expect(ok.first);
        expect(eq(ok.second, 1));

        // A write published while the callback runs: the snapshot is
        // consistent but no longer the latest.
        const auto stale = b.try_read([&b](const int& x, irt::u64) {
            b.write([](int& v) { v = 2; });
            return x;
        });
        expect(!stale.first);
        expect(eq(stale.second, 1));

        expect(b.try_read([](const int&, irt::u64) {}));
        expect(!b.try_read(
          [&b](const int&, irt::u64) { b.write([](int& v) { ++v; }); }));
    };

    "reset keeps versions strictly increasing"_test = [] {
        sb_vec b;
        for (int i = 0; i < 3; ++i)
            b.write([i](vec& v) { v.push_back(i); });
        expect(eq(snapshot(b).second, irt::u64{ 3 }));

        b.reset();
        {
            const auto [value, version] = snapshot(b);
            expect(value.empty());
            expect(eq(version, irt::u64{ 4 }));
        }

        b.write([](vec& v) { v.push_back(9); });
        {
            const auto [value, version] = snapshot(b);
            expect(value == vec{ 9 });
            expect(eq(version, irt::u64{ 5 }));
        }
    };

    "reset inside a read callback does not deadlock"_test = [] {
        sb_vec b;
        b.write([](vec& v) { v.push_back(1); });

        b.read([&b](const vec& v, irt::u64) {
            expect(eq(v.size(),
                      std::size_t{ 1 })); // the pinned snapshot is untouched
            b.reset();
            expect(eq(v.size(), std::size_t{ 1 }));
        });

        expect(snapshot(b).first.empty());
    };

    "copy is a consistent snapshot"_test = [] {
        sb_int a;
        for (int i = 1; i <= 7;
             ++i) // rotates the active slot through all slots
            a.write([i](int& x) { x = i; });

        const sb_int& ca = a;
        sb_int        c1(ca); // const lvalue
        sb_int        c2(a);  // non-const lvalue: not the variadic ctor
        sb_int        c3(std::move(c1)); // rvalue: copies

        for (sb_int* c : { &c2, &c3 }) {
            const auto [value, version] = snapshot(*c);
            expect(eq(value, 7));
            expect(eq(version, irt::u64{ 7 }));
        }

        c2.write([](int& x) { x = 100; });
        expect(eq(snapshot(a).first, 7)); // independent
        expect(eq(snapshot(c2).first, 100));
        expect(eq(snapshot(c2).second, irt::u64{ 8 }));
    };

    "variadic constructor builds the initial state"_test = [] {
        irt::shared_buffer<vec> b(1, 2, 3); // T{ 1, 2, 3 }
        {
            const auto [value, version] = snapshot(b);
            expect(value == vec{ 1, 2, 3 });
            expect(eq(version, irt::u64{ 0 }));
        }

        b.write([](vec& v) { v.push_back(4); });
        expect(snapshot(b).first == vec{ 1, 2, 3, 4 });
    };

    "model check: copy_merge_policy"_test = [] {
        for (irt::u64 seed = 1; seed <= 20; ++seed)
            model_check<sb_vec>(seed, 3000);
    };

    "model check: append_only_merge_policy"_test = [] {
        for (irt::u64 seed = 1; seed <= 20; ++seed)
            model_check<sb_app>(seed, 3000);
    };

    // -----------------------------------------------------------------------
    // Multi-threaded behaviour
    // -----------------------------------------------------------------------

    // Two readers pin two different slots and a writer needs a third one: the
    // writer waits, but readers are never blocked and the writer resumes as
    // soon as a slot is released.
    "writer waits for pinned slots but readers never wait"_test = [] {
        sb_int            b;
        std::atomic<int>  pinned{ 0 };
        std::atomic<bool> release1{ false };
        std::atomic<bool> release2{ false };
        std::atomic<bool> third_done{ false };

        std::thread r1([&] {
            b.read([&](const int&, irt::u64) {
                ++pinned;
                while (!release1.load())
                    std::this_thread::yield();
            });
        });
        expect(wait_until([&] { return pinned.load() == 1; }));

        b.write([](int& x) { x = 1; });

        std::thread r2([&] {
            b.read([&](const int&, irt::u64) {
                ++pinned;
                while (!release2.load())
                    std::this_thread::yield();
            });
        });
        expect(wait_until([&] { return pinned.load() == 2; }));

        b.write([](int& x) { x = 2; }); // the third slot is free: no wait

        std::thread w3([&] {
            b.write([](int& x) { x = 3; }); // both other slots are pinned
            third_done = true;
        });

        std::this_thread::sleep_for(100ms);
        expect(!third_done.load())
          << "the writer must wait for the pinned slots";
        expect(eq(b.read([](const int& x, irt::u64) { return x; }), 2))
          << "readers must not be blocked by a waiting writer";

        release1 = true;
        expect(wait_until([&] { return third_done.load(); }));

        release2 = true;
        r1.join();
        r2.join();
        w3.join();

        expect(eq(snapshot(b).first, 3));
        expect(eq(snapshot(b).second, irt::u64{ 3 }));
    };

    "stress: readers and one writer"_test = [] {
        stress_payload(3, 30000, false);
    };

    "stress: readers, one writer and reset()"_test = [] {
        stress_payload(3, 30000, true);
    };

    // Append-only vector: every snapshot must satisfy v[i] == i, whatever mix
    // of write(push_back), write_only(iota) and reset() produced it.
    "stress: append policy with mixed operations"_test = [] {
        sb_app64              buf;
        std::atomic<bool>     stop{ false };
        std::atomic<irt::u64> bad{ 0 };
        std::atomic<irt::u64> backwards{ 0 };
        std::atomic<irt::u64> reads{ 0 };
        std::atomic<int>      started{ 0 };

        std::vector<std::thread> threads;

        for (int r = 0; r < 3; ++r)
            threads.emplace_back([&] {
                irt::u64 last = 0;
                ++started;
                while (!stop.load(std::memory_order_acquire)) {
                    const auto [ok, ver] = buf.read(
                      [](const vec64& v, irt::u64 version) {
                          for (irt::u64 i = 0; i < v.size(); ++i)
                              if (v[i] != i)
                                  return std::pair{ false, version };
                          return std::pair{ true, version };
                      });
                    if (!ok)
                        ++bad;
                    if (ver < last)
                        ++backwards;
                    last = ver;
                    ++reads;
                }
            });

        threads.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                buf.reset();
                std::this_thread::sleep_for(300us);
            }
        });

        expect(wait_until([&] { return started.load() == 3; }));

        std::mt19937_64 rng(12345);
        for (int i = 0; i < 40000 || reads.load() < 5000; ++i) {
            const auto k = rng() % 100;
            if (k < 85)
                buf.write([](vec64& v) { v.push_back(v.size()); });
            else if (k < 98)
                buf.write_only([n = rng() % 64](vec64& v) {
                    v.assign(n, 0);
                    std::iota(v.begin(), v.end(), irt::u64{ 0 });
                });
            else
                buf.reset();
        }

        stop = true;
        for (auto& t : threads)
            t.join();

        expect(eq(bad.load(), irt::u64{ 0 })) << "snapshot breaking v[i] == i";
        expect(eq(backwards.load(), irt::u64{ 0 })) << "version went backwards";
        expect(ge(reads.load(), irt::u64{ 5000 }));
    };

    // Two writers (serialized) and readers: the value and the version of a
    // snapshot always belong together, and no publication is lost.
    "stress: two writers, value always matches version"_test = [] {
        irt::shared_buffer<irt::u64> buf;
        std::atomic<bool>            stop{ false };
        std::atomic<irt::u64>        mismatch{ 0 };
        std::atomic<int>             started{ 0 };

        std::vector<std::thread> readers;
        for (int r = 0; r < 2; ++r)
            readers.emplace_back([&] {
                ++started;
                while (!stop.load(std::memory_order_acquire)) {
                    const bool ok = buf.read(
                      [](const irt::u64& v, irt::u64 ver) { return v == ver; });
                    if (!ok)
                        ++mismatch;
                }
            });
        expect(wait_until([&] { return started.load() == 2; }));

        constexpr irt::u64 per_writer = 20000;
        auto               writer     = [&] {
            for (irt::u64 i = 0; i < per_writer; ++i)
                buf.write([](irt::u64& x) { ++x; });
        };

        std::thread w1(writer);
        std::thread w2(writer);
        w1.join();
        w2.join();

        stop = true;
        for (auto& t : readers)
            t.join();

        expect(eq(mismatch.load(), irt::u64{ 0 }));
        expect(eq(snapshot(buf).first, 2 * per_writer));
        expect(eq(snapshot(buf).second, 2 * per_writer));
    };

    // Copying while the source is written must always give a consistent
    // snapshot.
    "stress: copy while writing"_test = [] {
        irt::shared_buffer<payload> buf;
        std::atomic<bool>           stop{ false };
        std::atomic<irt::u64>       bad{ 0 };
        std::atomic<irt::u64>       copies{ 0 };

        std::thread copier([&] {
            while (!stop.load(std::memory_order_acquire)) {
                irt::shared_buffer<payload> c(buf);
                const auto [value, version] = snapshot(c);
                if (!payload_ok(value) || value.seq != version)
                    ++bad;
                ++copies;
            }
        });

        for (int i = 0; i < 20000 || copies.load() < 500; ++i)
            buf.write([](payload& p) {
                ++p.seq;
                p.data.assign(1 + p.seq % 97, p.seq);
            });

        stop = true;
        copier.join();
        expect(eq(bad.load(), irt::u64{ 0 })) << "inconsistent copy";
        expect(ge(copies.load(), irt::u64{ 500 }));
    };

    "data-task-copy-capture"_test = [] {
        fmt::print("data-task-copy-capture\n");
        irt::data_array<data_task, data_task_id, heap_mr> d(32);

        int a = 16;
        int b = 32;

        auto& first = d.alloc([a, b]() noexcept {
            expect(eq(a, 16));
            expect(eq(b, 32));
        });

        a *= 10;
        b *= 10;

        first();

        expect(eq(a, 160));
        expect(eq(b, 320));
    };

    "data-task-reference-capture"_test = [] {
        fmt::print("data-task-reference-capture\n");
        irt::data_array<data_task_ref, data_task_id, heap_mr> d(32);

        int a = 16;
        int b = 32;

        auto& first = d.alloc([&a, &b]() noexcept {
            expect(eq(a, 160));
            expect(eq(b, 320));
        });

        a *= 10;
        b *= 10;

        first();

        expect(eq(a, 160));
        expect(eq(b, 320));
    };

    "spin-lock"_test = [] {
        fmt::print("spin-lock\n");
        std::atomic_int counter = 0;
        irt::spin_mutex spin;

        std::thread j1([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    std::scoped_lock lock{ spin };
                    ++counter;
                }
                std::this_thread::yield();
            }
        });

        std::thread j2([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    std::scoped_lock lock{ spin };
                    --counter;
                }
                std::this_thread::yield();
            }
        });

        j1.join();
        j2.join();
        expect(eq(counter.load(), 0));
    };

    "scoped-lock"_test = [] {
        fmt::print("scoped-lock\n");
        irt::spin_mutex mutex_1;
        irt::spin_mutex mutex_2;

        for (int i = 0; i < 100; ++i) {
            std::atomic_int mult = 0;

            std::thread j1([&mult, &mutex_1]() {
                std::scoped_lock lock(mutex_1);
                mult += 1;
            });

            std::thread j2([&mult, &mutex_2]() {
                std::scoped_lock lock(mutex_2);
                mult += 10;
            });

            std::thread j3([&mult, &mutex_1, &mutex_2]() {
                std::scoped_lock lock(mutex_1, mutex_2);
                mult += 100;
            });

            j1.join();
            j2.join();
            j3.join();
            expect(eq(mult.load(), 111));
        }
    };

    // use-case-test: checks a classic use of task and task_list.
    "task-lists"_test = [] {
        fmt::print("task-lists\n");
        irt::task_manager tm(1, 0);
        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 100; ++i) {
            tm.ordered(0).add([&counter]() { function_1(counter); });
            tm.ordered(0).add([&counter]() { function_100(counter); });
            tm.ordered(0).add([&counter]() { function_1(counter); });
            tm.ordered(0).add([&counter]() { function_100(counter); });
            tm.ordered(0).wait_empty();
        }

        expect(eq(counter.load(), 20200));

        tm.shutdown();
    };

    // use-case-test: checks a classic use of task and task_list and do not use
    // wait.
    "task-lists-without-wait"_test = [] {
        fmt::print("task-lists-without-wait\n");
        irt::task_manager tm(1, 1);
        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 100; ++i) {
            tm.ordered(0).add([&counter]() { function_1(counter); });
            tm.ordered(0).add([&counter]() { function_100(counter); });
        }
        tm.ordered(0).wait_empty();
        expect(eq(counter.load(), 101 * 100));

        tm.shutdown();
    };

    // stress-test: checks to add 200 tasks for a task_list vector < 200.
    // task_list::add must wakeup the worker without the call to the submit
    // function to avoid dead lock.
    "large-task-lists"_test = [] {
        fmt::print("large-task-lists\n");
        irt::task_manager tm(1, 1);

        constexpr int loop = 100;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter = 0;

            for (int i = 0; i < loop; ++i) {
                tm.ordered(0).add([&counter]() { function_1(counter); });
                tm.ordered(0).add([&counter]() { function_100(counter); });
            }
            tm.ordered(0).wait_empty();

            for (int i = 0; i < loop; ++i) {
                tm.ordered(0).add([&counter]() { function_1(counter); });
                tm.ordered(0).add([&counter]() { function_100(counter); });
            }
            tm.ordered(0).wait_empty();

            for (int i = 0; i < loop; ++i) {
                tm.ordered(0).add([&counter]() { function_1(counter); });
                tm.ordered(0).add([&counter]() { function_100(counter); });
            }
            tm.ordered(0).wait_empty();

            for (int i = 0; i < loop; ++i) {
                tm.ordered(0).add([&counter]() { function_1(counter); });
                tm.ordered(0).add([&counter]() { function_100(counter); });
            }
            tm.ordered(0).wait_empty();

            expect(eq(counter.load(), 101 * 100 * 4));
        }

        tm.shutdown();
    };

    "n-worker-1-temp-task-lists-simple"_test = [] {
        fmt::print("n-worker-1-temp-task-lists-simple\n");
        irt::task_manager tm(0, 1);

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter_1 = 0;
            std::atomic_int counter_2 = 0;

            tm.unordered(0).add([&counter_1]() { function_1(counter_1); });
            tm.unordered(0).add([&counter_2]() { function_100(counter_2); });
            tm.unordered(0).add([&counter_1]() { function_1(counter_1); });
            tm.unordered(0).add([&counter_2]() { function_100(counter_2); });
            tm.unordered(0).add([&counter_1]() { function_1(counter_1); });
            tm.unordered(0).add([&counter_2]() { function_100(counter_2); });
            tm.unordered(0).add([&counter_1]() { function_1(counter_1); });
            tm.unordered(0).add([&counter_2]() { function_100(counter_2); });
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();
            expect(eq(counter_1.load(), 4));
            expect(eq(counter_2.load(), 400));
        }

        tm.shutdown();
    };

    "n-worker-1-temp-task-lists"_test = [] {
        fmt::print("n-worker-1-temp-task-lists\n");
        auto start = std::chrono::steady_clock::now();

        irt::task_manager tm(1, 1);
        tm.start();
        for (int n = 0; n < 40; ++n) {
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                tm.unordered(0).add([&counter]() { function_1(counter); });
                tm.unordered(0).add([&counter]() { function_100(counter); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();
            expect(eq(counter.load(), 101 * 100));

            for (int i = 0; i < 100; ++i) {
                tm.unordered(0).add([&counter]() { function_1(counter); });
                tm.unordered(0).add([&counter]() { function_100(counter); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();
            expect(eq(counter.load(), 101 * 100 * 2));

            for (int i = 0; i < 100; ++i) {
                tm.unordered(0).add([&counter]() { function_1(counter); });
                tm.unordered(0).add([&counter]() { function_100(counter); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();
            expect(eq(counter.load(), 101 * 100 * 3));

            for (int i = 0; i < 100; ++i) {
                tm.unordered(0).add([&counter]() { function_1(counter); });
                tm.unordered(0).add([&counter]() { function_100(counter); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();

            expect(eq(counter.load(), 101 * 100 * 4));
        }
        tm.shutdown();

        auto end = std::chrono::steady_clock::now();
        auto dif = std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                         start);
        fmt::print("shared: {}\n", dif.count());
    };

    "n-worker-1-temp-task-lists"_test = [] {
        fmt::print("n-worker-1-temp-task-lists\n");
        auto start = std::chrono::steady_clock::now();

        for (int n = 0; n < 40; ++n) {
            irt::task_manager tm(1, 1);

            tm.start();
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                function_1(counter);
                function_100(counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(counter);
                function_100(counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(counter);
                function_100(counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(counter);
                function_100(counter);
            }

            expect(eq(counter.load(), 101 * 100 * 4));

            tm.shutdown();
        }

        auto end = std::chrono::steady_clock::now();
        auto dif = std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                         start);
        fmt::print("linear: {}\n", dif.count());
    };

    "static-circular-buffer"_test = [] {
        fmt::print("static-circular-buffer\n");
        irt::task_manager tm(2, 0);
        std::atomic_int   buffer = 0;

        constexpr int loop = 100;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            for (int i = 0; i < loop; ++i) {
                tm.ordered(0).add([&buffer]() { buffer.fetch_add(1); });
                tm.ordered(1).add([&buffer]() { buffer.fetch_sub(1); });
            }

            tm.ordered(0).wait_empty();
            tm.ordered(1).wait_empty();
        }

        tm.shutdown();
    };

    "single_locker"_test = [] {
        fmt::print("single_locker\n");
        struct data {
            data() noexcept = default;

            explicit data(const int x_) noexcept
              : x{ x_ }
            {}

            int x = 0;
        };

        irt::shared_buffer<data> safe_data(100);

        {
            safe_data.read([](auto& x, auto /*v*/) { expect(eq(x.x, 100)); });
            safe_data.read([](auto& x, auto /*v*/) { expect(eq(x.x, 100)); });
            safe_data.write([](auto& x) {
                expect(eq(x.x, 100));
                x.x = 103;
            });
            safe_data.read([](auto& x, auto /*v*/) { expect(eq(x.x, 103)); });
            safe_data.read([](auto& x, auto /*v*/) { expect(eq(x.x, 103)); });
            safe_data.write([](auto& x) { expect(eq(x.x, 103)); });
        }

        irt::shared_buffer<data> safe_data_2(100);

        {
            safe_data_2.read([](auto& x, auto /*v*/) { expect(eq(x.x, 100)); });
            safe_data_2.read([](auto& x, auto /*v*/) { expect(eq(x.x, 100)); });
            safe_data_2.write([](auto& x) {
                expect(eq(x.x, 100));
                x.x = 103;
            });
            safe_data_2.read([](auto& x, auto /*v*/) { expect(eq(x.x, 103)); });
            safe_data_2.read([](auto& x, auto /*v*/) { expect(eq(x.x, 103)); });
            safe_data_2.write([](auto& x) { expect(eq(x.x, 103)); });
        }
    };

    "locker-in-task-manager"_test = [] {
        fmt::print("locker-in-task-manager\n");
        irt::task_manager tm(2, 0);
        tm.start();

        irt::shared_buffer<irt::small_vector<int, 16>> buffer;
        std::atomic_int                                counter = 0;

        for (int i = 0; i < 16; ++i) {
            tm.ordered(0).add([&buffer, &counter]() {
                buffer.read([&counter](const auto& vec, auto /*v*/) {
                    if (not vec.empty())
                        counter = vec.back();
                    else
                        counter = 0;
                });
            });

            tm.ordered(1).add([&buffer]() {
                buffer.write([](auto& vec) { vec.push_back(10); });
            });

            tm.ordered(0).add([&buffer, &counter]() {
                buffer.read([&counter](const auto& vec, auto /*ver*/) {
                    if (not vec.empty())
                        counter = vec.back();
                    else
                        counter = 0;
                });
            });
        }

        tm.ordered(0).wait_empty();
        tm.ordered(1).wait_empty();

        tm.shutdown();
    };

    struct Counter {
        std::atomic_int  value = 0;
        std::vector<int> history;

        Counter() = default;
        Counter(int v)
          : value(v)
        {}

        Counter(const Counter& o) noexcept
          : value(o.value.load())
          , history(o.history)
        {}

        Counter(Counter&& o) noexcept
          : value(o.value.load())
          , history(std::move(o.history))
        {}

        Counter& operator=(const Counter& o) noexcept
        {
            if (this != &o) {
                value   = o.value.load();
                history = o.history;
            }

            return *this;
        }

        Counter& operator=(Counter&& o) noexcept
        {
            if (this != &o) {
                value   = o.value.load();
                history = std::move(o.history);
            }

            return *this;
        }
    };

    struct ComplexData {
        std::vector<int> data;
        std::atomic_int  checksum = 0;

        ComplexData() = default;

        ComplexData(const ComplexData& o) noexcept
          : data(o.data)
          , checksum(o.checksum.load())
        {}

        ComplexData(ComplexData&& o) noexcept
          : data(std::move(o.data))
          , checksum(o.checksum.load())
        {}

        ComplexData& operator=(const ComplexData& o) noexcept
        {
            if (&o != this) {
                data     = o.data;
                checksum = o.checksum.load();
            }

            return *this;
        }

        ComplexData& operator=(ComplexData&& o) noexcept
        {
            if (&o != this) {
                data     = std::move(o.data);
                checksum = o.checksum.load();
            }

            return *this;
        }

        void add_value(int v)
        {
            data.push_back(v);
            checksum += v;
        }

        bool is_valid() const
        {
            int sum = std::accumulate(data.begin(), data.end(), 0);
            return sum == checksum;
        }
    };

    "test_concurrent_reads"_test = [] {
        fmt::print("test_concurrent_reads\n");

        irt::shared_buffer<Counter> buffer(Counter(42));
        std::atomic<bool>           start{ false };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            errors{ 0 };

        const int num_readers      = 4;
        const int reads_per_thread = 10000;

        std::vector<std::thread> threads;

        for (int i = 0; i < num_readers; ++i) {
            threads.emplace_back([&]() {
                while (!start.load()) {
                }

                for (int j = 0; j < reads_per_thread; ++j) {
                    buffer.read(
                      [&](const Counter& c, const irt::u64 /*version*/) {
                          if (c.value != 42) {
                              errors.fetch_add(1);
                          }
                          read_count.fetch_add(1);
                      });
                }
            });
        }

        start.store(true);

        for (auto& t : threads) {
            t.join();
        }

        fmt::print("  read count: {}\n", read_count.load());
        fmt::print("  errors: {}\n", errors.load());
    };

    "test_single_writer_multiple_readers"_test = [] {
        fmt::print("test_single_writer_multiple_readers\n");

        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };
        std::atomic<int>            write_count{ 0 };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            monotonic_errors{ 0 };

        const int num_readers = 3;

        std::thread writer([&]() {
            for (int i = 0; i < 1000; ++i) {
                buffer.write([i](Counter& c) { c.value = i; });
                write_count.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            stop.store(true);
        });

        std::vector<std::thread> readers;
        for (int i = 0; i < num_readers; ++i) {
            readers.emplace_back([&]() {
                int last_value = -1;
                while (!stop.load()) {
                    buffer.read(
                      [&](const Counter& c, const irt::u64 /*version*/) {
                          if (c.value < last_value) {
                              monotonic_errors.fetch_add(1);
                          }
                          last_value = c.value;
                          read_count.fetch_add(1);
                      });
                }
            });
        }

        writer.join();
        for (auto& r : readers)
            r.join();

        fmt::print("  write count: {}\n", write_count.load());
        fmt::print("  read count: {}\n", read_count.load());
        fmt::print("  monotonic errors: {}\n", monotonic_errors.load());
    };

    "test_multiple_writers"_test = [] {
        fmt::print("test_multiple_writers\n");

        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           start{ false };
        std::atomic<int>            total_writes{ 0 };

        const int num_writers       = 4;
        const int writes_per_thread = 1000;

        std::vector<std::thread> threads;

        for (int i = 0; i < num_writers; ++i) {
            threads.emplace_back([&, thread_id = i]() {
                while (!start.load()) {
                }

                for (int j = 0; j < writes_per_thread; ++j) {
                    buffer.write([thread_id, j](Counter& c) {
                        c.value++;
                        c.history.push_back(thread_id * 10000 + j);
                    });
                    total_writes.fetch_add(1);
                }
            });
        }

        start.store(true);

        for (auto& t : threads) {
            t.join();
        }

        int    final_value  = 0;
        size_t history_size = 0;
        buffer.read([&](const Counter& c, const irt::u64 /*version*/) {
            final_value  = c.value;
            history_size = c.history.size();
        });

        fmt::print("  Writes required: {}\n", num_writers, writes_per_thread);
        fmt::print("  final value: {}\n", final_value);
        fmt::print("  history size: {}\n", history_size);
    };

    "test_data_integrity"_test = [] {
        fmt::print("test_data_integrity\n");

        irt::shared_buffer<ComplexData> buffer;
        std::atomic<bool>               stop{ false };
        std::atomic<int>                integrity_errors{ 0 };
        std::atomic<int>                checks{ 0 };

        std::thread writer([&]() {
            std::random_device              rd;
            std::mt19937                    gen(rd());
            std::uniform_int_distribution<> dis(1, 100);

            for (int i = 0; i < 5000; ++i) {
                int value = dis(gen);
                buffer.write(
                  [value](ComplexData& data) { data.add_value(value); });
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            stop.store(true);
        });

        std::vector<std::thread> readers;
        for (int i = 0; i < 3; ++i) {
            readers.emplace_back([&]() {
                while (!stop.load()) {
                    buffer.read(
                      [&](const ComplexData& data, const irt::u64 /*version*/) {
                          if (!data.is_valid()) {
                              integrity_errors.fetch_add(1);
                          }
                          checks.fetch_add(1);
                      });
                }
            });
        }

        writer.join();
        for (auto& r : readers) {
            r.join();
        }

        fmt::print("  checks: {}\n", checks.load());
        fmt::print("  integrity errors: {}\n", integrity_errors.load());
    };

    "test_try_read_under_load"_test = [] {
        fmt::print("test_try_read_under_load\n");

        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };
        std::atomic<int>            successful_reads{ 0 };
        std::atomic<int>            failed_reads{ 0 };

        std::thread writer([&]() {
            int counter = 0;
            while (!stop.load()) {
                buffer.write([&counter](Counter& c) { c.value = counter++; });
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });

        std::vector<std::thread> readers;
        for (int i = 0; i < 3; ++i) {
            readers.emplace_back([&]() {
                auto start_time = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start_time <
                       std::chrono::seconds(2)) {
                    bool success = buffer.try_read(
                      [](const Counter& /*c*/, const irt::u64 /*version*/) {
                          // success !
                      });

                    if (success) {
                        successful_reads.fetch_add(1);
                    } else {
                        failed_reads.fetch_add(1);
                    }
                }
            });
        }

        for (auto& r : readers)
            r.join();

        stop.store(true);
        writer.join();

        int    total        = successful_reads.load() + failed_reads.load();
        double success_rate = (100.0 * successful_reads.load()) / total;

        fmt::print("  successful reads: {}\n", successful_reads.load());
        fmt::print("  failed reads: {}\n", failed_reads.load());
        fmt::print("  success rate: {}%\n", success_rate);
    };

    "test_stress_mixed"_test = [] {
        fmt::print("test_stress_mixed\n");

        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };

        auto       start_time = std::chrono::steady_clock::now();
        const auto duration   = std::chrono::seconds(3);

        std::vector<std::thread> threads;

        for (int i = 0; i < 2; ++i) {
            threads.emplace_back([&]() {
                while (std::chrono::steady_clock::now() - start_time <
                       duration) {
                    buffer.write([](Counter& c) { c.value++; });
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            });
        }

        for (int i = 0; i < 2; ++i) {
            threads.emplace_back([&]() {
                while (std::chrono::steady_clock::now() - start_time <
                       duration) {
                    buffer.read(
                      [](const Counter& c, const irt::u64 /*version*/) {
                          volatile int x = c.value;
                          (void)x;
                      });
                }
            });
        }

        for (int i = 0; i < 2; ++i) {
            threads.emplace_back([&]() {
                while (std::chrono::steady_clock::now() - start_time <
                       duration) {
                    buffer.try_read(
                      [](const Counter& c, const irt::u64 /*version*/) {
                          volatile int x = c.value;
                          (void)x;
                      });
                }
            });
        }

        stop = true;
        for (auto& t : threads) {
            t.join();
        }

        int final_value = 0;
        buffer.read([&](const Counter& c, const irt::u64 /*version*/) {
            final_value = c.value;
        });

        fmt::print("final value: {}\n", final_value);
    };

    struct Data {
        std::vector<double> values;

        Data()
          : values(1000, 0.0)
        {}

        void update(double v)
        {
            for (auto& x : values)
                x = v;
        }
    };

    "shared_buffer_test::SingleWriterReader"_test = [] {
        irt::shared_buffer<Data> buf;

        // Writer met à jour
        buf.write([](Data& d) { d.update(1.23); });

        // Reader lit
        bool ok = false;
        buf.read([&](const Data& d, std::uint64_t ver) {
            expect(not d.values.empty());
            for (auto x : d.values) {
                expect(irt::almost_equal(x, 1.23, 1e-10));
            }
            expect(ge(ver, 1u));
            ok = true;
        });

        expect(ok);
    };

    "shared_buffer_test::ConcurrentReaders"_test = [] {
        irt::shared_buffer<Data> buf;
        static const auto        max_reader = 3u; /*
   std::thread::hardware_concurrency() <= 1u
            ? 1u
            : std::thread::hardware_concurrency() - 1u; */
        std::atomic<bool> stop{ false };

        std::thread writer([&] {
            for (int i = 0; i < 50; ++i) {
                buf.write([&](Data& d) { d.update(double(i)); });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            stop.store(true, std::memory_order_release);
        });

        std::vector<std::thread> readers;
        std::atomic<int>         checks{ 0 };

        for (unsigned r = 0; r < max_reader; ++r) {
            readers.emplace_back([&] {
                while (!stop.load(std::memory_order_acquire)) {
                    buf.read([&](const Data& d, std::uint64_t ver) {
                        if (!d.values.empty()) {
                            const double v0 = d.values[0];
                            bool         ok = true;
                            for (std::size_t i = 1; i < d.values.size(); ++i) {
                                if (d.values[i] != v0) {
                                    ok = false;
                                    break;
                                }
                            }
                            if (ok)
                                checks.fetch_add(1, std::memory_order_relaxed);
                        }
                        (void)ver;
                    });
                }
            });
        }

        writer.join();
        for (auto& th : readers)
            th.join();

        expect(gt(checks.load(), 0));
    };

    "shared_buffer_test::ConcurrentReadersConditionVar"_test = [] {
        irt::shared_buffer<Data> buf;
        std::mutex               m;
        std::condition_variable  cv;
        bool                     stop       = false;
        static const auto        max_reader = 3u;
        // std::thread::hardware_concurrency() <= 1u
        //          ? 1u
        //          : std::thread::hardware_concurrency() - 1u;

        std::thread writer([&] {
            for (int i = 0; i < 50; ++i) {
                buf.write([&](Data& d) { d.update(double(i)); });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            {
                std::lock_guard<std::mutex> lk(m);
                stop = true;
            }
            cv.notify_all();
        });

        std::vector<std::thread> readers;
        std::atomic<int>         checks{ 0 };

        for (unsigned r = 0; r < max_reader; ++r) {
            readers.emplace_back([&] {
                std::unique_lock<std::mutex> lk(m);
                while (!stop) {
                    lk.unlock();
                    buf.read([&](const Data& d, std::uint64_t ver) {
                        if (!d.values.empty()) {
                            const double v0 = d.values[0];
                            bool         ok = true;
                            for (std::size_t i = 1; i < d.values.size(); ++i) {
                                if (d.values[i] != v0) {
                                    ok = false;
                                    break;
                                }
                            }
                            if (ok)
                                checks.fetch_add(1, std::memory_order_relaxed);
                        }
                        (void)ver;
                    });
                    lk.lock();
                    cv.wait_for(lk, std::chrono::milliseconds(1),
                                [&] { return stop; });
                }
            });
        }

        writer.join();
        for (auto& th : readers)
            th.join();

        expect(gt(checks.load(), 0));
    };
}
