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

static int new_model(application&                app,
                     project_editor&             pj_ed,
                     const command::new_model_t& data) noexcept
{
    int rebuild = false;

    if (not pj_ed.pj.sim.can_alloc(1)) {
        app.notifications.try_insert(
          log_level::error, [](auto& title, auto&) noexcept {
              title = "Internal error: fail to initialize new model.";
          });
    } else {
        auto& mdl = pj_ed.pj.sim.alloc(data.type);
        (void)pj_ed.pj.sim.make_initialize(mdl, pj_ed.pj.sim.t);

        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) {
            tn->children.push_back(tree_node::child_node{
              .mdl = &mdl, .type = tree_node::child_node::type::model });
        }
        ++rebuild;
    }

    return rebuild;
}

static int free_model(application& /*app*/,
                      project_editor&              pj_ed,
                      const command::free_model_t& data) noexcept
{
    if (auto* mdl = pj_ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) {
            for (auto i = 0, e = tn->children.ssize(); i < e; ++i) {
                if (tn->children[i].type ==
                      tree_node::child_node::type::model and
                    tn->children[i].mdl == mdl) {
                    tn->children[i].disable();
                    break;
                }
            }

            pj_ed.pj.sim.deallocate(data.mdl_id);
            return true;
        }
    }

    return false;
}

static int copy_model(application&                 app,
                      project_editor&              pj_ed,
                      const command::copy_model_t& data) noexcept
{
    if (auto* src_mdl = pj_ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (not pj_ed.pj.sim.can_alloc(1)) {
            app.notifications.try_insert(
              log_level::error, [](auto& title, auto&) noexcept {
                  title = "Internal error: fail to allocate more models.";
              });

            return 0;
        }

        auto& dst_mdl = pj_ed.pj.sim.clone(*src_mdl);

        if (not pj_ed.pj.sim.make_initialize(dst_mdl, pj_ed.pj.sim.t)) {
            app.notifications.try_insert(
              log_level::error, [](auto& title, auto&) noexcept {
                  title = "Internal error: fail to initialize new model.";
              });

            return 0;
        }

        dispatch(*src_mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
            if constexpr (has_output_port<Dynamics>) {
                for (int i = 0, e = length(dyn.y); i != e; ++i) {
                    auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);

                    pj_ed.pj.sim.for_each(
                      dyn.y[i], [&](auto& mdl_src, int port_src) {
                          const auto mdl_src_id = pj_ed.pj.sim.get_id(mdl_src);

                          (void)pj_ed.pj.sim.connect(
                            dst_dyn.y[i], mdl_src_id, port_src);
                      });
                }

                // if (auto* tn =
                // pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) { tn->
                // }
            }
        });

        return 1;
    }

    return 0;
}

static int new_connection(application&                     app,
                          project_editor&                  ed,
                          const command::new_connection_t& data) noexcept
{
    int rebuild = false;

    if (not ed.pj.sim.can_connect(1)) {
        app.notifications.try_insert(
          log_level::error, [](auto& title, auto&) noexcept {
              title = "Internal error: fail to initialize new model.";
          });
    } else {
        if (auto* src = ed.pj.sim.models.try_to_get(data.mdl_src_id)) {
            if (auto* dst = ed.pj.sim.models.try_to_get(data.mdl_dst_id)) {
                if (!!ed.pj.sim.connect(
                      *src, data.port_src, *dst, data.port_dst)) {
                    ++rebuild;

                    // if (auto* tn =
                    // pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) { tn->
                    // }

                } else {
                    app.notifications.try_insert(
                      log_level::error, [](auto& title, auto&) noexcept {
                          title =
                            "Internal error: fail to buid new connection.";
                      });
                }
            }
        }
    }

    return rebuild;
}

static int free_connection(application&                      app,
                           project_editor&                   ed,
                           const command::free_connection_t& data) noexcept

