// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>

#include "application.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imnodes.h>

namespace irt {

constexpr auto to_C(const rgba_color x) noexcept -> int
{
    return IM_COL32(x.r, x.g, x.b, x.a);
}

constexpr auto to(const rgba_color x) noexcept -> int
{
    return IM_COL32(x.r, x.g, x.b, 255);
}

static auto display_themes_selector(application& app) noexcept -> bool
{
    auto theme_id     = undefined<gui_theme_id>();
    auto old_theme_id = undefined<gui_theme_id>();

    {
        auto config               = app.config.get();
        old_theme_id              = config.vars().g_themes.selected;
        theme_id                  = old_theme_id;
        const char* previous_name = "-";

        if (config.vars().g_themes.ids.exists(old_theme_id)) {
            const auto selected_idx = get_index(old_theme_id);
            previous_name = config.vars().g_themes.names[selected_idx].c_str();
        } else {
            theme_id = undefined<gui_theme_id>();
        }

        if (ImGui::BeginCombo("Choose style", previous_name)) {
            for (const auto id : config.vars().g_themes.ids) {
                const auto  idx  = get_index(id);
                const auto& name = config.vars().g_themes.names[idx];

                if (ImGui::Selectable(name.c_str(), id == theme_id)) {
                    theme_id = id;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (old_theme_id != theme_id) {
        auto config_rw                     = app.config.get_rw();
        config_rw.vars().g_themes.selected = theme_id;
        return true;
    }

    return false;
}

void settings_window::show() noexcept
{
    ImGui::SetNextWindowPos(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_Once);

    if (!ImGui::Begin(settings_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Dir paths");

    static const char* dir_status[] = { "none", "read", "unread", "error" };

    auto& app = container_of(this, &application::settings_wnd);

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
        while (app.mod.registred_paths.next(dir)) {
            if (to_delete) {
                const auto id = app.mod.registred_paths.get_id(*to_delete);
                app.mod.registred_paths.free(*to_delete);
                to_delete = nullptr;

                i32 i = 0, e = app.mod.component_repertories.ssize();
                for (; i != e; ++i) {
                    if (app.mod.component_repertories[i] == id) {
                        app.mod.component_repertories.swap_pop_back(i);
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
                attempt_all(
                  [&]() noexcept -> status {
                      irt_check(app.mod.fill_components(*dir));
                      return success();
                  },

                  [&app](const json_archiver::error_code s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Refresh components from directory failed";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&app](const modeling::part s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Refresh components from directory failed";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&app]() noexcept -> void {
                      auto& n   = app.notifications.alloc();
                      n.title   = "Refresh components from directory failed";
                      n.message = "Error: Unknown";
                      app.notifications.enable(n);
                  });
            }
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            if (dir->status != registred_path::state::lock)
                if (ImGui::Button("Delete"))
                    to_delete = dir;
            ImGui::PopItemWidth();

            ImGui::PopID();
        }

        if (to_delete) {
            app.mod.free(*to_delete);
        }

        ImGui::EndTable();

        if (app.mod.registred_paths.can_alloc(1) &&
            ImGui::Button("Add directory")) {
            auto& dir    = app.mod.registred_paths.alloc();
            auto  id     = app.mod.registred_paths.get_id(dir);
            dir.status   = registred_path::state::unread;
            dir.path     = "";
            dir.priority = 127;
            app.show_select_directory_dialog = true;
            app.select_dir_path              = id;
            app.mod.component_repertories.emplace_back(id);
        }
    }

    ImGui::Separator();
    ImGui::Text("Graphics");

    if (display_themes_selector(app))
        apply_style(undefined<gui_theme_id>());

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

void settings_window::apply_style(gui_theme_id id) noexcept
{
    auto& app = container_of(this, &application::settings_wnd);

    const auto& config = app.config.get();
    if (not config.vars().g_themes.ids.exists(id)) {
        id = config.vars().g_themes.selected;

        if (not config.vars().g_themes.ids.exists(id)) {
            debug::ensure(config.vars().g_themes.ids.size() > 0);

            id = config.vars().g_themes.ids.begin().id;
        }
    }

    const auto  idx = get_index(id);
    const auto& src = config.vars().g_themes.colors[idx];

    debug::ensure(config.vars().g_themes.ids.exists(id));
    ImVec4* colors = ImGui::GetStyle().Colors;

    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        colors[i].x = static_cast<float>(src[i].r) / 255.f;
        colors[i].y = static_cast<float>(src[i].g) / 255.f;
        colors[i].z = static_cast<float>(src[i].b) / 255.f;
        colors[i].w = static_cast<float>(src[i].a) / 255.f;
    }

    auto* c = ImNodes::GetStyle().Colors;
    c[0]    = to_C(src[ImGuiCol_ScrollbarGrabHovered]);
    c[1]    = to_C(src[ImGuiCol_ScrollbarGrab]);
    c[2]    = to_C(src[ImGuiCol_ScrollbarGrab]);
    c[3]    = to_C(src[ImGuiCol_ScrollbarBg]);
    c[4]    = to_C(src[ImGuiCol_TitleBg]);
    c[5]    = to_C(src[ImGuiCol_TitleBgActive]);
    c[6]    = to_C(src[ImGuiCol_TitleBgCollapsed]);
    c[7]    = to_C(src[ImGuiCol_SliderGrab]);
    c[8]    = to_C(src[ImGuiCol_SliderGrabActive]);
    c[9]    = to_C(src[ImGuiCol_SliderGrabActive]);
    c[10]   = to_C(src[ImGuiCol_Button]);
    c[11]   = to_C(src[ImGuiCol_ButtonHovered]);
    c[12]   = to_C(src[ImGuiCol_ResizeGripHovered]);
    c[13]   = to_C(src[ImGuiCol_ResizeGrip]);
    c[14]   = to_C(lerp(src[ImGuiCol_WindowBg], src[ImGuiCol_Text], 0.10f));
    c[15]   = to_C(lerp(src[ImGuiCol_WindowBg], src[ImGuiCol_Text], 0.20f));
    c[16]   = to_C(lerp(src[ImGuiCol_WindowBg], src[ImGuiCol_Text], 0.30f));

    c[17] = to_C(src[ImGuiCol_ModalWindowDimBg]);
    c[18] = to_C(src[ImGuiCol_NavWindowingHighlight]);
    c[19] = to_C(src[ImGuiCol_TableRowBg]);
    c[20] = to_C(src[ImGuiCol_TableRowBgAlt]);
    c[21] = to_C(src[ImGuiCol_ResizeGripHovered]);
    c[22] = to_C(src[ImGuiCol_ResizeGripActive]);
    c[23] = to_C(src[ImGuiCol_TabHovered]);
    c[24] = to_C(src[ImGuiCol_Tab]);
    c[25] = to_C(src[ImGuiCol_TabSelected]);
    c[26] = to_C(src[ImGuiCol_TabSelectedOverline]);
    c[27] = to_C(src[ImGuiCol_TextSelectedBg]);
    c[28] = to_C(src[ImGuiCol_TextLink]);

    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding  = 20.0f;
    ImGui::GetStyle().GrabMinSize   = 10.0f;

    constexpr rgba_color red1(u8{ 16 }, u8{ 0 }, u8{ 0 }, u8{ 255 });
    constexpr rgba_color red2(u8{ 32 }, u8{ 0 }, u8{ 0 }, u8{ 255 });
    constexpr rgba_color red3(u8{ 48 }, u8{ 0 }, u8{ 0 }, u8{ 255 });
    constexpr rgba_color green1(u8{ 0 }, u8{ 16 }, u8{ 0 }, u8{ 255 });
    constexpr rgba_color green2(u8{ 0 }, u8{ 32 }, u8{ 0 }, u8{ 255 });
    constexpr rgba_color green3(u8{ 0 }, u8{ 48 }, u8{ 0 }, u8{ 255 });

    gui_model_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           red1);
    gui_hovered_model_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           red2);
    gui_selected_model_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           red3);
    gui_component_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           green1);
    gui_hovered_component_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           green2);
    gui_selected_component_color =
      to_C(lerp(src[ImGuiCol_TitleBg], src[ImNodesCol_TitleBarHovered], 0.50f) +
           green3);
}

} // irt
