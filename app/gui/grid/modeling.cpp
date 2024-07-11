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
    "in-out (in - out port)",
    "name (N, W, S, E, NW, ... port)",
    "number (4, 6, 44, 45, .., 66, port)",
};

constexpr static inline auto grid_type_count = 3;

static const char* grid_neighborhood[] = { "four", "eight" };

constexpr static inline auto grid_neighborhood_count = 2;

constexpr inline auto get_default_component_id(const grid_component& g) noexcept
  -> component_id
{
    return g.children.empty() ? undefined<component_id>() : g.children.front();
}

static bool show_row_column_widgets(grid_component& grid) noexcept
{
    bool is_changed = false;

    int row = grid.row;
    if (ImGui::InputInt(
          "row", &row, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
        row        = std::clamp(row, 1, 256);
        is_changed = row != grid.row;
        grid.row   = row;
    }

    int column = grid.column;
    if (ImGui::InputInt(
          "column", &column, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
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

    {
        int selected_type = ordinal(grid.out_connection_type);
        if (ImGui::Combo("Output connection type",
                         &selected_type,
                         grid_type,
                         grid_type_count)) {
            if (selected_type != ordinal(grid.out_connection_type))
                grid.out_connection_type =
                  enum_cast<grid_component::type>(selected_type);
        }
    }

    {
        int selected_type = ordinal(grid.in_connection_type);
        if (ImGui::Combo("Input connection type",
                         &selected_type,
                         grid_type,
                         grid_type_count)) {
            if (selected_type != ordinal(grid.in_connection_type))
                grid.in_connection_type =
                  enum_cast<grid_component::type>(selected_type);
        }
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

static bool get_or_add_x(component& compo, std::string_view name) noexcept
{
    if (is_defined(compo.get_x(name)))
        return true;

    if (compo.x.can_alloc(1)) {
        compo.x.alloc([&](auto /*id*/, auto& str) { str = name; });
        return true;
    }

    return false;
}

static bool get_or_add_y(component& compo, std::string_view name) noexcept
{
    if (is_defined(compo.get_y(name)))
        return true;

    if (compo.y.can_alloc(1)) {
        compo.y.alloc([&](auto /*id*/, auto& str) { str = name; });
        return true;
    }

    return false;
}

static bool new_in_out_x(component& compo) noexcept
{
    return get_or_add_x(compo, "in");
}

static bool new_in_out_y(component& compo) noexcept
{
    return get_or_add_y(compo, "out");
}

static bool new_number_4_x(component& compo) noexcept
{
    return get_or_add_x(compo, "45") and get_or_add_x(compo, "54") and
           get_or_add_x(compo, "56") and get_or_add_x(compo, "65");
}

static bool new_number_4_y(component& compo) noexcept
{
    return get_or_add_y(compo, "45") and get_or_add_y(compo, "54") and
           get_or_add_y(compo, "56") and get_or_add_y(compo, "65");
}

static bool new_name_4_x(component& compo) noexcept
{
    return get_or_add_x(compo, "N") and get_or_add_x(compo, "S") and
           get_or_add_x(compo, "W") and get_or_add_x(compo, "E");
}

static bool new_name_4_y(component& compo) noexcept
{
    return get_or_add_y(compo, "N") and get_or_add_y(compo, "S") and
           get_or_add_y(compo, "W") and get_or_add_y(compo, "E");
}

static bool new_number_8_x(component& compo) noexcept
{
    return new_number_4_x(compo) and get_or_add_x(compo, "44") and
           get_or_add_x(compo, "46") and get_or_add_x(compo, "64") and
           get_or_add_x(compo, "66");
}

static bool new_number_8_y(component& compo) noexcept
{
    return new_number_4_y(compo) and get_or_add_y(compo, "44") and
           get_or_add_y(compo, "46") and get_or_add_y(compo, "64") and
           get_or_add_y(compo, "66");
}

static bool new_name_8_x(component& compo) noexcept
{
    return new_name_4_x(compo) and get_or_add_x(compo, "NW") and
           get_or_add_x(compo, "NE") and get_or_add_x(compo, "SW") and
           get_or_add_x(compo, "SE");
}

static bool new_name_8_y(component& compo) noexcept
{
    return new_name_4_y(compo) and get_or_add_y(compo, "NW") and
           get_or_add_y(compo, "NE") and get_or_add_y(compo, "SW") and
           get_or_add_y(compo, "SE");
}

static bool assign_name_x(grid_component& grid, component& compo) noexcept
{
    switch (grid.neighbors) {
    case grid_component::neighborhood::four:
        switch (grid.in_connection_type) {
        case grid_component::type::in_out:
            return new_in_out_x(compo);

        case grid_component::type::name:
            return new_name_4_x(compo);

        case grid_component::type::number:
            return new_number_4_x(compo);
        }

        unreachable();
        break;

    case grid_component::neighborhood::eight:
        switch (grid.in_connection_type) {
        case grid_component::type::in_out:
            return new_in_out_x(compo);

        case grid_component::type::name:
            return new_name_8_x(compo);

        case grid_component::type::number:
            return new_number_8_x(compo);
        }

        unreachable();
        break;
    }

    unreachable();
}

static bool assign_name_y(grid_component& grid, component& compo) noexcept
{
    switch (grid.neighbors) {
    case grid_component::neighborhood::four:
        switch (grid.out_connection_type) {
        case grid_component::type::in_out:
            return new_in_out_y(compo);

        case grid_component::type::name:
            return new_name_4_y(compo);

        case grid_component::type::number:
            return new_number_4_y(compo);
        }

        unreachable();
        break;

    case grid_component::neighborhood::eight:
        switch (grid.out_connection_type) {
        case grid_component::type::in_out:
            return new_in_out_y(compo);

        case grid_component::type::name:
            return new_name_8_y(compo);

        case grid_component::type::number:
            return new_number_8_y(compo);
        }

        unreachable();
        break;
    }

    unreachable();
}

static bool assign_name(grid_component& grid, component& compo) noexcept
{
    return assign_name_x(grid, compo) and assign_name_y(grid, compo);
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
        assign_name(grid, compo);
    }

    ImGui::SameLine();
    if (app.mod.grid_components.can_alloc() && ImGui::Button("+ grid")) {
        auto& compo = app.mod.alloc_grid_component();
        assign_name(grid, compo);
    }

    ImGui::SameLine();
    if (app.mod.graph_components.can_alloc() && ImGui::Button("+ graph")) {
        auto& compo = app.mod.alloc_graph_component();
        assign_name(grid, compo);
    }

    ImGui::EndDisabled();
}

static void show_grid(application&                app,
                      component&                  compo,
                      grid_component_editor_data& ed,
                      grid_component&             data) noexcept
{
    if (ImGui::InputFloat2("Zoom", ed.zoom)) {
        ed.zoom[0] = ImClamp(ed.zoom[0], 0.1f, 10.f);
        ed.zoom[1] = ImClamp(ed.zoom[1], 0.1f, 10.f);
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

    const ImVec2 origin(canvas_p0.x + ed.scrolling.x,
                        canvas_p0.y + ed.scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                            mouse_threshold_for_pan)) {
        ed.scrolling.x += io.MouseDelta.x;
        ed.scrolling.y += io.MouseDelta.y;
    }

    if (is_hovered and io.MouseWheel != 0.f) {
        ed.zoom[0] = ed.zoom[0] + (io.MouseWheel * ed.zoom[0] * 0.1f);
        ed.zoom[1] = ed.zoom[1] + (io.MouseWheel * ed.zoom[1] * 0.1f);
        ed.zoom[0] = ImClamp(ed.zoom[0], 0.1f, 10.f);
        ed.zoom[1] = ImClamp(ed.zoom[1], 0.1f, 10.f);
    }

    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);

    if (drag_delta.x == 0.0f && drag_delta.y == 0.0f and
        ed.hovered_component == nullptr) {
        ed.col =
          static_cast<int>(ImFloor((io.MousePos.x - origin.x) / ed.zoom[0]));
        ed.row =
          static_cast<int>(ImFloor((io.MousePos.y - origin.y) / ed.zoom[1]));

        if (0 <= ed.row and ed.row < data.row and 0 <= ed.col and
            ed.col < data.column) {
            ed.hovered_component = app.mod.components.try_to_get(
              data.children[data.pos(ed.row, ed.col)]);
        }
    }

    if (ImGui::BeginPopupContextItem("Canvas-Context")) {
        auto deselect = false;

        if (ImGui::BeginMenu("Menu##compo")) {
            if (ImGui::BeginMenu("Connect to grid input port")) {
                if (ed.hovered_component) {
                    ed.hovered_component->x.for_each([&](auto  s_id,
                                                         auto& s_name) {
                        ImGui::PushID(ordinal(s_id));

                        compo.x.for_each([&](auto id, auto& name) noexcept {
                            ImGui::PushID(ordinal(id));
                            small_string<128> str;

                            format(str,
                                   "grid port {} to {}",
                                   name.sv(),
                                   s_name.sv());

                            if (ImGui::MenuItem(str.c_str())) {
                                auto ret =
                                  data.connect_input(s_id, ed.row, ed.col, id);
                                if (!ret) {
                                    auto& n = app.notifications.alloc();
                                    n.title = "Fail to connect input";
                                    app.notifications.enable(n);
                                }
                                deselect = true;
                            }
                            ImGui::PopID();
                        });

                        ImGui::PopID();
                    });
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Connect to grid output port")) {
                if (ed.hovered_component) {
                    ed.hovered_component->y.for_each([&](auto  s_id,
                                                         auto& s_name) {
                        ImGui::PushID(ordinal(s_id));

                        compo.y.for_each([&](auto id, auto& name) noexcept {
                            ImGui::PushID(ordinal(id));
                            small_string<128> str;

                            format(str,
                                   "{} to grid port {}",
                                   s_name.sv(),
                                   name.sv());

                            if (ImGui::MenuItem(str.c_str())) {
                                auto ret =
                                  data.connect_output(s_id, ed.row, ed.col, id);
                                if (!ret) {
                                    auto& n = app.notifications.alloc();
                                    n.title = "Fail to connect output";
                                    app.notifications.enable(n);
                                }
                                deselect = true;
                            }
                            ImGui::PopID();
                        });

                        ImGui::PopID();
                    });
                }

                ImGui::EndMenu();
            }

            if (deselect)
                ed.hovered_component = nullptr;
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::OpenPopupOnItemClick("Canvas-Context",
                                ImGuiPopupFlags_MouseButtonRight);

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    const float GRID_STEP = 64.0f;

    for (float x = fmodf(ed.scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           IM_COL32(200, 200, 200, 40));

    for (float y = fmodf(ed.scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           IM_COL32(200, 200, 200, 40));

    for (int row = 0; row < data.row; ++row) {
        for (int col = 0; col < data.column; ++col) {
            ImVec2 p_min(
              origin.x + (col * (ed.distance.x + ed.size.x) * ed.zoom[0]),
              origin.y + (row * (ed.distance.y + ed.size.y) * ed.zoom[1]));

            ImVec2 p_max(p_min.x + ed.zoom[0] * ed.size.x,
                         p_min.y + ed.zoom[1] * ed.size.y);

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
    zoom[0]           = 1.f;
    zoom[1]           = 1.f;
    hovered_component = nullptr;

    grid_id = undefined<grid_component_id>();
    m_id    = undefined<component_id>();
}

void grid_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app   = container_of(&ed, &application::component_ed);
    auto* compo = app.mod.components.try_to_get(m_id);
    auto* grid  = app.mod.grid_components.try_to_get(grid_id);

    debug::ensure(compo && grid);

    if (show_row_column_widgets(*grid)) {
        grid->resize(grid->row, grid->column, get_default_component_id(*grid));
    }

    show_grid_component_options(*grid);
    show_grid_editor_options(app, *this, *grid);
    show_grid(app, *compo, *this, *grid);
}

void grid_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/) noexcept
{}

bool grid_component_editor_data::need_show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    return false;
}

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
    debug::ensure(app && compo);

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
