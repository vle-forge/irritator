// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

static constexpr const task_manager_parameters tm_params = {
    .thread_number           = 3,
    .simple_task_list_number = 1,
    .multi_task_list_number  = 0,
};

static constexpr const i32 gui_task_number = 64;

static ImVec4 operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

void settings_manager::update() noexcept
{
    gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.25f);
    gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.5f);

    gui_hovered_component_color =
      ImGui::ColorConvertFloat4ToU32(gui_component_color * 1.25f);
    gui_selected_component_color =
      ImGui::ColorConvertFloat4ToU32(gui_component_color * 1.5f);
}

application::application() noexcept
  : task_mgr(tm_params)
{
    log_w.log(7,
              "GUI Irritator start with %d threads shared between %d simple "
              "task list and %d multi task list\n",
              tm_params.thread_number,
              tm_params.simple_task_list_number,
              tm_params.multi_task_list_number);

    task_mgr.workers[0].task_lists.emplace_back(&task_mgr.task_lists[0]);
    task_mgr.workers[1].task_lists.emplace_back(&task_mgr.task_lists[0]);
    task_mgr.workers[2].task_lists.emplace_back(&task_mgr.task_lists[0]);

    log_w.log(
      7, "Task manager started - %d tasks availables\n", gui_task_number);

    task_mgr.start();
    gui_tasks.init(gui_task_number);

    settings.update();
}

application::~application() noexcept
{
    log_w.log(7, "Task manager shutdown\n");
    task_mgr.finalize();
    log_w.log(7, "Application shutdown\n");
}

bool application::init() noexcept
{
    c_editor.init();

    // @todo DEBUG MODE: Prefer user settings or better timeline constructor
    s_editor.tl.init(32768, 4096, 65536, 4096, 32768, 32768);

    if (auto ret = c_editor.mod.registred_paths.init(max_component_dirs);
        is_bad(ret)) {
        log_w.log(2, "Fail to initialize registred dir paths");
    }

    if (auto ret = load_settings(); is_bad(ret))
        log_w.log(2, "Fail to read settings files. Default parameters used\n");

    if (auto ret = c_editor.mod.init(mod_init); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize modeling components: %s\n",
                  status_string(ret));
        std::fprintf(stderr,
                     "Fail to initialize modeling components: %s\n",
                     status_string(ret));
        return false;
    }

    if (c_editor.mod.registred_paths.size() == 0) {
        if (auto path = get_system_component_dir(); path) {
            auto& new_dir    = c_editor.mod.registred_paths.alloc();
            auto  new_dir_id = c_editor.mod.registred_paths.get_id(new_dir);
            new_dir.name     = "System directory";
            new_dir.path     = path.value().string().c_str();
            log_w.log(7, "Add system directory: %s\n", new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }

        if (auto path = get_default_user_component_dir(); path) {
            auto& new_dir    = c_editor.mod.registred_paths.alloc();
            auto  new_dir_id = c_editor.mod.registred_paths.get_id(new_dir);
            new_dir.name     = "User directory";
            new_dir.path     = path.value().string().c_str();
            log_w.log(7, "Add user directory: %s\n", new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }
    }

    if (auto ret = save_settings(); is_bad(ret)) {
        log_w.log(2, "Fail to save settings files.\n");
    }

    if (auto ret = s_editor.sim.init(mod_init.model_capacity,
                                     mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize simulation components: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = s_editor.tree_nodes.init(1024); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize simuliation tree: %s\n", status_string(ret));
        return false;
    }

    if (auto ret = c_editor.outputs.init(32); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize memory output: %s\n", status_string(ret));
        return false;
    }

    if (auto ret = c_editor.srcs.init(50); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize external sources: %s\n", status_string(ret));
        return false;
    } else {
        s_editor.sim.source_dispatch = c_editor.srcs;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w.log(2, "Fail to fill component list: %s\n", status_string(ret));
    }

    c_editor.mod.fill_components();
    c_editor.mod.head           = undefined<tree_node_id>();
    c_editor.selected_component = undefined<tree_node_id>();

    return true;
}

static application_status gui_task_clean_up(application& app) noexcept
{
    application_status ret    = 0;
    gui_task*          task   = nullptr;
    gui_task*          to_del = nullptr;

    while (app.gui_tasks.next(task)) {
        if (to_del) {
            app.gui_tasks.free(*to_del);
            to_del = nullptr;
        }

        if (task->state == gui_task_status::finished) {
            to_del = task;
        } else {
            ret |= task->editor_state;
        }
    }

    if (to_del) {
        app.gui_tasks.free(*to_del);
    }

    return ret;
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
              "Fix window layout", nullptr, &app.is_fixed_window_placement);

            ImGui::MenuItem("Merge main editors",
                            nullptr,
                            &app.is_fixed_main_window,
                            !app.is_fixed_window_placement);

            if (!app.is_fixed_main_window) {
                ImGui::MenuItem(
                  "Show modeling editor", nullptr, &app.show_modeling_editor);
                ImGui::MenuItem("Show simulation editor",
                                nullptr,
                                &app.show_simulation_editor);
                ImGui::MenuItem(
                  "Show output editor", nullptr, &app.show_output_editor);
            }

            ImGui::MenuItem("Show memory usage", nullptr, &app.show_memory);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("Demo window", nullptr, &app.show_demo);
            ImGui::Separator();
            ImGui::MenuItem("Component settings", nullptr, &app.show_settings);

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
        app.c_editor.new_project();
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

                    auto& task   = app.gui_tasks.alloc();
                    task.app     = &app;
                    task.param_1 = ordinal(id);
                    app.task_mgr.task_lists[0].add(load_project, &task);
                    app.task_mgr.task_lists[0].submit();
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

                auto& task   = app.gui_tasks.alloc();
                task.app     = &app;
                task.param_1 = ordinal(id);
                app.task_mgr.task_lists[0].add(save_project, &task);
                app.task_mgr.task_lists[0].submit();
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

                    auto& task   = app.gui_tasks.alloc();
                    task.app     = &app;
                    task.param_1 = ordinal(id);
                    app.task_mgr.task_lists[0].add(save_project, &task);
                    app.task_mgr.task_lists[0].submit();
                }
            }

            app.f_dialog.clear();
            app.save_as_project_file = false;
        }
    }
}

