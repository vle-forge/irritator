// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <imgui_internal.h>

namespace irt {

static constexpr const i32 simulation_task_number = 64;

application::application() noexcept
  : task_mgr{}
{
    settings.update();
    settings.apply_default_style();

    sim_tasks.init(simulation_task_number);
    gui_tasks.init(simulation_task_number);

    log_w(*this, log_level::info, "GUI Irritator start\n");

    log_w(*this,
          log_level::info,
          "Start with {} main threads and {} generic workers\n",
          task_mgr.main_workers.ssize(),
          task_mgr.temp_workers.ssize());

    log_w(*this, log_level::info, "Initialization successfull");

    task_mgr.start();
}

application::~application() noexcept
{
    log_w(*this, log_level::info, "Task manager shutdown\n");
    task_mgr.finalize();
    log_w(*this, log_level::info, "Application shutdown\n");
}

bool application::init() noexcept
{
    if (auto ret = c_editor.mod.registred_paths.init(max_component_dirs);
        is_bad(ret)) {
        log_w(
          *this, log_level::alert, "Fail to initialize registred dir paths");
    }

    if (auto ret = load_settings(); is_bad(ret))
        log_w(*this,
              log_level::alert,
              "Fail to read settings files. Default parameters used\n");

    if (auto ret = c_editor.mod.init(mod_init); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize modeling components: {}\n",
              status_string(ret));
        return false;
    }

    if (c_editor.mod.registred_paths.size() == 0) {
        if (auto path = get_system_component_dir(); path) {
            auto& new_dir    = c_editor.mod.registred_paths.alloc();
            auto  new_dir_id = c_editor.mod.registred_paths.get_id(new_dir);
            new_dir.name     = "System directory";
            new_dir.path     = path.value().string().c_str();
            log_w(*this,
                  log_level::info,
                  "Add system directory: {}\n",
                  new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }

        if (auto path = get_default_user_component_dir(); path) {
            auto& new_dir    = c_editor.mod.registred_paths.alloc();
            auto  new_dir_id = c_editor.mod.registred_paths.get_id(new_dir);
            new_dir.name     = "User directory";
            new_dir.path     = path.value().string().c_str();
            log_w(*this,
                  log_level::info,
                  "Add user directory: {}\n",
                  new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }
    }

    if (auto ret = save_settings(); is_bad(ret)) {
        log_w(*this, log_level::error, "Fail to save settings files.\n");
    }

    if (auto ret = s_editor.sim.init(mod_init.model_capacity,
                                     mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize simulation components: {}\n",
              status_string(ret));
        return false;
    }

    s_editor.displacements.resize(mod_init.model_capacity);

    if (auto ret = s_editor.sim_obs.init(16); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = s_editor.copy_obs.init(16); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize copy simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.srcs.init(50); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize external sources: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to fill component list: {}\n",
              status_string(ret));
    }

    c_editor.mod.fill_components();
    c_editor.mod.head           = undefined<tree_node_id>();
    c_editor.selected_component = undefined<tree_node_id>();

    auto id = c_editor.add_empty_component();
    c_editor.open_as_main(id);

    return true;
}

template<typename DataArray>
static void cleanup_sim_or_gui_task(DataArray& d_array) noexcept
{
    typename DataArray::value_type* task   = nullptr;
    typename DataArray::value_type* to_del = nullptr;

    while (d_array.next(task)) {
        if (to_del) {
            d_array.free(*to_del);
            to_del = nullptr;
        }

        if (task->state == task_status::finished) {
            to_del = task;
        }
    }

    if (to_del)
        d_array.free(*to_del);
}

static void application_show_menu(application& app) noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("New project"))
                app.new_project_file = true;

            if (ImGui::MenuItem("Open project"))
                app.load_project_file = true;

            if (ImGui::MenuItem("Save project"))
                app.save_project_file = true;

            if (ImGui::MenuItem("Save project as..."))
                app.save_as_project_file = true;

            if (ImGui::MenuItem("Quit"))
                app.menu_quit = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show modeling editor", nullptr, &app.c_editor.is_open);
            ImGui::MenuItem(
              "Show simulation editor", nullptr, &app.s_editor.is_open);
            ImGui::MenuItem(
              "Show output editor", nullptr, &app.output_ed.is_open);
            ImGui::MenuItem(
              "Show data editor", nullptr, &app.data_editor.is_open);

            ImGui::MenuItem(
              "Show preview window", nullptr, &app.preview_wnd.is_open);
            ImGui::MenuItem(
              "Show component hierarchy", nullptr, &app.library_wnd.is_open);

            ImGui::MenuItem(
              "Show memory usage", nullptr, &app.memory_wnd.is_open);
            ImGui::MenuItem("Show task usage", nullptr, &app.t_window.is_open);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("ImGui demo window", nullptr, &app.show_imgui_demo);
            ImGui::MenuItem(
              "ImPlot demo window", nullptr, &app.show_implot_demo);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &app.settings.is_open);

            if (ImGui::MenuItem("Load settings"))
                app.load_settings();

            if (ImGui::MenuItem("Save settings"))
                app.save_settings();

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

