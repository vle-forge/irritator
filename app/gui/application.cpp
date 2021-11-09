// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

bool
application::init()
{
    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
        std::fprintf(
          stderr, "Fail to initialize irritator: %s\n", status_string(ret));
        return false;
    }

    if (auto* ed = alloc_editor(); !ed) {
        std::fprintf(stderr, "Fail to initialize editor\n");
        return false;
    }

    try {
        simulation_duration.resize(editors.capacity(), 0);
    } catch (const std::bad_alloc& /*e*/) {
        return false;
    }

    try {
        if (auto home = get_home_directory(); home) {
            home_dir = home.value();
            home_dir /= "irritator";
        } else {
            log_w.log(3,
                      "Fail to retrieve home directory. Use current directory "
                      "instead\n");
            home_dir = std::filesystem::current_path();
        }

        if (auto install = get_executable_directory(); install) {
            executable_dir = install.value();
        } else {
            log_w.log(
              3,
              "Fail to retrieve executable directory. Use current directory "
              "instead\n");
            executable_dir = std::filesystem::current_path();
        }

        log_w.log(5,
                  "home: %s\ninstall: %s\n",
                  home_dir.u8string().c_str(),
                  executable_dir.u8string().c_str());
        return true;
    } catch (const std::exception& /*e*/) {
        log_w.log(2, "Fail to initialize application\n");
        return false;
    }
}

bool
application::show()
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
            ImGui::MenuItem("Settings", nullptr, &show_settings);
            ImGui::MenuItem("Log", nullptr, &show_log);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);

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

    if (show_log)
        log_w.show(&show_log);

    if (show_settings)
        show_settings_window();

    if (show_demo)
        ImGui::ShowDemoWindow();

    return ret;
}

editor*
application::alloc_editor()
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

