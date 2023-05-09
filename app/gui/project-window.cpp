// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"
#include "irritator/io.hpp"
#include "irritator/modeling.hpp"

namespace irt {

static void show_project_hierarchy(application& app, tree_node& parent) noexcept
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

    if (auto* compo = app.mod.components.try_to_get(parent.id); compo) {
        ImGui::PushID(&parent);
        small_string<64> str;
        bool             can_open = false;

        switch (compo->type) {
        case component_type::simple:
            format(str, "{} generic", compo->name.sv());
            can_open = true;
            break;
        case component_type::grid:
            format(str, "{} grid", compo->name.sv());
            flags |= ImGuiTreeNodeFlags_Leaf;
            break;
        default:
            format(str, "{}", compo->name.sv());
            break;
        }

        bool is_selected = app.project_wnd.is_selected(app.pj.node(parent));
        if (is_selected)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool is_open = ImGui::TreeNodeEx(str.c_str(), flags);

        if (ImGui::IsItemClicked())
            app.project_wnd.select(parent);

        if (is_open) {
            if (can_open) {
                if (auto* child = parent.tree.get_child(); child)
                    show_project_hierarchy(app, *child);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(app, *sibling);
    }
}

void project_window::clear() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    app->pj.clear();
}

bool project_window::is_selected(tree_node_id id) const noexcept
{
    return m_selected_tree_node == id;
}

bool project_window::is_selected(child_id id) const noexcept
{
    return m_selected_child == id;
}

void project_window::select(tree_node_id id) noexcept
{
    if (id != m_selected_tree_node) {
        auto* app = container_of(this, &application::project_wnd);

        if (auto* tree = app->pj.node(id); tree) {
            if (auto* compo = app->mod.components.try_to_get(tree->id); compo) {
                m_selected_tree_node = id;
                m_selected_child     = undefined<child_id>();
            }
        }
    }
}

void project_window::select(tree_node& node) noexcept
{
    auto* app = container_of(this, &application::project_wnd);
    auto  id  = app->pj.node(node);

    if (id != m_selected_tree_node) {
        if (auto* compo = app->mod.components.try_to_get(node.id); compo) {
            m_selected_tree_node = id;
            m_selected_child     = undefined<child_id>();
        }
    }
}

void project_window::select(child_id id) noexcept
{
    if (id != m_selected_child)
        m_selected_child = id;
}

void project_window::show() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    auto* parent = app->pj.tn_head();
    if (!parent) {
        clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags))
        show_project_hierarchy(*app, *parent);
}

void project_window::save(const char* filename) noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    app->cache.clear();

    auto* head  = app->pj.tn_head();
    auto* compo = app->mod.components.try_to_get(app->pj.head());

    if (!head || !compo) {
        auto& n = app->notifications.alloc(log_level::error);
        n.title = "Empty project";
        app->notifications.enable(n);
    } else {
        if (auto ret = app->pj.save(app->mod, app->sim, app->cache, filename);
            is_bad(ret)) {
            auto& n = app->notifications.alloc(log_level::error);
            n.title = "Save project fail";
            format(n.message, "Can not access file `{}'", filename);
            app->notifications.enable(n);
        } else {
            app->mod.state = modeling_status::unmodified;
            auto& n        = app->notifications.alloc(log_level::notice);
            n.title        = "The file was saved successfully.";
            app->notifications.enable(n);
        }
    }
}

void project_window::load(const char* filename) noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    app->cache.clear();

    if (auto ret = app->pj.load(app->mod, app->sim, app->cache, filename);
        is_bad(ret)) {
        auto& n = app->notifications.alloc(log_level::error);
        n.title = "Load project fail";
        format(n.message, "Can not access file `{}'", filename);
        app->notifications.enable(n);
    } else {
        auto& n = app->notifications.alloc(log_level::notice);
        n.title = "The file was loaded successfully.";
        app->notifications.enable(n);
        app->mod.state = modeling_status::unmodified;
    }
}

// Tasks to load/save project file.

void task_load_project(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->pj.load(g_task->app->mod,
                             g_task->app->sim,
                             g_task->app->cache,
                             file->path.c_str());
        g_task->app->mod.registred_paths.free(*file);
    }

    g_task->state = task_status::finished;
}

void task_save_project(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto  id   = enum_cast<registred_path_id>(g_task->param_1);
    auto* file = g_task->app->mod.registred_paths.try_to_get(id);
    if (file) {
        g_task->app->pj.save(g_task->app->mod,
                             g_task->app->sim,
                             g_task->app->cache,
                             file->path.c_str());
        g_task->app->mod.registred_paths.free(*file);
    }

    g_task->state = task_status::finished;
}

} // namespace irt
