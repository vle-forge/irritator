// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

const char* simulation_plot_type_string[] = { "None",
                                              "Plot raw dot",
                                              "Plot interpolate line",
                                              "Plot interpolate dot" };

static void show_output_widget(application& app) noexcept
{
    if (ImGui::CollapsingHeader("Raw observations list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {

        static ImGuiTableFlags flags =
          ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
          ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
          ImGuiTableFlags_Reorderable;

        if (ImGui::BeginTable("Observations", 8, flags)) {
            ImGui::TableSetupColumn(
              "name", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn(
              "id", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn(
              "time-step", ImGuiTableColumnFlags_WidthFixed, 100.f);
            ImGui::TableSetupColumn(
              "size", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn(
              "capacity", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn(
              "export as", ImGuiTableColumnFlags_WidthFixed, 100.f);
            ImGui::TableSetupColumn("path");
            ImGui::TableSetupColumn("plot");

            ImGui::TableHeadersRow();
            simulation_observation* out = nullptr;
            while (app.s_editor.sim_obs.next(out)) {
                const auto id = app.s_editor.sim_obs.get_id(*out);
                ImGui::PushID(out);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(out->name.c_str());

                ImGui::TableNextColumn();
                ImGui::TextFormat("{}",
                                  ordinal(app.s_editor.sim_obs.get_id(*out)));
                ImGui::TableNextColumn();
                if (ImGui::InputReal("##ts", &out->time_step))
                    out->time_step = std::clamp(
                      out->time_step, out->min_time_step, out->max_time_step);

                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", out->raw_ring_buffer.size());
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", out->raw_outputs.capacity());
                ImGui::TableNextColumn();
                if (ImGui::Button("raw")) {
                    app.s_editor.selected_sim_obs = id;
                    app.save_raw_file             = true;
                    auto err                      = std::error_code{};
                    auto file_path = std::filesystem::current_path(err);
                    out->save_raw(file_path);
                }
                ImGui::SameLine();
                if (ImGui::Button("int.")) {
                    app.s_editor.selected_sim_obs = id;
                    app.save_int_file             = true;
                    auto err                      = std::error_code{};
                    auto file_path = std::filesystem::current_path(err);
                    out->save_interpolate(file_path);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(reinterpret_cast<const char*>(
                  out->file.generic_string().c_str()));

                ImGui::TableNextColumn();
                if (ImGui::Combo("##plot",
                                 &out->plot_type,
                                 simulation_plot_type_string,
                                 IM_ARRAYSIZE(simulation_plot_type_string))) {
                    auto until = out->raw_ring_buffer.size() > 0
                                   ? out->raw_ring_buffer.back().t
                                   : app.s_editor.simulation_current;

                    switch (out->plot_type) {
                    case simulation_plot_type_raw:
                        out->plot_outputs.clear();
                        for (auto it = out->raw_ring_buffer.head();
                             it != out->raw_ring_buffer.end();
                             ++it)
                            out->plot_outputs.push_back(
                              ImVec2(static_cast<float>(it->t),
                                     static_cast<float>(it->msg[0])));
                        break;

                    case simulation_plot_type_plotlines:
                        out->plot_outputs.clear();
                        out->compute_interpolate(until, out->plot_outputs);
                        break;

                    case simulation_plot_type_plotscatters:
                        out->plot_outputs.clear();
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
        if (ImPlot::BeginPlot("Plot", ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            simulation_observation* obs = nullptr;
            while (app.s_editor.sim_obs.next(obs)) {
                if (obs->plot_outputs.size() > 0) {
                    switch (obs->plot_type) {
                    case simulation_plot_type_raw:
                        ImPlot::PlotScatter(obs->name.c_str(),
                                            &obs->plot_outputs[0].x,
                                            &obs->plot_outputs[0].y,
                                            obs->plot_outputs.size(),
                                            0,
                                            sizeof(ImVec2));
                        break;

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
