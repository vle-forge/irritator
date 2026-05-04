// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

status variable_observer::init(project& pj, simulation& sim) noexcept
{
    for (const auto id : subs) {
        auto       obs_id = undefined<observer_id>();
        const auto tn_id  = subs.get<tree_node_id>(id);
        const auto mdl_id = subs.get<model_id>(id);
        auto*      tn     = pj.tree_nodes.try_to_get(tn_id);
        auto*      mdl    = sim.models.try_to_get(mdl_id);

        if (tn and mdl) {
            auto* obs = sim.observers.try_to_get(mdl->obs_id);

            if (obs) {
                obs_id = mdl->obs_id;
                obs->init(observer::buffer_size_t(raw_buffer_size.value()),
                          observer::linearized_buffer_size_t(
                            linearized_buffer_size.value()),
                          time_step.value());
            } else {
                if (not sim.observers.can_alloc() and
                    not sim.observers.grow<3, 2>())
                    return new_error(simulation_errc::observers_container_full);

                auto& new_obs = sim.observers.alloc();
                new_obs.init(observer::buffer_size_t(raw_buffer_size.value()),
                             observer::linearized_buffer_size_t(
                               linearized_buffer_size.value()),
                             time_step.value());

                obs_id = sim.observers.get_id(new_obs);
                sim.observe(*mdl, new_obs);
            }
        }

        subs.get<observer_id>(id) = obs_id;
    }

    return success();
}

void variable_observer::clear() noexcept { subs.clear(); }

auto variable_observer::find(const tree_node_id tn, const model_id mdl) noexcept
  -> sub_id
{
    for (const auto id : subs)
        if (subs.get<tree_node_id>(id) == tn and subs.get<model_id>(id) == mdl)
            return id;

    return undefined<variable_observer::sub_id>();
}

bool variable_observer::exists(const tree_node_id tn) noexcept
{
    for (const auto id : subs)
        if (subs.get<tree_node_id>(id) == tn)
            return true;

    return false;
}

void variable_observer::erase(const tree_node_id tn,
                              const model_id     mdl) noexcept
{
    for (const auto id : subs)
        if (subs.get<tree_node_id>(id) == tn and subs.get<model_id>(id) == mdl)
            subs.free(id);
}

void variable_observer::erase(const sub_id i) noexcept
{
    if (subs.exists(i))
        subs.free(i);
}

variable_observer::sub_id variable_observer::push_back(
  const tree_node_id      tn,
  const model_id          mdl,
  const color&            c,
  const plot_type_options t,
  const std::string_view  name) noexcept
{
    if (not subs.can_alloc(1) or subs.grow<3, 2>(1))
        return undefined<variable_observer::sub_id>();

    if (const auto id = find(tn, mdl); is_defined(id))
        return id;

    const auto id = subs.alloc_id();

    subs.get<tree_node_id>(id)      = tn;
    subs.get<model_id>(id)          = mdl;
    subs.get<observer_id>(id)       = undefined<observer_id>();
    subs.get<color>(id)             = c;
    subs.get<plot_type_options>(id) = t;
    subs.get<name_str>(id)          = name;

    return id;
}

} // namespace irt
