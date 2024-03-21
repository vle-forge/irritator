// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

status variable_simulation_observer::init(project&           pj,
                                          simulation&        sim,
                                          variable_observer& v_obs) noexcept
{
    using string_t = decltype(observer::name);

    string_t tmp;

    for (auto i = 0, e = v_obs.tn_id.ssize(); i != e; ++i) {
        auto* tn  = pj.tree_nodes.try_to_get(v_obs.tn_id[i]);
        auto* mdl = sim.models.try_to_get(v_obs.mdl_id[i]);

        if (tn and mdl) {
            auto* obs    = sim.observers.try_to_get(mdl->obs_id);
            auto  obs_id = undefined<observer_id>();

            if (obs) {
                obs_id = mdl->obs_id;
            } else {
                if (sim.observers.can_alloc()) {
                    format(tmp, "{}", i);
                    auto& new_obs = sim.observers.alloc(tmp.sv());
                    obs_id        = sim.observers.get_id(new_obs);

                    sim.observe(*mdl, new_obs);
                    obs = &new_obs;
                } else {
                    // Maybe return another status?
                }
            }

            observers.emplace_back(obs_id);
        }
    }

    id = pj.variable_observers.get_id(v_obs);

    return success();
}

void variable_simulation_observer::clear() noexcept { observers.clear(); }

void variable_simulation_observer::update(simulation& /*sim*/) noexcept {}

} // namespace irt