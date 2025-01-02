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
                                           project&     pj,
                                           tree_node&   parent,
                                           tree_node_id id) noexcept;

static tree_node_id show_tree_node_children(application& app,
                                            project&     pj,
                                            tree_node&   parent,
                                            component&   compo,
                                            tree_node_id id) noexcept
{
    debug::ensure(parent.tree.get_child() != nullptr);

    const auto parent_id      = pj.tree_nodes.get_id(parent);
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
              show_project_hierarchy(app, pj, *child, next_selection);
            if (selection != next_selection)
                next_selection = selection;
        }

        ImGui::TreePop();
    }

    return next_selection;
}

static tree_node_id show_tree_node_no_children(application& app,
                                               project&     pj,
                                               tree_node&   parent,
                                               component&   compo,
                                               tree_node_id id) noexcept
{
    debug::ensure(parent.tree.get_child() == nullptr);

    const auto parent_id      = pj.tree_nodes.get_id(parent);
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
                                           project&     pj,
                                           tree_node&   parent,
                                           tree_node_id id) noexcept
{
    tree_node_id ret = id;

    if (auto* compo = app.mod.components.try_to_get(parent.id); compo) {
        ImGui::PushID(&parent);

        const auto have_children = parent.tree.get_child() != nullptr;
        const auto selection =
          have_children
            ? show_tree_node_children(app, pj, parent, *compo, id)
            : show_tree_node_no_children(app, pj, parent, *compo, id);

        if (selection != id)
            ret = selection;

        ImGui::PopID();

        if (auto* sibling = parent.tree.get_sibling(); sibling) {
            const auto selection =
              show_project_hierarchy(app, pj, *sibling, id);
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

static constexpr bool project_name_already_exists(
  const application&     app,
  const project_id       exclude,
  const std::string_view name) noexcept
{
    for (const auto& pj : app.pjs) {
        const auto pj_id = app.pjs.get_id(pj);

        if (pj_id != exclude and pj.name == name)
            return true;
    }

    return false;
}

static bool show_project_simulation_settings(application&    app,
                                             project_window& ed) noexcept
{
    auto up     = 0;
    auto begin  = ed.pj.t_limit.begin();
    auto end    = ed.pj.t_limit.end();
    auto is_inf = std::isinf(end);

    name_str name = ed.name;
    if (ImGui::InputFilteredString(
          "Name", name, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (not project_name_already_exists(app, app.pjs.get_id(ed), name.sv()))
            ed.name = name;
    }

    if (ImGui::InputReal("Begin", &begin))
        ed.pj.t_limit.set_bound(begin, end);

    if (ImGui::Checkbox("No time limit", &is_inf))
        ed.pj.t_limit.set_bound(begin,
                                is_inf ? time_domain<time>::infinity : 100.);

    ImGui::BeginDisabled(is_inf);
    if (ImGui::InputReal("End", &end))
        ed.pj.t_limit.set_bound(begin, end);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(not ed.real_time);
    {
        i64 value = ed.simulation_time_duration.count();

        if (ImGui::InputScalar("ms/u.t.", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                ed.simulation_time_duration = std::chrono::milliseconds(value);
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
        i64 value = ed.simulation_task_duration.count();

        if (ImGui::InputScalar("ms/task", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                ed.simulation_task_duration = std::chrono::milliseconds(value);
                ++up;
            }
        }
        ImGui::SameLine();
        HelpMarker("Duration in milliseconds per simulation task. Lower "
                   "value may increase CPU load.");
    }

    ImGui::BeginDisabled(ed.is_simulation_running());
    up += ImGui::Checkbox("Enable live edition", &ed.allow_user_changes);
    if (ImGui::Checkbox("Store simulation", &ed.store_all_changes)) {
        ++up;
        if (ed.store_all_changes and
            ed.simulation_state == simulation_status::running) {
            ed.start_enable_or_disable_debug(app);
        }
    }

    up += ImGui::Checkbox("Real time", &ed.real_time);
    ImGui::EndDisabled();

    ImGui::LabelFormat("time", "{:.6f}", ed.simulation_display_current);
    ImGui::SameLine();
    HelpMarker("Display the simulation current time.");

    ImGui::LabelFormat(
      "phase", "{}", simulation_status_names[ordinal(ed.simulation_state)]);
    ImGui::SameLine();
    HelpMarker("Display the simulation phase. Only for debug.");

    return up > 0;
}

void project_settings_widgets::show(project_window& ed) noexcept
{
    auto& app = container_of(this, &application::project_wnd);

    if (auto* parent = ed.pj.tn_head(); parent) {
        auto next_selection = ed.m_selected_tree_node;

        if (ImGui::BeginTabBar("Project")) {
            if (ImGui::BeginTabItem("Settings")) {
                if (ImGui::BeginChild("###PjHidden",
                                      ImGui::GetContentRegionMax()))
                    show_project_simulation_settings(app, ed);

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hierarchy")) {
                auto selection = show_project_hierarchy(
                  app, ed.pj, *parent, ed.m_selected_tree_node);
                if (selection != ed.m_selected_tree_node)
                    next_selection = selection;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (next_selection != ed.m_selected_tree_node)
            ed.select(app.mod, next_selection);
    }
}

} // namespace irt