static void application_show_windows(application& app) noexcept
{
    constexpr ImGuiWindowFlags window_fixed_flags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
    constexpr ImGuiCond        window_fixed_pos_flags  = ImGuiCond_None;
    constexpr ImGuiCond        window_fixed_size_flags = ImGuiCond_None;
    constexpr ImGuiWindowFlags window_floating_flags   = ImGuiWindowFlags_None;
    constexpr ImGuiCond window_floating_pos_flags      = ImGuiCond_FirstUseEver;
    constexpr ImGuiCond window_floating_size_flags     = ImGuiCond_Once;

    ImGuiWindowFlags window_flags      = window_floating_flags;
    ImGuiCond        window_pos_flags  = window_floating_pos_flags;
    ImGuiCond        window_size_flags = window_floating_size_flags;

    if (app.is_fixed_window_placement) {
        window_flags      = window_fixed_flags;
        window_pos_flags  = window_fixed_pos_flags;
        window_size_flags = window_fixed_size_flags;
    }

    const auto* viewport   = ImGui::GetMainViewport();
    const auto  region     = viewport->WorkSize;
    const float width_1_10 = region.x / 10.f;

    ImVec2 project_size(width_1_10 * 2.f, region.y);

    ImVec2 modeling_size(width_1_10 * 6.f, region.y - (region.y / 5.f));
    ImVec2 simulation_size(width_1_10 * 6.f, region.y / 5.f);

    ImVec2 components_size(width_1_10 * 2.f, region.y);

    ImVec2 project_pos(0.f, viewport->WorkPos.y);

    ImVec2 modeling_pos(project_size.x, viewport->WorkPos.y);
    ImVec2 simulation_pos(project_size.x,
                          viewport->WorkPos.y + modeling_size.y);

    ImVec2 components_pos(project_size.x + modeling_size.x,
                          viewport->WorkPos.y);

    ImGui::SetNextWindowPos(project_pos, window_pos_flags);
    ImGui::SetNextWindowSize(project_size, window_size_flags);
    if (ImGui::Begin("Project", 0, window_flags)) {
        app.show_project_window();
        ImGui::End();
    }

    if (app.is_fixed_main_window) {
        app.show_main_as_tabbar(modeling_pos,
                                modeling_size,
                                window_flags,
                                window_pos_flags,
                                window_size_flags);
    } else {
        app.show_main_as_window(modeling_pos, modeling_size);
    }

    ImGui::SetNextWindowPos(simulation_pos, window_pos_flags);
    ImGui::SetNextWindowSize(simulation_size, window_size_flags);
    if (ImGui::Begin("Simulation window", 0, window_flags)) {
        app.show_simulation_window();
        ImGui::End();
    }

    ImGui::SetNextWindowPos(components_pos, window_pos_flags);
    ImGui::SetNextWindowSize(components_size, window_size_flags);
    if (ImGui::Begin("Components list##Window", 0, window_flags)) {
        app.show_components_window();
        ImGui::End();
    }
}

