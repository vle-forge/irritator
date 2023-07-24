// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <optional>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/timeline.hpp>

#include "application.hpp"
#include "internal.hpp"

namespace irt {

bool show_local_observers(application& app,
                          tree_node&   tn,
                          component& /*compo*/,
                          graph_component& /*graph*/) noexcept
{
    if (ImGui::CollapsingHeader("Local graph observation")) {
        if (app.pj.graph_observers.can_alloc() && ImGui::Button("+##graph")) {
            auto& graph = app.pj.graph_observers.alloc();

            graph.child.parent_id = app.pj.tree_nodes.get_id(tn);
            graph.child.tn_id     = undefined<tree_node_id>();
            graph.child.mdl_id    = undefined<model_id>();

            tn.graph_observer_ids.emplace_back(
              app.pj.graph_observers.get_id(graph));

            format(graph.name,
                   "rename-{}",
                   get_index(app.pj.graph_observers.get_id(graph)));
        }

        std::optional<graph_observer_id> to_delete;
        bool                             is_modified = false;

        for_specified_data(
          app.pj.graph_observers,
          tn.graph_observer_ids,
          [&](auto& graph) noexcept {
              ImGui::PushID(&graph);

              if (ImGui::InputFilteredString("name", graph.name))
                  is_modified = true;

              ImGui::SameLine();

              if (ImGui::Button("del"))
                  to_delete =
                    std::make_optional(app.pj.graph_observers.get_id(graph));

              ImGui::TextFormatDisabled(
                "graph-id {} tree-node-id {} model-id {}",
                ordinal(graph.child.parent_id),
                ordinal(graph.child.tn_id),
                ordinal(graph.child.mdl_id));

              if_data_exists_do(
                app.sim.models, graph.child.mdl_id, [&](auto& mdl) noexcept {
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl.type)]);
                });

              show_select_model_box("Select model",
                                    "Choose model to observe",
                                    app,
                                    tn,
                                    graph.child);

              ImGui::PopID();
          });

        if (to_delete.has_value()) {
            is_modified = true;
            app.pj.graph_observers.free(*to_delete);
        }
    }

    return false;
}

bool graph_simulation_editor::show_settings(
  tree_node& /* tn */,
  component& /*compo*/,
  graph_component& /* graph */) noexcept
{
    // auto* ed  = container_of(this, &simulation_editor::graph_sim);
    // auto& app = container_of(ed, &application::simulation_ed);
    //
    // const auto graph_id = app.mod.graph_components.get_id(graph);
    // if (graph_id != current_id)
    //     graph_simulation_rebuild(*this, graph, graph_id);
    //
    // return graph_simulation_show_settings(*app, *this, tn, graph);

    return true;
}

bool graph_simulation_editor::show_observations(
  tree_node& /* tn */,
  component& /*compo*/,
  graph_component& /* graph */) noexcept
{
    // auto* ed  = container_of(this, &simulation_editor::graph_sim);
    // auto& app = container_of(ed, &application::simulation_ed);
    //
    // const auto graph_id = app.mod.graph_components.get_id(graph);
    // if (graph_id != current_id)
    //     graph_simulation_rebuild(*this, graph, graph_id);
    //
    // return graph_simulation_show_observations(*app, *this, tn, graph);

    return true;
}

} // namespace irt
