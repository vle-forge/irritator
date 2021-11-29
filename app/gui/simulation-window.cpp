// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

void component_editor::show_simulation_window() noexcept
{
    if (ImGui::CollapsingHeader("Simulation##Component",
                                ImGuiTreeNodeFlags_CollapsingHeader)) {
        ImGui::InputReal("Begin", &simulation_begin);
        ImGui::InputReal("End", &simulation_end);
        ImGui::TextFormat("Current time {:.6f}", simulation_begin);

        bool start_line = false;
        if (match(simulation_state,
                  component_simulation_status::not_started,
                  component_simulation_status::finished)) {
            start_line = true;
            if (ImGui::Button("init")) {
                simulation_init();
            }
        }

        if (simulation_state == component_simulation_status::initialized) {
            if (start_line)
                ImGui::SameLine();
            else
                start_line = true;

            if (ImGui::Button("start")) {
                simulation_start();
            }
        }

        if (simulation_state == component_simulation_status::pause_forced) {
            if (start_line)
                ImGui::SameLine();
            else
                start_line = true;

            if (ImGui::Button("continue")) {
                simulation_start();
            }
        }

        if (simulation_state == component_simulation_status::running) {
            if (start_line)
                ImGui::SameLine();

            if (ImGui::Button("pause")) {
                force_pause = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("stop")) {
                force_stop = true;
            }
        }
    }
}

} // namespace irt