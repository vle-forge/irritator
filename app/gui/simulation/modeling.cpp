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

bool simulation_component_editor_data::show_selected_nodes(component_editor& /*ed*/,
                                                           const component_access& /*ids*/,
                                                           component& /*compo*/) noexcept
{
    ImGui::TextUnformatted("empty node");

    return false;
}

bool simulation_component_editor_data::show(component_editor& ed,
                                            const component_access& ids,
                                            component& /*compo*/) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto& mod = app.mod;

    if (ImGui::CollapsingHeader("simulation-components")) {
        for (const auto& c : ids.sim_components) {
            const auto id = ids.sim_components.get_id(c);
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

    return false;
}

void simulation_component_editor_data::read(application& app) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        const auto& compo = ids.components[m_id];
        debug::ensure(compo.type == component_type::graph);

        if (auto* s = ids.sim_components.try_to_get(compo.id.sim_id)) {
            m_sim = *s;
        }
    });
}

void simulation_component_editor_data::write(application& app) noexcept
{
    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) {
            if (not ids.exists(m_id))
                return;

            auto& compo = ids.components[m_id];
            debug::ensure(compo.type == component_type::simulation);

            if (auto* s = ids.sim_components.try_to_get(compo.id.sim_id)) {
                *s = m_sim;
            }
        });
    });
}

} // namespace irt
