// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>

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

static inline constexpr std::size_t max_history_size = 4096;

static auto display_text(
  ImFont&                                       font,
  const std::string_view                        level,
  const small_string<log_record::title_length>& title,
  const small_string<log_record::msg_length>&   description) noexcept -> void
{
    ImGui::PushFont(&font);
    ImGui::TextFormat("{}", level);
    ImGui::PopFont();

    if (not title.empty()) {
        ImGui::SameLine();
        ImGui::TextFormat(" {}\n", title.sv());
    }

    if (not description.empty()) {
        ImGui::PushFont(&font);
        ImGui::TextUnformatted("\ue016");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", description.c_str());
    }

    ImGui::Separator();
}

void window_logger::show() noexcept
{
    auto& app = container_of(this, &application::log_wnd);
    const auto level_min = app.config.vars.loglevel.load();

    app.jn.collect();

    if (clear_expected) {
        app.jn.reset_history();
        clear_expected = false;
    }

    if (!ImGui::Begin(window_logger::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginPopup("Options")) {
        if (ImGui::Checkbox("Auto-scroll", &auto_scroll))
            if (auto_scroll)
                scroll_to_bottom = true;

        auto sel = ordinal(level_min);

        for (sz i = 0, e = std::size(log_level_names); i != e; ++i) {
            const auto label    = small_string<32>(log_level_names[i]);
            const auto selected = sel == i;

            if (ImGui::MenuItem(label.c_str(), nullptr, selected))
                sel = i;
        }

        if (sel != ordinal(level_min))
            app.config.vars.loglevel = enum_cast<log_level>(sel);

        ImGui::EndPopup();
    }


    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        app.jn.reset_history();

    ImGui::Separator();
    ImGui::BeginChild(
      "scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    auto cursor = u64{ 0 };

    app.jn.read_log(version, cursor, [&](const auto span) {
        for (const auto& l : span) {
            const auto l_min     = ordinal(level_min);
            const auto l_current = ordinal(l.level);
            const auto str       = log_level_enhanced_names[l_current];

            if (l_min >= l_current)
                display_text(*app.icons, str, l.t, l.msg);
        }

        if (span.size() > max_history_size)
            clear_expected = true;
    });

    ImGui::PopStyleVar();

    if (scroll_to_bottom)
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;
    ImGui::EndChild();

    ImGui::End();
}

} // irt
