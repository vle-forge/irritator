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
    return app.mod.ids.read([&](const auto& ids,
                                auto) noexcept -> tree_node_id {
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
            const auto& compo         = ids.components[tn.id];
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

                if (stack.back().children_read and
                    not stack.back().pop_required)
                    stack.pop_back(); // Optimization: do not let sibling into
                                      // the stack. The stack size is now
                                      // limited to the max component depth.

                if (auto* sibling = tn.tree.get_sibling())
                    stack.emplace_back(pj_ed.pj.tree_nodes.get_id(*sibling));
            }
        }

        return next_selection;
    });
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

        if (pj_id != exclude and pj.pj.name == name)
            return true;
    }

    return false;
}

static bool show_registred_obseravation_path(application&    app,
                                             project_editor& ed) noexcept
{
    static auto show_registred_path = false;
    const auto  old_observation_dir = ed.pj.observation_dir;

    app.mod.files.read([&](const auto& fs, const auto /*vers*/) {
        const auto* reg_dir =
          fs.registred_paths.try_to_get(ed.pj.observation_dir);
        const auto preview = reg_dir ? reg_dir->name.c_str() : "-";

        if (ImGui::BeginCombo("Path##Obs", preview)) {
            if (ImGui::Selectable("-", reg_dir == nullptr)) {
                ed.pj.observation_dir = undefined<registred_path_id>();
            }

            for (const auto& r : fs.registred_paths) {
                const auto r_id = fs.registred_paths.get_id(r);
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
              fs.registred_paths.try_to_get(ed.pj.observation_dir)) {
            HelpMarker(rr->path.c_str());
        } else {
            if (ImGui::Button("+"))
                show_registred_path = true;
        }
    });

    if (show_registred_path) {
        ImGui::OpenPopup("Select new output path");
        if (app.f_dialog.show_select_directory("Select new output path")) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.mod.files.write([&](auto& fs) {
                    if (fs.registred_paths.can_alloc(1)) {
                        auto& path            = fs.registred_paths.alloc();
                        ed.pj.observation_dir = fs.registred_paths.get_id(path);
                        path.path             = reinterpret_cast<const char*>(
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
                });
            }
            show_registred_path = false;
            app.f_dialog.clear();
        }
    }

    return old_observation_dir != ed.pj.observation_dir;
}

static bool EnhancedButton(const application& app,
                           const char*        label,
                           const ImVec2       button_size,
                           const char*        msg) noexcept
{
    ImGui::PushFont(app.icons);
    const auto click = ImGui::Button(label, button_size);
    ImGui::PopFont();

    if (msg and ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", msg);

    return click;
}

static void show_simulation_action_buttons(application&    app,
                                           project_editor& ed) noexcept
{
    const auto item_x         = ImGui::GetStyle().ItemSpacing.x;
    const auto region_x       = ImGui::GetContentRegionAvail().x;
    const auto button_x       = (region_x - 4.f * item_x) / 5.f;
    const auto small_button_x = (region_x - 2.f * item_x) / 3.f;
    const auto button         = ImVec2{ button_x, 0.f };
    const auto small_button   = ImVec2{ small_button_x, 0.f };

    const bool can_be_initialized = !any_equal(ed.simulation_state,
                                               simulation_status::not_started,
                                               simulation_status::finished,
                                               simulation_status::initialized,
                                               simulation_status::not_started);

    const bool can_be_started =
      !any_equal(ed.simulation_state, simulation_status::initialized);

    const bool can_be_paused = !any_equal(ed.simulation_state,
                                          simulation_status::running,
                                          simulation_status::run_requiring,
                                          simulation_status::paused);

    const bool can_be_restarted =
      !any_equal(ed.simulation_state, simulation_status::pause_forced);

    const bool can_be_stopped = !any_equal(ed.simulation_state,
                                           simulation_status::running,
                                           simulation_status::run_requiring,
                                           simulation_status::paused,
                                           simulation_status::pause_forced);

    ImGui::BeginDisabled(can_be_initialized);
    if (EnhancedButton(
          app, "\ue0c8", button, "Initialize simulation models and data."))
        ed.start_simulation_init(app);
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_started);
    if (EnhancedButton(app, "\ue0a9", button, "Start and run the simulation"))
        ed.start_simulation_start(app);
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_paused);
    if (EnhancedButton(app, "\ue09e", button, "Pause the simulation"))
        ed.force_pause = true;
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_restarted);
    if (EnhancedButton(
          app, "\ue030", button, "Restart the simulation after pause")) {
        ed.simulation_state = simulation_status::run_requiring;
        ed.force_pause      = false;
        ed.start_simulation_start(app);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_stopped);
    if (EnhancedButton(
          app,
          "\ue0cb",
          button,
          "Stop the simulation (can not be restart at the same state"))
        ed.force_stop = true;
    ImGui::EndDisabled();

    const bool history_mode = (ed.store_all_changes || ed.allow_user_changes) &&
                              (can_be_started || can_be_restarted);

    if (history_mode) {
        if (EnhancedButton(
              app, "\ue0c6", small_button, "Advance simulation step by step"))
            ed.start_simulation_step_by_step(app);

        ImGui::SameLine();

        const auto have_snaphot = ed.store_all_changes and not ed.snaps.empty();

        ImGui::BeginDisabled(not have_snaphot);
        if (EnhancedButton(
              app, "\ue0b3", small_button, "Rewind into simulation states"))
            ed.start_simulation_back(app);
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(not have_snaphot);
        if (EnhancedButton(
              app, "\ue05c", small_button, "Forward into simulation states"))
            ed.start_simulation_advance(app);
        ImGui::EndDisabled();
    }
}

