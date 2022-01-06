// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static status simulation_copy_model(component_editor& ed,
                                    tree_node&        tree,
                                    const component&  src)
{
    tree.sim.data.clear();

    child* c = nullptr;
    while (src.children.next(c)) {
        if (c->type == child_type::model) {
            auto  mdl_id = enum_cast<model_id>(c->id);
            auto* mdl    = src.models.try_to_get(mdl_id);

            if (mdl) {
                irt_return_if_fail(ed.sim.models.can_alloc(),
                                   status::simulation_not_enough_model);

                auto& new_mdl    = ed.sim.clone(*mdl);
                auto  new_mdl_id = ed.sim.models.get_id(new_mdl);

                tree.sim.data.emplace_back(mdl_id, new_mdl_id);
            }
        }
    }

    return status::success;
}

static status simulation_copy_models(component_editor& ed,
                                     tree_node*        head) noexcept
{
    vector<tree_node*> stack;
    stack.emplace_back(head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = ed.mod.components.try_to_get(compo_id);

        if (compo)
            irt_return_if_bad(simulation_copy_model(ed, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    return status::success;
}

static status simulation_copy_connections(component_editor& ed,
                                          tree_node&        tree,
                                          const component&  compo)
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
                mdl_src   = ed.sim.models.try_to_get(*real_mdl_src);
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
                        auto      orig_src = cp_src->y[con->index_src].id;
                        i8        port_src = cp_src->y[con->index_src].index;
                        model_id* real_src = child->sim.get(orig_src);

                        if (real_src) {
                            mdl_src   = ed.sim.models.try_to_get(*real_src);
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
                mdl_dst   = ed.sim.models.try_to_get(*real_mdl_dst);
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
                        model_id  orig_dst = cp_dst->x[con->index_dst].id;
                        i8        port_dst = cp_dst->x[con->index_dst].index;
                        model_id* real_dst = child->sim.get(orig_dst);

                        if (real_dst) {
                            mdl_dst   = ed.sim.models.try_to_get(*real_dst);
                            index_dst = port_dst;
                        }
                    }
                }
            }
        }

        if (mdl_src && mdl_dst) {
            irt_return_if_bad(
              ed.sim.connect(*mdl_src, index_src, *mdl_dst, index_dst));
        }
    }

    return status::success;
}

