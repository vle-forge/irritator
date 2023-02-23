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
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding  = 20.0f;
    ImGui::GetStyle().GrabMinSize   = 10.0f;

    ImVec4* colors                         = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

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

    settings.update();
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
              "Show modeling editor", nullptr, &app.show_modeling);
            ImGui::MenuItem(
              "Show simulation editor", nullptr, &app.show_simulation);
            ImGui::MenuItem("Show output editor", nullptr, &app.show_output);
            ImGui::MenuItem("Show data editor", nullptr, &app.show_data);

            ImGui::MenuItem(
              "Show observation window", nullptr, &app.show_observation);
            ImGui::MenuItem(
              "Show component hierarchy", nullptr, &app.show_components);

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
    constexpr ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID        dockspace_id =
      ImGui::DockSpaceOverViewport(viewport, dockspace_flags);

    constexpr ImGuiWindowFlags window_flags = 0;

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

        ImGui::DockBuilderDockWindow("Project", dock_id_left);

        ImGui::DockBuilderDockWindow("Modeling", dockspace_id);
        ImGui::DockBuilderDockWindow("Simulation", dockspace_id);
        ImGui::DockBuilderDockWindow("Outputs", dockspace_id);
        ImGui::DockBuilderDockWindow("Inputs", dockspace_id);

        ImGui::DockBuilderDockWindow("Libraries", dock_id_right);
        ImGui::DockBuilderDockWindow("Observations", dock_id_right);

        ImGui::DockBuilderDockWindow("Log", dock_id_down);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (app.show_project) {
        if (ImGui::Begin("Project", &app.show_project, window_flags))
            app.show_project_window();
        ImGui::End();
    }

    if (app.show_modeling) {
        if (ImGui::Begin("Modeling", &app.show_modeling, window_flags))
            app.show_modeling_editor_widget();
        ImGui::End();
    }

    if (app.show_simulation) {
        if (ImGui::Begin("Simulation", &app.show_simulation, window_flags))
            app.show_simulation_editor_widget();
        ImGui::End();
    }

    if (app.show_output) {
        if (ImGui::Begin("Outputs", &app.show_output, window_flags))
            app.show_output_editor_widget();
        ImGui::End();
    }

    if (app.show_data) {
        if (ImGui::Begin("Inputs", &app.show_data, window_flags))
            app.show_external_sources();
        ImGui::End();
    }

    if (app.show_log) {
        if (ImGui::Begin("Log", &app.show_log, window_flags))
            app.show_log_window();
        ImGui::End();
    }

    if (app.show_components) {
        if (ImGui::Begin("Libraries", &app.show_components, window_flags))
            app.show_components_window();
        ImGui::End();
    }

    if (app.show_observation) {
        if (ImGui::Begin("Observations", &app.show_observation, window_flags))
            app.show_simulation_observation_window();
        ImGui::End();
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
        ImGui::ShowDemoWindow();

    if (show_implot_demo)
        ImPlot::ShowDemoWindow();

    application_show_windows(*this);

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
