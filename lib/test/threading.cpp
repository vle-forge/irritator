// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <mutex>
#include <numeric>
#include <random>

#include <fmt/format.h>

#include <boost/ut.hpp>

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

int main()
{
    using namespace boost::ut;

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
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
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
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
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
                    cv.wait_for(
                      lk, std::chrono::milliseconds(1), [&] { return stop; });
                }
            });
        }

        writer.join();
        for (auto& th : readers)
            th.join();

        expect(gt(checks.load(), 0));
    };
}
