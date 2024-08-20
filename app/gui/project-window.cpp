// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"

#include <irritator/archiver.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static void show_project_hierarchy(application& app,
                                   tree_node&   parent) noexcept;

static void show_tree_node_children(application& app,
                                    tree_node&   parent,
                                    const char*  str) noexcept
{
    debug::ensure(str);
    debug::ensure(parent.tree.get_child() != nullptr);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    bool is_selected         = app.project_wnd.is_selected(app.pj.node(parent));
    if (is_selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool is_open = ImGui::TreeNodeEx(str, flags);

    if (ImGui::IsItemClicked())
        app.project_wnd.select(parent);

    if (is_open) {
        if (auto* child = parent.tree.get_child(); child)
            show_project_hierarchy(app, *child);

        ImGui::TreePop();
    }
}

static void show_tree_node_no_children(application& app,
                                       tree_node&   parent,
                                       const char*  str) noexcept
{
    debug::ensure(str);
    debug::ensure(parent.tree.get_child() == nullptr);

    bool is_selected = app.project_wnd.is_selected(app.pj.node(parent));

    if (ImGui::Selectable(str, &is_selected))
        app.project_wnd.select(parent);
}

static void show_project_hierarchy(application& app, tree_node& parent) noexcept
{
    if (auto* compo = app.mod.components.try_to_get(parent.id); compo) {
        ImGui::PushID(&parent);
        small_string<64> str;

        switch (compo->type) {
        case component_type::simple:
            format(str, "{} generic", compo->name.sv());
            break;
        case component_type::grid:
            format(str, "{} grid", compo->name.sv());
            break;
        case component_type::graph:
            format(str, "{} graph", compo->name.sv());
            break;
        default:
            format(str, "{}", compo->name.sv());
            break;
        }

        bool have_children = parent.tree.get_child() != nullptr;
        if (have_children)
            show_tree_node_children(app, parent, str.c_str());
        else
            show_tree_node_no_children(app, parent, str.c_str());

        ImGui::PopID();

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(app, *sibling);
    }
}

void project_window::clear() noexcept
{
    auto& app = container_of(this, &application::project_wnd);

    app.pj.clear();
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
        auto& app = container_of(this, &application::project_wnd);

        if (auto* tree = app.pj.node(id); tree) {
            if (auto* compo = app.mod.components.try_to_get(tree->id); compo) {
                m_selected_tree_node = id;
                m_selected_child     = undefined<child_id>();
            }
        }
    }
}

void project_window::select(tree_node& node) noexcept
{
    auto& app = container_of(this, &application::project_wnd);
    auto  id  = app.pj.node(node);

    if (id != m_selected_tree_node) {
        if (auto* compo = app.mod.components.try_to_get(node.id); compo) {
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
    auto& app = container_of(this, &application::project_wnd);

    auto* parent = app.pj.tn_head();
    if (!parent) {
        clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags))
        show_project_hierarchy(app, *parent);
}

} // namespace irt
