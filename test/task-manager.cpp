// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/thread.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/ut.hpp>

using namespace std::chrono_literals;

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

// Busy wait: a duration that is a lower bound whatever the timer granularity.
static void spin_for(std::chrono::microseconds d) noexcept
{
    const auto end = std::chrono::steady_clock::now() + d;
    while (std::chrono::steady_clock::now() < end)
        ;
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

// ---------------------------------------------------------------------------

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
        irt::spin_mutex mutex_1;
        irt::spin_mutex mutex_2;

        for (int i = 0; i < 30; ++i) {
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

    // -----------------------------------------------------------------------
    // Task system: ordered lists
    //
    // Rules of these tests:
    //  - the number of workers is explicit (the default is
    //    hardware_concurrency() threads, which is slow to start on Windows);
    //  - no sleep_for(): the tests wait for events (wait_empty,
    //    wait_completion, wait_until) or count operations;
    //  - `expect` is only called from the test thread, never from a task.
    // -----------------------------------------------------------------------

    "ordered: tasks run once, in submission order"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        std::vector<int> order; // written by the worker only, read after wait
        for (int i = 0; i < 200; ++i)
            expect(tm.ordered(0).add([&order, i] { order.push_back(i); }));

        tm.ordered(0).wait_empty();

        expect(eq(order.size(), std::size_t{ 200 }));
        bool sorted = true;
        for (int i = 0; i < static_cast<int>(order.size()); ++i)
            sorted = sorted && order[static_cast<std::size_t>(i)] == i;
        expect(sorted) << "FIFO order broken";

        expect(eq(tm.ordered(0).tasks_submitted(), irt::u64{ 200 }));
        expect(eq(tm.ordered(0).tasks_completed(), irt::u64{ 200 }));
        tm.shutdown();
    };

    "ordered: wait_empty on an idle list returns immediately"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();
        tm.ordered(0).wait_empty();
        tm.ordered(0).wait_empty();
        expect(eq(tm.ordered(0).tasks_submitted(), irt::u64{ 0 }));
        tm.shutdown();
    };

    "ordered: add / wait_empty cycles"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        std::atomic_int counter = 0;
        for (int i = 0; i < 30; ++i) {
            tm.ordered(0).add([&counter] { function_1(counter); });
            tm.ordered(0).add([&counter] { function_100(counter); });
            tm.ordered(0).add([&counter] { function_1(counter); });
            tm.ordered(0).add([&counter] { function_100(counter); });
            tm.ordered(0).wait_empty();
            expect(eq(counter.load(), (i + 1) * 202));
        }
        tm.shutdown();
    };

    // The queue holds up to 200 tasks (< task_max) that the worker has not
    // started yet: add() must wake the worker without any submit().
    "ordered: bursts close to the capacity of the queue"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        constexpr int burst = 200;
        static_assert(burst < irt::ordered_task_list::task_max);

        std::atomic_int counter = 0;
        for (int x = 0; x < 5; ++x) {
            for (int i = 0; i < burst / 2; ++i) {
                tm.ordered(0).add([&counter] { function_1(counter); });
                tm.ordered(0).add([&counter] { function_100(counter); });
            }
            tm.ordered(0).wait_empty();
            expect(eq(counter.load(), (x + 1) * (burst / 2) * 101));
        }
        tm.shutdown();
    };

    "ordered: add refuses a task when the queue is full"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        std::atomic<bool> running{ false };
        std::atomic<bool> release{ false };
        std::atomic<int>  executed{ 0 };

        // Keeps the worker busy: the queue itself is then empty.
        tm.ordered(0).add([&] {
            running = true;
            while (!release.load())
                std::this_thread::yield();
        });
        expect(wait_until([&] { return running.load(); }));

        constexpr int attempts = 300;
        int           accepted = 0;
        for (int i = 0; i < attempts; ++i)
            if (tm.ordered(0).add([&executed] { ++executed; }))
                ++accepted;

        // The ring buffer keeps one slot free: task_max - 1 usable slots.
        expect(
          ge(accepted, static_cast<int>(irt::ordered_task_list::task_max) - 1));
        expect(accepted < attempts) << "add() accepted more than the capacity";

        // Rejected tasks are not counted: wait_empty() cannot wait for them.
        expect(eq(tm.ordered(0).tasks_submitted(),
                  static_cast<irt::u64>(1 + accepted)));

        release = true;
        tm.ordered(0).wait_empty();

        expect(eq(executed.load(), accepted));
        expect(
          eq(tm.ordered(0).tasks_completed(), tm.ordered(0).tasks_submitted()));
        tm.shutdown();
    };

    "ordered: concurrent producers"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        constexpr int            producers = 3;
        constexpr int            per_prod  = 80; // 240 < task_max
        std::atomic_int          counter   = 0;
        std::atomic_int          refused   = 0;
        std::atomic<int>         ready{ 0 };
        std::atomic<bool>        go{ false };
        std::vector<std::thread> threads;

        for (int p = 0; p < producers; ++p)
            threads.emplace_back([&] {
                ++ready;
                while (!go.load())
                    std::this_thread::yield();
                for (int i = 0; i < per_prod; ++i)
                    if (!tm.ordered(0).add([&counter] { ++counter; }))
                        ++refused;
            });

        expect(wait_until([&] { return ready.load() == producers; }));
        go = true;
        for (auto& t : threads)
            t.join();

        tm.ordered(0).wait_empty();
        expect(eq(refused.load(), 0));
        expect(eq(counter.load(), producers * per_prod));
        expect(eq(tm.ordered(0).tasks_completed(),
                  irt::u64{ producers * per_prod }));
        tm.shutdown();
    };

    "ordered: two lists run independently and in parallel"_test = [] {
        irt::task_manager tm(2, 0, 1);
        tm.start();

        // Each task waits for the other one: this only terminates if the two
        // lists really run at the same time.
        std::atomic<bool> a_in{ false };
        std::atomic<bool> b_in{ false };
        std::atomic<bool> a_saw_b{ false };
        std::atomic<bool> b_saw_a{ false };

        tm.ordered(0).add([&] {
            a_in    = true;
            a_saw_b = wait_until([&] { return b_in.load(); });
        });
        tm.ordered(1).add([&] {
            b_in    = true;
            b_saw_a = wait_until([&] { return a_in.load(); });
        });

        tm.ordered(0).wait_empty();
        tm.ordered(1).wait_empty();

        expect(a_saw_b.load());
        expect(b_saw_a.load());
        tm.shutdown();
    };

    "ordered: two lists, balanced +1 / -1"_test = [] {
        irt::task_manager tm(2, 0, 1);
        std::atomic_int   buffer = 0;
        tm.start();

        for (int x = 0; x < 20; ++x) {
            for (int i = 0; i < 100; ++i) {
                tm.ordered(0).add([&buffer] { buffer.fetch_add(1); });
                tm.ordered(1).add([&buffer] { buffer.fetch_sub(1); });
            }
            tm.ordered(0).wait_empty();
            tm.ordered(1).wait_empty();
        }

        expect(eq(buffer.load(), 0));
        expect(eq(tm.ordered(0).tasks_completed(), irt::u64{ 2000 }));
        expect(eq(tm.ordered(1).tasks_completed(), irt::u64{ 2000 }));
        tm.shutdown();
    };

    "ordered: worker statistics"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        for (int i = 0; i < 3; ++i)
            tm.ordered(0).add([] { spin_for(2ms); });
        tm.ordered(0).wait_empty();

        // Updated before wait_empty() returns.
        expect(eq(tm.wordered_tasks_completed(0), irt::u64{ 3 }));
        // The execution time (ms) was the number of tasks before the fix.
        expect(ge(tm.wordered_execution_time(0), irt::u64{ 6 }));
        expect(tm.wordered_execution_time(0) < irt::u64{ 5000 });
        tm.shutdown();
    };

    // The worker must update its statistics BEFORE the list publishes the
    // completion (wait_empty() relies on it). The observer spins on the list
    // counter instead of sleeping on the condition variable, to catch the few
    // nanoseconds between the two updates.
    "ordered: statistics are published before the completion"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        int stale = 0;
        for (int i = 0; i < 3000; ++i) {
            tm.ordered(0).add([] {});
            while (tm.ordered(0).tasks_completed() !=
                   static_cast<irt::u64>(i + 1))
                std::this_thread::yield();
            stale += tm.wordered_tasks_completed(0) !=
                     static_cast<irt::u64>(i + 1);
        }
        expect(eq(stale, 0)) << "worker statistics published too late";
        tm.shutdown();
    };

    "ordered: statistics can be read while the worker runs"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        std::atomic<bool>     stop{ false };
        std::atomic<irt::u64> violations{ 0 };
        std::atomic<irt::u64> polls{ 0 };

        std::thread poller([&] {
            irt::u64 last = 0;
            while (!stop.load()) {
                // `completed` first: completed <= submitted at every instant.
                const auto completed = tm.ordered(0).tasks_completed();
                const auto submitted = tm.ordered(0).tasks_submitted();
                const auto wcomp     = tm.wordered_tasks_completed(0);
                if (completed > submitted || completed < last)
                    ++violations;
                (void)wcomp;
                (void)tm.wordered_execution_time(0);
                last = completed;
                ++polls;
            }
        });

        for (int i = 0; i < 1500; ++i)
            tm.ordered(0).add([] {});
        tm.ordered(0).wait_empty();
        // keep the poller running until it has observed something
        expect(wait_until([&] { return polls.load() > 10; }));

        stop = true;
        poller.join();
        expect(eq(violations.load(), irt::u64{ 0 }));
        tm.shutdown();
    };

    "ordered: shutdown wakes wait_empty"_test = [] {
        irt::task_manager tm(1, 0, 1);
        tm.start();

        std::atomic<bool> running{ false };
        std::atomic<bool> release{ false };
        std::atomic<bool> woken{ false };

        tm.ordered(0).add([&] {
            running = true;
            while (!release.load())
                std::this_thread::yield();
        });
        expect(wait_until([&] { return running.load(); }));

        std::atomic<bool> entered{ false };
        std::thread       waiter([&] {
            entered = true;
            tm.ordered(0).wait_empty();
            woken = true;
        });
        expect(wait_until([&] { return entered.load(); }));
        std::this_thread::sleep_for(20ms); // let the waiter block (once)
        expect(!woken.load()) << "wait_empty() returned with a task running";

        tm.ordered(0).shutdown();
        const bool ok = wait_until([&] { return woken.load(); });
        release       = true; // always, so that the test cannot hang
        waiter.join();
        expect(ok) << "wait_empty() was not woken by shutdown()";
        tm.shutdown();
    };

    "ordered: shutdown drains the accepted tasks, then refuses new ones"_test =
      [] {
          irt::task_manager tm(1, 0, 1);
          tm.start();

          std::atomic_int counter = 0;
          for (int i = 0; i < 100; ++i)
              tm.ordered(0).add([&counter] { ++counter; });

          tm.shutdown(); // joins the worker after the drain

          expect(eq(counter.load(), 100));
          expect(tm.ordered(0).stopping());
          expect(!tm.ordered(0).add([&counter] { ++counter; }));
          expect(eq(counter.load(), 100));
          tm.shutdown(); // idempotent
      };

    // -----------------------------------------------------------------------
    // Task system: unordered lists
    // -----------------------------------------------------------------------

    "unordered: every task of a batch runs exactly once"_test = [] {
        irt::task_manager tm(0, 1, 2);
        tm.start();

        constexpr int                n = 1000;
        std::vector<std::atomic_int> hits(n);
        for (auto& h : hits)
            h = 0;

        for (int i = 0; i < n; ++i)
            tm.unordered(0).add(
              [&hits, i] { ++hits[static_cast<std::size_t>(i)]; });
        tm.unordered(0).submit();
        tm.unordered(0).wait_completion();

        int wrong = 0;
        for (auto& h : hits)
            wrong += h.load() != 1;
        expect(eq(wrong, 0));

        expect(eq(tm.unordered(0).tasks_submitted(), irt::u64{ n }));
        expect(eq(tm.unordered(0).tasks_completed(), irt::u64{ n }));

        // Per-worker statistics are up to date when wait_completion returns.
        irt::u64 total = 0;
        for (std::size_t w = 0; w < tm.wunordered_size(); ++w)
            total += tm.wunordered_tasks_completed(w);
        expect(eq(total, irt::u64{ n }));
        tm.shutdown();
    };

    "unordered: the workers run in parallel"_test = [] {
        irt::task_manager tm(0, 1, 2);
        tm.start();

        // Each of the two tasks waits for the other one: sequential execution
        // by a single thread cannot pass.
        std::atomic<int> inside{ 0 };
        std::atomic<int> met{ 0 };

        for (int i = 0; i < 2; ++i)
            tm.unordered(0).add([&] {
                ++inside;
                if (wait_until([&] { return inside.load() == 2; }))
                    ++met;
            });
        tm.unordered(0).submit();
        tm.unordered(0).wait_completion();

        expect(eq(met.load(), 2));
        tm.shutdown();
    };

    "unordered: many small batches"_test = [] {
        irt::task_manager tm(0, 1, 2);
        tm.start();

        for (int x = 0; x < 100; ++x) {
            std::atomic_int counter_1 = 0;
            std::atomic_int counter_2 = 0;

            for (int i = 0; i < 4; ++i) {
                tm.unordered(0).add([&counter_1] { function_1(counter_1); });
                tm.unordered(0).add([&counter_2] { function_100(counter_2); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();

            expect(eq(counter_1.load(), 4));
            expect(eq(counter_2.load(), 400));
        }
        tm.shutdown();
    };

    // One worker, one task per batch: the worker goes back to sleep between
    // two batches, a lost wake-up blocks the next batch for ever. The driver
    // spins on the list counter (instead of sleeping in wait_completion()) so
    // that the next submit() races with the worker going back to sleep. The
    // worker statistics must be published before the completion.
    "unordered: no lost wake-up, statistics published first"_test = [] {
        irt::task_manager tm(0, 1, 1);
        tm.start();

        std::atomic_int counter = 0;
        int             stale   = 0;
        constexpr int   batches = 3000;

        for (int i = 0; i < batches; ++i) {
            tm.unordered(0).add([&counter] { ++counter; });
            tm.unordered(0).submit();
            while (tm.unordered(0).tasks_completed() !=
                   static_cast<irt::u64>(i + 1))
                std::this_thread::yield();
            stale += tm.wunordered_tasks_completed(0) !=
                     static_cast<irt::u64>(i + 1);
        }
        tm.unordered(0).wait_completion();

        expect(eq(counter.load(), batches));
        expect(eq(stale, 0)) << "worker statistics published too late";
        tm.shutdown();
    };

    "unordered: large batches"_test = [] {
        irt::task_manager tm(1, 1, 2);
        tm.start();

        for (int n = 0; n < 10; ++n) {
            std::atomic_int counter = 0;

            for (int i = 0; i < 100; ++i) {
                tm.unordered(0).add([&counter] { function_1(counter); });
                tm.unordered(0).add([&counter] { function_100(counter); });
            }
            tm.unordered(0).submit();
            tm.unordered(0).wait_completion();
            expect(eq(counter.load(), 101 * 100));
        }
        tm.shutdown();
    };

    "unordered: empty batch and wait_completion without submit"_test = [] {
        irt::task_manager tm(0, 1, 1);
        tm.start();

        tm.unordered(0).wait_completion();
        tm.unordered(0).submit(); // nothing to run
        tm.unordered(0).wait_completion();
        expect(eq(tm.unordered(0).tasks_submitted(), irt::u64{ 0 }));
        expect(eq(tm.unordered(0).tasks_completed(), irt::u64{ 0 }));

        std::atomic_int counter = 0;
        tm.unordered(0).add([&counter] { ++counter; });
        tm.unordered(0).submit();
        tm.unordered(0).wait_completion();
        expect(eq(counter.load(), 1));
        tm.shutdown();
    };

    "unordered: tasks_completed counts finished tasks only"_test = [] {
        irt::task_manager tm(0, 1, 1);
        tm.start();

        std::atomic<bool> running{ false };
        std::atomic<bool> release{ false };

        tm.unordered(0).add([&] {
            running = true;
            while (!release.load())
                std::this_thread::yield();
        });
        tm.unordered(0).submit();
        expect(wait_until([&] { return running.load(); }));

        // The task is stolen but not finished.
        expect(eq(tm.unordered(0).tasks_submitted(), irt::u64{ 1 }));
        const auto during = tm.unordered(0).tasks_completed();

        release = true;
        tm.unordered(0).wait_completion();

        expect(eq(during, irt::u64{ 0 }));
        expect(eq(tm.unordered(0).tasks_completed(), irt::u64{ 1 }));
        tm.shutdown();
    };

    "unordered: a task added during a batch is kept for the next one"_test =
      [] {
          irt::task_manager tm(0, 1, 1);
          tm.start();

          std::atomic<bool> running{ false };
          std::atomic<bool> release{ false };
          std::atomic_int   late = 0;

          tm.unordered(0).add([&] {
              running = true;
              while (!release.load())
                  std::this_thread::yield();
          });
          tm.unordered(0).submit();
          expect(wait_until([&] { return running.load(); }));

          expect(tm.unordered(0).add([&late] { ++late; })); // batch executing

          release = true;
          tm.unordered(0).wait_completion();
          expect(eq(late.load(), 0)) << "must not run in the current batch";

          tm.unordered(0).submit(); // the next batch
          tm.unordered(0).wait_completion();
          expect(eq(late.load(), 1)) << "task lost";

          expect(eq(tm.unordered(0).tasks_submitted(), irt::u64{ 2 }));
          expect(eq(tm.unordered(0).tasks_completed(), irt::u64{ 2 }));
          tm.shutdown();
      };

    "unordered: shutdown wakes wait_completion and abandons the queue"_test =
      [] {
          irt::task_manager tm(0, 1, 1);
          tm.start();

          std::atomic<bool> running{ false };
          std::atomic<bool> release{ false };
          std::atomic<bool> woken{ false };
          std::atomic_int   abandoned = 0;

          tm.unordered(0).add([&] {
              running = true;
              while (!release.load())
                  std::this_thread::yield();
          });
          tm.unordered(0).add([&abandoned] { ++abandoned; }); // never started
          tm.unordered(0).submit();
          expect(wait_until([&] { return running.load(); }));

          std::atomic<bool> entered{ false };
          std::thread       waiter([&] {
              entered = true;
              tm.unordered(0).wait_completion();
              woken = true;
          });
          expect(wait_until([&] { return entered.load(); }));
          std::this_thread::sleep_for(20ms); // let the waiter block (once)
          expect(!woken.load()) << "wait_completion() returned too early";

          tm.unordered(0).shutdown();
          const bool ok = wait_until([&] { return woken.load(); });
          release       = true;
          waiter.join();
          tm.shutdown();

          expect(ok) << "wait_completion() was not woken by shutdown()";
          expect(eq(abandoned.load(), 0));
          expect(!tm.unordered(0).add([] {}));
      };

    // -----------------------------------------------------------------------
    // Task system: manager
    // -----------------------------------------------------------------------

    "manager: number of workers"_test = [] {
        irt::task_manager a(1, 1, 3);
        expect(eq(a.wordered_size(), std::size_t{ 1 }));
        expect(eq(a.wunordered_size(), std::size_t{ 3 }));

        irt::task_manager b(1, 1, 0); // 0 means one worker
        expect(eq(b.wunordered_size(), std::size_t{ 1 }));

        irt::task_manager c(2, 0, 4); // no list: no thread to create
        expect(eq(c.wordered_size(), std::size_t{ 2 }));
        expect(eq(c.wunordered_size(), std::size_t{ 0 }));
    };

    "manager: start and shutdown cycles"_test = [] {
        for (int i = 0; i < 10; ++i) {
            irt::task_manager tm(1, 1, 2);
            tm.start();
            tm.start(); // idempotent

            std::atomic_int counter = 0;
            tm.ordered(0).add([&counter] { ++counter; });
            tm.unordered(0).add([&counter] { ++counter; });
            tm.unordered(0).submit();
            tm.ordered(0).wait_empty();
            tm.unordered(0).wait_completion();

            expect(eq(counter.load(), 2));
            tm.shutdown();
        }
    };

    "manager: destructor joins the workers"_test = [] {
        std::atomic_int counter = 0;
        {
            irt::task_manager tm(1, 1, 2);
            tm.start();
            for (int i = 0; i < 50; ++i)
                tm.ordered(0).add([&counter] { ++counter; });
            // no shutdown(): it was a std::terminate (joinable std::thread)
        }
        expect(eq(counter.load(), 50)) << "ordered tasks are drained";
    };

    "manager: shutdown before start does not hang"_test = [] {
        irt::task_manager tm(1, 1, 1);
        tm.shutdown();
        expect(tm.ordered(0).stopping());
        expect(tm.unordered(0).stopping());
    };

    // -----------------------------------------------------------------------
    // shared_buffer used from the tasks
    // -----------------------------------------------------------------------

    "shared_buffer: readers and a writer in the task manager"_test = [] {
        irt::task_manager tm(2, 0, 1);
        tm.start();

        irt::shared_buffer<irt::small_vector<int, 16>> buffer;
        std::atomic_int                                bad = 0;

        auto read_back = [&buffer, &bad] {
            buffer.read([&bad](const auto& v, auto /*version*/) {
                for (int x : v)
                    if (x != 10)
                        ++bad;
            });
        };

        for (int i = 0; i < 16; ++i) {
            tm.ordered(0).add(read_back);
            tm.ordered(1).add(
              [&buffer] { buffer.write([](auto& v) { v.push_back(10); }); });
            tm.ordered(0).add(read_back);
        }

        tm.ordered(0).wait_empty();
        tm.ordered(1).wait_empty();

        expect(eq(bad.load(), 0));
        const auto [size,
                    version] = buffer.read([](const auto& v, irt::u64 ver) {
            return std::pair{ v.size(), ver };
        });
        expect(eq(size, std::size_t{ 16 }));
        expect(eq(version, irt::u64{ 16 }));
        tm.shutdown();
    };
}