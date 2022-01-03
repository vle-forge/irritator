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

#define irritator_to_string(s) to_string(s)
#define to_string(s) #s

namespace irt {

#if defined(__linux__) || defined(__APPLE__)
static std::optional<std::filesystem::path> get_local_home_directory() noexcept
{
    if (auto* home = std::getenv("HOME"); home)
        return std::filesystem::path{ home };

    auto size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (size == -1)
        size = 16384u;

    std::vector<char> buf(size, '\0');
    struct passwd     pwd;
    struct passwd*    result = nullptr;

    const auto s = getpwuid_r(getpid(), &pwd, buf.data(), size, &result);
    if (s || !result)
        return std::nullopt;

    return std::filesystem::path{ std::string_view{ buf.data() } };
}
#elif defined(_WIN32)
static std::optional<std::filesystem::path> get_local_home_directory() noexcept
{
    PWSTR      path{ nullptr };
    const auto hr =
      ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);

    std::filesystem::path ret;

    if (SUCCEEDED(hr))
        ret = path;

    ::CoTaskMemFree(path);

    return ret;
}
#endif

std::optional<std::filesystem::path> get_home_directory()
{
    try {
        std::error_code ec;

        auto ret = get_local_home_directory();
        if (!ret) {
            ret = std::filesystem::current_path(ec);
        }

#if defined(_WIN32)
        *ret /= "irritator-" irritator_to_string(
          VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#else
        *ret /= ".irritator-" irritator_to_string(
          VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#endif
        if (std::filesystem::is_directory(*ret, ec))
            return ret;

        if (std::filesystem::create_directories(*ret, ec))
            return ret;
    } catch (...) {
    }

    return std::nullopt;
}

#if defined(__linux__)
std::optional<std::filesystem::path> get_executable_directory()
{
    std::vector<char> buf(PATH_MAX, '\0');
    const auto        ssize = readlink("/proc/self/exe", buf.data(), PATH_MAX);

    if (ssize <= 0)
        return std::nullopt;

    const auto size = static_cast<size_t>(ssize);

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(__APPLE__)
std::optional<std::filesystem::path> get_executable_directory()
{
    std::vector<char> buf(MAXPATHLEN, '\0');
    uint32_t          size{ 0 };

    if (_NSGetExecutablePath(buf.data(), &size))
        return std::nullopt;

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(_WIN32)
std::optional<std::filesystem::path> get_executable_directory()
{
    std::wstring filepath;
    auto         size = ::GetModuleFileNameW(NULL, &filepath[0], 0u);

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

#if defined(__linux__) || defined(__APPLE__)
std::optional<std::filesystem::path> get_system_component_dir()
{
    if (auto executable_path = get_executable_directory(); executable_path) {
        auto install_path = executable_path.value().parent_path();
        install_path /= "share";
        install_path /= "irritator-" irritator_to_string(
          VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
        install_path /= "components";

        std::error_code ec;
        if (std::filesystem::exists(install_path, ec))
            return install_path;

        if (std::filesystem::create_directories(install_path, ec))
            return install_path;
    }

    return std::nullopt;
}
#elif defined(_WIN32)
std::optional<std::filesystem::path> get_system_component_dir()
{
    if (auto executable_path = get_executable_directory(); executable_path) {
        auto install_path = executable_path.value().parent_path();
        install_path /= "components";

        std::error_code ec;
        if (std::filesystem::exists(install_path, ec))
            return install_path;

        if (std::filesystem::create_directories(install_path, ec))
            return install_path;
    }

    return std::nullopt;
}
#endif

#if defined(__linux__) || defined(__APPLE__)
std::optional<std::filesystem::path> get_default_user_component_dir()
{
    if (auto home_path = get_home_directory(); home_path) {
        auto compo_path = home_path.value();
        compo_path /= "components";

        std::error_code ec;
        if (std::filesystem::exists(compo_path, ec))
            return compo_path;

        if (std::filesystem::create_directories(compo_path, ec))
            return compo_path;
    }

    return std::nullopt;
}
#elif defined(_WIN32)
std::optional<std::filesystem::path> get_default_user_component_dir()
{
    if (auto home_path = get_home_directory(); home_path) {
        auto compo_path = home_path.value();
        compo_path /= "components";

        std::error_code ec;
        if (std::filesystem::exists(compo_path, ec))
            return compo_path;

        if (std::filesystem::create_directories(compo_path, ec))
            return compo_path;
    }

    return std::nullopt;
}
#endif

static std::optional<std::filesystem::path> get_home_filename(
  const char* filename) noexcept
{
    try {
        auto ret = get_home_directory();
        if (!ret) {
            std::error_code ec;
            ret = std::filesystem::current_path(ec);
        }

        *ret /= filename;

        return ret;
    } catch (...) {
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> get_settings_filename() noexcept
{
    return get_home_filename("settings.ini");
}

char* get_imgui_filename() noexcept
{
    char* ret = nullptr;

    if (auto path = get_home_filename("imgui.ini"); path) {
        auto* str = reinterpret_cast<const char*>(path->c_str());
#if defined(_WIN32)
        ret = _strdup(str);
#else
        ret = strdup(str);
#endif
    }

    return ret;
}

#ifdef _WIN32
uint32_t fill_drives(file_dialog& fd)
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
  const std::filesystem::path&        current_path,
  std::vector<std::filesystem::path>& paths,
  const char8_t**                     file_filters,
  const char8_t**                     extension_filters) noexcept
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

static void sort(std::vector<std::filesystem::path>& paths) noexcept
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
                 std::filesystem::path*       next)
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
                 std::filesystem::path&       /*selected*/,
                 const uint32_t               /*drives*/,
                 bool*                        /*path_click*/,
                 std::filesystem::path*       /*next*/)
{}
#endif

static void show_path(const std::filesystem::path& current,
                      std::filesystem::path&       selected,
                      bool*                        path_click,
                      std::filesystem::path*       next) noexcept
{
    for (auto it = current.begin(), et = current.end(); it != et; ++it) {
        if (it != current.begin())
            ImGui::SameLine();

        auto  name  = it->u8string();
        auto* c_str = reinterpret_cast<const char*>(name.c_str());
        if (ImGui::Button(c_str)) {
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

file_dialog::file_dialog() noexcept
  : drives{ 0 }
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

    buffer[0] = u8'\0';
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

            for (auto it = paths.begin(), et = paths.end(); it != et; ++it) {
                temp.clear();
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec)) {
                    temp = u8"[Dir] ";
                    temp += it->filename().u8string();
                } else
                    temp = it->filename().u8string();

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
        state                   = status::show;
        const auto default_size = std::size(default_file_name);
        const auto buffer_size  = std::size(buffer);
        const auto max_size     = std::min(default_size, buffer_size - 1);

        std::copy_n(std::begin(default_file_name), max_size, buffer);
        buffer[max_size] = u8'\0';
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

            for (auto it = paths.begin(), et = paths.end(); it != et; ++it) {
                temp.clear();
                std::error_code ec;
                if (std::filesystem::is_directory(*it, ec))
                    temp = u8"[Dir] ";

                temp = it->filename().u8string();

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
                        const size_t max_size =
                          std::min(std::size(it->filename().u8string()),
                                   std::size(buffer) - 1);

                        std::copy_n(std::begin(it->filename().u8string()),
                                    max_size,
                                    buffer);
                        buffer[max_size] = u8'\0';
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

        auto* buffer_ptr    = reinterpret_cast<char*>(buffer);
        auto  buffer_size   = std::size(buffer);
        auto  current_str   = current.u8string();
        auto* u8current_ptr = current_str.c_str();
        auto* current_ptr   = reinterpret_cast<const char*>(u8current_ptr);
        ImGui::InputText("File Name", buffer_ptr, buffer_size);
        ImGui::Text("Directory name: %s", current_ptr);

        if (ImGui::Button("Ok", button_size)) {
            result = current;
            result /= buffer;
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
    if (state == status::hide)
        state = status::show;

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

            for (auto it = paths.begin(), et = paths.end(); it != et; ++it) {
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
            result /= buffer;
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
