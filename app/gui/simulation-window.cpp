// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static void show_simulation(component_editor& ed) noexcept
{
    ImGui::InputReal("Begin", &ed.simulation_begin);
    ImGui::InputReal("End", &ed.simulation_end);
    ImGui::TextFormat("Current time {:.6f}", ed.simulation_begin);

    bool start_line = false;
    if (match(ed.simulation_state,
              component_simulation_status::not_started,
              component_simulation_status::finished)) {
        start_line = true;
        if (ImGui::Button("init")) {
            ed.simulation_init();
        }
    }

    if (ed.simulation_state == component_simulation_status::initialized) {
        if (start_line)
            ImGui::SameLine();
        else
            start_line = true;

        if (ImGui::Button("start")) {
            ed.simulation_start();
        }
    }

    if (ed.simulation_state == component_simulation_status::pause_forced) {
        if (start_line)
            ImGui::SameLine();
        else
            start_line = true;

        if (ImGui::Button("continue")) {
            ed.simulation_start();
        }
    }

    if (ed.simulation_state == component_simulation_status::running) {
        if (start_line)
            ImGui::SameLine();

        if (ImGui::Button("pause")) {
            ed.force_pause = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("stop")) {
            ed.force_stop = true;
        }
    }
}

void component_editor::show_simulation_window() noexcept
{
    if (ImGui::BeginTabBar("##Simulation")) {
        if (ImGui::BeginTabItem("Simulation")) {
            show_simulation(*this);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data")) {
            show_external_sources(srcs);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace irt