// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace irt {

static void update_bound(graph_component& graph, std::integral auto i) noexcept
{
    graph.top_left_limit[0] =
      std::min(graph.top_left_limit[0],
               graph.g.node_positions[i][0] - graph.g.node_areas[i]);
    graph.top_left_limit[1] =
      std::min(graph.top_left_limit[1],
               graph.g.node_positions[i][1] - graph.g.node_areas[i]);
    graph.bottom_right_limit[0] =
      std::max(graph.bottom_right_limit[0],
               graph.g.node_positions[i][0] + graph.g.node_areas[i]);
    graph.bottom_right_limit[1] =
      std::max(graph.bottom_right_limit[1],
               graph.g.node_positions[i][1] + graph.g.node_areas[i]);
}

bool graph_component_editor_data::compute_automatic_layout(
  graph_component& graph,
  vector<ImVec2>&  displacements,
  int              iteration) const noexcept
{
    const auto size = graph.g.nodes.size();
    if (size < 2)
        return false;

    const auto sqrt_size = std::sqrt(static_cast<float>(size));
    const auto W         = (sqrt_size + 1.f) * distance.x;
    const auto L         = (sqrt_size + 1.f) * distance.y;
    const auto area      = W * L;
    const auto k_square  = area / static_cast<float>(size);
    const auto k         = std::sqrt(k_square);

    const auto t =
      (1.f -
       (static_cast<float>(iteration) / static_cast<float>(iteration_limit))) *
      (1.f -
       (static_cast<float>(iteration) / static_cast<float>(iteration_limit)));

    for (const auto v_edge_id : graph.g.edges) {
        const auto& v_edge = graph.g.edges_nodes[get_index(v_edge_id)];

        if (not graph.g.nodes.exists(v_edge[1].first))
            continue;

        const auto v       = get_index(v_edge[1].first);
        displacements[v].x = 0.f;
        displacements[v].y = 0.f;

        for (const auto u_edge_id : graph.g.edges) {
            const auto& u_edge = graph.g.edges_nodes[get_index(u_edge_id)];
            if (not graph.g.nodes.exists(u_edge[0].first))
                continue;

            const auto u = get_index(u_edge[0].first);
            if (u != v) {
                const auto delta = ImVec2{
                    graph.g.node_positions[v][0] - graph.g.node_positions[u][0],
                    graph.g.node_positions[v][1] - graph.g.node_positions[u][1]
                };

                if (delta.x && delta.y) {
                    const float d2    = delta.x * delta.x + delta.y * delta.y;
                    const float coeff = k_square / d2;

                    displacements[v].x +=
                      std::clamp(coeff * delta.x, -distance.x, distance.x);
                    displacements[v].y +=
                      std::clamp(coeff * delta.y, -distance.y, distance.y);
                }
            }
        }
    }

    for (const auto edge_id : graph.g.edges) {
        const auto& edge = graph.g.edges_nodes[get_index(edge_id)];

        const auto  u = get_index(edge[0].first);
        const auto  v = get_index(edge[1].first);
        const float dx =
          graph.g.node_positions[v][0] - graph.g.node_positions[u][0];
        const float dy =
          graph.g.node_positions[v][1] - graph.g.node_positions[u][1];

        if (dx && dy) {
            const float coeff = std::sqrt((dx * dx) / (dy * dy)) / k;

            auto move_v_x = dx * coeff;
            auto move_v_y = dy * coeff;
            auto move_u_x = dx * coeff;
            auto move_u_y = dy * coeff;

            displacements[v].x -= std::clamp(move_v_x, -distance.x, distance.x);
            displacements[v].y -= std::clamp(move_v_y, -distance.y, distance.y);
            displacements[u].x += std::clamp(move_u_x, -distance.x, distance.x);
            displacements[u].y += std::clamp(move_u_y, -distance.y, distance.y);
        }
    }

    bool have_big_displacement = false;
    for (const auto edge_id : graph.g.edges) {
        const auto& edge = graph.g.edges_nodes[get_index(edge_id)];

        const auto  v = get_index(edge[1].first);
        const float d = std::sqrt((displacements[v].x * displacements[v].x) +
                                  (displacements[v].y * displacements[v].y));

        if (d > t) {
            const float coeff = t / d;
            displacements[v].x *= coeff;
            displacements[v].y *= coeff;
        }

        if (displacements[v].x > 10.f or displacements[v].y > 10.f)
            have_big_displacement = true;

        graph.g.node_positions[v][0] += displacements[v].x;
        graph.g.node_positions[v][1] += displacements[v].y;

        update_bound(graph, v);
    }

    return have_big_displacement or iteration < iteration_limit;
}

