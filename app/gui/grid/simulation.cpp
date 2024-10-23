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

static bool display_grid_simulation(application&            app,
                                    grid_simulation_editor& grid_sim,
                                    tree_node&              tn,
                                    const grid_component&   grid) noexcept
{
    if (ImGui::InputFloat2("Zoom", &grid_sim.zoom.x)) {
        grid_sim.zoom[0] = ImClamp(grid_sim.zoom[0], 0.1f, 10.f);
        grid_sim.zoom[1] = ImClamp(grid_sim.zoom[1], 0.1f, 10.f);
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

    const ImVec2 origin(canvas_p0.x + grid_sim.scrolling.x,
                        canvas_p0.y + grid_sim.scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                            mouse_threshold_for_pan)) {
        grid_sim.scrolling.x += io.MouseDelta.x;
        grid_sim.scrolling.y += io.MouseDelta.y;
    }

    if (is_hovered and io.MouseWheel != 0.f) {
        grid_sim.zoom[0] =
          grid_sim.zoom[0] + (io.MouseWheel * grid_sim.zoom[0] * 0.1f);
        grid_sim.zoom[1] =
          grid_sim.zoom[1] + (io.MouseWheel * grid_sim.zoom[1] * 0.1f);
        grid_sim.zoom[0] = ImClamp(grid_sim.zoom[0], 0.1f, 10.f);
        grid_sim.zoom[1] = ImClamp(grid_sim.zoom[1], 0.1f, 10.f);
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    const float GRID_STEP = 64.0f;

    for (float x = fmodf(grid_sim.scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           IM_COL32(200, 200, 200, 40));

    for (float y = fmodf(grid_sim.scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           IM_COL32(200, 200, 200, 40));

    std::pair<int, int> selected_position{ -1, -1 };
    for (int row = 0; row < grid.row; ++row) {
        for (int col = 0; col < grid.column; ++col) {
            ImVec2 p_min(
              origin.x + (col * (grid_sim.distance.x + grid_sim.size.x) *
                          grid_sim.zoom[0]),
              origin.y + (row * (grid_sim.distance.y + grid_sim.size.y) *
                          grid_sim.zoom[1]));

            ImVec2 p_max(p_min.x + grid_sim.zoom[0] * grid_sim.size.x,
                         p_min.y + grid_sim.zoom[1] * grid_sim.size.y);

            if (p_min.x <= io.MousePos.x && io.MousePos.x < p_max.x &&
                p_min.y <= io.MousePos.y && io.MousePos.y < p_max.y &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_position = std::make_pair(row, col);
            }

            draw_list->AddRectFilled(
              p_min,
              p_max,
              to_ImU32(app.mod.component_colors[get_index(
                grid.children[grid.pos(row, col)])]));
        }
    }

    draw_list->PopClipRect();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (selected_position.first != -1 and selected_position.second != -1) {
        const auto uid =
          grid.unique_id(selected_position.first, selected_position.second);

        const auto tn_id_opt = tn.get_tree_node_id(uid);
        if (tn_id_opt.has_value()) {
            app.project_wnd.select(app.pj.tree_nodes.get(*tn_id_opt));
        }
    }

    return true;
}

void grid_simulation_editor::reset() noexcept
{
    current_id = undefined<grid_component_id>();
    zoom       = ImVec2{ 1.f, 1.f };
    scrolling  = ImVec2{ 1.f, 1.f };
    size       = ImVec2{ 30.f, 30.f };
    distance   = ImVec2{ 5.f, 5.f };
}

bool grid_simulation_editor::display(tree_node& tn,
                                     component& /*compo*/,
                                     grid_component& grid) noexcept
{
    auto& ed  = container_of(this, &simulation_editor::grid_sim);
    auto& app = container_of(&ed, &application::simulation_ed);

    const auto grid_id = app.mod.grid_components.get_id(grid);
    if (grid_id != current_id) {
        reset();
        current_id = grid_id;
    }

    return display_grid_simulation(app, *this, tn, grid);
}

void alloc_grid_observer(irt::application& app, irt::tree_node& tn)
{
    auto& grid = app.pj.alloc_grid_observer();

    grid.parent_id = app.pj.tree_nodes.get_id(tn);
    grid.compo_id  = undefined<component_id>();
    grid.tn_id     = undefined<tree_node_id>();
    grid.mdl_id    = undefined<model_id>();
    tn.grid_observer_ids.emplace_back(app.pj.grid_observers.get_id(grid));
}

bool show_local_observers(application& app,
                          tree_node&   tn,
                          component& /*compo*/,
                          grid_component& /*grid*/) noexcept
{
    auto to_del      = std::optional<grid_observer_id>();
    auto is_modified = false;

    if (ImGui::BeginTable("Grid observers", 6)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("scale");
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("model");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_specified_data(
          app.pj.grid_observers,
          tn.grid_observer_ids,
          [&](auto& grid) noexcept {
              const auto id = app.pj.grid_observers.get_id(grid);
              ImGui::PushID(&grid);

              ImGui::TableNextRow();
              ImGui::TableNextColumn();

              ImGui::TextFormat("{}", ordinal(id));

              ImGui::TableNextColumn();

              ImGui::PushItemWidth(-1.0f);
              if (ImGui::InputFilteredString("name", grid.name))
                  is_modified = true;
              ImGui::PopItemWidth();

              ImGui::TableNextColumn();
              ImGui::PushItemWidth(-1);
              ImGui::DragFloatRange2(
                "##scale", &grid.scale_min, &grid.scale_max, 0.01f);
              ImGui::PopItemWidth();
              ImGui::TableNextColumn();
              if (ImPlot::ColormapButton(
                    ImPlot::GetColormapName(grid.color_map),
                    ImVec2(225, 0),
                    grid.color_map)) {
                  grid.color_map =
                    (grid.color_map + 1) % ImPlot::GetColormapCount();
              }

              ImGui::TableNextColumn();
              show_select_model_box(
                "Select model", "Choose model to observe", app, tn, grid);

              if_data_exists_do(
                app.sim.models, grid.mdl_id, [&](auto& mdl) noexcept {
                    ImGui::SameLine();
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl.type)]);
                });

              ImGui::TableNextColumn();

              if (ImGui::Button("del"))
                  to_del = id;

              ImGui::PopID();
          });

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (app.pj.grid_observers.can_alloc() && ImGui::Button("+##grid")) {
            alloc_grid_observer(app, tn);
        }

        ImGui::EndTable();
    }

    if (to_del.has_value()) {
        is_modified = true;
        app.pj.grid_observers.free(*to_del);
    }

    return is_modified;
}

} // namespace irt
