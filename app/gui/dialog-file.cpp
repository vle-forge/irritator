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

#if defined(__linux__) || defined(__APPLE__)
std::optional<std::filesystem::path>
get_home_directory()
{
    if (auto* home = std::getenv("HOME"); home)
        return std::filesystem::path{ home };

    auto size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (size == -1)
        size = 16384u;

    std::vector<char> buf(size, '\0');
    struct passwd pwd;
    struct passwd* result = nullptr;

    const auto s = getpwuid_r(getpid(), &pwd, buf.data(), size, &result);
    if (s || !result)
        return std::nullopt;

    return std::filesystem::path{ std::string_view{ buf.data() } };
}
#elif defined(_WIN32)
std::optional<std::filesystem::path>
get_home_directory()
{
    PWSTR path{ nullptr };
    const auto hr =
      ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);

    std::filesystem::path ret;

    if (SUCCEEDED(hr))
        ret = path;

    ::CoTaskMemFree(path);

    return ret;
}
#endif

#if defined(__linux__)
std::optional<std::filesystem::path>
get_executable_directory()
{
    std::vector<char> buf(PATH_MAX, '\0');
    const auto size = readlink("/proc/self/exe", buf.data(), PATH_MAX);

    if (size <= 0)
        return std::nullopt;

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(__APPLE__)
std::optional<std::filesystem::path>
get_executable_directory()
{
    std::vector<char> buf(MAXPATHLEN, '\0');
    uint32_t size{ 0 };

    if (_NSGetExecutablePath(buf.data(), &size))
        return std::nullopt;

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(_WIN32)
std::optional<std::filesystem::path>
get_executable_directory()
{
    std::wstring filepath;
    auto size = ::GetModuleFileNameW(NULL, &filepath[0], 0u);

    while (static_cast<size_t>(size) > filepath.size()) {
        filepath.resize(size, '\0');
        size = ::GetModuleFileNameW(
          NULL, &filepath[0], static_cast<DWORD>(filepath.size()));
    }

    if (!size)
        return std::nullopt;

    return std::filesystem::path{ filepath };
}
#endif

application::settings_manager::settings_manager() noexcept
{
    try {
        if (auto home = get_home_directory(); home) {
            home_dir = home.value();
            home_dir /= "irritator";
        } else {
            log_w.log(
              3,
              "Fail to retrieve home directory. Use current directory instead");
            home_dir = std::filesystem::current_path();
        }

        if (auto install = get_executable_directory(); install) {
            executable_dir = install.value();
        } else {
            log_w.log(
              3,
              "Fail to retrieve executable directory. Use current directory "
              "instead");
            executable_dir = std::filesystem::current_path();
        }

        log_w.log(5,
                  "home: %s\ninstall: %s\n",
                  home_dir.u8string().c_str(),
                  executable_dir.u8string().c_str());

        // TODO Fill the libraries vectors with users and systems directory.

    } catch (const std::exception& /*e*/) {
        log_w.log(2, "Fail to initialize application");
    }
}

struct file_dialog
{
    std::vector<std::filesystem::path> paths;
    std::filesystem::path current;
    std::filesystem::path selected;
#if defined(__APPLE__)
    // @TODO Remove this part when XCode allows u8string concatenation.
    std::string temp;
    char buffer[512];
#else
    std::u8string temp;
    char8_t buffer[512];
#endif

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

#if defined(__APPLE__)
        // @TODO Remove this part when XCode allows u8string find/compare.
        while (*filters) {
            if (p.u8string().find(reinterpret_cast<const char*>(*filters)) == 0)
                return true;

            ++filters;
        }
#else
        while (*filters) {
            if (p.u8string().find(*filters) == 0)
                return true;

            ++filters;
        }
#endif

        return false;
    }

    bool have_good_extension(const std::filesystem::path& p)
    {
        if (extension_filters == nullptr)
            return true;

        const char8_t** filters = extension_filters;

#if defined(__APPLE__)
        // @TODO Remove this part when XCode allows u8string find/compare.
        while (*filters) {
            if (p.extension().u8string() ==
                reinterpret_cast<const char*>(*filters))
                return true;

            ++filters;
        }
#else
        while (*filters) {
            if (p.extension().u8string() == *filters)
                return true;

            ++filters;
        }
#endif

        return false;
    }

    void copy_files_and_directories(const std::filesystem::path& current_path)
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
#if _WIN32
                if (it == current.begin())
                    ++it;
#endif
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

// static const char8_t* filters[] = { u8".irt", nullptr };
bool
load_file_dialog(std::filesystem::path& out,
                 const char* title,
                 const char8_t** filters)
{
    if (fd.current.empty()) {
        fd.fill_drives();
        fd.selected.clear();
        std::error_code error;
        fd.current = std::filesystem::current_path(error);
    }

    std::filesystem::path next;
    bool res = false;

    if (ImGui::BeginPopupModal(title)) {
        bool path_click = false;

        fd.show_drives(&path_click, &next);

        if (!path_click)
            fd.show_path(&path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, 350.f },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);

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
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec)) {
#if defined(__APPLE__)
                    // @TODO Remove this part when XCode allows u8string
                    // concatenation.
                    fd.temp = "[Dir] ";
                    fd.temp += reinterpret_cast<const char*>(
                      it->filename().u8string().c_str());
#else
                    fd.temp = u8"[Dir] ";
                    fd.temp += it->filename().u8string();
#endif
                } else
                    fd.temp = it->filename().u8string();

                if (ImGui::Selectable((const char*)fd.temp.c_str(),
                                      (it->filename() == fd.selected))) {
                    fd.selected = it->filename();

                    if (std::filesystem::is_directory(*it, ec)) {
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
            fd.extension_filters = filters;
            fd.file_filters = nullptr;

            fd.copy_files_and_directories(next);
            fd.sort();
            fd.current = next;
        }

        ImGui::Text("File Name: %s",
                    (const char*)fd.selected.u8string().c_str());

        const auto item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width = ImGui::GetContentRegionAvail().x;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        if (ImGui::Button("Ok", button_size)) {
            auto sel = fd.current;
            sel /= fd.selected;
            out = sel;
            res = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            res = true;
        }

        if (res) {
            ImGui::CloseCurrentPopup();
            fd.clear();
        }

        ImGui::EndPopup();
    }

    return res;
}

