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

namespace irt {

void project::save_simulation_graph(
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

status project::simulation_init_observation(const modeling& mod) noexcept
{
    for (auto& grid_obs : grid_observers)
        grid_obs.init(*this, mod);

    for (auto& graph_obs : graph_observers)
        graph_obs.init(*this, mod);

    for (auto& v_obs : variable_observers)
        irt_check(v_obs.init(*this));

    if (const auto path = get_observation_dir(mod); path.has_value())
        file_obs.initialize(*this, path->string());

    return success();
}

status project::simulation_copy(const modeling& mod) noexcept
{
    simulation_state = simulation_status::initializing;

    const auto ret = mod.ids.read([&](const auto& ids, auto) -> status {
        return mod.files.read([&](const auto& fs, auto) -> status {
            irt_check(set(ids, fs, head()));
            irt_check(sim.srcs.prepare());
            irt_check(sim.initialize());
            simulation_state = simulation_status::initialized;
            return success();
        });
    });

    if (not ret.has_value()) {
        simulation_state = simulation_status::not_started;

        using namespace std::literals;

        switch (ret.error().cat()) {
        case category::project:
            log(log_level::error, "Error in project copy"sv);
            break;
        case category::external_source:
            log(log_level::error, "Error external source preparation"sv);
            break;
        case category::simulation:
            log(log_level::error, "Error in simulation copy"sv);
            break;
        default:
            log(log_level::error, "Unknown copy error"sv);
            break;
        }
    }

    return success();
}

status project::simulation_init(const modeling& mod) noexcept
{
    using namespace std::literals;

    simulation_state = simulation_status::initializing;

    if (not tree_nodes.exists(tn_head())) {
        simulation_state = simulation_status::not_started;
        log(log_level::error,
            "Error during initialization"sv,
            "The component is empty"sv);
        return make_error(project_errc::empty_project);
    }

    sim.clean();
    sim.observers.clear();

    if (auto r = simulation_init_observation(mod); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error,
            "Error during initialization"sv,
            "Observation system failed"sv);
        return r.error();
    }

    if (auto r = sim.srcs.prepare(); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error,
            "Error during initialization"sv,
            "External source system failed"sv);
        return r.error();
    }

    if (auto r = sim.initialize(); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error,
            "Error during initialization"sv,
            "Simulation system failed"sv);
        return r.error();
    }

    simulation_state = simulation_status::initialized;

    // if (ed.save_simulation_raw_data != project_editor::raw_data_type::none)
    //     if (const auto path = ed.pj.get_observation_dir(app.mod);
    //         path.has_value())
    //         save_simulation_graph(ed.pj.sim, path->string());

    // if (ed.save_simulation_raw_data != project_editor::raw_data_type::none) {
    //     if (const auto path = ed.pj.get_observation_dir(app.mod);
    //         path.has_value()) {
    //         auto ret =
    //           save_simulation_raw_data(path->string(),
    //                                    ed.save_simulation_raw_data ==
    //                                      project_editor::raw_data_type::binary);

    //         if (ret.has_value())
    //             ed.raw_ofs = std::move(ret.value());
    //         else {
    //             ed.simulation_state = simulation_status::not_started;

    //             log(log_level::error, [&](auto& t, auto& m) {
    //                 t = "Error during initialization"sv,
    //                 format(m, "Fail to open raw data file {}",
    //                 path->string());
    //             });

    //             ed.save_simulation_raw_data =
    //               project_editor::raw_data_type::none;
    //         }
    //     }
    // }

    return success();
}

