// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <charconv>

namespace irt {

static auto init_or_reuse_observer(project&       pj,
                                   model&         mdl,
                                   const fraction timestep) noexcept
  -> observer_id
{
    if (auto* obs = pj.sim.observers.try_to_get(mdl.obs_id)) {
        obs->reset();
    } else {
        if (pj.sim.observe(mdl, timestep.to_double()).has_error()) {
            // @todo Handle error
        }
    }

    return mdl.obs_id;
}

static void build_graph(graph_observer&  graph_obs,
                        journal_handler& jn,
                        project&         pj,
                        simulation&      sim,
                        tree_node&       graph_parent,
                        graph_component& graph_compo) noexcept
{
    debug::ensure(pj.tree_nodes.try_to_get(graph_obs.tn_id) != nullptr);

    const auto* to = pj.tree_nodes.try_to_get(graph_obs.tn_id);

    if (debug::check(to)) {
        const auto relative_path =
          pj.build_relative_path(graph_parent, *to, graph_obs.mdl_id);

        const auto* child = graph_parent.tree.get_child();
        while (child) {
            if (child->id == graph_obs.compo_id) {
                const auto  tn_mdl = pj.get_model(*child, relative_path);
                const auto* tn     = pj.tree_nodes.try_to_get(tn_mdl.first);
                auto*       mdl    = pj.sim.models.try_to_get(tn_mdl.second);

                if (not(tn and mdl))
                    continue;

                std::size_t index = 0;
                const auto  ret   = std::from_chars(
                  child->unique_id.begin(), child->unique_id.end(), index);

                if (ret.ec == std::errc{}) {
                    debug::ensure(graph_obs.observers.size() ==
                                  graph_compo.g.nodes.size());
                    debug::ensure(index < graph_obs.observers.size());

                    graph_obs.observers[index] =
                      init_or_reuse_observer(pj, *mdl, graph_obs.timestep);
                } else {
                    jn.push(log_level::warning, [&](auto& t, auto& m) noexcept {
                        t = "Graph observer error";
                        format(m,
                               "unique_id {} is not found",
                               child->unique_id.sv());
                    });
                }
            }

            child = child->tree.get_sibling();
        }
    }
}

void graph_observer::init(project&         pj,
                          modeling&        mod,
                          simulation&      sim,
                          journal_handler& jn) noexcept
{
    observers.clear();
    values.clear();

    if (auto* tn = pj.tree_nodes.try_to_get(parent_id)) {
        mod.ids.read([&](const auto& ids, auto) noexcept {
            if (ids.exists(tn->id)) {
                if (ids.components[tn->id].type == component_type::graph) {
                    if (auto* graph = ids.graph_components.try_to_get(
                          ids.components[tn->id].id.graph_id)) {
                        const auto len = graph->g.nodes.ssize();

                        observers.resize(len);
                        std::fill_n(
                          observers.data(), len, undefined<observer_id>());

                        values.resize(len, zero);

                        build_graph(*this, jn, pj, sim, *tn, *graph);
                    }
                }
            }
        });
    }
}

void graph_observer::clear() noexcept
{
    observers.clear();
    values.clear();
}

void graph_observer::update(const project& pj) noexcept
{
    if (debug::check(values.size() == observers.size())) {
        for (sz i = 0, e = observers.size(); i < e; ++i) {
            const auto  obs_id = observers[i];
            const auto* obs    = pj.sim.observers.try_to_get(obs_id);

            if (obs) {
                obs->read_history([&](const auto& h, const auto) {
                    if (not h.empty())
                        values[i] = h.back().value;
                });
            }
        }
    }
}

} // namespace irt
