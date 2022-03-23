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

enum class notification_state
{
    fadein,
    wait,
    fadeout,
    expired
};

static inline const ImVec4 notification_text_color[] = {
    { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.78f, 0.59f, 1.f },
    { 0.78f, 0.49f, 0.46f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f }
};

static inline const ImVec4 notification_color[] = {
    { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.48f, 0.29f, 1.f },
    { 0.48f, 0.29f, 0.16f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f }
};

static inline const char* notification_prefix[] = { "",
                                                    "Success ",
                                                    "Warning ",
                                                    "Error ",
                                                    "Information " };

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

    irt_unreachable();
}

notification::notification(notification_type type_) noexcept
  : type(type_)
  , creation_time(get_tick_count_in_milliseconds())
{}

notification_manager::notification_manager() noexcept
  : r_buffer(buffer.data(), static_cast<i32>(buffer.size()))
{
    data.init(notification_number);
}

notification& notification_manager::alloc(notification_type type) noexcept
{
    for (;;) {
        {
            std::lock_guard<std::mutex> lock{ mutex };

            if (data.can_alloc())
                return data.alloc(type);
        }

        std::this_thread::yield();
    }
}

void notification_manager::enable(const notification& n) noexcept
{
    for (;;) {
        {
            std::lock_guard<std::mutex> lock{ mutex };

            if (!r_buffer.full()) {
                const auto id = data.get_id(n);
                r_buffer.enqueue(id);
                return;
            }
        }

        std::this_thread::yield();
    }
}

void notification_manager::show() noexcept
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);

    const auto vp_size = ImGui::GetMainViewport()->Size;
    auto       height  = 0.f;

    std::lock_guard<std::mutex> lock{ mutex };

    int i = 0;
    for (auto it = r_buffer.begin(), et = r_buffer.end(); it != et; ++it, ++i) {
        auto* notif = data.try_to_get(*it);
        if (!notif) {
            *it = undefined<notification_id>();
            continue;
        }

        if (get_state(*notif) == notification_state::expired) {
            data.free(*it);
            *it = undefined<notification_id>();
            continue;
        }

        const auto opacity    = get_fade_percent(*notif);
        auto       text_color = notification_text_color[ordinal(notif->type)];
        text_color.w          = opacity;

        ImGui::SetNextWindowBgAlpha(opacity);
        ImGui::SetNextWindowPos(
          ImVec2(vp_size.x - notification_x_padding,
                 vp_size.y - notification_y_padding - height),
          ImGuiCond_Always,
          ImVec2(1.0f, 1.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              notification_color[ordinal(notif->type)]);
        small_string<16> name;
        format(name, "##{}toast", i);
        ImGui::Begin(name.c_str(), nullptr, notification_flags);
        ImGui::PopStyleColor(1);

        ImGui::PushTextWrapPos(vp_size.x / 3.f);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::TextUnformatted(notification_prefix[ordinal(notif->type)]);
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
    }

    while (!r_buffer.empty() &&
           r_buffer.front() == undefined<notification_id>())
        r_buffer.dequeue();

    ImGui::PopStyleVar();
}

} // namespace irt
