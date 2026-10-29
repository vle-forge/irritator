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

    app.add_simulation_task(
      idx, [&c, fn, edited = std::exchange(c.edited, false),
            history = c.view != player_controller::kind::complete]() noexcept {
          const auto t0 = std::chrono::steady_clock::now();

          // The complete mode keeps no keyframe, the others keep them. It is
          // a no-op when the player is already in the right mode.

          c.result = edited ? c.player.forget_history() : success();
          if (not c.result.has_error())
              c.result = c.player.keep_history(history);
          if (not c.result.has_error())
              c.result = fn(c.player);
          if (history and not c.result.has_error())
              c.player.flush_observers();

          c.work += std::chrono::steady_clock::now() - t0;

          c.fresh.step        = c.player.step();
          c.fresh.furthest    = c.player.furthest_step();
          c.fresh.earliest    = c.player.earliest_step();
          c.fresh.t           = c.player.current_time();
          c.fresh.begin       = c.player.begin_time();
          c.fresh.end         = c.player.end_time();
          c.fresh.target      = c.pacer.target();
          c.fresh.lateness_ms = std::chrono::duration_cast<
                                  std::chrono::milliseconds>(
                                  c.pacer.lateness(c.player.next_time()))
                                  .count();
          c.fresh
            .work_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         c.work)
                         .count();
          c.fresh.resyncs   = c.resyncs;
          c.fresh.started   = c.player.started();
          c.fresh.finished  = c.player.finished();
          c.fresh.can_back  = c.player.can_step_back();
          c.fresh.keyframes = c.player.keyframe_number();
          c.fresh.bytes     = c.player.memory_usage();
          c.has_result      = true;
          c.busy.clear(std::memory_order_release); // always last instruction
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

void project_editor::schedule_player(application& app) noexcept
{
    using mode = player_controller::mode;

    const auto id  = app.pjs.get_id(*this);
    const auto idx = app.get_task_manager_list(id);

    if (sim_player.busy.test(std::memory_order_acquire))
        return;

    if (sim_player.has_result) {
        sim_player.has_result = false;
        sim_player.shown      = sim_player.fresh;

        if (sim_player.result.has_error()) {
            sim_player.current = mode::paused;
            log(log_level::error, [&](auto& m) noexcept {
                format(m, "Simulation player error: {}",
                       sim_player.result.error());
            });
        }
    }

    // The simulation belongs to the task while the player is busy: it is
    // built again here, between two tasks.
    if (std::exchange(sim_player.need_rebuild, false)) {
        if (pj.simulation_init(app.mod).has_error()) {
            sim_player.current = mode::paused;
            return;
        }

        sim_player.player.reset();

        if (sim_player.current == mode::complete) {
            sim_player.need_start = true;
        } else {
            sim_player.current = mode::paused;
            submit(app, idx, sim_player, [](auto& p) { return p.start(); });
            return;
        }
    }

    switch (sim_player.current) {
    case mode::paused:
        break;

    case mode::play:
        if (sim_player.shown.finished)
            sim_player.current = mode::paused;
        else
            submit(app, idx, sim_player,
                   [steps  = sim_player.steps_per_frame,
                    budget = sim_player.frame_budget](auto& p) {
                       return p.advance(budget, steps);
                   });
        break;

    case mode::reverse:
        if (not sim_player.shown.can_back)
            sim_player.current = mode::paused;
        else
            submit(app, idx, sim_player,
                   [steps  = sim_player.steps_per_frame,
                    budget = sim_player.frame_budget](auto& p) {
                       return p.retreat(budget, steps);
                   });
        break;

    case mode::realtime:
        if (sim_player.shown.finished) {
            sim_player.current = mode::paused;
            break;
        }

        submit(app, idx, sim_player,
               [&pacer = sim_player.pacer, &resyncs = sim_player.resyncs,
                unit     = sim_player.unit_duration,
                budget   = sim_player.task_duration,
                max_late = sim_player.max_lateness,
                reset    = std::exchange(sim_player.pace_reset, false)](
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

    case mode::complete:
        // need_start: the user asks for a new run, even if the last one is
        // finished (shown.finished is the state of the last run).
        if (sim_player.shown.finished and not sim_player.need_start) {
            sim_player.current = mode::paused;
            break;
        }

        submit(app, idx, sim_player,
               [&work = sim_player.work, budget = sim_player.complete_budget,
                restart = std::exchange(sim_player.need_start, false)](
                 auto& p) noexcept -> status {
                   if (restart) {
                       work = std::chrono::nanoseconds::zero();

                       if (auto ret = p.start(); ret.has_error())
                           return ret;
                   }

                   return p.advance(budget);
               });
        break;
    }
}

static void show_restart_button(const application& app,
                                player_controller& c) noexcept
{
    if (EnhancedButton(app, "restart", ImVec2{},
                       "Build the simulation again and start it")) {
        c.current      = player_controller::mode::paused;
        c.need_rebuild = true;
    }
}

static void show_debug_player(application&    app,
                              project_editor& ed,
                              const unsigned  idx) noexcept
{
    using mode = player_controller::mode;

    auto& c = ed.sim_player;

    if (not c.shown.started) {
        if (EnhancedButton(app, "start", ImVec2{}, "Start the player"))
            c.need_rebuild = true;
        return;
    }

    const auto size = ImGui::ComputeButtonSize(5);
    const auto busy = c.busy.test(std::memory_order_acquire);

    ImGui::BeginDisabled(busy);
    if (EnhancedButton(app, "##c", size, "Beginning")) {
        c.current = mode::paused;
        submit(app, idx, c, [](auto& p) { return p.go_to(0); });
    }

    ImGui::SameLine();

    if (EnhancedButton(app, "##c", size, "Back")) {
        c.current = mode::paused;
        submit(app, idx, c, [](auto& p) { return p.step_back(); });
    }

    ImGui::SameLine();

    if (EnhancedButton(app, "##c", size, "Forward")) {
        c.current = mode::paused;
        submit(app, idx, c, [](auto& p) { return p.step_forward(); });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (EnhancedButton(app, c.current == mode::play ? "##c" : "##c", size,
                       "Play"))
        c.current = (c.current == mode::play) ? mode::paused : mode::play;

    ImGui::SameLine();

    if (EnhancedButton(app, c.current == mode::reverse ? "##r" : "##r",
                       size, "Reverse"))
        c.current = (c.current == mode::reverse) ? mode::paused : mode::reverse;

    auto zero = static_cast<ImU64>(0);
    auto last = static_cast<ImU64>(c.shown.furthest);

    if (not c.dragging)
        c.target = c.shown.step;

    ImGui::SliderScalar("step", ImGuiDataType_U64, &c.target, &zero, &last);
    c.dragging = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit() and
        not c.busy.test(std::memory_order_acquire)) {
        c.current = mode::paused;
        submit(app, idx, c,
               [target = c.target](auto& p) { return p.go_to(target); });
    }

    ImGui::TextFormat("step {}, time {:g}", c.shown.step, c.shown.t);
    if (busy)
        ImGui::TextFormat("Work in progress...");

    show_restart_button(app, c);
}

static void show_realtime_player(application& app, project_editor& ed) noexcept
{
    using mode = player_controller::mode;

    auto& c = ed.sim_player;

    if (not c.shown.started) {
        if (EnhancedButton(app, "start", ImVec2{}, "Start the player"))
            c.need_rebuild = true;
        return;
    }

    const auto size = ImGui::ComputeButtonSize(5);

    if (EnhancedButton(app, c.current == mode::realtime ? "##t" : "RT##t",
                       size, "Real time")) {
        if (c.current == mode::realtime) {
            c.current = mode::paused;
        } else {
            c.current    = mode::realtime;
            c.pace_reset = true;
        }
    }

    ImGui::TextFormat("step {}, time {:g}", c.shown.step, c.shown.t);

    if (c.current == mode::realtime) {
        ImGui::TextFormat("target {:.3f}, lateness {} ms, resync {}",
                          c.shown.target, c.shown.lateness_ms, c.shown.resyncs);
        if (c.shown.lateness_ms > 0)
            ImGui::TextFormat("The simulation is slower than the real time.");
    }

    show_restart_button(app, c);
}

static void show_complete_player(application& app, project_editor& ed) noexcept
{
    using mode = player_controller::mode;

    auto& c = ed.sim_player;

    const auto size     = ImGui::ComputeButtonSize(2);
    const auto running  = c.current == mode::complete;
    const auto finished = c.shown.started and c.shown.finished;
    const auto resume   = c.shown.started and not finished;

    if (running) {
        if (EnhancedButton(app, "##full", size, "Pause"))
            c.current = mode::paused;
    } else {
        if (EnhancedButton(app, "##full", size, resume ? "Resume" : "Run")) {
            // Resume continues the current simulation: no rebuild.
            c.need_rebuild = not resume;
            c.current      = mode::complete;
        }
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(not c.shown.started);
    if (EnhancedButton(app, "##full", size, "Restart")) {
        c.need_rebuild = true;
        c.current      = mode::complete;
    }
    ImGui::EndDisabled();

    if (std::isfinite(c.shown.end) and c.shown.end > c.shown.begin) {
        const auto ratio = (c.shown.t - c.shown.begin) /
                           (c.shown.end - c.shown.begin);

        ImGui::ProgressBar(static_cast<float>(std::clamp(ratio, 0.0, 1.0)),
                           ImVec2{ -FLT_MIN, 0.f });
    }

    ImGui::TextFormat("step {}, time {:g}", c.shown.step, c.shown.t);

    if (c.shown.work_ms > 0) {
        const auto seconds = static_cast<double>(c.shown.work_ms) / 1000.0;

        ImGui::TextFormat("{:.2f} s, {:.0f} steps/s", seconds,
                          static_cast<double>(c.shown.step) / seconds);
    }

    if (finished)
        ImGui::TextUnformatted("The simulation is finished.");
}

static void show_simulation_player(application&    app,
                                   project_editor& ed,
                                   const unsigned  idx) noexcept
{
    using kind = player_controller::kind;

    static constexpr const char* names[] = { "Debug", "Real time", "Complete" };

    auto selected = static_cast<int>(ed.sim_player.view);
    for (auto i = 0; i != 3; ++i) {
        if (i != 0)
            ImGui::SameLine();

        ImGui::RadioButton(names[i], &selected, i);
    }

    if (selected != static_cast<int>(ed.sim_player.view)) {
        ed.sim_player.view    = enum_cast<kind>(selected);
        ed.sim_player.current = player_controller::mode::paused;
    }

    switch (ed.sim_player.view) {
    case kind::debug:
        show_debug_player(app, ed, idx);
        break;

    case kind::realtime:
        show_realtime_player(app, ed);
        break;

    case kind::complete:
        show_complete_player(app, ed);
        break;
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

    ImGui::BeginDisabled(ed.sim_player.view !=
                         player_controller::kind::realtime);
    {
        i64 value = ed.one_simulation_time_duration.count();

        if (ImGui::InputScalar("ms/u.t.", ImGuiDataType_S64, &value)) {
            if (value > 1) {
                ed.one_simulation_time_duration = std::chrono::milliseconds(
                  value);
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

    auto flags     = ed.pj.flags;
    auto write_irb = ed.pj.flags[project::simulation_flag::write_irtb];

    if (ImGui::Checkbox("Write on disk", &write_irb))
        flags.set(project::simulation_flag::write_irtb, write_irb);

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

    ed.sim_player.unit_duration = ed.one_simulation_time_duration;
    ed.sim_player.task_duration = ed.simulation_task_duration;

    show_simulation_player(
      app, ed,
      static_cast<unsigned>(app.get_task_manager_list(app.pjs.get_id(ed))));

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
    HelpMarker("None: do nothing.\n Graph: writes the simulation graph using a "
               " dot format into the  observation directory path defined "
               "above.\n Binary or Text: writes graph  and all transition for "
               "all models during the simulation. A csv file  format is used "
               "and the file is opened  into the observation directroy  "
               "defined above.\nPlease note, the file may be large.");

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
