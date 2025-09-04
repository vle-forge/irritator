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
                                  graph_compo.g.nodes.ssize());
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

    values.read_write([&](auto& v) noexcept {
        if (auto* tn = pj.tree_nodes.try_to_get(parent_id); tn) {
            if (auto* compo = mod.components.try_to_get<component>(tn->id);
                compo and compo->type == component_type::graph) {
                if (auto* graph =
                      mod.graph_components.try_to_get(compo->id.graph_id);
                    graph) {
                    const auto len = graph->g.nodes.ssize();

                    observers.resize(len);
                    std::fill_n(
                      observers.data(), len, undefined<observer_id>());
                    v.resize(len, zero);

                    build_graph(*this, pj, sim, *tn, *graph);
                }
            }
        }
    });

    tn = sim.current_time();
}

void graph_observer::clear() noexcept
{
    observers.clear();
    values.read_write([](auto& v) noexcept { v.clear(); });

    tn = 0;
}

void graph_observer::update(const simulation& sim) noexcept
{
    values.read_write([&](auto& v) noexcept {
        debug::ensure(v.ssize() == observers.ssize());

        if (v.ssize() != observers.ssize())
            return;

        std::fill_n(v.data(), v.capacity(), 0.0);
        for (int i = 0, e = observers.ssize(); i < e; ++i) {
            if (const auto* obs = sim.observers.try_to_get(observers[i]); obs) {
                if (obs->states[observer_flags::use_linear_buffer]) {
                    obs->linearized_buffer.try_read_only(
                      [](const auto& buf, auto& v) {
                          v = not buf.empty() ? buf.back().y : zero;
                      },
                      v[i]);
                } else {
                    obs->buffer.try_read_only(
                      [](const auto& buf, auto& v) {
                          v = not buf.empty() ? buf.back()[1] : zero;
                      },
                      v[i]);
                }
            }
        }

        tn = sim.current_time() + time_step;
    });
}

} // namespace irt
