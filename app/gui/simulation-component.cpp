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

void project_editor::update_simulation_state(application& app) noexcept
{
    const auto required_run =
      pj.simulation_state == simulation_status::run_requiring;
    const auto required_finish =
      any_equal(pj.simulation_state, simulation_status::finish_requiring);

    if (pj.flags[project::simulation_flag::real_time] and required_run and
        not pj.empty_commands()) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
            (void)pj.simulation_apply_command();
        });
    }

    if (required_run) {
        pj.simulation_state = simulation_status::run_requiring;

        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() {
            pj.simulation_run_for(app.get_unordered_task_list(),
                                  simulation_task_duration,
                                  force_pause);
        });
    }

    if (required_finish) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() {
            (void)pj.simulation_finish(app.get_unordered_task_list());
        });
    }
}

void project_editor::import_from_modeling(application& app) noexcept
{
    const auto is_task_running = pj.is_task_running();

    debug::ensure(not is_task_running);

    if (not is_task_running) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() {
            force_pause = false;

            pj.simulation_copy(app.mod);
            pj.simulation_init(app.mod);
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

            pj.simulation_init(app.mod);
        });
    }
}

void project_editor::run_bag_simulation(application& app) noexcept
{
    const auto can_run = any_equal(pj.simulation_state,
                                   simulation_status::initialized,
                                   simulation_status::paused);

    debug::ensure(can_run);

    if (can_run) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
            force_pause = false;

            (void)pj.simulation_step();
        });
    }
}

void project_editor::run_simulation(application& app) noexcept
{
    const auto can_run = any_equal(pj.simulation_state,
                                   simulation_status::initialized,
                                   simulation_status::run_requiring,
                                   simulation_status::paused);

    debug::ensure(can_run);

    if (can_run) {
        const auto pj_id = app.pjs.get_id(*this);

        if (pj.flags[project::simulation_flag::real_time]) {
            app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
                force_pause = false;

                (void)pj.simulation_live_run(app.get_unordered_task_list(),
                                             one_simulation_time_duration,
                                             simulation_task_duration,
                                             force_pause);
            });
        } else {
            app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
                force_pause = false;

                (void)pj.simulation_run_for(app.get_unordered_task_list(),
                                            simulation_task_duration,
                                            force_pause);
            });
        }
    }
}

void project_editor::pause_simulation() noexcept
{
    const auto state =
      any_equal(pj.simulation_state, simulation_status::running);

    debug::ensure(state);

    if (state)
        force_pause = true;
}

void project_editor::finish_simulation(application& app) noexcept
{
    const auto state =
      pj.simulation_state == simulation_status::finish_requiring;

    debug::ensure(state);

    if (state) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id), [&]() noexcept {
            (void)pj.simulation_finish(app.get_unordered_task_list());
        });
    }
}

void project_editor::advance_simulation(application& app) noexcept
{
    const auto can_debug = pj.simulation_state == simulation_status::paused and
                           pj.flags[project::simulation_flag::debug];

    debug::ensure(can_debug);

    if (can_debug) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id),
                                [&]() noexcept { pj.simulation_advance(); });
    }
}

void project_editor::back_simulation(application& app) noexcept
{
    const auto can_debug = pj.simulation_state == simulation_status::paused and
                           pj.flags[project::simulation_flag::debug];

    debug::ensure(can_debug);

    if (can_debug) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id),
                                [&]() noexcept { pj.simulation_back(); });
    }
}

} // namespace irt
