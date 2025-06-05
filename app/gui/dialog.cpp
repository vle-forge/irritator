// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "dialog.hpp"

#include <imgui.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

#if defined(__linux__)
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <mach-o/dyld.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <KnownFolders.h>
#include <shlobj.h>
#include <wchar.h>
#include <windows.h>
#endif

namespace irt {

#ifdef _WIN32
uint32_t fill_drives(file_dialog& fd) noexcept
{
    DWORD    mask = ::GetLogicalDrives();
    uint32_t ret  = { 0 };

    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1 << i)))
            continue;

        char rootName[4] = { static_cast<char>('A' + i), ':', '\\', '\0' };
        UINT type        = ::GetDriveTypeA(rootName);
        if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED)
            ret |= (1 << i);
    }

    return ret;
}
#else
uint32_t fill_drives(file_dialog& /*fd*/) noexcept { return 0; }
#endif

static bool have_good_file_name_starts(const std::filesystem::path& p,
                                       const char8_t** file_filters) noexcept
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

static bool have_good_extension(const std::filesystem::path& p,
                                const char8_t** extension_filters) noexcept
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

static void copy_files_and_directories(
  const std::filesystem::path&   current_path,
  vector<std::filesystem::path>& paths,
  const char8_t**                file_filters,
  const char8_t**                extension_filters) noexcept
{
    std::error_code err;
    for (std::filesystem::directory_iterator it(current_path, err), et;
         it != et;
         ++it) {

        if (it->is_directory(err)) {
            paths.emplace_back(*it);
            continue;
        }

        if (it->is_regular_file(err)) {
            if (have_good_extension(*it, extension_filters) &&
                have_good_file_name_starts(*it, file_filters)) {
                paths.emplace_back(*it);
                continue;
            }
        }
    }
}

static void sort(vector<std::filesystem::path>& paths) noexcept
{
    std::sort(
      std::begin(paths), std::end(paths), [](const auto& lhs, const auto& rhs) {
          std::error_code ec1, ec2;

          if (std::filesystem::is_directory(lhs, ec1)) {
              if (std::filesystem::is_directory(rhs, ec2))
                  return lhs.filename() < rhs.filename();

              return true;
          }

          if (std::filesystem::is_directory(rhs, ec2))
              return false;

          return lhs.filename() < rhs.filename();
      });
}

