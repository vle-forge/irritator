// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "editor.hpp"
#include "internal.hpp"

namespace irt {

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
{}

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
{}

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
{}

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

void show_dynamics_inputs(external_source& /*srcs*/, flow& /*dyn*/) {}

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
