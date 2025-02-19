// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>

#include "application.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imnodes.h>

namespace irt {

static ImVec4 operator*(const ImVec4& vec, float val) noexcept
{
    return ImVec4(vec.x * val, vec.y * val, vec.z * val, vec.w);
}

static ImVec4 alpha(const ImVec4& vec, const float f) noexcept
{
    return ImVec4(vec.x, vec.y, vec.z, f);
}

static auto display_themes_selector(application& app) noexcept -> bool
{
    auto theme_id     = undefined<gui_theme_id>();
    auto old_theme_id = undefined<gui_theme_id>();

    app.config.try_read(
      [](auto& vars, auto& theme_id, auto& old_theme_id) {
          old_theme_id              = vars.g_themes.selected;
          theme_id                  = old_theme_id;
          const char* previous_name = "-";

          if (vars.g_themes.ids.exists(old_theme_id)) {
              const auto selected_idx = get_index(old_theme_id);
              previous_name = vars.g_themes.names[selected_idx].c_str();
          } else {
              theme_id = undefined<gui_theme_id>();
          }

          if (ImGui::BeginCombo("Choose style", previous_name)) {
              for (const auto id : vars.g_themes.ids) {
                  const auto  idx  = get_index(id);
                  const auto& name = vars.g_themes.names[idx];

                  if (ImGui::Selectable(name.c_str(), id == theme_id)) {
                      theme_id = id;
                  }
              }
              ImGui::EndCombo();
          }
      },
      theme_id,
      old_theme_id);

    if (old_theme_id != theme_id) {
        app.config.read_write(
          [](auto& vars, auto theme_id) noexcept {
              vars.g_themes.selected = theme_id;
          },
          theme_id);

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

    auto& app     = container_of(this, &application::settings_wnd);
    int   changes = false;

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
            changes += ImGui::InputSmallString(
              "##path", dir->path, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(150.f);
            changes += ImGui::InputSmallString("##name", dir->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            constexpr i8 p_min = INT8_MIN;
            constexpr i8 p_max = INT8_MAX;
            changes += ImGui::SliderScalar(
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
            changes++;
            app.mod.free(*to_delete);
        }

        ImGui::EndTable();

        if (app.mod.registred_paths.can_alloc(1) &&
            ImGui::Button("Add directory")) {
            auto& ndir    = app.mod.registred_paths.alloc();
            auto  id      = app.mod.registred_paths.get_id(ndir);
            ndir.status   = registred_path::state::unread;
            ndir.path     = "";
            ndir.priority = 127;
            app.request_open_directory_dlg(id);
            app.mod.component_repertories.emplace_back(id);
            changes++;
        }
    }

    ImGui::Separator();
    ImGui::Text("Graphics");

    if (display_themes_selector(app)) {
        apply_style(undefined<gui_theme_id>());
        changes++;
    }

    ImGui::Separator();
    ImGui::Text("Automatic layout parameters");
    changes += ImGui::DragInt(
      "max iteration", &automatic_layout_iteration_limit, 1.f, 0, 1000);
    changes += ImGui::DragFloat(
      "a-x-distance", &automatic_layout_x_distance, 1.f, 150.f, 500.f);
    changes += ImGui::DragFloat(
      "a-y-distance", &automatic_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::Separator();
    ImGui::Text("Grid layout parameters");
    changes += ImGui::DragFloat(
      "g-x-distance", &grid_layout_x_distance, 1.f, 150.f, 500.f);
    changes += ImGui::DragFloat(
      "g-y-distance", &grid_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::End();

    if (changes > 0) {
        timer_started = true;
        last_change   = std::chrono::steady_clock::now();
    } else if (timer_started) {
        const auto end_at   = std::chrono::steady_clock::now();
        const auto duration = end_at - last_change;

        if (duration > std::chrono::seconds(5)) {
            timer_started = false;
            app.add_gui_task([&app]() noexcept {
                if (auto ret = app.config.save(); ret) {
                    app.notifications.try_insert(
                      log_level::error, [ret](auto& title, auto& msg) noexcept {
                          title = "Settings save failure";
                          format(msg, "Error in {}", ret.message());
                      });
                } else {
                    app.notifications.try_insert(
                      log_level::info, [](auto& title, auto&) noexcept {
                          title = "Settings save success";
                      });
                }
            });
        }
    }
}

void apply_theme_Modern_colors() noexcept;
void apply_theme_DarkTheme_colors() noexcept;
void apply_theme_LightTheme_colors() noexcept;
void apply_theme_BessDarkTheme_colors() noexcept;
void apply_theme_CatpuccinMocha_colors() noexcept;
void apply_theme_MaterialYou_colors() noexcept;
void apply_theme_FluentUITheme_colors() noexcept;
void apply_theme_FluentUILightTheme_colors() noexcept;

void apply_imnodes_theme_colors() noexcept;

void settings_window::apply_style(const gui_theme_id id) noexcept
{
    auto& app = container_of(this, &application::settings_wnd);
    auto  idx = 0u;

    if (is_undefined(id)) {
        app.config.read(
          [](auto& vars, auto& idx) noexcept {
              idx = get_index(vars.g_themes.selected);
          },
          idx);
    } else {
        idx = get_index(id);
    }

    switch (idx) {
    case 1:
        apply_theme_DarkTheme_colors();
        break;
    case 2:
        apply_theme_LightTheme_colors();
        break;
    case 3:
        apply_theme_BessDarkTheme_colors();
        break;
    case 4:
        apply_theme_CatpuccinMocha_colors();
        break;
    case 5:
        apply_theme_MaterialYou_colors();
        break;
    case 6:
        apply_theme_FluentUITheme_colors();
        break;
    case 7:
        apply_theme_FluentUILightTheme_colors();
        break;
    default:
        apply_theme_Modern_colors();
        break;
    }

    apply_imnodes_theme_colors();
}

void apply_imnodes_theme_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;
    auto*       c      = ImNodes::GetStyle().Colors;

    c[ImNodesCol_NodeBackground] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_Tab]);
    c[ImNodesCol_NodeBackgroundHovered] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_TabHovered]);
    c[ImNodesCol_NodeBackgroundSelected] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_TabSelected]);
    c[ImNodesCol_NodeOutline] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_TabSelectedOverline]);

    c[ImNodesCol_TitleBar] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_Separator]);
    c[ImNodesCol_TitleBarHovered] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_SeparatorHovered]);
    c[ImNodesCol_TitleBarSelected] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_SeparatorActive]);

    c[ImNodesCol_Link] = ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_Tab]);
    c[ImNodesCol_LinkHovered] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_TabHovered]);
    c[ImNodesCol_LinkSelected] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_TabSelected]);

    c[ImNodesCol_Pin] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_Separator]);
    c[ImNodesCol_PinHovered] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_SeparatorHovered]);

    c[ImNodesCol_BoxSelector] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_Header], 0.5f)); //?
    c[ImNodesCol_BoxSelectorOutline] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_Header]); //?

    c[ImNodesCol_GridBackground] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_WindowBg]);
    c[ImNodesCol_GridLine] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_MenuBarBg]);
    c[ImNodesCol_GridLinePrimary] =
      ImGui::ColorConvertFloat4ToU32(colors[ImGuiCol_MenuBarBg]);

    c[ImNodesCol_MiniMapBackground] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_WindowBg] * 0.66f, 0.8f));
    c[ImNodesCol_MiniMapBackgroundHovered] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_WindowBg] * 0.5f, 0.8f));
    c[ImNodesCol_MiniMapOutline] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_WindowBg] * 0.66f, 0.8f));
    c[ImNodesCol_MiniMapOutlineHovered] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_WindowBg] * 0.66f, 0.8f));

    c[ImNodesCol_MiniMapNodeBackground] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_Tab] * 0.66f, 0.8f));
    c[ImNodesCol_MiniMapNodeBackgroundHovered] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_TabHovered] * 0.66f, 0.8f));
    c[ImNodesCol_MiniMapNodeBackgroundSelected] =
      ImGui::ColorConvertFloat4ToU32(
        alpha(colors[ImGuiCol_TabSelected] * 0.66f, 0.8f));
    c[ImNodesCol_MiniMapNodeOutline] = ImGui::ColorConvertFloat4ToU32(
      alpha(colors[ImGuiCol_TabSelectedOverline] * 0.66f, 0.1f));

    c[ImNodesCol_MiniMapLink] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_Tab], 0.8f));
    c[ImNodesCol_MiniMapLinkSelected] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_TabSelected], 0.8f));
    c[ImNodesCol_MiniMapCanvas] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_WindowBg], 0.8f));
    c[ImNodesCol_MiniMapCanvasOutline] =
      ImGui::ColorConvertFloat4ToU32(alpha(colors[ImGuiCol_WindowBg], 0.8f));
}

