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
#include <string_view>

#define irritator_to_string(s) to_string(s)
#define to_string(s) #s

namespace irt {

#if defined(_WIN32)
constexpr static inline auto irritator_name = "irritator-" irritator_to_string(
  VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#else
constexpr static inline auto irritator_name = ".irritator-" irritator_to_string(
  VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
#endif

constexpr std::string_view get_irritator_name() noexcept
{
    return irritator_name;
}

#if defined(__linux__) || defined(__APPLE__)
static expected<path> get_local_home_directory() noexcept
{
    if (auto* home = std::getenv("HOME"); home)
        return { home };

    auto size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (size == -1)
        size = 16384u;

    vector<char>   buf(size, '\0');
    struct passwd  pwd;
    struct passwd* result = nullptr;

    const auto s = getpwuid_r(getpid(), &pwd, buf.data(), size, &result);
    if (s || !result) {
        std::error_code ec;
        if (auto ret = std::filesystem::current_path(ec); !ec) {
            if (auto exists = std::filesystem::exists(ret, ec); !ec && exists) {
                const auto u8 = ret.u8string();

                return path{ reinterpret_cast<const char*>(u8.c_str()) };
            }
        }

        return make_error(std::errc{ errno });
    } else {
        return path{ buf.data() };
    }

    return make_error(std::errc{ errno });
}
#elif defined(_WIN32)
static expected<path> get_local_home_directory() noexcept
{
    PWSTR p{ nullptr };

    if (SUCCEEDED(
          ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p) >= 0)) {
        std::filesystem::path ret;
        ret = p;
        ::CoTaskMemFree(p);
        const auto u8  = ret.u8string();
        const auto ptr = reinterpret_cast<const char*>(u8.c_str());
        const auto len = u8.size();
        return path{ std::string_view(ptr, len) };
    } else {
        std::error_code ec;
        if (auto ret = std::filesystem::current_path(ec); !ec) {
            if (auto exists = std::filesystem::exists(ret, ec); !ec && exists) {
                const auto u8  = ret.u8string();
                const auto ptr = reinterpret_cast<const char*>(u8.c_str());
                const auto len = u8.size();
                return path{ std::string_view(ptr, len) };
            }
        }

        return make_error(std::errc::no_such_file_or_directory);
    }
}
#endif

expected<path> get_home_directory() noexcept
{
    try {
        const auto local_home = get_local_home_directory();

        if (not local_home)
            return local_home.error();

        auto p = *local_home;

        if (not p.can_append(get_irritator_name()))
            return make_error(std::errc::not_enough_memory);

        p /= get_irritator_name();

        const auto std_path = p.to_std_path();

        std::error_code ec;
        if (std::filesystem::is_directory(std_path, ec))
            return p;

        if (std::filesystem::create_directories(std_path, ec))
            return p;
        else
            return make_error(std::errc::file_exists);
    } catch (...) {
    }

    return make_error(std::errc::not_enough_memory);
}

#if defined(__linux__)
expected<path> get_executable_directory() noexcept
{
    vector<char> buf(PATH_MAX, '\0');
    const auto   ssize = readlink("/proc/self/exe", buf.data(), PATH_MAX);

    if (ssize <= 0)
        return make_error(std::errc{ errno });

    const auto size = static_cast<size_t>(ssize);

    return path{ std::string_view{ buf.data(), size } };
}
#elif defined(__APPLE__)
expected<path> get_executable_directory() noexcept
{
    vector<char> buf(MAXPATHLEN, '\0');
    uint32_t     size{ 0 };

    if (_NSGetExecutablePath(buf.data(), &size))
        return make_error(std::errc::bad_address);

    return path{ std::string_view{ buf.data(), size } };
}
#elif defined(_WIN32)
expected<path> get_executable_directory() noexcept
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
            auto  std_path = std::filesystem::path(filepath);
            auto  u8       = std_path.u8string();
            auto* ptr      = reinterpret_cast<const char*>(u8.c_str());
            auto  len      = u8.size();

            return path{ std::string_view(ptr, len) };
        }
    }

    return make_error(std::errc::bad_address);
}
#endif

