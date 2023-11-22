// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <optional>

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

void simulation_editor::remove_simulation_observation_from(
  model_id mdl_id) noexcept
{
    auto& sim = container_of(this, &application::simulation_ed).sim;

    if_data_exists_do(sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
        sim.unobserve(mdl);
    });
}

void simulation_editor::add_simulation_observation_for(std::string_view name,
                                                       model_id mdl_id) noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    if_data_exists_do(app.sim.models, mdl_id, [&](auto& mdl) noexcept -> void {
        if (app.sim.observers.can_alloc(1)) {
            auto& obs = app.sim.observers.alloc(name, 0, 0);
            app.sim.observe(mdl, obs);
        } else {
            auto& app = container_of(this, &application::simulation_ed);
            auto& n   = app.notifications.alloc(log_level::error);
            n.title   = "Too many observer in simulation";
            app.notifications.enable(n);
        }
    });

    // if (auto* mdl = app.sim.models.try_to_get(mdl_id); mdl) {
    //     if (app.sim.observers.can_alloc(1) && sim_obs.can_alloc(1)) {
    //         auto& sobs    = sim_obs.alloc(mdl_id, 32768);
    //         auto  sobs_id = sim_obs.get_id(sobs);
    //         sobs.name     = name;

    //         auto& obs = app.sim.observers.alloc(name, ordinal(sobs_id), 0);
    //         app.sim.observe(*mdl, obs);
    //     } else {
    //         if (!app.sim.observers.can_alloc(1)) {
    //             auto& app = container_of(this, &application::simulation_ed);
    //             auto& n   = app.notifications.alloc(log_level::error);
    //             n.title   = "Too many observer in simulation";
    //             app.notifications.enable(n);
    //         }

    //         if (!sim_obs.can_alloc(1)) {
    //             auto& app = container_of(this, &application::simulation_ed);
    //             auto& n   = app.notifications.alloc(log_level::error);
    //             n.title   = "Too many simulation observation in simulation";
    //             app.notifications.enable(n);
    //         }
    //     }
    // }
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
    auto& app = container_of(this, &application::simulation_ed);

    unselect();

    force_pause           = false;
    force_stop            = false;
    show_minimap          = true;
    allow_user_changes    = true;
    store_all_changes     = false;
    infinity_simulation   = false;
    real_time             = false;
    have_use_back_advance = false;

    app.sim.clear();
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

    open = ImGui::Button("clear", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Destroy all simulations and observations data.");
    if (open)
        ed.simulation_clear();
    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_initialized);
    open = ImGui::Button("copy", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(
          "Clear all simulations and observations datas and copy "
          "components again.");
    if (open)
        ed.simulation_copy_modeling();
    ImGui::SameLine();

    open = ImGui::Button("init", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Initialize simulation models.");
    if (open)
        ed.simulation_init();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_started);
    if (ImGui::Button("start", button))
        ed.simulation_start();
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
        ed.simulation_start();
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
            ed.simulation_start_1();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_back());
    if (ImGui::Button("<", small_button))
        ed.simulation_back();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_advance());
    if (ImGui::Button(">", small_button))
        ed.simulation_advance();
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ed.tl.current_bag != ed.tl.points.end()) {
        ImGui::TextFormat(
          "debug bag: {}/{}", ed.tl.current_bag->bag, ed.tl.bag);
    } else {
        ImGui::TextFormat("debug bag: {}", ed.tl.bag);
    }

    ImGui::EndDisabled();
}

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
                app.simulation_ed.enable_or_disable_debug();
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

        ImGui::TextFormat("Simulation phase: {}",
                          ordinal(app.simulation_ed.simulation_state));

        ImGui::PopItemWidth();
    }

    return is_modified > 0;
}

