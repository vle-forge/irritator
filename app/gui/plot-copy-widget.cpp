// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include "implot.h"

namespace irt {

static void plot(const plot_copy& p) noexcept
{
    if (p.linear_outputs.empty())
        return;

    switch (p.plot_type) {
    case simulation_plot_type::plotlines: {
        ImPlotSpec spec;
        spec.LineWeight = 1.f;
        spec.MarkerSize = 6.f;
        spec.Stride     = sizeof(resampled_sample);

        ImPlot::PlotLine(p.name.c_str(), &p.linear_outputs[0].t,
                         &p.linear_outputs[0].value,
                         static_cast<int>(p.linear_outputs.size()), spec);

    } break;

    case simulation_plot_type::plotscatters: {
        ImPlotSpec spec;
        spec.LineWeight = 1.f;
        spec.MarkerSize = 6.f;
        spec.Stride     = sizeof(resampled_sample);

        ImPlot::PlotScatter(p.name.c_str(), &p.linear_outputs[0].t,
                            &p.linear_outputs[0].value,
                            static_cast<int>(p.linear_outputs.size()), spec);
    } break;

    default:
        break;
    }
}

void plot_copy_widget::show_plot_line(const plot_copy& p) noexcept { plot(p); }

} // namespace irt
