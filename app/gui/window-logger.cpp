// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "imgui.h"

namespace irt {

window_logger::window_logger() noexcept
  : entries{ window_logger::ring_buffer_length }
{}

auto window_logger::enqueue() noexcept -> window_logger::string_t&
{
    std::unique_lock lock(m_mutex);
    debug::ensure(entries.capacity() > 0);

    if (entries.full())
        entries.pop_head();

    entries.push_tail(small_string<string_length>(""));
    return entries.back();
}

void window_logger::show() noexcept
{
    if (!ImGui::Begin(window_logger::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (std::unique_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        if (ImGui::BeginPopup("Options")) {
            if (ImGui::Checkbox("Auto-scroll", &auto_scroll))
                if (auto_scroll)
                    scroll_to_bottom = true;
            ImGui::EndPopup();
        }

        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            entries.clear();

        ImGui::Separator();
        ImGui::BeginChild("scrolling",
                          ImVec2(0, 0),
                          false,
                          ImGuiWindowFlags_HorizontalScrollbar);

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

    ImGui::End();
}

} // irt
