// Copyright (c) 2025 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static void show_graph_observer(graph_component& compo,
                                ImVec2&          zoom,
                                ImVec2&          scrolling,
                                ImVec2&          distance,
                                graph_observer&  obs) noexcept
{
    ImGui::PushID(reinterpret_cast<void*>(&obs));
    ImPlot::PushColormap(obs.color_map);

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    float zoom_array[2] = { zoom.x, zoom.y };
    if (ImGui::InputFloat2("zoom x,y", zoom_array)) {
        zoom.x = ImClamp(zoom_array[0], 0.1f, 1000.f);
        zoom.y = ImClamp(zoom_array[1], 0.1f, 1000.f);
    }

    float distance_array[2] = { distance.x, distance.y };
    if (ImGui::InputFloat2("force x,y", distance_array)) {
        distance.x = ImClamp(distance_array[0], 0.1f, 100.f);
        distance.y = ImClamp(distance_array[0], 0.1f, 100.f);
    }

    if (ImGui::Button("center")) {
        ImVec2 dist(compo.bottom_right_limit[0] - compo.top_left_limit[0],
                    compo.bottom_right_limit[1] - compo.top_left_limit[1]);
        ImVec2 center(
          (compo.bottom_right_limit[0] - compo.top_left_limit[0]) / 2.0f +
            compo.top_left_limit[0],
          (compo.bottom_right_limit[1] - compo.top_left_limit[1]) / 2.0f +
            compo.top_left_limit[1]);

        scrolling.x = ((-center.x * zoom.x) + (canvas_sz.x / 2.f));
        scrolling.y = ((-center.y * zoom.y) + (canvas_sz.y / 2.f));
    }

    if (ImGui::Button("auto-fit")) {
        ImVec2 d(compo.bottom_right_limit[0] - compo.top_left_limit[0],
                 compo.bottom_right_limit[1] - compo.top_left_limit[1]);
        ImVec2 c(
          (compo.bottom_right_limit[0] - compo.top_left_limit[0]) / 2.0f +
            compo.top_left_limit[0],
          (compo.bottom_right_limit[1] - compo.top_left_limit[1]) / 2.0f +
            compo.top_left_limit[1]);

        zoom.x      = canvas_sz.x / d.x;
        zoom.y      = canvas_sz.y / d.y;
        scrolling.x = ((-c.x * zoom.x) + (canvas_sz.x / 2.f));
        scrolling.y = ((-c.y * zoom.y) + (canvas_sz.y / 2.f));
    }

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight);

    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active  = ImGui::IsItemActive();

    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                   mouse_threshold_for_pan)) {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
        }
    }

    if (is_hovered and io.MouseWheel != 0.f) {
        zoom.x = zoom.x + (io.MouseWheel * zoom.x * 0.1f);
        zoom.y = zoom.y + (io.MouseWheel * zoom.y * 0.1f);
        zoom.x = ImClamp(zoom.x, 0.1f, 1000.f);
        zoom.y = ImClamp(zoom.y, 0.1f, 1000.f);
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    const float GRID_STEP = 64.0f;

    for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           IM_COL32(200, 200, 200, 40));

    for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           IM_COL32(200, 200, 200, 40));

    obs.values.try_read_only([&](const auto& v) noexcept {
        if (v.empty())
            return;

        for (const auto id : compo.g.nodes) {
            const auto i = get_index(id);

            const ImVec2 p_min(
              origin.x + (compo.g.node_positions[i][0] * zoom.x),
              origin.y + (compo.g.node_positions[i][1] * zoom.y));

            const ImVec2 p_max(
              origin.x +
                ((compo.g.node_positions[i][0] + compo.g.node_areas[i]) *
                 zoom.x),
              origin.y +
                ((compo.g.node_positions[i][1] + compo.g.node_areas[i]) *
                 zoom.y));

            debug::ensure(i < v.size());

            const auto m = static_cast<double>(obs.scale_min);
            const auto M = static_cast<double>(obs.scale_max);
            const auto d = std::abs(m) + std::abs(M);
            const auto o = v[i] + m;
            const auto t = o / d;
            debug::ensure(0.0 <= t and t <= 1.0);

            draw_list->AddRectFilled(
              p_min,
              p_max,
              ImPlot::SampleColormapU32(static_cast<float>(t), IMPLOT_AUTO));
        }
    });

    for (const auto id : compo.g.edges) {
        const auto i   = get_index(id);
        const auto u_c = compo.g.edges_nodes[i][0];
        const auto v_c = compo.g.edges_nodes[i][1];

        if (not(compo.g.nodes.exists(u_c.first) and
                compo.g.nodes.exists(v_c.first)))
            continue;

        const auto p_src = get_index(u_c.first);
        const auto p_dst = get_index(v_c.first);

        const auto u_width  = compo.g.node_areas[p_src] / 2.f;
        const auto u_height = compo.g.node_areas[p_src] / 2.f;

        const auto v_width  = compo.g.node_areas[p_dst] / 2.f;
        const auto v_height = compo.g.node_areas[p_dst] / 2.f;

        ImVec2 src(
          origin.x + ((compo.g.node_positions[p_src][0] + u_width) * zoom.x),
          origin.y + ((compo.g.node_positions[p_src][1] + u_height) * zoom.y));

        ImVec2 dst(
          origin.x + ((compo.g.node_positions[p_dst][0] + v_width) * zoom.x),
          origin.y + ((compo.g.node_positions[p_dst][1] + v_height) * zoom.y));

        draw_list->AddLine(src, dst, IM_COL32(255, 255, 0, 255), 1.f);
    }

    draw_list->PopClipRect();

    ImPlot::PopColormap();
    ImGui::PopID();
}

