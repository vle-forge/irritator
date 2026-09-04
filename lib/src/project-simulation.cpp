// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/error.hpp"
#include <expected>
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
        if (auto r = grid_obs.init(*this, mod); r.has_error())
            return r.error();

    for (auto& graph_obs : graph_observers)
        if (auto r = graph_obs.init(*this, mod); r.has_error())
            return r.error();

    for (auto& v_obs : variable_observers)
        if (auto r = v_obs.init(*this); r.has_error())
            return r.error();

    if (const auto path = get_observation_dir(mod); path.has_value())
        file_obs.initialize(*this, path->string());

    return success();
}

status project::simulation_copy(const modeling& mod) noexcept
{
    simulation_state = simulation_status::initializing;

    return mod.ids
      .read([&](const auto& ids, auto) -> status {
          return mod.files.read([&](const auto& fs, auto) -> status {
              auto guard = make_scope_exit(
                [&] { simulation_state = simulation_status::initialized; });

              return set(ids, fs, head())
                .and_then([&] { return sim.srcs.prepare(); })
                .and_then([&] { return sim.initialize(); });
          });
      })
      .or_else([&](const auto ec) noexcept {
          simulation_state = simulation_status::not_started;

          using namespace std::literals;

          switch (ec.cat()) {
          case category::project:
              log(log_level::error,
                  "Error importing the project"sv,
                  "Error in project copy"sv);
              break;
          case category::external_source:
              log(log_level::error,
                  "Error importing the project"sv,
                  "Error external source preparation"sv);
              break;
          case category::simulation:
              log(log_level::error,
                  "Error importing the project"sv,
                  "Error in simulation copy"sv);
              break;
          default:
              log(log_level::error,
                  "Error importing the project"sv,
                  "Unknown copy error"sv);
              break;
          }

          return status{ ec };
      });
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

status project::simulation_new_model(const command::new_model_t& data) noexcept
{
    auto* tn = tree_nodes.try_to_get(data.tn_id);

    if (not tn) [[unlikely]] {
        log_m(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", ordinal(data.tn_id));
        });

        return make_error(project_errc::memory_error);
    }

    const auto tn_alloc =
      tn->children.can_alloc(1) or tn->children.grow<3, 2>(1);
    const auto sim_alloc = sim.can_alloc(1) or sim.grow_models<3, 2>();

    if (not tn_alloc or not sim_alloc) {
        log_m(log_level::error, [&](auto& m) noexcept {
            format(m,
                   "Fail to allocate new model in tree node {} (capacity: {}",
                   ordinal(data.tn_id),
                   tn->children.capacity());
        });
        return make_error(project_errc::memory_error);
    }

    auto& mdl = sim.alloc(data.type);

    if (auto ret = sim.make_initialize(mdl, sim.current_time());
        ret.has_error()) {
        log_m(log_level::error, [&](auto& m) noexcept {
            format(m,
                   "Fail to initialize new model of type {}",
                   dynamics_type_names[ordinal(data.type)]);
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
    auto* tn = tree_nodes.try_to_get(data.tn_id);
    if (not tn) [[unlikely]] {
        log_m(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", ordinal(data.tn_id));
        });

        return make_error(project_errc::memory_error);
    }

    auto* mdl = sim.models.try_to_get(data.mdl_id);
    if (not mdl) [[unlikely]] {
        log_m(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find model with ID {}", ordinal(data.mdl_id));
        });

        return make_error(project_errc::memory_error);
    }

    for (sz i = 0, e = tn->children.size(); i < e; ++i) {
        if (tn->children[i].type == tree_node::child_node::type::model and
            tn->children[i].mdl == data.mdl_id) {
            tn->children[i].disable();
            break;
        }
    }

    sim.deallocate(data.mdl_id);

    return success();
}

status project::simulation_copy_model(
  const command::copy_model_t& data) noexcept
{
    auto* src_mdl = sim.models.try_to_get(data.mdl_id);

    if (not src_mdl)
        return success();

    if (not sim.can_alloc(1) and not sim.grow_models<2, 1>()) {
        log_m(log_level::error, [](auto& m) noexcept {
            m = "Internal error: fail to allocate more models.";
        });

        return make_error(project_errc::memory_error);
    }

    auto& dst_mdl = sim.clone(*src_mdl);

    if (const auto r = sim.make_initialize(dst_mdl, sim.current_time());
        r.has_error()) {
        log_m(log_level::error, [r](auto& m) noexcept {
            format(m,
                   "Internal error: fail to initialize new model: {})",
                   r.error());
        });

        return r.error();
    }

    dispatch(*src_mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        if constexpr (has_output_port<Dynamics>) {
            for (int i = 0, e = length(dyn.y); i != e; ++i) {
                auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);

                sim.for_each(dyn.y[i], [&](auto& mdl_src, int port_src) {
                    const auto mdl_src_id = sim.get_id(mdl_src);

                    (void)sim.connect(dst_dyn.y[i], mdl_src_id, port_src);
                });
            }
        }
    });

    return success();
}