#if defined(__linux__) || defined(__APPLE__)
expected<path> get_system_component_dir() noexcept
{
    auto exe = get_executable_directory();
    if (!exe)
        return exe.error();

    auto install_path = exe.value().parent_directory();
    install_path /= "share";
    install_path /= get_irritator_name();
    install_path /= "components";

    const auto std_path = install_path.to_std_path();

    std::error_code ec;
    if (std::filesystem::exists(std_path, ec))
        return install_path;

    return make_error(std::errc{ errno });
}
#elif defined(_WIN32)
path build_system_component_path(const path& path) noexcept
{
    auto component_path(path);

    component_path /= "share";
    component_path /= get_irritator_name();
    component_path /= "components";

    return component_path;
}

expected<path> get_system_component_dir() noexcept
{
    auto exe = get_executable_directory();
    if (!exe)
        return exe.error();

    auto gui_path = exe.value().parent_directory();

    std::error_code ec;

    {
        // First, we try to search the system component directory into directory
        // where the executable is running.

        const auto first    = build_system_component_path(gui_path);
        const auto std_path = first.to_std_path();

        if (auto exists = std::filesystem::exists(std_path, ec); !ec && exists)
            return first;
    }

    {
        // If the system component directory is not found into the executable
        // directory, we try to search it into grandparent directory.

        const auto app_path     = gui_path.parent_directory();
        const auto install_path = app_path.parent_directory();
        const auto second       = build_system_component_path(install_path);
        const auto std_path     = second.to_std_path();

        if (auto exists = std::filesystem::exists(std_path, ec); !ec && exists)
            return install_path;
    }

    return make_error(std::errc::bad_address);
}
#endif

#if defined(IRT_DATAROOTDIR)
expected<path> get_system_prefix_component_dir() noexcept
{
    auto       ret         = path{ IRT_DATAROOTDIR };
    const auto irt_dirname = "irritator-" irritator_to_string(
      VERSION_MAJOR) "." irritator_to_string(VERSION_MINOR);
    const auto compo_dirname = "components";

    if (not ret.can_append(irt_dirname))
        return make_error(fs_errc::executable_access_fail);

    ret /= irt_dirname;

    if (not ret.can_append(compo_dirname.sv()))
        return make_error(fs_errc::executable_access_fail);

    ret /= "components";

    const auto      std_path = ret.to_std_path();
    std::error_code ec;

    if (not std::filesystem::exists(std_path, ec))
        return make_error(fs_errc::executable_access_fail);

    return ret;
}
#else
expected<path> get_system_prefix_component_dir() noexcept { return path{}; }
#endif

#if defined(__linux__) || defined(__APPLE__)
expected<path> get_default_user_component_dir() noexcept
{
    auto home_path = get_home_directory();

    if (not home_path)
        return home_path.error();

    auto compo_path = home_path.value();
    compo_path /= "components";

    const auto      std_path = compo_path.to_std_path();
    std::error_code ec;

    if (std::filesystem::exists(std_path, ec))
        return compo_path;

    if (std::filesystem::create_directories(std_path, ec))
        return compo_path;

    return make_error(std::errc::bad_address);
}
#elif defined(_WIN32)
expected<path> get_default_user_component_dir() noexcept
{
    auto home_path = get_home_directory();
    if (!home_path)
        return home_path.error();

    auto compo_path = home_path.value();
    compo_path /= "components";

    const auto      std_path = compo_path.to_std_path();
    std::error_code ec;

    if (std::filesystem::exists(std_path, ec))
        return compo_path;

    if (std::filesystem::create_directories(std_path, ec))
        return compo_path;

    return make_error(std::errc::bad_address);
}
#endif

