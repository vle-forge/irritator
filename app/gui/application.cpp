// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/external_source.hpp"

namespace irt {

static constexpr const i32 simulation_task_number = 64;

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
  : task_mgr{}
{
    sim_tasks.init(simulation_task_number);
    gui_tasks.init(simulation_task_number);

    log_w(*this, 7, "GUI Irritator start\n");

    log_w(*this,
          7,
          "Start with {} main threads and {} generic workers\n",
          task_mgr.main_workers.ssize(),
          task_mgr.temp_workers.ssize());

    log_w(*this, 7, "Initialization successfull");

    task_mgr.start();

    settings.update();
}

application::~application() noexcept
{
    log_w(*this, 7, "Task manager shutdown\n");
    task_mgr.finalize();
    log_w(*this, 7, "Application shutdown\n");
}

bool application::init() noexcept
{
    if (auto ret = c_editor.mod.registred_paths.init(max_component_dirs);
        is_bad(ret)) {
        log_w(*this, 2, "Fail to initialize registred dir paths");
    }

    if (auto ret = load_settings(); is_bad(ret))
        log_w(
          *this, 2, "Fail to read settings files. Default parameters used\n");

    if (auto ret = c_editor.mod.init(mod_init); is_bad(ret)) {
        log_w(*this,
              2,
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
            log_w(*this, 7, "Add system directory: {}\n", new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }

        if (auto path = get_default_user_component_dir(); path) {
            auto& new_dir    = c_editor.mod.registred_paths.alloc();
            auto  new_dir_id = c_editor.mod.registred_paths.get_id(new_dir);
            new_dir.name     = "User directory";
            new_dir.path     = path.value().string().c_str();
            log_w(*this, 7, "Add user directory: {}\n", new_dir.path.c_str());

            c_editor.mod.component_repertories.emplace_back(new_dir_id);
        }
    }

    if (auto ret = save_settings(); is_bad(ret)) {
        log_w(*this, 2, "Fail to save settings files.\n");
    }

    if (auto ret = s_editor.sim.init(mod_init.model_capacity,
                                     mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w(*this,
              2,
              "Fail to initialize simulation components: {}\n",
              status_string(ret));
        return false;
    }

    s_editor.displacements.resize(mod_init.model_capacity);

    if (auto ret = s_editor.sim_obs.init(16); is_bad(ret)) {
        log_w(*this,
              2,
              "Fail to initialize simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = s_editor.copy_obs.init(16); is_bad(ret)) {
        log_w(*this,
              2,
              "Fail to initialize copy simulation observation: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.srcs.init(50); is_bad(ret)) {
        log_w(*this,
              2,
              "Fail to initialize external sources: {}\n",
              status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w(
          *this, 2, "Fail to fill component list: {}\n", status_string(ret));
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
            ImGui::MenuItem("Show task usage", nullptr, &app.show_tasks_window);
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
                auto compo_flags = ImGuiTabItemFlags_None;
                auto obs_flags   = ImGuiTabItemFlags_SetSelected;

                if (app.show_modeling_editor) {
                    compo_flags = ImGuiTabItemFlags_SetSelected;
                    obs_flags   = ImGuiTabItemFlags_None;
                }

                if (app.show_modeling_editor &&
                    ImGui::BeginTabItem(
                      "component store", nullptr, compo_flags)) {
                    app.show_components_window();
                    ImGui::EndTabItem();
                }

                if (app.show_simulation_editor &&
                    ImGui::BeginTabItem("observations", nullptr, obs_flags)) {
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

void application::show_tasks_widgets() noexcept
{
    int workers = task_mgr.temp_workers.ssize();
    ImGui::InputInt("workers", &workers, 1, 100, ImGuiInputTextFlags_ReadOnly);

    int lists = task_mgr.temp_task_lists.ssize();
    ImGui::InputInt("lists", &lists, 1, 100, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::CollapsingHeader("Tasks list", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Main Tasks", 3)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("Submitted tasks");
            ImGui::TableSetupColumn("finished tasks");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("simulation");
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", task_mgr.main_task_lists_stats[0].num_submitted_tasks);
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", task_mgr.main_task_lists_stats[0].num_executed_tasks);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("gui");
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", task_mgr.main_task_lists_stats[1].num_submitted_tasks);
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", task_mgr.main_task_lists_stats[1].num_executed_tasks);

            for (int i = 0, e = task_mgr.temp_task_lists.ssize(); i != e; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("generic-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}", task_mgr.temp_task_lists_stats[i].num_submitted_tasks);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}", task_mgr.temp_task_lists_stats[i].num_executed_tasks);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Worker list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::BeginTable("Workers", 3)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("execution duration");
            ImGui::TableHeadersRow();

            int i = 0;
            for (int e = task_mgr.main_workers.ssize(); i != e; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("main-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{} ms", task_mgr.main_workers[i].exec_time.count() / 1000);
                ImGui::TableNextColumn();
            }

            int j = 0;
            for (int e = task_mgr.temp_workers.ssize(); j != e; ++j, ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("generic-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{} ms", task_mgr.temp_workers[j].exec_time.count() / 1000);
                ImGui::TableNextColumn();
            }

            ImGui::EndTable();
        }
    }
}

void application::show() noexcept
{
    cleanup_sim_or_gui_task(sim_tasks);
    cleanup_sim_or_gui_task(gui_tasks);

    if (!c_editor.mod.warnings.empty()) {
        while (!c_editor.mod.warnings.empty()) {
            auto& warning = c_editor.mod.warnings.front();
            c_editor.mod.warnings.pop_front();

            auto& n = notifications.alloc();
            if (warning.st != status::success) {
                n.type  = notification_type::information;
                n.title = warning.buffer.sv();
            } else {
                n.type = notification_type::warning;
                format(n.title, "{}", status_string(warning.st));
                n.message = warning.buffer.sv();
            }

            notifications.enable(n);
        }
    }

    application_show_menu(*this);
    application_manage_menu_action(*this);

    s_editor.simulation_update_state();

    if (show_imgui_demo)
        ImGui::ShowDemoWindow();

    if (show_implot_demo)
        ImPlot::ShowDemoWindow();

    application_show_windows(*this);

    if (show_hsm_editor) {
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Once);
        if (!ImGui::Begin("HSM editor", &show_hsm_editor)) {
            ImGui::End();
        } else {
            h_editor.show();
            ImGui::End();
        }
    }

    if (show_memory)
        show_memory_box(&show_memory);

    if (show_tasks_window) {
        ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
        if (!ImGui::Begin("Task usage", &show_tasks_window)) {
            ImGui::End();
        } else {
            show_tasks_widgets();
            ImGui::End();
        }
    }

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
                show_modeling_editor   = true;
                show_simulation_editor = false;
                show_modeling_editor_widget();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("simulation")) {
                show_modeling_editor   = false;
                show_simulation_editor = true;
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
            auto& n =
              g_task->app->notifications.alloc(notification_type::error);
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
            auto& n =
              g_task->app->notifications.alloc(notification_type::error);
            n.title = "Fail to advance the simulation";
            format(n.message, "Advance message: {}", status_string(ret));
            g_task->app->notifications.enable(n);
        }
    }

    g_task->state = task_status::finished;
}

} // namespace irt
