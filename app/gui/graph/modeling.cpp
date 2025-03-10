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

static const char* random_graph_type_names[] = { "dot-file",
                                                 "scale-free",
                                                 "small-world" };

static auto dir_path_combobox(const modeling& mod, dir_path_id& dir_id) -> bool
{
    const auto* dir         = mod.dir_paths.try_to_get(dir_id);
    const auto* preview_dir = dir ? dir->path.c_str() : "-";
    const auto  old_dir_id  = dir_id;

    if (ImGui::BeginCombo("Directory", preview_dir)) {
        if (ImGui::Selectable("-", dir == nullptr)) {
            dir_id = undefined<dir_path_id>();
        }

        auto local_id = 0;
        for (const auto& elem : mod.dir_paths) {
            const auto elem_id = mod.dir_paths.get_id(elem);
            ImGui::PushID(++local_id);

            if (ImGui::Selectable(elem.path.c_str(), dir_id == elem_id)) {
                dir_id = elem_id;
            }

            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return dir_id != old_dir_id;
}

static auto dot_file_combobox(const modeling& mod,
                              const dir_path& dir,
                              file_path_id&   file_id) noexcept -> bool
{
    const auto* file         = mod.file_paths.try_to_get(file_id);
    const auto* preview_file = file ? file->path.c_str() : "-";
    const auto  old_file_id  = file_id;

    if (ImGui::BeginCombo("Dot file", preview_file)) {
        if (ImGui::Selectable("-", file == nullptr)) {
            file_id = undefined<file_path_id>();
        }

        int local_id = 0;
        for (const auto elem_id : dir.children) {
            if (const auto* elem = mod.file_paths.try_to_get(elem_id);
                elem and elem->type == file_path::file_type::dot_file) {
                ImGui::PushID(++local_id);

                if (ImGui::Selectable(elem->path.c_str(), file_id == elem_id)) {
                    file_id = elem_id;
                }

                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    return file_id != old_file_id;
}

static auto dot_combobox_selector(const modeling& mod,
                                  dir_path_id&    dir_id,
                                  file_path_id&   file_id) noexcept -> bool
{
    const auto old_dir_id  = dir_id;
    const auto old_file_id = file_id;

    dir_path_combobox(mod, dir_id);
    if (const auto* dir = mod.dir_paths.try_to_get(dir_id))
        dot_file_combobox(mod, *dir, file_id);

    return not(old_dir_id == dir_id and old_file_id == file_id);
}

struct graph_component_editor_data::impl {
    graph_component_editor_data& ed;

    bool param_updated = false;

    bool compute_automatic_layout(graph_component& graph) noexcept
    {
        const auto size = graph.nodes.size();
        if (size < 2)
            return false;

        ed.displacements.resize(graph.nodes.size());

        const auto sqrt_size = std::sqrt(static_cast<float>(size));
        const auto W         = (sqrt_size + 1.f) * ed.distance.x;
        const auto L         = (sqrt_size + 1.f) * ed.distance.y;
        const auto area      = W * L;
        const auto k_square  = area / static_cast<float>(size);
        const auto k         = std::sqrt(k_square);

        const auto t = (1.f - (static_cast<float>(ed.iteration) /
                               static_cast<float>(ed.iteration_limit))) *
                       (1.f - (static_cast<float>(ed.iteration) /
                               static_cast<float>(ed.iteration_limit)));

        for (const auto v_edge_id : graph.edges) {
            const auto& v_edge = graph.edges_nodes[get_index(v_edge_id)];

            if (not graph.nodes.exists(v_edge[1]))
                continue;

            const auto v          = get_index(v_edge[1]);
            ed.displacements[v].x = 0.f;
            ed.displacements[v].y = 0.f;

            for (const auto u_edge_id : graph.edges) {
                const auto& u_edge = graph.edges_nodes[get_index(u_edge_id)];
                if (not graph.nodes.exists(u_edge[0]))
                    continue;

                const auto u = get_index(u_edge[0]);
                if (u != v) {
                    const auto delta = ImVec2{
                        graph.node_positions[v][0] - graph.node_positions[u][0],
                        graph.node_positions[v][1] - graph.node_positions[u][1]
                    };

                    if (delta.x && delta.y) {
                        const float d2 = delta.x * delta.x + delta.y * delta.y;
                        const float coeff = k_square / d2;

                        ed.displacements[v].x += std::clamp(
                          coeff * delta.x, -ed.distance.x, ed.distance.x);
                        ed.displacements[v].y += std::clamp(
                          coeff * delta.y, -ed.distance.y, ed.distance.y);
                    }
                }
            }
        }

        for (const auto edge_id : graph.edges) {
            const auto& edge = graph.edges_nodes[get_index(edge_id)];

            const auto  u = get_index(edge[0]);
            const auto  v = get_index(edge[1]);
            const float dx =
              graph.node_positions[v][0] - graph.node_positions[u][0];
            const float dy =
              graph.node_positions[v][1] - graph.node_positions[u][1];

            if (dx && dy) {
                const float coeff = std::sqrt((dx * dx) / (dy * dy)) / k;

                auto move_v_x = dx * coeff;
                auto move_v_y = dy * coeff;
                auto move_u_x = dx * coeff;
                auto move_u_y = dy * coeff;

                ed.displacements[v].x -=
                  std::clamp(move_v_x, -ed.distance.x, ed.distance.x);
                ed.displacements[v].y -=
                  std::clamp(move_v_y, -ed.distance.y, ed.distance.y);
                ed.displacements[u].x +=
                  std::clamp(move_u_x, -ed.distance.x, ed.distance.x);
                ed.displacements[u].y +=
                  std::clamp(move_u_y, -ed.distance.y, ed.distance.y);
            }
        }

        bool have_big_displacement = false;
        for (const auto edge_id : graph.edges) {
            const auto& edge = graph.edges_nodes[get_index(edge_id)];

            const auto  v = get_index(edge[1]);
            const float d =
              std::sqrt((ed.displacements[v].x * ed.displacements[v].x) +
                        (ed.displacements[v].y * ed.displacements[v].y));

            if (d > t) {
                const float coeff = t / d;
                ed.displacements[v].x *= coeff;
                ed.displacements[v].y *= coeff;
            }

            if (ed.displacements[v].x > 10.f or ed.displacements[v].y > 10.f)
                have_big_displacement = true;

            graph.node_positions[v][0] += ed.displacements[v].x;
            graph.node_positions[v][1] += ed.displacements[v].y;

            update_bound(graph, v);
        }

        return have_big_displacement or ed.iteration < ed.iteration_limit;
    }

    void show_random_graph_type(graph_component& graph) noexcept
    {
        auto current = static_cast<int>(graph.g_type);

        if (ImGui::Combo("type",
                         &current,
                         random_graph_type_names,
                         length(random_graph_type_names))) {
            if (current != static_cast<int>(graph.g_type)) {
                graph.g_type = enum_cast<graph_component::graph_type>(current);
                switch (graph.g_type) {
                case graph_component::graph_type::dot_file:
                    graph.param.dot = graph_component::dot_file_param{};
                    break;

                case graph_component::graph_type::scale_free:
                    graph.param.scale = graph_component::scale_free_param{};
                    break;

                case graph_component::graph_type::small_world:
                    graph.param.small = graph_component::small_world_param{};
                    break;
                }
            }
        }

        if (ordinal(graph.g_type) != current)
            param_updated = true;

        ImGui::SameLine();
        HelpMarker(
          "scale_free: graph typically has a very skewed degree distribution, "
          "where few vertices have a very high degree and a large number of "
          "vertices have a very small degree. Many naturally evolving "
          "networks, such as the World Wide Web, are scale-free graphs, making "
          "these graphs a good model for certain networking "
          "problems.\n\nsmall_world: consists of a ring graph (where each "
          "vertex is connected to its k nearest neighbors) .Edges in the graph "
          "are randomly rewired to different vertices with a probability p.");
    }

    void show_random_graph_params(application&     app,
                                  graph_component& graph) noexcept
    {
        switch (graph.g_type) {
        case graph_component::graph_type::dot_file: {
            auto& p = graph.param.dot;

            const auto old_param = p;
            dot_combobox_selector(app.mod, p.dir, p.file);
            if (p.dir != old_param.dir or p.file != old_param.file) {
                param_updated       = true;
                ed.automatic_layout = false;
            }
        } break;

        case graph_component::graph_type::scale_free: {
            auto& p = graph.param.scale;

            if (ImGui::InputInt("size", &p.nodes)) {
                p.nodes = std::clamp(p.nodes, 1, graph_component::children_max);
                param_updated = true;
            }

            if (ImGui::InputDouble("alpha", &p.alpha)) {
                p.alpha       = std::clamp(p.alpha, 0.0, 1000.0);
                param_updated = true;
            }

            if (ImGui::InputDouble("beta", &p.beta)) {
                p.beta        = std::clamp(p.beta, 0.0, 1000.0);
                param_updated = true;
            }

            if (app.component_sel.combobox("component", &p.id)) {
                param_updated = true;
            }
        } break;

        case graph_component::graph_type::small_world: {
            auto& p = graph.param.small;

            if (ImGui::InputInt("size", &p.nodes)) {
                p.nodes = std::clamp(p.nodes, 1, graph_component::children_max);
                param_updated = true;
            }

            if (ImGui::InputDouble("probability", &p.probability)) {
                p.probability = std::clamp(p.probability, 0.0, 1.0);
                param_updated = true;
            }

            if (ImGui::InputInt("k", &p.k, 1, 2)) {
                p.k           = std::clamp(p.k, 1, 8);
                param_updated = true;
            }

            if (app.component_sel.combobox("component", &p.id)) {
                param_updated = true;
            }
        } break;
        }
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
        const bool b_miss = (p1.x > bmax.x and p2.x > bmax.x) or
                            (p1.x < bmin.x and p2.x < bmin.x) or
                            (p1.y > bmax.y and p2.y > bmax.y) or
                            (p1.y < bmin.y and p2.y < bmin.y);

        return not(a_miss or b_miss);
    };

    void show_graph(application& app,
                    component& /*compo*/,
                    graph_component_editor_data& ed,
                    graph_component&             data) noexcept
    {
        float zoom[2] = { ed.zoom.x, ed.zoom.y };
        if (ImGui::InputFloat2("zoom x,y", zoom)) {
            ed.zoom.x = ImClamp(zoom[0], 0.1f, 1000.f);
            ed.zoom.y = ImClamp(zoom[1], 0.1f, 1000.f);
        }

        float distance[2] = { ed.distance.x, ed.distance.y };
        if (ImGui::InputFloat2("force x,y", distance)) {
            ed.distance.x = ImClamp(distance[0], 0.1f, 100.f);
            ed.distance.y = ImClamp(distance[0], 0.1f, 100.f);
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

        if (ed.st == graph_component_editor_data::status::center_required)
            center_camera(ImVec2(data.top_left[0], data.top_left[1]),
                          ImVec2(data.bottom_right[0], data.bottom_right[1]),
                          canvas_sz);

        if (ed.st == graph_component_editor_data::status::auto_fit_required)
            auto_fit_camera(ImVec2(data.top_left[0], data.top_left[1]),
                            ImVec2(data.bottom_right[0], data.bottom_right[1]),
                            canvas_sz);

        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        ImGui::InvisibleButton("Canvas",
                               canvas_sz,
                               ImGuiButtonFlags_MouseButtonLeft |
                                 ImGuiButtonFlags_MouseButtonRight);

        const bool is_hovered = ImGui::IsItemHovered();
        const bool is_active  = ImGui::IsItemActive();

        const ImVec2 origin(canvas_p0.x + ed.scrolling.x,
                            canvas_p0.y + ed.scrolling.y);
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                         io.MousePos.y - origin.y);

        const float mouse_threshold_for_pan = -1.f;
        if (is_active) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                       mouse_threshold_for_pan)) {
                ed.scrolling.x += io.MouseDelta.x;
                ed.scrolling.y += io.MouseDelta.y;
            }
        }

        if (is_hovered and io.MouseWheel != 0.f) {
            ed.zoom.x = ed.zoom.x + (io.MouseWheel * ed.zoom.x * 0.1f);
            ed.zoom.y = ed.zoom.y + (io.MouseWheel * ed.zoom.y * 0.1f);
            ed.zoom.x = ImClamp(ed.zoom.x, 0.1f, 1000.f);
            ed.zoom.y = ImClamp(ed.zoom.y, 0.1f, 1000.f);
        }

        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (drag_delta.x == 0.0f and drag_delta.y == 0.0f and
            (not ed.selected_nodes.empty() or not ed.selected_edges.empty())) {
            ImGui::OpenPopupOnItemClick("Canvas-Context",
                                        ImGuiPopupFlags_MouseButtonRight);
        }

        if (ImGui::BeginPopup("Canvas-Context")) {
            if (ImGui::BeginMenu("Actions")) {
                if (not ed.selected_nodes.empty() and
                    ImGui::MenuItem("Delete selected nodes?")) {
                    for (auto id : ed.selected_nodes) {
                        if (data.nodes.exists(id))
                            data.nodes.free(id);
                    }
                    ed.selected_nodes.clear();
                }

                if (not ed.selected_edges.empty() and
                    ImGui::MenuItem("Delete selected edges?")) {
                    for (auto id : ed.selected_edges)
                        if (data.edges.exists(id))
                            data.edges.free(id);
                    ed.selected_edges.clear();
                }

                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        if (is_hovered) {
            if (not ed.run_selection and
                ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ed.run_selection   = true;
                ed.start_selection = io.MousePos;
            }

            if (ed.run_selection and
                ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                ed.run_selection = false;
                ed.end_selection = io.MousePos;

                if (ed.start_selection == ed.end_selection) {
                    ed.selected_nodes.clear();
                    ed.selected_edges.clear();
                } else {
                    ImVec2 bmin{
                        std::min(ed.start_selection.x, ed.end_selection.x),
                        std::min(ed.start_selection.y, ed.end_selection.y),
                    };

                    ImVec2 bmax{
                        std::max(ed.start_selection.x, ed.end_selection.x),
                        std::max(ed.start_selection.y, ed.end_selection.y),
                    };

                    ed.selected_edges.clear();
                    ed.selected_nodes.clear();

                    for (const auto id : data.nodes) {
                        const auto i = get_index(id);

                        ImVec2 p_min(
                          origin.x + (data.node_positions[i][0] * ed.zoom.x),
                          origin.y + (data.node_positions[i][1] * ed.zoom.y));

                        ImVec2 p_max(
                          origin.x +
                            ((data.node_positions[i][0] + data.node_areas[i]) *
                             ed.zoom.x),
                          origin.y +
                            ((data.node_positions[i][1] + data.node_areas[i]) *
                             ed.zoom.y));

                        if (p_min.x >= bmin.x and p_max.x < bmax.x and
                            p_min.y >= bmin.y and p_max.y < bmax.y) {
                            ed.selected_nodes.emplace_back(id);
                        }
                    }

                    for (const auto id : data.edges) {
                        const auto us = data.edges_nodes[get_index(id)][0];
                        const auto vs = data.edges_nodes[get_index(id)][1];

                        if (data.nodes.exists(us) and data.nodes.exists(vs)) {
                            ImVec2 p1(
                              origin.x +
                                ((data.node_positions[get_index(us)][0] +
                                  data.node_areas[get_index(us)] / 2.f) *
                                 ed.zoom.x),
                              origin.y +
                                ((data.node_positions[get_index(us)][1] +
                                  data.node_areas[get_index(vs)] / 2.f) *
                                 ed.zoom.y));

                            ImVec2 p2(
                              origin.x +
                                ((data.node_positions[get_index(vs)][0] +
                                  data.node_areas[get_index(us)] / 2.f) *
                                 ed.zoom.x),
                              origin.y +
                                ((data.node_positions[get_index(vs)][1] +
                                  data.node_areas[get_index(vs)] / 2.f) *
                                 ed.zoom.y));

                            if (is_line_intersects_box(p1, p2, bmin, bmax))
                                ed.selected_edges.emplace_back(id);
                        }
                    }
                }
            }
        }

        draw_list->PushClipRect(canvas_p0, canvas_p1, true);
        const float GRID_STEP = 64.0f;

        for (float x = fmodf(ed.scrolling.x, GRID_STEP); x < canvas_sz.x;
             x += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                               ImVec2(canvas_p0.x + x, canvas_p1.y),
                               IM_COL32(200, 200, 200, 40));

        for (float y = fmodf(ed.scrolling.y, GRID_STEP); y < canvas_sz.y;
             y += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                               ImVec2(canvas_p1.x, canvas_p0.y + y),
                               IM_COL32(200, 200, 200, 40));

        for (const auto id : data.nodes) {
            const auto i = get_index(id);

            const ImVec2 p_min(
              origin.x + (data.node_positions[i][0] * ed.zoom.x),
              origin.y + (data.node_positions[i][1] * ed.zoom.y));

            const ImVec2 p_max(
              origin.x +
                ((data.node_positions[i][0] + data.node_areas[i]) * ed.zoom.x),
              origin.y +
                ((data.node_positions[i][1] + data.node_areas[i]) * ed.zoom.y));

            draw_list->AddRectFilled(
              p_min,
              p_max,
              to_ImU32(
                app.mod.component_colors[get_index(data.node_components[i])]));
        }

        for (const auto id : ed.selected_nodes) {
            const auto i = get_index(id);

            ImVec2 p_min(origin.x + (data.node_positions[i][0] * ed.zoom.x),
                         origin.y + (data.node_positions[i][1] * ed.zoom.y));

            ImVec2 p_max(
              origin.x +
                ((data.node_positions[i][0] + data.node_areas[i]) * ed.zoom.x),
              origin.y +
                ((data.node_positions[i][1] + data.node_areas[i]) * ed.zoom.y));

            draw_list->AddRect(
              p_min, p_max, IM_COL32(255, 255, 255, 255), 0.f, 0, 4.f);
        }

        for (const auto id : data.edges) {
            const auto i   = get_index(id);
            const auto u_c = data.edges_nodes[i][0];
            const auto v_c = data.edges_nodes[i][1];

            if (not(data.nodes.exists(u_c) and data.nodes.exists(v_c)))
                continue;

            const auto p_src = get_index(u_c);
            const auto p_dst = get_index(v_c);

            const auto u_width  = data.node_areas[p_src] / 2.f;
            const auto u_height = data.node_areas[p_src] / 2.f;

            const auto v_width  = data.node_areas[p_dst] / 2.f;
            const auto v_height = data.node_areas[p_dst] / 2.f;

            ImVec2 src(origin.x + ((data.node_positions[p_src][0] + u_width) *
                                   ed.zoom.x),
                       origin.y + ((data.node_positions[p_src][1] + u_height) *
                                   ed.zoom.y));

            ImVec2 dst(origin.x + ((data.node_positions[p_dst][0] + v_width) *
                                   ed.zoom.x),
                       origin.y + ((data.node_positions[p_dst][1] + v_height) *
                                   ed.zoom.y));

            draw_list->AddLine(src, dst, IM_COL32(255, 255, 0, 255), 1.f);
            // IM_COL32(200, 200, 200, 40), 1.0f);
        }

        for (const auto id : ed.selected_edges) {
            const auto idx = get_index(id);
            const auto u_c = data.edges_nodes[idx][0];
            const auto v_c = data.edges_nodes[idx][1];

            if (not(data.nodes.exists(u_c) and data.nodes.exists(v_c)))
                continue;

            const auto p_src = get_index(u_c);
            const auto p_dst = get_index(v_c);

            ImVec2 src(origin.x + ((data.node_positions[p_src][0] +
                                    data.node_areas[p_src] / 2.f) *
                                   ed.zoom.x),
                       origin.y + ((data.node_positions[p_src][1] +
                                    data.node_areas[p_src] / 2.f) *
                                   ed.zoom.y));

            ImVec2 dst(origin.x + ((data.node_positions[p_dst][0] +
                                    data.node_areas[p_dst] / 2.f) *
                                   ed.zoom.x),
                       origin.y + ((data.node_positions[p_dst][1] +
                                    data.node_areas[p_dst] / 2.f) *
                                   ed.zoom.y));

            draw_list->AddLine(src, dst, IM_COL32(255, 0, 0, 255), 1.0f);
        }

        if (ed.run_selection) {
            ed.end_selection = io.MousePos;

            if (ed.start_selection == ed.end_selection) {
                ed.selected_nodes.clear();
                ed.selected_edges.clear();
            } else {
                ImVec2 bmin{
                    std::min(ed.start_selection.x, io.MousePos.x),
                    std::min(ed.start_selection.y, io.MousePos.y),
                };

                ImVec2 bmax{
                    std::max(ed.start_selection.x, io.MousePos.x),
                    std::max(ed.start_selection.y, io.MousePos.y),
                };

                draw_list->AddRectFilled(bmin, bmax, IM_COL32(200, 0, 0, 127));
            }
        }

        draw_list->PopClipRect();
    }

    void center_camera(ImVec2 top_left,
                       ImVec2 bottom_right,
                       ImVec2 canvas_sz) noexcept
    {
        ImVec2 distance(bottom_right[0] - top_left[0],
                        bottom_right[1] - top_left[1]);
        ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                      (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

        ed.scrolling.x = ((-center.x * ed.zoom.x) + (canvas_sz.x / 2.f));
        ed.scrolling.y = ((-center.y * ed.zoom.y) + (canvas_sz.y / 2.f));
        ed.st          = graph_component_editor_data::status::none;
    }

    void auto_fit_camera(ImVec2 top_left,
                         ImVec2 bottom_right,
                         ImVec2 canvas_sz) noexcept
    {
        ImVec2 distance(bottom_right[0] - top_left[0],
                        bottom_right[1] - top_left[1]);
        ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                      (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

        ed.zoom.x      = canvas_sz.x / distance.x;
        ed.zoom.y      = canvas_sz.y / distance.y;
        ed.scrolling.x = ((-center.x * ed.zoom.x) + (canvas_sz.x / 2.f));
        ed.scrolling.y = ((-center.y * ed.zoom.y) + (canvas_sz.y / 2.f));
        ed.st          = graph_component_editor_data::status::none;
    }

    void show(application& app) noexcept
    {
        if (std::unique_lock lock(ed.mutex, std::try_to_lock);
            lock.owns_lock()) {
            debug::ensure(ed.st != status::updating);

            auto* compo = app.mod.components.try_to_get(ed.m_id);
            auto* graph = app.mod.graph_components.try_to_get(ed.graph_id);

            debug::ensure(compo);
            debug::ensure(graph);

            if (ImGui::CollapsingHeader("Settings",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {

                if (ImGui::Button(ed.automatic_layout ? "stop" : "start"))
                    ed.automatic_layout = not ed.automatic_layout;

                ImGui::SameLine();
                if (ImGui::Button("center"))
                    ed.st =
                      graph_component_editor_data::status::center_required;

                ImGui::SameLine();
                if (ImGui::Button("auto-fit"))
                    ed.st =
                      graph_component_editor_data::status::auto_fit_required;

                show_random_graph_type(*graph);
                show_random_graph_params(app, *graph);

                if (ed.automatic_layout) {
                    bool again = compute_automatic_layout(*graph);
                    if (not again) {
                        ed.iteration        = 0;
                        ed.automatic_layout = false;
                    }
                }
            }

            show_graph(app, *compo, ed, *graph);
            if (param_updated)
                ed.st = status::update_required;
        }
    }

    static void update_bound(graph_component&   graph,
                             std::integral auto i) noexcept
    {
        graph.top_left[0] = std::min(
          graph.top_left[0], graph.node_positions[i][0] - graph.node_areas[i]);
        graph.top_left[1] = std::min(
          graph.top_left[1], graph.node_positions[i][1] - graph.node_areas[i]);
        graph.bottom_right[0] =
          std::max(graph.bottom_right[0],
                   graph.node_positions[i][0] + graph.node_areas[i]);
        graph.bottom_right[1] =
          std::max(graph.bottom_right[1],
                   graph.node_positions[i][1] + graph.node_areas[i]);
    }

    static void update_position_to_grid(auto& graph, auto& graph_ed) noexcept
    {
        debug::ensure(not graph.nodes.empty());
        debug::ensure(graph.g_type != graph_component::graph_type::dot_file);

        const auto nb        = graph.nodes.size();
        const auto sqrt      = std::sqrt(nb);
        const auto line      = static_cast<unsigned>(sqrt);
        const auto col       = nb / line;
        const auto remaining = nb - (col * line);

        auto it = graph.nodes.begin();
        for (auto l = 0u; l < line; ++l) {
            for (auto c = 0u; c < col; ++c) {
                graph.node_positions[get_index(*it)][0] =
                  (graph_ed.distance.x + graph.node_areas[get_index(*it)]) * c;
                graph.node_positions[get_index(*it)][1] =
                  (graph_ed.distance.y + graph.node_areas[get_index(*it)]) * l;
                update_bound(graph, get_index(*it));
                ++it;
            }
        }

        for (auto r = 0u; r < remaining; ++r) {
            graph.node_positions[get_index(*it)][0] =
              (graph_ed.distance.x + graph.node_areas[get_index(*it)]) * r;
            graph.node_positions[get_index(*it)][1] =
              (graph_ed.distance.y + graph.node_areas[get_index(*it)]) * line;
            update_bound(graph, get_index(*it));
            ++it;
        }

        graph_ed.displacements.resize(graph.nodes.size());
        graph_ed.automatic_layout = true;
        graph_ed.iteration        = 0;
    }

    void start_update_task(application& app) noexcept
    {
        debug::ensure(ed.st == status::update_required);
        ed.st = graph_component_editor_data::status::updating;

        const auto id   = app.graphs.get_id(ed);
        const auto g_id = ed.graph_id;

        app.add_gui_task([&app, id, g_id]() {
            auto* graph_ed = app.graphs.try_to_get(id);
            auto* graph    = app.mod.graph_components.try_to_get(g_id);

            if (not graph_ed or not graph)
                return;

            std::scoped_lock lock(graph_ed->mutex);

            graph->update(app.mod);

            if (not graph->nodes.empty() and
                graph->g_type != graph_component::graph_type::dot_file)
                update_position_to_grid(*graph, *graph_ed);

            graph_ed->st = graph_component_editor_data::status::none;
        });
    }
};

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

void graph_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    graph_component_editor_data::impl impl{ *this };
    impl.show(app);

    if (st == status::update_required)
        impl.start_update_task(app);
}

void graph_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    if (auto* graph = app.mod.graph_components.try_to_get(graph_id)) {
        if (ImGui::CollapsingHeader("selected nodes")) {
            for (const auto id : selected_nodes) {
                if (graph->nodes.exists(id)) {
                    const auto idx = get_index(id);
                    ImGui::PushID(idx);

                    ImGui::LabelFormat("name", "{}", graph->node_names[idx]);
                    ImGui::LabelFormat("ids", "{}", graph->node_ids[idx]);
                    ImGui::LabelFormat("position",
                                       "{}x{}",
                                       graph->node_positions[idx][0],
                                       graph->node_positions[idx][1]);
                    ImGui::LabelFormat("area", "{}", graph->node_areas[idx]);

                    if (auto newid = graph->node_components[idx];
                        app.component_sel.combobox("component", &newid))
                        graph->node_components[idx] = newid;
                }

                ImGui::PopID();
            }
        }

        if (auto newid = undefined<component_id>();
            app.component_sel.combobox("apply for all", &newid)) {
            for (const auto id : selected_nodes) {
                if (graph->nodes.exists(id)) {
                    const auto idx              = get_index(id);
                    graph->node_components[idx] = newid;
                }
            }
        }
    }
}

bool graph_component_editor_data::need_show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    return false;
}

void graph_component_editor_data::clear_selected_nodes() noexcept
{
    selected_edges.clear();
    selected_nodes.clear();
}

} // namespace irt