static expected<path> get_home_filename(const char* filename) noexcept
{
    try {
        auto ret = get_home_directory();
        if (!ret)
            return ret.error();

        *ret /= filename;

        return ret;
    } catch (...) {
    }

    return make_error(std::errc::bad_address);
}

expected<path> get_settings_filename() noexcept
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
        log(0, "irritator-{}.{}.{}-{}\n", VERSION_MAJOR, VERSION_MINOR,
            VERSION_PATCH, VERSION_TWEAK);

#else
        log(0, "irritator-{}.{}.{}\n", VERSION_MAJOR, VERSION_MINOR,
            VERSION_PATCH);
#endif
    }

    expected<path> operator()(std::string_view dir_name,
                              std::string_view subdir_name,
                              std::string_view file_name) noexcept
    {
        debug::ensure(not dir_name.empty());
        debug::ensure(not subdir_name.empty());
        debug::ensure(not file_name.empty());

        auto ret = path(dir_name);
        log(0, "- check directory: {}\n", ret.sv());

        if (not is_directory_and_usable(ret.to_std_path())) {
            log(1, "Is not a directory or bad permissions\n");
            return error_code(std::errc::not_a_directory);
        }

        ret /= subdir_name;
        log(1, "- {}\n", ret.sv());
        if (not is_directory_and_usable(ret.to_std_path())) {
            log(2, "Directory not exists and not usable try to fix\n");
            if (not create_dir(ret.to_std_path())) {
                log(3, "Fail to create directory or change permissions\n");
                return error_code(std::errc::not_a_directory);
            }
        }

        ret /= file_name;
        log(1, "- {}\n", ret.sv());
        if (not is_file_and_usable(ret.to_std_path())) {
            log(2, "Fail to read or create the file. Abort.\n");
            return error_code(std::errc::no_such_file_or_directory);
        }

        log(1, "- irritator config file configured:\n", ret.sv());
        return ret;
    }

private:
    bool try_change_file_permission(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_read |
                                       std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add, ec);

        return not ec.value();
    }

    bool try_change_directory_permission(
      const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::add, ec);

        return ec.value();
    }

    bool is_directory_and_usable(const std::filesystem::path& path) noexcept
    {
        auto ec = std::error_code{};

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

path get_config_home(bool log) noexcept
{
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    config_home_manager m(log);
    small_string<64>    home_dir;

    format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);

    if (auto* xdg = getenv("XDG_CONFIG_HOME"); xdg)
        if (auto ret = m(xdg, home_dir.sv(), "config.ini"); ret.has_value())
            return ret.value();

    if (auto* home = getenv("HOME"); home) {
        auto p = std::filesystem::path(home);
        p /= ".config";

        format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);
        if (auto ret = m(p.c_str(), home_dir.sv(), "config.ini");
            ret.has_value())
            return ret.value().sv();

        format(home_dir, ".irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);

        if (auto ret = m(home, home_dir.sv(), "config.ini"); ret.has_value())
            return ret.value().sv();
    }

    std::error_code ec;
    if (auto path = std::filesystem::current_path(ec); ec)
        if (auto ret = m(path.string(), home_dir.sv(), "config.ini");
            ret.has_value())
            return ret.value().sv();

    if (auto ret = m(".", home_dir.sv(), "config.ini"); ret.has_value())
        return ret.value().sv();

    return "config.ini";
#elif defined(_WIN32)
    if (auto ret = get_home_directory(); ret) {
        auto path(std::move(*ret));
        path /= "config.ini";
        return path.sv();
    }

    return "config.ini";
#endif
}

path get_imgui_filename() noexcept
{
    if (auto path_opt = get_home_filename("imgui.ini"); path_opt.has_value())
        return *path_opt;

    using namespace std::string_view_literals;

    log(log_level::critical, "filesystem.: fail to get imgui.ini file"sv);

    return path{};
}

}
