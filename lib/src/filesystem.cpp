// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__linux__)
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
#endif

#include <fmt/format.h>

#include <irritator/container.hpp>
#include <irritator/error.hpp>
#include <irritator/format.hpp>

#include <filesystem>
#include <string>
#include <vector>

#define irritator_to_string(s) to_string(s)
#define to_string(s) #s

namespace irt {

#if defined(__linux__) || defined(__APPLE__)
static expected<std::filesystem::path> get_local_home_directory() noexcept
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
    if (s || !result) {
        std::error_code ec;
        if (auto ret = std::filesystem::current_path(ec); !ec)
            if (auto exists = std::filesystem::exists(ret, ec); !ec && exists)
                return ret;

        return new_error(fs_errc::user_directory_access_fail, category::fs);
    } else {
        return std::filesystem::path{ std::string_view{ buf.data() } };
    }

    return new_error(fs_errc::user_directory_access_fail, category::fs);
}
#elif defined(_WIN32)
static expected<std::filesystem::path> get_local_home_directory() noexcept
{
    PWSTR path{ nullptr };

    if (SUCCEEDED(::SHGetKnownFolderPath(
                    FOLDERID_LocalAppData, 0, nullptr, &path) >= 0)) {
        std::filesystem::path ret;
        ret = path;
        ::CoTaskMemFree(path);
        return ret;
    } else {
        std::error_code ec;
        if (auto ret = std::filesystem::current_path(ec); !ec)
            if (auto exists = std::filesystem::exists(ret, ec); !ec && exists)
                return ret;

        return new_error(fs_errc::user_directory_access_fail, category::fs);
    }
}
#endif

