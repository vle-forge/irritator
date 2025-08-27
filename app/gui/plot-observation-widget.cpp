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

            ImPlot::SetupAxis(ImAxis_X1,
                              "t",
                              ImPlotAxisFlags_AutoFit |
                                ImPlotAxisFlags_RangeFit);
            ImPlot::SetupAxis(ImAxis_Y1,
                              name.c_str(),
                              ImPlotAxisFlags_AutoFit |
                                ImPlotAxisFlags_RangeFit);

            ImPlot::SetupLegend(ImPlotLocation_North);
            ImPlot::SetupFinish();

            v_obs.for_each([&](const auto id) noexcept {
                const auto idx    = get_index(id);
                const auto obs_id = v_obs.get_obs_ids()[idx];
                auto*      obs    = pj.sim.observers.try_to_get(obs_id);

                ImPlot::SetupAxis(ImAxis_X1,
                                  "t",
                                  ImPlotAxisFlags_AutoFit |
                                    ImPlotAxisFlags_RangeFit);
                ImPlot::SetupAxis(ImAxis_Y1,
                                  v_obs.get_names()[ordinal(id)].c_str(),
                                  ImPlotAxisFlags_AutoFit |
                                    ImPlotAxisFlags_RangeFit);
                ImPlot::SetupLegend(ImPlotLocation_North);
                ImPlot::SetupFinish();

                obs->linearized_buffer.read_only([&](auto& lbuf) noexcept {
                    switch (v_obs.get_options()[idx]) {
                    case variable_observer::type_options::line:
                        ImPlot::PlotLineG(
                          name.c_str(),
                          ring_buffer_getter,
                          const_cast<void*>(
                            reinterpret_cast<const void*>(&lbuf)),
                          lbuf.ssize());
                        break;

                    case variable_observer::type_options::dash:
                        ImPlot::PlotScatterG(
                          name.c_str(),
                          ring_buffer_getter,
                          const_cast<void*>(
                            reinterpret_cast<const void*>(&lbuf)),
                          lbuf.ssize());
                        break;

                    default:
                        unreachable();
                    }
                });
            });

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }
}

static void show_discrete_plot_line(
  const variable_observer::type_options options,
  const name_str&                       name,
  const observer&                       obs) noexcept
{
    switch (options) {
    case variable_observer::type_options::line:
        obs.linearized_buffer.read_only(
          [](const auto& lbuf, const auto& name) {
              ImPlot::PlotStairsG(
                name.c_str(),
                ring_buffer_getter,
                const_cast<void*>(reinterpret_cast<const void*>(&lbuf)),
                lbuf.ssize());
          },
          name);
        break;

    case variable_observer::type_options::dash:
        obs.linearized_buffer.read_only(
          [](const auto& lbuf, const auto& name) {
              ImPlot::PlotBarsG(
                name.c_str(),
                ring_buffer_getter,
                const_cast<void*>(reinterpret_cast<const void*>(&lbuf)),
                lbuf.ssize(),
                1.5);
          },
          name);
        break;

    default:
        break;
    }
}

inline static auto local_ring_buffer_getter(int idx, void* data) noexcept
  -> ImPlotPoint
{
    const auto* lbuf   = reinterpret_cast<ring_buffer<observation>*>(data);
    const auto  index  = lbuf->index_from_begin(idx);
    const auto& values = (*lbuf)[index];

    return ImPlotPoint(values.x, values.y);
};

static void show_continuous_plot_line(
  const variable_observer::type_options options,
  const name_str&                       name,
  const observer&                       obs) noexcept
{
    obs.linearized_buffer.try_read_only([&](const auto& lbuf) noexcept {
        switch (options) {
        case variable_observer::type_options::line:
            ImPlot::PlotLineG(
              name.c_str(),
              local_ring_buffer_getter,
              const_cast<void*>(reinterpret_cast<const void*>(&lbuf)),
              lbuf.ssize());
            break;

        case variable_observer::type_options::dash:
            ImPlot::PlotScatterG(
              name.c_str(),
              local_ring_buffer_getter,
              const_cast<void*>(reinterpret_cast<const void*>(&lbuf)),
              lbuf.ssize());
            break;

        default:
            break;
        }
    });
}

void plot_observation_widget::show_plot_line(
  const observer&                       obs,
  const variable_observer::type_options options,
  const name_str&                       name) noexcept
{
    ImGui::PushID(&obs);

    if (obs.type == interpolate_type::none) {
        show_discrete_plot_line(options, name, obs);
    } else {
        show_continuous_plot_line(options, name, obs);
    }

    ImGui::PopID();
}

} // irt
