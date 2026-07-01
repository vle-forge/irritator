// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

static auto search_tn_model = []<typename T>(const T&           data,
                                             const tree_node_id tn_id,
                                             const model_id mdl_id) noexcept ->
  typename T::identifier_type {
      const auto& tns  = data.template get<tree_node_id>();
      const auto& mdls = data.template get<model_id>();

      for (const auto id : data)
          if (tns[id] == tn_id and mdls[id] == mdl_id)
              return id;

      return undefined<typename T::identifier_type>();
  };

static auto extract = []<typename T>(T& data, const project& pj) noexcept {
    for (const auto id : data) {
        const auto& path       = data.template get<unique_id_path>(id);
        const auto  tn_mdl_opt = pj.get_model_path(path);

        if (tn_mdl_opt.has_value()) {
            data.template get<tree_node_id>(id) = tn_mdl_opt->first;
            data.template get<model_id>(id)     = tn_mdl_opt->second;
        } else {
            data.template get<tree_node_id>(id) = undefined<tree_node_id>();
            data.template get<model_id>(id)     = undefined<model_id>();
        }
    }
};

static auto extract_all = [](simulation_component& sim) {
    extract(sim.factors, sim.pj);
    extract(sim.selections, sim.pj);
};

simulation_component_editor_data::simulation_component_editor_data(
  const component_id id,
  const simulation_component_id /*sid*/,
  const simulation_component& sim) noexcept
  : m_id(id)
  , m_sim(sim)
{
    extract_all(m_sim);
}

bool simulation_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/,
  const component_access& /*ids*/,
  component& /*compo*/) noexcept
{
    return false;
}

static bool display_factor_single(single_factor& factor,
                                  const bool     can_edit) noexcept
{
    auto u = 0;

    ImGui::BeginDisabled(not can_edit);

    const auto copy = factor.value;
    if (ImGui::InputReal("##single", &factor.value)) {
        debug::ensure(copy != factor.value);
        ++u;
    }

    ImGui::EndDisabled();

    return u > 0;
}

static bool display_factor_fixed(fixed_factor& factor,
                                 const bool    can_edit) noexcept
{
    auto u = 0;

    ImGui::BeginDisabled(not can_edit);

    if (ImGui::Button("clear")) {
        factor.values.clear();
        ++u;
    }

    ImGui::SameLine();
    if (ImGui::Button("edit"))
        ImGui::OpenPopup("Edit values");

    if (ImGui::BeginPopup("Edit values")) {
        auto size = static_cast<int>(factor.values.size());

        if (ImGui::InputInt("length", &size) &&
            size != static_cast<int>(factor.values.size()) &&
            size < external_source_chunk_size) {
            factor.values.resize(size, 0.0);
            ++u;
        }

        for (auto i = 0; i < size; ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InputDouble("##name", &factor.values[i]))
                ++u;
            ImGui::PopID();
        }

        ImGui::EndPopup();
    }

    switch (factor.values.size()) {
    case 0:
        ImGui::TextUnformatted("empty");
        break;

    case 1:
        ImGui::TextFormat("{}", factor.values.front());
        break;

    case 2:
        ImGui::TextFormat("{} {}", factor.values[0], factor.values[1]);
        break;

    default:
        ImGui::TextFormat(
          "{} {} {}...", factor.values[0], factor.values[1], factor.values[2]);
        break;
    }

    ImGui::EndDisabled();

    return u > 0;
}

static bool display_factor_random(random_factor& factor,
                                  const bool     can_edit) noexcept
{
    auto u = 0;

    ImGui::BeginDisabled(not can_edit);

    if (show_random_distribution_input(factor))
        ++u;

    ImGui::EndDisabled();

    return u > 0;
}

bool simulation_component_editor_data::display_objective(project& pj) noexcept
{
    auto u = 0;

    const auto preview_method =
      name_str(optimization_method_names[ordinal(m_sim.objective.method)]);

    if (ImGui::BeginCombo("method", preview_method.c_str())) {
        for (auto i = 0, e = length(optimization_method_names); i != e; ++i) {
            const auto label    = name_str(optimization_method_names[i]);
            const auto selected = i == ordinal(m_sim.objective.method);

            if (ImGui::Selectable(label.c_str(), selected)) {
                if (not selected) {
                    ++u;
                    m_sim.objective.method = enum_cast<optimization_method>(i);
                }
            }
        }
        ImGui::EndCombo();
    }

    const auto preview_type =
      name_str(optimization_type_names[ordinal(m_sim.objective.type)]);

    if (ImGui::BeginCombo("type", preview_type.c_str())) {
        for (auto i = 0, e = length(optimization_type_names); i != e; ++i) {
            const auto label    = name_str(optimization_type_names[i]);
            const auto selected = i == ordinal(m_sim.objective.type);
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (not selected) {
                    ++u;
                    m_sim.objective.type = enum_cast<optimization_type>(i);
                }
            }
        }
        ImGui::EndCombo();
    }

    return u > 0;
}

