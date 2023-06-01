// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "implot.h"
#include "internal.hpp"

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/observation.hpp>

#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>

namespace irt {

void simulation_observation::init() noexcept
{
    irt_assert(raw_buffer_limits.is_valid(raw_buffer_size));
    irt_assert(linearized_buffer_limits.is_valid(linearized_buffer_size));

    auto& sim = container_of(this, &application::sim_obs)->sim;

    for_each_data(sim.observers, [&](observer& obs) {
        obs.clear();
        obs.buffer.reserve(raw_buffer_size);
        obs.linearized_buffer.reserve(linearized_buffer_size);
    });
}

void simulation_observation::clear() noexcept
{
    auto& sim = container_of(this, &application::sim_obs)->sim;

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
    auto* app = container_of(this, &application::sim_obs);

    constexpr int              capacity = 255;
    simulation_observation_job jobs[capacity];

    auto& task_list = app->get_unordered_task_list(0);

    if (app->sim.immediate_observers.empty()) {
        int       obs_max = app->sim.observers.ssize();
        observer* obs     = nullptr;

        while (app->sim.observers.next(obs)) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = app->sim.observers.get_id(*obs);
                app->sim.observers.next(obs);

                jobs[i] = { .app = app, .id = obs_id };
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
        int obs_max = app->sim.immediate_observers.ssize();
        int current = 0;

        while (obs_max > 0) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = app->sim.immediate_observers[i + current];

                jobs[i] = { .app = app, .id = obs_id };
                task_list.add(simulation_observation_job_finish, &jobs[i]);
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

void plot_observation::clear() noexcept { children.clear(); }

void plot_observation::show(application& app) noexcept
{
    ImGui::PushID(this);

    if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        for_specified_data(app.sim.models, children, [&](auto& mdl) noexcept {
            if_data_exists_do(
              app.sim.observers, mdl.obs_id, [&](auto& obs) noexcept {
                  if (obs.linearized_buffer.size() > 0) {
                      switch (plot_type) {
                      case simulation_plot_type::plotlines:
                          ImPlot::PlotLineG(obs.name.c_str(),
                                            ring_buffer_getter,
                                            &obs.linearized_buffer,
                                            obs.linearized_buffer.ssize());
                          break;

                      case simulation_plot_type::plotscatters:
                          ImPlot::PlotScatterG(obs.name.c_str(),
                                               ring_buffer_getter,
                                               &obs.linearized_buffer,
                                               obs.linearized_buffer.ssize());
                          break;

                      default:
                          break;
                      }
                  }
              });
        });

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }

    ImGui::PopID();
}

void plot_observation::write(application&                 app,
                             const std::filesystem::path& file_path) noexcept
{
    if (auto ofs = std::ofstream{ file_path }; ofs.is_open()) {
        bool first_column = true;
        int  size         = std::numeric_limits<int>::max();

        for_specified_data(app.sim.models, children, [&](auto& mdl) noexcept {
            if_data_exists_do(
              app.sim.observers, mdl.obs_id, [&](auto& obs) noexcept {
                  if (first_column) {
                      ofs << "t," << obs.name.sv();
                      first_column = false;
                  } else {
                      ofs << "," << obs.name.sv();
                  }

                  size = std::min(size, obs.linearized_buffer.ssize());
              });
        });

        ofs << '\n';

        for (int i = 0; i < size; ++i) {
            first_column = true;

            for_specified_data(
              app.sim.models, children, [&](auto& mdl) noexcept {
                  if_data_exists_do(
                    app.sim.observers, mdl.obs_id, [&](auto& obs) noexcept {
                        auto idx = obs.linearized_buffer.index_from_begin(i);
                        if (first_column) {
                            ofs << obs.linearized_buffer[idx].x << ","
                                << obs.linearized_buffer[idx].y;
                            first_column = false;
                        } else {
                            ofs << "," << obs.linearized_buffer[idx].y;
                        }
                    });

                  ofs << '\n';
              });
        }
    }
}

void grid_observation::resize(int rows_, int cols_) noexcept
{
    const auto len = rows_ * cols_;
    rows           = rows_;
    cols           = cols_;

    irt_assert(len > 0);

    children.resize(len);
    values.resize(len);
    clear();
}

void grid_observation::clear() noexcept
{
    std::fill_n(children.data(), children.size(), undefined<model_id>());
    std::fill_n(values.data(), values.size(), none_value);
}

void grid_observation::show(application& app) noexcept
{
    irt_assert(rows * cols == children.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            values[row * cols + col] = if_data_exists_return(
              app.sim.models,
              children[row * cols + col],
              [&](auto& mdl) noexcept -> real {
                  return if_data_exists_return(
                    app.sim.observers,
                    mdl.obs_id,
                    [&](auto& obs) noexcept {
                        return obs.linearized_buffer.back().y;
                    },
                    none_value);
              },
              none_value);
        }
    }

    ImGui::PushID(this);
    if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::PlotHeatmap(name.c_str(), values.data(), rows, cols);

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }
    ImGui::PopID();
}

void plot_copy::show(application& /*app*/) noexcept
{
    ImGui::PushID(this);

    if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        if (linear_outputs.size() > 0) {
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
