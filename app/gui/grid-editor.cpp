// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"
#include "irritator/format.hpp"

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace irt {

constinit ImU32 undefined_color = IM_COL32(0, 0, 0, 255);
constinit ImU32 selected_col    = IM_COL32(255, 0, 0, 255);

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

constexpr inline auto get_default_component_id(const grid_component& g) noexcept
  -> component_id
{
    return g.children.empty() ? undefined<component_id>() : g.children.front();
}

static bool show_row_column_widgets(grid_component& grid) noexcept
{
    bool is_changed = false;

    int row = grid.row;
    if (ImGui::InputInt("row", &row)) {
        row        = std::clamp(row, 1, 256);
        is_changed = row != grid.row;
        grid.row   = row;
    }

    int column = grid.column;
    if (ImGui::InputInt("column", &column)) {
        column      = std::clamp(column, 1, 256);
        is_changed  = is_changed || column != grid.column;
        grid.column = column;
    }

    return is_changed;
}

static void show_type_widgets(grid_component& grid) noexcept
{
    int selected_options = ordinal(grid.opts);
    if (ImGui::Combo(
          "Options", &selected_options, grid_options, grid_options_count)) {
        if (selected_options != ordinal(grid.opts))
            grid.opts = static_cast<grid_component::options>(selected_options);
    }

    int selected_type = ordinal(grid.connection_type);
    if (ImGui::Combo("Type", &selected_type, grid_type, grid_type_count)) {
        if (selected_type != ordinal(grid.connection_type))
            grid.connection_type =
              enum_cast<grid_component::type>(selected_type);
    }
}

void show_default_component_widgets(application& app, grid_component& grid)
{
    auto id = get_default_component_id(grid);
    if (app.component_sel.combobox("Default component", &id)) {
        std::fill_n(grid.children.data(), grid.children.ssize(), id);
    }
}

/**
 * @brief Retrieves the selected @c component_id.
 * @details If the selection have several different @c component_id then the
 * function return @c std::nullopt otherwise, returns the @c component_id or @c
 * undefined<component_id>() if element is empty.
 *
 * @return The unique @c component_id or @c undefined<component_id>() if
 * selection is multiple.
 */
// static auto get_selected_id(const vector<component_id>& ids,
//                             const vector<bool>&         selected) noexcept
//   -> std::optional<component_id>
// {
//     irt_assert(ids.size() == selected.size());

//     auto found      = undefined<component_id>();
//     auto have_value = false;

//     for (sz i = 0, e = ids.size(); i != e; ++i) {
//         if (selected[i]) {
//             if (have_value) {
//                 if (found != ids[i])
//                     return std::nullopt;
//             } else {
//                 found      = ids[i];
//                 have_value = true;
//             }
//         }
//     }

//     return std::make_optional(found);
// }

// static void assign_selection(const vector<bool>&   selected,
//                              vector<component_id>& ids,
//                              component_id          value) noexcept
// {
//     irt_assert(ids.size() == selected.size());

//     for (sz i = 0, e = ids.size(); i != e; ++i)
//         if (selected[i])
//             ids[i] = value;
// }

static void show_selection(application&      app,
                           grid_editor_data& ed,
                           grid_component&   grid) noexcept
{
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        app.component_sel.combobox("component paint", &ed.selected_id);
    }

    if (ImGui::CollapsingHeader("Selected", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int row = 0; row < grid.row; ++row) {
            for (int col = 0; col < grid.column; ++col) {
                if (ed.selected[grid.pos(row, col)]) {
                    ImGui::Text("%d %d", row, col);
                }
            }
        }
    }
}