#ifdef _WIN32
void show_drives(const std::filesystem::path& current,
                 std::filesystem::path&       selected,
                 const uint32_t               drives,
                 bool*                        path_click,
                 std::filesystem::path*       next) noexcept
{
    char current_drive  = static_cast<char>(current.c_str()[0]);
    char drive_string[] = { current_drive, ':', '\0' };

    ImGui::PushItemWidth(4 * ImGui::GetFontSize());
    if (ImGui::BeginCombo("##select_win_drive", drive_string)) {
        for (int i = 0; i < 26; ++i) {
            if (!(drives & (1 << i)))
                continue;

            char drive_char          = static_cast<char>('A' + i);
            char selectable_string[] = { drive_char, ':', '\0' };
            bool is_selected         = current_drive == drive_char;
            if (ImGui::Selectable(selectable_string, is_selected) &&
                !is_selected) {
                char            new_current[] = { drive_char, ':', '\\', '\0' };
                std::error_code err;
                std::filesystem::current_path(new_current, err);
                if (!err) {
                    selected.clear();
                    *path_click = true;
                    *next       = std::filesystem::current_path(err);
                }
            }
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
}
#else
void show_drives(const std::filesystem::path& /*current*/,
                 std::filesystem::path& /*selected*/,
                 const uint32_t /*drives*/,
                 bool* /*path_click*/,
                 std::filesystem::path* /*next*/) noexcept
{}
#endif

static void show_path(const std::filesystem::path& current,
                      std::filesystem::path&       selected,
                      bool*                        path_click,
                      std::filesystem::path*       next) noexcept
{
    auto push_id          = 0;
    auto selected_path_it = current.end();

    for (auto it = current.begin(), et = current.end(); it != et; ++it) {
        if (it != current.begin())
            ImGui::SameLine();

        auto  u8name = it->u8string();
        auto* c_str  = reinterpret_cast<const char*>(u8name.c_str());

        ImGui::PushID(push_id++);
        if (ImGui::Button(c_str))
            selected_path_it = it;
        ImGui::PopID();
    }

    if (selected_path_it != current.end()) {
        next->clear();

#if _WIN32
        if (selected_path_it == current.begin())
            ++selected_path_it;
#endif

        for (auto jt = current.begin(); jt != selected_path_it; ++jt)
            *next /= jt->native();

        *next /= selected_path_it->native();

        selected.clear();
        *path_click = true;
    }
}

file_dialog::file_dialog() noexcept
  : buffer(512, reserve_tag{})
  , drives{ 0 }
  , state{ status::hide }
{
    std::error_code error;

    drives = fill_drives(*this);

    current = std::filesystem::current_path(error);
}

void file_dialog::clear() noexcept
{
    // paths.clear();
    // current.clear();

    // std::error_code error;
    // current = std::filesystem::current_path(error);

    next.clear();
    temp.clear();
    state = file_dialog::status::hide;

    buffer.clear();
    buffer.push_back(u8'\0');
}

bool file_dialog::show_load_file(const char*     title,
                                 const char8_t** filters) noexcept
{
    if (state == status::hide)
        state = status::show;

    next.clear();
    bool ret = false;

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(title)) {
        bool path_click = false;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        const auto child_size =
          region_height - 3.f * ImGui::GetFrameHeightWithSpacing();

        show_drives(current, selected, drives, &path_click, &next);

        if (!path_click)
            show_path(current, selected, &path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, child_size },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);

            if (ImGui::Selectable("..##select_file", (selected == ".."))) {
                if (next.empty()) {
                    next = current.parent_path();
                    selected.clear();
                    path_click = true;
                }
            }

            for (auto it = paths.cbegin(), et = paths.cend(); it != et; ++it) {
                temp.clear();
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec))
                    temp = u8"[Dir] ";

                const auto u8filename = it->filename().u8string();
                temp += u8filename;

                auto* u8c_str = temp.c_str();
                auto* c_str   = reinterpret_cast<const char*>(u8c_str);

                if (ImGui::Selectable(c_str, (it->filename() == selected))) {
                    selected = it->filename();

                    if (std::filesystem::is_directory(*it, ec)) {
                        if (next.empty()) {
                            selected.clear();
                            next = current;
                            next /= it->filename();
                            path_click = true;
                        }
                    }

                    if (std::filesystem::is_regular_file(*it, ec)) {
                        buffer.resize(u8filename.size());
                        std::copy_n(
                          u8filename.data(), u8filename.size(), buffer.begin());
                        buffer.push_back(u8'\0');
                    }
                    break;
                }
            }

            ImGui::EndChild();
        }

        if (path_click) {
            paths.clear();
            extension_filters = filters;
            file_filters      = nullptr;

            copy_files_and_directories(
              next, paths, file_filters, extension_filters);

            sort(paths);
            current = next;
        }

        auto  u8_str  = selected.u8string();
        auto* u8c_str = u8_str.c_str();
        auto* c_str   = reinterpret_cast<const char*>(u8c_str);
        ImGui::Text("File Name: %s", c_str);

        if (ImGui::Button("Ok", button_size)) {
            result = current;
            result /= selected;
            state = status::ok;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            result.clear();
            state = status::cancel;
        }

        if (state != status::show) {
            ret = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

bool file_dialog::show_save_file(const char*        title,
                                 std::u8string_view default_file_name,
                                 const char8_t**    filters) noexcept
{
    if (state == status::hide) {
        state = status::show;

        buffer.resize(std::size(default_file_name));
        std::copy_n(
          default_file_name.data(), default_file_name.size(), buffer.begin());
        buffer.push_back(u8'\0');
    }

    next.clear();
    bool res = false;

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(title)) {
        bool path_click = false;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        const auto child_size =
          region_height - 4.f * ImGui::GetFrameHeightWithSpacing();

        show_drives(current, selected, drives, &path_click, &next);

        if (!path_click)
            show_path(current, selected, &path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, child_size },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            if (ImGui::Selectable("..##select_file", (selected == ".."))) {
                if (next.empty()) {
                    next = current.parent_path();
                    selected.clear();
                    path_click = true;
                }
            }

            for (auto it = paths.cbegin(), et = paths.cend(); it != et; ++it) {
                temp.clear();
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec))
                    temp = u8"[Dir] ";

                const auto u8filename = it->filename().u8string();
                temp                  = u8filename;

                if (ImGui::Selectable((const char*)temp.c_str(),
                                      (it->filename() == selected))) {
                    selected = it->filename();

                    if (std::filesystem::is_directory(*it, ec)) {
                        if (next.empty()) {
                            selected.clear();
                            next = current;
                            next /= it->filename();
                            path_click = true;
                        }
                    }

                    if (std::filesystem::is_regular_file(*it, ec)) {
                        buffer.resize(u8filename.size());
                        std::copy_n(
                          u8filename.data(), u8filename.size(), buffer.begin());
                        buffer.push_back(u8'\0');
                    }

                    break;
                }
            }

            ImGui::EndChild();
        }

        if (path_click) {
            paths.clear();
            extension_filters = filters;
            file_filters      = nullptr;

            copy_files_and_directories(
              next, paths, file_filters, extension_filters);

            sort(paths);
            current = next;
        }

        auto* buffer_ptr    = reinterpret_cast<char*>(buffer.data());
        auto  buffer_size   = std::size(buffer);
        auto  current_str   = current.u8string();
        auto* u8current_ptr = current_str.c_str();
        auto* current_ptr   = reinterpret_cast<const char*>(u8current_ptr);
        ImGui::InputText("File Name", buffer_ptr, buffer_size);
        ImGui::Text("Directory name: %s", current_ptr);

        if (ImGui::Button("Ok", button_size)) {
            result = current;
            result /= buffer.data();
            state = status::ok;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            result.clear();
            state = status::cancel;
        }

        if (state != status::show) {
            res = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return res;
}

