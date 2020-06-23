// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "gui.hpp"
#include "node-editor.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace irt {

struct file_dialog
{
    std::vector<std::filesystem::path> paths;
    std::filesystem::path current;
    std::filesystem::path selected;
    std::u8string temp;
    char8_t buffer[512];

    bool is_open = true;

#ifdef _WIN32
    uint32_t drives{ 0 };

    void fill_drives()
    {
        DWORD mask = ::GetLogicalDrives();
        uint32_t ret = { 0 };

        for (int i = 0; i < 26; ++i) {
            if (!(mask & (1 << i)))
                continue;

            char rootName[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
            UINT type = ::GetDriveTypeA(rootName);
            if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED)
                ret |= (1 << i);
        }

        drives = ret;
    }
#else
    void fill_drives()
    {}
#endif

    const char8_t** file_filters;
    const char8_t** extension_filters;

    bool have_good_file_name_starts(const std::filesystem::path& p)
    {
        if (file_filters == nullptr)
            return true;

        const char8_t** filters = file_filters;
        while (*filters) {
            if (p.u8string().find(*filters) == 0)
                return true;

            ++filters;
        }

        return false;
    }

    bool have_good_extension(const std::filesystem::path& p)
    {
        if (extension_filters == nullptr)
            return true;

        const char8_t** filters = extension_filters;
        while (*filters) {
            if (p.extension().u8string() == *filters)
                return true;

            ++filters;
        }

        return false;
    }

    void copy_files_and_directories(const std::filesystem::path& current_path)
    {
        std::error_code err;
        for (std::filesystem::directory_iterator it(current_path, err), et;
             !err && it != et;
             ++it) {

            if (it->is_directory(err) && !err) {
                paths.emplace_back(*it);
                continue;
            }

            if (it->is_regular_file(err) && !err) {
                if (have_good_extension(*it) &&
                    have_good_file_name_starts(*it)) {
                    paths.emplace_back(*it);
                    continue;
                }
            }
        }
    }

    void sort()
    {
        std::sort(std::begin(paths),
                  std::end(paths),
                  [](const auto& lhs, const auto& rhs) {
                      if (std::filesystem::is_directory(lhs)) {
                          if (std::filesystem::is_directory(rhs))
                              return lhs.filename() < rhs.filename();

                          return true;
                      }

                      if (std::filesystem::is_directory(rhs))
                          return false;

                      return lhs.filename() < rhs.filename();
                  });
    }

    void clear()
    {
        paths.clear();
        selected.clear();
        current.clear();
    }

    void show_drives([[maybe_unused]] bool* path_click,
                     [[maybe_unused]] std::filesystem::path* next)
    {
#ifdef _WIN32
        char current_drive = static_cast<char>(current.c_str()[0]);
        char drive_string[] = { current_drive, ':', '\0' };

        ImGui::PushItemWidth(4 * ImGui::GetFontSize());
        if (ImGui::BeginCombo("##select_win_drive", drive_string)) {
            for (int i = 0; i < 26; ++i) {
                if (!(drives & (1 << i)))
                    continue;

                char drive_char = static_cast<char>('A' + i);
                char selectable_string[] = { drive_char, ':', '\0' };
                bool is_selected = current_drive == drive_char;
                if (ImGui::Selectable(selectable_string, is_selected) &&
                    !is_selected) {
                    char new_current[] = { drive_char, ':', '\\', '\0' };
                    std::error_code err;
                    std::filesystem::current_path(new_current, err);
                    if (!err) {
                        selected.clear();
                        *path_click = true;
                        *next = std::filesystem::current_path(err);
                    }
                }
            }

            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
#endif
    }

    void show_path(bool* path_click, std::filesystem::path* next)
    {
        for (auto it = current.begin(), et = current.end(); it != et; ++it) {
            if (it != current.begin())
                ImGui::SameLine();

            if (ImGui::Button((const char*)it->u8string().c_str())) {
                next->clear();
                for (auto jt = current.begin(); jt != it; ++jt)
                    *next /= jt->native();
                *next /= it->native();

                selected.clear();
                *path_click = true;
                break;
            }
        }
    }
};

file_dialog fd;

bool
load_file_dialog(std::filesystem::path& out)
{
    if (fd.current.empty()) {
        fd.fill_drives();
        fd.selected.clear();
        std::error_code error;
        fd.current = std::filesystem::current_path(error);
    }

    std::filesystem::path next;
    bool res = false;

    if (ImGui::BeginPopupModal("Select file path to load")) {
        bool path_click = false;

        fd.show_drives(&path_click, &next);

        if (!path_click)
            fd.show_path(&path_click, &next);

        if (!path_click) {
            ImVec2 size = ImGui::GetContentRegionMax();
            size.y *= 0.8f;

            ImGui::BeginChild("##select_files", size);

            if (ImGui::Selectable("..##select_file", (fd.selected == ".."))) {
                if (next.empty()) {
                    next = fd.current.parent_path();
                    fd.selected.clear();
                    path_click = true;
                }
            }

            for (auto it = fd.paths.begin(), et = fd.paths.end(); it != et;
                 ++it) {
                fd.temp.clear();
                if (std::filesystem::is_directory(*it)) {
                    fd.temp = u8"[Dir] ";
                    fd.temp += it->filename().u8string();
                } else
                    fd.temp = it->filename().u8string();

                if (ImGui::Selectable((const char*)fd.temp.c_str(),
                                      (it->filename() == fd.selected))) {
                    fd.selected = it->filename();

                    if (std::filesystem::is_directory(*it)) {
                        if (next.empty()) {
                            fd.selected.clear();
                            next = fd.current;
                            next /= it->filename();
                            path_click = true;
                        }
                    }
                    break;
                }
            }

            ImGui::EndChild();
        }

        if (path_click) {
            fd.paths.clear();
            static const char8_t* filters[] = { u8".irt", nullptr };
            fd.extension_filters = filters;
            fd.file_filters = nullptr;

            fd.copy_files_and_directories(next);
            fd.sort();
            fd.current = next;
        }

        ImGui::Text("File Name: %s",
                    (const char*)fd.selected.u8string().c_str());

        float width = ImGui::GetContentRegionAvailWidth();
        ImGui::PushItemWidth(width);

        if (ImGui::Button("Ok", ImVec2(width / 2, 0))) {
            auto sel = fd.current;
            sel /= fd.selected;
            out = sel;
            res = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(width / 2, 0))) {
            res = true;
        }

        ImGui::PopItemWidth();

        if (res) {
            ImGui::CloseCurrentPopup();
            fd.clear();
        }

        ImGui::EndPopup();
    }