// theme

void apply_theme_CatpuccinMocha_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Base colors inspired by Catppuccin Mocha
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.89f, 0.88f, 1.00f); // Latte
    colors[ImGuiCol_TextDisabled] =
      ImVec4(0.60f, 0.56f, 0.52f, 1.00f);                           // Surface2
    colors[ImGuiCol_WindowBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_ChildBg]  = ImVec4(0.18f, 0.16f, 0.22f, 1.00f); // Mantle
    colors[ImGuiCol_PopupBg]  = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_Border]   = ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]      = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_FrameBgHovered] =
      ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
    colors[ImGuiCol_FrameBgActive] =
      ImVec4(0.26f, 0.22f, 0.31f, 1.00f);                          // Overlay2
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f); // Mantle
    colors[ImGuiCol_TitleBgActive] =
      ImVec4(0.17f, 0.15f, 0.21f, 1.00f); // Mantle
    colors[ImGuiCol_TitleBgCollapsed] =
      ImVec4(0.14f, 0.12f, 0.18f, 1.00f);                              // Mantle
    colors[ImGuiCol_MenuBarBg]   = ImVec4(0.17f, 0.15f, 0.22f, 1.00f); // Base
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_ScrollbarGrab] =
      ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_ScrollbarGrabHovered] =
      ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
    colors[ImGuiCol_ScrollbarGrabActive] =
      ImVec4(0.26f, 0.22f, 0.31f, 1.00f);                            // Overlay2
    colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_SliderGrab] =
      ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_SliderGrabActive] =
      ImVec4(0.89f, 0.54f, 0.79f, 1.00f);                         // Pink
    colors[ImGuiCol_Button] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f); // Maroon
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f); // Red
    colors[ImGuiCol_ButtonActive]  = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_Header] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f); // Maroon
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f); // Red
    colors[ImGuiCol_HeaderActive]  = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_Separator] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_SeparatorHovered] =
      ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_SeparatorActive] =
      ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_ResizeGrip] =
      ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_ResizeGripHovered] =
      ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_ResizeGripActive] =
      ImVec4(0.92f, 0.61f, 0.85f, 1.00f);                      // Mauve
    colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_TabHovered] =
      ImVec4(0.82f, 0.61f, 0.85f, 1.00f);                            // Lavender
    colors[ImGuiCol_TabActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_TabUnfocused] =
      ImVec4(0.18f, 0.16f, 0.22f, 1.00f); // Mantle
    colors[ImGuiCol_TabUnfocusedActive] =
      ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_DockingPreview] =
      ImVec4(0.95f, 0.66f, 0.47f, 0.70f); // Peach
    colors[ImGuiCol_DockingEmptyBg] =
      ImVec4(0.12f, 0.12f, 0.12f, 1.00f);                            // Base
    colors[ImGuiCol_PlotLines] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_PlotLinesHovered] =
      ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_PlotHistogram] =
      ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_PlotHistogramHovered] =
      ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_TableHeaderBg] =
      ImVec4(0.19f, 0.19f, 0.20f, 1.00f); // Mantle
    colors[ImGuiCol_TableBorderStrong] =
      ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_TableBorderLight] =
      ImVec4(0.23f, 0.23f, 0.25f, 1.00f); // Surface2
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] =
      ImVec4(1.00f, 1.00f, 1.00f, 0.06f); // Surface0
    colors[ImGuiCol_TextSelectedBg] =
      ImVec4(0.82f, 0.61f, 0.85f, 0.35f); // Lavender
    colors[ImGuiCol_DragDropTarget] =
      ImVec4(0.95f, 0.66f, 0.47f, 0.90f); // Peach
    colors[ImGuiCol_NavHighlight] =
      ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Style adjustments
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.ChildRounding     = 4.0f;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowPadding    = ImVec2(8.0f, 8.0f);
    style.FramePadding     = ImVec2(5.0f, 4.0f);
    style.ItemSpacing      = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing    = 22.0f;

    style.ScrollbarSize = 14.0f;
    style.GrabMinSize   = 10.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
}

