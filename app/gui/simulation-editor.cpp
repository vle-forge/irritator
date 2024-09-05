// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mutex>
#include <optional>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/macros.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/timeline.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

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
    display_graph         = true;

    show_internal_values = false;
    show_internal_inputs = false;
    show_identifiers     = false;

    is_open = true;

    tl.reset();

    simulation_last_finite_t   = 0;
    simulation_begin           = 0;
    simulation_end             = 100;
    simulation_display_current = 0;

    nb_microsecond_per_simulation_time = 1000000;

    head    = undefined<tree_node_id>();
    current = undefined<tree_node_id>();
    mode    = visualization_mode::flat;

    simulation_state = simulation_status::not_started;

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

    auto open = false;

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
        ed.simulation_state = simulation_status::run_requiring;
        ed.force_pause      = false;
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

    ImGui::TextFormat("Current time {:.6f}", ed.simulation_display_current);

    ImGui::SameLine();

    if (ed.tl.current_bag != ed.tl.points.end()) {
        ImGui::TextFormat(
          "debug bag: {}/{}", ed.tl.current_bag->bag, ed.tl.bag);
    } else {
        ImGui::TextFormat("debug bag: {}", ed.tl.bag);
    }

    ImGui::EndDisabled();
}

static bool select_variable_observer(project&              pj,
                                     variable_observer_id& current) noexcept
{
    small_string<32> preview;

    auto* v_obs = pj.variable_observers.try_to_get(current);
    if (v_obs)
        preview = v_obs->name.sv();

    auto ret = false;
    if (ImGui::BeginCombo("Select group variable", preview.c_str())) {
        for (auto& v_obs : pj.variable_observers) {
            const auto id       = pj.variable_observers.get_id(v_obs);
            const auto selected = id == current;

            ImGui::PushID(ordinal(id));
            if (ImGui::Selectable(v_obs.name.c_str(), selected)) {
                current = id;
                ret     = true;
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return ret;
}

// Get the `variable_observer` if `vobs_id` exists else, try to get the first
// available `variable_observer` otherwise, allocate a new `variable_observer`.
static auto get_or_add_variable_observer(project&             pj,
                                         variable_observer_id vobs_id) noexcept
  -> variable_observer&
{
    if (auto* vobs = pj.variable_observers.try_to_get(vobs_id); vobs)
        return *vobs;

    if (not pj.variable_observers.empty())
        return *pj.variable_observers.begin();

    debug::ensure(pj.variable_observers.can_alloc((1)));

    auto& v = pj.variable_observers.alloc();
    v.name  = "New";

    return v;
}

static bool show_local_simulation_plot_observers_table(application& app,
                                                       tree_node&   tn) noexcept
{
    debug::ensure(!component_is_grid_or_graph(app.mod, tn));

    auto is_modified = 0;

    if (ImGui::CollapsingHeader("Plot observers",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Observation table", 4)) {
            ImGui::TableSetupColumn("enable");
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("model type");
            ImGui::TableSetupColumn("plot name");
            ImGui::TableHeadersRow();

            for_each_model(app.sim, tn, [&](auto uid, auto& mdl) noexcept {
                const auto mdl_id = app.sim.get_id(mdl);
                const auto tn_id  = app.pj.tree_nodes.get_id(tn);

                auto vobs_id    = undefined<variable_observer_id>();
                auto sub_obs_id = undefined<variable_observer::sub_id>();
                auto enable     = false;

                if (auto* ptr = tn.variable_observer_ids.get(uid); ptr) {
                    if (auto* vobs = app.pj.variable_observers.try_to_get(*ptr);
                        vobs) {
                        enable     = true;
                        vobs_id    = *ptr;
                        sub_obs_id = vobs->find(tn_id, mdl_id);
                    }
                }

                ImGui::PushID(&mdl);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (ImGui::Checkbox("##enable", &enable)) {
                    if (enable) {
                        auto& vobs =
                          get_or_add_variable_observer(app.pj, vobs_id);
                        vobs_id    = app.pj.variable_observers.get_id(vobs);
                        sub_obs_id = vobs.push_back(tn_id, mdl_id);
                        tn.variable_observer_ids.set(uid, vobs_id);

                        if (auto* c = app.mod.components.try_to_get(tn.id);
                            c and c->type == component_type::simple) {
                            if (auto* g = app.mod.generic_components.try_to_get(
                                  c->id.generic_id);
                                g) {
                                for (auto& ch : g->children) {
                                    if (ch.unique_id == uid) {
                                        vobs
                                          .get_names()[get_index(sub_obs_id)] =
                                          g->children_names[get_index(
                                            g->children.get_id(ch))];
                                        break;
                                    }
                                }
                            }
                        }

                    } else {
                        auto& vobs =
                          get_or_add_variable_observer(app.pj, vobs_id);
                        vobs_id = app.pj.variable_observers.get_id(vobs);
                        vobs.erase(tn_id, mdl_id);
                        tn.variable_observer_ids.erase(uid);
                    }
                }

                ImGui::TableNextColumn();

                if (enable) {
                    if (auto* vobs =
                          app.pj.variable_observers.try_to_get(vobs_id);
                        vobs) {
                        if (vobs->exists(sub_obs_id)) {
                            ImGui::PushItemWidth(-1.f);
                            if (ImGui::InputSmallString(
                                  "name",
                                  vobs->get_names()[get_index(sub_obs_id)]))
                                is_modified++;
                            ImGui::PopItemWidth();
                        }
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);
                ImGui::TableNextColumn();

                if (enable) {
                    const auto old_vobs_id = vobs_id;
                    if (select_variable_observer(app.pj, vobs_id) and
                        old_vobs_id != vobs_id) {
                        auto* o =
                          app.pj.variable_observers.try_to_get(old_vobs_id);
                        auto* n = app.pj.variable_observers.try_to_get(vobs_id);

                        if (o and n) {
                            const auto old_sub_id = o->find(tn_id, mdl_id);
                            auto       new_sub_id = n->push_back(tn_id, mdl_id);

                            n->get_colors()[get_index(new_sub_id)] =
                              o->get_colors()[get_index(old_sub_id)];
                            n->get_options()[get_index(new_sub_id)] =
                              o->get_options()[get_index(old_sub_id)];
                            n->get_names()[get_index(new_sub_id)] =
                              o->get_names()[get_index(old_sub_id)];

                            o->erase(tn_id, mdl_id);
                            tn.variable_observer_ids.set(uid, vobs_id);
                        }
                    }
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

static auto get_global_parameter(const auto& tn, const u64 uid) noexcept
  -> global_parameter_id
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
                auto       enable  = is_defined(current);

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
                        app.pj.parameters.free(current);
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
                    app.pj.parameters.if_exists_do<irt::parameter>(
                      current, [&](auto /*id*/, auto& param) noexcept {
                          show_parameter_editor(app, mdl, param);
                      });
                }

                ImGui::TableNextColumn();
                ImGui::PopID();
            });

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    return is_modified;
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
                    return show_local_observers(app, tn, compo, graph);
                },
                []() noexcept -> bool { return false; });

          case component_type::grid:
              return if_data_exists_do(
                app.mod.grid_components,
                compo.id.grid_id,
                [&](auto& grid) noexcept {
                    return show_local_observers(app, tn, compo, grid);
                },
                []() noexcept -> bool { return false; });

          case component_type::simple:
              return show_local_simulation_plot_observers_table(app, tn);

          default:
              ImGui::TextFormat(
                "Not yet implemented observers for component {}",
                component_type_names[ordinal(compo.type)]);
              return false;
          }

          return false;
      },
      []() noexcept -> bool { return false; });
}

static void show_local_variables_plot(application&       app,
                                      variable_observer& v_obs,
                                      tree_node_id       tn_id) noexcept
{
    v_obs.for_each([&](const auto id) noexcept {
        const auto idx = get_index(id);
        auto*      obs = app.sim.observers.try_to_get(v_obs.get_obs_ids()[idx]);

        if (obs and v_obs.get_tn_ids()[idx] == tn_id)
            app.simulation_ed.plot_obs.show_plot_line(
              *obs, v_obs.get_options()[idx], v_obs.get_names()[idx]);
    });
}

// @TODO merge the three next functions with a template on
// template<typename DataArray>
// static bool
// show_simulation_main_observations(application& app, DataArray& d)
// noexcept {...}

static bool show_simulation_table_grid_observers(application& app) noexcept
{
    auto to_delete   = undefined<grid_observer_id>();
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

static bool show_simulation_table_graph_observers(application& app) noexcept
{
    auto to_delete   = undefined<graph_observer_id>();
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

static bool show_simulation_table_variable_observers(application& app) noexcept
{
    auto to_delete   = undefined<variable_observer_id>();
    bool is_modified = false;

    if (not app.pj.variable_observers.can_alloc(1))
        ImGui::TextFormatDisabled(
          "Can not allocate more multi-plot observers (max reached: {})",
          app.pj.variable_observers.capacity());

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

    if (app.pj.variable_observers.can_alloc(1)) {
        if (ImGui::Button("New group")) {
            auto& v     = app.pj.variable_observers.alloc();
            v.name      = "New";
            is_modified = true;
        }
    }

    if (is_defined(to_delete)) {
        app.pj.variable_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_project_parameters(application& app) noexcept
{
    constexpr auto tflags = ImGuiTableFlags_SizingStretchProp;
    constexpr auto fflags = ImGuiTableColumnFlags_WidthFixed;
    constexpr auto sflags = ImGuiTableColumnFlags_WidthStretch;

    auto to_del      = std::optional<global_parameter_id>();
    auto is_modified = 0;

    if (ImGui::BeginTable("Parameter table", 5, tflags)) {
        ImGui::TableSetupColumn("id", fflags, 80.f);
        ImGui::TableSetupColumn("name", fflags, 100.f);
        ImGui::TableSetupColumn("action", fflags, 60.f);
        ImGui::TableSetupColumn("model type", fflags, 120.f);
        ImGui::TableSetupColumn("parameters", sflags);
        ImGui::TableHeadersRow();

        app.pj.parameters.for_each([&](auto  id,
                                       auto& name,
                                       auto& /*tn_id*/,
                                       auto& mdl_id,
                                       auto& p) noexcept {
            const auto* mdl = app.sim.models.try_to_get(mdl_id);
            ImGui::PushID(get_index(id));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(id));
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1.f);
            if (ImGui::InputFilteredString("name", name))
                is_modified++;
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("del"))
                to_del = id;

            if (mdl) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
                ImGui::TableNextColumn();
                if (show_parameter_editor(app, mdl->type, p))
                    is_modified++;
            } else {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("deleted model");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("");
            }

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    if (to_del.has_value()) {
        app.pj.parameters.free(*to_del);
        is_modified++;
    }

    return is_modified;
}

static void show_component_observations_actions(
  simulation_editor& sim_ed) noexcept
{
    ImGui::TextUnformatted("Column: ");
    ImGui::SameLine();
    if (ImGui::Button("1"))
        sim_ed.tree_node_observation = 1;
    ImGui::SameLine();
    if (ImGui::Button("2"))
        sim_ed.tree_node_observation = 2;
    ImGui::SameLine();
    if (ImGui::Button("3"))
        sim_ed.tree_node_observation = 3;
    ImGui::SameLine();
    if (ImGui::Button("4"))
        sim_ed.tree_node_observation = 4;
    ImGui::SameLine();

    ImGui::TextUnformatted("Height in pixel: ");
    ImGui::SameLine();
    if (ImGui::Button("200"))
        sim_ed.tree_node_observation_height = 200.f;
    ImGui::SameLine();
    if (ImGui::Button("+50"))
        sim_ed.tree_node_observation_height += 50.f;
    ImGui::SameLine();
    if (ImGui::Button("-50")) {
        sim_ed.tree_node_observation_height -= 50.f;
        if (sim_ed.tree_node_observation_height <= 0.f)
            sim_ed.tree_node_observation_height = 10.f;
    }
    ImGui::SameLine();
    if (ImGui::Button("x2"))
        sim_ed.tree_node_observation_height *= 2.f;
    ImGui::SameLine();
    if (ImGui::Button("x0.5")) {
        sim_ed.tree_node_observation_height *= 0.5f;
        if (sim_ed.tree_node_observation_height <= 0.f)
            sim_ed.tree_node_observation_height = 10.f;
    }
}

static bool show_project_observations(application& app) noexcept
{
    constexpr static auto flags = ImGuiTreeNodeFlags_DefaultOpen;

    auto updated = 0;

    if (ImGui::CollapsingHeader("Plots", flags))
        updated += show_simulation_table_variable_observers(app);

    if (not app.pj.grid_observers.empty() and
        ImGui::CollapsingHeader("Grid observers", flags))
        updated += show_simulation_table_grid_observers(app);

    if (not app.pj.graph_observers.empty() and
        ImGui::CollapsingHeader("Graph observers", flags))
        updated += show_simulation_table_graph_observers(app);

    auto& sim_ed = app.simulation_ed;
    show_component_observations_actions(sim_ed);

    const auto sub_obs_size =
      ImVec2(ImGui::GetContentRegionAvail().x / *sim_ed.tree_node_observation,
             sim_ed.tree_node_observation_height);

    auto pos = 0;
    if (ImGui::BeginTable("##obs-table", *sim_ed.tree_node_observation)) {
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        for_each_data(app.pj.grid_observers, [&](auto& grid) noexcept {
            app.simulation_ed.grid_obs.show(grid, sub_obs_size);

            ++pos;

            if (pos >= *sim_ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        });

        for (auto& vobs : app.pj.variable_observers) {
            ImGui::PushID(&vobs);
            if (ImPlot::BeginPlot(vobs.name.c_str(), ImVec2(-1, 200))) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

                vobs.for_each([&](const auto id) noexcept {
                    const auto  idx = get_index(id);
                    const auto* obs =
                      app.sim.observers.try_to_get(vobs.get_obs_ids()[idx]);

                    if (obs)
                        app.simulation_ed.plot_obs.show_plot_line(
                          *obs, vobs.get_options()[idx], vobs.get_names()[idx]);
                });

                ImPlot::PopStyleVar(2);
                ImPlot::EndPlot();
            }
            ImGui::PopID();

            ++pos;
            if (pos >= *sim_ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }

    return updated;
}

static void show_component_observations(application&       app,
                                        simulation_editor& sim_ed,
                                        tree_node&         selected)
{
    show_local_simulation_specific_observers(app, selected);
    show_component_observations_actions(sim_ed);

    const auto sub_obs_size =
      ImVec2(ImGui::GetContentRegionAvail().x / *sim_ed.tree_node_observation,
             sim_ed.tree_node_observation_height);

    auto pos = 0;
    if (ImGui::BeginTable("##obs-table", *sim_ed.tree_node_observation)) {
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        for_specified_data(app.pj.grid_observers,
                           selected.grid_observer_ids,
                           [&](auto& grid) noexcept {
                               app.simulation_ed.grid_obs.show(grid,
                                                               sub_obs_size);

                               ++pos;

                               if (pos >= *sim_ed.tree_node_observation) {
                                   pos = 0;
                                   ImGui::TableNextRow();
                               }
                               ImGui::TableNextColumn();
                           });

        for (auto& vobs : app.pj.variable_observers) {
            const auto tn_id = app.pj.tree_nodes.get_id(selected);
            if (vobs.exists(tn_id)) {
                ImGui::PushID(&vobs);
                if (ImPlot::BeginPlot(vobs.name.c_str(), ImVec2(-1, 200))) {
                    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);
                    show_local_variables_plot(app, vobs, tn_id);
                    ImPlot::PopStyleVar(2);
                    ImPlot::EndPlot();
                }
                ImGui::PopID();
            }

            ++pos;
            if (pos >= *sim_ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }
}

static void show_simulation_editor_treenode(application& app,
                                            tree_node&   tn) noexcept
{
    if (auto* compo = app.mod.components.try_to_get(tn.id); compo) {
        dispatch_component(app.mod, *compo, [&](auto& c) noexcept {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, grid_component>) {
                app.simulation_ed.grid_sim.show_observations(tn, *compo, c);
            } else if constexpr (std::is_same_v<T, graph_component>) {
                app.simulation_ed.graph_sim.show_observations(tn, *compo, c);
            } else if constexpr (std::is_same_v<T, generic_component>) {
                app.simulation_ed.generic_sim.show_observations(tn, *compo, c);
            } else if constexpr (std::is_same_v<T, hsm_component>) {
                app.simulation_ed.hsm_sim.show_observations(tn, *compo, c);
            } else
                ImGui::TextFormatDisabled(
                  "Undefined simulation editor for this component");
        });
    }
}

void simulation_editor::show() noexcept
{
    auto& app = container_of(this, &application::simulation_ed);

    if (!ImGui::Begin(simulation_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (std::unique_lock lock(app.sim_mutex, std::try_to_lock);
        lock.owns_lock()) {
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

            // if (ImGui::BeginChild("##s-c", ImVec2(0, 0), false)) {
            const auto selected_tn = app.project_wnd.selected_tn();
            auto*      selected    = app.pj.node(selected_tn);

            if (ImGui::BeginTabBar("##SimulationTabBar")) {
                if (ImGui::BeginTabItem("Parameters")) {
                    show_project_parameters(app);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Observations")) {
                    show_project_observations(app);
                    ImGui::EndTabItem();
                }

                if (selected) {
                    if (ImGui::BeginTabItem("Component parameters")) {
                        show_local_simulation_settings(app, *selected);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Component observations")) {
                        show_component_observations(app, *this, *selected);
                        ImGui::EndTabItem();
                    }
                }

                if (ImGui::BeginTabItem("Simulation graph")) {
                    if (app.simulation_ed.can_display_graph_editor()) {
                        if (selected) {
                            show_simulation_editor_treenode(app, *selected);
                        } else {
                            show_simulation_editor(app);
                        }
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            // }
            // ImGui::EndChild();

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namesapce irt
