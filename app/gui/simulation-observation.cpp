// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/io.hpp>

#include <cmath>

namespace irt {

simulation_observation::simulation_observation(model_id mdl_,
                                               i32 buffer_capacity) noexcept
  : model{ mdl_ }
  , linear_outputs{ buffer_capacity }
{
    irt_assert(buffer_capacity > 0);
}

void simulation_observation::clear() noexcept { linear_outputs.clear(); }

void simulation_observation::write(
  const std::filesystem::path& file_path) noexcept
{
    try {
        if (auto ofs = std::ofstream{ file_path }; ofs.is_open()) {
            auto it = linear_outputs.head();
            auto et = linear_outputs.end();

            for (; it != et; ++it)
                ofs << it->x << ',' << it->y << '\n';
        }
    } catch (const std::exception& /*e*/) {
    }
}

void simulation_observation::update(observer& obs) noexcept
{
    while (obs.buffer.size() > 2) {
        auto it = std::back_insert_iterator<simulation_observation>(*this);

        if (interpolate) {
            write_interpolate_data(obs, it, time_step);
        } else {
            write_raw_data(obs, it);
        }
    }
}

void simulation_observation::flush(observer& obs) noexcept
{
    auto it = std::back_insert_iterator<simulation_observation>(*this);

    if (interpolate) {
        flush_interpolate_data(obs, it, time_step);
    } else {
        flush_raw_data(obs, it);
    }
}

void simulation_observation::push_back(real r) noexcept
{
    if (output_vec.size() >= 2) {
        linear_outputs.push_back(ImPlotPoint{ output_vec[0], output_vec[1] });
        output_vec.clear();
    }

    output_vec.emplace_back(r);
}

static void task_remove_simulation_observation_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    auto& app    = *g_task->app;
    auto& sim_ed = app.s_editor;
    auto  mdl_id = enum_cast<model_id>(g_task->param_1);

    sim_ed.remove_simulation_observation_from(mdl_id);

    g_task->state = gui_task_status::finished;
}

static void task_add_simulation_observation_impl(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    auto& app    = *g_task->app;
    auto& sim_ed = app.s_editor;
    auto  mdl_id = enum_cast<model_id>(g_task->param_1);

    sim_ed.add_simulation_observation_for(std::string_view{}, mdl_id);

    g_task->state = gui_task_status::finished;
}

void task_remove_simulation_observation(application& app, model_id id) noexcept
{
    auto& task   = app.gui_tasks.alloc();
    task.param_1 = ordinal(id);
    task.app     = &app;
    app.task_mgr.task_lists[0].add(task_remove_simulation_observation_impl,
                                   &task);
    app.task_mgr.task_lists[0].submit();
}

void task_add_simulation_observation(application& app, model_id id) noexcept
{
    auto& task   = app.gui_tasks.alloc();
    task.param_1 = ordinal(id);
    task.app     = &app;
    app.task_mgr.task_lists[0].add(task_add_simulation_observation_impl, &task);
    app.task_mgr.task_lists[0].submit();
}

void task_build_observation_output(void* param) noexcept
{
    auto* g_task = reinterpret_cast<gui_task*>(param);

    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    for (auto obs_id : g_task->app->s_editor.sim.immediate_observers) {
        if (auto* obs =
              g_task->app->s_editor.sim.observers.try_to_get(obs_id)) {
            auto sim_obs_id =
              enum_cast<simulation_observation_id>(obs->user_id);
            if (auto* sobs =
                  g_task->app->s_editor.sim_obs.try_to_get(sim_obs_id);
                sobs)
                sobs->update(*obs);
        }
    }

    // auto obs_id = enum_cast<observer_id>(g_task->param_1);
    // if (auto* obs =
    //       g_task->app->s_editor.sim.observers.try_to_get(obs_id)) {
    //     auto sim_obs_id =
    //       enum_cast<simulation_observation_id>(obs->user_id);
    //     if (auto* sobs =
    //           g_task->app->s_editor.sim_obs.try_to_get(sim_obs_id);
    //         sobs)
    //         sobs->update(*obs);
    // }

    g_task->state = gui_task_status::finished;
}