void apply_theme_MaterialYou_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Base colors inspired by Material You (dark mode)
    colors[ImGuiCol_Text]                  = ImVec4(0.93f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Border]                = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.45f, 0.76f, 0.29f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.22f, 0.52f, 0.91f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.29f, 0.70f, 0.91f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.29f, 0.62f, 0.91f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.29f, 0.62f, 0.91f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.29f, 0.62f, 0.91f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Style adjustments
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.ChildRounding     = 4.0f;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowPadding    = ImVec2(10.0f, 10.0f);
    style.FramePadding     = ImVec2(8.0f, 4.0f);
    style.ItemSpacing      = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing    = 22.0f;

    style.ScrollbarSize = 16.0f;
    style.GrabMinSize   = 10.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
}

void apply_theme_Modern_colors() noexcept { ImGui::StyleColorsClassic(); }
void apply_theme_DarkTheme_colors() noexcept { ImGui::StyleColorsDark(); }
void apply_theme_LightTheme_colors() noexcept { ImGui::StyleColorsLight(); }

void apply_theme_BessDarkTheme_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // Base colors for a pleasant and modern dark theme with dark accents
    colors[ImGuiCol_Text] =
      ImVec4(0.92f, 0.93f, 0.94f, 1.00f); // Light grey text for readability
    colors[ImGuiCol_TextDisabled] =
      ImVec4(0.50f, 0.52f, 0.54f, 1.00f); // Subtle grey for disabled text
    colors[ImGuiCol_WindowBg] =
      ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Dark background with a hint of blue
    colors[ImGuiCol_ChildBg] =
      ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Slightly lighter for child elements
    colors[ImGuiCol_PopupBg] =
      ImVec4(0.18f, 0.18f, 0.20f, 1.00f); // Popup background
    colors[ImGuiCol_Border] =
      ImVec4(0.28f, 0.29f, 0.30f, 0.60f); // Soft border color
    colors[ImGuiCol_BorderShadow] =
      ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No border shadow
    colors[ImGuiCol_FrameBg] =
      ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Frame background
    colors[ImGuiCol_FrameBgHovered] =
      ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Frame hover effect
    colors[ImGuiCol_FrameBgActive] =
      ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Active frame background
    colors[ImGuiCol_TitleBg] =
      ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Title background
    colors[ImGuiCol_TitleBgActive] =
      ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Active title background
    colors[ImGuiCol_TitleBgCollapsed] =
      ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Collapsed title background
    colors[ImGuiCol_MenuBarBg] =
      ImVec4(0.20f, 0.20f, 0.22f, 1.00f); // Menu bar background
    colors[ImGuiCol_ScrollbarBg] =
      ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Scrollbar background
    colors[ImGuiCol_ScrollbarGrab] =
      ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Dark accent for scrollbar grab
    colors[ImGuiCol_ScrollbarGrabHovered] =
      ImVec4(0.28f, 0.30f, 0.32f, 1.00f); // Scrollbar grab hover
    colors[ImGuiCol_ScrollbarGrabActive] =
      ImVec4(0.32f, 0.34f, 0.36f, 1.00f); // Scrollbar grab active
    colors[ImGuiCol_CheckMark] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Dark blue checkmark
    colors[ImGuiCol_SliderGrab] =
      ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Dark blue slider grab
    colors[ImGuiCol_SliderGrabActive] =
      ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Active slider grab
    colors[ImGuiCol_Button] =
      ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Dark blue button
    colors[ImGuiCol_ButtonHovered] =
      ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Button hover effect
    colors[ImGuiCol_ButtonActive] =
      ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active button
    colors[ImGuiCol_Header] =
      ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Header color similar to button
    colors[ImGuiCol_HeaderHovered] =
      ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Header hover effect
    colors[ImGuiCol_HeaderActive] =
      ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active header
    colors[ImGuiCol_Separator] =
      ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Separator color
    colors[ImGuiCol_SeparatorHovered] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for separator
    colors[ImGuiCol_SeparatorActive] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Active separator
    colors[ImGuiCol_ResizeGrip] =
      ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Resize grip
    colors[ImGuiCol_ResizeGripHovered] =
      ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for resize grip
    colors[ImGuiCol_ResizeGripActive] =
      ImVec4(0.44f, 0.54f, 0.64f, 1.00f); // Active resize grip
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Inactive tab
    colors[ImGuiCol_TabHovered] =
      ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Hover effect for tab
    colors[ImGuiCol_TabActive] =
      ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active tab color
    colors[ImGuiCol_TabUnfocused] =
      ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Unfocused tab
    colors[ImGuiCol_TabUnfocusedActive] =
      ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active but unfocused tab
    colors[ImGuiCol_DockingPreview] =
      ImVec4(0.24f, 0.34f, 0.44f, 0.70f); // Docking preview
    colors[ImGuiCol_DockingEmptyBg] =
      ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Empty docking background
    colors[ImGuiCol_PlotLines] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Plot lines
    colors[ImGuiCol_PlotLinesHovered] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for plot lines
    colors[ImGuiCol_PlotHistogram] =
      ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Histogram color
    colors[ImGuiCol_PlotHistogramHovered] =
      ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for histogram
    colors[ImGuiCol_TableHeaderBg] =
      ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table header background
    colors[ImGuiCol_TableBorderStrong] =
      ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Strong border for tables
    colors[ImGuiCol_TableBorderLight] =
      ImVec4(0.24f, 0.25f, 0.26f, 1.00f); // Light border for tables
    colors[ImGuiCol_TableRowBg] =
      ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table row background
    colors[ImGuiCol_TableRowBgAlt] =
      ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Alternate row background
    colors[ImGuiCol_TextSelectedBg] =
      ImVec4(0.24f, 0.34f, 0.44f, 0.35f); // Selected text background
    colors[ImGuiCol_DragDropTarget] =
      ImVec4(0.46f, 0.56f, 0.66f, 0.90f); // Drag and drop target
    colors[ImGuiCol_NavHighlight] =
      ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Navigation highlight
    colors[ImGuiCol_NavWindowingHighlight] =
      ImVec4(1.00f, 1.00f, 1.00f, 0.70f); // Windowing highlight
    colors[ImGuiCol_NavWindowingDimBg] =
      ImVec4(0.80f, 0.80f, 0.80f, 0.20f); // Dim background for windowing
    colors[ImGuiCol_ModalWindowDimBg] =
      ImVec4(0.80f, 0.80f, 0.80f, 0.35f); // Dim background for modal windows

    // Style adjustments
    style.WindowRounding    = 8.0f; // Softer rounded corners for windows
    style.FrameRounding     = 4.0f; // Rounded corners for frames
    style.ScrollbarRounding = 6.0f; // Rounded corners for scrollbars
    style.GrabRounding      = 4.0f; // Rounded corners for grab elements
    style.ChildRounding     = 4.0f; // Rounded corners for child windows

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f); // Centered window title
    style.WindowPadding    = ImVec2(10.0f, 10.0f); // Comfortable padding
    style.FramePadding     = ImVec2(6.0f, 4.0f);   // Frame padding
    style.ItemSpacing      = ImVec2(8.0f, 8.0f);   // Item spacing
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);   // Inner item spacing
    style.IndentSpacing    = 22.0f;                // Indentation spacing

    style.ScrollbarSize = 16.0f; // Scrollbar size
    style.GrabMinSize   = 10.0f; // Minimum grab size

    style.AntiAliasedLines = true; // Enable anti-aliased lines
    style.AntiAliasedFill  = true; // Enable anti-aliased fill
}

