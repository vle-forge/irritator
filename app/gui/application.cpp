// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

bool application::init()
{
    c_editor.init();

    c_editor.mod.report =
      [](int level, const char* title, const char* message) noexcept -> void {
        const auto clamp_level = level < 0 ? 0 : level < 4 ? level : 4;

        fmt::print(stdout, "level: {} ", clamp_level);
        if (title)
            std::fputs(title, stdout);

        if (message)
            std::fputs(message, stdout);
    };

    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
        std::fprintf(
          stderr, "Fail to initialize irritator: %s\n", status_string(ret));
        return false;
    }

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

    if (auto ret = c_editor.sim.init(mod_init.model_capacity,
                                     mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize simulation components: %s\n",
                  status_string(ret));
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
        c_editor.sim.source_dispatch = c_editor.srcs;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w.log(2, "Fail to fill component list: %s\n", status_string(ret));
    }

    c_editor.mod.fill_components();
    c_editor.mod.head           = undefined<tree_node_id>();
    c_editor.selected_component = undefined<tree_node_id>();

    try {
        simulation_duration.resize(editors.capacity(), 0);
    } catch (const std::bad_alloc& /*e*/) {
        return false;
    }

    c_editor.mod.report = [this](int         level,
                                 const char* title,
                                 const char* message) noexcept -> void {
        const auto clamp_level = level < 0 ? 0 : level < 4 ? level : 4;
        const auto type        = enum_cast<notification_type>(clamp_level);

        auto& notif = notifications.alloc();
        notif.type  = type;

        if (title)
            notif.title.assign(title);

        if (message)
            notif.message.assign(message);

        notifications.enable(notif);
    };

    return true;
}

bool application::show()
{
    static bool new_project_file     = false;
    static bool load_project_file    = false;
    static bool save_project_file    = false;
    static bool save_as_project_file = false;
    bool        ret                  = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("New project"))
                new_project_file = true;

            if (ImGui::MenuItem("Open project"))
                load_project_file = true;

            if (ImGui::MenuItem("Save project"))
                save_project_file = true;

            if (ImGui::MenuItem("Save project as..."))
                save_as_project_file = true;

            if (ImGui::MenuItem("Quit"))
                ret = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show component memory window", nullptr, &c_editor.show_memory);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);
            ImGui::Separator();
            ImGui::MenuItem(
              "Component settings", nullptr, &c_editor.show_settings);

            if (ImGui::MenuItem("Load settings"))
                load_settings();

            if (ImGui::MenuItem("Save settings"))
                save_settings();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Old menu")) {
            if (ImGui::MenuItem("New")) {
                if (auto* ed = alloc_editor(); ed)
                    ed->context = ImNodes::EditorContextCreate();
            }

            editor* ed = nullptr;
            while (editors.next(ed))
                ImGui::MenuItem(ed->name.c_str(), nullptr, &ed->show);

            ImGui::MenuItem("Simulation", nullptr, &show_simulation);
            ImGui::MenuItem("Plot", nullptr, &show_plot);
            ImGui::MenuItem("Settings", nullptr, &show_settings);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (new_project_file) {
        c_editor.new_project();
        new_project_file = false;
    }

    if (load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (f_dialog.show_load_file(title, filters)) {
            if (f_dialog.state == file_dialog::status::ok) {
                c_editor.project_file = f_dialog.result;
                auto  u8str           = c_editor.project_file.u8string();
                auto* str = reinterpret_cast<const char*>(u8str.c_str());

                if (c_editor.mod.registred_paths.can_alloc(1)) {
                    auto& path = c_editor.mod.registred_paths.alloc();
                    auto  id   = c_editor.mod.registred_paths.get_id(path);
                    path.path  = str;

                    auto& task   = c_editor.gui_tasks.alloc();
                    task.ed      = &c_editor;
                    task.param_1 = ordinal(id);
                    c_editor.task_mgr.task_lists[0].add(load_project, &task);
                    c_editor.task_mgr.task_lists[0].submit();
                }
            }

            f_dialog.clear();
            load_project_file = false;
        }
    }

    if (save_project_file) {
        const bool have_file = !c_editor.project_file.empty();

        if (have_file) {
            auto  u8str = c_editor.project_file.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (c_editor.mod.registred_paths.can_alloc(1)) {
                auto& path = c_editor.mod.registred_paths.alloc();
                auto  id   = c_editor.mod.registred_paths.get_id(path);
                path.path  = str;

                auto& task   = c_editor.gui_tasks.alloc();
                task.ed      = &c_editor;
                task.param_1 = ordinal(id);
                c_editor.task_mgr.task_lists[0].add(save_project, &task);
                c_editor.task_mgr.task_lists[0].submit();
            }
        } else {
            save_project_file    = false;
            save_as_project_file = true;
        }
    }

    if (save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (f_dialog.show_save_file(title, default_filename, filters)) {
            if (f_dialog.state == file_dialog::status::ok) {
                c_editor.project_file = f_dialog.result;
                auto  u8str           = c_editor.project_file.u8string();
                auto* str = reinterpret_cast<const char*>(u8str.c_str());

                if (c_editor.mod.registred_paths.can_alloc(1)) {
                    auto& path = c_editor.mod.registred_paths.alloc();
                    auto  id   = c_editor.mod.registred_paths.get_id(path);
                    path.path  = str;

                    auto& task   = c_editor.gui_tasks.alloc();
                    task.ed      = &c_editor;
                    task.param_1 = ordinal(id);
                    c_editor.task_mgr.task_lists[0].add(save_project, &task);
                    c_editor.task_mgr.task_lists[0].submit();
                }
            }

            f_dialog.clear();
            save_as_project_file = false;
        }
    }

    editor* ed = nullptr;
    while (editors.next(ed)) {
        if (ed->show) {
            if (!ed->show_window(log_w)) {
                editor* next = ed;
                editors.next(next);
                free_editor(*ed);
            } else {
                if (ed->show_settings)
                    ed->settings.show(&ed->show_settings);
            }
        }
    }

    if (show_simulation)
        show_simulation_window();

    if (show_plot)
        show_plot_window();

    if (show_settings)
        show_settings_window();

    if (show_demo)
        ImGui::ShowDemoWindow();

    if (show_modeling)
        c_editor.show(&show_modeling);

    notifications.show();

    return ret;
}

