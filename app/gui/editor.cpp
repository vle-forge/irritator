// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "editor.hpp"
#include "internal.hpp"

namespace irt {

template<typename T>
    requires(std::is_integral_v<T>)
inline u8 to_u8(T value) noexcept
{
    if constexpr (std::is_signed_v<T>) {
        irt_assert(INT8_MIN <= value && value <= INT8_MAX);
        return static_cast<u8>(value);
    }

    irt_assert(value <= INT8_MAX);
    return static_cast<u8>(value);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("dQ", &dyn.default_dQ);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_multiplier& /*dyn*/)
{
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_sum_2& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_sum_3& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_sum_4& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_multiplier& /*dyn*/)
{
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_sum_2& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_sum_3& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_sum_4& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_multiplier& /*dyn*/)
{
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_sum_2& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_sum_3& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_sum_4& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(external_source& /*srcs*/, integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_current_value);
    ImGui::InputReal("reset", &dyn.default_reset_value);
}

void show_dynamics_inputs(external_source& /*srcs*/, quantifier& dyn)
{
    ImGui::InputReal("quantum", &dyn.default_step_size);
    ImGui::SliderInt("archive length", &dyn.default_past_length, 3, 100);
}

void show_dynamics_inputs(external_source& /*srcs*/, adder_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, adder_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, adder_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(external_source& /*srcs*/, mult_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, mult_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, mult_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

void show_dynamics_inputs(external_source& /*srcs*/, counter& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, queue& dyn)
{
    ImGui::InputReal("delay", &dyn.default_ta);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");
}

void show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src)
{
    small_string<63> label("None");

    external_source_type type;
    if (!(external_source_type_cast(src.type, &type))) {
        src.reset();
    } else {
        switch (type) {
        case external_source_type::binary_file: {
            const auto id    = enum_cast<binary_file_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.binary_file_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::binary_file),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::constant: {
            const auto id    = enum_cast<constant_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.constant_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::constant),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::random: {
            const auto id    = enum_cast<random_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.random_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::random),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::text_file: {
            const auto id    = enum_cast<text_file_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.text_file_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::text_file),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        default:
            irt_unreachable();
        }
    }

    if (ImGui::BeginCombo(title, label.c_str())) {
        {
            bool is_selected = src.type == -1;
            if (ImGui::Selectable("None", is_selected)) {
                src.reset();
            }
        }

        {
            constant_source* s = nullptr;
            while (srcs.constant_sources.next(s)) {
                const auto id    = srcs.constant_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::constant),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::constant) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::constant);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
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
                       ordinal(external_source_type::binary_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::binary_file) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::binary_file);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
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
                       ordinal(external_source_type::random),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::random) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::random);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
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
                       ordinal(external_source_type::text_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::text_file) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::text_file);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        ImGui::EndCombo();
    }
}

void show_dynamics_inputs(external_source& srcs, dynamic_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the dynamic queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(srcs, "time", dyn.default_source_ta);
}

void show_dynamics_inputs(external_source& srcs, priority_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the priority queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(srcs, "time", dyn.default_source_ta);
}

