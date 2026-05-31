// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include "imgui.h"

namespace irt {

simulation_component_editor_data::simulation_component_editor_data(
  const component_id            id,
  const simulation_component_id sid,
  const simulation_component&   sim) noexcept
  : m_id(id)
  , m_sim_id(sid)
  , m_sim(sim)
{}

bool simulation_component_editor_data::show_selected_nodes(
  component_editor& ed,
  const component_access& /*ids*/,
  component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);

    ImGui::TextUnformatted("empty node");

    return false;
}

bool simulation_component_editor_data::show(component_editor&       ed,
                                            const component_access& ids,
                                            component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& mod = app.mod;

    read(app, compo);

    debug::ensure(compo.type == component_type::simulation);

    if (auto pj_opt = m_task_project.try_take(); pj_opt.has_value()) {
        m_sim.pj = std::make_unique<project>(std::move(*pj_opt));
    }

    const auto new_file_id =
      mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept {
          auto new_file_id = m_sim.file_id;

          if (ImGui::BeginCombo("##project-file", "Select a project file")) {
              const auto none_selected =
                fs.file_paths.try_to_get(m_sim.file_id) == nullptr;

              if (ImGui::Selectable("-", none_selected)) {
                  new_file_id = undefined<file_path_id>();
              }

              for (const auto& f : fs.file_paths) {
                  if (f.type == file_type::project_file) {
                      const auto f_id       = fs.file_paths.get_id(f);
                      const auto f_selected = f_id == m_sim.file_id;

                      if (ImGui::Selectable(f.path.c_str(), f_selected)) {
                          new_file_id = fs.file_paths.get_id(f);
                      }
                  }
              }

              ImGui::EndCombo();
          }

          return new_file_id;
      });

    if (new_file_id != m_sim.file_id) {
        if (m_task_project.should_request()) {
            app.add_gui_task([&app, this, new_file_id]() {
                app.mod.files.read(
                  [&](const auto& fs, const auto /*vers*/) noexcept {
                      app.mod.ids.read(
                        [&](const auto& ids, const auto /*vers*/) noexcept {
                            auto exp_pj =
                              project::load(fs, ids, new_file_id, app.jn);

                            if (exp_pj.has_value()) {
                                m_task_project.fulfill(std::move(*exp_pj));
                            }
                        });
                  });
            });
        }

        if (m_sim.pj.get()) {
        }
    }

    if (not m_sim.pj.get()) {
        ImGui::TextDisabled("No project loaded");
    } else {
        auto& pj = *m_sim.pj;

        ImGui::TextDisabled("Only QSS integrator and constant models can "
                            "be used into experimental frames");

        if (ImGui::CollapsingHeader("Parameters")) {
            if (ImGui::BeginTable("Parameters", 4)) {
                auto& tns    = pj.parameters.get<tree_node_id>();
                auto& models = pj.parameters.get<model_id>();
                auto& names  = pj.parameters.get<name_str>();
                auto& p      = pj.parameters.get<parameter>();
                auto& vec_p  = pj.parameters.get<vector<real>>();

                ImGui::TableSetupColumn("name",
                                        ImGuiTableColumnFlags_WidthStretch);
                // ImGuiTableColumnFlags_WidthFixed, compute size max name_str);
                ImGui::TableSetupColumn("dynamics type",
                                        ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("default value",
                                        ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("multiple values",
                                        ImGuiTableColumnFlags_WidthStretch);

                for (const auto id : pj.parameters) {
                    const auto  idx = get_index(id);
                    const auto& mdl = pj.sim.models.get(models[idx]);
                    const auto  disable_parameter =
                      any_equal(mdl.type,
                                dynamics_type::constant,
                                dynamics_type::qss1_integrator,
                                dynamics_type::qss2_integrator,
                                dynamics_type::qss3_integrator);

                    ImGui::PushID(idx);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(-1.f);
                    ImGui::TextUnformatted(names[idx].c_str());
                    ImGui::PopItemWidth();
                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(-1.f);
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl.type)]);
                    ImGui::PopItemWidth();
                    ImGui::TableNextColumn();

                    ImGui::BeginDisabled(not disable_parameter);
                    ImGui::InputReal("##single", &p[idx].reals[0]);
                    ImGui::EndDisabled();
                    ImGui::TableNextColumn();

                    ImGui::BeginDisabled(not disable_parameter);
                    if (ImGui::SmallButton("clear"))
                        vec_p[idx].clear();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("edit"))
                        ImGui::OpenPopup("Edit values");

                    if (ImGui::BeginPopup("Edit values")) {
                        auto size = static_cast<int>(vec_p[idx].size());

                        if (ImGui::InputInt("length", &size) &&
                            size != static_cast<int>(vec_p[idx].size()) &&
                            size < external_source_chunk_size) {
                            vec_p[idx].resize(size, 0.0);
                        }

                        for (auto i = 0; i < size; ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::InputDouble("##name", &vec_p[idx][i]);
                            ImGui::PopID();
                        }

                        ImGui::EndPopup();
                    }

                    const auto len = vec_p[idx].size();
                    switch (len) {
                    case 0:
                        ImGui::TextUnformatted("empty");
                        break;

                    case 1:
                        ImGui::Text("%f", vec_p[idx][0]);
                        break;

                    case 2:
                        ImGui::Text("%f %f", vec_p[idx][0], vec_p[idx][1]);
                        break;

                    default:
                        ImGui::Text("%f %f %f ...",
                                    vec_p[idx][0],
                                    vec_p[idx][1],
                                    vec_p[idx][2]);
                        break;
                    }
                    ImGui::EndDisabled();

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if (ImGui::CollapsingHeader("Observations")) {
                if (ImGui::BeginTable("Observation", 4)) {
                    ImGui::TableSetupColumn("name",
                                            ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("dynamics type",
                                            ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("default value",
                                            ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("multiple values",
                                            ImGuiTableColumnFlags_WidthStretch);

                    // TODO: For now, we only support one variable observers in
                    // the experimental frame.

                    auto mdl_id = [&]() {
                        for (const auto id : pj.observers)
                            return pj.observers.get<model_id>(id);

                        return undefined<model_id>();
                    }();

                    for (const auto& vobs : pj.variable_observers) {
                        auto& models = vobs.subs.get<model_id>();
                        auto& names  = vobs.subs.get<name_str>();

                        for (const auto sub_vobs : vobs.subs) {
                            const auto& name = names[sub_vobs];

                            if (ImGui::RadioButton(name.c_str(),
                                                   models[sub_vobs] == mdl_id))
                                mdl_id = models[sub_vobs];
                        }
                    }

                    ImGui::EndTable();
                }
            }
        }
    }

    return false;
}

void simulation_component_editor_data::read(application& app,
                                            component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        debug::ensure(compo.type == component_type::simulation);

        if (auto* s = ids.sim_components.try_to_get(compo.id.sim_id)) {
            m_sim = *s;
        }
    });
}

void simulation_component_editor_data::write(application& app,
                                             component&   compo) noexcept
{
    auto c_ptr = std::make_unique<component>(std::move(compo));
    auto g_ptr = std::make_unique<simulation_component>(std::move(m_sim));

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (debug::check(c->type == component_type::simulation)) {
                  const auto sim_id = c->id.sim_id;

                  if (auto* sim = ids.sim_components.try_to_get(sim_id))
                      *sim = *g;
              }
          });
      });
}

} // namespace irt
