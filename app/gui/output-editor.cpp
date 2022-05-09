// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

const char* simulation_plot_type_string[] = { "None",
                                              "Plot line",
                                              "Plot scatter" };

static void show_output_widget(application& app) noexcept
{
    if (ImGui::CollapsingHeader("Raw observations list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Observations", 7)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("time-step");
            ImGui::TableSetupColumn("size");
            ImGui::TableSetupColumn("capacity");
            ImGui::TableSetupColumn("plot");
            ImGui::TableSetupColumn("action");

            ImGui::TableHeadersRow();
            simulation_observation* out = nullptr;
            while (app.s_editor.sim_obs.next(out)) {
                const auto id = app.s_editor.sim_obs.get_id(*out);
                ImGui::PushID(out);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat("{}",
                                  ordinal(app.s_editor.sim_obs.get_id(*out)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(out->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", out->time_step);
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", out->raw_ring_buffer.size());
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", out->raw_outputs.capacity());
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("raw")) {
                    app.s_editor.selected_sim_obs = id;
                    app.save_raw_file             = true;
                    auto err                      = std::error_code{};
                    auto file_path = std::filesystem::current_path(err);
                    out->save_raw(file_path);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("int")) {
                    app.s_editor.selected_sim_obs = id;
                    app.save_int_file             = true;
                    auto err                      = std::error_code{};
                    auto file_path = std::filesystem::current_path(err);
                    out->save_interpolate(file_path);
                }

                ImGui::TableNextColumn();
                if (ImGui::Combo("##plot",
                                 &out->plot_type,
                                 simulation_plot_type_string,
                                 IM_ARRAYSIZE(simulation_plot_type_string))) {

                    auto until = out->raw_ring_buffer.size() > 0
                                   ? out->raw_ring_buffer.back().t
                                   : app.s_editor.simulation_current;

                    switch (out->plot_type) {
                    case simulation_plot_type_plotlines:
                        out->compute_interpolate(until, out->plot_outputs);
                        break;

                    case simulation_plot_type_plotscatters:
                        out->compute_interpolate(until, out->plot_outputs);
                        break;

                    default:
                        out->plot_outputs.clear();
                        break;
                    }
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Plots outputs",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImPlot::BeginPlot("Plot", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            simulation_observation* obs = nullptr;
            while (app.s_editor.sim_obs.next(obs)) {
                if (obs->plot_outputs.size() > 0) {
                    switch (obs->plot_type) {
                    case simulation_plot_type_plotlines:
                        ImPlot::PlotLine(obs->name.c_str(),
                                         &obs->plot_outputs[0].x,
                                         &obs->plot_outputs[0].y,
                                         obs->plot_outputs.size(),
                                         0,
                                         sizeof(ImVec2));
                        break;

                    case simulation_plot_type_plotscatters:
                        ImPlot::PlotScatter(obs->name.c_str(),
                                            &obs->plot_outputs[0].x,
                                            &obs->plot_outputs[0].y,
                                            obs->plot_outputs.size(),
                                            0,
                                            sizeof(ImVec2));
                        break;

                    default:
                        break;
                    }
                }
            }

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }
}

void application::show_output_editor_widget() noexcept
{
    show_output_widget(*this);
}

} // namespace irt