// static const char8_t* filters[] = { u8".irt", nullptr };
bool
save_file_dialog(std::filesystem::path& out,
                 const char* title,
                 const char8_t** filters)
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

    if (ImGui::BeginPopupModal(title)) {
        bool path_click = false;

        fd.show_drives(&path_click, &next);

        if (!path_click)
            fd.show_path(&path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, 350.f },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);
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
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec))
#if defined(__APPLE__)
                    // @TODO Remove this part when XCode allows u8string
                    // concatenation.
                    fd.temp = "[Dir] ";
#else
                    fd.temp = u8"[Dir] ";
#endif

                fd.temp = it->filename().u8string();

                if (ImGui::Selectable((const char*)fd.temp.c_str(),
                                      (it->filename() == fd.selected))) {
                    fd.selected = it->filename();

                    if (std::filesystem::is_directory(*it, ec)) {
                        if (next.empty()) {
                            fd.selected.clear();
                            next = fd.current;
                            next /= it->filename();
                            path_click = true;
                        }
                    }

                    if (std::filesystem::is_regular_file(*it, ec)) {
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

        const auto item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width = ImGui::GetContentRegionAvail().x;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        if (ImGui::Button("Ok", button_size)) {
            auto sel = fd.current;
            sel /= fd.buffer;
            out = sel;
            res = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            res = true;
        }

        if (res) {
            ImGui::CloseCurrentPopup();
            fd.clear();
        }

        ImGui::EndPopup();
    }

    return res;
}

bool
select_directory_dialog(std::filesystem::path& out)
{
    if (fd.current.empty()) {
        fd.fill_drives();
        fd.selected.clear();
        std::error_code error;
        fd.current = std::filesystem::current_path(error);
    }

    std::filesystem::path next;
    bool res = false;

    if (ImGui::BeginPopupModal("Select directory")) {
        bool path_click = false;

        fd.show_drives(&path_click, &next);

        if (!path_click)
            fd.show_path(&path_click, &next);

        if (!path_click) {
            ImGui::BeginChild("##select_files",
                              ImVec2{ 0.f, 350.f },
                              true,
                              ImGuiWindowFlags_HorizontalScrollbar);
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
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec)) {
#if defined(__APPLE__)
                    // @TODO Remove this part when XCode allows u8string
                    // concatenation.
                    fd.temp = "[Dir] ";
                    fd.temp += reinterpret_cast<const char*>(
                      it->filename().u8string().c_str());
#else
                    fd.temp = u8"[Dir] ";
                    fd.temp += it->filename().u8string();
#endif

                    if (ImGui::Selectable((const char*)fd.temp.c_str(),
                                          (it->filename() == fd.selected))) {
                        fd.selected = it->filename();

                        if (next.empty()) {
                            fd.selected.clear();
                            next = fd.current;
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
            fd.paths.clear();
            fd.extension_filters = nullptr;
            fd.file_filters = nullptr;

            fd.copy_files_and_directories(next);
            fd.sort();
            fd.current = next;
        }

        ImGui::Text("Directory name: %s",
                    (const char*)fd.current.u8string().c_str());

        const auto item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width = ImGui::GetContentRegionAvail().x;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };

        if (ImGui::Button("Ok", button_size)) {
            auto sel = fd.current;
            sel /= fd.buffer;
            out = sel;
            res = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            res = true;
        }

        if (res) {
            ImGui::CloseCurrentPopup();
            fd.clear();
        }

        ImGui::EndPopup();
    }

    return res;
}

} // namespace irt