bool simulation_component_editor_data::display_parameter_table(
  project& pj) noexcept
{
    auto u = 0;

    if (ImGui::BeginTable("Parameters", 4, ImGuiTableFlags_RowBg)) {
        auto& models         = m_sim.factors.get<model_id>();
        auto& names          = m_sim.factors.get<name_str>();
        auto& types          = m_sim.factors.get<factor_type>();
        auto& single_factors = m_sim.factors.get<single_factor>();
        auto& fixed_factors  = m_sim.factors.get<fixed_factor>();
        auto& random_factors = m_sim.factors.get<random_factor>();

        ImGui::TableSetupColumn(
          "name", ImGuiTableColumnFlags_WidthStretch, .2f);
        ImGui::TableSetupColumn(
          "dynamics", ImGuiTableColumnFlags_WidthStretch, .2f);
        ImGui::TableSetupColumn(
          "type", ImGuiTableColumnFlags_WidthStretch, .2f);
        ImGui::TableSetupColumn(
          "definition", ImGuiTableColumnFlags_WidthStretch, .4f);

        ImGui::TableHeadersRow();

        for (const auto id : m_sim.factors) {
            const auto  idx           = get_index(id);
            const auto* mdl           = pj.sim.models.try_to_get(models[idx]);
            const auto  is_accessible = mdl != nullptr;
            const auto  can_edit =
              is_accessible and any_equal(mdl->type,
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
            if (is_accessible)
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
            else
                ImGui::TextUnformatted("model is inaccessible from ");

            ImGui::PopItemWidth();
            ImGui::TableNextColumn();

            const auto factor_type_idx = static_cast<int>(types[idx]);
            const auto preview = name_str(factor_type_names[factor_type_idx]);

            ImGui::PushItemWidth(-1.f);
            if (ImGui::BeginCombo("##factory-type", preview.c_str())) {
                for (auto i = 0, e = length(factor_type_names); i != e; ++i) {
                    const auto label    = name_str(factor_type_names[i]);
                    const auto selected = i == factor_type_idx;

                    if (ImGui::Selectable(label.c_str(), selected)) {
                        if (factor_type_idx != i) {
                            types[idx] = enum_cast<factor_type>(i);
                            ++u;
                        }
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            switch (types[idx]) {
            case factor_type::single:
            case factor_type::single_add:
            case factor_type::single_mult:
                u += display_factor_single(single_factors[idx], can_edit);
                break;

            case factor_type::fixed:
            case factor_type::fixed_add:
            case factor_type::fixed_mult:
                u += display_factor_fixed(fixed_factors[idx], can_edit);
                break;

            case factor_type::random:
            case factor_type::random_add:
            case factor_type::random_mult:
                u += display_factor_random(random_factors[idx], can_edit);
                break;

            default:
                break;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    return u > 0;
}

bool simulation_component_editor_data::display_observation_table(
  project& pj) noexcept
{
    auto u = 0;

    if (m_sim.objective.method == optimization_method::weighted_sum) {
        if (ImGui::BeginTable("Observation", 4, ImGuiTableFlags_RowBg)) {
            auto& names     = m_sim.selections.get<name_str>();
            auto& criterias = m_sim.selections.get<criteria_type>();

            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("criteria",
                                    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("weights",
                                    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto id : m_sim.selections) {
                const auto idx = get_index(id);

                ImGui::PushID(idx);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(names[idx].c_str());
                ImGui::TableNextColumn();

                const auto criteria_idx = static_cast<int>(criterias[idx]);
                const auto preview =
                  name_str(criteria_type_names[criteria_idx]);

                if (ImGui::BeginCombo("##crit", preview.c_str())) {
                    for (auto i = 0, e = length(criteria_type_names); i != e;
                         ++i) {
                        const auto label    = name_str(criteria_type_names[i]);
                        const auto selected = i == criteria_idx;

                        if (ImGui::Selectable(label.c_str(), selected)) {
                            if (criteria_idx != i) {
                                criterias[idx] = enum_cast<criteria_type>(i);
                                ++u;
                            }
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);
                auto copy = m_sim.objective.weighted_sum_params.weights[idx];
                if (ImGui::InputDouble("##avlue", &copy)) {
                    if (std::isfinite(copy)) {
                        m_sim.objective.weighted_sum_params.weights[idx] = copy;
                        ++u;
                    }
                }
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1);

                const auto preview_type =
                  name_str(optimization_type_names[ordinal(
                    m_sim.objective.weighted_sum_params.types[idx])]);

                if (ImGui::BeginCombo("type", preview_type.c_str())) {
                    for (auto i = 0, e = length(optimization_type_names);
                         i != e;
                         ++i) {
                        const auto label = name_str(optimization_type_names[i]);
                        const auto selected =
                          i == ordinal(m_sim.objective.type);
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            if (not selected) {
                                ++u;
                                m_sim.objective.weighted_sum_params.types[idx] =
                                  enum_cast<optimization_type>(i);
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    } else {
        if (ImGui::BeginTable("Observation", 2, ImGuiTableFlags_RowBg)) {
            auto& names     = m_sim.selections.get<name_str>();
            auto& criterias = m_sim.selections.get<criteria_type>();

            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("criteria",
                                    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto id : m_sim.selections) {
                const auto idx = get_index(id);

                ImGui::PushID(idx);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(names[idx].c_str());
                ImGui::TableNextColumn();

                const auto criteria_idx = static_cast<int>(criterias[idx]);
                const auto preview =
                  name_str(criteria_type_names[criteria_idx]);

                if (ImGui::BeginCombo("##crit", preview.c_str())) {
                    for (auto i = 0, e = length(criteria_type_names); i != e;
                         ++i) {
                        const auto label    = name_str(criteria_type_names[i]);
                        const auto selected = i == criteria_idx;

                        if (ImGui::Selectable(label.c_str(), selected)) {
                            if (criteria_idx != i) {
                                criterias[idx] = enum_cast<criteria_type>(i);
                                ++u;
                            }
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    return u > 0;
}

bool simulation_component_editor_data::show(component_editor& ed,
                                            const component_access& /*ids*/,
                                            component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& mod = app.mod;
    auto  u   = 0;

    read(app, compo);

    debug::ensure(compo.type == component_type::simulation);

    if (auto pj_opt = m_task_project.try_take(); pj_opt.has_value()) {
        m_sim.assign(std::move(*pj_opt));
        m_sim.file_id = m_sim.pj.file;
        ++u;
    }

    const auto old_file_id = m_sim.file_id;
    m_sim.file_id =
      mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept {
          const auto* file        = fs.file_paths.try_to_get(m_sim.file_id);
          const auto* preview     = file ? file->path.c_str() : "-";
          auto        new_file_id = m_sim.file_id;

          if (ImGui::BeginCombo("Select project file", preview)) {
              if (ImGui::Selectable("-", file == nullptr)) {
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

    if (old_file_id != m_sim.file_id) {
        if (m_task_project.should_request()) {
            app.add_gui_task([&app, this, f = m_sim.file_id]() {
                app.mod.files.read([&](const auto& fs,
                                       const auto /*vers*/) noexcept {
                    app.mod.ids.read([&](const auto& ids,
                                         const auto /*vers*/) noexcept {
                        auto exp_pj = project::load(fs, ids, f, app.jn);

                        m_task_project.fulfill(
                          exp_pj.has_value() ? std::move(*exp_pj) : project{});
                    });
                });
            });
        }
    }

    ImGui::TextDisabled("Only QSS integrator and constant models can "
                        "be used into experimental frames for factors.");

    if (ImGui::TreeNodeEx(compo.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNode("objective")) {
            u += display_objective(m_sim.pj);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("parameters")) {
            u += display_parameter_table(m_sim.pj);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("observations")) {
            u += display_observation_table(m_sim.pj);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    if (u > 0)
        write(app, compo);

    return u > 0;
}

void simulation_component_editor_data::read(application& app,
                                            component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version == version)
            return;

        m_version = version;

        if (not ids.exists(m_id))
            return;

        debug::ensure(compo.type == component_type::simulation);

        if (auto* s = ids.sim_components.try_to_get(compo.id.sim_id)) {
            m_sim = *s;
            extract_all(m_sim);
        }
    });
}

void simulation_component_editor_data::write(application& app,
                                             component&   compo) noexcept
{
    auto c_ptr = std::make_unique<component>(compo);
    auto g_ptr = std::make_unique<simulation_component>(m_sim);

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = std::move(*c);

              if (debug::check(c->type == component_type::simulation)) {
                  const auto sim_id = c->id.sim_id;

                  if (auto* sim = ids.sim_components.try_to_get(sim_id)) {
                      *sim = std::move(*g);
                  }
              }
          });
      });
}

} // namespace irt
