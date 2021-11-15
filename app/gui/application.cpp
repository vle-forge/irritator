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

    modeling_initializer m_init = { .model_capacity           = 256 * 64 * 16,
                                    .tree_capacity            = 256 * 16,
                                    .description_capacity     = 256 * 16,
                                    .component_capacity       = 256 * 128,
                                    .observer_capacity        = 256 * 16,
                                    .dir_path_capacity        = 256 * 16,
                                    .file_path_capacity       = 256 * 256,
                                    .children_capacity        = 256 * 64 * 16,
                                    .connection_capacity      = 256 * 64,
                                    .port_capacity            = 256 * 64,
                                    .constant_source_capacity = 16,
                                    .binary_file_source_capacity = 16,
                                    .text_file_source_capacity   = 16,
                                    .random_source_capacity      = 16,
                                    .random_generator_seed       = 123456789u };

    if (auto ret = c_editor.mod.init(m_init); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize modeling components: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w.log(2, "Fail to fill component list: %s\n", status_string(ret));
    }

    if (auto path = get_system_component_dir(); path) {
        auto& new_dir    = c_editor.mod.dir_paths.alloc();
        auto  new_dir_id = c_editor.mod.dir_paths.get_id(new_dir);
        new_dir.path     = path.value().string().c_str();
        c_editor.mod.component_repertories.emplace_back(new_dir_id);

        log_w.log(7, "Add component directory: %s\n", new_dir.path.c_str());
    }

    if (auto path = get_default_user_component_dir(); path) {
        auto& new_dir    = c_editor.mod.dir_paths.alloc();
        auto  new_dir_id = c_editor.mod.dir_paths.get_id(new_dir);
        new_dir.path     = path.value().string().c_str();
        c_editor.mod.component_repertories.emplace_back(new_dir_id);

        log_w.log(7, "Add component directory: %s\n", new_dir.path.c_str());
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

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                if (auto* ed = alloc_editor(); ed)
                    ed->context = ImNodes::EditorContextCreate();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
                ret = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            editor* ed = nullptr;
            while (editors.next(ed))
                ImGui::MenuItem(ed->name.c_str(), nullptr, &ed->show);

            ImGui::MenuItem("Simulation", nullptr, &show_simulation);
            ImGui::MenuItem("Plot", nullptr, &show_plot);
            ImGui::MenuItem("Plot Styles", nullptr, &show_alt_plot_styles);
            ImGui::MenuItem("Settings", nullptr, &show_settings);
            ImGui::MenuItem("Log", nullptr, &show_log);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);
            ImGui::Separator();
            ImGui::MenuItem("Component memory", nullptr, &c_editor.show_memory);
            ImGui::MenuItem(
              "Component settings", nullptr, &c_editor.show_settings);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
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

    if (show_alt_plot_styles)
        show_alt_plot_window();

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
        //plot_output* out = nullptr;// placed here in outer scope for all plot styles
        if (ImPlot::BeginPlot("simulation", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 0.5f);
            plot_output* out = nullptr; // moved this to outer scope
            while (ed->plot_outs.next(out)) {
                if (!out->xs.empty() && !out->ys.empty())
                    ImPlot::PlotLine(out->name.c_str(),
                                     out->xs.data(),
                                     out->ys.data(),
                                     static_cast<int>(out->xs.size()));
            }

            ImPlot::PopStyleVar(1);
            ImPlot::EndPlot();
        }// ends default line plot
    }// ------------------------end

// all other plot styles were her before

    ImGui::End();
}

