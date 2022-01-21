// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "internal.hpp"

#include <imgui.h>

namespace irt {

void HelpMarker(const char* desc) noexcept
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

const char* status_string(const status s) noexcept
{
    static const char* str[] = {
        "success",
        "unknown_dynamics",
        "block_allocator_bad_capacity",
        "block_allocator_not_enough_memory",
        "head_allocator_bad_capacity",
        "head_allocator_not_enough_memory",
        "simulation_not_enough_model",
        "simulation_not_enough_message",
        "simulation_not_enough_connection",
        "vector_init_capacity_error",
        "vector_not_enough_memory",
        "data_array_init_capacity_error",
        "data_array_not_enough_memory",
        "source_unknown",
        "source_empty",
        "model_connect_output_port_unknown",
        "model_connect_already_exist",
        "model_connect_bad_dynamics",
        "model_queue_bad_ta",
        "model_queue_full",
        "model_dynamic_queue_source_is_null",
        "model_dynamic_queue_full",
        "model_priority_queue_source_is_null",
        "model_priority_queue_full",
        "model_integrator_dq_error",
        "model_integrator_X_error",
        "model_integrator_internal_error",
        "model_integrator_output_error",
        "model_integrator_running_without_x_dot",
        "model_integrator_ta_with_bad_x_dot",
        "model_quantifier_bad_quantum_parameter",
        "model_quantifier_bad_archive_length_parameter",
        "model_quantifier_shifting_value_neg",
        "model_quantifier_shifting_value_less_1",
        "model_time_func_bad_init_message",
        "model_flow_bad_samplerate",
        "model_flow_bad_data",

        "modeling_too_many_description_open",
        "modeling_too_many_file_open",
        "modeling_too_many_directory_open",
        "modeling_registred_path_access_error",
        "modeling_directory_access_error",
        "modeling_file_access_error",
        "modeling_component_save_error",

        "gui_not_enough_memory",
        "io_filesystem_error",
        "io_filesystem_make_directory_error",
        "io_filesystem_not_directory_error",
        "io_not_enough_memory",
        "io_file_format_error",
        "io_file_format_source_number_error",
        "io_file_source_full",
        "io_file_format_model_error",
        "io_file_format_model_number_error",
        "io_file_format_model_unknown",
        "io_file_format_dynamics_unknown",
        "io_file_format_dynamics_limit_reach",
        "io_file_format_dynamics_init_error",
    };

    static_assert(std::size(str) == status_size());

    return str[static_cast<int>(s)];
}

} // namespace irt