void show_dynamics_inputs(external_source& srcs, generator& dyn)
{
    ImGui::InputReal("offset", &dyn.default_offset);
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker("Unchecked, the generator stops to send data if the source are "
               "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(srcs, "source", dyn.default_source_value);
    show_external_sources_combo(srcs, "time", dyn.default_source_ta);
}

void show_dynamics_inputs(external_source& /*srcs*/, constant& dyn)
{
    ImGui::InputReal("value", &dyn.default_value);
    ImGui::InputReal("offset", &dyn.default_offset);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_filter& dyn)
{
    ImGui::InputReal("lowe threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_filter& dyn)
{
    ImGui::InputReal("lower threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_filter& dyn)
{
    ImGui::InputReal("lower threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

void show_dynamics_inputs(external_source& /*srcs*/, qss1_square& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss2_square& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, qss3_square& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
}

void show_dynamics_inputs(external_source& /*srcs*/, accumulator_2& /*dyn*/) {}

void show_dynamics_inputs(external_source& /*srcs*/, filter& dyn)
{
    ImGui::InputReal("lower threshold", &dyn.default_lower_threshold);
    ImGui::InputReal("upper threshold", &dyn.default_upper_threshold);
}

void show_dynamics_inputs(external_source& /*srcs*/, logical_and_2& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, logical_or_2& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
}

void show_dynamics_inputs(external_source& /*srcs*/, logical_and_3& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
    ImGui::Checkbox("value 3", &dyn.default_values[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, logical_or_3& dyn)
{
    ImGui::Checkbox("value 1", &dyn.default_values[0]);
    ImGui::Checkbox("value 2", &dyn.default_values[1]);
    ImGui::Checkbox("value 3", &dyn.default_values[2]);
}

void show_dynamics_inputs(external_source& /*srcs*/, logical_invert& /*dyn*/) {}

static const char* variable_names[] = { "none",       "port_0",  "port_1",
                                        "port_2",     "port_3",  "variable a",
                                        "variable b", "constant" };

static const char* action_names[] = {
    "none",        "set port",   "unset port", "reset ports",
    "output port", "x = y",      "x = x + y",  "x = x - y",
    "x = -x",      "x = x * y",  "x = x / y",  "x = x % y",
    "x = x and y", "x = x or y", "x = not x",  "x = x xor y"
};

static const char* condition_names[] = {
    "none",          "value on port", "a = constant",  "a != constant",
    "a > constant",  "a < constant",  "a >= constant", "a <= constant",
    "b = constant",  "b != constant", "b > constant",  "b < constant",
    "b >= constant", "b <= constant", "a = b",         "a != b",
    "a > b",         "a < b",         "a >= b",        "a <= b",
};

static void show_only_variable_widget(
  hierarchical_state_machine::variable& act) noexcept
{
    ImGui::PushID(&act);
    int var = static_cast<int>(act) - 5;
    if (!(0 <= var && var < 2))
        var = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &var, variable_names + 5, 2)) {
        irt_assert(0 <= var && var < 2);
        act = enum_cast<hierarchical_state_machine::variable>(var + 5);
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_variable_widget(hierarchical_state_machine::variable& act,
                                 i32& parameter) noexcept
{
    ImGui::PushID(&act);
    int var = static_cast<int>(act) - 5;
    if (!(0 <= var && var < 3))
        var = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &var, variable_names + 5, 3)) {
        irt_assert(0 <= var && var < 3);
        act = enum_cast<hierarchical_state_machine::variable>(var + 5);
    }

    ImGui::TableNextColumn();
    if (act == hierarchical_state_machine::variable::constant) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &parameter);
        ImGui::PopItemWidth();
    }

    ImGui::TableNextColumn();

    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_port_widget(hierarchical_state_machine::variable& var) noexcept
{
    ImGui::PushID(&var);
    int port = static_cast<int>(var) - 1;
    if (!(0 <= port && port < 4))
        port = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##port", &port, variable_names + 1, 4)) {
        irt_assert(0 <= port && port <= 3);
        var = enum_cast<hierarchical_state_machine::variable>(port + 1);
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_ports(u8& value, u8& mask) noexcept
{
    ImGui::PushID(static_cast<void*>(&value));

    const int sub_value_3 = value & 0b0001 ? 1 : 0;
    const int sub_value_2 = value & 0b0010 ? 1 : 0;
    const int sub_value_1 = value & 0b0100 ? 1 : 0;
    const int sub_value_0 = value & 0b1000 ? 1 : 0;

    int value_3 = mask & 0b0001 ? sub_value_3 : -1;
    int value_2 = mask & 0b0010 ? sub_value_2 : -1;
    int value_1 = mask & 0b0100 ? sub_value_1 : -1;
    int value_0 = mask & 0b1000 ? sub_value_0 : -1;

    bool have_changed = false;

    have_changed = ImGui::CheckBoxTristate("0", &value_0);
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("1", &value_1) || have_changed;
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("2", &value_2) || have_changed;
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("3", &value_3) || have_changed;

    if (have_changed) {
        value = value_0 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_1 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_2 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_3 == 1 ? 1u : 0u;

        mask = value_0 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_1 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_2 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_3 != -1 ? 1u : 0u;
    }

    ImGui::PopID();
}

static void show_state_action(
  hierarchical_state_machine::state_action& action) noexcept
{
    using hsm = hierarchical_state_machine;

    ImGui::PushID(static_cast<void*>(&action));

    int action_type = static_cast<int>(action.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &action_type, action_names, length(action_names)))
        action.type = enum_cast<hsm::action_type>(action_type);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();

    switch (action.type) {
    case hsm::action_type::none:
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        break;
    case hsm::action_type::set:
        show_port_widget(action.var1);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        break;
    case hsm::action_type::unset:
        show_port_widget(action.var1);
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        break;
    case hsm::action_type::reset:
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        break;
    case hsm::action_type::output:
        show_port_widget(action.var1);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        break;
    case hsm::action_type::affect:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::plus:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::minus:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::negate:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::multiplies:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::divides:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::modulus:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::bit_and:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::bit_or:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::bit_not:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm::action_type::bit_xor:
        show_only_variable_widget(action.var1);
        ImGui::TableNextColumn();
        show_variable_widget(action.var2, action.parameter);
        break;

    default:
        break;
    }

    ImGui::PopID();
}

static void show_state_condition(
  hierarchical_state_machine::condition_action& condition) noexcept
{
    using hsm = hierarchical_state_machine;

    ImGui::PushID(&condition);

    int type = static_cast<int>(condition.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &type, condition_names, length(condition_names)))
        irt_assert(0 <= type && type < hsm::condition_type_count);
    condition.type = enum_cast<hsm::condition_type>(type);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();

    switch (condition.type) {
    case hsm::condition_type::none:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::port:
        show_ports(condition.port, condition.mask);
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_not_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_greater_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_less_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_greater_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_less_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_not_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_greater_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_less_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_greater_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::b_less_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_equal_to_b:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_not_equal_to_b:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_greater_b:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_less_b:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_greater_equal_b:
        ImGui::TableNextColumn();
        break;
    case hsm::condition_type::a_less_equal_b:
        ImGui::TableNextColumn();
        break;
    }
    ImGui::PopID();
}

static void show_state_id_editor(
  hierarchical_state_machine::state_id& current) noexcept
{
    using hsm = hierarchical_state_machine;

    ImGui::PushID(&current);

    small_string<7> preview_value("-");
    if (current != hsm::invalid_state_id)
        format(preview_value, "{}", current);

    ImGui::PushItemWidth(-1);
    if (ImGui::BeginCombo("##transition", preview_value.c_str())) {
        if (ImGui::Selectable("-", current == hsm::invalid_state_id))
            current = hsm::invalid_state_id;

        for (u8 i = 0, e = hsm::max_number_of_state; i < e; ++i) {
            format(preview_value, "{}", i);
            if (ImGui::Selectable(preview_value.c_str(), i == current))
                current = i;
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_hsm_state(hierarchical_state_machine& machine, int i) noexcept
{
    auto& state = machine.states[i];

    ImGui::TableNextColumn();
    ImGui::Text("%d", i);
    ImGui::TableNextColumn();

    if (i != 0)
        show_state_id_editor(state.super_id);
    else
        ImGui::Text("start");

    ImGui::TableNextColumn();
    show_state_id_editor(state.sub_id);
    ImGui::TableNextColumn();

    constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("nested", 8, flags)) {
        ImGui::TableSetupColumn(
          "event", ImGuiTableColumnFlags_WidthFixed, 55.f);
        ImGui::TableSetupColumn(
          "condition", ImGuiTableColumnFlags_WidthFixed, 130.f);
        ImGui::TableSetupColumn(
          "parameter##1", ImGuiTableColumnFlags_WidthFixed, 140.f);
        ImGui::TableSetupColumn(
          "action", ImGuiTableColumnFlags_WidthFixed, 105.f);
        ImGui::TableSetupColumn(
          "parameter##2", ImGuiTableColumnFlags_WidthFixed, 110.f);
        ImGui::TableSetupColumn(
          "parameter##3", ImGuiTableColumnFlags_WidthFixed, 105.f);
        ImGui::TableSetupColumn(
          "parameter##4", ImGuiTableColumnFlags_WidthFixed, 90.f);
        ImGui::TableSetupColumn(
          "transition", ImGuiTableColumnFlags_WidthFixed, 90.f);
        ImGui::TableHeadersRow();

        ImGui::PushID("enter-event");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("enter");
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        show_state_action(state.enter_action);
        ImGui::TableNextColumn();
        ImGui::PopID();

        ImGui::TableNextRow();
        ImGui::PushID("if-condition");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("if");
        ImGui::TableNextColumn();
        show_state_condition(state.condition);
        show_state_action(state.if_action);
        show_state_id_editor(state.if_transition);
        ImGui::PopID();

        ImGui::TableNextRow();
        ImGui::PushID("else-condition");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("else");
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        show_state_action(state.else_action);
        show_state_id_editor(state.else_transition);
        ImGui::PopID();

        ImGui::TableNextRow();
        ImGui::PushID("exit-event");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("exit");
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        show_state_action(state.exit_action);
        ImGui::TableNextColumn();
        ImGui::PopID();

        ImGui::EndTable();
    }
}

static void show_hsm_inputs(hierarchical_state_machine& machine)
{
    bool start_valid =
      machine.m_top_state != hierarchical_state_machine::invalid_state_id;

    if (!start_valid)
        ImGui::Text("Top step undefined");

    constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable; /* |
                                 ImGuiTableFlags_SizingStretchSame*/
    ;

    if (ImGui::BeginTable("states", 4, flags)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn(
          "super-id", ImGuiTableColumnFlags_WidthFixed, 64.f);
        ImGui::TableSetupColumn(
          "sub-id", ImGuiTableColumnFlags_WidthFixed, 64.f);
        ImGui::TableSetupColumn(
          "state", ImGuiTableColumnFlags_WidthStretch, 850.f);
        ImGui::TableHeadersRow();

        for (int i = 0; i != length(machine.states); ++i) {
            ImGui::PushID(i);
            show_hsm_state(machine, i);
            ImGui::PopID();
            ImGui::TableNextRow(ImGuiTableRowFlags_None);
        }
        ImGui::EndTable();
    }
}

void show_dynamics_inputs(external_source& /*srcs*/,
                          hsm_wrapper& /*dyn*/,
                          hierarchical_state_machine& machine)
{
    hierarchical_state_machine copy{ machine };

    ImGui::Text("current state: %d",
                static_cast<int>(machine.get_current_state()));

    if (ImGui::Button("Edit"))
        ImGui::OpenPopup("Edit HSM");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Edit HSM", nullptr)) {
        ImGui::BeginChild("Child windows",
                          ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        show_hsm_inputs(machine);
        ImGui::EndChild();

        if (ImGui::Button("OK", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            machine = copy;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void show_dynamics_inputs(external_source& /*srcs*/, time_func& dyn)
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

} // irt
