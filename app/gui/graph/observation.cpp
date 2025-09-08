// Copyright (c) 2025 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"

namespace irt {

constexpr auto rotate_x(const float angle) noexcept -> std::array<float, 9>
{
    return { 1.0f,
             0.0f,
             0.0f,
             0.0f,
             +std::cos(angle),
             -std::sin(angle),
             0.0f,
             +std::sin(angle),
             +std::cos(angle) };
}

constexpr auto rotate_y(const float angle) noexcept -> std::array<float, 9>
{
    return { +std::cos(angle), 0.0f, +std::sin(angle), 0.0f, 1.0f, 0.0f,
             -std::sin(angle), 0.0f, +std::cos(angle) };
}

constexpr auto rotate_z(const float angle) noexcept -> std::array<float, 9>
{
    return { +std::cos(angle),
             -std::sin(angle),
             0.0f,
             +std::sin(angle),
             +std::cos(angle),
             0.0f,
             0.0f,
             0.0f,
             0.0f };
}

constexpr auto compute_vertex(const std::array<float, 9>& rot_x,
                              const std::array<float, 9>& rot_y,
                              const std::array<float, 9>& rot_z,
                              const std::array<float, 3>& center,
                              const std::array<float, 3>& dot) noexcept
{
    const auto py_00 = rot_y[0] * (dot[0] - center[0]);
    const auto py_01 = rot_y[1] * (dot[1] - center[1]);
    const auto py_02 = rot_y[2] * (dot[2] - center[2]);

    const auto py_10 = rot_y[3] * (dot[0] - center[0]);
    const auto py_11 = rot_y[4] * (dot[1] - center[1]);
    const auto py_12 = rot_y[5] * (dot[2] - center[2]);

    const auto py_20 = rot_y[6] * (dot[0] - center[0]);
    const auto py_21 = rot_y[7] * (dot[1] - center[1]);
    const auto py_22 = rot_y[8] * (dot[2] - center[2]);

    const std::array<float, 3> py{ py_00 + py_01 + py_02 + center[0],
                                   py_10 + py_11 + py_12 + center[1],
                                   py_20 + py_21 + py_22 + center[2] };

    const auto px_00 = rot_x[0] * (py[0] - center[0]);
    const auto px_01 = rot_x[1] * (py[1] - center[1]);
    const auto px_02 = rot_x[2] * (py[2] - center[2]);

    const auto px_10 = rot_x[3] * (py[0] - center[0]);
    const auto px_11 = rot_x[4] * (py[1] - center[1]);
    const auto px_12 = rot_x[5] * (py[2] - center[2]);

    const auto px_20 = rot_x[6] * (py[0] - center[0]);
    const auto px_21 = rot_x[7] * (py[1] - center[1]);
    const auto px_22 = rot_x[8] * (py[2] - center[2]);

    const std::array<float, 3> px{ px_00 + px_01 + px_02 + center[0],
                                   px_10 + px_11 + px_12 + center[1],
                                   px_20 + px_21 + px_22 + center[2] };

    const auto pz_00 = rot_z[0] * (px[0] - center[0]);
    const auto pz_01 = rot_z[1] * (px[1] - center[1]);
    const auto pz_02 = rot_z[2] * (px[2] - center[2]);

    const auto pz_10 = rot_z[3] * (px[0] - center[0]);
    const auto pz_11 = rot_z[4] * (px[1] - center[1]);
    const auto pz_12 = rot_z[5] * (px[2] - center[2]);

    const auto pz_20 = rot_z[6] * (px[0] - center[0]);
    const auto pz_21 = rot_z[7] * (px[1] - center[1]);
    const auto pz_22 = rot_z[8] * (px[2] - center[2]);

    return std::array<float, 3>{ pz_00 + pz_01 + pz_02 + center[0],
                                 pz_10 + pz_11 + pz_12 + center[1],
                                 pz_20 + pz_21 + pz_22 + center[2] };
}

auto projection_3d::update_matrices(const std::array<float, 3>& angles) noexcept
  -> void
{
    m_angles = angles;

    m_rot_x = rotate_x(angles[0]);
    m_rot_y = rotate_y(angles[1]);
    m_rot_z = rotate_z(angles[2]);
}

auto projection_3d::compute(float x, float y, float z) noexcept
  -> std::array<float, 3>
{
    const auto py_00 = m_rot_y[0] * (x - m_center[0]);
    const auto py_01 = m_rot_y[1] * (y - m_center[1]);
    const auto py_02 = m_rot_y[2] * (z - m_center[2]);

    const auto py_10 = m_rot_y[3] * (x - m_center[0]);
    const auto py_11 = m_rot_y[4] * (y - m_center[1]);
    const auto py_12 = m_rot_y[5] * (z - m_center[2]);

    const auto py_20 = m_rot_y[6] * (x - m_center[0]);
    const auto py_21 = m_rot_y[7] * (y - m_center[1]);
    const auto py_22 = m_rot_y[8] * (z - m_center[2]);

    const std::array<float, 3> py{ py_00 + py_01 + py_02 + m_center[0],
                                   py_10 + py_11 + py_12 + m_center[1],
                                   py_20 + py_21 + py_22 + m_center[2] };

    const auto px_00 = m_rot_x[0] * (py[0] - m_center[0]);
    const auto px_01 = m_rot_x[1] * (py[1] - m_center[1]);
    const auto px_02 = m_rot_x[2] * (py[2] - m_center[2]);

    const auto px_10 = m_rot_x[3] * (py[0] - m_center[0]);
    const auto px_11 = m_rot_x[4] * (py[1] - m_center[1]);
    const auto px_12 = m_rot_x[5] * (py[2] - m_center[2]);

    const auto px_20 = m_rot_x[6] * (py[0] - m_center[0]);
    const auto px_21 = m_rot_x[7] * (py[1] - m_center[1]);
    const auto px_22 = m_rot_x[8] * (py[2] - m_center[2]);

    const std::array<float, 3> px{ px_00 + px_01 + px_02 + m_center[0],
                                   px_10 + px_11 + px_12 + m_center[1],
                                   px_20 + px_21 + px_22 + m_center[2] };

    const auto pz_00 = m_rot_z[0] * (px[0] - m_center[0]);
    const auto pz_01 = m_rot_z[1] * (px[1] - m_center[1]);
    const auto pz_02 = m_rot_z[2] * (px[2] - m_center[2]);

    const auto pz_10 = m_rot_z[3] * (px[0] - m_center[0]);
    const auto pz_11 = m_rot_z[4] * (px[1] - m_center[1]);
    const auto pz_12 = m_rot_z[5] * (px[2] - m_center[2]);

    const auto pz_20 = m_rot_z[6] * (px[0] - m_center[0]);
    const auto pz_21 = m_rot_z[7] * (px[1] - m_center[1]);
    const auto pz_22 = m_rot_z[8] * (px[2] - m_center[2]);

    return std::array<float, 3>{ pz_00 + pz_01 + pz_02 + m_center[0],
                                 pz_10 + pz_11 + pz_12 + m_center[1],
                                 pz_20 + pz_21 + pz_22 + m_center[2] };
}

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

void graph_observation_widget::show(application&    app,
                                    project_editor& ed,
                                    graph_observer& graph,
                                    const ImVec2& /*size*/) noexcept
{
    ImGui::PushID(&graph);
    if (ImGui::BeginChild("graph")) {
        if (auto* tn = ed.pj.tree_nodes.try_to_get(graph.parent_id)) {
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

} // namespace irt