static void show_grid(application&      app,
                      grid_editor_data& ed,
                      grid_component&   data) noexcept
{
    ImGui::BeginChild("Editor",
                      ed.disp,
                      true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar);

    auto* w  = ImGui::GetCurrentWindow();
    auto& io = ImGui::GetIO();

    if (w->InnerClipRect.Contains(io.MousePos)) {
        if (io.MouseWheel > 0.0f) {
            ed.scale *= 1.1f;
            ed.disp *= 1.1f;
        } else if (io.MouseWheel < 0.0f) {
            ed.scale *= 0.9f;
            ed.disp *= 0.9f;
        }
    }

    auto* draw_list = ImGui::GetWindowDrawList();
    auto  p         = ImGui::GetCursorScreenPos();
    auto  mouse     = io.MousePos - p;

    ImVec2 upper_left;
    ImVec2 lower_right;

    for (int row = 0; row < data.row; ++row) {
        upper_left.x  = p.x + static_cast<float>(row) * ed.scale;
        lower_right.x = p.x + static_cast<float>(row) * ed.scale + ed.scale;

        for (int col = 0; col < data.column; ++col) {
            upper_left.y  = p.y + static_cast<float>(col) * ed.scale;
            lower_right.y = p.y + static_cast<float>(col) * ed.scale + ed.scale;

            const bool under_mouse =
              upper_left.x <= io.MousePos.x && io.MousePos.x <= lower_right.x &&
              upper_left.y <= io.MousePos.y && io.MousePos.y <= lower_right.y;

            if (under_mouse && io.MouseDown[0]) {
                if (ed.start_selection == true) {
                    ed.start_selection = false;
                }

                data.children[data.pos(row, col)] = ed.selected_id;
            }

            if (under_mouse && !ed.start_selection && io.MouseDown[1]) {
                ed.start_selection = true;
                std::fill_n(ed.selected.data(), ed.selected.size(), false);
            }

            if (under_mouse && ed.start_selection && io.MouseDown[1])
                ed.selected[data.pos(row, col)] = true;

            if (under_mouse && ed.start_selection && io.MouseReleased[1])
                ed.start_selection = false;

            ImU32 color = is_undefined(data.children[data.pos(row, col)])
                            ? undefined_color
                            : ImGui::ColorConvertFloat4ToU32(
                                to_ImVec4(app.mod.component_colors[get_index(
                                  data.children[data.pos(row, col)])]));

            draw_list->AddRectFilled(upper_left, lower_right, color, 0.f);
            if (ed.selected[data.pos(row, col)])
                draw_list->AddRect(upper_left,
                                   lower_right,
                                   selected_col,
                                   0.f,
                                   ImDrawFlags_None,
                                   1.f);
        }
    }

    if (!ed.start_selection && io.MouseClicked[1])
        std::fill_n(ed.selected.data(), ed.selected.size(), false);

    ImGuiContext& g = *GImGui;
    ImGui::SetCursorPos({ 0, w->InnerClipRect.GetHeight() - g.FontSize });
    ImGui::Text("mouse x:%.3f  y:%.3f", mouse.x, mouse.y);

    ImGui::EndChild();
}

grid_editor_data::grid_editor_data(const component_id      id_,
                                   const grid_component_id grid_id_) noexcept
  : grid_id(grid_id_)
  , id(id_)
{
}

void grid_editor_data::clear() noexcept
{
    selected.clear();
    scale = 10.f;

    grid_id = undefined<grid_component_id>();
    id      = undefined<component_id>();
}

void grid_editor_data::show(component_editor& ed) noexcept
{
    auto* app   = container_of(&ed, &application::component_ed);
    auto* compo = app->mod.components.try_to_get(id);
    auto* grid  = app->mod.grid_components.try_to_get(grid_id);

    irt_assert(compo && grid);
    if (selected.capacity() == 0) {
        selected.resize(grid->row * grid->column);
        std::fill_n(selected.data(), selected.size(), false);
    }

    if (show_row_column_widgets(*grid)) {
        grid->resize(grid->row, grid->column, get_default_component_id(*grid));
        selected.resize(grid->row * grid->column);
        std::fill_n(selected.data(), selected.size(), false);
    }

    show_type_widgets(*grid);
    show_default_component_widgets(*app, *grid);

    if (ImGui::BeginTable("##array", 2)) {
        ImGui::TableNextColumn();
        show_grid(*app, *this, *grid);
        ImGui::TableNextColumn();
        show_selection(*app, *this, *grid);

        ImGui::EndTable();
    }
}

grid_editor_dialog::grid_editor_dialog() noexcept
{
    grid.resize(5, 5, undefined<component_id>());
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

        ImGui::BeginChild("##dialog", ImVec2(0.f, child_size), true);
        if (show_row_column_widgets(grid))
            grid.resize(grid.row, grid.column, get_default_component_id(grid));

        show_type_widgets(grid);
        show_default_component_widgets(
          *container_of(this, &application::grid_dlg), grid);
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
