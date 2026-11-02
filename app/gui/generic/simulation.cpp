// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <limits>

namespace irt {

const char* observable_type_names[] = {
    "none", "file", "plot", "graph", "grid",
};

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_integrator& dyn)
{
    ImGui::LabelFormat("X", "{}", dyn.X);
    ImGui::LabelFormat("dQ", "{}", dyn.dQ);
}

template<size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_flipflop<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_integrator& dyn)
{
    ImGui::LabelFormat("X", "{}", dyn.X);
    ImGui::LabelFormat("dQ", "{}", dyn.dQ);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_integrator& dyn)
{
    ImGui::LabelFormat("X", "{}", dyn.X);
    ImGui::LabelFormat("dQ", "{}", dyn.dQ);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_sum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_sum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_sum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
    ImGui::LabelFormat("value-4", "{}", dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_multiplier& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_wsum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_wsum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_wsum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
    ImGui::LabelFormat("value-4", "{}", dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_sum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_sum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[3]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[4]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_sum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[4]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[5]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[6]);
    ImGui::LabelFormat("value-4", "{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_multiplier& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_wsum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_wsum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[3]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[4]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_wsum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[4]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[5]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[6]);
    ImGui::LabelFormat("value-4", "{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_sum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_sum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[3]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[4]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_sum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[4]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[5]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[6]);
    ImGui::LabelFormat("value-4", "{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_multiplier& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_wsum_2& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[2]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_wsum_3& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[3]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[4]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_wsum_4& dyn)
{
    ImGui::LabelFormat("value-1", "{} {}", dyn.values[0], dyn.values[4]);
    ImGui::LabelFormat("value-2", "{} {}", dyn.values[1], dyn.values[5]);
    ImGui::LabelFormat("value-3", "{} {}", dyn.values[2], dyn.values[6]);
    ImGui::LabelFormat("value-4", "{} {}", dyn.values[3], dyn.values[7]);
}

template<sz QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_integer<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<sz QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_compare<QssLevel>& dyn)
{
    ImGui::LabelFormat("a", "{}", dyn.a[0]);
    ImGui::LabelFormat("b", "{}", dyn.b[0]);
    ImGui::LabelFormat("a < b", "{}", dyn.output[0]);
    ImGui::LabelFormat("not a < b", "{}", dyn.output[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const counter& dyn)
{
    ImGui::LabelFormat("event-number", "{}", dyn.event_number);
    ImGui::LabelFormat("last-value", "{}", dyn.last_value);
    ImGui::LabelFormat("sum-values", "{}", dyn.sum_values);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& sim,
                                 const queue&    dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::LabelFormat("queue", "{}", "empty");
    } else {
        ImGui::LabelFormat("next ta", "{}", ar->front()[0]);
        ImGui::LabelFormat("next value", "{}", ar->front()[1]);
    }
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor&      sim,
                                 const dynamic_queue& dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::LabelFormat("queue", "{}", "empty");
    } else {
        ImGui::LabelFormat("next ta", "{}", ar->front()[0]);
        ImGui::LabelFormat("next value", "{}", ar->front()[1]);
    }
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor&       sim,
                                 const priority_queue& dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::LabelFormat("queue", "{}", "empty");
    } else {
        ImGui::LabelFormat("next ta", "{}", ar->front()[0]);
        ImGui::LabelFormat("next value", "{}", ar->front()[1]);
    }
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const generator& dyn)
{
    ImGui::LabelFormat("next", "{}", dyn.sigma);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& sim,
                                 constant&       dyn)
{
    ImGui::LabelFormat("next ta", "{}", dyn.sigma);
    ImGui::InputDouble("value", &dyn.value);

    if (ImGui::Button("Send value now")) {
        auto& mdl    = get_model(dyn);
        auto  mdl_id = sim.pj.sim.models.get_id(mdl);

        sim.pj.push(command{ .type = command_type::send_message,
                             .data{ .send_message{ .mdl_id = mdl_id } } });
    }
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_cross<QssLevel>& dyn)
{
    ImGui::LabelFormat("threshold", "{}", dyn.threshold);
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel, bool IsMax>
static void show_dynamics_values(
  application& /*app*/,
  project_editor& /*sim*/,
  const struct abstract_min_max_hold<QssLevel, IsMax>& dyn)
{
    ImGui::LabelFormat("extremum", "{}", dyn.extremum);
}

template<std::size_t QssLevel, bool IsMax>
static void show_dynamics_values(
  application& /*app*/,
  project_editor& /*sim*/,
  const struct abstract_min_max<QssLevel, IsMax>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.values[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_filter<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
    ImGui::LabelFormat("lower-threshold", "{}", dyn.lower_threshold);
    ImGui::LabelFormat("upper-threshold", "{}", dyn.upper_threshold);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_inverse<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_sin<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_cos<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_log<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const abstract_exp<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_power& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_power& dyn)
{
    ImGui::LabelFormat("value", "{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_power& dyn)
{
    ImGui::LabelFormat(
      "value", "{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_gain& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_gain& dyn)
{
    ImGui::LabelFormat("value", "{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_gain& dyn)
{
    ImGui::LabelFormat(
      "value", "{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss1_square& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss2_square& dyn)
{
    ImGui::LabelFormat("value", "{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const qss3_square& dyn)
{
    ImGui::LabelFormat(
      "value", "{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const accumulator_2& dyn)
{
    ImGui::LabelFormat("number", "{}", dyn.number);
    ImGui::LabelFormat("value-1", "{}", dyn.numbers[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.numbers[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const time_func& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const logical_and_2& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.is_valid);
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const logical_or_2& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.is_valid);
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const logical_and_3& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.is_valid);
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const logical_or_3& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.is_valid);
    ImGui::LabelFormat("value-1", "{}", dyn.values[0]);
    ImGui::LabelFormat("value-2", "{}", dyn.values[1]);
    ImGui::LabelFormat("value-3", "{}", dyn.values[2]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*sim*/,
                                 const logical_invert& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value);
}

static auto get_state_name(application&       app,
                           project_editor&    pj_ed,
                           const hsm_wrapper& dyn) noexcept -> std::string_view
{
    return app.mod.ids.read(
      [&](const auto& ids, const auto /*v*/) noexcept -> std::string_view {
          if (const auto* sim_hsm = pj_ed.pj.sim.hsms.try_to_get(dyn.id))
              if (const auto* mod_hsm = ids.hsm_components.try_to_get(
                    enum_cast<hsm_component_id>(sim_hsm->parent_id)))
                  return mod_hsm->names[dyn.exec.current_state].sv();

          return std::string_view();
      });
}

static void show_dynamics_values(application&       app,
                                 project_editor&    pj_ed,
                                 const hsm_wrapper& dyn)
{
    const auto name = get_state_name(app, pj_ed, dyn);

    if (name.empty())
        ImGui::LabelFormat("state", "{}", dyn.exec.current_state);
    else
        ImGui::LabelFormat("state", "{} ({})", name, dyn.exec.current_state);

    ImGui::LabelFormat("i1", "{}", dyn.exec.i1);
    ImGui::LabelFormat("i2", "{}", dyn.exec.i2);
    ImGui::LabelFormat("r1", "{}", dyn.exec.r1);
    ImGui::LabelFormat("r2", "{}", dyn.exec.r2);
    ImGui::LabelFormat("timer", "{}", dyn.exec.timer);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_sample_hold<QssLevel>& dyn)
{
    ImGui::LabelFormat("time-step", "{}", dyn.ts);
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const zero_order_hold& /*dyn*/)
{}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_quantizer<QssLevel>& dyn)
{
    ImGui::LabelFormat("level", "{}", dyn.level);
    ImGui::LabelFormat("q", "{}", dyn.q);
}

template<std::size_t QssLevel>
static void show_dynamics_values(
  application& /*app*/,
  project_editor& /*pj_ed*/,
  const abstract_integrate_and_fire<QssLevel>& dyn)
{
    ImGui::LabelFormat("acc", "{}", dyn.acc);
    ImGui::LabelFormat("fire", "{}", dyn.fire);
    ImGui::LabelFormat("theta", "{}", dyn.theta);
}

template<std::size_t QssLevel>
static void show_dynamics_values(
  application& /*app*/,
  project_editor& /*pj_ed*/,
  const abstract_threshold_crossing<QssLevel>& dyn)
{
    ImGui::LabelFormat("threshold", "{}", dyn.threshold);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_saturation<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_dead_zone<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_hysteresis<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_wrap<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_pwm<QssLevel>& dyn)
{
    ImGui::LabelFormat("amplitude", "{}", dyn.amplitude);
    ImGui::LabelFormat("period", "{}", dyn.period);
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const simulation_wrapper& dyn)
{
    ImGui::LabelFormat("run-type", "{}", ordinal(dyn.run));
    ImGui::LabelFormat("id", "{}", ordinal(dyn.sim_id));
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_sqrt<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_atan<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_tan<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_tanh<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_sigmoid<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_division<QssLevel>& dyn)
{
    ImGui::LabelFormat("value 0", "{}", dyn.values[0]);
    ImGui::LabelFormat("value 1", "{}", dyn.values[1]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_atan2<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.values[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_abs<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

template<std::size_t QssLevel>
static void show_dynamics_values(application& /*app*/,
                                 project_editor& /*pj_ed*/,
                                 const abstract_sign<QssLevel>& dyn)
{
    ImGui::LabelFormat("value", "{}", dyn.value[0]);
}

/// Maps the index of a model in the simulation to its index in @c nodes, to
/// build the links without searching the nodes.
static void build_node_indices(
  const simulation&                                sim,
  std::span<const generic_simulation_editor::node> nodes,
  vector<u32>&                                     indices) noexcept
{
    indices.clear();
    indices.resize(sim.models.capacity(), std::numeric_limits<u32>::max());

    for (u32 i = 0, e = static_cast<u32>(nodes.size()); i != e; ++i)
        indices[get_index(nodes[i].mdl)] = i;
}

static void build_model_links(
  const simulation&                        sim,
  const model&                             mdl,
  std::span<const u32>                     indices,
  vector<generic_simulation_editor::link>& links) noexcept
{
    dispatch(mdl, [&]<typename Dynamics>(const Dynamics& dyn) noexcept {
        if constexpr (has_output_port<Dynamics>) {
            const auto src = indices[get_index(sim.get_id(dyn))];

            for (int i = 0, e = length(dyn.y); i != e; ++i) {
                sim.for_each(dyn.y[i], [&](auto& dst, auto dst_port) {
                    const auto dst_idx = indices[get_index(sim.get_id(dst))];

                    if (src != std::numeric_limits<u32>::max() and
                        dst_idx != std::numeric_limits<u32>::max())
                        links.push_back(generic_simulation_editor::link{
                          .out_node = src,
                          .out_port = static_cast<u32>(i),
                          .in_node  = dst_idx,
                          .in_port  = static_cast<u32>(dst_port) });
                });
            }
        }
    });
}

static void build_links(const simulation&                                sim,
                        std::span<const generic_simulation_editor::node> nodes,
                        vector<generic_simulation_editor::link>& links) noexcept
{
    vector<u32> indices;
    build_node_indices(sim, nodes, indices);

    for (const auto& n : nodes)
        if (const auto* mdl = sim.models.try_to_get(n.mdl))
            build_model_links(sim, *mdl,
                              std::span(indices.data(), indices.size()), links);
}

static void build_nodes(const simulation&                        sim,
                        const tree_node&                         tn,
                        vector<generic_simulation_editor::node>& nodes) noexcept
{
    constexpr std::string_view empty_name;

    for (const auto& child : tn.children) {
        if (child.type == tree_node::child_node::type::model) {
            if (sim.models.try_to_get(child.mdl)) {
                const auto* name = tn.model_id_to_unique_id.get(child.mdl);

                nodes.push_back(generic_simulation_editor::node{
                  .mdl = child.mdl, .name = name ? name->sv() : empty_name });
            }
        }
    }
}

static void build_flat_nodes(
  const project_editor&                    pj_ed,
  const simulation&                        sim,
  vector<generic_simulation_editor::node>& nodes) noexcept
{
    nodes.clear();
    nodes.reserve(sim.models.ssize());

    if (const auto* head = pj_ed.pj.tree_nodes.try_to_get(pj_ed.pj.tn_head())) {
        vector<const tree_node*> stack(max_component_stack_size, reserve_tag);
        stack.push_back(head);

        while (not stack.empty()) {
            const auto* top = stack.back();
            stack.pop_back();

            if (const auto* child = top->tree.get_child())
                stack.push_back(child);

            if (const auto* sibling = top->tree.get_sibling())
                stack.push_back(sibling);

            build_nodes(sim, *top, nodes);
        }
    }
}

static int push_command(project_editor& pj_ed, const command& cmd) noexcept
{
    if (pj_ed.pj.push(cmd))
        return 1;

    log(log_level::error, [](auto& msg) noexcept {
        msg = "simulation: commands order list is full";
    });

    return 0;
}

static int copy(project_editor&                                pj_ed,
                const vector<generic_simulation_editor::node>& nodes,
                const tree_node_id                             current,
                std::span<const u32> selection) noexcept
{
    int ret = 0;

    for (const auto index : selection)
        if (pj_ed.pj.sim.models.try_to_get(nodes[index].mdl))
            ret += push_command(
              pj_ed,
              command{ .type = command_type::copy_model,
                       .data{ .copy_model{ .tn_id  = current,
                                           .mdl_id = nodes[index].mdl } } });

    return ret;
}

static int new_model(project_editor&     pj_ed,
                     const tree_node_id  current,
                     const dynamics_type type,
                     const ImVec2        click_pos) noexcept
{
    return push_command(pj_ed,
                        command{ .type = command_type::new_model,
                                 .data{ .new_model{ .tn_id = current,
                                                    .type  = type,
                                                    .x     = click_pos.x,
                                                    .y     = click_pos.y } } });
}

static int free_model(project_editor&                                pj_ed,
                      const vector<generic_simulation_editor::node>& nodes,
                      const tree_node_id                             current,
                      std::span<const u32> selection) noexcept
{
    int ret = 0;

    for (const auto index : selection)
        if (pj_ed.pj.sim.models.try_to_get(nodes[index].mdl))
            ret += push_command(
              pj_ed,
              command{ .type = command_type::free_model,
                       .data{ .free_model{ .tn_id  = current,
                                           .mdl_id = nodes[index].mdl } } });

    return ret;
}

static int connect(project_editor&                                pj_ed,
                   const vector<generic_simulation_editor::node>& nodes,
                   const tree_node_id                             current,
                   const node_editor::link_request&               req) noexcept
{
    auto* out = pj_ed.pj.sim.models.try_to_get(nodes[req.out.node].mdl);
    auto* in  = pj_ed.pj.sim.models.try_to_get(nodes[req.in.node].mdl);

    if (not(out and in and pj_ed.pj.sim.can_connect(1)))
        return 0;

    const auto port_out = static_cast<int>(req.out.port);
    const auto port_in  = static_cast<int>(req.in.port);

    if (not is_ports_compatible(*out, port_out, *in, port_in))
        return 0;

    return push_command(pj_ed,
                        command{ .type = command_type::new_connection,
                                 .data{ .new_connection{
                                   .tn_id      = current,
                                   .mdl_src_id = nodes[req.out.node].mdl,
                                   .mdl_dst_id = nodes[req.in.node].mdl,
                                   .port_src   = static_cast<i8>(port_out),
                                   .port_dst = static_cast<i8>(port_in) } } });
}

static int disconnect(project_editor&                                pj_ed,
                      const vector<generic_simulation_editor::node>& nodes,
                      const tree_node_id                             current,
                      const vector<generic_simulation_editor::link>& links,
                      std::span<const u32> selection) noexcept
{
    int ret = 0;

    for (const auto index : selection) {
        const auto& l = links[index];

        ret += push_command(
          pj_ed, command{ .type = command_type::free_connection,
                          .data{ .free_connection{
                            .tn_id      = current,
                            .mdl_src_id = nodes[l.out_node].mdl,
                            .mdl_dst_id = nodes[l.in_node].mdl,
                            .port_src   = static_cast<i8>(l.out_port),
                            .port_dst   = static_cast<i8>(l.in_port) } } });
    }

    return ret;
}

/// The models without position, new or never shown, take the position of the
/// last context menu or a place on a grid after the other nodes.
void generic_simulation_editor::place_new_nodes(
  const project_editor& pj_ed) noexcept
{
    const auto capacity = pj_ed.pj.sim.models.capacity();
    if (positions.size() < static_cast<sz>(capacity))
        positions.resize(capacity,
                         ImVec2(std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::quiet_NaN()));

    auto placed = 0;
    for (sz i = 0, e = nodes.size(); i != e; ++i) {
        auto& p = positions[get_index(nodes[i].mdl)];

        if (not std::isnan(p.x))
            continue;

        if (new_model_position.has_value()) {
            p = *new_model_position;
            new_model_position.reset();
        } else {
            p = node_editor::grid_position(
              static_cast<u32>(i),
              node_editor::grid_columns(static_cast<u32>(e)),
              ImVec2(grid_layout_x_distance, grid_layout_y_distance));
            ++placed;
        }
    }

    if (placed > 0)
        editor.request_fit();
}

static int popup_menu(project_editor&     pj_ed,
                      const tree_node_id  current,
                      const dynamics_type type,
                      const ImVec2        click_pos) noexcept
{
    if (ImGui::MenuItem(dynamics_type_names[ordinal(type)]))
        return new_model(pj_ed, current, type, click_pos);

    return false;
}

static int show_menu_edit(project_editor&    pj_ed,
                          const tree_node_id current,
                          const ImVec2       click_pos) noexcept
{
    int r = false;

    if (ImGui::BeginMenu("QSS1")) {
        auto       i = static_cast<int>(dynamics_type::qss1_integrator);
        const auto e = static_cast<int>(dynamics_type::qss1_compare) + 1;
        for (; i != e; ++i)
            r += popup_menu(
              pj_ed, current, static_cast<dynamics_type>(i), click_pos);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("QSS2")) {
        auto       i = static_cast<int>(dynamics_type::qss2_integrator);
        const auto e = static_cast<int>(dynamics_type::qss2_compare) + 1;

        for (; i != e; ++i)
            r += popup_menu(
              pj_ed, current, static_cast<dynamics_type>(i), click_pos);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("QSS3")) {
        auto       i = static_cast<int>(dynamics_type::qss3_integrator);
        const auto e = static_cast<int>(dynamics_type::qss3_compare) + 1;

        for (; i != e; ++i)
            r += popup_menu(
              pj_ed, current, static_cast<dynamics_type>(i), click_pos);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Logical")) {
        r +=
          popup_menu(pj_ed, current, dynamics_type::logical_and_2, click_pos);
        r += popup_menu(pj_ed, current, dynamics_type::logical_or_2, click_pos);
        r +=
          popup_menu(pj_ed, current, dynamics_type::logical_and_3, click_pos);
        r += popup_menu(pj_ed, current, dynamics_type::logical_or_3, click_pos);
        r +=
          popup_menu(pj_ed, current, dynamics_type::logical_invert, click_pos);
        ImGui::EndMenu();
    }

    r += popup_menu(pj_ed, current, dynamics_type::counter, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::queue, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::dynamic_queue, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::priority_queue, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::generator, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::constant, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::time_func, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::accumulator_2, click_pos);
    r += popup_menu(pj_ed, current, dynamics_type::hsm_wrapper, click_pos);

    return r;
}

int generic_simulation_editor::show_menu(project_editor& pj_ed,
                                         ImVec2          click_pos) noexcept
{
    const bool open_popup = ImGui::IsWindowFocused(
                              ImGuiFocusedFlags_RootAndChildWindows) and
                            editor.is_background_hovered() and
                            ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    int r = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::MenuItem("Grid layout"))
            editor.request_grid_layout();

        if (ImGui::MenuItem("Automatic layout"))
            editor.request_force_layout();

        ImGui::MenuItem("Show internal values", "", &show_internal_values);
        ImGui::MenuItem("Show internal parameters", "", &show_parameter_values);
        ImGui::MenuItem("Show identifiers", "", &show_identifiers);
        ImGui::MenuItem("Show minimap", "", &editor.show_minimap);

        ImGui::Separator();

        if (pj_ed.can_edit()) {
            const auto added = show_menu_edit(pj_ed, current, click_pos);
            if (added > 0)
                new_model_position = editor.to_world(click_pos);
            r += added;
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    return r;
}

/// The view of the models of the simulation for the @c node_editor.
struct generic_simulation_editor::graph {
    application&               app;
    project_editor&            pj_ed;
    generic_simulation_editor& ed;
    small_string<127>          title_buffer;

    model& mdl(const u32 node) noexcept
    {
        return pj_ed.pj.sim.models.get(ed.nodes[node].mdl);
    }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(ed.nodes.size());
    }

    ImVec2& position(const u32 node) noexcept
    {
        return ed.positions[get_index(ed.nodes[node].mdl)];
    }

    std::string_view title(const u32 node) noexcept
    {
        const auto type = dynamics_type_names[ordinal(mdl(node).type)];

        if (not ed.show_identifiers or ed.nodes[node].name.empty())
            return type;

        format(title_buffer, "{} ({})", ed.nodes[node].name.sv(), type);
        return title_buffer.sv();
    }

    u32 input_count(const u32 node) noexcept
    {
        return input_port_count(mdl(node));
    }

    u32 output_count(const u32 node) noexcept
    {
        return output_port_count(mdl(node));
    }

    std::string_view input_name(const u32 node, const u32 port) noexcept
    {
        return input_port_name(mdl(node), port);
    }

    std::string_view output_name(const u32 node, const u32 port) noexcept
    {
        return output_port_name(mdl(node), port);
    }

    void content(const u32 node) noexcept
    {
        auto& m = mdl(node);

        dispatch(m, [&]<typename Dynamics>(Dynamics& dyn) {
            if (ed.show_internal_values) {
                ImGui::PushID(0);
                ImGui::PushItemWidth(120.0f);
                show_dynamics_values(app, pj_ed, dyn);
                ImGui::PopItemWidth();
                ImGui::PopID();
            }

            if (ed.show_parameter_values and ed.can_edit_parameters) {
                ImGui::PushID(1);
                ImGui::PushItemWidth(120.0f);
                show_parameter_editor(
                  app, pj_ed.pj.sim.srcs, m.type,
                  pj_ed.pj.sim.parameters[get_index(ed.nodes[node].mdl)]);
                ImGui::PopItemWidth();
                ImGui::PopID();
            }
        });
    }

    template<typename Fn>
    void for_each_link(Fn&& fn) noexcept
    {
        for (const auto& l : ed.links)
            fn(l.out_node, l.out_port, l.in_node, l.in_port);
    }
};

void generic_simulation_editor::init(application&     app,
                                     project_editor&  pj_ed,
                                     const tree_node& tn,
                                     const component& /*compo*/,
                                     const generic_component& /*gen*/) noexcept
{
    enable_show = false;

    app.add_gui_task([&]() {
        nodes_2nd.clear();
        links_2nd.clear();

        build_nodes(pj_ed.pj.sim, tn, nodes_2nd);
        build_links(pj_ed.pj.sim, std::span(nodes_2nd.data(), nodes_2nd.size()),
                    links_2nd);

        if (std::unique_lock lock(mutex); lock.owns_lock()) {
            std::swap(links, links_2nd);
            std::swap(nodes, nodes_2nd);
            need_reset = true;
        }

        current     = pj_ed.pj.tree_nodes.get_id(tn);
        enable_show = true;
        rebuild_wip = false;
    });
}

void generic_simulation_editor::init(application&    app,
                                     project_editor& pj_ed) noexcept
{
    enable_show = false;

    app.add_gui_task([&]() {
        nodes_2nd.clear();
        links_2nd.clear();

        build_flat_nodes(pj_ed, pj_ed.pj.sim, nodes_2nd);
        build_links(pj_ed.pj.sim, std::span(nodes_2nd.data(), nodes_2nd.size()),
                    links_2nd);

        if (std::unique_lock lock(mutex); lock.owns_lock()) {
            std::swap(links, links_2nd);
            std::swap(nodes, nodes_2nd);
            need_reset = true;
        }

        current     = undefined<tree_node_id>();
        enable_show = true;
        rebuild_wip = false;
    });
}

void generic_simulation_editor::start_rebuild_task(
  application&    app,
  project_editor& pj_ed) noexcept
{
    app.add_gui_task([&]() {
        nodes_2nd.clear();
        links_2nd.clear();

        if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(current))
            build_nodes(pj_ed.pj.sim, *tn, nodes_2nd);
        else
            build_flat_nodes(pj_ed, pj_ed.pj.sim, nodes_2nd);

        build_links(pj_ed.pj.sim, std::span(nodes_2nd.data(), nodes_2nd.size()),
                    links_2nd);

        if (std::unique_lock lock(mutex); lock.owns_lock()) {
            std::swap(links, links_2nd);
            std::swap(nodes, nodes_2nd);
            need_reset = true;
        }

        enable_show = true;
        rebuild_wip = false;
    });
}

bool generic_simulation_editor::display(application&    app,
                                        project_editor& pj_ed) noexcept
{
    int changes = false;

    if (std::unique_lock lock(mutex, std::try_to_lock); lock.owns_lock()) {
        if (enable_show) {
            if (std::exchange(need_reset, false))
                editor.reset();

            place_new_nodes(pj_ed);

            auto g = graph{
                .app = app, .pj_ed = pj_ed, .ed = *this, .title_buffer = {}
            };
            const auto ev = editor.show("simulation-graph", g);

            changes += show_menu(pj_ed,
                                 ImGui::GetMousePosOnOpeningCurrentPopup());

            if (pj_ed.can_edit()) {
                if (ev.link_created)
                    changes += connect(pj_ed, nodes, current, *ev.link_created);

                if (ev.delete_selection) {
                    changes += free_model(pj_ed, nodes, current,
                                          editor.selected_nodes());
                    changes += disconnect(pj_ed, nodes, current, links,
                                          editor.selected_links());
                    editor.clear_selection();
                }

                if (ev.duplicate_selection) {
                    changes += copy(pj_ed, nodes, current,
                                    editor.selected_nodes());
                    editor.clear_selection();
                }
            }
        }
    }

    return changes;
}

} // namespace irt
