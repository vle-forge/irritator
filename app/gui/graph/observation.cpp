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

graph_editor::graph_editor() noexcept
  : selected_nodes{ 128u, reserve_tag }
  , selected_edges{ 128u, reserve_tag }
{}

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

    if (is_hovered and io.MouseWheel != 0.f) {
        zoom = zoom + (io.MouseWheel * zoom * 0.1f);
        zoom = ImClamp(zoom, 0.1f, 1000.f);
    }
}

void graph_editor::draw_graph(const graph& g, ImVec2 top_left) noexcept
{
    ImDrawList*  draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin    = top_left + scrolling;

    nodes_locker.try_read_only([&](const auto& d) noexcept {
        for (const auto& node : d.nodes) {
            const ImVec2 p_min(origin.x + (node[0] * zoom),
                               origin.y + (node[1] * zoom));

            const ImVec2 p_max(origin.x + ((node[0] + node[3]) * zoom),
                               origin.y + ((node[1] + node[3]) * zoom));

            // @todo Computes the color from color map before.
            draw_list->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 0, 255));
        }

        for (const auto& [from, to] : d.edges) {
            ImVec2 src(
              origin.x + ((d.nodes[from][0] + d.nodes[from][3] / 2.f) * zoom),
              origin.y + ((d.nodes[from][1] + d.nodes[from][3] / 2.f) * zoom));

            ImVec2 dst(
              origin.x + ((d.nodes[to][0] + d.nodes[to][3] / 2.f) * zoom),
              origin.y + ((d.nodes[to][1] + d.nodes[to][3] / 2.f) * zoom));

            draw_list->AddLine(
              src, dst, to_ImU32(app.config.colors[style_color::edge]), 1.f);
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

void graph_editor::show(application&    app,
                        project_editor& ed,
                        tree_node&      tn) noexcept
{
    debug::ensure(app.mod.components.exists(tn.id));
    debug::ensure(app.mod.components.get<component>(tn.id).type ==
                  component_type::graph);

    if (not app.mod.components.exists(tn.id))
        return;

    auto& compo = app.mod.components.get<component>(tn.id);
    if (compo.type != component_type::graph)
        return;

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

    draw_graph(graph, canvas_p0);
}

bool graph_editor::show(application&     app,
                        component&       c,
                        graph_component& g) noexcept
{
    auto       edited    = false;
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

    draw_graph(g.g, canvas_p0);

    // ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    // if (drag_delta.x == 0.0f and drag_delta.y == 0.0f)
    //     ImGui::OpenPopupOnItemClick("Canvas-Context",
    //                                 ImGuiPopupFlags_MouseButtonRight);

    // if (ImGui::BeginPopup("Canvas-Context")) {
    //     const auto click = ImGui::GetMousePosOnOpeningCurrentPopup();
    //     if (ImGui::BeginMenu("Actions")) {
    //         if (ImGui::MenuItem("New node")) {
    //             if (auto id = data.g.alloc_node(); id.has_value()) {
    //                 const auto idx = get_index(*id);

    //                 data.g.node_positions[idx] = {
    //                     (click.x - origin.x) / ed.zoom.x,
    //                     (click.y - origin.y) / ed.zoom.y,
    //                 };

    //                 ed.selected_nodes.emplace_back(*id);
    //             } else {
    //                 app.jn.push(
    //                   log_level::error,
    //                   [](auto& t, auto& m, auto& id) {
    //                       t = "Failed to add new node.";
    //                       format(m,
    //                              "Error: category {} value {}",
    //                              ordinal(id.error().cat()),
    //                              id.error().value());
    //                   },
    //                   id);
    //             }
    //         }

    //         if (not ed.selected_nodes.empty() and
    //         ImGui::MenuItem("Connect"))
    //         {
    //             const auto e = ed.selected_nodes.ssize();
    //             for (int i = 0; i < e; ++i) {
    //                 for (int j = i + 1; j < e; ++j) {
    //                     data.g.alloc_edge(ed.selected_nodes[i],
    //                                       ed.selected_nodes[j]);
    //                 }
    //             }
    //             ed.selected_nodes.clear();
    //         }

    //         if (not ed.selected_nodes.empty() and
    //             ImGui::MenuItem("Delete nodes")) {
    //             for (auto id : ed.selected_nodes) {
    //                 if (data.g.nodes.exists(id))
    //                     data.g.nodes.free(id);
    //             }
    //             ed.selected_nodes.clear();
    //         }

    //         if (not ed.selected_edges.empty() and
    //             ImGui::MenuItem("Delete edges")) {
    //             for (auto id : ed.selected_edges)
    //                 if (data.g.edges.exists(id))
    //                     data.g.edges.free(id);
    //             ed.selected_edges.clear();
    //         }

    //         ImGui::EndMenu();
    //     }
    //     ImGui::EndPopup();
    // }

    // if (is_hovered) {
    //     if (not run_selection and
    //     ImGui::IsMouseDown(ImGuiMouseButton_Left))
    //     {
    //         run_selection   = true;
    //         start_selection = io.MousePos;
    //     }

    //     if (run_selection and
    //     ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    //     {
    //         run_selection = false;
    //         end_selection = io.MousePos;

    //         if (start_selection == end_selection) {
    //             selected_nodes.clear();
    //             selected_edges.clear();
    //         } else {
    //             ImVec2 bmin{
    //                 std::min(ed.start_selection.x, ed.end_selection.x),
    //                 std::min(ed.start_selection.y, ed.end_selection.y),
    //             };

    //             ImVec2 bmax{
    //                 std::max(ed.start_selection.x, ed.end_selection.x),
    //                 std::max(ed.start_selection.y, ed.end_selection.y),
    //             };

    //             ed.selected_edges.clear();
    //             ed.selected_nodes.clear();

    //             for (const auto id : data.g.nodes) {
    //                 const auto i = get_index(id);

    //                 ImVec2 p_min(
    //                   origin.x + (data.g.node_positions[i][0] *
    //                   ed.zoom.x), origin.y + (data.g.node_positions[i][1]
    //                   * ed.zoom.y));

    //                 ImVec2 p_max(
    //                   origin.x +
    //                     ((data.g.node_positions[i][0] +
    //                     data.g.node_areas[i])
    //                     *
    //                      ed.zoom.x),
    //                   origin.y +
    //                     ((data.g.node_positions[i][1] +
    //                     data.g.node_areas[i])
    //                     *
    //                      ed.zoom.y));

    //                 if (p_min.x >= bmin.x and p_max.x < bmax.x and
    //                     p_min.y >= bmin.y and p_max.y < bmax.y) {
    //                     ed.selected_nodes.emplace_back(id);
    //                 }
    //             }

    //             for (const auto id : data.g.edges) {
    //                 const auto us =
    //                 data.g.edges_nodes[get_index(id)][0].first; const
    //                 auto vs = data.g.edges_nodes[get_index(id)][1].first;

    //                 if (data.g.nodes.exists(us) and
    //                 data.g.nodes.exists(vs))
    //                 {
    //                     ImVec2 p1(
    //                       origin.x +
    //                       ((data.g.node_positions[get_index(us)][0] +
    //                                    data.g.node_areas[get_index(us)]
    //                                    / 2.f) *
    //                                   ed.zoom.x),
    //                       origin.y +
    //                       ((data.g.node_positions[get_index(us)][1] +
    //                                    data.g.node_areas[get_index(vs)]
    //                                    / 2.f) *
    //                                   ed.zoom.y));

    //                     ImVec2 p2(
    //                       origin.x +
    //                       ((data.g.node_positions[get_index(vs)][0] +
    //                                    data.g.node_areas[get_index(us)]
    //                                    / 2.f) *
    //                                   ed.zoom.x),
    //                       origin.y +
    //                       ((data.g.node_positions[get_index(vs)][1] +
    //                                    data.g.node_areas[get_index(vs)]
    //                                    / 2.f) *
    //                                   ed.zoom.y));

    //                     if (is_line_intersects_box(p1, p2, bmin, bmax))
    //                         ed.selected_edges.emplace_back(id);
    //                 }
    //             }
    //         }
    //     }
    // }

    nodes_locker.try_read_only([&](const auto& d) noexcept {
        for (const auto& node : d.nodes) {
            const ImVec2 p_min(origin.x + (node[0] * zoom),
                               origin.y + (node[1] * zoom));

            const ImVec2 p_max(origin.x + ((node[0] + node[3]) * zoom),
                               origin.y + ((node[1] + node[3]) * zoom));

            // @todo Computes the color from color map before.
            draw_list->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 0, 255));
        }

        // for (const auto id : ed.selected_nodes) {
        //     const auto i = get_index(id);

        //     ImVec2 p_min(origin.x + (data.g.node_positions[i][0] *
        //     ed.zoom.x),
        //                  origin.y + (data.g.node_positions[i][1] *
        //                  ed.zoom.y));

        //     ImVec2 p_max(
        //       origin.x + ((data.g.node_positions[i][0] +
        //       data.g.node_areas[i]) *
        //                   ed.zoom.x),
        //       origin.y + ((data.g.node_positions[i][1] +
        //       data.g.node_areas[i]) *
        //                   ed.zoom.y));

        //     draw_list->AddRect(
        //       p_min,
        //       p_max,
        //       to_ImU32(app.config.colors[style_color::node_active]),
        //       0.f,
        //       0,
        //       4.f);
        // }

        for (const auto& [from, to] : d.edges) {
            ImVec2 src(
              origin.x + ((d.nodes[from][0] + d.nodes[from][3] / 2.f) * zoom),
              origin.y + ((d.nodes[from][1] + d.nodes[from][3] / 2.f) * zoom));

            ImVec2 dst(
              origin.x + ((d.nodes[to][0] + d.nodes[to][3] / 2.f) * zoom),
              origin.y + ((d.nodes[to][1] + d.nodes[to][3] / 2.f) * zoom));

            draw_list->AddLine(
              src, dst, to_ImU32(app.config.colors[style_color::edge]), 1.f);
        }

        // for (const auto id : ed.selected_edges) {
        //     const auto idx = get_index(id);
        //     const auto u_c = data.g.edges_nodes[idx][0].first;
        //     const auto v_c = data.g.edges_nodes[idx][1].first;

        //     if (not(data.g.nodes.exists(u_c) and
        //     data.g.nodes.exists(v_c)))
        //         continue;

        //     const auto p_src = get_index(u_c);
        //     const auto p_dst = get_index(v_c);

        //     ImVec2 src(origin.x + ((data.g.node_positions[p_src][0] +
        //                             data.g.node_areas[p_src] / 2.f) *
        //                            ed.zoom.x),
        //                origin.y + ((data.g.node_positions[p_src][1] +
        //                             data.g.node_areas[p_src] / 2.f) *
        //                            ed.zoom.y));

        //     ImVec2 dst(origin.x + ((data.g.node_positions[p_dst][0] +
        //                             data.g.node_areas[p_dst] / 2.f) *
        //                            ed.zoom.x),
        //                origin.y + ((data.g.node_positions[p_dst][1] +
        //                             data.g.node_areas[p_dst] / 2.f) *
        //                            ed.zoom.y));

        //     draw_list->AddLine(
        //       src,
        //       dst,
        //       to_ImU32(app.config.colors[style_color::edge_active]),
        //       1.0f);
        // }

        // if (ed.run_selection) {
        //     ed.end_selection = io.MousePos;

        //     if (ed.start_selection == ed.end_selection) {
        //         ed.selected_nodes.clear();
        //         ed.selected_edges.clear();
        //     } else {
        //         ImVec2 bmin{
        //             std::min(ed.start_selection.x, io.MousePos.x),
        //             std::min(ed.start_selection.y, io.MousePos.y),
        //         };

        //         ImVec2 bmax{
        //             std::max(ed.start_selection.x, io.MousePos.x),
        //             std::max(ed.start_selection.y, io.MousePos.y),
        //         };

        //         draw_list->AddRectFilled(
        //           bmin,
        //           bmax,
        //           to_ImU32(
        //             app.config.colors[style_color::background_selection]));
        //     }
        // }
    });

    draw_list->PopClipRect();

    return edited;
}

void graph_editor::update(application& app, const graph& g) noexcept
{
    app.add_gui_task([&]() noexcept {
        nodes_locker.read_write([&](auto& d) noexcept {
            d.nodes.resize(g.nodes.ssize());

            for (const auto id : g.nodes) {
                const auto idx = get_index(id);

                d.nodes[idx][0] = g.node_positions[get_index(id)][0];
                d.nodes[idx][1] = g.node_positions[get_index(id)][1];
                d.nodes[idx][2] = g.node_positions[get_index(id)][2];
                d.nodes[idx][3] = g.node_areas[get_index(id)];
                d.nodes[idx][4] = 0.f;
            }
        });
    });
}

} // namespace irt
