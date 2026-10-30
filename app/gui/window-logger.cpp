// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>

#include <ctime>

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static inline constexpr std::string_view log_level_enhanced_names[] = {
    "\ue003", // emergency
    "\ue003", // alert
    "\ue002", // critical
    "\ue002", // error
    "\ue004", // warning
    "\ue077", // notice
    "\ue08a", // info
    "\ue08a", // debug
};

enum class log_category : u8 { error, warning, info, debug };

static constexpr std::string_view log_category_names[] = {
    "Errors",
    "Warnings",
    "Infos",
    "Debugs",
};

static constexpr auto get_log_category(const log_level l) noexcept
{
    switch (l) {
    case log_level::emergency:
    case log_level::alert:
    case log_level::critical:
    case log_level::error:
        return log_category::error;
    case log_level::warning:
        return log_category::warning;
    case log_level::notice:
    case log_level::info:
        return log_category::info;
    case log_level::debug:
        return log_category::debug;
    }

    unreachable();
}

// Hard-coded mid-tone colors to stay readable with both light and dark themes.
static auto get_log_category_color(const log_category c) noexcept -> ImVec4
{
    switch (c) {
    case log_category::error:
        return ImVec4(0.86f, 0.24f, 0.24f, 1.f);
    case log_category::warning:
        return ImVec4(0.85f, 0.55f, 0.05f, 1.f);
    case log_category::info:
        return ImGui::GetStyle().Colors[ImGuiCol_Text];
    case log_category::debug:
        return ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
    }

    unreachable();
}

static auto format_time(const u64 ms_since_epoch) noexcept -> small_string<15>
{
    const auto t  = static_cast<std::time_t>(ms_since_epoch / 1000u);
    auto       tm = std::tm{};

#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    small_string<15> ret;
    format(ret, "{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return ret;
}

static void show_log_record(ImFont&           icons,
                            const log_record& l,
                            const int         row) noexcept
{
    const auto color = get_log_category_color(get_log_category(l.level));
    const auto time  = format_time(l.ts);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", time.c_str());

    ImGui::TableNextColumn();
    ImGui::PushFont(&icons);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(log_level_enhanced_names[ordinal(l.level)].data());
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", l.msg.c_str());
    ImGui::PopStyleColor();

    ImGui::PushID(row);
    if (ImGui::BeginPopupContextItem("copy")) {
        if (ImGui::MenuItem("Copy"))
            ImGui::SetClipboardText(l.msg.c_str());
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

static void append_log_record(ImGuiTextBuffer&  buffer,
                              const log_record& l) noexcept
{
    const auto time  = format_time(l.ts);
    const auto level = log_level_names[ordinal(l.level)];

    buffer.appendf("%s %.*s %s\n", time.c_str(), static_cast<int>(level.size()),
                   level.data(), l.msg.c_str());
}

void window_logger::show() noexcept
{
    auto&      app       = container_of(this, &application::log_wnd);
    const auto level_min = app.config.vars.loglevel.load();

    // Limits the refresh rate of the logs to one every 60 frames (one per
    // second by default).

    if (loop_number > max_loop_number) {
        app.jn.collect();
        loop_number = 0u;
    } else {
        ++loop_number;
    }

    if (clear_expected) {
        app.jn.reset_history();
        clear_expected = false;
    }

    if (!ImGui::Begin(window_logger::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginPopup("Options")) {
        if (ImGui::BeginMenu("Log level")) {
            auto sel = static_cast<std::size_t>(ordinal(level_min));

            for (std::size_t i = 0, e = std::size(log_level_names); i != e;
                 ++i) {
                const auto label    = small_string<32>(log_level_names[i]);
                const auto selected = sel == i;

                if (ImGui::MenuItem(label.c_str(), nullptr, selected))
                    sel = i;
            }

            if (sel != static_cast<std::size_t>(ordinal(level_min)))
                app.config.vars.loglevel = enum_cast<log_level>(sel);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("History Length")) {
            auto use_64 = max_history_size == 64;
            if (ImGui::Checkbox("64", &use_64))
                max_history_size = 64u;

            auto use_256 = max_history_size == 256;
            if (ImGui::Checkbox("256", &use_256))
                max_history_size = 256u;

            auto use_4096 = max_history_size == 4096;
            if (ImGui::Checkbox("4096", &use_4096))
                max_history_size = 4096u;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("History Refresh")) {
            auto use_1 = max_loop_number == 1;
            if (ImGui::Checkbox("1", &use_1))
                max_loop_number = 1u;

            auto use_60 = max_loop_number == 60;
            if (ImGui::Checkbox("60", &use_60))
                max_loop_number = 60u;

            auto use_120 = max_loop_number == 120;
            if (ImGui::Checkbox("120", &use_120))
                max_loop_number = 120u;

            ImGui::EndMenu();
        }

#if defined(IRRITATOR_ENABLE_DEBUG)
        if (ImGui::MenuItem("Ping")) {
            using namespace std::string_view_literals;
            log(log_level::info, "Ping from menu"sv);
        }
#endif

        ImGui::EndPopup();
    }

    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        app.jn.reset_history();
    ImGui::SameLine();
    if (ImGui::Button("Copy all"))
        copy_expected = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-scroll", &auto_scroll) and auto_scroll)
        scroll_to_bottom = true;

    for (auto i = 0; i < 4; ++i) {
        small_string<31> label;
        format(label, "{} ({})###category-{}", log_category_names[i],
               categories_count[i], i);

        ImGui::PushStyleColor(
          ImGuiCol_Text, get_log_category_color(enum_cast<log_category>(i)));
        ImGui::Checkbox(label.c_str(), &visible_categories[i]);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##filter", "Filter (incl, -excl)",
                                 filter.InputBuf,
                                 IM_ARRAYSIZE(filter.InputBuf)))
        filter.Build();

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false);

    ImGuiTextBuffer clipboard;
    categories_count.fill(0u);

    if (ImGui::BeginTable(
          "logs", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("level", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("message", ImGuiTableColumnFlags_WidthStretch);

        auto version = u64{ 0 };
        auto cursor  = u64{ 0 };
        auto row     = 0;

        app.jn.read_log(version, cursor, [&](const auto span) {
            for (const auto& l : span) {
                if (ordinal(l.level) > ordinal(level_min))
                    continue;

                const auto category = ordinal(get_log_category(l.level));
                ++categories_count[category];

                if (not visible_categories[category] or
                    not filter.PassFilter(l.msg.c_str()))
                    continue;

                show_log_record(*app.icons, l, row++);

                if (copy_expected)
                    append_log_record(clipboard, l);
            }

            if (span.size() > max_history_size)
                clear_expected = true;
        });

        ImGui::EndTable();
    }

    if (copy_expected) {
        ImGui::SetClipboardText(clipboard.c_str());
        copy_expected = false;
    }

    // Follows the new messages only if the user has not scrolled up to read.
    if (scroll_to_bottom or
        (auto_scroll and ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;
    ImGui::EndChild();

    ImGui::End();
}

} // irt
