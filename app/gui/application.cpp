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
    settings_wnd.update();
    settings_wnd.apply_default_style();

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
    if (auto ret = mod.registred_paths.init(max_component_dirs); is_bad(ret)) {
        log_w(
          *this, log_level::alert, "Fail to initialize registred dir paths");
    }

    if (auto ret = load_settings(); is_bad(ret))
        log_w(*this,
              log_level::alert,
              "Fail to read settings files. Default parameters used\n");

    if (auto ret = mod.init(mod_init); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize modeling components: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = pj.init(mod_init.component_capacity); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize project: {}\n",
              status_string(ret));
        return false;
    }

    if (mod.registred_paths.size() == 0) {
        if (auto path = get_system_component_dir(); path) {
            auto& new_dir    = mod.registred_paths.alloc();
            auto  new_dir_id = mod.registred_paths.get_id(new_dir);
            new_dir.name     = "System directory";
            new_dir.path     = path.value().string().c_str();
            log_w(*this,
                  log_level::info,
                  "Add system directory: {}\n",
                  new_dir.path.c_str());

            mod.component_repertories.emplace_back(new_dir_id);
        }

        if (auto path = get_default_user_component_dir(); path) {
            auto& new_dir    = mod.registred_paths.alloc();
            auto  new_dir_id = mod.registred_paths.get_id(new_dir);
            new_dir.name     = "User directory";
            new_dir.path     = path.value().string().c_str();
            log_w(*this,
                  log_level::info,
                  "Add user directory: {}\n",
                  new_dir.path.c_str());

            mod.component_repertories.emplace_back(new_dir_id);
        }
    }

    if (auto ret = save_settings(); is_bad(ret)) {
        log_w(*this, log_level::error, "Fail to save settings files.\n");
    }

    if (auto ret =
          sim.init(mod_init.model_capacity, mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize simulation components: {}\n",
              status_string(ret));
        return false;
    }

    simulation_ed.displacements.resize(mod_init.model_capacity);

    if (auto ret = simulation_ed.sim_obs.init(16); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = simulation_ed.copy_obs.init(16); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize copy simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = mod.srcs.init(50); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize external sources: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = mod.fill_internal_components(); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to fill internal component list: {}\n",
              status_string(ret));
    }

    if (auto ret = grids.init(32); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize grid component editors: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = generics.init(32); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize generic component editors: {}\n",
              status_string(ret));
        return false;
    }

    mod.fill_components();

    project_wnd.selected_component = undefined<tree_node_id>();
    component_sel.update();

    // @TODO at beggining, open a default generic component ?
    // auto id = component_ed.add_generic_component();
    // component_ed.open_as_main(id);

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
                app.menu_new_project_file = true;

            if (ImGui::MenuItem("Open project"))
                app.menu_load_project_file = true;

            if (ImGui::MenuItem("Save project"))
                app.menu_save_project_file = true;

            if (ImGui::MenuItem("Save project as..."))
                app.menu_save_as_project_file = true;

            if (ImGui::MenuItem("Quit"))
                app.menu_quit = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show modeling editor", nullptr, &app.component_ed.is_open);
            ImGui::MenuItem(
              "Show simulation editor", nullptr, &app.simulation_ed.is_open);
            ImGui::MenuItem(
              "Show output editor", nullptr, &app.output_ed.is_open);
            ImGui::MenuItem("Show data editor", nullptr, &app.data_ed.is_open);

            ImGui::MenuItem(
              "Show preview window", nullptr, &app.preview_wnd.is_open);
            ImGui::MenuItem(
              "Show component hierarchy", nullptr, &app.library_wnd.is_open);

            ImGui::MenuItem(
              "Show memory usage", nullptr, &app.memory_wnd.is_open);
            ImGui::MenuItem("Show task usage", nullptr, &app.task_wnd.is_open);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("ImGui demo window", nullptr, &app.show_imgui_demo);
            ImGui::MenuItem(
              "ImPlot demo window", nullptr, &app.show_implot_demo);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &app.settings_wnd.is_open);

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
    if (app.menu_new_project_file) {
        app.project_wnd.clear();
        // auto id = app.component_ed.add_generic_component();
        // app.project_wnd.open_as_main(id);
        app.menu_new_project_file = false;
    }

    if (app.menu_load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_load_file(title, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.mod.registred_paths.alloc();
                    auto  id   = app.mod.registred_paths.get_id(path);
                    path.path  = str;

                    app.add_simulation_task(task_load_project, ordinal(id));
                }
            }

            app.f_dialog.clear();
            app.menu_load_project_file = false;
        }
    }

    if (app.menu_save_project_file) {
        const bool have_file = !app.project_file.empty();

        if (have_file) {
            auto  u8str = app.project_file.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (app.mod.registred_paths.can_alloc(1)) {
                auto& path = app.mod.registred_paths.alloc();
                auto  id   = app.mod.registred_paths.get_id(path);
                path.path  = str;

                app.add_simulation_task(task_save_project, ordinal(id));
            }
        } else {
            app.menu_save_project_file    = false;
            app.menu_save_as_project_file = true;
        }
    }

    if (app.menu_save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_save_file(title, default_filename, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.mod.registred_paths.alloc();
                    auto  id   = app.mod.registred_paths.get_id(path);
                    path.path  = str;

                    app.add_simulation_task(task_save_project, ordinal(id));
                }
            }

            app.f_dialog.clear();
            app.menu_save_as_project_file = false;
        }
    }

    if (app.output_ed.write_output) {
        auto* obs = app.simulation_ed.sim_obs.try_to_get(
          app.simulation_ed.selected_sim_obs);

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

                app.simulation_ed.selected_sim_obs =
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
        // auto dock_id_left = ImGui::DockBuilderSplitNode(
        //   dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
        auto dock_id_down = ImGui::DockBuilderSplitNode(
          dockspace_id, ImGuiDir_Down, 0.2f, nullptr, &dockspace_id);

        // ImGui::DockBuilderDockWindow(project::name, dock_id_left);

        ImGui::DockBuilderDockWindow(component_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(simulation_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(output_editor::name, dockspace_id);
        ImGui::DockBuilderDockWindow(data_window::name, dockspace_id);

        ImGui::DockBuilderDockWindow(app.library_wnd.name, dock_id_right);
        ImGui::DockBuilderDockWindow(app.preview_wnd.name, dock_id_right);

        ImGui::DockBuilderDockWindow(window_logger::name, dock_id_down);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (app.component_ed.is_open)
        app.component_ed.show();

    if (app.simulation_ed.is_open)
        app.simulation_ed.show();

    if (app.output_ed.is_open)
        app.output_ed.show();

    if (app.data_ed.is_open)
        app.data_ed.show();

    if (app.log_wnd.is_open)
        app.log_wnd.show();

    if (app.library_wnd.is_open)
        app.library_wnd.show();

    if (app.preview_wnd.is_open)
        app.preview_wnd.show();
}

static void component_selector_make_selected_name(
  small_string<254>& name) noexcept
{
    name = "undefined";
}

static void component_selector_make_selected_name(
  std::string_view   reg,
  std::string_view   dir,
  std::string_view   file,
  component&         compo,
  component_id       id,
  small_string<254>& name) noexcept
{
    if (compo.name.empty()) {
        format(name, "{}/{}/{}/unamed {}", reg, dir, file, ordinal(id));
    } else {
        format(name, "{}/{}/{}/{}", reg, dir, file, compo.name.sv());
    }
}

static void component_selector_make_selected_name(
  std::string_view   component_name,
  small_string<254>& name) noexcept
{
    format(name, "internal {}", component_name);
}

static void component_selector_make_selected_name(
  std::string_view   component_name,
  component_id       id,
  small_string<254>& name) noexcept
{
    if (component_name.empty()) {
        format(name, "unamed {} (unsaved)", ordinal(id));
    } else {
        format(name, "{} (unsaved)", component_name);
    }
}

static void component_selector_select(modeling&          mod,
                                      component_id       id,
                                      small_string<254>& name) noexcept
{
    component_selector_make_selected_name(name);

    if (auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::internal) {
            component_selector_make_selected_name(
              internal_component_names[compo->id.internal_id], name);
        } else {
            if (auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
                reg) {
                if (auto* dir = mod.dir_paths.try_to_get(compo->dir); dir) {
                    if (auto* file = mod.file_paths.try_to_get(compo->file);
                        file) {
                        component_selector_make_selected_name(reg->name.sv(),
                                                              dir->path.sv(),
                                                              file->path.sv(),
                                                              *compo,
                                                              id,
                                                              name);
                    }
                }
            }

            component_selector_make_selected_name(compo->name.sv(), id, name);
        }
    }
}

void component_selector::update() noexcept
{
    auto* app = container_of(this, &application::component_sel);
    auto& mod = app->mod;

    ids.clear();
    names.clear();
    files     = 0;
    internals = 0;
    unsaved   = 0;

    ids.emplace_back(undefined<component_id>());
    component_selector_make_selected_name(names.emplace_back());

    for (auto id : mod.component_repertories) {
        if (auto* reg = mod.registred_paths.try_to_get(id); reg) {
            for (auto dir_id : reg->children) {
                if (auto* dir = mod.dir_paths.try_to_get(dir_id); dir) {
                    for (auto file_id : dir->children) {
                        if (auto* file = mod.file_paths.try_to_get(file_id);
                            file) {
                            if (auto* compo =
                                  mod.components.try_to_get(file->component);
                                compo) {
                                ids.emplace_back(file->component);
                                auto& str = names.emplace_back();

                                component_selector_make_selected_name(
                                  reg->name.sv(),
                                  dir->path.sv(),
                                  file->path.sv(),
                                  *compo,
                                  file->component,
                                  str);
                            }
                        }
                    }
                }
            }
        }
    }

    files = ids.size();

    {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            if (compo->type == component_type::internal) {
                auto id = mod.components.get_id(*compo);
                ids.emplace_back(id);
                auto& str = names.emplace_back();
                component_selector_make_selected_name(
                  internal_component_names[compo->id.internal_id], str);
            }
        }
    }

    internals = ids.size() - files;

    {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            const auto is_not_saved =
              compo->type != component_type::internal &&
              mod.file_paths.try_to_get(compo->file) == nullptr;

            if (is_not_saved) {
                auto id = mod.components.get_id(*compo);
                ids.emplace_back(id);
                auto& str = names.emplace_back();
                component_selector_make_selected_name(
                  compo->name.sv(), id, str);
            }
        }
    }

    unsaved = ids.size() - internals - files;
}

