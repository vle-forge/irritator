// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021

#include <filesystem>
#include <optional>
#include <vector>

namespace irt {

std::optional<std::filesystem::path> get_home_directory();
std::optional<std::filesystem::path> get_executable_directory();
std::optional<std::filesystem::path> get_system_component_dir();
std::optional<std::filesystem::path> get_default_user_component_dir();
std::optional<std::filesystem::path> get_settings_filename() noexcept;

struct file_dialog
{
    enum class status
    {
        show,
        ok,
        cancel,
        hide
    };

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
