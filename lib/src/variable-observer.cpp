// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static bool check(vector<tree_node_id>&                    tn_ids,
                  vector<model_id>&                        mdl_ids,
                  vector<observer_id>&                     obs_ids,
                  vector<color>&                           colors,
                  vector<variable_observer::type_options>& options) noexcept
{
    const auto len = tn_ids.ssize();

    return len == tn_ids.ssize() and len == mdl_ids.ssize() and
           len == obs_ids.ssize() and len == colors.ssize() and
           len == options.ssize();
}

static void init_obs(observer&   obs,
                     const i32   raw_buffer_size,
                     const i32   linearized_buffer_size,
                     const float time_step) noexcept
{
    obs.buffer.clear();
    obs.buffer.reserve(raw_buffer_size);
    obs.linearized_buffer.clear();
    obs.linearized_buffer.reserve(linearized_buffer_size);
    obs.time_step = time_step;
}

status variable_observer::init(project& pj, simulation& sim) noexcept
{
    using string_t = decltype(observer::name);

    string_t tmp;

    for (auto i = 0, e = m_tn_ids.ssize(); i != e; ++i) {
        auto  obs_id = undefined<observer_id>();
        auto* tn     = pj.tree_nodes.try_to_get(m_tn_ids[i]);
        auto* mdl    = sim.models.try_to_get(m_mdl_ids[i]);

        if (tn and mdl) {
            auto* obs = sim.observers.try_to_get(mdl->obs_id);

            if (obs) {
                obs_id = mdl->obs_id;
                init_obs(*obs,
                         raw_buffer_size.value(),
                         linearized_buffer_size.value(),
                         time_step.value());
            } else {
                if (sim.observers.can_alloc()) {
                    format(tmp, "{}", i);
                    auto& new_obs = sim.observers.alloc(tmp.sv());
                    init_obs(new_obs,
                             raw_buffer_size.value(),
                             linearized_buffer_size.value(),
                             time_step.value());

                    obs_id = sim.observers.get_id(new_obs);
                    sim.observe(*mdl, new_obs);
                    obs = &new_obs;
                } else {
                    // Maybe return another status?
                }
            }
        }

        m_obs_ids[i] = obs_id;
    }

    return success();
}

void variable_observer::clear() noexcept { m_obs_ids.clear(); }

void variable_observer::update(simulation& /*sim*/) noexcept {}

void variable_observer::erase(const tree_node_id tn,
                              const model_id     mdl) noexcept
{
    auto i = 0;

    while (i < m_tn_ids.ssize()) {
        if (m_tn_ids[i] == tn and m_mdl_ids[i] == mdl) {
            erase(i);
        } else {
            ++i;
        }
    }
}

void variable_observer::erase(const int i) noexcept
{
    debug::ensure(0 <= i and i < m_tn_ids.ssize());

    if (0 <= i and i < m_tn_ids.ssize()) {
        m_tn_ids.swap_pop_back(i);
        m_mdl_ids.swap_pop_back(i);
        m_colors.swap_pop_back(i);
        m_options.swap_pop_back(i);
        m_obs_ids.swap_pop_back(i);
    }
}

void variable_observer::push_back(const tree_node_id tn,
                                  const model_id     mdl,
                                  const color        c,
                                  const type_options t) noexcept
{
    check(m_tn_ids, m_mdl_ids, m_obs_ids, m_colors, m_options);

    auto already = false;

    for (auto i = 0, e = m_tn_ids.ssize(); i != e; ++i) {
        if (m_tn_ids[i] == tn and m_mdl_ids[i] == mdl) {
            already = true;
            break;
        }
    }

    if (not already) {
        m_tn_ids.emplace_back(tn);
        m_mdl_ids.emplace_back(mdl);
        m_obs_ids.emplace_back(undefined<observer_id>());
        m_colors.emplace_back(c);
        m_options.emplace_back(t);
    }
}

} // namespace irt
