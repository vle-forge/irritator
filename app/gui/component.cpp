// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/error.hpp"

namespace irt {

// static void print_tree(const data_array<component, component_id>& components,
//                        tree_node* top_id) noexcept
// {
//     struct node
//     {
//         node(tree_node* tree_) noexcept
//           : tree(tree_)
//           , i(0)
//         {
//         }

//         node(tree_node* tree_, int i_) noexcept
//           : tree(tree_)
//           , i(i_)
//         {
//         }

//         tree_node* tree;
//         int        i = 0;
//     };

//     vector<node> stack;
//     stack.emplace_back(top_id);

//     while (!stack.empty()) {
//         auto cur = stack.back();
//         stack.pop_back();

//         auto  compo_id = cur.tree->id;
//         auto* compo    = components.try_to_get(compo_id);

//         if (compo) {
//             fmt::print("{:{}} {}\n", " ", cur.i, fmt::ptr(compo));
//         }

//         if (auto* sibling = cur.tree->tree.get_sibling(); sibling) {
//             stack.emplace_back(sibling, cur.i);
//         }

//         if (auto* child = cur.tree->tree.get_child(); child) {
//             stack.emplace_back(child, cur.i + 1);
//         }
//     }
// }

// static void unselect_editor_component_ref(component_editor_data& data)
// noexcept
// {
//     ImNodes::EditorContextSet(data.context);
//     data.selected_component = undefined<tree_node_id>();

//     ImNodes::ClearLinkSelection();
//     ImNodes::ClearNodeSelection();
//     data.selected_links.clear();
//     data.selected_nodes.clear();
// }

void component_editor::add_generic_component() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    if (app.mod.can_alloc_generic_component()) {
        auto& compo = app.mod.alloc_generic_component();

        auto& notif = app.notifications.alloc(log_level::notice);
        notif.title = "Component";

        format(notif.message,
               "Generic component {} allocated: {}\n, Tree nodes allocated: {}",
               compo.name.c_str(),
               app.mod.components.size(),
               app.pj.tree_nodes_size().first);
    } else {
        auto& notif = app.notifications.alloc(log_level::error);
        notif.title = "Can not allocate new generic component";

        format(notif.message,
               "Components allocated: {}\n, Tree nodes allocated: {}",
               app.mod.components.size(),
               app.pj.tree_nodes_size().first);
    }
}

void component_editor::add_grid_component() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    if (app.mod.can_alloc_grid_component()) {
        auto& compo = app.mod.alloc_grid_component();

        auto& notif = app.notifications.alloc(log_level::notice);
        notif.title = "Component";

        format(notif.message,
               "Grid component {} allocated: {}\nTree nodes allocated: {}",
               compo.name.c_str(),
               app.mod.components.size(),
               app.pj.tree_nodes_size().first);
    } else {
        auto& notif = app.notifications.alloc(log_level::error);
        notif.title = "Can not allocate new generic component";

        format(notif.message,
               "Components allocated: {}\nTree nodes allocated: {}",
               app.mod.components.size(),
               app.pj.tree_nodes_size().first);
    }
}

// void component_editor_data ::unselect() noexcept
// {
//     auto& app = container_of(this, &application::component_ed);
//     app.mod.clear_project();
//     unselect_editor_component_ref(*this);
// }

//
// task implementation
//

} // irt
