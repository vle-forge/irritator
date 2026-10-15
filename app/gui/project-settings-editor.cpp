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
    return app.mod.ids.read(
      [&](const auto& ids, auto) noexcept -> tree_node_id {
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

              const auto  tn_id       = stack.back().tn;
              const auto& tn          = *pj_ed.pj.tree_nodes.try_to_get(tn_id);
              const auto& compo       = ids.components[tn.id];
              auto        is_selected = tn_id == selection;
              const auto  copy_selected = is_selected;

              const auto name = format_n<127>("{} ({})", compo.name.sv(),
                                              tn.unique_id.sv());

              if (not stack.back().children_read) {
                  stack.back().children_read = true;
                  if (not tn.tree.get_child()) {
                      if (ImGui::SelectableWithHint(
                            name.c_str(),
                            component_type_names[ordinal(compo.type)],
                            &is_selected)) {
                          next_selection = is_selected
                                             ? (selection != tn_id ? tn_id
                                                                   : selection)
                                             : undefined<tree_node_id>();
                      }
                  } else {
                      const auto open = ImGui::TreeNodeExSelectableWithHint(
                        name.c_str(), component_type_names[ordinal(compo.type)],
                        &is_selected,
                        ImGuiTreeNodeFlags_OpenOnArrow |
                          ImGuiTreeNodeFlags_SpanAvailWidth);

                      if (copy_selected != is_selected)
                          next_selection = is_selected
                                             ? tn_id
                                             : undefined<tree_node_id>();

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
        const auto* reg_dir = fs.registred_paths.try_to_get(
          ed.pj.observation_dir);
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
        if (const auto* rr = fs.registred_paths.try_to_get(
              ed.pj.observation_dir)) {
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
                        path.name = app.f_dialog.result.has_stem()
                                      ? reinterpret_cast<const char*>(
                                          app.f_dialog.result.stem()
                                            .u8string()
                                            .c_str())
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

template<class Fn>
static bool submit(application&       app,
                   unsigned           idx,
                   player_controller& c,
                   Fn                 fn) noexcept
{
    if (c.busy.test_and_set(std::memory_order_acquire))
        return false;

    app.add_simulation_task(idx, [&c, fn,
                                  edited = std::exchange(c.edited,
                                                         false)]() noexcept {
        c.result = edited ? c.player.forget_history() : success();
        if (not c.result.has_error())
            c.result = fn(c.player);

        c.fresh.step     = c.player.step();
        c.fresh.furthest = c.player.furthest_step();
        c.fresh.earliest = c.player.earliest_step();
        c.fresh.t        = c.player.current_time();
        c.fresh.target   = c.pacer.target();
        c.fresh
          .lateness_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                            c.pacer.lateness(c.player.next_time()))
                            .count();
        c.fresh.resyncs = c.resyncs;
        c.fresh.started = c.player.started();
        c.fresh.finished  = c.player.finished();
        c.fresh.can_back  = c.player.can_step_back();
        c.fresh.keyframes = c.player.keyframe_number();
        c.fresh.bytes     = c.player.memory_usage();
        c.has_result      = true;
        c.busy.clear(std::memory_order_release); // TOUJOURS en dernier
    });

    return true;
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

void schedule_player(application&       app,
                     const unsigned     idx,
                     player_controller& c) noexcept
{
    using mode = player_controller::mode;

    if (c.busy.test(std::memory_order_acquire))
        return;

    if (c.has_result) {
        c.has_result = false;
        c.shown      = c.fresh;

        if (c.result.has_error()) {
            c.current = mode::paused;
            log(log_level::error, [&](auto& m) noexcept {
                format(m, "Simulation player error: {}", c.result.error());
            });
        }
    }

    switch (c.current) {
    case mode::paused:
        break;

    case mode::play:
        if (c.shown.finished)
            c.current = mode::paused;
        else
            submit(app, idx, c,
                   [steps = c.steps_per_frame, budget = c.frame_budget](
                     auto& p) { return p.advance(budget, steps); });
        break;

    case mode::reverse:
        if (not c.shown.can_back)
            c.current = mode::paused;
        else
            submit(app, idx, c,
                   [steps = c.steps_per_frame, budget = c.frame_budget](
                     auto& p) { return p.retreat(budget, steps); });
        break;

    case mode::realtime:
        if (c.shown.finished) {
            c.current = mode::paused;
            break;
        }

        submit(app, idx, c,
               [&pacer = c.pacer, &resyncs = c.resyncs, unit = c.unit_duration,
                budget = c.task_duration, max_late = c.max_lateness,
                reset = std::exchange(c.pace_reset, false)](
                 auto& p) noexcept -> status {
                   const auto now = realtime_pacer::clock::now();

                   if (reset)
                       pacer.rebase(p.current_time(), now);

                   if (pacer.unit_duration() != unit)
                       pacer.set_unit_duration(unit, now);

                   if (pacer.resync_if_late(p.next_time(), max_late, now))
                       ++resyncs;

                   return p.advance_until(pacer.target(now), budget);
               });
        break;
    }
}

static void show_simulation_player(application&       app,
                                   const unsigned     idx,
                                   player_controller& c) noexcept
{
    const auto size = ImGui::ComputeButtonSize(5);

    if (EnhancedButton(app,
                       c.current == player_controller::mode::realtime
                         ? "\ue057##t"
                         : "RT##t",
                       size, "Real time")) {
        if (c.current == player_controller::mode::realtime) {
            c.current = player_controller::mode::paused;
        } else {
            c.current    = player_controller::mode::realtime;
            c.pace_reset = true;
        }
    }

    if (not c.shown.started) {
        if (EnhancedButton(app, "start", ImVec2{}, "Start the player"))
            submit(app, idx, c, [](auto& p) { return p.start(); });
        return;
    }

    const auto busy = c.busy.test(std::memory_order_acquire);
    ImGui::BeginDisabled(busy);
    if (EnhancedButton(app, "\ue0c5##c", size, "Beginning")) {
        c.current = player_controller::mode::paused;
        submit(app, idx, c, [](auto& p) { return p.go_to(0); });
    }

    ImGui::SameLine();

    if (EnhancedButton(app, "\ue02f##c", size, "Back")) {
        c.current = player_controller::mode::paused;
        submit(app, idx, c, [](auto& p) { return p.step_back(); });
    }

    ImGui::SameLine();

    if (EnhancedButton(app, "\ue030##c", size, "Forward")) {
        c.current = player_controller::mode::paused;
        submit(app, idx, c, [](auto& p) { return p.step_forward(); });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (EnhancedButton(app,
                       c.current == player_controller::mode::play ? "\ue09e##c"
                                                                  : "\ue034##c",
                       size, "Play"))
        c.current = (c.current == player_controller::mode::play)
                      ? player_controller::mode::paused
                      : player_controller::mode::play;

    ImGui::SameLine();

    if (EnhancedButton(app,
                       c.current == player_controller::mode::reverse
                         ? "\ue09e##r"
                         : "\ue033##r",
                       size, "Reverse"))
        c.current = (c.current == player_controller::mode::reverse)
                      ? player_controller::mode::paused
                      : player_controller::mode::reverse;

    auto zero = static_cast<ImU64>(0);
    auto last = static_cast<ImU64>(c.shown.furthest);

    if (not c.dragging)
        c.target = c.shown.step;

    ImGui::SliderScalar("step", ImGuiDataType_U64, &c.target, &zero, &last);
    c.dragging = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit() and
        not c.busy.test(std::memory_order_acquire)) {
        c.current = player_controller::mode::paused;
        submit(app, idx, c,
               [target = c.target](auto& p) { return p.go_to(target); });
    }

    ImGui::TextFormat("step {}, time {:g}", c.shown.step, c.shown.t);
    if (busy)
        ImGui::TextFormat("Work in progress...");

    if (c.current == player_controller::mode::realtime) {
        ImGui::TextFormat("target {:.3f}, lateness {} ms, resync {}",
                          c.shown.target, c.shown.lateness_ms, c.shown.resyncs);
        if (c.shown.lateness_ms > 0)
            ImGui::TextFormat("The simulation is slower than the real time.");
    }
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

    const bool can_be_initialized = any_equal(
      ed.pj.simulation_state, simulation_status::not_started,
      simulation_status::initialized, simulation_status::paused,
      simulation_status::finished);

    const bool can_be_started = ed.pj.simulation_state ==
                                simulation_status::initialized;

    const bool can_be_paused = ed.pj.simulation_state ==
                               simulation_status::running;

    const bool can_be_restarted = ed.pj.simulation_state ==
                                  simulation_status::paused;

    ImGui::BeginDisabled(not can_be_initialized);
    if (EnhancedButton(app, "\ue0c8", button,
                       "Initialize simulation models and data."))
        ed.init_simulation(app);
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(not can_be_started);
    if (EnhancedButton(app, "\ue0a9", button, "Start and run the simulation"))
        ed.run_simulation(app);
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(not can_be_paused);
    if (EnhancedButton(app, "\ue09e", button, "Pause the simulation"))
        ed.pause_simulation();
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(not can_be_restarted);
    if (EnhancedButton(app, "\ue030", button,
                       "Restart the simulation after pause")) {
        ed.pj.simulation_state = simulation_status::run_requiring;
        ed.force_pause         = false;
    }
    ImGui::EndDisabled();

    // ImGui::BeginDisabled(can_be_stopped);
    // if (EnhancedButton(
    //       app,
    //       "\ue0cb",
    //       button,
    //       "Stop the simulation (can not be restart at the same state"))
    //     ed.force_stop = true;
    // ImGui::EndDisabled();

    if (ed.pj.flags[project::simulation_flag::debug] /* and not start */) {
        ImGui::SeparatorText("Debug mode");
        ed.sim_player.unit_duration = ed.one_simulation_time_duration;
        ed.sim_player.task_duration = ed.simulation_task_duration;

        show_simulation_player(app, get_index(app.pjs.get_id(ed)),
                               ed.sim_player);
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
    if (ImGui::InputFilteredString("Name", name,
                                   ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (not project_name_already_exists(app, app.pjs.get_id(ed),
                                            name.sv())) {
            ed.pj.name      = name;
            ed.is_dock_init = false;
        }
    }

    app.mod.files.read([&](const auto& fs, auto) noexcept {
        const auto selected = ed.file_select.combobox(
          app, fs, file_type::project_file,
          file_selector::flags(file_selector::flag::show_save_button));
        if (selected.save and not ed.save_in_progress.test_and_set()) {
            const auto pj_id   = app.pjs.get_id(ed);
            ed.pj.project_file = selected.file_id;

            app.add_gui_task([&app, pj_id]() {
                auto& ed = app.pjs.get(pj_id);

                app.mod.files.read([&](const auto& fs, auto) noexcept {
                    app.mod.ids.read([&](const auto& ids, auto) noexcept {
                        if (auto ret = ed.pj.save(fs, ids); ret) {
                            log(log_level::info, [&](auto& msg) noexcept {
                                app.mod.files.read([&](const auto& fs,
                                                       const auto /*vers*/) {
                                    format(
                                      msg,
                                      "project: saving project file {} success",
                                      fs.file_paths.get(ed.pj.project_file)
                                        .path.sv());
                                });
                            });
                        } else {
                            log(log_level::error, [&](auto& msg) noexcept {
                                const path name = app.mod.files.read(
                                  [&](const auto& fs, const auto /*vers*/) {
                                      const auto* f = fs.file_paths.try_to_get(
                                        ed.pj.project_file);
                                      return f ? f->path.sv()
                                               : std::string_view{ "-" };
                                  });

                                format(
                                  msg,
                                  "project: saving project file {} error {}",
                                  name.sv(), ret.error());
                            });
                        }

                        ed.save_in_progress.clear();
                    });
                });
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

    ImGui::BeginDisabled(not ed.pj.flags[project::simulation_flag::real_time]);
    {
        i64 value = ed.one_simulation_time_duration.count();

        ImGui::BeginDisabled(
          not(ed.pj.flags[project::simulation_flag::real_time] or
              ed.pj.flags[project::simulation_flag::debug]));

        if (ImGui::InputScalar("ms/u.t.", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                ed.one_simulation_time_duration = std::chrono::milliseconds(
                  value);
                ++up;
            }
        }
        ImGui::EndDisabled();

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

    const auto started = any_equal(ed.pj.simulation_state,
                                   simulation_status::not_started,
                                   simulation_status::initialized);

    auto flags     = ed.pj.flags;
    auto debug     = ed.pj.flags[project::simulation_flag::debug];
    auto real_time = ed.pj.flags[project::simulation_flag::real_time];
    auto write_irb = ed.pj.flags[project::simulation_flag::write_irtb];

    ImGui::BeginDisabled(not started);
    if (ImGui::Checkbox("Debug", &debug))
        flags.set(project::simulation_flag::debug, debug);
    if (ImGui::Checkbox("Real time", &real_time))
        flags.set(project::simulation_flag::real_time, real_time);
    if (ImGui::Checkbox("Write on disk", &write_irb))
        flags.set(project::simulation_flag::write_irtb, write_irb);
    ImGui::EndDisabled();

    if (flags != ed.pj.flags) {
        ed.pj.flags            = flags; /* Disable start button */
        ed.pj.simulation_state = simulation_status::not_started;
    }

    ImGui::LabelFormat("time", "{:.6f}", ed.simulation_display_current);
    ImGui::SameLine();
    HelpMarker("Display the simulation current time.");

    ImGui::LabelFormat(
      "phase", "{}", simulation_status_names[ordinal(ed.pj.simulation_state)]);

    ImGui::SameLine();
    HelpMarker("Display the simulation phase. Only for debug.");

    show_simulation_action_buttons(app, ed);

    ImGui::SeparatorText("Observation");

    up += show_registred_obseravation_path(app, ed);

    static const char* raw_data_type_str[] = {
        "None", "Graph (dot file)", "Binary (dot file + all transitions)",
        "Text (dot file + all transitions)"
    };

    int current = ordinal(ed.save_simulation_raw_data);

    if (ImGui::Combo("Type", &current, raw_data_type_str,
                     IM_ARRAYSIZE(raw_data_type_str))) {
        if (current != ordinal(ed.save_simulation_raw_data)) {
            ed.save_simulation_raw_data = enum_cast<
              project_editor::raw_data_type>(current);
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
    if (auto* parent = pj.tree_nodes.try_to_get(pj.tn_head())) {
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
