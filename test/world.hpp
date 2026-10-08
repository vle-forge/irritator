// A simulation that uses most of what the binary serialization has to keep:
// QSS models in a loop, generators that read a random and a constant external
// source, a queue (dated messages), a hierarchical state machine, counters,
// observers with a history.

#pragma once

#include <irritator/binary-simulation.hpp>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace wtest {

using namespace irt;

struct world {
    model_id integ, gain, gen_rnd, gen_cst, queue_id, cnt_queue, cnt_cst,
      cnt_hsm, hsm_model;
    random_source_id   rnd;
    constant_source_id cst;
    hsm_id             machine;
};

inline void build_world(simulation& sim, world& w) noexcept
{
    using H = hierarchical_state_machine;

    sim.limits.set_bound(0.0, 1000.0);

    // external sources

    sim.srcs.random_sources.reserve(8);
    sim.srcs.constant_sources.reserve(8);

    auto& rs        = sim.srcs.random_sources.alloc();
    rs.distribution = distribution_type::uniform_real;
    rs.reals        = { 0.1, 1.0 };
    rs.ints         = { 0, 0 };
    rs.name         = "uniform";
    w.rnd           = sim.srcs.random_sources.get_id(rs);

    auto& cs  = sim.srcs.constant_sources.alloc();
    cs.name   = "ta";
    cs.length = 3;
    cs.buffer.fill(0.0);
    cs.buffer[0] = 0.5;
    cs.buffer[1] = 0.25;
    cs.buffer[2] = 0.75;
    w.cst        = sim.srcs.constant_sources.get_id(cs);

    // state machine

    auto& h   = sim.hsms.alloc();
    w.machine = sim.hsms.get_id(h);
    h.set_state(0, H::invalid_state_id, 1);
    h.set_state(1, 0, H::invalid_state_id);
    h.set_state(2, 0, H::invalid_state_id);
    h.constants[0] = 0.5;
    h.states[1].enter_action.set_affect(H::variable::var_timer, 1.0f);
    h.states[1].condition.set_timer();
    h.states[1].if_transition = 2;
    h.states[1].if_action.set_output(H::variable::port_0, 3.0f);
    h.states[2].enter_action.set_affect(H::variable::var_timer, 0.75f);
    h.states[2].condition.set_timer();
    h.states[2].if_transition = 1;
    h.states[2].if_action.set_output(H::variable::port_1, 4.0f);

    // models

    auto& integ = sim.alloc<qss2_integrator>();
    auto& gain  = sim.alloc<qss2_gain>();
    auto& gr    = sim.alloc<generator>();
    auto& gc    = sim.alloc<generator>();
    auto& q     = sim.alloc<irt::queue>();
    auto& c1    = sim.alloc<counter>();
    auto& c2    = sim.alloc<counter>();
    auto& c3    = sim.alloc<counter>();
    auto& hw    = sim.alloc<hsm_wrapper>();

    w.integ     = sim.get_id(integ);
    w.gain      = sim.get_id(gain);
    w.gen_rnd   = sim.get_id(gr);
    w.gen_cst   = sim.get_id(gc);
    w.queue_id  = sim.get_id(q);
    w.cnt_queue = sim.get_id(c1);
    w.cnt_cst   = sim.get_id(c2);
    w.cnt_hsm   = sim.get_id(c3);
    w.hsm_model = sim.get_id(hw);

    sim.parameters[get_index(w.integ)].set_integrator(1.0, 0.01);
    sim.parameters[get_index(w.gain)].set_gain(-1.0);
    sim.parameters[get_index(w.queue_id)].set_queue(0.5);

    gr.flags.set(generator::option::ta_use_source, true);
    gr.flags.set(generator::option::value_use_source, true);
    gr.source_ta.reset(source_type::random, source_any_id(w.rnd));
    gr.source_value.reset(source_type::random, source_any_id(w.rnd));

    gc.flags.set(generator::option::ta_use_source, true);
    gc.source_ta.reset(source_type::constant, source_any_id(w.cst));
    gc.value = 1.5;

    c1.type = counter::observation_type::event_number;
    c2.type = counter::observation_type::sum_value;
    c3.type = counter::observation_type::last_value;

    hw.id = w.machine;

    // simulation::initialize() copies the parameters into the dynamics: what
    // was set directly in the dynamics above must be in the parameters too.

    for (const auto id : { w.gen_rnd, w.gen_cst, w.cnt_queue, w.cnt_cst,
                           w.cnt_hsm, w.hsm_model })
        sim.parameters[get_index(id)].copy_from(*sim.models.try_to_get(id));

    // connections

    [[maybe_unused]] auto s = sim.connect_dynamics(integ, 0, gain, 0);
    s = sim.connect_dynamics(gain, 0, integ, 0);
    s = sim.connect_dynamics(gr, 0, q, 0);
    s = sim.connect_dynamics(q, 0, c1, 0);
    s = sim.connect_dynamics(gc, 0, c2, 0);
    s = sim.connect_dynamics(hw, 0, c3, 0);

    // observers

    for (const auto id : { w.integ, w.gain, w.cnt_queue, w.cnt_cst,
                           w.cnt_hsm }) {
        auto* mdl = sim.models.try_to_get(id);
        sim.observe(*mdl, 0.05);
        sim.observers.get<observer_name>(mdl->obs_id) = "obs";
    }
}

inline bool start(simulation& sim) noexcept
{
    const auto ret = sim.initialize();
    if (not ret.has_value())
        std::fprintf(stderr,
                     "simulation::initialize failed: category %d, value %d\n",
                     static_cast<int>(ret.error().cat()),
                     static_cast<int>(ret.error().value()));

    return ret.has_value();
}

/// @return false after an error of the simulation.
inline bool run_steps(simulation& sim, int n) noexcept
{
    for (int i = 0; i != n; ++i) {
        if (not sim.run().has_value())
            return false;

        sim.tick_resamplers();
    }

    return true;
}

using bytes = std::vector<u8>;

/// Comparison without printing megabytes when it fails.
inline bool same(const std::vector<u8>& a, const std::vector<u8>& b, const char* what = "")
{
    if (a == b)
        return true;

    std::size_t i = 0;
    while (i < a.size() and i < b.size() and a[i] == b[i])
        ++i;

    std::printf("DIFFERENT %s: sizes %zu %zu, first difference at %zu\n", what,
                a.size(), b.size(), i);
    return false;
}

inline bytes copy_of(std::span<const u8> s) { return bytes(s.begin(), s.end()); }

inline bytes dump(const simulation& sim)
{
    binary_writer w;
    if (not write_simulation(w, sim).has_value())
        return {};

    return copy_of(w.bytes());
}

inline bytes snapshot(const simulation& sim)
{
    binary_writer w;
    if (not write_snapshot(w, sim).has_value())
        return {};

    return copy_of(w.bytes());
}

} // namespace wtest
