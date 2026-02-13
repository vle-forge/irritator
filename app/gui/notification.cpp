// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static u64 get_tick_count_in_milliseconds() noexcept
{
    namespace sc = std::chrono;

    return duration_cast<sc::milliseconds>(
             sc::steady_clock::now().time_since_epoch())
      .count();
}

constexpr float notification_x_padding         = 20.f;
constexpr float notification_y_padding         = 20.f;
constexpr float notification_y_message_padding = 20.f;
constexpr u64   notification_fade_duration     = 150;

constexpr ImGuiWindowFlags notification_flags =
  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
  ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
  ImGuiWindowFlags_NoFocusOnAppearing;

enum class notification_state : u8 { fadein, wait, fadeout, expired };

static inline const char* notification_prefix[] = { "emergency error ",
                                                    "alert error ",
                                                    "critical error ",
                                                    "error ",
                                                    "warnings ",
                                                    "",
                                                    "",
                                                    "debug " };

static u64 get_elapsed_time(const u64 creation_time) noexcept
{
    return get_tick_count_in_milliseconds() - creation_time;
}

static notification_state get_state(const u64 creation_time) noexcept
{
    const auto elapsed = get_elapsed_time(creation_time);

    if (elapsed > notification_fade_duration +
                    notification_manager::notification_duration +
                    notification_fade_duration)
        return notification_state::expired;

    if (elapsed > notification_fade_duration +
                    notification_manager::notification_duration)
        return notification_state::fadeout;

    if (elapsed > notification_fade_duration)
        return notification_state::wait;

    return notification_state::fadein;
}

static float get_fade_percent(const u64 creation_time) noexcept
{
    const auto state   = get_state(creation_time);
    const auto elapsed = get_elapsed_time(creation_time);

    switch (state) {
    case notification_state::fadein:
        return static_cast<float>(elapsed) / notification_fade_duration;

    case notification_state::wait:
        return 1.f;

    case notification_state::fadeout:
        return 1.f -
               ((static_cast<float>(elapsed) - notification_fade_duration -
                 static_cast<float>(
                   notification_manager::notification_duration)) /
                notification_fade_duration);

    case notification_state::expired:
        return 1.f;
    }

    unreachable();
}

static ImU32 get_background_color(const theme_colors& t,
                                  const log_level     l) noexcept
{
    switch (l) {
    case log_level::emergency:
        return to_ImU32(t[style_color::background_error_notification]);
    case log_level::alert:
        return to_ImU32(t[style_color::background_error_notification]);
    case log_level::critical:
        return to_ImU32(t[style_color::background_error_notification]);
    case log_level::error:
        return to_ImU32(t[style_color::background_error_notification]);
    case log_level::warning:
        return to_ImU32(t[style_color::background_warning_notification]);
    case log_level::notice:
        return to_ImU32(t[style_color::background_info_notification]);
    case log_level::info:
        return to_ImU32(t[style_color::background_info_notification]);
    case log_level::fatal:
        return to_ImU32(t[style_color::background_error_notification]);
    }

    irt::unreachable();
}

void notification_manager::show() noexcept
{
    auto& app = container_of(this, &application::notifications);

    const auto need_cleanup = app.jn.read([&](const auto& ring,
                                              const auto& ids,
                                              const auto& titles,
                                              const auto& descriptions) {
        const auto vp_pos       = ImGui::GetMainViewport()->Pos;
        const auto vp_size      = ImGui::GetMainViewport()->Size;
        auto       height       = 0.f;
        auto       i            = 0;
        auto       need_cleanup = false;

        for (const auto id : ring) {
            const auto  date    = ids[id].first;
            const auto  level   = ids[id].second;
            const auto& title   = titles[id];
            const auto& desc    = descriptions[id];
            const auto  opacity = get_fade_percent(date);

            if (get_state(date) == notification_state::expired) {
                need_cleanup = true;
                continue;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);
            ImGui::SetNextWindowBgAlpha(opacity);
            ImGui::SetNextWindowPos(
              ImVec2(vp_size.x - notification_x_padding,
                     vp_size.y - notification_y_padding - height),
              ImGuiCond_Always,
              ImVec2(1.0f, 1.0f));

            ImGui::PushStyleColor(
              ImGuiCol_WindowBg,
              get_background_color(app.config.colors, level));

            small_string<16> name;
            format(name, "##{}toast", i);
            ImGui::Begin(name.c_str(), nullptr, notification_flags);
            ImGui::PopStyleColor(1);

            ImGui::PushTextWrapPos(vp_size.x / 3.f);
            ImGui::TextUnformatted(notification_prefix[ordinal(level)]);
            ImGui::SameLine();
            ImGui::TextUnformatted(title.c_str());

            if (not desc.empty()) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
                ImGui::Separator();
                ImGui::TextUnformatted(desc.c_str());
            }

            ImGui::PopTextWrapPos();
            height += ImGui::GetWindowHeight() + notification_y_message_padding;

            ImGui::End();

            ++i;

            ImGui::PopStyleVar();
        }

        return need_cleanup;
    });

    if (need_cleanup) {

        // We need copy the expired tasks to the @c window-logger and the we
        // clear all items from the journal handler.

        app.add_gui_task([&app] {
            app.jn.read([&](const auto& ring,
                            const auto& ids,
                            const auto& titles,
                            const auto& descriptions) {
                for (const auto id : ring) {
                    const auto  date    = ids[id].first;
                    const auto  level   = ids[id].second;
                    const auto& title   = titles[id];
                    const auto& desc    = descriptions[id];

                    if (get_state(date) == notification_state::expired) {
                        auto& str = app.log_wnd.enqueue();
                        format(str,
                               "{} {}: {}",
                               log_level_names[ordinal(level)],
                               titles[id].c_str(),
                               descriptions[id].c_str());
                    }
                }
            });

            app.jn.cleanup_expired(3000u);
        });
    }
}

} // namespace irt