static bool show_project_simulation_settings(application&    app,
                                             project_editor& ed) noexcept
{
    auto up     = 0;
    auto begin  = ed.pj.sim.limits.begin();
    auto end    = ed.pj.sim.limits.end();
    auto is_inf = std::isinf(end);

    name_str name = ed.pj.name;
    if (ImGui::InputFilteredString(
          "Name", name, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (not project_name_already_exists(
              app, app.pjs.get_id(ed), name.sv())) {
            ed.pj.name      = name;
            ed.is_dock_init = false;
        }
    }

    app.mod.files.read([&](const auto& fs, auto) noexcept {
        const auto selected = ed.file_select.combobox(
          app,
          fs,
          file_path::file_type::project_file,
          file_selector::flags(file_selector::flag::show_save_button));
        if (selected.save and not ed.save_in_progress.test_and_set()) {
            const auto pj_id = app.pjs.get_id(ed);
            ed.pj.file       = selected.file_id;

            app.add_gui_task([&app, pj_id]() {
                auto& ed = app.pjs.get(pj_id);

                if (auto ret = ed.pj.save(app.mod, app.jn); ret) {
                    app.jn.push(
                      log_level::info,
                      [&](auto& title, auto& /*msg*/) noexcept {
                          app.mod.files.read(
                            [&](const auto& fs, const auto /*vers*/) {
                                format(title,
                                       "Saving project file {} success",
                                       fs.file_paths.get(ed.pj.file).path.sv());
                            });
                      });
                } else {
                    app.jn.push(
                      log_level::error, [&](auto& title, auto& msg) noexcept {
                          const small_string<127> name = app.mod.files.read(
                            [&](const auto& fs, const auto /*vers*/) {
                                const auto* f =
                                  fs.file_paths.try_to_get(ed.pj.file);
                                return f ? f->path.sv()
                                         : std::string_view{ "-" };
                            });

                          format(
                            title, "Saving project file {} error", name.sv());

                          if (ret.error().cat() == category::project) {
                              if (static_cast<project_errc>(
                                    ret.error().value()) ==
                                  project_errc::file_access_error) {
                                  msg = "Access error.";
                              }
                          } else if (ret.error().cat() == category::json) {
                              switch (
                                static_cast<json_errc>(ret.error().value())) {
                              case json_errc::memory_error:
                                  format(msg,
                                         "json archiving memory error: not "
                                         "enough memory\n");
                                  break;

                              case json_errc::arg_error:
                                  format(msg,
                                         "json archiving internal error\n");
                                  break;

                              case json_errc::file_error:
                                  format(msg,
                                         "json archiving memory error: not "
                                         "enough memory\n");
                                  break;

                              case json_errc::invalid_project_format:
                                  format(msg,
                                         "json archiving json format error "
                                         "`{}' at offset "
                                         "{}\n",
                                         0,
                                         0);
                                  break;

                              default:
                                  format(msg,
                                         "json de-archiving unknown error\n");
                              }
                          }
                      });
                }

                ed.save_in_progress.clear();
            });
        }
    });

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

    show_simulation_action_buttons(app, ed);

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