void application::show_alt_plot_window() {
    ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Plot Styles", &show_alt_plot_styles)) {
        ImGui::End();
        return;
    }

    
    if (ImGui::BeginMenu("Plot styles")) {
        ImGui::MenuItem("Scatter plot", nullptr, &show_scatter_plot);
        ImGui::MenuItem("Shaded plot", nullptr, &show_shaded_plot);
        ImGui::MenuItem("Bar chart", nullptr, &show_bar_chart);
        ImGui::MenuItem("Pie chart", nullptr, &show_pie_chart);
        ImGui::MenuItem("Heat map", nullptr, &show_heat_map);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View data")) {
        // ImGui::MenuItem("Data src", nullptr, &show_load_data_file_field);
        ImGui::MenuItem("Data src1", nullptr, &show_load_data_file_field1);
        ImGui::MenuItem("Data src2", nullptr, &show_load_data_file_field2);
        ImGui::MenuItem("Data src3", nullptr, &show_load_data_file_field3);
        ImGui::MenuItem("Data src4", nullptr, &show_load_data_file_field4);
        ImGui::MenuItem("Data src5", nullptr, &show_load_data_file_field5);
        ImGui::MenuItem("Data src6", nullptr, &show_load_data_file_field6);
        ImGui::MenuItem("Data src7", nullptr, &show_load_data_file_field7);

        ImGui::EndMenu();
    }

    // if (ImGui::BeginMenu("Plot Style settings")) {
    //    ImGui::MenuItem("Scatter plot params", nullptr,
    //    &set_scatter_plot_params); ImGui::MenuItem("Shaded plot params",
    //    nullptr, &set_shaded_plot_params); ImGui::MenuItem("Bar chart params",
    //    nullptr, &set_bar_chart_params); ImGui::MenuItem("Pie chart params",
    //    nullptr, &set_pie_chart_params); ImGui::MenuItem("Heat map params",
    //    nullptr, &set_heat_map_params);

    //    ImGui::EndMenu();
    //}

    static editor_id current = undefined<editor_id>();// this line works for all plot styles


        ImGui::Text("Data source directory:");
    // std::filesystem::path dp1; // these are defined in the applicattion.hpp
    // file std::filesystem::path dp2;
    if (show_load_data_file_field1) {
        const char*    title     = "Data src1";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src1"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp1, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp1.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src1",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp1.u8string().c_str())),
                         dp1.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field2) {
        const char*    title     = "Data src2";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src2"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp2, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp2.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src2",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp2.u8string().c_str())),
                         dp2.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field3) {
        const char*    title     = "Data src3";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src3"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp3, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp3.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src3",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp3.u8string().c_str())),
                         dp3.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field4) {
        const char*    title     = "Data src4";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src4"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp4, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp4.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src4",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp4.u8string().c_str())),
                         dp4.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field5) {
        const char*    title     = "Data src5";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src5"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp5, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp5.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src5",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp5.u8string().c_str())),
                         dp5.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field6) {
        const char*    title     = "Data src6";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src6"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp6, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp6.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src6",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp6.u8string().c_str())),
                         dp6.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    if (show_load_data_file_field7) {
        const char*    title     = "Data src7";
        const char8_t* filters[] = { u8".dat", nullptr };
        if (ImGui::Button("data src7"))
            ImGui::OpenPopup(title);
        if (load_file_dialog(dp7, title, filters)) {
            // what do i do here
            // show_load_data_file_field1 = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)dp7.u8string().c_str());
        }
        // place this here for directory field display
        ImGui::SameLine();
        ImGui::InputText("Data src7",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           dp7.u8string().c_str())),
                         dp7.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
    }

    //can i check or open the .dat file contents here to have it available to all plot styles
    is_dp1.open(dp1);
    float         data_in_dp1 = 0.0;
    vector<float> vec_dp1;
    while (is_dp1 >> data_in_dp1) {
        vec_dp1.emplace_back(data_in_dp1);
    }
    size_t sz_didp1 = vec_dp1.size();

    is_dp2.open(dp2);
    float data_in_dp2 = 0.0;
    is_dp2 >> data_in_dp2;

    is_dp3.open(dp3);
    float data_in_dp3 = 0.0;
    is_dp3 >> data_in_dp3;

    is_dp4.open(dp4);
    float data_in_dp4 = 0.0;
    is_dp4 >> data_in_dp4;

    if (show_scatter_plot) {
        if (auto* ed = make_combo_editor_name(current); ed) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Scatter plot parameters");

            // I opened the data files here previously
            // but now moved to outer scope for all plot styles
            static int count = 0;
            ImGui::InputInt("Count:", &count);

            static int offset = 0;
            ImGui::InputInt("Offset:", &offset);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImPlot::BeginPlot("Scatter plot", "t", "s")) {
                // ImPlotStyleVar_Marker | ImPlotStyleVar_MarkerWeight |
                // ImPlotStyleVar_MarkerSize;
                ImPlot::PushStyleVar(ImPlotStyleVar_Marker,
                                     ImPlotMarker_Square);
                ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 6);
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                plot_output* out = nullptr; // moved this to outer scope
                while (ed->plot_outs.next(out)) {
                    // InputInt(const char*  label, int* v, int step = 1, int
                    // step_fast = 100,ImGuiInputTextFlags flags= 0);
                    if (!out->xs.empty() && !out->ys.empty())
                        ImPlot::PlotScatter(
                          out->name.c_str(),
                          out->xs.data(),
                          out->ys.data(),
                          count,  // int count taken from InputInt field
                          offset, // int offset taken from InputInt field
                          static_cast<int>(out->xs.size()));
                }

                ImPlot::PopStyleVar(3);
                ImPlot::EndPlot();
            }
        } // ------------------------end if auto* ed
    }     // -----------------ends scatter plot

    if (show_shaded_plot) {
        if (auto* ed = make_combo_editor_name(current); ed) {

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Shaded plot parameters");

            static int count = 0;
            ImGui::InputInt("Count:", &count);

            static float y_ref = 0.0;
            ImGui::InputFloat("Y_ref", &y_ref);

            static int offset = 0;
            ImGui::InputInt("Offset:", &offset);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImPlot::BeginPlot("shaded plot", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                plot_output* out = nullptr; // moved this to outer scope
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty())
                        ImPlot::PlotShaded(out->name.c_str(),
                                           out->xs.data(),
                                           out->ys.data(),
                                           count,  // int count
                                           y_ref,  // float y_ref
                                           offset, // int offset
                                           static_cast<int>(out->xs.size()));
                }

                ImPlot::PopStyleVar(2);
                ImPlot::EndPlot();
            }
        } // ------------------------end if auto* ed
    }     // -----------------ends shaded plot

    if (show_bar_chart) {
        if (auto* ed = make_combo_editor_name(current); ed) {

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Bar chart parameters");


            static int count = 0;
            ImGui::InputInt("Count:", &count);

            static float bar_width = 0.0;
            ImGui::InputFloat("Width:", &bar_width);

            static float bar_shift = 0.0;
            ImGui::InputFloat("Shift:", &bar_shift);

            static int offset = 0;
            ImGui::InputInt("Offset:", &offset);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImPlot::BeginPlot("simulation bar chart", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                plot_output* out = nullptr; // moved this to outer scope
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty())
                        ImPlot::PlotBars(out->name.c_str(),
                                         out->ys.data(),
                                         count,     // int count
                                         bar_width, // float width
                                         bar_shift, // float shift
                                         offset,    // int offset
                                         static_cast<int>(out->xs.size()));
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ------------------------end if auto* ed
    }     // ends bar chart plot

    if (show_pie_chart) {
        if (auto* ed = make_combo_editor_name(current); ed) {

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Pie chart parameters");


            static int count = 0;
            ImGui::InputInt("Count:", &count);

            static float pos_x = 0.0;
            ImGui::InputFloat("X_pos:", &pos_x);

            static float pos_y = 0.0;
            ImGui::InputFloat("Y_pos:", &pos_y);

            static float radius = 0.0;
            ImGui::InputFloat("Radius:", &radius);

            static bool isNormalized = false;
            ImGui::Checkbox("Normalize", &isNormalized);

            static float angle = 0.0;
            ImGui::InputFloat("Angle:", &angle);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImPlot::BeginPlot("simulation pie chart", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                plot_output*       out = nullptr; // moved this to outer scope
                static const char* label_ids[] = {
                    "s", "e", "i", "r", "d", "v"
                }; // otherwise: const char** label_ids
                   //*label_ids = out->name.c_str();
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty())
                        ImPlot::PlotPieChart(
                          label_ids,      // const char** label_ids,
                          out->ys.data(), // const float* values,
                          count,          // int count,
                          pos_x,          // float x,
                          pos_y,          // float y,
                          radius,         // float radius,
                          isNormalized,   // bool normalize = false,
                          "%.1f",         // const char* label_fmt = "%.1f",
                          angle);         // float angle0 = 90.0f
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ------------------------end if auto* ed
    }     // end pie chart

    if (show_heat_map) {
        if (auto* ed = make_combo_editor_name(current); ed) {

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Heat map parameters");


            static int rows = 0;
            ImGui::InputInt("Rows:", &rows);

            static int columns = 0;
            ImGui::InputInt("Columns:", &columns);

            static float scale_min = 0.0;
            ImGui::InputFloat("Minimum scale:", &scale_min);

            static float scale_max = 0.0;
            ImGui::InputFloat("Maximum scale:", &scale_max);

            ImGui::Spacing();
            ImGui::Separator();

            if (ImPlot::BeginPlot("simulation heat map", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 2.0f);
                plot_output*       out = nullptr; // moved this to outer scope
                const ImPlotPoint& ippmin = ImPlotPoint(-1.0, -1.0);
                const ImPlotPoint& ippmax = ImPlotPoint(0.0, 0.0);
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty())
                        ImPlot::PlotHeatmap(
                          out->name.c_str(),
                          out->ys.data(),
                          rows,      // int rows
                          columns,   // int cols,
                          scale_min, // float scale_min,
                          scale_max, // float scale_max,
                          "%.1f",    // const char* label_fmt = "%.1f"
                          ippmin,    // const ImPlotPoint& bounds_min =
                                  // ImPlotPoint(0, 0),
                          ippmax); // const ImPlotPoint& bounds_max =
                                   // ImPlotPoint(1, 1))
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ------------------------end if auto* ed
    }     // end heat map

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
