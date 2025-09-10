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
                          tree_node&      tn,
                          component& /*compo*/,
                          graph_component& /*graph*/) noexcept
{
    auto to_del      = std::optional<graph_observer_id>();
    bool is_modified = false;

    if (ImGui::BeginTable("Graph observers", 6)) {
        for (const auto id : tn.graph_observer_ids) {
            if (auto* graph = ed.pj.graph_observers.try_to_get(id)) {
                ImGui::PushID(graph);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(-1.0f);
                if (ImGui::InputFilteredString("name", graph->name))
                    is_modified = true;
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                ImGui::DragFloatRange2(
                  "##scale", &graph->scale_min, &graph->scale_max, 0.01f);
                ImGui::PopItemWidth();
                ImGui::TableNextColumn();
                if (ImPlot::ColormapButton(
                      ImPlot::GetColormapName(graph->color_map),
                      ImVec2(225, 0),
                      graph->color_map)) {
                    graph->color_map =
                      (graph->color_map + 1) % ImPlot::GetColormapCount();
                }

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                float time_step = graph->time_step;
                if (ImGui::DragFloat("time-step",
                                     &time_step,
                                     0.01f,
                                     graph->time_step.lower,
                                     graph->time_step.upper))
                    graph->time_step.set(time_step);
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();
                if (show_select_model_box("Select model",
                                          "Choose model to observe",
                                          app,
                                          ed,
                                          tn,
                                          *graph)) {
                    if (auto* mdl =
                          ed.pj.sim.models.try_to_get(graph->mdl_id)) {
                        if (mdl->type == dynamics_type::hsm_wrapper) {
                            const auto& hw = get_dyn<hsm_wrapper>(*mdl);
                            if (auto* hsm = ed.pj.sim.hsms.try_to_get(hw.id)) {
                                graph->scale_min = 0.f;
                                graph->scale_max = static_cast<float>(
                                  hsm->compute_max_state_used());
                            } else {
                                graph->scale_min = 0.f;
                                graph->scale_max = 255.f;
                            }
                        }
                    }
                }

                if (auto* mdl = ed.pj.sim.models.try_to_get(graph->mdl_id);
                    mdl) {
                    ImGui::SameLine();
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl->type)]);
                }

                ImGui::TableNextColumn();

                if (ImGui::Button("del"))
                    to_del = id;

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    if (ed.pj.graph_observers.can_alloc() && ImGui::Button("+##graph")) {
        auto&      graph    = ed.pj.alloc_graph_observer();
        const auto graph_id = ed.pj.graph_observers.get_id(graph);

        graph.parent_id = ed.pj.tree_nodes.get_id(tn);
        graph.compo_id  = undefined<component_id>();
        graph.tn_id     = undefined<tree_node_id>();
        graph.mdl_id    = undefined<model_id>();
        tn.graph_observer_ids.emplace_back(graph_id);

        if (not ed.pj.file_obs.ids.can_alloc(1))
            ed.pj.file_obs.grow();

        if (ed.pj.file_obs.ids.can_alloc(1)) {
            const auto file_obs_id = ed.pj.file_obs.ids.alloc();
            const auto idx         = get_index(file_obs_id);

            ed.pj.file_obs.subids[idx].graph = graph_id;
            ed.pj.file_obs.types[idx]        = file_observers::type::graph;
            ed.pj.file_obs.enables[idx]      = false;
        }

        is_modified = true;
    }

    if (to_del.has_value()) {
        is_modified = true;
        ed.pj.graph_observers.free(*to_del);
    }

    return is_modified;
}

struct graph_simulation_editor::impl {

