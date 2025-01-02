// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include "implot.h"

namespace irt {

static void plot(const plot_copy& p) noexcept
{
    if (p.linear_outputs.ssize() <= 0)
        return;

    switch (p.plot_type) {
    case simulation_plot_type::plotlines:
        ImPlot::PlotLineG(
          p.name.c_str(),
          ring_buffer_getter,
          const_cast<void*>(reinterpret_cast<const void*>(&p.linear_outputs)),
          p.linear_outputs.ssize());
        break;

    case simulation_plot_type::plotscatters:
        ImPlot::PlotScatterG(
          p.name.c_str(),
          ring_buffer_getter,
          const_cast<void*>(reinterpret_cast<const void*>(&p.linear_outputs)),
          p.linear_outputs.ssize());
        break;

    default:
        break;
    }
}

void plot_copy_widget::show(const char* name) noexcept
{
    auto& sim_ed = container_of(this, &project_window::plot_copy_wgt);

    ImGui::PushID(this);

    if (ImPlot::BeginPlot(name, ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        for_each_data(sim_ed.copy_obs,
                      [&](auto& plot_copy) noexcept { plot(plot_copy); });

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }

    ImGui::PopID();
}

void plot_copy_widget::show_plot_line(const plot_copy& p) noexcept { plot(p); }

} // namespace irt
