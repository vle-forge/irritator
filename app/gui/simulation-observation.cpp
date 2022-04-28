// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/io.hpp>

#include <cmath>

namespace irt {

status simulation_observation::reserve(i32 default_raw_length,
                                       i32 default_linear_length) noexcept
{
    clear();

    irt_assert(default_raw_length > 0);

    raw_outputs.resize(default_raw_length);
    raw_ring_buffer.reset(raw_outputs.data(), raw_outputs.ssize());

    if (default_linear_length > 0) {
        linear_outputs.resize(default_linear_length);
        linear_ring_buffer.reset(linear_outputs.data(), linear_outputs.ssize());
    }

    return status::success;
}

void simulation_observation::clear() noexcept
{
    raw_ring_buffer.clear();
    linear_ring_buffer.clear();
}

void simulation_observation::push(const irt::observer&     obs,
                                  const irt::dynamics_type type,
                                  const irt::time          t) noexcept
{
    raw_ring_buffer.force_emplace_enqueue(obs.msg, t, type);
}

static real compute_value_1(const observation_message& msg,
                            const time                 elapsed) noexcept
{
    return msg[0] + msg[1] * elapsed;
}

static real compute_value_2(const observation_message& msg,
                            const time                 elapsed) noexcept
{
    return msg[0] + msg[1] * elapsed + (msg[2] * elapsed * elapsed / two);
}

static real compute_value_3(const observation_message& msg,
                            const time                 elapsed) noexcept
{
    return msg[0] + msg[1] * elapsed + (msg[2] * elapsed * elapsed / two) +
           (msg[3] * elapsed * elapsed * elapsed / three);
}

void simulation_observation::compute_linear_buffer(real next) noexcept
{
    irt_assert(linear_outputs.size() > 0);

    if (raw_ring_buffer.empty())
        return;

    auto end = last_position;
    auto it  = raw_ring_buffer.rbegin();

    for (; it != end; ++it) {
        const auto elapsed   = it->t - tl;
        const auto nb        = static_cast<i32>(elapsed / time_step);
        const auto remaining = std::fmod(elapsed, time_step);
        auto       td        = it->t;
        real       value;

        for (i32 i = 0; i < nb; ++i, td += time_step) {
            switch (it->type) {
            case dynamics_type::qss1_integrator:
                value = compute_value_1(it->msg, time_step);
                break;
            case dynamics_type::qss2_integrator:
                value = compute_value_2(it->msg, time_step);
                break;
            case dynamics_type::qss3_integrator:
                value = compute_value_3(it->msg, time_step);
                break;
            default:
                value = it->msg[0];
                break;
            }

            linear_ring_buffer.force_emplace_enqueue(value, td);
        }

        if (remaining > zero) {
            switch (it->type) {
            case dynamics_type::qss1_integrator:
                value = compute_value_1(it->msg, next - td);
                break;
            case dynamics_type::qss2_integrator:
                value = compute_value_2(it->msg, next - td);
                break;
            case dynamics_type::qss3_integrator:
                value = compute_value_3(it->msg, next - td);
                break;
            default:
                value = it->msg[0];
                break;
            }

            linear_ring_buffer.force_emplace_enqueue(value, td);
        }
    }

    tl            = next;
    last_position = raw_ring_buffer.rbegin();
}

static inline void simulation_observation_initialize(
  simulation_observation&                   output,
  [[maybe_unused]] const irt::observer&     obs,
  [[maybe_unused]] const irt::dynamics_type type,
  [[maybe_unused]] const irt::time          tl,
  [[maybe_unused]] const irt::time          t) noexcept
{
    output.raw_ring_buffer.clear();
    output.linear_ring_buffer.clear();
    output.last_position = output.raw_ring_buffer.rend();
}

static inline void simulation_observation_run(
  simulation_observation&          output,
  const irt::observer&             obs,
  const irt::dynamics_type         type,
  [[maybe_unused]] const irt::time tl,
  const irt::time                  t) noexcept
{
    // Store only one value for a time
    while (!output.raw_ring_buffer.empty() &&
           output.raw_ring_buffer.back().t == t)
        output.raw_ring_buffer.pop_back();

    output.raw_ring_buffer.force_emplace_enqueue(obs.msg, t, type);
    output.compute_linear_buffer(t);
}

static inline void simulation_observation_finalize(
  [[maybe_unused]] simulation_observation&  output,
  [[maybe_unused]] const irt::observer&     obs,
  [[maybe_unused]] const irt::dynamics_type type,
  [[maybe_unused]] const irt::time          tl,
  [[maybe_unused]] const irt::time          t) noexcept
{}

void simulation_observation_update(const irt::observer&        obs,
                                   const irt::dynamics_type    type,
                                   const irt::time             tl,
                                   const irt::time             t,
                                   const irt::observer::status s) noexcept
{
    auto* s_ed   = reinterpret_cast<simulation_editor*>(obs.user_data);
    auto  id     = enum_cast<simulation_observation_id>(obs.user_id);
    auto* output = s_ed->sim_obs.try_to_get(id);
    irt_assert(output);

    switch (s) {
    case observer::status::initialize:
        simulation_observation_initialize(*output, obs, type, tl, t);
        break;

    case observer::status::run:
        simulation_observation_run(*output, obs, type, tl, t);
        break;

    case observer::status::finalize:
        simulation_observation_finalize(*output, obs, type, tl, t);
        break;
    }
}

static void task_simulation_observation_remove(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    auto mdl_id = enum_cast<model_id>(g_task->param_1);

    simulation_observation* obs = nullptr;
    while (g_task->app->s_editor.sim_obs.next(obs)) {
        if (obs->model == mdl_id) {
            obs->clear();

            auto obs_id = g_task->app->s_editor.sim_obs.get_id(*obs);
            g_task->app->s_editor.sim_obs.free(obs_id);
            break;
        }
    }

    if (auto* mdl = g_task->app->s_editor.sim.models.try_to_get(mdl_id); mdl)
        g_task->app->s_editor.sim.unobserve(*mdl);

    g_task->state = gui_task_status::finished;
}

static void task_simulation_observation_add(void* param) noexcept
{
    auto* g_task  = reinterpret_cast<gui_task*>(param);
    g_task->state = gui_task_status::started;
    g_task->app->state |= application_status_read_only_simulating |
                          application_status_read_only_modeling;

    auto& app    = *g_task->app;
    auto& sim_ed = app.s_editor;
    auto& sim    = app.s_editor.sim;

    auto mdl_id = enum_cast<model_id>(g_task->param_1);
    if (auto* mdl = sim.models.try_to_get(mdl_id); mdl) {
        if (sim.observers.can_alloc(1) && sim_ed.sim_obs.can_alloc(1)) {
            auto& obs    = sim_ed.sim_obs.alloc();
            auto  obs_id = sim_ed.sim_obs.get_id(obs);
            obs.model    = mdl_id;
            obs.reserve(4096, 4099);

            auto& output =
              sim_ed.sim.observers.alloc(obs.name.c_str(),
                                         simulation_observation_update,
                                         &sim_ed,
                                         ordinal(obs_id),
                                         0);
            sim_ed.sim.observe(*mdl, output);
        } else {
            if (!sim.observers.can_alloc(1)) {
                auto& n = app.notifications.alloc(notification_type::error);
                n.title = "Too many observer in simulation";
                g_task->app->notifications.enable(n);
            }

            if (!sim_ed.sim_obs.can_alloc(1)) {
                auto& n = app.notifications.alloc(notification_type::error);
                n.title = "Too many simulation observation in simulation";
                g_task->app->notifications.enable(n);
            }
        }
    }

    g_task->state = gui_task_status::finished;
}

static void remove_simulation_observation_from(application& app,
                                               model_id     mdl_id) noexcept
{
    auto& task   = app.gui_tasks.alloc();
    task.param_1 = ordinal(mdl_id);
    task.app     = &app;
    app.task_mgr.task_lists[0].add(task_simulation_observation_remove, &task);
    app.task_mgr.task_lists[0].submit();
}

static void add_simulation_observation_for(application& app,
                                           model_id     mdl_id) noexcept
{
    auto& task   = app.gui_tasks.alloc();
    task.param_1 = ordinal(mdl_id);
    task.app     = &app;
    app.task_mgr.task_lists[0].add(task_simulation_observation_add, &task);
    app.task_mgr.task_lists[0].submit();
}

static float values_getter(void* data, int idx) noexcept
{
    auto* obs   = reinterpret_cast<simulation_observation*>(data);
    auto  index = obs->linear_ring_buffer.index_from_begin(idx);

    return static_cast<float>(obs->linear_outputs[index].msg);
}

void application::show_simulation_observation_window() noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Observations", flags)) {
        simulation_observation* obs = nullptr;
        while (s_editor.sim_obs.next(obs)) {
            ImGui::PushID(obs);
            ImGui::InputFilteredString("name", obs->name);
            ImGui::PlotLines("test",
                             values_getter,
                             obs,
                             obs->linear_ring_buffer.ssize(),
                             0,
                             nullptr,
                             FLT_MIN,
                             FLT_MAX,
                             ImVec2{ 0.f, 80.f });
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Selected", flags)) {
        for (int i = 0, e = s_editor.selected_nodes.size(); i != e; ++i) {
            const auto index = s_editor.selected_nodes[i];

            if (index == -1)
                continue;

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

            ImGui::TextFormat(
              "ID.....: {}",
              static_cast<u64>(s_editor.sim.models.get_id(*mdl)));
            ImGui::TextFormat("Type...: {}",
                              dynamics_type_names[ordinal(mdl->type)]);

            if (already_observed) {
                if (ImGui::Button("remove"))
                    remove_simulation_observation_from(*this, mdl_id);
            } else {
                if (ImGui::Button("observe"))
                    add_simulation_observation_for(*this, mdl_id);
            }

            ImGui::PopID();
        }

        ImGui::Separator();
    }
}

} // namespace irt