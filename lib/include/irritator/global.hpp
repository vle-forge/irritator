// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
#define ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP

#include <irritator/container.hpp>

#include <array>
#include <shared_mutex>

namespace irt {

struct rgba_color {
    std::uint8_t r, g, b, a;
};

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

    config get() const { return config{ m_mutex, m_vars }; }

    std::error_code save() const noexcept;
    std::error_code load() noexcept;

    void swap(std::shared_ptr<const variables>& other) noexcept;
    std::shared_ptr<const variables> copy() const noexcept;

private:
    std::shared_ptr<const variables> m_vars;

    /** Stores the configuration path (@c `$XDG_RUNTIME_DIR/irritator.ini` or @c
       `%HOMEDIR%/%HOMEPATH&/irritator.ini`) directories. @TODO move from
       std::string into small_string or vector<char> with cold memory allocator.
     */
    const std::string m_path;

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

} // namespace irt

#endif // ORG_VLEPROJECT_IRRITATOR_GLOBAL_HPP
