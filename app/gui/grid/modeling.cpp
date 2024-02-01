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

static const char* grid_options[] = {
    "none",
    "row_cylinder",
    "column_cylinder",
    "torus",
};

constexpr auto grid_options_count = 4;

static const char* grid_type[] = {
    "number (in - out port)",
    "name (N, W, S, E ports)",
};

constexpr auto grid_type_count = 2;

static const char* grid_neighborhood[]     = { "four", "eight" };
constexpr auto     grid_neighborhood_count = 2;

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

static void show_grid_component_options(grid_component& grid) noexcept
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

    int selected_neighbors = ordinal(grid.neighbors);
    if (ImGui::Combo("Neighbors",
                     &selected_neighbors,
                     grid_neighborhood,
                     grid_neighborhood_count)) {
        if (selected_neighbors != ordinal(grid.neighbors))
            grid.neighbors =
              enum_cast<grid_component::neighborhood>(selected_neighbors);
    }
}

static bool new_name_4(modeling& mod, component& compo) noexcept
{
    if (mod.ports.can_alloc(8)) {
        const auto id = mod.components.get_id(compo);

        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("N", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("S", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("W", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("E", id)));

        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("N", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("S", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("W", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("E", id)));

        return true;
    }

    return false;
}

static bool new_number_4(modeling& mod, component& compo) noexcept
{
    if (mod.ports.can_alloc(2)) {
        const auto id = mod.components.get_id(compo);

        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("in", id)));
        compo.y_names.emplace_back(
          mod.ports.get_id(mod.ports.alloc("out", id)));

        return true;
    }

    return false;
}

