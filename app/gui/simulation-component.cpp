// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

#include "application.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/helpers.hpp"
#include "irritator/modeling.hpp"

namespace irt {

static void simulation_clear(component_editor&  ed,
                             simulation_editor& sim_ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    app.pj.clean_simulation();

    sim_ed.clear();
    sim_ed.display_graph = true;
}

static status simulation_init_grid_observation(application& app) noexcept
{
    app.pj.grid_observation_systems.resize(app.pj.grid_observers.capacity());

    for (auto& grid_obs : app.pj.grid_observers) {
        const auto id  = app.pj.grid_observers.get_id(grid_obs);
        const auto idx = get_index(id);

        if (auto ret = app.pj.grid_observation_systems[idx].init(
              app.pj, app.mod, app.sim, grid_obs);
            !ret)
            return ret.error();
    }

    return success();
}

static status simulation_init_observation(application& app) noexcept
{
    app.simulation_ed.plot_obs.init(app);
    // @TODO maybe clear project grid and graph obs ?
    for (auto& elem : app.pj.grid_observation_systems)
        elem.clear();

    // app.simulation_ed.grid_obs.clear();
    // app.simulation_ed.graph_obs.clear();

    irt_check(simulation_init_grid_observation(app));
    // irt_return_if_bad(simulation_init_graph_observation(app));

    app.sim_obs.init();

    return success();
}

template<typename S, typename... Args>
static void make_copy_error_msg(component_editor& ed,
                                const S&          fmt,
                                Args&&... args) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& n   = app.notifications.alloc(log_level::error);
    n.title   = "Component copy failed";

    auto ret = fmt::vformat_to_n(n.message.begin(),
                                 static_cast<size_t>(n.message.capacity() - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    n.message.resize(static_cast<int>(ret.size));

    app.notifications.enable(n);
}

template<typename S, typename... Args>
static void make_init_error_msg(component_editor& ed,
                                const S&          fmt,
                                Args&&... args) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& n   = app.notifications.alloc(log_level::error);
    n.title   = "Simulation initialization fail";

    auto ret = fmt::vformat_to_n(n.message.begin(),
                                 static_cast<size_t>(n.message.capacity() - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    n.message.resize(static_cast<int>(ret.size));

    app.notifications.enable(n);
}

static void simulation_copy(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    sim_ed.simulation_state = simulation_status::initializing;
    simulation_clear(ed, sim_ed);

    app.sim.clear();
    sim_ed.simulation_current = sim_ed.simulation_begin;

    auto  compo_id = app.pj.head();
    auto* compo    = app.mod.components.try_to_get(compo_id);
    auto* head     = app.pj.tn_head();
    if (!head || !compo) {
        sim_ed.simulation_state = simulation_status::not_started;
        make_copy_error_msg(ed, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          irt_check(app.pj.set(app.mod, app.sim, *compo));
          irt_check(app.mod.srcs.prepare());
          irt_check(app.sim.initialize(sim_ed.simulation_begin));
          sim_ed.simulation_state = simulation_status::initialized;
          return success();
      },
      [&](const project::part s) noexcept -> void {
          sim_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(ed, "Error {} in project copy", ordinal(s));
      },
      [&](const simulation::part s) noexcept -> void {
          sim_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(ed, "Error {} in simulation copy", ordinal(s));
      },

      [&]() noexcept -> void {
          sim_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(ed, "Unknown error");
      });
}

static void simulation_init(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    sim_ed.simulation_state   = simulation_status::initializing;
    sim_ed.simulation_current = sim_ed.simulation_begin;

    sim_ed.tl.reset();

    auto* head = app.pj.tn_head();
    if (!head) {
        sim_ed.simulation_state = simulation_status::not_started;
        make_init_error_msg(ed, "Empty component");
        return;
    }

    attempt_all(
      [&]() noexcept -> status {
          irt_check(simulation_init_observation(app));
          irt_check(app.mod.srcs.prepare());
          irt_check(app.sim.initialize(sim_ed.simulation_begin));
          sim_ed.simulation_state = simulation_status::initialized;
          return success();
      },

      [&](const simulation::part s) noexcept -> void {
          sim_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(ed, "Error in simulation: {}", ordinal(s));

          // make_init_error_msg(
          //   ed, "Fail to initalize external sources: {}",
          //   status_string(ret));
          // sim_ed.simulation_state = simulation_status::not_started;

          // make_init_error_msg(
          //   ed, "Models initialization models: {}", status_string(ret));
          // sim_ed.simulation_state = simulation_status::not_started;
      },

      [&]() noexcept -> void {
          sim_ed.simulation_state = simulation_status::not_started;
          make_copy_error_msg(ed, "Unknown error");
      });
}

static status debug_run(simulation_editor& sim_ed) noexcept
{
    auto& app = container_of(&sim_ed, &application::simulation_ed);

    if (auto ret = run(sim_ed.tl, app.sim, sim_ed.simulation_current); !ret) {
        auto& app = container_of(&sim_ed, &application::simulation_ed);
        auto& n   = app.notifications.alloc(log_level::error);
        n.title   = "Debug run error";
        app.notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret.error();
    }

    return success();
}

static status run(simulation_editor& sim_ed) noexcept
{
    auto& app = container_of(&sim_ed, &application::simulation_ed);

    if (auto ret = app.sim.run(sim_ed.simulation_current); !ret) {
        auto& app = container_of(&sim_ed, &application::simulation_ed);
        auto& n   = app.notifications.alloc(log_level::error);
        n.title   = "Run error";
        app.notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret.error();
    }

    return success();
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
                    return;
                }
            } else {
                if (auto ret = run(app.simulation_ed); !ret) {
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    return;
                }
            }

            if (!app.sim.immediate_observers.empty()) {
                app.sim_obs.update();
            }

            if (!app.simulation_ed.infinity_simulation &&
                app.simulation_ed.simulation_current >=
                  app.simulation_ed.simulation_end) {
                app.simulation_ed.simulation_current =
                  app.simulation_ed.simulation_end;
                app.simulation_ed.simulation_state =
                  simulation_status::finish_requiring;
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

void simulation_editor::start_simulation_live_run() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        app.simulation_ed.simulation_state = simulation_status::running;
        namespace stdc                     = std::chrono;

        const auto max_sim_duration =
          static_cast<real>(simulation_editor::thread_frame_duration) /
          static_cast<real>(app.simulation_ed.simulation_real_time_relation);

        const auto sim_start_at = app.simulation_ed.simulation_current;
        auto       start_at     = stdc::high_resolution_clock::now();
        auto       end_at       = stdc::high_resolution_clock::now();
        auto       duration     = end_at - start_at;

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
                    return;
                }
            } else {
                if (auto ret = run(app.simulation_ed); !ret) {
                    app.simulation_ed.simulation_state =
                      simulation_status::finish_requiring;
                    return;
                }
            }

