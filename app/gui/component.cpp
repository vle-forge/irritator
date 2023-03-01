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

static void unselect_editor_component_ref(component_editor& ed) noexcept
{
    ImNodes::EditorContextSet(ed.context);
    ed.selected_component = undefined<tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();
    ed.selected_links.clear();
    ed.selected_nodes.clear();
}

component_id component_editor::add_empty_component() noexcept
{
    auto ret = undefined<component_id>();

    if (mod.components.can_alloc() && mod.simple_components.can_alloc()) {
        auto& new_compo = mod.components.alloc();
        new_compo.name.assign("New component");
        new_compo.type  = component_type::simple;
        new_compo.state = component_status::modified;

        auto& new_s_compo      = mod.simple_components.alloc();
        new_compo.id.simple_id = mod.simple_components.get_id(new_s_compo);

        ret = mod.components.get_id(new_compo);
    } else {
        auto* app   = container_of(this, &application::component_ed);
        auto& notif = app->notifications.alloc(log_level::error);
        notif.title = "Can not allocate new container";
        format(notif.message,
               "Components allocated: {}\nTree nodes allocated: {}",
               mod.components.size(),
               mod.tree_nodes.size());
    }

    return ret;
}

void component_editor::unselect() noexcept
{
    mod.clear_project();
    unselect_editor_component_ref(*this);
}

void component_editor::select(tree_node_id id) noexcept
{
    if (auto* tree = mod.tree_nodes.try_to_get(id); tree) {
        if (auto* compo = mod.components.try_to_get(tree->id); compo) {
            unselect_editor_component_ref(*this);

            selected_component  = id;
            force_node_position = true;
        }
    }
}

void component_editor::open_as_main(component_id id) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        unselect();

        tree_node_id parent_id;
        if (is_success(mod.make_tree_from(*compo, &parent_id))) {
            mod.head            = parent_id;
            selected_component  = parent_id;
            force_node_position = true;
        }
    }
}

//
// task implementation
//

static status save_component_impl(const modeling&       mod,
                                  const component&      compo,
                                  const registred_path& reg_path,
                                  const dir_path&       dir,
                                  const file_path&      file) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ reg_path.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return status::io_filesystem_make_directory_error;
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return status::io_filesystem_not_directory_error;
            }
        }

        p /= file.path.sv();
        p.replace_extension(".irt");

        json_cache cache;
        irt_return_if_bad(
          component_save(mod, compo, cache, p.string().c_str()));
    } catch (...) {
        ret = status::io_not_enough_memory;
    }

    return ret;
}

void task_save_component(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo = g_task->app->component_ed.mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg = g_task->app->component_ed.mod.registred_paths.try_to_get(
          compo->reg_path);
        auto* dir =
          g_task->app->component_ed.mod.dir_paths.try_to_get(compo->dir);
        auto* file =
          g_task->app->component_ed.mod.file_paths.try_to_get(compo->file);

        if (reg && dir && file) {
            if (is_bad(save_component_impl(
                  g_task->app->component_ed.mod, *compo, *reg, *dir, *file))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
                compo->type  = component_type::simple;
            }
        }
    }

    g_task->state = task_status::finished;
}

static status save_component_description_impl(const registred_path& reg_dir,
                                              const dir_path&       dir,
                                              const file_path&      file,
                                              const description& desc) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ reg_dir.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return status::io_filesystem_make_directory_error;
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return status::io_filesystem_not_directory_error;
            }
        }

        p /= file.path.sv();
        p.replace_extension(".desc");

        std::ofstream ofs{ p };
        if (ofs.is_open()) {
            ofs.write(desc.data.c_str(), desc.data.size());
        } else {
            ret = status::io_file_format_error;
        }
    } catch (...) {
        ret = status::io_not_enough_memory;
    }

    return ret;
}

void task_save_description(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo = g_task->app->component_ed.mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg = g_task->app->component_ed.mod.registred_paths.try_to_get(
          compo->reg_path);
        auto* dir =
          g_task->app->component_ed.mod.dir_paths.try_to_get(compo->dir);
        auto* file =
          g_task->app->component_ed.mod.file_paths.try_to_get(compo->file);
        auto* desc =
          g_task->app->component_ed.mod.descriptions.try_to_get(compo->desc);

        if (dir && file && desc) {
            if (is_bad(
                  save_component_description_impl(*reg, *dir, *file, *desc))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
            }
        }
    }

    g_task->state = task_status::finished;
}

} // irt
