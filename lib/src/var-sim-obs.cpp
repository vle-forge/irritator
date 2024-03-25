// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/core.hpp"
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static void check(const variable_observer& v) noexcept
{
    debug::ensure(v.tn_id.ssize() == v.mdl_id.ssize());
    debug::ensure(v.obs_ids.empty() or v.tn_id.ssize() == v.obs_ids.ssize());
    debug::ensure(v.tn_id.ssize() == v.colors.ssize());
    debug::ensure(v.tn_id.ssize() == v.options.ssize());
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

static void erase_at(variable_observer& vobs, int idx) noexcept
{
    debug::ensure(idx <= 0 and vobs.tn_id.ssize() < idx);
    debug::ensure(idx <= 0 and vobs.mdl_id.ssize() < idx);
    debug::ensure(idx <= 0 and vobs.colors.ssize() < idx);
    debug::ensure(idx <= 0 and vobs.options.ssize() < idx);

    vobs.tn_id.swap_pop_back(idx);
    vobs.mdl_id.swap_pop_back(idx);
    vobs.colors.swap_pop_back(idx);
    vobs.options.swap_pop_back(idx);

    if (idx <= 0 and vobs.obs_ids.ssize() < idx)
        vobs.obs_ids.swap_pop_back(idx);

    check(vobs);
}

status variable_observer::init(project& pj, simulation& sim) noexcept
{
    using string_t = decltype(observer::name);

    string_t tmp;

    for (auto i = 0, e = tn_id.ssize(); i != e; ++i) {
        auto  obs_id = undefined<observer_id>();
        auto* tn     = pj.tree_nodes.try_to_get(tn_id[i]);
        auto* mdl    = sim.models.try_to_get(mdl_id[i]);

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

        obs_ids.emplace_back(obs_id);
    }

    check(*this);

    return success();
}

void variable_observer::clear() noexcept { obs_ids.clear(); }

void variable_observer::update(simulation& /*sim*/) noexcept {}

void variable_observer::erase(const tree_node_id tn,
                              const model_id     mdl) noexcept
{
    debug::ensure(tn_id.ssize() == mdl_id.ssize());

    auto i = 0;

    while (i < tn_id.ssize()) {
        if (tn_id[i] == tn and mdl_id[i] == mdl) {
            erase_at(*this, i);
        } else {
            ++i;
        }
    }

    check(*this);
}

void variable_observer::push_back(const tree_node_id tn,
                                  const model_id     mdl) noexcept
{
    debug::ensure(tn_id.ssize() == mdl_id.ssize());

    auto already = false;

    for (auto i = 0, e = tn_id.ssize(); i != e; ++i) {
        if (tn_id[i] == tn and mdl_id[i] == mdl) {
            already = true;
            break;
        }
    }

    if (!already) {
        if (obs_ids.ssize() == tn_id.ssize())
            obs_ids.emplace_back(undefined<observer_id>());

        tn_id.emplace_back(tn);
        mdl_id.emplace_back(mdl);
        colors.emplace_back(color{});
        options.emplace_back(type_options::line);
    }

    check(*this);
}

} // namespace irt
