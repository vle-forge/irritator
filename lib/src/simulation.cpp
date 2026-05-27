// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/observation.hpp>

namespace irt {

//                 +------------------+
//                 |simulation wrapper|
//                 +------------------+
//                 |                  |
//                 |                  |
//                 |> init            |
//                 |                  |
//                 |> run             |
//                 |                  |
//                 |                  |
//                 |                  |
//                 |                  |
//                 |                  |
//                 |> public          |> public
//                 |  parameters      |  parameters
//                 |> defines         |> with or
// --------------> |  in the          |  or without ----------->
//                 |> project         |> new values
//                 |  without         |  according
//                 |> range values    |> to selection
//                 |                  |  criteria on
//                 |                  |  observation
//                 |                  |
//                 |                  |
//                 +------------------+

constexpr real time_step = 0.1;

static status update_observations(
  simulation&                                 sim,
  simulation_wrapper::simulation_observation& sim_obs) noexcept
{
    for (auto& obs : sim.observers) {
        const auto mdl_id = obs.model;

        auto* vec = sim_obs.get(mdl_id);
        if (not vec)
            sim_obs.set(mdl_id, vector<std::array<real, 2>>());

        if (obs.states[observer_flags::buffer_full]) {
            auto grow_error = false;

            write_interpolate_data(
              obs, time_step, [vec, &grow_error](auto t, auto v) noexcept {
                  if (grow_error = not vec->can_alloc(1); not grow_error) {
                      vec->push_back(std::array<real, 2>());
                      auto& ar = vec->back();
                      ar[0]    = t;
                      ar[1]    = v;
                  };
              });

            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_observation_error);
        }
    }

    return success();
}

static status flush_observations(
  simulation&                                 sim,
  simulation_wrapper::simulation_observation& sim_obs) noexcept
{
    for (auto& obs : sim.observers) {
        const auto mdl_id = obs.model;

        auto* vec = sim_obs.get(mdl_id);
        if (not vec)
            sim_obs.set(mdl_id, vector<std::array<real, 2>>());

        if (obs.states[observer_flags::buffer_full]) {
            auto grow_error = false;

            flush_interpolate_data(
              obs, time_step, [vec, &grow_error](auto t, auto v) noexcept {
                  if (grow_error = not vec->can_alloc(1); not grow_error) {
                      vec->push_back(std::array<real, 2>());
                      auto& ar = vec->back();
                      ar[0]    = t;
                      ar[1]    = v;
                  };
              });

            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_observation_error);
        }
    }

    return success();
}

static status run_complete(simulation_wrapper& wrapper) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        while (not sim.current_time_expired()) {
            irt_check(sim.run());

            update_observations(sim, sim_o);
        }

        if (sim.current_time_expired()) {
            flush_observations(sim, sim_o);
        }
    }

    return success();
}

static status run_bag(simulation_wrapper& wrapper) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        irt_check(sim.run());

        update_observations(sim, sim_o);
    }

    return success();
}

static status run_time(simulation_wrapper& wrapper) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];
        const auto t     = sim.current_time();

        while (not sim.current_time_expired() and sim.current_time() == t) {
            irt_check(sim.run());

            update_observations(sim, sim_o);
        }

        if (sim.current_time_expired()) {
            flush_observations(sim, sim_o);
        }
    }

    return success();
}

static status run_until(simulation_wrapper& wrapper, const time until) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        while (not sim.current_time_expired() and sim.current_time() < until) {
            irt_check(sim.run());

            update_observations(sim, sim_o);
        }

        if (sim.current_time_expired()) {
            flush_observations(sim, sim_o);
        }
    }

    return success();
}

static status run_during(simulation_wrapper& wrapper,
                         const time          during) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];
        const auto limit = sim.current_time() + during;

        while (not sim.current_time_expired() and sim.current_time() < limit) {
            irt_check(sim.run());

            update_observations(sim, sim_o);
        }

        if (sim.current_time_expired()) {
            flush_observations(sim, sim_o);
        }
    }

    return success();
}

