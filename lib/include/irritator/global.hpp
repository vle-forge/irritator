// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
#define ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP

#include <irritator/container.hpp>
#include <irritator/error.hpp>

#include <filesystem>
#include <mutex>
#include <shared_mutex>

namespace irt {

enum class recorded_path_id : u32 {};
enum class gui_theme_id : u32 {};

struct gui_themes {
    id_array<gui_theme_id> ids;

    vector<small_string<31>> names;

    gui_theme_id selected{ 0 };
};

struct recorded_paths {
    id_array<recorded_path_id> ids;

    vector<small_string<256 * 16 - 2>> paths;
    vector<small_string<31>>           names;
    vector<int>                        priorities;

    vector<recorded_path_id> sort_by_priorities() const noexcept;
};

struct variables {
    gui_themes     g_themes;
    recorded_paths rec_paths;
};

class config_manager
{
public:
    /** Build a @a variables object from static data. Useful in unit-tests. */
    config_manager() noexcept;

    /** Build a @a variables object from the file @a config_path. If it fail,
       the default object from static data is build. Priority is given to @a
       config_path data. */
    explicit config_manager(const std::string config_path) noexcept;

    config_manager(config_manager&&) noexcept            = delete;
    config_manager& operator=(config_manager&&) noexcept = delete;

    /**
     * Get the underlying @a variables object under @a shared_lock locker.
     *
     * @attention Do not store pointer or reference to any members of @a
     * variables after the destruction of the @a config object (@c
     * use-after-free). the config.
     *
     * @code
     * config_manager cfgm;
     * ...
     * small_string<127> name;
     * {
     *     read_write([&name](const auto& vars) {
     *         name = vars.g_themes.names[0];
     *     });
     * }
     * @endcode
     */
    template<typename Function, typename... Args>
    void read(Function&& fn, Args&&... args) noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        fn(std::as_const(*m_vars), args...);
    }

    /**
     * Get the underlying @a variables object under a @a unique_lock locker.
     *
     * @attention Do not store pointer or reference to any members of @a
     * variables after the destruction of the @a config object (@c
     * use-after-free). the config.
     *
     * @code
     * config_manager cfgm;
     * ...
     * small_string<127> name = "toto";
     * {
     *     read_write([&name](auto& vars) {
     *         vars.g_themes.names[0] = name;
     *     });
     * }
     * @endcode
     */
    template<typename Function, typename... Args>
    void read_write(Function&& fn, Args&&... args) noexcept
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        fn(*m_vars, args...);
    }

    /**
     * Try to get the underlying @a variables object under @a shared_lock
     * try-locker. The function @a fn is call if and only if the lock is
     * acquired otherwise, the function is not call and the caller take the
     * control.
     *
     * @attention Do not store pointer or reference to any members of @a
     * variables after the destruction of the @a config object (@c
     * use-after-free). the config.
     *
     * @code
     * config_manager cfgm;
     * ...
     * small_string<127> name;
     * {
     *     read_write([&name](const auto& vars) {
     *         name = vars.g_themes.names[0];
     *     });
     * }
     * @endcode
     */
    template<typename Function, typename... Args>
    void try_read(Function&& fn, Args&&... args) noexcept
    {
        if (std::shared_lock<std::shared_mutex> lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            fn(std::as_const(*m_vars), args...);
        }
    }

    std::error_code save() const noexcept;
    std::error_code load() noexcept;

    void                       swap(std::shared_ptr<variables>& other) noexcept;
    std::shared_ptr<variables> copy() const noexcept;

private:
    /**
     * Stores the configuration path (@a `$XDG_RUNTIME_DIR/irritator.ini` or @a
     *
     * `%HOMEDIR%/%HOMEPATH&/irritator.ini`) directories. @TODO move from
     * std::string into small_string or vector<char> with cold memory allocator.
     */
    const std::string m_path;

    std::shared_ptr<variables> m_vars;

    /** Use a @a std::shared_mutex to permit multiple readers with @a
     * std::shared_lock in @a config object and unique writer with @a *
     * std::unique_lock. */
    mutable std::shared_mutex m_mutex;
};

/** Retrieves the path of the file @a "irritator.ini" from the directoy @a
 * conf_dir/irritator-x.y/ where @a conf_dir equals:
 *
 *  - Unix/Linux: Try to the directories @a XDG_CONFIG_HOME, @a HOME or current
 *    directory.
 *  - Win32: Use the local application data directory.
 */
std::string get_config_home(bool log = false) noexcept;

/** Retrives the home directory of the current user:
 * - unix/linux : Get the user home directory from the @a $HOME environment
 *   variable or operating system file entry using @a getpwuid_r otherwise
 *   use the current directory.
 * - win32: Use the @a SHGetKnownFolderPath to retrieves the path of the user
 *   directory otherwise use the current directory.
 */
expected<std::filesystem::path> get_home_directory() noexcept;

/** Retrieves the path of the application binary (the gui, the CLI or unit test)
 * running this code if it exists. */
expected<std::filesystem::path> get_executable_directory() noexcept;

/** Retrieves the path `get_executable_directory/irritator-0.1/components` if it
 * exists. */
expected<std::filesystem::path> get_system_component_dir() noexcept;

/** Retrieves the path `CMAKE_INSTALL_FULL_DATAROOTDIR/irritator-0.1/components`
 * if it exists.
 * @TODO Win32 need to use the windows executable or registry to
 * found the correct system folder. On Unix/Linux the path ise install directory
 * but, we can use also the executable path to determine the install directory.
 */
expected<std::filesystem::path> get_system_prefix_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/components` if it exists. */
expected<std::filesystem::path> get_default_user_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/settings.ini` if it exists. */
expected<std::filesystem::path> get_settings_filename() noexcept;

} // namespace irt

#endif // ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
