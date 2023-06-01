// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"
#include "irritator/helpers.hpp"

#include <irritator/io.hpp>

#include <cmath>
#include <limits>

namespace irt {

void task_remove_simulation_observation(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& app    = *task->app;
    auto& sim_ed = app.simulation_ed;
    auto  mdl_id = enum_cast<model_id>(task->param_1);

    sim_ed.remove_simulation_observation_from(mdl_id);

    task->state = task_status::finished;
}

void task_add_simulation_observation(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto& app    = *g_task->app;
    auto& sim_ed = app.simulation_ed;
    auto  mdl_id = enum_cast<model_id>(g_task->param_1);

    small_string<15> name;
    format(name, "{}", g_task->param_1);
    sim_ed.add_simulation_observation_for(name.sv(), mdl_id);

    g_task->state = task_status::finished;
}

void preview_window::show() noexcept
{
    if (!ImGui::Begin(preview_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    // auto* app = container_of(this, &application::preview_wnd);
    // auto& s_editor = app->simulation_ed;

    // const ImGuiTableFlags flags =
    //   ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
    //   ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
    //   ImGuiTableFlags_Reorderable;

    ImGui::Checkbox("Enable history", &preview_scrolling);

    ImGui::BeginDisabled(!preview_scrolling);
    if (ImGui::InputDouble("History", &preview_history))
        preview_history = preview_history <= 0.0 ? 1.0 : preview_history;
    ImGui::EndDisabled();

    // if (ImGui::BeginTable("##table", 1, flags, ImVec2(-1, 0))) {
    //     ImGui::TableSetupColumn("preview");
    //     ImGui::TableHeadersRow();
    //     ImPlot::PushColormap(ImPlotColormap_Pastel);

    //     plot_observation* obs = nullptr;
    //     int               row = -1;
    //     while (s_editor.plot_obs.next(obs)) {
    //         if_data_exists_do(app->sim.observers, obs)
    //         observer* o = nullptr;
    //         irt_assert(get_observer(*app, *obs, o));

    //         ++row;
    //         if (o->linearized_buffer.empty())
    //             continue;

    //         ImGui::TableNextRow();
    //         ImGui::TableSetColumnIndex(0);

    //         ImGui::PushID(obs);

    //         ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
    //         if (ImPlot::BeginPlot("##Plot",
    //                               ImVec2(-1, 70),
    //                               ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
    //                                 ImPlotFlags_NoBoxSelect |
    //                                 ImPlotFlags_NoChild)) {
    //             ImPlot::SetupAxes(nullptr,
    //                               nullptr,
    //                               ImPlotAxisFlags_NoDecorations,
    //                               ImPlotAxisFlags_NoDecorations);

    //             auto start_t = app->sim_obs.limits.Min;
    //             if (preview_scrolling) {
    //                 start_t = app->sim_obs.limits.Max - preview_history;
    //                 if (start_t < app->sim_obs.limits.Min)
    //                     start_t = app->sim_obs.limits.Min;
    //             }

    //             if (std::isfinite(start_t)) {
    //                 ImPlot::SetupAxisLimits(ImAxis_X1,
    //                                         start_t,
    //                                         app->sim_obs.limits.Max,
    //                                         ImPlotCond_Always);

    //                 ImPlot::PushStyleColor(ImPlotCol_Line,
    //                                        ImPlot::GetColormapColor(row));

    //                 ImPlot::PlotLineG(obs->name.c_str(),
    //                                   ring_buffer_getter,
    //                                   &o->linearized_buffer,
    //                                   o->linearized_buffer.ssize());

    //                 ImPlot::PopStyleColor();
    //             }

    //             ImPlot::EndPlot();
    //         }

    //         ImPlot::PopStyleVar();
    //         ImGui::PopID();
    //     }

    //     ImPlot::PopColormap();
    // }

    // ImGui::EndTable();

    // if (ImGui::CollapsingHeader("Selected", ImGuiTreeNodeFlags_DefaultOpen))
    // {
    //     if (ImGui::BeginTable("models", 3)) {
    //         ImGui::TableSetupColumn("type");
    //         ImGui::TableSetupColumn("name");
    //         ImGui::TableSetupColumn("action");
    //         ImGui::TableHeadersRow();

    //         for (int i = 0, e = s_editor.selected_nodes.size(); i != e; ++i)
    //         {
    //             const auto index = s_editor.selected_nodes[i];
    //             auto* mdl =
    //             app->sim.models.try_to_get(static_cast<u32>(index)); if
    //             (!mdl)
    //                 continue;

    //             ImGui::TableNextRow();

    //             const auto mdl_id = app->sim.models.get_id(*mdl);
    //             ImGui::PushID(i);

    //             bool              already_observed = false;
    //             plot_observation* obs              = nullptr;
    //             while (s_editor.plot_obs.next(obs)) {
    //                 if (obs->model == mdl_id) {
    //                     already_observed = true;
    //                     break;
    //                 }
    //             }

    //             ImGui::TableNextColumn();
    //             ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
    //             ImGui::TableNextColumn();

    //             if (already_observed) {
    //                 ImGui::PushItemWidth(-1);
    //                 ImGui::InputSmallString("##name", obs->name);
    //                 ImGui::PopItemWidth();
    //             }

    //             ImGui::TableNextColumn();

    //             if (already_observed) {
    //                 if (ImGui::Button("remove"))
    //                     app->add_simulation_task(
    //                       task_remove_simulation_observation,
    //                       ordinal(mdl_id));
    //             } else {
    //                 if (ImGui::Button("observe"))
    //                     app->add_simulation_task(
    //                       task_add_simulation_observation, ordinal(mdl_id));
    //             }

    //             ImGui::PopID();
    //         }
    //         ImGui::EndTable();
    //     }
    // }

    ImGui::End();
}

} // namespace irt
