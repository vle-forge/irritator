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
        int obs_max = app.sim.immediate_observers.ssize();
        int current = 0;

        while (obs_max > 0) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = app.sim.immediate_observers[i + current];

                jobs[i] = { .app = &app, .id = obs_id };
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

status plot_observation_widget::init(application& app) noexcept
{
    const auto len = app.pj.variable_observers.size();
    clear();

    observers.reserve(len);
    plot_types.reserve(len);
    ids.reserve(len);

    for_each_data(app.pj.variable_observers, [&](auto& var) noexcept {
        const auto var_id = app.pj.variable_observers.get_id(var);

        if_data_exists_do(
          app.sim.models, var.child.mdl_id, [&](auto& mdl) noexcept {
              auto& obs =
                app.sim.observers.alloc(var.name.sv(), ordinal(var_id), 0);
              app.sim.observe(mdl, obs);

              observers.emplace_back(mdl.obs_id);
              plot_types.emplace_back(simulation_plot_type::plotlines);
              ids.emplace_back(app.pj.variable_observers.get_id(var));
          });
    });

    return status::success;
}

void plot_observation_widget::clear() noexcept
{
    observers.clear();
    plot_types.clear();
    ids.clear();
}

void plot_observation_widget::show(application& app) noexcept
{
    if (ImPlot::BeginPlot("variables", ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        for (int i = 0, e = observers.ssize(); i != e; ++i) {
            if_data_exists_do(
              app.sim.observers, observers[i], [&](auto& obs) noexcept {
                  if (obs.linearized_buffer.size() > 0) {
                      switch (plot_types[i]) {
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
        }

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }
}

static void plot_observation_widget_write(plot_observation_widget& plot_widget,
                                          application&             app,
                                          std::ofstream&           ofs) noexcept
{
    ofs.imbue(std::locale::classic());

    bool first_column = true;
    int  size         = std::numeric_limits<int>::max();

    for_specified_data(
      app.sim.observers, plot_widget.observers, [&](auto& obs) noexcept {
          if (first_column) {
              ofs << "t," << obs.name.sv();
              first_column = false;
          } else {
              ofs << "," << obs.name.sv();
          }

          size = std::min(size, obs.linearized_buffer.ssize());
      });

    ofs << '\n';

    for (int i = 0; i < size; ++i) {
        first_column = true;

        for_specified_data(
          app.sim.observers, plot_widget.observers, [&](auto& obs) noexcept {
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
    }
}

static void notification_fail_open_file(application&                 app,
                                        const std::filesystem::path& file_path,
                                        std::string_view title) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = title;
    format(
      n.message, "The file `{}` is not openable", file_path.generic_string());
    app.notifications.enable(n);
}

void plot_observation_widget::write(
  application&                 app,
  const std::filesystem::path& file_path) noexcept
{
    auto ofs = std::ofstream{ file_path };

    return ofs.is_open()
             ? plot_observation_widget_write(*this, app, ofs)
             : notification_fail_open_file(
                 app, file_path, "Fail to open plot observation file");
}

static observer_id get_observer_id(application&         app,
                                   const tree_node&     tn,
                                   const std::span<u64> unique_ids,
                                   const model_id       mdl_id) noexcept
{
    const tree_node* ptr = &tn;

    for (const auto unique_id : unique_ids) {
        if (auto tree_node_id_opt = ptr->get_tree_node_id(unique_id);
            tree_node_id_opt.has_value()) {
            ptr = app.pj.tree_nodes.try_to_get(*tree_node_id_opt);
        }
    }

    if (auto mdl_id_opt = ptr->get_model_id(mdl_id); mdl_id_opt.has_value())
        if (auto* mdl = app.sim.models.try_to_get(*mdl_id_opt); mdl)
            return mdl->obs_id;

    return undefined<observer_id>();
}

status grid_observation_widget_init(grid_observation_widget& grid_widget,
                                    application&             app,
                                    grid_observer&           grid,
                                    grid_component&          compo,
                                    tree_node& grid_parent) noexcept
{
    return if_data_exists_return(
      app.pj.tree_nodes,
      grid.child.tn_id,
      [&](auto& tn) noexcept {
          small_vector<u64, 16> stack;

          // First step, build the stack with unique_id from parent to
          // grid_parent.
          stack.emplace_back(tn.unique_id);
          auto* parent = tn.tree.get_parent();
          while (parent && parent != &grid_parent) {
              stack.emplace_back(parent->unique_id);
              parent = parent->tree.get_parent();
          }

          // Second step, from grid parent, search child
          if (const auto* child = grid_parent.tree.get_child(); child) {
              do {
                  if (child->unique_id == stack.back()) {
                      auto [row, col] = unpack_doubleword(child->unique_id);

                      grid_widget.observers[row * compo.column + col] =
                        get_observer_id(
                          app,
                          *child,
                          std::span(stack.begin(), stack.end() - 1),
                          grid.child.mdl_id);
                  }
              } while (child);
          }

          return status::success;
      },
      status::unknown_dynamics);
}

status grid_observation_widget::init(application&   app,
                                     grid_observer& grid) noexcept
{
    return if_data_exists_return(
      app.pj.tree_nodes,
      grid.child.parent_id,
      [&](auto& grid_tn) noexcept {
          return if_data_exists_return(
            app.mod.components,
            grid_tn.id,
            [&](auto& compo) noexcept {
                irt_assert(compo.type == component_type::grid);

                return if_data_exists_return(
                  app.mod.grid_components,
                  compo.id.grid_id,
                  [&](auto& grid_compo) noexcept {
                      const auto len = grid_compo.row * grid_compo.column;

                      observers.resize(len);
                      values.resize(len);

                      std::fill_n(
                        observers.data(), len, undefined<observer_id>());
                      std::fill_n(values.data(), len, none_value);

                      id = app.pj.grid_observers.get_id(grid);

                      return grid_observation_widget_init(
                        *this, app, grid, grid_compo, grid_tn);
                  },
                  status::unknown_dynamics); // unknown grid-compo
            },
            status::unknown_dynamics); // unknown compo
      },
      status::unknown_dynamics); // unknown_grid
}

void grid_observation_widget::resize(int rows_, int cols_) noexcept
{
    const auto len = rows_ * cols_;
    rows           = rows_;
    cols           = cols_;

    irt_assert(len > 0);

    observers.resize(len);
    values.resize(len);
    clear();
}

void grid_observation_widget::clear() noexcept
{
    std::fill_n(observers.data(), observers.size(), undefined<observer_id>());
    std::fill_n(values.data(), values.size(), none_value);
}

void grid_observation_widget::update(application& app) noexcept
{
    irt_assert(rows * cols == observers.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            values[row * cols + col] = if_data_exists_return(
              app.sim.observers,
              observers[row * cols + col],
              [&](auto& obs) noexcept -> real {
                  return obs.linearized_buffer.back().y;
              },
              none_value);
        }
    }
}

void grid_observation_widget::show(application& app) noexcept
{
    if_data_exists_do(app.pj.grid_observers, id, [&](auto& grid_obs) noexcept {
        ImGui::PushID(this);
        if (ImPlot::BeginPlot(grid_obs.name.c_str(), ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::PlotHeatmap(
              grid_obs.name.c_str(), values.data(), rows, cols);

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
        ImGui::PopID();
    });
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