{
    if (auto* src = ed.pj.sim.models.try_to_get(data.mdl_src_id)) {
        if (auto* dst = ed.pj.sim.models.try_to_get(data.mdl_dst_id)) {
            ed.pj.sim.disconnect(*src, data.port_src, *dst, data.port_dst);

            // if (auto* tn =
            // pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) { tn->
            // }

            return true;
        } else {
            app.notifications.try_insert(
              log_level::error, [](auto& title, auto&) noexcept {
                  title = "Internal error: fail to buid new connection.";
              });
        }
    }

    return false;
}

static void new_observer(application&                   app,
                         project_editor&                ed,
                         const command::new_observer_t& data) noexcept

{
    if (auto* mdl = ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (ed.pj.sim.observers.can_alloc(1)) {
            auto& obs = ed.pj.sim.observers.alloc();
            ed.pj.sim.observe(*mdl, obs);
        } else {
            app.notifications.try_insert(
              log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
                  title = "Internal error: fail to add observer.";
              });
        }
    }
}

static void free_observer(application&                    app,
                          project_editor&                 ed,
                          const command::free_observer_t& data) noexcept
{
    if (auto* mdl = ed.pj.sim.models.try_to_get(data.mdl_id)) {
        ed.pj.sim.unobserve(*mdl);
    } else {
        app.notifications.try_insert(
          log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
              title = "Internal error: fail to delete observer.";
          });
    }
}

static void send_message(application&                   app,
                         project_editor&                ed,
                         const command::send_message_t& data) noexcept
{
    const auto t = irt::time_domain<time>::is_infinity(ed.pj.sim.t)
                     ? ed.pj.sim.last_valid_t
                     : ed.pj.sim.t;

    if (auto* mdl = ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (mdl->type == dynamics_type::constant) {
            if (mdl->handle == invalid_heap_handle) {
                ed.pj.sim.sched.alloc(*mdl, data.mdl_id, t);
            } else {
                if (ed.pj.sim.sched.is_in_tree(mdl->handle)) {
                    ed.pj.sim.sched.update(*mdl, t);
                } else {
                    ed.pj.sim.sched.reintegrate(*mdl, t);
                }
            }

            mdl->tn = t;
            return;
        }
    }

    app.notifications.try_insert(
      log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
          title = "Internal error: fail to send message.";
      });
}

void start_simulation_commands_apply(application& app, project_id id) noexcept
{
    app.add_simulation_task([&app, id]() noexcept {
        if (auto* ed = app.pjs.try_to_get(id)) {
            int rebuild = false;

            while (not ed->commands.empty()) {
                command c;
                if (ed->commands.pop(c)) {
                    switch (c.type) {
                    case command_type::none:
                        break;
                    case command_type::new_model:
                        rebuild += new_model(app, *ed, c.data.new_model);
                        break;
                    case command_type::free_model:
                        rebuild += free_model(app, *ed, c.data.free_model);
                        break;
                    case command_type::copy_model:
                        rebuild += copy_model(app, *ed, c.data.copy_model);
                        break;
                    case command_type::new_connection:
                        rebuild +=
                          new_connection(app, *ed, c.data.new_connection);
                        break;
                    case command_type::free_connection:
                        rebuild +=
                          free_connection(app, *ed, c.data.free_connection);
                        break;
                    case command_type::new_observer:
                        new_observer(app, *ed, c.data.new_observer);
                        break;
                    case command_type::free_observer:
                        free_observer(app, *ed, c.data.free_observer);
                        break;
                    case command_type::send_message:
                        send_message(app, *ed, c.data.send_message);
                        break;
                    }
                }
            }

            if (rebuild)
                ed->generic_sim.reinit(app);
        }
    });
}

void project_editor::start_simulation_update_state(application& app) noexcept
{
    if (not commands.empty())
        start_simulation_commands_apply(app, app.pjs.get_id(*this));

    if (any_equal(simulation_state,
                  simulation_status::paused,
                  simulation_status::run_requiring)) {

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
        pj.clear();
        pj.sim.clear();
    });
}

void project_editor::start_simulation_clear(application& app) noexcept
{
    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    display_graph = false;

    app.add_simulation_task([&]() noexcept { pj.sim.clear(); });
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

            sim_t      = pj.sim.t;
            sim_next_t = pj.sim.sched.tn();

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

            // There is no real time available for this simulation task.
            // Program the next.
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

} // namespace irt