void show_project_params_menu(graph_component& graph) noexcept
{
    static const char* names[] = { "in-out", "name", "name+suffix" };

    int type = ordinal(graph.type);
    if (ImGui::Combo("connection type", &type, names, length(names))) {
        graph.type = enum_cast<graph_component::connection_type>(type);
    }

    ImGui::SameLine();
    HelpMarker(
      "- in-out: only connect output port `out' to input port `in'.\n"
      "- name: only connect output port to input port with the same name.\n"
      "- name+suffix: connect output port to input port with the same "
      "prefix. The suffix, an integer, is used to allow connection of "
      "output port from varius models to input ports with suffix (ie. two "
      "models m1 and m2 with output port M connect to input port M_0 and "
      "M_1 of a thrid model.");
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

bool graph_component_editor_data::show_graph(application& app,
                                             component& /*compo*/,
                                             graph_component& data) noexcept
{
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    canvas_sz        = ImGui::GetContentRegionAvail();
    auto update      = 0;

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(
      canvas_p0,
      canvas_p1,
      app.config.vars.colors.read([&](const auto& colors, const auto /*vers*/) {
          return to_ImU32(colors[style_color::outer_border]);
      }));

    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active  = ImGui::IsItemActive();

    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,
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

    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (drag_delta.x == 0.0f and drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("Canvas-Context",
                                    ImGuiPopupFlags_MouseButtonRight);

    if (ImGui::BeginPopup("Canvas-Context")) {
        const auto click = ImGui::GetMousePosOnOpeningCurrentPopup();
        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("New node")) {
                if (auto id = data.g.alloc_node(); id.has_value()) {
                    const auto idx = get_index(*id);

                    data.g.node_positions[idx] = {
                        (click.x - origin.x) / zoom.x,
                        (click.y - origin.y) / zoom.y,
                    };

                    selected_nodes.emplace_back(*id);
                    ++update;
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
                const auto e = selected_nodes.size();
                for (sz i = 0; i < e; ++i) {
                    for (sz j = i + 1; j < e; ++j) {
                        data.g.alloc_edge(selected_nodes[i], selected_nodes[j]);
                        ++update;
                    }
                }
                selected_nodes.clear();
            }

            if (not selected_nodes.empty() and
                ImGui::MenuItem("Delete nodes")) {
                for (auto id : selected_nodes) {
                    if (data.g.nodes.exists(id)) {
                        data.g.nodes.free(id);
                        ++update;
                    }
                }
                selected_nodes.clear();
            }

            if (not selected_edges.empty() and
                ImGui::MenuItem("Delete edges")) {
                for (auto id : selected_edges)
                    if (data.g.edges.exists(id)) {
                        data.g.edges.free(id);
                        ++update;
                    }
                selected_edges.clear();
            }

            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

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

                for (const auto id : data.g.nodes) {
                    const auto i = get_index(id);

                    ImVec2 p_min(
                      origin.x + (data.g.node_positions[i][0] * zoom.x),
                      origin.y + (data.g.node_positions[i][1] * zoom.y));

                    ImVec2 p_max(
                      origin.x +
                        ((data.g.node_positions[i][0] + data.g.node_areas[i]) *
                         zoom.x),
                      origin.y +
                        ((data.g.node_positions[i][1] + data.g.node_areas[i]) *
                         zoom.y));

                    if (p_min.x >= bmin.x and p_max.x < bmax.x and
                        p_min.y >= bmin.y and p_max.y < bmax.y) {
                        selected_nodes.emplace_back(id);
                    }
                }

                for (const auto id : data.g.edges) {
                    const auto us = data.g.edges_nodes[get_index(id)][0].first;
                    const auto vs = data.g.edges_nodes[get_index(id)][1].first;

                    if (data.g.nodes.exists(us) and data.g.nodes.exists(vs)) {
                        ImVec2 p1(
                          origin.x + ((data.g.node_positions[get_index(us)][0] +
                                       data.g.node_areas[get_index(us)] / 2.f) *
                                      zoom.x),
                          origin.y + ((data.g.node_positions[get_index(us)][1] +
                                       data.g.node_areas[get_index(vs)] / 2.f) *
                                      zoom.y));

                        ImVec2 p2(
                          origin.x + ((data.g.node_positions[get_index(vs)][0] +
                                       data.g.node_areas[get_index(us)] / 2.f) *
                                      zoom.x),
                          origin.y + ((data.g.node_positions[get_index(vs)][1] +
                                       data.g.node_areas[get_index(vs)] / 2.f) *
                                      zoom.y));

                        if (is_line_intersects_box(p1, p2, bmin, bmax))
                            selected_edges.emplace_back(id);
                    }
                }
            }
        }
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    const float GRID_STEP = 64.0f;

    auto [inner_border, node_active, edge, edge_active, background_selection] =
      app.config.vars.colors.read([&](const auto& colors, const auto /*v*/) {
          return std::tuple(
            to_ImU32(colors[style_color::inner_border]),
            to_ImU32(colors[style_color::node_active]),
            to_ImU32(colors[style_color::edge]),
            to_ImU32(colors[style_color::edge_active]),
            to_ImU32(colors[style_color::background_selection]));
      });

    for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           inner_border);

    for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           inner_border);

    for (const auto id : data.g.nodes) {
        const auto i = get_index(id);

        const ImVec2 p_min(origin.x + (data.g.node_positions[i][0] * zoom.x),
                           origin.y + (data.g.node_positions[i][1] * zoom.y));

        const ImVec2 p_max(
          origin.x +
            ((data.g.node_positions[i][0] + data.g.node_areas[i]) * zoom.x),
          origin.y +
            ((data.g.node_positions[i][1] + data.g.node_areas[i]) * zoom.y));

        draw_list->AddRectFilled(
          p_min, p_max, get_component_u32color(app, data.g.node_components[i]));
    }

    for (const auto id : selected_nodes) {
        const auto i = get_index(id);

        ImVec2 p_min(origin.x + (data.g.node_positions[i][0] * zoom.x),
                     origin.y + (data.g.node_positions[i][1] * zoom.y));

        ImVec2 p_max(
          origin.x +
            ((data.g.node_positions[i][0] + data.g.node_areas[i]) * zoom.x),
          origin.y +
            ((data.g.node_positions[i][1] + data.g.node_areas[i]) * zoom.y));

        draw_list->AddRect(p_min, p_max, node_active, 0.f, 0, 4.f);
    }

    for (const auto id : data.g.edges) {
        const auto i   = get_index(id);
        const auto u_c = data.g.edges_nodes[i][0].first;
        const auto v_c = data.g.edges_nodes[i][1].first;

        if (not(data.g.nodes.exists(u_c) and data.g.nodes.exists(v_c)))
            continue;

        const auto p_src = get_index(u_c);
        const auto p_dst = get_index(v_c);

        const auto u_width  = data.g.node_areas[p_src] / 2.f;
        const auto u_height = data.g.node_areas[p_src] / 2.f;

        const auto v_width  = data.g.node_areas[p_dst] / 2.f;
        const auto v_height = data.g.node_areas[p_dst] / 2.f;

        ImVec2 src(
          origin.x + ((data.g.node_positions[p_src][0] + u_width) * zoom.x),
          origin.y + ((data.g.node_positions[p_src][1] + u_height) * zoom.y));

        ImVec2 dst(
          origin.x + ((data.g.node_positions[p_dst][0] + v_width) * zoom.x),
          origin.y + ((data.g.node_positions[p_dst][1] + v_height) * zoom.y));

        draw_list->AddLine(src, dst, edge, 1.f);
    }

    for (const auto id : selected_edges) {
        const auto idx = get_index(id);
        const auto u_c = data.g.edges_nodes[idx][0].first;
        const auto v_c = data.g.edges_nodes[idx][1].first;

        if (not(data.g.nodes.exists(u_c) and data.g.nodes.exists(v_c)))
            continue;

        const auto p_src = get_index(u_c);
        const auto p_dst = get_index(v_c);

        ImVec2 src(origin.x + ((data.g.node_positions[p_src][0] +
                                data.g.node_areas[p_src] / 2.f) *
                               zoom.x),
                   origin.y + ((data.g.node_positions[p_src][1] +
                                data.g.node_areas[p_src] / 2.f) *
                               zoom.y));

        ImVec2 dst(origin.x + ((data.g.node_positions[p_dst][0] +
                                data.g.node_areas[p_dst] / 2.f) *
                               zoom.x),
                   origin.y + ((data.g.node_positions[p_dst][1] +
                                data.g.node_areas[p_dst] / 2.f) *
                               zoom.y));

        draw_list->AddLine(src, dst, edge_active, 1.0f);
    }

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

            draw_list->AddRectFilled(bmin, bmax, background_selection);
        }
    }

    draw_list->PopClipRect();

    return update > 0;
}

