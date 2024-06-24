// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021

#include <filesystem>
#include <vector>

#include <irritator/error.hpp>

namespace irt {

enum class fs_error {
    user_directory_access_fail,
    user_directory_file_access_fail,
    user_component_directory_access_fail,
    executable_access_fail,
};

/// - unix/linux : Get the user home directory from the @c $HOME environment
///   variable or operating system file entry using @c getpwuid_r otherwise
///   use the current directory.
/// - win32: Use the @c SHGetKnownFolderPath to retrieves the path of the user
///   directory otherwise use the current directory.
result<std::filesystem::path> get_home_directory() noexcept;

/** Retrieves the path of the application binary (the gui, the CLI or unit test)
 * running this code if it exists. */
result<std::filesystem::path> get_executable_directory() noexcept;

/** Retrieves the path `get_executable_directory/irritator-0.1/components` if it
 * exists. */
result<std::filesystem::path> get_system_component_dir() noexcept;

/** Retrieves the path `CMAKE_INSTALL_FULL_DATAROOTDIR/irritator-0.1/components`
 * if it exists. */
result<std::filesystem::path> get_system_prefix_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/components` if it exists. */
result<std::filesystem::path> get_default_user_component_dir() noexcept;

/** Retrieves the path `$HOME/irritator-0.1/settings.ini` if it exists. */
result<std::filesystem::path> get_settings_filename() noexcept;

struct file_dialog {
    enum class status { show, ok, cancel, hide };

    std::vector<std::filesystem::path> paths;
    std::filesystem::path              current;
    std::filesystem::path              selected;
    std::filesystem::path              next;
    std::filesystem::path              result;
    std::u8string                      temp;
    char8_t                            buffer[512];
    uint32_t                           drives = 0;
    status                             state;

    const char8_t** file_filters;
    const char8_t** extension_filters;

    file_dialog() noexcept;

    void clear() noexcept;

    bool show_load_file(const char* title, const char8_t** filters) noexcept;

    bool show_save_file(const char*        title,
                        std::u8string_view default_file_name,
                        const char8_t**    filters) noexcept;

    bool show_select_directory(const char* title) noexcept;
};

}

#endif