    static void center_camera(graph_simulation_editor& ed,
                              ImVec2                   top_left,
                              ImVec2                   bottom_right,
                              ImVec2                   canvas_sz) noexcept
    {
        ImVec2 distance(bottom_right[0] - top_left[0],
                        bottom_right[1] - top_left[1]);
        ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                      (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

        ed.scrolling.x = ((-center.x * ed.zoom.x) + (canvas_sz.x / 2.f));
        ed.scrolling.y = ((-center.y * ed.zoom.y) + (canvas_sz.y / 2.f));
    }

    static void auto_fit_camera(graph_simulation_editor& ed,
                                ImVec2                   top_left,
                                ImVec2                   bottom_right,
                                ImVec2                   canvas_sz) noexcept
    {
        ImVec2 distance(bottom_right[0] - top_left[0],
                        bottom_right[1] - top_left[1]);
        ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                      (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

        ed.zoom.x      = canvas_sz.x / distance.x;
        ed.zoom.y      = canvas_sz.y / distance.y;
        ed.scrolling.x = ((-center.x * ed.zoom.x) + (canvas_sz.x / 2.f));
        ed.scrolling.y = ((-center.y * ed.zoom.y) + (canvas_sz.y / 2.f));
    }

    bool static display_graph_simulation(application& app,
                                         project_editor& /*ed*/,
                                         tree_node& /*tn*/,
                                         component& /*compo*/,
                                         graph_component& graph) noexcept
    {
        float zoom_array[2] = { app.graph_sim.zoom.x, app.graph_sim.zoom.y };
        if (ImGui::InputFloat2("zoom x,y", zoom_array)) {
            app.graph_sim.zoom.x = ImClamp(zoom_array[0], 0.1f, 1000.f);
            app.graph_sim.zoom.y = ImClamp(zoom_array[1], 0.1f, 1000.f);
        }

        float distance_array[2] = { app.graph_sim.distance.x,
                                    app.graph_sim.distance.y };
        if (ImGui::InputFloat2("force x,y", distance_array)) {
            app.graph_sim.distance.x = ImClamp(distance_array[0], 0.1f, 100.f);
            app.graph_sim.distance.y = ImClamp(distance_array[0], 0.1f, 100.f);
        }

        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

        if (canvas_sz.x < 50.0f)
            canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f)
            canvas_sz.y = 50.0f;

        ImVec2 canvas_p1 =
          ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        const ImGuiIO& io        = ImGui::GetIO();
        ImDrawList*    draw_list = ImGui::GetWindowDrawList();

        if (app.graph_sim.actions[action::camera_center])
            center_camera(
              app.graph_sim,
              ImVec2(graph.top_left_limit[0], graph.top_left_limit[1]),
              ImVec2(graph.bottom_right_limit[0], graph.bottom_right_limit[1]),
              canvas_sz);

        if (app.graph_sim.actions[action::camera_auto_fit])
            auto_fit_camera(
              app.graph_sim,
              ImVec2(graph.top_left_limit[0], graph.top_left_limit[1]),
              ImVec2(graph.bottom_right_limit[0], graph.bottom_right_limit[1]),
              canvas_sz);

        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        ImGui::InvisibleButton("Canvas",
                               canvas_sz,
                               ImGuiButtonFlags_MouseButtonLeft |
                                 ImGuiButtonFlags_MouseButtonMiddle |
                                 ImGuiButtonFlags_MouseButtonRight);

        const bool is_hovered = ImGui::IsItemHovered();
        const bool is_active  = ImGui::IsItemActive();

        const ImVec2 origin(canvas_p0.x + app.graph_sim.scrolling.x,
                            canvas_p0.y + app.graph_sim.scrolling.y);
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                         io.MousePos.y - origin.y);

        const float mouse_threshold_for_pan = -1.f;
        if (is_active) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,
                                       mouse_threshold_for_pan)) {
                app.graph_sim.scrolling.x += io.MouseDelta.x;
                app.graph_sim.scrolling.y += io.MouseDelta.y;
            }
        }

        if (is_hovered and io.MouseWheel != 0.f) {
            app.graph_sim.zoom.x =
              app.graph_sim.zoom.x +
              (io.MouseWheel * app.graph_sim.zoom.x * 0.1f);
            app.graph_sim.zoom.y =
              app.graph_sim.zoom.y +
              (io.MouseWheel * app.graph_sim.zoom.y * 0.1f);
            app.graph_sim.zoom.x = ImClamp(app.graph_sim.zoom.x, 0.1f, 1000.f);
            app.graph_sim.zoom.y = ImClamp(app.graph_sim.zoom.y, 0.1f, 1000.f);
        }

        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (drag_delta.x == 0.0f and drag_delta.y == 0.0f and
            (not app.graph_sim.selected_nodes.empty())) {
            ImGui::OpenPopupOnItemClick("Canvas-Context",
                                        ImGuiPopupFlags_MouseButtonRight);
        }

        if (is_hovered) {
            if (not app.graph_sim.run_selection and
                ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                app.graph_sim.run_selection   = true;
                app.graph_sim.start_selection = io.MousePos;
            }

            if (app.graph_sim.run_selection and
                ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                app.graph_sim.run_selection = false;
                app.graph_sim.end_selection = io.MousePos;

                if (app.graph_sim.start_selection ==
                    app.graph_sim.end_selection) {
                    app.graph_sim.selected_nodes.clear();
                } else {
                    ImVec2 bmin{
                        std::min(app.graph_sim.start_selection.x,
                                 app.graph_sim.end_selection.x),
                        std::min(app.graph_sim.start_selection.y,
                                 app.graph_sim.end_selection.y),
                    };

                    ImVec2 bmax{
                        std::max(app.graph_sim.start_selection.x,
                                 app.graph_sim.end_selection.x),
                        std::max(app.graph_sim.start_selection.y,
                                 app.graph_sim.end_selection.y),
                    };

                    app.graph_sim.selected_nodes.clear();

                    for (const auto id : graph.g.nodes) {
                        const auto i = get_index(id);

                        ImVec2 p_min(origin.x + (graph.g.node_positions[i][0] *
                                                 app.graph_sim.zoom.x),
                                     origin.y + (graph.g.node_positions[i][1] *
                                                 app.graph_sim.zoom.y));

                        ImVec2 p_max(origin.x + ((graph.g.node_positions[i][0] +
                                                  graph.g.node_areas[i]) *
                                                 app.graph_sim.zoom.x),
                                     origin.y + ((graph.g.node_positions[i][1] +
                                                  graph.g.node_areas[i]) *
                                                 app.graph_sim.zoom.y));

                        if (p_min.x >= bmin.x and p_max.x < bmax.x and
                            p_min.y >= bmin.y and p_max.y < bmax.y) {
                            app.graph_sim.selected_nodes.emplace_back(id);
                        }
                    }

                    for (const auto id : graph.g.edges) {
                        const auto us =
                          graph.g.edges_nodes[get_index(id)][0].first;
                        const auto vs =
                          graph.g.edges_nodes[get_index(id)][1].first;

                        if (graph.g.nodes.exists(us) and
                            graph.g.nodes.exists(vs)) {
                            ImVec2 p1(
                              origin.x +
                                ((graph.g.node_positions[get_index(us)][0] +
                                  graph.g.node_areas[get_index(us)] / 2.f) *
                                 app.graph_sim.zoom.x),
                              origin.y +
                                ((graph.g.node_positions[get_index(us)][1] +
                                  graph.g.node_areas[get_index(vs)] / 2.f) *
                                 app.graph_sim.zoom.y));

                            ImVec2 p2(
                              origin.x +
                                ((graph.g.node_positions[get_index(vs)][0] +
                                  graph.g.node_areas[get_index(us)] / 2.f) *
                                 app.graph_sim.zoom.x),
                              origin.y +
                                ((graph.g.node_positions[get_index(vs)][1] +
                                  graph.g.node_areas[get_index(vs)] / 2.f) *
                                 app.graph_sim.zoom.y));
                        }
                    }
                }
            }
        }

        draw_list->PushClipRect(canvas_p0, canvas_p1, true);
        const float GRID_STEP = 64.0f;

        for (float x = fmodf(app.graph_sim.scrolling.x, GRID_STEP);
             x < canvas_sz.x;
             x += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                               ImVec2(canvas_p0.x + x, canvas_p1.y),
                               IM_COL32(200, 200, 200, 40));

        for (float y = fmodf(app.graph_sim.scrolling.y, GRID_STEP);
             y < canvas_sz.y;
             y += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                               ImVec2(canvas_p1.x, canvas_p0.y + y),
                               IM_COL32(200, 200, 200, 40));

        for (const auto id : graph.g.nodes) {
            const auto i = get_index(id);

            const ImVec2 p_min(
              origin.x + (graph.g.node_positions[i][0] * app.graph_sim.zoom.x),
              origin.y + (graph.g.node_positions[i][1] * app.graph_sim.zoom.y));

            const ImVec2 p_max(
              origin.x +
                ((graph.g.node_positions[i][0] + graph.g.node_areas[i]) *
                 app.graph_sim.zoom.x),
              origin.y +
                ((graph.g.node_positions[i][1] + graph.g.node_areas[i]) *
                 app.graph_sim.zoom.y));

            draw_list->AddRectFilled(
              p_min, p_max, IM_COL32(255, 255, 255, 255));
        }

        for (const auto id : app.graph_sim.selected_nodes) {
            const auto i = get_index(id);

            ImVec2 p_min(
              origin.x + (graph.g.node_positions[i][0] * app.graph_sim.zoom.x),
              origin.y + (graph.g.node_positions[i][1] * app.graph_sim.zoom.y));

            ImVec2 p_max(
              origin.x +
                ((graph.g.node_positions[i][0] + graph.g.node_areas[i]) *
                 app.graph_sim.zoom.x),
              origin.y +
                ((graph.g.node_positions[i][1] + graph.g.node_areas[i]) *
                 app.graph_sim.zoom.y));

            draw_list->AddRect(
              p_min, p_max, IM_COL32(255, 255, 255, 255), 0.f, 0, 4.f);
        }

        for (const auto id : graph.g.edges) {
            const auto i   = get_index(id);
            const auto u_c = graph.g.edges_nodes[i][0].first;
            const auto v_c = graph.g.edges_nodes[i][1].first;

            if (not(graph.g.nodes.exists(u_c) and graph.g.nodes.exists(v_c)))
                continue;

            const auto p_src = get_index(u_c);
            const auto p_dst = get_index(v_c);

            const auto u_width  = graph.g.node_areas[p_src] / 2.f;
            const auto u_height = graph.g.node_areas[p_src] / 2.f;

            const auto v_width  = graph.g.node_areas[p_dst] / 2.f;
            const auto v_height = graph.g.node_areas[p_dst] / 2.f;

            ImVec2 src(
              origin.x + ((graph.g.node_positions[p_src][0] + u_width) *
                          app.graph_sim.zoom.x),
              origin.y + ((graph.g.node_positions[p_src][1] + u_height) *
                          app.graph_sim.zoom.y));

            ImVec2 dst(
              origin.x + ((graph.g.node_positions[p_dst][0] + v_width) *
                          app.graph_sim.zoom.x),
              origin.y + ((graph.g.node_positions[p_dst][1] + v_height) *
                          app.graph_sim.zoom.y));

            draw_list->AddLine(src, dst, IM_COL32(255, 255, 0, 255), 1.f);
        }

        if (app.graph_sim.run_selection) {
            app.graph_sim.end_selection = io.MousePos;

            if (app.graph_sim.start_selection == app.graph_sim.end_selection) {
                app.graph_sim.selected_nodes.clear();
            } else {
                ImVec2 bmin{
                    std::min(app.graph_sim.start_selection.x, io.MousePos.x),
                    std::min(app.graph_sim.start_selection.y, io.MousePos.y),
                };

                ImVec2 bmax{
                    std::max(app.graph_sim.start_selection.x, io.MousePos.x),
                    std::max(app.graph_sim.start_selection.y, io.MousePos.y),
                };

                draw_list->AddRectFilled(bmin, bmax, IM_COL32(200, 0, 0, 127));
            }
        }

        draw_list->PopClipRect();

        return true;
    }
};

void graph_simulation_editor::reset() noexcept
{
    distance        = { 15.f, 15.f };
    scrolling       = { 0.f, 0.f };
    zoom            = { 1.f, 1.f };
    start_selection = { 0.f, 0.f };
    end_selection   = { 0.f, 0.f };

    selected_nodes.clear();
    run_selection = false;
}

bool graph_simulation_editor::display(application&     app,
                                      project_editor&  ed,
                                      tree_node&       tn,
                                      component&       compo,
                                      graph_component& graph) noexcept
{
    const auto graph_id = app.mod.graph_components.get_id(graph);
    if (graph_id != current_id) {
        reset();
        current_id = graph_id;
    }

    return graph_simulation_editor::impl::display_graph_simulation(
      app, ed, tn, compo, graph);
}

} // namespace irt
