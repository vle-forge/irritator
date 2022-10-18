// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

static constexpr const task_manager_parameters tm_params = {
    .thread_number           = 1,
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
    for (int i = 0, e = task_mgr.workers.ssize(); i != e; ++i)
        task_mgr.workers[i].task_lists.emplace_back(&task_mgr.task_lists[0]);

    log_w.log(7, "GUI Irritator start\n");

    log_w.log(7, "Start with %d threads\n", tm_params.thread_number);

    log_w.log(7,
              "%d simple task list and %d multi task list\n",
              tm_params.simple_task_list_number,
              tm_params.multi_task_list_number);

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

static void modeling_log(int              level,
                         std::string_view message,
                         void*            user_data) noexcept
{
    if (auto* app = reinterpret_cast<application*>(user_data); app) {
        auto  new_level = std::clamp(level, 0, 5);
        auto  type      = enum_cast<notification_type>(level);
        auto& n         = app->notifications.alloc(type);

        app->log_w.log(
          new_level, "%.*s: ", to_int(message.size()), message.data());

        n.title   = "Modeling message";
        n.message = message;
        app->notifications.enable(n);
    }
}

bool application::init() noexcept
{
    c_editor.mod.register_log_callback(modeling_log,
                                       reinterpret_cast<void*>(this));

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

    s_editor.displacements.resize(mod_init.model_capacity);

    if (auto ret = s_editor.tree_nodes.init(1024); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize simuliation tree: %s\n", status_string(ret));
        return false;
    }

    if (auto ret = s_editor.sim_obs.init(16); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize simulation observation: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = s_editor.copy_obs.init(16); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize copy simulation observation: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.srcs.init(50); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize external sources: %s\n", status_string(ret));
        return false;
    } else {
        s_editor.sim.source_dispatch = c_editor.mod.srcs;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w.log(2, "Fail to fill component list: %s\n", status_string(ret));
    }

    c_editor.mod.fill_components();
    c_editor.mod.head           = undefined<tree_node_id>();
    c_editor.selected_component = undefined<tree_node_id>();

    auto id = c_editor.add_empty_component();
    c_editor.open_as_main(id);

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
                ImGui::MenuItem(
                  "Show data editor", nullptr, &app.show_data_editor);

                ImGui::MenuItem("Show observation window",
                                nullptr,
                                &app.show_observation_window);
                ImGui::MenuItem("Show component hierarchy",
                                nullptr,
                                &app.show_component_store_window);
            }

            ImGui::MenuItem("Show memory usage", nullptr, &app.show_memory);
            ImGui::MenuItem("Show task usage", nullptr, &app.show_task_window);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("ImGui demo window", nullptr, &app.show_imgui_demo);
            ImGui::MenuItem(
              "ImPlot demo window", nullptr, &app.show_implot_demo);
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
        app.c_editor.unselect();
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

    if (app.s_editor.output_ed.write_output) {
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
                app.s_editor.output_ed.write_output = false;
                app.f_dialog.clear();
            }
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
    }
    ImGui::End();

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
    if (ImGui::Begin("Log", 0, window_flags)) {
        app.show_log_window();
    }
    ImGui::End();

    if (app.is_fixed_main_window) {
        ImGui::SetNextWindowPos(components_pos, window_pos_flags);
        ImGui::SetNextWindowSize(components_size, window_size_flags);

        if (ImGui::Begin("Tools", 0, window_flags)) {
            if (ImGui::BeginTabBar("##Obs-Compo")) {
                if (ImGui::BeginTabItem("Component store")) {
                    app.show_components_window();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Observations")) {
                    app.show_simulation_observation_window();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::End();
        }
    } else {
        if (app.show_observation_window) {
            if (ImGui::Begin("Observations##Window",
                             &app.show_observation_window)) {
                app.show_simulation_observation_window();
            }
            ImGui::End();
        }

        if (app.show_component_store_window) {
            if (ImGui::Begin("Components store##Window",
                             &app.show_component_store_window)) {
                app.show_components_window();
            }
            ImGui::End();
        }
    }
}

static void show_task_box(application& app, bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Task usage", is_open)) {
        ImGui::End();
        return;
    }

    int workers = app.task_mgr.workers.ssize();
    ImGui::InputInt("workers", &workers, 1, 100, ImGuiInputTextFlags_ReadOnly);

    int lists = app.task_mgr.task_lists.ssize();
    ImGui::InputInt("lists", &lists, 1, 100, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::CollapsingHeader("Tasks list", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Tasks", 7)) {
            ImGui::TableSetupColumn("submit");
            ImGui::TableSetupColumn("tasks");
            ImGui::TableSetupColumn("tasks-start");
            ImGui::TableSetupColumn("tasks-end");
            ImGui::TableSetupColumn("tasks");
            ImGui::TableSetupColumn("priority");
            ImGui::TableSetupColumn("terminate");
            ImGui::TableHeadersRow();

            for (const auto& elem : app.task_mgr.task_lists) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.submit_task.ssize());
                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.tasks.ssize());

                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.tasks.head_index());
                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.tasks.tail_index());

                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.task_number.load());

                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.priority);

                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.is_terminating ? 1 : 0);
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Worker list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::BeginTable("Workers", 3)) {
            ImGui::TableSetupColumn("list number");
            ImGui::TableSetupColumn("is running");
            ImGui::TableSetupColumn("list(s)");
            ImGui::TableHeadersRow();

            for (const auto& elem : app.task_mgr.workers) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::Text("%d", elem.task_lists.ssize());
                ImGui::TableNextColumn();
                ImGui::Text("%d", elem.is_running ? 1 : 0);
                ImGui::TableNextColumn();

                small_string<task_manager_list_max + 1> buffer;

                buffer.resize(task_manager_list_max);
                for (int i = 0, e = buffer.ssize(); i != e; ++i)
                    buffer[i] = ' ';

                for (int i = 0, e = elem.task_lists.ssize(); i != e; ++i) {
                    auto diff =
                      &app.task_mgr.task_lists[0] - elem.task_lists[i];

                    irt_assert(diff >= 0 && diff < task_manager_list_max);
                    buffer[to_int(diff)] = '1';
                }

                ImGui::TextUnformatted(buffer.c_str());
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void application::show() noexcept
{
    state = gui_task_clean_up(*this);

    application_show_menu(*this);
    application_manage_menu_action(*this);

    s_editor.simulation_update_state();

    if (show_imgui_demo)
        ImGui::ShowDemoWindow();

    if (show_implot_demo)
        ImPlot::ShowDemoWindow();

    application_show_windows(*this);

    if (show_memory)
        show_memory_box(&show_memory);

    if (show_task_window)
        show_task_box(*this, &show_task_window);

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
            if (ImGui::BeginTabItem("modeling")) {
                show_modeling_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("simulation")) {
                show_simulation_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("output")) {
                show_output_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("input")) {
                show_external_sources();
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
        if (ImGui::Begin("modeling", &show_modeling_editor)) {
            show_modeling_editor_widget();
        }
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_simulation_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("simulation", &show_simulation_editor)) {
            show_simulation_editor_widget();
        }
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_output_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("outputs", &show_output_editor)) {
            show_output_editor_widget();
        }
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_data_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("inputs", &show_data_editor)) {
            show_external_sources();
        }
        ImGui::End();
    }
}

void task_simulation_back(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    if (g_task->app->s_editor.tl.can_back()) {
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

    if (g_task->app->s_editor.tl.can_advance()) {
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
