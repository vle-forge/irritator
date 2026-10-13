// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include <irritator/simulation-player.hpp>

#include "world.hpp"

#include <random>

namespace ut = boost::ut;

using namespace wtest;

namespace {

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
        if (not run_steps(sim, 1))
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

    if (not start(sim) or not run_steps(sim, static_cast<int>(n)))
        return {};

    return dump(sim);
}

} // namespace

int main()
{
    using namespace ut;
    using namespace ut::literals;

    static const auto ref = reference(300);

    "player: step forward, then step back one by one"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, { 8, 16 });

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
        simulation sim;
        world      w;
        build_world(sim, w);

        // 300 steps, a keyframe every 4 steps, at most 8: the history is
        // thinned several times.
        simulation_player player(sim, { 4, 8 });
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
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, { 16, 64 });
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
        simulation sim;
        world      w;
        build_world(sim, w);
        // The time of this world reaches 1.0 after about 100 steps: the
        // simulation ends before the 300 steps of the reference.
        sim.limits.set_bound(0.0, 0.5);
        simulation_player player(sim, { 8, 16 });
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
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, { 8, 16 });
        expect(player.start().has_value());

        expect(player.advance(std::chrono::milliseconds(5)).has_value());
        const auto n = player.step();
        expect(n > 0_u) << "at least one step per call";
        expect(same(dump(sim), reference_at(n), "advance"));

        expect(player.retreat(std::chrono::milliseconds(5)).has_value());
        expect(player.step() < n);
        expect(same(dump(sim), reference_at(player.step()), "retreat"));
    };

    "player: forget the future after an edition"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, { 8, 16 });
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
        simulation sim;
        world      w;
        build_world(sim, w);
        simulation_player player(sim, { 2, 10 });
        expect(player.start().has_value());
        expect(player.go_to(300).has_value());

        expect(player.keyframe_number() <= 10_u);
        expect(player.memory_usage() < 200u * 1024u);
    };
}