void application::show() noexcept
{
    state = gui_task_clean_up(*this);

    application_show_menu(*this);
    application_manage_menu_action(*this);

    s_editor.simulation_update_state();

    if (show_demo)
        ImGui::ShowDemoWindow();

    application_show_windows(*this);

    if (show_memory)
        show_memory_box(&show_memory);

    if (show_settings)
        settings.show(&show_settings);

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

    notifications.show();
}

// editor* application::alloc_editor()
//{
//     if (!editors.can_alloc(1u)) {
//         auto& notif = notifications.alloc(notification_type::error);
//         notif.title = "Too many open editor";
//         notifications.enable(notif);
//         return nullptr;
//     }
//
//     auto& ed = editors.alloc();
//     if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret))
//     {
//         auto& notif = notifications.alloc(notification_type::error);
//         notif.title = "Editor allocation error";
//         format(notif.message, "Error: {}", status_string(ret));
//         notifications.enable(notif);
//         editors.free(ed);
//         return nullptr;
//     }
//
//     log_w.log(5, "Open editor %s\n", ed.name.c_str());
//     return &ed;
// }

// void application::free_editor(editor& ed)
//{
//     log_w.log(5, "Close editor %s\n", ed.name.c_str());
//     editors.free(ed);
// }

// void application::show_plot_window()
//{
//     ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
//     ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
//     if (!ImGui::Begin("Plot", &show_plot)) {
//         ImGui::End();
//         return;
//     }
//
//     static editor_id current = undefined<editor_id>();
//     if (auto* ed = make_combo_editor_name(current); ed) {
//         if (ImPlot::BeginPlot("simulation", "t", "s")) {
//             ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
//
//             plot_output* out = nullptr;
//             while (ed->plot_outs.next(out)) {
//                 if (!out->xs.empty() && !out->ys.empty())
//                     ImPlot::PlotLine(out->name.c_str(),
//                                      out->xs.data(),
//                                      out->ys.data(),
//                                      static_cast<int>(out->xs.size()));
//             }
//
//             ImPlot::PopStyleVar(1);
//             ImPlot::EndPlot();
//         }
//     }
//
//     ImGui::End();
// }

