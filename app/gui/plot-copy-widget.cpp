// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include "implot.h"

namespace irt {

static void show_plot_line_widget(simulation_editor& /*sim_ed*/,
                                  plot_copy& p) noexcept
{
    if (p.linear_outputs.ssize() > 0) {
        switch (p.plot_type) {
        case simulation_plot_type::plotlines:
            ImPlot::PlotLineG(p.name.c_str(),
                              ring_buffer_getter,
                              &p.linear_outputs,
                              p.linear_outputs.ssize());
            break;

        case simulation_plot_type::plotscatters:
            ImPlot::PlotScatterG(p.name.c_str(),
                                 ring_buffer_getter,
                                 &p.linear_outputs,
                                 p.linear_outputs.ssize());
            break;

        default:
            break;
        }
    }
}

void plot_copy_widget::show(const char* name) noexcept
{
    auto& sim_ed = container_of(this, &simulation_editor::plot_copy_wgt);

    ImGui::PushID(this);

    if (ImPlot::BeginPlot(name, ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        for_each_data(sim_ed.copy_obs, [&](auto& plot_copy) noexcept {
            show_plot_line_widget(sim_ed, plot_copy);
        });

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }

    ImGui::PopID();
}

void plot_copy_widget::show_plot_line(const plot_copy_id id) noexcept
{
    auto& sim_ed = container_of(this, &simulation_editor::plot_copy_wgt);

    if_data_exists_do(sim_ed.copy_obs, id, [&](auto& plot_copy) noexcept {
        show_plot_line_widget(sim_ed, plot_copy);
    });
}

} // namespace irt
