// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

#include <fmt/chrono.h>

#include "application.hpp"
#include "irritator/core.hpp"
#include "irritator/format.hpp"
#include "irritator/helpers.hpp"
#include "irritator/modeling.hpp"

namespace irt {

static constexpr std::string_view simulation_part_names[] = {
    "messages", "nodes",     "dated_messages", "models",
    "hsms",     "observers", "scheduler",      "external_sources"
};

static status simulation_init_observation(application& app) noexcept
{
    for (auto& grid_obs : app.pj.grid_observers)
        grid_obs.init(app.pj, app.mod, app.pj.sim);

    for (auto& graph_obs : app.pj.graph_observers)
        graph_obs.init(app.pj, app.mod, app.pj.sim);

    for (auto& v_obs : app.pj.variable_observers)
        irt_check(v_obs.init(app.pj, app.pj.sim));

    app.pj.file_obs.initialize(app.pj.sim, app.pj);

    return success();
}

template<typename S>
constexpr static void make_copy_error_msg(application& app,
                                          const S&     str) noexcept
{
    auto& n   = app.notifications.alloc(log_level::error);
    n.title   = "Component copy failed";
    n.message = str;
    app.notifications.enable(n);
}

template<typename S, typename... Args>
constexpr static void make_copy_error_msg(application& app,
                                          const S&     fmt,
                                          Args&&... args) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = "Component copy failed";
    format(n.message, fmt, std::forward<Args...>(args...));
    app.notifications.enable(n);
}

