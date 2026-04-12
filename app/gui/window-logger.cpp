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

static auto get_level(const log_level level, const log_level config) noexcept
  -> std::string_view

{
    constexpr auto empty_string = std::string_view{};

    return std::string_view(ordinal(level) > ordinal(config)
                              ? empty_string
                              : log_level_enhanced_names[ordinal(level)]);
}

static auto display_text(ImFont&                       font,
                         std::string_view              level,
                         const journal_handler::title& title,
                         const journal_handler::descr& description) noexcept
  -> void
{
    debug::ensure(not level.empty());

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
    if (!ImGui::Begin(window_logger::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::log_wnd);
    if (ImGui::BeginPopup("Options")) {
        if (ImGui::Checkbox("Auto-scroll", &auto_scroll))
            if (auto_scroll)
                scroll_to_bottom = true;
        ImGui::EndPopup();
    }

    const auto level_min = app.config.vars.loglevel.load();

    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        app.jn.clear();

    ImGui::Separator();
    ImGui::BeginChild(
      "scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    app.jn.read([&](const auto& ring,
                    const auto& ids,
                    const auto& titles,
                    const auto& descriptions) noexcept {
        for (const auto id : ring) {
            if (not ids.exists(id))
                continue;

            const auto  level       = get_level(ids[id].second, level_min);
            const auto& title       = titles[id];
            const auto& description = descriptions[id];

            display_text(*app.icons, level, title, description);
        }
    });

    ImGui::PopStyleVar();

    if (scroll_to_bottom)
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;
    ImGui::EndChild();

    ImGui::End();
}

} // irt