            if (!app.sim.immediate_observers.empty())
                app.sim_obs.update();

            const auto sim_end_at   = app.simulation_ed.simulation_current;
            const auto sim_duration = sim_end_at - sim_start_at;

            if (!app.simulation_ed.infinity_simulation &&
                app.simulation_ed.simulation_current >=
                  app.simulation_ed.simulation_end) {
                app.simulation_ed.simulation_current =
                  app.simulation_ed.simulation_end;
                app.simulation_ed.simulation_state =
                  simulation_status::finish_requiring;
                return;
            }

            end_at        = stdc::high_resolution_clock::now();
            duration      = end_at - start_at;
            duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();

            if (sim_duration > max_sim_duration) {
                auto remaining =
                  stdc::microseconds(simulation_editor::thread_frame_duration) -
                  duration_cast;

                std::this_thread::sleep_for(remaining);
            }

            stop_or_pause =
              app.simulation_ed.force_pause || app.simulation_ed.force_stop;
        } while (!stop_or_pause && duration_since_start <
                                     app.simulation_ed.thread_frame_duration);

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

void simulation_editor::start_simulation_update_state() noexcept
{
    if (simulation_state == simulation_status::paused) {
        simulation_state = simulation_status::run_requiring;
        start_simulation_start();
    }

    if (simulation_state == simulation_status::finish_requiring) {
        simulation_state = simulation_status::finishing;
        start_simulation_finish();
    }

    if (mutex.try_lock()) {
        auto       it = models_to_move.begin();
        const auto et = models_to_move.end();

        for (; it != et; ++it) {
            const auto index   = get_index(it->first);
            const auto index_i = static_cast<int>(index);

            ImNodes::SetNodeScreenSpacePos(index_i, it->second);
        }

        mutex.unlock();
    }
}

void simulation_editor::start_simulation_copy_modeling() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    irt_assert(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);

        auto* modeling_head = app.pj.tn_head();
        if (!modeling_head) {
            auto& notif = app.notifications.alloc(log_level::error);
            notif.title = "Empty model";
            app.notifications.enable(notif);
        } else {
            start_simulation_delete();

            app.add_simulation_task([&app]() noexcept {
                app.simulation_ed.force_pause = false;
                app.simulation_ed.force_stop  = false;
                simulation_copy(app.component_ed, app.simulation_ed);
            });
        }
    }
}

