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

    bool state = any_equal(simulation_state,
                           simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

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

status project::simulation_run() noexcept
{
    debug::ensure(simulation_state == simulation_status::running or
                  simulation_state == simulation_status::run_requiring);

    if (auto ret = sim.run(); ret.has_error()) {
        ed.simulation_state = simulation_status::finish_requiring;

        log_m(log_level::error, [&](auto& msg) noexcept {
            format(msg,
                   "Fail in {} with error {}",
                   ordinal(ret.error().cat()),
                   ret.error().value());
        });

        return ret.error();
    }

    return success();
}

status project::simulation_new_model(const command::new_model_t& data) noexcept
{
    auto* tn = tree_nodes.try_to_get(data.tn_id);

    if (not tn) [[unlikely]] {
        log_m(log_level::error, [](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", data.tn_id);
        });

        return make_error(project_errc::simulation_model_allocation_error);
    }

    const auto tn_alloc =
      tn->children.can_alloc(1) or tn->children.grow<3, 2>(1);
    const auto sim_alloc = sim.can_alloc(1) or sim.grow<3, 2>(1);

    if (not tn_alloc or not sim_alloc) {
        log_m(log_level::error, [](auto& m) noexcept {
            format(m,
                   "Fail to allocate new model in tree node {} (capacity: {}",
                   data.tn_id,
                   tn->children.capacity());
        });
        return make_error(project_errc::simulation_model_allocation_error);
    }

    auto& mdl = sim.alloc(data.type);

    if (auto ret = sim.make_initialize(mdl, sim.current_time());
        ret.has_error()) {
        log_m(log_level::error, [](auto& m) noexcept {
            format(m, "Fail to initialize new model of type {}", data.type);
        });

        return ret.error();
    }

    tn->children.push_back(tree_node::child_node{
      .mdl = sim.get_id(mdl), .type = tree_node::child_node::type::model });

    return success();
}

status project::simulation_free_model(
  const command::free_model_t& data) noexcept
{
    auto* tn = pj_ed.pj.tree_nodes.try_to_get(data.tn_id);
    if (not tn) [[unlikely]] {
        log_m(log_level::error, [](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", data.tn_id);
        });

        return make_error(project_errc::simulation_model_allocation_error);
    }

    auto* mdl = pj_ed.pj.sim.models.try_to_get(data.mdl_id);
    if (not mdl) [[unlikely]] {
        log_m(log_level::error, [](auto& m) noexcept {
            format(m, "Fail to find model with ID {}", data.mdl_id);
        });

        return make_error(project_errc::simulation_model_allocation_error);
    }

    for (sz i = 0, e = tn->children.size(); i < e; ++i) {
        if (tn->children[i].type == tree_node::child_node::type::model and
            tn->children[i].mdl == data.mdl_id) {
            tn->children[i].disable();
            break;
        }
    }

    pj_ed.pj.sim.deallocate(data.mdl_id);

    return success();
}

#if 0
static int copy_model(const command::copy_model_t& data) noexcept
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
#endif

status project::simulation_new_observer(
  const command::new_observer_t& data) noexcept
{
    if (auto* mdl = = sim.models.try_to_get(data.mdl_id)) {
        if (sim.observers.exists(mdl->obs_id))
            return success();

        if (sim.observers.can_alloc(1) or sim.observers.grow<3, 2>(1)) {
            sim.observe(*mdl);
        } else {
            log_m(log_level::error, [&](auto& m) noexcept {
                format(m,
                       "Failed to allocate observer. (capacity:{})",
                       sim.observers.capacity());
            });
        }
    } else {
        log_m(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", data.mdl_id);
        });
    }

    return success();
}

status project::simulation_free_observer(
  const command::free_observer_t& data) noexcept
{
    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
        sim.unobserve(*mdl);
    } else {
        log_m(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", data.mdl_id);
        });
    }

    return success();
}

