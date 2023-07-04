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

static void show_selection(application&                app,
                           grid_component_editor_data& ed,
                           grid_component&             grid) noexcept
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

static void show_grid(application&                app,
                      grid_component_editor_data& ed,
                      grid_component&             data) noexcept
{
    static const float item_width  = 100.0f;
    static const float item_height = 100.0f;

    static float zoom         = 1.0f;
    static float new_zoom     = 1.0f;
    static bool  zoom_changed = false;

    ImGui::BeginChild("Editor",
                      ImVec2(0.f, 0.f),
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
              (data.row * zoom);

            {
                auto origin = ImGui::GetCursorScreenPos();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    ImVec2(0.0f, 0.0f));
                ImGui::Dummy(
                  ImVec2(data.row * ImFloor(item_width * new_zoom),
                         data.column * ImFloor(item_height * new_zoom)));
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

    for (int row = 0; row < data.row; ++row) {
        for (int col = 0; col < data.column; ++col) {
            ImGui::SetCursorPos(
              ImFloor(ImVec2(item_width, item_height) * zoom) *
              ImVec2(static_cast<float>(row), static_cast<float>(col)));

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_ImVec4(app.mod.component_colors[get_index(
                                    data.children[data.pos(row, col)])]));

            small_string<32> x;
            format(x, "{}x{}", row, col);

            if (ImGui::Button(x.c_str(),
                              ImVec2(ImFloor(item_width * zoom),
                                     ImFloor(item_height * zoom)))) {
                data.children[data.pos(row, col)] = ed.selected_id;
            }

            ImGui::PopStyleColor();
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

grid_component_editor_data::grid_component_editor_data(
  const component_id      id_,
  const grid_component_id grid_id_) noexcept
  : grid_id(grid_id_)
  , m_id(id_)
{
}

void grid_component_editor_data::clear() noexcept
{
    selected.clear();
    scale = 10.f;

    grid_id = undefined<grid_component_id>();
    m_id    = undefined<component_id>();
}

void grid_component_editor_data::show(component_editor& ed) noexcept
{
    auto* app   = container_of(&ed, &application::component_ed);
    auto* compo = app->mod.components.try_to_get(m_id);
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