void
application::free_editor(editor& ed)
{
    log_w.log(5, "Close editor %s\n", ed.name.c_str());
    editors.free(ed);
}
/// <summary>
/// my plotter modificatiosn start here and only in this part of the ImGui or ImPlot mode
/// </summary>
void
application::show_plot_window()
{
    ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
    //ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
    //ImGui::SetNextWindowContentSize(ImVec2(400, 0.0f));
    ImPlotFlags flag_option = ImPlotFlags_Query;
    //static auto flag_opt = ImPlotFlags_Query;

    if (!ImGui::Begin("Plot", &show_plot,flag_option)) {
        ImGui::End();
        return;
    }

    //editor* ed = nullptr;

    if (ImGui::BeginMenu("Visualization")) {
        ImGui::MenuItem("Scatter plot", nullptr, &show_scatter_plot);
        ImGui::MenuItem("Shader plot", nullptr, &show_shaded_plot);
        ImGui::MenuItem("Bar chart", nullptr, &show_bar_chart);
        ImGui::MenuItem("Pie chart", nullptr, &show_pie_chart);
        ImGui::MenuItem("Heat Map", nullptr, &show_heat_map);

        ImGui::EndMenu();
    }

        
    // if (show_scatter_plot)
    //    show_scatter_plot_window();
    // if (show_shaded_plot)
    //    show_shaded_plot_window();
    // if (show_bar_chart)
    //    show_bar_chart_window();
    // if (show_pie_chart)
    //    show_pie_chart_window();
    // if (show_heat_map)
    //    show_heat_map_window(); 


    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {//////covers all the plot options in single editor...default editor-0
        if (ImPlot::BeginPlot("simulation default plot", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);

            plot_output* out = nullptr;
            while (ed->plot_outs.next(out)) {
                if (!out->xs.empty() && !out->ys.empty()) {
                    ImPlot::PlotLine(out->name.c_str(),
                                     out->xs.data(),
                                     out->ys.data(),
                                     static_cast<int>(out->xs.size()));
                }
            }

            ImPlot::PopStyleVar(1);
            ImPlot::EndPlot();
        }

        if (show_scatter_plot) {
            if (ImPlot::BeginPlot("simulation scatter plot", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);// whats the right style to use for scatter plot
                plot_output* out = nullptr;
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty()) {
                        ImPlot::PlotScatter(out->name.c_str(),
                                            out->xs.data(),
                                            out->ys.data(),
                                            1,// int count represents whats
                                            1,// int offset
                                            static_cast<int>(out->xs.size()));
                    }
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        }// ends scatter plot

        if (show_shaded_plot) {
            if (ImPlot::BeginPlot("simulation shaded plot", "t", "s")) {
                        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 5.f);
                        plot_output* out = nullptr;
                        //for (const int& i : out->ys) {
                        //    out->y0.push_back(i) = 0.0;
                        //}

                        while (ed->plot_outs.next(out)) {
                            if (!out->xs.empty() && !out->ys.empty() && !out->y0.empty()) {
                                ImPlot::PlotShaded(out->name.c_str(),
                                                   out->xs.data(),
                                                   out->ys.data(),// what value should be used here for minimum
                                                   4,
                                                   0.f,
                                                   1,
                                                   static_cast<int>(out->xs.size()));
                            }
                        }

                        ImPlot::PopStyleVar(1);
                        ImPlot::EndPlot();
                    }        
        }// ends shader plot

        if (show_bar_chart) {
            if (ImPlot::BeginPlot("simulation bar chart", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_ErrorBarWeight, 2);

                plot_output* out = nullptr;
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty()) {
                        ImPlot::PlotBars(out->name.c_str(),
                                        //out->xs.data(),
                                        out->ys.data(),
                                        4,// int count
                                        0.67f,// float width
                                        0.0,
                                        1,//int offset = 0
                                        static_cast<int>(out->xs.size()));// int stride
                    }
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ends bar chart

        if (show_pie_chart) {
            if (ImPlot::BeginPlot("simulation pie chart", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.f);

                plot_output* out = nullptr;
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty()) {
                        ImPlot::PlotLine(out->name.c_str(),
                                         out->xs.data(),
                                         out->ys.data(),
                                         static_cast<int>(out->xs.size()));
                    }
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ends pie chart

        if (show_heat_map) {
            if (ImPlot::BeginPlot("simulation heat map", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotColormap_Hot,6);
                plot_output* out = nullptr;
                while (ed->plot_outs.next(out)) {
                    if (!out->xs.empty() && !out->ys.empty()) {
                        ImPlot::PlotHeatmap(out->name.c_str(),
                                            out->ys.data(),
                                          50,// int rows
                                          50,// int columns
                                          0.1f,// float scale min
                                          1.0f,// float scale max
                                          "%.1f",// const char* label_fmt
                                          ImPlotPoint(0, 0),//const ImPlotPoint& bounds_min = 
                                          ImPlotPoint(100, 100));//const ImPlotPoint& bounds_max = 
                    }
                }

                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        } // ends heat map

    }

            
    //if (show_scatter_plot)
    //    show_scatter_plot_window();
    //if (show_shaded_plot)
    //    show_shaded_plot_window();
    //if (show_bar_chart)
    //    show_bar_chart_window();
    //if (show_pie_chart)
    //    show_pie_chart_window();
    //if (show_heat_map)
    //    show_heat_map_window(); 

    ImGui::End();
} // Ese the plotter ends here and all my modificatiuons will come here


//void
//application::show_scatter_plot_window()
//{
//    static editor_id current = undefined<editor_id>();
//    if (auto* ed = make_combo_editor_name(current); ed) {
//        if (ImPlot::BeginPlot("simulation scatter plot", "t", "s")) {
//            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.f);
//
//            plot_output* out = nullptr;
//            while (ed->plot_outs.next(out)) {
//                if (!out->xs.empty() && !out->ys.empty()) {
//                    ImPlot::PlotLine(out->name.c_str(),
//                                     out->xs.data(),
//                                     out->ys.data(),
//                                     static_cast<int>(out->xs.size()));
//                }
//            }
//
//            ImPlot::PopStyleVar(1);
//            ImPlot::EndPlot();
//        }
//    }
//}
//
//void
//application::show_shaded_plot_window()
//{
//    static editor_id current = undefined<editor_id>();
//    if (auto* ed = make_combo_editor_name(current); ed) {
//        if (ImPlot::BeginPlot("simulation vertical bar-chart", "t", "s")) {
//            //ImPlot::PushStyleVar(ImPlotStyleVar_ErrorBarWeight, 3.f);
//
//            plot_output* out = nullptr;
//            while (ed->plot_outs.next(out)) {
//                if (!out->xs.empty() && !out->ys.empty()) {
//                    double yval = 0;
//                    double* ptryval = &yval;
//                    ImPlot::PlotShaded(out->name.c_str(),
//                                       out->xs.data(),
//                                       ptryval,
//                                       out->ys.data(),
//                                       0,
//                                       0,
//                                       static_cast<int>(out->xs.size()));
//                }
//            }
//
//            ImPlot::PopStyleVar(1);
//            ImPlot::EndPlot();
//        }
//    }
//
//}
//
//void
//application::show_bar_chart_window()
//{
//    static editor_id current4 = undefined<editor_id>();
//    if (auto* ed = make_combo_editor_name(current4); ed) {
//        if (ImPlot::BeginPlot("simulation horizontal bar-chart", "t", "s")) {
//           // ImPlot::PushStyleVar(ImPlotStyleVar_ErrorBarWeight, 3.f);
//
//            plot_output* out = nullptr;
//            while (ed->plot_outs.next(out)) {
//                if (!out->xs.empty() && !out->ys.empty()) {
//                    ImPlot::PlotBars(out->name.c_str(),
//                                      out->ys.data(),
//                                      out->xs.size(),
//                                      0.67f,
//                                      0,
//                                      0,
//                                      static_cast<int>(out->xs.size()));
//            }
//
//            ImPlot::PopStyleVar(1);
//            ImPlot::EndPlot();
//        }
//    }
//
//}
//
//void
//application::show_pie_chart_window()
//{
//    static editor_id current = undefined<editor_id>();
//    if (auto* ed = make_combo_editor_name(current); ed) {
//        if (ImPlot::BeginPlot("simulation Pie plot", "t", "s")) {
//            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 5.f);
//
//            plot_output* out = nullptr;
//            while (ed->plot_outs.next(out)) {
//                if (!out->xs.empty() && !out->ys.empty()) {
//                    ImPlot::PlotLine(out->name.c_str(),
//                                     out->xs.data(),
//                                     out->ys.data(),
//                                     static_cast<int>(out->xs.size()));
//                }
//            }
//
//            ImPlot::PopStyleVar(1);
//            ImPlot::EndPlot();
//        }
//    }
//}
//
//void
//application::show_heat_map_window()
//{
//    static editor_id current = undefined<editor_id>();
//    if (auto* ed = make_combo_editor_name(current); ed) {
//        if (ImPlot::BeginPlot("simulation heat map", "t", "s")) {
//            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
//
//            plot_output* out = nullptr;
//            while (ed->plot_outs.next(out)) {
//                if (!out->xs.empty() && !out->ys.empty()) {
//                    ImPlot::PlotLine(out->name.c_str(),
//                                     out->xs.data(),
//                                     out->ys.data(),
//                                     static_cast<int>(out->xs.size()));
//                }
//            }
//
//            ImPlot::PopStyleVar(1);
//            ImPlot::EndPlot();
//        }
//    }
//
//}

void
application::show_settings_window()
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Settings", &show_settings)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Home.......: %s", home_dir.u8string().c_str());
    ImGui::Text("Executable.: %s", executable_dir.u8string().c_str());

    ImGui::End();
}

void
application::shutdown()
{
    editors.clear();
    log_w.clear();
}

static void
run_for(editor& ed, long long int duration_in_microseconds) noexcept
{
    namespace stdc = std::chrono;

    auto start_at = stdc::high_resolution_clock::now();
    long long int duration_since_start;

    if (ed.simulation_bag_id < 0) {
        ed.sim.clean();
        ed.simulation_current = ed.simulation_begin;
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
        ed.simulation_bag_id = 0;
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

            auto end_at = stdc::high_resolution_clock::now();
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

void
application::run_simulations()
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

editor*
application::make_combo_editor_name(editor_id& current) noexcept
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