template<typename S, typename... Args>
static void make_init_error_msg(application& app,
                                const S&     fmt,
                                Args&&... args) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = "Simulation initialization fail";

    auto ret = fmt::vformat_to_n(n.message.begin(),
                                 static_cast<size_t>(n.message.capacity() - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    n.message.resize(static_cast<int>(ret.size));

    app.notifications.enable(n);
}

static void simulation_copy(application& app) noexcept
{
    app.simulation_ed.simulation_state = simulation_status::initializing;

    auto  compo_id = app.pj.head();
    auto* compo    = app.mod.components.try_to_get(compo_id);
    auto* head     = app.pj.tn_head();

    if (!head || !compo) {
        app.simulation_ed.simulation_state = simulation_status::not_started;
        make_copy_error_msg(app, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          irt_check(app.pj.set(app.mod, *compo));
          irt_check(app.pj.sim.srcs.prepare());
          app.pj.sim.t = app.pj.t_limit.begin();
          irt_check(app.pj.sim.initialize());
          app.simulation_ed.simulation_state = simulation_status::initialized;
          return success();
      },

      [&](const project::part s) noexcept -> void {
          app.simulation_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error {} in project copy", ordinal(s));
      },

      [&](const simulation::part s) noexcept -> void {
          app.simulation_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error {} in simulation copy", ordinal(s));
      },

      [&]() noexcept -> void {
          app.simulation_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Unknown error");
      });
}

static void simulation_init(application& app) noexcept
{
    app.simulation_ed.simulation_state = simulation_status::initializing;

    app.simulation_ed.tl.reset();

    auto* head = app.pj.tn_head();
    if (!head) {
        app.simulation_ed.simulation_state = simulation_status::not_started;
        make_init_error_msg(app, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          app.pj.sim.t                                 = app.pj.t_limit.begin();
          app.simulation_ed.simulation_last_finite_t   = app.pj.sim.t;
          app.simulation_ed.simulation_display_current = app.pj.sim.t;

          irt_check(simulation_init_observation(app));
          irt_check(app.pj.sim.srcs.prepare());
          irt_check(app.pj.sim.initialize());
          app.simulation_ed.simulation_state = simulation_status::initialized;
          return success();
      },

      [&](const simulation::part s) noexcept -> void {
          app.simulation_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error in simulation: {}", ordinal(s));
      },

      [&]() noexcept -> void {
          app.simulation_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Unknown error");
      });
}

static bool debug_run(simulation_editor& sim_ed) noexcept
{
    auto success = true;

    attempt_all(
      [&] {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          return run(sim_ed.tl, app.pj.sim, app.pj.sim.t);
      },

      [&](simulation::part type, simulation::model_error* ptr) noexcept {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          sim_ed.simulation_state = simulation_status::finish_requiring;
          success                 = false;

          app.notifications.try_insert(
            log_level::error, [&](auto& t, auto& msg) noexcept {
                t = "Simulation debug task run error";

                if (ptr)
                    format(msg,
                           "Model error in part {}",
                           simulation_part_names[ordinal(type)]);
                else
                    format(msg,
                           "Error in part {}",
                           simulation_part_names[ordinal(type)]);
            });
      },

      [&] {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          sim_ed.simulation_state = simulation_status::finish_requiring;
          success                 = false;

          app.notifications.try_insert(
            log_level::error, [&](auto& t, auto& /*msg*/) noexcept {
                t = "Simulation debug task run error";
            });
      });

    return success;
}

static bool run(simulation_editor& sim_ed) noexcept
{
    auto success = true;

    attempt_all(
      [&] {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          return app.pj.sim.run();
      },

      [&](simulation::part type, simulation::model_error* ptr) noexcept {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          sim_ed.simulation_state = simulation_status::finish_requiring;
          success                 = false;

          app.notifications.try_insert(
            log_level::error, [&](auto& t, auto& msg) noexcept {
                t = "Simulation task run error";

                if (ptr)
                    format(msg,
                           "Model error in part {}",
                           simulation_part_names[ordinal(type)]);
                else
                    format(msg,
                           "Error in part {}",
                           simulation_part_names[ordinal(type)]);
            });
      },

      [&] {
          auto& app = container_of(&sim_ed, &application::simulation_ed);
          sim_ed.simulation_state = simulation_status::finish_requiring;
          success                 = false;

          app.notifications.try_insert(log_level::error,
                                       [&](auto& t, auto& /*msg*/) noexcept {
                                           t = "Simulation  task run error";
                                       });
      });

    return success;
}

void simulation_editor::start_simulation_static_run() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        app.simulation_ed.simulation_state = simulation_status::running;
        namespace stdc                     = std::chrono;

        auto start_at = stdc::high_resolution_clock::now();
        auto end_at   = stdc::high_resolution_clock::now();
        auto duration = end_at - start_at;

        auto duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
        auto duration_since_start = duration_cast.count();

        bool stop_or_pause;

        do {
            if (app.simulation_ed.simulation_state !=
                simulation_status::running)
                return;

            if (app.simulation_ed.store_all_changes) {
                if (auto ret = debug_run(app.simulation_ed); !ret) {
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    app.simulation_ed.simulation_display_current = app.pj.sim.t;
                    return;
                }
            } else {
                if (auto ret = run(app.simulation_ed); !ret) {
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    app.simulation_ed.simulation_display_current = app.pj.sim.t;
                    return;
                }
            }

            app.simulation_ed.start_simulation_observation();

            if (not time_domain<time>::is_infinity(app.pj.t_limit.begin()) &&
                app.pj.sim.t >= app.pj.t_limit.end()) {
                app.pj.sim.t = app.pj.t_limit.end();
                app.simulation_ed.simulation_state =
                  simulation_status::finish_requiring;
                app.simulation_ed.simulation_display_current = app.pj.sim.t;
                return;
            }

            end_at        = stdc::high_resolution_clock::now();
            duration      = end_at - start_at;
            duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();
            stop_or_pause =
              app.simulation_ed.force_pause || app.simulation_ed.force_stop;
        } while (!stop_or_pause && duration_since_start <
                                     app.simulation_ed.thread_frame_duration);

        app.simulation_ed.simulation_display_current = app.pj.sim.t;

        if (app.simulation_ed.force_pause) {
            app.simulation_ed.force_pause = false;
            app.simulation_ed.simulation_state =
              simulation_status::pause_forced;
        } else if (app.simulation_ed.force_stop) {
            app.simulation_ed.force_stop = false;
            app.simulation_ed.simulation_state =
              simulation_status::finish_requiring;
        } else {
            app.simulation_ed.simulation_state = simulation_status::paused;
        }
    });
}

static bool is_simulation_state_not_running(simulation_status s) noexcept
{
    return s != simulation_status::running;
}

static bool is_simulation_force_pause(simulation_status& s, bool pause) noexcept
{
    if (pause)
        s = simulation_status::pause_forced;

    return pause;
}

