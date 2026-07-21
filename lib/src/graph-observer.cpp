// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <charconv>

namespace irt {

static void build_graph(graph_observer&  graph_obs,
                        journal_handler& jn,
                        project&         pj,
                        simulation&      sim,
                        tree_node&       graph_parent,
                        graph_component& graph_compo) noexcept
{
    debug::ensure(pj.tree_nodes.try_to_get(graph_obs.tn_id) != nullptr);

    const auto* to = pj.tree_nodes.try_to_get(graph_obs.tn_id);

    if (!to)
        return;

    const auto relative_path =
      pj.build_relative_path(graph_parent, *to, graph_obs.mdl_id);

    const auto* child = graph_parent.tree.get_child();
    while (child) {
        if (child->id == graph_obs.compo_id) {
            const auto  tn_mdl = pj.get_model(*child, relative_path);
            const auto* tn     = pj.tree_nodes.try_to_get(tn_mdl.first);
            auto*       mdl    = sim.models.try_to_get(tn_mdl.second);

            if (tn and mdl) {
                sz index = 0;

                if (auto ret = std::from_chars(
                      child->unique_id.begin(), child->unique_id.end(), index);
                    ret.ec == std::errc{}) {
                    debug::ensure(graph_obs.observers.size() ==
                                  graph_compo.g.nodes.size());
                    debug::ensure(index < graph_obs.observers.size());

                    sim.observe(*mdl, graph_obs.timestep.to_double());

                    graph_obs.observers[index] = mdl->obs_id;
                } else {
                    jn.push(log_level::warning, [&](auto& t, auto& m) noexcept {
                        t = "Graph observer error";
                        format(m,
                               "unique_id {} is not found",
                               child->unique_id.sv());
                    });
                }
            }
        }
        child = child->tree.get_sibling();
    }
}

void graph_observer::init(project&         pj,
                          modeling&        mod,
                          simulation&      sim,
                          journal_handler& jn) noexcept
{
    observers.clear();

    values.write([&](auto& v) noexcept {
        if (auto* tn = pj.tree_nodes.try_to_get(parent_id); tn) {
            mod.ids.read([&](const auto& ids, auto) noexcept {
                if (ids.exists(tn->id)) {
                    if (ids.components[tn->id].type == component_type::graph) {
                        if (auto* graph = ids.graph_components.try_to_get(
                              ids.components[tn->id].id.graph_id)) {
                            const auto len = graph->g.nodes.ssize();

                            observers.resize(len);
                            std::fill_n(
                              observers.data(), len, undefined<observer_id>());
                            v.resize(len, zero);

                            build_graph(*this, jn, pj, sim, *tn, *graph);
                        }
                    }
                }
            });
        }
    });
}

void graph_observer::clear() noexcept
{
    observers.clear();
    values.write([](auto& v) noexcept { v.clear(); });
}

void graph_observer::update(const simulation& sim) noexcept
{
    values.write([&](auto& v) noexcept {
        debug::ensure(v.size() == observers.size());

        if (v.size() != observers.size())
            return;

        for (sz i = 0, e = observers.size(); i < e; ++i) {
            const auto obs_id = observers[i];
            auto*      obs    = sim.observers.try_to_get(obs_id);

            if (not obs)
                continue;

            obs->read_history([&](const auto& h, const auto) {
                if (not h.empty())
                    v[i] = h.back().value;
            });
        }
    });
}

} // namespace irt
