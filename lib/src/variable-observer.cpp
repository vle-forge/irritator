// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

status variable_observer::init(project& pj, simulation& sim) noexcept
{
    for (const auto id : m_vars) {
        auto       obs_id = undefined<observer_id>();
        const auto tn_id  = m_vars.get<tree_node_id>(id);
        const auto mdl_id = m_vars.get<model_id>(id);
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

        m_vars.get<observer_id>(id) = obs_id;
    }

    return success();
}

void variable_observer::clear() noexcept { m_vars.clear(); }

auto variable_observer::find(const tree_node_id tn, const model_id mdl) noexcept
  -> sub_id
{
    for (const auto id : m_vars)
        if (m_vars.get<tree_node_id>(id) == tn and
            m_vars.get<model_id>(id) == mdl)
            return id;

    return undefined<variable_observer::sub_id>();
}

bool variable_observer::exists(const tree_node_id tn) noexcept
{
    for (const auto id : m_vars)
        if (m_vars.get<tree_node_id>(id) == tn)
            return true;

    return false;
}

void variable_observer::erase(const tree_node_id tn,
                              const model_id     mdl) noexcept
{
    for (const auto id : m_vars)
        if (m_vars.get<tree_node_id>(id) == tn and
            m_vars.get<model_id>(id) == mdl)
            m_vars.free(id);
}

void variable_observer::erase(const sub_id i) noexcept
{
    if (m_vars.exists(i))
        m_vars.free(i);
}

variable_observer::sub_id variable_observer::push_back(
  const tree_node_id     tn,
  const model_id         mdl,
  const color&           c,
  const type_options     t,
  const std::string_view name) noexcept
{
    if (not m_vars.can_alloc(1) or m_vars.grow<3, 2>(1))
        return undefined<variable_observer::sub_id>();

    if (const auto id = find(tn, mdl); is_defined(id))
        return id;

    const auto id = m_vars.alloc_id();

    m_vars.get<tree_node_id>(id) = tn;
    m_vars.get<model_id>(id)     = mdl;
    m_vars.get<observer_id>(id)  = undefined<observer_id>();
    m_vars.get<color>(id)        = c;
    m_vars.get<type_options>(id) = t;
    m_vars.get<name_str>(id)     = name;

    return id;
}

} // namespace irt
