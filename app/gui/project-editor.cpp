// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/macros.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/timeline.hpp>

#include <optional>

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace irt {

project_editor::project_editor(const std::string_view default_name) noexcept
  : tl(32768, 4096, 65536, 65536, 32768, 32768)
  , name{ default_name }
{
    pj.grid_observers.reserve(8);
    pj.graph_observers.reserve(8);
    pj.variable_observers.reserve(8);

    output_context = ImPlot::CreateContext();
}

project_editor::~project_editor() noexcept
{
    if (output_context) {
        ImPlot::DestroyContext(output_context);
    }
}

bool project_editor::is_selected(tree_node_id id) const noexcept
{
    return m_selected_tree_node == id;
}

void project_editor::select(application& app, tree_node_id id) noexcept
{
    if (id != m_selected_tree_node) {
        m_selected_tree_node = undefined<tree_node_id>();

        if (auto* tree = pj.node(id); tree) {
            if (auto* compo =
                  app.mod.components.try_to_get<component>(tree->id);
                compo) {
                m_selected_tree_node = id;

                if (compo->type == component_type::generic) {
                    if (auto* gen = app.mod.generic_components.try_to_get(
                          compo->id.generic_id)) {
                        generic_sim.init(app, *tree, *compo, *gen);
                    }
                }
            }
        } else {
            generic_sim.init(app);
        }
    }
}

static void show_simulation_action_buttons(application&    app,
                                           project_editor& ed,
                                           bool            can_be_initialized,
                                           bool            can_be_started,
                                           bool            can_be_paused,
                                           bool            can_be_restarted,
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
        ed.start_simulation_delete(app);

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_initialized);
    open = ImGui::Button("Import", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Destroy all simulations and observations data and "
                          "reimport the components.");
    if (open)
        ed.start_simulation_copy_modeling(app);
    ImGui::SameLine();

    open = ImGui::Button("init", button);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Initialize simulation models and data.");
    if (open)
        ed.start_simulation_init(app);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_started);
    if (ImGui::Button("start", button))
        ed.start_simulation_start(app);
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
        ed.start_simulation_start(app);
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
            ed.start_simulation_start_1(app);
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_back());
    if (ImGui::Button("<", small_button))
        ed.start_simulation_back(app);
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_advance());
    if (ImGui::Button(">", small_button))
        ed.start_simulation_advance(app);
    ImGui::EndDisabled();

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

            ImGui::PushID(static_cast<int>(get_index(id)));
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

