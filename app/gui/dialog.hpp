// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_DIALOG_2021

#include <filesystem>

#include <irritator/container.hpp>

namespace irt {

struct file_dialog {
    enum class status { show, ok, cancel, hide };

    vector<std::filesystem::path> paths;
    std::filesystem::path         current;
    std::filesystem::path         selected;
    std::filesystem::path         next;
    std::filesystem::path         result;
    std::u8string                 temp;
    vector<char8_t>               buffer;
    uint32_t                      drives = 0;
    status                        state;

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
