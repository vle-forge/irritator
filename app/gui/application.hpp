// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>

#include <filesystem>
#include <optional>

namespace irt {

struct application;
struct editor;

struct application
{
    data_array<editor, editor_id> editors;
    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;
    external_source srcs;

    bool show_log = true;
    bool show_simulation = true;
    bool show_demo = false;
    bool show_plot = true;
    bool show_settings = false;
    bool show_sources_window = false;

    bool init();
    bool show();
    void shutdown();

    // For each editor run the simulation. Use this function outside of the
    // ImGui::Render/NewFrame to not block the graphical user interface.
    void run_simulations();

    void show_sources(bool* is_show);
    void show_menu_sources(const char* title, source& src);

    void show_plot_window();
    void show_simulation_window();
    void show_settings_window();

    editor* alloc_editor();
    void free_editor(editor& ed);
};



} // namespace irt

#endif
