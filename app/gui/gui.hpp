// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_GUI_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_GUI_2020

#include <imgui.h>

#include <filesystem>
#include <string>

namespace irt {

void
node_editor_initialize();

void
node_editor_show();

void
node_editor_shutdown();

struct window_logger
{
    ImGuiTextBuffer buffer;
    ImGuiTextFilter filter;
    ImVector<int> line_offsets;

    bool auto_scroll = true;
    bool scroll_to_bottom = false;
    window_logger() = default;
    void clear() noexcept;

    void log(const int level, const char* fmt, ...) IM_FMTARGS(3);
    void log(const int level, const char* fmt, va_list args) IM_FMTLIST(3);
    void show(bool* is_show);
};

/* Filesytem dialog box */

bool
load_file_dialog(std::filesystem::path& out);

bool
save_file_dialog(std::filesystem::path& out);

} // namespace irt

#endif