void graph_component_editor_data::center_camera() noexcept
{
    ImVec2 distance(m_graph.bottom_right_limit[0] - m_graph.top_left_limit[0],
                    m_graph.bottom_right_limit[1] - m_graph.top_left_limit[1]);
    ImVec2 center(
      (m_graph.bottom_right_limit[0] - m_graph.top_left_limit[0]) / 2.0f +
        m_graph.top_left_limit[0],
      (m_graph.bottom_right_limit[1] - m_graph.top_left_limit[1]) / 2.0f +
        m_graph.top_left_limit[1]);

    scrolling.x = ((-center.x * zoom.x) + (canvas_sz.x / 2.f));
    scrolling.y = ((-center.y * zoom.y) + (canvas_sz.y / 2.f));
}

void graph_component_editor_data::auto_fit_camera() noexcept
{
    ImVec2 distance(m_graph.bottom_right_limit[0] - m_graph.top_left_limit[0],
                    m_graph.bottom_right_limit[1] - m_graph.top_left_limit[1]);
    ImVec2 center(
      (m_graph.bottom_right_limit[0] - m_graph.top_left_limit[0]) / 2.0f +
        m_graph.top_left_limit[0],
      (m_graph.bottom_right_limit[1] - m_graph.top_left_limit[1]) / 2.0f +
        m_graph.top_left_limit[1]);

    zoom.x = canvas_sz.x / distance.x;
    zoom.y = canvas_sz.y / distance.y;

    zoom.x = almost_equal(zoom.x, 0.f, 1e-3F) ? 1.f : zoom.x;
    zoom.y = almost_equal(zoom.y, 0.f, 1e-3F) ? 1.f : zoom.y;

    scrolling.x = ((-center.x * zoom.x) + (canvas_sz.x / 2.f));
    scrolling.y = ((-center.y * zoom.y) + (canvas_sz.y / 2.f));
}

