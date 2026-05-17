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

bool show_local_observers(application&    app,
                          project_editor& ed,
                          const component_access& /*ids*/,
                          tree_node& tn,
                          const component& /*compo*/,
                          const graph_component& cgraph) noexcept
{
    auto       to_del      = std::optional<graph_observer_id>();
    auto       is_modified = 0;
    const auto can_alloc =
      ed.pj.graph_observers.can_alloc(1) or ed.pj.graph_observers.grow<3, 2>(8);

    if (ImGui::BeginTable("Graph observers", 6)) {
        tn.graph_observer_ids.erase_if([&](const auto id) {
            return ed.pj.graph_observers.try_to_get(id) == nullptr;
        });

        for (const auto id : tn.graph_observer_ids) {
            auto& graph = ed.pj.graph_observers.get(id);
            ImGui::PushID(get_index(id));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputFilteredString("name", graph.name))
                ++is_modified;
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::DragFloatRange2(
              "##scale", &graph.scale_min, &graph.scale_max, 0.01f);
            ImGui::PopItemWidth();
            ImGui::TableNextColumn();
            if (ImPlot::ColormapButton(ImPlot::GetColormapName(graph.color_map),
                                       ImVec2(225, 0),
                                       graph.color_map)) {
                graph.color_map =
                  (graph.color_map + 1) % ImPlot::GetColormapCount();
            }

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            float time_step = graph.time_step;
            if (ImGui::DragFloat("time-step",
                                 &time_step,
                                 0.01f,
                                 graph.time_step.lower,
                                 graph.time_step.upper))
                graph.time_step.set(time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (show_select_model_box("Select model",
                                      "Choose model to observe",
                                      app,
                                      ed,
                                      tn,
                                      graph)) {
                if (auto* mdl = ed.pj.sim.models.try_to_get(graph.mdl_id)) {
                    if (mdl->type == dynamics_type::hsm_wrapper) {
                        const auto& hw = get_dyn<hsm_wrapper>(*mdl);
                        if (auto* hsm = ed.pj.sim.hsms.try_to_get(hw.id)) {
                            graph.scale_min = 0.f;
                            graph.scale_max =
                              static_cast<float>(hsm->compute_max_state_used());
                        } else {
                            graph.scale_min = 0.f;
                            graph.scale_max = 255.f;
                        }
                    }

                    if (ed.graph_eds.can_alloc()) {
                        auto& g_ed = ed.graph_eds.alloc();
                        ed.visualisation_eds.push_back(
                          project_editor::visualisation_editor{
                            .graph_ed_id = ed.graph_eds.get_id(g_ed),
                            .tn_id       = ed.pj.tree_nodes.get_id(tn),
                            .graph_obs_id =
                              ed.pj.graph_observers.get_id(graph) });

                        g_ed.update(app, cgraph.g);
                    } else {
                        app.jn.push(
                          log_level::error, [](auto& title, auto& msg) {
                              title = "Project editor";
                              msg   = "Too many graph editor opened. Close "
                                      "some before to open a new one.";
                          });
                    }
                }
            }

            if (auto* mdl = ed.pj.sim.models.try_to_get(graph.mdl_id); mdl) {
                ImGui::SameLine();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
            }

            ImGui::TableNextColumn();

            if (ImGui::Button("del"))
                to_del = id;

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (can_alloc and ImGui::Button("+##graph")) {
        auto&      files    = ed.pj.file_obs.files;
        auto&      graph    = ed.pj.alloc_graph_observer();
        const auto graph_id = ed.pj.graph_observers.get_id(graph);

        graph.parent_id = ed.pj.tree_nodes.get_id(tn);
        graph.compo_id  = undefined<component_id>();
        graph.tn_id     = undefined<tree_node_id>();
        graph.mdl_id    = undefined<model_id>();
        tn.graph_observer_ids.emplace_back(graph_id);

        if (not files.can_alloc(1) or not files.grow<3, 2>()) {
            app.jn.push(log_level::error, [](auto& t, auto& m) {
                t = "Grid observer creation failed";
                m = "Not enough memory to create a grid observer.";
            });

            return false;
        }

        const auto id  = files.alloc_id();
        const auto idx = get_index(id);

        files.template get<file_observers::id_type>(id).graph = graph_id;
        files.template get<file_observers::type>(id) =
          file_observers::type::graph;
        files.template get<bool>(id) = false;

        is_modified = true;

        ++is_modified;
    }

    if (to_del.has_value()) {
        is_modified = true;
        ed.pj.graph_observers.free(*to_del);

        for_each_cond(ed.visualisation_eds, [&](const auto v) noexcept {
            if (v.graph_obs_id == *to_del) {
                ed.graph_eds.free(v.graph_ed_id);
                return true;
            }
            return false;
        });
    }

    return is_modified;
}

} // namespace irt
