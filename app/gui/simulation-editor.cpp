// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/macros.hpp"
#include <mutex>
#include <optional>
#include <utility>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/timeline.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace irt {

simulation_editor::simulation_editor() noexcept
  : tl(32768, 4096, 65536, 65536, 32768, 32768)
{
    output_context = ImPlot::CreateContext();
    context        = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;
}

simulation_editor::~simulation_editor() noexcept
{
    if (output_context) {
        ImPlot::DestroyContext(output_context);
    }

    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void simulation_editor::select(tree_node_id id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    if (auto* tree = app.pj.node(id); tree) {
        unselect();

        head    = id;
        current = id;
    }
}

void simulation_editor::unselect() noexcept
{
    head    = undefined<tree_node_id>();
    current = undefined<tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();

    selected_links.clear();
    selected_nodes.clear();
}

void simulation_editor::clear() noexcept
{
    unselect();

    force_pause           = false;
    force_stop            = false;
    show_minimap          = true;
    allow_user_changes    = true;
    store_all_changes     = false;
    infinity_simulation   = false;
    real_time             = false;
    have_use_back_advance = false;

    tl.reset();

    simulation_begin   = 0;
    simulation_end     = 100;
    simulation_current = 0;

    simulation_real_time_relation = 1000000;

    head    = undefined<tree_node_id>();
    current = undefined<tree_node_id>();
    mode    = visualization_mode::flat;

    simulation_state = simulation_status::not_started;

    plot_obs.clear();

    // @TODO maybe clear project grid and graph obs ?
    // grid_obs.clear();
    // graph_obs.clear();

    selected_links.clear();
    selected_nodes.clear();

    automatic_layout_iteration = 0;
    displacements.clear();
}

static void show_simulation_action_buttons(simulation_editor& ed,
                                           bool can_be_initialized,
                                           bool can_be_started,
                                           bool can_be_paused,
                                           bool can_be_restarted,
                                           bool can_be_stopped) noexcept
{
    const auto item_x         = ImGui::GetStyle().ItemSpacing.x;
    const auto region_x       = ImGui::GetContentRegionAvail().x;
    const auto button_x       = (region_x - item_x) / 10.f;
    const auto small_button_x = (region_x - (button_x * 9.f) - item_x) / 3.f;
    const auto button         = ImVec2{ button_x, 0.f };
    const auto small_button   = ImVec2{ small_button_x, 0.f };

    bool open = false;

    open = ImGui::Button("Clear", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Destroy all simulations and observations data.");
    if (open)
        ed.start_simulation_delete();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_initialized);
    open = ImGui::Button("Import", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Destroy all simulations and observations data and "
                          "reimport the components.");
    if (open)
        ed.start_simulation_copy_modeling();
    ImGui::SameLine();

    open = ImGui::Button("init", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Initialize simulation models and data.");
    if (open)
        ed.start_simulation_init();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_started);
    if (ImGui::Button("start", button))
        ed.start_simulation_start();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_paused);
    if (ImGui::Button("pause", button)) {
        ed.force_pause = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_restarted);
    if (ImGui::Button("continue", button)) {
        ed.start_simulation_start();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_stopped);
    if (ImGui::Button("stop", button)) {
        ed.force_stop = true;
    }
    ImGui::EndDisabled();

    const bool history_mode = (ed.store_all_changes || ed.allow_user_changes) &&
                              (can_be_started || can_be_restarted);

    ImGui::BeginDisabled(!history_mode);

    if (ed.store_all_changes) {
        ImGui::SameLine();
        if (ImGui::Button("step-by-step", small_button))
            ed.start_simulation_start_1();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_back());
    if (ImGui::Button("<", small_button))
        ed.start_simulation_back();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_advance());
    if (ImGui::Button(">", small_button))
        ed.start_simulation_advance();
    ImGui::EndDisabled();

    ImGui::TextFormat("Current time {:.6f}", ed.simulation_current);

    ImGui::SameLine();

    if (ed.tl.current_bag != ed.tl.points.end()) {
        ImGui::TextFormat(
          "debug bag: {}/{}", ed.tl.current_bag->bag, ed.tl.bag);
    } else {
        ImGui::TextFormat("debug bag: {}", ed.tl.bag);
    }

    ImGui::EndDisabled();
}

