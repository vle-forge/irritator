// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static status simulation_copy_model(simulation&           sim,
                                    tree_node&            tree,
                                    simulation_tree_node& sim_tree,
                                    const component&      src)
{
    sim_tree.children.clear();

    child* c = nullptr;
    while (src.children.next(c)) {
        if (c->type == child_type::model) {
            auto  mdl_id = enum_cast<model_id>(c->id);
            auto* mdl    = src.models.try_to_get(mdl_id);

            if (mdl) {
                irt_return_if_fail(sim.models.can_alloc(),
                                   status::simulation_not_enough_model);

                auto& new_mdl    = sim.clone(*mdl);
                auto  new_mdl_id = sim.models.get_id(new_mdl);

                sim_tree.children.emplace_back(new_mdl_id);
                tree.sim.data.emplace_back(mdl_id, new_mdl_id);
            }
        }
    }

    return status::success;
}

static status simulation_copy_models(component_editor&  ed,
                                     simulation_editor& sim_ed,
                                     tree_node&         head) noexcept
{
    vector<tree_node*> stack;
    stack.emplace_back(&head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = ed.mod.components.try_to_get(compo_id);

        if (compo) {
            auto* sim_tree = sim_ed.tree_nodes.try_to_get(cur->sim_tree_node);
            irt_assert(sim_tree);

            irt_return_if_bad(
              simulation_copy_model(sim_ed.sim, *cur, *sim_tree, *compo));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    tree_node* tree = nullptr;
    while (ed.mod.tree_nodes.next(tree))
        tree->sim.sort();

    return status::success;
}

static status simulation_copy_connections(component_editor&  ed,
                                          simulation_editor& sim_ed,
                                          tree_node&         tree,
                                          const component&   compo)
{
    connection* con = nullptr;
    while (compo.connections.next(con)) {
        auto* src = compo.children.try_to_get(con->src);
        auto* dst = compo.children.try_to_get(con->dst);

        if (!src || !dst)
            continue;

        model* mdl_src   = nullptr;
        model* mdl_dst   = nullptr;
        i8     index_src = -1;
        i8     index_dst = -1;

        if (src->type == child_type::model) {
            auto  orig_mdl_src = enum_cast<model_id>(src->id);
            auto* real_mdl_src = tree.sim.get(orig_mdl_src);
            if (real_mdl_src) {
                mdl_src   = sim_ed.sim.models.try_to_get(*real_mdl_src);
                index_src = con->index_src;
            }
        } else {
            auto  cp_src_id = enum_cast<component_id>(src->id);
            auto* cp_src    = ed.mod.components.try_to_get(cp_src_id);

            if (cp_src) {
                if (0 <= con->index_src && con->index_src < cp_src->y.ssize()) {
                    tree_node* child = tree.tree.get_child();
                    while (child) {
                        if (child->id_in_parent == con->src)
                            break;
                        child = child->tree.get_sibling();
                    }

                    if (child) {
                        auto  orig_src = cp_src->y[con->index_src].id;
                        auto* child_orig_src =
                          compo.children.try_to_get(orig_src);

                        irt_assert(child_orig_src);
                        irt_assert(child_orig_src->type == child_type::model);

                        i8        port_src = cp_src->y[con->index_src].index;
                        model_id* real_src = child->sim.get(
                          enum_cast<model_id>(child_orig_src->id));

                        if (real_src) {
                            mdl_src   = sim_ed.sim.models.try_to_get(*real_src);
                            index_src = port_src;
                        }
                    }
                }
            }
        }

        if (dst->type == child_type::model) {
            auto  orig_mdl_dst = enum_cast<model_id>(dst->id);
            auto* real_mdl_dst = tree.sim.get(orig_mdl_dst);
            if (real_mdl_dst) {
                mdl_dst   = sim_ed.sim.models.try_to_get(*real_mdl_dst);
                index_dst = con->index_dst;
            }
        } else {
            auto  cp_dst_id = enum_cast<component_id>(dst->id);
            auto* cp_dst    = ed.mod.components.try_to_get(cp_dst_id);

            if (cp_dst) {
                if (0 <= con->index_dst && con->index_dst < cp_dst->x.ssize()) {
                    tree_node* child = tree.tree.get_child();
                    while (child) {
                        if (child->id_in_parent == con->dst)
                            break;
                        child = child->tree.get_sibling();
                    }

                    if (child) {
                        child_id child_orig_dst = cp_dst->x[con->index_dst].id;
                        auto*    orig_dst =
                          compo.children.try_to_get(child_orig_dst);

                        irt_assert(orig_dst);
                        irt_assert(orig_dst->type == child_type::model);

                        i8        port_dst = cp_dst->x[con->index_dst].index;
                        model_id* real_dst =
                          child->sim.get(enum_cast<model_id>(orig_dst->id));

                        if (real_dst) {
                            mdl_dst   = sim_ed.sim.models.try_to_get(*real_dst);
                            index_dst = port_dst;
                        }
                    }
                }
            }
        }

        if (mdl_src && mdl_dst) {
            irt_return_if_bad(
              sim_ed.sim.connect(*mdl_src, index_src, *mdl_dst, index_dst));
        }
    }

    return status::success;
}

static status simulation_copy_connections(component_editor&  ed,
                                          simulation_editor& sim_ed,
                                          tree_node&         head) noexcept
{
    vector<tree_node*> stack;
    stack.emplace_back(&head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = ed.mod.components.try_to_get(compo_id);

        if (compo) {
            irt_return_if_bad(
              simulation_copy_connections(ed, sim_ed, *cur, *compo));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    return status::success;
}

static status simulation_copy_tree(component_editor&  ed,
                                   simulation_editor& sim_ed,
                                   tree_node&         head) noexcept
{
    tree_node* tree = nullptr;
    while (ed.mod.tree_nodes.next(tree)) {
        auto& sim_tree      = sim_ed.tree_nodes.alloc();
        tree->sim_tree_node = sim_ed.tree_nodes.get_id(sim_tree);
        tree->sim.data.clear();
    }

    tree = nullptr;
    while (ed.mod.tree_nodes.next(tree)) {
        auto* sim_tree = sim_ed.tree_nodes.try_to_get(tree->sim_tree_node);
        irt_assert(sim_tree);

        if (auto* parent = tree->tree.get_parent(); parent) {
            const auto parent_sim_tree_id = parent->sim_tree_node;
            auto*      parent_sim_tree =
              sim_ed.tree_nodes.try_to_get(parent_sim_tree_id);

            irt_assert(parent_sim_tree);

            sim_tree->tree.parent_to(parent_sim_tree->tree);
        }
    }

    sim_ed.head = head.sim_tree_node;

    return status::success;
}

static void simulation_clear(component_editor&  ed,
                             simulation_editor& sim_ed) noexcept
{
    tree_node* tree = nullptr;
    while (ed.mod.tree_nodes.next(tree))
        tree->sim.data.clear();

    sim_ed.clear();
}

static status simulation_init_observation(simulation_editor& sim_ed,
                                          tree_node&         tree,
                                          component&         compo)
{
    child* c = nullptr;
    while (compo.children.next(c)) {
        if (c->observable) {
            irt_assert(c->type == child_type::model);

            const auto mdl_id = enum_cast<model_id>(c->id);

            if (auto* obs_map = tree.observables.get(mdl_id); obs_map) {
                auto obs_id = enum_cast<simulation_observation_id>(obs_map->id);
                auto* obs   = sim_ed.sim_obs.try_to_get(obs_id);
                irt_assert(obs);

                auto* sim_map = tree.sim.get(mdl_id);
                irt_assert(sim_map);

                const auto sim_id = enum_cast<model_id>(*sim_map);
                auto*      mdl    = sim_ed.sim.models.try_to_get(sim_id);

                irt_assert(mdl);

                irt_return_if_fail(sim_ed.sim.observers.can_alloc(1),
                                   status::simulation_not_enough_model);

                auto& output =
                  sim_ed.sim.observers.alloc(obs->name.c_str(),
                                             simulation_observation_update,
                                             &sim_ed,
                                             obs_map->id,
                                             0);
                sim_ed.sim.observe(*mdl, output);
            }
        }
    }

    return status::success;
}

static status simulation_init_observation(component_editor&  ed,
                                          simulation_editor& sim_ed,
                                          tree_node&         head) noexcept
{
    simulation_observation* mem = nullptr;
    while (sim_ed.sim_obs.next(mem)) {
        mem->raw_ring_buffer.clear();
        mem->linear_ring_buffer.clear();
    }

    sim_ed.sim.observers.clear();

    vector<tree_node*> stack;
    stack.emplace_back(&head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = ed.mod.components.try_to_get(compo_id);

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

static void simulation_copy(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    sim_ed.simulation_state = simulation_status::initializing;
    simulation_clear(ed, sim_ed);

    sim_ed.sim.clear();
    sim_ed.simulation_current = sim_ed.simulation_begin;

    auto* head = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    if (!head) {
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_copy_tree(ed, sim_ed, *head); is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message, "Copy hierarchy failed: {}", status_string(ret));
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_copy_models(ed, sim_ed, *head); is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message, "Copy model failed: {}", status_string(ret));
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_copy_connections(ed, sim_ed, *head);
        is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message, "Copy connection failed: {}", status_string(ret));
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_init_observation(ed, sim_ed, *head);
        is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message,
               "Initialization of observation failed: {}",
               status_string(ret));
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    if (auto ret = sim_ed.sim.initialize(sim_ed.simulation_begin);
        is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(
          n.message, "Models initialization models: {}", status_string(ret));
        app->notifications.enable(n);

        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    sim_ed.simulation_state = simulation_status::initialized;
}

static void simulation_init(component_editor&  ed,
                            simulation_editor& sim_ed) noexcept
{
    sim_ed.simulation_state   = simulation_status::initializing;
    sim_ed.simulation_current = sim_ed.simulation_begin;

    sim_ed.tl.reset();

    auto* head = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    if (!head) {
        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    simulation_observation* mem = nullptr;
    while (sim_ed.sim_obs.next(mem)) {
        mem->raw_ring_buffer.clear();
        mem->linear_ring_buffer.clear();
    }

    if (auto ret = sim_ed.sim.initialize(sim_ed.simulation_begin);
        is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(
          n.message, "Models initialization models: {}", status_string(ret));
        app->notifications.enable(n);

        sim_ed.simulation_state = simulation_status::not_started;
        return;
    }

    sim_ed.simulation_state = simulation_status::initialized;
}

static void simulation_clear_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    simulation_clear(g_task->app->c_editor, g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static void simulation_copy_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    g_task->app->s_editor.force_pause = false;
    g_task->app->s_editor.force_stop  = false;
    simulation_copy(g_task->app->c_editor, g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static void simulation_init_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    g_task->app->s_editor.force_pause = false;
    g_task->app->s_editor.force_stop  = false;
    simulation_init(g_task->app->c_editor, g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static status debug_run(simulation_editor& sim_ed) noexcept
{
    if (auto ret = run(sim_ed.tl, sim_ed.sim, sim_ed.simulation_current);
        is_bad(ret)) {
        auto* app = container_of(&sim_ed, &application::s_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Debug run error";
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret;
    }

    return status::success;
}

static status run(simulation_editor& sim_ed) noexcept
{
    if (auto ret = sim_ed.sim.run(sim_ed.simulation_current); is_bad(ret)) {
        auto* app = container_of(&sim_ed, &application::s_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Run error";
        app->notifications.enable(n);
        sim_ed.simulation_state = simulation_status::finish_requiring;
        return ret;
    }

    return status::success;
}

static void task_simulation_static_run(simulation_editor& sim_ed) noexcept
{
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
    sim_ed.simulation_state = simulation_status::finishing;

    if (sim_ed.store_all_changes)
        finalize(sim_ed.tl, sim_ed.sim, sim_ed.simulation_current);
    else
        sim_ed.sim.finalize(sim_ed.simulation_end);

    sim_ed.simulation_state = simulation_status::finished;
}

//
//
//

static void task_simulation_run(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    task_simulation_run(g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static void task_simulation_run_1(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    task_simulation_run_1(g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static void simulation_pause_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;

    g_task->app->s_editor.force_pause = true;

    g_task->state = gui_task_status::finished;
}

static void task_simulation_stop(void* param) noexcept
{
    auto* g_task = reinterpret_cast<gui_task*>(param);

    g_task->app->s_editor.force_stop = true;

    g_task->state = gui_task_status::finished;
}

static void task_simulation_finish(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    task_simulation_finish(g_task->app->c_editor, g_task->app->s_editor);

    g_task->state = gui_task_status::finished;
}

static void task_enable_or_disable_debug(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    g_task->app->s_editor.tl.reset();

    if (g_task->app->s_editor.store_all_changes) {
        auto ret = initialize(g_task->app->s_editor.tl,
                              g_task->app->s_editor.sim,
                              g_task->app->s_editor.simulation_current);

        if (is_bad(ret)) {
            auto& n =
              g_task->app->notifications.alloc(notification_type::error);
            n.title = "Debug mode failed to initialize";
            format(n.message,
                   "Fail to initialize the debug mode: {}",
                   status_string(ret));
            g_task->app->notifications.enable(n);
            g_task->app->s_editor.simulation_state =
              simulation_status::not_started;
        }
    }

    g_task->state = gui_task_status::finished;
}

//
// public API
//

void simulation_editor::simulation_update_state() noexcept
{
    if (simulation_state == simulation_status::paused) {
        simulation_state = simulation_status::run_requiring;
        auto* app        = container_of(this, &application::s_editor);
        auto& task       = app->gui_tasks.alloc();
        task.app         = app;
        app->task_mgr.task_lists[0].add(task_simulation_run, &task);
        app->task_mgr.task_lists[0].submit();
    }

    if (simulation_state == simulation_status::finish_requiring) {
        simulation_state = simulation_status::finishing;
        auto* app        = container_of(this, &application::s_editor);
        auto& task       = app->gui_tasks.alloc();
        task.app         = app;
        app->task_mgr.task_lists[0].add(task_simulation_finish, &task);
        app->task_mgr.task_lists[0].submit();
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
        auto* app = container_of(this, &application::s_editor);
        auto& mod = app->c_editor.mod;

        auto* modeling_head = mod.tree_nodes.try_to_get(mod.head);
        if (!modeling_head) {
            auto& notif = app->notifications.alloc(notification_type::error);
            notif.title = "Empty model";
            app->notifications.enable(notif);
        } else {
            app->s_editor.clear();

            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(simulation_copy_impl, &task);
            app->task_mgr.task_lists[0].submit();
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
        if (auto* parent = tree_nodes.try_to_get(head); parent) {
            auto* app  = container_of(this, &application::s_editor);
            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(simulation_init_impl, &task);
            app->task_mgr.task_lists[0].submit();
        }
    }
}

void simulation_editor::simulation_clear() noexcept
{
    if (auto* parent = tree_nodes.try_to_get(head); parent) {
        auto* app  = container_of(this, &application::s_editor);
        auto& task = app->gui_tasks.alloc();
        task.app   = app;
        app->task_mgr.task_lists[0].add(simulation_clear_impl, &task);
        app->task_mgr.task_lists[0].submit();
    }
}

void simulation_editor::simulation_start() noexcept
{
    bool state = match(simulation_state,
                       simulation_status::initialized,
                       simulation_status::pause_forced);

    irt_assert(state);

    if (state) {
        if (auto* parent = tree_nodes.try_to_get(head); parent) {
            auto* app  = container_of(this, &application::s_editor);
            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(task_simulation_run, &task);
            app->task_mgr.task_lists[0].submit();
        }
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
        if (auto* parent = tree_nodes.try_to_get(head); parent) {
            auto* app  = container_of(this, &application::s_editor);
            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(task_simulation_run_1, &task);
            app->task_mgr.task_lists[0].submit();
        }
    }
}

void simulation_editor::simulation_pause() noexcept
{
    bool state = match(simulation_state, simulation_status::running);

    irt_assert(state);

    if (state) {
        if (auto* parent = tree_nodes.try_to_get(head); parent) {
            auto* app  = container_of(this, &application::s_editor);
            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(simulation_pause_impl, &task);
            app->task_mgr.task_lists[0].submit();
        }
    }
}

void simulation_editor::simulation_stop() noexcept
{
    bool state = match(
      simulation_state, simulation_status::running, simulation_status::paused);

    irt_assert(state);

    if (state) {
        if (auto* parent = tree_nodes.try_to_get(head); parent) {
            auto* app  = container_of(this, &application::s_editor);
            auto& task = app->gui_tasks.alloc();
            task.app   = app;
            app->task_mgr.task_lists[0].add(task_simulation_stop, &task);
            app->task_mgr.task_lists[0].submit();
        }
    }
}

void simulation_editor::simulation_advance() noexcept
{
    if (auto* parent = tree_nodes.try_to_get(head); parent) {
        simulation_state      = simulation_status::debugged;
        have_use_back_advance = true;

        auto* app  = container_of(this, &application::s_editor);
        auto& task = app->gui_tasks.alloc();
        task.app   = app;
        app->task_mgr.task_lists[0].add(task_simulation_advance, &task);
        app->task_mgr.task_lists[0].submit();
    }
}

void simulation_editor::simulation_back() noexcept
{
    if (auto* parent = tree_nodes.try_to_get(head); parent) {
        simulation_state      = simulation_status::debugged;
        have_use_back_advance = true;

        auto* app  = container_of(this, &application::s_editor);
        auto& task = app->gui_tasks.alloc();
        task.app   = app;
        app->task_mgr.task_lists[0].add(task_simulation_back, &task);
        app->task_mgr.task_lists[0].submit();
    }
}

void simulation_editor::enable_or_disable_debug() noexcept
{
    if (auto* parent = tree_nodes.try_to_get(head); parent) {
        auto* app  = container_of(this, &application::s_editor);
        auto& task = app->gui_tasks.alloc();
        task.app   = app;
        app->task_mgr.task_lists[0].add(task_enable_or_disable_debug, &task);
        app->task_mgr.task_lists[0].submit();
    }
}

} // namespace irt