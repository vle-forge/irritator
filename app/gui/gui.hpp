// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_GUI_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_GUI_2020

#include <filesystem>

namespace irt {

void
node_editor_initialize();

bool
node_editor_show();

void
node_editor_shutdown();

/* Filesytem dialog box */

bool
load_file_dialog(std::filesystem::path& out);

bool
save_file_dialog(std::filesystem::path& out);

static inline const char* simulation_status_string[] = {
    "success",
    "running",
    "uninitialized",
    "internal_error",
};

static inline const char* status_string[] = {
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
    "dynamics_unknown_id",
    "dynamics_unknown_port_id",
    "dynamics_not_enough_memory",
    "model_connect_output_port_unknown",
    "model_connect_input_port_unknown",
    "model_connect_already_exist",
    "model_connect_bad_dynamics",
    "model_adder_empty_init_message",
    "model_adder_bad_init_message",
    "model_adder_bad_external_message",
    "model_mult_empty_init_message",
    "model_mult_bad_init_message",
    "model_mult_bad_external_message",
    "model_integrator_internal_error",
    "model_integrator_output_error",
    "model_integrator_running_without_x_dot",
    "model_integrator_ta_with_bad_x_dot",
    "model_integrator_bad_external_message",
    "model_quantifier_bad_quantum_parameter",
    "model_quantifier_bad_archive_length_parameter",
    "model_quantifier_shifting_value_neg",
    "model_quantifier_shifting_value_less_1",
    "model_quantifier_bad_external_message",
    "model_cross_bad_external_message",
    "model_time_func_bad_init_message",
    "model_accumulator_bad_external_message",
    "gui_not_enough_memory",
    "io_file_format_error",
    "io_file_format_model_error",
    "io_file_format_model_number_error",
    "io_file_format_model_unknown",
    "io_file_format_dynamics_unknown",
    "io_file_format_dynamics_limit_reach",
    "io_file_format_dynamics_init_error"
};

} // namespace irt

#endif
