// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

#include "application.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/modeling.hpp"

namespace irt {

static void simulation_clear(component_editor&  ed,
                             simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    tree_node* tree = nullptr;
    while (app->mod.tree_nodes.next(tree))
        tree->sim.data.clear();

    sim_ed.clear();
    sim_ed.display_graph = true;
}

static status simulation_init_observation(simulation_editor& sim_ed,
                                          tree_node&         tree,
                                          component&         compo)
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);
    auto& mod = app->mod;

    if (compo.type == component_type::simple) {
        if (auto* s_compo =
              mod.simple_components.try_to_get(compo.id.simple_id);
            s_compo) {
            for (auto child_id : s_compo->children) {
                auto* c = mod.children.try_to_get(child_id);
                if (!c)
                    continue;

                if (c->observable) {
                    irt_assert(c->type == child_type::model);

                    const auto mdl_id = c->id.mdl_id;

                    if (auto* obs_map = tree.observables.get(mdl_id); obs_map) {
                        auto* sim_map = tree.sim.get(mdl_id);
                        irt_assert(sim_map);

                        const auto sim_id = enum_cast<model_id>(*sim_map);
                        auto*      mdl    = app->sim.models.try_to_get(sim_id);

                        irt_assert(mdl);
                        sim_ed.add_simulation_observation_for(c->name.sv(),
                                                              sim_id);
                    }
                }
            }
        }
    }

    return status::success;
}

