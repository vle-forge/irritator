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

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));
    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active  = ImGui::IsItemActive();

    const ImVec2 origin(canvas_p0.x + grid_sim.scrolling.x,
                        canvas_p0.y + grid_sim.scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle,
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

                // @TODO Adding action on user left mouse button click.
            }

            draw_list->AddRectFilled(
              p_min,
              p_max,
              to_ImU32(app.mod.component_colors[get_index(
                grid.children[grid.pos(row, col)])]));
        }
    }

    draw_list->PopClipRect();

    if (ImGui::BeginPopupContextItem("Canvas-Context")) {
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
        const auto row =
          (click_pos.x - origin.x) /
          ((grid_sim.distance.x + grid_sim.size.x) * grid_sim.zoom[0]);
        const auto col =
          (click_pos.y - origin.y) /
          ((grid_sim.distance.y + grid_sim.size.y) * grid_sim.zoom[1]);

        if (0 <= row and row < grid.row and 0 <= col and col < grid.column) {
            const auto irow      = static_cast<int>(row);
            const auto icol      = static_cast<int>(col);
            const auto uid       = grid.unique_id(irow, icol);
            const auto tn_id_opt = tn.get_tree_node_id(uid);

            if (tn_id_opt.has_value()) {
                if (ImGui::BeginMenu("Action")) {
                    if (ImGui::MenuItem("open"))
                        app.project_wnd.select(
                          app.pj.tree_nodes.get(*tn_id_opt));
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndPopup();
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
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("scale");
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("time-step");
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
              ImGui::PushItemWidth(-1);
              float time_step = grid.time_step;
              if (ImGui::DragFloat("time-step",
                                   &time_step,
                                   0.01f,
                                   grid.time_step.lower,
                                   grid.time_step.upper))
                  grid.time_step.set(time_step);
              ImGui::PopItemWidth();

              ImGui::TableNextColumn();
              if (show_select_model_box(
                    "Select model", "Choose model to observe", app, tn, grid)) {
                  if (auto* mdl = app.sim.models.try_to_get(grid.mdl_id); mdl) {
                      if (mdl->type == dynamics_type::hsm_wrapper) {
                          if (auto* hsm =
                                app.sim.hsms.try_to_get(enum_cast<hsm_id>(
                                  get_dyn<hsm_wrapper>(*mdl).id));
                              hsm) {
                              grid.scale_min = 0.f;
                              grid.scale_max =
                                (float)hsm->compute_max_state_used();
                          }
                      }
                  }
              }

              if (auto* mdl = app.sim.models.try_to_get(grid.mdl_id); mdl) {
                  ImGui::SameLine();
                  ImGui::TextUnformatted(
                    dynamics_type_names[ordinal(mdl->type)]);
              }

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
