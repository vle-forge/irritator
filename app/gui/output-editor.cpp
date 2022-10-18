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
                                                     "Plot interpolate line",
                                                     "Plot interpolate dot" };

output_editor::output_editor() noexcept
  : implot_context{ ImPlot::CreateContext() }
{
}

output_editor::~output_editor() noexcept
{
    if (implot_context)
        ImPlot::DestroyContext(implot_context);
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
            ImGui::PushItemWidth(-1);
            ImGui::InputFilteredString("##name", out->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(sim_ed.sim_obs.get_id(*out)));
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            if (ImGui::InputReal("##ts", &out->time_step))
                out->time_step = std::clamp(
                  out->time_step, out->min_time_step, out->max_time_step);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->linear_outputs.size());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->linear_outputs.capacity());

            ImGui::TableNextColumn();
            ImGui::Combo("##plot",
                         &out->plot_type,
                         simulation_plot_type_string,
                         IM_ARRAYSIZE(simulation_plot_type_string));

            ImGui::TableNextColumn();
            if (ImGui::Button("copy")) {
                if (sim_ed.copy_obs.can_alloc(1)) {
                    auto& new_obs          = sim_ed.copy_obs.alloc();
                    new_obs.name           = out->name;
                    new_obs.linear_outputs = out->linear_outputs;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("write")) {
                sim_ed.selected_sim_obs       = id;
                sim_ed.output_ed.write_output = true;
                auto err                      = std::error_code{};
                auto file_path = std::filesystem::current_path(err);
                out->write(file_path);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(reinterpret_cast<const char*>(
              out->file.generic_string().c_str()));

            ImGui::PopID();
        }

        simulation_observation_copy *copy = nullptr, *prev = nullptr;
        while (sim_ed.copy_obs.next(copy)) {
            const auto id = sim_ed.copy_obs.get_id(*copy);
            ImGui::PushID(copy);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputFilteredString("##name", copy->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(id));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("-");

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("-");
            ;
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", copy->linear_outputs.size());

            ImGui::TableNextColumn();
            ImGui::Combo("##plot",
                         &copy->plot_type,
                         simulation_plot_type_string,
                         IM_ARRAYSIZE(simulation_plot_type_string));

            ImGui::TableNextColumn();
            if (ImGui::Button("del")) {
                sim_ed.copy_obs.free(*copy);
                copy = prev;
            }

            ImGui::PopID();
            prev = copy;
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
            if (obs->linear_outputs.size() > 0) {
                switch (obs->plot_type) {
                case simulation_plot_type_plotlines:
                    ImPlot::PlotLineG(obs->name.c_str(),
                                      ring_buffer_getter,
                                      &obs->linear_outputs,
                                      obs->linear_outputs.ssize());
                    break;

                case simulation_plot_type_plotscatters:
                    ImPlot::PlotScatterG(obs->name.c_str(),
                                         ring_buffer_getter,
                                         &obs->linear_outputs,
                                         obs->linear_outputs.ssize());
                    break;

                default:
                    break;
                }
            }
        }

        simulation_observation_copy* copy = nullptr;
        while (sim_ed.copy_obs.next(copy)) {
            if (copy->linear_outputs.size() > 0) {
                switch (copy->plot_type) {
                case simulation_plot_type_plotlines:
                    ImPlot::PlotLineG(copy->name.c_str(),
                                      ring_buffer_getter,
                                      &copy->linear_outputs,
                                      copy->linear_outputs.ssize());
                    break;

                case simulation_plot_type_plotscatters:
                    ImPlot::PlotScatterG(copy->name.c_str(),
                                         ring_buffer_getter,
                                         &copy->linear_outputs,
                                         copy->linear_outputs.ssize());
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
