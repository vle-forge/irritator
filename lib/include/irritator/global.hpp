// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
#define ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP

#include <irritator/container.hpp>
#include <irritator/error.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <span>

namespace irt {

enum class recorded_path_id : u32 {};

constexpr static const char* themes[] = {
    "Modern",          "Dark",         "Light",     "Bess Dark",
    "Catpuccin Mocha", "Material You", "Fluent UI", "Fluent UI - Light"
};

enum class style_color {
    background_error_notification,   /**< Background of error notification
                                        windows. */
    background_warning_notification, /**< Background of warning notification
                                        windows. */
    background_info_notification, /**< Background of info notification windows.
                                   */
    background, /** Background of generic, grid, graph, hsm components. */
    background_selection,
    inner_border,
    outer_border,
    component_undefined, /**< Color of undefined object in components. */
    edge,
    edge_hovered,
    edge_active,
    node,
    node_hovered,
    node_active,
    node_2,
    node_2_hovered,
    node_2_active,
    COUNT,
};

struct theme_colors {
    using color_type = std::array<float, 4>;

    static constexpr auto color_size = static_cast<int>(style_color::COUNT);

    static constexpr int color_of(const style_color elem) noexcept
    {
        fatal::ensure(elem != style_color::COUNT);

        return static_cast<int>(elem);
    }

    std::span<const float, 4> operator[](const style_color c) const noexcept
    {
        return std::span(colors[color_of(c)]);
    }

    std::span<float, 4> operator[](const style_color c) noexcept
    {
        return std::span(colors[color_of(c)]);
    }

    std::array<color_type, color_size> colors;
};

struct recorded_paths {
    id_array<recorded_path_id> ids;

    vector<small_string<256 * 16 - 2>> paths;
    vector<small_string<31>>           names;
    vector<int>                        priorities;

    vector<recorded_path_id> sort_by_priorities() const noexcept;
};

struct variables {
    recorded_paths rec_paths;
};

//! Enumeration class used everywhere in irritator to produce log data.
enum class log_level {
    emergency,
    alert,
    critical,
    error,
    warning,
    notice,
    info,
    fatal
};

/**
 * @brief A thread safe log handler. A log entry is have a @a log_level, a
 * tittle, a description and a creation time since epoch.
 *
 * Two containers are used, a @a id_data_array to stores data and a @a
 * ring_buffer to build a circular buffer to keep order of creation.
 *
 * Several thread can read the containers at the same time but, the registration
 * of new entries is done with a unique thread at a time.
 */
class journal_handler
{
public:
    enum class entry_id : u32; /**< The @a id_data_array identifier to keep in
                                  your own container or in the @a ring_buffer.
                                */

    using title = small_string<127>;
    using descr = small_string<1022>;

    /**
     * @brief Reserve memory for both @a ring_buffer and @a id_data_array for 32
     * entries.
     */
    journal_handler() noexcept;

    explicit journal_handler(
      const constrained_value<int, 4, INT_MAX> len) noexcept;

    /**
     * @brief Wait lock and give access to log and ring data then clear the
     * structure
     * @tparam Function Any callable.
     * @tparam ...Args Any type to argument of @a Function.
     * @param fn The callable.
     * @param ...args The argument of the @a Function or nothing.
     */
    template<typename Function, typename... Args>
    void get(Function&& fn, Args&&... args) noexcept
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        std::invoke(std::forward<Function>(fn),
                    m_logs,
                    m_ring,
                    std::forward<Args>(args)...);
        m_logs.clear();
        m_ring.clear();
        m_number = 0;
    }

    /**
     * @brief Give access to log and ring data then clear the structure if the
     * lock successed otherwise the function do nothing.
     * @tparam Function Any callable.
     * @tparam ...Args Any type to argument of @a Function.
     * @param fn The callable.
     * @param ...args The argument of the @a Function or nothing.
     */
    template<typename Function, typename... Args>
    void try_get(Function&& fn, Args&&... args) noexcept
    {
        if (std::unique_lock<std::shared_mutex> lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            std::invoke(std::forward<Function>(fn),
                        m_logs,
                        m_ring,
                        std::forward<Args>(args)...);
            m_logs.clear();
            m_ring.clear();
            m_number = 0;
        }
    }

