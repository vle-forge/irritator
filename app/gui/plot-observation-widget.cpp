// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "irritator/macros.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>

namespace irt {

void plot_observation_widget::show(application& app) noexcept
{
    for (auto& v_obs : app.pj.variable_observers) {
        const auto id  = app.pj.variable_observers.get_id(v_obs);
        const auto idx = get_index(id);

        small_string<32> name;
        format(name, "{}##{}", v_obs.name.sv(), idx);

        if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupAxes(nullptr,
                              nullptr,
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);

            v_obs.for_each([&](const auto id) noexcept {
                const auto idx    = get_index(id);
                const auto obs_id = v_obs.get_obs_ids()[idx];
                auto*      obs    = app.pj.sim.observers.try_to_get(obs_id);

                if (obs->linearized_buffer.size() > 0) {
                    switch (v_obs.get_options()[idx]) {
                    case variable_observer::type_options::line:
                        ImPlot::PlotLineG(name.c_str(),
                                          ring_buffer_getter,
                                          &obs->linearized_buffer,
                                          obs->linearized_buffer.ssize());
                        break;

                    case variable_observer::type_options::dash:
                        ImPlot::PlotScatterG(name.c_str(),
                                             ring_buffer_getter,
                                             &obs->linearized_buffer,
                                             obs->linearized_buffer.ssize());
                        break;

                    default:
                        unreachable();
                    }
                } else if (obs->buffer.size() > 0) {
                    switch (v_obs.get_options()[idx]) {
                    case variable_observer::type_options::line:
                        ImPlot::PlotLineG(name.c_str(),
                                          ring_buffer_getter,
                                          &obs->buffer,
                                          obs->buffer.ssize());
                        break;

                    case variable_observer::type_options::dash:
                        ImPlot::PlotScatterG(name.c_str(),
                                             ring_buffer_getter,
                                             &obs->buffer,
                                             obs->buffer.ssize());
                        break;

                    default:
                        unreachable();
                    }
                }
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
        ImPlot::PlotStairsG(name.c_str(),
                            ring_buffer_getter,
                            const_cast<void*>(reinterpret_cast<const void*>(
                              &obs.linearized_buffer)),
                            obs.linearized_buffer.ssize());
        break;

    case variable_observer::type_options::dash:
        ImPlot::PlotBarsG(name.c_str(),
                          ring_buffer_getter,
                          const_cast<void*>(reinterpret_cast<const void*>(
                            &obs.linearized_buffer)),
                          obs.linearized_buffer.ssize(),
                          1.5);
        break;

    default:
        break;
    }
}

static void show_continuous_plot_line(
  const variable_observer::type_options options,
  const name_str&                       name,
  const observer&                       obs) noexcept
{
    switch (options) {
    case variable_observer::type_options::line:
        ImPlot::PlotLineG(name.c_str(),
                          ring_buffer_getter,
                          const_cast<void*>(reinterpret_cast<const void*>(
                            &obs.linearized_buffer)),
                          obs.linearized_buffer.ssize());
        break;

    case variable_observer::type_options::dash:
        ImPlot::PlotScatterG(name.c_str(),
                             ring_buffer_getter,
                             const_cast<void*>(reinterpret_cast<const void*>(
                               &obs.linearized_buffer)),
                             obs.linearized_buffer.ssize());
        break;

    default:
        break;
    }
}

void plot_observation_widget::show_plot_line(
  const observer&                       obs,
  const variable_observer::type_options options,
  const name_str&                       name) noexcept
{
    if (obs.linearized_buffer.size() <= 1)
        return;

    ImGui::PushID(&obs);

    if (get_interpolate_type(obs.type) == interpolate_type::none) {
        show_discrete_plot_line(options, name, obs);
    } else {
        show_continuous_plot_line(options, name, obs);
    }

    ImGui::PopID();
}

} // irt
