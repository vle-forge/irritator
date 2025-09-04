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

    small_string<127> str;
    format(str, "{} ({})", compo.name.sv(), parent.unique_id.sv());

    auto is_open = ImGui::TreeNodeExSelectableWithHint(
      str.c_str(),
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

static tree_node_id show_tree_node_no_children(application& /*app*/,
                                               project&     pj,
                                               tree_node&   parent,
                                               component&   compo,
                                               tree_node_id id) noexcept
{
    debug::ensure(parent.tree.get_child() == nullptr);

    const auto parent_id      = pj.tree_nodes.get_id(parent);
    auto       next_selection = id;
    auto       is_selected    = parent_id == id;

    small_string<127> str;
    format(str, "{} ({})", compo.name.sv(), parent.unique_id.sv());

    if (ImGui::SelectableWithHint(str.c_str(),
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

    if (auto* compo = app.mod.components.try_to_get<component>(parent.id)) {
        ImGui::PushID(&parent);

        const auto have_children = parent.tree.get_child() != nullptr;
        const auto child_selection =
          have_children
            ? show_tree_node_children(app, pj, parent, *compo, id)
            : show_tree_node_no_children(app, pj, parent, *compo, id);

        if (child_selection != id)
            ret = child_selection;

        ImGui::PopID();

        if (auto* sibling = parent.tree.get_sibling(); sibling) {
            const auto sibling_selection =
              show_project_hierarchy(app, pj, *sibling, id);
            if (id != sibling_selection)
                ret = sibling_selection;
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

static bool show_registred_obseravation_path(application&    app,
                                             project_editor& ed) noexcept
{
    static bool show_registred_path = false;
    const auto  old_observation_dir = ed.pj.observation_dir;

    const auto* reg_dir =
      app.mod.registred_paths.try_to_get(ed.pj.observation_dir);
    const auto preview = reg_dir ? reg_dir->name.c_str() : "-";

    if (ImGui::BeginCombo("Path", preview)) {
        if (ImGui::Selectable("-", reg_dir == nullptr)) {
            ed.pj.observation_dir = undefined<registred_path_id>();
        }

        for (const auto& r : app.mod.registred_paths) {
            const auto r_id = app.mod.registred_paths.get_id(r);
            ImGui::PushID(ordinal(r_id));
            if (ImGui::Selectable(r.name.c_str(),
                                  ed.pj.observation_dir == r_id)) {
                ed.pj.observation_dir = r_id;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (const auto* rr =
          app.mod.registred_paths.try_to_get(ed.pj.observation_dir)) {
        HelpMarker(rr->path.c_str());
    } else {
        if (ImGui::Button("+"))
            show_registred_path = true;
    }

    if (show_registred_path) {
        ImGui::OpenPopup("Select new output path");
        if (app.f_dialog.show_select_directory("Select new output path")) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.mod.registred_paths.alloc();
                    ed.pj.observation_dir =
                      app.mod.registred_paths.get_id(path);
                    path.path = reinterpret_cast<const char*>(
                      app.f_dialog.result.u8string().c_str());
                    path.name =
                      app.f_dialog.result.has_stem()
                        ? reinterpret_cast<const char*>(
                            app.f_dialog.result.stem().u8string().c_str())
                        : reinterpret_cast<const char*>(
                            app.f_dialog.result.parent_path()
                              .stem()
                              .u8string()
                              .c_str());
                }
            }
            show_registred_path = false;
            app.f_dialog.clear();
        }
    }

    return old_observation_dir != ed.pj.observation_dir;
}

static bool show_project_simulation_settings(application&    app,
                                             project_editor& ed) noexcept
{
    auto up     = 0;
    auto begin  = ed.pj.sim.limits.begin();
    auto end    = ed.pj.sim.limits.end();
    auto is_inf = std::isinf(end);

    name_str name = ed.name;
    if (ImGui::InputFilteredString(
          "Name", name, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (not project_name_already_exists(app, app.pjs.get_id(ed), name.sv()))
            ed.name = name;
    }

    if (ImGui::InputReal("Begin", &begin))
        ed.pj.sim.limits.set_bound(begin, end);

    if (ImGui::Checkbox("No time limit", &is_inf))
        ed.pj.sim.limits.set_bound(begin,
                                   is_inf ? time_domain<time>::infinity : 100.);

    ImGui::BeginDisabled(is_inf);
    if (ImGui::InputReal("End", &end))
        ed.pj.sim.limits.set_bound(begin, end);
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
      "phase",
      "{}",
      simulation_status_names[ordinal(ed.simulation_state.load())]);

    ImGui::SameLine();
    HelpMarker("Display the simulation phase. Only for debug.");

    ImGui::SeparatorText("Save");

    const auto sz = ImGui::ComputeButtonSize(2);
    ImGui::BeginDisabled(is_undefined(ed.project_file));
    if (ImGui::Button("Save", sz))
        ed.save_project_file = true;
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ImGui::Button("Save as...", sz))
        ed.save_as_project_file = true;

    ImGui::SeparatorText("Observation");

    up += show_registred_obseravation_path(app, ed);

    static const char* raw_data_type_str[] = {
        "None",
        "Graph (dot file)",
        "Binary (dot file + all transitions)",
        "Text (dot file + all transitions)"
    };

    int current = ordinal(ed.save_simulation_raw_data);

    if (ImGui::Combo("Type",
                     &current,
                     raw_data_type_str,
                     IM_ARRAYSIZE(raw_data_type_str))) {
        if (current != ordinal(ed.save_simulation_raw_data)) {
            ed.save_simulation_raw_data =
              enum_cast<project_editor::raw_data_type>(current);
            ++up;
        }
    }

    ImGui::SameLine();
    HelpMarker(
      "None: do nothing.\n"
      "Graph: writes the simulation graph using a dot format into the "
      "observation directory path defined above"
      "Binary or Text: writes graph and all transition for all models during "
      "the simulation. A csv file format is used and the file is opened into "
      "the observation directroy defined above.\nPlease note, the file "
      "may be large.");

    app.sim_to_cpp.show(std::as_const(ed));

    return up > 0;
}

void project_settings_editor::show(project_editor& ed) noexcept
{
    auto& app = container_of(this, &application::project_wnd);

    if (auto* parent = ed.pj.tn_head(); parent) {
        auto next_selection = ed.m_selected_tree_node;

        if (ImGui::BeginTabBar("Project")) {
            if (ImGui::BeginTabItem("Settings")) {
                if (ImGui::BeginChild("###settings",
                                      ImGui::GetContentRegionAvail()))
                    show_project_simulation_settings(app, ed);

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hierarchy")) {
                if (ImGui::BeginChild("###hierarchy",
                                      ImGui::GetContentRegionAvail())) {
                    auto selection = show_project_hierarchy(
                      app, ed.pj, *parent, ed.m_selected_tree_node);
                    if (selection != ed.m_selected_tree_node)
                        next_selection = selection;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (next_selection != ed.m_selected_tree_node)
            ed.select(app, next_selection);
    }
}

} // namespace irt