static bool show_local_simulation_plot_observers_table(application& app,
                                                       tree_node&   tn) noexcept
{
    irt_assert(!component_is_grid_or_graph(app.mod, tn));

    int is_modified = 0;

    if (ImGui::CollapsingHeader("Plot observers",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto can_add = app.pj.variable_observers.can_alloc();

        if (!can_add)
            ImGui::TextFormatDisabled(
              "Not enough variable observers memory available (default: {})",
              app.pj.variable_observers.capacity());

        if (ImGui::BeginTable("Observation table", 4)) {
            ImGui::TableSetupColumn("enable");
            ImGui::TableSetupColumn("unique id");
            ImGui::TableSetupColumn("model type");
            ImGui::TableSetupColumn("type");
            ImGui::TableHeadersRow();

            for_each_model(
              app.sim,
              tn,
              [&](auto& sim,
                  auto& /*tn*/,
                  auto /*unique_id*/,
                  auto& mdl) noexcept {
                  const auto mdl_id = sim.get_id(mdl);

                  auto* current_selection = find_specified_data_if(
                    app.pj.variable_observers,
                    tn.variable_observer_ids,
                    [&](auto& var_obs) { return var_obs.mdl_id == mdl_id; });

                  bool enable = current_selection != nullptr;

                  ImGui::PushID(static_cast<int>(get_index(mdl_id)));

                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();

                  if (can_add && ImGui::Checkbox("##enable", &enable)) {
                      if (enable) {
                          irt_assert(current_selection == nullptr);
                          auto& gp    = app.pj.variable_observers.alloc();
                          auto  gp_id = app.pj.variable_observers.get_id(gp);
                          gp.tn_id    = app.pj.tree_nodes.get_id(tn);
                          gp.mdl_id   = sim.models.get_id(mdl);
                          gp.type     = variable_observer::type_options::line;
                          format(gp.name, "{}", ordinal(gp_id));

                          tn.variable_observer_ids.emplace_back(gp_id);
                          current_selection = &gp;
                          irt_assert(current_selection != nullptr);
                      } else {
                          irt_assert(current_selection != nullptr);
                          app.pj.variable_observers.free(*current_selection);
                          current_selection = nullptr;
                          enable            = false;
                      }
                  }

                  ImGui::TableNextColumn();
                  ImGui::TextFormat("{}", ordinal(mdl_id));
                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(
                    dynamics_type_names[ordinal(mdl.type)]);
                  ImGui::TableNextColumn();

                  if (enable) {
                      irt_assert(current_selection != nullptr);
                      ImGui::TextUnformatted("line");
                  } else {
                      ImGui::TextUnformatted("-");
                  }
                  ImGui::TableNextColumn();

                  ImGui::PopID();
              });

            ImGui::EndTable();
        }
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

static bool show_local_simulation_settings(application& app,
                                           tree_node&   tn) noexcept
{
    int is_modified = 0;

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Parameter table", 4)) {
            ImGui::TableSetupColumn("enable");
            ImGui::TableSetupColumn("unique id");
            ImGui::TableSetupColumn("model type");
            ImGui::TableSetupColumn("parameter");
            ImGui::TableHeadersRow();

            for_each_model(
              app.sim,
              tn,
              [&](auto& sim,
                  auto& /*tn*/,
                  auto /*unique_id*/,
                  auto& mdl) noexcept {
                  const auto mdl_id = sim.get_id(mdl);

                  auto* current_selection = find_specified_data_if(
                    app.pj.global_parameters,
                    tn.parameters_ids,
                    [&](auto& global_param) {
                        return global_param.mdl_id == mdl_id;
                    });

                  bool enable = current_selection != nullptr;

                  ImGui::PushID(static_cast<int>(get_index(mdl_id)));

                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();

                  if (ImGui::Checkbox("##enable", &enable)) {
                      if (enable) {
                          irt_assert(current_selection == nullptr);
                          auto& gp    = app.pj.global_parameters.alloc();
                          auto  gp_id = app.pj.global_parameters.get_id(gp);
                          gp.mdl_id   = sim.models.get_id(mdl);
                          gp.tn_id    = app.pj.tree_nodes.get_id(tn);
                          gp.param.copy_from(mdl);
                          format(gp.name, "{}", ordinal(gp_id));

                          tn.parameters_ids.emplace_back(gp_id);
                          current_selection = &gp;
                          irt_assert(current_selection != nullptr);
                      } else {
                          irt_assert(current_selection != nullptr);
                          app.pj.global_parameters.free(*current_selection);
                          current_selection = nullptr;
                          enable            = false;
                      }
                  }

                  ImGui::TableNextColumn();

                  ImGui::TextFormat("{}", ordinal(mdl_id));
                  ImGui::TableNextColumn();

                  ImGui::TextUnformatted(
                    dynamics_type_names[ordinal(mdl.type)]);
                  ImGui::TableNextColumn();

                  if (enable) {
                      irt_assert(current_selection != nullptr);
                      show_parameter_editor(app, mdl, current_selection->param);
                  }

                  ImGui::TableNextColumn();
                  ImGui::PopID();
              });

            ImGui::EndTable();
        }
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
              irt_unreachable();
          }

          return false;
      },
      []() noexcept -> bool { return false; });
}

static void show_local_variable_plot(variable_observer& var_obs,
                                     observer&          sim_obs) noexcept
{
    if (sim_obs.linearized_buffer.size() > 0) {
        switch (var_obs.type) {
        case variable_observer::type_options::line:
            ImPlot::PlotLineG(var_obs.name.c_str(),
                              ring_buffer_getter,
                              &sim_obs.linearized_buffer,
                              sim_obs.linearized_buffer.ssize());
            break;

        case variable_observer::type_options::dash:
            ImPlot::PlotScatterG(var_obs.name.c_str(),
                                 ring_buffer_getter,
                                 &sim_obs.linearized_buffer,
                                 sim_obs.linearized_buffer.ssize());
            break;

        default:
            irt_unreachable();
        }
    }
}

