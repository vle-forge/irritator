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

enum class notification_state { fadein, wait, fadeout, expired };

static inline const ImVec4 notification_text_color[] = {
    { 0.46f, 0.59f, 0.78f, 1.f }, { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.78f, 0.59f, 1.f }, { 0.78f, 0.49f, 0.46f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f }, { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f }, { 0.46f, 0.59f, 0.78f, 1.f },
};

static inline const ImVec4 notification_color[] = {
    { 0.16f, 0.29f, 0.48f, 1.f }, { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.48f, 0.29f, 1.f }, { 0.48f, 0.29f, 0.16f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f }, { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f }, { 0.16f, 0.29f, 0.48f, 1.f },
};

static inline const char* notification_prefix[] = { "emergency error ",
                                                    "alert error ",
                                                    "critical error ",
                                                    "error ",
                                                    "warnings ",
                                                    "",
                                                    "",
                                                    "debug " };

static u64 get_elapsed_time(const notification& n) noexcept
{
    return get_tick_count_in_milliseconds() - n.creation_time;
}

static notification_state get_state(const notification& n) noexcept
{
    const auto elapsed = get_elapsed_time(n);

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

static float get_fade_percent(const notification& n) noexcept
{
    const auto state   = get_state(n);
    const auto elapsed = get_elapsed_time(n);

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

notification::notification() noexcept
  : creation_time(get_tick_count_in_milliseconds())
  , level(log_level::info)
{}

notification::notification(log_level level_) noexcept
  : creation_time(get_tick_count_in_milliseconds())
  , level(level_)
{}

notification_manager::notification_manager() noexcept
  : m_enabled_ids(notification_number)
{
    m_data.reserve(notification_number);
}

notification& notification_manager::alloc() noexcept
{
    std::scoped_lock lock{ m_mutex };

    if (m_enabled_ids.full()) {
        const auto front = m_enabled_ids.front();
        m_data.free(front);
        m_enabled_ids.dequeue();
    }

    return m_data.alloc();
}

notification& notification_manager::alloc(log_level level) noexcept
{
    auto& n = alloc();
    n.level = level;

    return n;
}

void notification_manager::enable(const notification& n) noexcept
{
    std::scoped_lock lock{ m_mutex };

    if (m_enabled_ids.full()) {
        const auto front = m_enabled_ids.front();
        m_data.free(front);
        m_enabled_ids.dequeue();
    }

    const auto id = m_data.get_id(n);
    m_enabled_ids.enqueue(id);
}

void notification_manager::show() noexcept
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);

    const auto vp_size = ImGui::GetMainViewport()->Size;
    auto       height  = 0.f;
    auto       i       = 0;

    if (std::unique_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        for (auto it = m_enabled_ids.head(); it != m_enabled_ids.end(); ++it) {
            auto* notif = m_data.try_to_get(*it);
            if (!notif) {
                *it = undefined<notification_id>();
                continue;
            }

            if (get_state(*notif) == notification_state::expired) {
                auto& app = container_of(this, &application::notifications);
                auto& msg = app.log_wnd.enqueue();

                if (notif->message.empty())
                    msg.assign(notif->title.sv());
                else
                    format(
                      msg, "{}: {}", notif->title.sv(), notif->message.sv());

                m_data.free(*it);
                *it = undefined<notification_id>();
                continue;
            }

            if (notif->only_log)
                continue;

            const auto opacity = get_fade_percent(*notif);
            auto text_color    = notification_text_color[ordinal(notif->level)];
            text_color.w       = opacity;

            ImGui::SetNextWindowBgAlpha(opacity);
            ImGui::SetNextWindowPos(
              ImVec2(vp_size.x - notification_x_padding,
                     vp_size.y - notification_y_padding - height),
              ImGuiCond_Always,
              ImVec2(1.0f, 1.0f));

            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  notification_color[ordinal(notif->level)]);
            small_string<16> name;
            format(name, "##{}toast", i);
            ImGui::Begin(name.c_str(), nullptr, notification_flags);
            ImGui::PopStyleColor(1);

            ImGui::PushTextWrapPos(vp_size.x / 3.f);
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::TextUnformatted(notification_prefix[ordinal(notif->level)]);
            ImGui::SameLine();
            ImGui::TextUnformatted(notif->title.c_str());
            ImGui::PopStyleColor();

            if (!notif->message.empty()) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
                ImGui::Separator();
                ImGui::TextUnformatted(notif->message.c_str());
            }

            ImGui::PopTextWrapPos();
            height += ImGui::GetWindowHeight() + notification_y_message_padding;

            ImGui::End();

            ++i;
        }

        while (!m_enabled_ids.empty() &&
               m_enabled_ids.front() == undefined<notification_id>())
            m_enabled_ids.dequeue();

        ImGui::PopStyleVar();
    }
}

} // namespace irt
