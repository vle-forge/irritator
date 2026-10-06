// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <atomic>
#include <chrono>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <boost/ut.hpp>

using namespace std::chrono_literals;

// Never use sleep_for() to pace a loop: on Windows it costs one timer tick
// (1 to 15.6 ms) even for 50 us. yield() is enough to let the other threads
// run.
static void pause(int n) noexcept
{
    for (int i = 0; i < n; ++i)
        std::this_thread::yield();
}

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
                pause(20);
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

// Types of the shared_buffer scenarios

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

        std::this_thread::sleep_for(30ms);
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
                pause(20);
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

    // -----------------------------------------------------------------------
    // shared_buffer: scenarios with user-defined types (ex-"test_*")
    //
    // No sleep_for() and no fixed duration: the work is counted, and the
    // threads are released together by a start flag.
    // -----------------------------------------------------------------------

    "single_locker"_test = [] {
        struct data {
            data() noexcept = default;

            explicit data(const int x_) noexcept
              : x{ x_ }
            {}

            int x = 0;
        };

        for (int round = 0; round < 2; ++round) {
            irt::shared_buffer<data> safe_data(100);

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
    };

    "concurrent readers of a constant value"_test = [] {
        irt::shared_buffer<Counter> buffer(Counter(42));
        std::atomic<bool>           start{ false };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            errors{ 0 };

        constexpr int num_readers      = 3;
        constexpr int reads_per_thread = 3000;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_readers; ++i)
            threads.emplace_back([&] {
                while (!start.load())
                    std::this_thread::yield();

                for (int j = 0; j < reads_per_thread; ++j)
                    buffer.read([&](const Counter& c, irt::u64 /*version*/) {
                        if (c.value != 42)
                            errors.fetch_add(1);
                        read_count.fetch_add(1);
                    });
            });

        start = true;
        for (auto& t : threads)
            t.join();

        expect(eq(errors.load(), 0));
        expect(eq(read_count.load(), num_readers * reads_per_thread));
    };

    "single writer, readers see a monotonic value"_test = [] {
        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           start{ false };
        std::atomic<bool>           stop{ false };
        std::atomic<int>            ready{ 0 };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            monotonic_errors{ 0 };

        constexpr int num_readers = 3;
        constexpr int writes      = 2000;

        std::vector<std::thread> readers;
        for (int i = 0; i < num_readers; ++i)
            readers.emplace_back([&] {
                int last_value = -1;
                ++ready;
                while (!start.load())
                    std::this_thread::yield();

                while (!stop.load()) {
                    buffer.read([&](const Counter& c, irt::u64 /*version*/) {
                        if (c.value < last_value)
                            monotonic_errors.fetch_add(1);
                        last_value = c.value;
                        read_count.fetch_add(1);
                    });
                }
            });

        expect(wait_until([&] { return ready.load() == num_readers; }));
        start = true;

        for (int i = 1; i <= writes; ++i) {
            buffer.write([i](Counter& c) { c.value = i; });
            if (i % 64 == 0)
                std::this_thread::yield(); // let the readers run
        }

        // Do not stop before the readers have really overlapped the writer.
        expect(wait_until([&] { return read_count.load() >= 1000; }));
        stop = true;
        for (auto& r : readers)
            r.join();

        expect(eq(monotonic_errors.load(), 0));
        expect(eq(buffer.read(
                    [](const Counter& c, irt::u64) { return c.value.load(); }),
                  writes));
    };

    "multiple writers: no lost update"_test = [] {
        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           start{ false };

        constexpr int num_writers       = 4;
        constexpr int writes_per_thread = 500;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_writers; ++i)
            threads.emplace_back([&, thread_id = i] {
                while (!start.load())
                    std::this_thread::yield();

                for (int j = 0; j < writes_per_thread; ++j)
                    buffer.write([thread_id, j](Counter& c) {
                        c.value++;
                        c.history.push_back(thread_id * 10000 + j);
                    });
            });

        start = true;
        for (auto& t : threads)
            t.join();

        constexpr int total = num_writers * writes_per_thread;

        const auto [value, size,
                    version] = buffer.read([](const Counter& c, irt::u64 ver) {
            return std::tuple{ c.value.load(), c.history.size(), ver };
        });
        expect(eq(value, total));
        expect(eq(size, static_cast<std::size_t>(total)));
        expect(eq(version, static_cast<irt::u64>(total)));

        // Each writer's entries are in its own submission order.
        const auto history = buffer.read(
          [](const Counter& c, irt::u64) { return c.history; });
        std::vector<int> next(num_writers, 0);
        bool             ordered = true;
        for (int x : history) {
            const auto id = static_cast<std::size_t>(x / 10000);
            ordered       = ordered && x % 10000 == next[id]++;
        }
        expect(ordered);
    };

    "data integrity: checksum always matches the content"_test = [] {
        irt::shared_buffer<ComplexData> buffer;
        std::atomic<bool>               stop{ false };
        std::atomic<int>                ready{ 0 };
        std::atomic<int>                integrity_errors{ 0 };
        std::atomic<int>                checks{ 0 };

        constexpr int readers_count = 3;

        std::vector<std::thread> readers;
        for (int i = 0; i < readers_count; ++i)
            readers.emplace_back([&] {
                ++ready;
                while (!stop.load()) {
                    buffer.read([&](const ComplexData& data, irt::u64) {
                        if (!data.is_valid())
                            integrity_errors.fetch_add(1);
                        checks.fetch_add(1);
                    });
                }
            });

        expect(wait_until([&] { return ready.load() == readers_count; }));

        std::mt19937                    gen(42);
        std::uniform_int_distribution<> dis(1, 100);
        for (int i = 0; i < 1500; ++i) {
            const int value = dis(gen);
            buffer.write([value](ComplexData& d) { d.add_value(value); });
            if (i % 32 == 0)
                std::this_thread::yield();
        }

        expect(wait_until([&] { return checks.load() >= 1000; }));
        stop = true;
        for (auto& r : readers)
            r.join();

        expect(eq(integrity_errors.load(), 0));
        expect(buffer.read([](const ComplexData& d, irt::u64) {
            return d.data.size() == 1500 && d.is_valid();
        }));
    };

    "try_read under load"_test = [] {
        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };
        std::atomic<int>            ready{ 0 };
        std::atomic<int>            successful_reads{ 0 };
        std::atomic<int>            failed_reads{ 0 };

        constexpr int num_readers = 3;
        constexpr int tries       = 5000;

        std::thread writer([&] {
            int counter = 0;
            ++ready;
            while (!stop.load()) {
                buffer.write([&counter](Counter& c) { c.value = counter++; });
                std::this_thread::yield();
            }
        });

        std::vector<std::thread> readers;
        for (int i = 0; i < num_readers; ++i)
            readers.emplace_back([&] {
                ++ready;
                for (int j = 0; j < tries; ++j) {
                    if (buffer.try_read([](const Counter&, irt::u64) {}))
                        successful_reads.fetch_add(1);
                    else
                        failed_reads.fetch_add(1);
                }
            });

        for (auto& r : readers)
            r.join();
        stop = true;
        writer.join();

        expect(eq(successful_reads.load() + failed_reads.load(),
                  num_readers * tries));

        // Quiescent buffer: try_read must succeed.
        expect(buffer.try_read([](const Counter&, irt::u64) {}));
    };

    "mixed stress: writers, readers and try_readers"_test = [] {
        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };
        std::atomic<int>            writers_done{ 0 };
        std::atomic<int>            ready{ 0 };
        std::atomic<irt::u64>       backwards{ 0 };

        constexpr int num_writers       = 2;
        constexpr int writes_per_thread = 2000;
        constexpr int num_readers       = 4; // 2 read(), 2 try_read()

        std::vector<std::thread> threads;

        for (int i = 0; i < num_writers; ++i)
            threads.emplace_back([&] {
                ++ready;
                for (int j = 0; j < writes_per_thread; ++j)
                    buffer.write([](Counter& c) { c.value++; });
                ++writers_done;
            });

        for (int i = 0; i < num_readers; ++i)
            threads.emplace_back([&, use_try = (i % 2 == 1)] {
                irt::u64 last = 0;
                ++ready;
                while (writers_done.load() < num_writers) {
                    auto fn = [&](const Counter&, irt::u64 version) {
                        if (version < last)
                            ++backwards;
                        last = version;
                    };
                    if (use_try)
                        buffer.try_read(fn);
                    else
                        buffer.read(fn);
                }
            });

        for (auto& t : threads)
            t.join();

        const auto [value,
                    version] = buffer.read([](const Counter& c, irt::u64 v) {
            return std::pair{ c.value.load(), v };
        });
        expect(eq(value, num_writers * writes_per_thread));
        expect(
          eq(version, static_cast<irt::u64>(num_writers * writes_per_thread)));
        expect(eq(backwards.load(), irt::u64{ 0 }));
    };

    "large value: readers never see a torn update"_test = [] {
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

        irt::shared_buffer<Data> buf;

        // single-threaded
        buf.write([](Data& d) { d.update(1.23); });
        bool ok = false;
        buf.read([&](const Data& d, irt::u64 ver) {
            expect(not d.values.empty());
            for (auto x : d.values)
                expect(irt::almost_equal(x, 1.23, 1e-10));
            expect(ge(ver, irt::u64{ 1 }));
            ok = true;
        });
        expect(ok);

        // concurrent
        std::atomic<bool> stop{ false };
        std::atomic<int>  ready{ 0 };
        std::atomic<int>  checks{ 0 };
        std::atomic<int>  torn{ 0 };

        constexpr unsigned num_readers = 3;

        std::vector<std::thread> readers;
        for (unsigned r = 0; r < num_readers; ++r)
            readers.emplace_back([&] {
                ++ready;
                while (!stop.load(std::memory_order_acquire)) {
                    buf.read([&](const Data& d, irt::u64) {
                        const double v0 = d.values[0];
                        for (std::size_t i = 1; i < d.values.size(); ++i)
                            if (d.values[i] != v0) {
                                ++torn;
                                break;
                            }
                        ++checks;
                    });
                }
            });

        expect(wait_until(
          [&] { return ready.load() == static_cast<int>(num_readers); }));

        for (int i = 0; i < 300; ++i) {
            buf.write([i](Data& d) { d.update(double(i)); });
            std::this_thread::yield();
        }

        expect(wait_until([&] { return checks.load() >= 500; }));
        stop.store(true, std::memory_order_release);
        for (auto& th : readers)
            th.join();

        expect(eq(torn.load(), 0));
        expect(gt(checks.load(), 0));
    };
}