void apply_theme_FluentUITheme_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // General window settings
    style.WindowRounding    = 5.0f;
    style.FrameRounding     = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.PopupRounding     = 5.0f;

    // Setting the colors
    colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]             = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
    colors[ImGuiCol_Border]               = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // Accent colors changed to darker olive-green/grey shades
    colors[ImGuiCol_CheckMark] =
      ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Dark gray for check marks
    colors[ImGuiCol_SliderGrab] =
      ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Dark gray for sliders
    colors[ImGuiCol_SliderGrabActive] =
      ImVec4(0.50f, 0.50f, 0.50f, 1.00f); // Slightly lighter gray when active
    colors[ImGuiCol_Button] =
      ImVec4(0.25f, 0.25f, 0.25f, 1.00f); // Button background (dark gray)
    colors[ImGuiCol_ButtonHovered] =
      ImVec4(0.30f, 0.30f, 0.30f, 1.00f); // Button hover state
    colors[ImGuiCol_ButtonActive] =
      ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // Button active state
    colors[ImGuiCol_Header] =
      ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Dark gray for menu headers
    colors[ImGuiCol_HeaderHovered] =
      ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Slightly lighter on hover
    colors[ImGuiCol_HeaderActive] =
      ImVec4(0.50f, 0.50f, 0.50f, 1.00f); // Lighter gray when active
    colors[ImGuiCol_Separator] =
      ImVec4(0.30f, 0.30f, 0.30f, 1.00f); // Separators in dark gray
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_SeparatorActive]  = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ResizeGrip] =
      ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Resize grips in dark gray
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_Tab] =
      ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Tabs background
    colors[ImGuiCol_TabHovered] =
      ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Darker gray on hover
    colors[ImGuiCol_TabActive]          = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_DockingPreview] =
      ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Docking preview in gray
    colors[ImGuiCol_DockingEmptyBg] =
      ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Empty dock background
    // Additional styles
    style.FramePadding  = ImVec2(8.0f, 4.0f);
    style.ItemSpacing   = ImVec2(8.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 16.0f;
}

