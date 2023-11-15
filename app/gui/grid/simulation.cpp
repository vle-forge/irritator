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

static void grid_simulation_rebuild(grid_simulation_editor& grid_sim,
                                    grid_component&         grid,
                                    grid_component_id       id) noexcept
{
    grid_sim.clear();
    grid_sim.current_id = id;

    grid_sim.selected.resize(grid.row * grid.column);
    std::fill_n(grid_sim.selected.data(), grid_sim.selected.size(), false);

    for (int r = 0, re = grid.row; r != re; ++r) {
        for (int c = 0, ce = grid.column; c != ce; ++c) {
            auto id = grid.children[grid.pos(r, c)];

            if (is_defined(id)) {
                if (auto it = std::find(grid_sim.children_class.begin(),
                                        grid_sim.children_class.end(),
                                        id);
                    it == grid_sim.children_class.end()) {
                    grid_sim.children_class.emplace_back(id);
                }
            }
        }
    }
}

static bool grid_simulation_combobox_component(application&            app,
                                               grid_simulation_editor& grid_sim,
                                               component_id& selected) noexcept
{
    small_string<31> preview = if_data_exists_return(
      app.mod.components,
      selected,
      [&](auto& compo) noexcept -> std::string_view { return compo.name.sv(); },
      std::string_view("-"));

    bool ret = false;

    if (ImGui::BeginCombo("Component type", preview.c_str())) {
        if (ImGui::Selectable("-", is_undefined(selected))) {
            selected = undefined<component_id>();
            ret      = true;
        }

        for_specified_data(
          app.mod.components,
          grid_sim.children_class,
          [&](auto& compo) noexcept {
              ImGui::PushID(&compo);
              const auto id = app.mod.components.get_id(compo);
              if (ImGui::Selectable(compo.name.c_str(), id == selected)) {
                  selected = id;
                  ret      = true;
              }
              ImGui::PopID();
          });

        ImGui::EndCombo();
    }

    return ret;
}