    return res;
}

bool
save_file_dialog(std::filesystem::path& out)
{
    if (fd.current.empty()) {
        fd.fill_drives();
        fd.selected.clear();
        std::error_code error;
        fd.current = std::filesystem::current_path(error);

        static const std::u8string default_file_name = u8"file-name.irt";
        const size_t max_size =
          std::min(std::size(default_file_name), std::size(fd.buffer) - 1);

        std::copy_n(std::begin(default_file_name), max_size, fd.buffer);
        fd.buffer[max_size] = u8'\0';
    }

    std::filesystem::path next;
    bool res = false;

    if (ImGui::BeginPopupModal("Select file path to save")) {
        bool path_click = false;

        fd.show_drives(&path_click, &next);

        if (!path_click)
            fd.show_path(&path_click, &next);

        if (!path_click) {
            ImVec2 size = ImGui::GetContentRegionMax();
            size.y *= 0.8f;

            ImGui::BeginChild("##select_files", size);
            if (ImGui::Selectable("..##select_file", (fd.selected == ".."))) {
                if (next.empty()) {
                    next = fd.current.parent_path();
                    fd.selected.clear();
                    path_click = true;
                }
            }

            for (auto it = fd.paths.begin(), et = fd.paths.end(); it != et;
                 ++it) {
                fd.temp.clear();
                if (std::filesystem::is_directory(*it))
                    fd.temp = u8"[Dir] ";

                fd.temp = it->filename().u8string();

                if (ImGui::Selectable((const char*)fd.temp.c_str(),
                                      (it->filename() == fd.selected))) {
                    fd.selected = it->filename();

                    if (std::filesystem::is_directory(*it)) {
                        if (next.empty()) {
                            fd.selected.clear();
                            next = fd.current;
                            next /= it->filename();
                            path_click = true;
                        }
                    }

                    if (std::filesystem::is_regular_file(*it)) {
                        const size_t max_size =
                          std::min(std::size(it->filename().u8string()),
                                   std::size(fd.buffer) - 1);

                        std::copy_n(std::begin(it->filename().u8string()),
                                    max_size,
                                    fd.buffer);
                        fd.buffer[max_size] = u8'\0';
                    }

                    break;
                }
            }

            ImGui::EndChild();
        }

        if (path_click) {
            fd.paths.clear();
            static const char8_t* filters[] = { u8".irt", nullptr };
            fd.extension_filters = filters;
            fd.file_filters = nullptr;

            fd.copy_files_and_directories(next);
            fd.sort();
            fd.current = next;
        }

        ImGui::InputText(
          "File Name", (char*)fd.buffer, IM_ARRAYSIZE(fd.buffer));
        ImGui::Text("Directory name: %s",
                    (const char*)fd.current.u8string().c_str());

        float width = ImGui::GetContentRegionAvailWidth();
        ImGui::PushItemWidth(width);

        if (ImGui::Button("Ok", ImVec2(width / 2, 0))) {
            auto sel = fd.current;
            sel /= fd.buffer;
            out = sel;
            res = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(width / 2, 0))) {
            res = true;
        }

        ImGui::PopItemWidth();

        if (res) {
            ImGui::CloseCurrentPopup();
            fd.clear();
        }

        ImGui::EndPopup();
    }

    return res;
}

} // namespace irt
