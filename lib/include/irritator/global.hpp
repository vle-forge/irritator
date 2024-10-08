// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
#define ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP

#include <irritator/container.hpp>
#include <irritator/error.hpp>

#include <array>
#include <filesystem>
#include <mutex>
#include <shared_mutex>

namespace irt {

struct rgba_color {
    constexpr rgba_color() noexcept = default;

    template<std::integral T>
        requires(std::is_same_v<u8, T>)
    constexpr rgba_color(T r_, T g_, T b_, T a_ = 255) noexcept
      : r{ r_ }
      , g{ g_ }
      , b{ b_ }
      , a{ a_ }
    {}

    template<std::integral T>
        requires(not std::is_same_v<u8, T>)
    constexpr rgba_color(T r_, T g_, T b_, T a_ = 255) noexcept
      : r{ static_cast<u8>(std::clamp(r_, 0, 255)) }
      , g{ static_cast<u8>(std::clamp(g_, 0, 255)) }
      , b{ static_cast<u8>(std::clamp(b_, 0, 255)) }
      , a{ static_cast<u8>(std::clamp(a_, 0, 255)) }
    {}

    template<std::floating_point T>
    constexpr rgba_color(T r_, T g_, T b_, T a_ = 255) noexcept
      : r{ static_cast<u8>(std::clamp(r_* T{ 255 }, T{ 0 }, T{ 255 })) }
      , g{ static_cast<u8>(std::clamp(g_* T{ 255 }, T{ 0 }, T{ 255 })) }
      , b{ static_cast<u8>(std::clamp(b_* T{ 255 }, T{ 0 }, T{ 255 })) }
      , a{ static_cast<u8>(std::clamp(a_* T{ 255 }, T{ 0 }, T{ 255 })) }
    {}

    std::uint8_t r, g, b, a;
};

constexpr inline rgba_color operator*(const rgba_color& lhs,
                                      const rgba_color& rhs)
{
    const auto r = static_cast<int>(lhs.r) * static_cast<int>(rhs.r);
    const auto g = static_cast<int>(lhs.g) * static_cast<int>(rhs.g);
    const auto b = static_cast<int>(lhs.b) * static_cast<int>(rhs.b);
    const auto a = static_cast<int>(lhs.a) * static_cast<int>(rhs.a);

    return rgba_color(r, g, b, a);
}

constexpr inline rgba_color operator*(const rgba_color& lhs, const float t)
{
    const auto r = static_cast<int>(static_cast<float>(lhs.r) * t);
    const auto g = static_cast<int>(static_cast<float>(lhs.g) * t);
    const auto b = static_cast<int>(static_cast<float>(lhs.b) * t);
    const auto a = static_cast<int>(static_cast<float>(lhs.a) * t);

    return rgba_color(r, g, b, a);
}

constexpr inline rgba_color operator-(const rgba_color& lhs,
                                      const rgba_color& rhs)
{
    const auto r = static_cast<int>(lhs.r) - static_cast<int>(rhs.r);
    const auto g = static_cast<int>(lhs.g) - static_cast<int>(rhs.g);
    const auto b = static_cast<int>(lhs.b) - static_cast<int>(rhs.b);
    const auto a = static_cast<int>(lhs.a) - static_cast<int>(rhs.a);

    return rgba_color(r, g, b, a);
}

constexpr inline rgba_color operator+(const rgba_color& lhs,
                                      const rgba_color& rhs)
{
    const auto r = static_cast<int>(lhs.r) + static_cast<int>(rhs.r);
    const auto g = static_cast<int>(lhs.g) + static_cast<int>(rhs.g);
    const auto b = static_cast<int>(lhs.b) + static_cast<int>(rhs.b);
    const auto a = static_cast<int>(lhs.a) + static_cast<int>(rhs.a);

    return rgba_color(r, g, b, a);
}

constexpr inline rgba_color lerp(const rgba_color& a,
                                 const rgba_color& b,
                                 const float       t) noexcept
{
    const auto rr = static_cast<int>(
      (static_cast<float>(a.r) +
       (static_cast<float>(b.r) - static_cast<float>(a.r)) * t));
    const auto rg = static_cast<int>(
      (static_cast<float>(a.g) +
       (static_cast<float>(b.g) - static_cast<float>(a.g)) * t));
    const auto rb = static_cast<int>(
      (static_cast<float>(a.b) +
       (static_cast<float>(b.b) - static_cast<float>(a.b)) * t));
    const auto ra = static_cast<int>(
      (static_cast<float>(a.a) +
       (static_cast<float>(b.a) - static_cast<float>(a.a)) * t));

    return rgba_color{ rr, rg, rb, ra };
}

enum class gui_theme_id : u32;
enum class recorded_path_id : u32 {};

struct gui_themes {
    id_array<gui_theme_id> ids;

    vector<std::array<rgba_color, 60>> colors;
    vector<small_string<31>>           names;

    gui_theme_id selected{ 0 };
};

struct recorded_paths {
    id_array<recorded_path_id> ids;