void graph_observation_widget::show(project_editor& ed,
                                    graph_observer& graph,
                                    const ImVec2& /*size*/) noexcept
{

    ImGui::PushID(&graph);
    if (ImGui::BeginChild("graph")) {
        if (auto* tn = ed.pj.tree_nodes.try_to_get(graph.parent_id)) {
            auto& app = container_of(this, &application::graph_obs);

            if (auto* c = app.mod.components.try_to_get<component>(tn->id)) {
                if (c->type == component_type::graph) {
                    if (auto* g =
                          app.mod.graph_components.try_to_get(c->id.graph_id)) {
                        show_graph_observer(
                          *g, zoom, scrolling, distance, graph);
                    }
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
}

constexpr static bool is_line_intersects_box(ImVec2 p1,
                                             ImVec2 p2,
                                             ImVec2 bmin,
                                             ImVec2 bmax) noexcept
{
    const int corner_1 = ((p2.y - p1.y) * bmin.x + (p1.x - p2.x) * bmin.y +
                          (p2.x * p1.y - p1.x * p2.y)) >= 0
                           ? 1
                           : 0;
    const int corner_2 = ((p2.y - p1.y) * bmin.x + (p1.x - p2.x) * bmax.y +
                          (p2.x * p1.y - p1.x * p2.y)) >= 0
                           ? 1
                           : 0;
    const int corner_3 = ((p2.y - p1.y) * bmax.x + (p1.x - p2.x) * bmax.y +
                          (p2.x * p1.y - p1.x * p2.y)) >= 0
                           ? 1
                           : 0;
    const int corner_4 = ((p2.y - p1.y) * bmax.x + (p1.x - p2.x) * bmin.y +
                          (p2.x * p1.y - p1.x * p2.y)) >= 0
                           ? 1
                           : 0;

    const auto sum    = corner_1 + corner_2 + corner_3 + corner_4;
    const auto a_miss = sum == 0 or sum == 4;
    const bool b_miss =
      (p1.x > bmax.x and p2.x > bmax.x) or (p1.x < bmin.x and p2.x < bmin.x) or
      (p1.y > bmax.y and p2.y > bmax.y) or (p1.y < bmin.y and p2.y < bmin.y);

    return not(a_miss or b_miss);
};

/* -----------------------------------------------------------------------------
 *
 * graph-editor implementation file
 *
 * ---------------------------------------------------------------------------*/

graph_editor::graph_editor() noexcept
  : selected_nodes{ graph_editor::selection_buffer_size, reserve_tag }
  , selected_edges{ graph_editor::selection_buffer_size, reserve_tag }
{}

void graph_editor::auto_fit_camera() noexcept
{
    nodes_locker.read_only([&](auto& d) noexcept {
        const std::array dist{ d.bottom_right[0] - d.top_left[0],
                               d.bottom_right[1] - d.top_left[1] };

        zoom        = std::min(canvas_sz.x / dist[0], canvas_sz.y / dist[1]);
        scrolling.x = ((-d.center[0] * zoom) + (canvas_sz.x / 2.f));
        scrolling.y = ((-d.center[1] * zoom) + (canvas_sz.y / 2.f));
    });
}

void graph_editor::center_camera() noexcept
{
    nodes_locker.read_only([&](auto& d) noexcept {
        scrolling.x = ((-d.center[0] * zoom) + (canvas_sz.x / 2.f));
        scrolling.y = ((-d.center[1] * zoom) + (canvas_sz.y / 2.f));
    });
}

void graph_editor::reset_camera() noexcept
{
    nodes_locker.read_only([&](auto& d) noexcept {
        scrolling.x = d.center[0];
        scrolling.y = d.center[1];
        zoom        = 1.f;
    });
}

void graph_editor::initialize_canvas(ImVec2 top_left,
                                     ImVec2 bottom_right,
                                     ImU32  color) noexcept
{
    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(top_left, bottom_right, color);

    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

    draw_list->PushClipRect(top_left, bottom_right, true);

    const bool  is_hovered              = ImGui::IsItemHovered();
    const bool  is_active               = ImGui::IsItemActive();
    const float mouse_threshold_for_pan = -1.f;

    if (is_active) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,
                                   mouse_threshold_for_pan)) {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
        }
    }

    if (is_hovered and io.MouseWheel != 0.f)
        zoom = zoom + (io.MouseWheel * zoom * 0.1f);
}

void graph_editor::draw_graph(const graph&          g,
                              ImVec2                top_left,
                              ImU32                 color,
                              const graph_observer& obs) noexcept
{
    ImPlot::PushColormap(obs.color_map);

    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin    = top_left + scrolling;

    nodes_locker.try_read_only([&](const auto& d) noexcept {
        if (d.nodes.empty())
            return;

        obs.values.try_read_only([&](const auto& v) noexcept {
            if (d.nodes.size() != v.size())
                return;

            for (const auto id : g.nodes) {
                const auto i      = get_index(id);
                const auto area   = g.node_areas[i];
                const auto [x, y] = d.nodes[i];

                const ImVec2 p_min(origin.x + (x * zoom),
                                   origin.y + (y * zoom));

                const ImVec2 p_max(origin.x + ((x + area) * zoom),
                                   origin.y + ((y + area) * zoom));

                debug::ensure(i < v.size());

                const auto m = static_cast<double>(obs.scale_min);
                const auto M = static_cast<double>(obs.scale_max);
                const auto d = std::abs(m) + std::abs(M);
                const auto o = v[i] + m;
                const auto t = o / d;
                debug::ensure(0.0 <= t and t <= 1.0);

                draw_list->AddRectFilled(p_min,
                                         p_max,
                                         ImPlot::SampleColormapU32(
                                           static_cast<float>(t), IMPLOT_AUTO));

                draw_list->AddRectFilled(p_min, p_max, color);
            }
        });

        for (const auto id : g.edges) {
            const auto& [from, to] = g.edges_nodes[id];
            if (g.nodes.exists(from.first) and g.nodes.exists(to.first)) {
                const auto area_from        = g.node_areas[from.first] / 2.f;
                const auto area_to          = g.node_areas[to.first] / 2.f;
                const auto [from_x, from_y] = d.nodes[from.first];
                const auto [to_x, to_y]     = d.nodes[to.first];

                ImVec2 src(origin.x + ((from_x + area_from) * zoom),
                           origin.y + ((from_y + area_from) * zoom));

                ImVec2 dst(origin.x + ((to_x + area_to) * zoom),
                           origin.y + ((to_y + area_to) * zoom));

                draw_list->AddLine(src, dst, color, 1.f);
            }
        }
    });

    ImPlot::PopColormap();
}

void graph_editor::draw_graph(const graph& g,
                              ImVec2       top_left,
                              ImU32        color,
                              application& app) noexcept
{
    nodes_locker.try_read_only([&](const auto& d) noexcept {
        ImDrawList*  draw_list = ImGui::GetWindowDrawList();
        const ImVec2 origin    = top_left + scrolling;

        for (const auto id : g.nodes) {
            const auto i      = get_index(id);
            const auto area   = g.node_areas[i];
            const auto [x, y] = d.nodes[i];

            const ImVec2 p_min(origin.x + (x * zoom), origin.y + (y * zoom));

            const ImVec2 p_max(origin.x + ((x + area) * zoom),
                               origin.y + ((y + area) * zoom));

            draw_list->AddRectFilled(
              p_min, p_max, get_component_u32color(app, g.node_components[id]));
        }

        for (const auto id : g.edges) {
            const auto& [from, to] = g.edges_nodes[id];
            if (g.nodes.exists(from.first) and g.nodes.exists(to.first)) {
                const auto area_from        = g.node_areas[from.first] / 2.f;
                const auto area_to          = g.node_areas[to.first] / 2.f;
                const auto [from_x, from_y] = d.nodes[from.first];
                const auto [to_x, to_y]     = d.nodes[to.first];

                ImVec2 src(origin.x + ((from_x + area_from) * zoom),
                           origin.y + ((from_y + area_from) * zoom));

                ImVec2 dst(origin.x + ((to_x + area_to) * zoom),
                           origin.y + ((to_y + area_to) * zoom));

                draw_list->AddLine(src, dst, color, 1.f);
            }
        }
    });
}

void graph_editor::draw_grid(ImVec2 top_left,
                             ImVec2 bottom_right,
                             ImU32  color) noexcept
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (float x = fmodf(scrolling.x, grid_step); x < canvas_sz.x;
         x += grid_step)
        draw_list->AddLine(ImVec2(top_left.x + x, top_left.y),
                           ImVec2(top_left.x + x, bottom_right.y),
                           color);

    for (float y = fmodf(scrolling.y, grid_step); y < canvas_sz.y;
         y += grid_step)
        draw_list->AddLine(ImVec2(top_left.x, top_left.y + y),
                           ImVec2(bottom_right.x, top_left.y + y),
                           color);
}