bool graph_component_editor_data::show_scale_free_menu(
  application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Generate scale free graph")) {
        HelpMarker("scale_free: graph typically has a very skewed degree "
                   "distribution, where few vertices have a very high "
                   "degree and a large number of vertices have a very "
                   "small degree. Many naturally evolving networks, such as "
                   "the World Wide Web, are scale-free graphs, making these "
                   "graphs a good model for certain networking.");

        if (ImGui::InputInt("size", &m_graph.scale.nodes)) {
            m_graph.scale.nodes =
              std::clamp(m_graph.scale.nodes, 1, graph_component::children_max);
            ++u;
        }

        if (ImGui::InputDouble("alpha", &m_graph.scale.alpha)) {
            m_graph.scale.alpha = std::clamp(m_graph.scale.alpha, 0.0, 1000.0);
            ++u;
        }

        if (ImGui::InputDouble("beta", &m_graph.scale.beta)) {
            m_graph.scale.beta = std::clamp(m_graph.scale.beta, 0.0, 1000.0);
            ++u;
        }

        if (auto r = app.component_sel.combobox("component", m_graph.scale.id);
            r) {
            m_graph.scale.id = r.id;
            ++u;
        }

        const auto size = ImGui::ComputeButtonSize(2);
        ImGui::BeginDisabled(task_is_running.test());
        if (ImGui::Button("generate", size)) {
            clear_selected_nodes();

            if (not task_is_running.test_and_set()) {
                if (m_graph_2nd.should_request()) {
                    app.add_gui_task([this]() {
                        graph_component new_graph;
                        new_graph.g.init_scale_free_graph(m_graph.scale.alpha,
                                                          m_graph.scale.beta,
                                                          m_graph.scale.id,
                                                          m_graph.scale.nodes,
                                                          new_graph.rng);
                        new_graph.scale = m_graph.scale;
                        new_graph.g_type =
                          graph_component::graph_type::scale_free;
                        new_graph.update_position();
                        new_graph.assign_grid_position(distance.x, distance.y);

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("cancel", size)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    return u > 0;
}

bool graph_component_editor_data::show_small_world_menu(
  application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Generate Small world graph")) {
        HelpMarker(
          "small_world: consists of a ring graph (where each vertex is "
          "connected to its k nearest neighbors).Edges in the graph are "
          "randomly rewired to different vertices with a probability p.");

        if (ImGui::InputInt("size", &m_graph.small.nodes)) {
            ++u;
            m_graph.small.nodes =
              std::clamp(m_graph.small.nodes, 1, graph_component::children_max);
        }

        if (ImGui::InputDouble("probability", &m_graph.small.probability)) {
            ++u;
            m_graph.small.probability =
              std::clamp(m_graph.small.probability, 0.0, 1.0);
        }

        if (ImGui::InputInt("k", &m_graph.small.k, 1, 2)) {
            ++u;
            m_graph.small.k = std::clamp(m_graph.small.k, 1, 8);
        }

        if (auto r = app.component_sel.combobox("component", m_graph.small.id);
            r) {
            ++u;
            m_graph.small.id = r.id;
        }

        const auto size = ImGui::ComputeButtonSize(2);
        ImGui::BeginDisabled(task_is_running.test());
        if (ImGui::Button("generate", size)) {
            clear_selected_nodes();

            if (not task_is_running.test_and_set()) {
                if (m_graph_2nd.should_request()) {
                    app.add_gui_task([this]() {
                        graph_component new_graph;
                        new_graph.g.init_small_world_graph(
                          m_graph.small.probability,
                          m_graph.small.k,
                          m_graph.small.id,
                          m_graph.small.nodes,
                          new_graph.rng);
                        new_graph.small = m_graph.small;
                        new_graph.g_type =
                          graph_component::graph_type::small_world;
                        new_graph.update_position();
                        new_graph.assign_grid_position(distance.x, distance.y);

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("cancel", size)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    return u > 0;
}

bool graph_component_editor_data::show_dot_file_menu(application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Read Dot graph")) {
        HelpMarker("dot: consists of a file defining the graph "
                   "according to the dot file format.");

        auto close = app.mod.files.read([&](const auto& fs,
                                            auto) noexcept -> bool {
            const auto selected = file_select.combobox_ro(
              fs, reg, dir, file, file_path::file_type::dot_file);

            reg  = selected.reg_id;
            dir  = selected.dir_id;
            file = selected.file_id;

            if (selected.save and not task_is_running.test_and_set()) {
                m_graph.dot.file = selected.file_id;

                if (m_graph_2nd.should_request()) {
                    app.add_gui_task([&app, this]() {
                        graph_component new_graph;

                        app.mod.files.read([&](const auto& fs, auto) noexcept {
                            if (auto path = fs.get_fs_path(m_graph.dot.file)) {
                                app.mod.ids.read([&](const auto& ids,
                                                     auto) noexcept {
                                    if (auto dot_graph = parse_dot_file(
                                          fs, ids, *path, app.jn)) {

                                        new_graph.g   = std::move(*dot_graph);
                                        new_graph.dot = m_graph.dot;
                                        new_graph.g_type =
                                          graph_component::graph_type::dot_file;
                                        new_graph.update_position();
                                    }
                                });
                            }
                        });

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            return selected.close;
        });

        if (close)
            ImGui::CloseCurrentPopup();

        ImGui::EndMenu();
    }

    return u > 0;
}

void save_dot_file(application&       app,
                   const graph&       g,
                   const file_path_id file_id) noexcept
{
    auto g_ptr = std::make_unique<graph>(g);

    app.add_gui_task([&app, g = std::move(g_ptr), file_id]() {
        app.mod.files.read([&](const auto& fs, auto) {
            app.mod.ids.read([&](const auto& ids, auto) {
                const auto file = fs.get_fs_path(file_id);
                if (file.has_error()) {
                    app.jn.push(log_level::error, [&](auto& t, auto& m) {
                        t = "Graph component editor";
                        format(m, "Fail to open file {}\n", ordinal(file_id));
                    });

                    return;
                }

                const auto ret = write_dot_file(fs, ids, *g, *file);
                if (ret.has_error()) {
                    app.jn.push(log_level::error, [&](auto& t, auto& m) {
                        t = "Graph component editor";
                        format(m, "Fail to write file {}\n", file->string());
                    });
                }
            });
        });
    });
}

void graph_component_editor_data::automatic_layout_task(
  application& app) noexcept
{
    if (not task_is_running.test_and_set()) {
        auto g_ptr = std::make_unique<graph_component>(m_graph);

        app.add_gui_task([this, g = std::move(g_ptr)]() {
            namespace sc = std::chrono;

            auto       displacements = vector<ImVec2>(g->g.nodes.size());
            const auto start         = sc::system_clock::now();
            auto       next_update   = sc::milliseconds{ 0 };

            for (i32 iteration = 0; iteration < iteration_limit; ++iteration) {
                if (not compute_automatic_layout(*g, displacements, iteration))
                    break;

                const auto now = sc::system_clock::now();
                const auto ms =
                  sc::duration_cast<sc::milliseconds>(now - start);
                if (ms >= sc::milliseconds{ time_limit_ms })
                    break;

                next_update += ms;
                if (next_update > sc::milliseconds{ update_frequence_ms }) {
                    if (m_graph_2nd.should_request()) {
                        next_update  = sc::milliseconds{ 0 };
                        auto to_move = graph_component(*g);

                        m_graph_2nd.fulfill(std::move(to_move));
                    }
                }
            }

            task_is_running.clear();
        });
    }
}

bool graph_component_editor_data::graph_component_editor_data::show(
  application& app,
  component&   compo) noexcept
{
    int u = 0;

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Model")) {
            u += show_scale_free_menu(app);
            u += show_small_world_menu(app);
            u += show_dot_file_menu(app);
            ImGui::Separator();

            const auto already_saved_defined =
              is_defined(reg) and is_defined(dir) and is_defined(file);
            auto close = false;

            ImGui::BeginDisabled(not already_saved_defined);
            if (ImGui::MenuItem("Save")) {
                save_dot_file(app, m_graph.g, file);
            }
            ImGui::EndDisabled();

            if (ImGui::BeginMenu("Save as...")) {
                close = app.mod.files.read([&](const auto& fs,
                                               auto) noexcept -> bool {
                    const auto selected = file_select.combobox(
                      app, fs, reg, dir, file, file_path::file_type::dot_file);

                    reg  = selected.reg_id;
                    dir  = selected.dir_id;
                    file = selected.file_id;

                    if (selected.save)
                        save_dot_file(app, m_graph.g, selected.file_id);

                    return selected.close;
                });

                if (close)
                    ImGui::CloseCurrentPopup();

                ImGui::EndMenu();
            }

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(
              center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Display##Menu")) {
            ImGui::BeginDisabled(task_is_running.test());
            if (ImGui::MenuItem("Automatic layout"))
                automatic_layout_task(app);
            ImGui::EndDisabled();

            if (ImGui::MenuItem("Center"))
                center_camera();
            if (ImGui::MenuItem("Auto-fit"))
                auto_fit_camera();

            ImGui::SeparatorText("Automatic Layout");

            if (ImGui::InputInt("iteration limit", &iteration_limit)) {
                iteration_limit = std::clamp(iteration_limit, 1'000, 100'000);
            }

            if (ImGui::InputInt("time limit (ms)", &time_limit_ms)) {
                time_limit_ms = std::clamp(time_limit_ms, 0'500, 10'000);
            }

            if (ImGui::InputInt("draw frequence (ms)", &update_frequence_ms)) {
                update_frequence_ms =
                  std::clamp(update_frequence_ms, 0'160, 1'000);
            }

            ImGui::SeparatorText("Drawing settings");

            float wzoom[2] = { zoom.x, zoom.y };
            if (ImGui::MenuItem("Reset zoom")) {
                zoom.x = 1.0f;
                zoom.y = 1.0f;
            }

            if (ImGui::InputFloat2("Zoom x,y", wzoom)) {
                zoom.x = ImClamp(wzoom[0], 0.1f, 1000.f);
                zoom.y = ImClamp(wzoom[1], 0.1f, 1000.f);
            }

            float wdistance[2] = { distance.x, distance.y };
            if (ImGui::InputFloat2("Distance between node", wdistance)) {
                distance.x = ImClamp(wdistance[0], 0.1f, 100.f);
                distance.y = ImClamp(wdistance[0], 0.1f, 100.f);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Connectivity")) {
            show_project_params_menu(m_graph);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    u += show_graph(app, compo, m_graph);

    return u > 0;
}

void graph_component_editor_data::clear_file_access() noexcept
{
    reg  = undefined<registred_path_id>();
    dir  = undefined<dir_path_id>();
    file = undefined<file_path_id>();
}

graph_component_editor_data::graph_component_editor_data(
  const component_id       id_,
  const graph_component_id graph_id_) noexcept
  : graph_id(graph_id_)
  , m_id(id_)
{}

void graph_component_editor_data::clear() noexcept
{
    graph_id = undefined<graph_component_id>();
    m_id     = undefined<component_id>();
}

bool graph_component_editor_data::show(component_editor& ed,
                                       const component_access& /*ids*/,
                                       component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    int   u   = 0;

    if (auto new_graph_opt = m_graph_2nd.try_take()) {
        const auto t_l = m_graph.top_left_limit;
        const auto b_r = m_graph.bottom_right_limit;
        m_graph        = *new_graph_opt;

        if (t_l != m_graph.top_left_limit or b_r != m_graph.bottom_right_limit)
            auto_fit_camera();

        ++u;
    } else {
        read(app, compo);
    }

    if (not is_initialized and canvas_sz.x and canvas_sz.y) {
        auto_fit_camera();
        is_initialized = true;
    }

    if (ImGui::BeginChild("##graph-ed",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_MenuBar)) {
        u += show(app, compo);
    }
    ImGui::EndChild();

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

bool graph_component_editor_data::show_selected_nodes(
  component_editor&       ed,
  const component_access& ids,
  component&              compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);

    int u = 0;
    if (ImGui::TreeNodeEx("selected nodes")) {
        for (const auto id : selected_nodes) {
            if (m_graph.g.nodes.exists(id)) {
                const auto idx = get_index(id);
                ImGui::PushID(idx);

                name_str name = m_graph.g.node_names[idx];
                if (ImGui::InputFilteredString("name", name)) {
                    ++u;
                    m_graph.g.node_names[idx] =
                      m_graph.g.buffer.append(name.sv());
                }

                name_str id_str = m_graph.g.node_names[idx];
                if (ImGui::InputFilteredString("id", id_str)) {
                    ++u;
                    m_graph.g.node_names[idx] =
                      m_graph.g.buffer.append(id_str.sv());
                }

                ImGui::LabelFormat("position",
                                   "{}x{}",
                                   m_graph.g.node_positions[idx][0],
                                   m_graph.g.node_positions[idx][1]);

                if (auto area = m_graph.g.node_areas[idx];
                    ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
                    m_graph.g.node_areas[idx] = area;
                    ++u;
                }

                if (auto r = app.component_sel.combobox(
                      "component", m_graph.g.node_components[idx]);
                    r) {
                    m_graph.g.node_components[idx] = r.id;
                    ++u;
                }

                if (ids.exists(m_graph.g.node_components[idx])) {
                    auto& compo =
                      ids.components[m_graph.g.node_components[idx]];
                    if (not compo.x.empty()) {
                        if (ImGui::TreeNodeEx("input ports")) {
                            const auto& xnames =
                              compo.x.template get<port_str>();
                            for (const auto id : compo.x) {
                                ImGui::TextFormat("{}", xnames[id].sv());
                            }
                            ImGui::TreePop();
                        }
                    }

                    if (not compo.y.empty()) {
                        if (ImGui::TreeNodeEx("output ports")) {
                            const auto& ynames =
                              compo.y.template get<port_str>();
                            for (const auto id : compo.y) {
                                ImGui::TextFormat("{}", ynames[id].sv());
                            }
                            ImGui::TreePop();
                        }
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("apply for all selected")) {
        if (auto r = app.component_sel.combobox("component",
                                                undefined<component_id>());
            r) {
            for (const auto id : selected_nodes) {
                if (m_graph.g.nodes.exists(id)) {
                    m_graph.g.node_components[id] = r.id;
                    ++u;
                }
            }
        }

        static auto area = 1.f;
        if (ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
            for (const auto id : selected_nodes) {
                if (m_graph.g.nodes.exists(id)) {
                    m_graph.g.node_areas[id] = area;
                    ++u;
                }
            }
        }

        ImGui::TreePop();
    }

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

bool graph_component_editor_data::need_show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    return not selected_nodes.empty();
}

void graph_component_editor_data::clear_selected_nodes() noexcept
{
    selected_edges.clear();
    selected_nodes.clear();
}

void graph_component_editor_data::read(application& app,
                                       component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        compo = ids.components[m_id];
        debug::ensure(compo.type == component_type::graph);

        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id)) {
            m_graph = *g;
        }
    });
}

void graph_component_editor_data::write(application& app,
                                        component&   compo) noexcept
{
    auto c_ptr = std::make_unique<component>(std::move(compo));
    auto g_ptr = std::make_unique<graph_component>(std::move(m_graph));

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (debug::check(c->type == component_type::graph)) {
                  const auto graph_id = c->id.graph_id;

                  if (auto* graph = ids.graph_components.try_to_get(graph_id))
                      *graph = *g;
              }
          });
      });
}

} // namespace irt
