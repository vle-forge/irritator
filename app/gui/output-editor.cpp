// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"

#include <utility>

namespace irt {

static const char* simulation_plot_type_string[] = { "None",
                                                     "Plot line",
                                                     "Plot dot" };

output_editor::output_editor() noexcept
  : implot_context{ ImPlot::CreateContext() }
{
}

output_editor::~output_editor() noexcept
{
    if (implot_context)
        ImPlot::DestroyContext(implot_context);
}

static void show_observation_table(application& app) noexcept
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
        while (app.simulation_ed.sim_obs.next(out)) {
            const auto id = app.simulation_ed.sim_obs.get_id(*out);
            ImGui::PushID(out);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputFilteredString("##name", out->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}",
                              ordinal(app.simulation_ed.sim_obs.get_id(*out)));
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

            int plot_type = ordinal(out->plot_type);
            if (ImGui::Combo("##plot",
                             &plot_type,
                             simulation_plot_type_string,
                             IM_ARRAYSIZE(simulation_plot_type_string)))
                out->plot_type = enum_cast<simulation_plot_type>(plot_type);

            ImGui::TableNextColumn();
            if (ImGui::Button("copy")) {
                if (app.simulation_ed.copy_obs.can_alloc(1)) {
                    auto& new_obs          = app.simulation_ed.copy_obs.alloc();
                    new_obs.name           = out->name;
                    new_obs.linear_outputs = out->linear_outputs;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("write")) {
                app.simulation_ed.selected_sim_obs = id;
                app.output_ed.write_output         = true;
                auto err                           = std::error_code{};
                auto file_path = std::filesystem::current_path(err);
                out->write(file_path);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(reinterpret_cast<const char*>(
              out->file.generic_string().c_str()));

            ImGui::PopID();
        }

        simulation_observation_copy *copy = nullptr, *prev = nullptr;
        while (app.simulation_ed.copy_obs.next(copy)) {
            const auto id = app.simulation_ed.copy_obs.get_id(*copy);
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
            int plot_type = ordinal(copy->plot_type);
            if (ImGui::Combo("##plot",
                             &plot_type,
                             simulation_plot_type_string,
                             IM_ARRAYSIZE(simulation_plot_type_string)))
                copy->plot_type = enum_cast<simulation_plot_type>(plot_type);

            ImGui::TableNextColumn();
            if (ImGui::Button("del")) {
                app.simulation_ed.copy_obs.free(*copy);
                copy = prev;
            }

            ImGui::PopID();
            prev = copy;
        }

        ImGui::EndTable();
    }
}

static void show_observation_plot(application& app) noexcept
{
    ImPlot::SetCurrentContext(app.output_ed.implot_context);
    if (ImPlot::BeginPlot("Plot", ImVec2(-1, -1))) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

        ImPlot::SetupAxes(
          nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        simulation_observation* obs = nullptr;
        while (app.simulation_ed.sim_obs.next(obs)) {
            if (obs->linear_outputs.size() > 0) {
                switch (obs->plot_type) {
                case simulation_plot_type::plotlines:
                    ImPlot::PlotLineG(obs->name.c_str(),
                                      ring_buffer_getter,
                                      &obs->linear_outputs,
                                      obs->linear_outputs.ssize());
                    break;

                case simulation_plot_type::plotscatters:
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
        while (app.simulation_ed.copy_obs.next(copy)) {
            if (copy->linear_outputs.size() > 0) {
                switch (copy->plot_type) {
                case simulation_plot_type::plotlines:
                    ImPlot::PlotLineG(copy->name.c_str(),
                                      ring_buffer_getter,
                                      &copy->linear_outputs,
                                      copy->linear_outputs.ssize());
                    break;

                case simulation_plot_type::plotscatters:
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
    if (!ImGui::Begin(output_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto* app = container_of(this, &application::output_ed);

    static const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Observations list", flags))
        show_observation_table(*app);

    if (ImGui::CollapsingHeader("Plots outputs", flags))
        show_observation_plot(*app);

    ImGui::End();
}

} // namespace irt