status project::simulation_new_connection(
  const command::new_connection_t& data) noexcept
{
    const auto can_connect = sim.can_connect(1) or sim.grow_connections<3, 2>();

    if (not can_connect) {
        log_m(log_level::error,
              [](auto& m) noexcept { m = "Fail to allocate new connection."; });

        return make_error(project_errc::memory_error);
    }

    auto* src = sim.models.try_to_get(data.mdl_src_id);
    auto* dst = sim.models.try_to_get(data.mdl_dst_id);

    if (not(src and dst))
        return success();

    const auto r = sim.connect(*src, data.port_src, *dst, data.port_dst);
    if (r.has_error()) {
        log_m(log_level::error, [r](auto& m) noexcept {
            format(m, "fail to allocate new connection ({}) ", r.error());
        });

        return r.error();
    }
    return success();
}

status project::simulation_free_connection(
  const command::free_connection_t& data) noexcept

{
    auto* src = sim.models.try_to_get(data.mdl_src_id);
    auto* dst = sim.models.try_to_get(data.mdl_dst_id);

    if (not(src and dst))
        return success();

    sim.disconnect(*src, data.port_src, *dst, data.port_dst);

    return success();
}

status project::simulation_new_observer(
  const command::new_observer_t& data) noexcept
{
    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
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
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
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
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
        });
    }

    return success();
}

status project::simulation_send_message(
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
        } else {
            log_m(log_level::error, [&](auto& msg) noexcept {
                format(msg,
                       "Model ID {} is not a constant model.",
                       ordinal(data.mdl_id));
            });
        }
    } else {
        log_m(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
        });
    }

    return success();
}

status project::simulation_apply_command() noexcept
{
    commands.for_each([&](const command& cmd) noexcept {
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
            simulation_copy_model(cmd.data.copy_model);
            break;
        case command_type::new_connection:
            simulation_new_connection(cmd.data.new_connection);
            break;
        case command_type::free_connection:
            simulation_free_connection(cmd.data.free_connection);
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
    });

    commands.clear();

    return success();
}

bool project::push(const command& cmd) noexcept
{
    using namespace std::literals;

    if (not commands.push(cmd)) {
        log(log_level::error,
            "Simulation  live modeling error"sv,
            "Fail to add command order in live simulation"sv);
        return false;
    }

    return true;
}

bool project::empty_snapshots() const noexcept { return snaps.empty(); }

bool project::empty_commands() const noexcept { return commands.empty(); }

