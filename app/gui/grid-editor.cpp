// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"
#include "irritator/format.hpp"

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#include <imgui.h>

#include <algorithm>

namespace irt {

static const char* grid_options[] = {
    "none",
    "row_cylinder",
    "column_cylinder",
    "torus",
};

constexpr auto grid_options_count = 4;

static const char* grid_type[] = {
    "number",
    "name",
};

constexpr auto grid_type_count = 2;

static void show_parameters(grid_component& grid) noexcept
{
    int row    = grid.row;
    int column = grid.column;

    if (ImGui::InputInt("row", &row))
        grid.row = std::clamp(row, 1, 256);
    if (ImGui::InputInt("column", &column))
        grid.column = std::clamp(column, 1, 256);

    int selected_options = ordinal(grid.opts);
    int selected_type    = ordinal(grid.connection_type);

    if (ImGui::Combo(
          "Options", &selected_options, grid_options, grid_options_count)) {
        if (selected_options != ordinal(grid.opts))
            grid.opts = static_cast<grid_component::options>(selected_options);
    }

    if (ImGui::Combo("Type", &selected_type, grid_type, grid_type_count)) {
        if (selected_type != ordinal(grid.connection_type))
            grid.connection_type =
              enum_cast<grid_component::type>(selected_type);
    }
}

static void show_default_grid(component_editor& ed,
                              grid_editor_data& data,
                              float             height) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    ImGui::BeginChild("Editor", ImVec2(0, height));

    app->component_sel.combobox(
      "top-left", &data.m_grid.default_children[0][0].id.compo_id);
    app->component_sel.combobox(
      "top", &data.m_grid.default_children[0][1].id.compo_id);
    app->component_sel.combobox(
      "top right", &data.m_grid.default_children[0][2].id.compo_id);

    app->component_sel.combobox(
      "left", &data.m_grid.default_children[1][0].id.compo_id);
    app->component_sel.combobox(
      "middle", &data.m_grid.default_children[1][1].id.compo_id);
    app->component_sel.combobox(
      "right", &data.m_grid.default_children[1][1].id.compo_id);

    app->component_sel.combobox(
      "bottom-left", &data.m_grid.default_children[2][0].id.compo_id);
    app->component_sel.combobox(
      "bottom", &data.m_grid.default_children[2][1].id.compo_id);
    app->component_sel.combobox(
      "bottom right", &data.m_grid.default_children[2][2].id.compo_id);

    ImGui::EndChild();
}

static void show_grid(grid_editor_data& data, float height) noexcept
{
    ImGui::BeginChild("Editor", ImVec2(0, height));

    constexpr ImU32 empty_col   = IM_COL32(127, 127, 127, 255);
    constexpr ImU32 default_col = IM_COL32(255, 255, 255, 255);

    auto  p  = ImGui::GetCursorScreenPos();
    auto* dl = ImGui::GetWindowDrawList();

    ImVec2 upper_left;
    ImVec2 lower_right;
    int    x = 0;
    int    y = 0;

    for (int row = 0; row < data.m_grid.row; ++row) {
        upper_left.x  = p.x + 10.f * static_cast<float>(row);
        lower_right.x = p.x + 10.f * static_cast<float>(row) + 8.f;

        if (row == 0)
            y = 0;
        else if (1 <= row && row + 1 < data.m_grid.row)
            y = 1;
        else
            y = 2;

        for (int col = 0; col < data.m_grid.column; ++col) {
            upper_left.y  = p.y + 10.f * static_cast<float>(col);
            lower_right.y = p.y + 10.f * static_cast<float>(col) + 8.f;

            if (col == 0)
                x = 0;
            else if (1 <= col && col + 1 < data.m_grid.column)
                x = 1;
            else
                x = 2;

            dl->AddRectFilled(
              upper_left,
              lower_right,
              is_defined(data.m_grid.default_children[x][y].id.compo_id)
                ? default_col
                : empty_col,
              0.f);
        }
    }

    ImGui::EndChild();
}

void grid_editor_data::load(const component_id    id,
                            const grid_component& grid) noexcept
{
    this->id = id;
    m_grid   = grid;
}

void grid_editor_data::save(grid_component& grid) noexcept { grid = m_grid; }

void grid_editor_data::show(component_editor& ed) noexcept
{
    const auto item_spacing = ImGui::GetStyle().ItemSpacing.x;
    const auto region_width = ImGui::GetContentRegionAvail().x;
    const auto button_size  = (region_width - item_spacing) / 2.f;
    const auto child_height = ImGui::GetContentRegionAvail().y -
                              ImGui::GetFrameHeightWithSpacing() * 6.f;

    show_parameters(m_grid);

    if (ImGui::BeginTabBar("Editor")) {
        if (ImGui::BeginTabItem("Default")) {
            show_default_grid(ed, *this, child_height);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Nodes")) {
            show_grid(*this, child_height);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (ImGui::Button("Save", ImVec2(button_size, 0.f))) {
        auto* app = container_of(&ed, &application::component_ed);
        auto& mod = app->mod;

        auto* compo   = mod.components.try_to_get(id);
        auto  is_grid = compo && compo->type == component_type::grid;

        if (is_grid) {
            if (auto* grid = mod.grid_components.try_to_get(compo->id.grid_id);
                grid) {
                save(*grid);
            }
        }
    }

    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();

    if (ImGui::Button("Restore", ImVec2(button_size, 0.f))) {
        auto* app = container_of(&ed, &application::component_ed);
        auto& mod = app->mod;

        auto* compo   = mod.components.try_to_get(id);
        auto  is_grid = compo && compo->type == component_type::grid;

        if (is_grid) {
            if (auto* grid = mod.grid_components.try_to_get(compo->id.grid_id);
                grid) {
                load(id, *grid);
            }
        }
    }
}

void grid_editor_dialog::load(application*       app_,
                              generic_component* compo_) noexcept
{
    app        = app_;
    compo      = compo_;
    is_running = true;
    is_ok      = false;
}

void grid_editor_dialog::save() noexcept
{
    irt_assert(app && compo);

    app->mod.copy(grid, *compo);
}

void grid_editor_dialog::show() noexcept
{
    ImGui::OpenPopup(grid_editor_dialog::name);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(grid_editor_dialog::name)) {
        is_ok        = false;
        bool is_show = true;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };
        const auto child_size =
          region_height - ImGui::GetFrameHeightWithSpacing();

        int row    = grid.row;
        int column = grid.column;

        ImGui::BeginChild("##dialog", ImVec2(0.f, child_size), true);

        if (ImGui::InputInt("row", &row))
            grid.row = std::clamp(row, 1, 256);
        if (ImGui::InputInt("column", &column))
            grid.column = std::clamp(column, 1, 256);

        int selected_options = ordinal(grid.opts);
        int selected_type    = ordinal(grid.connection_type);

        if (ImGui::Combo(
              "Options", &selected_options, grid_options, grid_options_count)) {
            if (selected_options != ordinal(grid.opts))
                grid.opts =
                  static_cast<grid_component::options>(selected_options);
        }

        if (ImGui::Combo("Type", &selected_type, grid_type, grid_type_count)) {
            if (selected_type != ordinal(grid.connection_type))
                grid.connection_type =
                  enum_cast<grid_component::type>(selected_type);
        }

        app->component_sel.combobox("Default component",
                                    &grid.default_children[0][0].id.compo_id);

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
