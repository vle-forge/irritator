// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static inline void memory_output_emplace(memory_output& out,
                                         float          x,
                                         float          y) noexcept
{
    if (out.xs.size() < out.xs.capacity()) {
        out.xs.emplace_back(x);
        out.ys.emplace_back(y);
    }
}

void memory_output_update(const irt::observer&        obs,
                          const irt::dynamics_type    type,
                          const irt::time             tl,
                          const irt::time             t,
                          const irt::observer::status s) noexcept
{
    auto* c_ed   = reinterpret_cast<component_editor*>(obs.user_data);
    auto  id     = enum_cast<memory_output_id>(obs.user_id);
    auto& output = c_ed->outputs.get(id);

    if (s == observer::status::initialize) {
        output.xs.clear();
        output.ys.clear();
        output.tl = zero;
        return;
    }

    // Store only one value for a time
    while (!output.xs.empty() && output.xs.back() == tl) {
        output.xs.pop_back();
        output.ys.pop_back();
    }

    if (output.interpolate) {
        switch (type) {
        // todo perhaps select algorithm for all QSS[1-3] model.
        // Here qss1_integrator, qss1_sum etc.
        case irt::dynamics_type::qss1_integrator: {
            for (auto td = tl; td < t; td += output.time_step) {
                const auto e     = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e;
                memory_output_emplace(output, td, value);
            }
            const auto e     = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e;
            memory_output_emplace(output, t, value);
        } break;

        case irt::dynamics_type::qss2_integrator: {
            for (auto td = tl; td < t; td += output.time_step) {
                const auto e = td - tl;
                const auto value =
                  obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
                memory_output_emplace(output, td, value);
            }
            const auto e = t - tl;
            const auto value =
              obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
            memory_output_emplace(output, t, value);
        } break;

        case irt::dynamics_type::qss3_integrator: {
            for (auto td = tl; td < t; td += output.time_step) {
                const auto e     = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e +
                                   (obs.msg[2] * e * e / two) +
                                   (obs.msg[3] * e * e * e / three);
                memory_output_emplace(output, td, value);
            }
            const auto e     = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e +
                               (obs.msg[2] * e * e / two) +
                               (obs.msg[3] * e * e * e / three);
            memory_output_emplace(output, t, value);
        } break;

        default:
            memory_output_emplace(output, t, obs.msg[0]);
            break;
        }
    } else {
        memory_output_emplace(output, t, obs.msg[0]);
    }
}

static void show_simulation(component_editor& ed) noexcept
{
    ImGui::InputReal("Begin", &ed.simulation_begin);
    ImGui::InputReal("End", &ed.simulation_end);
    ImGui::TextFormat("Current time {:.6f}", ed.simulation_current);

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
            auto* app = container_of(this, &application::c_editor);
            show_external_sources(*app, srcs);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Log")) {
            auto *app = container_of(this, &application::c_editor);
            app->log_w.show();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace irt