static void show_local_variables_plot(application& app, tree_node& tn) noexcept
{
    if (ImPlot::BeginPlot("variables", ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        for_specified_data(
          app.pj.variable_observers,
          tn.variable_observer_ids,
          [&](auto& var_obs) noexcept {
              if_data_exists_do(
                app.sim.models, var_obs.mdl_id, [&](auto& sim_mdl) {
                    if_data_exists_do(
                      app.sim.observers, sim_mdl.obs_id, [&](auto& sim_obs) {
                          show_local_variable_plot(var_obs, sim_obs);
                      });
                });
          });

        ImPlot::PopStyleVar(2);
        ImPlot::EndPlot();
    }
}

static void show_local_grid(application& app, tree_node& tn) noexcept
{
    // @TODO get a grid_observation_system then

    for_specified_data(
      app.pj.grid_observers, tn.grid_observer_ids, [&](auto& grid) noexcept {
          const auto id    = app.pj.grid_observers.get_id(grid);
          const auto index = get_index(id);

          ImGui::PushID(&grid);
          app.simulation_ed.grid_obs.show(
            app.pj.grid_observation_systems[index]);
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
    auto to_delete   = undefined<grid_modeling_observer_id>();
    bool is_modified = false;

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

    if (is_defined(to_delete)) {
        app.pj.grid_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_simulation_graph_observers(application& app) noexcept
{
    auto to_delete   = undefined<graph_modeling_observer_id>();
    bool is_modified = false;

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

            ImGui::TextFormat("{}", ordinal(variable.mdl_id));

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

    constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    const bool can_be_initialized = !match(simulation_state,
                                           simulation_status::not_started,
                                           simulation_status::finished,
                                           simulation_status::initialized,
                                           simulation_status::not_started);

    const bool can_be_started =
      !match(simulation_state, simulation_status::initialized);

    const bool can_be_paused = !match(simulation_state,
                                      simulation_status::running,
                                      simulation_status::run_requiring,
                                      simulation_status::paused);

    const bool can_be_restarted =
      !match(simulation_state, simulation_status::pause_forced);

    const bool can_be_stopped = !match(simulation_state,
                                       simulation_status::running,
                                       simulation_status::run_requiring,
                                       simulation_status::paused,
                                       simulation_status::pause_forced);

    show_simulation_action_buttons(*this,
                                   can_be_initialized,
                                   can_be_started,
                                   can_be_paused,
                                   can_be_restarted,
                                   can_be_stopped);

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
                    if (auto* selected = app.pj.node(selected_tn); selected) {
                        if (!component_is_grid_or_graph(app.mod, *selected))
                            show_local_simulation_plot_observers_table(
                              app, *selected);
                        show_local_simulation_specific_observers(app,
                                                                 *selected);

                        if (!selected->variable_observer_ids.empty())
                            show_local_variables_plot(app, *selected);

                        if (!selected->grid_observer_ids.empty())
                            show_local_grid(app, *selected);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("graph")) {
                    if (app.simulation_ed.can_display_graph_editor())
                        show_simulation_editor(app);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
}

//
// simulation tasks
//

/* Task: add a model into current simulation (running, started etc.). */
void task_simulation_model_add(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& sim = task->app->sim;

    if (!sim.can_alloc(1)) {
        auto& n = task->app->notifications.alloc(log_level::error);
        n.title = "To many model in simulation editor";
        task->app->notifications.enable(n);
        task->state = task_status::finished;
        return;
    }

    auto& mdl    = sim.alloc(enum_cast<dynamics_type>(task->param_1));
    auto  mdl_id = sim.models.get_id(mdl);

    auto ret =
      sim.make_initialize(mdl, task->app->simulation_ed.simulation_current);
    if (is_bad(ret)) {
        sim.deallocate(mdl_id);

        auto& n = task->app->notifications.alloc(log_level::error);
        n.title = "Fail to initialize model";
        task->app->notifications.enable(n);
        task->state = task_status::finished;
        return;
    }

    const auto   x = static_cast<float>(task->param_2);
    const auto   y = static_cast<float>(task->param_3);
    const ImVec2 pos{ x, y };
    task->app->simulation_ed.models_to_move.push(
      simulation_editor::model_to_move{ .id = mdl_id, .position = pos });

    task->state = task_status::finished;
}

void task_simulation_model_del(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& sim    = task->app->sim;
    auto  mdl_id = enum_cast<model_id>(task->param_1);
    sim.deallocate(mdl_id);

    task->state = task_status::finished;
}

} // namesapce irt