static status embedded_sims_alloc(simulation_wrapper& wrapper,
                                  const simulation&   source,
                                  const sz            nb_sims) noexcept
{
    wrapper.embedded_sims.clear();

    if (not wrapper.embedded_sims.can_alloc(nb_sims) and
        not wrapper.embedded_sims.grow<2, 1>(nb_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (sz i = 0; i < nb_sims; ++i) {
        const auto id  = wrapper.embedded_sims.alloc_id();
        const auto idx = get_index(id);

        sims[idx] = source;
        sim_obs[idx].data.clear();
    }

    return success();
}

static status embedded_sims_copy_parameters(simulation_wrapper& wrapper,
                                            const sz nb_sims) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();

    if (debug::check(std::cmp_equal(nb_sims, wrapper.embedded_sims.size())))
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    for (const auto id : wrapper.embedded_sims) {
        for (sz j = 0, f = wrapper.single_parameters.size(); j < f; ++j) {
            const auto idx = get_index(wrapper.single_parameters[j].mdl_id);
            sims[id].parameters[idx] = wrapper.single_parameters[idx].p;
        }
    }

    const auto parameters = wrapper.multiple_parameters.size();
    if (parameters == 0)
        return success();

    vector<sz> cross(parameters, 0);
    if (debug::check(cross.size() < parameters))
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    while (cross.back() < wrapper.multiple_parameters.back().p.size()) {

        // First, copy the multiple parameters values into embedded simulation
        // parameters.

        for (const auto id : wrapper.embedded_sims) {
            for (sz j = 0, f = parameters; j < f; ++j) {
                const auto mdl_id  = wrapper.multiple_parameters[j].mdl_id;
                const auto mdl_idx = get_index(mdl_id);

                sims[id].parameters[mdl_idx] =
                  wrapper.multiple_parameters[j].p[cross[j]];
            }
        }

        // Second we advance the cross product using a pointer into a vector
        // of position. For example for 3 parameters with 3 values:
        // step -> cross-product-vector
        // 0    -> [0 0 0]
        // 1    -> [1 0 0]
        // 2    -> [2 0 0]
        // 3    -> [0 1 0]
        // ...
        // 24    -> [0 2 2]
        // 25    -> [1 2 2]
        // 26    -> [2 2 2]

        sz ptr = 0u;
        do {
            ++cross[ptr];

            if (cross[ptr] < wrapper.multiple_parameters[ptr].p.size())
                break;

            cross[ptr] = 0u;
            ++ptr;
        } while (ptr < parameters);
    }

    return success();
}

static status embedded_sims_init(simulation_wrapper& wrapper) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx = get_index(id);

        sim_obs[idx].data.clear();
    }

    for (const auto id : wrapper.embedded_sims) {
        const auto idx = get_index(id);

        if (sims[idx].srcs.prepare().has_error())
            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_source_error);

        if (sims[idx].initialize().has_error())
            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_initialization_error);
    }

    return success();
}

simulation_wrapper::simulation_wrapper(const simulation_wrapper& other) noexcept
  : x(other.x.capacity(), input_port{})
  , y(other.y.capacity(), output_port_id{ 0 })
{
    run = other.run;
}