static void application_manage_menu_action(application& app) noexcept
{
    if (app.new_project_file) {
        app.c_editor.unselect();
        auto id = app.c_editor.add_empty_component();
        app.c_editor.open_as_main(id);
        app.new_project_file = false;
    }

    if (app.load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_load_file(title, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.c_editor.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.c_editor.mod.registred_paths.alloc();
                    auto  id   = app.c_editor.mod.registred_paths.get_id(path);
                    path.path  = str;

                    app.add_simulation_task(task_load_project, ordinal(id));
                }
            }

            app.f_dialog.clear();
            app.load_project_file = false;
        }
    }

    if (app.save_project_file) {
        const bool have_file = !app.project_file.empty();

        if (have_file) {
            auto  u8str = app.project_file.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (app.c_editor.mod.registred_paths.can_alloc(1)) {
                auto& path = app.c_editor.mod.registred_paths.alloc();
                auto  id   = app.c_editor.mod.registred_paths.get_id(path);
                path.path  = str;

                app.add_simulation_task(task_save_project, ordinal(id));
            }
        } else {
            app.save_project_file    = false;
            app.save_as_project_file = true;
        }
    }

    if (app.save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_save_file(title, default_filename, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.c_editor.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.c_editor.mod.registred_paths.alloc();
                    auto  id   = app.c_editor.mod.registred_paths.get_id(path);
                    path.path  = str;

                    app.add_simulation_task(task_save_project, ordinal(id));
                }
            }

            app.f_dialog.clear();
            app.save_as_project_file = false;
        }
    }

    if (app.output_ed.write_output) {
        auto* obs =
          app.s_editor.sim_obs.try_to_get(app.s_editor.selected_sim_obs);

        if (obs) {
            const char*              title = "Select raw file path to save";
            const std::u8string_view default_filename = u8"filename.txt";
            const char8_t*           filters[]        = { u8".txt", nullptr };

            ImGui::OpenPopup(title);
            if (app.f_dialog.show_save_file(title, default_filename, filters)) {
                if (app.f_dialog.state == file_dialog::status::ok) {
                    obs->file = app.f_dialog.result;
                    obs->write(obs->file);
                }

                app.s_editor.selected_sim_obs =
                  undefined<simulation_observation_id>();
                app.output_ed.write_output = false;
                app.f_dialog.clear();
            }
        }
    }
}

