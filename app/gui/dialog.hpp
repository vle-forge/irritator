// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021

#include <filesystem>
#include <optional>

namespace irt {

std::optional<std::filesystem::path> get_home_directory();
std::optional<std::filesystem::path> get_executable_directory();
std::optional<std::filesystem::path> get_system_component_dir();
std::optional<std::filesystem::path> get_default_user_component_dir();
std::optional<std::filesystem::path> get_settings_filename() noexcept;

bool load_file_dialog(std::filesystem::path& out,
                      const char*            title,
                      const char8_t**        filters);

bool save_file_dialog(std::filesystem::path& out,
                      const char*            title,
                      const char8_t**        filters);

bool select_directory_dialog(std::filesystem::path& out);

}

#endif