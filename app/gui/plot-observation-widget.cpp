// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "irritator/macros.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>

namespace irt {

static void show_discrete_plot_line(const plot_type_options options,
                                    const char*             name,
                                    const observer&         obs) noexcept
{
    switch (options) {
    case plot_type_options::line:
        obs.read_history(
          [](const auto& lbuf, const auto /*version*/, const auto& name) {
              ImPlotSpec spec;
              spec.LineWeight = 1.f;
              spec.MarkerSize = 6.f;
              spec.Stride     = sizeof(resampled_sample);

              ImPlot::PlotStairs(name, &lbuf[0].t, &lbuf[0].value,
                                 static_cast<int>(lbuf.size()), spec);
          },
          name);
        break;

    case plot_type_options::dash:
        obs.read_history(
          [](const auto& lbuf, const auto /*version*/, const auto& name) {
              ImPlotSpec spec;
              spec.LineWeight = 1.f;
              spec.MarkerSize = 6.f;
              spec.Stride     = sizeof(resampled_sample);

              ImPlot::PlotBars(name, &lbuf[0].t, &lbuf[0].value,
                               static_cast<int>(lbuf.ssize()), 1.5, spec);
          },
          name);
        break;

    default:
        break;
    }
}

static void show_continuous_plot_line(const plot_type_options options,
                                      const char*             name,
                                      const observer&         obs) noexcept
{
    obs.read_history([&](const auto& lbuf, const auto /*version*/) noexcept {
        switch (options) {
        case plot_type_options::line: {
            ImPlotSpec spec;
            spec.LineWeight = 1.f;
            spec.MarkerSize = 6.f;
            spec.Stride     = sizeof(resampled_sample);

            ImPlot::PlotLine(name, &lbuf[0].t, &lbuf[0].value,
                             static_cast<int>(lbuf.size()), spec);

        } break;

        case plot_type_options::dash: {
            ImPlotSpec spec;
            spec.LineWeight = 1.f;
            spec.MarkerSize = 6.f;
            spec.Stride     = sizeof(resampled_sample);

            ImPlot::PlotScatter(name, &lbuf[0].t, &lbuf[0].value,
                                static_cast<int>(lbuf.size()), spec);

        } break;

        default:
            break;
        }
    });
}

void plot_observation_widget::show_plot_line(const observer&         obs,
                                             const plot_type_options options,
                                             const char* name) noexcept
{
    ImGui::PushID(&obs);

    if (options == plot_type_options::dash)
        show_discrete_plot_line(options, name, obs);
    else
        show_continuous_plot_line(options, name, obs);

    ImGui::PopID();
}

} // irt
