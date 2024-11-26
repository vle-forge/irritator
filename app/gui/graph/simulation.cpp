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
    auto to_del      = std::optional<graph_observer_id>();
    bool is_modified = false;

    for_specified_data(
      app.pj.graph_observers, tn.graph_observer_ids, [&](auto& graph) noexcept {
          ImGui::PushID(&graph);

          if (ImGui::InputFilteredString("name", graph.name))
              is_modified = true;

          ImGui::SameLine();

          if (ImGui::Button("del"))
              to_del = std::make_optional(app.pj.graph_observers.get_id(graph));

          ImGui::TextFormatDisabled(
            "graph-id {} component {} tree-node-id {} model-id {}",
            ordinal(graph.parent_id),
            ordinal(graph.compo_id),
            ordinal(graph.tn_id),
            ordinal(graph.mdl_id));

          if_data_exists_do(
            app.sim.models, graph.mdl_id, [&](auto& mdl) noexcept {
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);
            });

          show_select_model_box(
            "Select model", "Choose model to observe", app, tn, graph);

          ImGui::PopID();
      });

    if (app.pj.graph_observers.can_alloc() && ImGui::Button("+##graph")) {
        auto&      graph    = app.pj.alloc_graph_observer();
        const auto graph_id = app.pj.graph_observers.get_id(graph);

        graph.parent_id = app.pj.tree_nodes.get_id(tn);
        graph.compo_id  = undefined<component_id>();
        graph.tn_id     = undefined<tree_node_id>();
        graph.mdl_id    = undefined<model_id>();
        tn.graph_observer_ids.emplace_back(graph_id);

        if (not app.pj.file_obs.ids.can_alloc(1))
            app.pj.file_obs.grow();

        if (app.pj.file_obs.ids.can_alloc(1)) {
            const auto file_obs_id = app.pj.file_obs.ids.alloc();
            const auto idx         = get_index(file_obs_id);

            app.pj.file_obs.subids[idx].graph = graph_id;
            app.pj.file_obs.types[idx]        = file_observers::type::graph;
            app.pj.file_obs.enables[idx]      = false;
        }

        is_modified = true;
    }

    if (to_del.has_value()) {
        is_modified = true;
        app.pj.graph_observers.free(*to_del);
    }

    return is_modified;
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
    // auto& ed  = container_of(this, &simulation_editor::graph_sim);
    // auto& app = container_of(&ed, &application::simulation_ed);

    // const auto graph_id = app.mod.graph_components.get_id(graph);
    // if (graph_id != current_id)
    //     graph_simulation_rebuild(*this, graph, graph_id);
    // return graph_simulation_show_observations(*app, *this, tn, graph);

    return true;
}

} // namespace irt
