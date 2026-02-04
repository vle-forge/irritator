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

static auto dir_path_combobox(const modeling::file_access& fs,
                              dir_path_id&                 dir_id) -> bool
{
    const auto* dir         = fs.dir_paths.try_to_get(dir_id);
    const auto* preview_dir = dir ? dir->path.c_str() : "-";
    const auto  old_dir_id  = dir_id;

    if (ImGui::BeginCombo("Directory", preview_dir)) {
        if (ImGui::Selectable("-", dir == nullptr)) {
            dir_id = undefined<dir_path_id>();
        }

        auto local_id = 0;
        for (const auto& elem : fs.dir_paths) {
            const auto elem_id = fs.dir_paths.get_id(elem);
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

static auto dot_file_combobox(const modeling::file_access& fs,
                              const dir_path&              dir,
                              file_path_id& file_id) noexcept -> bool
{
    const auto* file         = fs.file_paths.try_to_get(file_id);
    const auto* preview_file = file ? file->path.c_str() : "-";
    const auto  old_file_id  = file_id;

    if (ImGui::BeginCombo("Dot file", preview_file)) {
        if (ImGui::Selectable("-", file == nullptr)) {
            file_id = undefined<file_path_id>();
        }

        int local_id = 0;
        for (const auto elem_id : dir.children) {
            if (const auto* elem = fs.file_paths.try_to_get(elem_id);
                elem and elem->type == file_path::file_type::dot_file) {
                ImGui::PushID(++local_id);

                if (ImGui::Selectable(elem->path.c_str(), file_id == elem_id))
                    file_id = elem_id;

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

    return mod.files.read([&](const auto& fs, const auto /*vers*/) {
        dir_path_combobox(fs, dir_id);
        if (const auto* dir = fs.dir_paths.try_to_get(dir_id))
            dot_file_combobox(fs, *dir, file_id);

        return not(old_dir_id == dir_id and old_file_id == file_id);
    });
}

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
  graph_component& graph) noexcept
{
    if (iteration == 0)
        update_position_to_grid(graph);

    const auto size = graph.g.nodes.size();
    if (size < 2)
        return false;

    displacements.resize(graph.g.nodes.size());

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

void graph_component_editor_data::show_graph(application& app,
                                             component& /*compo*/,
                                             graph_component& data) noexcept
{
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    canvas_sz        = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(canvas_p0,
                       canvas_p1,
                       to_ImU32(app.config.colors[style_color::outer_border]));

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
                        data.g.alloc_edge(selected_nodes[i], selected_nodes[j]);
                    }
                }
                selected_nodes.clear();
            }

            if (not selected_nodes.empty() and
                ImGui::MenuItem("Delete nodes")) {
                for (auto id : selected_nodes) {
                    if (data.g.nodes.exists(id))
                        data.g.nodes.free(id);
                }
                selected_nodes.clear();
            }

            if (not selected_edges.empty() and
                ImGui::MenuItem("Delete edges")) {
                for (auto id : selected_edges)
                    if (data.g.edges.exists(id))
                        data.g.edges.free(id);
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

    for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(
          ImVec2(canvas_p0.x + x, canvas_p0.y),
          ImVec2(canvas_p0.x + x, canvas_p1.y),
          to_ImU32(app.config.colors[style_color::inner_border]));

    for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(
          ImVec2(canvas_p0.x, canvas_p0.y + y),
          ImVec2(canvas_p1.x, canvas_p0.y + y),
          to_ImU32(app.config.colors[style_color::inner_border]));

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

        draw_list->AddRect(
          p_min,
          p_max,
          to_ImU32(app.config.colors[style_color::node_active]),
          0.f,
          0,
          4.f);
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

        draw_list->AddLine(
          src, dst, to_ImU32(app.config.colors[style_color::edge]), 1.f);
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

        draw_list->AddLine(
          src,
          dst,
          to_ImU32(app.config.colors[style_color::edge_active]),
          1.0f);
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

            draw_list->AddRectFilled(
              bmin,
              bmax,
              to_ImU32(app.config.colors[style_color::background_selection]));
        }
    }

    draw_list->PopClipRect();
}

void graph_component_editor_data::center_camera(ImVec2 top_left,
                                                ImVec2 bottom_right,
                                                ImVec2 canvas_sz) noexcept
{
    ImVec2 distance(bottom_right[0] - top_left[0],
                    bottom_right[1] - top_left[1]);
    ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                  (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

    scrolling.x = ((-center.x * zoom.x) + (canvas_sz.x / 2.f));
    scrolling.y = ((-center.y * zoom.y) + (canvas_sz.y / 2.f));

    st = graph_component_editor_data::job::none;
}

void graph_component_editor_data::auto_fit_camera(ImVec2 top_left,
                                                  ImVec2 bottom_right,
                                                  ImVec2 canvas_sz) noexcept
{
    ImVec2 distance(bottom_right[0] - top_left[0],
                    bottom_right[1] - top_left[1]);
    ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                  (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

    zoom.x = canvas_sz.x / distance.x;
    zoom.y = canvas_sz.y / distance.y;

    zoom.x = almost_equal(zoom.x, 0.f, 1e-3F) ? 1.f : zoom.x;
    zoom.y = almost_equal(zoom.y, 0.f, 1e-3F) ? 1.f : zoom.y;

    scrolling.x = ((-center.x * zoom.x) + (canvas_sz.x / 2.f));
    scrolling.y = ((-center.y * zoom.y) + (canvas_sz.y / 2.f));

    st = graph_component_editor_data::job::none;
}

void graph_component_editor_data::show_scale_free_menu(
  application&     app,
  graph_component& graph) noexcept
{
    if (ImGui::BeginMenu("Generate scale free graph")) {
        HelpMarker("scale_free: graph typically has a very skewed degree "
                   "distribution, where few vertices have a very high "
                   "degree and a large number of vertices have a very "
                   "small degree. Many naturally evolving networks, such as "
                   "the World Wide Web, are scale-free graphs, making these "
                   "graphs a good model for certain networking.");

        if (ImGui::InputInt("size", &psf.nodes))
            psf.nodes = std::clamp(psf.nodes, 1, graph_component::children_max);

        if (ImGui::InputDouble("alpha", &psf.alpha))
            psf.alpha = std::clamp(psf.alpha, 0.0, 1000.0);

        if (ImGui::InputDouble("beta", &psf.beta))
            psf.beta = std::clamp(psf.beta, 0.0, 1000.0);

        if (auto r = app.component_sel.combobox("component", psf.id); r)
            psf.id = r.id;

        if (ImGui::Button("generate")) {
            clear_selected_nodes();
            graph.g.clear();
            graph.g_type      = graph_component::graph_type::scale_free;
            graph.param.scale = psf;
            st                = job::build_scale_free_required;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }
}

void graph_component_editor_data::show_small_world_menu(
  application&     app,
  graph_component& graph) noexcept
{
    if (ImGui::BeginMenu("Generate Small world graph")) {
        HelpMarker(
          "small_world: consists of a ring graph (where each vertex is "
          "connected to its k nearest neighbors).Edges in the graph are "
          "randomly rewired to different vertices with a probability p.");

        if (ImGui::InputInt("size", &psw.nodes))
            psw.nodes = std::clamp(psw.nodes, 1, graph_component::children_max);

        if (ImGui::InputDouble("probability", &psw.probability))
            psw.probability = std::clamp(psw.probability, 0.0, 1.0);

        if (ImGui::InputInt("k", &psw.k, 1, 2))
            psw.k = std::clamp(psw.k, 1, 8);

        if (auto r = app.component_sel.combobox("component", psw.id); r)
            psw.id = r.id;

        if (ImGui::Button("generate")) {
            clear_selected_nodes();
            graph.g.clear();
            graph.g_type      = graph_component::graph_type::small_world;
            graph.param.small = psw;
            st                = job::build_small_world_required;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }
}

void graph_component_editor_data::show_dot_file_menu(
  application&     app,
  graph_component& graph) noexcept
{
    if (ImGui::BeginMenu("Read Dot graph")) {
        HelpMarker("dot: consists of a file defining the graph "
                   "according to the dot file format.");

        dot_combobox_selector(app.mod, pdf.dir, pdf.file);

        if (is_defined(pdf.dir) and is_defined(pdf.file)) {
            if (ImGui::Button("Load")) {
                clear_selected_nodes();
                graph.g.clear();
                graph.g_type    = graph_component::graph_type::dot_file;
                graph.param.dot = pdf;
                st              = job::build_dot_graph_required;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndMenu();
    }
}

void graph_component_editor_data::graph_component_editor_data::show(
  application& app) noexcept
{
    if (std::unique_lock lock(mutex, std::try_to_lock); lock.owns_lock()) {

        auto* compo = app.mod.components.try_to_get<component>(m_id);
        auto* graph = app.mod.graph_components.try_to_get(graph_id);

        debug::ensure(compo);
        debug::ensure(graph);

        static bool show_save = false;

        ImGui::BeginDisabled(running.test());

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Model")) {
                show_scale_free_menu(app, *graph);
                show_small_world_menu(app, *graph);
                show_dot_file_menu(app, *graph);
                ImGui::Separator();
                if (ImGui::MenuItem("Save")) {
                    show_save = true;
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(
                  center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Display##Menu")) {
                if (ImGui::MenuItem("Center"))
                    st = graph_component_editor_data::job::center_required;
                if (ImGui::MenuItem("Auto-fit"))
                    st = graph_component_editor_data::job::auto_fit_required;

                float wzoom[2] = { zoom.x, zoom.y };
                if (ImGui::MenuItem("Reset zoom")) {
                    zoom.x = 1.0f;
                    zoom.y = 1.0f;
                }

                if (ImGui::InputFloat2("Zoom x,y", wzoom)) {
                    zoom.x = ImClamp(wzoom[0], 0.1f, 1000.f);
                    zoom.y = ImClamp(wzoom[1], 0.1f, 1000.f);
                }

                if (ImGui::Checkbox("Automatic layout", &automatic_layout)) {
                    if (automatic_layout)
                        iteration = 0;
                }

                float wdistance[2] = { distance.x, distance.y };
                if (ImGui::InputFloat2("Distance between node", wdistance)) {
                    distance.x = ImClamp(wdistance[0], 0.1f, 100.f);
                    distance.y = ImClamp(wdistance[0], 0.1f, 100.f);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Connectivity")) {
                show_project_params_menu(*graph);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::EndDisabled();

        if (show_save)
            ImGui::OpenPopup("Save dot graph");

        if (ImGui::BeginPopupModal("Save dot graph",
                                   &show_save,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto can_save =
              file_path_selector(app,
                                 file_path_selector_option::force_dot_extension,
                                 reg,
                                 dir,
                                 file);

            ImGui::BeginDisabled(can_save == false);

            if (ImGui::Button("Save")) {
                if (auto f = make_file(app.mod, file); f.has_value()) {
                    if (write_dot_file(app.mod, graph->g, *f)) {
                        graph->g_type = graph_component::graph_type::dot_file;
                        graph->param  = { .dot = { .dir = dir, .file = file } };
                    } else {
                        app.add_gui_task([&app, id = file]() {
                            app.mod.files.write(
                              [&](auto& fs) { fs.remove_file(id); });
                        });

                        clear_file_access();

                        app.jn.push(
                          log_level::error,
                          [](auto& t, auto& m, const auto& f) {
                              t = "Fail to save dot file";
                              format(m,
                                     "{}",
                                     reinterpret_cast<const char*>(f.c_str()));
                          },
                          *f);
                    }
                } else {
                    app.add_gui_task([&app, id = file]() {
                        app.mod.files.write(
                          [&](auto& fs) { fs.remove_file(id); });
                    });

                    clear_file_access();
                }
                show_save = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndDisabled();
            ImGui::SameLine();

            if (ImGui::Button("Close")) {
                show_save = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (automatic_layout and not graph->g.nodes.empty()) {
            bool again = compute_automatic_layout(*graph);
            if (not again) {
                iteration        = 0;
                automatic_layout = false;
            }
        }

        show_graph(app, *compo, *graph);
    }
}

void graph_component_editor_data::update_position_to_grid(
  graph_component& graph) noexcept
{
    graph.assign_grid_position(distance.x, distance.y);
    displacements.resize(graph.g.nodes.size());
    automatic_layout = true;
    iteration        = 0;
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
  , st(job::auto_fit_required)
{}

void graph_component_editor_data::clear() noexcept
{
    graph_id = undefined<graph_component_id>();
    m_id     = undefined<component_id>();
}

void graph_component_editor_data::show(component_editor& ed) noexcept
{
    if (ImGui::BeginChild("##graph-ed",
                          ImVec2(0, 0),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_MenuBar)) {
        auto& app = container_of(&ed, &application::component_ed);

        show(app);

        if (st != job::none) {
            scoped_flag_run(running, [&]() {
                const auto g_id = graph_id;

                switch (st) {
                case job::none:
                    break;

                case job::center_required:
                    if (auto* graph_compo =
                          app.mod.graph_components.try_to_get(g_id)) {
                        center_camera(
                          ImVec2(graph_compo->top_left_limit[0],
                                 graph_compo->top_left_limit[1]),
                          ImVec2(graph_compo->bottom_right_limit[0],
                                 graph_compo->bottom_right_limit[1]),
                          canvas_sz);
                    }
                    st = job::none;
                    break;

                case job::auto_fit_required:
                    if (auto* graph_compo =
                          app.mod.graph_components.try_to_get(g_id)) {
                        auto_fit_camera(
                          ImVec2(graph_compo->top_left_limit[0],
                                 graph_compo->top_left_limit[1]),
                          ImVec2(graph_compo->bottom_right_limit[0],
                                 graph_compo->bottom_right_limit[1]),
                          canvas_sz);
                    }
                    st = job::none;
                    break;

                case job::build_scale_free_required:
                    scale_free_builder = { .ed       = &ed,
                                           .graph_ed = this,
                                           .g_id     = graph_id };

                    app.add_gui_task([&builder = scale_free_builder]() {
                        auto& app =
                          container_of(builder.ed, &application::component_ed);
                        auto& graph_compo =
                          app.mod.graph_components.get(builder.g_id);

                        if (builder.graph_ed->g.init_scale_free_graph(
                              builder.graph_ed->psf.alpha,
                              builder.graph_ed->psf.beta,
                              builder.graph_ed->psf.id,
                              builder.graph_ed->psf.nodes,
                              graph_compo.rng)) {
                            std::scoped_lock lock(builder.graph_ed->mutex);
                            builder.graph_ed->g.swap(graph_compo.g);
                            graph_compo.param.scale = builder.graph_ed->psf;
                            graph_compo.g_type =
                              graph_component::graph_type::scale_free;

                            if (not graph_compo.g.nodes.empty()) {
                                builder.graph_ed->update_position_to_grid(
                                  graph_compo);
                                graph_compo.update_position();
                                builder.graph_ed->psf.reset();
                            }
                            builder.graph_ed->st = job::auto_fit_required;
                        }
                    });
                    break;

                case job::build_small_world_required:
                    app.add_gui_task([&builder = scale_free_builder]() {
                        auto& app =
                          container_of(builder.ed, &application::component_ed);
                        auto& graph_compo =
                          app.mod.graph_components.get(builder.g_id);

                        if (builder.graph_ed->g.init_small_world_graph(
                              builder.graph_ed->psw.probability,
                              builder.graph_ed->psw.k,
                              builder.graph_ed->psw.id,
                              builder.graph_ed->psw.nodes,
                              graph_compo.rng)) {

                            std::scoped_lock lock(builder.graph_ed->mutex);
                            builder.graph_ed->g.swap(graph_compo.g);
                            graph_compo.param.small = builder.graph_ed->psw;
                            graph_compo.g_type =
                              graph_component::graph_type::small_world;

                            if (not graph_compo.g.nodes.empty()) {
                                builder.graph_ed->update_position_to_grid(
                                  graph_compo);
                                graph_compo.update_position();
                                builder.graph_ed->psw.reset();
                            }
                        }
                        builder.graph_ed->st = job::auto_fit_required;
                    });
                    break;

                case job::build_dot_graph_required:
                    app.add_gui_task([&builder = scale_free_builder]() {
                        auto& app =
                          container_of(builder.ed, &application::component_ed);
                        auto& graph_compo =
                          app.mod.graph_components.get(builder.g_id);
                        auto id = app.mod.files.read(
                          [&](const auto& fs,
                              const auto /*vers*/) -> irt::graph_id {
                              if (const auto* f = fs.file_paths.try_to_get(
                                    builder.graph_ed->pdf.file)) {
                                  return f->g_id;
                              }

                              return undefined<irt::graph_id>();
                          });

                        if (const auto* g_glob =
                              app.mod.graphs.try_to_get(id)) {
                            std::scoped_lock lock(builder.graph_ed->mutex);
                            builder.graph_ed->g.swap(graph_compo.g);
                            graph_compo.g         = *g_glob;
                            graph_compo.param.dot = builder.graph_ed->pdf;
                            graph_compo.g_type =
                              graph_component::graph_type::dot_file;
                            builder.graph_ed->pdf.reset();

                            if (not graph_compo.g.nodes.empty()) {
                                graph_compo.update_position();
                            }
                        }

                        builder.graph_ed->st = job::auto_fit_required;
                    });
                    break;
                }
            });
        }

        ImGui::EndChild();
    }
}

void graph_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* graph = app.mod.graph_components.try_to_get(graph_id)) {
        if (ImGui::TreeNodeEx("selected nodes")) {
            for (const auto id : selected_nodes) {
                if (graph->g.nodes.exists(id)) {
                    const auto idx = get_index(id);
                    ImGui::PushID(idx);

                    name_str name = graph->g.node_names[idx];
                    if (ImGui::InputFilteredString("name", name)) {
                        graph->g.node_names[idx] =
                          graph->g.buffer.append(name.sv());
                    }

                    name_str id_str = graph->g.node_ids[idx];
                    if (ImGui::InputFilteredString("id", id_str)) {
                        graph->g.node_ids[idx] =
                          graph->g.buffer.append(id_str.sv());
                    }

                    ImGui::LabelFormat("position",
                                       "{}x{}",
                                       graph->g.node_positions[idx][0],
                                       graph->g.node_positions[idx][1]);

                    if (auto area = graph->g.node_areas[idx];
                        ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
                        graph->g.node_areas[idx] = area;
                    }

                    if (auto r = app.component_sel.combobox(
                          "component", graph->g.node_components[idx]);
                        r)
                        graph->g.node_components[idx] = r.id;

                    if (auto* compo = app.mod.components.try_to_get<component>(
                          graph->g.node_components[idx])) {
                        if (not compo->x.empty()) {
                            if (ImGui::TreeNodeEx("input ports")) {
                                const auto& xnames = compo->x.get<port_str>();
                                for (const auto id : compo->x) {
                                    ImGui::TextFormat("{}", xnames[id].sv());
                                }
                                ImGui::TreePop();
                            }
                        }

                        if (not compo->y.empty()) {
                            if (ImGui::TreeNodeEx("output ports")) {
                                const auto& ynames = compo->y.get<port_str>();
                                for (const auto id : compo->y) {
                                    ImGui::TextFormat("{}", ynames[id].sv());
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx("apply for all selected")) {
            if (auto r = app.component_sel.combobox("component",
                                                    undefined<component_id>());
                r) {
                for (const auto id : selected_nodes) {
                    if (graph->g.nodes.exists(id)) {
                        graph->g.node_components[id] = r.id;
                    }
                }
            }

            static auto area = 1.f;
            if (ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
                for (const auto id : selected_nodes) {
                    if (graph->g.nodes.exists(id)) {
                        graph->g.node_areas[id] = area;
                    }
                }
            }

            ImGui::TreePop();
        }
    }
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

} // namespace irt
