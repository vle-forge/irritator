// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>

namespace irt {

static void update_observations(simulation& sim) noexcept
{
    if (not sim.immediate_observers.empty())
        sim.tick_resamplers();
}

/// Appends the new points to the observations: they are cleared when the
/// embedded simulation starts again. After this call the resamplers keep no
/// output, so a dump of the simulation loses nothing.
static status copy_history(
  simulation&                                  sim,
  embedded_simulation::simulation_observation& sim_obs) noexcept
{
    sim.tick_resamplers();

    return sim.drain_observations([&](const observation_batch& b) -> status {
        auto* vec = sim_obs.get(b.obs.model());

        if (vec == nullptr)
            return success();

        const auto n = static_cast<int>(b.points.size());
        if (not vec->values.can_alloc(n) and
            not vec->values.template grow<2, 1>(n))
            return make_error(
              simulation_errc::simulation_wrapper_not_enough_memory);

        vec->values.insert(vec->values.end(), b.points.begin(), b.points.end());
        return success();
    });
}

/// A sub-simulation is over when it reaches its end or when no event remains:
/// it is then finalized once and never runs again.
static bool is_over(const simulation& sim) noexcept
{
    return sim.current_time_expired() or sim.sched.empty();
}

/// Without any more event the date is infinite, and simulation::finalize()
/// needs a date: the date of the last event (see simulation_player).
static status finish(
  simulation&                                  sim,
  embedded_simulation::simulation_observation& sim_obs) noexcept
{
    if (time_domain<time>::is_infinity(sim.current_time()))
        sim.current_time(sim.last_time());

    if (not time_domain<time>::is_infinity(sim.current_time()))
        irt_check(sim.finalize());

    return copy_history(sim, sim_obs);
}

/// Runs each sub-simulation while @a keep_running(sim, start, steps) is true,
/// where @a start is its date before the run and @a steps the number of
/// bags already run.
template<typename Predicate>
static status run_each(embedded_simulation& embed_sims,
                       Predicate&&          keep_running) noexcept
{
    auto& sims    = embed_sims.embedded_sims.get<simulation>();
    auto& sim_obs = embed_sims.embedded_sims
                      .get<embedded_simulation::simulation_observation>();

    for (const auto id : embed_sims.embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        if (is_over(sim))
            continue;

        const auto start = sim.current_time();
        for (int steps = 0;
             not is_over(sim) and keep_running(sim, start, steps); ++steps) {
            irt_check(sim.run());
            update_observations(sim);
        }

        irt_check(is_over(sim) ? finish(sim, sim_o) : copy_history(sim, sim_o));
    }

    return success();
}

static status run_complete(embedded_simulation& embed_sims) noexcept
{
    return run_each(embed_sims,
                    [](const simulation&, time, int) noexcept { return true; });
}

static status run_bag(embedded_simulation& embed_sims) noexcept
{
    return run_each(embed_sims, [](const simulation&, time,
                                   int steps) noexcept { return steps == 0; });
}

static status run_time(embedded_simulation& embed_sims) noexcept
{
    return run_each(embed_sims,
                    [](const simulation& sim, time start, int) noexcept {
                        return sim.current_time() == start;
                    });
}

static status run_until(embedded_simulation& embed_sims,
                        const time           until) noexcept
{
    return run_each(embed_sims,
                    [until](const simulation& sim, time, int) noexcept {
                        return sim.current_time() < until;
                    });
}

static status run_during(embedded_simulation& embed_sims,
                         const time           during) noexcept
{
    return run_each(embed_sims,
                    [during](const simulation& sim, time start, int) noexcept {
                        return sim.current_time() < start + during;
                    });
}

static status embedded_sims_alloc(embedded_simulation& embed_sims,
                                  const sz             nb_sims) noexcept
{
    debug::ensure(nb_sims >= 1);

    embed_sims.embedded_sims.clear();
    if (not embed_sims.embedded_sims.can_alloc(nb_sims) and
        not embed_sims.embedded_sims.grow<2, 1>(nb_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    embed_sims.input_parameters.clear();
    if (not embed_sims.input_parameters.reserve(embed_sims.factors.size()))
        return make_error(
          simulation_errc::
            simulation_wrapper_too_many_embedded_simulation_error);

    auto& sims    = embed_sims.embedded_sims.get<simulation>();
    auto& sim_obs = embed_sims.embedded_sims
                      .get<embedded_simulation::simulation_observation>();

    for (sz i = 0; i < nb_sims; ++i) {
        const auto id  = embed_sims.embedded_sims.alloc_id();
        const auto idx = get_index(id);

        sims[idx] = embed_sims.sim;
        sim_obs[idx].data.clear();

        for (const auto sel_id : embed_sims.selections) {
            const auto mdl_id = embed_sims.selections.get<model_id>(sel_id);

            sim_obs[idx].data.emplace_back(
              mdl_id, embedded_simulation::embedded_model_observation{});
        }

        sim_obs[idx].sort();
    }

    return success();
}

static status embedded_sims_copy_parameters(
  embedded_simulation& embed_sims) noexcept
{
    const auto nb_sims = embed_sims.embedded_sims.size();
    auto&      sims    = embed_sims.embedded_sims.get<simulation>();

    if (std::cmp_not_equal(nb_sims, embed_sims.embedded_sims.size()))
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    struct parameters {
        factor_id id       = undefined<factor_id>();
        sz        position = 0u;
        sz        count    = 0u;
    };

    embed_sims.input_parameters.resize(embed_sims.factors.size());
    vector<parameters> params(embed_sims.input_parameters.size());

    if (params.capacity() < embed_sims.factors.size() or
        embed_sims.input_parameters.size() < embed_sims.factors.size())
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    const auto& types  = embed_sims.factors.get<factor_type>();
    const auto& models = embed_sims.factors.get<model_id>();

    sz i = 0;
    for (const auto f_id : embed_sims.factors) {
        const auto f_idx = get_index(f_id);

        auto values = vector<real>{};

        switch (types[f_idx]) {
        case factor_type::single:
        case factor_type::single_add:
        case factor_type::single_mult:
            values.resize(1, embed_sims.factors.get<single_factor>(f_id).value);
            break;

        case factor_type::fixed:
        case factor_type::fixed_add:
        case factor_type::fixed_mult:
            values = embed_sims.factors.get<fixed_factor>(f_id).values;
            break;

        case factor_type::random:
        case factor_type::random_add:
        case factor_type::random_mult:
            values = embed_sims.factors.get<random_factor>(f_id).gen(
              embed_sims.seed);
            break;

        default:
            unreachable();
        }

        params[i].count    = values.size();
        params[i].position = 0;
        params[i].id       = f_id;

        embed_sims.input_parameters[i].mdl_id = models[f_idx];
        embed_sims.input_parameters[i].values = std::move(values);

        ++i;
    }

    dlogln(0, "emedded_sim = {}", embed_sims.embedded_sims.size());
    dlogln(0, "parameters  = {}", embed_sims.input_parameters.size());

    dlog(0, "{:^12}", "subid");
    for (sz i = 0, e = params.size(); i < e; ++i) {
        const auto factor_id = params[i].id;
        const auto name      = embed_sims.factors.get<name_str>(factor_id).sv();
        dlog(0, "{:^12}", name);
    }
    dlog(0, "\n");

    for (const auto id : embed_sims.embedded_sims) {
        dlog(0, "{:^12}", get_index(id));

        // First, copy the multiple parameters values into embedded simulation
        // parameters.

        for (sz i = 0, e = params.size(); i < e; ++i) {
            const auto mdl_id  = embed_sims.input_parameters[i].mdl_id;
            const auto mdl_idx = get_index(mdl_id);

            const auto f_id  = params[i].id;
            const auto type  = embed_sims.factors.get<factor_type>(f_id);
            const auto pos   = params[i].position;
            const auto value = embed_sims.input_parameters[i].values[pos];

            debug::ensure(embed_sims.factors.exists(f_id));

            const auto new_value = [&]() {
                switch (type) {
                case factor_type::single:
                case factor_type::fixed:
                case factor_type::random:
                    return embed_sims.input_parameters[i].value.has_value()
                             ? *embed_sims.input_parameters[i].value
                             : value;

                case factor_type::single_add:
                case factor_type::fixed_add:
                case factor_type::random_add:
                    return value +
                           (embed_sims.input_parameters[i].value.has_value()
                              ? *embed_sims.input_parameters[i].value
                              : 0.0);

                case factor_type::single_mult:
                case factor_type::fixed_mult:
                case factor_type::random_mult:
                    return value *
                           (embed_sims.input_parameters[i].value.has_value()
                              ? *embed_sims.input_parameters[i].value
                              : 1.0);

                default:
                    unreachable();
                }
            }();

            sims[id].parameters[mdl_idx].reals[0] = new_value;

            dlog(0, "{:^12}", new_value);
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

        for (sz ptr = 0u; ptr < params.size(); ++ptr) {
            ++params[ptr].position;

            if (params[ptr].position < params[ptr].count)
                break;

            params[ptr].position = 0u;
        }
    }

    dlog(0, "\n");

    return success();
}

static status embedded_sims_init(embedded_simulation& embed_sims) noexcept
{
    auto& sims    = embed_sims.embedded_sims.get<simulation>();
    auto& sim_obs = embed_sims.embedded_sims
                      .get<embedded_simulation::simulation_observation>();

    for (const auto id : embed_sims.embedded_sims) {
        const auto idx = get_index(id);

        for (auto i = 0, e = length(sim_obs[idx].data); i != e; ++i)
            sim_obs[idx].data[i].value.values.clear();
    }

    for (const auto id : embed_sims.embedded_sims) {
        const auto idx = get_index(id);
        sims[idx].observers.clear();

        for (const auto sel_id : embed_sims.selections) {
            const auto mdl_id = embed_sims.selections.get<model_id>(sel_id);
            debug::ensure(sims[idx].models.exists(mdl_id));

            irt_check(sims[idx].observe(sims[idx].models.get(mdl_id)));
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
  : sim_id{ undefined<embedded_simulation_id>() }
  , run{ other.run }
{}

static auto compute_simulation_number(
  const embedded_simulation& embed_sims) noexcept -> sz
{
    auto nb = sz{ 1 };

    for (const auto id : embed_sims.factors) {
        switch (embed_sims.factors.get<factor_type>(id)) {
        case factor_type::single:
        case factor_type::single_add:
        case factor_type::single_mult:
            break;

        case factor_type::fixed:
        case factor_type::fixed_add:
        case factor_type::fixed_mult:
            nb *= embed_sims.factors.get<fixed_factor>(id).values.size();
            break;

        case factor_type::random:
        case factor_type::random_add:
        case factor_type::random_mult:
            nb *= embed_sims.factors.get<random_factor>(id).count;
            break;
        }
    }

    return nb;
}

/// The ports and the objective must match the component: the selections of
/// the objective and the ports of the factors and the selections.
static bool is_valid_objective(const embedded_simulation& embed_sims) noexcept
{
    const auto& obj = embed_sims.objective;
    const auto  n   = embed_sims.selections.size();

    switch (obj.method) {
    case optimization_method::simple:
        return embed_sims.selections.exists(obj.simple_params.primary);

    case optimization_method::epsilon_constrained:
        return embed_sims.selections.exists(
                 obj.epsilon_constrained_params.primary) and
               std::cmp_equal(obj.epsilon_constrained_params.epsilons.size(),
                              n) and
               std::cmp_equal(obj.epsilon_constrained_params.operations.size(),
                              n);

    case optimization_method::weighted_sum:
        return std::cmp_equal(obj.weighted_sum_params.types.size(), n) and
               std::cmp_equal(obj.weighted_sum_params.weights.size(), n);
    }

    return false;
}

status simulation_wrapper::initialize(simulation& sim) noexcept
{
    auto* embed_sims = sim.sims.try_to_get(sim_id);

    if (not embed_sims)
        return make_error(
          simulation_errc::simulation_wrapper_embedded_simulation_search_error);

    if (std::cmp_not_equal(x.size(), embed_sims->factors.size() + 2))
        return make_error(simulation_errc::simulation_wrapper_input_error);

    if (embed_sims->selections.empty() or
        std::cmp_not_equal(y.size(), embed_sims->factors.size() +
                                       embed_sims->selections.size()))
        return make_error(simulation_errc::simulation_wrapper_output_error);

    if (not is_valid_objective(*embed_sims))
        return make_error(
          simulation_errc::
            simulation_wrapper_embedded_simulation_initialization_error);

    // A factor without value (empty fixed values or random count) leaves no
    // scenario to simulate.
    const auto nb_sims = compute_simulation_number(*embed_sims);
    if (nb_sims == 0)
        return make_error(simulation_errc::simulation_wrapper_input_error);

    irt_check(embedded_sims_alloc(*embed_sims, nb_sims)
                .and_then(embedded_sims_copy_parameters, *embed_sims)
                .and_then(embedded_sims_init, *embed_sims));

    state = run_state::initialized;

    return success();
}

status simulation_wrapper::transition(simulation&           sim,
                                      [[maybe_unused]] time t,
                                      [[maybe_unused]] time e,
                                      time                  r) noexcept
{
    auto* embed_sims = sim.sims.try_to_get(sim_id);

    if (not embed_sims)
        return make_error(
          simulation_errc::simulation_wrapper_embedded_simulation_search_error);

    // The results were just sent (r == 0): wait for the next run message.
    // Otherwise the pending results keep their date.
    sigma = r == zero ? time_domain<time>::infinity : r;

    const auto init_msg = get_message(sim, x[ordinal(input_init)]);
    const auto run_msg  = get_message(sim, x[ordinal(input_run)]);

    for (sz i = 0; i < embed_sims->input_parameters.size(); ++i) {
        const auto i_param_msg = get_message(sim, x[i + 2]);

        if (not i_param_msg.empty()) {
            embed_sims->input_parameters[i].value = i_param_msg.back()[0];
            state                                 = run_state::input_changed;
        }
    }

    if (not init_msg.empty()) {
        const auto ret = embedded_sims_copy_parameters(*embed_sims)
                           .and_then(embedded_sims_init, *embed_sims);

        if (ret.has_error())
            return ret.error();

        state = run_state::initialized;
        sigma = time_domain<time>::infinity;
    }

    if (run_msg.empty())
        return success();

    // The factors are copied into the sub-simulations at their
    // initialization: a new factor value restarts them.
    if (state == run_state::input_changed) {
        const auto ret = embedded_sims_copy_parameters(*embed_sims)
                           .and_then(embedded_sims_init, *embed_sims);

        if (ret.has_error())
            return ret.error();

        state = run_state::initialized;
    }

    const auto time_param = get_min_message(run_msg);

    // The value of the message delays the results except for the until run
    // type where it is a date of the sub-simulations.
    sigma = time_domain<time>::is_infinity(time_param) or run == run_type::until
              ? zero
              : time_param;

    switch (run) {
    case run_type::complete:
        if (state == run_state::initialized) {
            state   = run_state::running;
            auto st = run_complete(*embed_sims);
            state   = run_state::finish;
            return st;
        }
        return success();

    case run_type::bag:
        return run_bag(*embed_sims);

    case run_type::time:
        return run_time(*embed_sims);

    case run_type::until:
        return run_until(*embed_sims, time_param);

    case run_type::during:
        return run_during(*embed_sims, time_param);
    }

    return success();
}

/*

  Function to search the min, max, last min, last max

 */

/* * * * *
 *
 * compute result of a embedded_model_observation
 *
 * * * * */

real embedded_simulation::embedded_model_observation::compute_result(
  const criteria_type type) const noexcept
{
    if (values.empty())
        return std::numeric_limits<real>::quiet_NaN();

    auto ret = real{};

    switch (type) {
    case criteria_type::min_last:
        ret = values.back().value;
        break;

    case criteria_type::max_last:
        ret = values.back().value;
        break;

    case criteria_type::min:
        ret = std::numeric_limits<real>::max();

        for (auto i = 0, e = length(values); i != e; ++i)
            ret = std::min(ret, values[i].value);

        break;

    case criteria_type::max:
        ret = std::numeric_limits<real>::lowest();

        for (auto i = 0, e = length(values); i != e; ++i)
            ret = std::max(ret, values[i].value);

        break;
    }

    return ret;
}

struct pos_in_objective_function {

    pos_in_objective_function(u32 objective_fn_size_) noexcept
      : objective_fn_size{ objective_fn_size_ }
    {}

    u32 operator()(const embedded_simulation::sub_id sub_id,
                   const selection_id                obj_fn_id) const noexcept
    {
        return static_cast<u32>(get_index(sub_id) * objective_fn_size +
                                get_index(obj_fn_id));
    }

    const u32 objective_fn_size;
};

static auto send(const embedded_simulation&        embed_sims,
                 const simulation_wrapper&         wrapper,
                 const embedded_simulation::sub_id best_sub_id,
                 const std::span<const real>       objective_fn,
                 simulation&                       sim) noexcept -> status
{
    if (not embed_sims.embedded_sims.exists(best_sub_id))
        return success();

    const pos_in_objective_function pos(embed_sims.selections.size());
    auto                            output_port_index = 0;

    // Same order as the output ports shown by the component editor: the
    // factors values, then the selections values.
    const auto& sub_sims = embed_sims.embedded_sims.get<simulation>();
    for (const auto factor_id : embed_sims.factors) {
        const auto id  = embed_sims.factors.get<model_id>(factor_id);
        const auto idx = get_index(id);
        const auto msg = message{
            sub_sims[best_sub_id].parameters[idx].reals[0],
            sub_sims[best_sub_id].parameters[idx].reals[1],
            sub_sims[best_sub_id].parameters[idx].reals[2]
        };

        if (auto ret = send_message(sim, wrapper.y[output_port_index], msg[0],
                                    msg[1], msg[2]);
            ret.has_error())
            return ret.error();

        ++output_port_index;
    }

    for (const auto obj_fn_id : embed_sims.selections) {
        const auto idx = pos(best_sub_id, obj_fn_id);
        const auto val = objective_fn[idx];

        if (auto ret = send_message(sim, wrapper.y[output_port_index], val);
            ret.has_error())
            return ret.error();

        ++output_port_index;
    }

    return success();
}

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
static auto compute_embedded_simulation_results(
  const embedded_simulation& embed_sims) noexcept -> expected<vector<real>>
{
    const auto objective_fn_size = embed_sims.selections.size();
    const auto simulation_size   = embed_sims.embedded_sims.size();
    const auto size              = objective_fn_size * simulation_size;
    auto       objective_fn      = vector<real>();

    // NaN marks a selection without observation: the comparisons then never
    // select its sub-simulation.
    if (not objective_fn.resize(size, std::numeric_limits<real>::quiet_NaN()))
        return make_error(
          simulation_errc::simulation_wrapper_not_enough_memory);

    const pos_in_objective_function pos(objective_fn_size);

    // For each embedded simulation, for each objective function element,
    // computes result of the observation trajectory according to tge
    // criteria_type and fill the objective funiton.

    auto& sim_obs = embed_sims.embedded_sims
                      .get<embedded_simulation::simulation_observation>();

    for (const auto sub_id : embed_sims.embedded_sims) {
        for (const auto obj_fn_id : embed_sims.selections) {
            const auto type = embed_sims.selections.get<criteria_type>(
              obj_fn_id);
            const auto mdl_id = embed_sims.selections.get<model_id>(obj_fn_id);

            if (const auto* ptr = sim_obs[sub_id].get(mdl_id)) {
                const auto val = ptr->compute_result(type);
                const auto idx = pos(sub_id, obj_fn_id);

                objective_fn[idx] = val;
            }
        }
    }

    dlogln(0, "full observation (selections in {} objective",
           embed_sims.selections.size());

    for (const auto obj_fn_id : embed_sims.selections) {
        dlogln(2, "- model {}",
               embed_sims.selections.get<name_str>(obj_fn_id).sv());

        const auto mdl_id = embed_sims.selections.get<model_id>(obj_fn_id);

        for (const auto sub_id : embed_sims.embedded_sims) {
            dlogln(4, "- sub-id {}", get_index(sub_id));
            dlogln(4, "|{:^12}|{:^12}|", "time", "value");
            dlogln(4, "|{:-^12}|{:-^12}|", "", "");
            if (const auto* ptr = sim_obs[sub_id].get(mdl_id)) {
                for (const auto& v : ptr->values)
                    dlogln(4, "|{:^12.5}|{:^12.5}|", v.t, v.value);
            }
            dlogln(1, "\n");
        }
    }

    dlogln(0, "compute embedeed selection");
    dlog(4, "|{:^12}|", "sub-id");
    for (const auto obj_fn_id : embed_sims.selections) {
        dlog(0, "{:^12}|", embed_sims.selections.get<name_str>(obj_fn_id).sv());
    }
    dlogln(0, "");
    dlog(4, "|{:-^12}|", "");
    for (const auto obj_fn_id : embed_sims.selections) {
        (void)obj_fn_id;
        dlog(0, "{:-^12}|", "");
    }

    dlogln(0, "");
    for (const auto sub_id : embed_sims.embedded_sims) {
        dlog(4, "|{:^12}|", get_index(sub_id));

        for (const auto obj_fn_id : embed_sims.selections) {
            const auto idx = pos(sub_id, obj_fn_id);

            dlog(0, "{:^12.5}|", objective_fn[idx]);
        }

        dlogln(0, "");
    }

    return objective_fn;
}

/* * * * * *
 *
 * weighted_sum
 *
 * * * * * */

static auto compute_weighted_sum_objective(
  const embedded_simulation& embed_sims,
  std::span<real>            objective_fn) noexcept
  -> expected<embedded_simulation::sub_id>
{
    const pos_in_objective_function pos(embed_sims.selections.size());

    debug::ensure(embed_sims.objective.weighted_sum_params.types.size() ==
                  embed_sims.selections.size());
    debug::ensure(embed_sims.objective.weighted_sum_params.weights.size() ==
                  embed_sims.selections.size());

    // If min-max linearization

    if (embed_sims.objective.weighted_sum_params.norm == norm_type::min_max) {
        for (const auto obj_fn_id : embed_sims.selections) {
            const auto is_max = embed_sims.objective.weighted_sum_params
                                  .types[get_index(obj_fn_id)] ==
                                optimization_type::maximize;

            auto min_val = std::numeric_limits<real>::max();
            auto max_val = std::numeric_limits<real>::lowest();

            for (const auto sub_id : embed_sims.embedded_sims) {
                const auto idx = pos(sub_id, obj_fn_id);
                const auto val = objective_fn[idx];

                min_val = std::min(min_val, val);
                max_val = std::max(max_val, val);
            }

            const auto diff = max_val - min_val;
            const auto div  = diff < 1e-10 ? 1e-10 : diff;

            if (is_max) {
                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx    = pos(sub_id, obj_fn_id);
                    objective_fn[idx] = (objective_fn[idx] - min_val) / div;
                }
            } else {
                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx    = pos(sub_id, obj_fn_id);
                    objective_fn[idx] = (max_val - objective_fn[idx]) / div;
                }
            }
        }
    } else {
        for (const auto obj_fn_id : embed_sims.selections) {
            const auto is_max = embed_sims.objective.weighted_sum_params
                                  .types[get_index(obj_fn_id)] ==
                                optimization_type::maximize;

            if (is_max) {
                auto sum    = 0.0;
                auto sq_sum = 0.0;

                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    sum += objective_fn[idx];
                    sq_sum += objective_fn[idx] * objective_fn[idx];
                }

                const auto mean    = sum / embed_sims.embedded_sims.size();
                const auto std_dev = std::sqrt(sq_sum /
                                               embed_sims.embedded_sims.size());
                const auto div     = std_dev < 1e-10 ? 1e-10 : std_dev;

                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    objective_fn[idx] = (objective_fn[idx] - mean) / div;
                }
            } else {
                auto sum    = 0.0;
                auto sq_sum = 0.0;

                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    sum += objective_fn[idx];
                    sq_sum += objective_fn[idx] * objective_fn[idx];
                }

                const auto mu    = sum / embed_sims.embedded_sims.size();
                const auto sigma = std::sqrt(
                  sq_sum / embed_sims.embedded_sims.size() - mu * mu);

                const auto div = sigma < 1e-10 ? 1e-10 : sigma;

                for (const auto sub_id : embed_sims.embedded_sims) {
                    const auto idx = pos(sub_id, obj_fn_id);

                    objective_fn[idx] = (mu - objective_fn[idx]) / div;
                }
            }
        }
    }

    const auto is_max = embed_sims.objective.type ==
                        optimization_type::maximize;

    auto best_sub_id = undefined<embedded_simulation::sub_id>();
    auto best_value  = is_max ? std::numeric_limits<real>::lowest()
                              : std::numeric_limits<real>::max();

    for (const auto sub_id : embed_sims.embedded_sims) {
        auto value = 0.0;
        for (const auto obj_fn_id : embed_sims.selections) {
            const auto w = embed_sims.objective.weighted_sum_params
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

static auto do_weighted_sum_optimization(const embedded_simulation& embed_sims,
                                         const simulation_wrapper&  wrapper,
                                         simulation& sim) noexcept -> status
{
    debug::ensure(embed_sims.objective.method ==
                  optimization_method::weighted_sum);

    auto objective_fn_ret = compute_embedded_simulation_results(embed_sims);

    if (objective_fn_ret.has_error())
        return objective_fn_ret.error();

    const auto best_sub_id = compute_weighted_sum_objective(embed_sims,
                                                            *objective_fn_ret);

    if (best_sub_id.has_error())
        return best_sub_id.error();

    return embed_sims.embedded_sims.exists(*best_sub_id)
             ? send(embed_sims, wrapper, *best_sub_id, *objective_fn_ret, sim)
             : success();
}

/* * * * * *
 *
 * compute epsilon constrained optimization
 *
 * * * * * */

static auto filter(const embedded_simulation& embed_sims,
                   const std::span<real>      objective_fn) noexcept
  -> vector<embedded_simulation::sub_id>
{
    const pos_in_objective_function pos(embed_sims.selections.size());
    const auto                      max_size = embed_sims.embedded_sims.size();

    auto ret = vector<embedded_simulation::sub_id>(max_size, reserve_tag);

    for (const auto sub_id : embed_sims.embedded_sims) {
        auto all_obj_valid = true;

        for (const auto obj_fn_id : embed_sims.selections) {
            const auto idx = pos(sub_id, obj_fn_id);
            const auto val = objective_fn[idx];

            if (std::isnan(val) or
                not embed_sims.objective.epsilon_constrained_params.valid(
                  get_index(obj_fn_id), val)) {
                all_obj_valid = false;
                break;
            }
        }

        if (all_obj_valid)
            ret.push_back(sub_id);
    }

    return ret;
}

static auto select(
  const embedded_simulation&                   embed_sims,
  const std::span<real>                        objective_fn,
  const std::span<embedded_simulation::sub_id> candidates) noexcept
  -> embedded_simulation::sub_id
{
    const pos_in_objective_function pos(embed_sims.selections.size());
    const auto prim_id = embed_sims.objective.epsilon_constrained_params
                           .primary;

    debug::ensure(embed_sims.selections.exists(prim_id));

    const auto is_max      = embed_sims.objective.type ==
                             optimization_type::maximize;
    auto       best_sub_id = undefined<embedded_simulation::sub_id>();
    auto       best_value  = is_max ? std::numeric_limits<real>::lowest()
                                    : std::numeric_limits<real>::max();

    for (const auto sub_id : candidates) {
        const auto idx = pos(sub_id, prim_id);
        const auto val = objective_fn[idx];

        if (is_max) {
            if (val > best_value) {
                best_value  = val;
                best_sub_id = sub_id;
            }
        } else {
            if (val < best_value) {
                best_value  = val;
                best_sub_id = sub_id;
            }
        }
    }

    return best_sub_id;
}

static auto do_epsilon_constrained_optimization(
  const embedded_simulation& embed_sims,
  const simulation_wrapper&  wrapper,
  simulation&                sim) noexcept -> status
{
    debug::ensure(embed_sims.objective.method ==
                  optimization_method::epsilon_constrained);
    debug::ensure(
      embed_sims.objective.epsilon_constrained_params.epsilons.size() ==
      embed_sims.selections.size());
    debug::ensure(
      embed_sims.objective.epsilon_constrained_params.operations.size() ==
      embed_sims.selections.size());
    debug::ensure(embed_sims.selections.exists(
      embed_sims.objective.epsilon_constrained_params.primary));

    auto objective_fn_ret = compute_embedded_simulation_results(embed_sims);

    if (objective_fn_ret.has_error())
        return objective_fn_ret.error();

    auto filtered    = filter(embed_sims, *objective_fn_ret);
    auto best_sub_id = select(embed_sims, *objective_fn_ret, filtered);

    dlogln(0, "select sub-id {}", get_index(best_sub_id));

    return embed_sims.embedded_sims.exists(best_sub_id)
             ? send(embed_sims, wrapper, best_sub_id, *objective_fn_ret, sim)
             : success();
}

/* * * * * *
 *
 * compute simple optimization for a single @c selection_id
 *
 * * * * * */

static auto do_simple_optimization(const embedded_simulation& embed_sims,
                                   const simulation_wrapper&  wrapper,
                                   simulation& sim) noexcept -> status
{
    debug::ensure(embed_sims.objective.method == optimization_method::simple);
    debug::ensure(
      embed_sims.selections.exists(embed_sims.objective.simple_params.primary));

    const pos_in_objective_function pos(embed_sims.selections.size());

    auto objective_fn_ret = compute_embedded_simulation_results(embed_sims);

    if (objective_fn_ret.has_error())
        return objective_fn_ret.error();

    auto&      objective_fn = *objective_fn_ret;
    const auto sel_id       = embed_sims.objective.simple_params.primary;
    const auto is_max       = embed_sims.objective.type ==
                              optimization_type::maximize;
    auto       best_sub_id  = undefined<embedded_simulation::sub_id>();
    auto       best_value   = is_max ? std::numeric_limits<real>::lowest()
                                     : std::numeric_limits<real>::max();

    for (const auto sub_id : embed_sims.embedded_sims) {
        const auto idx = pos(sub_id, sel_id);
        const auto val = objective_fn[idx];

        if (is_max) {
            if (val > best_value) {
                best_value  = val;
                best_sub_id = sub_id;
            }
        } else {
            if (val < best_value) {
                best_value  = val;
                best_sub_id = sub_id;
            }
        }
    }

    dlogln(0, "select sub-id {}", get_index(best_sub_id));

    return embed_sims.embedded_sims.exists(best_sub_id)
             ? send(embed_sims, wrapper, best_sub_id, objective_fn, sim)
             : success();
}

status simulation_wrapper::lambda(simulation& sim) noexcept
{
    const auto* embed_sims = sim.sims.try_to_get(sim_id);

    if (not embed_sims)
        return make_error(
          simulation_errc::simulation_wrapper_embedded_simulation_search_error);

    switch (embed_sims->objective.method) {
    case optimization_method::epsilon_constrained:
        return do_epsilon_constrained_optimization(*embed_sims, *this, sim);

    case optimization_method::simple:
        return do_simple_optimization(*embed_sims, *this, sim);

    case optimization_method::weighted_sum:
        return do_weighted_sum_optimization(*embed_sims, *this, sim);
    }

    return success();
}

status simulation_wrapper::finalize(simulation& sim) noexcept
{
    auto* embed_sims = sim.sims.try_to_get(sim_id);

    // Nothing to finalize after a failed initialization.
    if (not embed_sims)
        return success();

    auto& sims    = embed_sims->embedded_sims.get<simulation>();
    auto& sim_obs = embed_sims->embedded_sims
                      .get<embedded_simulation::simulation_observation>();

    for (const auto id : embed_sims->embedded_sims) {
        const auto idx   = get_index(id);
        auto&      sim   = sims[idx];
        auto&      sim_o = sim_obs[idx];

        // An over sub-simulation is already finalized by its last run.
        if (is_over(sim))
            continue;

        if (finish(sim, sim_o).has_error())
            return make_error(
              simulation_errc::
                simulation_wrapper_embedded_simulation_finalization_error);
    }

    return success();
}

} // namespace irt
