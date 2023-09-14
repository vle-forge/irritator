// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irt {

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
    if (auto ret = mod.init(mod_init); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize modeling components: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = pj.init(mod_init.tree_capacity); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize project: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = load_settings(); is_bad(ret)) {
        log_w(*this,
              log_level::alert,
              "Fail to read settings files. Default parameters used\n");

        mod_init = modeling_initializer{};

        if (auto ret = save_settings(); is_bad(ret)) {
            log_w(*this,
                  log_level::alert,
                  "Fail to save settings files. Default parameters used\n");
        }
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
    simulation_ed.plot_obs.clear();
    simulation_ed.grid_obs.resize(pj.grid_observers.size());

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

    if (auto ret = graphs.init(32); is_bad(ret)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize graph component editors: {}\n",
              status_string(ret));
        return false;
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
        // auto* obs = app.simulation_ed.plot_obs.try_to_get(
        //   app.simulation_ed.selected_sim_obs);

        // if (obs) {
        //     if (auto* mdl = app.sim.models.try_to_get(obs->model); mdl) {
        //         if (auto* o = app.sim.observers.try_to_get(mdl->obs_id); o) {
        //             const char* title = "Select raw file path to save";
        //             const std::u8string_view default_filename =
        //               u8"filename.txt";
        //             const char8_t* filters[] = { u8".txt", nullptr };

        //             ImGui::OpenPopup(title);
        //             if (app.f_dialog.show_save_file(
        //                   title, default_filename, filters)) {
        //                 if (app.f_dialog.state == file_dialog::status::ok) {
        //                     obs->file = app.f_dialog.result;
        //                     obs->write(*o, obs->file);
        //                 }

        //                 app.simulation_ed.selected_sim_obs =
        //                   undefined<simulation_observation_id>();
        //                 app.output_ed.write_output = false;
        //                 app.f_dialog.clear();
        //             }
        //         }
        //     }
        // }
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

static void show_select_model_box_recursive(application&   app,
                                            tree_node&     tn,
                                            global_access& access) noexcept
{
    constexpr auto flags = ImGuiTreeNodeFlags_DefaultOpen;

    if_data_exists_do(app.mod.components, tn.id, [&](auto& compo) noexcept {
        small_string<64> str;

        switch (compo.type) {
        case component_type::simple:
            format(str, "{} generic", compo.name.sv());
            break;
        case component_type::grid:
            format(str, "{} grid", compo.name.sv());
            break;
        case component_type::graph:
            format(str, "{} graph", compo.name.sv());
            break;
        default:
            format(str, "{} unknown", compo.name.sv());
            break;
        }

        ImGui::PushID(&tn);
        if (ImGui::TreeNodeEx(str.c_str(), flags)) {
            for_each_model(
              app.sim,
              tn,
              [&](auto& sim, auto& tn, u64 /*unique_id*/, auto& mdl) noexcept {
                  const auto mdl_id = sim.models.get_id(mdl);
                  ImGui::PushID(get_index(mdl_id));

                  const auto current_tn_id = app.pj.node(tn);
                  str = dynamics_type_names[ordinal(mdl.type)];
                  if (ImGui::Selectable(str.c_str(),
                                        access.tn_id == current_tn_id &&
                                          access.mdl_id == mdl_id,
                                        ImGuiSelectableFlags_DontClosePopups)) {
                      access.tn_id  = current_tn_id;
                      access.mdl_id = mdl_id;
                  }

                  ImGui::PopID();
              });

            if (auto* child = tn.tree.get_child(); child)
                show_select_model_box_recursive(app, *child, access);

            ImGui::TreePop();
        }
        ImGui::PopID();
    });

    if (auto* sibling = tn.tree.get_sibling(); sibling)
        show_select_model_box_recursive(app, *sibling, access);
}

bool show_select_model_box(const char*    button_label,
                           const char*    popup_label,
                           application&   app,
                           tree_node&     tn,
                           global_access& access) noexcept
{
    static global_access copy;

    auto ret = false;

    if (ImGui::Button(button_label)) {
        copy = access;
        ImGui::OpenPopup(popup_label);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(
          popup_label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        show_select_model_box_recursive(app, tn, access);

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            ret = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            access = copy;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
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
