// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

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

            if (tn && mdl) {
                debug::ensure(is_numeric_castable<i32>(child->unique_id));

                const auto index = static_cast<i32>(child->unique_id);
                debug::ensure(0 <= index);
                debug::ensure(graph_obs.observers.ssize() ==
                              graph_compo.children.ssize());
                debug::ensure(index < graph_obs.observers.ssize());

                auto&      obs    = sim.observers.alloc();
                const auto obs_id = sim.observers.get_id(obs);
                sim.observe(*mdl, obs);

                graph_obs.observers[index] = obs_id;
            }
        }

        child = child->tree.get_sibling();
    }
}

void graph_observer::init(project& pj, modeling& mod, simulation& sim) noexcept
{
    observers.clear();
    values.clear();

    if_tree_node_is_graph_do(
      pj,
      mod,
      parent_id,
      [&](auto& graph_parent_tn, auto& compo, auto& graph) noexcept {
          debug::ensure(compo.type == component_type::graph);

          const auto len = graph.children.ssize();

          observers.resize(len);
          values.resize(len);

          std::fill_n(observers.data(), len, undefined<observer_id>());
          std::fill_n(values.data(), len, zero);

          build_graph(*this, pj, sim, graph_parent_tn, graph);
      });
}

void graph_observer::clear() noexcept
{
    observers.clear();
    values.clear();
}

void graph_observer::update(const simulation& sim) noexcept
{
    debug::ensure(nodes == observers.ssize());
    debug::ensure(values.ssize() == observers.ssize());

    for (int i = 0; i < nodes; ++i) {
        const auto* obs = sim.observers.try_to_get(observers[i]);

        values[i] = obs && !obs->linearized_buffer.empty()
                      ? obs->linearized_buffer.back().y
                      : zero;
    }
}

} // namespace irt
