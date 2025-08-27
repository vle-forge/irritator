// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
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
        if (std::ofstream ofs(path); ofs.is_open())
            write_dot_graph_simulation(ofs, sim);
    } catch (...) {
    }
}

static expected<buffered_file> save_simulation_raw_data(
  const std::string_view absolute_path,
  const bool             is_binary) noexcept
{
    try {
        std::filesystem::path path(absolute_path);
        path /= "simulation-raw.txt";

        bitflags<buffered_file_mode> options(buffered_file_mode::write);
        if (is_binary)
            options.set(buffered_file_mode::text_or_binary);

        return open_buffered_file(path, options);
    } catch (...) {
        return new_error(fs_errc::user_directory_access_fail);
    }
}

static status simulation_init_observation(modeling& mod, project& pj) noexcept
{
    for (auto& grid_obs : pj.grid_observers)
        grid_obs.init(pj, mod, pj.sim);

    for (auto& graph_obs : pj.graph_observers)
        graph_obs.init(pj, mod, pj.sim);

    for (auto& v_obs : pj.variable_observers)
        irt_check(v_obs.init(pj, pj.sim));

    pj.file_obs.initialize(pj.sim, pj, pj.get_observation_dir(mod));

    return success();
}

template<typename S>
constexpr static void make_copy_error_msg(application& app,
                                          const S&     str) noexcept
{
    app.jn.push(log_level::error, [&](auto& t, auto& m) {
        t = "Component copy failed";
        m = str;
    });
}

template<typename S, typename... Args>
constexpr static void make_copy_error_msg(application& app,
                                          const S&     fmt,
                                          Args&&... args) noexcept
{
    app.jn.push(log_level::error, [&](auto& t, auto& m) {
        t = "Component copy failed";
        format(m, fmt, std::forward<Args>(args)...);
    });
}

template<typename S, typename... Args>
static void make_init_error_msg(application& app,
                                const S&     fmt,
                                Args&&... args) noexcept
{
    app.jn.push(log_level::error, [&](auto& t, auto& m) {
        t        = "Simulation initialization fail";
        auto ret = fmt::vformat_to_n(m.begin(),
                                     static_cast<size_t>(m.capacity() - 1),
                                     fmt,
                                     fmt::make_format_args(args...));
        m.resize(static_cast<int>(ret.size));
    });
}

static void simulation_copy(application& app, project_editor& ed) noexcept
{
    ed.simulation_state = simulation_status::initializing;

    auto  compo_id = ed.pj.head();
    auto* compo    = app.mod.components.try_to_get<component>(compo_id);
    auto* head     = ed.pj.tn_head();

    if (!head || !compo) {
        ed.simulation_state = simulation_status::not_started;
        make_copy_error_msg(app, "Empty component");
        return;
    }

    auto ret = [&]() noexcept -> status {
        irt_check(ed.pj.set(app.mod, *compo));
        irt_check(ed.pj.sim.srcs.prepare());
        irt_check(ed.pj.sim.initialize());
        ed.simulation_state = simulation_status::initialized;
        return success();
    }();

    if (not ret.has_value()) {
        ed.simulation_state = simulation_status::not_started;

        switch (ret.error().cat()) {
        case category::project:
            make_copy_error_msg(app, "Error in project copy");
            break;
        case category::external_source:
            make_copy_error_msg(app, "Error {} external source preparation");
            break;
        case category::simulation:
            make_copy_error_msg(app, "Error in simulation copy");
            break;
        default:
            make_copy_error_msg(app, "Unknown copy error");
            break;
        }
    }
}

static void simulation_init(application& app, project_editor& ed) noexcept
{
    ed.simulation_state = simulation_status::initializing;

    ed.tl.reset();

    if (ed.pj.tn_head()) {
        ed.pj.sim.clean();
        ed.pj.sim.observers.clear();
        ed.simulation_last_finite_t   = ed.pj.sim.limits.begin();
        ed.simulation_display_current = ed.pj.sim.limits.begin();

        if (simulation_init_observation(app.mod, ed.pj) and
            ed.pj.sim.srcs.prepare() and ed.pj.sim.initialize()) {
            ed.simulation_state = simulation_status::initialized;
        } else {
            ed.simulation_state = simulation_status::not_started;
            make_copy_error_msg(app, "Error in initialization");
        }
    } else {
        ed.simulation_state = simulation_status::not_started;
        make_init_error_msg(app, "Empty component");
        return;
    }

    if (ed.save_simulation_raw_data != project_editor::raw_data_type::none)
        save_simulation_graph(ed.pj.sim, ed.pj.get_observation_dir(app.mod));

    if (ed.save_simulation_raw_data != project_editor::raw_data_type::none) {
        auto ret = save_simulation_raw_data(
          ed.pj.get_observation_dir(app.mod),
          ed.save_simulation_raw_data == project_editor::raw_data_type::binary);

        if (ret.has_value())
            ed.raw_ofs = std::move(ret.value());
        else {
            ed.simulation_state = simulation_status::not_started;
            make_init_error_msg(app, "Fail to open raw data file");
            ed.save_simulation_raw_data = project_editor::raw_data_type::none;
        }
    }
}