void application::show_main_as_tabbar(ImVec2           position,
                                      ImVec2           size,
                                      ImGuiWindowFlags window_flags,
                                      ImGuiCond        position_flags,
                                      ImGuiCond        size_flags) noexcept
{

    ImGui::SetNextWindowPos(position, position_flags);
    ImGui::SetNextWindowSize(size, size_flags);
    if (ImGui::Begin("Main", 0, window_flags)) {
        auto* tree =
          c_editor.mod.tree_nodes.try_to_get(c_editor.selected_component);
        if (!tree) {
            ImGui::End();
            return;
        }

        component* compo = c_editor.mod.components.try_to_get(tree->id);
        if (!compo) {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("##ModelingTabBar")) {
            if (ImGui::BeginTabItem("Modeling editor")) {
                show_modeling_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Simulation editor")) {
                show_simulation_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Output editor")) {
                show_output_editor_widget();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void application::show_main_as_window(ImVec2 position, ImVec2 size) noexcept
{
    size.x -= 50;
    size.y -= 50;

    if (show_modeling_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Modeling editor", &show_modeling_editor))
            show_modeling_editor_widget();
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_simulation_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Simulation editor", &show_simulation_editor))
            show_simulation_editor_widget();
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_output_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Output editor", &show_output_editor))
            show_output_editor_widget();
        ImGui::End();
    }
}

void application::shutdown() noexcept
{
    c_editor.shutdown();
    s_editor.shutdown();
    log_w.clear();
}

// static void run_for(editor& ed, long long int duration_in_microseconds)
// noexcept
//{
//     namespace stdc = std::chrono;
//
//     auto          start_at             = stdc::high_resolution_clock::now();
//     long long int duration_since_start = 0;
//
//     if (ed.simulation_bag_id < 0) {
//         ed.sim.clean();
//         ed.simulation_current     = ed.simulation_begin;
//         ed.simulation_during_date = ed.simulation_begin;
//         ed.models_make_transition.resize(ed.sim.models.capacity(), false);
//
//         if (ed.sim_st = ed.sim.initialize(ed.simulation_current);
//             irt::is_bad(ed.sim_st)) {
//             return;
//         }
//
//         ed.simulation_next_time = ed.sim.sched.empty()
//                                     ? time_domain<time>::infinity
//                                     : ed.sim.sched.tn();
//         ed.simulation_bag_id    = 0;
//     }
//
//     if (ed.st == editor_status::running) {
//         do {
//             ++ed.simulation_bag_id;
//             if (ed.sim_st = ed.sim.run(ed.simulation_current);
//                 is_bad(ed.sim_st)) {
//                 ed.st = editor_status::editing;
//                 ed.sim.finalize(ed.simulation_end);
//                 return;
//             }
//
//             if (ed.simulation_current >= ed.simulation_end) {
//                 ed.st = editor_status::editing;
//                 ed.sim.finalize(ed.simulation_end);
//                 return;
//             }
//
//             auto end_at   = stdc::high_resolution_clock::now();
//             auto duration = end_at - start_at;
//             auto duration_cast =
//               stdc::duration_cast<stdc::microseconds>(duration);
//             duration_since_start = duration_cast.count();
//         } while (duration_since_start < duration_in_microseconds);
//         return;
//     }
//
//     const int loop = ed.st == editor_status::running_1_step    ? 1
//                      : ed.st == editor_status::running_10_step ? 10
//                                                                : 100;
//
//     for (int i = 0; i < loop; ++i) {
//         ++ed.simulation_bag_id;
//         if (ed.sim_st = ed.sim.run(ed.simulation_current); is_bad(ed.sim_st))
//         {
//             ed.st = editor_status::editing;
//             ed.sim.finalize(ed.simulation_end);
//             return;
//         }
//
//         if (ed.simulation_current >= ed.simulation_end) {
//             ed.st = editor_status::editing;
//             ed.sim.finalize(ed.simulation_end);
//             return;
//         }
//     }
//
//     ed.st = editor_status::running_pause;
// }

// void application::run_simulations()
//{
//     auto running_simulation = 0.f;
//
//     editor* ed = nullptr;
//     while (editors.next(ed))
//         if (ed->is_running())
//             ++running_simulation;
//
//     if (!running_simulation)
//         return;
//
//     const auto duration = 10000.f / running_simulation;
//
//     ed = nullptr;
//     while (editors.next(ed))
//         if (ed->is_running())
//             run_for(
//               *ed,
//               static_cast<long long int>(duration *
//               ed->synchronize_timestep));
// }

// editor* application::make_combo_editor_name(editor_id& current) noexcept
//{
//     editor* first = editors.try_to_get(current);
//     if (first == nullptr) {
//         if (!editors.next(first)) {
//             current = undefined<editor_id>();
//             return nullptr;
//         }
//     }
//
//     current = editors.get_id(first);
//
//     if (ImGui::BeginCombo("Name", first->name.c_str())) {
//         editor* ed = nullptr;
//         while (editors.next(ed)) {
//             const bool is_selected = current == editors.get_id(ed);
//
//             if (ImGui::Selectable(ed->name.c_str(), is_selected))
//                 current = editors.get_id(ed);
//
//             if (is_selected)
//                 ImGui::SetItemDefaultFocus();
//         }
//
//         ImGui::EndCombo();
//     }
//
//     return editors.try_to_get(current);
// }

//
// Thread tasks
//

void task_simulation_back(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    if (g_task->app->s_editor.tl.current_bag > 0) {
        auto ret = back(g_task->app->s_editor.tl,
                        g_task->app->s_editor.sim,
                        g_task->app->s_editor.simulation_current);

        if (is_bad(ret)) {
            auto& n =
              g_task->app->notifications.alloc(notification_type::error);
            n.title = "Fail to back the simulation";
            format(n.message, "Advance message: {}", status_string(ret));
            g_task->app->notifications.enable(n);
        }
    }

    g_task->state = gui_task_status::finished;
}

void task_simulation_advance(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    if (g_task->app->s_editor.tl.current_bag < g_task->app->s_editor.tl.bag) {
        auto ret = advance(g_task->app->s_editor.tl,
                           g_task->app->s_editor.sim,
                           g_task->app->s_editor.simulation_current);

        if (is_bad(ret)) {
            auto& n =
              g_task->app->notifications.alloc(notification_type::error);
            n.title = "Fail to advance the simulation";
            format(n.message, "Advance message: {}", status_string(ret));
            g_task->app->notifications.enable(n);
        }
    }

    g_task->state = gui_task_status::finished;
}

} // namespace irt