    template<typename Function, typename... Args>
    void read(Function&& fn, Args&&... args) const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::invoke(std::forward<Function>(fn),
                    std::as_const(m_logs),
                    std::as_const(m_ring),
                    std::forward<Args>(args)...);
    }

    template<typename Function, typename... Args>
    bool try_read(Function&& fn, Args&&... args) const noexcept
    {
        if (std::shared_lock<std::shared_mutex> lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            if (m_ring.empty()) {
                std::invoke(std::forward<Function>(fn),
                            std::as_const(m_logs),
                            std::as_const(m_ring),
                            std::forward<Args>(args)...);
                return true;
            }
        }

        return false;
    }

    template<typename Function, typename... Args>
    void push(log_level level, Function&& fn, Args&&... args) noexcept
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (not m_logs.can_alloc(1))
            pop();

        const auto id                = m_logs.alloc();
        const auto idx               = get_index(id);
        m_logs.get<log_level>()[idx] = level;
        m_logs.get<u64>()[idx]       = get_tick_count_in_milliseconds();

        std::invoke(std::forward<Function>(fn),
                    m_logs.get<title>()[idx],
                    m_logs.get<descr>()[idx],
                    std::forward<Args>(args)...);

        m_ring.emplace_tail(id);
        ++m_number;
    }

    template<typename Function, typename... Args>
    bool try_push(log_level level, Function&& fn, Args&&... args) noexcept
    {
        if (std::unique_lock<std::shared_mutex> lock(m_mutex, std::try_to_lock);
            lock.owns_lock()) {
            if (not m_logs.can_alloc(1))
                pop();

            const auto id                = m_logs.alloc();
            const auto idx               = get_index(id);
            m_logs.get<log_level>()[idx] = level;
            m_logs.get<u64>()[idx]       = get_tick_count_in_milliseconds();

            std::invoke(std::forward<Function>(fn),
                        m_logs.get<title>()[idx],
                        m_logs.get<descr>()[idx],
                        std::forward<Args>(args)...);

            m_ring.emplace_tail(id);
            ++m_number;
            return true;
        }

        return false;
    }

    void clear() noexcept;
    void clear(std::span<entry_id> entries) noexcept;

    unsigned capacity() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_logs.capacity();
    }

    unsigned size() const noexcept { return static_cast<unsigned>(m_number); }
    int      ssize() const noexcept { return m_number; }

    static u64 get_tick_count_in_milliseconds() noexcept;
    static u64 get_elapsed_time(const u64 from) noexcept;

private:
    void pop() noexcept
    {
        auto old = m_ring.tail();
        m_ring.pop_tail();
        m_logs.free(*old);
    }

    mutable std::shared_mutex m_mutex;
    std::atomic_int           m_number;

    id_data_array<entry_id,
                  allocator<new_delete_memory_resource>,
                  title,
                  descr,
                  u64,
                  log_level>
      m_logs;

    ring_buffer<entry_id> m_ring;
};

/**
 * @brief Consume the journal and push into @a stdout by default or in a file @a
 * std::FILE.
 */
class stdfile_journal_consumer
{
    std::FILE* m_fp = stdout;

public:
    stdfile_journal_consumer() noexcept = default;
    ~stdfile_journal_consumer() noexcept;

    explicit stdfile_journal_consumer(
      const std::filesystem::path& path) noexcept;

    void read(journal_handler& lm) noexcept;
};

class gui_log_consumer
{};

class config_manager
{
public:
    /** Build a @a variables object from static data. Useful in unit-tests.
     */
    config_manager() noexcept;

    /** Build a @a variables object from the file @a config_path. If it
       fail, the default object from static data is build. Priority is given
       to @a config_path data. */
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
        std::invoke(std::forward<Function>(fn),
                    std::as_const(*m_vars),
                    std::forward<Args>(args)...);
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
        std::invoke(
          std::forward<Function>(fn), *m_vars, std::forward<Args>(args)...);
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
            std::invoke(std::forward<Function>(fn),
                        std::as_const(*m_vars),
                        std::forward<Args>(args)...);
        }
    }

    std::error_code save() const noexcept;
    std::error_code load() noexcept;

    void                       swap(std::shared_ptr<variables>& other) noexcept;
    std::shared_ptr<variables> copy() const noexcept;

    theme_colors colors;
    int          theme = 0;

private:
    /**
     * Stores the configuration path (@a `$XDG_RUNTIME_DIR/irritator.ini` or
     * @a
     *
     * `%HOMEDIR%/%HOMEPATH&/irritator.ini`) directories. @TODO move from
     * std::string into small_string or vector<char> with cold memory
     * allocator.
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
 *  - Unix/Linux: Try to the directories @a XDG_CONFIG_HOME, @a HOME or
 * current directory.
 *  - Win32: Use the local application data directory.
 */
std::string get_config_home(bool log = false) noexcept;

/** Retrives the home directory of the current user:
 * - unix/linux : Get the user home directory from the @a $HOME environment
 *   variable or operating system file entry using @a getpwuid_r otherwise
 *   use the current directory.
 * - win32: Use the @a SHGetKnownFolderPath to retrieves the path of the
 * user directory otherwise use the current directory.
 */
expected<std::filesystem::path> get_home_directory() noexcept;

/** Retrieves the path of the application binary (the gui, the CLI or unit
 * test) running this code if it exists. */
expected<std::filesystem::path> get_executable_directory() noexcept;

/** Retrieves the path `get_executable_directory/irritator-0.1/components`
 * if it exists. */
expected<std::filesystem::path> get_system_component_dir() noexcept;

/** Retrieves the path
 * `CMAKE_INSTALL_FULL_DATAROOTDIR/irritator-0.1/components` if it exists.
 * @TODO Win32 need to use the windows executable or registry to
 * found the correct system folder. On Unix/Linux the path ise install
 * directory but, we can use also the executable path to determine the
 * install directory.
 */
expected<std::filesystem::path> get_system_prefix_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/components` if it exists. */
expected<std::filesystem::path> get_default_user_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/settings.ini` if it exists. */
expected<std::filesystem::path> get_settings_filename() noexcept;

} // namespace irt

#endif // ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
