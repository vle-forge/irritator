// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

static status run_complete(simulation_wrapper& wrapper,
                           simulation&         sim,
                           simulation&         embedded) noexcept
{
    (void)wrapper;
    (void)sim;

    while (not embedded.current_time_expired())
        irt_check(embedded.run());

    return success();
}

static status run_bag(simulation_wrapper& wrapper,
                      simulation&         sim,
                      simulation&         embedded) noexcept
{
    (void)wrapper;
    (void)sim;

    if (not embedded.current_time_expired())
        irt_check(embedded.run());

    return success();
}

static status run_time(simulation_wrapper& wrapper,
                       simulation&         sim,
                       simulation&         embedded) noexcept
{
    (void)wrapper;
    (void)sim;

    const auto t = embedded.current_time();
    while (not embedded.current_time_expired() and embedded.current_time() == t)
        irt_check(embedded.run());

    return embedded.run();
}

static status run_until(simulation_wrapper& wrapper,
                        simulation&         sim,
                        simulation&         embedded,
                        const time          until) noexcept
{
    (void)wrapper;
    (void)sim;

    while (not embedded.current_time_expired() and
           embedded.current_time() < until)
        irt_check(embedded.run());

    return success();
}

static status run_during(simulation_wrapper& wrapper,
                         simulation&         sim,
                         simulation&         embedded,
                         const time          during) noexcept
{
    (void)wrapper;
    (void)sim;

    const auto limit = embedded.current_time() + during;
    while (not embedded.current_time_expired() and
           embedded.current_time() < limit)
        irt_check(embedded.run());

    return success();
}

simulation_wrapper::simulation_wrapper(const simulation_wrapper& other) noexcept
{
    std::fill_n(x, length(x), input_port{});
    std::fill_n(y, length(y), undefined<output_port_id>());
    sim_id = undefined<simulation_id>();
    run    = other.run;
}

status simulation_wrapper::initialize(simulation& sim) noexcept
{
    if (auto* embedded = sim.sims.try_to_get(sim_id)) {
        if (embedded->srcs.prepare().has_error())
            return new_error(simulation_errc::embedded_simulation_source_error);

        if (embedded->initialize().has_error())
            return new_error(
              simulation_errc::embedded_simulation_initialization_error);

        return success();
    }

    return new_error(simulation_errc::embedded_simulation_search_error);
}

status simulation_wrapper::transition(simulation& sim,
                                      time        t,
                                      time        e,
                                      time        r) noexcept
{
    (void)t;
    (void)e;
    (void)r;

    auto* embedded = sim.sims.try_to_get(sim_id);
    if (not embedded)
        return new_error(simulation_errc::embedded_simulation_search_error);

    const auto init_msg  = get_message(sim, x[input_init]);
    const auto run_msg   = get_message(sim, x[input_run]);
    const auto param_msg = get_message(sim, x[input_parameter]);

    if (not param_msg.empty()) {
        // Read parameter messages first to ensure @c init() is called with the
        // updated parameters.

        if (not param_msg[0].empty()) {
            // if (auto* mdl = sim.models.try_to_get(parameters[0])) {
            //     sim.parameters[get_index(parameters[0])].reals[0] =
            // }
            for (const auto& p : param_msg) {
                const auto idx   = static_cast<i64>(p[0]);
                const auto value = p[1];

                (void)idx;
                (void)value;
            }
        }

        sigma = time_domain<time>::infinity;
    }

    if (not init_msg.empty()) {
        // Read the init message before @c run() to ensure initialization is
        // done before run().

        if (embedded->srcs.prepare().has_error())
            return new_error(simulation_errc::embedded_simulation_source_error);

        if (embedded->initialize().has_error())
            return new_error(
              simulation_errc::embedded_simulation_initialization_error);

        sigma = time_domain<time>::infinity;
    }

    if (not run_msg.empty()) {
        const auto time_param = get_min_message(run_msg);
        sigma                 = zero;

        switch (run) {
        case run_type::complete:
            return run_complete(*this, sim, *embedded);
        case run_type::bag:
            return run_bag(*this, sim, *embedded);
        case run_type::time:
            return run_time(*this, sim, *embedded);
        case run_type::until:
            return run_until(*this, sim, *embedded, time_param);
        case run_type::during:
            return run_during(*this, sim, *embedded, time_param);
        }
    }

    return success();
}

status simulation_wrapper::lambda(simulation& sim) noexcept
{
    if (auto* embedded = sim.sims.try_to_get(sim_id)) {
        debug::ensure(embedded->observers.size() == 1u);

        observer* obs = nullptr;
        if (embedded->observers.next(obs)) {
            message msg;

            obs->buffer.read([&](const auto& vec, const auto /*version*/) {
                const auto& front = vec.front();

                msg[0] = front[0];
                msg[1] = front[1];
                msg[2] = front[2];
            });

            return send_message(
              sim, y[output_observation], msg[0], msg[1], msg[2]);
        }
    }

    return success();
}

status simulation_wrapper::finalize(simulation& sim) noexcept
{
    if (auto* embedded = sim.sims.try_to_get(sim_id)) {
        if (embedded->finalize().has_error())
            return new_error(
              simulation_errc::embedded_simulation_finalization_error);
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