bool component_selector::combobox(const char*   label,
                                  component_id* new_selected) noexcept
{
    if (*new_selected != selected_id) {
        auto* app   = container_of(this, &application::component_sel);
        selected_id = *new_selected;

        component_selector_select(app->mod, selected_id, selected_name);
    }

    bool ret = false;

    if (ImGui::BeginCombo(label, selected_name.c_str())) {
        for (sz i = 0, e = ids.size(); i != e; ++i) {
            if (ImGui::Selectable(names[i].c_str(), ids[i] == *new_selected)) {
                *new_selected = ids[i];
                ret           = true;
            }
        }

        ImGui::EndCombo();
    }

    return ret;
}

bool component_selector::menu(const char*   label,
                              component_id* new_selected) noexcept
{
    if (*new_selected != selected_id) {
        auto* app   = container_of(this, &application::component_sel);
        selected_id = *new_selected;

        component_selector_select(app->mod, selected_id, selected_name);
    }

    bool ret = false;

    if (ImGui::BeginMenu(label)) {
        if (ImGui::BeginMenu("Files components")) {
            for (int i = 0, e = files; i < e; ++i) {
                if (ImGui::MenuItem(names[i].c_str())) {
                    ret           = true;
                    *new_selected = ids[i];
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Internal components")) {
            for (int i = files, e = files + internals; i < e; ++i) {
                if (ImGui::MenuItem(names[i].c_str())) {
                    ret           = true;
                    *new_selected = ids[i];
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Unsaved components")) {
            for (int i = files + internals, e = ids.size(); i < e; ++i) {
                if (ImGui::MenuItem(names[i].c_str())) {
                    ret           = true;
                    *new_selected = ids[i];
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    return ret;
}

void application::show() noexcept
{
#ifdef IRRITATOR_USE_TTF
    if (ttf)
        ImGui::PushFont(ttf);
#endif

    cleanup_sim_or_gui_task(sim_tasks);
    cleanup_sim_or_gui_task(gui_tasks);

    if (!mod.log_entries.empty()) {
        while (!mod.log_entries.empty()) {
            auto& w = mod.log_entries.front();
            mod.log_entries.pop_front();

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

    simulation_ed.simulation_update_state();

    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);

    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);

    application_show_windows(*this);

    if (memory_wnd.is_open)
        memory_wnd.show();

    if (task_wnd.is_open)
        task_wnd.show();

    if (settings_wnd.is_open)
        settings_wnd.show();

    if (show_select_directory_dialog) {
        const char* title = "Select directory";
        ImGui::OpenPopup(title);
        if (f_dialog.show_select_directory(title)) {
            if (f_dialog.state == file_dialog::status::ok) {
                select_directory = f_dialog.result;
                auto* dir_path =
                  mod.registred_paths.try_to_get(select_dir_path);
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
        if (hsm_ed.show("HSM editor")) {
            if (hsm_ed.state_ok())
                hsm_ed.save();

            hsm_ed.clear();
            hsm_ed.hide();
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

    if (g_task->app->simulation_ed.tl.can_back()) {
        auto ret = back(g_task->app->simulation_ed.tl,
                        g_task->app->sim,
                        g_task->app->simulation_ed.simulation_current);

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

    if (g_task->app->simulation_ed.tl.can_advance()) {
        auto ret = advance(g_task->app->simulation_ed.tl,
                           g_task->app->sim,
                           g_task->app->simulation_ed.simulation_current);

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
