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
                        app.generic_sim.init(app, *this, *tree, *compo, *gen);
                    }
                }
            }
        } else {
            app.generic_sim.init(app, *this);
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

static void show_local_variables_plot(application&       app,
                                      project_editor&    ed,
                                      variable_observer& v_obs,
                                      tree_node_id       tn_id) noexcept
{
    v_obs.for_each([&](const auto id) noexcept {
        const auto idx = get_index(id);
        auto* obs = ed.pj.sim.observers.try_to_get(v_obs.get_obs_ids()[idx]);

        if (obs and v_obs.get_tn_ids()[idx] == tn_id)
            app.plot_obs.show_plot_line(
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
    ImGui::TextUnformatted("-");
    ImGui::SameLine();
    if (ImGui::Button("Default"))
        sim_ed.tree_node_observation_height = 300.f;
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
    ImGuiContext& g = *GImGui;
    const auto    sub_obs_size =
      ImVec2((ImGui::GetContentRegionAvail().x - 2.f * g.Style.IndentSpacing) /
               *ed.tree_node_observation,
             ed.tree_node_observation_height / *ed.tree_node_observation);

    auto updated = 0;

    if (ImGui::TreeNodeEx("All", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNodeEx("Observers")) {
            if (not ed.pj.variable_observers.empty())
                updated += show_simulation_table_variable_observers(app, ed);

            if (not ed.pj.grid_observers.empty())
                updated += show_simulation_table_grid_observers(app, ed);

            if (not ed.pj.graph_observers.empty())
                updated += show_simulation_table_graph_observers(app, ed);

            if (not ed.pj.file_obs.ids.empty())
                updated += show_simulation_table_file_observers(app, ed);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_component_observations_actions(ed);

            if (ImGui::BeginTable("##obs-table", *ed.tree_node_observation)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto pos = 0;
                for_each_data(ed.pj.grid_observers, [&](auto& grid) noexcept {
                    app.grid_obs.show(grid, sub_obs_size);

                    ++pos;

                    if (pos >= *ed.tree_node_observation) {
                        pos = 0;
                        ImGui::TableNextRow();
                    }
                    ImGui::TableNextColumn();
                });

                for_each_data(ed.pj.graph_observers, [&](auto& graph) noexcept {
                    app.graph_obs.show(ed, graph, sub_obs_size);

                    ++pos;

                    if (pos >= *ed.tree_node_observation) {
                        pos = 0;
                        ImGui::TableNextRow();
                    }
                    ImGui::TableNextColumn();
                });

                for (auto& vobs : ed.pj.variable_observers) {
                    ImGui::PushID(&vobs);
                    ImGui::BeginChild("##vobs", sub_obs_size);
                    if (ImPlot::BeginPlot(vobs.name.c_str(), sub_obs_size)) {
                        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

                        ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                        ImPlot::SetupAxisLimits(ImAxis_X1,
                                                ed.pj.sim.limits.begin(),
                                                ed.pj.sim.limits.end());
                        ImPlot::SetupFinish();

                        vobs.for_each([&](const auto id) noexcept {
                            const auto idx = get_index(id);
                            if (const auto* obs =
                                  ed.pj.sim.observers.try_to_get(
                                    vobs.get_obs_ids()[idx]))
                                app.plot_obs.show_plot_line(
                                  *obs,
                                  vobs.get_options()[idx],
                                  vobs.get_names()[idx]);
                        });

                        ImPlot::PopStyleVar(2);
                        ImPlot::EndPlot();
                    }
                    ImGui::EndChild();
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
            ImGui::TreePop();
        }

        ImGui::TreePop();
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
                               app.grid_obs.show(grid, sub_obs_size);
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
                               app.graph_obs.show(sim_ed, graph, sub_obs_size);
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

                    ImPlot::SetupLegend(ImPlotLocation_NorthWest);
                    ImPlot::SetupAxisLimits(ImAxis_X1,
                                            sim_ed.pj.sim.limits.begin(),
                                            sim_ed.pj.sim.limits.end());
                    ImPlot::SetupAxis(ImAxis_Y1,
                                      vobs.name.c_str(),
                                      ImPlotAxisFlags_AutoFit |
                                        ImPlotAxisFlags_RangeFit);
                    ImPlot::SetupFinish();

                    if (sim_ed.simulation_state != // TODO may be adding a
                                                   // spin_mutex in observer
                                                   // and lock/try_lock the
                                                   // linear buffer?
                        simulation_status::initializing)
                        show_local_variables_plot(app, sim_ed, vobs, tn_id);
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
                app.grid_sim.display(app, ed, tn, *compo, c);
            } else if constexpr (std::is_same_v<T, graph_component>) {
                app.graph_sim.display(app, ed, tn, *compo, c);
            } else if constexpr (std::is_same_v<T, generic_component>) {
                app.generic_sim.display(app, ed);
            } else if constexpr (std::is_same_v<T, hsm_component>) {
                app.hsm_sim.show_observations(app, ed, tn, *compo, c);
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
                }

                if (ImGui::BeginTabItem("Full simulation graph")) {
                    app.flat_sim.display(app, *this);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Input data")) {
                    app.data_ed.show(app, *this);
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

void project_editor::display_subwindows(application& app) noexcept
{
    for (auto it = visualisation_eds.begin(); it != visualisation_eds.end();) {
        auto del = true;

        if (auto* ed = app.graph_eds.try_to_get(it->graph_ed_id)) {
            if (auto* tn = pj.tree_nodes.try_to_get(it->tn_id)) {
                if (auto* obs =
                      pj.graph_observers.try_to_get(it->graph_obs_id)) {
                    if (ed->show(app, *this, *tn, *obs) ==
                        graph_editor::show_result_type::request_to_close) {
                        app.graph_eds.free(*ed);
                    } else
                        del = false;
                }
            }
        }

        if (del) {
            it = visualisation_eds.erase(it);
        } else
            ++it;
    }
}

void project_editor::close_subwindows(application& app) noexcept
{
    for (auto it = visualisation_eds.begin(); it != visualisation_eds.end();)
        if (auto* ed = app.graph_eds.try_to_get(it->graph_ed_id))
            app.graph_eds.free(*ed);

    visualisation_eds.clear();
}

} // namesapce irt
