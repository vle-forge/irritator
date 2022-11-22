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

void simulation_observation::clear() noexcept
{
    output_vec.clear();
    linear_outputs.clear();

    limits.Min = -INFINITY;
    limits.Max = +INFINITY;
}

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
        write_interpolate_data(obs, it, time_step);
    }

    if (linear_outputs.ssize() >= 1) {
        limits.Min = linear_outputs.head()->x;
        limits.Max = linear_outputs.tail()->x;
    }
}

void simulation_observation::flush(observer& obs) noexcept
{
    auto it = std::back_insert_iterator<simulation_observation>(*this);
    flush_interpolate_data(obs, it, time_step);

    if (linear_outputs.ssize() >= 1) {
        limits.Min = linear_outputs.head()->x;
        limits.Max = linear_outputs.tail()->x;
    }
}

void simulation_observation::push_back(real r) noexcept
{
    if (output_vec.size() >= 2) {
        linear_outputs.force_enqueue(
          ImPlotPoint{ output_vec[0], output_vec[1] });
        output_vec.clear();
    }

    output_vec.emplace_back(r);
}

struct simulation_observation_job
{
    application* app = nullptr;
    observer_id  id  = undefined<observer_id>();
};

/* This job retrieves observation data from observer to interpolate data a fill
   simulation_observation structure.  */
static void simulation_observation_job_update(void* param) noexcept
{
    auto* job = reinterpret_cast<simulation_observation_job*>(param);

    if (auto* obs = job->app->s_editor.sim.observers.try_to_get(job->id)) {
        auto sim_obs_id = enum_cast<simulation_observation_id>(obs->user_id);
        if (auto* sobs = job->app->s_editor.sim_obs.try_to_get(sim_obs_id);
            sobs)
            sobs->update(*obs);
    }
}

/* This job retrieves observation data from observer to interpolate data a fill
   simulation_observation structure.  */
static void simulation_observation_job_finish(void* param) noexcept
{
    auto* job = reinterpret_cast<simulation_observation_job*>(param);

    if (auto* obs = job->app->s_editor.sim.observers.try_to_get(job->id)) {
        auto sim_obs_id = enum_cast<simulation_observation_id>(obs->user_id);
        if (auto* sobs = job->app->s_editor.sim_obs.try_to_get(sim_obs_id);
            sobs)
            sobs->flush(*obs);
    }
}

/* This task performs output interpolation Internally, it uses the
   unordered_task_list to compute observations, one job by observers. If the
   immediate_observers is empty then all observers are update. */
void simulation_editor::build_observation_output() noexcept
{
    auto* app = container_of(this, &application::s_editor);

    constexpr int              capacity = 255;
    simulation_observation_job jobs[capacity];

    if (sim.immediate_observers.empty()) {
        int       obs_max = sim.observers.size();
        int       current = 0;
        observer* obs     = nullptr;

        while (sim.observers.next(obs)) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = sim.observers.get_id(*obs);
                sim.observers.next(obs);

                jobs[i] = { .app = app, .id = obs_id };
                app->task_mgr.temp_task_lists[1].add(
                  simulation_observation_job_update, &jobs[i]);
            }

            app->task_mgr.temp_task_lists[1].submit();
            app->task_mgr.temp_task_lists[1].wait();

            current += loop;
            if (obs_max >= capacity)
                obs_max -= capacity;
            else
                obs_max = 0;
        }
    } else {
        int obs_max = sim.immediate_observers.ssize();
        int current = 0;

        while (obs_max > 0) {
            int loop = std::min(obs_max, capacity);

            for (int i = 0; i != loop; ++i) {
                auto obs_id = sim.immediate_observers[i + current];

                jobs[i] = { .app = app, .id = obs_id };
                app->task_mgr.temp_task_lists[1].add(
                  simulation_observation_job_finish, &jobs[i]);
            }

            app->task_mgr.temp_task_lists[1].submit();
            app->task_mgr.temp_task_lists[1].wait();

            current += loop;
            if (obs_max > capacity)
                obs_max -= capacity;
            else
                obs_max = 0;
        }
    }
}

void task_remove_simulation_observation(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& app    = *task->app;
    auto& sim_ed = app.s_editor;
    auto  mdl_id = enum_cast<model_id>(task->param_1);

    sim_ed.remove_simulation_observation_from(mdl_id);

    task->state = task_status::finished;
}

void task_add_simulation_observation(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<simulation_task*>(param);
    g_task->state = task_status::started;

    auto& app    = *g_task->app;
    auto& sim_ed = app.s_editor;
    auto  mdl_id = enum_cast<model_id>(g_task->param_1);

    small_string<15> name;
    format(name, "{}", g_task->param_1);
    sim_ed.add_simulation_observation_for(name.sv(), mdl_id);

    g_task->state = task_status::finished;
}

void application::show_simulation_observation_window() noexcept
{
    const ImGuiTableFlags flags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    ImGui::Checkbox("Enable history", &s_editor.preview_scrolling);

    ImGui::BeginDisabled(!s_editor.preview_scrolling);
    if (ImGui::InputDouble("History", &s_editor.preview_history))
        s_editor.preview_history =
          s_editor.preview_history <= 0.0 ? 1.0 : s_editor.preview_history;
    ImGui::EndDisabled();

    if (ImGui::BeginTable("##table", 1, flags, ImVec2(-1, 0))) {
        ImGui::TableSetupColumn("preview");
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

                auto start_t = obs->limits.Min;

                if (s_editor.preview_scrolling) {
                    start_t = obs->limits.Max - s_editor.preview_history;
                    if (start_t < obs->limits.Min)
                        start_t = obs->limits.Min;
                }

                ImPlot::SetupAxisLimits(
                  ImAxis_X1, start_t, obs->limits.Max, ImPlotCond_Always);

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

    if (ImGui::CollapsingHeader("Selected", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (s_editor.selected_nodes.empty())
            return;

        if (ImGui::BeginTable("models", 3)) {
            ImGui::TableSetupColumn("type");
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("action");
            ImGui::TableHeadersRow();

            for (int i = 0, e = s_editor.selected_nodes.size(); i != e; ++i) {
                const auto index = s_editor.selected_nodes[i];
                auto*      mdl =
                  s_editor.sim.models.try_to_get(static_cast<u32>(index));
                if (!mdl)
                    continue;

                ImGui::TableNextRow();

                const auto mdl_id = s_editor.sim.models.get_id(*mdl);
                ImGui::PushID(i);

                bool                    already_observed = false;
                simulation_observation* obs              = nullptr;
                while (s_editor.sim_obs.next(obs)) {
                    if (obs->model == mdl_id) {
                        already_observed = true;
                        break;
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl->type)]);
                ImGui::TableNextColumn();

                if (already_observed) {
                    ImGui::PushItemWidth(-1);
                    ImGui::InputSmallString("##name", obs->name);
                    ImGui::PopItemWidth();
                }

                ImGui::TableNextColumn();

                if (already_observed) {
                    if (ImGui::Button("remove"))
                        add_simulation_task(task_remove_simulation_observation,
                                            ordinal(mdl_id));
                } else {
                    if (ImGui::Button("observe"))
                        add_simulation_task(task_add_simulation_observation,
                                            ordinal(mdl_id));
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
}

} // namespace irt