static bool debug_run(application& app, project_editor& sim_ed) noexcept
{
    if (auto ret = run(sim_ed.tl, sim_ed.pj.sim); not ret) {
        sim_ed.simulation_state = simulation_status::finish_requiring;

        app.jn.push(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg,
                   "Fail in {} with error {}",
                   ordinal(ret.error().cat()),
                   ret.error().value());
        });
        return false;
    }

    return true;
}

static void finalize_raw_obs(project_editor& ed) noexcept
{
    debug::ensure(ed.raw_ofs);
    debug::ensure(std::ferror(ed.raw_ofs.get()) == 0);

    if (ed.save_simulation_raw_data == project_editor::raw_data_type::binary) {
        for (const auto& mdl : ed.pj.sim.models) {
            dispatch(
              mdl,
              []<typename Dynamics>(const Dynamics& dyn,
                                    auto&           ofs,
                                    const auto      index,
                                    const auto      t,
                                    const auto      tl) noexcept {
                  if constexpr (has_observation_function<Dynamics>) {
                      const auto obs = dyn.observation(t, t - tl);

                      std::fwrite(&t, sizeof(t), 1, ofs.get());
                      std::fwrite(&index, sizeof(index), 1, ofs.get());
                      std::fwrite(obs.data(), obs.size(), 1, ofs.get());
                  }
              },
              ed.raw_ofs,
              get_index(ed.pj.sim.get_id(mdl)),
              ed.pj.sim.current_time(),
              mdl.tl);
        }
    } else {
        for (const auto& mdl : ed.pj.sim.models) {
            dispatch(
              mdl,
              []<typename Dynamics>(const Dynamics& dyn,
                                    auto&           ofs,
                                    const auto      index,
                                    const auto      t,
                                    const auto      tl) noexcept {
                  if constexpr (has_observation_function<Dynamics>) {
                      const auto obs = dyn.observation(t, t - tl);

                      fmt::print(ofs.get(),
                                 "{};{};{};{};{};{};{}\n",
                                 t,
                                 index,
                                 obs[0],
                                 obs[1],
                                 obs[2],
                                 obs[3],
                                 obs[4]);
                  }
              },
              ed.raw_ofs,
              get_index(ed.pj.sim.get_id(mdl)),
              ed.pj.sim.current_time(),
              mdl.tl);
        }
    }

    ed.raw_ofs.reset();
}

static bool run_raw_obs(application& app, project_editor& ed) noexcept
{
    debug::ensure(ed.raw_ofs);
    debug::ensure(std::ferror(ed.raw_ofs.get()) == 0);

    const auto ret = ed.pj.sim.run_with_cb(
      [](const auto& sim, const auto mdls, auto& ofs, auto type) noexcept {
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

                                std::fwrite(&t, sizeof(t), 1, ofs.get());
                                std::fwrite(
                                  &index, sizeof(index), 1, ofs.get());
                                std::fwrite(
                                  obs.data(), obs.size(), 1, ofs.get());
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

                                fmt::print(ofs.get(),
                                           "{};{};{};{};{};{};{}\n",
                                           t,
                                           index,
                                           obs[0],
                                           obs[1],
                                           obs[2],
                                           obs[3],
                                           obs[4]);
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
      ed.raw_ofs,
      ed.save_simulation_raw_data);

    if (ret.has_error()) {
        ed.simulation_state = simulation_status::finish_requiring;

        app.jn.push(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg,
                   "Fail in {} with error {}",
                   ordinal(ret.error().cat()),
                   ret.error().value());
        });
    }

    if (std::ferror(ed.raw_ofs.get())) {
        app.jn.push(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg, "Fail to write raw data to file");
        });
        ed.save_simulation_raw_data = project_editor::raw_data_type::none;
        ed.raw_ofs.reset();
    }

    return ret.has_value();
}

static bool run(application& app, project_editor& ed) noexcept
{
    if (auto ret = ed.pj.sim.run(); not ret) {
        ed.simulation_state = simulation_status::finish_requiring;

        app.jn.push(log_level::error, [&](auto& t, auto& msg) noexcept {
            t = "Simulation debug task run error";
            format(msg,
                   "Fail in {} with error {}",
                   ordinal(ret.error().cat()),
                   ret.error().value());
        });
        return false;
    }

    return true;
}

