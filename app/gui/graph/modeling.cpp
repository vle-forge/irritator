// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "fmt/core.h"
#include "internal.hpp"
#include "irritator/format.hpp"

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace irt {

static const char* random_graph_type_names[] = { "dot-file",
                                                 "scale-free",
                                                 "small-world" };

static bool compute_automatic_layout(const graph_component& graph,
                                     const int              iteration,
                                     const int              iteration_limit,
                                     const ImVec2           vertex_distance,
                                     vector<ImVec2>&        positions,
                                     vector<ImVec2>& displacements) noexcept
{
    const auto size = graph.children.size();
    if (size < 2)
        return false;

    const auto sqrt_size = std::sqrt(static_cast<float>(size));
    const auto W         = (sqrt_size + 1.f) * vertex_distance.x;
    const auto L         = (sqrt_size + 1.f) * vertex_distance.x;
    const auto area      = W * L;
    const auto k_square  = area / static_cast<float>(size);
    const auto k         = std::sqrt(k_square);

    const auto t =
      (1.f -
       (static_cast<float>(iteration) / static_cast<float>(iteration_limit))) *
      (1.f -
       (static_cast<float>(iteration) / static_cast<float>(iteration_limit)));

    for (const auto& i_v : graph.edges) {
        const auto v       = get_index(i_v.v);
        displacements[v].x = 0.f;
        displacements[v].y = 0.f;

        for (const auto& i_u : graph.edges) {
            const auto u = get_index(i_u.u);

            if (u != v) {
                const auto delta = ImVec2{ positions[v].x - positions[u].x,
                                           positions[v].y - positions[u].y };

                if (delta.x && delta.y) {
                    const float d2    = delta.x * delta.x + delta.y * delta.y;
                    const float coeff = k_square / d2;

                    displacements[v].x += std::clamp(
                      coeff * delta.x, -vertex_distance.x, vertex_distance.x);
                    displacements[v].y += std::clamp(
                      coeff * delta.y, -vertex_distance.y, vertex_distance.y);
                }
            }
        }
    }

    for (const auto& edge : graph.edges) {
        const auto  u  = get_index(edge.u);
        const auto  v  = get_index(edge.v);
        const float dx = positions[v].x - positions[u].x;
        const float dy = positions[v].y - positions[u].y;

        if (dx && dy) {
            const float coeff = std::sqrt((dx * dx) / (dy * dy)) / k;

            auto move_v_x = dx * coeff;
            auto move_v_y = dy * coeff;
            auto move_u_x = dx * coeff;
            auto move_u_y = dy * coeff;

            displacements[v].x -=
              std::clamp(move_v_x, -vertex_distance.x, vertex_distance.x);
            displacements[v].y -=
              std::clamp(move_v_y, -vertex_distance.y, vertex_distance.y);
            displacements[u].x +=
              std::clamp(move_u_x, -vertex_distance.x, vertex_distance.x);
            displacements[u].y +=
              std::clamp(move_u_y, -vertex_distance.y, vertex_distance.y);
        }
    }

    bool have_big_displacement = false;
    for (const auto& edge : graph.edges) {
        const auto  v = get_index(edge.v);
        const float d = std::sqrt((displacements[v].x * displacements[v].x) +
                                  (displacements[v].y * displacements[v].y));

        if (d > t) {
            const float coeff = t / d;
            displacements[v].x *= coeff;
            displacements[v].y *= coeff;
        }

        if (displacements[v].x > 10.f or displacements[v].y > 10.f)
            have_big_displacement = true;

        positions[v].x += displacements[v].x;
        positions[v].y += displacements[v].y;
    }

    return have_big_displacement or iteration < iteration_limit;
}

static void update_position_to_grid(const ImVec2    vertex_distance,
                                    const ImVec2    vertex_size,
                                    vector<ImVec2>& positions) noexcept
{
    debug::ensure(positions.ssize() > 0);

    const auto size   = positions.ssize();
    const auto raw_sq = std::sqrt(static_cast<float>(size));
    const auto sq     = ImFloor(raw_sq);

    auto i = 0;
    auto y = 0.f;

    for (auto x = 0.f; x < sq; ++x) {
        for (y = 0.f; y < sq; ++i, ++y) {
            positions[i].x = x * (vertex_size.x + vertex_distance.x);
            positions[i].y = y * (vertex_size.y + vertex_distance.y);
        }
    }

    for (auto x = 0.f; i < size; ++i, ++x) {
        positions[i].x = x * (vertex_size.x + vertex_distance.x);
        positions[i].y = y * (vertex_size.y + vertex_distance.y);
    }
}

static std::pair<bool, int> show_size_widget(
  graph_component_editor_data& /*graph_ed*/,
  graph_component& graph) noexcept
{
    auto size = graph.children.ssize();

    if (ImGui::InputInt(
          "size", &size, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
        size = std::clamp(size, 1, graph_component::children_max);
        if (size != graph.children.ssize())
            return std::make_pair(true, size);
    }

    return std::make_pair(false, size);
}

// constexpr inline static auto get_default_component_id(
//   const vector<component_id>& g) noexcept -> component_id
// {
//     return g.empty() ? undefined<component_id>() : g.front();
// }

static bool show_random_graph_type(graph_component& graph) noexcept
{
    bool is_changed = false;
    auto current    = static_cast<int>(graph.param.index());

    if (ImGui::Combo("type",
                     &current,
                     random_graph_type_names,
                     length(random_graph_type_names))) {
        if (current != static_cast<int>(graph.param.index())) {
            switch (current) {
            case 0:
                graph.param = graph_component::dot_file_param{};
                is_changed  = true;
                break;

            case 1:
                graph.param = graph_component::scale_free_param{};
                is_changed  = true;
                break;

            case 2:
                graph.param = graph_component::small_world_param{};
                is_changed  = true;
                break;
            }
        }
    }

    ImGui::SameLine();
    HelpMarker(
      "scale_free: graph typically has a very skewed degree distribution, "
      "where few vertices have a very high degree and a large number of "
      "vertices have a very small degree. Many naturally evolving networks, "
      "such as the World Wide Web, are scale-free graphs, making these "
      "graphs "
      "a good model for certain networking problems.\n\n"
      "small_world: consists of a ring graph (where each vertex is connected "
      "to its k nearest neighbors) .Edges in the graph are randomly rewired "
      "to "
      "different vertices with a probability p.");

    return is_changed;
}

// void task_read_dot_file(void* param) noexcept
// {
//     auto* g_task  = reinterpret_cast<gui_task*>(param);
//     g_task->state = task_status::started;

//     [[maybe_unused]] auto graph_id =
//       enum_cast<graph_component_id>(g_task->param_1);
//     [[maybe_unused]] auto dir_id = enum_cast<dir_path_id>(g_task->param_2);
//     //[[maybe_unused]] auto file_id =
//     enum_cast<dir_path_id>(g_task->param_3);

//     g_task->state = task_status::finished;
// };

static bool show_random_graph_params(application&     app,
                                     modeling&        mod,
                                     graph_component& graph) noexcept
{
    bool is_changed = false;

    switch (graph.param.index()) {
    case 0: {
        auto* param =
          std::get_if<graph_component::dot_file_param>(&graph.param);

        auto* dir = mod.dir_paths.try_to_get(param->dir);
        if (not dir) {
            param->file = undefined<file_path_id>();
            param->dir  = undefined<dir_path_id>();
        }

        auto* file = mod.file_paths.try_to_get(param->file);
        if (not file)
            param->file = undefined<file_path_id>();

        {
            const char* preview_value = dir ? dir->path.c_str() : "undefined";
            if (ImGui::BeginCombo("dir", preview_value)) {
                for (const auto& elem : mod.dir_paths) {
                    const auto elem_id     = mod.dir_paths.get_id(elem);
                    const auto is_selected = elem_id == param->dir;

                    const auto is_lock =
                      dir != nullptr and dir_path::state::lock == dir->status;

                    if (is_lock) {
                        if (ImGui::Selectable(elem.path.c_str(), is_selected)) {
                            param->dir  = elem_id;
                            param->file = undefined<file_path_id>();
                            is_changed  = true;
                        }

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    } else {
                        ImGui::SetNextItemAllowOverlap();
                        if (ImGui::Selectable(elem.path.c_str(), is_selected)) {
                            param->dir  = elem_id;
                            param->file = undefined<file_path_id>();
                            is_changed  = true;
                        }

                        ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                                        ImGui::GetStyle().ItemSpacing.x - 20.f);

                        ImGui::BeginDisabled(is_lock);
                        if (ImGui::SmallButton("R"))
                            app.start_dir_path_refresh(elem_id);
                        ImGui::EndDisabled();

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (dir) {
            const char* preview_value = file ? file->path.c_str() : "undefined";
            if (ImGui::BeginCombo("dot file", preview_value)) {
                for (const auto elem_id : dir->children)
                    if (const auto* f = mod.file_paths.try_to_get(elem_id);
                        f and f->type == file_path::file_type::dot_file) {
                        const auto is_selected = elem_id == param->file;

                        if (ImGui::Selectable(f->path.c_str(), is_selected)) {
                            param->file = elem_id;
                            is_changed  = true;
                        }

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                ImGui::EndCombo();
            }
        }
    } break;

    case 1: {
        auto* param =
          std::get_if<graph_component::scale_free_param>(&graph.param);

        auto alpha = param->alpha;
        auto beta  = param->beta;

        if (ImGui::InputDouble("alpha", &alpha)) {
            param->alpha = std::clamp(alpha, 0.0, 1000.0);
            is_changed   = true;
        }
        if (ImGui::InputDouble("beta", &beta)) {
            param->beta = std::clamp(beta, 0.0, 1000.0);
            is_changed  = true;
        }
    } break;

    case 2: {
        auto* param =
          std::get_if<graph_component::small_world_param>(&graph.param);

        auto probability = param->probability;
        auto k           = param->k;

        if (ImGui::InputDouble("probability", &probability)) {
            param->probability = std::clamp(probability, 0.0, 1.0);
            is_changed         = true;
        }

        if (ImGui::InputInt(
              "k", &k, 1, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
            is_changed = true;
            param->k   = std::clamp(k, 1, 8);
        }
    } break;
    }

    return is_changed;
}

static bool show_default_component_widgets(graph_component_editor_data& ed,
                                           application&                 app,
                                           graph_component& graph) noexcept
{
    bool is_changed = false;

    if (show_random_graph_type(graph))
        is_changed = true;

    if (show_random_graph_params(app, app.mod, graph))
        is_changed = true;

    if (app.component_sel.combobox("Default component", &ed.selected_id)) {
        for (auto& vertex : graph.children)
            vertex.id = ed.selected_id;

        is_changed = true;
    }

    return is_changed;
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

static void show_graph(application& app,
                       component& /*compo*/,
                       graph_component_editor_data& ed,
                       graph_component&             data) noexcept
{
    if (ImGui::InputFloat2("Width and height of vertex", ed.zoom)) {
        ed.zoom[0] = ImClamp(ed.zoom[0], 0.1f, 10.f);
        ed.zoom[1] = ImClamp(ed.zoom[1], 0.1f, 10.f);
    }

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    ImGuiIO&    io        = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

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
        ed.zoom[0] = ed.zoom[0] + (io.MouseWheel * ed.zoom[0] * 0.1f);
        ed.zoom[1] = ed.zoom[1] + (io.MouseWheel * ed.zoom[1] * 0.1f);
        ed.zoom[0] = ImClamp(ed.zoom[0], 0.1f, 10.f);
        ed.zoom[1] = ImClamp(ed.zoom[1], 0.1f, 10.f);
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
                for (auto id : ed.selected_nodes)
                    if (auto* vertex = data.children.try_to_get(id); vertex)
                        data.children.free(*vertex);
                ed.selected_nodes.clear();
            }

            if (not ed.selected_edges.empty() and
                ImGui::MenuItem("Delete selected edges?")) {
                for (auto id : ed.selected_edges)
                    if (auto* edge = data.edges.try_to_get(id); edge)
                        data.edges.free(*edge);
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

                for (auto& vertex : data.children) {
                    const auto id = data.children.get_id(vertex);
                    const auto i  = get_index(id);

                    ImVec2 p_min(origin.x + (ed.positions[i].x * ed.zoom[0]),
                                 origin.y + (ed.positions[i].y * ed.zoom[1]));

                    ImVec2 p_max(
                      origin.x + ((ed.positions[i].x + ed.size.x) * ed.zoom[0]),
                      origin.y +
                        ((ed.positions[i].y + ed.size.y) * ed.zoom[1]));

                    if (p_min.x >= bmin.x and p_max.x < bmax.x and
                        p_min.y >= bmin.y and p_max.y < bmax.y) {
                        ed.selected_nodes.emplace_back(id);
                    }
                }

                for (auto& edge : data.edges) {
                    auto* us = data.children.try_to_get(edge.u);
                    auto* vs = data.children.try_to_get(edge.v);

                    if (us and vs) {
                        ImVec2 p1(
                          origin.x + ((ed.positions[get_index(edge.u)].x +
                                       ed.size.x / 2.f) *
                                      ed.zoom[0]),
                          origin.y + ((ed.positions[get_index(edge.u)].y +
                                       ed.size.y / 2.f) *
                                      ed.zoom[1]));

                        ImVec2 p2(
                          origin.x + ((ed.positions[get_index(edge.v)].x +
                                       ed.size.x / 2.f) *
                                      ed.zoom[0]),
                          origin.y + ((ed.positions[get_index(edge.v)].y +
                                       ed.size.y / 2.f) *
                                      ed.zoom[1]));

                        if (is_line_intersects_box(p1, p2, bmin, bmax)) {
                            ed.selected_edges.emplace_back(
                              data.edges.get_id(edge));
                        }
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

    for (auto& vertex : data.children) {
        const auto id = data.children.get_id(vertex);
        const auto i  = get_index(id);

        ImVec2 p_min(origin.x + (ed.positions[i].x * ed.zoom[0]),
                     origin.y + (ed.positions[i].y * ed.zoom[1]));

        ImVec2 p_max(origin.x + ((ed.positions[i].x + ed.size.x) * ed.zoom[0]),
                     origin.y + ((ed.positions[i].y + ed.size.y) * ed.zoom[1]));

        draw_list->AddRectFilled(
          p_min,
          p_max,
          to_ImU32(app.mod.component_colors[get_index(vertex.id)]));
    }

    for_specified_data(data.children, ed.selected_nodes, [&](auto& vertex) {
        const auto id = data.children.get_id(vertex);
        const auto i  = get_index(id);

        ImVec2 p_min(origin.x + (ed.positions[i].x * ed.zoom[0]),
                     origin.y + (ed.positions[i].y * ed.zoom[1]));

        ImVec2 p_max(origin.x + ((ed.positions[i].x + ed.size.x) * ed.zoom[0]),
                     origin.y + ((ed.positions[i].y + ed.size.y) * ed.zoom[1]));

        draw_list->AddRect(
          p_min, p_max, IM_COL32(255, 255, 255, 255), 0.f, 0, 4.f);
    });

    for (const auto& edge : data.edges) {
        auto* u_c = data.children.try_to_get(edge.u);
        auto* v_c = data.children.try_to_get(edge.v);

        if (u_c == nullptr or v_c == nullptr)
            continue;

        const auto p_src = get_index(edge.u);
        const auto p_dst = get_index(edge.v);

        ImVec2 src(
          origin.x + ((ed.positions[p_src].x + ed.size.x / 2.f) * ed.zoom[0]),
          origin.y + ((ed.positions[p_src].y + ed.size.y / 2.f) * ed.zoom[1]));

        ImVec2 dst(
          origin.x + ((ed.positions[p_dst].x + ed.size.x / 2.f) * ed.zoom[0]),
          origin.y + ((ed.positions[p_dst].y + ed.size.y / 2.f) * ed.zoom[1]));

        draw_list->AddLine(src, dst, IM_COL32(200, 200, 200, 40), 1.0f);
    }

    for_specified_data(data.edges, ed.selected_edges, [&](auto& edge) noexcept {
        auto* u_c = data.children.try_to_get(edge.u);
        auto* v_c = data.children.try_to_get(edge.v);

        if (u_c == nullptr or v_c == nullptr)
            return;

        const auto p_src = get_index(edge.u);
        const auto p_dst = get_index(edge.v);

        ImVec2 src(
          origin.x + ((ed.positions[p_src].x + ed.size.x / 2.f) * ed.zoom[0]),
          origin.y + ((ed.positions[p_src].y + ed.size.y / 2.f) * ed.zoom[1]));

        ImVec2 dst(
          origin.x + ((ed.positions[p_dst].x + ed.size.x / 2.f) * ed.zoom[0]),
          origin.y + ((ed.positions[p_dst].y + ed.size.y / 2.f) * ed.zoom[1]));

        draw_list->AddLine(src, dst, IM_COL32(255, 0, 0, 255), 1.0f);
    });

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
    auto& app   = container_of(&ed, &application::component_ed);
    auto* compo = app.mod.components.try_to_get(m_id);
    auto* graph = app.mod.graph_components.try_to_get(graph_id);

    debug::ensure(compo);
    debug::ensure(graph);

    if (positions.empty())
        positions.resize(graph->children.capacity());

    if (displacements.empty())
        positions.resize(graph->children.capacity());

    ImGui::TextFormatDisabled("graph-editor-data size: {}",
                              graph->children.size());
    ImGui::TextFormatDisabled("edges: {}", graph->edges.size());
    ImGui::TextFormatDisabled("positions: {} displacements: {}",
                              positions.size(),
                              displacements.size());
    ImGui::TextFormatDisabled("{} iteration {}", automatic_layout, iteration);

    const auto b1 = show_size_widget(*this, *graph);
    const auto b2 = show_default_component_widgets(*this, app, *graph);

    if (b1.first or b2) {
        graph->resize(b1.second, selected_id);
        graph->update();

        positions.resize(graph->children.capacity());
        displacements.resize(graph->children.capacity());

        selected_nodes.clear();
        selected_edges.clear();

        update_position_to_grid(distance, size, positions);

        iteration        = 0;
        automatic_layout = true;
    }

    if (automatic_layout) {
        bool again = compute_automatic_layout(*graph,
                                              ++iteration,
                                              iteration_limit,
                                              distance,
                                              positions,
                                              displacements);
        if (not again) {
            iteration        = 0;
            automatic_layout = false;
        }
    }

    show_graph(app, *compo, *this, *graph);
}

void graph_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/) noexcept
{}

graph_editor_dialog::graph_editor_dialog() noexcept
{
    graph.resize(30, undefined<component_id>());
}

void graph_editor_dialog::load(application&       app_,
                               generic_component* compo_) noexcept
{
    app        = &app_;
    compo      = compo_;
    is_running = true;
    is_ok      = false;
}

void graph_editor_dialog::save() noexcept
{
    debug::ensure(app && compo);

    if (auto ret = app->mod.copy(graph, *compo); !ret)
        log_w(*app, log_level::error, "Fail to copy the graph into component");
}

void graph_editor_dialog::show() noexcept
{
    ImGui::OpenPopup(graph_editor_dialog::name);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(graph_editor_dialog::name)) {
        is_ok        = false;
        bool is_show = true;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };
        const auto child_size =
          region_height - ImGui::GetFrameHeightWithSpacing();

        ImGui::BeginChild("##dialog", ImVec2(0.f, child_size), true);
        // show_size_widget(graph);

        // show_default_component_widgets(
        //   container_of(this, &application::graph_dlg), graph);
        ImGui::EndChild();

        if (ImGui::Button("Ok", button_size)) {
            is_ok   = true;
            is_show = false;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            is_show = false;
        }

        if (is_show == false) {
            ImGui::CloseCurrentPopup();
            is_running = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace irt
