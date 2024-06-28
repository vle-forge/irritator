// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "editor.hpp"
#include "application.hpp"
#include "internal.hpp"

#include <utility>

namespace irt {

void show_dynamics_inputs(simulation& /*sim*/, qss1_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_multiplier& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, qss1_sum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_sum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_sum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_wsum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_wsum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_wsum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_multiplier& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, qss2_sum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_sum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_sum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_wsum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_wsum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_wsum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_multiplier& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, qss3_sum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_sum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_sum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_wsum_2& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_wsum_3& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_wsum_4& dyn)
{
    ImGui::InputReal("value-0", &dyn.default_values[0]);
    ImGui::InputReal("value-1", &dyn.default_values[1]);
    ImGui::InputReal("value-2", &dyn.default_values[2]);
    ImGui::InputReal("value-3", &dyn.default_values[3]);

    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(simulation& /*sim*/, counter& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, queue& dyn)
{
    ImGui::InputReal("delay", &dyn.default_ta);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");
}

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 id,
                                 source::source_type& type) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept
{
    u64                 id   = src.id;
    source::source_type type = src.type;

    if (show_external_sources_combo(srcs, title, id, type)) {
        src.id   = id;
        src.type = type;
        return true;
    }

    return false;
}

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 i64&             integer_0,
                                 i64&             integer_1) noexcept
{
    debug::ensure(is_numeric_castable<u64>(integer_0));
    debug::ensure(std::cmp_less_equal(0, integer_1) &&
                  std::cmp_less_equal(integer_1, source::source_type_count));

    u64                 id   = static_cast<u64>(integer_0);
    source::source_type type = enum_cast<source::source_type>(integer_0);

    if (show_external_sources_combo(srcs, title, id, type)) {
        debug::ensure(is_numeric_castable<i64>(id));
        integer_0 = static_cast<i64>(id);
        integer_1 = ordinal(type);
        return true;
    }

    return false;
}

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 src_id,
                                 source::source_type& src_type) noexcept
{
    bool             is_changed = false;
    small_string<63> label("None");

    switch (src_type) {
    case source::source_type::binary_file: {
        const auto id    = enum_cast<binary_file_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.binary_file_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::binary_file),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::constant: {
        const auto id    = enum_cast<constant_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.constant_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::constant),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::random: {
        const auto id    = enum_cast<random_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.random_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::random),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::text_file: {
        const auto id    = enum_cast<text_file_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.text_file_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::text_file),
                   index,
                   es->name.c_str());
        }
    } break;
    default:
        unreachable();
    }

    if (ImGui::BeginCombo(title, label.c_str())) {
        {
            bool is_selected = src_type == source::source_type::constant;
            ImGui::Selectable("None", is_selected);
        }

        {
            constant_source* s = nullptr;
            while (srcs.constant_sources.next(s)) {
                const auto id    = srcs.constant_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::constant),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::constant &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::constant;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            binary_file_source* s = nullptr;
            while (srcs.binary_file_sources.next(s)) {
                const auto id    = srcs.binary_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::binary_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src_type == source::source_type::binary_file &&
                  src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::binary_file;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            random_source* s = nullptr;
            while (srcs.random_sources.next(s)) {
                const auto id    = srcs.random_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::random),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::random &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::random;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            text_file_source* s = nullptr;
            while (srcs.text_file_sources.next(s)) {
                const auto id    = srcs.text_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::text_file),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::text_file &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::text_file;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        ImGui::EndCombo();
    }

    return is_changed;
}

void show_dynamics_inputs(simulation& sim, dynamic_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the dynamic queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(sim.srcs, "time", dyn.default_source_ta);
}

void show_dynamics_inputs(simulation& sim, priority_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the priority queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(sim.srcs, "time", dyn.default_source_ta);
}

void show_dynamics_inputs(simulation& sim, generator& dyn)
{
    const char* menu[] = { "source", "external events" };

    auto combo_ta    = dyn.flags[generator::option::ta_use_source] ? 0 : 1;
    auto combo_value = dyn.flags[generator::option::value_use_source] ? 0 : 1;

    {
        auto ret = ImGui::Combo("ta", &combo_ta, menu, length(menu));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input ports.");

        if (ret)
            dyn.flags.set(generator::option::ta_use_source, combo_ta == 0);
    }

    {
        auto ret = ImGui::Combo("value", &combo_value, menu, length(menu));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input port.");

        if (ret)
            dyn.flags.set(generator::option::value_use_source,
                          combo_value == 0);
    }

    if (dyn.flags[generator::option::ta_use_source]) {
        auto stop_on_error = dyn.flags[generator::option::stop_on_error];

        show_external_sources_combo(sim.srcs, "time", dyn.default_source_ta);
        ImGui::InputReal("offset", &dyn.default_offset);
        if (ImGui::Checkbox("Stop on error", &stop_on_error))
            dyn.flags.set(generator::option::stop_on_error);
        ImGui::SameLine();
        HelpMarker(
          "Unchecked, the generator stops to send data if the source are "
          "empty or undefined. Checked, the simulation will stop.");
    }

    if (dyn.flags[generator::option::value_use_source])
        show_external_sources_combo(
          sim.srcs, "source", dyn.default_source_value);
}

void show_dynamics_inputs(simulation& /*sim*/, constant& dyn)
{
    static const char* type_names[] = { "constant",
                                        "incoming_component_all",
                                        "outcoming_component_all",
                                        "incoming_component_n",
                                        "outcoming_component_n" };

    ImGui::InputReal("value", &dyn.default_value);
    ImGui::InputReal("offset", &dyn.default_offset);

    int i = ordinal(dyn.type);
    if (ImGui::Combo("type", &i, type_names, length(type_names)))
        dyn.type = enum_cast<constant::init_type>(i);

    if (any_equal(dyn.type,
                  constant::init_type::incoming_component_n,
                  constant::init_type::outcoming_component_n)) {
        ImGui::InputScalar("port", ImGuiDataType_U64, &dyn.port);
    }
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_filter& dyn)
{
    ImGui::InputReal("lowe threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_filter& dyn)
{
    ImGui::InputReal("lower threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_filter& dyn)
{
    ImGui::InputReal("lower threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(simulation& /*sim*/, qss2_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(simulation& /*sim*/, qss3_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(simulation& /*sim*/, qss1_square& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, qss2_square& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, qss3_square& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, accumulator_2& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, logical_and_2& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, logical_or_2& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
}

void show_dynamics_inputs(simulation& /*sim*/, logical_and_3& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
    ImGui::Checkbox("value 3", &dyn.default_values[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, logical_or_3& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
    ImGui::Checkbox("value 3", &dyn.default_values[2]);
}

void show_dynamics_inputs(simulation& /*sim*/, logical_invert& /*dyn*/) {}

void show_dynamics_inputs(simulation& /*sim*/, hsm_wrapper& dyn)
{
    ImGui::InputInt("integer 1", &dyn.exec.i1);
    ImGui::InputInt("integer 2", &dyn.exec.i2);
    ImGui::InputDouble("real 1", &dyn.exec.r1);
    ImGui::InputDouble("real 2", &dyn.exec.r2);
    ImGui::InputDouble("timer", &dyn.exec.sigma);
}

void show_dynamics_inputs(simulation& /*sim*/, time_func& dyn)
{
    static const char* items[] = { "time", "square", "sin" };

    ImGui::PushItemWidth(120.0f);
    int item_current = dyn.default_f == &time_function          ? 0
                       : dyn.default_f == &square_time_function ? 1
                                                                : 2;

    if (ImGui::Combo("function", &item_current, items, IM_ARRAYSIZE(items))) {
        dyn.default_f = item_current == 0   ? &time_function
                        : item_current == 1 ? &square_time_function
                                            : sin_time_function;
    }
    ImGui::PopItemWidth();
}

//
//
//

static bool show_parameter_editor(application& /*app*/,
                                  qss1_integrator& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value", &p.reals[0]) ||
           ImGui::InputReal("dQ", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_integrator& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value", &p.reals[0]) ||
           ImGui::InputReal("dQ", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_integrator& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value", &p.reals[0]) ||
           ImGui::InputReal("dQ", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_multiplier& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_sum_2& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_sum_3& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_sum_4& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("value-4", &p.reals[3]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_wsum_2& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("coeff-1", &p.reals[2]) ||
           ImGui::InputReal("coeff-2", &p.reals[3]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_wsum_3& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("coeff-1", &p.reals[3]) ||
           ImGui::InputReal("coeff-2", &p.reals[4]) ||
           ImGui::InputReal("coeff-3", &p.reals[5]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_wsum_4& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("value-4", &p.reals[3]) ||
           ImGui::InputReal("coeff-1", &p.reals[4]) ||
           ImGui::InputReal("coeff-2", &p.reals[5]) ||
           ImGui::InputReal("coeff-3", &p.reals[6]) ||
           ImGui::InputReal("coeff-4", &p.reals[7]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_multiplier& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_sum_2& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_sum_3& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_sum_4& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_wsum_2& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("coeff-1", &p.reals[2]) ||
           ImGui::InputReal("coeff-2", &p.reals[3]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_wsum_3& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("coeff-1", &p.reals[3]) ||
           ImGui::InputReal("coeff-2", &p.reals[4]) ||
           ImGui::InputReal("coeff-3", &p.reals[5]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_wsum_4& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("value-4", &p.reals[3]) ||
           ImGui::InputReal("coeff-1", &p.reals[4]) ||
           ImGui::InputReal("coeff-2", &p.reals[5]) ||
           ImGui::InputReal("coeff-3", &p.reals[6]) ||
           ImGui::InputReal("coeff-4", &p.reals[7]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_multiplier& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_sum_2& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_sum_3& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_sum_4& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_wsum_2& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("coeff-1", &p.reals[2]) ||
           ImGui::InputReal("coeff-2", &p.reals[3]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_wsum_3& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("coeff-1", &p.reals[3]) ||
           ImGui::InputReal("coeff-2", &p.reals[4]) ||
           ImGui::InputReal("coeff-3", &p.reals[5]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_wsum_4& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("value-1", &p.reals[0]) ||
           ImGui::InputReal("value-2", &p.reals[1]) ||
           ImGui::InputReal("value-3", &p.reals[2]) ||
           ImGui::InputReal("value-4", &p.reals[3]) ||
           ImGui::InputReal("coeff-1", &p.reals[4]) ||
           ImGui::InputReal("coeff-2", &p.reals[5]) ||
           ImGui::InputReal("coeff-3", &p.reals[6]) ||
           ImGui::InputReal("coeff-4", &p.reals[7]);
}

static bool show_parameter_editor(application& /*app*/,
                                  counter& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  queue& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("delay", &p.reals[0]);
}

static bool show_parameter_editor(application& app,
                                  dynamic_queue& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value      = p.integers[0] != 0;

    if (ImGui::Checkbox("Stop on error", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    if (show_external_sources_combo(
          app.mod.srcs, "time", p.integers[1], p.integers[2])) {
        is_changed = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& app,
                                  priority_queue& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value      = p.integers[0] != 0;

    if (ImGui::Checkbox("Stop on error", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    if (show_external_sources_combo(
          app.mod.srcs, "time", p.integers[1], p.integers[2])) {
        is_changed = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& app,
                                  generator& /* dyn */,
                                  parameter& p) noexcept
{
    static const char* items[] = { "source", "external events" };

    auto flags      = bitflags<generator::option>(p.integers[0]);
    auto is_changed = false;

    auto combo_ta    = flags[generator::option::ta_use_source] ? 0 : 1;
    auto combo_value = flags[generator::option::value_use_source] ? 0 : 1;

    {
        auto ret = ImGui::Combo("ta", &combo_ta, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input ports.");

        if (ret) {
            flags.set(generator::option::ta_use_source, combo_ta == 0);
            is_changed = true;
        }
    }

    {
        auto ret = ImGui::Combo("value", &combo_value, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input port.");

        if (ret) {
            flags.set(generator::option::value_use_source, combo_value == 0);
            is_changed = true;
        }
    }

    if (is_changed)
        p.integers[0] = flags.to_unsigned();

    if (flags[generator::option::ta_use_source]) {
        auto stop_on_error = flags[generator::option::stop_on_error];

        if (show_external_sources_combo(
              app.mod.srcs, "time", p.integers[1], p.integers[2]))
            is_changed = true;

        if (ImGui::InputReal("offset", &p.reals[0])) {
            p.reals[0] = p.reals[0] < 0.0 ? 0.0 : p.reals[0];
            is_changed = true;
        }

        if (ImGui::Checkbox("Stop on error", &stop_on_error)) {
            flags.set(generator::option::stop_on_error, stop_on_error);
            is_changed = true;
        }

        ImGui::SameLine();
        HelpMarker(
          "Unchecked, the generator stops to send data if the source are "
          "empty or undefined. Checked, the simulation will stop.");
    }

    if (flags[generator::option::value_use_source])
        if (show_external_sources_combo(
              app.mod.srcs, "source", p.integers[3], p.integers[4]))
            is_changed = true;

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  constant& /*dyn*/,
                                  parameter& p) noexcept
{
    static const char* type_names[] = { "constant",
                                        "incoming_component_all",
                                        "outcoming_component_all",
                                        "incoming_component_n",
                                        "outcoming_component_n" };

    bool is_changed = false;
    debug::ensure(
      std::cmp_equal(std::size(type_names), constant::init_type_count));

    if (ImGui::InputReal("value", &p.reals[0]))
        is_changed = true;

    if (ImGui::InputReal("offset", &p.reals[1]))
        is_changed = true;

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], constant::init_type_count));

    int i = static_cast<int>(p.integers[0]);
    if (ImGui::Combo("type", &i, type_names, length(type_names))) {
        p.integers[0] = i;
        is_changed    = true;
    }

    if (i == ordinal(constant::init_type::incoming_component_n) ||
        i == ordinal(constant::init_type::outcoming_component_n)) {
        int port = static_cast<int>(p.integers[1]);
        if (ImGui::InputInt("port", &port)) {
            p.integers[1] = port < 0     ? 0
                            : port > 127 ? 127
                                         : static_cast<i8>(port);
            is_changed    = true;
        }
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_cross& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_cross& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_cross& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_filter& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("lowe threshold", &p.reals[0]) ||
           ImGui::InputReal("upper threshold", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_filter& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("lower threshold", &p.reals[0]) ||
           ImGui::InputReal("upper threshold", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_filter& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("lower threshold", &p.reals[0]) ||
           ImGui::InputReal("upper threshold", &p.reals[1]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_power& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_power& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_power& /*dyn*/,
                                  parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

static bool show_parameter_editor(application& /*app*/,
                                  qss1_square& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss2_square& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  qss3_square& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  accumulator_2& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  logical_and_2& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  logical_or_2& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  logical_and_3& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  logical_or_3& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

static bool show_parameter_editor(application& /*app*/,
                                  logical_invert& /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  hsm_wrapper /*dyn*/,
                                  parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter_editor(application& /*app*/,
                                  time_func& /*dyn*/,
                                  parameter& p) noexcept
{
    bool is_changed = false;

    static const char* items[] = { "time", "square", "sin" };

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], 3));

    auto value = static_cast<int>(p.integers[0]);

    ImGui::PushItemWidth(120.0f);

    if (ImGui::Combo("function", &value, items, IM_ARRAYSIZE(items))) {
        p.integers[0] = value;
        is_changed    = true;
    }

    ImGui::PopItemWidth();

    return is_changed;
}

bool show_parameter_editor(application& app, model& mdl, parameter& p) noexcept
{
    return dispatch(
      mdl, [&app, &p]<typename Dynamics>(Dynamics& dyn) noexcept -> bool {
          return show_parameter_editor(app, dyn, p);
      });
}

/////////////////////////////////////////////////////////////////////

bool show_parameter(dynamics_qss1_integrator_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss2_integrator_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss3_integrator_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_multiplier_tag,
                    application& /* app */,
                    parameter& /* p */) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss1_sum_2_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss1_sum_3_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss1_sum_4_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss1_wsum_2_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[3]);

    return ret;
}

bool show_parameter(dynamics_qss1_wsum_3_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[5]);

    return ret;
}

bool show_parameter(dynamics_qss1_wsum_4_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("value-3", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[5]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[6]);
    ret |= ImGui::InputReal("coeff-3", &p.reals[7]);

    return ret;
}

bool show_parameter(dynamics_qss2_multiplier_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_sum_2_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_sum_3_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_sum_4_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_wsum_2_tag,
                    application& /* app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[3]);

    return ret;
}

bool show_parameter(dynamics_qss2_wsum_3_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[5]);

    return ret;
}

bool show_parameter(dynamics_qss2_wsum_4_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("value-3", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[5]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[6]);
    ret |= ImGui::InputReal("coeff-3", &p.reals[7]);

    return ret;
}

bool show_parameter(dynamics_qss3_multiplier_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_sum_2_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_sum_3_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_sum_4_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_wsum_2_tag,
                    application& /* app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[3]);

    return ret;
}

bool show_parameter(dynamics_qss3_wsum_3_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[5]);

    return ret;
}

bool show_parameter(dynamics_qss3_wsum_4_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    auto ret = ImGui::InputReal("value-0", &p.reals[0]);
    ret |= ImGui::InputReal("value-1", &p.reals[1]);
    ret |= ImGui::InputReal("value-2", &p.reals[2]);
    ret |= ImGui::InputReal("value-3", &p.reals[3]);
    ret |= ImGui::InputReal("coeff-0", &p.reals[4]);
    ret |= ImGui::InputReal("coeff-1", &p.reals[5]);
    ret |= ImGui::InputReal("coeff-2", &p.reals[6]);
    ret |= ImGui::InputReal("coeff-3", &p.reals[7]);

    return ret;
}

bool show_parameter(dynamics_counter_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_queue_tag,
                    application& /* app */,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("delay", &p.reals[0]);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");

    return b1;
}

bool show_parameter(dynamics_dynamic_queue_tag,
                    application& app,
                    parameter&   p) noexcept
{
    auto       stop_on_error = p.integers[0] != 0;
    const auto b1            = ImGui::Checkbox("Stop on error", &stop_on_error);
    if (b1)
        p.integers[0] = stop_on_error ? 1 : 0;

    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the dynamic queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    const auto b2 = show_external_sources_combo(
      app.sim.srcs, "time", p.integers[1], p.integers[2]);

    return b1 or b2;
}

bool show_parameter(dynamics_priority_queue_tag,
                    application& app,
                    parameter&   p) noexcept
{
    bool is_changed = false;
    bool value      = p.integers[0] != 0;

    if (ImGui::Checkbox("Stop on error", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    if (show_external_sources_combo(
          app.mod.srcs, "time", p.integers[1], p.integers[2])) {
        is_changed = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_generator_tag,
                    application& app,
                    parameter&   p) noexcept
{
    static const char* items[] = { "source", "external events" };

    auto flags      = bitflags<generator::option>(p.integers[0]);
    auto is_changed = false;

    auto combo_ta    = flags[generator::option::ta_use_source] ? 0 : 1;
    auto combo_value = flags[generator::option::value_use_source] ? 0 : 1;

    {
        auto ret = ImGui::Combo("ta", &combo_ta, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input ports.");

        if (ret) {
            flags.set(generator::option::ta_use_source, combo_ta == 0);
            is_changed = true;
        }
    }

    {
        auto ret = ImGui::Combo("value", &combo_value, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input port.");

        if (ret) {
            flags.set(generator::option::value_use_source, combo_value == 0);
            is_changed = true;
        }
    }

    if (is_changed)
        p.integers[0] = flags.to_unsigned();

    if (flags[generator::option::ta_use_source]) {
        auto stop_on_error = flags[generator::option::stop_on_error];

        if (show_external_sources_combo(
              app.mod.srcs, "time", p.integers[1], p.integers[2]))
            is_changed = true;

        if (ImGui::InputReal("offset", &p.reals[0])) {
            p.reals[0] = p.reals[0] < 0.0 ? 0.0 : p.reals[0];
            is_changed = true;
        }

        if (ImGui::Checkbox("Stop on error", &stop_on_error)) {
            flags.set(generator::option::stop_on_error, stop_on_error);
            is_changed = true;
        }

        ImGui::SameLine();
        HelpMarker(
          "Unchecked, the generator stops to send data if the source are "
          "empty or undefined. Checked, the simulation will stop.");
    }

    if (flags[generator::option::value_use_source])
        if (show_external_sources_combo(
              app.mod.srcs, "source", p.integers[3], p.integers[4]))
            is_changed = true;

    return is_changed;
}

bool show_parameter(dynamics_constant_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    static const char* type_names[] = { "constant",
                                        "incoming component all",
                                        "outcoming component all",
                                        "incoming component n",
                                        "outcoming component n" };

    bool is_changed = false;
    debug::ensure(
      std::cmp_equal(std::size(type_names), constant::init_type_count));

    if (ImGui::InputReal("value", &p.reals[0]))
        is_changed = true;

    if (ImGui::InputReal("offset", &p.reals[1]))
        is_changed = true;

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], constant::init_type_count));

    int i = static_cast<int>(p.integers[0]);
    if (ImGui::Combo("type", &i, type_names, length(type_names))) {
        p.integers[0] = i;
        is_changed    = true;
    }

    if (i == ordinal(constant::init_type::incoming_component_n) ||
        i == ordinal(constant::init_type::outcoming_component_n)) {
        int port = static_cast<int>(p.integers[1]);
        if (ImGui::SliderInt("port", &port, 0, 32)) {
            p.integers[1] = static_cast<i64>(port);
            is_changed    = true;
        }
    }

    return is_changed;
}

bool show_parameter(dynamics_qss1_cross_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss2_cross_tag,
                    application& /*app*/,
                    parameter& p) noexcept

{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss3_cross_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss1_filter_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss2_filter_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss3_filter_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_power_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss2_power_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss3_power_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss1_square_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_square_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_square_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_accumulator_2_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_time_func_tag,
                    application& /* app */,
                    parameter& p) noexcept
{
    static const char* items[] = { "time", "square", "sin" };

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], 3));

    bool is_changed = false;
    auto value      = static_cast<int>(p.integers[0]);

    ImGui::PushItemWidth(120.0f);
    if (ImGui::Combo("function", &value, items, IM_ARRAYSIZE(items))) {
        p.integers[0] = value;
        is_changed    = true;
    }
    ImGui::PopItemWidth();

    return is_changed;
}

bool show_parameter(dynamics_logical_and_2_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_or_2_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_and_3_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_or_3_tag,
                    application& /*app*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_invert_tag,
                    application& /*app*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

static auto get_current_component_name(application& app, parameter& p) noexcept
  -> const char*
{
    static constexpr auto undefined_name = "-";

    const auto* compo =
      app.mod.components.try_to_get(enum_cast<component_id>(p.integers[0]));
    if (not compo or compo->type != component_type::hsm) {
        p.integers[0] = 0;
        compo         = nullptr;
    }

    return compo ? compo->name.c_str() : undefined_name;
}

bool show_parameter(dynamics_hsm_wrapper_tag,
                    application& app,
                    parameter&   p) noexcept
{
    auto up = false;

    if (ImGui::BeginCombo("hsm component",
                          get_current_component_name(app, p))) {
        auto imgui_id = 0;

        ImGui::PushID(imgui_id++);
        if (ImGui::Selectable("-", p.integers[0] == 0)) {
            p.integers[0] = 0;
            up            = true;
        }
        ImGui::PopID();

        for (const auto& c : app.mod.components) {
            if (c.type == component_type::hsm) {
                ImGui::PushID(imgui_id++);
                const auto c_id =
                  static_cast<i64>(app.mod.components.get_id(c));
                if (ImGui::Selectable(c.name.c_str(), p.integers[0] == c_id)) {
                    p.integers[0] = c_id;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    up = ImGui::InputScalar("i1", ImGuiDataType_S64, &p.integers[1]) or up;
    up = ImGui::InputScalar("i2", ImGuiDataType_S64, &p.integers[2]) or up;
    up = ImGui::InputDouble("r1", &p.reals[0]) or up;
    up = ImGui::InputDouble("r2", &p.reals[1]) or up;
    up = ImGui::InputDouble("timer", &p.reals[2]) or up;

    return up;
}

bool show_parameter_editor(application&  app,
                           dynamics_type type,
                           parameter&    p) noexcept
{
    return dispatcher(
      type,
      [](auto tag, application& app, parameter& p) noexcept -> bool {
          return show_parameter(tag, app, p);
      },
      app,
      p);
}

///////////////////////////////////////////////////////////

auto get_dynamics_input_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "set-t", "add-t", "mult_t"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_input_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "nb-1", "nb-2"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_input_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

// Dynamics output names

auto get_dynamics_output_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_output_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_output_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "out-1", "out-2", "out-3", "out-4"
    };

    return std::span{ names.data(), names.size() };
}

} // irt
