// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <fmt/format.h>

#include <boost/ut.hpp>

void function_1(void* param) noexcept
{
    auto* counter = reinterpret_cast<int*>(param);
    (*counter) += 1;
}

void function_100(void* param) noexcept
{
    auto* counter = reinterpret_cast<int*>(param);
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
                    irt::scoped_spin_lock lock{ spin };
                    ++counter;
                }
                std::this_thread::yield();
            }
        });

        std::thread j2([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    irt::scoped_spin_lock lock{ spin };
                    --counter;
                }
                std::this_thread::yield();
            }
        });

        j1.join();
        j2.join();
        expect(counter == 0);
    };

    "task-lists"_test = [] {
        irt::task_manager_parameters init{ .thread_number           = 1,
                                           .simple_task_list_number = 1,
                                           .multi_task_list_number  = 0 };

        irt::task_manager tm(init);

        assert(tm.task_lists.ssize() == 1);
        assert(tm.workers.ssize() == 1);

        tm.workers[0].task_lists.emplace_back(&tm.task_lists[0]);

        tm.start();
        int counter = 0;

        tm.task_lists[0].add(&function_1, &counter);
        tm.task_lists[0].add(&function_100, &counter);
        tm.task_lists[0].add(&function_1, &counter);
        tm.task_lists[0].add(&function_100, &counter);
        tm.task_lists[0].submit();
        tm.task_lists[0].wait();

        tm.finalize();

        assert(counter == 202);
    };

    "large-task-lists"_test = [] {
        irt::task_manager_parameters init{ .thread_number           = 1,
                                           .simple_task_list_number = 1,
                                           .multi_task_list_number  = 0 };

        irt::task_manager tm(init);

        assert(tm.task_lists.ssize() == 1);
        assert(tm.workers.ssize() == 1);

        tm.workers[0].task_lists.emplace_back(&tm.task_lists[0]);

        tm.start();
        int counter = 0;

        for (int i = 0; i < 100; ++i) {
            tm.task_lists[0].add(&function_1, &counter);
            tm.task_lists[0].add(&function_100, &counter);
        }
        tm.task_lists[0].submit();
        tm.task_lists[0].wait();

        for (int i = 0; i < 100; ++i) {
            tm.task_lists[0].add(&function_1, &counter);
            tm.task_lists[0].add(&function_100, &counter);
        }
        tm.task_lists[0].submit();
        tm.task_lists[0].wait();

        for (int i = 0; i < 100; ++i) {
            tm.task_lists[0].add(&function_1, &counter);
            tm.task_lists[0].add(&function_100, &counter);
        }
        tm.task_lists[0].submit();
        tm.task_lists[0].wait();

        for (int i = 0; i < 100; ++i) {
            tm.task_lists[0].add(&function_1, &counter);
            tm.task_lists[0].add(&function_100, &counter);
        }
        tm.task_lists[0].submit();
        tm.task_lists[0].wait();
        tm.finalize();

        assert(counter == 101 * 100 * 4);
    };

    "1-worker-2-task-lists"_test = [] {
        irt::task_manager_parameters init{ .thread_number           = 1,
                                           .simple_task_list_number = 2,
                                           .multi_task_list_number  = 0 };

        irt::task_manager tm(init);

        assert(tm.task_lists.ssize() == 2);
        assert(tm.workers.ssize() == 1);

        tm.workers[0].task_lists.emplace_back(&tm.task_lists[0]);
        tm.workers[0].task_lists.emplace_back(&tm.task_lists[1]);

        tm.start();
        int counter_1 = 0;
        int counter_2 = 0;

        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].submit();
        tm.task_lists[1].submit();

        tm.finalize();

        assert(counter_1 == 4);
        assert(counter_2 == 400);
    };

    "2-worker-2-task-lists"_test = [] {
        irt::task_manager_parameters init{ .thread_number           = 2,
                                           .simple_task_list_number = 2,
                                           .multi_task_list_number  = 0 };

        irt::task_manager tm(init);

        assert(tm.task_lists.ssize() == 2);
        assert(tm.workers.ssize() == 2);

        tm.workers[0].task_lists.emplace_back(&tm.task_lists[0]);
        tm.workers[1].task_lists.emplace_back(&tm.task_lists[1]);

        tm.start();
        int counter_1 = 0;
        int counter_2 = 0;

        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].add(&function_1, &counter_1);
        tm.task_lists[1].add(&function_100, &counter_2);
        tm.task_lists[0].submit();
        tm.task_lists[1].submit();

        tm.finalize();

        assert(counter_1 == 4);
        assert(counter_2 == 400);
    };
}
