// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

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
    auto  compo_id = head->id;
    auto* compo    = ed.mod.components.try_to_get(compo_id);

    if (compo) {
        irt_return_if_bad(simulation_copy_model(ed, *head, *compo));

        vector<tree_node*> stack;
        stack.emplace_back(head);

        while (!stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();

            compo_id = cur->id;
            compo    = ed.mod.components.try_to_get(compo_id);

            if (compo)
                irt_return_if_bad(simulation_copy_model(ed, *cur, *compo));

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child(); child)
                stack.emplace_back(child);
        }
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
    auto  compo_id = head->id;
    auto* compo    = ed.mod.components.try_to_get(compo_id);

    if (compo) {
        irt_return_if_bad(simulation_copy_connections(ed, *head, *compo));

        vector<tree_node*> stack;
        stack.emplace_back(head);

        while (!stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();

            compo_id = cur->id;
            compo    = ed.mod.components.try_to_get(compo_id);

            if (compo)
                irt_return_if_bad(
                  simulation_copy_connections(ed, *cur, *compo));

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child(); child)
                stack.emplace_back(child);
        }
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
    simulation_clear(ed);

    auto* head = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    if (head) {
        if (auto ret = simulation_copy_models(ed, head); is_bad(ret)) {
            ed.state |= component_editor_status_simulation_not_ready;
            return;
        }

        if (auto ret = simulation_copy_connections(ed, head); is_bad(ret)) {
            ed.state |= component_editor_status_simulation_not_ready;
            return;
        }

        ed.state |= component_editor_status_simulation_ready;
    }
}

static void simulation_clear_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    simulation_clear(*g_task->ed);

    g_task->ed->sim.clean();
    g_task->state = gui_task_status::finished;
}

static void simulation_init_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->ed->state |= component_editor_status_read_only_simulating |
                         component_editor_status_read_only_modeling;

    simulation_init(*g_task->ed);

    g_task->ed->sim.clean();
    g_task->state = gui_task_status::finished;
}

void component_editor::simulation_init() noexcept
{
    if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
        auto& task = gui_tasks.alloc();
        task.ed    = this;
        task_mgr.task_lists[0].add(simulation_init_impl, &task);
        task_mgr.task_lists[0].submit();
    }
}

void component_editor::simulation_clear() noexcept
{
    if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent) {
        auto& task = gui_tasks.alloc();
        task.ed    = this;
        task_mgr.task_lists[0].add(simulation_clear_impl, &task);
        task_mgr.task_lists[0].submit();
    }
}

} // namespace irt