static bool show_local_simulation_plot_observers_table(application&    app,
                                                       project_editor& ed,
                                                       tree_node& tn) noexcept
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

            for_each_model(ed.pj.sim, tn, [&](auto uid, auto& mdl) noexcept {
                const auto mdl_id = ed.pj.sim.get_id(mdl);
                const auto tn_id  = ed.pj.tree_nodes.get_id(tn);

                auto vobs_id    = undefined<variable_observer_id>();
                auto sub_obs_id = undefined<variable_observer::sub_id>();
                auto enable     = false;

                if (auto* ptr = tn.variable_observer_ids.get(uid); ptr) {
                    if (auto* vobs = ed.pj.variable_observers.try_to_get(*ptr);
                        vobs) {
                        enable     = true;
                        vobs_id    = *ptr;
                        sub_obs_id = vobs->find(tn_id, mdl_id);
                    }
                }

                ImGui::PushID(&mdl);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::BeginDisabled(ed.is_simulation_running());
                if (ImGui::Checkbox("##enable", &enable)) {
                    if (enable) {
                        auto& vobs =
                          get_or_add_variable_observer(ed.pj, vobs_id);
                        vobs_id    = ed.pj.variable_observers.get_id(vobs);
                        sub_obs_id = vobs.push_back(tn_id, mdl_id);
                        tn.variable_observer_ids.set(uid, vobs_id);

                        if (auto* c =
                              app.mod.components.try_to_get<component>(tn.id);
                            c and c->type == component_type::generic) {
                            if (auto* g = app.mod.generic_components.try_to_get(
                                  c->id.generic_id);
                                g) {
                                for (auto& ch : g->children) {
                                    const auto ch_id  = g->children.get_id(ch);
                                    const auto ch_idx = get_index(ch_id);
                                    const auto ch_uid =
                                      g->children_names[ch_idx].sv();

                                    if (ch_uid == uid) {
                                        vobs
                                          .get_names()[get_index(sub_obs_id)] =
                                          ch_uid;
                                        break;
                                    }
                                }
                            }
                        }

                    } else {
                        auto& vobs =
                          get_or_add_variable_observer(ed.pj, vobs_id);
                        vobs_id = ed.pj.variable_observers.get_id(vobs);
                        vobs.erase(tn_id, mdl_id);
                        tn.variable_observer_ids.erase(uid);
                    }
                }
                ImGui::EndDisabled();

                ImGui::TableNextColumn();

                if (enable) {
                    if (auto* vobs =
                          ed.pj.variable_observers.try_to_get(vobs_id);
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
                    if (select_variable_observer(ed.pj, vobs_id) and
                        old_vobs_id != vobs_id) {
                        auto* o =
                          ed.pj.variable_observers.try_to_get(old_vobs_id);
                        auto* n = ed.pj.variable_observers.try_to_get(vobs_id);

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

static auto get_global_parameter(const auto&            tn,
                                 const std::string_view uid) noexcept
  -> global_parameter_id
{
    auto* ptr = tn.parameters_ids.get(uid);
    return ptr ? *ptr : undefined<global_parameter_id>();
}

static bool show_local_simulation_settings(application&    app,
                                           project_editor& ed,
                                           tree_node&      tn) noexcept
{
    int is_modified = 0;

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("c-p",
                          ImVec2(ImGui::GetContentRegionAvail().x, 260.f),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);

        constexpr auto tflags = ImGuiTableFlags_SizingStretchProp;
        constexpr auto fflags = ImGuiTableColumnFlags_WidthFixed;
        constexpr auto sflags = ImGuiTableColumnFlags_WidthStretch;

        if (ImGui::BeginTable("Parameter table", 5, tflags)) {
            ImGui::TableSetupColumn("enable", fflags, 30.f);
            ImGui::TableSetupColumn("name", fflags, 100.f);
            ImGui::TableSetupColumn("model type", fflags, 120.f);
            ImGui::TableSetupColumn("parameter", sflags);
            ImGui::TableSetupColumn("action", fflags, 30.f);
            ImGui::TableHeadersRow();

            auto to_del = undefined<global_parameter_id>();

            for_each_model(ed.pj.sim, tn, [&](auto uid, auto& mdl) noexcept {
                const auto param_id  = get_global_parameter(tn, uid);
                const auto is_enable = ed.pj.parameters.exists(param_id);
                const auto param_idx = get_index(param_id);

                ImGui::PushID(param_idx);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto is_enable_copy = is_enable;
                if (ImGui::Checkbox("##enable", &is_enable_copy)) {
                    if (is_enable_copy) {
                        auto new_id = ed.pj.parameters.alloc();
                        ed.pj.parameters.get<name_str>(new_id) = "New";
                        ed.pj.parameters.get<tree_node_id>(new_id) =
                          ed.pj.tree_nodes.get_id(tn);
                        ed.pj.parameters.get<model_id>(new_id) =
                          ed.pj.sim.models.get_id(mdl);
                        ed.pj.parameters.get<parameter>(new_id).clear();
                        tn.parameters_ids.data.emplace_back(uid, new_id);
                        tn.parameters_ids.sort();
                    } else {
                        tn.parameters_ids.erase(uid);
                        to_del = param_id;
                    }
                }

                ImGui::TableNextColumn();
                if (is_enable) {
                    ImGui::PushItemWidth(-1);
                    ImGui::InputSmallString(
                      "##name", ed.pj.parameters.get<name_str>(param_id));
                    ImGui::PopItemWidth();
                } else {
                    ImGui::TextUnformatted("-");
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);

                ImGui::TableNextColumn();
                if (is_enable)
                    show_parameter_editor(
                      app,
                      ed.pj.sim.srcs,
                      mdl.type,
                      ed.pj.parameters.get<parameter>(param_id));

                ImGui::TableNextColumn();
                if (ImGui::Button("del"))
                    to_del = param_id;

                ImGui::PopID();
            });

            if (is_defined(to_del))
                ed.pj.parameters.free(to_del);

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    return is_modified;
}

static bool show_local_simulation_specific_observers(application&    app,
                                                     project_editor& ed,
                                                     tree_node& tn) noexcept
{
    auto& mod = app.mod;

    if (auto* compo = mod.components.try_to_get<component>(tn.id); compo) {
        switch (compo->type) {
        case component_type::graph:
            if (auto* g = mod.graph_components.try_to_get(compo->id.graph_id);
                g)
                return show_local_observers(app, ed, tn, *compo, *g);
            break;

        case component_type::grid:
            if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id); g)
                return show_local_observers(app, ed, tn, *compo, *g);
            break;

        case component_type::generic:
            return show_local_simulation_plot_observers_table(app, ed, tn);

        default:
            ImGui::TextFormat("Not yet implemented observers for component {}",
                              component_type_names[ordinal(compo->type)]);
            break;
        }
    }

    return false;
}

static void show_local_variables_plot(project_editor&    ed,
                                      variable_observer& v_obs,
                                      tree_node_id       tn_id) noexcept
{
    v_obs.for_each([&](const auto id) noexcept {
        const auto idx = get_index(id);
        auto* obs = ed.pj.sim.observers.try_to_get(v_obs.get_obs_ids()[idx]);

        if (obs and v_obs.get_tn_ids()[idx] == tn_id)
            ed.plot_obs.show_plot_line(
              *obs, v_obs.get_options()[idx], v_obs.get_names()[idx]);
    });
}

// @TODO merge the three next functions with a template on
// template<typename DataArray>
// static bool
// show_simulation_main_observations(application& app, DataArray& d)
// noexcept {...}

static bool show_simulation_table_grid_observers(application& /*app*/,
                                                 project_editor& ed) noexcept
{
    auto to_delete   = undefined<grid_observer_id>();
    bool is_modified = false;

    if (ImGui::BeginTable("Grid observers", 5)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("scale");
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("time-step");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for (auto& grid : ed.pj.grid_observers) {
            ImGui::PushID(&grid);

            ImGui::TableNextRow();
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
            float time_step = grid.time_step;
            ImGui::PushItemWidth(-1.0f);
            if (ImGui::DragFloat("time-step",
                                 &time_step,
                                 0.01f,
                                 grid.time_step.lower,
                                 grid.time_step.upper))
                grid.time_step.set(time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (ImGui::Button("del"))
                to_delete = ed.pj.grid_observers.get_id(grid);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (is_defined(to_delete)) {
        ed.pj.grid_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_simulation_table_graph_observers(application& /*app*/,
                                                  project_editor& ed) noexcept
{
    auto to_delete   = undefined<graph_observer_id>();
    bool is_modified = false;

    if (ImGui::BeginTable("Graph observers", 5)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("child");
        ImGui::TableSetupColumn("enable");
        ImGui::TableSetupColumn("time-step");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_each_data(ed.pj.graph_observers, [&](auto& graph) noexcept {
            ImGui::PushID(&graph);

            ImGui::TableNextRow();
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
            ImGui::BeginDisabled(ed.is_simulation_running());
            ImGui::Checkbox("##button", &enable);
            ImGui::EndDisabled();
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            float time_step = graph.time_step;
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("time-step",
                                 &time_step,
                                 0.01f,
                                 graph.time_step.lower,
                                 graph.time_step.upper))
                graph.time_step.set(time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (ImGui::Button("del"))
                to_delete = ed.pj.graph_observers.get_id(graph);

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    if (is_defined(to_delete)) {
        ed.pj.graph_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_simulation_table_variable_observers(
  application& /*app*/,
  project_editor& ed) noexcept
{
    auto to_delete   = undefined<variable_observer_id>();
    bool is_modified = false;

    if (not ed.pj.variable_observers.can_alloc(1))
        ImGui::TextFormatDisabled(
          "Can not allocate more multi-plot observers (max reached: {})",
          ed.pj.variable_observers.capacity());

    if (ImGui::BeginTable("Plot observers", 5)) {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("child");
        ImGui::TableSetupColumn("enable");
        ImGui::TableSetupColumn("time-step");
        ImGui::TableSetupColumn("delete");
        ImGui::TableHeadersRow();

        for_each_data(ed.pj.variable_observers, [&](auto& variable) noexcept {
            ImGui::PushID(&variable);

            ImGui::TableNextRow();
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
            ImGui::BeginDisabled(ed.is_simulation_running());
            ImGui::Checkbox("##button", &enable);
            ImGui::EndDisabled();
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            float time_step = variable.time_step;
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("time-step",
                                 &time_step,
                                 0.01f,
                                 variable.time_step.lower,
                                 variable.time_step.upper))
                variable.time_step.set(time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            if (ImGui::Button("del"))
                to_delete = ed.pj.variable_observers.get_id(variable);

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    if (ed.pj.variable_observers.can_alloc(1)) {
        if (ImGui::Button("new plot")) {
            auto& o = ed.pj.alloc_variable_observer();
            o.clear();
            is_modified = true;
        }
    }

    if (is_defined(to_delete)) {
        ed.pj.variable_observers.free(to_delete);
        is_modified = true;
    }

    return is_modified;
}

static bool show_project_parameters(application&    app,
                                    project_editor& ed) noexcept
{
    constexpr auto tflags = ImGuiTableFlags_SizingStretchProp;
    constexpr auto fflags = ImGuiTableColumnFlags_WidthFixed;
    constexpr auto sflags = ImGuiTableColumnFlags_WidthStretch;

    auto to_del      = std::optional<global_parameter_id>();
    auto is_modified = 0;

    if (ImGui::BeginTable("Parameter table", 5, tflags)) {
        ImGui::TableSetupColumn("name", fflags, 100.f);
        ImGui::TableSetupColumn("model type", fflags, 120.f);
        ImGui::TableSetupColumn("parameters", sflags);
        ImGui::TableSetupColumn("action", fflags, 60.f);
        ImGui::TableHeadersRow();

        ed.pj.parameters.for_each([&](auto  id,
                                      auto& name,
                                      auto& /*tn_id*/,
                                      auto& mdl_id,
                                      auto& p) noexcept {
            const auto* mdl = ed.pj.sim.models.try_to_get(mdl_id);
            ImGui::PushID(get_index(id));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1.f);
            if (ImGui::InputFilteredString("name", name))
                is_modified++;
            ImGui::PopItemWidth();

            if (mdl) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
                ImGui::TableNextColumn();
                if (show_parameter_editor(app, ed.pj.sim.srcs, mdl->type, p))
                    is_modified++;
            } else {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("deleted model");
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
            }

            ImGui::TableNextColumn();
            if (ImGui::Button("del"))
                to_del = id;

            ImGui::PopID();
        });

        ImGui::EndTable();
    }

    if (to_del.has_value()) {
        ed.pj.parameters.free(*to_del);
        is_modified++;
    }

    return is_modified;
}

static void show_component_observations_actions(project_editor& sim_ed) noexcept
{
    ImGui::TextUnformatted("Column: ");
    ImGui::SameLine();
    if (ImGui::Button("1"))
        sim_ed.tree_node_observation =
          project_editor::tree_node_observation_t(1);
    ImGui::SameLine();
    if (ImGui::Button("2"))
        sim_ed.tree_node_observation =
          project_editor::tree_node_observation_t(2);
    ImGui::SameLine();
    if (ImGui::Button("3"))
        sim_ed.tree_node_observation =
          project_editor::tree_node_observation_t(3);
    ImGui::SameLine();
    if (ImGui::Button("4"))
        sim_ed.tree_node_observation =
          project_editor::tree_node_observation_t(4);
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

static int show_simulation_table_file_observers(application& /*app*/,
                                                project_editor& ed) noexcept
{
    auto is_modified = 0;

    if (ImGui::BeginTable("File observers", 3)) {
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("enable");

        for (const auto id : ed.pj.file_obs.ids) {
            ImGui::TableHeadersRow();
            ImGui::TableNextColumn();

            const auto idx = get_index(id);
            switch (ed.pj.file_obs.types[idx]) {
            case file_observers::type::variables:
                ImGui::TextUnformatted("plot");
                ImGui::TableNextColumn();
                if (auto* sub = ed.pj.variable_observers.try_to_get(
                      ed.pj.file_obs.subids[idx].var);
                    sub)
                    ImGui::TextUnformatted(sub->name.c_str());
                else
                    ImGui::TextUnformatted("-");
                break;
            case file_observers::type::grid:
                ImGui::TextUnformatted("grid");
                ImGui::TableNextColumn();
                if (auto* sub = ed.pj.grid_observers.try_to_get(
                      ed.pj.file_obs.subids[idx].grid);
                    sub)
                    ImGui::TextUnformatted(sub->name.c_str());
                else
                    ImGui::TextUnformatted("-");
                break;
            case file_observers::type::graph:
                ImGui::TextUnformatted("graph");
                ImGui::TableNextColumn();
                if (auto* sub = ed.pj.graph_observers.try_to_get(
                      ed.pj.file_obs.subids[idx].graph);
                    sub)
                    ImGui::TextUnformatted(sub->name.c_str());
                else
                    ImGui::TextUnformatted("-");
                break;
            }

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            if (ImGui::Checkbox("##enable", &ed.pj.file_obs.enables[idx]))
                ++is_modified;
            ImGui::PopItemWidth();
        }

        ImGui::EndTable();
    }

    return is_modified;
}

static bool show_project_observations(application&    app,
                                      project_editor& ed) noexcept
{
    constexpr static auto flags = ImGuiTreeNodeFlags_DefaultOpen;

    auto updated = 0;

    if (ImGui::CollapsingHeader("Plots", flags))
        updated += show_simulation_table_variable_observers(app, ed);

    if (not ed.pj.grid_observers.empty() and
        ImGui::CollapsingHeader("Grid observers", flags))
        updated += show_simulation_table_grid_observers(app, ed);

    if (not ed.pj.graph_observers.empty() and
        ImGui::CollapsingHeader("Graph observers", flags))
        updated += show_simulation_table_graph_observers(app, ed);

    if (not ed.pj.file_obs.ids.empty() and
        ImGui::CollapsingHeader("File observers", flags))
        updated += show_simulation_table_file_observers(app, ed);

    show_component_observations_actions(ed);

    const auto sub_obs_size =
      ImVec2(ImGui::GetContentRegionAvail().x / *ed.tree_node_observation,
             ed.tree_node_observation_height);

    if (ImGui::BeginTable("##obs-table", *ed.tree_node_observation)) {
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        auto pos = 0;
        for_each_data(ed.pj.grid_observers, [&](auto& grid) noexcept {
            ed.grid_obs.show(grid, sub_obs_size);

            ++pos;

            if (pos >= *ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        });

        for_each_data(ed.pj.graph_observers, [&](auto& graph) noexcept {
            ed.graph_obs.show(app, ed, graph, sub_obs_size);

            ++pos;

            if (pos >= *ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        });

        for (auto& vobs : ed.pj.variable_observers) {
            ImGui::PushID(&vobs);
            if (ImPlot::BeginPlot(vobs.name.c_str(), ImVec2(-1, 200))) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

                ImPlot::SetupAxis(ImAxis_X1,
                                  "t",
                                  ImPlotAxisFlags_AutoFit |
                                    ImPlotAxisFlags_RangeFit);
                ImPlot::SetupAxis(ImAxis_Y1,
                                  vobs.name.c_str(),
                                  ImPlotAxisFlags_AutoFit |
                                    ImPlotAxisFlags_RangeFit);

                ImPlot::SetupLegend(ImPlotLocation_North);
                ImPlot::SetupFinish();

                vobs.for_each([&](const auto id) noexcept {
                    const auto idx = get_index(id);
                    if (const auto* obs = ed.pj.sim.observers.try_to_get(
                          vobs.get_obs_ids()[idx]))
                        ed.plot_obs.show_plot_line(
                          *obs, vobs.get_options()[idx], vobs.get_names()[idx]);
                });

                ImPlot::PopStyleVar(2);
                ImPlot::EndPlot();
            }
            ImGui::PopID();

            ++pos;
            if (pos >= *ed.tree_node_observation) {
                pos = 0;
                ImGui::TableNextRow();
            }
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }

    return updated;
}

static void show_component_observations(application&    app,
                                        project_editor& sim_ed,
                                        tree_node&      selected)
{
    show_local_simulation_specific_observers(app, sim_ed, selected);
    show_component_observations_actions(sim_ed);

    const auto sub_obs_size =
      ImVec2(ImGui::GetContentRegionAvail().x / *sim_ed.tree_node_observation,
             sim_ed.tree_node_observation_height);

    auto pos = 0;
    if (ImGui::BeginTable("##obs-table", *sim_ed.tree_node_observation)) {
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        for_specified_data(sim_ed.pj.grid_observers,
                           selected.grid_observer_ids,
                           [&](auto& grid) noexcept {
                               sim_ed.grid_obs.show(grid, sub_obs_size);
                               ++pos;

                               if (pos >= *sim_ed.tree_node_observation) {
                                   pos = 0;
                                   ImGui::TableNextRow();
                               }
                               ImGui::TableNextColumn();
                           });

        for_specified_data(sim_ed.pj.graph_observers,
                           selected.graph_observer_ids,
                           [&](auto& graph) noexcept {
                               sim_ed.graph_obs.show(
                                 app, sim_ed, graph, sub_obs_size);
                               ++pos;

                               if (pos >= *sim_ed.tree_node_observation) {
                                   pos = 0;
                                   ImGui::TableNextRow();
                               }
                               ImGui::TableNextColumn();
                           });

        for (auto& vobs : sim_ed.pj.variable_observers) {
            const auto tn_id = sim_ed.pj.tree_nodes.get_id(selected);
            if (vobs.exists(tn_id)) {
                ImGui::PushID(&vobs);
                if (ImPlot::BeginPlot(vobs.name.c_str(), ImVec2(-1, 200))) {
                    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);
                    ImPlot::SetupAxis(ImAxis_X1,
                                      "t",
                                      ImPlotAxisFlags_AutoFit |
                                        ImPlotAxisFlags_RangeFit);
                    ImPlot::SetupAxis(ImAxis_Y1,
                                      vobs.name.c_str(),
                                      ImPlotAxisFlags_AutoFit |
                                        ImPlotAxisFlags_RangeFit);

                    ImPlot::SetupLegend(ImPlotLocation_North);
                    ImPlot::SetupFinish();
                    if (sim_ed.simulation_state != // TODO may be adding a
                                                   // spin_mutex in observer and
                                                   // lock/try_lock the linear
                                                   // buffer?
                        simulation_status::initializing)
                        show_local_variables_plot(sim_ed, vobs, tn_id);
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

static void show_simulation_editor_treenode(application&    app,
                                            project_editor& ed,
                                            tree_node&      tn) noexcept
{
    if (auto* compo = app.mod.components.try_to_get<component>(tn.id); compo) {
        dispatch_component(app.mod, *compo, [&](auto& c) noexcept {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, grid_component>) {
                ed.grid_sim.display(app, ed, tn, *compo, c);
            } else if constexpr (std::is_same_v<T, graph_component>) {
                ed.graph_sim.display(app, ed, tn, *compo, c);
            } else if constexpr (std::is_same_v<T, generic_component>) {
                ed.generic_sim.display(app);
            } else if constexpr (std::is_same_v<T, hsm_component>) {
                ed.hsm_sim.show_observations(app, ed, tn, *compo, c);
            } else
                ImGui::TextFormatDisabled(
                  "Undefined simulation editor for this component");
        });
    }
}

auto project_editor::show(application& app) noexcept -> show_result_t
{
    if (disable_access)
        return show_result_t::success;

    if (not is_dock_init) {
        ImGui::SetNextWindowDockID(app.get_main_dock_id());
        is_dock_init = true;
    }

    bool is_open = true;
    if (not ImGui::Begin(name.c_str(), &is_open)) {
        ImGui::End();
        return is_open ? show_result_t::success
                       : show_result_t::request_to_close;
    }

    const bool can_be_initialized = !any_equal(simulation_state,
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

    if (ImGui::BeginTable("##ed", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn(
          "Hierarchy", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn(
          "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        app.project_wnd.show(*this);

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("##ed-sim", ImGui::GetContentRegionAvail())) {
            show_simulation_action_buttons(app,
                                           *this,
                                           can_be_initialized,
                                           can_be_started,
                                           can_be_paused,
                                           can_be_restarted,
                                           can_be_stopped);

            auto* selected = pj.node(m_selected_tree_node);

            if (ImGui::BeginTabBar("##SimulationTabBar")) {
                if (ImGui::BeginTabItem("Parameters")) {
                    show_project_parameters(app, *this);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Observations")) {
                    show_project_observations(app, *this);
                    ImGui::EndTabItem();
                }

                if (selected) {
                    if (ImGui::BeginTabItem("Component parameters")) {
                        show_local_simulation_settings(app, *this, *selected);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Component observations")) {
                        show_component_observations(app, *this, *selected);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Component graph")) {
                        show_simulation_editor_treenode(app, *this, *selected);
                        ImGui::EndTabItem();
                    }
                } else {
                    if (ImGui::BeginTabItem("Full simulation graph")) {
                        flat_sim.display(app);
                        ImGui::EndTabItem();
                    }
                }

                if (ImGui::BeginTabItem("Input data")) {
                    data_ed.show(app);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::End();

    if (save_project_file) {
        if (auto* regf = app.mod.registred_paths.try_to_get(project_file);
            regf) {
            save_project_file    = false;
            save_as_project_file = false;

            app.start_save_project(project_file, app.pjs.get_id(*this));
        } else {
            save_project_file    = false;
            save_as_project_file = true;
        }
    }

    if (save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_save_file(title, default_filename, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path   = app.mod.registred_paths.alloc();
                    project_file = app.mod.registred_paths.get_id(path);
                    auto  u8str  = app.f_dialog.result.u8string();
                    auto* str    = reinterpret_cast<const char*>(u8str.c_str());
                    path.path    = str;

                    app.start_save_project(project_file, app.pjs.get_id(*this));
                }
            }
            save_project_file    = false;
            save_as_project_file = false;

            app.f_dialog.clear();
        }
    }

    return is_open ? show_result_t::success : show_result_t::request_to_close;
}

void flat_simulation_editor::center_camera(const ImVec2 canvas) noexcept
{
    ImVec2 distance(bottom_right[0] - top_left[0],
                    bottom_right[1] - top_left[1]);
    ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                  (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

    scrolling.x = ((-center.x * zoom) + (canvas.x / 2.f));
    scrolling.y = ((-center.y * zoom) + (canvas.y / 2.f));
}

void flat_simulation_editor::auto_fit_camera(const ImVec2 canvas) noexcept
{
    ImVec2 distance(bottom_right[0] - top_left[0],
                    bottom_right[1] - top_left[1]);
    ImVec2 center((bottom_right[0] - top_left[0]) / 2.0f + top_left[0],
                  (bottom_right[1] - top_left[1]) / 2.0f + top_left[1]);

    zoom        = std::min(canvas.x / distance.x, canvas.y / distance.y);
    scrolling.x = ((-center.x * zoom) + (canvas.x / 2.f));
    scrolling.y = ((-center.y * zoom) + (canvas.y / 2.f));
}

constexpr float  MW  = 50.f;
constexpr float  MH  = 50.f;
constexpr float  MW2 = MW / 2.f;
constexpr float  MH2 = MW / 2.f;
constexpr ImVec2 model_width_height(MW, MH);

bool flat_simulation_editor::display(application& app) noexcept
{
    if (not is_ready.test_and_set()) {
        rebuild(app);
        return false;
    }

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    if (actions[action::camera_center])
        center_camera(canvas_sz);

    if (actions[action::camera_auto_fit])
        auto_fit_camera(canvas_sz);

    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active  = ImGui::IsItemActive();

    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    const float mouse_threshold_for_pan = -1.f;
    if (is_active) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle,
                                   mouse_threshold_for_pan)) {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
        }
    }

    if (is_hovered and io.MouseWheel != 0.f) {
        zoom = zoom + (io.MouseWheel * zoom * 0.1f);
        zoom = ImClamp(zoom, 0.1f, 1000.f);
    }

    // ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    // if (drag_delta.x == 0.0f and drag_delta.y == 0.0f and
    //     (not selected_nodes.empty())) {
    //     ImGui::OpenPopupOnItemClick("Canvas-Context",
    //                                 ImGuiPopupFlags_MouseButtonRight);
    // }

    // if (ImGui::BeginPopup("Canvas-Context")) {
    //     const auto click = ImGui::GetMousePosOnOpeningCurrentPopup();
    //     if (ImGui::BeginMenu("Actions")) {
    //         ImGui::EndMenu();
    //     }
    //     ImGui::EndPopup();
    // }

    if (is_hovered) {
        if (not run_selection and ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            run_selection   = true;
            start_selection = io.MousePos;
        }

        if (run_selection and ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            run_selection = false;
            end_selection = io.MousePos;

            if (start_selection == end_selection) {
                selected_nodes.clear();
            } else {
                ImVec2 bmin{
                    std::min(start_selection.x, end_selection.x),
                    std::min(start_selection.y, end_selection.y),
                };

                ImVec2 bmax{
                    std::max(start_selection.x, end_selection.x),
                    std::max(start_selection.y, end_selection.y),
                };

                selected_nodes.clear();

                data.try_read_only([&](const auto& d) {
                    auto& pj_ed = container_of(this, &project_editor::flat_sim);
                    for (const auto& mdl : pj_ed.pj.sim.models) {
                        const auto mdl_id = pj_ed.pj.sim.models.get_id(mdl);
                        const auto i      = get_index(mdl_id);

                        ImVec2 p_min(
                          origin.x + ((d.positions[i][0] - MW2) * zoom),
                          origin.y + ((d.positions[i][1] - MH2) * zoom));

                        ImVec2 p_max(
                          origin.x + ((d.positions[i][0] + MW2) * zoom),
                          origin.y + ((d.positions[i][1] + MH2) * zoom));

                        if (p_min.x >= bmin.x and p_max.x < bmax.x and
                            p_min.y >= bmin.y and p_max.y < bmax.y) {
                            selected_nodes.emplace_back(mdl_id);
                        }
                    }
                });
            }
        }
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    const float GRID_STEP = 64.0f;

    for (float x = std::fmod(scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           IM_COL32(200, 200, 200, 40));

    for (float y = std::fmod(scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           IM_COL32(200, 200, 200, 40));

    data.try_read_only([&](const auto& d) {
        auto& pj_ed = container_of(this, &project_editor::flat_sim);

        if (d.positions.empty())
            return;

        small_vector<tree_node*, max_component_stack_size> stack;
        stack.emplace_back(pj_ed.pj.tn_head());

        while (!stack.empty()) {
            auto cur   = stack.back();
            auto tn_id = pj_ed.pj.tree_nodes.get_id(*cur);
            stack.pop_back();

            for (const auto& ch : cur->children) {
                if (ch.type == tree_node::child_node::type::model) {
                    const auto  mdl_id = ch.mdl;
                    const auto* mdl    = pj_ed.pj.sim.models.try_to_get(mdl_id);
                    const auto  i      = get_index(mdl_id);

                    if (not mdl)
                        continue;

                    ImVec2 p_min(origin.x + ((d.positions[i][0] - MW2) * zoom),
                                 origin.y + ((d.positions[i][1] - MH2) * zoom));

                    ImVec2 p_max(origin.x + ((d.positions[i][0] + MW2) * zoom),
                                 origin.y + ((d.positions[i][1] + MH2) * zoom));

                    if (canvas_p0.x <= p_min.x and p_min.x <= canvas_p1.x and
                        canvas_p0.y <= p_max.y and p_max.y <= canvas_p1.y)
                        draw_list->AddRectFilled(
                          p_min, p_max, d.tn_colors[tn_id]);

                    dispatch(
                      *mdl,
                      [&]<typename Dynamics>(
                        Dynamics& dyn, const auto& sim, const auto src_mdl_id) {
                          if constexpr (has_output_port<Dynamics>) {
                              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                                  for (auto* block =
                                         sim.nodes.try_to_get(dyn.y[i]);
                                       block;
                                       block =
                                         sim.nodes.try_to_get(block->next)) {

                                      const auto src_idx =
                                        get_index(src_mdl_id);
                                      for (auto it = block->nodes.begin(),
                                                et = block->nodes.end();
                                           it != et;
                                           ++it) {
                                          if (const auto* dst =
                                                sim.models.try_to_get(
                                                  it->model)) {

                                              ImVec2 from(
                                                origin.x +
                                                  (d.positions[src_idx][0] *
                                                   zoom),
                                                origin.y +
                                                  (d.positions[src_idx][1] *
                                                   zoom));

                                              ImVec2 to(
                                                origin.x +
                                                  (d.positions[get_index(
                                                     it->model)][0] *
                                                   zoom),
                                                origin.y +
                                                  (d.positions[get_index(
                                                     it->model)][1] *
                                                   zoom));

                                              draw_list->AddLine(
                                                from,
                                                to,
                                                IM_COL32(0, 255, 0, 255),
                                                1.0f);
                                          }
                                      }
                                  }
                              }
                          }
                      },
                      pj_ed.pj.sim,
                      pj_ed.pj.sim.models.get_id(mdl));
                }
            }

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child(); child)
                stack.emplace_back(child);
        }

        /*
        for (const auto& mdl : pj_ed.pj.sim.models) {
            const auto mdl_id = pj_ed.pj.sim.models.get_id(mdl);
            const auto i      = get_index(mdl_id);

            ImVec2 p_min(origin.x + ((d.positions[i][0] - 2.f) * zoom),
                         origin.y + ((d.positions[i][1] - 2.f) * zoom));

            ImVec2 p_max(origin.x + ((d.positions[i][0] + 2.f) * zoom),
                         origin.y + ((d.positions[i][1] + 2.f) * zoom));

            if (canvas_p0.x <= p_min.x and p_min.x <= canvas_p1.x and
                canvas_p0.y <= p_max.y and p_max.y <= canvas_p1.y)
                draw_list->AddRectFilled(
                  p_min, p_max, IM_COL32(255, 0, 0, 255));
        }

        for (const auto& mdl : pj_ed.pj.sim.models) {
            dispatch(
              mdl,
              [&]<typename Dynamics>(
                Dynamics& dyn, const auto& sim, const auto src_mdl_id) {
                  if constexpr (has_output_port<Dynamics>) {
                      for (int i = 0, e = length(dyn.y); i != e; ++i) {
                          for (auto* block = sim.nodes.try_to_get(dyn.y[i]);
                               block;
                               block = sim.nodes.try_to_get(block->next)) {

                              const auto src_idx = get_index(src_mdl_id);
                              for (auto it = block->nodes.begin(),
                                        et = block->nodes.end();
                                   it != et;
                                   ++it) {
                                  if (const auto* dst =
                                        sim.models.try_to_get(it->model)) {

                                      ImVec2 from(
                                        origin.x +
                                          (d.positions[src_idx][0] * zoom),
                                        origin.y +
                                          (d.positions[src_idx][1] * zoom));

                                      ImVec2 to(
                                        origin.x + (d.positions[get_index(
                                                      it->model)][0] *
                                                    zoom),
                                        origin.y + (d.positions[get_index(
                                                      it->model)][1] *
                                                    zoom));

                                      draw_list->AddLine(
                                        from,
                                        to,
                                        IM_COL32(0, 255, 0, 255),
                                        1.0f);
                                  }
                              }
                          }
                      }
                  }
              },
              pj_ed.pj.sim,
              pj_ed.pj.sim.models.get_id(mdl));
        }
        */
    });

    if (run_selection) {
        end_selection = io.MousePos;

        if (start_selection == end_selection) {
            selected_nodes.clear();
        } else {
            ImVec2 bmin{
                std::min(start_selection.x, io.MousePos.x),
                std::min(start_selection.y, io.MousePos.y),
            };

            ImVec2 bmax{
                std::max(start_selection.x, io.MousePos.x),
                std::max(start_selection.y, io.MousePos.y),
            };

            draw_list->AddRectFilled(bmin, bmax, IM_COL32(200, 0, 0, 127));
        }
    }

    draw_list->PopClipRect();

    return true;
}

void flat_simulation_editor::reset() noexcept
{
    distance        = { 15.f, 15.f };
    scrolling       = { 0.f, 0.f };
    zoom            = 1.f;
    start_selection = { 0.f, 0.f };
    end_selection   = { 0.f, 0.f };

    selected_nodes.clear();
    run_selection = false;
}

constexpr static auto clear(auto&      data,
                            const auto models,
                            const auto tns) noexcept
{
    data.positions.resize(models, ImVec2(0.f, 0.f));

    data.tn_rects.resize(tns, ImVec2(0.f, 0.f));
    data.tn_centers.resize(tns, ImVec2(0.f, 0.f));
    data.tn_factors.resize(tns, ImVec2(1.f, 1.f));
    data.tn_colors.resize(tns, IM_COL32(255, 255, 255, 255));
    data.tn_children.resize(tns, 0u);
}

static ImU32 compute_color(const float t) noexcept
{
    static const ImColor tables[] = {
        IM_COL32(103, 0, 31, 255),    IM_COL32(178, 24, 43, 255),
        IM_COL32(214, 96, 77, 255),   IM_COL32(244, 165, 130, 255),
        IM_COL32(253, 219, 199, 255), IM_COL32(247, 247, 247, 255),
        IM_COL32(209, 229, 240, 255), IM_COL32(146, 197, 222, 255),
        IM_COL32(67, 147, 195, 255),  IM_COL32(33, 102, 172, 255),
        IM_COL32(5, 48, 97, 255)
    };

    const auto i1 =
      static_cast<int>(static_cast<float>(length(tables) - 1) * t);
    const auto i2 = i1 + 1;

    if (i2 == length(tables) || length(tables) == 1)
        return tables[i1];

    const float den = 1.0f / (length(tables) - 1);
    const float t1  = static_cast<float>(i1) * den;
    const float t2  = static_cast<float>(i2) * den;
    const float tr  = (t - t1) / (t2 - t1);
    const auto  s   = static_cast<ImU32>(tr * 256.f);
    const ImU32 af  = 256 - s;
    const ImU32 bf  = s;
    const ImU32 al  = (tables[i1] & 0x00ff00ff);
    const ImU32 ah  = (tables[i1] & 0xff00ff00) >> 8;
    const ImU32 bl  = (tables[i2] & 0x00ff00ff);
    const ImU32 bh  = (tables[i2] & 0xff00ff00) >> 8;
    const ImU32 ml  = (al * af + bl * bf);
    const ImU32 mh  = (ah * af + bh * bf);
    return (mh & 0xff00ff00) | ((ml & 0xff00ff00) >> 8);
}

constexpr static void move_tn(auto&              data,
                              const tree_node_id tn_id,
                              const float        shift_x,
                              const float        shift_y) noexcept
{
    data.tn_centers[tn_id].x += shift_x;
    data.tn_centers[tn_id].y += shift_y;
}

constexpr static void move_model(auto&          data,
                                 const model_id mdl_id,
                                 const float    shift_x,
                                 const float    shift_y) noexcept
{
    data.positions[mdl_id][0] += shift_x;
    data.positions[mdl_id][1] += shift_y;
}

template<typename T>
concept float_position_xy = requires(T t) {
    { t.x } -> std::convertible_to<float>;
    { t.y } -> std::convertible_to<float>;
};

class rect_bound
{
    float x_min = +INFINITY;
    float x_max = -INFINITY;
    float y_min = +INFINITY;
    float y_max = -INFINITY;

public:
    constexpr void update(const float_position_xy auto pos) noexcept
    {
        x_min = std::min(x_min, static_cast<float>(pos.x) - MW2);
        x_max = std::max(x_max, static_cast<float>(pos.x) + MW2);
        y_min = std::min(y_min, static_cast<float>(pos.y) - MH2);
        y_max = std::max(y_max, static_cast<float>(pos.y) + MH2);
    }

    constexpr void update(const float x, const float y) noexcept
    {
        x_min = std::min(x_min, x - MW2);
        x_max = std::max(x_max, x + MW2);
        y_min = std::min(y_min, y - MH2);
        y_max = std::max(y_max, y + MH2);
    }

    constexpr auto distance() const noexcept
    {
        return ImVec2(x_max - x_min, y_max - y_min);
    }
};

constexpr static void compute_colors(auto&       data,
                                     const auto& tree_nodes) noexcept
{
    const auto tns_f = static_cast<float>(tree_nodes.size());

    for (const auto& tn : tree_nodes) {
        const auto tn_id   = tree_nodes.get_id(tn);
        const auto tn_id_f = static_cast<float>(get_index(tn_id));

        data.tn_colors[tn_id] = compute_color(tn_id_f / tns_f);
    }
}

static auto compute_max_rect(const vector<ImVec2>& tn_rects,
                             const project&        pj,
                             const tree_node&      parent) noexcept
{
    auto ret = model_width_height;

    for (const auto& child : parent.children) {
        if (child.is_tree_node()) {
            const auto* sub_tn    = child.tn;
            const auto  sub_tn_id = pj.tree_nodes.get_id(sub_tn);

            ret = ImMax(ret, tn_rects[sub_tn_id]);
        }
    }

    return ret;
}

struct max_point_in_vh_lines_result {
    unsigned hpoints;
    unsigned vpoints;
};

static auto max_point_in_vh_lines(const graph_component& g) noexcept
  -> max_point_in_vh_lines_result
{
    vector<float> hlines(g.cache.size(), reserve_tag{});
    vector<float> vlines(g.cache.size(), reserve_tag{});

    for (const auto& child : g.cache) {
        const auto  child_id      = g.cache.get_id(child);
        const auto  graph_node_id = child.node_id;
        const auto& pos           = g.g.node_positions[graph_node_id];

        hlines.push_back(pos[0]);
        vlines.push_back(pos[1]);
    }

    std::ranges::sort(hlines);
    std::ranges::sort(vlines);

    {
        auto it = hlines.begin();
        while (it != hlines.end()) {
            auto next = it + 1;
            if (next == hlines.end())
                break;

            if (*next - *it < .1f)
                hlines.erase(next);
            else
                ++it;
        }
    }

    {
        auto it = vlines.begin();
        while (it != vlines.end()) {
            auto next = it + 1;
            if (next == vlines.end())
                break;

            if (*next - *it < .1f)
                vlines.erase(next);
            else
                ++it;
        }
    }

    return { .hpoints = hlines.size(), .vpoints = vlines.size() };
}

static void compute_rect(auto&            data,
                         const project&   pj,
                         const modeling&  mod,
                         const tree_node& tn,
                         const component& compo) noexcept
{
    const auto tn_id = pj.tree_nodes.get_id(tn);

    switch (compo.type) {
    case component_type::generic:
        if (auto* g = mod.generic_components.try_to_get(compo.id.generic_id)) {
            rect_bound bound;

            for (const auto& c : g->children) {
                const auto  c_id = g->children.get_id(c);
                const auto& pos  = g->children_positions[c_id];

                switch (c.type) {
                case child_type::model:
                    bound.update(pos);
                    break;

                case child_type::component: {
                    const auto* sub_tn    = tn.children[c_id].tn;
                    const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);
                    const auto& sub_pos   = data.tn_rects[sub_tn_id];
                    bound.update(pos.x + sub_pos.x, pos.y + sub_pos.y);
                } break;
                }
            }

            data.tn_rects[tn_id] = bound.distance();
        }
        break;

    case component_type::graph:
        if (auto* g = mod.graph_components.try_to_get(compo.id.graph_id)) {
            const auto tn_rect_max = compute_max_rect(data.tn_rects, pj, tn);
            const auto max_points  = max_point_in_vh_lines(*g);
            // rect_bound bound;

            // for (const auto& child : g->cache) {
            //     const auto child_id      = g->cache.get_id(child);
            //     const auto graph_node_id = child.node_id;
            //     const auto pos           =
            //     g->g.node_positions[graph_node_id];

            //    debug::ensure(tn.children[child_id].is_tree_node());
            //    debug::ensure(tn.children[child_id].tn);

            //    if (tn.children[child_id].is_tree_node()) {
            //        const auto* sub_tn    = tn.children[child_id].tn;
            //        const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);
            //        const auto& sub_pos   = data.tn_rects[sub_tn_id];

            //        bound.update(pos[0] + sub_pos[0], pos[1] + sub_pos[1]);
            //    }
            //}

            data.tn_rects[tn_id] = ImVec2(max_points.hpoints * tn_rect_max.x,
                                          max_points.vpoints * tn_rect_max.y);
            data.tn_factors[tn_id] =
              ImVec2(data.tn_rects[tn_id].x / max_points.hpoints,
                     data.tn_rects[tn_id].y / max_points.vpoints);
        }
        break;

    case component_type::grid:
        if (auto* g = mod.grid_components.try_to_get(compo.id.grid_id)) {
            const auto rows    = g->row();
            const auto columns = g->column();
            auto       dist    = ImVec2(MH, MH);

            for (const auto& child : g->cache) {
                const auto child_id = g->cache.get_id(child);

                if (tn.children[child_id].is_tree_node()) {
                    debug::ensure(tn.children[child_id].tn);

                    const auto* sub_tn    = tn.children[child_id].tn;
                    const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);
                    const auto& sub_rect  = data.tn_rects[sub_tn_id];

                    dist = ImMax(dist, sub_rect);
                }
            }

            dist.x *= g->column();
            dist.y *= g->row();

            data.tn_rects[tn_id] = dist;
        }
        break;

    case component_type::hsm:
    case component_type::none:
        break;
    }
}

static void compute_center_and_position(auto&            data,
                                        const project&   pj,
                                        const modeling&  mod,
                                        const tree_node& tn,
                                        const component& compo) noexcept
{
    const auto tn_id  = pj.tree_nodes.get_id(tn);
    const auto center = data.tn_centers[tn_id];
    const auto rect   = data.tn_rects[tn_id];
    const auto factor = data.tn_factors[tn_id];

    switch (compo.type) {
    case component_type::generic:
        if (auto* g = mod.generic_components.try_to_get(compo.id.generic_id)) {
            for (const auto& c : g->children) {
                const auto  c_id = g->children.get_id(c);
                const auto& pos  = g->children_positions[c_id];

                switch (c.type) {
                case child_type::model:
                    move_model(data,
                               tn.children[c_id].mdl,
                               center.x + pos.x - MW,
                               center.y + pos.y - MH);
                    break;

                case child_type::component: {
                    const auto* sub_tn    = tn.children[c_id].tn;
                    const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);

                    debug::ensure(sub_tn_id != tn_id);

                    move_tn(data,
                            sub_tn_id,
                            center.x + pos.x - MW,
                            center.y + pos.y - MH);
                } break;
                }
            }
        }
        break;

    case component_type::graph:
        if (auto* g = mod.graph_components.try_to_get(compo.id.graph_id)) {
            for (const auto& c : g->cache) {
                const auto  c_id      = g->cache.get_id(c);
                const auto* sub_tn    = tn.children[c_id].tn;
                const auto& pos       = g->g.node_positions[c.node_id];
                const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);
                const auto  sub_rect  = data.tn_rects[sub_tn_id];

                debug::ensure(sub_tn_id != tn_id);

                move_tn(data,
                        sub_tn_id,
                        center.x + (factor.x * pos[0]) + sub_rect[0] - MW,
                        center.y + (factor.y * pos[1]) + sub_rect[1] - MH);
            }
        }
        break;

    case component_type::grid:
        if (auto* g = mod.grid_components.try_to_get(compo.id.grid_id)) {
            const auto sub_rect =
              ImVec2(rect.x / g->column(), rect.y / g->row());

            for (const auto& child : g->cache) {
                const auto  child_id  = g->cache.get_id(child);
                const auto* sub_tn    = tn.children[child_id].tn;
                const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);

                move_tn(data,
                        sub_tn_id,
                        center.x + sub_rect.x * static_cast<float>(child.col),
                        center.y + sub_rect.y * static_cast<float>(child.row));
            }
        }
        break;

    case component_type::hsm:
    case component_type::none:
        break;
    }
}

constexpr static auto update_children(vector<u32>&       children,
                                      const tree_node&   tn,
                                      const tree_node_id tn_id) noexcept
{
    children[tn_id] += tn.children.size();
}

static auto get_tn_id(const project& pj, const tree_node& tn) noexcept
  -> tree_node_id
{
    return pj.tree_nodes.get_id(tn);
}

static auto get_tn_id(const project& pj, const tree_node* tn) noexcept
  -> tree_node_id
{
    return pj.tree_nodes.get_id(tn);
}

void flat_simulation_editor::compute_children(application& /*app*/,
                                              data_type& d) noexcept
{
    auto& pj_ed = container_of(this, &project_editor::flat_sim);

    const auto* head    = pj_ed.pj.tn_head();
    const auto  head_id = get_tn_id(pj_ed.pj, *head);

    struct stack_elem {
        const tree_node* tn           = nullptr;
        bool             read_child   = false;
        bool             read_sibling = false;
    };

    vector<stack_elem> stack(max_component_stack_size, reserve_tag{});
    update_children(d.tn_children, *head, head_id);
    stack.emplace_back(head);

    while (not stack.empty()) {
        const auto cur       = stack.back();
        const auto cur_tn_id = get_tn_id(pj_ed.pj, cur.tn);

        if (cur.read_child) {
            if (cur.read_sibling) {
                stack.pop_back();
                if (auto* parent = cur.tn->tree.get_parent())
                    d.tn_children[get_tn_id(pj_ed.pj, parent)] +=
                      d.tn_children[cur_tn_id];
            } else {
                stack.back().read_sibling = true;
                if (auto* sibling = cur.tn->tree.get_sibling(); sibling) {
                    update_children(
                      d.tn_children, *sibling, get_tn_id(pj_ed.pj, *sibling));
                    stack.emplace_back(sibling);
                }
            }
        } else {
            stack.back().read_child = true;
            if (auto* child = cur.tn->tree.get_child()) {
                update_children(
                  d.tn_children, *child, get_tn_id(pj_ed.pj, *child));
                stack.emplace_back(child);
            }
        }
    }
}

void flat_simulation_editor::compute_rects(application& app,
                                           data_type&   d) noexcept
{
    auto& pj_ed = container_of(this, &project_editor::flat_sim);

    struct stack_elem {
        tree_node* tn;
        bool       read_child   = false;
        bool       read_sibling = false;
    };

    vector<stack_elem> stack(max_component_stack_size, reserve_tag{});
    stack.emplace_back(pj_ed.pj.tn_head());

    while (not stack.empty()) {
        const auto cur = stack.back();
        // const auto cur_tn_id = get_tn_id(pj_ed.pj, cur.tn);

        if (cur.read_child) {
            if (cur.read_sibling) {
                stack.pop_back();
                auto& c = app.mod.components.get<component>(cur.tn->id);

                // children and sibling tn_rects are already computed.
                compute_rect(d, pj_ed.pj, app.mod, *cur.tn, c);
            } else {
                stack.back().read_sibling = true;
                if (auto* sibling = cur.tn->tree.get_sibling(); sibling) {
                    stack.emplace_back(sibling);
                }
            }
        } else {
            stack.back().read_child = true;
            if (auto* child = cur.tn->tree.get_child()) {
                stack.emplace_back(child);
            }
        }
    }
}

void flat_simulation_editor::compute_centers_and_positions(
  application& app,
  data_type&   d) noexcept
{
    auto& pj_ed = container_of(this, &project_editor::flat_sim);

    struct stack_elem {
        const tree_node* tn           = nullptr;
        bool             read_child   = false;
        bool             read_sibling = false;
    };

    vector<stack_elem> stack(max_component_stack_size, reserve_tag{});
    stack.emplace_back(pj_ed.pj.tn_head());

    while (not stack.empty()) {
        const auto cur = stack.back();
        stack.pop_back();

        auto& c = app.mod.components.get<component>(cur.tn->id);
        compute_center_and_position(d, pj_ed.pj, app.mod, *cur.tn, c);

        if (auto* sibling = cur.tn->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur.tn->tree.get_child())
            stack.emplace_back(child);
    }
}

static auto to_print(const std::string_view name,
                     const auto&            d,
                     const project&         pj) noexcept
{
    fmt::print("- {:{}} ", name, 20);
    for (const auto& tn : pj.tree_nodes) {
        const auto tn_id = pj.tree_nodes.get_id(tn);

        fmt::print("rect {},{} centers {},{} colors {} children {}\n",
                   d.tn_rects[tn_id].x,
                   d.tn_rects[tn_id].y,
                   d.tn_centers[tn_id].x,
                   d.tn_centers[tn_id].y,
                   d.tn_colors[tn_id],
                   d.tn_children[tn_id]);
    }
}

void flat_simulation_editor::rebuild(application& app) noexcept
{
    app.add_gui_task([&]() {
        data.read_write([&](auto& d) {
            auto&      pj_ed = container_of(this, &project_editor::flat_sim);
            const auto mdls  = pj_ed.pj.sim.models.size();
            const auto tns   = pj_ed.pj.tree_nodes.size();

            clear(d, mdls, tns);

            compute_children(app, d);
            to_print("   compute-children:\n", d, pj_ed.pj);
            compute_rects(app, d);
            to_print("   compute-rects   :\n", d, pj_ed.pj);
            compute_centers_and_positions(app, d);
            to_print("   compute-centers :\n", d, pj_ed.pj);
            compute_colors(d, pj_ed.pj.tree_nodes);
            to_print("   compute-colors  :\n", d, pj_ed.pj);

            const auto* head    = pj_ed.pj.tn_head();
            const auto  head_id = get_tn_id(pj_ed.pj, head);

            top_left     = ImVec2(0.f, d.tn_rects[head_id].y);
            bottom_right = ImVec2(d.tn_rects[head_id].x, 0.f);

            // for (const auto& mdl : pj_ed.pj.sim.models) {
            //     const auto mdl_id = pj_ed.pj.sim.get_id(mdl);

            //    fmt::print("mdl_id: {},{}\n",
            //               d.positions[mdl_id].x,
            //               d.positions[mdl_id].y);
            //}
        });
    });
}

} // namesapce irt