bool file_dialog::show_select_directory(const char* title) noexcept
{
    if (state == status::hide) {
        state = status::show;

        buffer.reserve(512);
        buffer.push_back(u8'\0');
    }

    next.clear();
    bool res = false;

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(title)) {
        bool path_click = false;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        const auto child_size =
          region_height - 4.f * ImGui::GetFrameHeightWithSpacing();

        show_drives(current, selected, drives, &path_click, &next);

        if (!path_click)
            show_path(current, selected, &path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, child_size },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            if (ImGui::Selectable("..##select_file", (selected == ".."))) {
                if (next.empty()) {
                    next = current.parent_path();
                    selected.clear();
                    path_click = true;
                }
            }

            for (auto it = paths.cbegin(), et = paths.cend(); it != et; ++it) {
                temp.clear();
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec)) {
                    temp = u8"[Dir] ";
                    temp += it->filename().u8string();

                    if (ImGui::Selectable((const char*)temp.c_str(),
                                          (it->filename() == selected))) {
                        selected = it->filename();

                        if (next.empty()) {
                            selected.clear();
                            next = current;
                            next /= it->filename();
                            path_click = true;
                        }
                        break;
                    }
                }
            }

            ImGui::EndChild();
        }

        if (path_click) {
            paths.clear();
            extension_filters = nullptr;
            file_filters      = nullptr;

            copy_files_and_directories(
              next, paths, file_filters, extension_filters);

            sort(paths);
            current = next;
        }

        auto  str   = current.u8string();
        auto* u8dir = str.c_str();
        auto* dir   = reinterpret_cast<const char*>(u8dir);
        ImGui::Text("Directory name: %s", dir);

        if (ImGui::Button("Ok", button_size)) {
            result = current;
            result /= buffer.data();
            state = status::ok;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            result.clear();
            state = status::cancel;
        }

        if (state != status::show) {
            res = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return res;
}

} // namespace irt