expected<std::filesystem::path> get_home_directory() noexcept
{
    try {
        auto ret = get_local_home_directory();

        if (!ret)
            return ret.error();

#if defined(_WIN32)
        *ret /= "irritator-" irritator_to_string(
          VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#else
        *ret /= ".irritator-" irritator_to_string(
          VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#endif
        std::error_code ec;
        if (std::filesystem::is_directory(*ret, ec))
            return ret;

        if (std::filesystem::create_directories(*ret, ec))
            return ret;
    } catch (...) {
    }

    return new_error(fs_errc::user_directory_access_fail, category::fs);
}

#if defined(__linux__)
expected<std::filesystem::path> get_executable_directory() noexcept
{
    std::vector<char> buf(PATH_MAX, '\0');
    const auto        ssize = readlink("/proc/self/exe", buf.data(), PATH_MAX);

    if (ssize <= 0)
        return new_error(fs_errc::executable_access_fail, category::fs);

    const auto size = static_cast<size_t>(ssize);

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(__APPLE__)
expected<std::filesystem::path> get_executable_directory() noexcept
{
    std::vector<char> buf(MAXPATHLEN, '\0');
    uint32_t          size{ 0 };

    if (_NSGetExecutablePath(buf.data(), &size))
        return new_error(fs_errc::executable_access_fail, category::fs);

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(_WIN32)
expected<std::filesystem::path> get_executable_directory() noexcept
{
    std::wstring filepath;
    DWORD        len   = MAX_PATH;
    DWORD        error = ERROR_SUCCESS;

    filepath.resize(len, '\0');

    for (int i = 1; i < 16; ++i) {
        auto size = ::GetModuleFileNameW(nullptr, filepath.data(), len);
        if (size == 0) {
            error = ::GetLastError();
            len *= 2;
            filepath.resize(len);
        } else {
            filepath.resize(static_cast<std::size_t>(size));
            return std::filesystem::path{ filepath };
        }
    }

    return new_error(fs_errc::executable_access_fail, category::fs);
}
#endif

#if defined(__linux__) || defined(__APPLE__)
expected<std::filesystem::path> get_system_component_dir() noexcept
{
    auto exe = get_executable_directory();
    if (!exe)
        return exe.error();

    auto install_path = exe.value().parent_path();
    install_path /= "share";
    install_path /= "irritator-" irritator_to_string(
      VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
    install_path /= "components";

    std::error_code ec;
    if (std::filesystem::exists(install_path, ec))
        return install_path;

    return new_error(fs_errc::executable_access_fail, category::fs);
}
#elif defined(_WIN32)
expected<std::filesystem::path> get_system_component_dir() noexcept
{
    auto exe = get_executable_directory();
    if (!exe)
        return exe.error();

    auto bin_path     = exe.value().parent_path();
    auto install_path = bin_path.parent_path();
    install_path /= "share";
    install_path /= "irritator-" irritator_to_string(
      VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
    install_path /= "components";

    std::error_code ec;
    if (auto exists = std::filesystem::exists(install_path, ec); !ec && exists)
        return install_path;

    return new_error(fs_errc::executable_access_fail, category::fs);
}
#endif

#if defined(IRT_DATAROOTDIR)
expected<std::filesystem::path> get_system_prefix_component_dir() noexcept
{
    std::filesystem::path path(IRT_DATAROOTDIR);

    path /= "irritator-" irritator_to_string(
      VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
    path /= "components";

    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        return path;

    return new_error(fs_errc::executable_access_fail, category::fs);
}
#else
expected<std::filesystem::path> get_system_prefix_component_dir() noexcept
{
    return std::filesystem::path();
}
#endif

#if defined(__linux__) || defined(__APPLE__)
expected<std::filesystem::path> get_default_user_component_dir() noexcept
{
    auto home_path = get_home_directory();

    if (!home_path)
        return home_path.error();

    auto compo_path = home_path.value();
    compo_path /= "components";

    std::error_code ec;
    if (std::filesystem::exists(compo_path, ec))
        return compo_path;

    if (std::filesystem::create_directories(compo_path, ec))
        return compo_path;

    return new_error(fs_errc::user_component_directory_access_fail,
                     category::fs);
}
#elif defined(_WIN32)
expected<std::filesystem::path> get_default_user_component_dir() noexcept
{
    auto home_path = get_home_directory();
    if (!home_path)
        return home_path.error();

    auto compo_path = home_path.value();
    compo_path /= "components";

    std::error_code ec;
    if (std::filesystem::exists(compo_path, ec))
        return compo_path;

    if (std::filesystem::create_directories(compo_path, ec))
        return compo_path;

    return new_error(fs_errc::user_component_directory_access_fail,
                     category::fs);
}
#endif

static expected<std::filesystem::path> get_home_filename(
  const char* filename) noexcept
{
    try {
        auto ret = get_home_directory();
        if (!ret)
            return ret.error();

        *ret /= filename;

        return ret;
    } catch (...) {
    }

    return new_error(fs_errc::user_directory_access_fail, category::fs);
}

expected<std::filesystem::path> get_settings_filename() noexcept
{
    return get_home_filename("settings.ini");
}

class config_home_manager
{
private:
    bool m_log = false;

    template<typename S, typename... Args>
    constexpr void log(int indent, const S& s, Args&&... args) noexcept
    {
        if (m_log) {
            fmt::print(stderr, "{:{}}", "", indent);
            fmt::vprint(stderr, s, fmt::make_format_args(args...));
        }
    }

public:
    explicit config_home_manager(bool use_log) noexcept
      : m_log{ use_log }
    {
#if defined(VERSION_TWEAK) and (0 - VERSION_TWEAK - 1) != 1
        log(0,
            "irritator-{}.{}.{}-{}\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_PATCH,
            VERSION_TWEAK);

#else
        log(0,
            "irritator-{}.{}.{}\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_PATCH);
#endif
    }

    expected<std::filesystem::path> operator()(
      std::string_view dir_name,
      std::string_view subdir_name,
      std::string_view file_name) noexcept
    {
        debug::ensure(not dir_name.empty());
        debug::ensure(not subdir_name.empty());
        debug::ensure(not file_name.empty());

        auto path = std::filesystem::path(dir_name);
        log(0, "- check directory: {}\n", path.string());

        if (not is_directory_and_usable(path)) {
            log(1, "Is not a directory or bad permissions\n");
            return error_code(fs_errc::user_directory_access_fail,
                              category::fs);
        }

        path /= subdir_name;
        log(1, "- {}\n", path.string());
        if (not is_directory_and_usable(path)) {
            log(2, "Directory not exists and not usable try to fix\n");
            if (not create_dir(path)) {
                log(3, "Fail to create directory or change permissions\n");
                return error_code(fs_errc::user_directory_access_fail,
                                  category::fs);
            }
        }

        path /= file_name;
        log(1, "- {}\n", path.string());
        if (not is_file_and_usable(path)) {
            log(2, "Fail to read or create the file. Abort.\n");
            return error_code(fs_errc::user_directory_access_fail,
                              category::fs);
        }

        log(1, "- irritator config file configured:\n", path.string());
        return path;
    }

private:
    bool try_change_file_permission(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_read |
                                       std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add,
                                     ec);

        return not ec.value();
    }

    bool try_change_directory_permission(
      const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::add,
                                     ec);

        return ec.value();
    }

    bool is_directory_and_usable(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        auto status = std::filesystem::status(path, ec);
        if (ec)
            return false;

        if (status.type() != std::filesystem::file_type::directory)
            return false;

        auto perms = status.permissions();
        if (std::filesystem::perms::none !=
            (perms & std::filesystem::perms::owner_all))
            return true;

        return try_change_directory_permission(path);
    }

    bool create_dir(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;
        if (not std::filesystem::create_directory(path, ec))
            return false;

        auto perms = std::filesystem::status(path, ec).permissions();
        if (ec)
            return false;

        return std::filesystem::perms::none !=
               (perms & std::filesystem::perms::owner_all);
    }

    bool is_file_and_usable(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        auto status = std::filesystem::status(path, ec);
        if (ec)
            return false;

        if (status.type() == std::filesystem::file_type::not_found) {
            std::ofstream ofs(path.string());
            return ofs.is_open();
        }

        if (status.type() != std::filesystem::file_type::regular)
            return false;

        auto perms = status.permissions();
        if (std::filesystem::perms::none !=
            (perms & (std::filesystem::perms::owner_read |
                      std::filesystem::perms::owner_write)))
            return true;

        return try_change_file_permission(path);
    }
};

std::string get_config_home(bool log) noexcept
{
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    config_home_manager m(log);
    small_string<64>    home_dir;

    format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);

    if (auto* xdg = getenv("XDG_CONFIG_HOME"); xdg)
        if (auto ret = m(xdg, home_dir.sv(), "config.ini"); ret.has_value())
            return ret.value().string();

    if (auto* home = getenv("HOME"); home) {
        auto p = std::filesystem::path(home);
        p /= ".config";

        format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);
        if (auto ret = m(p.c_str(), home_dir.sv(), "config.ini");
            ret.has_value())
            return ret.value().string();

        format(home_dir, ".irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);

        if (auto ret = m(home, home_dir.sv(), "config.ini"); ret.has_value())
            return ret.value().string();
    }

    std::error_code ec;
    if (auto path = std::filesystem::current_path(ec); ec)
        if (auto ret = m(path.string(), home_dir.sv(), "config.ini");
            ret.has_value())
            return ret.value().string();

    if (auto ret = m(".", home_dir.sv(), "config.ini"); ret.has_value())
        return ret.value().string();

    return "config.ini";
#elif defined(_WIN32)
    if (auto ret = get_home_directory(); ret) {
        auto path(std::move(*ret));
        path /= "config.ini";
        return path.string();
    }

    return "config.ini";
#endif
}

std::filesystem::path get_imgui_filename() noexcept
{
    if (auto path_opt = get_home_filename("imgui.ini"); path_opt.has_value())
        return *path_opt;

    return std::filesystem::current_path();
}

}
