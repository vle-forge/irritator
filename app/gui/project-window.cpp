// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <irritator/archiver.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include "imgui.h"

namespace irt {

/** Show the hierarchy of @c tree_node in a @c ImGui::TreeNode and @c
 * ImGui::Selectable widgets. This function returns the selected @c
 * tree_node_id. */
static tree_node_id show_project_hierarchy(application& app,
                                           tree_node&   parent,
                                           tree_node_id id) noexcept;

static tree_node_id show_tree_node_children(application& app,
                                            tree_node&   parent,
                                            component&   compo,
                                            tree_node_id id) noexcept
{
    debug::ensure(parent.tree.get_child() != nullptr);

    const auto parent_id      = app.pj.tree_nodes.get_id(parent);
    auto       is_selected    = parent_id == id;
    auto       next_selection = id;
    auto       copy_selected  = is_selected;

    auto is_open = ImGui::TreeNodeExSelectableWithHint(
      compo.name.c_str(),
      component_type_names[ordinal(compo.type)],
      &is_selected,
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);

    if (copy_selected != is_selected)
        next_selection = is_selected ? parent_id : undefined<tree_node_id>();

    if (is_open) {
        if (auto* child = parent.tree.get_child(); child) {
            auto selection =
              show_project_hierarchy(app, *child, next_selection);
            if (selection != next_selection)
                next_selection = selection;
        }

        ImGui::TreePop();
    }

    return next_selection;
}

static tree_node_id show_tree_node_no_children(application& app,
                                               tree_node&   parent,
                                               component&   compo,
                                               tree_node_id id) noexcept
{
    debug::ensure(parent.tree.get_child() == nullptr);

    const auto parent_id      = app.pj.tree_nodes.get_id(parent);
    auto       next_selection = id;
    auto       is_selected    = parent_id == id;

    if (ImGui::SelectableWithHint(compo.name.c_str(),
                                  component_type_names[ordinal(compo.type)],
                                  &is_selected)) {
        if (is_selected)
            next_selection = id != parent_id ? parent_id : id;
        else
            next_selection = undefined<tree_node_id>();
    }

    return next_selection;
}

static tree_node_id show_project_hierarchy(application& app,
                                           tree_node&   parent,
                                           tree_node_id id) noexcept
{
    tree_node_id ret = id;

    if (auto* compo = app.mod.components.try_to_get(parent.id); compo) {
        ImGui::PushID(&parent);

        const auto have_children = parent.tree.get_child() != nullptr;
        const auto selection =
          have_children ? show_tree_node_children(app, parent, *compo, id)
                        : show_tree_node_no_children(app, parent, *compo, id);

        if (selection != id)
            ret = selection;

        ImGui::PopID();

        if (auto* sibling = parent.tree.get_sibling(); sibling) {
            const auto selection = show_project_hierarchy(app, *sibling, id);
            if (id != selection)
                ret = selection;
        }
    }

    return ret;
}

static inline constexpr std::string_view simulation_status_names[] = {
    "not_started", "initializing", "initialized",  "run_requiring",
    "running",     "paused",       "pause_forced", "finish_requiring",
    "finishing",   "finished",     "debugged",
};

static bool show_project_simulation_settings(application& app) noexcept
{
    auto& sim_ed = app.simulation_ed;
    auto  up     = 0;

    up += ImGui::InputReal("Begin", &sim_ed.simulation_begin);
    ImGui::BeginDisabled(sim_ed.infinity_simulation);
    up += ImGui::InputReal("End", &sim_ed.simulation_end);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(not sim_ed.real_time);
    {
        i64 value = sim_ed.simulation_time_duration.count();

        if (ImGui::InputScalar("ms/u.t.", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                sim_ed.simulation_time_duration =
                  std::chrono::milliseconds(value);
                ++up;
            }
        }
        ImGui::SameLine();
        HelpMarker("Duration in milliseconds per unit of simulation time. "
                   "Default is to "
                   "run 1 unit time of simulation in one second.");
    }
    ImGui::EndDisabled();

    {
        i64 value = sim_ed.simulation_task_duration.count();

        if (ImGui::InputScalar("ms/task", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                sim_ed.simulation_task_duration =
                  std::chrono::milliseconds(value);
                ++up;
            }
        }
        ImGui::SameLine();
        HelpMarker("Duration in milliseconds per simulation task. Lower "
                   "value may increase CPU load.");
    }

    ImGui::BeginDisabled(sim_ed.is_simulation_running());
    up += ImGui::Checkbox("Enable live edition", &sim_ed.allow_user_changes);
    if (ImGui::Checkbox("Store simulation", &sim_ed.store_all_changes)) {
        ++up;
        if (sim_ed.store_all_changes and
            sim_ed.simulation_state == simulation_status::running) {
            sim_ed.start_enable_or_disable_debug();
        }
    }

    up += ImGui::Checkbox("No time limit", &sim_ed.infinity_simulation);
    up += ImGui::Checkbox("Real time", &sim_ed.real_time);
    ImGui::EndDisabled();

    ImGui::LabelFormat("time", "{:.6f}", sim_ed.simulation_display_current);
    ImGui::SameLine();
    HelpMarker("Display the simulation current time.");

    ImGui::LabelFormat(
      "phase", "{}", simulation_status_names[ordinal(sim_ed.simulation_state)]);
    ImGui::SameLine();
    HelpMarker("Display the simulation phase. Only for debug.");

    return up > 0;
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
    auto& app = container_of(this, &application::project_wnd);

    if (id != m_selected_tree_node) {
        m_selected_tree_node = undefined<tree_node_id>();
        m_selected_child     = undefined<child_id>();

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
        m_selected_tree_node = undefined<tree_node_id>();
        m_selected_child     = undefined<child_id>();

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
    if (!parent)
        return;

    auto next_selection = m_selected_tree_node;

    if (ImGui::BeginTabBar("Project")) {
        if (ImGui::BeginTabItem("Settings")) {
            show_project_simulation_settings(app);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hierarchy")) {
            if (ImGui::BeginChild("##zone", ImGui::GetContentRegionAvail())) {
                auto selection =
                  show_project_hierarchy(app, *parent, m_selected_tree_node);
                if (selection != m_selected_tree_node)
                    next_selection = selection;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (next_selection != m_selected_tree_node)
        app.project_wnd.select(next_selection);
}

} // namespace irt
