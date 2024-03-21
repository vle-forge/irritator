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

static const char* simulation_plot_type_string[] = { "None",
                                                     "Plot line",
                                                     "Plot dot" };

output_editor::output_editor() noexcept
  : implot_context{ ImPlot::CreateContext() }
{}

output_editor::~output_editor() noexcept
{
    if (implot_context)
        ImPlot::DestroyContext(implot_context);
}

static void show_plot_observation(application&                  app,
                                  variable_observer&            var,
                                  variable_simulation_observer& sys) noexcept
{
    ImGui::PushID(&var);

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);
    ImGui::InputFilteredString("##name", var.name);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();

    ImGui::TableNextColumn();
    ImGui::PushItemWidth(-1);
    float copy = sys.time_step;

    if (ImGui::InputReal("##ts", &copy))
        sys.time_step.set(copy);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();
    ImGui::TextFormat("{}", sys.observers.size());
    ImGui::TableNextColumn();

    int plot_type = ordinal(var.type);
    if (ImGui::Combo("##plot",
                     &plot_type,
                     simulation_plot_type_string,
                     IM_ARRAYSIZE(simulation_plot_type_string)))
        var.type = enum_cast<variable_observer::type_options>(plot_type);

    ImGui::TableNextColumn();

    const bool can_copy = app.simulation_ed.copy_obs.can_alloc(1);
    ImGui::BeginDisabled(!can_copy);
    if (ImGui::Button("copy")) {
        // auto& new_obs          = app.simulation_ed.copy_obs.alloc();
        // new_obs.name           = var.name.sv();
        // new_obs.linear_outputs = obs.linearized_buffer;
        auto& n = app.notifications.alloc(log_level::notice);
        n.title = "Not yet implemented";
        app.notifications.enable(n);
    }

    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("write")) {
        app.output_ed.write_output = true;
        auto err                   = std::error_code{};
        auto file_path             = std::filesystem::current_path(err);
        app.simulation_ed.plot_obs.write(app, file_path);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(reinterpret_cast<const char*>(
      app.simulation_ed.plot_obs.file.generic_string().c_str()));

    ImGui::PopID();
}

static void show_observation_table(application& app) noexcept
{
    static const ImGuiTableFlags flags =
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
        ImGui::TableSetupColumn("export as",
                                ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableHeadersRow();

        for (auto& v_obs : app.pj.variable_observers) {
            const auto id  = app.pj.variable_observers.get_id(v_obs);
            const auto idx = get_index(id);

            show_plot_observation(
              app, v_obs, app.pj.variable_observation_systems[idx]);
        }

        plot_copy *copy = nullptr, *prev = nullptr;
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

void output_editor::show() noexcept
{
    if (!ImGui::Begin(output_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::output_ed);

    static const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Observations list", flags))
        show_observation_table(app);

    if (ImGui::CollapsingHeader("Plots outputs", flags)) {
        ImPlot::SetCurrentContext(app.output_ed.implot_context);
        app.simulation_ed.plot_copy_wgt.show("Copy");
    }

    ImGui::End();
}

} // namespace irt
