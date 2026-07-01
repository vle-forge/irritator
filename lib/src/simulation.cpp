// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/observation.hpp>
#include <irritator/random.hpp>

#include <numeric>

namespace irt {

// @TODO Make @c time_step a parameter of the simulation-wrapper.
constexpr real time_step = 0.1;

static status update_observations(
  simulation&                                 sim,
  simulation_wrapper::simulation_observation& sim_obs) noexcept
{
    for (auto& obs : sim.observers) {
        const auto mdl_id = obs.model;

        if (auto* vec = sim_obs.get(mdl_id); debug::check(vec)) {
            if (obs.states[observer_flags::buffer_full]) {
                auto have_error = true;

                write_interpolate_data(
                  obs, time_step, [vec, &have_error](auto t, auto v) noexcept {
                      if (not vec->values.can_alloc(1) and
                          not vec->values.grow<2, 1>(32))
                          return;

                      vec->values.push_back(std::array<real, 2>());
                      auto& ar   = vec->values.back();
                      ar[0]      = t;
                      ar[1]      = v;
                      have_error = false;
                  });

                if (have_error)
                    return make_error(
                      simulation_errc::
                        simulation_wrapper_embedded_simulation_observation_error);
            }
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

        if (auto* vec = sim_obs.get(mdl_id); debug::check(vec)) {
            if (obs.states[observer_flags::buffer_full]) {
                auto have_error = true;

                flush_interpolate_data(
                  obs, time_step, [vec, &have_error](auto t, auto v) noexcept {
                      if (not vec->values.can_alloc(1) and
                          not vec->values.grow<2, 1>(32))
                          return;

                      vec->values.push_back(std::array<real, 2>());
                      auto& ar   = vec->values.back();
                      ar[0]      = t;
                      ar[1]      = v;
                      have_error = false;
                  });

                if (have_error)
                    return make_error(
                      simulation_errc::
                        simulation_wrapper_embedded_simulation_observation_error);
            }
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
            irt_check(update_observations(sim, sim_o));
        }

        if (sim.current_time_expired()) {
            irt_check(flush_observations(sim, sim_o));
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
        irt_check(update_observations(sim, sim_o));
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
            irt_check(update_observations(sim, sim_o));
        }

        if (sim.current_time_expired()) {
            irt_check(flush_observations(sim, sim_o));
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
            irt_check(update_observations(sim, sim_o));
        }

        if (sim.current_time_expired()) {
            irt_check(flush_observations(sim, sim_o));
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
            irt_check(update_observations(sim, sim_o));
        }

        if (sim.current_time_expired()) {
            irt_check(flush_observations(sim, sim_o));
        }
    }

    return success();
}

static status embedded_sims_alloc(simulation_wrapper& wrapper,
                                  const simulation&   source,
                                  const sz            nb_sims) noexcept
{
    debug::ensure(nb_sims > 1);
    debug::ensure(wrapper.x.size() == source.factors.size() + 2);

    wrapper.embedded_sims.clear();
    if (not wrapper.embedded_sims.can_alloc(nb_sims) and
        not wrapper.embedded_sims.grow<2, 1>(nb_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    wrapper.input_parameters.clear();
    if (not wrapper.input_parameters.reserve(source.factors.size()))
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

        for (const auto sel_id : source.selections) {
            const auto mdl_id = source.selections.get<model_id>(sel_id);

            sim_obs[idx].data.emplace_back(
              mdl_id, simulation_wrapper::embedded_model_observation{});
        }

        sim_obs[idx].sort();
    }

    return success();
}

static status embedded_sims_copy_parameters(simulation_wrapper& wrapper,
                                            const simulation&   sim_src,
                                            const sz nb_sims) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();

    if (std::cmp_not_equal(nb_sims, wrapper.embedded_sims.size()))
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    struct parameters {
        factor_id id       = undefined<factor_id>();
        sz        position = 0u;
        sz        count    = 0u;
    };

    wrapper.input_parameters.resize(sim_src.factors.size());
    vector<parameters> params(wrapper.input_parameters.size());

    if (params.capacity() < sim_src.factors.size() or
        wrapper.input_parameters.size() < sim_src.factors.size())
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    const auto& types  = sim_src.factors.get<factor_type>();
    const auto& models = sim_src.factors.get<model_id>();

    sz i = 0;
    for (const auto f_id : sim_src.factors) {
        const auto f_idx = get_index(f_id);

        auto values = vector<real>{};
        auto value  = real{ zero };

        switch (types[f_idx]) {
        case factor_type::single:
            value = zero;
            values.resize(1, sim_src.factors.get<single_factor>(f_id).value);
            break;

        case factor_type::single_add:
            value = zero;
            values.resize(1, sim_src.factors.get<single_factor>(f_id).value);
            break;

        case factor_type::single_mult:
            value = one;
            values.resize(1, sim_src.factors.get<single_factor>(f_id).value);
            break;

        case factor_type::fixed:
            value  = zero;
            values = sim_src.factors.get<fixed_factor>(f_id).values;
            break;

        case factor_type::fixed_add:
            value  = zero;
            values = sim_src.factors.get<fixed_factor>(f_id).values;
            break;

        case factor_type::fixed_mult:
            value  = one;
            values = sim_src.factors.get<fixed_factor>(f_id).values;
            break;

        case factor_type::random:
            value  = zero;
            values = sim_src.factors.get<random_factor>(f_id).gen(wrapper.seed);
            break;

        case factor_type::random_add:
            value  = zero;
            values = sim_src.factors.get<random_factor>(f_id).gen(wrapper.seed);
            break;

        case factor_type::random_mult:
            value  = one;
            values = sim_src.factors.get<random_factor>(f_id).gen(wrapper.seed);
            break;

        default:
            unreachable();
        }

        params[i].count    = values.size();
        params[i].position = 0;
        params[i].id       = f_id;

        wrapper.input_parameters[i].mdl_id = models[f_idx];
        wrapper.input_parameters[i].value  = value;
        wrapper.input_parameters[i].values = std::move(values);

        ++i;
    }

    dlogln(0, "emedded_sim     = {}", wrapper.embedded_sims.size());
    dlogln(0, "input_parameter = {}", wrapper.input_parameters.size());

    dlog(0, "embedded-simulation;");
    for (sz i = 0, e = params.size(); i < e; ++i) {
        const auto factor_id = params[i].id;
        const auto name      = sim_src.factors.get<name_str>(factor_id).sv();

        dlog(0, "{}", name);
        if (i + 1 < e)
            dlog(0, ";");
    }
    dlog(0, "\n");

    for (const auto id : wrapper.embedded_sims) {
        dlog(0, "{};", get_index(id));

        // First, copy the multiple parameters values into embedded simulation
        // parameters.

        for (sz i = 0, e = params.size(); i < e; ++i) {
            const auto mdl_id  = wrapper.input_parameters[i].mdl_id;
            const auto mdl_idx = get_index(mdl_id);

            const auto f_id  = params[i].id;
            const auto type  = sim_src.factors.get<factor_type>(f_id);
            const auto pos   = params[i].position;
            const auto value = wrapper.input_parameters[i].values[pos];

            debug::ensure(sim_src.factors.exists(f_id));

            const auto new_value = [&]() {
                switch (type) {
                case factor_type::single:
                case factor_type::fixed:
                case factor_type::random:
                    return value;

                case factor_type::single_add:
                case factor_type::fixed_add:
                case factor_type::random_add:
                    return wrapper.input_parameters[i].value + value;

                case factor_type::single_mult:
                case factor_type::fixed_mult:
                case factor_type::random_mult:
                    return wrapper.input_parameters[i].value * value;

                default:
                    unreachable();
                }
            }();

            sims[id].parameters[mdl_idx].reals[0] = new_value;

            dlog(0, "{}", new_value);
            if (i + 1 < e)
                dlog(0, ";");
        }

        dlog(0, "\n");

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
            ++params[ptr].position;

            if (params[ptr].position < params[ptr].count)
                break;

            params[ptr].position = 0u;
            ++ptr;
        } while (ptr < params.size());
    }

    return success();
}

static status embedded_sims_init(simulation_wrapper& wrapper,
                                 const simulation&   sim_src) noexcept
{
    auto& sims = wrapper.embedded_sims.get<simulation>();
    auto& sim_obs =
      wrapper.embedded_sims.get<simulation_wrapper::simulation_observation>();

    for (const auto id : wrapper.embedded_sims) {
        const auto idx = get_index(id);

        for (auto i = 0, e = length(sim_obs[idx].data); i != e; ++i)
            sim_obs[idx].data[i].value.values.clear();
    }

    for (const auto id : wrapper.embedded_sims) {
        const auto idx = get_index(id);
        sims[idx].observers.clear();

        for (const auto sel_id : sim_src.selections) {
            const auto mdl_id = sim_src.selections.get<model_id>(sel_id);
            auto&      obs    = sims[idx].observers.alloc();
            debug::ensure(sims[idx].models.exists(mdl_id));

            sims[idx].observe(sims[idx].models.get(mdl_id), obs);
        }

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

auto compute_simulation_number = [](const simulation& sim) noexcept -> sz {
    auto nb = sz{ 1 };

    for (const auto id : sim.factors) {
        switch (sim.factors.get<factor_type>(id)) {
        case factor_type::single:
        case factor_type::single_add:
        case factor_type::single_mult:
            break;

        case factor_type::fixed:
        case factor_type::fixed_add:
        case factor_type::fixed_mult:
            nb *= sim.factors.get<fixed_factor>(id).values.size();
            break;

        case factor_type::random:
        case factor_type::random_add:
        case factor_type::random_mult:
            nb *= sim.factors.get<random_factor>(id).count;
            break;
        }
    }

    return nb;
};

status simulation_wrapper::initialize(simulation& sim) noexcept
{
    const auto* sim_src = sim.sims.try_to_get(sim_id);

    if (not sim_src)
        return make_error(simulation_errc::simulation_wrapper_source_error);

    if (x.size() <= 2)
        return make_error(simulation_errc::simulation_wrapper_input_error);

    if (y.size() <= 2)
        return make_error(simulation_errc::simulation_wrapper_output_error);

    const auto nb_sims = compute_simulation_number(*sim_src);

    embedded_sims.clear();

    if (not embedded_sims.can_alloc(nb_sims) and
        not embedded_sims.grow<2, 1>(nb_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    if (const auto r = embedded_sims_alloc(*this, *sim_src, nb_sims); not r)
        return r.error();

    if (const auto r = embedded_sims_copy_parameters(*this, *sim_src, nb_sims);
        not r)
        return r.error();

    return embedded_sims_alloc(*this, *sim_src, nb_sims)
      .and_then(embedded_sims_copy_parameters, *this, *sim_src, nb_sims)
      .and_then(embedded_sims_init, *this, *sim_src);
}

status simulation_wrapper::transition(simulation&           sim,
                                      [[maybe_unused]] time t,
                                      [[maybe_unused]] time e,
                                      [[maybe_unused]] time r) noexcept
{
    if (state == run_state::finish)
        sigma = time_domain<time>::infinity;

    debug::ensure(x.size() == input_parameters.size() + 2);

    const auto* sim_src  = sim.sims.try_to_get(sim_id);
    const auto  init_msg = get_message(sim, x[ordinal(input_init)]);
    const auto  run_msg  = get_message(sim, x[ordinal(input_run)]);

    for (sz i = 0; i < input_parameters.size(); ++i) {
        const auto i_param_msg = get_message(sim, x[i + 2]);

        if (not i_param_msg.empty()) {
            input_parameters[i].value = i_param_msg.back()[0];
            sigma                     = r;
        }
    }

    if (not init_msg.empty()) {
        const auto ret =
          embedded_sims_copy_parameters(*this, *sim_src, embedded_sims.size())
            .and_then(embedded_sims_init, *this, *sim_src);

        if (ret.has_error())
            return ret.error();

        state = run_state::initialized;
        sigma = time_domain<time>::infinity;
    }

    if (not run_msg.empty()) {
        const auto time_param = get_min_message(run_msg);
        sigma = time_domain<time>::is_infinity(time_param) ? zero : time_param;

        switch (run) {
        case run_type::complete:
            if (state == run_state::initialized) {
                state   = run_state::running;
                auto st = run_complete(*this);
                state   = run_state::finish;
                return st;
            }
            break;

        case run_type::bag:
            return run_bag(*this);

        case run_type::time:
            return run_time(*this);

        case run_type::until:
            return run_until(*this, time_param);

        case run_type::during:
            return run_during(*this, time_param);
        }
    }

    return success();
}

static auto compute_min_last(const auto& embedded_sims,
                             const auto  mdl_id) noexcept
  -> std::pair<simulation_wrapper::sub_id, size_t>
{
    auto        best_id = undefined<simulation_wrapper::sub_id>();
    auto        index   = size_t(0);
    auto        value   = std::numeric_limits<real>::max();
    const auto& sim_obs =
      embedded_sims.template get<simulation_wrapper::simulation_observation>();

    for (const auto id : embedded_sims) {
        if (const auto* ptr = sim_obs[id].get(mdl_id)) {
            debug::ensure(ptr->values.size() > 0);
            const auto new_value = std::min(ptr->values.back()[1], value);

            if (new_value < value) {
                best_id = id;
                index   = ptr->values.size() - 1;
            }
        }
    }

    return std::make_pair(best_id, index);
}

/*

  Function to search the min, max, last min, last max

 */

static auto compute_max_last(const auto& embedded_sims,
                             const auto  mdl_id) noexcept
  -> std::pair<simulation_wrapper::sub_id, size_t>
{
    auto        best_id = undefined<simulation_wrapper::sub_id>();
    auto        index   = size_t(0);
    auto        value   = std::numeric_limits<real>::lowest();
    const auto& sim_obs =
      embedded_sims.template get<simulation_wrapper::simulation_observation>();

    for (const auto id : embedded_sims) {
        if (const auto* ptr = sim_obs[id].get(mdl_id)) {
            const auto new_value = std::max(ptr->values.back()[1], value);

            if (new_value < value)
                best_id = id;
        }
    }

    return std::make_pair(best_id, index);
}

static auto compute_min(const auto& embedded_sims, const auto mdl_id) noexcept
  -> std::pair<simulation_wrapper::sub_id, size_t>
{
    auto        best_id = undefined<simulation_wrapper::sub_id>();
    auto        index   = size_t(0);
    auto        value   = std::numeric_limits<real>::max();
    const auto& sim_obs =
      embedded_sims.template get<simulation_wrapper::simulation_observation>();

    for (const auto id : embedded_sims) {
        if (const auto* ptr = sim_obs[id].get(mdl_id)) {
            for (auto i = size_t(0), e = ptr->values.size(); i != e; ++i) {
                value   = std::min(ptr->values[i][1], value);
                index   = i;
                best_id = id;
            }
        }
    }

    return std::make_pair(best_id, index);
}

static auto compute_max(const auto& embedded_sims, const auto mdl_id) noexcept
  -> std::pair<simulation_wrapper::sub_id, size_t>
{
    auto        best_id = undefined<simulation_wrapper::sub_id>();
    auto        index   = size_t(0);
    auto        value   = std::numeric_limits<real>::max();
    const auto& sim_obs =
      embedded_sims.template get<simulation_wrapper::simulation_observation>();

    for (const auto id : embedded_sims) {
        if (const auto* ptr = sim_obs[id].get(mdl_id)) {
            for (auto i = size_t(0), e = ptr->values.size(); i != e; ++i) {
                value   = std::max(ptr->values[i][1], value);
                index   = i;
                best_id = id;
            }
        }
    }

    return std::make_pair(best_id, index);
}

/* * * * *
 *
 * compute result of a embedded_model_observation
 *
 * * * * */

real simulation_wrapper::embedded_model_observation::compute_result(
  const criteria_type type) const noexcept
{
    debug::ensure(std::cmp_greater(values.size(), 1));

    auto ret = real{};

    switch (type) {
    case criteria_type::min_last:
        ret = values.back()[1];
        break;

    case criteria_type::max_last:
        ret = values.back()[1];
        break;

    case criteria_type::min:
        ret = std::numeric_limits<real>::max();

        for (auto i = 0, e = length(values); i != e; ++i)
            ret = std::min(ret, values[i][1]);

        break;

    case criteria_type::max:
        ret = std::numeric_limits<real>::lowest();

        for (auto i = 0, e = length(values); i != e; ++i)
            ret = std::max(ret, values[i][1]);

        break;
    }

    return ret;
}

/* * * * *
 *
 * weighted_sum
 *
 * * * * */

static auto compute_weighted_sum_objective(
  const simulation& sim_src,
  id_data_array<void,
                simulation_wrapper::sub_id,
                allocator<new_delete_memory_resource>,
                simulation,
                simulation_wrapper::simulation_observation>
    embedded_sims) noexcept -> expected<simulation_wrapper::sub_id>
{
    auto& sim_obs =
      embedded_sims.get<simulation_wrapper::simulation_observation>();

    // allocate vector to stores result, objective function etc. and be sure
    // allocation success.

    // A vector of reals models the objectives functions. The size equals the
    // multiplication of the numbers of embedded simulations and objective
    // function element.
    //
    // The form is:
    // -------------------------------------------------------------------------
    // | obj_1 | obj_2 | obj_3 | obj_1 | obj_2 | obj_3 | obj_1 | obj_2 | obj_3 |
    // -------------------------------------------------------------------------
    // | sub_simulation_1      | sub_simulation_2      | sub_simulation_3      |
    // -------------------------------------------------------------------------

    const auto objective_fn_size = sim_src.selections.size();
    const auto simulation_size   = embedded_sims.size();
    const auto size              = objective_fn_size * simulation_size;
    auto       objective_fn      = vector<real>(size);

    auto pos =
      [objective_fn_size](const simulation_wrapper::sub_id sub_id,
                          const selection_id obj_fn_id) noexcept -> sz {
        return static_cast<sz>(get_index(sub_id) * objective_fn_size +
                               get_index(obj_fn_id));
    };

    if (std::cmp_not_equal(objective_fn.size(), size))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    // For each embedded simulation, for each objective function element,
    // computes result of the observation trajectory according to tge
    // criteria_type and fill the objective funiton.

    for (const auto sub_id : embedded_sims) {
        for (const auto obj_fn_id : sim_src.selections) {
            const auto type = sim_src.selections.get<criteria_type>(obj_fn_id);
            const auto mdl_id = sim_src.selections.get<model_id>(obj_fn_id);

            if (const auto* ptr = sim_obs[sub_id].get(mdl_id)) {
                const auto val = ptr->compute_result(type);
                const auto idx = pos(sub_id, obj_fn_id);

                objective_fn[idx] = val;
            }
        }
    }

    debug::ensure(sim_src.objective.weighted_sum_params.types.size() ==
                  sim_src.selections.size());
    debug::ensure(sim_src.objective.weighted_sum_params.weights.size() ==
                  sim_src.selections.size());

    // If min-max linearization

    if (sim_src.objective.weighted_sum_params.norm == norm_type::min_max) {
        for (const auto obj_fn_id : sim_src.selections) {
            const auto is_max =
              sim_src.objective.weighted_sum_params
                .types[get_index(obj_fn_id)] == optimization_type::maximize;

            auto min_val = std::numeric_limits<real>::max();
            auto max_val = std::numeric_limits<real>::lowest();

            for (const auto sub_id : embedded_sims) {
                const auto idx = pos(sub_id, obj_fn_id);
                const auto val = objective_fn[idx];

                min_val = std::min(min_val, val);
                max_val = std::max(max_val, val);
            }

            const auto diff = max_val - min_val;
            const auto div  = diff < 1e-10 ? 1e-10 : diff;

            if (is_max) {
                for (const auto sub_id : embedded_sims) {
                    const auto idx    = pos(sub_id, obj_fn_id);
                    objective_fn[idx] = (objective_fn[idx] - min_val) / div;
                }
            } else {
                for (const auto sub_id : embedded_sims) {
                    const auto idx    = pos(sub_id, obj_fn_id);
                    objective_fn[idx] = (max_val - objective_fn[idx]) / div;
                }
            }
        }
    } else {
        for (const auto obj_fn_id : sim_src.selections) {
            const auto is_max =
              sim_src.objective.weighted_sum_params
                .types[get_index(obj_fn_id)] == optimization_type::maximize;

            if (is_max) {
                auto sum    = 0.0;
                auto sq_sum = 0.0;

                for (const auto sub_id : embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    sum += objective_fn[idx];
                    sq_sum += objective_fn[idx] * objective_fn[idx];
                }

                const auto mean    = sum / simulation_size;
                const auto std_dev = std::sqrt(sq_sum / simulation_size);
                const auto div     = std_dev < 1e-10 ? 1e-10 : std_dev;

                for (const auto sub_id : embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    objective_fn[idx] = (objective_fn[idx] - mean) / div;
                }
            } else {
                auto sum    = 0.0;
                auto sq_sum = 0.0;

                for (const auto sub_id : embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    sum += objective_fn[idx];
                    sq_sum += objective_fn[idx] * objective_fn[idx];
                }

                const auto mu = sum / simulation_size;
                const auto sigma =
                  std::sqrt(sq_sum / simulation_size - mu * mu);

                const auto div = sigma < 1e-10 ? 1e-10 : sigma;

                for (const auto sub_id : embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    objective_fn[idx] = (mu - objective_fn[idx]) / div;
                }
            }
        }
    }

    const auto is_max = sim_src.objective.type == optimization_type::maximize;

    auto best_sub_id = undefined<simulation_wrapper::sub_id>();
    auto best_value  = is_max ? std::numeric_limits<real>::lowest()
                              : std::numeric_limits<real>::max();

    for (const auto sub_id : embedded_sims) {
        auto value = 0.0;
        for (const auto obj_fn_id : sim_src.selections) {
            const auto w = sim_src.objective.weighted_sum_params
                             .weights[get_index(obj_fn_id)];

            const auto idx = pos(sub_id, obj_fn_id);
            value += w * objective_fn[idx];
        }

        if (is_max) {
            if (value > best_value) {
                best_value  = value;
                best_sub_id = sub_id;
            }
        } else {
            if (value < best_value) {
                best_value  = value;
                best_sub_id = sub_id;
            }
        }
    }

    return best_sub_id;
}

status simulation_wrapper::lambda(simulation& sim) noexcept
{
    const auto* sim_src = sim.sims.try_to_get(sim_id);

    debug::ensure(y.size() ==
                  input_parameters.size() + sim_src->selections.size());

    if (sim_src->objective.method == optimization_method::weighted_sum) {
        const auto best_sub_id =
          compute_weighted_sum_objective(*sim_src, embedded_sims);

        if (best_sub_id.has_error())
            return best_sub_id.error();

        if (not embedded_sims.exists(best_sub_id.value()))
            return success();

        auto        output_port_index = 0;
        const auto& sub_sims_obs = embedded_sims.get<simulation_observation>();
        const auto& sub_sim_obs  = sub_sims_obs[best_sub_id.value()];

        for (const auto& elem : sub_sim_obs.data) {
            const auto msg = message{ elem.value.values.back()[0],
                                      elem.value.values.back()[1],
                                      0.0 };
            send_message(sim, y[output_port_index], msg[0], msg[1], msg[2]);
            ++output_port_index;
        }
        const auto& sub_sims = embedded_sims.get<simulation>();
        for (const auto factor_id : sim_src->factors) {
            const auto id  = sim_src->factors.get<model_id>(factor_id);
            const auto idx = get_index(id);
            const auto msg =
              message{ sub_sims[best_sub_id.value()].parameters[idx].reals[0],
                       sub_sims[best_sub_id.value()].parameters[idx].reals[1],
                       sub_sims[best_sub_id.value()].parameters[idx].reals[2] };
            send_message(sim, y[output_port_index], msg[0], msg[1], msg[2]);
            ++output_port_index;
        }

        return success();
    }

    /* Ajout d'un mode simple ? */

    for (const auto id : sim_src->selections) {
        const auto type   = sim_src->selections.get<criteria_type>(id);
        const auto mdl_id = sim_src->selections.get<model_id>(id);
        const auto [best_sub_id, best_index] =
          [&]() -> std::pair<simulation_wrapper::sub_id, size_t> {
            switch (type) {
            case criteria_type::min_last:
                return compute_min_last(embedded_sims, mdl_id);

            case criteria_type::max_last:
                return compute_max_last(embedded_sims, mdl_id);

            case criteria_type::min:
                return compute_min(embedded_sims, mdl_id);

            case criteria_type::max:
                return compute_max(embedded_sims, mdl_id);
            }

            unreachable();
        }();

        if (not embedded_sims.exists(best_sub_id))
            return success();

        auto output_port_index = 0;

        const auto& sub_sims_obs = embedded_sims.get<simulation_observation>();
        const auto& sub_sim_obs  = sub_sims_obs[best_sub_id];

        for (const auto& elem : sub_sim_obs.data) {
            const auto msg = message{ elem.value.values[best_index][0],
                                      elem.value.values[best_index][1],
                                      0.0 };

            send_message(sim, y[output_port_index], msg[0], msg[1], msg[2]);
            ++output_port_index;
        }

        const auto& sub_sims = embedded_sims.get<simulation>();
        for (const auto factor_id : sim_src->factors) {
            const auto id  = sim_src->factors.get<model_id>(factor_id);
            const auto idx = get_index(id);
            const auto msg =
              message{ sub_sims[best_sub_id].parameters[idx].reals[0],
                       sub_sims[best_sub_id].parameters[idx].reals[1],
                       sub_sims[best_sub_id].parameters[idx].reals[2] };

            send_message(sim, y[output_port_index], msg[0], msg[1], msg[2]);
            ++output_port_index;
        }

        break;
    }

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
                                                    time /*e*/) const noexcept
{
    return { t, static_cast<real>(embedded_sims.size()) };
}

} // namespace irt
