// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static bool already_exists(const simulation_component::selection_t& sel,
                           const tree_node_id                       tn_id,
                           const model_id mdl_id) noexcept
{
    const auto& tn = sel.get<tree_node_id>();
    const auto& md = sel.get<model_id>();

    for (const auto id : sel)
        if (tn[id] == tn_id and md[id] == mdl_id)
            return true;

    return false;
};

status simulation_component::assign(project&& pj_to_move) noexcept
{
    pj = std::move(pj_to_move);

    {
        factors.clear();

        const auto& names = pj.parameters.get<name_str>();
        const auto& tns   = pj.parameters.get<tree_node_id>();
        const auto& mdls  = pj.parameters.get<model_id>();

        for (const auto id : pj.parameters) {
            const auto idx = get_index(id);

            if (not pj.tree_nodes.exists(tns[idx]) or
                not pj.sim.models.exists(mdls[idx]))
                continue;

            const auto& mdl      = pj.sim.models.get(mdls[idx]);
            const auto  can_edit = any_equal(mdl.type,
                                             dynamics_type::constant,
                                             dynamics_type::qss1_integrator,
                                             dynamics_type::qss2_integrator,
                                             dynamics_type::qss3_integrator);

            if (not can_edit)
                continue;

            if (not factors.can_alloc(1) and not factors.grow<2, 1>(1))
                return make_error(modeling_errc::component_container_full);

            const auto new_id = factors.alloc_id();

            factors.get<unique_id_path>(new_id) =
              pj.build_unique_id_path(tns[idx], mdls[idx]);
            factors.get<tree_node_id>(new_id)        = tns[idx];
            factors.get<model_id>(new_id)            = mdls[idx];
            factors.get<name_str>(new_id)            = names[idx];
            factors.get<factor_type>(new_id)         = factor_type::single;
            factors.get<single_factor>(new_id).value = 0;
            factors.get<fixed_factor>(new_id).values.clear();
            factors.get<random_factor>(new_id) = random_factor{};
        }
    }

    {
        selections.clear();

        for (const auto& vobs : pj.variable_observers) {
            const auto& tns   = vobs.subs.get<tree_node_id>();
            const auto& mdls  = vobs.subs.get<model_id>();
            const auto& names = vobs.subs.get<name_str>();

            for (const auto sub_id : vobs.subs) {
                const auto idx = get_index(sub_id);

                if (not pj.tree_nodes.exists(tns[idx]) or
                    not pj.sim.models.exists(mdls[idx]))
                    continue;

                // Do not add a selection if it already exists in the selection
                // list. @todo: we should document this feature or block it in
                // the GUI

                if (already_exists(selections, tns[idx], mdls[idx]))
                    continue;

                if (not selections.can_alloc(1) and
                    not selections.grow<2, 1>(1))
                    return make_error(modeling_errc::component_container_full);

                const auto new_id = selections.alloc_id();

                selections.get<unique_id_path>(new_id) =
                  pj.build_unique_id_path(tns[idx], mdls[idx]);
                selections.get<tree_node_id>(new_id)  = tns[idx];
                selections.get<model_id>(new_id)      = mdls[idx];
                selections.get<name_str>(new_id)      = names[idx];
                selections.get<criteria_type>(new_id) = criteria_type::max;
            }
        }
    }

    {
        objective.clear();

        if (not selections.empty()) {
            objective.weighted_sum_params.types.resize(
              selections.size(), optimization_type::maximize);
            objective.weighted_sum_params.weights.resize(
              selections.size(), 1.0 / selections.size());
            objective.epsilon_constrained_params.epsilons.resize(
              selections.size(), 0.0);
            objective.epsilon_constrained_params.operations.resize(
              selections.size(), operation_type::greater_equal);
            objective.epsilon_constrained_params.primary = *selections.begin();
            objective.simple_params.primary              = *selections.begin();
        }
    }

    return success();
}

status simulation_component::copy_to(simulation& sim) const noexcept
{
    sim = pj.sim;

    sim.factors.clear();
    sim.selections.clear();

    if (not sim.factors.can_alloc(factors.size()) and
        not sim.factors.grow<2, 1>(factors.size()))
        return make_error(
          simulation_errc::simulation_wrapper_not_enough_memory);

    if (not sim.selections.can_alloc(selections.size()) and
        not sim.selections.grow<2, 1>(selections.size()))
        return make_error(
          simulation_errc::simulation_wrapper_not_enough_memory);

    for (const auto id : factors) {
        const auto path       = factors.get<unique_id_path>(id);
        const auto type       = factors.get<factor_type>(id);
        const auto tn_mdl_opt = pj.get_model_path(path);

        if (tn_mdl_opt.has_value()) {
            const auto new_id = sim.factors.alloc_id();

            sim.factors.get<model_id>(new_id)    = tn_mdl_opt->second;
            sim.factors.get<name_str>(new_id)    = factors.get<name_str>(id);
            sim.factors.get<factor_type>(new_id) = type;

            switch (type) {
            case factor_type::single:
            case factor_type::single_add:
            case factor_type::single_mult:
                sim.factors.get<single_factor>(new_id) =
                  factors.get<single_factor>(id);
                break;

            case factor_type::fixed:
            case factor_type::fixed_add:
            case factor_type::fixed_mult:
                sim.factors.get<fixed_factor>(new_id) =
                  factors.get<fixed_factor>(id);
                break;

            case factor_type::random:
            case factor_type::random_add:
            case factor_type::random_mult:
                sim.factors.get<random_factor>(new_id) =
                  factors.get<random_factor>(id);
                break;
            }
        }
    }

    for (const auto id : selections) {
        const auto& path       = selections.get<unique_id_path>(id);
        const auto tn_mdl_opt = pj.get_model_path(path);

        if (tn_mdl_opt.has_value()) {
            const auto new_id = sim.selections.alloc_id();

            sim.selections.get<model_id>(new_id) = tn_mdl_opt->second;
            sim.selections.get<name_str>(new_id) = selections.get<name_str>(id);
            sim.selections.get<criteria_type>(new_id) =
              selections.get<criteria_type>(id);
        }
    }

    sim.objective = objective;

    return success();
}

} // namespace irt