static void application_show_windows(application& app) noexcept
{
    constexpr ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID        dockspace_id =
      ImGui::DockSpaceOverViewport(viewport, dockspace_flags);

    static auto first_time = true;
    if (first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(
          dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        auto dock_id_right = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
        auto dock_id_left = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
        auto dock_id_down = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Down, 0.2f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow(project_window::name, dock_id_left);

        ImGui::DockBuilderDockWindow(component_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(simulation_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(output_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(data_window::name, dockspace_id);

        ImGui::DockBuilderDockWindow(app.library_wnd.name, dock_id_right);
        ImGui::DockBuilderDockWindow(app.preview_wnd.name, dock_id_right);

        ImGui::DockBuilderDockWindow(window_logger::name, dock_id_down);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (app.project_wnd.is_open)
        app.project_wnd.show();

    if (app.c_editor.is_open)
        app.c_editor.show();

    if (app.s_editor.is_open)
        app.s_editor.show();

    if (app.output_ed.is_open)
        app.output_ed.show();

    if (app.data_editor.is_open)
        app.data_editor.show();

    if (app.log_window.is_open)
        app.log_window.show();

    if (app.library_wnd.is_open)
        app.library_wnd.show();

    if (app.preview_wnd.is_open)
        app.preview_wnd.show();
}

void application::show() noexcept
{
#ifdef IRRITATOR_USE_TTF
    if (ttf)
        ImGui::PushFont(ttf);
#endif

    cleanup_sim_or_gui_task(sim_tasks);
    cleanup_sim_or_gui_task(gui_tasks);

    if (!c_editor.mod.log_entries.empty()) {
        while (!c_editor.mod.log_entries.empty()) {
            auto& w = c_editor.mod.log_entries.front();
            c_editor.mod.log_entries.pop_front();

            auto& n   = notifications.alloc();
            n.level   = w.level;
            n.title   = w.buffer.sv();
            n.message = status_string(w.st);

            if (match(w.level, log_level::info, log_level::debug))
                n.only_log = true;

            notifications.enable(n);
        }
    }

    application_show_menu(*this);
    application_manage_menu_action(*this);

    s_editor.simulation_update_state();

    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);

    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);

    application_show_windows(*this);

    if (memory_wnd.is_open)
        memory_wnd.show();

    if (t_window.is_open)
        t_window.show();

    if (settings.is_open)
        settings.show();

    if (show_select_directory_dialog) {
        const char* title = "Select directory";
        ImGui::OpenPopup(title);
        if (f_dialog.show_select_directory(title)) {
            if (f_dialog.state == file_dialog::status::ok) {
                select_directory = f_dialog.result;
                auto* dir_path =
                  c_editor.mod.registred_paths.try_to_get(select_dir_path);
                if (dir_path) {
                    auto str = select_directory.string();
                    dir_path->path.assign(str);
                }

                show_select_directory_dialog = false;
                select_dir_path              = undefined<registred_path_id>();
                select_directory.clear();
            }

            f_dialog.clear();
            show_select_directory_dialog = false;
        }
    }

    if (show_hsm_editor) {
        ImGui::OpenPopup("HSM editor");
        if (h_editor.show("HSM editor")) {
            if (h_editor.state_ok())
                h_editor.save();

            h_editor.clear();
            h_editor.hide();
            show_hsm_editor = false;
        }
    }

    notifications.show();

#ifdef IRRITATOR_USE_TTF
    if (ttf)
        ImGui::PopFont();
#endif
}

void application::add_simulation_task(task_function fn,
                                      u64           param_1,
                                      u64           param_2,
                                      u64           param_3) noexcept
{
    irt_assert(fn);

    while (!sim_tasks.can_alloc())
        task_mgr.main_task_lists[ordinal(main_task::simulation)].wait();

    auto& task   = sim_tasks.alloc();
    task.app     = this;
    task.param_1 = param_1;
    task.param_2 = param_2;
    task.param_3 = param_3;

    task_mgr.main_task_lists[ordinal(main_task::simulation)].add(fn, &task);
    task_mgr.main_task_lists[ordinal(main_task::simulation)].submit();
}

void application::add_gui_task(task_function fn,
                               u64           param_1,
                               u64           param_2,
                               void*         param_3) noexcept
{
    irt_assert(fn);

    while (!gui_tasks.can_alloc())
        task_mgr.main_task_lists[ordinal(main_task::gui)].wait();

    auto& task   = gui_tasks.alloc();
    task.app     = this;
    task.param_1 = param_1;
    task.param_2 = param_2;
    task.param_3 = param_3;

    task_mgr.main_task_lists[ordinal(main_task::gui)].add(fn, &task);
    task_mgr.main_task_lists[ordinal(main_task::gui)].submit();
}

unordered_task_list& application::get_unordered_task_list(int idx) noexcept
{
    idx = idx < 0 ? 0
          : idx >= length(task_mgr.temp_task_lists)
            ? length(task_mgr.temp_task_lists) - 1
            : idx;

    return task_mgr.temp_task_lists[idx];
}

void task_simulation_back(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    if (g_task->app->s_editor.tl.can_back()) {
        auto ret = back(g_task->app->s_editor.tl,
                        g_task->app->s_editor.sim,
                        g_task->app->s_editor.simulation_current);

        if (is_bad(ret)) {
            auto& n = g_task->app->notifications.alloc(log_level::error);
            n.title = "Fail to back the simulation";
            format(n.message, "Advance message: {}", status_string(ret));
            g_task->app->notifications.enable(n);
        }
    }

    g_task->state = task_status::finished;
}

void task_simulation_advance(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    if (g_task->app->s_editor.tl.can_advance()) {
        auto ret = advance(g_task->app->s_editor.tl,
                           g_task->app->s_editor.sim,
                           g_task->app->s_editor.simulation_current);

        if (is_bad(ret)) {
            auto& n = g_task->app->notifications.alloc(log_level::error);
            n.title = "Fail to advance the simulation";
            format(n.message, "Advance message: {}", status_string(ret));
            g_task->app->notifications.enable(n);
        }
    }

    g_task->state = task_status::finished;
}

} // namespace irt
