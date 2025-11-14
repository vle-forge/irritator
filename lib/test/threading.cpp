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
        }

    };
}