static bool is_simulation_force_stop(simulation_status& s, bool stop) noexcept
{
    if (stop)
        s = simulation_status::finish_requiring;

    return stop;
}

void simulation_editor::start_simulation_live_run() noexcept
{
    namespace stdc = std::chrono;

    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&]() noexcept {
        app.simulation_ed.simulation_state = simulation_status::running;
        const auto start_task_rt           = stdc::high_resolution_clock::now();
        const auto end_task_rt = start_task_rt + simulation_task_duration;

        for (;;) {
            if (is_simulation_state_not_running(simulation_state) or
                is_simulation_force_pause(simulation_state, force_pause) or
                is_simulation_force_stop(simulation_state, force_stop))
                return;

            time sim_t;
            time sim_next_t;

            {
                std::scoped_lock lock(app.sim_mutex);

                sim_t      = app.pj.sim.t;
                sim_next_t = app.pj.sim.sched.tn();
            }

            if (time_domain<time>::is_infinity(sim_t)) {
                sim_t      = simulation_last_finite_t;
                sim_next_t = sim_t + 1.0;
            } else {
                if (time_domain<time>::is_infinity(sim_next_t)) {
                    sim_next_t = sim_t + 1.0;
                }
            }

            if (app.pj.file_obs.can_update(app.pj.sim.t))
                app.pj.file_obs.update(app.pj.sim, app.pj);

            const auto current_rt   = stdc::high_resolution_clock::now();
            const auto diff_rt      = current_rt - start_task_rt;
            const auto remaining_rt = simulation_task_duration - diff_rt;

            const std::chrono::duration<double, std::nano> x =
              current_rt - start;
            const std::chrono::duration<double, std::nano> y =
              simulation_time_duration;

            simulation_display_current = x / y;

            // There is no real time available for this simulation task. Program
            // the next.
            if (remaining_rt.count() < 0) {
                app.simulation_ed.simulation_state = simulation_status::paused;
                return;
            }

            const auto wakeup_rt =
              start + (sim_next_t * simulation_time_duration);

            // If the next wakeup exceed the simulation frame, do nothing.
            if (wakeup_rt > end_task_rt) {
                simulation_state = simulation_status::paused;
                return;
            }

            if (wakeup_rt >= start_task_rt + std::chrono::milliseconds{ 1 })
                std::this_thread::sleep_until(wakeup_rt);

            {
                std::scoped_lock lock(app.sim_mutex);
                simulation_last_finite_t = sim_t;
                app.pj.sim.t             = sim_t;

                if (store_all_changes) {
                    if (auto ret = debug_run(app.simulation_ed); !ret) {
                        simulation_state = simulation_status::finish_requiring;
                        return;
                    }
                } else {
                    if (auto ret = run(app.simulation_ed); !ret) {
                        simulation_state = simulation_status::finish_requiring;
                        return;
                    }
                }
            }

            if (time_domain<time>::is_infinity(app.pj.sim.t))
                simulation_last_finite_t = sim_next_t;

            start_simulation_observation();
        }
    });
}

void simulation_editor::start_simulation_update_state() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    if (any_equal(simulation_state,
                  simulation_status::paused,
                  simulation_status::run_requiring)) {

        if (have_send_message) {
            const auto mdl_id = *have_send_message;
            const auto t = irt::time_domain<time>::is_infinity(app.pj.sim.t)
                             ? app.pj.sim.last_valid_t
                             : app.pj.sim.t;

            if_data_exists_do(app.pj.sim.models, mdl_id, [&](auto& m) noexcept {
                if (m.type == dynamics_type::constant) {
                    if (m.handle == invalid_heap_handle) {
                        app.pj.sim.sched.alloc(m, mdl_id, t);
                    } else {
                        if (app.pj.sim.sched.is_in_tree(m.handle)) {
                            app.pj.sim.sched.update(m, t);
                        } else {
                            app.pj.sim.sched.reintegrate(m, t);
                        }
                    }

                    m.tn = t;
                }
            });

            have_send_message.reset();
        }

        simulation_state = simulation_status::run_requiring;

        if (real_time)
            start_simulation_live_run();
        else
            start_simulation_static_run();
    }

    if (simulation_state == simulation_status::finish_requiring) {
        simulation_state = simulation_status::finishing;
        start_simulation_finish();
    }

    auto       it = models_to_move.begin();
    const auto et = models_to_move.end();

    for (; it != et; ++it) {
        const auto index   = get_index(it->first);
        const auto index_i = static_cast<int>(index);

        ImNodes::SetNodeScreenSpacePos(index_i, it->second);
    }
}

