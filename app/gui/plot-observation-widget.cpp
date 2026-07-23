// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "irritator/macros.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>

namespace irt {

void plot_observation_widget::show(project& pj) noexcept
{
    for (auto& v_obs : pj.variable_observers) {
        const auto id  = pj.variable_observers.get_id(v_obs);
        const auto idx = get_index(id);

        small_string<32> name;
        format(name, "{}##{}", v_obs.name.sv(), idx);

        if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupLegend(ImPlotLocation_NorthWest);
            ImPlot::SetupAxisLimits(
              ImAxis_X1, pj.sim.limits.begin(), pj.sim.limits.end());
            ImPlot::SetupFinish();

            for (const auto id : v_obs.subs) {
                const auto opt = v_obs.subs.template get<plot_type_options>(id);
                const auto obs_id = v_obs.subs.template get<observer_id>(id);
                auto*      obs    = pj.sim.observers.try_to_get(obs_id);

                if (not obs)
                    continue;

                obs->read_history(
                  [&](const auto& h, const auto /*version*/) noexcept {
                      switch (opt) {
                      case plot_type_options::line:
                          ImPlot::PlotLine(name.c_str(),
                                           &h[0].t,
                                           &h[0].value,
                                           static_cast<int>(h.size()),
                                           0,
                                           0,
                                           sizeof(resampled_sample));
                          break;

                      case plot_type_options::dash:
                          ImPlot::PlotScatter(name.c_str(),
                                              &h[0].t,
                                              &h[0].value,
                                              static_cast<int>(h.size()),
                                              0,
                                              0,
                                              sizeof(resampled_sample));
                          break;

                      default:
                          unreachable();
                      }
                  });
            }

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }
}

static void show_discrete_plot_line(const plot_type_options options,
                                    const char*             name,
                                    const observer&         obs) noexcept
{
    switch (options) {
    case plot_type_options::line:
        obs.read_history(
          [](const auto& lbuf, const auto /*version*/, const auto& name) {
              ImPlot::PlotStairs(name,
                                 &lbuf[0].t,
                                 &lbuf[0].value,
                                 static_cast<int>(lbuf.size()),
                                 0,
                                 0,
                                 sizeof(resampled_sample));
          },
          name);
        break;

    case plot_type_options::dash:
        obs.read_history(
          [](const auto& lbuf, const auto /*version*/, const auto& name) {
              ImPlot::PlotBars(name,
                               &lbuf[0].t,
                               &lbuf[0].value,
                               static_cast<int>(lbuf.ssize()),
                               1.5,
                               0,
                               0,
                               sizeof(resampled_sample));
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
        case plot_type_options::line:
            ImPlot::PlotLine(name,
                             &lbuf[0].t,
                             &lbuf[0].value,
                             static_cast<int>(lbuf.size()),
                             0,
                             0,
                             sizeof(resampled_sample));
            break;

        case plot_type_options::dash:
            ImPlot::PlotScatter(name,
                                &lbuf[0].t,
                                &lbuf[0].value,
                                static_cast<int>(lbuf.size()),
                                0,
                                0,
                                sizeof(resampled_sample));
            break;

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
