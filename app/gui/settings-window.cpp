// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static ImVec4 operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

void settings_window::update() noexcept
{
    gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.25f);
    gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.5f);

    gui_hovered_component_color =
      ImGui::ColorConvertFloat4ToU32(gui_component_color * 1.25f);
    gui_selected_component_color =
      ImGui::ColorConvertFloat4ToU32(gui_component_color * 1.5f);
}

void settings_window::show() noexcept
{
    if (!is_open)
        return;

    ImGui::SetNextWindowPos(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_Once);

    if (!ImGui::Begin(settings_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Dir paths");

    static const char* dir_status[] = { "none", "read", "unread" };

    auto* app      = container_of(this, &application::settings);
    auto& c_editor = app->c_editor;

    if (ImGui::BeginTable("Component directories", 6)) {
        ImGui::TableSetupColumn(
          "Path", ImGuiTableColumnFlags_WidthStretch, -FLT_MIN);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Refresh", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        registred_path* dir       = nullptr;
        registred_path* to_delete = nullptr;
        while (c_editor.mod.registred_paths.next(dir)) {
            if (to_delete) {
                const auto id = c_editor.mod.registred_paths.get_id(*to_delete);
                c_editor.mod.registred_paths.free(*to_delete);
                to_delete = nullptr;

                i32 i = 0, e = c_editor.mod.component_repertories.ssize();
                for (; i != e; ++i) {
                    if (c_editor.mod.component_repertories[i] == id) {
                        c_editor.mod.component_repertories.swap_pop_back(i);
                        break;
                    }
                }
            }

            ImGui::PushID(dir);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputSmallString(
              "##path", dir->path, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(150.f);
            ImGui::InputSmallString("##name", dir->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            constexpr i8 p_min = INT8_MIN;
            constexpr i8 p_max = INT8_MAX;
            ImGui::SliderScalar(
              "##input", ImGuiDataType_S8, &dir->priority, &p_min, &p_max);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            ImGui::TextUnformatted(dir_status[ordinal(dir->status)]);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            if (ImGui::Button("Refresh")) {
                c_editor.mod.fill_components(*dir);
            }
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            if (ImGui::Button("Delete"))
                to_delete = dir;
            ImGui::PopItemWidth();

            ImGui::PopID();
        }

        if (to_delete) {
            c_editor.mod.free(*to_delete);
        }

        ImGui::EndTable();

        if (c_editor.mod.registred_paths.can_alloc(1) &&
            ImGui::Button("Add directory")) {
            auto& dir    = c_editor.mod.registred_paths.alloc();
            auto  id     = c_editor.mod.registred_paths.get_id(dir);
            dir.status   = registred_path::state::none;
            dir.path     = "";
            dir.priority = 127;
            app->show_select_directory_dialog = true;
            app->select_dir_path              = id;
            c_editor.mod.component_repertories.emplace_back(id);
        }
    }

    ImGui::Separator();
    ImGui::Text("Graphics");

    {
        if (ImGui::Combo(
              "Style selector", &style_selector, "Dark\0Light\0Classic\0")) {
            switch (style_selector) {
            case 0:
                ImGui::StyleColorsDark();
                ImNodes::StyleColorsDark();
                break;
            case 1:
                ImGui::StyleColorsLight();
                ImNodes::StyleColorsLight();
                break;
            case 2:
                ImGui::StyleColorsClassic();
                ImNodes::StyleColorsClassic();
                break;
            }
        }
    }

    if (ImGui::ColorEdit3(
          "model", (float*)&gui_model_color, ImGuiColorEditFlags_NoOptions))
        update();

    if (ImGui::ColorEdit3("component",
                          (float*)&gui_component_color,
                          ImGuiColorEditFlags_NoOptions))
        update();

    ImGui::Separator();
    ImGui::Text("Automatic layout parameters");
    ImGui::DragInt(
      "max iteration", &automatic_layout_iteration_limit, 1.f, 0, 1000);
    ImGui::DragFloat(
      "a-x-distance", &automatic_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "a-y-distance", &automatic_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::Separator();
    ImGui::Text("Grid layout parameters");
    ImGui::DragFloat(
      "g-x-distance", &grid_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "g-y-distance", &grid_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::End();
}

} // irt