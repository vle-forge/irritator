// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "application.hpp"

namespace irt {

static void save_simulation_graph(const simulation&      sim,
                                  const std::string_view absolute_path) noexcept
{
    try {
        std::filesystem::path path(absolute_path);
        path /= "simulation-graph.dot";

        auto f = file::open(path, file_mode{ file_open_options::write });
        if (f.has_value())
            write_dot_graph_simulation(f->to_file(), sim);
    } catch (...) {
    }
}

static expected<file> save_simulation_raw_data(
  const std::string_view absolute_path,
  const bool             is_binary) noexcept
{
    try {
        std::filesystem::path path(absolute_path);
        path /= "simulation-raw.txt";

        const auto mode = is_binary ? file_mode{ file_open_options::write }
                                    : file_mode{ file_open_options::write,
                                                 file_open_options::text };

        return file::open(path, mode);
    } catch (...) {
        return make_error(fs_errc::user_directory_access_fail);
    }
}

static void finalize_raw_obs(project_editor& ed) noexcept
{
    debug::ensure(ed.raw_ofs.is_open());
    debug::ensure(std::ferror(ed.raw_ofs.to_file()) == 0);

    if (ed.save_simulation_raw_data == project_editor::raw_data_type::binary) {
        for (const auto& mdl : ed.pj.sim.models) {
            dispatch(
              mdl,
              []<typename Dynamics>(const Dynamics& dyn,
                                    auto*           ofs,
                                    const auto      index,
                                    const auto      t,
                                    const auto      tl) noexcept {
                  if constexpr (has_observation_function<Dynamics>) {
                      const auto obs = dyn.observation(t, t - tl);

                      std::fwrite(&t, sizeof(t), 1, ofs);
                      std::fwrite(&index, sizeof(index), 1, ofs);
                      std::fwrite(&obs, sizeof(raw_sample), 1, ofs);
                  }
              },
              ed.raw_ofs.to_file(),
              get_index(ed.pj.sim.get_id(mdl)),
              ed.pj.sim.current_time(),
              mdl.tl);
        }
    } else {
        for (const auto& mdl : ed.pj.sim.models) {
            dispatch(
              mdl,
              []<typename Dynamics>(const Dynamics& dyn,
                                    auto*           ofs,
                                    const auto      index,
                                    const auto      t,
                                    const auto      tl) noexcept {
                  if constexpr (has_observation_function<Dynamics>) {
                      const auto obs = dyn.observation(t, t - tl);

                      fmt::print(ofs,
                                 "{};{};{};{};{};{}\n",
                                 t,
                                 index,
                                 obs.t,
                                 obs.value,
                                 obs.slope,
                                 obs.curvature);
                  }
              },
              ed.raw_ofs.to_file(),
              get_index(ed.pj.sim.get_id(mdl)),
              ed.pj.sim.current_time(),
              mdl.tl);
        }
    }

    ed.raw_ofs.close();
}

static bool run_raw_obs(project_editor& ed) noexcept
{
    debug::ensure(ed.raw_ofs.is_open());
    debug::ensure(std::ferror(ed.raw_ofs.to_file()) == 0);

    const auto ret = ed.pj.sim.run_with_cb(
      [](const auto& sim, const auto mdls, auto* ofs, auto type) noexcept {
          if (type == project_editor::raw_data_type::binary) {
              for (const auto mdl_id : mdls) {
                  if (const auto* mdl = sim.models.try_to_get(mdl_id)) {
                      dispatch(
                        *mdl,
                        []<typename Dynamics>(const Dynamics& dyn,
                                              auto&           ofs,
                                              const auto      index,
                                              const auto      t,
                                              const auto      tl) noexcept {
                            if constexpr (has_observation_function<Dynamics>) {
                                const auto obs = dyn.observation(t, t - tl);

                                std::fwrite(&t, sizeof(t), 1, ofs);
                                std::fwrite(&index, sizeof(index), 1, ofs);
                                std::fwrite(&obs, sizeof(raw_sample), 1, ofs);
                            }
                        },
                        ofs,
                        get_index(mdl_id),
                        sim.current_time(),
                        mdl->tl);
                  }
              }
          } else {
              for (const auto mdl_id : mdls) {
                  if (const auto* mdl = sim.models.try_to_get(mdl_id)) {
                      dispatch(
                        *mdl,
                        []<typename Dynamics>(const Dynamics& dyn,
                                              auto&           ofs,
                                              const auto      index,
                                              const auto      t,
                                              const auto      tl) noexcept {
                            if constexpr (has_observation_function<Dynamics>) {
                                const auto obs = dyn.observation(t, t - tl);

                                fmt::print(ofs,
                                           "{};{};{};{};{};{}\n",
                                           t,
                                           index,
                                           obs.t,
                                           obs.value,
                                           obs.slope,
                                           obs.curvature);
                            }
                        },
                        ofs,
                        get_index(mdl_id),
                        sim.current_time(),
                        mdl->tl);
                  }
              }
          }
      },
      ed.raw_ofs.to_file(),
      ed.save_simulation_raw_data);

    if (ret.has_error()) {
        ed.pj.simulation_state = simulation_status::finish_requiring;

        log(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg,
                   "Fail in {} with error {}",
                   ordinal(ret.error().cat()),
                   ret.error().value());
        });
    }

    if (std::ferror(ed.raw_ofs.to_file())) {
        log(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg, "Fail to write raw data to file");
        });
        ed.save_simulation_raw_data = project_editor::raw_data_type::none;
        ed.raw_ofs.close();
    }

    return ret.has_value();
}

void project_editor::update_simulation_state(application& app) noexcept
{
    const auto required_run =
      pj.simulation_state == simulation_status::run_requiring;
    const auto required_finish =
      any_equal(pj.simulation_state, simulation_status::finish_requiring);

    if (pj.real_time_mode and required_run and not pj.empty_commands()) {
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

        if (pj.real_time_mode) {
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
    const auto can_debug =
      pj.simulation_state == simulation_status::paused and pj.debug_mode;

    debug::ensure(can_debug);

    if (can_debug) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id),
                                [&]() noexcept { pj.simulation_advance(); });
    }
}

void project_editor::back_simulation(application& app) noexcept
{
    const auto can_debug =
      pj.simulation_state == simulation_status::paused and pj.debug_mode;

    debug::ensure(can_debug);

    if (can_debug) {
        const auto pj_id = app.pjs.get_id(*this);

        app.add_simulation_task(ordinal(pj_id),
                                [&]() noexcept { pj.simulation_back(); });
    }
}

} // namespace irt
