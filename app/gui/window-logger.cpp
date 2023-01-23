// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

void window_logger::clear() noexcept { entries.clear(); }

auto window_logger::enqueue() noexcept -> window_logger::string_t&
{
    if (entries.full())
        entries.pop_front();

    entries.push_back("");
    return entries.back();
}

void window_logger::show() noexcept
{
    if (ImGui::BeginPopup("Options")) {
        if (ImGui::Checkbox("Auto-scroll", &auto_scroll))
            if (auto_scroll)
                scroll_to_bottom = true;
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool need_clear = ImGui::Button("Clear");

    ImGui::Separator();
    ImGui::BeginChild(
      "scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (need_clear)
        clear();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    auto it = entries.begin();
    auto et = entries.end();

    for (; it != et; ++it)
        ImGui::TextUnformatted(it->c_str());

    ImGui::PopStyleVar();

    if (scroll_to_bottom)
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;
    ImGui::EndChild();
}

void application::show_log_window() noexcept { log_window.show(); }

} // irt