static bool new_name_8(modeling& mod, component& compo) noexcept
{
    if (mod.ports.can_alloc(16)) {
        const auto id = mod.components.get_id(compo);

        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("N", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("S", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("W", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("E", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("NE", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("SE", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("NW", id)));
        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("SW", id)));

        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("N", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("S", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("W", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("E", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("NE", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("SE", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("NW", id)));
        compo.y_names.emplace_back(mod.ports.get_id(mod.ports.alloc("SW", id)));

        return true;
    }

    return false;
}

static bool new_number_8(modeling& mod, component& compo) noexcept
{
    if (mod.ports.can_alloc(2)) {
        const auto id = mod.components.get_id(compo);

        compo.x_names.emplace_back(mod.ports.get_id(mod.ports.alloc("in", id)));
        compo.y_names.emplace_back(
          mod.ports.get_id(mod.ports.alloc("out", id)));

        return true;
    }

    return false;
}

static bool assign_name(modeling&       mod,
                        grid_component& grid,
                        component&      compo) noexcept
{

    switch (grid.neighbors) {
    case grid_component::neighborhood::four:
        switch (grid.connection_type) {
        case grid_component::type::name:
            return new_name_4(mod, compo);

        case grid_component::type::number:
            return new_number_4(mod, compo);
        }

        unreachable();
        break;

    case grid_component::neighborhood::eight:
        switch (grid.connection_type) {
        case grid_component::type::name:
            return new_name_8(mod, compo);

        case grid_component::type::number:
            return new_number_8(mod, compo);
        }

        unreachable();
        break;
    }

    unreachable();
}

static void show_grid_editor_options(application&                app,
                                     grid_component_editor_data& ed,
                                     grid_component&             grid) noexcept
{
    auto id = get_default_component_id(grid);

    {
        auto updated = app.component_sel.combobox("Default component", &id);
        ImGui::SameLine();
        HelpMarker(
          "Reset the content of the grid with the selected component.");

        if (updated)
            std::fill_n(grid.children.data(), grid.children.ssize(), id);
    }

    {
        app.component_sel.combobox("Paint component", &ed.selected_id);
        ImGui::SameLine();
        HelpMarker("Select a component in the list and draw the grid using the "
                   "left button of your mouse.");
    }

    ImGui::BeginDisabled(!(app.mod.components.can_alloc(1) &&
                           app.mod.generic_components.can_alloc(1)));

    ImGui::TextUnformatted("Create input/outputs ports compatible "
                           "component:");

    ImGui::SameLine();
    if (app.mod.generic_components.can_alloc() && ImGui::Button("+ generic")) {
        auto& compo = app.mod.alloc_generic_component();
        assign_name(app.mod, grid, compo);
    }

    ImGui::SameLine();
    if (app.mod.grid_components.can_alloc() && ImGui::Button("+ grid")) {
        auto& compo = app.mod.alloc_grid_component();
        assign_name(app.mod, grid, compo);
    }

    ImGui::SameLine();
    if (app.mod.graph_components.can_alloc() && ImGui::Button("+ graph")) {
        auto& compo = app.mod.alloc_graph_component();
        assign_name(app.mod, grid, compo);
    }

    ImGui::EndDisabled();
}

static void show_grid(application&                app,
                      grid_component_editor_data& ed,
                      grid_component&             data) noexcept
{

    static ImVec2 scrolling(0.0f, 0.0f);
    static int    v[2] = { 50, 50 };

    float item_width  = v[0];
    float item_height = v[1];

    if (ImGui::InputInt2("Column and row size", v)) {
        item_width  = static_cast<float>(v[0] < 5     ? v[0]     = 5
                                         : v[0] > 100 ? v[0] = 100
                                                      : v[0]);
        item_height = static_cast<float>(v[1] < 5     ? v[1]     = 5
                                         : v[1] > 100 ? v[1] = 100
                                                      : v[1]);
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

    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                            mouse_threshold_for_pan)) {
        scrolling.x += io.MouseDelta.x;
        scrolling.y += io.MouseDelta.y;
    }

    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (drag_delta.x == 0.0f && drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("Canvas-Context",
                                    ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginPopup("Canvas-Context")) {
        ImGui::MenuItem("Connect to grid input port");
        // Loop over component.x_names

        ImGui::MenuItem("Connect to grid output port");
        // Loop over component.y_names

        ImGui::EndPopup();
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

    for (int row = 0; row < data.row; ++row) {
        for (int col = 0; col < data.column; ++col) {
            ImVec2 p_min(origin.x + col * item_width,
                         origin.y + row * item_height);
            ImVec2 p_max(origin.x + ((col + 1) * item_width) - 3,
                         origin.y + ((row + 1) * item_height) - 3);

            if (p_min.x <= io.MousePos.x && io.MousePos.x < p_max.x &&
                p_min.y <= io.MousePos.y && io.MousePos.y < p_max.y &&
                ImGui::IsMouseReleased(0)) {
                data.children[data.pos(row, col)] = ed.selected_id;
            }

            draw_list->AddRectFilled(
              p_min,
              p_max,
              to_ImU32(app.mod.component_colors[get_index(
                data.children[data.pos(row, col)])]));
        }
    }

    draw_list->PopClipRect();
}

grid_component_editor_data::grid_component_editor_data(
  const component_id      id_,
  const grid_component_id grid_id_) noexcept
  : grid_id(grid_id_)
  , m_id(id_)
{}

void grid_component_editor_data::clear() noexcept
{
    selected.clear();
    scale = 10.f;

    grid_id = undefined<grid_component_id>();
    m_id    = undefined<component_id>();
}

void grid_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app   = container_of(&ed, &application::component_ed);
    auto* compo = app.mod.components.try_to_get(m_id);
    auto* grid  = app.mod.grid_components.try_to_get(grid_id);

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

    show_grid_component_options(*grid);
    show_grid_editor_options(app, *this, *grid);

    show_grid(app, *this, *grid);
}

void grid_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/) noexcept
{}

grid_editor_dialog::grid_editor_dialog() noexcept
{
    grid.resize(5, 5, undefined<component_id>());
}

void grid_editor_dialog::load(application&       app_,
                              generic_component* compo_) noexcept
{
    app        = &app_;
    compo      = compo_;
    is_running = true;
    is_ok      = false;
}

void grid_editor_dialog::save() noexcept
{
    irt_assert(app && compo);

    if (auto ret = app->mod.copy(grid, *compo); !ret) {
        auto& n = app->notifications.alloc();
        n.title = "Fail to save grid";
        app->notifications.enable(n);
    }
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

        show_grid_component_options(grid);
        // show_grid_editor_options(
        //   container_of(this, &application::grid_dlg), *this, grid);
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
