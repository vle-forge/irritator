// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

namespace irt {

void
window_logger::clear() noexcept
{
    buffer.clear();
    line_offsets.clear();
    line_offsets.push_back(0);
}

const char*
log_string(const log_status s) noexcept
{
    static const char* str[] = { "[emergency]", "[alert]",   "[critical]",
                                 "[error]",     "[warning]", "[notice]",
                                 "[info]",      "[debug]" };

    return str[static_cast<int>(s)];
}

void
window_logger::log(const int level, const char* fmt, ...)
{
    auto l = std::clamp(level, 0, 7);
    auto old_size = buffer.size();
    va_list args;
    va_start(args, fmt);
    buffer.append(log_string(enum_cast<log_status>(l)));
    buffer.appendfv(fmt, args);
    va_end(args);

    for (auto new_size = buffer.size(); old_size < new_size; ++old_size)
        if (buffer[old_size] == '\n')
            line_offsets.push_back(old_size + 1);

    if (auto_scroll)
        scroll_to_bottom = true;
}

void
window_logger::log(const int level, const char* fmt, va_list args)
{
    auto old_size = buffer.size();
    auto l = std::clamp(level, 0, 7);
    buffer.append(log_string(enum_cast<log_status>(l)));
    buffer.appendfv(fmt, args);

    for (auto new_size = buffer.size(); old_size < new_size; ++old_size)
        if (buffer[old_size] == '\n')
            line_offsets.push_back(old_size + 1);

    if (auto_scroll)
        scroll_to_bottom = true;
}

void
window_logger::show(bool* is_show)
{
    ImGui::SetNextWindowPos(ImVec2(70, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Log", is_show)) {
        ImGui::End();
        return;
    }

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
    ImGui::SameLine();
    bool need_copy = ImGui::Button("Copy");
    ImGui::SameLine();
    filter.Draw("Filter", -100.0f);

    ImGui::Separator();
    ImGui::BeginChild(
      "scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (need_clear)
        clear();

    if (need_copy)
        ImGui::LogToClipboard();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* buf = buffer.begin();
    const char* buf_end = buffer.end();

    if (filter.IsActive()) {
        for (auto line_no = 0; line_no < line_offsets.Size; line_no++) {
            const char* line_start = buf + line_offsets[line_no];
            const char* line_end = (line_no + 1 < line_offsets.Size)
                                     ? (buf + line_offsets[line_no + 1] - 1)
                                     : buf_end;
            if (filter.PassFilter(line_start, line_end))
                ImGui::TextUnformatted(line_start, line_end);
        }
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(line_offsets.Size);
        while (clipper.Step()) {
            for (int line_no = clipper.DisplayStart;
                 line_no < clipper.DisplayEnd;
                 line_no++) {
                const char* line_start = buf + line_offsets[line_no];
                const char* line_end = (line_no + 1 < line_offsets.Size)
                                         ? (buf + line_offsets[line_no + 1] - 1)
                                         : buf_end;
                ImGui::TextUnformatted(line_start, line_end);
            }
        }
        clipper.End();
    }

    ImGui::PopStyleVar();

    if (scroll_to_bottom)
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;
    ImGui::EndChild();
    ImGui::End();
}

} // irt
