// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

namespace irt {

void project_editor::import_from_modeling(application& app) noexcept
{
    const auto is_task_running = pj.is_task_running();

    debug::ensure(not is_task_running);

    if (not is_task_running) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() {
            force_pause = false;

            if (auto ret = pj.simulation_copy(app.mod); ret.has_error()) {
                log(log_level::error, [&](auto& m) {
                    format(m, "Simulation copy failed: {}", ret.error());
                });

                force_pause = true;
            } else {
                if (auto ret = pj.simulation_init(app.mod); ret.has_error()) {
                    log(log_level::error, [&](auto& m) {
                        format(m, "Simulation initialization failed: {}",
                               ret.error());
                    });

                    force_pause = true;
                }
            }
        });
    }
}

void project_editor::init_simulation(application& app) noexcept
{
    const auto is_task_running = pj.is_task_running();

    debug::ensure(not is_task_running);

    if (not is_task_running) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
            force_pause = false;

            if (auto ret = pj.simulation_init(app.mod); ret.has_error()) {
                log(log_level::error, [&](auto& m) {
                    format(m, "Simulation initialization failed: {}",
                           ret.error());
                });

                force_pause = true;
            }
        });
    }
}

} // namespace irt
