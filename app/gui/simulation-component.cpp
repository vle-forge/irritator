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

static status simulation_init_observation(modeling& mod, project& pj) noexcept
{
    for (auto& grid_obs : pj.grid_observers)
        grid_obs.init(pj, mod, pj.sim);

    for (auto& graph_obs : pj.graph_observers)
        graph_obs.init(pj, mod, pj.sim);

    for (auto& v_obs : pj.variable_observers)
        irt_check(v_obs.init(pj, pj.sim));

    pj.file_obs.initialize(pj.sim, pj);

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

static void simulation_copy(application& app, project_editor& ed) noexcept
{
    ed.simulation_state = simulation_status::initializing;

    auto  compo_id = ed.pj.head();
    auto* compo    = app.mod.components.try_to_get(compo_id);
    auto* head     = ed.pj.tn_head();

    if (!head || !compo) {
        ed.simulation_state = simulation_status::not_started;
        make_copy_error_msg(app, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          irt_check(ed.pj.set(app.mod, *compo));
          irt_check(ed.pj.sim.srcs.prepare());
          ed.pj.sim.t = ed.pj.t_limit.begin();
          irt_check(ed.pj.sim.initialize());
          ed.simulation_state = simulation_status::initialized;
          return success();
      },

      [&](const project::part s) noexcept -> void {
          ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error {} in project copy", ordinal(s));
      },

      [&](const simulation::part s) noexcept -> void {
          ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error {} in simulation copy", ordinal(s));
      },

      [&]() noexcept -> void {
          ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Unknown error");
      });
}

static void simulation_init(application& app, project_editor& ed) noexcept
{
    ed.simulation_state = simulation_status::initializing;

    ed.tl.reset();

    auto* head = ed.pj.tn_head();
    if (!head) {
        ed.simulation_state = simulation_status::not_started;
        make_init_error_msg(app, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          ed.pj.sim.t                   = ed.pj.t_limit.begin();
          ed.simulation_last_finite_t   = ed.pj.sim.t;
          ed.simulation_display_current = ed.pj.sim.t;

          irt_check(simulation_init_observation(app.mod, ed.pj));
          irt_check(ed.pj.sim.srcs.prepare());
          irt_check(ed.pj.sim.initialize());
          ed.simulation_state = simulation_status::initialized;
          return success();
      },

      [&](const simulation::part s) noexcept -> void {
          ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Error in simulation: {}", ordinal(s));
      },

      [&]() noexcept -> void {
          ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(app, "Unknown error");
      });
}

static bool debug_run(application& app, project_editor& sim_ed) noexcept
{
    auto success = true;

    attempt_all(
      [&] { return run(sim_ed.tl, sim_ed.pj.sim, sim_ed.pj.sim.t); },

      [&](simulation::part type, simulation::model_error* ptr) noexcept {
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
          sim_ed.simulation_state = simulation_status::finish_requiring;
          success                 = false;

          app.notifications.try_insert(
            log_level::error, [&](auto& t, auto& /*msg*/) noexcept {
                t = "Simulation debug task run error";
            });
      });

    return success;
}

static bool run(application& app, project_editor& ed) noexcept
{
    auto success = true;

    attempt_all(
      [&] { return ed.pj.sim.run(); },

      [&](simulation::part type, simulation::model_error* ptr) noexcept {
          ed.simulation_state = simulation_status::finish_requiring;
          success             = false;

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
          ed.simulation_state = simulation_status::finish_requiring;
          success             = false;

          app.notifications.try_insert(log_level::error,
                                       [&](auto& t, auto& /*msg*/) noexcept {
                                           t = "Simulation  task run error";
                                       });
      });

    return success;
}

void project_editor::start_simulation_update_state(application& app) noexcept
{
    if (any_equal(simulation_state,
                  simulation_status::paused,
                  simulation_status::run_requiring)) {

        if (have_send_message) {
            const auto mdl_id = *have_send_message;
            const auto t      = irt::time_domain<time>::is_infinity(pj.sim.t)
                                  ? pj.sim.last_valid_t
                                  : pj.sim.t;

            if_data_exists_do(pj.sim.models, mdl_id, [&](auto& m) noexcept {
                if (m.type == dynamics_type::constant) {
                    if (m.handle == invalid_heap_handle) {
                        pj.sim.sched.alloc(m, mdl_id, t);
                    } else {
                        if (pj.sim.sched.is_in_tree(m.handle)) {
                            pj.sim.sched.update(m, t);
                        } else {
                            pj.sim.sched.reintegrate(m, t);
                        }
                    }

                    m.tn = t;
                }
            });

            have_send_message.reset();
        }

        simulation_state = simulation_status::run_requiring;

        if (real_time)
            start_simulation_live_run(app);
        else
            start_simulation_static_run(app);
    }

    if (simulation_state == simulation_status::finish_requiring) {
        simulation_state = simulation_status::finishing;
        start_simulation_finish(app);
    }

    auto       it = models_to_move.begin();
    const auto et = models_to_move.end();

    for (; it != et; ++it) {
        const auto index   = get_index(it->first);
        const auto index_i = static_cast<int>(index);

        ImNodes::SetNodeScreenSpacePos(index_i, it->second);
    }
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

void project_editor::start_simulation_copy_modeling(application& app) noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    if (state) {
        auto* modeling_head = pj.tn_head();
        if (!modeling_head) {
            auto& notif = app.notifications.alloc(log_level::error);
            notif.title = "Empty model";
            app.notifications.enable(notif);
        } else {
            force_pause = false;
            force_stop  = false;

            start_simulation_clear(app);

            app.add_simulation_task(
              [&]() noexcept { simulation_copy(app, *this); });

            start_simulation_init(app);
        }
    }
}

void project_editor::start_simulation_init(application& app) noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    if (state) {
        app.add_simulation_task([&]() noexcept {
            force_pause = false;
            force_stop  = false;
            simulation_init(app, *this);
        });
    }
}

