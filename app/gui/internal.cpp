// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "internal.hpp"

namespace irt {

void
HelpMarker(const char* desc) noexcept;
{
    try {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    } catch (const std::exception& /*e*/) {
    }
}

const char*
status_string(const status s) noexcept
{
    static const char* str[] = {
        "success",
        "unknown_dynamics",
        "block_allocator_bad_capacity",
        "block_allocator_not_enough_memory",
        "head_allocator_bad_capacity",
        "head_allocator_not_enough_memory",
        "simulation_not_enough_model",
        "simulation_not_enough_memory_message_list_allocator",
        "simulation_not_enough_memory_input_port_list_allocator",
        "simulation_not_enough_memory_output_port_list_allocator",
        "data_array_init_capacity_error",
        "data_array_not_enough_memory",
        "data_array_archive_init_capacity_error",
        "data_array_archive_not_enough_memory",
        "array_init_capacity_zero",
        "array_init_capacity_too_big",
        "array_init_not_enough_memory",
        "vector_init_capacity_zero",
        "vector_init_capacity_too_big",
        "vector_init_not_enough_memory",
        "source_unknown_id",
        "source_empty",
        "dynamics_unknown_id",
        "dynamics_unknown_port_id",
        "dynamics_not_enough_memory",
        "model_connect_output_port_unknown",
        "model_connect_input_port_unknown",
        "model_connect_already_exist",
        "model_connect_bad_dynamics",
        "model_queue_bad_ta",
        "model_queue_empty_allocator",
        "model_queue_full",
        "model_dynamic_queue_source_is_null",
        "model_dynamic_queue_empty_allocator",
        "model_dynamic_queue_full",
        "model_priority_queue_source_is_null",
        "model_priority_queue_empty_allocator",
        "model_priority_queue_full",
        "model_integrator_dq_error",
        "model_integrator_X_error",
        "model_integrator_internal_error",
        "model_integrator_output_error",
        "model_integrator_running_without_x_dot",
        "model_integrator_ta_with_bad_x_dot",
        "model_generator_null_ta_source",
        "model_generator_empty_ta_source",
        "model_generator_null_value_source",
        "model_generator_empty_value_source",
        "model_quantifier_bad_quantum_parameter",
        "model_quantifier_bad_archive_length_parameter",
        "model_quantifier_shifting_value_neg",
        "model_quantifier_shifting_value_less_1",
        "model_time_func_bad_init_message",
        "model_flow_bad_samplerate",
        "model_flow_bad_data",
        "gui_not_enough_memory",
        "io_not_enough_memory",
        "io_file_format_error",
        "io_file_format_source_number_error",
        "io_file_source_full",
        "io_file_format_model_error",
        "io_file_format_model_number_error",
        "io_file_format_model_unknown",
        "io_file_format_dynamics_unknown",
        "io_file_format_dynamics_limit_reach",
        "io_file_format_dynamics_init_error"
    };

    static_assert(std::size(str) == status_size());

    return str[static_cast<int>(s)];
}

} // namespace irt