void graph_editor::draw_popup(const application& app,
                              graph&             g,
                              const ImVec2       top_left) noexcept
{
    const auto drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);

    if (drag_delta.x == 0.0f and drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("Graph-Editor#Popup",
                                    ImGuiPopupFlags_MouseButtonRight);

    if (ImGui::BeginPopup("Graph-Editor#Popup")) {
        const auto origin = top_left + scrolling;
        const auto click  = ImGui::GetMousePosOnOpeningCurrentPopup();

        if (ImGui::BeginMenu("Display")) {
            if (ImGui::MenuItem("Center camera"))
                center_camera();
            if (ImGui::MenuItem("Automatic zoom and center"))
                auto_fit_camera();
            if (ImGui::MenuItem("Reset camera"))
                reset_camera();
            ImGui::Separator();

            {
                auto show_grid = flags[option::show_grid];
                if (ImGui::MenuItem("Show grid", nullptr, &show_grid))
                    flags.set(option::show_grid, show_grid);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("New node")) {
                if (auto id = g.alloc_node(); id.has_value()) {
                    const auto idx = get_index(*id);

                    g.node_positions[idx] = {
                        (click.x - origin.x) / zoom,
                        (click.y - origin.y) / zoom,
                    };

                    selected_nodes.emplace_back(*id);
                } else {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto& id) {
                          t = "Failed to add new node.";
                          format(m,
                                 "Error: category {} value {}",
                                 ordinal(id.error().cat()),
                                 id.error().value());
                      },
                      id);
                }
            }

            if (not selected_nodes.empty() and ImGui::MenuItem("Connect")) {
                const auto e = selected_nodes.ssize();
                for (int i = 0; i < e; ++i) {
                    for (int j = i + 1; j < e; ++j) {
                        g.alloc_edge(selected_nodes[i], selected_nodes[j]);
                    }
                }
                selected_nodes.clear();
            }

            if (not selected_nodes.empty() and
                ImGui::MenuItem("Delete nodes")) {
                for (auto id : selected_nodes) {
                    if (g.nodes.exists(id))
                        g.nodes.free(id);
                }
                selected_nodes.clear();
            }

            if (not selected_edges.empty() and
                ImGui::MenuItem("Delete edges")) {
                for (auto id : selected_edges)
                    if (g.edges.exists(id))
                        g.edges.free(id);
                selected_edges.clear();
            }

            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void graph_editor::draw_selection(const graph& g,
                                  ImVec2       top_left,
                                  ImU32        node_color,
                                  ImU32        edge_color,
                                  ImU32 background_selection_color) noexcept
{
    const auto     origin     = top_left + scrolling;
    const bool     is_hovered = ImGui::IsItemHovered();
    const ImGuiIO& io         = ImGui::GetIO();

    if (is_hovered) {

        if (not run_selection and ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            run_selection   = true;
            start_selection = io.MousePos;
        }

        if (run_selection and ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            run_selection = false;
            end_selection = io.MousePos;

            if (start_selection == end_selection) {
                selected_nodes.clear();
                selected_edges.clear();
            } else {
                ImVec2 bmin{
                    std::min(start_selection.x, end_selection.x),
                    std::min(start_selection.y, end_selection.y),
                };

                ImVec2 bmax{
                    std::max(start_selection.x, end_selection.x),
                    std::max(start_selection.y, end_selection.y),
                };

                selected_edges.clear();
                selected_nodes.clear();

                for (const auto id : g.nodes) {
                    const auto i = get_index(id);

                    ImVec2 p_min(origin.x + (g.node_positions[i][0] * zoom),
                                 origin.y + (g.node_positions[i][1] * zoom));

                    ImVec2 p_max(
                      origin.x +
                        ((g.node_positions[i][0] + g.node_areas[i]) * zoom),
                      origin.y +
                        ((g.node_positions[i][1] + g.node_areas[i]) * zoom));

                    if (p_min.x >= bmin.x and p_max.x < bmax.x and
                        p_min.y >= bmin.y and p_max.y < bmax.y) {
                        selected_nodes.emplace_back(id);
                    }
                }

                for (const auto id : g.edges) {
                    const auto us = g.edges_nodes[id][0].first;
                    const auto vs = g.edges_nodes[id][1].first;

                    if (g.nodes.exists(us) and g.nodes.exists(vs)) {
                        ImVec2 p1(origin.x + ((g.node_positions[us][0] +
                                               g.node_areas[us] / 2.f) *
                                              zoom),
                                  origin.y + ((g.node_positions[us][1] +
                                               g.node_areas[vs] / 2.f) *
                                              zoom));

                        ImVec2 p2(origin.x + ((g.node_positions[vs][0] +
                                               g.node_areas[us] / 2.f) *
                                              zoom),
                                  origin.y + ((g.node_positions[vs][1] +
                                               g.node_areas[vs] / 2.f) *
                                              zoom));

                        if (is_line_intersects_box(p1, p2, bmin, bmax))
                            selected_edges.emplace_back(id);
                    }
                }
            }
        }
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (run_selection) {
        end_selection = io.MousePos;

        if (start_selection == end_selection) {
            selected_nodes.clear();
            selected_edges.clear();
        } else {
            ImVec2 bmin{
                std::min(start_selection.x, io.MousePos.x),
                std::min(start_selection.y, io.MousePos.y),
            };

            ImVec2 bmax{
                std::max(start_selection.x, io.MousePos.x),
                std::max(start_selection.y, io.MousePos.y),
            };

            draw_list->AddRectFilled(bmin, bmax, background_selection_color);
        }
    }

    for (const auto id : selected_nodes) {
        const auto i = get_index(id);

        ImVec2 p_min(origin.x + (g.node_positions[i][0] * zoom),
                     origin.y + (g.node_positions[i][1] * zoom));

        ImVec2 p_max(
          origin.x + ((g.node_positions[i][0] + g.node_areas[i]) * zoom),
          origin.y + ((g.node_positions[i][1] + g.node_areas[i]) * zoom));

        draw_list->AddRect(p_min, p_max, node_color, 0.f, 0, 4.f);
    }

    for (const auto id : selected_edges) {
        const auto idx = get_index(id);
        const auto u_c = g.edges_nodes[idx][0].first;
        const auto v_c = g.edges_nodes[idx][1].first;

        if (not(g.nodes.exists(u_c) and g.nodes.exists(v_c)))
            continue;

        const auto p_src = get_index(u_c);
        const auto p_dst = get_index(v_c);

        ImVec2 src(
          origin.x +
            ((g.node_positions[p_src][0] + g.node_areas[p_src] / 2.f) * zoom),
          origin.y +
            ((g.node_positions[p_src][1] + g.node_areas[p_src] / 2.f) * zoom));

        ImVec2 dst(
          origin.x +
            ((g.node_positions[p_dst][0] + g.node_areas[p_dst] / 2.f) * zoom),
          origin.y +
            ((g.node_positions[p_dst][1] + g.node_areas[p_dst] / 2.f) * zoom));

        draw_list->AddLine(src, dst, edge_color, 1.0f);
    }
}

auto graph_editor::show(application&    app,
                        project_editor& ed,
                        tree_node&      tn,
                        graph_observer& obs) noexcept
  -> graph_editor::show_result_type
{
    if (not dock_init) {
        ImGui::SetNextWindowDockID(app.get_main_dock_id());
        format(name, "visu-{}", ed.name.sv());
        dock_init = true;
    }

    bool is_open = true;
    if (not ImGui::Begin(name.c_str(), &is_open)) {
        ImGui::End();
        return is_open ? show_result_type::none
                       : show_result_type::request_to_close;
    }

    debug::ensure(app.mod.components.exists(tn.id));
    debug::ensure(app.mod.components.get<component>(tn.id).type ==
                  component_type::graph);

    if (not app.mod.components.exists(tn.id))
        return show_result_type::request_to_close;

    auto& compo = app.mod.components.get<component>(tn.id);
    if (compo.type != component_type::graph)
        return show_result_type::request_to_close;

    auto&      graph     = app.mod.graph_components.get(compo.id.graph_id).g;
    const auto canvas_p0 = ImGui::GetCursorScreenPos();
    canvas_sz            = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    const auto canvas_p1 = canvas_p0 + canvas_sz;

    initialize_canvas(canvas_p0,
                      canvas_p1,
                      to_ImU32(app.config.colors[style_color::outer_border]));

    if (flags[option::show_grid])
        draw_grid(canvas_p0,
                  canvas_p1,
                  to_ImU32(app.config.colors[style_color::inner_border]));

    draw_graph(
      graph, canvas_p0, to_ImU32(app.config.colors[style_color::edge]), obs);

    draw_popup(app, graph, canvas_p0);

    draw_selection(
      graph,
      canvas_p0,
      to_ImU32(app.config.colors[style_color::node_active]),
      to_ImU32(app.config.colors[style_color::edge_active]),
      to_ImU32(app.config.colors[style_color::background_selection]));

    ImGui::GetWindowDrawList()->PopClipRect();
    ImGui::End();

    return show_result_type::none;
}

auto graph_editor::show(application&     app,
                        component&       c,
                        graph_component& g) noexcept
  -> graph_editor::show_result_type
{
    if (not dock_init) {
        ImGui::SetNextWindowDockID(app.get_main_dock_id());
        format(name, "g-{}", c.name.sv());
        dock_init = true;
    }

    bool is_open = true;
    if (not ImGui::Begin(name.c_str(), &is_open)) {
        ImGui::End();
        return is_open ? show_result_type::none
                       : show_result_type::request_to_close;
    }

    auto       edited    = 0;
    const auto canvas_p0 = ImGui::GetCursorScreenPos();
    canvas_sz            = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    const auto canvas_p1 = canvas_p0 + canvas_sz;

    initialize_canvas(canvas_p0,
                      canvas_p1,
                      to_ImU32(app.config.colors[style_color::outer_border]));

    if (flags[option::show_grid])
        draw_grid(canvas_p0,
                  canvas_p1,
                  to_ImU32(app.config.colors[style_color::inner_border]));

    draw_graph(
      g.g, canvas_p0, to_ImU32(app.config.colors[style_color::edge]), app);

    if (flags[option::allow_graph_edit])
        draw_popup(app, g.g, canvas_p0);

    draw_popup(app, g.g, canvas_p0);

    draw_selection(
      g.g,
      canvas_p0,
      to_ImU32(app.config.colors[style_color::node_active]),
      to_ImU32(app.config.colors[style_color::edge_active]),
      to_ImU32(app.config.colors[style_color::background_selection]));

    ImGui::GetWindowDrawList()->PopClipRect();
    ImGui::End();

    return edited ? show_result_type::edited : show_result_type::none;
}

void graph_editor::update(application& app, const graph& g) noexcept
{
    app.add_gui_task([&]() noexcept {
        nodes_locker.read_write([&](auto& d) noexcept {
            d.nodes.resize(g.nodes.ssize());
            std::array<float, 2> x{ std::numeric_limits<float>::max(),
                                    std::numeric_limits<float>::lowest() };
            std::array<float, 2> y{ std::numeric_limits<float>::max(),
                                    std::numeric_limits<float>::lowest() };
            std::array<float, 2> z{ std::numeric_limits<float>::max(),
                                    std::numeric_limits<float>::lowest() };

            for (const auto id : g.nodes) {
                const auto i = get_index(id);

                x[0] = std::min(x[0], g.node_positions[i][0] - g.node_areas[i]);
                x[1] = std::max(x[1], g.node_positions[i][1] + g.node_areas[i]);
                y[0] = std::min(y[0], g.node_positions[i][0] - g.node_areas[i]);
                y[1] = std::max(y[1], g.node_positions[i][1] + g.node_areas[i]);
                z[0] = std::min(z[0], g.node_positions[i][0] - g.node_areas[i]);
                z[1] = std::max(z[1], g.node_positions[i][1] + g.node_areas[i]);

                d.nodes[i][0] = g.node_positions[id][0];
                d.nodes[i][1] = g.node_positions[id][1];
            }

            if (x[0] == x[1])
                ++x[1];

            if (y[0] == y[1])
                ++y[1];

            if (z[0] == z[1])
                ++z[1];

            d.top_left     = { x[0], y[1], z[0] };
            d.bottom_right = { x[1], y[0], z[1] };
            d.center       = { (x[1] - x[0]) / 2.f + x[0],
                               (y[1] - y[0]) / 2.f + y[0],
                               (z[1] - z[0]) / 2.f + z[0] };

            auto_fit_camera();
        });
    });
}

} // namespace irt
