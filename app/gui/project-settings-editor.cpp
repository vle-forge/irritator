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
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "imgui.h"

namespace irt {

static tree_node_id show_project_hierarchy(application&    app,
                                           project_editor& pj_ed,
                                           tree_node&      root,
                                           tree_node_id    selection) noexcept
{
    struct elem {
        explicit constexpr elem(const tree_node_id id) noexcept
          : tn(id)
        {}

        tree_node_id tn;

        bool children_read = false;
        bool sibling_read  = false;
        bool pop_required  = false;
    };

    small_vector<elem, max_component_stack_size> stack;

    auto next_selection = selection;

    stack.emplace_back(pj_ed.pj.tree_nodes.get_id(root));

    while (not stack.empty()) {
        if (stack.back().children_read and stack.back().sibling_read) {
            if (stack.back().pop_required)
                ImGui::TreePop();
            stack.pop_back();
            continue;
        }

        const auto  tn_id         = stack.back().tn;
        const auto& tn            = pj_ed.pj.tree_nodes.get(tn_id);
        const auto& compo         = app.mod.components.get<component>(tn.id);
        auto        is_selected   = tn_id == selection;
        const auto  copy_selected = is_selected;

        const auto name =
          format_n<127>("{} ({})", compo.name.sv(), tn.unique_id.sv());

        if (not stack.back().children_read) {
            stack.back().children_read = true;
            if (not tn.tree.get_child()) {
                if (ImGui::SelectableWithHint(
                      name.c_str(),
                      component_type_names[ordinal(compo.type)],
                      &is_selected)) {
                    next_selection =
                      is_selected ? (selection != tn_id ? tn_id : selection)
                                  : undefined<tree_node_id>();
                }
            } else {
                const auto open = ImGui::TreeNodeExSelectableWithHint(
                  name.c_str(),
                  component_type_names[ordinal(compo.type)],
                  &is_selected,
                  ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_SpanAvailWidth);

                if (copy_selected != is_selected)
                    next_selection =
                      is_selected ? tn_id : undefined<tree_node_id>();

                if (open) {
                    stack.back().pop_required = true;
                    stack.emplace_back(
                      pj_ed.pj.tree_nodes.get_id(*tn.tree.get_child()));
                }
            }
            continue;
        }

        if (not stack.back().sibling_read) {
            stack.back().sibling_read = true;

            if (stack.back().children_read and not stack.back().pop_required)
                stack.pop_back(); // Optimization: do not let sibling into the
                                  // stack. The stack size is now limited to the
                                  // max component depth.

            if (auto* sibling = tn.tree.get_sibling())
                stack.emplace_back(pj_ed.pj.tree_nodes.get_id(*sibling));
        }
    }

    return next_selection;
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

    if (ImGui::BeginCombo("Path##Obs", preview)) {
        if (ImGui::Selectable("-", reg_dir == nullptr)) {
            ed.pj.observation_dir = undefined<registred_path_id>();
        }

        for (const auto& r : app.mod.registred_paths) {
            const auto r_id = app.mod.registred_paths.get_id(r);
            ImGui::PushID(static_cast<int>(ordinal(r_id)));
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

static void show_project_file_access(application&    app,
                                     project_editor& ed) noexcept
{
    auto&                        pj    = ed.pj;
    static constexpr const char* empty = "";

    const auto* f = app.mod.file_paths.try_to_get(ed.pj.file);

    auto*       reg_dir     = app.mod.registred_paths.try_to_get(pj.reg);
    const char* reg_preview = reg_dir ? reg_dir->path.c_str() : empty;

    if (ImGui::BeginCombo("Path##FileAccess", reg_preview)) {
        registred_path* list = nullptr;
        while (app.mod.registred_paths.next(list)) {
            if (list->status == registred_path::state::error)
                continue;

            if (ImGui::Selectable(list->path.c_str(),
                                  reg_dir == list,
                                  ImGuiSelectableFlags_None)) {
                pj.reg  = app.mod.registred_paths.get_id(list);
                reg_dir = list;
            }
        }
        ImGui::EndCombo();
    }

    if (reg_dir) {
        auto* dir         = app.mod.dir_paths.try_to_get(pj.dir);
        auto* dir_preview = dir ? dir->path.c_str() : empty;

        if (ImGui::BeginCombo("Dir", dir_preview)) {
            if (ImGui::Selectable("##empty-dir", dir == nullptr)) {
                pj.dir = undefined<dir_path_id>();
                dir    = nullptr;
            }

            dir_path* list = nullptr;
            while (app.mod.dir_paths.next(list)) {
                if (ImGui::Selectable(list->path.c_str(), dir == list)) {
                    pj.dir = app.mod.dir_paths.get_id(list);
                    dir    = list;
                }
            }
            ImGui::EndCombo();
        }

        if (dir == nullptr) {
            directory_path_str dir_name;

            if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                const auto exists = reg_dir->children.read(
                  [&](const auto& vec, const auto /*ver*/) {
                      return path_exist(app.mod.dir_paths, vec, dir_name.sv());
                  });

                if (exists) {
                    auto& new_dir  = app.mod.dir_paths.alloc();
                    auto  dir_id   = app.mod.dir_paths.get_id(new_dir);
                    auto  reg_id   = app.mod.registred_paths.get_id(*reg_dir);
                    new_dir.parent = reg_id;
                    new_dir.path   = dir_name;
                    new_dir.status = dir_path::state::unread;

                    reg_dir->children.write(
                      [&](auto& vec) { vec.emplace_back(dir_id); });

                    pj.reg = reg_id;
                    pj.dir = dir_id;

                    if (!app.mod.create_directories(new_dir)) {
                        app.jn.push(log_level::error,
                                    [&](auto& title, auto& /*msg*/) noexcept {
                                        format(title,
                                               "Fail to create directory {}",
                                               new_dir.path.sv());
                                    });
                    }
                }
            }
        }

        if (dir) {
            auto* file = app.mod.file_paths.try_to_get(pj.file);
            if (!file) {
                auto& f  = app.mod.file_paths.alloc();
                auto  id = app.mod.file_paths.get_id(f);
                f.parent = app.mod.dir_paths.get_id(*dir);
                pj.file  = id;

                dir->children.write([&](auto& vec) { vec.emplace_back(id); });

                file = &f;
            }

            if (ImGui::InputFilteredString("File##text", file->path)) {
                if (not has_extension(file->path.sv(),
                                      file_path::file_type::project_file)) {
                    add_extension(file->path,
                                  file_path::file_type::project_file);
                }
            }

            const auto is_save_enabled = is_valid_filename(
              file->path.sv(), file_path::file_type::project_file);

            ImGui::BeginDisabled(!is_save_enabled);
            if (ImGui::Button("Save")) {
                const auto pj_id = app.pjs.get_id(ed);
                debug::ensure(app.pjs.try_to_get(pj_id));
                app.start_save_project(pj_id);
            }

            ImGui::EndDisabled();
        }
    }
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

    show_project_file_access(app, ed);

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
      "Binary or Text: writes graph and all transition for all models "
      "during "
      "the simulation. A csv file format is used and the file is opened "
      "into "
      "the observation directroy defined above.\nPlease note, the file "
      "may be large.");

    app.sim_to_cpp.show(std::as_const(ed));

    return up > 0;
}

void project_editor::show_settings_and_hierarchy(application& app) noexcept
{
    if (auto* parent = pj.tn_head(); parent) {
        auto next_selection = m_selected_tree_node;

        if (ImGui::BeginTabBar("Project")) {
            if (ImGui::BeginTabItem("Settings")) {
                if (ImGui::BeginChild("###settings",
                                      ImGui::GetContentRegionAvail()))
                    show_project_simulation_settings(app, *this);

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hierarchy")) {
                if (ImGui::BeginChild("###hierarchy",
                                      ImGui::GetContentRegionAvail())) {
                    const auto selection = show_project_hierarchy(
                      app, *this, *parent, m_selected_tree_node);
                    if (selection != m_selected_tree_node)
                        next_selection = selection;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (next_selection != m_selected_tree_node)
            select(app, next_selection);
    }
}

} // namespace irt