    vector<small_string<256 * 16 - 2>> paths;
    vector<small_string<31>>           names;
    vector<int>                        priorities;

    vector<recorded_path_id> sort_by_priorities() const noexcept
    {
        vector<recorded_path_id> ret(ids.size());

        if (ids.capacity() >= ids.ssize()) {
            for (auto id : ids)
                ret.emplace_back(id);

            std::sort(
              ret.begin(), ret.end(), [&](const auto a, const auto b) noexcept {
                  const auto idx_a = get_index(a);
                  const auto idx_b = get_index(b);

                  return idx_a < idx_b;
              });
        }

        return ret;
    }
};

struct variables {
    gui_themes     g_themes;
    recorded_paths rec_paths;
};

class config
{
public:
    config(std::shared_mutex&                      mutex,
           const std::shared_ptr<const variables>& vars)
      : m_lock(mutex)
      , m_vars{ vars }
    {}

    /** Get the underlying @c variables object.
     *
     * @c attention Do not store the underlying variable @c variables and free
     * the config. It may be delete or replace in writer thread.
     */
    [[nodiscard]] const variables& vars() const noexcept { return *m_vars; }

private:
    /** Multiple reader (@c config object) can take the lock. */
    std::shared_lock<std::shared_mutex> m_lock;
    std::shared_ptr<const variables>    m_vars;
};

class config_rw
{
public:
    config_rw(std::shared_mutex& mutex, const std::shared_ptr<variables>& vars)
      : m_lock{ mutex }
      , m_vars{ vars }
    {}

    /** Get the underlying @c variables object.
     *
     * @c attention Do not store the underlying variable @c variables and free
     * the config. It may be delete or replace in writer thread.
     */
    [[nodiscard]] variables& vars() const noexcept { return *m_vars; }

private:
    /** Unique writer (@c config_rw object) at a time can take the lock. */
    std::unique_lock<std::shared_mutex> m_lock;
    std::shared_ptr<variables>          m_vars;
};

class config_manager
{
public:
    /** Build a @c variables object from static data. Useful in unit-tests. */
    config_manager() noexcept;

    /** Build a @c variables object from the file @c config_path. If it fail,
       the default object from static data is build. */
    config_manager(const std::string config_path) noexcept;

    config_manager(config_manager&&) noexcept            = delete;
    config_manager& operator=(config_manager&&) noexcept = delete;

    config get() const
    {
        return config{ m_mutex,
                       std::static_pointer_cast<const variables>(m_vars) };
    }

    config_rw get_rw() const { return config_rw{ m_mutex, m_vars }; }

    std::error_code save() const noexcept;
    std::error_code load() noexcept;

    void                       swap(std::shared_ptr<variables>& other) noexcept;
    std::shared_ptr<variables> copy() const noexcept;

private:
    /** Stores the configuration path (@c `$XDG_RUNTIME_DIR/irritator.ini` or @c

       `%HOMEDIR%/%HOMEPATH&/irritator.ini`) directories. @TODO move from
       std::string into small_string or vector<char> with cold memory allocator.
     */
    const std::string m_path;

    std::shared_ptr<variables> m_vars;

    /** Use a @c std::shared_mutex to permit multiple readers with @c
       std::shared_lock in @c config object and unique writer with @c
       std::unique_lock. */
    mutable std::shared_mutex m_mutex;
};

/** Retrieves the path of the file @c "irritator.ini" from the directoy @c
 * conf_dir/irritator-x.y/ where @c conf_dir equals:
 *
 *  - Unix/Linux: Try to the directories @c XDG_CONFIG_HOME, @c HOME or current
 *    directory.
 *  - Win32: Use the local application data directory.
 */
std::string get_config_home(bool log = false) noexcept;

/** Retrives the home directory of the current user:
 * - unix/linux : Get the user home directory from the @c $HOME environment
 *   variable or operating system file entry using @c getpwuid_r otherwise
 *   use the current directory.
 * - win32: Use the @c SHGetKnownFolderPath to retrieves the path of the user
 *   directory otherwise use the current directory.
 */
result<std::filesystem::path> get_home_directory() noexcept;

/** Retrieves the path of the application binary (the gui, the CLI or unit test)
 * running this code if it exists. */
result<std::filesystem::path> get_executable_directory() noexcept;

/** Retrieves the path `get_executable_directory/irritator-0.1/components` if it
 * exists. */
result<std::filesystem::path> get_system_component_dir() noexcept;

/** Retrieves the path `CMAKE_INSTALL_FULL_DATAROOTDIR/irritator-0.1/components`
 * if it exists.
 * @TODO Win32 need to use the windows executable or registry to
 * found the correct system folder. On Unix/Linux the path ise install directory
 * but, we can use also the executable path to determine the install directory.
 */
result<std::filesystem::path> get_system_prefix_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/components` if it exists. */
result<std::filesystem::path> get_default_user_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/settings.ini` if it exists. */
result<std::filesystem::path> get_settings_filename() noexcept;

} // namespace irt

#endif // ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
