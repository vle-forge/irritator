// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "implot.h"
#include "internal.hpp"
#include "irritator/modeling.hpp"

#include <algorithm>
#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/observation.hpp>

#include <cmath>
#include <limits>
#include <locale>
#include <optional>
#include <type_traits>

namespace irt {

void simulation_observation::init() noexcept
{
    irt_assert(raw_buffer_limits.is_valid(raw_buffer_size));
    irt_assert(linearized_buffer_limits.is_valid(linearized_buffer_size));

    auto& sim = container_of(this, &application::sim_obs).sim;

    for_each_data(sim.observers, [&](observer& obs) {
        obs.clear();
        obs.buffer.reserve(raw_buffer_size);
        obs.linearized_buffer.reserve(linearized_buffer_size);
    });
}

void simulation_observation::clear() noexcept
{
    auto& sim = container_of(this, &application::sim_obs).sim;

    for_each_data(sim.observers, [&](observer& obs) { obs.clear(); });
}

struct simulation_observation_job
{
    application* app = nullptr;
    observer_id  id  = undefined<observer_id>();
};

/* This job retrieves observation data from observer to interpolate data a fill
   simulation_observation structure.  */
static void simulation_observation_job_update(void* param) noexcept
{
    auto* job = reinterpret_cast<simulation_observation_job*>(param);

    if_data_exists_do(
      job->app->sim.observers, job->id, [&](observer& obs) noexcept -> void {
          while (obs.buffer.ssize() > 2)
              write_interpolate_data(obs, job->app->sim_obs.time_step);
      });
}

/* This job retrieves observation data from observer to interpolate data a fill
   simulation_observation structure.  */
static void simulation_observation_job_finish(void* param) noexcept
{
    auto* job = reinterpret_cast<simulation_observation_job*>(param);

    if_data_exists_do(
      job->app->sim.observers, job->id, [&](observer& obs) noexcept -> void {
          flush_interpolate_data(obs, job->app->sim_obs.time_step);
      });
}

/* This task performs output interpolation Internally, it uses the
   unordered_task_list to compute observations, one job by observers. If the
   immediate_observers is empty then all observers are update. */
void simulation_observation::update() noexcept
{
    auto& app = container_of(this, &application::sim_obs);

    constexpr int              capacity = 255;
    simulation_observation_job jobs[capacity];

    auto& task_list = app.get_unordered_task_list(0);

    if (app.sim.immediate_observers.empty()) {
        int       obs_max = app.sim.observers.ssize();
        observer* obs     = nullptr;

        while (app.sim.observers.next(obs)) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = app.sim.observers.get_id(*obs);
                app.sim.observers.next(obs);

                jobs[i] = { .app = &app, .id = obs_id };
                task_list.add(simulation_observation_job_update, &jobs[i]);
            }

            task_list.submit();
            task_list.wait();

            if (obs_max >= capacity)
                obs_max -= capacity;
            else
                obs_max = 0;
        }
    } else {
        irt_assert(app.simulation_ed.simulation_state !=
                   simulation_status::finished);

        int obs_max = app.sim.immediate_observers.ssize();
        int current = 0;

        while (obs_max > 0) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = app.sim.immediate_observers[i + current];

                jobs[i] = { .app = &app, .id = obs_id };
                task_list.add(simulation_observation_job_update, &jobs[i]);
            }

            task_list.submit();
            task_list.wait();

            current += loop;
            if (obs_max > capacity)
                obs_max -= capacity;
            else
                obs_max = 0;
        }
    }
}

void plot_copy::show() noexcept
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