status projct::simulation_send_message(
  const command::send_message_t& data) noexcept
{
    const auto t = irt::time_domain<time>::is_infinity(sim.current_time())
                     ? sim.last_time()
                     : sim.current_time();

    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
        if (mdl->type == dynamics_type::constant) {
            if (mdl->handle == invalid_heap_handle) {
                sim.sched.alloc(*mdl, data.mdl_id, t);
            } else {
                if (sim.sched.is_in_tree(mdl->handle)) {
                    sim.sched.update(*mdl, t);
                } else {
                    sim.sched.reintegrate(*mdl, t);
                }
            }

            mdl->tn = t;
            return;
        } else {
            log_m(log_level::error, [&](auto& msg) noexcept {
                format(
                  msg, "Model ID {} is not a constant model.", data.mdl_id);
            });
        }
    } else {
        log_m(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", data.mdl_id);
        });
    }

    return success();
}

status project::simulation_apply_command() noexcept
{
    for (const auto& cmd : commands) {
        switch (cmd.type) {
        case command_type::none:
            break;
        case command_type::new_model:
            simulation_new_model(cmd.data.new_model);
            break;
        case command_type::free_model:
            simulation_free_model(cmd.data.free_model);
            break;
        case command_type::copy_model:
            copy_model(cmd.data.copy_model);
            break;
        case command_type::new_connection:
            new_connection(*this, cmd.data.new_connection);
            break;
        case command_type::free_connection:
            free_connection(*this, cmd.data.free_connection);
            break;
        case command_type::new_observer:
            simulation_new_observer(cmd.data.new_observer);
            break;
        case command_type::free_observer:
            simulation_free_observer(cmd.data.free_observer);
            break;
        case command_type::send_message:
            simulation_send_message(cmd.data.send_message);
            break;
        }
    }

    commands.clear();

    return success();
}

// void project_editor::start_simulation_update_state(application& app) noexcept
//{
//     if (not commands.empty())
//         start_simulation_commands_apply(app, app.pjs.get_id(*this));
//
//     if (any_equal(simulation_state,
//                   simulation_status::paused,
//                   simulation_status::run_requiring)) {
//
//         simulation_state = simulation_status::run_requiring;
//
//         if (real_time)
//             start_simulation_live_run(app);
//         else
//             start_simulation_static_run(app);
//     }
//
//     if (simulation_state == simulation_status::finish_requiring) {
//         simulation_state = simulation_status::finishing;
//         start_simulation_finish(app);
//     }
// }

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

void project::simulation_observation_for_imm_observers(
  unordered_task_list& tasks) noexcept
{
    debug::ensure(simulation_state != simulation_status::finished);

    constexpr std::size_t capacity = 255;
    std::size_t           obs_max  = pj.sim.immediate_observers.size();
    std::size_t           current  = 0;

    while (obs_max > 0) {
        const auto loop = std::min(obs_max, capacity);

        for (std::size_t i = 0; i != loop; ++i) {
            auto obs_id = pj.sim.immediate_observers[i + current];

            tasks.add([&, obs_id]() noexcept {
                if (auto* obs = sim.observers.try_to_get(obs_id)) {
                    auto& res = sim.observers.get<resampler>(obs_id);

                    res.tick(*obs, sim.current_time());
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

void project::simulation_observation_for_all_observers(
  unordered_task_list& tasks) noexcept
{
    debug::ensure(simulation_state != simulation_status::finished);

    constexpr std::size_t capacity = 255;
    std::size_t           obs_max  = pj.sim.observers.ssize();
    std::size_t           current  = 0;

    auto it = pj.sim.observers.begin();
    auto et = pj.sim.observers.end();

    while (it != et) {
        const auto loop = std::min(obs_max, capacity);

        for (std::size_t i = 0; i != loop; ++i) {
            const auto obs_id = pj.sim.observers.get_id(*it);

            tasks.add([&, obs_id]() noexcept {
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
