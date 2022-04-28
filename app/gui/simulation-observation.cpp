// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/io.hpp>

#include <cmath>

namespace irt {

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

void simulation_observation::save(
  const std::filesystem::path& file_path) noexcept
{
    try {
        auto raw_file = file_path / "raw.csv";
        if (auto ofs = std::ofstream{ raw_file }; ofs.is_open()) {
            auto it = raw_ring_buffer.head();
            auto et = raw_ring_buffer.end();

            switch (type) {
            case dynamics_type::qss1_integrator:
                ofs << "t,value\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;

            case dynamics_type::qss2_integrator:
                ofs << "t,value,value2\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << ',' << it->msg[1]
                        << '\n';
                break;

            case dynamics_type::qss3_integrator:
                ofs << "t,value,value2,value3\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << ',' << it->msg[1]
                        << ',' << it->msg[2] << '\n';
                break;
            }
        }

        auto linear_file = file_path / "interpolate.csv";
        if (auto ofs = std::ofstream{ raw_file }; ofs.is_open()) {
            auto it = raw_ring_buffer.head();
            auto et = raw_ring_buffer.end();

            switch (type) {
            case dynamics_type::qss1_integrator:
                ofs << "t,value\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;

            case dynamics_type::qss2_integrator:
                ofs << "t,value,value2\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;

            case dynamics_type::qss3_integrator:
                ofs << "t,value,value2,value3\n";
                for (; it != et; ++it)
                    ofs << it->t << ',' << it->msg[0] << '\n';
                break;
            }
        }
    } catch (const std::exception& /*e*/) {
    }
}
static real compute_value_0(const observation_message& msg,
                            const time                 elapsed) noexcept
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

template<typename Function>
static void compute_interpolate(
  simulation_observation&                        obs,
  const simulation_observation::raw_observation& raw,
  real                                           next,
  Function                                       f) noexcept
{
    const auto elapsed = next - raw.t;
    const auto nb      = static_cast<int>(elapsed / obs.time_step);
    auto       td      = raw.t;

    for (int i = 0; i < nb; ++i, td += obs.time_step) {
        const auto e     = td - raw.t;
        const auto value = f(raw.msg, e);
        obs.linear_ring_buffer.force_emplace_enqueue(value, td);
    }

    const auto e = next - td;
    if (e > 0) {
        const auto value = f(raw.msg, e);
        obs.linear_ring_buffer.force_emplace_enqueue(value, next);
    }
}

template<typename Function>
static void compute_n_interpolate(
  simulation_observation&                        obs,
  const simulation_observation::raw_observation& raw,
  real                                           next,
  Function                                       f) noexcept
{
    const auto elapsed = next - raw.t;
    const auto nb      = static_cast<int>(elapsed / obs.time_step);
    auto       td      = raw.t;

    while (!obs.plot_outputs.empty() && obs.plot_outputs.back().x == td)
        obs.plot_outputs.pop_back();

    for (int i = 0; i < nb; ++i, td += obs.time_step) {
        const auto e     = td - raw.t;
        const auto value = f(raw.msg, e);
        obs.plot_outputs.push_back(ImVec2{ (float)td, (float)value });
    }

    const auto value = f(raw.msg, elapsed);
    obs.plot_outputs.push_back(ImVec2{ (float)next, (float)value });
}

static auto get_min_it(auto head, auto tail, auto end, real min_time) noexcept
{
    for (; tail != end; --tail)
        if (tail->t < min_time)
            return tail;

    return head;
}

void simulation_observation::compute_complete_interpolate() noexcept
{
    auto it{ raw_ring_buffer.head() };
    auto et{ raw_ring_buffer.end() };

    switch (type) {
    case dynamics_type::qss1_integrator:
        for (; it != et; ++it) {
            auto next{ it };
            ++next;

            if (next != et)
                compute_n_interpolate(*this, *it, next->t, compute_value_1);
        }
        break;
    case dynamics_type::qss2_integrator:
        for (; it != et; ++it) {
            auto next{ it };
            ++next;

            if (next != et)
                compute_n_interpolate(*this, *it, next->t, compute_value_2);
        }
        break;
    case dynamics_type::qss3_integrator:
        for (int i = 0; it != et; ++it, ++i) {
            auto next{ it };
            ++next;

            if (next != et)
                compute_n_interpolate(*this, *it, next->t, compute_value_3);
        }

        break;
    default:
        for (; it != et; ++it) {
            auto next{ it };
            ++next;

            if (next != et)
                compute_n_interpolate(*this, *it, next->t, compute_value_0);
        }
        break;
    }
}

void simulation_observation::compute_linear_buffer(real t) noexcept
{
    auto head     = raw_ring_buffer.head();
    auto tail     = raw_ring_buffer.tail();
    auto et       = raw_ring_buffer.end();
    auto min_time = t - window;
    auto min_it   = get_min_it(head, tail, et, min_time);

    linear_ring_buffer.clear();

    switch (type) {
    case dynamics_type::qss1_integrator:
        for (; min_it != et; ++min_it) {
            auto next{ min_it };
            ++next;

            compute_interpolate(
              *this, *min_it, next == et ? t : next->t, compute_value_1);
        }
        break;
    case dynamics_type::qss2_integrator:
        for (; min_it != et; ++min_it) {
            auto next{ min_it };
            ++next;

            compute_interpolate(
              *this, *min_it, next == et ? t : next->t, compute_value_2);
        }
        break;
    case dynamics_type::qss3_integrator:
        for (int i = 0; min_it != et; ++min_it, ++i) {
            auto next{ min_it };
            ++next;

            compute_interpolate(
              *this, *min_it, next == et ? t : next->t, compute_value_3);
        }

        break;
    default:
        for (; min_it != et; ++min_it) {
            auto next{ min_it };
            ++next;

            compute_interpolate(
              *this, *min_it, next == et ? t : next->t, compute_value_0);
        }
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

static inline void simulation_observation_run(simulation_observation& output,
                                              const irt::observer&    obs,
                                              const irt::dynamics_type /*type*/,
                                              const irt::time /*tl*/,
                                              const irt::time t) noexcept
{
    // Store only one raw value for a time.

    while (!output.raw_ring_buffer.empty() &&
           output.raw_ring_buffer.back().t == t)
        output.raw_ring_buffer.pop_back();

    output.raw_ring_buffer.force_emplace_enqueue(obs.msg, t);
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