void simulation_editor::start_simulation_copy_modeling() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    if (state) {
        auto& app           = container_of(this, &application::simulation_ed);
        auto* modeling_head = app.pj.tn_head();
        if (!modeling_head) {
            auto& notif = app.notifications.alloc(log_level::error);
            notif.title = "Empty model";
            app.notifications.enable(notif);
        } else {
            app.simulation_ed.force_pause = false;
            app.simulation_ed.force_stop  = false;

            start_simulation_clear();

            app.add_simulation_task(
              [&app]() noexcept { simulation_copy(app); });

            start_simulation_init();
        }
    }
}

void simulation_editor::start_simulation_init() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);
        app.add_simulation_task([&app]() noexcept {
            app.simulation_ed.force_pause = false;
            app.simulation_ed.force_stop  = false;
            simulation_init(app);
        });
    }
}

void simulation_editor::start_simulation_clear() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    app.simulation_ed.display_graph = false;

    app.add_simulation_task([&app]() noexcept {
        std::scoped_lock lock(app.sim_mutex);
        app.simulation_ed.clear();
    });
}

void simulation_editor::start_simulation_delete() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    app.simulation_ed.display_graph = false;

    app.add_simulation_task([&app]() noexcept {
        std::scoped_lock lock(app.sim_mutex);
        app.pj.clear();
        app.pj.sim.clear();
        app.simulation_ed.clear();
    });
}

void simulation_editor::start_simulation_start() noexcept
{
    const auto state = any_equal(simulation_state,
                                 simulation_status::initialized,
                                 simulation_status::pause_forced,
                                 simulation_status::run_requiring);

    debug::ensure(state);

    if (state) {
        start = std::chrono::high_resolution_clock::now();

        if (real_time)
            start_simulation_live_run();
        else
            start_simulation_static_run();
    }
}

