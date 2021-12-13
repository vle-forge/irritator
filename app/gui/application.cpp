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

    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
        std::fprintf(
          stderr, "Fail to initialize irritator: %s\n", status_string(ret));
        return false;
    }

    mod_init = { .model_capacity              = 256 * 64 * 16,
                 .tree_capacity               = 256 * 16,
                 .description_capacity        = 256 * 16,
                 .component_capacity          = 256 * 128,
                 .observer_capacity           = 256 * 16,
                 .file_path_capacity          = 256 * 256,
                 .children_capacity           = 256 * 64 * 16,
                 .connection_capacity         = 256 * 64,
                 .port_capacity               = 256 * 64,
                 .constant_source_capacity    = 16,
                 .binary_file_source_capacity = 16,
                 .text_file_source_capacity   = 16,
                 .random_source_capacity      = 16,
                 .random_generator_seed       = 123456789u };

    if (auto ret = c_editor.mod.dir_paths.init(64); is_bad(ret)) {
        log_w.log(2, "Fail to initialize dir paths");
    }

    if (auto path = get_system_component_dir(); path) {
        auto& new_dir = c_editor.mod.dir_paths.alloc();
        new_dir.name  = "System directory";
        new_dir.path  = path.value().string().c_str();
        log_w.log(7, "Add system directory: %s\n", new_dir.path.c_str());
    }

    if (auto path = get_default_user_component_dir(); path) {
        auto& new_dir = c_editor.mod.dir_paths.alloc();
        new_dir.name  = "User directory";
        new_dir.path  = path.value().string().c_str();
        log_w.log(7, "Add user directory: %s\n", new_dir.path.c_str());
    }

    if (auto ret = load_settings(); is_bad(ret)) {
        log_w.log(2, "Fail to read settings files. Default parameters used\n");
    }

    {
        // Initialize component_repertories used into coponents-window

        dir_path* dir = nullptr;
        while (c_editor.mod.dir_paths.next(dir)) {
            auto id = c_editor.mod.dir_paths.get_id(*dir);
            c_editor.mod.component_repertories.emplace_back(id);
        }
    }

    if (auto ret = c_editor.mod.init(mod_init); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize modeling components: %s\n",
                  status_string(ret));
        return false;
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

    return true;
}

bool application::show()
{
    bool ret = true;

    bool new_project_file     = false;
    bool load_project_file    = false;
    bool save_project_file    = false;
    bool save_as_project_file = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            bool is_modified = c_editor.mod.state == modeling_status::modified;
            bool have_file   = !c_editor.project_file.empty();

            if (!is_modified && ImGui::MenuItem("New project")) {
                new_project_file = true;
            }

            if (!is_modified && ImGui::MenuItem("Open project")) {
                load_project_file = true;
            }

            if (is_modified && have_file && ImGui::MenuItem("Save project")) {
                save_project_file = true;
            }

            if (ImGui::MenuItem("Save project as...")) {
                save_as_project_file = true;
            }

            if (!is_modified && ImGui::MenuItem("Quit")) {
                ret = false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Log", nullptr, &show_log);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);
            ImGui::Separator();
            ImGui::MenuItem("Component memory", nullptr, &c_editor.show_memory);
            ImGui::MenuItem(
              "Component settings", nullptr, &c_editor.show_settings);
            if (ImGui::MenuItem("Load settings")) {
                load_settings();
            }
            if (ImGui::MenuItem("Save settings")) {
                save_settings();
            }

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
        c_editor.mod.clear_project();
    }

    if (load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (load_file_dialog(c_editor.project_file, title, filters)) {
            auto* u8str = c_editor.project_file.u8string().c_str();
            auto* str   = reinterpret_cast<const char*>(u8str);
            auto  ret   = c_editor.mod.load_project(str);

            if (is_success(ret))
                log_w.log(5, "success\n");
            else
                log_w.log(4, "fail\n");
        }
    }

    if (save_project_file || save_as_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (save_file_dialog(c_editor.project_file, title, filters)) {
            auto* u8str = c_editor.project_file.u8string().c_str();
            auto* str   = reinterpret_cast<const char*>(u8str);
            auto  ret   = c_editor.mod.save_project(str);

            if (is_success(ret)) {
                log_w.log(5, "success\n");
            } else
                log_w.log(4, "fail\n");
        }
    }

    editor* ed = nullptr;
    while (editors.next(ed)) {
        if (ed->show) {
            if (!ed->show_window()) {
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

    if (show_log)
        log_w.show(&show_log);

    if (show_settings)
        show_settings_window();

    if (show_demo)
        ImGui::ShowDemoWindow();

    if (show_modeling)
        c_editor.show(&show_modeling);

    return ret;
}

editor* application::alloc_editor()
{
    if (!editors.can_alloc(1u)) {
        log_w.log(2, "Too many open editor\n");
        return nullptr;
    }

    auto& ed = editors.alloc();
    if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
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
            log_w.log(3,
                      "Simulation initialisation failure (%s)\n",
                      irt::status_string(ed.sim_st));
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
