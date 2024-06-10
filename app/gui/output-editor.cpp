// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/helpers.hpp"
#include "irritator/macros.hpp"
#include "irritator/modeling.hpp"

#include <optional>
#include <utility>

namespace irt {

constexpr static inline const char* plot_type_str[] = { "None",
                                                        "Plot line",
                                                        "Plot dot" };

static void show_obervers_table(application& app) noexcept
{
    for (auto& vobs : app.pj.variable_observers) {
        auto to_copy  = std::optional<variable_observer::sub_id>();
        auto to_write = std::optional<variable_observer::sub_id>();

        vobs.for_each([&](const auto id) noexcept {
            const auto  idx    = get_index(id);
            const auto  obs_id = vobs.get_obs_ids()[idx];
            const auto* obs    = app.sim.observers.try_to_get(obs_id);
            ImGui::PushID(idx);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputFilteredString("##name", vobs.get_names()[idx]);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(id));

            ImGui::TableNextColumn();
            if (obs)
                ImGui::TextFormat("{}", obs->time_step);
            else
                ImGui::TextUnformatted("-");

            ImGui::TableNextColumn();
            if (obs)
                ImGui::TextFormat("{}", obs->linearized_buffer.size());
            else
                ImGui::TextUnformatted("-");

            ImGui::TableNextColumn();
            int plot_type = ordinal(vobs.get_options()[idx]);
            if (ImGui::Combo("##plot",
                             &plot_type,
                             plot_type_str,
                             IM_ARRAYSIZE(plot_type_str)))
                vobs.get_options()[idx] =
                  enum_cast<variable_observer::type_options>(plot_type);

            ImGui::TableNextColumn();
            const bool can_copy = app.simulation_ed.copy_obs.can_alloc(1);
            ImGui::BeginDisabled(!can_copy);
            if (ImGui::Button("copy"))
                to_copy = id;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("write"))
                to_write = id;

            ImGui::PopID();
        });

        if (to_copy.has_value()) {
            const auto obs_id = vobs.get_obs_ids()[get_index(*to_copy)];
            const auto obs    = app.sim.observers.try_to_get(obs_id);

            auto& new_obs          = app.simulation_ed.copy_obs.alloc();
            new_obs.name           = vobs.get_names()[get_index(*to_copy)].sv();
            new_obs.linear_outputs = obs->linearized_buffer;
        }

        if (to_write.has_value()) {
            app.output_ed.write_output = true;
            auto err                   = std::error_code{};
            auto file_path             = std::filesystem::current_path(err);
            app.simulation_ed.plot_obs.write(app, file_path);
        }
    }
}

static void show_copy_table(irt::application& app) noexcept
{
    auto to_del = std::optional<plot_copy_id>();

    for (auto& copy : app.simulation_ed.copy_obs) {
        const auto id = app.simulation_ed.copy_obs.get_id(copy);

        ImGui::PushID(&copy);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        ImGui::InputFilteredString("##name", copy.name);
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", ordinal(id));

        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");

        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", copy.linear_outputs.size());

        ImGui::TableNextColumn();
        int plot_type = ordinal(copy.plot_type);
        if (ImGui::Combo(
              "##plot", &plot_type, plot_type_str, IM_ARRAYSIZE(plot_type_str)))
            copy.plot_type = enum_cast<simulation_plot_type>(plot_type);

        ImGui::TableNextColumn();

        if (ImGui::Button("del"))
            to_del = id;

        ImGui::PopID();
    }

    if (to_del.has_value())
        app.simulation_ed.copy_obs.free(*to_del);
}

static void show_observation_table(application& app) noexcept
{
    constexpr static auto flags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    if (ImGui::BeginTable("Observations", 6, flags)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn(
          "time-step", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn(
          "plot", ImGuiTableColumnFlags_WidthFixed, 180.f);
        ImGui::TableSetupColumn("actions", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableHeadersRow();

        show_obervers_table(app);
        show_copy_table(app);

        ImGui::EndTable();
    }
}

output_editor::output_editor() noexcept
  : implot_context{ ImPlot::CreateContext() }
{}

output_editor::~output_editor() noexcept
{
    if (implot_context)
        ImPlot::DestroyContext(implot_context);
}

void output_editor::show() noexcept
{
    if (!ImGui::Begin(output_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::output_ed);

    if (ImGui::CollapsingHeader("Observations list",
                                ImGuiTreeNodeFlags_DefaultOpen))
        show_observation_table(app);

    if (ImGui::CollapsingHeader("Plots outputs",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImPlot::SetCurrentContext(app.output_ed.implot_context);

        if (ImPlot::BeginPlot("Plots", ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupAxes(nullptr,
                              nullptr,
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);

            for (auto& vobs : app.pj.variable_observers) {
                vobs.for_each([&](const auto id) noexcept {
                    const auto idx = get_index(id);

                    const auto  obs_id = vobs.get_obs_ids()[idx];
                    const auto* obs    = app.sim.observers.try_to_get(obs_id);

                    if (not obs)
                        return;

                    if (vobs.get_options()[idx] !=
                        variable_observer::type_options::none)
                        app.simulation_ed.plot_obs.show_plot_line(
                          *obs, vobs.get_options()[idx], vobs.get_names()[idx]);
                });
            }

            for (auto& p : app.simulation_ed.copy_obs)
                if (p.plot_type != simulation_plot_type::none)
                    app.simulation_ed.plot_copy_wgt.show_plot_line(p);

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

} // namespace irt