void project::simulation_observation_for_imm_observers(
  unordered_task_list& utl) noexcept
{
    debug::ensure(simulation_state != simulation_status::finished);

    constexpr std::size_t capacity = 255;
    std::size_t           obs_max  = sim.immediate_observers.size();
    std::size_t           current  = 0;

    while (obs_max > 0) {
        const auto loop = std::min(obs_max, capacity);

        for (std::size_t i = 0; i != loop; ++i) {
            auto obs_id = sim.immediate_observers[i + current];

            utl.add([&, obs_id]() noexcept {
                if (auto* obs = sim.observers.try_to_get(obs_id)) {
                    auto& res = sim.observers.get<resampler>(obs_id);

                    res.tick(*obs, sim.current_time());
                }
            });
        }

        utl.submit();
        utl.wait_completion();

        current += loop;
        if (obs_max > capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    if (current > 0) {
        utl.submit();
        utl.wait_completion();
    }
}

void project::simulation_observation_for_all_observers(
  unordered_task_list& utl) noexcept
{
    debug::ensure(simulation_state != simulation_status::finished);

    constexpr std::size_t capacity = 255;
    std::size_t           obs_max  = sim.observers.ssize();
    std::size_t           current  = 0;

    auto it = sim.observers.begin();
    auto et = sim.observers.end();

    while (it != et) {
        const auto loop = std::min(obs_max, capacity);

        for (std::size_t i = 0; i != loop; ++i) {
            const auto obs_id = sim.observers.get_id(*it);

            utl.add([&, obs_id]() noexcept {
                if (auto* obs = sim.observers.try_to_get(obs_id)) {
                    auto& res = sim.observers.get<resampler>(obs_id);

                    res.tick(*obs, sim.current_time());
                }
            });

            ++it;
        }

        utl.submit();
        utl.wait_completion();

        current += loop;
        if (obs_max >= capacity)
            obs_max -= capacity;
        else
            obs_max = 0;
    }

    if (current > 0) {
        utl.submit();
        utl.wait_completion();
    }
}

status project::simulation_run_for(
  unordered_task_list&            utl,
  const std::chrono::milliseconds task_duration,
  std::atomic_bool&               pause) noexcept
{
    simulation_state = simulation_status::running;
    namespace stdc   = std::chrono;

    auto start_at = stdc::high_resolution_clock::now();
    auto end_at   = stdc::high_resolution_clock::now();
    auto duration = end_at - start_at;

    auto duration_cast = stdc::duration_cast<stdc::microseconds>(task_duration);

    do {
        if (simulation_state != simulation_status::running)
            return success();

        // if (save_simulation_raw_data !=
        //     project_editor::raw_data_type::none) {
        //     if (auto ret = run_raw_obs(*this); !ret) {
        //         simulation_state = simulation_status::finish_requiring;
        //         simulation_display_current = pj.sim.current_time();
        //         return;
        //     }
        // } else {
        //     if (store_all_changes)
        //         snaps.emplace_back(pj.sim);

        if (auto ret = sim.run(); ret.has_error()) {
            simulation_state = simulation_status::finish_requiring;

            log_m(log_level::error, [&](auto& msg) noexcept {
                format(msg,
                       "Fail in {} with error {}",
                       ordinal(ret.error().cat()),
                       ret.error().value());
            });

            return ret.error();
        }

        if (not sim.immediate_observers.empty())
            simulation_observation_for_imm_observers(utl);

        if (sim.current_time_expired()) {
            simulation_state = simulation_status::finish_requiring;
            return success();
        }

        end_at        = stdc::high_resolution_clock::now();
        duration      = end_at - start_at;
        duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
        // duration_since_start = duration_cast.count();
        //  stop_or_pause        = force_pause || force_stop;
    } while (!pause and duration_cast < task_duration);

    if (pause)
        simulation_state = simulation_status::paused;

    simulation_observation_for_all_observers(utl);

    return success();
}

status project::simulation_complete_run(unordered_task_list& utl) noexcept
{
    debug::ensure(simulation_state == simulation_status::initialized);

    simulation_state = simulation_status::running;

    do {
        if (const auto r = sim.run(); r.has_error())
            simulation_state = simulation_status::finish_requiring;

        if (not sim.immediate_observers.empty())
            simulation_observation_for_imm_observers(utl);

        if (sim.current_time_expired())
            simulation_state = simulation_status::finish_requiring;
    } while (not sim.current_time_expired() or
             simulation_state == simulation_status::running);

    return simulation_finish(utl);
}

status project::simulation_live_run(
  unordered_task_list&            utl,
  const std::chrono::milliseconds one_simulation_time_duration,
  const std::chrono::milliseconds task_duration,
  std::atomic_bool&               force_pause) noexcept
{
    namespace stdc = std::chrono;

    simulation_state = simulation_status::running;

    const auto start_task_rt = stdc::high_resolution_clock::now();
    const auto end_task_rt   = start_task_rt + task_duration;

    do {
        if (simulation_state != simulation_status::running)
            return success();

        if (force_pause) {
            simulation_state = simulation_status::paused;
            return success();
        }

        time sim_t;
        time sim_next_t;

        sim_t      = sim.current_time();
        sim_next_t = sim.sched.tn();

        if (time_domain<time>::is_infinity(sim_t)) {
            sim_next_t = sim_t + 1.0;
        } else {
            if (time_domain<time>::is_infinity(sim_next_t)) {
                sim_next_t = sim_t + 1.0;
            }
        }

        const auto current_rt   = stdc::high_resolution_clock::now();
        const auto diff_rt      = current_rt - start_task_rt;
        const auto remaining_rt = task_duration - diff_rt;

        // There is no real time available for this simulation task.
        // Program the next.
        if (remaining_rt.count() < 0) {
            simulation_state = simulation_status::paused;
            return success();
        }

        const auto wakeup_rt =
          start_task_rt + (sim_next_t * one_simulation_time_duration);

        // If the next wakeup exceed the simulation frame, do nothing.
        if (wakeup_rt > end_task_rt) {
            simulation_state = simulation_status::paused;
            return success();
        }

        if (wakeup_rt >= start_task_rt + std::chrono::milliseconds{ 1 })
            std::this_thread::sleep_until(wakeup_rt);

        sim.current_time(sim_t);

        if (const auto r = sim.run(); r.has_error())
            simulation_state = simulation_status::finish_requiring;

        if (not sim.immediate_observers.empty())
            simulation_observation_for_imm_observers(utl);

        if (sim.current_time_expired())
            simulation_state = simulation_status::finish_requiring;

    } while (not sim.current_time_expired() or
             simulation_state == simulation_status::running);

    simulation_observation_for_all_observers(utl);

    return success();
}

status project::simulation_step() noexcept
{
    const auto state = any_equal(simulation_state,
                                 simulation_status::initialized,
                                 simulation_status::paused);

    if (state) {
        if (tree_nodes.try_to_get(tn_head())) {
            simulation_state = simulation_status::running;

            const auto current_time = sim.current_time();

            if (auto ret = sim.run(); ret.has_error()) {
                simulation_state = simulation_status::finish_requiring;

                log_m(log_level::error, [&](auto& msg) noexcept {
                    format(msg,
                           "Fail in {} with error {}",
                           ordinal(ret.error().cat()),
                           ret.error().value());
                });

                return ret.error();
            }

            if (current_time != sim.current_time())
                snaps.emplace_back(sim);

            if (file_obs.can_update(sim.current_time()))
                file_obs.update(*this);

            if (sim.current_time_expired()) {
                simulation_state = simulation_status::finish_requiring;
                return success();
            }

            simulation_state = simulation_status::paused;
        }
    }

    return success();
}

status project::simulation_finish(unordered_task_list& utl) noexcept
{
    simulation_state = simulation_status::finishing;

    const auto ret = sim.finalize();

    simulation_observation_for_all_observers(utl);
    simulation_state = simulation_status::finished;

    if (ret.has_error()) {
        log(log_level::error, [](auto& t, auto& m) {
            t = "Simulation finalizing fail";
            m = "FIXME from ret";
        });
    }

    return ret;
}

void project::simulation_advance() noexcept
{
    debug::ensure(debug_mode);

    debug::ensure(any_equal(simulation_state,
                            simulation_status::initialized,
                            simulation_status::paused));

    if (snaps.empty())
        return;

    if (not current_snap.has_value() or
        snaps.ptr_from_index(*current_snap) == nullptr) {
        sim          = *snaps.front();
        current_snap = snaps.index_of(snaps.front());
    }

    const auto* snap = snaps.ptr_from_index(*current_snap);
    if (snaps.previous(snap)) {
        sim          = *snap;
        current_snap = snaps.index_of(snap);
    }
}

void project::simulation_back() noexcept
{
    debug::ensure(debug_mode);

    debug::ensure(any_equal(simulation_state,
                            simulation_status::initialized,
                            simulation_status::paused));

    if (snaps.empty())
        return;

    if (not current_snap.has_value() or
        snaps.ptr_from_index(*current_snap) == nullptr) {
        sim          = *snaps.back();
        current_snap = snaps.index_of(snaps.back());
    }

    const auto* snap = snaps.ptr_from_index(*current_snap);
    if (snaps.next(snap)) {
        sim          = *snap;
        current_snap = snaps.index_of(snap);
    }
}

} // namespace irt
