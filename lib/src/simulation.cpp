// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

simulation_wrapper::simulation_wrapper(const simulation_wrapper& other) noexcept
{
    std::fill_n(x, length(x), input_port{});
    std::fill_n(y, length(y), output_port{});
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

            if (auto* mdl = sim.models.try_to_get(parameters[0])) {
                sim.parameters[get_index(parameters[0])].reals[0] =
            }
            for (const auto& p : param_msg) {
                const auto idx   = static_cast<i64>(p[0]);
                const auto value = p[1];
            }
        }

        sigma = time_domain<time>::infinity;
    }

    if (not init_msg.empty()) {
        // Read the init message before @c run() to ensure initialization is
        // done before run().

        if (embedded->srcs.prepare().has_error())
            return new_error(
              simulation_errc::embedded_simulation_source_error);

        if (embedded->initialize().has_error())
            return new_error(
              simulation_errc::embedded_simulation_initialization_error);

        sigma = time_domain<time>::infinity;
    }

    if (not run_msg.empty()) {
        sigma = zero;

        switch (run) {
        case run_type::complete:
            return embedded->run();
        case run_type::bag:
            return embedded->run();
        case run_type::time:
            return embedded->run();
        case run_type::until:
            return embedded->run();
        case run_type::during:
            return embedded->run();
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
            const auto& msg = obs->buffer.front();

            return send_message(sim, y[output_observation], msg[0], msg[1], msg[2]);
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
}

} // namespace irt