static inline constexpr std::string_view simulation_status_names[] = {
    "not_started", "initializing", "initialized",  "run_requiring",
    "running",     "paused",       "pause_forced", "finish_requiring",
    "finishing",   "finished",     "debugged",
};

static bool show_main_simulation_settings(application& app) noexcept
{
    int is_modified = 0;

    if (ImGui::CollapsingHeader("Main parameters")) {
        ImGui::PushItemWidth(100.f);
        is_modified +=
          ImGui::InputReal("Begin", &app.simulation_ed.simulation_begin);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        is_modified +=
          ImGui::Checkbox("Edit", &app.simulation_ed.allow_user_changes);

        is_modified +=
          ImGui::InputReal("End", &app.simulation_ed.simulation_end);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::Checkbox("Debug", &app.simulation_ed.store_all_changes)) {
            is_modified = true;
            if (app.simulation_ed.store_all_changes &&
                app.simulation_ed.simulation_state ==
                  simulation_status::running) {
                app.simulation_ed.start_enable_or_disable_debug();
            }
        }

        ImGui::BeginDisabled(!app.simulation_ed.real_time);
        is_modified +=
          ImGui::InputScalar("Micro second for 1 unit time",
                             ImGuiDataType_S64,
                             &app.simulation_ed.simulation_real_time_relation);
        ImGui::EndDisabled();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        is_modified += ImGui::Checkbox("No time limit",
                                       &app.simulation_ed.infinity_simulation);

        ImGui::TextFormat("Current time {:.6f}",
                          app.simulation_ed.simulation_current);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        is_modified +=
          ImGui::Checkbox("Real time", &app.simulation_ed.real_time);

        ImGui::TextFormat(
          "Simulation phase: {}",
          simulation_status_names[ordinal(app.simulation_ed.simulation_state)]);

        ImGui::PopItemWidth();
    }

    return is_modified > 0;
}

static bool select_variable_observer(project&              pj,
                                     variable_observer_id& current) noexcept
{
    small_string<32> preview;

    auto* v_obs = pj.variable_observers.try_to_get(current);
    if (v_obs)
        preview = v_obs->name.sv();

    bool ret = false;
    if (ImGui::BeginCombo("Select group variable", preview.c_str())) {
        for (auto& v_obs : pj.variable_observers) {
            const auto id       = pj.variable_observers.get_id(v_obs);
            const auto selected = id == current;

            if (ImGui::Selectable(v_obs.name.c_str(), selected)) {
                current = id;
                ret     = true;
            }
        }

        ImGui::EndCombo();
    }

    return ret;
}

static auto get_first_variable_observer(project& pj) noexcept
  -> std::optional<variable_observer_id>
{
    if (not pj.variable_observers.empty())
        return pj.variable_observers.get_id(*pj.variable_observers.begin());

    if (pj.variable_observers.can_alloc()) {
        auto& v_obs = pj.variable_observers.alloc();
        v_obs.name  = "New";
        return pj.variable_observers.get_id(v_obs);
    }

    return std::nullopt;
}

