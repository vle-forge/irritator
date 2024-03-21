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

// void simulation_observation::init() noexcept
// {
//     irt_assert(raw_buffer_limits.is_valid(raw_buffer_size));
//     irt_assert(linearized_buffer_limits.is_valid(linearized_buffer_size));

//     auto& sim = container_of(this, &application::sim_obs).sim;

//     for_each_data(sim.observers, [&](observer& obs) {
//         obs.clear();
//         obs.buffer.reserve(raw_buffer_size);
//         obs.linearized_buffer.reserve(linearized_buffer_size);
//     });
// }

// void simulation_observation::clear() noexcept
// {
//     // auto& sim = container_of(this, &application::sim_obs).sim;

//     // for_each_data(sim.observers, [&](observer& obs) { obs.clear(); });
// }

// /* This task performs output interpolation Internally, it uses the
//    unordered_task_list to compute observations, one job by observers. If the
//    immediate_observers is empty then all observers are update. */
// void simulation_observation::update() noexcept
// {
//     auto& app       = container_of(this, &application::sim_obs);
//     auto& task_list = app.get_unordered_task_list(0);

//     constexpr int capacity = 255;

//     if (app.sim.immediate_observers.empty()) {
//         int       obs_max = app.sim.observers.ssize();
//         observer* obs     = nullptr;

//         while (app.sim.observers.next(obs)) {
//             int loop = std::min(obs_max, capacity);

//             for (int i = 0; i != loop; ++i) {
//                 auto obs_id = app.sim.observers.get_id(*obs);
//                 app.sim.observers.next(obs);

//                 task_list.add([&app, obs_id]() noexcept {
//                     if_data_exists_do(app.sim.observers,
//                                       obs_id,
//                                       [&](observer& obs) noexcept -> void {
//                                           while (obs.buffer.ssize() > 2)
//                                               flush_interpolate_data(
//                                                 obs, app.sim_obs.time_step);
//                                       });
//                 });
//             }

//             task_list.submit();
//             task_list.wait();

//             if (obs_max >= capacity)
//                 obs_max -= capacity;
//             else
//                 obs_max = 0;
//         }
//     } else {
//         irt_assert(app.simulation_ed.simulation_state !=
//                    simulation_status::finished);

//         int obs_max = app.sim.immediate_observers.ssize();
//         int current = 0;

//         while (obs_max > 0) {
//             int loop = std::min(obs_max, capacity);

//             for (int i = 0; i != loop; ++i) {
//                 auto obs_id = app.sim.immediate_observers[i + current];

//                 task_list.add([&app, obs_id]() noexcept {
//                     if_data_exists_do(app.sim.observers,
//                                       obs_id,
//                                       [&](observer& obs) noexcept -> void {
//                                           while (obs.buffer.ssize() > 2)
//                                               write_interpolate_data(
//                                                 obs, app.sim_obs.time_step);
//                                       });
//                 });
//             }

//             task_list.submit();
//             task_list.wait();

//             current += loop;
//             if (obs_max > capacity)
//                 obs_max -= capacity;
//             else
//                 obs_max = 0;
//         }
//     }
// }

} // namespace irt
