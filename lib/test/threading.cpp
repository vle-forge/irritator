// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <fmt/format.h>

#include <boost/ut.hpp>

void function_1(void* param) noexcept
{
    fmt::print("F1\n");
    auto* counter = reinterpret_cast<int*>(param);
    std::this_thread::sleep_for(std::chrono::microseconds{ 10 });
    (*counter) += 1;
    fmt::print("-> {}\n", *counter);
}

void function_100(void* param) noexcept
{
    fmt::print("F100\n");
    auto* counter = reinterpret_cast<int*>(param);
    std::this_thread::sleep_for(std::chrono::microseconds{ 10 });
    (*counter) += 100;
    fmt::print("-> {}\n", *counter);
}

int main()
{
    using namespace boost::ut;

    "spin-lock"_test = [] {
        int            counter = 0;
        irt::spin_lock spin;

        std::jthread j1([&counter, &spin]() {
            for (int i = 0; i < 1000; ++i) {
                {
                    irt::scoped_spin_lock lock{ spin };
                    ++counter;
                }
                std::this_thread::yield();
            }
        });

        std::jthread j2([&counter, &spin]() {
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

        irt::task_manager tm;
        irt::status       ret = tm.init(init);
        expect(irt::is_success(ret));

        expect(tm.task_lists.ssize() == 1);
        expect(tm.workers.ssize() == 1);

        tm.start();
        int counter = 0;
        tm.workers[0].task_lists.emplace_back(&tm.task_lists[0]);
        tm.task_lists[0].worker = &tm.workers[0];

        tm.task_lists[0].add(&function_1, &counter);
        tm.task_lists[0].add(&function_100, &counter);
        tm.task_lists[0].add(&function_1, &counter);
        tm.task_lists[0].add(&function_100, &counter);
        fmt::print("tm1: {}\n", tm.task_lists[0].task_number);
        expect(tm.task_lists[0].task_number == 4);
        expect(tm.task_lists[0].task_submit == 0);

        tm.task_lists[0].submit();
        expect(tm.task_lists[0].task_number == 0);
        expect(tm.task_lists[0].task_submit == 4);

        tm.task_lists[0].wait();
        expect(tm.task_lists[0].task_number == 0);
        expect(tm.task_lists[0].task_submit == 0);

        fmt::print("counter: {}\n", counter);
        expect(counter == 202);

        fmt::print("tm2: {}\n", tm.task_lists[0].task_number);
        expect(tm.task_lists[0].task_number == 0);
        tm.finalize();
    };
}