void apply_theme_FluentUILightTheme_colors() noexcept
{
    ImGuiStyle& style  = ImGui::GetStyle();
    ImVec4*     colors = style.Colors;

    // General window settings
    style.WindowRounding    = 5.0f;
    style.FrameRounding     = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.PopupRounding     = 5.0f;

    // Setting the colors (Light version)
    colors[ImGuiCol_Text]         = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] =
      ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Light background
    colors[ImGuiCol_ChildBg]      = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_PopupBg]      = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_Border]       = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] =
      ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Light frame background
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

    // Accent colors with a soft pastel gray-green
    colors[ImGuiCol_CheckMark] =
      ImVec4(0.55f, 0.65f, 0.55f, 1.00f); // Soft gray-green for check marks
    colors[ImGuiCol_SliderGrab]       = ImVec4(0.55f, 0.65f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.70f, 0.60f, 1.00f);
    colors[ImGuiCol_Button] =
      ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Light button background
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_Header]           = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_HeaderHovered]    = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_HeaderActive]     = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_Separator]        = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_SeparatorActive]  = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_ResizeGrip] =
      ImVec4(0.55f, 0.65f, 0.55f, 1.00f); // Accent color for resize grips
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.70f, 0.60f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.65f, 0.75f, 0.65f, 1.00f);
    colors[ImGuiCol_Tab] =
      ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Tabs background
    colors[ImGuiCol_TabHovered]         = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_DockingPreview] =
      ImVec4(0.55f, 0.65f, 0.55f, 1.00f); // Docking preview in gray-green
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

    // Additional styles
    style.FramePadding  = ImVec2(8.0f, 4.0f);
    style.ItemSpacing   = ImVec2(8.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 16.0f;
}

} // irt
