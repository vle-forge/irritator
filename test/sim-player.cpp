// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include <irritator/simulation-player.hpp>

#include "world.hpp"

#include <atomic>
#include <random>
#include <thread>

namespace ut = boost::ut;

using namespace wtest;

namespace {

/// The task manager of the tests: one simulation (one pair of lists) and 3
/// unordered workers. A bare `unordered_task_list` is not attached to workers,
/// `submit()` has nobody to wake up.
task_manager& test_tm() noexcept
{
    static task_manager tm(task_manager_options{ 1, 3 });
    static const bool   started = (tm.start(), true);
    (void)started;

    return tm;
}

/// A plain loop of steps, with the observers policy of the player.
bool plain_steps(simulation& sim, const int n) noexcept
{
    for (int i = 0; i != n; ++i) {
        if (not sim.run().has_value())
            return false;

        tick_resamplers_after_run(sim);
    }

    return true;
}

/// What a plain run gives at each step: the reference.
std::vector<bytes> reference(const int steps)
{
    simulation sim;
    world      w;
    build_world(sim, w);

    std::vector<bytes> ret;
    if (not start(sim))
        return ret;

    ret.push_back(dump(sim));
    for (int i = 0; i != steps; ++i) {
        if (not plain_steps(sim, 1))
            break;
        ret.push_back(dump(sim));
    }

    return ret;
}

/// The state after @c n steps of a plain run.
bytes reference_at(const std::size_t n)
{
    simulation sim;
    world      w;
    build_world(sim, w);

    if (not start(sim) or not plain_steps(sim, static_cast<int>(n)))
        return {};

    return dump(sim);
}

} // namespace

namespace ut = boost::ut;

using namespace wtest;
using namespace std::chrono_literals;