static status simulation_init_observation(component_editor& /* ed */,
                                          simulation_editor& sim_ed,
                                          tree_node&         head) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);
    sim_ed.sim_obs.clear();
    app->sim.observers.clear();

    vector<tree_node*> stack;
    stack.emplace_back(&head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = app->mod.components.try_to_get(compo_id);

        if (compo)
            irt_return_if_bad(
              simulation_init_observation(sim_ed, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    return status::success;
}

template<typename S, typename... Args>
static void make_copy_error_msg(component_editor& ed,
                                const S&          fmt,
                                Args&&... args) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);
    auto& n   = app->notifications.alloc(log_level::error);
    n.title   = "Component copy failed";

    auto ret = fmt::vformat_to_n(n.message.begin(),
                                 static_cast<size_t>(n.message.capacity() - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    n.message.resize(static_cast<int>(ret.size));

    app->notifications.enable(n);
}

template<typename S, typename... Args>
static void make_init_error_msg(component_editor& ed,
                                const S&          fmt,
                                Args&&... args) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);
    auto& n   = app->notifications.alloc(log_level::error);
    n.title   = "Simulation initialization fail";

    auto ret = fmt::vformat_to_n(n.message.begin(),
                                 static_cast<size_t>(n.message.capacity() - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    n.message.resize(static_cast<int>(ret.size));

    app->notifications.enable(n);
}

static void simulation_copy(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    sim_ed.simulation_state = simulation_status::initializing;
    simulation_clear(ed, sim_ed);

    app->sim.clear();
    sim_ed.simulation_current = sim_ed.simulation_begin;

    auto* head = app->mod.tree_nodes.try_to_get(app->mod.head);
    if (!head) {
        make_copy_error_msg(ed, "Empty component");
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = app->mod.export_to(app->pj.m_compo, sim_ed.cache, app->sim);
        is_bad(ret)) {
        make_copy_error_msg(
          ed, "Copy hierarchy failed: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_init_observation(ed, sim_ed, *head);
        is_bad(ret)) {
        make_copy_error_msg(
          ed, "Initialization of observation failed: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = app->mod.srcs.prepare(); is_bad(ret)) {
        make_copy_error_msg(
          ed, "External sources initialization: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = app->sim.initialize(sim_ed.simulation_begin); is_bad(ret)) {
        make_copy_error_msg(
          ed, "Models initialization models: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    sim_ed.simulation_state = simulation_status::initialized;
}

static void simulation_init(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    sim_ed.simulation_state   = simulation_status::initializing;
    sim_ed.simulation_current = sim_ed.simulation_begin;

    sim_ed.tl.reset();

    auto* head = app->mod.tree_nodes.try_to_get(app->mod.head);
    if (!head) {
        make_init_error_msg(ed, "Empty component");
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    simulation_observation* mem = nullptr;
    while (sim_ed.sim_obs.next(mem)) {
        mem->clear();
    }

    if (auto ret = app->mod.srcs.prepare(); is_bad(ret)) {
        make_init_error_msg(
          ed, "Fail to initalize external sources: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = app->sim.initialize(sim_ed.simulation_begin); is_bad(ret)) {
        make_init_error_msg(
          ed, "Models initialization models: {}", status_string(ret));
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    sim_ed.simulation_state = simulation_status::initialized;
}

static void task_simulation_clear(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    simulation_clear(g_task->app->component_ed, g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static void task_simulation_copy(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    g_task->app->simulation_ed.force_pause = false;
    g_task->app->simulation_ed.force_stop  = false;
    simulation_copy(g_task->app->component_ed, g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static void task_simulation_init(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    g_task->app->simulation_ed.force_pause = false;
    g_task->app->simulation_ed.force_stop  = false;
    simulation_init(g_task->app->component_ed, g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static status debug_run(simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);

    if (auto ret = run(sim_ed.tl, app->sim, sim_ed.simulation_current);
        is_bad(ret)) {
        auto* app = container_of(&sim_ed, &application::simulation_ed);
        auto& n   = app->notifications.alloc(log_level::error);
        n.title   = "Debug run error";
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret;
    }

    return status::success;
}

static status run(simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);

    if (auto ret = app->sim.run(sim_ed.simulation_current); is_bad(ret)) {
        auto* app = container_of(&sim_ed, &application::simulation_ed);
        auto& n   = app->notifications.alloc(log_level::error);
        n.title   = "Run error";
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret;
    }

    return status::success;
}

static void task_simulation_static_run(simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);

    sim_ed.simulation_state = simulation_status::running;
    namespace stdc          = std::chrono;

    auto start_at = stdc::high_resolution_clock::now();
    auto end_at   = stdc::high_resolution_clock::now();
    auto duration = end_at - start_at;

    auto duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
    auto duration_since_start = duration_cast.count();

    bool stop_or_pause;

    do {
        if (sim_ed.simulation_state != simulation_status::running)
            return;

        if (sim_ed.store_all_changes) {
            if (auto ret = debug_run(sim_ed); is_bad(ret)) {
                sim_ed.simulation_state = simulation_status::finish_requiring;
                return;
            }
        } else {
            if (auto ret = run(sim_ed); is_bad(ret)) {
                sim_ed.simulation_state = simulation_status::finish_requiring;
                return;
            }
        }

        if (!app->sim.immediate_observers.empty())
            sim_ed.build_observation_output();

        if (!sim_ed.infinity_simulation &&
            sim_ed.simulation_current >= sim_ed.simulation_end) {
            sim_ed.simulation_current = sim_ed.simulation_end;
            sim_ed.simulation_state   = simulation_status::finish_requiring;
            return;
        }

        end_at        = stdc::high_resolution_clock::now();
        duration      = end_at - start_at;
        duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
        duration_since_start = duration_cast.count();
        stop_or_pause        = sim_ed.force_pause || sim_ed.force_stop;
    } while (!stop_or_pause &&
             duration_since_start < sim_ed.thread_frame_duration);

    if (sim_ed.force_pause) {
        sim_ed.force_pause      = false;
        sim_ed.simulation_state = simulation_status::pause_forced;
    } else if (sim_ed.force_stop) {
        sim_ed.force_stop       = false;
        sim_ed.simulation_state = simulation_status::finish_requiring;
    } else {
        sim_ed.simulation_state = simulation_status::paused;
    }
}

static void task_simulation_live_run(simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);

    sim_ed.simulation_state = simulation_status::running;
    namespace stdc          = std::chrono;

    const auto max_sim_duration =
      static_cast<real>(simulation_editor::thread_frame_duration) /
      static_cast<real>(sim_ed.simulation_real_time_relation);

    const auto sim_start_at = sim_ed.simulation_current;
    auto       start_at     = stdc::high_resolution_clock::now();
    auto       end_at       = stdc::high_resolution_clock::now();
    auto       duration     = end_at - start_at;

    auto duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
    auto duration_since_start = duration_cast.count();

    bool stop_or_pause;

    do {
        if (sim_ed.simulation_state != simulation_status::running)
            return;

        if (sim_ed.store_all_changes) {
            if (auto ret = debug_run(sim_ed); is_bad(ret)) {
                sim_ed.simulation_state = simulation_status::finish_requiring;
                return;
            }
        } else {
            if (auto ret = run(sim_ed); is_bad(ret)) {
                sim_ed.simulation_state = simulation_status::finish_requiring;
                return;
            }
        }

        if (!app->sim.immediate_observers.empty())
            sim_ed.build_observation_output();

        const auto sim_end_at   = sim_ed.simulation_current;
        const auto sim_duration = sim_end_at - sim_start_at;

        if (!sim_ed.infinity_simulation &&
            sim_ed.simulation_current >= sim_ed.simulation_end) {
            sim_ed.simulation_current = sim_ed.simulation_end;
            sim_ed.simulation_state   = simulation_status::finish_requiring;
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

        stop_or_pause = sim_ed.force_pause || sim_ed.force_stop;
    } while (!stop_or_pause &&
             duration_since_start < sim_ed.thread_frame_duration);

    if (sim_ed.force_pause) {
        sim_ed.force_pause      = false;
        sim_ed.simulation_state = simulation_status::pause_forced;
    } else if (sim_ed.force_stop) {
        sim_ed.force_stop       = false;
        sim_ed.simulation_state = simulation_status::finish_requiring;
    } else {
        sim_ed.simulation_state = simulation_status::paused;
    }
}

static void task_simulation_run(simulation_editor& sim_ed) noexcept
{
    if (sim_ed.real_time) {
        task_simulation_live_run(sim_ed);
    } else {
        task_simulation_static_run(sim_ed);
    }
}

static void task_simulation_run_1(simulation_editor& sim_ed) noexcept
{
    sim_ed.simulation_state = simulation_status::running;

    if (auto ret = debug_run(sim_ed); is_bad(ret)) {
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return;
    }

    if (!sim_ed.infinity_simulation &&
        sim_ed.simulation_current >= sim_ed.simulation_end) {
        sim_ed.simulation_current = sim_ed.simulation_end;
        sim_ed.simulation_state   = simulation_status::finish_requiring;
        return;
    }

    if (sim_ed.force_pause) {
        sim_ed.force_pause      = false;
        sim_ed.simulation_state = simulation_status::pause_forced;
    } else if (sim_ed.force_stop) {
        sim_ed.force_stop       = false;
        sim_ed.simulation_state = simulation_status::finish_requiring;
    } else {
        sim_ed.simulation_state = simulation_status::pause_forced;
    }
}

static void task_simulation_finish(component_editor& /*ed*/,
                                   simulation_editor& sim_ed) noexcept
{
    auto* app = container_of(&sim_ed, &application::simulation_ed);

    sim_ed.simulation_state = simulation_status::finishing;

    app->sim.immediate_observers.clear();
    sim_ed.build_observation_output();

    if (sim_ed.store_all_changes)
        finalize(sim_ed.tl, app->sim, sim_ed.simulation_current);
    else
        app->sim.finalize(sim_ed.simulation_end);

    sim_ed.simulation_state = simulation_status::finished;
}

static void task_simulation_run(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    task_simulation_run(g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static void task_simulation_run_1(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    task_simulation_run_1(g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static void task_simulation_pause(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    g_task->app->simulation_ed.force_pause = true;

    g_task->state = task_status::finished;
}

static void task_simulation_stop(void* param) noexcept
{
    auto* g_task = reinterpret_cast<simulation_task*>(param);

    g_task->app->simulation_ed.force_stop = true;

    g_task->state = task_status::finished;
}

static void task_simulation_finish(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    task_simulation_finish(g_task->app->component_ed,
                           g_task->app->simulation_ed);

    g_task->state = task_status::finished;
}

static void task_enable_or_disable_debug(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    g_task->app->simulation_ed.tl.reset();

    if (g_task->app->simulation_ed.store_all_changes) {
        auto ret = initialize(g_task->app->simulation_ed.tl,
                              g_task->app->sim,
                              g_task->app->simulation_ed.simulation_current);

        if (is_bad(ret)) {
            auto& n = g_task->app->notifications.alloc(log_level::error);
            n.title = "Debug mode failed to initialize";
            format(n.message,
                   "Fail to initialize the debug mode: {}",
                   status_string(ret));
            g_task->app->notifications.enable(n);
            g_task->app->simulation_ed.simulation_state =
              simulation_status::not_started;
        }
    }

    g_task->state = task_status::finished;
}

//
// public API
//

void simulation_editor::simulation_update_state() noexcept
{
    if (simulation_state == simulation_status::paused) {
        auto* app        = container_of(this, &application::simulation_ed);
        simulation_state = simulation_status::run_requiring;
        app->add_simulation_task(task_simulation_run);
    }

    if (simulation_state == simulation_status::finish_requiring) {
        auto* app        = container_of(this, &application::simulation_ed);
        simulation_state = simulation_status::finishing;
        app->add_simulation_task(task_simulation_finish);
    }

    simulation_editor::model_to_move new_position;
    while (models_to_move.pop(new_position)) {
        const auto index   = get_index(new_position.id);
        const auto index_i = static_cast<int>(index);

        ImNodes::SetNodeScreenSpacePos(index_i, new_position.position);
    }
}

void simulation_editor::simulation_copy_modeling() noexcept
{
    bool state = match(simulation_state,
                       simulation_status::initialized,
                       simulation_status::not_started,
                       simulation_status::finished);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        auto& mod = app->mod;

        auto* modeling_head = mod.tree_nodes.try_to_get(mod.head);
        if (!modeling_head) {
            auto& notif = app->notifications.alloc(log_level::error);
            notif.title = "Empty model";
            app->notifications.enable(notif);
        } else {
            app->simulation_ed.clear();
            app->add_simulation_task(task_simulation_copy);
        }
    }
}

void simulation_editor::simulation_init() noexcept
{
    bool state = match(simulation_state,
                       simulation_status::initialized,
                       simulation_status::not_started,
                       simulation_status::finished);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        app->add_simulation_task(task_simulation_init);
    }
}

void simulation_editor::simulation_clear() noexcept
{
    auto* app = container_of(this, &application::simulation_ed);

    // Disable display graph node to avoid data race on @c
    // simulation_editor::simulation data.
    app->simulation_ed.display_graph = false;

    app->add_simulation_task(task_simulation_clear);
}

void simulation_editor::simulation_start() noexcept
{
    bool state = match(simulation_state,
                       simulation_status::initialized,
                       simulation_status::pause_forced);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        app->add_simulation_task(task_simulation_run);
    }
}

void simulation_editor::simulation_start_1() noexcept
{
    bool state = match(simulation_state,
                       simulation_status::initialized,
                       simulation_status::pause_forced,
                       simulation_status::debugged);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        if (auto* parent = app->mod.tree_nodes.try_to_get(head); parent) {
            app->add_simulation_task(task_simulation_run_1);
        }
    }
}

void simulation_editor::simulation_pause() noexcept
{
    bool state = match(simulation_state, simulation_status::running);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        app->add_simulation_task(task_simulation_pause);
    }
}

void simulation_editor::simulation_stop() noexcept
{
    bool state = match(
      simulation_state, simulation_status::running, simulation_status::paused);

    irt_assert(state);

    if (state) {
        auto* app = container_of(this, &application::simulation_ed);
        app->add_simulation_task(task_simulation_stop);
    }
}

void simulation_editor::simulation_advance() noexcept
{
    simulation_state      = simulation_status::debugged;
    have_use_back_advance = true;

    auto* app = container_of(this, &application::simulation_ed);
    app->add_simulation_task(task_simulation_advance);
}

void simulation_editor::simulation_back() noexcept
{
    simulation_state      = simulation_status::debugged;
    have_use_back_advance = true;

    auto* app = container_of(this, &application::simulation_ed);
    app->add_simulation_task(task_simulation_back);
}

void simulation_editor::enable_or_disable_debug() noexcept
{
    auto* app = container_of(this, &application::simulation_ed);
    app->add_simulation_task(task_enable_or_disable_debug);
}

} // namespace irt