void project_editor::start_simulation_delete(application& app) noexcept
{
    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    display_graph = false;

    app.add_simulation_task([&]() noexcept {
        std::scoped_lock lock(app.sim_mutex);
        pj.clear();
        pj.sim.clear();
    });
}

void project_editor::start_simulation_clear(application& app) noexcept
{
    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    display_graph = false;

    app.add_simulation_task([&]() noexcept {
        std::scoped_lock lock(app.sim_mutex);
        pj.sim.clear();
    });
}

void project_editor::start_simulation_start(application& app) noexcept
{
    const auto state = any_equal(simulation_state,
                                 simulation_status::initialized,
                                 simulation_status::pause_forced,
                                 simulation_status::run_requiring);

    debug::ensure(state);

    if (state) {
        start = std::chrono::high_resolution_clock::now();

        if (real_time)
            start_simulation_live_run(app);
        else
            start_simulation_static_run(app);
    }
}

void project_editor::start_simulation_observation(application& app) noexcept
{
    auto& task_list = app.get_unordered_task_list(0);

    debug::ensure(simulation_state != simulation_status::finished);

    constexpr int capacity = 255;
    int           obs_max  = pj.sim.immediate_observers.ssize();
    int           current  = 0;

    while (obs_max > 0) {
        int loop = std::min(obs_max, capacity);

        for (int i = 0; i != loop; ++i) {
            auto obs_id = pj.sim.immediate_observers[i + current];

            task_list.add([&, obs_id]() noexcept {
                if_data_exists_do(pj.sim.observers,
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
    for (auto& g : pj.grid_observers) {
        const auto g_id = pj.grid_observers.get_id(g);
        task_list.add([&, g_id]() noexcept {
            if (auto* g = pj.grid_observers.try_to_get(g_id); g)
                if (g->can_update(pj.sim.t))
                    g->update(pj.sim);
        });

        ++current;
        if (current == obs_max) {
            task_list.submit();
            task_list.wait();
            current = 0;
        }
    }

    for (auto& g : pj.graph_observers) {
        const auto g_id = pj.graph_observers.get_id(g);
        task_list.add([&, g_id]() noexcept {
            if (auto* g = pj.graph_observers.try_to_get(g_id); g)
                if (g->can_update(pj.sim.t))
                    g->update(pj.sim);
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

    if (pj.file_obs.can_update(pj.sim.t))
        pj.file_obs.update(pj.sim, pj);
}

void project_editor::stop_simulation_observation(application& app) noexcept
{
    auto& task_list = app.get_unordered_task_list(0);

    debug::ensure(simulation_state == simulation_status::finishing);

    constexpr int capacity = 255;
    int           obs_max  = pj.sim.observers.ssize();
    observer*     obs      = nullptr;

    while (pj.sim.observers.next(obs)) {
        int loop = std::min(obs_max, capacity);

        for (int i = 0; i != loop; ++i) {
            auto obs_id = pj.sim.observers.get_id(*obs);
            pj.sim.observers.next(obs);

            task_list.add([&, obs_id]() noexcept {
                if_data_exists_do(pj.sim.observers,
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

    pj.file_obs.finalize();
}

void project_editor::start_simulation_live_run(application& app) noexcept
{
    namespace stdc = std::chrono;

    app.add_simulation_task([&]() noexcept {
        simulation_state         = simulation_status::running;
        const auto start_task_rt = stdc::high_resolution_clock::now();
        const auto end_task_rt   = start_task_rt + simulation_task_duration;

        for (;;) {
            if (is_simulation_state_not_running(simulation_state) or
                is_simulation_force_pause(simulation_state, force_pause) or
                is_simulation_force_stop(simulation_state, force_stop))
                return;

            time sim_t;
            time sim_next_t;

            {
                std::scoped_lock lock(app.sim_mutex);

                sim_t      = pj.sim.t;
                sim_next_t = pj.sim.sched.tn();
            }

            if (time_domain<time>::is_infinity(sim_t)) {
                sim_t      = simulation_last_finite_t;
                sim_next_t = sim_t + 1.0;
            } else {
                if (time_domain<time>::is_infinity(sim_next_t)) {
                    sim_next_t = sim_t + 1.0;
                }
            }

            if (pj.file_obs.can_update(pj.sim.t))
                pj.file_obs.update(pj.sim, pj);

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
                simulation_state = simulation_status::paused;
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
                pj.sim.t                 = sim_t;

                if (store_all_changes) {
                    if (auto ret = debug_run(app, *this); !ret) {
                        simulation_state = simulation_status::finish_requiring;
                        return;
                    }
                } else {
                    if (auto ret = run(app, *this); !ret) {
                        simulation_state = simulation_status::finish_requiring;
                        return;
                    }
                }
            }

            if (time_domain<time>::is_infinity(pj.sim.t))
                simulation_last_finite_t = sim_next_t;

            start_simulation_observation(app);
        }
    });
}

void project_editor::start_simulation_static_run(application& app) noexcept
{
    app.add_simulation_task([&]() noexcept {
        simulation_state = simulation_status::running;
        namespace stdc   = std::chrono;

        auto start_at = stdc::high_resolution_clock::now();
        auto end_at   = stdc::high_resolution_clock::now();
        auto duration = end_at - start_at;

        auto duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
        auto duration_since_start = duration_cast.count();

        bool stop_or_pause;

        do {
            if (simulation_state != simulation_status::running)
                return;

            if (store_all_changes) {
                if (auto ret = debug_run(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.t;
                    return;
                }
            } else {
                if (auto ret = run(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.t;
                    return;
                }
            }

            start_simulation_observation(app);

            if (not time_domain<time>::is_infinity(pj.t_limit.begin()) &&
                pj.sim.t >= pj.t_limit.end()) {
                pj.sim.t         = pj.t_limit.end();
                simulation_state = simulation_status::finish_requiring;
                simulation_display_current = pj.sim.t;
                return;
            }

            end_at        = stdc::high_resolution_clock::now();
            duration      = end_at - start_at;
            duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();
            stop_or_pause        = force_pause || force_stop;
        } while (!stop_or_pause &&
                 duration_since_start < thread_frame_duration);

        simulation_display_current = pj.sim.t;

        if (force_pause) {
            force_pause      = false;
            simulation_state = simulation_status::pause_forced;
        } else if (force_stop) {
            force_stop       = false;
            simulation_state = simulation_status::finish_requiring;
        } else {
            simulation_state = simulation_status::paused;
        }
    });
}

void project_editor::start_simulation_start_1(application& app) noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::pause_forced,
                           simulation_status::debugged);

    debug::ensure(state);

    if (state) {
        app.add_simulation_task([&]() noexcept {
            if (auto* parent = pj.tn_head(); parent) {
                simulation_state = simulation_status::running;

                if (auto ret = debug_run(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    return;
                }

                if (pj.file_obs.can_update(pj.sim.t))
                    pj.file_obs.update(pj.sim, pj);

                if (not time_domain<time>::is_infinity(pj.t_limit.begin()) &&
                    pj.sim.t >= pj.t_limit.end()) {
                    pj.sim.t         = pj.t_limit.end();
                    simulation_state = simulation_status::finish_requiring;
                    return;
                }

                if (force_pause) {
                    force_pause      = false;
                    simulation_state = simulation_status::pause_forced;
                } else if (force_stop) {
                    force_stop       = false;
                    simulation_state = simulation_status::finish_requiring;
                } else {
                    simulation_state = simulation_status::pause_forced;
                }
            }
        });
    }
}

void project_editor::start_simulation_pause(application& app) noexcept
{
    bool state = any_equal(simulation_state, simulation_status::running);

    debug::ensure(state);

    if (state) {
        app.add_simulation_task([&]() noexcept { force_pause = true; });
    }
}

void project_editor::start_simulation_stop(application& app) noexcept
{
    bool state = any_equal(
      simulation_state, simulation_status::running, simulation_status::paused);

    debug::ensure(state);

    if (state) {
        app.add_simulation_task([&]() noexcept { force_stop = true; });
    }
}

void project_editor::start_simulation_finish(application& app) noexcept
{
    app.add_simulation_task([&]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };

        simulation_state = simulation_status::finishing;
        pj.sim.immediate_observers.clear();
        stop_simulation_observation(app);

        if (store_all_changes) {
            if (auto ret = finalize(tl, pj.sim, pj.sim.t); !ret) {
                auto& n = app.notifications.alloc();
                n.title = "Simulation finalizing fail (with store all "
                          "changes option)";
                app.notifications.enable(n);
            }
        } else {
            pj.sim.t = pj.t_limit.end();
            if (auto ret = pj.sim.finalize(); !ret) {
                auto& n = app.notifications.alloc();
                n.title = "simulation finish fail";
                app.notifications.enable(n);
            }
        };

        simulation_state = simulation_status::finished;
    });
}

void project_editor::start_simulation_advance(application& app) noexcept
{
    app.add_simulation_task([&]() noexcept {
        if (tl.can_advance()) {
            attempt_all(
              [&]() noexcept -> status {
                  return advance(tl, pj.sim, pj.sim.t);
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

void project_editor::start_simulation_back(application& app) noexcept
{
    app.add_simulation_task([&]() noexcept {
        if (tl.can_back()) {
            attempt_all(
              [&]() noexcept -> status { return back(tl, pj.sim, pj.sim.t); },

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

void project_editor::start_enable_or_disable_debug(application& app) noexcept
{
    app.add_simulation_task([&]() noexcept {
        tl.reset();

        attempt_all(
          [&]() -> status {
              irt_check(initialize(tl, pj.sim, pj.sim.t));

              return success();
          },

          [&](const simulation::part s) noexcept -> void {
              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Debug mode failed to initialize";
              format(
                n.message, "Fail to initialize the debug mode: {}", ordinal(s));
              app.notifications.enable(n);
              simulation_state = simulation_status::not_started;
          },

          [&]() noexcept -> void {
              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Debug mode failed to initialize";
              format(n.message,
                     "Fail to initialize the debug mode: Unknown error");
              app.notifications.enable(n);
              simulation_state = simulation_status::not_started;
          });
    });
}

void project_editor::start_simulation_model_add(application&        app,
                                                const dynamics_type type,
                                                const float         x,
                                                const float         y) noexcept
{
    app.add_simulation_task([&, type, x, y]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };

        if (!pj.sim.can_alloc(1)) {
            auto& n = app.notifications.alloc(log_level::error);
            n.title = "To many model in simulation editor";
            app.notifications.enable(n);
            return;
        }

        auto& mdl    = pj.sim.alloc(type);
        auto  mdl_id = pj.sim.models.get_id(mdl);

        attempt_all(
          [&]() noexcept -> status {
              irt_check(pj.sim.make_initialize(mdl, pj.sim.t));

              models_to_move.emplace_enqueue(mdl_id, ImVec2(x, y));

              return success();
          },

          [&](const simulation::part s) noexcept -> void {
              pj.sim.deallocate(mdl_id);

              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Fail to initialize model";
              format(n.message, "Error: {}", ordinal(s));
              app.notifications.enable(n);
          },

          [&]() noexcept -> void {
              pj.sim.deallocate(mdl_id);

              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Fail to initialize model";
              format(n.message, "Unknown error");
              app.notifications.enable(n);
          });
    });
}

void project_editor::start_simulation_model_del(application&   app,
                                                const model_id id) noexcept
{
    app.add_simulation_task([&, id]() noexcept {
        std::scoped_lock lock{ app.sim_mutex };
        pj.sim.deallocate(id);
    });
}

void project_editor::remove_simulation_observation_from(
  application& app,
  model_id     mdl_id) noexcept
{
    app.add_simulation_task([&, mdl_id]() noexcept {
        std::scoped_lock _(app.sim_mutex);

        if_data_exists_do(
          pj.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              pj.sim.unobserve(mdl);
          });
    });
}

void project_editor::add_simulation_observation_for(
  application&   app,
  const model_id mdl_id) noexcept
{
    app.add_simulation_task([&, mdl_id]() noexcept {
        std::scoped_lock _(app.sim_mutex);

        if_data_exists_do(
          pj.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              if (pj.sim.observers.can_alloc(1)) {
                  auto& obs = pj.sim.observers.alloc();
                  pj.sim.observe(mdl, obs);
              } else {
                  app.notifications.try_insert(
                    log_level::error, [&](auto& title, auto& msg) noexcept {
                        title = "Simulation editor";
                        format(msg,
                               "Too many observers ({}) in simulation",
                               pj.sim.observers.capacity());
                    });
              }
          });
    });
}

} // namespace irt