static bool show_local_simulation_plot_observers_table(application& app,
                                                       tree_node&   tn) noexcept
{
    debug::ensure(!component_is_grid_or_graph(app.mod, tn));

    int is_modified = 0;

    if (ImGui::CollapsingHeader("Plot observers",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto can_add = app.pj.variable_observers.can_alloc();

        if (!can_add)
            ImGui::TextFormatDisabled(
              "Not enough variable observers memory available (default: {})",
              app.pj.variable_observers.capacity());

        ImGui::BeginChild("c-show-plot",
                          ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        if (ImGui::BeginTable("Observation table", 3)) {
            ImGui::TableSetupColumn("enable");
            ImGui::TableSetupColumn("model type");
            ImGui::TableSetupColumn("plot name");
            ImGui::TableHeadersRow();

            for_each_model(app.sim, tn, [&](auto uid, auto& mdl) noexcept {
                const auto mdl_id  = app.sim.get_id(mdl);
                const auto tn_id   = app.pj.tree_nodes.get_id(tn);
                auto       current = undefined<variable_observer_id>();
                auto       enable  = false;

                {
                    auto* ptr = tn.variable_observer_ids.get(uid);
                    if (ptr) {
                        enable = app.pj.variable_observers.try_to_get(*ptr);
                        if (enable)
                            current = *ptr;
                    }
                }

                ImGui::PushID(static_cast<int>(get_index(mdl_id)));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (can_add && ImGui::Checkbox("##enable", &enable)) {
                    if (enable) {
                        debug::ensure(is_undefined(current));

                        auto id_opt = get_first_variable_observer(app.pj);
                        if (not id_opt.has_value()) {
                            auto& n =
                              app.notifications.alloc(log_level::warning);
                            n.title = "Can not allocate more observers";
                            app.notifications.enable(n);
                        } else {
                            auto  v_obs_id = *id_opt;
                            auto* v_obs =
                              app.pj.variable_observers.try_to_get(v_obs_id);
                            if (v_obs) {
                                v_obs->push_back(tn_id, mdl_id);
                                tn.variable_observer_ids.set(uid, v_obs_id);
                                current = v_obs_id;
                            } else {
                                tn.variable_observer_ids.erase(uid);
                            }
                        }
                    } else {
                        auto* v_obs =
                          app.pj.variable_observers.try_to_get(current);
                        if (v_obs) {
                            v_obs->erase(tn_id, mdl_id);
                        } else {
                            tn.variable_observer_ids.erase(uid);
                        }

                        tn.variable_observer_ids.erase(uid);
                        enable = false;
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);
                ImGui::TableNextColumn();

                if (enable) {
                    auto old_current = current;
                    if (select_variable_observer(app.pj, current)) {
                        if_data_exists_do(app.pj.variable_observers,
                                          old_current,
                                          [&](auto& v_obs) noexcept {
                                              v_obs.erase(tn_id, mdl_id);
                                          });

                        tn.variable_observer_ids.set(uid, current);
                        if_data_exists_do(app.pj.variable_observers,
                                          current,
                                          [&](auto& v_obs) noexcept {
                                              v_obs.push_back(tn_id, mdl_id);
                                          });
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }

                ImGui::TableNextColumn();

                ImGui::PopID();
            });

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    return is_modified > 0;
}

static bool show_local_grid_observers(application&    app,
                                      tree_node&      tn,
                                      component&      compo,
                                      grid_component& grid) noexcept
{
    return show_local_observers(app, tn, compo, grid);
}

static bool show_local_graph_observers(application&     app,
                                       tree_node&       tn,
                                       component&       compo,
                                       graph_component& graph) noexcept
{
    return show_local_observers(app, tn, compo, graph);
}

static auto get_global_parameter(const auto& tn,
                                 const u64 uid) noexcept -> global_parameter_id
{
    auto* ptr = tn.parameters_ids.get(uid);
    return ptr ? *ptr : undefined<global_parameter_id>();
}

static bool show_local_simulation_settings(application& app,
                                           tree_node&   tn) noexcept
{
    int is_modified = 0;

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("c-p",
                          ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        if (ImGui::BeginTable("Parameter table", 4)) {
            ImGui::TableSetupColumn("enable");
            ImGui::TableSetupColumn("unique id");
            ImGui::TableSetupColumn("model type");
            ImGui::TableSetupColumn("parameter");
            ImGui::TableHeadersRow();

            for_each_model(app.sim, tn, [&](auto uid, auto& mdl) noexcept {
                const auto mdl_id  = app.sim.get_id(mdl);
                auto       current = get_global_parameter(tn, uid);
                bool       enable  = is_defined(current);

                ImGui::PushID(static_cast<int>(get_index(mdl_id)));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (ImGui::Checkbox("##enable", &enable)) {
                    if (enable and app.pj.parameters.can_alloc(1)) {
                        const auto gp_id =
                          app.pj.parameters.alloc([&](auto  id,
                                                      auto& name,
                                                      auto& tn_id,
                                                      auto& mdl_id,
                                                      auto& p) noexcept {
                              format(name, "{}", ordinal(id));
                              tn_id  = app.pj.tree_nodes.get_id(tn);
                              mdl_id = app.sim.models.get_id(mdl);
                              p.copy_from(mdl);
                          });

                        tn.parameters_ids.set(uid, gp_id);
                        current = gp_id;
                    } else {
                        app.pj.parameters.erase(current);
                        tn.parameters_ids.erase(uid);
                        current = undefined<global_parameter_id>();
                        enable  = false;
                    }
                }

                ImGui::TableNextColumn();

                ImGui::TextFormat("{}", ordinal(mdl_id));
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);
                ImGui::TableNextColumn();

                if (enable) {
                    if (app.pj.parameters.exists(current)) {
                        const auto idx = get_index(current);
                        show_parameter_editor(
                          app, mdl, app.pj.parameters.parameters(idx));
                    }
                }

                ImGui::TableNextColumn();
                ImGui::PopID();
            });

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    return is_modified > 0;
}

static bool show_local_simulation_specific_observers(application& app,
                                                     tree_node&   tn) noexcept
{
    return if_data_exists_do(
      app.mod.components,
      tn.id,
      [&](auto& compo) noexcept -> bool {
          switch (compo.type) {
          case component_type::graph:
              return if_data_exists_do(
                app.mod.graph_components,
                compo.id.graph_id,
                [&](auto& graph) noexcept -> bool {
                    return show_local_graph_observers(app, tn, compo, graph);
                },
                []() noexcept -> bool { return false; });

          case component_type::grid:
              return if_data_exists_do(
                app.mod.grid_components,
                compo.id.grid_id,
                [&](auto& grid) noexcept {
                    return show_local_grid_observers(app, tn, compo, grid);
                },
                []() noexcept -> bool { return false; });

          case component_type::simple:
              // return if_data_exists_do(
              //   app.mod.generic_components,
              //   compo.id.generic_id,
              //   [&](auto& generic) noexcept {
              //       return show_local_observers(app, tn, compo, generic);
              //   });
              return true;

          default:
              unreachable();
          }

          return false;
      },
      []() noexcept -> bool { return false; });
}

static void show_local_variable_plot(
  observer&                       obs,
  variable_observer::type_options type) noexcept
{
    if (obs.linearized_buffer.size() > 0) {
        switch (type) {
        case variable_observer::type_options::line:
            ImPlot::PlotLineG(obs.name.c_str(),
                              ring_buffer_getter,
                              &obs.linearized_buffer,
                              obs.linearized_buffer.ssize());
            break;

        case variable_observer::type_options::dash:
            ImPlot::PlotScatterG(obs.name.c_str(),
                                 ring_buffer_getter,
                                 &obs.linearized_buffer,
                                 obs.linearized_buffer.ssize());
            break;

        default:
            unreachable();
        }
    }
}

static void show_local_variables_plot(application&       app,
                                      variable_observer& v_obs) noexcept
{
    v_obs.for_each_obs(
      [&](const auto obs_id, const auto /*color*/, const auto option) noexcept {
          if (auto* obs = app.sim.observers.try_to_get(obs_id); obs)
              show_local_variable_plot(*obs, option);
      });
}

static void show_local_variables_plot(application& app, tree_node& tn) noexcept
{
    if (ImPlot::BeginPlot("variables", ImVec2(-1, 200))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        auto i = 0;

        while (i != tn.variable_observer_ids.data.ssize()) {
            auto  v_obs_id = tn.variable_observer_ids.data[i].value;
            auto* v_obs    = app.pj.variable_observers.try_to_get(v_obs_id);

            if (v_obs) {
                show_local_variables_plot(app, *v_obs);
                ++i;
            } else {
                tn.variable_observer_ids.data.swap_pop_back(i);
                tn.variable_observer_ids.sort();
            }
        }

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }
}

static void show_local_grid(application& app, tree_node& tn) noexcept
{
    for_specified_data(
      app.pj.grid_observers, tn.grid_observer_ids, [&](auto& grid) noexcept {
          ImGui::PushID(&grid);
          app.simulation_ed.grid_obs.show(grid);
          ImGui::PopID();
      });
}

// @TODO merge the three next functions with a template on
// template<typename DataArray>
// static bool
// show_simulation_main_observations(application& app, DataArray& d)
// noexcept {...}

static bool show_simulation_grid_observers(application& app) noexcept
{
    auto to_delete   = undefined<grid_observer_id>();
    bool is_modified = false;

    ImGui::BeginChild("c-show-grid",
                      ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("Grid observers", 5)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("scale");
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_each_data(app.pj.grid_observers, [&](auto& grid) noexcept {
            ImGui::PushID(&grid);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::TextFormat("{}",
                              ordinal(app.pj.grid_observers.get_id(grid)));

            ImGui::TableNextColumn();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputFilteredString("name", grid.name))
                is_modified = true;
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::DragFloatRange2(
              "##scale", &grid.scale_min, &grid.scale_max, 0.01f);
            ImGui::PopItemWidth();
            ImGui::TableNextColumn();
            if (ImPlot::ColormapButton(ImPlot::GetColormapName(grid.color_map),
                                       ImVec2(225, 0),
                                       grid.color_map)) {
                grid.color_map =
                  (grid.color_map + 1) % ImPlot::GetColormapCount();
            }

            ImGui::TableNextColumn();

            if (ImGui::Button("del"))
                to_delete = app.pj.grid_observers.get_id(grid);

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    ImGui::EndChild();

    if (is_defined(to_delete)) {
        app.pj.grid_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_simulation_graph_observers(application& app) noexcept
{
    auto to_delete   = undefined<graph_observer_id>();
    bool is_modified = false;

    ImGui::BeginChild("c-show-graph",
                      ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("Graph observers", 5)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("child");
        ImGui::TableSetupColumn("enable");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_each_data(app.pj.graph_observers, [&](auto& graph) noexcept {
            ImGui::PushID(&graph);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::TextFormat("{}",
                              ordinal(app.pj.graph_observers.get_id(graph)));

            ImGui::TableNextColumn();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputFilteredString("name", graph.name))
                is_modified = true;
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            ImGui::TextFormat("{}", ordinal(graph.mdl_id));

            ImGui::TableNextColumn();

            bool enable = true;
            ImGui::PushItemWidth(-1.0f);
            ImGui::Checkbox("##button", &enable);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            if (ImGui::Button("del"))
                to_delete = app.pj.graph_observers.get_id(graph);

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    ImGui::EndChild();

    if (is_defined(to_delete)) {
        app.pj.graph_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_simulation_variable_observers(application& app) noexcept
{
    auto to_delete   = undefined<variable_observer_id>();
    bool is_modified = false;

    ImGui::BeginChild("c-show-plot",
                      ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("Plot observers", 5)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("child");
        ImGui::TableSetupColumn("enable");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_each_data(app.pj.variable_observers, [&](auto& variable) noexcept {
            ImGui::PushID(&variable);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::TextFormat(
              "{}", ordinal(app.pj.variable_observers.get_id(variable)));

            ImGui::TableNextColumn();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputFilteredString("name", variable.name))
                is_modified = true;
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            ImGui::TextFormat("{}", variable.size());

            ImGui::TableNextColumn();

            bool enable = true;
            ImGui::PushItemWidth(-1.0f);
            ImGui::Checkbox("##button", &enable);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            if (ImGui::Button("del"))
                to_delete = app.pj.variable_observers.get_id(variable);

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    ImGui::EndChild();

    if (is_defined(to_delete)) {
        app.pj.variable_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_main_simulation_observers(application& app) noexcept
{
    constexpr static auto flags = ImGuiTreeNodeFlags_DefaultOpen;

    bool variable_updated = false;
    bool grid_updated     = false;
    bool graph_updated    = false;

    if (ImGui::CollapsingHeader("Main observations", flags)) {
        if (app.pj.variable_observers.ssize() > 0 &&
            ImGui::CollapsingHeader("All plots", flags))
            variable_updated = show_simulation_variable_observers(app);

        if (app.pj.grid_observers.ssize() > 0 &&
            ImGui::CollapsingHeader("All grids", flags))
            grid_updated = show_simulation_grid_observers(app);

        if (app.pj.graph_observers.ssize() > 0 &&
            ImGui::CollapsingHeader("All graphs", flags))
            graph_updated = show_simulation_graph_observers(app);
    }

    return variable_updated or grid_updated or graph_updated;
}

void simulation_editor::show() noexcept
{
    if (!ImGui::Begin(simulation_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (std::unique_lock lock(mutex, std::try_to_lock); lock.owns_lock()) {
        constexpr ImGuiTableFlags flags =
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
          ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
          ImGuiTableFlags_Reorderable;

        const bool can_be_initialized =
          !any_equal(simulation_state,
                     simulation_status::not_started,
                     simulation_status::finished,
                     simulation_status::initialized,
                     simulation_status::not_started);

        const bool can_be_started =
          !any_equal(simulation_state, simulation_status::initialized);

        const bool can_be_paused = !any_equal(simulation_state,
                                              simulation_status::running,
                                              simulation_status::run_requiring,
                                              simulation_status::paused);

        const bool can_be_restarted =
          !any_equal(simulation_state, simulation_status::pause_forced);

        const bool can_be_stopped = !any_equal(simulation_state,
                                               simulation_status::running,
                                               simulation_status::run_requiring,
                                               simulation_status::paused,
                                               simulation_status::pause_forced);

        if (ImGui::BeginTable("##ed", 2, flags)) {
            ImGui::TableSetupColumn(
              "Hierarchy", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableSetupColumn(
              "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);

            auto& app = container_of(this, &application::simulation_ed);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            app.project_wnd.show();

            ImGui::TableSetColumnIndex(1);
            show_simulation_action_buttons(*this,
                                           can_be_initialized,
                                           can_be_started,
                                           can_be_paused,
                                           can_be_restarted,
                                           can_be_stopped);

            if (ImGui::BeginChild("##s-c", ImVec2(0, 0), false)) {
                if (ImGui::BeginTabBar("##SimulationTabBar")) {
                    if (ImGui::BeginTabItem("Parameters")) {
                        show_main_simulation_settings(app);

                        auto selected_tn = app.project_wnd.selected_tn();
                        if (auto* selected = app.pj.node(selected_tn); selected)
                            show_local_simulation_settings(app, *selected);

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Observations")) {
                        show_main_simulation_observers(app);

                        auto selected_tn = app.project_wnd.selected_tn();
                        if (auto* selected = app.pj.node(selected_tn);
                            selected) {
                            if (!component_is_grid_or_graph(app.mod, *selected))
                                show_local_simulation_plot_observers_table(
                                  app, *selected);
                            show_local_simulation_specific_observers(app,
                                                                     *selected);

                            if (!selected->variable_observer_ids.data.empty())
                                show_local_variables_plot(app, *selected);

                            if (!selected->grid_observer_ids.empty())
                                show_local_grid(app, *selected);
                        }

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("graph")) {
                        // if (app.simulation_ed.can_display_graph_editor())
                        show_simulation_editor(app);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namesapce irt