static int new_model(application&                app,
                     project_editor&             pj_ed,
                     const command::new_model_t& data) noexcept
{
    int rebuild = false;

    if (not pj_ed.pj.sim.can_alloc(1)) {
        app.jn.push(log_level::error, [](auto& title, auto&) noexcept {
            title = "Internal error: fail to initialize new model.";
        });
    } else {
        auto& mdl = pj_ed.pj.sim.alloc(data.type);
        (void)pj_ed.pj.sim.make_initialize(mdl, pj_ed.pj.sim.current_time());

        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) {
            tn->children.push_back(tree_node::child_node{
              .mdl  = pj_ed.pj.sim.get_id(mdl),
              .type = tree_node::child_node::type::model });
        }
        ++rebuild;
    }

    return rebuild;
}

static int free_model(application& /*app*/,
                      project_editor&              pj_ed,
                      const command::free_model_t& data) noexcept
{
    if (pj_ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) {
            for (auto i = 0, e = tn->children.ssize(); i < e; ++i) {
                if (tn->children[i].type ==
                      tree_node::child_node::type::model and
                    tn->children[i].mdl == data.mdl_id) {
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
            app.jn.push(log_level::error, [](auto& title, auto&) noexcept {
                title = "Internal error: fail to allocate more models.";
            });

            return 0;
        }

        auto& dst_mdl = pj_ed.pj.sim.clone(*src_mdl);

        if (not pj_ed.pj.sim.make_initialize(dst_mdl,
                                             pj_ed.pj.sim.current_time())) {
            app.jn.push(log_level::error, [](auto& title, auto&) noexcept {
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
        app.jn.push(log_level::error, [](auto& title, auto&) noexcept {
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
                    app.jn.push(log_level::error,
                                [](auto& title, auto&) noexcept {
                                    title = "Internal error: fail to buid new "
                                            "connection.";
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
            app.jn.push(log_level::error, [](auto& title, auto&) noexcept {
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
            app.jn.push(log_level::error,
                        [&](auto& title, auto& /*msg*/) noexcept {
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
        app.jn.push(log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
            title = "Internal error: fail to delete observer.";
        });
    }
}

static void send_message(application&                   app,
                         project_editor&                ed,
                         const command::send_message_t& data) noexcept
{
    const auto t = irt::time_domain<time>::is_infinity(ed.pj.sim.current_time())
                     ? ed.pj.sim.last_time()
                     : ed.pj.sim.current_time();

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

    app.jn.push(log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
        title = "Internal error: fail to send message.";
    });
}

void start_simulation_commands_apply(application& app, project_id id) noexcept
{
    app.add_simulation_task(id, [&app, id]() noexcept {
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
            app.jn.push(log_level::error,
                        [](auto& t, auto&) { t = "Empty model"; });
        } else {
            force_pause = false;
            force_stop  = false;

            start_simulation_clear(app);

            app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
                simulation_copy(app, *this);
            });

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
        app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
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

    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        pj.clear();
        pj.sim.clear();
    });
}

void project_editor::start_simulation_clear(application& app) noexcept
{
    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    display_graph = false;

    app.add_simulation_task(app.pjs.get_id(*this),
                            [&]() noexcept { pj.sim.clear(); });
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
    auto& task_list =
      app.get_unordered_task_list(get_index(app.pjs.get_id(*this)));

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
                if (g->can_update(pj.sim.current_time()))
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
                if (g->can_update(pj.sim.current_time()))
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

    if (pj.file_obs.can_update(pj.sim.current_time()))
        pj.file_obs.update(pj.sim, pj);
}

void project_editor::stop_simulation_observation(application& app) noexcept
{
    auto& task_list =
      app.get_unordered_task_list(get_index(app.pjs.get_id(*this)));

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

    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
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

            sim_t      = pj.sim.current_time();
            sim_next_t = pj.sim.sched.tn();

            if (time_domain<time>::is_infinity(sim_t)) {
                sim_t      = simulation_last_finite_t;
                sim_next_t = sim_t + 1.0;
            } else {
                if (time_domain<time>::is_infinity(sim_next_t)) {
                    sim_next_t = sim_t + 1.0;
                }
            }

            if (pj.file_obs.can_update(pj.sim.current_time()))
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
            pj.sim.current_time(sim_t);

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

            if (time_domain<time>::is_infinity(pj.sim.current_time()))
                simulation_last_finite_t = sim_next_t;

            start_simulation_observation(app);
        }
    });
}

void project_editor::start_simulation_static_run(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
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

            if (save_simulation_raw_data !=
                project_editor::raw_data_type::none) {
                if (auto ret = run_raw_obs(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.current_time();
                    return;
                }
            } else if (store_all_changes) {
                if (auto ret = debug_run(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.current_time();
                    return;
                }
            } else if (auto ret = run(app, *this); !ret) {
                simulation_state = simulation_status::finish_requiring;
                simulation_display_current = pj.sim.current_time();
                return;
            }

            start_simulation_observation(app);

            if (pj.sim.current_time_expired()) {
                simulation_state = simulation_status::finish_requiring;
                simulation_display_current = pj.sim.current_time();
                return;
            }

            end_at        = stdc::high_resolution_clock::now();
            duration      = end_at - start_at;
            duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();
            stop_or_pause        = force_pause || force_stop;
        } while (!stop_or_pause &&
                 duration_since_start < thread_frame_duration);

        simulation_display_current = pj.sim.current_time();

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
        app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
            if (auto* parent = pj.tn_head(); parent) {
                simulation_state = simulation_status::running;

                if (auto ret = debug_run(app, *this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    return;
                }

                if (pj.file_obs.can_update(pj.sim.current_time()))
                    pj.file_obs.update(pj.sim, pj);

                if (pj.sim.current_time_expired()) {
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
        app.add_simulation_task(app.pjs.get_id(*this),
                                [&]() noexcept { force_pause = true; });
    }
}

void project_editor::start_simulation_stop(application& app) noexcept
{
    bool state = any_equal(
      simulation_state, simulation_status::running, simulation_status::paused);

    debug::ensure(state);

    if (state) {
        app.add_simulation_task(app.pjs.get_id(*this),
                                [&]() noexcept { force_stop = true; });
    }
}

void project_editor::start_simulation_finish(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        simulation_state = simulation_status::finishing;
        pj.sim.immediate_observers.clear();

        if (store_all_changes) {
            if (auto ret = finalize(tl, pj.sim); !ret) {
                app.jn.push(log_level::error, [](auto& t, auto& m) {
                    t = "Simulation finalizing fail (with store all changes "
                        "option)";
                    m = "FIXME from ret";
                });
            } else {
                stop_simulation_observation(app);
            }
        } else {
            if (auto ret = pj.sim.finalize(); !ret) {
                app.jn.push(log_level::error, [](auto& t, auto& m) {
                    t = "Simulation finalizing fail";
                    m = "FIXME from ret";
                });
            } else {
                stop_simulation_observation(app);
            }

            if (save_simulation_raw_data !=
                project_editor::raw_data_type::none) {
                finalize_raw_obs(*this);
            }
        };

        simulation_state = simulation_status::finished;
    });
}

void project_editor::start_simulation_advance(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        if (tl.can_advance()) {
            if (auto ret = advance(tl, pj.sim); not ret) {
                if (ret.error().cat() == category::simulation) {
                    app.jn.push(log_level::error, [](auto& t, auto& m) {
                        t = "Fail to advance the simulation";
                        format(m, "Advance message");
                    });
                } else {
                    app.jn.push(log_level::error, [](auto& t, auto& m) {
                        t = "Fail to advance the simulation";
                        format(m, "Unknwon message");
                    });
                }
            }
        }
    });
}

void project_editor::start_simulation_back(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        if (tl.can_back()) {
            if (auto ret = back(tl, pj.sim); not ret) {
                if (ret.error().cat() == category::simulation) {
                    app.jn.push(log_level::error, [](auto& t, auto& m) {
                        t = "Fail to advance the simulation";
                        format(m, "Advance message");
                    });
                } else {
                    app.jn.push(log_level::error, [](auto& t, auto& m) {
                        t = "Fail to advance the simulation";
                        format(m, "Unknwon message");
                    });
                }
            }
        }
    });
}

void project_editor::start_enable_or_disable_debug(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        tl.reset();

        if (auto ret = initialize(tl, pj.sim); not ret) {
            simulation_state = simulation_status::not_started;

            if (ret.error().cat() == category::simulation) {
                app.jn.push(log_level::error, [&ret](auto& t, auto& m) {
                    t = "Debug mode failed to initialize";
                    format(m,
                           "Fail to initialize the debug mode: {}",
                           ret.error().value());
                });
            } else {
                app.jn.push(log_level::error, [](auto& t, auto& m) {
                    t = "Debug mode failed to initialize";
                    format(m,
                           "Fail to initialize the debug mode: Unknown error");
                });
            }
        }
    });
}

} // namespace irt
