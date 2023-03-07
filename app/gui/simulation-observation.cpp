// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/io.hpp>

#include <cmath>
#include <limits>

namespace irt {

simulation_observation::simulation_observation(model_id mdl_,
                                               i32 buffer_capacity) noexcept
  : model{ mdl_ }
  , linear_outputs{ buffer_capacity }
{
    irt_assert(buffer_capacity > 0);
}

void simulation_observation::clear() noexcept
{
    output_vec.clear();
    linear_outputs.clear();

    limits.Min = -std::numeric_limits<double>::infinity();
    limits.Max = +std::numeric_limits<double>::infinity();
}

void simulation_observation::write(
  const std::filesystem::path& file_path) noexcept
{
    try {
        if (auto ofs = std::ofstream{ file_path }; ofs.is_open()) {
            auto it = linear_outputs.head();
            auto et = linear_outputs.end();

            for (; it != et; ++it)
                ofs << it->x << ',' << it->y << '\n';
        }
    } catch (const std::exception& /*e*/) {
    }
}

void simulation_observation::update(observer& obs) noexcept
{
    while (obs.buffer.ssize() > 2) {
        auto it = std::back_insert_iterator<simulation_observation>(*this);
        write_interpolate_data(obs, it, time_step);
    }

    if (linear_outputs.ssize() >= 1) {
        limits.Min = linear_outputs.head()->x;
        limits.Max = linear_outputs.tail()->x;
    }
}

void simulation_observation::flush(observer& obs) noexcept
{
    auto it = std::back_insert_iterator<simulation_observation>(*this);
    flush_interpolate_data(obs, it, time_step);

    if (linear_outputs.ssize() >= 1) {
        limits.Min = linear_outputs.head()->x;
        limits.Max = linear_outputs.tail()->x;
    }
}

void simulation_observation::push_back(real r) noexcept
{
    if (output_vec.size() >= 2) {
        linear_outputs.force_enqueue(
          ImPlotPoint{ output_vec[0], output_vec[1] });
        output_vec.clear();
    }

    output_vec.emplace_back(r);
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

    if (auto* obs = job->app->sim.observers.try_to_get(job->id)) {
        auto sim_obs_id = enum_cast<simulation_observation_id>(obs->user_id);
        if (auto* sobs = job->app->simulation_ed.sim_obs.try_to_get(sim_obs_id);
            sobs)
            sobs->update(*obs);
    }
}

/* This job retrieves observation data from observer to interpolate data a fill
   simulation_observation structure.  */
static void simulation_observation_job_finish(void* param) noexcept
{
    auto* job = reinterpret_cast<simulation_observation_job*>(param);

    if (auto* obs = job->app->sim.observers.try_to_get(job->id)) {
        auto sim_obs_id = enum_cast<simulation_observation_id>(obs->user_id);
        if (auto* sobs = job->app->simulation_ed.sim_obs.try_to_get(sim_obs_id);
            sobs)
            sobs->flush(*obs);
    }
}

/* This task performs output interpolation Internally, it uses the
   unordered_task_list to compute observations, one job by observers. If the
   immediate_observers is empty then all observers are update. */
void simulation_editor::build_observation_output() noexcept
{
    auto* app = container_of(this, &application::simulation_ed);

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

} // namespace irt