status simulation_wrapper::initialize(simulation& sim) noexcept
{
    const auto* sim_src = sim.sims.try_to_get(sim_id);

    if (not sim_src)
        return make_error(simulation_errc::simulation_wrapper_source_error);

    const auto nb_sims = [&]() {
        sz nb = 1u;
        for (sz j = 0, f = multiple_parameters.size(); j < f; ++j) {
            nb *= multiple_parameters[j].p.size();
        }

        return nb;
    }();

    embedded_sims.clear();

    if (not embedded_sims.can_alloc(nb_sims) and
        not embedded_sims.grow<2, 1>(nb_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    return embedded_sims_alloc(*this, *sim_src, nb_sims)
      .and_then([&]() { return embedded_sims_copy_parameters(*this, nb_sims); })
      .and_then([&]() { return embedded_sims_init(*this); });
}

status simulation_wrapper::transition(simulation& sim,
                                      time        t,
                                      time        e,
                                      time        r) noexcept
{
    (void)t;
    (void)e;
    (void)r;

    const auto init_msg = get_message(sim, x[input_init]);
    const auto run_msg  = get_message(sim, x[input_run]);

    for (sz i = 0; i < single_parameters.size(); ++i) {
        const auto i_param_msg = get_message(sim, x[i + 2]);

        if (not i_param_msg.empty())
            single_parameters[i].p.reals[0] = i_param_msg.back()[0];

        sigma = r;
    }

    if (not init_msg.empty()) {
        const auto ret =
          embedded_sims_copy_parameters(*this, embedded_sims.size())
            .and_then([&]() { return embedded_sims_init(*this); });

        if (ret.has_error())
            return ret.error();

        sigma = time_domain<time>::infinity;
    }

    if (not run_msg.empty()) {
        const auto time_param = get_min_message(run_msg);

        switch (run) {
        case run_type::complete:
            return run_complete(*this);
        case run_type::bag:
            return run_bag(*this);
        case run_type::time:
            return run_time(*this);
        case run_type::until:
            return run_until(*this, time_param);
        case run_type::during:
            return run_during(*this, time_param);
        }

        sigma = zero;
    }

    return success();
}

status simulation_wrapper::lambda(simulation& /*sim*/) noexcept
{
    // Need to compute... For example the max:
    const auto& sim_obs   = embedded_sims.get<simulation_observation>();
    auto        max_value = std::numeric_limits<real>::lowest();
    auto        min_value = std::numeric_limits<real>::max();
    auto        max_id    = undefined<sub_id>();
    auto        min_id    = undefined<sub_id>();
    const auto  mdl_id    = operand.mdl_id;

    for (const auto id : embedded_sims) {
        const auto  idx    = get_index(id);
        const auto& sim_ob = sim_obs[idx];

        if (const auto* vec = sim_ob.get(operand.mdl_id)) {
            if (max_value < vec->max_element) {
                max_value = vec->max_element;
                max_id    = id;
            }

            if (min_value > vec->min_element) {
                min_value = vec->min_element;
                min_id    = id;
            }
        }
    }

    if (is_defined(min_id) or is_defined(max_id)) {
        // Get the single_parameter for the selected embedded simulation and
        // send it as output message in y[0..n] port.


    }

    // if (auto* embedded = sim.sims.try_to_get(sim_id)) {
    //     debug::ensure(embedded->observers.size() == 1u);

    //     observer* obs = nullptr;
    //     if (embedded->observers.next(obs)) {
    //         message msg;

    //         obs->buffer.read([&](const auto& vec, const auto /*version*/)
    //         {
    //             const auto& front = vec.front();

    //             msg[0] = front[0];
    //             msg[1] = front[1];
    //             msg[2] = front[2];
    //         });

    //         return send_message(
    //           sim, y[output_observation], msg[0], msg[1], msg[2]);
    //     }
    // }

    return success();
}

status simulation_wrapper::finalize(simulation& /*sim*/) noexcept
{
    auto& sims    = embedded_sims.get<simulation>();
    auto& sim_obs = embedded_sims.get<simulation_observation>();

    for (const auto id : embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        if (sim.finalize().has_error())
            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_finalization_error);

        if (flush_observations(sim, sim_o).has_error())
            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_finalization_error);
    }

    return success();
}

observation_message simulation_wrapper::observation(time t,
                                                    time e) const noexcept
{
    (void)t;
    (void)e;

    return {};
}

} // namespace irt
