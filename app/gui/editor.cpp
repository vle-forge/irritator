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

static void show_ports(u8* value) noexcept
{
    bool sub_value_0 = (*value) & 0b0001;
    bool sub_value_1 = (*value) & 0b0010;
    bool sub_value_2 = (*value) & 0b0100;
    bool sub_value_3 = (*value) & 0b1000;

    ImGui::PushID(static_cast<void*>(value));
    ImGui::Checkbox("0", &sub_value_0);
    ImGui::SameLine();
    ImGui::Checkbox("1", &sub_value_1);
    ImGui::SameLine();
    ImGui::Checkbox("2", &sub_value_2);
    ImGui::SameLine();
    ImGui::Checkbox("3", &sub_value_3);
    ImGui::PopID();

    *value = static_cast<u8>((static_cast<unsigned>(sub_value_3) << 3) |
                             (static_cast<unsigned>(sub_value_2) << 2) |
                             (static_cast<unsigned>(sub_value_1) << 1) |
                             static_cast<unsigned>(sub_value_0));
}

static void show_state_action(
  hierarchical_state_machine::state_action& action) noexcept
{
    static const char* action_names[] = {
        "none", "set", "unset", "reset", "output"
    };

    static const char* action_input_port[] = {
        "port 0", "port 1", "port 2", "port 3"
    };

    static const char* action_output_port[] = { "port 0", "port 1" };

    int action_type = static_cast<int>(action.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &action_type, action_names, length(action_names)))
        action.type = static_cast<u8>(action_type);
    ImGui::PopItemWidth();

    ImGui::TableNextColumn();

    switch (action_type) {
    case hierarchical_state_machine::action_type_set: {
        int value = static_cast<int>(action.parameter_1);
        ImGui::PushItemWidth(-1);
        if (ImGui::Combo("##input-set",
                         &value,
                         action_input_port,
                         length(action_input_port)))
            action.parameter_1 = static_cast<u8>(value);
        ImGui::PopItemWidth();
    } break;

    case hierarchical_state_machine::action_type_unset: {
        int value = static_cast<int>(action.parameter_1);
        ImGui::PushItemWidth(-1);
        if (ImGui::Combo("##input-unset",
                         &value,
                         action_input_port,
                         length(action_input_port)))
            action.parameter_1 = static_cast<u8>(value);
        ImGui::PopItemWidth();
    } break;

    case hierarchical_state_machine::action_type_reset:
        break;

    case hierarchical_state_machine::action_type_output: {
        int value = static_cast<int>(action.parameter_1);
        if (ImGui::Combo("##output",
                         &value,
                         action_output_port,
                         length(action_output_port)))
            action.parameter_1 = static_cast<u8>(value);
        ImGui::SameLine();
        ImGui::InputScalar("value", ImGuiDataType_U8, &action.parameter_2);
    } break;

    default:
        break;
    }
}

static void show_state_id_editor(
  hierarchical_state_machine::state_id* current) noexcept
{
    ImGui::PushID(current);

    small_string<7> preview_value("-");
    if (*current != hierarchical_state_machine::invalid_state_id)
        format(preview_value, "{}", *current);

    ImGui::PushItemWidth(-1);
    if (ImGui::BeginCombo("##transition", preview_value.c_str())) {

        if (ImGui::Selectable(
              "-", *current == hierarchical_state_machine::invalid_state_id))
            *current = hierarchical_state_machine::invalid_state_id;

        for (u8 i = 0, e = hierarchical_state_machine::max_number_of_state;
             i < e;
             ++i) {
            format(preview_value, "{}", i);
            if (ImGui::Selectable(preview_value.c_str(), i == *current))
                *current = i;
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_hsm_inputs(hierarchical_state_machine& machine)
{
    bool start_valid =
      machine.m_top_state != hierarchical_state_machine::invalid_state_id;

    if (!start_valid)
        ImGui::Text("Top step undefined");

    static const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    if (ImGui::BeginTable("states", 5, flags)) {
        ImGui::TableSetupColumn("id");
        ImGui::TableSetupColumn("top", ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn(
          "super-id", ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn(
          "sub-id", ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn("state", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i != length(machine.states); ++i) {
            ImGui::PushID(i);
            auto& state = machine.states[i];

            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();
            if (machine.m_top_state ==
                hierarchical_state_machine::invalid_state_id) {
                if (ImGui::Button("set top")) {
                    machine.m_top_state = to_u8(i);
                }
            }
            ImGui::TableNextColumn();
            show_state_id_editor(&state.super_id);
            ImGui::TableNextColumn();
            show_state_id_editor(&state.sub_id);
            ImGui::TableNextColumn();

            if (ImGui::BeginTable("nested", 6, flags)) {
                ImGui::TableSetupColumn("event");
                ImGui::TableSetupColumn("input port");
                ImGui::TableSetupColumn("mandatory port");
                ImGui::TableSetupColumn("action");
                ImGui::TableSetupColumn("parameters");
                ImGui::TableSetupColumn("transition");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::PushID("enter-event");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("enter");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                show_state_action(state.enter_action);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::PopID();

                ImGui::TableNextRow();
                ImGui::PushID("input-if-event");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("if");
                ImGui::TableNextColumn();
                show_ports(&state.input_changed_action.value_condition_1);
                ImGui::TableNextColumn();
                show_ports(&state.input_changed_action.value_mask_1);
                ImGui::TableNextColumn();
                show_state_action(state.input_changed_action.action_1);
                ImGui::TableNextColumn();
                show_state_id_editor(&state.input_changed_action.transition_1);
                ImGui::PopID();

                ImGui::TableNextRow();
                ImGui::PushID("else-if");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("else-if");
                ImGui::TableNextColumn();
                show_ports(&state.input_changed_action.value_condition_2);
                ImGui::TableNextColumn();
                show_ports(&state.input_changed_action.value_mask_2);
                ImGui::TableNextColumn();
                show_state_action(state.input_changed_action.action_2);
                ImGui::TableNextColumn();
                show_state_id_editor(&state.input_changed_action.transition_2);
                ImGui::PopID();

                ImGui::TableNextRow();
                ImGui::PushID("exit-event");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("exit");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                show_state_action(state.exit_action);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("-");
                ImGui::PopID();

                ImGui::EndTable();
            }
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
