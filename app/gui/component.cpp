// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

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

static status save_component_impl(modeling&             mod,
                                  component&            compo,
                                  const registred_path& reg_path,
                                  const dir_path&       dir,
                                  const file_path&      file) noexcept
{
    try {
        std::filesystem::path p{ reg_path.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return new_error(
                  old_status::io_filesystem_make_directory_error);
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return new_error(old_status::io_filesystem_not_directory_error);
            }
        }

        p /= file.path.sv();
        p.replace_extension(".irt");

        cache_rw      cache;
        json_archiver j;
        irt_check(j.component_save(mod, compo, cache, p.string().c_str()));
    } catch (...) {
        return new_error(old_status::io_not_enough_memory);
    }

    return success();
}

void task_save_component(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->app->mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg =
          g_task->app->mod.registred_paths.try_to_get(compo->reg_path);
        auto* dir  = g_task->app->mod.dir_paths.try_to_get(compo->dir);
        auto* file = g_task->app->mod.file_paths.try_to_get(compo->file);

        if (reg && dir && file) {
            if (auto ret = save_component_impl(
                  g_task->app->mod, *compo, *reg, *dir, *file);
                !ret)
                compo->state = component_status::unmodified;
            else
                compo->state = component_status::modified;
        }
    }

    g_task->state = task_status::finished;
}

static status save_component_description_impl(const registred_path& reg_dir,
                                              const dir_path&       dir,
                                              const file_path&      file,
                                              const description& desc) noexcept
{
    try {
        std::filesystem::path p{ reg_dir.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return new_error(
                  old_status::io_filesystem_make_directory_error);
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return new_error(old_status::io_filesystem_not_directory_error);
            }
        }

        p /= file.path.sv();
        p.replace_extension(".desc");

        std::ofstream ofs{ p };
        if (ofs.is_open()) {
            ofs.write(desc.data.c_str(), desc.data.size());
        } else {
            return new_error(old_status::io_filesystem_error);
        }
    } catch (...) {
        return new_error(old_status::io_not_enough_memory);
    }

    return success();
}

void task_save_description(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->app->mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg =
          g_task->app->mod.registred_paths.try_to_get(compo->reg_path);
        auto* dir  = g_task->app->mod.dir_paths.try_to_get(compo->dir);
        auto* file = g_task->app->mod.file_paths.try_to_get(compo->file);
        auto* desc = g_task->app->mod.descriptions.try_to_get(compo->desc);

        if (dir && file && desc) {
            if (auto ret =
                  save_component_description_impl(*reg, *dir, *file, *desc);
                ret) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
            }
        }
    }

    g_task->state = task_status::finished;
}

} // irt