editor* application::alloc_editor()
{
    if (!editors.can_alloc(1u)) {
        auto& notif = notifications.alloc(notification_type::error);
        notif.title = "Too many open editor";
        notifications.enable(notif);
        return nullptr;
    }

    auto& ed = editors.alloc();
    if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret)) {
        auto& notif = notifications.alloc(notification_type::error);
        notif.title = "Editor allocation error";
        format(notif.message, "Error: {}", status_string(ret));
        notifications.enable(notif);
        editors.free(ed);
        return nullptr;
    }

    log_w.log(5, "Open editor %s\n", ed.name.c_str());
    return &ed;
}

void application::free_editor(editor& ed)
{
    log_w.log(5, "Close editor %s\n", ed.name.c_str());
    editors.free(ed);
}

void application::show_plot_window()
{
    ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Plot", &show_plot)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        if (ImPlot::BeginPlot("simulation", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);

            plot_output* out = nullptr;
            while (ed->plot_outs.next(out)) {
                if (!out->xs.empty() && !out->ys.empty())
                    ImPlot::PlotLine(out->name.c_str(),
                                     out->xs.data(),
                                     out->ys.data(),
                                     static_cast<int>(out->xs.size()));
            }

            ImPlot::PopStyleVar(1);
            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

void application::show_settings_window()
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Settings", &show_settings)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Home.......: %s",
                reinterpret_cast<const char*>(home_dir.u8string().c_str()));
    ImGui::Text(
      "Executable.: %s",
      reinterpret_cast<const char*>(executable_dir.u8string().c_str()));

    ImGui::End();
}

void application::shutdown()
{
    c_editor.shutdown();
    editors.clear();
    log_w.clear();
}

static void run_for(editor& ed, long long int duration_in_microseconds) noexcept
{
    namespace stdc = std::chrono;

    auto          start_at             = stdc::high_resolution_clock::now();
    long long int duration_since_start = 0;

    if (ed.simulation_bag_id < 0) {
        ed.sim.clean();
        ed.simulation_current     = ed.simulation_begin;
        ed.simulation_during_date = ed.simulation_begin;
        ed.models_make_transition.resize(ed.sim.models.capacity(), false);

        if (ed.sim_st = ed.sim.initialize(ed.simulation_current);
            irt::is_bad(ed.sim_st)) {
            return;
        }

        ed.simulation_next_time = ed.sim.sched.empty()
                                    ? time_domain<time>::infinity
                                    : ed.sim.sched.tn();
        ed.simulation_bag_id    = 0;
    }

    if (ed.st == editor_status::running) {
        do {
            ++ed.simulation_bag_id;
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                ed.sim.finalize(ed.simulation_end);
                return;
            }

            if (ed.simulation_current >= ed.simulation_end) {
                ed.st = editor_status::editing;
                ed.sim.finalize(ed.simulation_end);
                return;
            }

            auto end_at   = stdc::high_resolution_clock::now();
            auto duration = end_at - start_at;
            auto duration_cast =
              stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();
        } while (duration_since_start < duration_in_microseconds);
        return;
    }

    const int loop = ed.st == editor_status::running_1_step    ? 1
                     : ed.st == editor_status::running_10_step ? 10
                                                               : 100;

    for (int i = 0; i < loop; ++i) {
        ++ed.simulation_bag_id;
        if (ed.sim_st = ed.sim.run(ed.simulation_current); is_bad(ed.sim_st)) {
            ed.st = editor_status::editing;
            ed.sim.finalize(ed.simulation_end);
            return;
        }

        if (ed.simulation_current >= ed.simulation_end) {
            ed.st = editor_status::editing;
            ed.sim.finalize(ed.simulation_end);
            return;
        }
    }

    ed.st = editor_status::running_pause;
}

void application::run_simulations()
{
    auto running_simulation = 0.f;

    editor* ed = nullptr;
    while (editors.next(ed))
        if (ed->is_running())
            ++running_simulation;

    if (!running_simulation)
        return;

    const auto duration = 10000.f / running_simulation;

    ed = nullptr;
    while (editors.next(ed))
        if (ed->is_running())
            run_for(
              *ed,
              static_cast<long long int>(duration * ed->synchronize_timestep));
}

editor* application::make_combo_editor_name(editor_id& current) noexcept
{
    editor* first = editors.try_to_get(current);
    if (first == nullptr) {
        if (!editors.next(first)) {
            current = undefined<editor_id>();
            return nullptr;
        }
    }

    current = editors.get_id(first);

    if (ImGui::BeginCombo("Name", first->name.c_str())) {
        editor* ed = nullptr;
        while (editors.next(ed)) {
            const bool is_selected = current == editors.get_id(ed);

            if (ImGui::Selectable(ed->name.c_str(), is_selected))
                current = editors.get_id(ed);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return editors.try_to_get(current);
}

} // namespace irt