static status simulation_copy_connections(component_editor& ed,
                                          tree_node*        head) noexcept
{
    vector<tree_node*> stack;
    stack.emplace_back(head);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = ed.mod.components.try_to_get(compo_id);

        if (compo)
            irt_return_if_bad(simulation_copy_connections(ed, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    return status::success;
}

static void simulation_clear(component_editor& ed) noexcept
{
    tree_node* tree = nullptr;
    while (ed.mod.tree_nodes.next(tree))
        tree->sim.data.clear();
}

static void simulation_init(component_editor& ed) noexcept
{
    ed.simulation_state = component_simulation_status::initializing;
    simulation_clear(ed);

    auto* head = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    if (!head) {
        ed.simulation_state = component_simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_copy_models(ed, head); is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message, "Copy model failed: {}", status_string(ret));
        app->notifications.enable(n);
        ed.simulation_state = component_simulation_status::not_started;
        return;
    }

    if (auto ret = simulation_copy_connections(ed, head); is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(n.message, "Copy connection failed: {}", status_string(ret));
        app->notifications.enable(n);
        ed.simulation_state = component_simulation_status::not_started;
        return;
    }

    ed.sim.clean();
    ed.simulation_current = ed.simulation_begin;

    if (auto ret = ed.sim.initialize(ed.simulation_begin); is_bad(ret)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc(notification_type::error);
        n.title   = "Simulation initialization fail";
        format(
          n.message, "Models initialization models: {}", status_string(ret));
        app->notifications.enable(n);

        ed.simulation_state = component_simulation_status::not_started;
        return;
    }

    ed.simulation_state = component_simulation_status::initialized;
}

static void simulation_clear_impl(void* param) noexcept
{
    // fmt::print("simulation_clear_impl\n");
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    simulation_clear(*g_task->ed);

    g_task->state = gui_task_status::finished;
    // fmt::print("simulation_clear_impl finished\n");
}

static void simulation_init_impl(void* param) noexcept
{
    // fmt::print("simulation_init_impl\n");
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    g_task->ed->force_pause = false;
    g_task->ed->force_stop  = false;
    simulation_init(*g_task->ed);

    g_task->state = gui_task_status::finished;
    // fmt::print("simulation_init_impl finished\n");
}

static void task_simulation_run(component_editor& ed) noexcept
{
    // fmt::print("simulation_run_impl\n");
    ed.simulation_state = component_simulation_status::running;
    namespace stdc      = std::chrono;

    auto start_at = stdc::high_resolution_clock::now();
    auto end_at   = stdc::high_resolution_clock::now();
    auto duration = end_at - start_at;

    auto duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
    auto duration_since_start = duration_cast.count();
    const decltype(duration_since_start) duration_in_microseconds = 1000000;

    bool stop_or_pause;

    do {
        // fmt::print("loop\n");
        if (ed.simulation_state != component_simulation_status::running)
            return;

        auto ret = ed.sim.run(ed.simulation_current);
        // fmt::print("{}\n", (int)ret);
        // fmt::print("time {} {} {}\n",
        //           ed.simulation_begin,
        //           ed.simulation_current,
        //           ed.simulation_end);

        if (is_bad(ret)) {
            ed.simulation_state = component_simulation_status::finish_requiring;
            return;
        }

        if (ed.simulation_current >= ed.simulation_end) {
            ed.simulation_state = component_simulation_status::finish_requiring;
            return;
        }

        end_at        = stdc::high_resolution_clock::now();
        duration      = end_at - start_at;
        duration_cast = stdc::duration_cast<stdc::microseconds>(duration);
        duration_since_start = duration_cast.count();

        // fmt::print(
        //  "duration: {}/{}\n", duration_since_start,
        //  duration_in_microseconds);

        stop_or_pause = ed.force_pause || ed.force_stop;
    } while (!stop_or_pause && duration_since_start < duration_in_microseconds);

    if (ed.force_pause) {
        ed.force_pause      = false;
        ed.simulation_state = component_simulation_status::pause_forced;
    } else if (ed.force_stop) {
        ed.force_stop       = false;
        ed.simulation_state = component_simulation_status::finish_requiring;
    } else {
        // fmt::print("simulation_run_impl finished\n");
        ed.simulation_state = component_simulation_status::paused;
    }
}

static void task_simulation_finish(component_editor& ed) noexcept
{
    // fmt::print("simulation_finish_impl\n");
    ed.simulation_state = component_simulation_status::finishing;

    ed.sim.finalize(ed.simulation_end);

    ed.simulation_state = component_simulation_status::finished;
    // fmt::print("simulation_finish_impl finished\n");
}

//
//
//

static void task_simulation_run(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    task_simulation_run(*g_task->ed);

    g_task->state = gui_task_status::finished;
}

static void simulation_pause_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;

    g_task->ed->force_pause = true;

    g_task->state = gui_task_status::finished;
}

static void task_simulation_stop(void* param) noexcept
{
    auto* g_task = reinterpret_cast<gui_task*>(param);

    g_task->ed->force_stop = true;

    g_task->state = gui_task_status::started;
    g_task->state = gui_task_status::finished;
}

static void task_simulation_finish(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    task_simulation_finish(*g_task->ed);

    g_task->state = gui_task_status::finished;
}

//
// public API
//

void component_editor::simulation_update_state() noexcept
{
    if (simulation_state == component_simulation_status::paused) {
        simulation_state = component_simulation_status::run_requiring;
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(task_simulation_run, &task);
            task_mgr.task_lists[0].submit();
        }
    }

    if (simulation_state == component_simulation_status::finish_requiring) {
        simulation_state = component_simulation_status::finishing;
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(task_simulation_finish, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

void component_editor::simulation_init() noexcept
{
    bool state = match(simulation_state,
                       component_simulation_status::initialized,
                       component_simulation_status::not_started,
                       component_simulation_status::finished);

    irt_assert(state);

    if (state) {
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(simulation_init_impl, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

void component_editor::simulation_clear() noexcept
{
    bool state =
      match(simulation_state, component_simulation_status::not_started);

    irt_assert(state);

    if (state) {
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(simulation_clear_impl, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

void component_editor::simulation_start() noexcept
{
    bool state = match(simulation_state,
                       component_simulation_status::initialized,
                       component_simulation_status::pause_forced);

    irt_assert(state);

    if (state) {
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(task_simulation_run, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

void component_editor::simulation_pause() noexcept
{
    bool state = match(simulation_state, component_simulation_status::running);

    irt_assert(state);

    if (state) {
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(simulation_pause_impl, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

void component_editor::simulation_stop() noexcept
{
    bool state = match(simulation_state,
                       component_simulation_status::running,
                       component_simulation_status::paused);

    irt_assert(state);

    if (state) {
        if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
            auto& task = gui_tasks.alloc();
            task.ed    = this;
            task_mgr.task_lists[0].add(task_simulation_stop, &task);
            task_mgr.task_lists[0].submit();
        }
    }
}

} // namespace irt