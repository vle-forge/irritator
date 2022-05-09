// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/io.hpp>

#include <cmath>

namespace irt {

using interpolate_type = i32;
enum interpolate_type_
{
    interpolate_type_none,
    interpolate_type_qss1,
    interpolate_type_qss2,
    interpolate_type_qss3
};

static interpolate_type get_interpolate_type(const dynamics_type type) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_cross:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
        return interpolate_type_qss1;

    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
        return interpolate_type_qss2;

    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_cross:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
        return interpolate_type_qss3;

    case dynamics_type::integrator:
    case dynamics_type::quantifier:
    case dynamics_type::adder_2:
    case dynamics_type::adder_3:
    case dynamics_type::adder_4:
    case dynamics_type::mult_2:
    case dynamics_type::mult_3:
    case dynamics_type::mult_4:
        return interpolate_type_qss1;

    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::cross:
    case dynamics_type::time_func:
    case dynamics_type::accumulator_2:
    case dynamics_type::filter:
    case dynamics_type::flow:
        return interpolate_type_none;
    }

    irt_unreachable();
}

static real compute_value_0(const observation_message& msg,
                            const time /*elapsed*/) noexcept
{
    return msg[0];
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

static void compute_interpolate_step(
  const simulation_observation::raw_observation&       prev,
  const real                                           next,
  const real                                           time_step,
  function_ref<real(const observation_message&, time)> compute_f,
  function_ref<void(real, time)>                       output_f) noexcept

{
    const auto elapsed = next - prev.t;

    if (elapsed <= 0)
        return;

    const auto nb = static_cast<int>(elapsed / time_step);
    auto       td = prev.t;

    for (int i = 0; i < nb; ++i, td += time_step) {
        const auto e     = td - prev.t;
        const auto value = compute_f(prev.msg, e);
        output_f(value, td);
    }

    const auto e = next - td - std::numeric_limits<real>::epsilon();
    if (e > zero) {
        const auto value = compute_f(prev.msg, e);
        output_f(value, td + e);
    }
}

static void compute_and_store_interpolate(
  simulation_observation&                              obs,
  const real                                           until,
  function_ref<real(const observation_message&, time)> f,
  ring_buffer<ImVec2>&                                 out) noexcept
{
    auto prev = obs.raw_ring_buffer.head();
    auto et   = obs.raw_ring_buffer.end();
    auto it   = prev;
    ++it;

    auto out_cb = [&out](real value, time t) {
        out.force_emplace_enqueue(static_cast<float>(value),
                                  static_cast<float>(t));
    };

    for (; it != et; ++it, ++prev)
        compute_interpolate_step(*prev, it->t, obs.time_step, f, out_cb);

    if (!time_domain<real>::is_infinity(until) && prev->t < until)
        compute_interpolate_step(*prev, until, obs.time_step, f, out_cb);
}

static void compute_and_store_interpolate(
  simulation_observation&                              obs,
  const real                                           until,
  function_ref<real(const observation_message&, time)> f,
  ImVector<ImVec2>&                                    out) noexcept
{
    auto prev = obs.raw_ring_buffer.head();
    auto et   = obs.raw_ring_buffer.end();
    auto it   = prev;
    ++it;

    auto out_cb = [&out](real value, time t) {
        out.push_back(ImVec2(static_cast<float>(t), static_cast<float>(value)));
    };

    for (; it != et; ++it, ++prev)
        compute_interpolate_step(*prev, it->t, obs.time_step, f, out_cb);

    if (!time_domain<real>::is_infinity(until) && prev->t < until)
        compute_interpolate_step(*prev, until, obs.time_step, f, out_cb);
}

static void compute_and_stream_interpolate(
  simulation_observation&                              obs,
  const real                                           until,
  std::ofstream&                                       ofs,
  function_ref<real(const observation_message&, time)> f) noexcept
{
    auto prev = obs.raw_ring_buffer.head();
    auto et   = obs.raw_ring_buffer.end();
    auto it   = prev;
    ++it;

    auto out = [&ofs](real value, time t) { ofs << t << ',' << value << '\n'; };

    for (; it != et; ++it, ++prev)
        compute_interpolate_step(*prev, it->t, obs.time_step, f, out);

    if (!time_domain<real>::is_infinity(until) && prev->t < until)
        compute_interpolate_step(*prev, until, obs.time_step, f, out);
}

simulation_observation::simulation_observation(
  model_id      mdl_,
  dynamics_type type_,
  i32           default_raw_length,
  i32           default_linear_length) noexcept
  : model{ mdl_ }
  , type{ type_ }
{
    irt_assert(default_raw_length > 0);

    raw_outputs.resize(default_raw_length);
    raw_ring_buffer.reset(raw_outputs.data(), raw_outputs.ssize());

    if (default_linear_length > 0) {
        linear_outputs.resize(default_linear_length);
        linear_ring_buffer.reset(linear_outputs.data(), linear_outputs.ssize());
    }
}

void simulation_observation::clear() noexcept
{
    raw_ring_buffer.clear();
    linear_ring_buffer.clear();
}

void simulation_observation::save_raw(
  const std::filesystem::path& file_path) noexcept
{
    try {
        if (auto ofs = std::ofstream{ file_path }; ofs.is_open()) {
            const auto i_type = get_interpolate_type(type);
            auto       it     = raw_ring_buffer.head();
            auto       et     = raw_ring_buffer.end();

            switch (i_type) {
            case interpolate_type_qss1:
                ofs << "t,value\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;

            case interpolate_type_qss2:
                ofs << "t,value,value2\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << ',' << it->msg[1]
                        << '\n';
                break;

            case interpolate_type_qss3:
                ofs << "t,value,value2,value3\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << ',' << it->msg[1]
                        << ',' << it->msg[2] << '\n';
                break;

            default:
                ofs << "t,value\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;
            }
        }
    } catch (const std::exception& /*e*/) {
    }
}

void simulation_observation::save_interpolate(
  const std::filesystem::path& file_path) noexcept
{
    try {
        if (auto ofs = std::ofstream{ file_path }; ofs.is_open()) {
            const auto i_type = get_interpolate_type(type);
            ofs << "t,value\n";

            if (!raw_ring_buffer.empty()) {
                switch (i_type) {
                case interpolate_type_qss1:
                    compute_and_stream_interpolate(
                      *this, raw_ring_buffer.back().t, ofs, compute_value_1);
                    break;

                case interpolate_type_qss2:
                    compute_and_stream_interpolate(
                      *this, raw_ring_buffer.back().t, ofs, compute_value_2);
                    break;

                case interpolate_type_qss3:
                    compute_and_stream_interpolate(
                      *this, raw_ring_buffer.back().t, ofs, compute_value_3);
                    break;

                default:
                    compute_and_stream_interpolate(
                      *this, raw_ring_buffer.back().t, ofs, compute_value_0);
                    break;
                }
            }
        }
    } catch (const std::exception& /*e*/) {
    }
}

void simulation_observation::compute_interpolate(
  const real           until,
  ring_buffer<ImVec2>& out) noexcept
{
    const auto i_type = get_interpolate_type(type);

    switch (i_type) {
    case interpolate_type_qss1:
        compute_and_store_interpolate(*this, until, compute_value_1, out);
        break;

    case interpolate_type_qss2:
        compute_and_store_interpolate(*this, until, compute_value_2, out);
        break;

    case interpolate_type_qss3:
        compute_and_store_interpolate(*this, until, compute_value_3, out);
        break;

    default:
        compute_and_store_interpolate(*this, until, compute_value_0, out);
        break;
    }
}

void simulation_observation::compute_interpolate(const real        until,
                                                 ImVector<ImVec2>& out) noexcept
{
    const auto i_type = get_interpolate_type(type);

    switch (i_type) {
    case interpolate_type_qss1:
        compute_and_store_interpolate(*this, until, compute_value_1, out);
        break;

    case interpolate_type_qss2:
        compute_and_store_interpolate(*this, until, compute_value_2, out);
        break;

    case interpolate_type_qss3:
        compute_and_store_interpolate(*this, until, compute_value_3, out);
        break;

    default:
        compute_and_store_interpolate(*this, until, compute_value_0, out);
        break;
    }
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
    output.last_position.reset();
}

static inline void simulation_observation_run(simulation_observation&  output,
                                              const irt::observer&     obs,
                                              const irt::dynamics_type type,
                                              const irt::time /*tl*/,
                                              const irt::time t) noexcept
{
    // Store only one raw value for a time.
    while (!output.raw_ring_buffer.empty() &&
           output.raw_ring_buffer.back().t == t)
        output.raw_ring_buffer.pop_back();

    if (output.raw_ring_buffer.empty()) {
        output.raw_ring_buffer.force_emplace_enqueue(obs.msg, t);
    } else {
        const auto i_type   = get_interpolate_type(type);
        auto       old_tail = output.raw_ring_buffer.tail();
        output.raw_ring_buffer.force_emplace_enqueue(obs.msg, t);

        auto out = [&output](real value, time t) {
            output.linear_ring_buffer.force_emplace_enqueue(
              static_cast<float>(value), static_cast<float>(t));
        };

        switch (i_type) {
        case interpolate_type_qss1:
            compute_interpolate_step(
              *old_tail, t, output.time_step, compute_value_1, out);
            break;

        case interpolate_type_qss2:
            compute_interpolate_step(
              *old_tail, t, output.time_step, compute_value_2, out);
            break;

        case interpolate_type_qss3:
            compute_interpolate_step(
              *old_tail, t, output.time_step, compute_value_3, out);
            break;

        default:
            compute_interpolate_step(
              *old_tail, t, output.time_step, compute_value_0, out);
            break;
        }
    }
}

static inline void simulation_observation_finalize(
  [[maybe_unused]] simulation_observation&  output,
  [[maybe_unused]] const irt::observer&     obs,
  [[maybe_unused]] const irt::dynamics_type type,
  [[maybe_unused]] const irt::time          tl,
  [[maybe_unused]] const irt::time          t) noexcept
{
    simulation_observation_run(output, obs, type, tl, t);
}

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
            auto& obs =
              sim_ed.sim_obs.alloc(mdl_id, mdl->type, 4096, 4096 * 4096);
            auto obs_id = sim_ed.sim_obs.get_id(obs);

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

    return static_cast<float>(obs->linear_outputs[index].x);
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