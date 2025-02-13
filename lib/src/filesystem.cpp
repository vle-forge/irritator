// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#endif

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
#endif

#include <fmt/format.h>
#include <irritator/error.hpp>

#include <filesystem>
#include <string>
#include <vector>

#define irritator_to_string(s) to_string(s)
#define to_string(s) #s

namespace irt {

#if defined(__linux__) || defined(__APPLE__)
static result<std::filesystem::path> get_local_home_directory() noexcept
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

        return boost::leaf::new_error(fs_error::user_directory_access_fail);
    } else {
        return std::filesystem::path{ std::string_view{ buf.data() } };
    }

    return boost::leaf::new_error(fs_error::user_directory_access_fail);
}
#elif defined(_WIN32)
static result<std::filesystem::path> get_local_home_directory() noexcept
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

        return boost::leaf::new_error(fs_error::user_directory_access_fail);
    }
}
#endif

result<std::filesystem::path> get_home_directory() noexcept
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

    return boost::leaf::new_error(fs_error::user_directory_access_fail);
}

#if defined(__linux__)
result<std::filesystem::path> get_executable_directory() noexcept
{
    std::vector<char> buf(PATH_MAX, '\0');
    const auto        ssize = readlink("/proc/self/exe", buf.data(), PATH_MAX);

    if (ssize <= 0)
        return boost::leaf::new_error(fs_error::executable_access_fail);

    const auto size = static_cast<size_t>(ssize);

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(__APPLE__)
result<std::filesystem::path> get_executable_directory() noexcept
{
    std::vector<char> buf(MAXPATHLEN, '\0');
    uint32_t          size{ 0 };

    if (_NSGetExecutablePath(buf.data(), &size))
        return boost::leaf::new_error(fs_error::executable_access_fail);

    return std::filesystem::path{ std::string_view{ buf.data(), size } };
}
#elif defined(_WIN32)
result<std::filesystem::path> get_executable_directory() noexcept
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

    return boost::leaf::new_error(fs_error::executable_access_fail, error);
}
#endif

#if defined(__linux__) || defined(__APPLE__)
result<std::filesystem::path> get_system_component_dir() noexcept
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

    return boost::leaf::new_error(fs_error::executable_access_fail);
}
#elif defined(_WIN32)
result<std::filesystem::path> get_system_component_dir() noexcept
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
    if (auto exists = std::filesystem::exists(install_path, ec); !ec && exists)
        return install_path;

    return boost::leaf::new_error(fs_error::executable_access_fail);
}
#endif

#if defined(IRT_DATAROOTDIR)
result<std::filesystem::path> get_system_prefix_component_dir() noexcept
{
    std::filesystem::path path(IRT_DATAROOTDIR);

    path /= "irritator-" irritator_to_string(
      VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
    path /= "components";

    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        return path;

    return boost::leaf::new_error(fs_error::executable_access_fail);
}
#else
result<std::filesystem::path> get_system_prefix_component_dir() noexcept
{
    return std::filesystem::path();
}
#endif

#if defined(__linux__) || defined(__APPLE__)
result<std::filesystem::path> get_default_user_component_dir() noexcept
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

    return boost::leaf::new_error(
      fs_error::user_component_directory_access_fail);
}
#elif defined(_WIN32)
result<std::filesystem::path> get_default_user_component_dir() noexcept
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

    return boost::leaf::new_error(
      fs_error::user_component_directory_access_fail);
}
#endif

static result<std::filesystem::path> get_home_filename(
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

    return boost::leaf::new_error(fs_error::user_directory_file_access_fail);
}

result<std::filesystem::path> get_settings_filename() noexcept
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
    config_home_manager(bool use_log) noexcept
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

    std::pair<bool, std::string> operator()(const char* dir_name,
                                            const char* subdir_name,
                                            const char* file_name) noexcept
    {
        debug::ensure(dir_name);
        debug::ensure(subdir_name);
        debug::ensure(file_name);

        auto path = std::filesystem::path(dir_name);
        log(0, "- check directory: {}\n", path.string());

        if (not is_directory_and_usable(path)) {
            log(1, "Is not a directory or bad permissions\n");
            return { false, std::string{} };
        }

        path /= subdir_name;
        log(1, "- {}\n", path.string());
        if (not is_directory_and_usable(path)) {
            log(2, "Directory not exists and not usable try to fix\n");
            if (not create_dir(path)) {
                log(3, "Fail to create directory or change permissions\n");
                return { false, std::string{} };
            }
        }

        path /= file_name;
        log(1, "- {}\n", path.string());
        if (not is_file_and_usable(path)) {
            log(2, "Fail to read or create the file. Abort.\n");
            return { false, std::string{} };
        }

        log(1, "- irritator config file configured:\n", path.string());
        return { true, path.string() };
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
    char                home_dir[64];

    fmt::format_to_n(home_dir,
                     std::size(home_dir),
                     "irritator-{}.{}",
                     VERSION_MAJOR,
                     VERSION_MINOR);
    if (auto* xdg = getenv("XDG_CONFIG_HOME"); xdg)
        if (auto ret = m(xdg, home_dir, "config.ini"); ret.first)
            return ret.second;

    if (auto* home = getenv("HOME"); home) {
        auto p = std::filesystem::path(home);
        p /= ".config";

        fmt::format_to_n(home_dir,
                         std::size(home_dir),
                         "irritator-{}.{}",
                         VERSION_MAJOR,
                         VERSION_MINOR);
        if (auto ret = m(p.c_str(), home_dir, "config.ini"); ret.first)
            return ret.second;

        fmt::format_to_n(home_dir,
                         std::size(home_dir),
                         ".irritator-{}.{}",
                         VERSION_MAJOR,
                         VERSION_MINOR);

        if (auto ret = m(home, home_dir, "config.ini"); ret.first)
            return ret.second;
    }

    std::error_code ec;
    if (auto path = std::filesystem::current_path(ec); ec)
        if (auto ret = m(path.c_str(), home_dir, "config.ini"); ret.first)
            return ret.second;

    if (auto ret = m(".", home_dir, "config.ini"); ret.first)
        return ret.second;

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

}
