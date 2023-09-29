// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include "implot.h"

namespace irt {

void plot_copy_widget::show() noexcept
{
    ImGui::PushID(this);

    if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        if (linear_outputs.ssize() > 0) {
            switch (plot_type) {
            case simulation_plot_type::plotlines:
                ImPlot::PlotLineG(name.c_str(),
                                  ring_buffer_getter,
                                  &linear_outputs,
                                  linear_outputs.ssize());
                break;

            case simulation_plot_type::plotscatters:
                ImPlot::PlotScatterG(name.c_str(),
                                     ring_buffer_getter,
                                     &linear_outputs,
                                     linear_outputs.ssize());
                break;

            default:
                break;
            }
        }

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }

    ImGui::PopID();
}

} // namespace irt
