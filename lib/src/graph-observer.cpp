// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <charconv>

namespace irt {

static void build_graph(graph_observer&  graph_obs,
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
                auto index = 0;

                if (auto ret = std::from_chars(
                      child->unique_id.begin(), child->unique_id.end(), index);
                    ret.ec == std::errc{}) {
                    debug::ensure(0 <= index);
                    debug::ensure(graph_obs.observers.ssize() ==
                                  graph_compo.nodes.ssize());
                    debug::ensure(index < graph_obs.observers.ssize());

                    auto&      obs    = sim.observers.alloc();
                    const auto obs_id = sim.observers.get_id(obs);
                    sim.observe(*mdl, obs);

                    graph_obs.observers[index] = obs_id;
                } else {
                    debug::log("unique_id {} is not found",
                               child->unique_id.sv());
                }
            }
        }
        child = child->tree.get_sibling();
    }
}

void graph_observer::init(project& pj, modeling& mod, simulation& sim) noexcept
{
    observers.clear();
    values.clear();
    values_2nd.clear();

    if (auto* tn = pj.tree_nodes.try_to_get(parent_id); tn) {
        if (auto* compo = mod.components.try_to_get(tn->id);
            compo and compo->type == component_type::graph) {
            if (auto* graph =
                  mod.graph_components.try_to_get(compo->id.graph_id);
                graph) {
                const auto len = graph->nodes.ssize();

                observers.resize(len);
                values.resize(len);

                std::fill_n(observers.data(), len, undefined<observer_id>());
                std::fill_n(values.data(), len, zero);
                std::fill_n(values_2nd.data(), len, zero);

                build_graph(*this, pj, sim, *tn, *graph);
            }
        }
    }

    tn = sim.t;
}

void graph_observer::clear() noexcept
{
    observers.clear();
    values.clear();
    values_2nd.clear();

    tn = 0;
}

void graph_observer::update(const simulation& sim) noexcept
{
    debug::ensure(nodes == observers.ssize());
    debug::ensure(values.ssize() == observers.ssize());

    std::fill_n(values_2nd.data(), values_2nd.capacity(), 0.0);
    for (int i = 0; i < nodes; ++i) {
        if (const auto* obs = sim.observers.try_to_get(observers[i]); obs) {
            if (obs->states[observer_flags::use_linear_buffer]) {
                values_2nd[i] = not obs->linearized_buffer.empty()
                                  ? obs->linearized_buffer.back().y
                                  : zero;
            } else {
                values_2nd[i] =
                  not obs->buffer.empty() ? obs->buffer.back()[1] : zero;
            }
        }
    }

    tn = sim.t + time_step;

    std::unique_lock lock{ mutex };
    std::swap(values, values_2nd);
}

} // namespace irt