static bool grid_simulation_show_settings(application&            app,
                                          grid_simulation_editor& grid_sim,
                                          tree_node&              tn,
                                          grid_component&         grid) noexcept
{
    static const float item_width  = 100.0f;
    static const float item_height = 100.0f;

    static float zoom         = 1.0f;
    static float new_zoom     = 1.0f;
    static bool  zoom_changed = false;
    bool         ret          = false;

    if (grid_simulation_combobox_component(
          app, grid_sim, grid_sim.selected_setting_component))
        ret = true;

    ImGui::BeginChild("Settings",
                      ImVec2(0, 0),
                      false,
                      ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar);

    if (zoom_changed) {
        zoom         = new_zoom;
        zoom_changed = false;
    } else {
        if (ImGui::IsWindowHovered()) {
            const float zoom_step = 2.0f;

            auto& io = ImGui::GetIO();
            if (io.MouseWheel > 0.0f) {
                new_zoom     = zoom * zoom_step * io.MouseWheel;
                zoom_changed = true;
            } else if (io.MouseWheel < 0.0f) {
                new_zoom     = zoom / (zoom_step * -io.MouseWheel);
                zoom_changed = true;
            }
        }

        if (zoom_changed) {
            auto mouse_position_on_window =
              ImGui::GetMousePos() - ImGui::GetWindowPos();

            auto mouse_position_on_list =
              (ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY()) +
               mouse_position_on_window) /
              (grid.row * zoom);

            {
                auto origin = ImGui::GetCursorScreenPos();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    ImVec2(0.0f, 0.0f));
                ImGui::Dummy(
                  ImVec2(grid.row * ImFloor(item_width * new_zoom),
                         grid.column * ImFloor(item_height * new_zoom)));
                ImGui::PopStyleVar();
                ImGui::SetCursorScreenPos(origin);
            }

            auto new_mouse_position_on_list =
              mouse_position_on_list * (item_height * new_zoom);
            auto new_scroll =
              new_mouse_position_on_list - mouse_position_on_window;

            ImGui::SetScrollX(new_scroll.x);
            ImGui::SetScrollY(new_scroll.y);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    std::optional<tree_node*> selected_child;

    if (tree_node* child = tn.tree.get_child(); child) {
        do {
            if (child->id == grid_sim.selected_setting_component) {
                const auto pos = grid.unique_id(child->unique_id);

                ImGui::SetCursorPos(
                  ImFloor(ImVec2(item_width, item_height) * zoom) *
                  ImVec2(static_cast<float>(pos.first),
                         static_cast<float>(pos.second)));

                ImGui::PushStyleColor(
                  ImGuiCol_Button,
                  to_ImVec4(app.mod.component_colors[get_index(
                    grid.children[grid.pos(pos.first, pos.second)])]));

                small_string<32> x;
                format(x, "{}x{}", pos.first, pos.second);

                if (ImGui::Button(x.c_str(),
                                  ImVec2(ImFloor(item_width * zoom),
                                         ImFloor(item_height * zoom)))) {
                    selected_child = child;
                }

                ImGui::PopStyleColor();
            }
            child = child->tree.get_sibling();
        } while (child);
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (selected_child.has_value())
        app.project_wnd.select(**selected_child);

    return ret;
}

static bool grid_simulation_show_observations(application&            app,
                                              grid_simulation_editor& grid_sim,
                                              tree_node&              tn,
                                              grid_component& grid) noexcept
{
    static const float item_width  = 100.0f;
    static const float item_height = 100.0f;

    static float zoom         = 1.0f;
    static float new_zoom     = 1.0f;
    static bool  zoom_changed = false;
    bool         ret          = false;

    if (grid_simulation_combobox_component(
          app, grid_sim, grid_sim.selected_observation_component))
        ret = true;

    ImGui::BeginChild("Observations",
                      ImVec2(0, 0),
                      true,
                      ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar);

    if (zoom_changed) {
        zoom         = new_zoom;
        zoom_changed = false;
    } else {
        if (ImGui::IsWindowHovered()) {
            const float zoom_step = 2.0f;

            auto& io = ImGui::GetIO();
            if (io.MouseWheel > 0.0f) {
                new_zoom     = zoom * zoom_step * io.MouseWheel;
                zoom_changed = true;
            } else if (io.MouseWheel < 0.0f) {
                new_zoom     = zoom / (zoom_step * -io.MouseWheel);
                zoom_changed = true;
            }
        }

        if (zoom_changed) {
            auto mouse_position_on_window =
              ImGui::GetMousePos() - ImGui::GetWindowPos();

            auto mouse_position_on_list =
              (ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY()) +
               mouse_position_on_window) /
              (grid.row * zoom);

            {
                auto origin = ImGui::GetCursorScreenPos();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    ImVec2(0.0f, 0.0f));
                ImGui::Dummy(
                  ImVec2(grid.row * ImFloor(item_width * new_zoom),
                         grid.column * ImFloor(item_height * new_zoom)));
                ImGui::PopStyleVar();
                ImGui::SetCursorScreenPos(origin);
            }

            auto new_mouse_position_on_list =
              mouse_position_on_list * (item_height * new_zoom);
            auto new_scroll =
              new_mouse_position_on_list - mouse_position_on_window;

            ImGui::SetScrollX(new_scroll.x);
            ImGui::SetScrollY(new_scroll.y);
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    if (tree_node* child = tn.tree.get_child(); child) {
        do {
            if (child->id == grid_sim.selected_observation_component) {
                const auto pos = grid.unique_id(child->unique_id);

                ImGui::SetCursorPos(
                  ImFloor(ImVec2(item_width, item_height) * zoom) *
                  ImVec2(static_cast<float>(pos.first),
                         static_cast<float>(pos.second)));

                ImGui::PushStyleColor(
                  ImGuiCol_Button,
                  to_ImVec4(app.mod.component_colors[get_index(
                    grid.children[grid.pos(pos.first, pos.second)])]));

                small_string<32> x;
                format(x, "{}x{}", pos.first, pos.second);

                if (ImGui::Button(x.c_str(),
                                  ImVec2(ImFloor(item_width * zoom),
                                         ImFloor(item_height * zoom)))) {
                    grid_sim.selected_position          = pos;
                    grid_sim.selected_observation_model = undefined<model_id>();
                    grid_sim.selected_tn                = child;
                }

                ImGui::PopStyleColor();
            }
            child = child->tree.get_sibling();
        } while (child);
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (grid_sim.selected_position.has_value())
        ImGui::OpenPopup("Choose model to observe");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (grid_sim.selected_position.has_value() &&
        ImGui::BeginPopupModal("Choose model to observe",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Select the model to observe");

        irt_assert(grid_sim.selected_tn != nullptr);
        //show_select_observation_model(app,
        //                              grid_sim,
        //                              *grid_sim.selected_tn,
        //                              &grid_sim.selected_observation_model);

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            grid_sim.selected_position.reset();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            grid_sim.selected_position.reset();
            grid_sim.selected_tn                = nullptr;
            grid_sim.selected_observation_model = undefined<model_id>();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

void grid_simulation_editor::clear() noexcept
{
    show_position   = ImVec2{ 0.f, 0.f };
    disp            = ImVec2{ 1000.f, 1000.f };
    scale           = 10.f;
    start_selection = false;
    current_id      = undefined<grid_component_id>();

    selected_setting_component = undefined<component_id>();
    selected_setting_model     = undefined<model_id>();

    selected_observation_component = undefined<component_id>();
    selected_observation_model     = undefined<model_id>();

    selected.clear();
    children_class.clear();
}

bool grid_simulation_editor::show_settings(tree_node& tn,
                                           component& /*compo*/,
                                           grid_component& grid) noexcept
{
    auto& ed  = container_of(this, &simulation_editor::grid_sim);
    auto& app = container_of(&ed, &application::simulation_ed);

    const auto grid_id = app.mod.grid_components.get_id(grid);
    if (grid_id != current_id)
        grid_simulation_rebuild(*this, grid, grid_id);

    return grid_simulation_show_settings(app, *this, tn, grid);
}

bool grid_simulation_editor::show_observations(tree_node& tn,
                                               component& /*compo*/,
                                               grid_component& grid) noexcept
{
    auto& ed  = container_of(this, &simulation_editor::grid_sim);
    auto& app = container_of(&ed, &application::simulation_ed);

    const auto grid_id = app.mod.grid_components.get_id(grid);
    if (grid_id != current_id)
        grid_simulation_rebuild(*this, grid, grid_id);

    return grid_simulation_show_observations(app, *this, tn, grid);
}

bool show_local_observers(application& app,
                          tree_node&   tn,
                          component& /*compo*/,
                          grid_component& /*grid*/) noexcept
{
    if (ImGui::CollapsingHeader("Local grid observation")) {
        if (app.pj.grid_observers.can_alloc() && ImGui::Button("+##grid")) {
            auto& grid = app.pj.grid_observers.alloc();

            grid.parent_id = app.pj.tree_nodes.get_id(tn);
            grid.compo_id  = undefined<component_id>();
            grid.tn_id     = undefined<tree_node_id>();
            grid.mdl_id    = undefined<model_id>();
            tn.grid_observer_ids.emplace_back(
              app.pj.grid_observers.get_id(grid));

            format(grid.name,
                   "rename-{}",
                   get_index(app.pj.grid_observers.get_id(grid)));
        }

        std::optional<grid_modeling_observer_id> to_delete;
        bool                            is_modified = false;

        for_specified_data(
          app.pj.grid_observers,
          tn.grid_observer_ids,
          [&](auto& grid) noexcept {
              ImGui::PushID(&grid);

              if (ImGui::InputFilteredString("name", grid.name))
                  is_modified = true;

              ImGui::SameLine();

              if (ImGui::Button("del"))
                  to_delete =
                    std::make_optional(app.pj.grid_observers.get_id(grid));

              ImGui::TextFormatDisabled(
                "grid-id {} component_id {} tree-node-id {} model-id {}",
                ordinal(grid.parent_id),
                ordinal(grid.compo_id),
                ordinal(grid.tn_id),
                ordinal(grid.mdl_id));

              if_data_exists_do(
                app.sim.models, grid.mdl_id, [&](auto& mdl) noexcept {
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl.type)]);
                });

              show_select_model_box(
                "Select model", "Choose model to observe", app, tn, grid);

              ImGui::PopID();
          });

        if (to_delete.has_value()) {
            is_modified = true;
            app.pj.grid_observers.free(*to_delete);
        }
    }

    return false;
}

} // namespace irt
