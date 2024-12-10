// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static bool check(const vector<tree_node_id>&                    tn_ids,
                  const vector<model_id>&                        mdl_ids,
                  const vector<observer_id>&                     obs_ids,
                  const vector<color>&                           colors,
                  const vector<variable_observer::type_options>& options,
                  const vector<double>& values) noexcept
{
    const auto len = tn_ids.ssize();

    return len == tn_ids.ssize() and len == mdl_ids.ssize() and
           len == obs_ids.ssize() and len == colors.ssize() and
           len == options.ssize() and len == values.ssize();
}

status variable_observer::init(project& pj, simulation& sim) noexcept
{
    for (auto i = 0, e = m_tn_ids.ssize(); i != e; ++i) {
        auto  obs_id = undefined<observer_id>();
        auto* tn     = pj.tree_nodes.try_to_get(m_tn_ids[i]);
        auto* mdl    = sim.models.try_to_get(m_mdl_ids[i]);

        if (tn and mdl) {
            auto* obs = sim.observers.try_to_get(mdl->obs_id);

            if (obs) {
                obs_id = mdl->obs_id;
                obs->init(raw_buffer_size.value(),
                          linearized_buffer_size.value(),
                          time_step.value());
            } else {
                if (sim.observers.can_alloc()) {
                    auto& new_obs = sim.observers.alloc();
                    new_obs.init(raw_buffer_size.value(),
                                 linearized_buffer_size.value(),
                                 time_step.value());

                    obs_id = sim.observers.get_id(new_obs);
                    sim.observe(*mdl, new_obs);
                } else {
                    // Maybe return another status?
                }
            }
        }

        m_obs_ids[i] = obs_id;
    }

    return success();
}

void variable_observer::clear() noexcept
{
    std::fill_n(m_obs_ids.data(), m_obs_ids.size(), undefined<observer_id>());
}

auto variable_observer::find(const tree_node_id tn,
                             const model_id     mdl) noexcept -> sub_id
{
    for (const auto id : m_ids) {
        const auto idx = get_index(id);

        if (m_tn_ids[idx] == tn and m_mdl_ids[idx] == mdl)
            return id;
    }

    return undefined<variable_observer::sub_id>();
}

bool variable_observer::exists(const tree_node_id tn) noexcept
{
    for (const auto id : m_ids) {
        const auto idx = get_index(id);

        if (m_tn_ids[idx] == tn)
            return true;
    }

    return false;
}

void variable_observer::erase(const tree_node_id tn,
                              const model_id     mdl) noexcept
{
    for (const auto id : m_ids) {
        const auto idx = get_index(id);

        if (m_tn_ids[idx] == tn and m_mdl_ids[idx] == mdl)
            erase(id);
    }
}

void variable_observer::erase(const sub_id i) noexcept
{
    if (const auto idx_opt = m_ids.get(i); idx_opt.has_value())
        m_ids.free(i);
}

variable_observer::sub_id variable_observer::push_back(
  const tree_node_id     tn,
  const model_id         mdl,
  const color            c,
  const type_options     t,
  const std::string_view name) noexcept
{
    check(m_tn_ids, m_mdl_ids, m_obs_ids, m_colors, m_options, m_values);

    if (not m_ids.capacity()) {
        m_ids.reserve(max_observers.value());
        m_tn_ids.resize(max_observers.value());
        m_mdl_ids.resize(max_observers.value());
        m_obs_ids.resize(max_observers.value());
        m_colors.resize(max_observers.value());
        m_options.resize(max_observers.value());
        m_names.resize(max_observers.value());
        m_values.resize(max_observers.value());
    }

    for (auto id : m_ids) {
        const auto idx = get_index(id);

        if (m_tn_ids[idx] == tn and m_mdl_ids[idx] == mdl)
            return id;
    }

    debug::ensure(m_ids.can_alloc(1));

    const auto id  = m_ids.alloc();
    const auto idx = get_index(id);
    m_tn_ids[idx]  = tn;
    m_mdl_ids[idx] = mdl;
    m_obs_ids[idx] = undefined<observer_id>();
    m_colors[idx]  = c;
    m_options[idx] = t;
    m_values[idx]  = 0.0;
    m_names[idx]   = name;

    return id;
}

} // namespace irt