void simulation_editor::stop_simulation_observation() noexcept
{
    auto& app       = container_of(this, &application::simulation_ed);
    auto& task_list = app.get_unordered_task_list(0);

    debug::ensure(app.simulation_ed.simulation_state ==
                  simulation_status::finishing);

    constexpr int capacity = 255;
    int           obs_max  = app.pj.sim.observers.ssize();
    observer*     obs      = nullptr;

    while (app.pj.sim.observers.next(obs)) {
        int loop = std::min(obs_max, capacity);

        for (int i = 0; i != loop; ++i) {
            auto obs_id = app.pj.sim.observers.get_id(*obs);
            app.pj.sim.observers.next(obs);

            task_list.add([&app, obs_id]() noexcept {
                if_data_exists_do(app.pj.sim.observers,
                                  obs_id,
                                  [&](observer& obs) noexcept -> void {
                                      while (obs.buffer.ssize() > 2)
                                          flush_interpolate_data(obs,
                                                                 obs.time_step);
                                  });
            });
        }

        task_list.submit();
        task_list.wait();

        if (obs_max >= capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    app.pj.file_obs.finalize();
}

void simulation_editor::start_simulation_observation() noexcept
{
    auto& app       = container_of(this, &application::simulation_ed);
    auto& task_list = app.get_unordered_task_list(0);

    debug::ensure(app.simulation_ed.simulation_state !=
                  simulation_status::finished);

    constexpr int capacity = 255;
    int           obs_max  = app.pj.sim.immediate_observers.ssize();
    int           current  = 0;

    while (obs_max > 0) {
        int loop = std::min(obs_max, capacity);

        for (int i = 0; i != loop; ++i) {
            auto obs_id = app.pj.sim.immediate_observers[i + current];

            task_list.add([&app, obs_id]() noexcept {
                if_data_exists_do(app.pj.sim.observers,
                                  obs_id,
                                  [&](observer& obs) noexcept -> void {
                                      while (obs.buffer.ssize() > 2)
                                          write_interpolate_data(obs,
                                                                 obs.time_step);
                                  });
            });
        }

        task_list.submit();
        task_list.wait();

        current += loop;
        if (obs_max > capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    current = 0;
    for (auto& g : app.pj.grid_observers) {
        const auto g_id = app.pj.grid_observers.get_id(g);
        task_list.add([&app, g_id]() noexcept {
            if (auto* g = app.pj.grid_observers.try_to_get(g_id); g)
                if (g->can_update(app.pj.sim.t))
                    g->update(app.pj.sim);
        });

        ++current;
        if (current == obs_max) {
            task_list.submit();
            task_list.wait();
            current = 0;
        }
    }

    for (auto& g : app.pj.graph_observers) {
        const auto g_id = app.pj.graph_observers.get_id(g);
        task_list.add([&app, g_id]() noexcept {
            if (auto* g = app.pj.graph_observers.try_to_get(g_id); g)
                if (g->can_update(app.pj.sim.t))
                    g->update(app.pj.sim);
        });
        ++current;
        if (current == obs_max) {
            task_list.submit();
            task_list.wait();
            current = 0;
        }
    }

    if (current > 0) {
        task_list.submit();
        task_list.wait();
    }

    if (app.pj.file_obs.can_update(app.pj.sim.t))
        app.pj.file_obs.update(app.pj.sim, app.pj);
}

void simulation_editor::start_simulation_start_1() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::pause_forced,
                           simulation_status::debugged);

    debug::ensure(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);
        app.add_simulation_task([&app]() noexcept {
            if (auto* parent = app.pj.tn_head(); parent) {
                app.simulation_ed.simulation_state = simulation_status::running;

                if (auto ret = debug_run(app.simulation_ed); !ret) {
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    return;
                }

                if (app.pj.file_obs.can_update(app.pj.sim.t))
                    app.pj.file_obs.update(app.pj.sim, app.pj);

                if (not time_domain<time>::is_infinity(
                      app.pj.t_limit.begin()) &&
                    app.pj.sim.t >= app.pj.t_limit.end()) {
                    app.pj.sim.t = app.pj.t_limit.end();
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    return;
                }

                if (app.simulation_ed.force_pause) {
                    app.simulation_ed.force_pause = false;
                    app.simulation_ed.simulation_state =
                      simulation_status::pause_forced;
                } else if (app.simulation_ed.force_stop) {
                    app.simulation_ed.force_stop = false;
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                } else {
                    app.simulation_ed.simulation_state =
                      simulation_status::pause_forced;
                }
            }
        });
    }
}

void simulation_editor::start_simulation_pause() noexcept
{
    bool state = any_equal(simulation_state, simulation_status::running);

    debug::ensure(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);
        app.add_simulation_task(
          [&app]() noexcept { app.simulation_ed.force_pause = true; });
    }
}

void simulation_editor::start_simulation_stop() noexcept
{
    bool state = any_equal(
      simulation_state, simulation_status::running, simulation_status::paused);

    debug::ensure(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);
        app.add_simulation_task(
          [&app]() noexcept { app.simulation_ed.force_stop = true; });
    }
}

void simulation_editor::start_simulation_finish() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };

        app.simulation_ed.simulation_state = simulation_status::finishing;
        app.pj.sim.immediate_observers.clear();
        app.simulation_ed.stop_simulation_observation();

        if (app.simulation_ed.store_all_changes) {
            if (auto ret =
                  finalize(app.simulation_ed.tl, app.pj.sim, app.pj.sim.t);
                !ret) {
                auto& n = app.notifications.alloc();
                n.title = "Simulation finalizing fail (with store all "
                          "changes option)";
                app.notifications.enable(n);
            }
        } else {
            app.pj.sim.t = app.pj.t_limit.end();
            if (auto ret = app.pj.sim.finalize(); !ret) {
                auto& n = app.notifications.alloc();
                n.title = "simulation finish fail";
                app.notifications.enable(n);
            }
        };

        app.simulation_ed.simulation_state = simulation_status::finished;
    });
}

void simulation_editor::start_simulation_advance() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        if (app.simulation_ed.tl.can_advance()) {
            attempt_all(
              [&]() noexcept -> status {
                  return advance(
                    app.simulation_ed.tl, app.pj.sim, app.pj.sim.t);
              },

              [&](const simulation::part s) noexcept -> void {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Fail to advance the simulation";
                  format(n.message, "Advance message: {}", ordinal(s));
                  app.notifications.enable(n);
              },

              [&]() noexcept -> void {
                  auto& n   = app.notifications.alloc(log_level::error);
                  n.title   = "Fail to advance the simulation";
                  n.message = "Error: Unknown";
                  app.notifications.enable(n);
              });
        }
    });
}

