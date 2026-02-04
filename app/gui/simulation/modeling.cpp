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
  simulation_component& /*sim*/) noexcept
  : m_id(id)
  , m_sim_id(sid)
{}

void simulation_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    ImGui::TextUnformatted("empty node");
}

void simulation_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& mod = app.mod;

    if (ImGui::CollapsingHeader("simulation-components")) {
        for (const auto& c : mod.sim_components) {
            const auto id = mod.sim_components.get_id(c);
            ImGui::TextFormat("simulation-component: {}", ordinal(id));
        }
    }

    if (ImGui::CollapsingHeader("projects")) {
        mod.files.read([](const auto& fs, const auto /*vers*/) {
            for (const auto& f : fs.file_paths) {
                if (f.type == file_path::file_type::project_file) {
                    ImGui::TextFormat("project-file: {}", f.path.sv());
                }
            }
        });
    }
}

} // namespace irt
