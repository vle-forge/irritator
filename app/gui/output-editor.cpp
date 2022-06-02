// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <utility>

namespace irt {

static const char* simulation_plot_type_string[] = { "None",
                                                     "Plot raw dot",
                                                     "Plot interpolate line",
                                                     "Plot interpolate dot" };

output_editor::output_editor() noexcept
  : implot_context{ ImPlot::CreateContext() }
{}

output_editor::~output_editor() noexcept
{
    if (implot_context)
        ImPlot::DestroyContext(implot_context);
}

static void compute_plot_outputs(simulation_editor&      sim_ed,
                                 simulation_observation& obs) noexcept
{
    auto until = obs.raw_ring_buffer.size() > 0 ? obs.raw_ring_buffer.back().t
                                                : sim_ed.simulation_current;

    switch (obs.plot_type) {
    case simulation_plot_type_raw:
        obs.plot_outputs.clear();
        for (auto it = obs.raw_ring_buffer.head();
             it != obs.raw_ring_buffer.end();
             ++it)
            obs.plot_outputs.push_back(ImVec2(static_cast<float>(it->t),
                                              static_cast<float>(it->msg[0])));
        break;

    case simulation_plot_type_plotlines:
        obs.plot_outputs.clear();
        obs.compute_interpolate(until, obs.plot_outputs);
        break;

    case simulation_plot_type_plotscatters:
        obs.plot_outputs.clear();
        obs.compute_interpolate(until, obs.plot_outputs);
        break;

    default:
        obs.plot_outputs.clear();
        break;
    }
}

static void show_observation_table(simulation_editor& sim_ed) noexcept
{
    static const ImGuiTableFlags flags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    if (ImGui::BeginTable("Observations", 7, flags)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn(
          "time-step", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn(
          "capacity", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn(
          "plot", ImGuiTableColumnFlags_WidthFixed, 180.f);
        ImGui::TableSetupColumn("export as",
                                ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableHeadersRow();
        simulation_observation* out = nullptr;
        while (sim_ed.sim_obs.next(out)) {
            const auto id = sim_ed.sim_obs.get_id(*out);
            ImGui::PushID(out);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(out->name.c_str());

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(sim_ed.sim_obs.get_id(*out)));
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            if (ImGui::InputReal("##ts", &out->time_step))
                out->time_step = std::clamp(
                  out->time_step, out->min_time_step, out->max_time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->raw_ring_buffer.size());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->raw_outputs.capacity());

            ImGui::TableNextColumn();
            if (ImGui::Combo("##plot",
                             &out->plot_type,
                             simulation_plot_type_string,
                             IM_ARRAYSIZE(simulation_plot_type_string))) {
                compute_plot_outputs(sim_ed, *out);
            }

            ImGui::SameLine();
            if (ImGui::Button("refresh")) {
                compute_plot_outputs(sim_ed, *out);
            }

            ImGui::TableNextColumn();
            if (ImGui::Button("raw")) {
                sim_ed.selected_sim_obs        = id;
                sim_ed.output_ed.save_raw_file = true;
                auto err                       = std::error_code{};
                auto file_path = std::filesystem::current_path(err);
                out->save_raw(file_path);
            }
            ImGui::SameLine();
            if (ImGui::Button("int.")) {
                sim_ed.selected_sim_obs        = id;
                sim_ed.output_ed.save_int_file = true;
                auto err                       = std::error_code{};
                auto file_path = std::filesystem::current_path(err);
                out->save_interpolate(file_path);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(reinterpret_cast<const char*>(
              out->file.generic_string().c_str()));

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

static ImVec4 compute_plot_limits(simulation_editor& sim_ed) noexcept
{
    simulation_observation* obs = nullptr;
    ImVec4                  limits;
    bool                    engaged = false;

    while (sim_ed.sim_obs.next(obs)) {
        if (obs->plot_type != simulation_plot_type_none) {
            if (engaged) {
                limits.x = std::min(limits.x, obs->limits.x);
                limits.y = std::min(limits.y, obs->limits.y);
                limits.z = std::max(limits.z, obs->limits.z);
                limits.w = std::max(limits.w, obs->limits.w);
            } else {
                limits  = obs->limits;
                engaged = true;
            }
        }
    }

    return limits;
}

static void show_observation_plot(simulation_editor& sim_ed) noexcept
{
    ImPlot::SetCurrentContext(sim_ed.output_ed.implot_context);
    if (ImPlot::BeginPlot("Plot", ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        const auto limits = compute_plot_limits(sim_ed);
        ImPlot::SetupAxesLimits(
          limits.x, limits.z, limits.y, limits.w, ImPlotCond_None);

        simulation_observation* obs = nullptr;
        while (sim_ed.sim_obs.next(obs)) {
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

void output_editor::show() noexcept
{
    auto* s_editor = container_of(this, &simulation_editor::output_ed);

    static const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Observations list", flags))
        show_observation_table(*s_editor);

    if (ImGui::CollapsingHeader("Plots outputs", flags))
        show_observation_plot(*s_editor);
}

void application::show_output_editor_widget() noexcept
{
    s_editor.output_ed.show();
}

} // namespace irt