void simulation_editor::start_simulation_back() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        if (app.simulation_ed.tl.can_back()) {
            attempt_all(
              [&]() noexcept -> status {
                  return back(app.simulation_ed.tl, app.pj.sim, app.pj.sim.t);
              },

              [&](const simulation::part s) noexcept -> void {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Fail to back the simulation";
                  format(n.message, "Advance message: {}", ordinal(s));
                  app.notifications.enable(n);
              },

              [&]() noexcept -> void {
                  auto& n   = app.notifications.alloc(log_level::error);
                  n.title   = "Fail to back the simulation";
                  n.message = "Error: Unknown";
                  app.notifications.enable(n);
              });
        }
    });
}

void simulation_editor::start_enable_or_disable_debug() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);
    app.add_simulation_task([&app]() noexcept {
        app.simulation_ed.tl.reset();

        attempt_all(
          [&]() -> status {
              irt_check(
                initialize(app.simulation_ed.tl, app.pj.sim, app.pj.sim.t));

              return success();
          },

          [&](const simulation::part s) noexcept -> void {
              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Debug mode failed to initialize";
              format(
                n.message, "Fail to initialize the debug mode: {}", ordinal(s));
              app.notifications.enable(n);
              app.simulation_ed.simulation_state =
                simulation_status::not_started;
          },

          [&]() noexcept -> void {
              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Debug mode failed to initialize";
              format(n.message,
                     "Fail to initialize the debug mode: Unknown error");
              app.notifications.enable(n);
              app.simulation_ed.simulation_state =
                simulation_status::not_started;
          });
    });
}

void simulation_editor::start_simulation_model_add(const dynamics_type type,
                                                   const float         x,
                                                   const float y) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, type, x, y]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };

        if (!app.pj.sim.can_alloc(1)) {
            auto& n = app.notifications.alloc(log_level::error);
            n.title = "To many model in simulation editor";
            app.notifications.enable(n);
            return;
        }

        auto& mdl    = app.pj.sim.alloc(type);
        auto  mdl_id = app.pj.sim.models.get_id(mdl);

        attempt_all(
          [&]() noexcept -> status {
              irt_check(app.pj.sim.make_initialize(mdl, app.pj.sim.t));

              app.simulation_ed.models_to_move.emplace_enqueue(mdl_id,
                                                               ImVec2(x, y));

              return success();
          },

          [&](const simulation::part s) noexcept -> void {
              app.pj.sim.deallocate(mdl_id);

              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Fail to initialize model";
              format(n.message, "Error: {}", ordinal(s));
              app.notifications.enable(n);
          },

          [&]() noexcept -> void {
              app.pj.sim.deallocate(mdl_id);

              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Fail to initialize model";
              format(n.message, "Unknown error");
              app.notifications.enable(n);
          });
    });
}

void simulation_editor::start_simulation_model_del(const model_id id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, id]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };
        app.pj.sim.deallocate(id);
    });
}

void simulation_editor::remove_simulation_observation_from(
  model_id mdl_id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, mdl_id]() noexcept {
        std::scoped_lock _(app.sim_mutex);

        if_data_exists_do(
          app.pj.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              app.pj.sim.unobserve(mdl);
          });
    });
}

void simulation_editor::add_simulation_observation_for(
  const model_id mdl_id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, mdl_id]() noexcept {
        std::scoped_lock _(app.sim_mutex);

        if_data_exists_do(
          app.pj.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              if (app.pj.sim.observers.can_alloc(1)) {
                  auto& obs = app.pj.sim.observers.alloc();
                  app.pj.sim.observe(mdl, obs);
              } else {
                  app.notifications.try_insert(
                    log_level::error, [&](auto& title, auto& msg) noexcept {
                        title = "Simulation editor";
                        format(msg,
                               "Too many observers ({}) in simulation",
                               app.pj.sim.observers.capacity());
                    });
              }
          });
    });
}

} // namespace irt
