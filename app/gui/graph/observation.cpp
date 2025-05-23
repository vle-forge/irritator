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

    for (const auto id : compo.g.nodes) {
        const auto i = get_index(id);

        const ImVec2 p_min(origin.x + (compo.g.node_positions[i][0] * zoom.x),
                           origin.y + (compo.g.node_positions[i][1] * zoom.y));

        const ImVec2 p_max(
          origin.x +
            ((compo.g.node_positions[i][0] + compo.g.node_areas[i]) * zoom.x),
          origin.y +
            ((compo.g.node_positions[i][1] + compo.g.node_areas[i]) * zoom.y));

        debug::ensure(i < obs.values.size());

        const auto m = static_cast<double>(obs.scale_min);
        const auto M = static_cast<double>(obs.scale_max);
        const auto d = std::abs(m) + std::abs(M);
        const auto o = obs.values[i] + m;
        const auto t = o / d;
        debug::ensure(0.0 <= t and t <= 1.0);

        draw_list->AddRectFilled(
          p_min,
          p_max,
          ImPlot::SampleColormapU32(static_cast<float>(t), IMPLOT_AUTO));
    }

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
                                    graph_observer& graph,
                                    const ImVec2& /*size*/) noexcept
{
    if (auto* c = app.mod.components.try_to_get<component>(graph.compo_id)) {
        if (c->type == component_type::graph) {
            if (auto* g = app.mod.graph_components.try_to_get(c->id.graph_id)) {
                show_graph_observer(*g, zoom, scrolling, distance, graph);
            }
        }
    }
}

} // namespace irt