void simulation_editor::start_simulation_init() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    irt_assert(state);

    if (state) {
        auto& app = container_of(this, &application::simulation_ed);
        app.add_simulation_task([&app]() noexcept {
            app.simulation_ed.force_pause = false;
            app.simulation_ed.force_stop  = false;
            simulation_init(app.component_ed, app.simulation_ed);
        });
    }
}

void simulation_editor::start_simulation_delete() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    app.simulation_ed.display_graph = false;

    app.add_simulation_task([&app]() noexcept {
        std::scoped_lock lock(app.simulation_ed.mutex);
        app.pj.clean_simulation();
        app.simulation_ed.clear();
    });
}

void simulation_editor::start_simulation_start() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::pause_forced);

    irt_assert(state);

    if (state) {
        if (real_time) {
            start_simulation_live_run();
        } else {
            start_simulation_static_run();
        }
    }
}

void simulation_editor::start_simulation_start_1() noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::pause_forced,
                           simulation_status::debugged);

    irt_assert(state);

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

                if (!app.simulation_ed.infinity_simulation &&
                    app.simulation_ed.simulation_current >=
                      app.simulation_ed.simulation_end) {
                    app.simulation_ed.simulation_current =
                      app.simulation_ed.simulation_end;
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

    irt_assert(state);

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

    irt_assert(state);

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
        std::scoped_lock lock{ app.simulation_ed.mutex };

        app.simulation_ed.simulation_state = simulation_status::finishing;
        app.sim.immediate_observers.clear();
        app.sim_obs.update();

        if (app.simulation_ed.store_all_changes) {
            if (auto ret = finalize(app.simulation_ed.tl,
                                    app.sim,
                                    app.simulation_ed.simulation_current);
                !ret) {
                auto& n = app.notifications.alloc();
                n.title = "Simulation finalizing fail (with store all "
                          "changes option)";
                app.notifications.enable(n);
            }
        } else {
            if (auto ret = app.sim.finalize(app.simulation_ed.simulation_end);
                !ret) {
                auto& n = app.notifications.alloc();
                n.title = "simulation finish fail";
                app.notifications.enable(n);
            }
        };
    });
}

void simulation_editor::start_simulation_advance() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app]() noexcept {
        if (app.simulation_ed.tl.can_advance()) {
            attempt_all(
              [&]() noexcept -> status {
                  return advance(app.simulation_ed.tl,
                                 app.sim,
                                 app.simulation_ed.simulation_current);
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
                  return back(app.simulation_ed.tl,
                              app.sim,
                              app.simulation_ed.simulation_current);
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
              irt_check(initialize(app.simulation_ed.tl,
                                   app.sim,
                                   app.simulation_ed.simulation_current));

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
        std::scoped_lock lock{ app.simulation_ed.mutex };

        if (!app.sim.can_alloc(1)) {
            auto& n = app.notifications.alloc(log_level::error);
            n.title = "To many model in simulation editor";
            app.notifications.enable(n);
            return;
        }

        auto& mdl    = app.sim.alloc(type);
        auto  mdl_id = app.sim.models.get_id(mdl);

        attempt_all(
          [&]() noexcept -> status {
              irt_check(app.sim.make_initialize(
                mdl, app.simulation_ed.simulation_current));

              app.simulation_ed.models_to_move.emplace_enqueue(mdl_id,
                                                               ImVec2(x, y));

              return success();
          },

          [&](const simulation::part s) noexcept -> void {
              app.sim.deallocate(mdl_id);

              auto& n = app.notifications.alloc(log_level::error);
              n.title = "Fail to initialize model";
              format(n.message, "Error: {}", ordinal(s));
              app.notifications.enable(n);
          },

          [&]() noexcept -> void {
              app.sim.deallocate(mdl_id);

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
        std::scoped_lock lock{ app.simulation_ed.mutex };
        app.sim.deallocate(id);
    });
}

void simulation_editor::remove_simulation_observation_from(
  model_id mdl_id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, mdl_id]() noexcept {
        std::scoped_lock _(app.simulation_ed.mutex);

        if_data_exists_do(
          app.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              app.sim.unobserve(mdl);
          });
    });
}

void simulation_editor::add_simulation_observation_for(
  const model_id mdl_id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    app.add_simulation_task([&app, mdl_id]() noexcept {
        std::scoped_lock _(app.simulation_ed.mutex);

        if_data_exists_do(
          app.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
              if (app.sim.observers.can_alloc(1)) {
                  auto& obs = app.sim.observers.alloc("new", 0, 0);
                  app.sim.observe(mdl, obs);
              } else {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Too many observer in simulation";
                  app.notifications.enable(n);
              }
          });
    });
}

} // namespace irt