int main()
{
    using namespace ut;
    using namespace ut::literals;
    using clock = realtime_pacer::clock;

    static const auto ref = reference(300);

    "player: step forward, then step back one by one"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });

        expect(player.start().has_value());
        expect(same(dump(sim), ref[0], "step 0"));

        for (int i = 1; i <= 100; ++i) {
            expect(player.step_forward().has_value());
            expect(player.step() == static_cast<u64>(i));
            if (not same(dump(sim), ref[i], "forward"))
                break;
        }

        for (int i = 99; i >= 0; --i) {
            expect(player.step_back().has_value());
            expect(player.step() == static_cast<u64>(i));
            if (not same(dump(sim), ref[i], "backward"))
                break;
        }

        expect(not player.can_step_back());
        expect(player.step_back().has_value()) << "nothing at the step 0";
        expect(player.step() == 0_u);
        expect(player.furthest_step() == 100_u);

        // and forward again, in the known part of the history

        expect(player.go_to(100).has_value());
        expect(same(dump(sim), ref[100], "forward again"));
    };

    "player: random access, with sparse keyframes"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);

        // 300 steps, a keyframe every 4 steps, at most 8: the history is
        // thinned several times.
        simulation_player player(sim, tm, 0, { 4, 8 });
        expect(player.start().has_value());
        expect(player.go_to(300).has_value());
        expect(same(dump(sim), ref[300], "300"));
        expect(player.keyframe_number() <= 8_u);

        std::mt19937 rng(11);
        for (int k = 0; k != 60; ++k) {
            const auto n = rng() % 301;
            expect(player.go_to(n).has_value());
            if (not same(dump(sim), ref[n], "random"))
                break;
        }
    };

    "player: step back after a jump, window and keyframes"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 16, 64 });
        expect(player.start().has_value());

        expect(player.go_to(150).has_value());
        for (int i = 0; i != 40; ++i)
            expect(player.step_back().has_value());
        expect(same(dump(sim), ref[110], "110"));

        // forward in the window, then back again
        for (int i = 0; i != 5; ++i)
            expect(player.step_forward().has_value());
        expect(same(dump(sim), ref[115], "115"));
        for (int i = 0; i != 10; ++i)
            expect(player.step_back().has_value());
        expect(same(dump(sim), ref[105], "105"));
    };

    "player: the end of the simulation"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        // The time of this world reaches 1.0 after about 100 steps: the
        // simulation ends before the 300 steps of the reference.
        sim.limits.set_bound(0.0, 0.5);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());
        expect(not player.finished());

        for (int i = 0; i != 1000 and not player.finished(); ++i)
            expect(player.step_forward().has_value());

        expect(player.finished());

        const auto last = player.step();
        expect(last > 0_u and last < 300_u);

        // nothing more
        expect(player.step_forward().has_value());
        expect(player.step() == last);

        // and back from the end
        expect(player.step_back().has_value());
        expect(not player.finished());
        expect(player.step() == last - 1);
        expect(player.step_forward().has_value());
        expect(player.finished());
        expect(player.step() == last);
    };

    "player: advance and retreat with a time budget"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        expect(player.advance(std::chrono::milliseconds(5)).has_value());
        const auto n = player.step();
        expect(n > 0_u) << "at least one step per call";
        expect(same(dump(sim), reference_at(n), "advance"));

        expect(player.retreat(std::chrono::milliseconds(5)).has_value());
        expect(player.step() < n);
        expect(same(dump(sim), reference_at(player.step()), "retreat"));
    };

    "player: max_steps limits the speed of a playback"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        for (int i = 0; i != 5; ++i) {
            const auto before = player.step();
            expect(player.advance(std::chrono::seconds(1), 3).has_value());
            expect(player.step() == before + 3);
        }
        expect(same(dump(sim), reference_at(player.step()), "advance 3"));

        for (int i = 0; i != 4; ++i) {
            const auto before = player.step();
            expect(player.retreat(std::chrono::seconds(1), 2).has_value());
            expect(player.step() == before - 2);
        }
        expect(same(dump(sim), reference_at(player.step()), "retreat 2"));

        expect(player.retreat(std::chrono::seconds(1), 100).has_value());
        expect(player.step() == 0_u);
    };

    "player: forget the future after an edition"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());
        expect(player.go_to(60).has_value());
        expect(player.go_to(20).has_value());
        expect(player.furthest_step() == 60_u);

        expect(player.forget_future().has_value());
        expect(player.furthest_step() == 20_u);
        expect(same(dump(sim), ref[20], "state is kept"));

        // the simulation runs on, from the step 20
        expect(player.go_to(30).has_value());
        expect(same(dump(sim), ref[30], "30"));
        expect(player.go_to(20).has_value());
        expect(same(dump(sim), ref[20], "20"));
    };

    "player: the memory is bounded"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 2, 10 });
        expect(player.start().has_value());
        expect(player.go_to(300).has_value());

        expect(player.keyframe_number() <= 10_u);
        expect(player.memory_usage() < 200u * 1024u);
    };

    // realtime_pacer

    "pacer: target follows the wall clock"_test = [] {
        const auto t0 = clock::now();

        realtime_pacer p(1s);
        p.rebase(10.0, t0);

        expect(p.target(t0) == 10.0);
        expect(std::abs(p.target(t0 + 500ms) - 10.5) < 1e-12);
        expect(std::abs(p.target(t0 + 3s) - 13.0) < 1e-12);

        realtime_pacer fast(10ms);
        fast.rebase(0.0, t0);
        expect(std::abs(fast.target(t0 + 1s) - 100.0) < 1e-9);

        // wall_of is the inverse of target
        expect(p.wall_of(12.0) == t0 + 2s);
        expect(p.wall_of(10.0) == t0);
    };

    "pacer: a new speed does not make the target jump"_test = [] {
        const auto t0 = clock::now();

        realtime_pacer p(1s);
        p.rebase(0.0, t0);

        const auto now = t0 + 2s; // target = 2.0
        p.set_unit_duration(100ms, now);
        expect(std::abs(p.target(now) - 2.0) < 1e-12);
        expect(std::abs(p.target(now + 1s) - 12.0) < 1e-9);
        expect(p.unit_duration() == 100ms);
    };

    "pacer: lateness and resynchronization"_test = [] {
        const auto t0 = clock::now();

        realtime_pacer p(1s);
        p.rebase(0.0, t0);

        const auto now = t0 + 5s; // target = 5.0

        expect(p.lateness(7.0, now) == 0ns) << "not due: not late";
        expect(p.lateness(5.0, now) == 0ns) << "due now: not late";
        expect(p.lateness(3.0, now) == 2s);

        expect(not p.resync_if_late(3.0, 2s, now)) << "just on the limit";
        expect(std::abs(p.target(now) - 5.0) < 1e-12) << "unchanged";

        expect(p.resync_if_late(3.0, 1s, now));
        expect(p.lateness(3.0, now) == 0ns);
        expect(std::abs(p.target(now) - 3.0) < 1e-12) << "on time again";
        expect(std::abs(p.target(now + 1s) - 4.0) < 1e-12);
    };

    "player: advance_until runs the due events, only"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        expect(player.next_time() >= player.current_time());

        // The target does not reach the first event: nothing happens.
        const auto first = player.next_time();
        expect(player.advance_until(first - 1e-6, 1s).has_value());
        expect(player.step() == 0_u);

        double target = first;
        for (int i = 0; i != 20 and not player.finished(); ++i) {
            target += 0.03;
            expect(player.advance_until(target, 1s).has_value());

            // all the due events are done, and no more than the due ones.
            expect(player.current_time() <= target) << "never ahead";
            expect(player.finished() or player.next_time() > target)
              << "nothing due is left";

            // the same target: nothing to do
            const auto step = player.step();
            expect(player.advance_until(target, 1s).has_value());
            expect(player.step() == step);
        }

        expect(player.step() > 0_u);
    };

    "player: advance_until gives the state of the plain run"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());
        expect(player.advance_until(0.4, 1s).has_value());

        const auto n = player.step();
        expect(n > 0_u);

        // the plain run, step by step
        simulation sim2;
        world      w2;
        build_world(sim2, w2);
        expect(start(sim2));
        expect(plain_steps(sim2, static_cast<int>(n)));

        // The only difference: advance_until moves the clock to its target.
        expect(player.current_time() == 0.4);
        expect(sim2.current_time() <= 0.4);
        sim2.current_time(0.4);
        expect(same(dump(sim), dump(sim2), "advance_until"));
    };

    "player: max_steps and the budget limit advance_until"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        expect(player.advance_until(0.9, 1s, 3).has_value());
        expect(player.step() == 3_u);

        // a null budget: one step at least when something is due
        expect(player.advance_until(0.9, 0ns).has_value());
        expect(player.step() == 4_u);
    };

    "player: the end of the simulation in real time"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        sim.limits.set_bound(0.0, 0.5);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        expect(player.advance_until(0.4, 1s).has_value());
        expect(not player.finished());

        // The end is reached when the target reaches it, not before.
        expect(player.advance_until(0.4999, 1s).has_value());
        expect(not player.finished());

        expect(player.advance_until(0.5, 1s).has_value());
        expect(player.finished());
        expect(player.advance_until(100.0, 1s).has_value());
        expect(player.finished());
    };

    "real time: the simulation never goes faster than the clock"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        sim.limits.set_bound(0.0, 0.5);

        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        // one unit of simulation time = 100 ms: 50 ms for the whole run
        realtime_pacer pacer(100ms);
        const auto     t0 = clock::now();
        pacer.rebase(player.current_time(), t0);

        int tasks = 0;
        while (not player.finished() and tasks < 10000) {
            const auto now = clock::now();
            expect(player.advance_until(pacer.target(now), 16ms).has_value());
            expect(player.current_time() <= pacer.target(clock::now()) + 1e-9)
              << "the simulation is never ahead of the real time";

            std::this_thread::sleep_for(1ms); // one "frame"
            ++tasks;
        }

        const auto elapsed = clock::now() - t0;
        expect(player.finished());
        expect(elapsed >= 50ms) << "the run lasts at least its real duration";
        expect(elapsed < 5s);
    };

    "real time: pause and resume without catch-up"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        const auto     t0 = clock::now();
        realtime_pacer pacer(1s);
        pacer.rebase(player.current_time(), t0);

        // 0.2 s later
        expect(player.advance_until(pacer.target(t0 + 200ms), 1s).has_value());
        const auto reached = player.current_time();
        expect(reached <= 0.2 + 1e-9);

        // The user pauses for ten seconds, then resumes: rebase.
        const auto resume = t0 + 10s;
        pacer.rebase(player.current_time(), resume);
        expect(player.advance_until(pacer.target(resume), 1s).has_value());
        expect(player.current_time() == reached) << "no catch-up burst";

        expect(
          player.advance_until(pacer.target(resume + 100ms), 1s).has_value());
        expect(player.current_time() <= reached + 0.1 + 1e-9);
    };

    "live: an empty simulation without end waits, the clock goes on"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        sim.limits.set_bound(0.0, irt::time_domain<irt::time>::infinity);

        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());
        expect(player.is_live());
        expect(not player.finished());
        expect(not player.can_step_forward());

        for (const double target : { 1.0, 2.0, 5.5, 100.0 }) {
            expect(player.advance_until(target, 1s).has_value());
            expect(not player.finished()) << "never finishes";
            expect(player.current_time() == target) << "the clock goes on";
            expect(player.step() == 0_u);
        }

        // the others ways to move do not finish nor spin
        const auto t0 = clock::now();
        expect(player.step_forward().has_value());
        expect(player.go_to(10).has_value());
        expect(player.advance(1s).has_value());
        expect(clock::now() - t0 < 500ms) << "advance does not spin";
        expect(player.step() == 0_u);
        expect(not player.finished());
        expect(player.current_time() == 100.0);

        // the clock never goes back
        expect(player.advance_until(50.0, 1s).has_value());
        expect(player.current_time() == 100.0);
    };

    "live: models added while the simulation waits are taken in charge"_test =
      [] {
          auto&      tm = test_tm();
          simulation sim;
          sim.limits.set_bound(0.0, irt::time_domain<irt::time>::infinity);

          simulation_player player(sim, tm, 0, { 8, 16 });
          expect(player.start().has_value());

          auto add_constant = [&sim](const double offset) {
              auto& c  = sim.alloc<irt::constant>();
              auto  id = sim.get_id(c);
              sim.parameters[get_index(id)].set_constant(1.0, offset);
              expect(sim
                       .make_initialize(*sim.models.try_to_get(id),
                                        sim.current_time())
                       .has_value());
          };

          expect(player.advance_until(2.0, 1s).has_value());
          expect(player.current_time() == 2.0);

          add_constant(0.5); // at 2.0: the event is at 2.5
          expect(player.forget_history().has_value());
          expect(player.earliest_step() == 0_u);
          expect(player.next_time() == 2.5);

          expect(player.advance_until(2.4, 1s).has_value());
          expect(player.step() == 0_u) << "not due";
          expect(player.current_time() == 2.4);

          expect(player.advance_until(2.5, 1s).has_value());
          expect(player.step() == 1_u) << "the event of the new model";
          expect(player.current_time() == 2.5);

          // nothing more to do: it waits again.
          expect(player.advance_until(10.0, 1s).has_value());
          expect(not player.finished());
          expect(player.current_time() == 10.0);
          expect(player.step() == 1_u);

          add_constant(0.25); // at 10.0
          expect(player.forget_history().has_value());
          expect(player.earliest_step() == 1_u) << "the new beginning";
          expect(not player.can_step_back());

          expect(player.advance_until(11.0, 1s).has_value());
          expect(player.step() == 2_u);
          expect(player.current_time() == 11.0);
          expect(player.can_step_back());
      };

    "player: the history restarts after an edition"_test = [] {
        auto edit = [](simulation& sim) {
            auto& c  = sim.alloc<irt::constant>();
            auto  id = sim.get_id(c);
            sim.parameters[get_index(id)].set_constant(1.0, 0.0625);
            expect(
              sim
                .make_initialize(*sim.models.try_to_get(id), sim.current_time())
                .has_value());
        };

        auto&      tm = test_tm();
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, tm, 0, { 4, 8 });
        expect(player.start().has_value());
        expect(player.go_to(20).has_value());

        edit(sim);
        expect(player.forget_history().has_value());
        expect(player.earliest_step() == 20_u);
        expect(player.step() == 20_u);
        expect(not player.can_step_back());

        expect(player.step_back().has_value());
        expect(player.step() == 20_u) << "no step back before the edition";
        expect(player.go_to(3).has_value());
        expect(player.step() == 20_u) << "go_to is limited to the beginning";

        expect(player.go_to(40).has_value());
        expect(player.go_to(25).has_value());

        // The reference: the same steps and the same edition, with no player.
        simulation sim2;
        world      w2;
        build_world(sim2, w2);
        expect(start(sim2));
        expect(plain_steps(sim2, 20));
        edit(sim2);
        expect(plain_steps(sim2, 5));
        expect(same(dump(sim), dump(sim2), "rewind after an edition"));
    };

    "real time: a live simulation follows the clock and waits"_test = [] {
        auto&      tm = test_tm();
        simulation sim;
        sim.limits.set_bound(0.0, irt::time_domain<irt::time>::infinity);
        simulation_player player(sim, tm, 0, { 8, 16 });
        expect(player.start().has_value());

        realtime_pacer pacer(10ms);
        pacer.rebase(player.current_time(), clock::now());

        for (int i = 0; i != 30; ++i) {
            const auto now = clock::now();
            expect(player.advance_until(pacer.target(now), 16ms).has_value());
            expect(player.current_time() == pacer.target(now));
            expect(not player.finished());
            std::this_thread::sleep_for(1ms);
        }

        expect(player.current_time() >= 2.0) << "30 ms at 10 ms per unit";
    };

    /*
     * parallel tick of the observers
     */

    "parallel tick: the same states as the sequential tick"_test = [] {
        // A chunk of one observer: the 5 observers of the world are shared
        // as soon as 2 are due, here at the end of the simulation (all the
        // observers).
        for (const double end : { 0.6 }) {
            auto build = [end](simulation& sim, world& w) {
                build_world(sim, w);
                sim.limits.set_bound(0.0, end);
            };

            std::vector<bytes> plain;
            {
                simulation sim;
                world      w;
                build(sim, w);
                expect(start(sim));
                plain.push_back(dump(sim));
                for (int i = 0; i != 300; ++i) {
                    if (sim.current_time_expired())
                        break;
                    expect(plain_steps(sim, 1));
                    plain.push_back(dump(sim));
                }
            }
            expect(plain.size() > 20_u);

            simulation sim;
            world      w;
            build(sim, w);

            simulation_player_options opt{ 8, 16 };
            opt.observers_per_chunk = 1;

            simulation_player player(sim, test_tm(), 0, opt);
            expect(player.start().has_value());

            for (std::size_t i = 1; i < plain.size(); ++i) {
                expect(player.step_forward().has_value());
                if (not same(dump(sim), plain[i], "forward"))
                    break;
            }
            expect(player.finished());

            // back and forth: the restores and the replays use the parallel
            // tick
            expect(player.go_to(5).has_value());
            expect(same(dump(sim), plain[5], "5"));
            expect(player.go_to(plain.size() - 1).has_value());
            expect(same(dump(sim), plain.back(), "the end"));
            expect(player.step_back().has_value());
            expect(same(dump(sim), plain[plain.size() - 2], "before the end"));
        }
    };

    "parallel tick: near-full observers"_test = [] {
        // A time_func sends an event every 0.01 to 200 counters (two steps by
        // event): after 192 events (3/4 of the raw buffer) all the observers
        // are near-full at the same step (immediate observers).
        constexpr int fan = 200;

        auto build = [](simulation& sim) {
            sim.limits.set_bound(0.0, 1000.0);

            auto&      tf    = sim.alloc<time_func>();
            const auto tf_id = sim.get_id(tf);
            sim.parameters[get_index(tf_id)].copy_from(
              *sim.models.try_to_get(tf_id));

            for (int i = 0; i != fan; ++i) {
                auto& c = sim.alloc<counter>();
                c.type  = counter::observation_type::event_number;

                const auto id = sim.get_id(c);
                sim.parameters[get_index(id)].copy_from(
                  *sim.models.try_to_get(id));

                expect(sim.connect_dynamics(tf, 0, c, 0).has_value());
                expect(
                  sim.observe(*sim.models.try_to_get(id), 0.05).has_value());
            }
        };

        constexpr int steps = 400; // 2 steps by event: near-full at about 382

        std::vector<bytes> plain;
        {
            simulation sim;
            build(sim);
            expect(start(sim));
            for (int i = 1; i <= steps; ++i) {
                expect(plain_steps(sim, 1));
                if (i % 50 == 0 or i >= 380)
                    plain.push_back(dump(sim));
            }
        }

        simulation sim;
        build(sim);

        simulation_player_options opt{ 16, 16 };
        opt.observers_per_chunk = 16; // 13 chunks

        simulation_player player(sim, test_tm(), 0, opt);
        expect(player.start().has_value());

        int    big_steps = 0;
        size_t k         = 0;
        for (int i = 1; i <= steps; ++i) {
            expect(player.step_forward().has_value());
            if (sim.immediate_observers.size() >= 2u * 16u)
                ++big_steps;
            if (i % 50 == 0 or i >= 380)
                if (not same(dump(sim), plain[k++], "near-full"))
                    break;
        }
        expect(big_steps > 0) << "the parallel tick of near-full observers";
        expect(player.step() == static_cast<u64>(steps));

        // a rollback replays the near-full steps with the parallel tick
        expect(player.go_to(370).has_value());
        expect(player.go_to(steps).has_value());
        expect(same(dump(sim), plain.back(), "near-full, replay"));
    };

    "parallel tick: many observers at the end of the simulation"_test = [] {
        constexpr int extra = 300;

        auto build = [](simulation& sim, world& w) {
            build_world(sim, w);
            sim.limits.set_bound(0.0, 0.3);

            for (int i = 0; i != extra; ++i) {
                auto& c = sim.alloc<counter>();
                c.type  = counter::observation_type::event_number;

                const auto id = sim.get_id(c);
                sim.parameters[get_index(id)].copy_from(
                  *sim.models.try_to_get(id));
                expect(
                  sim.observe(*sim.models.try_to_get(id), 0.05).has_value());
            }
        };

        simulation sim2;
        world      w2;
        build(sim2, w2);
        expect(start(sim2));
        int n = 0;
        while (not sim2.current_time_expired() and n < 10000) {
            expect(plain_steps(sim2, 1));
            ++n;
        }
        expect(n > 0);
        const auto expected = dump(sim2);

        simulation sim;
        world      w;
        build(sim, w);

        simulation_player_options opt{ 8, 16 };
        opt.observers_per_chunk = 16; // 19 chunks

        simulation_player player(sim, test_tm(), 0, opt);
        expect(player.start().has_value());
        expect(player.go_to(static_cast<u64>(n)).has_value());
        expect(player.step() == static_cast<u64>(n));
        expect(same(dump(sim), expected, "305 observers"));

        // and again, through a rollback
        expect(player.go_to(3).has_value());
        expect(player.go_to(static_cast<u64>(n)).has_value());
        expect(same(dump(sim), expected, "305 observers, replay"));
    };

    "parallel tick: the task list is optional"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);

        simulation_player player(sim); // nullptr: sequential
        expect(player.start().has_value());
        expect(player.go_to(100).has_value());
        expect(same(dump(sim), ref[100], "no list"));
    };

    "parallel tick: an edition adds observers"_test = [] {
        auto edit = [](simulation& sim) {
            for (int i = 0; i != 100; ++i) {
                auto& c = sim.alloc<counter>();
                c.type  = counter::observation_type::event_number;

                const auto id = sim.get_id(c);
                sim.parameters[get_index(id)].copy_from(
                  *sim.models.try_to_get(id));
                expect(sim
                         .make_initialize(*sim.models.try_to_get(id),
                                          sim.current_time())
                         .has_value());
                expect(
                  sim.observe(*sim.models.try_to_get(id), 0.05).has_value());
            }
        };

        auto run = [&](const bool parallel) {
            simulation sim;
            world      w;
            build_world(sim, w);
            sim.limits.set_bound(0.0, 0.5);

            simulation_player_options opt{ 8, 16 };
            opt.observers_per_chunk = 16;

            simulation_player player = parallel ? simulation_player(
                                                    sim, test_tm(), 0, opt)
                                                : simulation_player(sim, opt);
            expect(player.start().has_value());
            expect(player.go_to(10).has_value());

            edit(sim);
            expect(player.forget_history().has_value());
            expect(player.go_to(1000).has_value());
            expect(player.finished());

            return dump(sim);
        };

        expect(same(run(true), run(false), "parallel = sequential"));
    };

    "parallel tick: one worker, the second pair of lists"_test = [] {
        // Two simulations (two pairs of lists) and a single unordered worker:
        // the player uses the list of its index, and one helper at most.
        task_manager tm2(task_manager_options{ 2, 1 });
        tm2.start();
        expect(tm2.list_size() == 2_u);
        expect(tm2.unordered_worker_size() == 1_u);

        auto build = [](simulation& sim) {
            world w;
            build_world(sim, w);
            sim.limits.set_bound(0.0, 0.3);

            for (int i = 0; i != 120; ++i) {
                auto& c = sim.alloc<counter>();
                c.type  = counter::observation_type::event_number;

                const auto id = sim.get_id(c);
                sim.parameters[get_index(id)].copy_from(
                  *sim.models.try_to_get(id));
                expect(
                  sim.observe(*sim.models.try_to_get(id), 0.05).has_value());
            }
        };

        simulation ref_sim;
        build(ref_sim);
        expect(start(ref_sim));
        u64 n = 0;
        while (not ref_sim.current_time_expired() and n < 10000) {
            expect(plain_steps(ref_sim, 1));
            ++n;
        }
        const auto expected = dump(ref_sim);

        simulation_player_options opt{ 8, 16 };
        opt.observers_per_chunk = 8;

        for (const size_t index : { size_t{ 0 }, size_t{ 1 }, size_t{ 2 } }) {
            // index 2 is out of range: sequential tick
            simulation sim;
            build(sim);

            simulation_player player(sim, tm2, index, opt);
            expect(player.start().has_value());
            expect(player.go_to(n).has_value());
            expect(same(dump(sim), expected, "list index"));
        }
    };
}
