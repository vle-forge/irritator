// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <fmt/format.h>

#include <boost/ut.hpp>

void function_1(void* param) noexcept
{
    auto* counter = reinterpret_cast<std::atomic_int*>(param);
    (*counter) += 1;
}

void function_100(void* param) noexcept
{
    auto* counter = reinterpret_cast<std::atomic_int*>(param);
    (*counter) += 100;
}

int main()
{
    using namespace boost::ut;

    "spin-lock"_test = [] {
        int            counter = 0;
        irt::spin_lock spin;

        std::thread j1([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    irt::scoped_lock lock{ spin };
                    ++counter;
                }
                std::this_thread::yield();
            }
        });

        std::thread j2([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    irt::scoped_lock lock{ spin };
                    --counter;
                }
                std::this_thread::yield();
            }
        });

        j1.join();
        j2.join();
        expect(counter == 0);
    };

    // use-case-test: checks a classic use of task and task_list.
    "task-lists"_test = [] {
        irt::task_manager tm;

        tm.start();

        for (int i = 0; i < 100; ++i) {
            std::atomic_int counter = 0;

            tm.main_task_lists[0].add(&function_1, &counter);
            tm.main_task_lists[0].add(&function_100, &counter);
            tm.main_task_lists[0].add(&function_1, &counter);
            tm.main_task_lists[0].add(&function_100, &counter);
            tm.main_task_lists[0].submit();
            tm.main_task_lists[0].wait();

            expect(counter == 202);
        }

        tm.finalize();
    };

    // use-case-test: checks a classic use of task and task_list and do not use
    // wait.
    "task-lists-without-wait"_test = [] {
        irt::task_manager tm;

        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 100; ++i) {
            tm.main_task_lists[0].add(&function_1, &counter);
            tm.main_task_lists[0].add(&function_100, &counter);
            tm.main_task_lists[0].add(&function_1, &counter);
            tm.main_task_lists[0].add(&function_100, &counter);
            tm.main_task_lists[0].submit();
        }
        tm.main_task_lists[0].wait();
        expect(counter == 202 * 100);

        tm.finalize();
    };

    // stress-test: checks to add 200 tasks for a task_list vector < 200.
    // task_list::add must wakeup the worker without the call to the submit
    // function to avoid dead lock.
    "large-task-lists"_test = [] {
        irt::task_manager tm;

        constexpr int loop = 100;

        expect(tm.main_task_lists[0].tasks.capacity() < loop * 2);

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter = 0;

            for (int i = 0; i < loop; ++i) {
                tm.main_task_lists[0].add(&function_1, &counter);
                tm.main_task_lists[0].add(&function_100, &counter);
            }
            tm.main_task_lists[0].submit();
            tm.main_task_lists[0].wait();

            for (int i = 0; i < loop; ++i) {
                tm.main_task_lists[0].add(&function_1, &counter);
                tm.main_task_lists[0].add(&function_100, &counter);
            }
            tm.main_task_lists[0].submit();
            tm.main_task_lists[0].wait();

            for (int i = 0; i < loop; ++i) {
                tm.main_task_lists[0].add(&function_1, &counter);
                tm.main_task_lists[0].add(&function_100, &counter);
            }
            tm.main_task_lists[0].submit();
            tm.main_task_lists[0].wait();

            for (int i = 0; i < loop; ++i) {
                tm.main_task_lists[0].add(&function_1, &counter);
                tm.main_task_lists[0].add(&function_100, &counter);
            }
            tm.main_task_lists[0].submit();
            tm.main_task_lists[0].wait();

            expect(counter == 101 * 100 * 4);
        }

        tm.finalize();
    };

    "n-worker-1-temp-task-lists-simple"_test = [] {
        irt::task_manager tm;

        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter_1 = 0;
            std::atomic_int counter_2 = 0;

            tm.temp_task_lists[0].add(&function_1, &counter_1);
            tm.temp_task_lists[0].add(&function_100, &counter_2);
            tm.temp_task_lists[0].add(&function_1, &counter_1);
            tm.temp_task_lists[0].add(&function_100, &counter_2);
            tm.temp_task_lists[0].add(&function_1, &counter_1);
            tm.temp_task_lists[0].add(&function_100, &counter_2);
            tm.temp_task_lists[0].add(&function_1, &counter_1);
            tm.temp_task_lists[0].add(&function_100, &counter_2);
            tm.temp_task_lists[0].submit();
            tm.temp_task_lists[0].wait();
            expect(counter_1 == 4);
            expect(counter_2 == 400);
        }

        tm.finalize();
    };

    "n-worker-1-temp-task-lists"_test = [] {
        auto start = std::chrono::steady_clock::now();

        irt::task_manager tm;
        tm.start();
        for (int i = 0; i < 40; ++i) {
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                tm.temp_task_lists[0].add(&function_1, &counter);
                tm.temp_task_lists[0].add(&function_100, &counter);
            }
            tm.temp_task_lists[0].submit();
            tm.temp_task_lists[0].wait();
            expect(counter == 101 * 100);

            for (int i = 0; i < 100; ++i) {
                tm.temp_task_lists[0].add(&function_1, &counter);
                tm.temp_task_lists[0].add(&function_100, &counter);
            }
            tm.temp_task_lists[0].submit();
            tm.temp_task_lists[0].wait();
            expect(counter == 101 * 100 * 2);

            for (int i = 0; i < 100; ++i) {
                tm.temp_task_lists[0].add(&function_1, &counter);
                tm.temp_task_lists[0].add(&function_100, &counter);
            }
            tm.temp_task_lists[0].submit();
            tm.temp_task_lists[0].wait();
            expect(counter == 101 * 100 * 3);

            for (int i = 0; i < 100; ++i) {
                tm.temp_task_lists[0].add(&function_1, &counter);
                tm.temp_task_lists[0].add(&function_100, &counter);
            }
            tm.temp_task_lists[0].submit();
            tm.temp_task_lists[0].wait();

            expect(counter == 101 * 100 * 4);
        }
        tm.finalize();

        auto end = std::chrono::steady_clock::now();
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        fmt::print("shared: {}\n", dif.count());
    };

    "n-worker-1-temp-task-lists"_test = [] {
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < 40; ++i) {
            irt::task_manager tm;

            tm.start();
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                function_1(&counter);
                function_100(&counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(&counter);
                function_100(&counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(&counter);
                function_100(&counter);
            }

            for (int i = 0; i < 100; ++i) {
                function_1(&counter);
                function_100(&counter);
            }

            expect(counter == 101 * 100 * 4);
        }

        auto end = std::chrono::steady_clock::now();
        auto dif =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        fmt::print("linear: {}\n", dif.count());
    };
}
