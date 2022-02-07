// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static inline void memory_output_emplace_1(memory_output&  out,
                                           const observer& obs,
                                           const time      td,
                                           const time      elapsed)
{
    const auto value = obs.msg[0] + obs.msg[1] * elapsed;

    out.xs.push_back(td);
    out.ys.push_back(value);
}

static inline void memory_output_emplace_2(memory_output&  out,
                                           const observer& obs,
                                           const time      td,
                                           const time      elapsed)
{
    const auto value = obs.msg[0] + obs.msg[1] * elapsed +
                       (obs.msg[2] * elapsed * elapsed / two);

    out.xs.push_back(td);
    out.ys.push_back(value);
}

static inline void memory_output_emplace_3(memory_output&  out,
                                           const observer& obs,
                                           const time      td,
                                           const time      elapsed)
{
    const auto value = obs.msg[0] + obs.msg[1] * elapsed +
                       (obs.msg[2] * elapsed * elapsed / two) +
                       (obs.msg[3] * elapsed * elapsed * elapsed / three);

    out.xs.push_back(td);
    out.ys.push_back(value);
}

static inline void memory_output_emplace(memory_output& out,
                                         real           x,
                                         real           y) noexcept
{
    if (out.xs.size() < out.xs.capacity()) {
        out.xs.push_back(x);
        out.ys.push_back(y);
    }
}

static inline void memory_output_initialize(memory_output& output,
                                            const irt::observer& /*obs*/,
                                            const irt::dynamics_type /*type*/,
                                            const irt::time /*tl*/,
                                            const irt::time /*t*/) noexcept
{
    output.xs.clear();
    output.ys.clear();
}

static inline void memory_output_run(memory_output&           output,
                                     const irt::observer&     obs,
                                     const irt::dynamics_type type,
                                     const irt::time          tl,
                                     const irt::time          t) noexcept
{
    // Store only one value for a time
    while (!output.xs.empty() && output.xs.back() == t) {
        output.xs.pop_back();
        output.ys.pop_back();
    }

    // @TODO Select algorithm for all QSS[1-3] model.
    // Here qss1_integrator, qss1_sum etc.

    if (output.ys.Size == output.ys.Capacity)
        return;

    if (output.interpolate) {
        const auto elapsed = t - tl;
        auto       nb      = static_cast<i32>(elapsed / output.time_step);
        nb = std::min(output.ys.Capacity - output.ys.Size - 1, nb);

        switch (type) {
        case irt::dynamics_type::qss1_integrator: {
            auto td = tl;

            for (i32 i = 0; i < nb; ++i) {
                memory_output_emplace_1(output, obs, td, output.time_step);
                td += output.time_step;
            }

            if (const auto e = t - td; e > 0) {
                memory_output_emplace_1(output, obs, t, e);
            }
        } break;

        case irt::dynamics_type::qss2_integrator: {
            auto td = tl;

            for (i32 i = 0; i < nb; ++i) {
                memory_output_emplace_2(output, obs, td, output.time_step);
                td += output.time_step;
            }

            if (const auto e = t - td; e > 0) {
                memory_output_emplace_2(output, obs, t, e);
            }
        } break;

        case irt::dynamics_type::qss3_integrator: {
            auto td = tl;

            for (i32 i = 0; i < nb; ++i) {
                memory_output_emplace_3(output, obs, td, output.time_step);
                td += output.time_step;
            }

            if (const auto e = t - td; e > 0) {
                memory_output_emplace_3(output, obs, t, e);
            }
        } break;

        default:
            memory_output_emplace(output, t, obs.msg[0]);
            break;
        }
    } else {
        memory_output_emplace(output, t, obs.msg[0]);
    }
}

static inline void memory_output_finalize(
  [[maybe_unused]] memory_output&           output,
  [[maybe_unused]] const irt::observer&     obs,
  [[maybe_unused]] const irt::dynamics_type type,
  [[maybe_unused]] const irt::time          tl,
  [[maybe_unused]] const irt::time          t) noexcept
{}

void memory_output_update(const irt::observer&        obs,
                          const irt::dynamics_type    type,
                          const irt::time             tl,
                          const irt::time             t,
                          const irt::observer::status s) noexcept
{
    auto* c_ed   = reinterpret_cast<component_editor*>(obs.user_data);
    auto  id     = enum_cast<memory_output_id>(obs.user_id);
    auto* output = c_ed->outputs.try_to_get(id);

    irt_assert(output);

    switch (s) {
    case observer::status::initialize:
        memory_output_initialize(*output, obs, type, tl, t);
        break;

    case observer::status::run:
        memory_output_run(*output, obs, type, tl, t);
        break;

    case observer::status::finalize:
        memory_output_finalize(*output, obs, type, tl, t);
        break;
    }
}

void application::show_simulation_window() noexcept
{
    if (ImGui::BeginTabBar("##Simulation")) {
        if (ImGui::BeginTabItem("Log")) {
            log_w.show();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data")) {
            show_external_sources();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace irt