void simulation_editor::build_observation_output() noexcept
{
    fmt::print("Add task_build_observation_output\n");
    auto* app = container_of(this, &application::s_editor);

    fmt::print("simulation_editor gui_task: {} / {}\n",
               app->gui_tasks.size(),
               app->gui_tasks.capacity());

    // for (auto elem : sim.immediate_observers) {
    auto& task = app->gui_tasks.alloc();
    task.app   = app;
    // task.param_1 = ordinal(elem);
    app->task_mgr.task_lists[0].add(task_build_observation_output, &task);
    app->task_mgr.task_lists[0].submit();
    // }
}

void application::show_simulation_observation_window() noexcept
{
    ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Reorderable;

    ImGui::Checkbox("Enable history", &s_editor.scrolling);

    ImGui::BeginDisabled(!s_editor.scrolling);
    if (ImGui::InputFloat("History", &s_editor.history))
        s_editor.history = s_editor.history <= 0 ? 1 : s_editor.history;
    ImGui::EndDisabled();

    if (ImGui::BeginTable("##table", 1, flags, ImVec2(-1, 0))) {
        ImGui::TableSetupColumn("Signal");
        ImGui::TableHeadersRow();
        ImPlot::PushColormap(ImPlotColormap_Pastel);

        simulation_observation* obs = nullptr;
        int                     row = -1;
        while (s_editor.sim_obs.next(obs)) {
            ++row;
            if (obs->linear_outputs.empty())
                continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::PushID(obs);

            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
            if (ImPlot::BeginPlot("##Plot",
                                  ImVec2(-1, 70),
                                  ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
                                    ImPlotFlags_NoBoxSelect |
                                    ImPlotFlags_NoChild)) {
                ImPlot::SetupAxes(nullptr,
                                  nullptr,
                                  ImPlotAxisFlags_NoDecorations,
                                  ImPlotAxisFlags_NoDecorations);

                float start_t = obs->limits.x;

                // if (s_editor.scrolling) {
                //     start_t = obs->limits.z - s_editor.history;
                //     if (start_t < obs->limits.x)
                //         start_t = obs->limits.x;
                // }

                // ImPlot::SetupAxesLimits(start_t,
                //                         obs->limits.z,
                //                         obs->limits.y,
                //                         obs->limits.w,
                //                         ImGuiCond_Always);

                ImPlot::PushStyleColor(ImPlotCol_Line,
                                       ImPlot::GetColormapColor(row));

                ImPlot::PlotLineG(obs->name.c_str(),
                                  ring_buffer_getter,
                                  &obs->linear_outputs,
                                  obs->linear_outputs.ssize());

                ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }

            ImPlot::PopStyleVar();
            ImGui::PopID();
        }

        ImPlot::PopColormap();
    }

    ImGui::EndTable();

    if (ImGui::CollapsingHeader("Selected", flags)) {
        for (int i = 0, e = s_editor.selected_nodes.size(); i != e; ++i) {
            const auto index = s_editor.selected_nodes[i];
            auto* mdl = s_editor.sim.models.try_to_get(static_cast<u32>(index));
            if (!mdl)
                continue;

            const auto mdl_id = s_editor.sim.models.get_id(*mdl);
            ImGui::PushID(mdl);

            bool                    already_observed = false;
            simulation_observation* obs              = nullptr;
            while (s_editor.sim_obs.next(obs)) {
                if (obs->model == mdl_id) {
                    already_observed = true;
                    break;
                }
            }

            ImGui::TextFormat("Type...: {}",
                              dynamics_type_names[ordinal(mdl->type)]);

            if (obs)
                ImGui::InputSmallString("Name", obs->name);

            ImGui::TextFormat(
              "ID.....: {}",
              static_cast<u64>(s_editor.sim.models.get_id(*mdl)));

            if (already_observed) {
                if (ImGui::Button("remove"))
                    task_remove_simulation_observation(*this, mdl_id);
            } else {
                if (ImGui::Button("observe"))
                    task_add_simulation_observation(*this, mdl_id);
            }

            ImGui::PopID();
        }

        ImGui::Separator();
    }
}

} // namespace irt
