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

static void function_1(std::atomic_int& counter) noexcept { counter += 1; }
static void function_100(std::atomic_int& counter) noexcept { counter += 100; }

using data_task = irt::small_function<sizeof(int) * 2, void(void)>;
enum class data_task_id : irt::u32;

using data_task_ref = irt::small_function<sizeof(void*) * 2, void(void)>;

int main()
{
    using namespace boost::ut;

    "data-task-copy-capture"_test = [] {
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
        int             counter = 0;
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
        expect(counter == 0);
    };

    "scoped-lock"_test = [] {
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
        irt::task_manager<1, 1> tm;
        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 100; ++i) {
            tm.get_ordered_list(0).add([&counter]() { function_1(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_100(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_1(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_100(counter); });
            tm.get_ordered_list(0).wait_completion();
        }

        tm.get_ordered_list(0).wait_completion();
        expect(counter == 20200);

        tm.shutdown();
    };

    // use-case-test: checks a classic use of task and task_list and do not use
    // wait.
    "task-lists-without-wait"_test = [] {
        irt::task_manager<1, 1> tm;
        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 100; ++i) {
            tm.get_ordered_list(0).add([&counter]() { function_1(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_100(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_1(counter); });
            tm.get_ordered_list(0).add([&counter]() { function_100(counter); });
        }
        tm.get_ordered_list(0).wait_completion();
        expect(counter == 202 * 100);

        tm.shutdown();
    };

    // stress-test: checks to add 200 tasks for a task_list vector < 200.
    // task_list::add must wakeup the worker without the call to the submit
    // function to avoid dead lock.
    "large-task-lists"_test = [] {
        irt::task_manager<1, 1> tm;

        constexpr int loop = 100;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter = 0;

            for (int i = 0; i < loop; ++i) {
                tm.get_ordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_ordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_ordered_list(0).wait_completion();

            for (int i = 0; i < loop; ++i) {
                tm.get_ordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_ordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_ordered_list(0).wait_completion();

            for (int i = 0; i < loop; ++i) {
                tm.get_ordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_ordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_ordered_list(0).wait_completion();

            for (int i = 0; i < loop; ++i) {
                tm.get_ordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_ordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_ordered_list(0).wait_completion();

            expect(counter == 101 * 100 * 4);
        }

        tm.shutdown();
    };

    "n-worker-1-temp-task-lists-simple"_test = [] {
        irt::task_manager tm;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter_1 = 0;
            std::atomic_int counter_2 = 0;

            tm.get_unordered_list(0).add(
              [&counter_1]() { function_1(counter_1); });
            tm.get_unordered_list(0).add(
              [&counter_2]() { function_100(counter_2); });
            tm.get_unordered_list(0).add(
              [&counter_1]() { function_1(counter_1); });
            tm.get_unordered_list(0).add(
              [&counter_2]() { function_100(counter_2); });
            tm.get_unordered_list(0).add(
              [&counter_1]() { function_1(counter_1); });
            tm.get_unordered_list(0).add(
              [&counter_2]() { function_100(counter_2); });
            tm.get_unordered_list(0).add(
              [&counter_1]() { function_1(counter_1); });
            tm.get_unordered_list(0).add(
              [&counter_2]() { function_100(counter_2); });
            tm.get_unordered_list(0).submit();
            tm.get_unordered_list(0).wait_completion();
            expect(counter_1 == 4);
            expect(counter_2 == 400);
        }

        tm.shutdown();
    };

    "n-worker-1-temp-task-lists"_test = [] {
        auto start = std::chrono::steady_clock::now();

        irt::task_manager tm;
        tm.start();
        for (int n = 0; n < 40; ++n) {
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                tm.get_unordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_unordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_unordered_list(0).submit();
            tm.get_unordered_list(0).wait_completion();
            expect(counter == 101 * 100);

            for (int i = 0; i < 100; ++i) {
                tm.get_unordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_unordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_unordered_list(0).submit();
            tm.get_unordered_list(0).wait_completion();
            expect(counter == 101 * 100 * 2);

            for (int i = 0; i < 100; ++i) {
                tm.get_unordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_unordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_unordered_list(0).submit();
            tm.get_unordered_list(0).wait_completion();
            expect(counter == 101 * 100 * 3);

            for (int i = 0; i < 100; ++i) {
                tm.get_unordered_list(0).add(
                  [&counter]() { function_1(counter); });
                tm.get_unordered_list(0).add(
                  [&counter]() { function_100(counter); });
            }
            tm.get_unordered_list(0).submit();
            tm.get_unordered_list(0).wait_completion();

            expect(counter == 101 * 100 * 4);
        }
        tm.shutdown();

        auto end = std::chrono::steady_clock::now();
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        fmt::print("shared: {}\n", dif.count());
    };

    "n-worker-1-temp-task-lists"_test = [] {
        auto start = std::chrono::steady_clock::now();

        for (int n = 0; n < 40; ++n) {
            irt::task_manager tm;

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

            expect(counter == 101 * 100 * 4);
        }

        auto end = std::chrono::steady_clock::now();
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        fmt::print("linear: {}\n", dif.count());
    };

    "static-circular-buffer"_test = [] {
        irt::task_manager             tm;
        irt::circular_buffer<int, 16> buffer;

        constexpr int loop = 100;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            for (int i = 0; i < loop; ++i) {
                tm.get_ordered_list(0).add(
                  [&buffer]() { (void)buffer.try_push(0); });

                tm.get_ordered_list(1).add([&buffer]() {
                    int r;
                    (void)buffer.try_pop(r);
                });
            }

            tm.get_ordered_list(0).wait_completion();
            tm.get_ordered_list(1).wait_completion();
        }
    };

    "single_locker"_test = [] {
        struct data {
            data() noexcept = default;

            explicit data(const int x_) noexcept
              : x{ x_ }
            {}

            int x = 0;
        };

        irt::locker<data> safe_data(100);

        {
            safe_data.try_read_only([](auto& x) { expect(eq(x.x, 100)); });
            safe_data.read_only([](auto& x) { expect(eq(x.x, 100)); });
            safe_data.read_write([](auto& x) {
                expect(eq(x.x, 100));
                x.x = 103;
            });
            safe_data.try_read_only([](auto& x) { expect(eq(x.x, 103)); });
            safe_data.read_only([](auto& x) { expect(eq(x.x, 103)); });
            safe_data.read_write([](auto& x) { expect(eq(x.x, 103)); });
        }

        irt::locker_2<data> safe_data_2(100);

        {
            safe_data_2.try_read_only([](auto& x) { expect(eq(x.x, 100)); });
            safe_data_2.read_only([](auto& x) { expect(eq(x.x, 100)); });
            safe_data_2.read_write([](auto& x) {
                expect(eq(x.x, 100));
                x.x = 103;
            });
            safe_data_2.try_read_only([](auto& x) { expect(eq(x.x, 103)); });
            safe_data_2.read_only([](auto& x) { expect(eq(x.x, 103)); });
            safe_data_2.read_write([](auto& x) { expect(eq(x.x, 103)); });
        }
    };

    "locker-in-task-manager"_test = [] {
        irt::task_manager tm;
        tm.start();

        irt::locker_2<irt::small_vector<int, 16>> buffer;
        std::atomic_int                           counter = 0;

        for (int i = 0; i < 16; ++i) {
            tm.get_ordered_list(0).add([&buffer, &counter]() {
                buffer.read_only([&counter](const auto& vec) {
                    if (not vec.empty())
                        counter = vec.back();
                    else
                        counter = 0;
                });
            });

            tm.get_ordered_list(1).add([&buffer]() {
                buffer.read_write([](auto& vec) { vec.push_back(10); });
            });

            tm.get_ordered_list(0).add([&buffer, &counter]() {
                buffer.read_only([&counter](const auto& vec) {
                    if (not vec.empty())
                        counter = vec.back();
                    else
                        counter = 0;
                });
            });
        }

        tm.get_ordered_list(0).wait_completion();
        tm.get_ordered_list(1).wait_completion();
    };

    struct Counter {
        int              value = 0;
        std::vector<int> history;

        Counter() = default;
        Counter(int v)
          : value(v)
        {}
    };

    struct ComplexData {
        std::vector<int> data;
        int              checksum = 0;

        ComplexData() = default;

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
        boost::ut::log << "test_concurrent_reads";

        irt::shared_buffer<Counter> buffer(Counter(42));
        std::atomic<bool>           start{ false };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            errors{ 0 };

        const int num_readers      = 10;
        const int reads_per_thread = 10000;

        std::vector<std::thread> threads;

        for (int i = 0; i < num_readers; ++i) {
            threads.emplace_back([&]() {
                while (!start.load()) {
                }

                for (int j = 0; j < reads_per_thread; ++j) {
                    buffer.read([&](const Counter& c, const irt::u64 /*version*/) {
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

        boost::ut::log << "  read count: " << read_count.load();
        boost::ut::log << "  errors: " << errors.load();
    };

    "test_single_writer_multiple_readers"_test = [] {
        boost::ut::log << "test_single_writer_multiple_readers";

        irt::shared_buffer<Counter> buffer(Counter(0));
        std::atomic<bool>           stop{ false };
        std::atomic<int>            write_count{ 0 };
        std::atomic<int>            read_count{ 0 };
        std::atomic<int>            monotonic_errors{ 0 };

        const int num_readers = 8;

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

        boost::ut::log << "  write count: " << write_count.load();
        boost::ut::log << "  read count: " << read_count.load();
        boost::ut::log << "  monotonic errors: " << monotonic_errors.load();
    };

    "test_multiple_writers"_test = [] {
        boost::ut::log << "test_multiple_writers";

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

        boost::ut::log << "  Writes required: "
                       << num_writers * writes_per_thread;
        boost::ut::log << "  final value: " << final_value;
        boost::ut::log << "  history size: " << history_size;
    };

    "test_data_integrity"_test = [] {
        boost::ut::log << "test_data_integrity";

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
        for (int i = 0; i < 4; ++i) {
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

        boost::ut::log << "  checks: " << checks.load();
        boost::ut::log << "  integrity errors: " << integrity_errors.load();
    };

    "test_try_read_under_load"_test = [] {
        boost::ut::log << "test_try_read_under_load";

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
        for (int i = 0; i < 6; ++i) {
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

        boost::ut::log << "  successful reads: " << successful_reads.load();
        boost::ut::log << "  failed reads: " << failed_reads.load();
        boost::ut::log << "  success rate: " << success_rate << "%";
    };

    "test_stress_mixed"_test = [] {
        boost::ut::log << "test_stress_mixed";

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

        for (int i = 0; i < 8; ++i) {
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

        for (int i = 0; i < 4; ++i) {
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

        for (auto& t : threads) {
            t.join();
        }

        int final_value = 0;
        buffer.read([&](const Counter& c, const irt::u64 /*version*/) {
            final_value = c.value;
        });

        boost::ut::log << "final value " << final_value;
    };
}