static bool run(project_editor& ed) noexcept
{
    if (auto ret = ed.pj.sim.run(); not ret) {
        ed.simulation_state = simulation_status::finish_requiring;

        log(log_level::error, [&](auto& t, auto& msg) noexcept {
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

static int new_model(project_editor&             pj_ed,
                     const command::new_model_t& data) noexcept
{
    int rebuild = false;

    if (not pj_ed.pj.sim.can_alloc(1)) {
        log(log_level::error, [](auto& title, auto&) noexcept {
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

static int free_model(project_editor&              pj_ed,
                      const command::free_model_t& data) noexcept
{
    if (pj_ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id)) {
            for (sz i = 0, e = tn->children.size(); i < e; ++i) {
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

static int copy_model(project_editor&              pj_ed,
                      const command::copy_model_t& data) noexcept
{
    if (auto* src_mdl = pj_ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (not pj_ed.pj.sim.can_alloc(1)) {
            log(log_level::error, [](auto& title, auto&) noexcept {
                title = "Internal error: fail to allocate more models.";
            });

            return 0;
        }

        auto& dst_mdl = pj_ed.pj.sim.clone(*src_mdl);

        if (not pj_ed.pj.sim.make_initialize(dst_mdl,
                                             pj_ed.pj.sim.current_time())) {
            log(log_level::error, [](auto& title, auto&) noexcept {
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

static int new_connection(project_editor&                  ed,
                          const command::new_connection_t& data) noexcept
{
    int rebuild = false;

    if (not ed.pj.sim.can_connect(1)) {
        log(log_level::error, [](auto& title, auto&) noexcept {
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
                    log(log_level::error, [](auto& title, auto&) noexcept {
                        title = "Internal error: fail to buid new "
                                "connection.";
                    });
                }
            }
        }
    }

    return rebuild;
}

static int free_connection(project_editor&                   ed,
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
            log(log_level::error, [](auto& title, auto&) noexcept {
                title = "Internal error: fail to buid new connection.";
            });
        }
    }

    return false;
}

static void new_observer(project_editor&                ed,
                         const command::new_observer_t& data) noexcept

{
    if (auto* mdl = ed.pj.sim.models.try_to_get(data.mdl_id)) {
        if (ed.pj.sim.observers.can_alloc(1)) {
            ed.pj.sim.observe(*mdl);
        } else {
            log(log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
                title = "Internal error: fail to add observer.";
            });
        }
    }
}

static void free_observer(project_editor&                 ed,
                          const command::free_observer_t& data) noexcept
{
    if (auto* mdl = ed.pj.sim.models.try_to_get(data.mdl_id)) {
        ed.pj.sim.unobserve(*mdl);
    } else {
        log(log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
            title = "Internal error: fail to delete observer.";
        });
    }
}

static void send_message(project_editor&                ed,
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

    log(log_level::error, [&](auto& title, auto& /*msg*/) noexcept {
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
                        rebuild += new_model(*ed, c.data.new_model);
                        break;
                    case command_type::free_model:
                        rebuild += free_model(*ed, c.data.free_model);
                        break;
                    case command_type::copy_model:
                        rebuild += copy_model(*ed, c.data.copy_model);
                        break;
                    case command_type::new_connection:
                        rebuild += new_connection(*ed, c.data.new_connection);
                        break;
                    case command_type::free_connection:
                        rebuild += free_connection(*ed, c.data.free_connection);
                        break;
                    case command_type::new_observer:
                        new_observer(*ed, c.data.new_observer);
                        break;
                    case command_type::free_observer:
                        free_observer(*ed, c.data.free_observer);
                        break;
                    case command_type::send_message:
                        send_message(*ed, c.data.send_message);
                        break;
                    }
                }
            }

            if (rebuild)
                app.generic_sim.reinit(app, *ed);
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

void project_editor::start_simulation_copy_modeling(application& app) noexcept
{
    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    if (state) {
        auto* modeling_head = pj.tree_nodes.try_to_get(pj.tn_head());
        if (!modeling_head) {
            log(log_level::error, [](auto& t, auto&) { t = "Empty model"; });
        } else {
            force_pause = false;
            force_stop  = false;

            pj.sim.clear();

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

void project_editor::simulation_observation_for_imm_observers(
  application& app) noexcept
{
    auto& task_list = app.get_unordered_task_list();

    debug::ensure(simulation_state != simulation_status::finished);

    constexpr sz capacity = 255;
    sz           obs_max  = pj.sim.immediate_observers.size();
    sz           current  = 0;

    while (obs_max > 0) {
        const auto loop = std::min(obs_max, capacity);

        for (sz i = 0; i != loop; ++i) {
            auto obs_id = pj.sim.immediate_observers[i + current];

            task_list.add([&, obs_id]() noexcept {
                if (auto* obs = pj.sim.observers.try_to_get(obs_id)) {
                    auto& res = pj.sim.observers.get<resampler>(obs_id);

                    res.tick(*obs, pj.sim.current_time());
                }
            });
        }

        task_list.submit();
        task_list.wait_completion();

        current += loop;
        if (obs_max > capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    if (current > 0) {
        task_list.submit();
        task_list.wait_completion();
    }
}

void project_editor::simulation_observation_for_all_observers(
  application& app) noexcept
{
    auto& task_list = app.get_unordered_task_list();

    debug::ensure(simulation_state != simulation_status::finished);

    constexpr sz capacity = 255;
    sz           obs_max  = pj.sim.observers.ssize();
    sz           current  = 0;

    auto it = pj.sim.observers.begin();
    auto et = pj.sim.observers.end();

    while (it != et) {
        const auto loop = std::min(obs_max, capacity);

        for (sz i = 0; i != loop; ++i) {
            const auto obs_id = pj.sim.observers.get_id(*it);

            task_list.add([&, obs_id]() noexcept {
                if (auto* obs = pj.sim.observers.try_to_get(obs_id)) {
                    auto& res = pj.sim.observers.get<resampler>(obs_id);

                    res.tick(*obs, pj.sim.current_time());
                }
            });

            ++it;
        }

        task_list.submit();
        task_list.wait_completion();

        current += loop;
        if (obs_max >= capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    if (current > 0) {
        task_list.submit();
        task_list.wait_completion();
    }
}

void project_editor::start_simulation_live_run(application& app) noexcept
{
    namespace stdc = std::chrono;

    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        simulation_state         = simulation_status::running;
        const auto start_task_rt = stdc::high_resolution_clock::now();
        const auto end_task_rt   = start_task_rt + simulation_task_duration;

        for (;;) {
            if (simulation_state != simulation_status::running)
                return;

            if (force_pause) {
                simulation_state = simulation_status::pause_forced;
                return;
            }

            if (force_stop) {
                simulation_state = simulation_status::finish_requiring;
                return;
            }

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

            if (store_all_changes)
                snaps.emplace_back(pj.sim);

            if (auto ret = run(*this); !ret) {
                simulation_state = simulation_status::finish_requiring;
                return;
            }

            if (not pj.sim.immediate_observers.empty())
                simulation_observation_for_imm_observers(app);

            if (time_domain<time>::is_infinity(pj.sim.current_time()))
                simulation_last_finite_t = sim_next_t;
        }

        simulation_observation_for_all_observers(app);
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
                if (auto ret = run_raw_obs(*this); !ret) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.current_time();
                    return;
                }
            } else {
                if (store_all_changes)
                    snaps.emplace_back(pj.sim);

                if (not run(*this)) {
                    simulation_state = simulation_status::finish_requiring;
                    simulation_display_current = pj.sim.current_time();
                    return;
                }
            }

            if (pj.sim.immediate_observers.empty())
                simulation_observation_for_imm_observers(app);

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

        simulation_observation_for_all_observers(app);
    });
}

void project_editor::start_simulation_step_by_step(application& app) noexcept
{
    const auto state = any_equal(simulation_state,
                                 simulation_status::initialized,
                                 simulation_status::pause_forced,
                                 simulation_status::debugged);

    if (state) {
        app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
            if (pj.tree_nodes.try_to_get(pj.tn_head())) {
                simulation_state = simulation_status::running;

                const auto current_time = pj.sim.current_time();

                if (not run(*this)) {
                    simulation_state = simulation_status::finish_requiring;
                    return;
                }

                if (current_time != pj.sim.current_time())
                    snaps.emplace_back(pj.sim);

                if (pj.file_obs.can_update(pj.sim.current_time()))
                    pj.file_obs.update(pj);

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

                simulation_observation_for_all_observers(app);
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

        if (store_all_changes)
            snaps.emplace_back(pj.sim);

        if (pj.sim.finalize().has_error()) {
            log(log_level::error, [](auto& t, auto& m) {
                t = "Simulation finalizing fail";
                m = "FIXME from ret";
            });
        } else {
            simulation_observation_for_all_observers(app);
        }

        if (save_simulation_raw_data != project_editor::raw_data_type::none) {
            finalize_raw_obs(*this);
        }

        simulation_state = simulation_status::finished;
    });
}

void project_editor::start_simulation_advance(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        if (snaps.empty())
            return;

        if (current_snap >= 0) {
            const auto* snap = snaps.ptr_from_index(current_snap);
            if (snaps.previous(snap)) {
                pj.sim       = *snap;
                current_snap = snaps.index_of(snap);
            }
        }
    });
}

void project_editor::start_simulation_back(application& app) noexcept
{
    app.add_simulation_task(app.pjs.get_id(*this), [&]() noexcept {
        if (snaps.empty())
            return;

        if (current_snap >= 0) {
            const auto* snap = snaps.ptr_from_index(current_snap);
            if (snaps.next(snap)) {
                pj.sim       = *snap;
                current_snap = snaps.index_of(snap);
            }
        }
    });
}

} // namespace irt
