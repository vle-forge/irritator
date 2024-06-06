// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#ifdef IRRITATOR_ENABLE_DEBUG
#include <fmt/format.h>
#endif

#include <cstdint>

using namespace std::literals;

namespace irt {

enum class stack_id {
    child,
    child_model_dynamics,
    child_model,
    copy_internal_component,
    child_internal_component,
    child_simple_or_grid_component,
    dispatch_child_component_type,
    read_child_component,
    dispatch_child_component_or_model,
    child_component_or_model,
    children_array,
    children,
    internal_component,
    component,
    component_color,
    component_ports,
    component_grid,
    component_grid_children,
    component_graph,
    component_graph_param,
    component_graph_type,
    component_graph_children,
    component_hsm,
    component_children,
    component_generic,
    component_generic_connections,
    component_generic_connect,
    component_generic_x_port,
    component_generic_y_port,
    component_generic_connect_input,
    component_generic_connect_output,
    component_generic_dispatch_connection,
    component_generic_internal_connection,
    component_generic_output_connection,
    component_generic_input_connection,
    srcs_constant_source,
    srcs_constant_sources,
    srcs_text_file_source,
    srcs_text_file_sources,
    srcs_binary_file_source,
    srcs_binary_file_sources,
    srcs_random_source,
    srcs_random_source_distribution,
    srcs_random_sources,
    dynamics,
    dynamics_qss_integrator,
    dynamics_qss_multiplier,
    dynamics_qss_sum,
    dynamics_qss_wsum_2,
    dynamics_qss_wsum_3,
    dynamics_qss_wsum_4,
    dynamics_counter,
    dynamics_queue,
    dynamics_dynamic_queue,
    dynamics_priority_queue,
    dynamics_generator,
    dynamics_constant,
    dynamics_qss_cross,
    dynamics_qss_filter,
    dynamics_qss_power,
    dynamics_qss_square,
    dynamics_accumulator_2,
    dynamics_time_func,
    dynamics_logical_and_2,
    dynamics_logical_or_2,
    dynamics_logical_and_3,
    dynamics_logical_or_3,
    dynamics_logical_invert,
    dynamics_hsm_condition_action,
    dynamics_hsm_state_action,
    dynamics_hsm_state,
    dynamics_hsm_states,
    dynamics_hsm_output,
    dynamics_hsm_outputs,
    dynamics_hsm,
    simulation_model_dynamics,
    simulation_hsm,
    simulation_hsms,
    simulation_model,
    simulation_models,
    simulation_connections,
    simulation_connection,
    simulation_connect,
    simulation,
    project,
    project_convert_to_tn_model_ids,
    project_convert_to_tn_id,
    project_access,
    project_real_parameter,
    project_integer_parameter,
    project_parameter,
    project_global_parameters,
    project_global_parameter,
    project_global_parameter_children,
    project_global_parameter_child,
    project_grid_parameters,
    project_grid_parameter,
    project_parameters,
    project_observations,
    project_observation_assign,
    project_observation_type,
    project_observation,
    project_top_component,
    project_set_components,
    project_unique_id_path,
    project_plot_observation_child,
    project_plot_observation_children,
    project_plot_observation,
    project_plot_observations,
    project_grid_observation,
    project_grid_observations,
    load_color,
    search_directory,
    search_file_in_directory,
    undefined,
    COUNT
};

static inline constexpr std::string_view stack_id_names[] = {
    "child",
    "child_model_dynamics",
    "child_model",
    "copy_internal_component",
    "child_internal_component",
    "child_simple_or_grid_component",
    "dispatch_child_component_type",
    "read_child_component",
    "dispatch_child_component_or_model",
    "child_component_or_model",
    "children_array",
    "children",
    "internal_component",
    "component",
    "component_color",
    "component_ports",
    "component_grid",
    "component_grid_children",
    "component_graph",
    "component_graph_param",
    "component_graph_type",
    "component_graph_children",
    "component_hsm",
    "component_children",
    "component_generic",
    "component_generic_connections",
    "component_generic_connect",
    "component_generic_x_port",
    "component_generic_y_port",
    "component_generic_connect_input",
    "component_generic_connect_output",
    "component_generic_dispatch_connection",
    "component_generic_internal_connection",
    "component_generic_output_connection",
    "component_generic_input_connection",
    "srcs_constant_source",
    "srcs_constant_sources",
    "srcs_text_file_source",
    "srcs_text_file_sources",
    "srcs_binary_file_source",
    "srcs_binary_file_sources",
    "srcs_random_source",
    "srcs_random_source_distribution",
    "srcs_random_sources",
    "dynamics",
    "dynamics_qss_integrator",
    "dynamics_qss_multiplier",
    "dynamics_qss_sum",
    "dynamics_qss_wsum_2",
    "dynamics_qss_wsum_3",
    "dynamics_qss_wsum_4",
    "dynamics_counter",
    "dynamics_queue",
    "dynamics_dynamic_queue",
    "dynamics_priority_queue",
    "dynamics_generator",
    "dynamics_constant",
    "dynamics_qss_cross",
    "dynamics_qss_filter",
    "dynamics_qss_power",
    "dynamics_qss_square",
    "dynamics_accumulator_2",
    "dynamics_time_func",
    "dynamics_logical_and_2",
    "dynamics_logical_or_2",
    "dynamics_logical_and_3",
    "dynamics_logical_or_3",
    "dynamics_logical_invert",
    "dynamics_hsm_condition_action",
    "dynamics_hsm_state_action",
    "dynamics_hsm_state",
    "dynamics_hsm_states",
    "dynamics_hsm_output",
    "dynamics_hsm_outputs",
    "dynamics_hsm",
    "simulation_model_dynamics",
    "simulation_hsm",
    "simulation_hsms",
    "simulation_model",
    "simulation_models",
    "simulation_connections",
    "simulation_connection",
    "simulation_connect",
    "simulation",
    "project",
    "project_convert_to_tn_model_ids",
    "project_convert_to_tn_id",
    "project_access",
    "project_real_parameter",
    "project_integer_parameter",
    "project_parameter",
    "project_global_parameters",
    "project_global_parameter",
    "project_global_parameter_children",
    "project_global_parameter_child",
    "project_grid_parameters",
    "project_grid_parameter",
    "project_parameters",
    "project_observations",
    "project_observation_assign",
    "project_observation_type",
    "project_observation",
    "project_top_component",
    "project_set_components",
    "project_unique_id_path",
    "project_plot_observation_child",
    "project_plot_observation_children",
    "project_plot_observation",
    "project_plot_observations",
    "project_grid_observation",
    "project_grid_observations",
    "load_color",
    "search_directory",
    "search_file_in_directory",
    "undefined"
};

static_assert(std::cmp_equal(std::size(stack_id_names),
                             ordinal(stack_id::COUNT)));

enum class connection_type { internal, input, output };

enum class error_id {
    none,
    object_name_not_found,
    directory_not_found,
    file_not_found,
    filesystem_error,
    missing_integer,
    missing_bool,
    missing_u64,
    missing_float,
    missing_double,
    missing_string,
    missing_time_function,
    missing_quantifier_adapt_state,
    missing_distribution_type,
    missing_component_type,
    missing_internal_component_type,
    missing_connection_type,
    missing_grid_component_type,
    missing_model_child_type_error,
    missing_mandatory_arg,
    missing_constant_init_type,
    file_system_not_enough_memory,
    integer_to_i8_error,
    integer_to_u8_error,
    integer_to_u32_error,
    integer_to_i32_error,
    integer_to_hsm_action_type,
    integer_to_hsm_condition_type,
    integer_to_hsm_variable,
    modeling_not_enough_children,
    modeling_not_enough_model,
    modeling_connect_error,
    modeling_connect_input_error,
    modeling_connect_output_error,
    modeling_internal_component_missing,
    modeling_component_missing,
    modeling_hsm_id_error,
    generic_component_error_port_identifier,
    generic_component_unknown_component,
    generic_component_unknown_component_x_port,
    generic_component_unknown_component_y_port,
    grid_component_size_error,
    graph_component_type_error,
    srcs_constant_sources_buffer_not_enough,
    srcs_constant_sources_not_enough,
    srcs_text_file_sources_not_enough,
    srcs_binary_file_sources_not_enough,
    srcs_random_sources_not_enough,
    double_min_error,
    double_max_error,
    integer_min_error,
    value_not_array,
    value_array_bad_size,
    value_array_size_error,
    value_not_object,
    unknown_element,
    cache_model_mapping_unfound,
    simulation_hsms_not_enough,
    simulation_models_not_enough,
    simulation_connect_src_unknown,
    simulation_connect_dst_unknown,
    simulation_connect_src_port_unknown,
    simulation_connect_dst_port_unknown,
    simulation_connect_error,
    project_set_no_head,
    project_set_error,
    project_access_parameter_error,
    project_access_observable_error,
    project_access_tree_error,
    project_variable_observers_not_enough,
    project_grid_observers_not_enough,
    project_global_parameters_not_enough,
    project_grid_parameters_not_enough,
    project_fail_convert_access_to_tn_model_ids,
    project_fail_convert_access_to_tn_id,
    COUNT
};

static inline constexpr std::string_view error_id_names[] = {
    "none",
    "object_name_not_found",
    "directory_not_found",
    "file_not_found",
    "filesystem_error",
    "missing_integer",
    "missing_bool",
    "missing_u64",
    "missing_float",
    "missing_double",
    "missing_string",
    "missing_time_function",
    "missing_quantifier_adapt_state",
    "missing_distribution_type",
    "missing_component_type",
    "missing_internal_component_type",
    "missing_connection_type",
    "missing_grid_component_type",
    "missing_model_child_type_error",
    "missing_mandatory_arg",
    "missing_constant_init_type",
    "file_system_not_enough_memory",
    "integer_to_i8_error",
    "integer_to_u8_error",
    "integer_to_u32_error",
    "integer_to_i32_error",
    "integer_to_hsm_action_type",
    "integer_to_hsm_condition_type",
    "integer_to_hsm_variable",
    "modeling_not_enough_children",
    "modeling_not_enough_model",
    "modeling_connect_error",
    "modeling_connect_input_error",
    "modeling_connect_output_error",
    "modeling_internal_component_missing",
    "modeling_component_missing",
    "modeling_hsm_id_error",
    "generic_component_error_port_identifier",
    "generic_component_unknown_component",
    "generic_component_unknown_component_x_port",
    "generic_component_unknown_component_y_port",
    "grid_component_size_error",
    "graph_component_type_error",
    "srcs_constant_sources_buffer_not_enough",
    "srcs_constant_sources_not_enough",
    "srcs_text_file_sources_not_enough",
    "srcs_binary_file_sources_not_enough",
    "srcs_random_sources_not_enough",
    "double_min_error",
    "double_max_error",
    "integer_min_error",
    "value_not_array",
    "value_array_bad_size",
    "value_array_size_error",
    "value_not_object",
    "unknown_element",
    "cache_model_mapping_unfound",
    "simulation_hsms_not_enough",
    "simulation_models_not_enough",
    "simulation_connect_src_unknown",
    "simulation_connect_dst_unknown",
    "simulation_connect_src_port_unknown",
    "simulation_connect_dst_port_unknown",
    "simulation_connect_error",
    "project_set_no_head",
    "project_set_error",
    "project_access_parameter_error",
    "project_access_observable_error",
    "project_access_tree_error",
    "project_variable_observers_not_enough",
    "project_grid_observers_not_enough",
    "project_global_parameters_not_enough",
    "project_grid_parameters_not_enough",
    "project_fail_convert_access_to_tn_model_ids",
    "project_fail_convert_access_to_tn_id",
};

static_assert(std::cmp_equal(std::size(error_id_names),
                             ordinal(error_id::COUNT)));

#define report_json_error(error_id__)                                          \
    do {                                                                       \
        error = error_id__;                                                    \
        if (on_error_callback) {                                               \
            on_error_callback();                                               \
        }                                                                      \
        return false;                                                          \
    } while (0)

template<typename T>
static bool buffer_reserve(std::integral auto len, vector<T>& vec) noexcept
{
    vec.reserve(len);
    return true;
}

struct reader {
    template<typename Function, typename... Args>
    bool for_each_array(const rapidjson::Value& array,
                        Function&&              f,
                        Args... args) noexcept
    {
        if (!array.IsArray())
            report_json_error(error_id::value_not_array);

        rapidjson::SizeType i = 0, e = array.GetArray().Size();
        for (; i != e; ++i) {
            debug_logi(stack.ssize(), "for-array: {}/{}\n", i, e);

            if (!f(i, array.GetArray()[i], args...))
                return false;
        }

        return true;
    }

    template<size_t N, typename Function>
    bool for_members(const rapidjson::Value& val,
                     const std::string_view (&names)[N],
                     Function&& fn) noexcept
    {
        if (!val.IsObject())
            report_json_error(error_id::value_not_object);

        auto       it = val.MemberBegin();
        const auto et = val.MemberEnd();

        while (it != et) {
            const auto x =
              binary_find(std::begin(names),
                          std::end(names),
                          std::string_view{ it->name.GetString(),
                                            it->name.GetStringLength() });

            if (x == std::end(names)) {
                debug_logi(stack.ssize(),
                           "for-member: unknown element {}\n",
                           std::string_view{ it->name.GetString(),
                                             it->name.GetStringLength() });

                report_json_error(error_id::unknown_element);
            }

            if (!fn(std::distance(std::begin(names), x), it->value)) {
                debug_logi(stack.ssize(),
                           "for-member: element {} return false\n",
                           std::string_view{ it->name.GetString(),
                                             it->name.GetStringLength() });
                return false;
            }

            ++it;
        }

        return true;
    }

    template<typename Function, typename... Args>
    bool for_each_member(const rapidjson::Value& val,
                         Function&&              f,
                         Args... args) noexcept
    {
        if (!val.IsObject())
            report_json_error(error_id::value_not_object);

        for (auto it = val.MemberBegin(), et = val.MemberEnd(); it != et;
             ++it) {
            debug_logi(stack.ssize(), "for-member: {}\n", it->name.GetString());
            if (!f(it->name.GetString(), it->value, args...))
                return false;
        }

        return true;
    }

    template<typename Function, typename... Args>
    bool for_first_member(const rapidjson::Value& val,
                          std::string_view        name,
                          Function&&              f,
                          Args... args) noexcept
    {
        if (!val.IsObject())
            report_json_error(error_id::value_not_object);

        for (auto it = val.MemberBegin(), et = val.MemberEnd(); it != et; ++it)
            if (name == it->name.GetString())
                return f(it->value, args...);

        report_json_error(error_id::object_name_not_found);
    }

    bool read_temp_integer(const rapidjson::Value& val) noexcept
    {
        if (!val.IsInt64())
            report_json_error(error_id::missing_integer);

        temp_integer = val.GetInt64();

        return true;
    }

    bool read_temp_bool(const rapidjson::Value& val) noexcept
    {
        if (!val.IsBool())
            report_json_error(error_id::missing_bool);

        temp_bool = val.GetBool();

        return true;
    }

    bool read_temp_unsigned_integer(const rapidjson::Value& val) noexcept
    {
        if (!val.IsUint64())
            report_json_error(error_id::missing_u64);

        temp_u64 = val.GetUint64();

        return true;
    }

    bool read_u64(const rapidjson::Value& val, u64& integer) noexcept
    {
        if (val.IsUint64()) {
            integer = val.GetUint64();
            return true;
        }

        report_json_error(error_id::missing_u64);
    }

    bool read_real(const rapidjson::Value& val, double& r) noexcept
    {
        if (val.IsDouble()) {
            r = val.GetDouble();
            return true;
        }

        report_json_error(error_id::missing_double);
    }

    bool read_temp_real(const rapidjson::Value& val) noexcept
    {
        if (!val.IsDouble())
            report_json_error(error_id::missing_double);

        temp_double = val.GetDouble();

        return true;
    }

    bool read_temp_string(const rapidjson::Value& val) noexcept
    {
        if (!val.IsString())
            report_json_error(error_id::missing_string);

        temp_string = val.GetString();

        return true;
    }

    bool copy_to(binary_file_source& src) noexcept
    {
        try {
            src.file_path = temp_string;
            return true;
        } catch (...) {
        }

        report_json_error(error_id::filesystem_error);
    }

    bool copy_to(text_file_source& src) noexcept
    {
        try {
            src.file_path = temp_string;
            return true;
        } catch (...) {
        }

        report_json_error(error_id::filesystem_error);
    }

    bool copy_to(constant::init_type& type) noexcept
    {
        if (temp_string == "constant"sv)
            type = constant::init_type::constant;
        else if (temp_string == "incoming_component_all"sv)
            type = constant::init_type::incoming_component_all;
        else if (temp_string == "outcoming_component_all"sv)
            type = constant::init_type::outcoming_component_all;
        else if (temp_string == "incoming_component_n"sv)
            type = constant::init_type::incoming_component_n;
        else if (temp_string == "outcoming_component_n"sv)
            type = constant::init_type::outcoming_component_n;
        else
            report_json_error(error_id::missing_constant_init_type);

        return true;
    }

    bool copy_to(connection_type& type) noexcept
    {
        if (temp_string == "internal"sv)
            type = connection_type::internal;
        else if (temp_string == "output"sv)
            type = connection_type::output;
        else if (temp_string == "input"sv)
            type = connection_type::input;
        else
            report_json_error(error_id::missing_connection_type);

        return true;
    }

    bool copy_to(distribution_type& dst) noexcept
    {
        if (auto dist_opt = get_distribution_type(temp_string);
            dist_opt.has_value()) {
            dst = dist_opt.value();
            return true;
        }

        report_json_error(error_id::missing_distribution_type);
    }

    bool copy_to(dynamics_type& dst) noexcept
    {
        if (auto opt = get_dynamics_type(temp_string); opt.has_value()) {
            dst = opt.value();
            return true;
        }

        report_json_error(error_id::missing_model_child_type_error);
    }

    bool copy_to(child_type& dst_1, dynamics_type& dst_2) noexcept
    {
        if (temp_string == "component"sv) {
            dst_1 = child_type::component;
            return true;
        } else {
            dst_1 = child_type::model;

            if (auto opt = get_dynamics_type(temp_string); opt.has_value()) {
                dst_2 = opt.value();
                return true;
            } else {
                report_json_error(error_id::missing_model_child_type_error);
            }
        }
    }

    bool copy_to(grid_component::type& dst) noexcept
    {
        if (0 <= temp_integer && temp_integer < grid_component::type_count) {
            dst = enum_cast<grid_component::type>(temp_integer);
            return true;
        }

        report_json_error(error_id::missing_grid_component_type);
    }

    bool copy_to(component_type& dst) noexcept
    {
        if (auto opt = get_component_type(temp_string); opt.has_value()) {
            dst = opt.value();
            return true;
        }

        report_json_error(error_id::missing_component_type);
    }

    bool copy_to(internal_component& dst) noexcept
    {
        if (auto compo_opt = get_internal_component_type(temp_string);
            compo_opt.has_value()) {
            dst = compo_opt.value();
            return true;
        }

        report_json_error(error_id::missing_internal_component_type);
    }

    template<int Length>
    bool copy_to(small_string<Length>& dst) noexcept
    {
        dst.assign(temp_string);

        return true;
    }

    bool copy_to(bool& dst) noexcept
    {
        dst = temp_bool;

        return true;
    }

    bool copy_to(double& dst) noexcept
    {
        dst = temp_double;

        return true;
    }

    bool copy_to(i64& dst) noexcept
    {
        dst = temp_integer;

        return true;
    }

    bool copy_to(i32& dst) noexcept
    {
        if (!(INT32_MIN <= temp_integer && temp_integer <= INT32_MAX))
            report_json_error(error_id::integer_to_i32_error);

        dst = static_cast<i32>(temp_integer);

        return true;
    }

    bool copy_to(u32& dst) noexcept
    {
        if (temp_u64 >= UINT32_MAX)
            report_json_error(error_id::integer_to_u32_error);

        dst = static_cast<u8>(temp_u64);

        return true;
    }

    bool copy_to(i8& dst) noexcept
    {
        if (!(0 <= temp_integer && temp_integer <= INT8_MAX))
            report_json_error(error_id::integer_to_i8_error);

        dst = static_cast<i8>(temp_integer);

        return true;
    }

    bool copy_to(u8& dst) noexcept
    {
        if (!(0 <= temp_integer && temp_integer <= UINT8_MAX))
            report_json_error(error_id::integer_to_u8_error);

        dst = static_cast<u8>(temp_integer);

        return true;
    }

    bool copy_to(hierarchical_state_machine::action_type& dst) noexcept
    {
        if (!(0 <= temp_integer &&
              temp_integer < hierarchical_state_machine::action_type_count))
            report_json_error(error_id::integer_to_hsm_action_type);

        dst = enum_cast<hierarchical_state_machine::action_type>(temp_integer);

        return true;
    }

    bool copy_to(hierarchical_state_machine::condition_type& dst) noexcept
    {
        if (!(0 <= temp_integer &&
              temp_integer < hierarchical_state_machine::condition_type_count))
            report_json_error(error_id::integer_to_hsm_condition_type);

        dst =
          enum_cast<hierarchical_state_machine::condition_type>(temp_integer);

        return true;
    }

    bool copy_to(hierarchical_state_machine::variable& dst) noexcept
    {
        if (!(0 <= temp_integer &&
              temp_integer < hierarchical_state_machine::variable_count))
            report_json_error(error_id::integer_to_hsm_variable);

        dst = enum_cast<hierarchical_state_machine::variable>(temp_integer);

        return true;
    }

    bool copy_to(real (*&dst)(real)) noexcept
    {
        if (temp_string == "time"sv)
            dst = &time_function;
        else if (temp_string == "square"sv)
            dst = &square_time_function;
        else if (temp_string == "sin"sv)
            dst = &sin_time_function;
        else
            report_json_error(error_id::missing_time_function);

        return true;
    }

    template<typename T>
    bool optional_has_value(const std::optional<T>& v) noexcept
    {
        if (v.has_value())
            return true;

        report_json_error(error_id::missing_mandatory_arg);
    }

    bool copy_to(u64& dst) noexcept
    {
        dst = temp_u64;

        return true;
    }

    bool copy_to(std::optional<u64>& dst) noexcept
    {
        dst = temp_u64;

        return true;
    }

    bool copy_to(std::optional<i8>& dst) noexcept
    {
        if (!(INT8_MIN <= temp_integer && temp_integer <= INT8_MAX))
            report_json_error(error_id::integer_to_i8_error);

        dst = static_cast<i8>(temp_integer);
        return true;
    }

    bool copy_to(std::optional<i32>& dst) noexcept
    {
        if (!(INT32_MIN <= temp_integer && temp_integer <= INT32_MAX))
            report_json_error(error_id::integer_to_i32_error);

        dst = static_cast<int>(temp_integer);
        return true;
    }

    bool copy_to(std::optional<std::string>& dst) noexcept
    {
        dst = temp_string;

        return true;
    }

    bool copy_to(float& dst) noexcept
    {
        dst = static_cast<float>(temp_double);

        return true;
    }

    bool copy_to(source::source_type& dst) noexcept
    {
        dst = enum_cast<source::source_type>(temp_integer);

        return true;
    }

    bool project_global_parameters_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().parameters.can_alloc(i))
            report_json_error(error_id::project_global_parameters_not_enough);

        return true;
    }

    bool project_variable_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().variable_observers.can_alloc(i))
            report_json_error(error_id::project_variable_observers_not_enough);

        return true;
    }

    bool project_grid_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().grid_observers.can_alloc(i))
            report_json_error(error_id::project_grid_observers_not_enough);

        return true;
    }

    bool generic_can_alloc(const generic_component& compo,
                           std::integral auto       i) noexcept
    {
        if (not compo.children.can_alloc(i))
            report_json_error(
              error_id::modeling_not_enough_children); // @TODO replace with a
                                                       // better `error_id`.

        return true;
    }

    bool is_double_greater_than(double excluded_min) noexcept
    {
        if (temp_double <= excluded_min)
            report_json_error(error_id::double_min_error);

        return true;
    }

    bool is_double_greater_equal_than(double included_min) noexcept
    {
        if (temp_double < included_min)
            report_json_error(error_id::double_min_error);

        return true;
    }

    bool is_int_less_than(int excluded_max) noexcept
    {
        if (temp_integer >= excluded_max)
            report_json_error(error_id::double_max_error);

        return true;
    }

    bool is_int_greater_equal_than(int included_min) noexcept
    {
        if (temp_integer < included_min)
            report_json_error(error_id::integer_min_error);

        return true;
    }

    bool is_value_array_size_equal(const rapidjson::Value& val, int to) noexcept
    {
        debug::ensure(val.IsArray());

        if (std::cmp_equal(val.GetArray().Size(), to))
            return true;

        report_json_error(error_id::value_array_bad_size);
    }

    bool is_value_array(const rapidjson::Value& val) noexcept
    {
        if (!val.IsArray())
            report_json_error(error_id::value_not_array);

        return true;
    }

    bool copy_array_size(const rapidjson::Value& val, i64& dst) noexcept
    {
        debug::ensure(val.IsArray());

        dst = static_cast<i64>(val.GetArray().Size());

        return true;
    }

    bool is_value_array_size_less(const rapidjson::Value& val,
                                  std::integral auto      i) noexcept
    {
        debug::ensure(val.IsArray());

        if (std::cmp_less(val.GetArray().Size(), i))
            return true;

        report_json_error(error_id::value_array_bad_size);
    }

    bool is_value_object(const rapidjson::Value& val) noexcept
    {
        if (!val.IsObject())
            report_json_error(error_id::value_not_object);

        return true;
    }

    bool affect_configurable_to(bitflags<child_flags>& flag) noexcept
    {
        flag.set(child_flags::configurable, temp_bool);

        return true;
    }

    bool affect_observable_to(bitflags<child_flags>& flag) noexcept
    {
        flag.set(child_flags::observable, temp_bool);

        return true;
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&        val,
                       abstract_integrator<QssLevel>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_integrator);

        static constexpr std::string_view n[] = { "X", "dQ" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_real(value, dyn.default_X);
            case 1:
                return read_real(value, dyn.default_dQ);
            default:
                report_json_error(error_id::unknown_element);
            }
        });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value& /*val*/,
                       abstract_multiplier<QssLevel>& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_multiplier);

        return true;
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_sum<QssLevel, 2>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_sum);

        static constexpr std::string_view n[] = { "value-0", "value-1" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_values[0]);
              case 1:
                  return read_real(value, dyn.default_values[1]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_sum<QssLevel, 3>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_sum);

        static constexpr std::string_view n[] = { "value-0",
                                                  "value-1",
                                                  "value-2" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_values[0]);
              case 1:
                  return read_real(value, dyn.default_values[1]);
              case 2:
                  return read_real(value, dyn.default_values[2]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_sum<QssLevel, 4>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_sum);

        static constexpr std::string_view n[] = {
            "value-0", "value-1", "value-2", "value-3"
        };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_values[0]);
              case 1:
                  return read_real(value, dyn.default_values[1]);
              case 2:
                  return read_real(value, dyn.default_values[2]);
              case 3:
                  return read_real(value, dyn.default_values[3]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&     val,
                       abstract_wsum<QssLevel, 2>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_2);

        static constexpr std::string_view n[] = {
            "coeff-0", "coeff-1", "value-0", "value-1"
        };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_input_coeffs[0]);
              case 1:
                  return read_real(value, dyn.default_input_coeffs[1]);
              case 2:
                  return read_real(value, dyn.default_values[0]);
              case 3:
                  return read_real(value, dyn.default_values[1]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&     val,
                       abstract_wsum<QssLevel, 3>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_3);

        static constexpr std::string_view n[] = { "coeff-0", "coeff-1",
                                                  "coeff-2", "value-0",
                                                  "value-1", "value-2" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_input_coeffs[0]);
              case 1:
                  return read_real(value, dyn.default_input_coeffs[1]);
              case 2:
                  return read_real(value, dyn.default_input_coeffs[2]);
              case 3:
                  return read_real(value, dyn.default_values[0]);
              case 4:
                  return read_real(value, dyn.default_values[1]);
              case 5:
                  return read_real(value, dyn.default_values[2]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&     val,
                       abstract_wsum<QssLevel, 4>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_4);

        static constexpr std::string_view n[] = { "coeff-0", "coeff-1",
                                                  "coeff-2", "coeff-3",
                                                  "value-0", "value-1",
                                                  "value-2", "value-3" };

        return for_members(
          val, n, [&](const auto idx, const auto& value) noexcept -> bool {
              switch (idx) {
              case 0:
                  return read_real(value, dyn.default_input_coeffs[0]);
              case 1:
                  return read_real(value, dyn.default_input_coeffs[1]);
              case 2:
                  return read_real(value, dyn.default_input_coeffs[2]);
              case 3:
                  return read_real(value, dyn.default_input_coeffs[3]);
              case 4:
                  return read_real(value, dyn.default_values[0]);
              case 5:
                  return read_real(value, dyn.default_values[1]);
              case 6:
                  return read_real(value, dyn.default_values[2]);
              case 7:
                  return read_real(value, dyn.default_values[3]);
              default:
                  report_json_error(error_id::unknown_element);
              }
          });
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       counter& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_counter);

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val, queue& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_queue);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("ta"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0.0) &&
                         copy_to(dyn.default_ta);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, dynamic_queue& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_dynamic_queue);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("source-ta-type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(source::source_type_count) &&
                         copy_to(dyn.default_source_ta.type);
              if ("source-ta-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(dyn.default_source_ta.id);
              if ("stop-on-error"sv == name)
                  return read_temp_bool(value) && copy_to(dyn.stop_on_error);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val,
                       priority_queue&         dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_priority_queue);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("ta"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0.0) &&
                         copy_to(dyn.default_ta);
              if ("source-ta-type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(source::source_type_count) &&
                         copy_to(dyn.default_source_ta.type);
              if ("source-ta-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(dyn.default_source_ta.id);
              if ("stop-on-error"sv == name)
                  return read_temp_bool(value) && copy_to(dyn.stop_on_error);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, generator& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_generator);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("offset"sv == name)
                  return read_temp_real(value) &&
                         is_double_greater_equal_than(0.0) &&
                         copy_to(dyn.default_offset);

              if ("source-ta-type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(source::source_type_count) &&
                         copy_to(dyn.default_source_ta.type);
              if ("source-ta-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(dyn.default_source_ta.id);

              if ("source-value-type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(source::source_type_count) &&
                         copy_to(dyn.default_source_value.type);
              if ("source-value-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(dyn.default_source_value.id);

              if ("stop-on-error"sv == name)
                  return read_temp_bool(value) && copy_to(dyn.stop_on_error);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, constant& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_constant);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value"sv == name)
                  return read_temp_real(value) && copy_to(dyn.default_value);
              if ("offset"sv == name)
                  return read_temp_real(value) &&
                         is_double_greater_equal_than(0.0) &&
                         copy_to(dyn.default_offset);
              if ("type"sv == name)
                  return read_temp_string(value) && copy_to(dyn.type);
              if ("port"sv == name)
                  return read_temp_integer(value) && copy_to(dyn.port);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&   val,
                       abstract_cross<QssLevel>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_cross);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("threshold"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_threshold);
              if ("detect-up"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_detect_up);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_filter<QssLevel>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_filter);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("lower-threshold"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_lower_threshold);
              if ("upper-threshold"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_upper_threshold);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&   val,
                       abstract_power<QssLevel>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_power);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("n"sv == name)
                  return read_temp_real(value) && copy_to(dyn.default_n);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value& /*val*/,
                       abstract_square<QssLevel>& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_square);

        return true;
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       accumulator_2& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_accumulator_2);

        return true;
    }

    bool read_dynamics(const rapidjson::Value& val, time_func& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_time_func);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("function"sv == name)
                  return read_temp_string(value) && copy_to(dyn.default_f);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, logical_and_2& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_logical_and_2);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[1]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, logical_or_2& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_logical_or_2);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[1]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, logical_and_3& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_logical_and_3);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[2]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, logical_or_3& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_logical_or_3);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_values[2]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& /*val*/,
                       logical_invert& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_logical_invert);

        return true;
    }

    bool read_hsm_condition_action(
      const rapidjson::Value&                       val,
      hierarchical_state_machine::condition_action& s) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_condition_action);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("parameter"sv == name)
                  return read_temp_integer(value) && copy_to(s.parameter);

              if ("type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(
                           hierarchical_state_machine::condition_type_count) &&
                         copy_to(s.type);

              if ("port"sv == name)
                  return read_temp_integer(value) && copy_to(s.port);

              if ("mask"sv == name)
                  return read_temp_integer(value) && copy_to(s.mask);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_hsm_state_action(
      const rapidjson::Value&                   val,
      hierarchical_state_machine::state_action& s) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_state_action);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("parameter"sv == name)
                  return read_temp_integer(value) && copy_to(s.parameter);

              if ("var-1"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_to(s.var1);

              if ("var-2"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(
                           hierarchical_state_machine::variable_count) &&
                         copy_to(s.var2);

              if ("type"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(0) &&
                         is_int_less_than(
                           hierarchical_state_machine::action_type_count) &&
                         copy_to(s.type);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_hsm_state(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack                        a(this, stack_id::dynamics_hsm_state);
        std::optional<int>                id;
        hierarchical_state_machine::state s;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("id"sv == name)
                      return read_temp_integer(value) and copy_to(id);

                  if ("enter"sv == name)
                      return read_hsm_state_action(value, s.enter_action);

                  if ("exit"sv == name)
                      return read_hsm_state_action(value, s.exit_action);

                  if ("if"sv == name)
                      return read_hsm_state_action(value, s.if_action);

                  if ("else"sv == name)
                      return read_hsm_state_action(value, s.else_action);

                  if ("condition"sv == name)
                      return read_hsm_condition_action(value, s.condition);

                  if ("if-transition"sv == name)
                      return read_temp_integer(value) &&
                             copy_to(s.if_transition);

                  if ("else-transition"sv == name)
                      return read_temp_integer(value) &&
                             copy_to(s.else_transition);

                  if ("super-id"sv == name)
                      return read_temp_integer(value) && copy_to(s.super_id);

                  if ("sub-id"sv == name)
                      return read_temp_integer(value) && copy_to(s.sub_id);

                  report_json_error(error_id::unknown_element);
              }) or
            not id.has_value())
            return false;

        states[*id] = s;

        return true;
    }

    bool read_hsm_states(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_states);

        return for_each_array(
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states);
          });
    }

    bool read_hsm_outputs(
      const rapidjson::Value& val,
      small_vector<hierarchical_state_machine::output_message, 4>&
        outputs) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_outputs);

        return for_each_array(
          val, [&](const auto i, const auto& value) noexcept -> bool {
              auto_stack a(this, stack_id::dynamics_hsm_output);

              return for_each_member(
                value,
                [&](const auto name, const auto& value) noexcept -> bool {
                    if ("port"sv == name)
                        return read_temp_integer(value) &&
                               is_int_greater_equal_than(0) &&
                               is_int_less_than(UINT8_MAX) &&
                               copy_to(outputs[i].port);

                    if ("value"sv == name)
                        return read_temp_integer(value) &&
                               copy_to(outputs[i].value);

                    report_json_error(error_id::unknown_element);
                });
          });
    }

    bool read_simulation_dynamics(const rapidjson::Value& val,
                                  hsm_wrapper&            wrapper) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm);

        static constexpr std::string_view n[] = { "a", "b", "hsm" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_temp_integer(value) && copy_to(wrapper.exec.a);
            case 1:
                return read_temp_integer(value) && copy_to(wrapper.exec.b);
            case 2: {
                u64 id_in_file = 0;

                return read_u64(value, id_in_file) &&
                       ((std::cmp_greater(id_in_file, 0) &&
                         sim_hsms_mapping_get(id_in_file, wrapper.id)) ||
                        (copy<hsm_id>(0, wrapper.id) &&
                         warning("hsm_wrapper does not reference a valid hsm",
                                 log_level::error)));
            }
            default:
                report_json_error(error_id::unknown_element);
            }
        });
    }

    bool read_modeling_dynamics(const rapidjson::Value& val,
                                hsm_wrapper&            wrapper) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm);

        static constexpr std::string_view n[] = { "a", "b", "hsm" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0:
                return read_temp_integer(value) && copy_to(wrapper.exec.a);
            case 1:
                return read_temp_integer(value) && copy_to(wrapper.exec.b);
            case 2: {
                component_id c;
                return read_child_component(value, c) &&
                       copy<component_id>(c, wrapper.compo_id);
            }
            default:
                report_json_error(error_id::unknown_element);
            }
        });
    }

    ////

    template<std::integral T, std::integral R>
    bool copy(T from, R& to) noexcept
    {
        if (std::cmp_greater_equal(from, std::numeric_limits<R>::min()) &&
            std::cmp_less_equal(from, std::numeric_limits<R>::max())) {
            to = from;
            return true;
        }

        report_json_error(error_id::integer_min_error);
    }

    template<typename T>
    bool copy(const u64 from, T& id) noexcept
    {
        if constexpr (std::is_enum_v<T>) {
            id = enum_cast<T>(from);
            return true;
        }

        if constexpr (std::is_integral_v<T>) {
            if (is_numeric_castable<T>(id)) {
                id = numeric_cast<T>(id);
                return true;
            }

            report_json_error(error_id::missing_integer);
        }

        if constexpr (std::is_same_v<T, std::optional<u64>>) {
            id = from;
            return true;
        }

        report_json_error(error_id::missing_integer);
    }

    template<typename T>
    bool copy(const T from, u64& id) noexcept
    {
        if constexpr (std::is_enum_v<T>) {
            id = ordinal(from);
            return true;
        }

        if constexpr (std::is_integral_v<T>) {
            if (is_numeric_castable<u64>(id)) {
                id = numeric_cast<u64>(id);
                return true;
            }

            report_json_error(error_id::missing_integer);
        }

        if constexpr (std::is_same_v<T, std::optional<u64>>) {
            if (from.has_value()) {
                id = *from;
                return true;
            }

            report_json_error(error_id::missing_integer);
        }

        report_json_error(error_id::missing_integer);
    }

    bool read_child(const rapidjson::Value& val,
                    generic_component&      generic,
                    child&                  c,
                    child_id                c_id) noexcept
    {
        auto_stack a(this, stack_id::child);

        std::optional<u64> id;
        std::optional<u64> unique_id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("id"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                copy_to(id);
                     if ("unique-id"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                copy_to(unique_id) &&
                                std::cmp_greater(unique_id.value(), 0) &&
                                copy(unique_id, c.unique_id);
                     if ("x"sv == name)
                         return read_temp_real(value) and
                                copy_to(
                                  generic.children_positions[get_index(c_id)]
                                    .x);
                     if ("y"sv == name)
                         return read_temp_real(value) and
                                copy_to(
                                  generic.children_positions[get_index(c_id)]
                                    .y);
                     if ("name"sv == name)
                         return read_temp_string(value) and
                                copy_to(
                                  generic.children_names[get_index(c_id)]);
                     if ("configurable"sv == name)
                         return read_temp_bool(value) and
                                affect_configurable_to(c.flags);
                     if ("observable"sv == name)
                         return read_temp_bool(value) and
                                affect_observable_to(c.flags);

                     return true;
                 }) &&
               optional_has_value(id) &&
               cache_model_mapping_add(*id, ordinal(c_id));
    }

    bool read_child_model_dynamics(const rapidjson::Value& val,
                                   generic_component&      compo,
                                   child&                  c,
                                   model&                  mdl) noexcept
    {
        auto_stack a(this, stack_id::child_model_dynamics);

        const auto c_id  = compo.children.get_id(c);
        const auto c_idx = get_index(c_id);

        auto& param = compo.children_parameters[c_idx];
        param.clear();

        return for_first_member(
                 val,
                 "dynamics"sv,
                 [&](const auto& value) noexcept -> bool {
                     return dispatch(
                       mdl,
                       [&]<typename Dynamics>(Dynamics& dyn) noexcept -> bool {
                           if constexpr (std::is_same_v<Dynamics,
                                                        hsm_wrapper>) {
                               return read_modeling_dynamics(value, dyn);
                           } else {
                               return read_dynamics(value, dyn);
                           }
                       });
                 }) &&
                 [&param, &mdl]() -> bool {
            param.copy_from(mdl);
            return true;
        }();
    }

    bool read_child_model(const rapidjson::Value& val,
                          generic_component&      compo,
                          const dynamics_type     type,
                          child&                  c) noexcept
    {
        auto_stack a(this, stack_id::child_model);

        c.type        = child_type::model;
        c.id.mdl_type = type;

        model mdl;
        mdl.type = type;

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
            new (&dyn) Dynamics{};

            if constexpr (has_input_port<Dynamics>)
                for (int i = 0, e = length(dyn.x); i != e; ++i)
                    dyn.x[i] = undefined<message_id>();

            if constexpr (has_output_port<Dynamics>)
                for (int i = 0, e = length(dyn.y); i != e; ++i)
                    dyn.y[i] = undefined<node_id>();
        });

        return read_child_model_dynamics(val, compo, c, mdl);
    }

    bool copy_internal_component(internal_component type,
                                 component_id&      c_id) noexcept
    {
        auto_stack a(this, stack_id::copy_internal_component);

        component* compo = nullptr;
        while (mod().components.next(compo)) {
            if (compo->type == component_type::internal &&
                compo->id.internal_id == type) {
                c_id = mod().components.get_id(*compo);
                return true;
            }
        }

        report_json_error(error_id::modeling_internal_component_missing);
    }

    bool read_child_internal_component(const rapidjson::Value& val,
                                       component_id&           c_id) noexcept
    {
        auto_stack a(this, stack_id::child_internal_component);

        internal_component compo = internal_component::qss1_izhikevich;

        return read_temp_string(val) && copy_to(compo) &&
               copy_internal_component(compo, c_id);
    }

    auto search_reg(std::string_view name) noexcept -> registred_path*
    {
        registred_path* reg = nullptr;
        while (mod().registred_paths.next(reg))
            if (name == reg->name.sv())
                return reg;

        return nullptr;
    }

    auto search_dir_in_reg(registred_path&  reg,
                           std::string_view name) noexcept -> dir_path*
    {
        for (auto dir_id : reg.children) {
            if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                if (name == dir->path.sv())
                    return dir;
            }
        }

        return nullptr;
    }

    bool search_dir(std::string_view name, dir_path_id& out) noexcept
    {
        auto_stack s(this, stack_id::search_directory);

        for (auto reg_id : mod().component_repertories) {
            if (auto* reg = mod().registred_paths.try_to_get(reg_id); reg) {
                for (auto dir_id : reg->children) {
                    if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                        if (dir->path.sv() == name) {
                            out = dir_id;
                            return true;
                        }
                    }
                }
            }
        }

        report_json_error(error_id::directory_not_found);
    }

    bool search_file(dir_path_id      id,
                     std::string_view file_name,
                     file_path_id&    out) noexcept
    {
        auto_stack s(this, stack_id::search_file_in_directory);

        if (auto* dir = mod().dir_paths.try_to_get(id); dir) {
            for (int i = 0, e = dir->children.ssize(); i != e; ++i) {
                if (auto* f = mod().file_paths.try_to_get(dir->children[i]);
                    f) {
                    if (f->path.sv() == file_name) {
                        out = dir->children[i];
                        return true;
                    }
                }
            }
        }

        report_json_error(error_id::file_not_found);
    }

    auto search_dir(std::string_view name) noexcept -> dir_path*
    {
        for (auto reg_id : mod().component_repertories) {
            if (auto* reg = mod().registred_paths.try_to_get(reg_id); reg) {
                for (auto dir_id : reg->children) {
                    if (auto* dir = mod().dir_paths.try_to_get(dir_id); dir) {
                        if (dir->path.sv() == name)
                            return dir;
                    }
                }
            }
        }

        return nullptr;
    }

    auto search_file(dir_path&        dir,
                     std::string_view name) noexcept -> file_path*
    {
        for (auto file_id : dir.children)
            if (auto* file = mod().file_paths.try_to_get(file_id); file)
                if (file->path.sv() == name)
                    return file;

        return nullptr;
    }

    auto search_component(std::string_view name) noexcept -> component*
    {
        component* c = nullptr;
        while (mod().components.next(c)) {
            if (c->name.sv() == name)
                return c;
        }

        return nullptr;
    }

    bool modeling_copy_component_id(const small_string<31>&   reg,
                                    const directory_path_str& dir,
                                    const file_path_str&      file,
                                    component_id&             c_id)
    {
        registred_path* reg_ptr  = search_reg(reg.sv());
        dir_path*       dir_ptr  = nullptr;
        file_path*      file_ptr = nullptr;

        if (reg_ptr)
            dir_ptr = search_dir_in_reg(*reg_ptr, dir.sv());

        if (!dir_ptr)
            dir_ptr = search_dir(dir.sv());

        if (dir_ptr)
            file_ptr = search_file(*dir_ptr, file.sv());

        if (file_ptr) {
            c_id = file_ptr->component;
            return true;
        }

        if (auto* c = search_component(file.sv()); c) {
            const auto id = mod().components.get_id(*c);
            if (c->state == component_status::unread) {
                append_dependency(id);
                report_json_error(error_id::unknown_element);
            }

            c_id = id;

            return true;
        }

        report_json_error(error_id::modeling_component_missing);
    }

    bool read_child_simple_or_grid_component(const rapidjson::Value& val,
                                             component_id& c_id) noexcept
    {
        auto_stack a(this, stack_id::child_simple_or_grid_component);

        name_str           reg_name;
        directory_path_str dir_path;
        file_path_str      file_path;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("path"sv == name)
                         return read_temp_string(value) && copy_to(reg_name);

                     if ("directory"sv == name)
                         return read_temp_string(value) && copy_to(dir_path);

                     if ("file"sv == name)
                         return read_temp_string(value) && copy_to(file_path);

                     return true;
                 }) &&
               modeling_copy_component_id(reg_name, dir_path, file_path, c_id);
    }

    bool dispatch_child_component_type(const rapidjson::Value& val,
                                       component_type          type,
                                       component_id&           c_id) noexcept
    {
        auto_stack a(this, stack_id::dispatch_child_component_type);

        switch (type) {
        case component_type::none:
            return true;

        case component_type::internal:
            return read_child_internal_component(val, c_id);

        case component_type::simple:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::grid:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::graph:
            return read_child_simple_or_grid_component(val, c_id);

        case component_type::hsm:
            return read_child_simple_or_grid_component(val, c_id);
        }

        report_json_error(error_id::unknown_element);
    }

    bool read_child_component(const rapidjson::Value& val,
                              component_id&           c_id) noexcept
    {
        auto_stack a(this, stack_id::read_child_component);

        component_type type = component_type::none;

        return for_first_member(
          val, "component-type", [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) && copy_to(type) &&
                     dispatch_child_component_type(val, type, c_id);
          });
    }

    bool dispatch_child_component_or_model(const rapidjson::Value& val,
                                           generic_component&      generic,
                                           dynamics_type           d_type,
                                           child&                  c) noexcept
    {
        auto_stack a(this, stack_id::dispatch_child_component_or_model);

        return c.type == child_type::component
                 ? read_child_component(val, c.id.compo_id)
                 : read_child_model(val, generic, d_type, c);
    }

    bool read_child_component_or_model(const rapidjson::Value& val,
                                       generic_component&      generic,
                                       child&                  c) noexcept
    {
        auto_stack a(this, stack_id::child_component_or_model);

        dynamics_type type = dynamics_type::constant;

        return for_first_member(val,
                                "type"sv,
                                [&](const auto& value) noexcept -> bool {
                                    return read_temp_string(value) &&
                                           copy_to(c.type, type);
                                }) &&
               dispatch_child_component_or_model(val, generic, type, c);
    }

    bool read_children_array(const rapidjson::Value& val,
                             generic_component&      generic) noexcept
    {
        auto_stack a(this, stack_id::children_array);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               generic_can_alloc(generic, size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& new_child    = generic.children.alloc();
                     auto  new_child_id = generic.children.get_id(new_child);
                     return read_child(
                              value, generic, new_child, new_child_id) &&
                            read_child_component_or_model(
                              value, generic, new_child);
                 });
    }

    bool cache_model_mapping_sort() noexcept
    {
        cache().model_mapping.sort();

        return true;
    }

    bool read_children(const rapidjson::Value& val,
                       generic_component&      generic) noexcept
    {
        auto_stack a(this, stack_id::children);

        return read_children_array(val, generic) && cache_model_mapping_sort();
    }

    bool constant_sources_can_alloc(external_source&   srcs,
                                    std::integral auto i) noexcept
    {
        if (srcs.constant_sources.can_alloc(i))
            return true;

        report_json_error(error_id::srcs_constant_sources_not_enough);
    }

    bool text_file_sources_can_alloc(external_source&   srcs,
                                     std::integral auto i) noexcept
    {
        if (srcs.text_file_sources.can_alloc(i))
            return true;

        report_json_error(error_id::srcs_text_file_sources_not_enough);
    }

    bool binary_file_sources_can_alloc(external_source&   srcs,
                                       std::integral auto i) noexcept
    {
        if (srcs.binary_file_sources.can_alloc(i))
            return true;

        report_json_error(error_id::srcs_binary_file_sources_not_enough);
    }

    bool random_sources_can_alloc(external_source&   srcs,
                                  std::integral auto i) noexcept
    {
        if (srcs.random_sources.can_alloc(i))
            return true;

        report_json_error(error_id::srcs_random_sources_not_enough);
    }

    bool constant_buffer_size_can_alloc(std::integral auto i) noexcept
    {
        if (std::cmp_greater_equal(i, 0) &&
            std::cmp_less(i, external_source_chunk_size))
            return true;

        report_json_error(error_id::srcs_constant_sources_buffer_not_enough);
    }

    bool read_constant_source(const rapidjson::Value& val,
                              constant_source&        src) noexcept
    {
        auto_stack s(this, stack_id::srcs_constant_source);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               constant_buffer_size_can_alloc(len) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     src.length = static_cast<u32>(i);
                     return read_temp_real(value) && copy_to(src.buffer[i]);
                 });
    }

    bool cache_constant_mapping_add(u64                id_in_file,
                                    constant_source_id id) noexcept
    {
        cache().constant_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool read_constant_sources(const rapidjson::Value& val,
                               external_source&        srcs) noexcept
    {
        auto_stack s(this, stack_id::srcs_constant_source);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len),
               constant_sources_can_alloc(srcs, len) &&
                 for_each_array(
                   val,
                   [&](const auto /*i*/, const auto& value) noexcept -> bool {
                       auto& cst = srcs.constant_sources.alloc();
                       auto  id  = srcs.constant_sources.get_id(cst);
                       std::optional<u64> id_in_file;

                       return for_each_member(
                                value,
                                [&](const auto  name,
                                    const auto& value) noexcept -> bool {
                                    if ("id"sv == name)
                                        return read_temp_unsigned_integer(
                                                 value) &&
                                               copy_to(id_in_file);

                                    if ("parameters"sv == name)
                                        return read_constant_source(value, cst);

                                    return true;
                                }) &&
                              optional_has_value(id_in_file) &&
                              cache_constant_mapping_add(*id_in_file, id);
                   });
    }

    bool cache_text_file_mapping_add(u64                 id_in_file,
                                     text_file_source_id id) noexcept
    {
        cache().text_file_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool read_text_file_sources(const rapidjson::Value& val,
                                external_source&        srcs) noexcept
    {
        auto_stack s(this, stack_id::srcs_text_file_sources);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               text_file_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& text = srcs.text_file_sources.alloc();
                     auto  id   = srcs.text_file_sources.get_id(text);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, stack_id::srcs_text_file_source);

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_unsigned_integer(
                                               value) &&
                                             copy_to(id_in_file);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(text);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_text_file_mapping_add(*id_in_file, id);
                 });
    }

    bool cache_binary_file_mapping_add(u64                   id_in_file,
                                       binary_file_source_id id) noexcept
    {
        cache().binary_file_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool read_binary_file_sources(const rapidjson::Value& val,
                                  external_source&        srcs) noexcept
    {
        auto_stack s(this, stack_id::srcs_binary_file_sources);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               binary_file_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& text = srcs.binary_file_sources.alloc();
                     auto  id   = srcs.binary_file_sources.get_id(text);
                     std::optional<u64> id_in_file;

                     auto_stack s(this, stack_id::srcs_binary_file_source);

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_unsigned_integer(
                                               value) &&
                                             copy_to(id_in_file);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(text);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_binary_file_mapping_add(*id_in_file, id);
                 });
    }

    bool read_distribution_type(const rapidjson::Value& val,
                                random_source&          r) noexcept
    {
        auto_stack s(this, stack_id::srcs_random_source_distribution);

        switch (r.distribution) {
        case distribution_type::uniform_int:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_integer(value) && copy_to(r.a32);
                  if ("b"sv == name)
                      return read_temp_integer(value) && copy_to(r.b32);
                  return true;
              });

        case distribution_type::uniform_real:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_real(value) && copy_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_to(r.b);
                  return true;
              });

        case distribution_type::bernouilli:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_to(r.p);
                  return true;
              });

        case distribution_type::binomial:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("t"sv == name)
                      return read_temp_integer(value) && copy_to(r.t32);
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_to(r.p);
                  return true;
              });

        case distribution_type::negative_binomial:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("t"sv == name)
                      return read_temp_integer(value) && copy_to(r.t32);
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_to(r.p);
                  return true;
              });

        case distribution_type::geometric:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("p"sv == name)
                      return read_temp_real(value) && copy_to(r.p);
                  return true;
              });

        case distribution_type::poisson:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("mean"sv == name)
                      return read_temp_real(value) && copy_to(r.mean);
                  return true;
              });

        case distribution_type::exponential:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("lambda"sv == name)
                      return read_temp_real(value) && copy_to(r.lambda);
                  return true;
              });

        case distribution_type::gamma:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("alpha"sv == name)
                      return read_temp_integer(value) && copy_to(r.alpha);
                  if ("beta"sv == name)
                      return read_temp_real(value) && copy_to(r.beta);
                  return true;
              });

        case distribution_type::weibull:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_integer(value) && copy_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_to(r.b);
                  return true;
              });

        case distribution_type::exterme_value:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_integer(value) && copy_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_to(r.b);
                  return true;
              });

        case distribution_type::normal:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("mean"sv == name)
                      return read_temp_integer(value) && copy_to(r.mean);
                  if ("stddev"sv == name)
                      return read_temp_real(value) && copy_to(r.stddev);
                  return true;
              });

        case distribution_type::lognormal:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("m"sv == name)
                      return read_temp_integer(value) && copy_to(r.m);
                  if ("s"sv == name)
                      return read_temp_real(value) && copy_to(r.s);
                  return true;
              });

        case distribution_type::chi_squared:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("n"sv == name)
                      return read_temp_integer(value) && copy_to(r.n);
                  return true;
              });

        case distribution_type::cauchy:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("a"sv == name)
                      return read_temp_integer(value) && copy_to(r.a);
                  if ("b"sv == name)
                      return read_temp_real(value) && copy_to(r.b);
                  return true;
              });

        case distribution_type::fisher_f:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("m"sv == name)
                      return read_temp_integer(value) && copy_to(r.m);
                  if ("n"sv == name)
                      return read_temp_real(value) && copy_to(r.n);
                  return true;
              });

        case distribution_type::student_t:
            return for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("n"sv == name)
                      return read_temp_real(value) && copy_to(r.n);
                  return true;
              });
        }

        unreachable();
    }

    bool cache_random_mapping_add(u64 id_in_file, random_source_id id) noexcept
    {
        cache().random_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool read_random_sources(const rapidjson::Value& val,
                             external_source&        srcs) noexcept
    {
        auto_stack s(this, stack_id::srcs_random_sources);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               random_sources_can_alloc(srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& r  = srcs.random_sources.alloc();
                     auto  id = srcs.random_sources.get_id(r);

                     std::optional<u64> id_in_file;

                     auto_stack s(this, stack_id::srcs_random_source);

                     return for_each_member(
                              value,
                              [&](const auto  name,
                                  const auto& value) noexcept -> bool {
                                  if ("id"sv == name)
                                      return read_temp_unsigned_integer(
                                               value) &&
                                             copy_to(id_in_file);

                                  if ("type"sv == name) {
                                      return read_temp_string(value) &&
                                             copy_to(r.distribution) &&
                                             read_distribution_type(value, r);
                                  }

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_random_mapping_add(*id_in_file, id);
                 });
    }

    bool read_internal_component(const rapidjson::Value& val,
                                 component&              compo) noexcept
    {
        auto_stack a(this, stack_id::internal_component);

        return for_first_member(
          val, "component"sv, [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) && copy_to(compo.id.internal_id);
          });
    }

    bool modeling_connect(generic_component&     compo,
                          const child_id         src,
                          const connection::port p_src,
                          const child_id         dst,
                          const connection::port p_dst) noexcept
    {
        auto_stack a(this, stack_id::component_generic_connect);

        if (auto* c_src = compo.children.try_to_get(src); c_src)
            if (auto* c_dst = compo.children.try_to_get(dst); c_dst)
                if (compo.connect(mod(), *c_src, p_src, *c_dst, p_dst))
                    return true;

        report_json_error(error_id::modeling_connect_error);
    }

    bool modeling_connect_input(generic_component& compo,
                                port_id            src_port,
                                child_id           dst,
                                connection::port   p_dst) noexcept
    {
        auto_stack a(this, stack_id::component_generic_connect_input);

        if (auto* c_dst = compo.children.try_to_get(dst); c_dst)
            if (compo.connect_input(src_port, *c_dst, p_dst))
                return true;

        report_json_error(error_id::modeling_connect_error);
    }

    bool modeling_connect_output(generic_component& compo,
                                 child_id           src,
                                 connection::port   p_src,
                                 port_id            dst_port) noexcept
    {
        auto_stack a(this, stack_id::component_generic_connect_output);

        if (auto* c_src = compo.children.try_to_get(src); c_src)
            if (compo.connect_output(dst_port, *c_src, p_src))
                return true;

        report_json_error(error_id::modeling_connect_error);
    }

    bool cache_model_mapping_to(child_id& dst) noexcept
    {
        if (auto* elem = cache().model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool cache_model_mapping_to(std::optional<child_id>& dst) noexcept
    {
        if (auto* elem = cache().model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool get_x_port(generic_component&                generic,
                    const child_id                    dst_id,
                    const std::optional<std::string>& dst_str_port,
                    const std::optional<int>&         dst_int_port,
                    std::optional<connection::port>&  out) noexcept
    {
        auto_stack a(this, stack_id::component_generic_x_port);

        if (auto* child = generic.children.try_to_get(dst_id); child) {
            if (dst_int_port.has_value()) {
                if (child->type != child_type::model)
                    report_json_error(
                      error_id::generic_component_error_port_identifier);

                out = std::make_optional(
                  connection::port{ .model = *dst_int_port });
                return true;
            } else if (dst_str_port.has_value()) {
                if (child->type != child_type::component)
                    report_json_error(
                      error_id::generic_component_error_port_identifier);

                auto* compo = mod().components.try_to_get(child->id.compo_id);
                if (!compo)
                    report_json_error(
                      error_id::generic_component_unknown_component);

                auto p_id = compo->get_x(*dst_str_port);
                if (is_undefined(p_id))
                    report_json_error(
                      error_id::generic_component_unknown_component_x_port);

                out = std::make_optional(connection::port{ .compo = p_id });
                return true;
            } else {
                unreachable();
            }
        }

        report_json_error(error_id::unknown_element);
    }

    bool get_y_port(generic_component&                generic,
                    const child_id                    src_id,
                    const std::optional<std::string>& src_str_port,
                    const std::optional<int>&         src_int_port,
                    std::optional<connection::port>&  out) noexcept
    {
        auto_stack a(this, stack_id::component_generic_y_port);

        if (auto* child = generic.children.try_to_get(src_id); child) {
            if (src_int_port.has_value()) {
                if (child->type != child_type::model)
                    report_json_error(
                      error_id::generic_component_error_port_identifier);

                out = std::make_optional(
                  connection::port{ .model = *src_int_port });
                return true;
            } else if (src_str_port.has_value()) {
                if (child->type != child_type::component)
                    report_json_error(
                      error_id::generic_component_error_port_identifier);

                auto* compo = mod().components.try_to_get(child->id.compo_id);
                if (!compo)
                    report_json_error(
                      error_id::generic_component_unknown_component);

                auto p_id = compo->get_y(*src_str_port);
                if (is_undefined(p_id))
                    report_json_error(
                      error_id::generic_component_unknown_component_y_port);

                out = std::make_optional(connection::port{ .compo = p_id });
                return true;
            } else {
                unreachable();
            }
        }

        report_json_error(error_id::unknown_element);
    }

    bool get_x_port(component&                        compo,
                    const std::optional<std::string>& str_port,
                    std::optional<port_id>&           out) noexcept
    {
        if (!str_port.has_value())
            report_json_error(error_id::missing_mandatory_arg);

        auto port_id = compo.get_x(*str_port);
        if (is_undefined(port_id))
            report_json_error(
              error_id::generic_component_unknown_component_x_port);

        out = port_id;
        return true;
    }

    bool get_y_port(component&                        compo,
                    const std::optional<std::string>& str_port,
                    std::optional<port_id>&           out) noexcept
    {
        if (!str_port.has_value())
            report_json_error(error_id::missing_mandatory_arg);

        auto port_id = compo.get_y(*str_port);
        if (is_undefined(port_id))
            report_json_error(
              error_id::generic_component_unknown_component_y_port);

        out = port_id;
        return true;
    }

    bool read_internal_connection(const rapidjson::Value& val,
                                  generic_component&      gen) noexcept
    {
        auto_stack s(this, stack_id::component_generic_internal_connection);

        std::optional<child_id>         src_id;
        std::optional<child_id>         dst_id;
        std::optional<std::string>      src_str_port;
        std::optional<std::string>      dst_str_port;
        std::optional<int>              src_int_port;
        std::optional<int>              dst_int_port;
        std::optional<connection::port> src_port;
        std::optional<connection::port> dst_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(src_id);

                     if ("destination"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(dst_id);

                     if ("port-source"sv == name)
                         return value.IsString() ? read_temp_string(value) &&
                                                     copy_to(src_str_port)
                                                 : read_temp_integer(value) &&
                                                     copy_to(src_int_port);

                     if ("port-destination"sv == name)
                         return value.IsString() ? read_temp_string(value) &&
                                                     copy_to(dst_str_port)
                                                 : read_temp_integer(value) &&
                                                     copy_to(dst_int_port);

                     return true;
                 }) &&
               optional_has_value(src_id) &&
               get_y_port(gen, *src_id, src_str_port, src_int_port, src_port) &&
               optional_has_value(dst_id) &&
               get_x_port(gen, *dst_id, dst_str_port, dst_int_port, dst_port) &&
               optional_has_value(src_port) && optional_has_value(dst_port) &&
               gen.connections.can_alloc() &&
               modeling_connect(gen, *src_id, *src_port, *dst_id, *dst_port);
    }

    bool read_output_connection(const rapidjson::Value& val,
                                component&              compo,
                                generic_component&      gen) noexcept
    {
        auto_stack s(this, stack_id::component_generic_output_connection);

        child_id src_id = undefined<child_id>();

        std::optional<connection::port> src_port;
        std::optional<std::string>      src_str_port;
        std::optional<int>              src_int_port;

        std::optional<port_id>     port;
        std::optional<std::string> str_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(src_id);
                     if ("port-source"sv == name)
                         return value.IsString() ? read_temp_string(value) &&
                                                     copy_to(src_str_port)
                                                 : read_temp_integer(value) &&
                                                     copy_to(src_int_port);
                     if ("port"sv == name)
                         return read_temp_string(value) && copy_to(str_port);

                     return true;
                 }) &&
               get_y_port(compo, str_port, port) &&
               get_y_port(gen, src_id, src_str_port, src_int_port, src_port) &&
               gen.connections.can_alloc() && optional_has_value(src_port) &&
               optional_has_value(port) &&
               modeling_connect_output(gen, src_id, *src_port, *port);
    }

    bool read_input_connection(const rapidjson::Value& val,
                               component&              compo,
                               generic_component&      gen) noexcept
    {
        auto_stack s(this, stack_id::component_generic_input_connection);

        child_id dst_id = undefined<child_id>();

        std::optional<connection::port> dst_port;
        std::optional<std::string>      dst_str_port;
        std::optional<int>              dst_int_port;

        std::optional<port_id>     port;
        std::optional<std::string> str_port;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("destination"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(dst_id);
                     if ("port-destination"sv == name)
                         return value.IsString() ? read_temp_string(value) &&
                                                     copy_to(dst_str_port)
                                                 : read_temp_integer(value) &&
                                                     copy_to(dst_int_port);

                     if ("port"sv == name)
                         return read_temp_string(value) && copy_to(str_port);

                     return true;
                 }) &&
               get_x_port(compo, str_port, port) &&
               get_x_port(gen, dst_id, dst_str_port, dst_int_port, dst_port) &&
               gen.connections.can_alloc() && optional_has_value(dst_port) &&
               optional_has_value(port) &&
               modeling_connect_input(gen, *port, dst_id, *dst_port);
    }

    bool dispatch_connection_type(const rapidjson::Value& val,
                                  connection_type         type,
                                  component&              compo,
                                  generic_component&      gen) noexcept
    {
        auto_stack s(this, stack_id::component_generic_dispatch_connection);

        switch (type) {
        case connection_type::internal:
            return read_internal_connection(val, gen);
        case connection_type::output:
            return read_output_connection(val, compo, gen);
        case connection_type::input:
            return read_input_connection(val, compo, gen);
        }

        unreachable();
    }

    bool read_connections(const rapidjson::Value& val,
                          component&              compo,
                          generic_component&      gen) noexcept
    {
        auto_stack s(this, stack_id::component_generic_connections);

        return is_value_array(val) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& val_con) noexcept -> bool {
                     return for_each_member(
                       val_con,
                       [&](const auto  name,
                           const auto& value) noexcept -> bool {
                           if ("type"sv == name) {
                               connection_type type = connection_type::internal;
                               return read_temp_string(value) &&
                                      copy_to(type) &&
                                      dispatch_connection_type(
                                        val_con, type, compo, gen);
                           }

                           return true;
                       });
                 });
    }

    bool read_generic_component(const rapidjson::Value& val,
                                component&              compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic);

        auto& generic       = mod().generic_components.alloc();
        compo.type          = component_type::simple;
        compo.id.generic_id = mod().generic_components.get_id(generic);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("next-unique-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(generic.next_unique_id);

              if ("children"sv == name)
                  return read_children(value, generic);

              if ("connections"sv == name)
                  return read_connections(value, compo, generic);

              return true;
          });
    }

    bool grid_children_add(vector<component_id>& out,
                           component_id          c_id) noexcept
    {
        out.emplace_back(c_id);

        return true;
    }

    bool grid_children_add(
      data_array<graph_component::vertex, graph_component::vertex_id>& out,
      component_id c_id) noexcept
    {
        if (not out.can_alloc()) {
            out.reserve(out.capacity() * 2);

            if (not out.can_alloc())
                return false;
        }

        out.alloc(c_id);

        return true;
    }

    bool read_grid_children(const rapidjson::Value& val,
                            grid_component&         compo) noexcept
    {
        auto_stack s(this, stack_id::component_grid_children);

        return is_value_array(val) &&
               is_value_array_size_equal(val, compo.row * compo.column) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     component_id c_id = undefined<component_id>();

                     return read_child_component(value, c_id) &&
                            grid_children_add(compo.children, c_id);
                 });
    }

    bool dispatch_graph_type(const rapidjson::Value& val,
                             const rapidjson::Value& name,
                             graph_component&        graph) noexcept
    {
        auto_stack s(this, stack_id::component_graph_type);

        debug::ensure(name.IsString());

        if ("dot-file"sv == name.GetString()) {
            graph.param.dot = graph_component::dot_file_param{};
            return read_graph_param(val, graph.param.dot);
        }

        if ("scale-free"sv == name.GetString()) {
            graph.param.scale = graph_component::scale_free_param{};
            return read_graph_param(val, graph.param.scale);
        }

        if ("small-world"sv == name.GetString()) {
            graph.param.small = graph_component::small_world_param{};
            return read_graph_param(val, graph.param.small);
        }

        report_json_error(error_id::graph_component_type_error);
    }

    bool read_graph_param(const rapidjson::Value&          val,
                          graph_component::dot_file_param& p) noexcept
    {
        auto_stack s(this, stack_id::component_graph_param);

        directory_path_str dir_path;
        file_path_str      file_path;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("dir"sv == name)
                         return read_temp_string(value) && copy_to(dir_path);

                     if ("file"sv == name)
                         return read_temp_string(value) && copy_to(file_path);

                     return true;
                 }) &&
               search_dir(dir_path.sv(), p.dir) &&
               search_file(p.dir, file_path.sv(), p.file);
    }

    bool read_graph_param(const rapidjson::Value&            val,
                          graph_component::scale_free_param& p) noexcept
    {
        auto_stack s(this, stack_id::component_graph_param);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("alpha"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_to(p.alpha);

              if ("beta"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_to(p.beta);

              return true;
          });
    }

    bool read_graph_param(const rapidjson::Value&             val,
                          graph_component::small_world_param& p) noexcept
    {
        auto_stack s(this, stack_id::component_graph_param);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("probability"sv == name)
                  return read_temp_real(value) && is_double_greater_than(0) &&
                         copy_to(p.probability);

              if ("k"sv == name)
                  return read_temp_integer(value) &&
                         is_int_greater_equal_than(1) && copy_to(p.k);

              return true;
          });
    }

    bool read_graph_children(const rapidjson::Value& val,
                             graph_component&        compo) noexcept
    {
        auto_stack s(this, stack_id::component_graph_children);

        compo.children.clear();

        return for_each_array(
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              component_id c_id = undefined<component_id>();

              return read_child_component(value, c_id) &&
                     grid_children_add(compo.children, c_id);
          });
    }

    bool is_grid_valid(const grid_component& grid) noexcept
    {
        if (grid.row * grid.column == grid.children.ssize())
            return true;

        report_json_error(error_id::grid_component_size_error);
    }

    bool read_grid_component(const rapidjson::Value& val,
                             component&              compo) noexcept
    {
        auto_stack s(this, stack_id::component_grid);

        auto& grid       = mod().grid_components.alloc();
        compo.type       = component_type::grid;
        compo.id.grid_id = mod().grid_components.get_id(grid);

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("rows"sv == name)
                         return read_temp_integer(value) &&
                                is_int_greater_equal_than(1) &&
                                is_int_less_than(grid_component::row_max) &&
                                copy_to(grid.row);

                     if ("columns"sv == name)
                         return read_temp_integer(value) &&
                                is_int_greater_equal_than(1) &&
                                is_int_less_than(grid_component::row_max) &&
                                copy_to(grid.column);

                     if ("in-connection-type"sv == name)
                         return read_temp_integer(value) &&
                                copy_to(grid.in_connection_type);

                     if ("out-connection-type"sv == name)
                         return read_temp_integer(value) &&
                                copy_to(grid.out_connection_type);

                     if ("children"sv == name)
                         return read_grid_children(value, grid);

                     return true;
                 }) &&
               is_grid_valid(grid);
    }

    bool read_graph_component(const rapidjson::Value& val,
                              component&              compo) noexcept
    {
        auto_stack s(this, stack_id::component_graph);

        auto& graph       = mod().graph_components.alloc();
        compo.type        = component_type::graph;
        compo.id.graph_id = mod().graph_components.get_id(graph);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("graph-type"sv == name)
                  return value.IsString() &&
                         dispatch_graph_type(val, value, graph);

              if ("children"sv == name)
                  return read_graph_children(value, graph);

              return true;
          });
    }

    bool read_hsm_component(const rapidjson::Value& val,
                            component&              compo) noexcept
    {
        auto_stack s(this, stack_id::component_hsm);

        auto& hsm       = mod().hsm_components.alloc();
        compo.type      = component_type::hsm;
        compo.id.hsm_id = mod().hsm_components.get_id(hsm);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("states"sv == name)
                  return read_hsm_states(value, hsm.states);

              if ("top"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(hsm.top_state);
              return true;
          });
    }

    bool dispatch_component_type(const rapidjson::Value& val,
                                 component&              compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            return true;

        case component_type::internal:
            return read_internal_component(val, compo);

        case component_type::simple:
            return read_generic_component(val, compo);

        case component_type::grid:
            return read_grid_component(val, compo);

        case component_type::graph:
            return read_graph_component(val, compo);

        case component_type::hsm:
            return read_hsm_component(val, compo);
        }

        report_json_error(error_id::unknown_element);
    }

    bool convert_to_component(component& compo) noexcept
    {
        if (auto type = get_component_type(temp_string); type.has_value()) {
            compo.type = type.value();
            return true;
        }

        report_json_error(error_id::missing_component_type);
    }

    bool read_ports(const rapidjson::Value& val,
                    id_array<port_id>&      port,
                    vector<port_str>&       names) noexcept
    {
        auto_stack s(this, stack_id::component_ports);

        return is_value_array(val) && port.can_alloc(val.GetArray().Size()) &&
               buffer_reserve(val.GetArray().Size(), names) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     if (read_temp_string(value)) {
                         auto port_id              = port.alloc();
                         names[get_index(port_id)] = temp_string;
                         return true;
                     }

                     report_json_error(error_id::missing_string);
                 });
    }

    bool read_component_colors(const rapidjson::Value& val,
                               std::array<float, 4>&   color) noexcept
    {
        auto_stack s(this, stack_id::component_color);

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_to(color[i]);
                 });
    }

    bool read_component(const rapidjson::Value& val, component& compo) noexcept
    {
        auto_stack s(this, stack_id::component);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(compo.name);
              if ("constant-sources"sv == name)
                  return read_constant_sources(value, mod().srcs);
              if ("binary-file-sources"sv == name)
                  return read_binary_file_sources(value, mod().srcs);
              if ("text-file-sources"sv == name)
                  return read_text_file_sources(value, mod().srcs);
              if ("random-sources"sv == name)
                  return read_random_sources(value, mod().srcs);
              if ("x"sv == name)
                  return read_ports(value, compo.x, compo.x_names);
              if ("y"sv == name)
                  return read_ports(value, compo.y, compo.y_names);
              if ("type"sv == name)
                  return read_temp_string(value) &&
                         convert_to_component(compo) &&
                         dispatch_component_type(val, compo);
              if ("colors"sv == name)
                  return read_component_colors(
                    value,
                    mod().component_colors[get_index(
                      mod().components.get_id(compo))]);

              return true;
          });
    }

    bool read_simulation_model_dynamics(const rapidjson::Value& val,
                                        model&                  mdl) noexcept
    {
        auto_stack s(this, stack_id::simulation_model_dynamics);

        return for_first_member(
          val, "dynamics"sv, [&](const auto& value) -> bool {
              return dispatch(
                mdl, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
                    new (&dyn) Dynamics{};

                    if constexpr (has_input_port<Dynamics>)
                        for (int i = 0, e = length(dyn.x); i != e; ++i)
                            dyn.x[i] = undefined<message_id>();

                    if constexpr (has_output_port<Dynamics>)
                        for (int i = 0, e = length(dyn.y); i != e; ++i)
                            dyn.y[i] = undefined<node_id>();

                    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                        return read_simulation_dynamics(value, dyn);
                    } else {
                        return read_dynamics(value, dyn);
                    }
                });
          });
    }

    bool cache_model_mapping_add(u64 id_in_file, u64 id) noexcept
    {
        cache().model_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool sim_hsms_mapping_clear() noexcept
    {
        cache().sim_hsms_mapping.data.clear();
        return true;
    }

    bool sim_hsms_mapping_add(const u64 id_in_file, const hsm_id id) noexcept
    {
        cache().sim_hsms_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool sim_hsms_mapping_get(const u64 id_in_file, hsm_id& id) noexcept
    {
        if (auto* ptr = cache().sim_hsms_mapping.get(id_in_file); ptr) {
            id = *ptr;
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool sim_hsms_mapping_sort() noexcept
    {
        cache().sim_hsms_mapping.sort();
        return true;
    }

    bool read_simulation_model(const rapidjson::Value& val, model& mdl) noexcept
    {
        auto_stack s(this, stack_id::simulation_model);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("type"sv == name)
                  return read_temp_string(value) && copy_to(mdl.type) &&
                         read_simulation_model_dynamics(val, mdl);

              if ("id"sv == name) {
                  std::optional<u64> id_in_file;

                  return read_temp_unsigned_integer(value) &&
                         copy_to(id_in_file) &&
                         optional_has_value(id_in_file) &&
                         cache_model_mapping_add(
                           *id_in_file, ordinal(sim().models.get_id(mdl)));
              }

              return true;
          });
    }

    bool read_simulation_hsm(const rapidjson::Value&     val,
                             hierarchical_state_machine& machine) noexcept
    {
        auto_stack s(this, stack_id::simulation_hsm);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("id"sv == name) {
                  const auto machine_id = sim().hsms.get_id(machine);
                  u64        id_in_file = 0;

                  return read_u64(value, id_in_file) &&
                         sim_hsms_mapping_add(id_in_file, machine_id);
              }

              if ("states"sv == name)
                  return read_hsm_states(value, machine.states);

              if ("top"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(machine.top_state);
              return true;
          });
    }

    bool sim_models_can_alloc(std::integral auto i) noexcept
    {
        if (sim().models.can_alloc(i))
            return true;

        report_json_error(error_id::simulation_models_not_enough);
    }

    bool sim_hsms_can_alloc(std::integral auto i) noexcept
    {
        if (sim().hsms.can_alloc(i))
            return true;

        report_json_error(error_id::simulation_hsms_not_enough);
    }

    bool read_simulation_hsms(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::simulation_hsms);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               sim_hsms_mapping_clear() && sim_hsms_can_alloc(len) &&
               for_each_array(
                 val,
                 [&](const auto /* i */, const auto& value) noexcept -> bool {
                     auto& hsm = sim().hsms.alloc();

                     return read_simulation_hsm(value, hsm);
                 }) &&
               sim_hsms_mapping_sort();
    }

    bool read_simulation_models(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::simulation_models);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               sim_models_can_alloc(len) &&
               for_each_array(
                 val,
                 [&](const auto /* i */, const auto& value) noexcept -> bool {
                     auto& mdl  = sim().models.alloc();
                     mdl.handle = invalid_heap_handle;

                     return read_simulation_model(value, mdl);
                 });
    }

    bool simulation_connect(u64 src, i8 port_src, u64 dst, i8 port_dst) noexcept
    {
        auto_stack s(this, stack_id::simulation_connect);

        auto* mdl_src_id = cache().model_mapping.get(src);
        if (!mdl_src_id)
            report_json_error(error_id::simulation_connect_src_unknown);

        auto* mdl_dst_id = cache().model_mapping.get(dst);
        if (!mdl_dst_id)
            report_json_error(error_id::simulation_connect_dst_unknown);

        auto* mdl_src =
          sim().models.try_to_get(enum_cast<model_id>(*mdl_src_id));
        if (!mdl_src)
            report_json_error(error_id::simulation_connect_src_unknown);

        auto* mdl_dst =
          sim().models.try_to_get(enum_cast<model_id>(*mdl_dst_id));
        if (!mdl_dst)
            report_json_error(error_id::simulation_connect_dst_unknown);

        node_id*    out = nullptr;
        message_id* in  = nullptr;

        if (auto ret = get_output_port(*mdl_src, port_src, out); !ret)
            report_json_error(error_id::simulation_connect_src_port_unknown);

        if (auto ret = get_input_port(*mdl_dst, port_dst, in); !ret)
            report_json_error(error_id::simulation_connect_dst_port_unknown);

        if (auto ret = sim().connect(*mdl_src, port_src, *mdl_dst, port_dst);
            !ret)
            report_json_error(error_id::simulation_connect_error);

        return true;
    }

    bool read_simulation_connection(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::simulation_connection);

        std::optional<u64> src;
        std::optional<u64> dst;
        std::optional<i8>  port_src;
        std::optional<i8>  port_dst;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                copy_to(src);

                     if ("port-source"sv == name)
                         return read_temp_integer(value) && copy_to(port_src);

                     if ("destination"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                copy_to(dst);

                     if ("port_destination"sv == name)
                         return read_temp_integer(value) && copy_to(port_dst);

                     return true;
                 }) &&
               optional_has_value(src) && optional_has_value(dst) &&
               optional_has_value(port_src) && optional_has_value(port_dst) &&
               simulation_connect(*src, *port_src, *dst, *port_dst);
    }

    bool read_simulation_connections(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::simulation_connections);

        return is_value_array(val) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_simulation_connection(value);
                 });
    }

    bool read_simulation(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::simulation);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("constant-sources"sv == name)
                  return read_constant_sources(value, sim().srcs);
              if ("binary-file-sources"sv == name)
                  return read_binary_file_sources(value, sim().srcs);
              if ("text-file-sources"sv == name)
                  return read_text_file_sources(value, sim().srcs);
              if ("random-sources"sv == name)
                  return read_random_sources(value, sim().srcs);
              if ("hsms"sv == name)
                  return read_simulation_hsms(value);
              if ("models"sv == name)
                  return read_simulation_models(value);
              if ("connections"sv == name)
                  return read_simulation_connections(value);

              report_json_error(error_id::unknown_element);
          });
    }

    bool project_set(component_id c_id) noexcept
    {
        auto_stack s(this, stack_id::project_set_components);

        if (auto* compo = mod().components.try_to_get(c_id); compo) {
            if (auto ret = pj().set(mod(), sim(), *compo); ret)
                return true;
            else
                report_json_error(error_id::project_set_error);
        } else
            report_json_error(error_id::project_set_no_head);
    }

    bool read_project_top_component(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_top_component);

        small_string<31>   reg_name;
        directory_path_str dir_path;
        file_path_str      file_path;
        component_id       c_id = undefined<component_id>();

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("component-path"sv == name)
                         return read_temp_string(value) && copy_to(reg_name);

                     if ("component-directory"sv == name)
                         return read_temp_string(value) && copy_to(dir_path);

                     if ("component-file"sv == name)
                         return read_temp_string(value) && copy_to(file_path);

                     return true;
                 }) &&
               modeling_copy_component_id(
                 reg_name, dir_path, file_path, c_id) &&
               project_set(c_id);
    }

    template<typename T>
    bool vector_add(vector<T>& vec, const T t) noexcept
    {
        vec.emplace_back(t);
        return true;
    }

    template<typename T>
    bool vector_not_empty(const vector<T>& vec) noexcept
    {
        return !vec.empty();
    }

    bool read_real_parameter(const rapidjson::Value& val,
                             std::array<real, 8>&    reals) noexcept
    {
        auto_stack s(this, stack_id::project_real_parameter);

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_to(reals[i]);
                 });
    }

    bool read_integer_parameter(const rapidjson::Value& val,
                                std::array<i64, 4>&     integers) noexcept
    {
        auto_stack s(this, stack_id::project_integer_parameter);

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_unsigned_integer(value) &&
                            copy_to(integers[i]);
                 });
    }

    bool read_parameter(const rapidjson::Value& val, parameter& param) noexcept
    {
        auto_stack s(this, stack_id::project_parameter);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("real"sv == name)
                  return read_real_parameter(value, param.reals);
              if ("integer"sv == name)
                  return read_integer_parameter(value, param.integers);

              return true;
          });
    }

    bool read_global_parameter(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameter);

        std::optional<std::string>  name_opt;
        std::optional<tree_node_id> tn_id_opt;
        std::optional<model_id>     mdl_id_opt;
        std::optional<parameter>    parameter_opt;

        bool success = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name) {
                  if (not read_temp_string(value))
                      return false;

                  name_opt = temp_string;

                  return true;
              }

              if ("access"sv == name) {
                  unique_id_path path;
                  if (not read_project_unique_id_path(val, path))
                      return false;

                  tree_node_id tn_id;
                  model_id     mdl_id;
                  if (not convert_to_tn_model_ids(path, tn_id, mdl_id))
                      return false;

                  tn_id_opt  = tn_id;
                  mdl_id_opt = mdl_id;

                  return true;
              }

              if ("parameter"sv == name) {
                  parameter p;
                  if (not read_parameter(value, p))
                      return false;

                  parameter_opt = p;
                  return true;
              }

              return true;
          });

        if (success and name_opt.has_value() and tn_id_opt.has_value() and
            mdl_id_opt.has_value() and parameter_opt.has_value()) {
            pj().parameters.alloc([&](auto /*id*/,
                                      auto& name,
                                      auto& tn_id,
                                      auto& mdl_id,
                                      auto& p) noexcept {
                name   = *name_opt;
                tn_id  = *tn_id_opt;
                mdl_id = *mdl_id_opt;
                p      = *parameter_opt;
            });

            return true;
        }

        return false;
    }

    bool read_global_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameters);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_global_parameters_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_global_parameter(value);
                 });
    }

    bool read_project_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_parameters);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("global"sv == name)
                  return read_global_parameters(value);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_project_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_parameters);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("global"sv == name)
                  return read_project_plot_observations(value);

              if ("grid"sv == name)
                  return read_project_grid_observations(value);

              report_json_error(error_id::unknown_element);
          });
    }

    bool convert_to_tn_model_ids(const unique_id_path& path,
                                 tree_node_id&         parent_id,
                                 model_id&             mdl_id) noexcept
    {
        auto_stack s(this, stack_id::project_convert_to_tn_model_ids);

        if (auto ret_opt = pj().get_model_path(path); ret_opt.has_value()) {
            parent_id = ret_opt->first;
            mdl_id    = ret_opt->second;
            return true;
        }

        report_json_error(
          error_id::project_fail_convert_access_to_tn_model_ids);
    }

    bool convert_to_tn_model_ids(const unique_id_path& path,
                                 vector<tree_node_id>& parent_id,
                                 vector<model_id>&     mdl_id) noexcept
    {
        auto_stack s(this, stack_id::project_convert_to_tn_model_ids);

        if (auto ret_opt = pj().get_model_path(path); ret_opt.has_value()) {
            parent_id.emplace_back(ret_opt->first);
            mdl_id.emplace_back(ret_opt->second);
            return true;
        }

        report_json_error(
          error_id::project_fail_convert_access_to_tn_model_ids);
    }

    bool convert_to_tn_id(const unique_id_path& path,
                          tree_node_id&         tn_id) noexcept
    {
        auto_stack s(this, stack_id::project_convert_to_tn_id);

        if (auto ret_opt = pj().get_tn_id(path); ret_opt.has_value()) {
            tn_id = *ret_opt;
            return true;
        }

        report_json_error(error_id::project_fail_convert_access_to_tn_id);
    }

    bool read_project_unique_id_path(const rapidjson::Value& val,
                                     unique_id_path&         out) noexcept
    {
        auto_stack s(this, stack_id::project_unique_id_path);

        return is_value_array(val) &&
               is_value_array_size_less(val, length(out) + 1) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_unsigned_integer(value) &&
                            copy_to(out[i]);
                 });
    }

    bool copy(const component_color& cc, color& c) noexcept
    {
        c = 0;
        c = (u32)((int)(std::clamp(cc[0], 0.f, 1.f) * 255.f + 0.5f));
        c |= ((u32)((int)(std::clamp(cc[1], 0.f, 1.f) * 255.0f + 0.5f)) << 8);
        c |= ((u32)((int)(std::clamp(cc[2], 0.f, 1.f) * 255.0f + 0.5f)) << 16);
        c |= ((u32)((int)(std::clamp(cc[3], 0.f, 1.f) * 255.0f + 0.5f)) << 24);
        return true;
    }

    bool read_color(const rapidjson::Value& val, color& c) noexcept
    {
        auto_stack      s(this, stack_id::load_color);
        component_color cc{};

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val,
                 [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_unsigned_integer(value) && copy_to(cc[i]);
                 }) &&
               copy(cc, c);
    }

    bool read_project_plot_observation_child(const rapidjson::Value& val,
                                             variable_observer& plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation_child);
        auto       tn_id  = undefined<tree_node_id>();
        auto       mdl_id = undefined<model_id>();
        auto       c      = color(0xff0ff);
        auto       t      = variable_observer::type_options::line;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;

                     if ("name"sv == name)
                         return read_temp_string(value) && copy_to(plot.name);

                     if ("access"sv == name)
                         return read_project_unique_id_path(val, path) &&
                                convert_to_tn_model_ids(path, tn_id, mdl_id);

                     if ("color"sv == name)
                         return read_color(value, c);

                     if ("type"sv == name)
                         return read_temp_string(value);

                     report_json_error(error_id::unknown_element);
                 }) &&
                 [&]() noexcept -> bool {
            if (is_defined(tn_id) and is_defined(mdl_id)) {
                plot.push_back(tn_id, mdl_id, c, t);
                return true;
            }
            return false;
        }();
    }

    bool copy_to(variable_observer::type_options& type) noexcept
    {
        if (temp_string == "line")
            type = variable_observer::type_options::line;

        if (temp_string == "dash")
            type = variable_observer::type_options::dash;

        report_json_error(error_id::unknown_element);
    }

    bool read_project_plot_observation_children(
      const rapidjson::Value& val,
      variable_observer&      plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation_children);

        return read_project_plot_observation_child(val, plot);
    }

    bool read_project_plot_observation(const rapidjson::Value& val,
                                       variable_observer&      plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(plot.name);

              if ("models"sv == name)
                  return read_project_plot_observation_children(value, plot);

              return true;
          });
    }

    bool read_project_plot_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observations);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_variable_observers_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& plot = pj().variable_observers.alloc();
                     return read_project_plot_observation(value, plot);
                 });
    }

    bool read_project_grid_observation(const rapidjson::Value& val,
                                       grid_observer&          grid) noexcept
    {
        auto_stack s(this, stack_id::project_grid_observation);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              unique_id_path path;

              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(grid.name);

              if ("grid"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_id(path, grid.parent_id);

              if ("access"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_model_ids(path, grid.tn_id, grid.mdl_id);

              return true;
          });
    }

    bool read_project_grid_observations(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_grid_observations);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_grid_observers_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& grid = pj().grid_observers.alloc();
                     return read_project_grid_observation(value, grid);
                 });
    }

    bool read_project(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project);

        return read_project_top_component(val) &&
               for_each_member(
                 val, [&](const auto name, const auto& value) noexcept -> bool {
                     if ("parameters"sv == name)
                         return read_project_parameters(value);

                     if ("observations"sv == name)
                         return read_project_observations(value);

                     return true;
                 });
    }

    void clear() noexcept
    {
        if (m_cache)
            m_cache->clear();

        temp_integer = 0;
        temp_u64     = 0;
        temp_double  = 0.0;
        temp_bool    = false;
        temp_string.clear();
        stack.clear();
        error = error_id::none;
    }

    cache_rw*   m_cache = nullptr;
    modeling*   m_mod   = nullptr;
    simulation* m_sim   = nullptr;
    project*    m_pj    = nullptr;

    vector<component_id> dependencies;

    void append_dependency(component_id id) noexcept
    {
        if (auto i = std::find(dependencies.begin(), dependencies.end(), id);
            i == dependencies.end())
            dependencies.emplace_back(id);
    }

    i64         temp_integer = 0;
    u64         temp_u64     = 0;
    double      temp_double  = 0.0;
    bool        temp_bool    = false;
    std::string temp_string;

    small_vector<stack_id, 16> stack;

    error_id error = error_id::none;

    reader(cache_rw& cache_, modeling& mod_) noexcept
      : m_cache(&cache_)
      , m_mod(&mod_)
    {}

    reader(cache_rw& cache_, simulation& mod_) noexcept
      : m_cache(&cache_)
      , m_sim(&mod_)
    {}

    reader(cache_rw&   cache_,
           modeling&   mod_,
           simulation& sim_,
           project&    pj_) noexcept
      : m_cache(&cache_)
      , m_mod(&mod_)
      , m_sim(&sim_)
      , m_pj(&pj_)
    {}

    struct auto_stack {
        auto_stack(reader* r_, const stack_id id) noexcept
          : r(r_)
        {
            debug::ensure(r->stack.can_alloc(1));
            r->stack.emplace_back(id);
        }

        ~auto_stack() noexcept
        {
            if (!r->have_error()) {
                debug::ensure(!r->stack.empty());
                r->stack.pop_back();
            }
        }

        reader* r = nullptr;
    };

    bool have_error() const noexcept { return error != error_id::none; }

    void show_stack() const noexcept
    {
        for (int i = 0, e = stack.ssize(); i != e; ++i)
            fmt::print("{:{}} {}\n", "", i, stack_id_names[ordinal(stack[i])]);
    }

    void show_state() const noexcept
    {
        fmt::print("error_id: {}\n", error_id_names[ordinal(error)]);
    }

    void show_error() const noexcept
    {
        show_state();
        show_stack();
    }

    modeling& mod() const noexcept
    {
        debug::ensure(m_mod);
        return *m_mod;
    }

    simulation& sim() const noexcept
    {
        debug::ensure(m_sim);
        return *m_sim;
    }

    project& pj() const noexcept
    {
        debug::ensure(m_pj);
        return *m_pj;
    }

    cache_rw& cache() const noexcept
    {
        debug::ensure(m_cache);
        return *m_cache;
    }

    bool return_true() const noexcept { return true; }
    bool return_false() const noexcept { return false; }
    bool can_warning() const noexcept { return !!cache().warning_cb; }

    bool warning(const std::string_view str, const log_level level) noexcept
    {
        if (can_warning())
            cache().warning_cb(str, ordinal(level));

        return true;
    }
};

static status read_file_to_buffer(cache_rw& cache, file& f) noexcept
{
    debug::ensure(f.is_open());
    debug::ensure(f.get_mode() == open_mode::read);

    if (const auto len = f.length(); len > 0) {
        cache.buffer.resize(len);

        if (!f.read(cache.buffer.data(), len))
            return new_error(json_archiver::part::read_file_error);
    }

    return success();
}

static status parse_json_data(const std::span<char>& buffer,
                              rapidjson::Document&   doc) noexcept
{
    doc.Parse(buffer.data(), buffer.size());

    if (doc.HasParseError())
        return new_error(
          json_archiver::part::json_format_error,
          e_json{ doc.GetErrorOffset(),
                  rapidjson::GetParseError_En(doc.GetParseError()) });

    return success();
}

//
//

template<typename Writer, int QssLevel>
static void write(Writer&                              writer,
                  const abstract_integrator<QssLevel>& dyn) noexcept
{
    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
}

template<typename Writer, int QssLevel>
static void write(Writer& writer,
                  const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_sum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_sum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_sum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_wsum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_wsum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_wsum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_sum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_sum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_sum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_wsum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_wsum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_wsum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_sum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_sum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_sum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_wsum_2& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_wsum_3& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_wsum_4& dyn) noexcept
{
    writer.StartObject();

    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);

    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);

    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const counter& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const dynamic_queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const priority_queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const generator& dyn) noexcept
{
    writer.StartObject();
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("source-value-type");
    writer.Int(ordinal(dyn.default_source_value.type));
    writer.Key("source-value-id");
    writer.Uint64(dyn.default_source_value.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const constant& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_value);
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    writer.Key("type");

    switch (dyn.type) {
    case constant::init_type::constant:
        writer.String("constant");
        break;
    case constant::init_type::incoming_component_all:
        writer.String("incoming_component_all");
        break;
    case constant::init_type::outcoming_component_all:
        writer.String("outcoming_component_all");
        break;
    case constant::init_type::incoming_component_n:
        writer.String("incoming_component_n");
        writer.Key("port");
        writer.Uint64(dyn.port);
        break;
    case constant::init_type::outcoming_component_n:
        writer.String("outcoming_component_n");
        writer.Key("port");
        writer.Uint64(dyn.port);
        break;
    }
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss1_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss2_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const qss3_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<typename Writer, int QssLevel>
static void write(Writer& writer,
                  const abstract_square<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer, int PortNumber>
static void write(Writer& writer,
                  const accumulator<PortNumber>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const time_func& dyn) noexcept
{
    writer.StartObject();
    writer.Key("function");
    writer.String(dyn.default_f == &time_function          ? "time"
                  : dyn.default_f == &square_time_function ? "square"
                                                           : "sin");
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const logical_and_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const logical_and_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const logical_or_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const logical_or_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const logical_invert& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
static void write(Writer& writer, const hsm_wrapper& dyn) noexcept
{
    writer.StartObject();
    writer.Key("hsm");
    writer.Uint64(get_index(dyn.id));
    writer.Key("a");
    writer.Int(dyn.exec.a);
    writer.Key("b");
    writer.Int(dyn.exec.b);
    writer.EndObject();
}

template<typename Writer>
static void write(modeling&          mod,
                  Writer&            writer,
                  const hsm_wrapper& dyn) noexcept
{
    writer.StartObject();
    writer.Key("hsm");
    write_child_component(mod, dyn.id, writer);

    writer.Uint64(ordinal(dyn.id));
    writer.Key("a");
    writer.Int(dyn.exec.a);
    writer.Key("b");
    writer.Int(dyn.exec.b);
    writer.EndObject();
}

template<typename Writer>
static void write(
  Writer&                                         writer,
  std::string_view                                name,
  const hierarchical_state_machine::state_action& state) noexcept
{
    writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
    writer.StartObject();
    writer.Key("parameter");
    writer.Int(state.parameter);
    writer.Key("var-1");
    writer.Int(static_cast<int>(state.var1));
    writer.Key("var-2");
    writer.Int(static_cast<int>(state.var2));
    writer.Key("type");
    writer.Int(static_cast<int>(state.type));
    writer.EndObject();
}

template<typename Writer>
static void write(
  Writer&                                             writer,
  std::string_view                                    name,
  const hierarchical_state_machine::condition_action& state) noexcept
{
    writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
    writer.StartObject();
    writer.Key("parameter");
    writer.Int(state.parameter);
    writer.Key("type");
    writer.Int(static_cast<int>(state.type));
    writer.Key("port");
    writer.Int(static_cast<int>(state.port));
    writer.Key("mask");
    writer.Int(static_cast<int>(state.mask));
    writer.EndObject();
}

void cache_rw::clear() noexcept
{
    buffer.clear();
    stack.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
    sim_hsms_mapping.data.clear();
}

void cache_rw::destroy() noexcept
{
    buffer.destroy();
    stack.destroy();

    model_mapping.data.destroy();
    constant_mapping.data.destroy();
    binary_file_mapping.data.destroy();
    random_mapping.data.destroy();
    text_file_mapping.data.destroy();
    sim_hsms_mapping.data.destroy();
}

static status parse_json_component(modeling&                  mod,
                                   component&                 compo,
                                   cache_rw&                  cache,
                                   const rapidjson::Document& doc) noexcept
{
    reader r{ cache, mod };
    r.dependencies.emplace_back(mod.components.get_id(compo));

    while (!r.dependencies.empty()) {
        r.clear();

        const auto id = r.dependencies.back();
        auto*      c  = mod.components.try_to_get(id);
        r.dependencies.pop_back();

        debug::ensure(c);
        if (c->state == component_status::unmodified)
            continue;

        const auto old_size = r.dependencies.size();
        if (r.read_component(doc.GetObject(), *c)) {
            c->state = component_status::unmodified;
        } else {
            c->state = component_status::unread;
            mod.clear(*c);

            if (old_size != r.dependencies.size()) {
                const auto new_id = r.dependencies.back();
                r.dependencies.pop_back();
                r.dependencies.emplace_back(id);
                r.dependencies.emplace_back(new_id);
            }

#ifdef IRRITATOR_ENABLE_DEBUG
            r.show_error();
#endif

            if (!r.dependencies.empty()) {
                compo.state = component_status::unreadable;
                return new_error(project::file_error);
            }
        }
    }

    return success();
}

static status parse_component(modeling&  mod,
                              component& compo,
                              cache_rw&  cache,
                              file&      f) noexcept
{
    rapidjson::Document doc;

    irt_check(read_file_to_buffer(cache, f));

    irt_check(parse_json_data(
      std::span(cache.buffer.data(), cache.buffer.size()), doc));
    irt_check(parse_json_component(mod, compo, cache, doc));

    return success();
}

static status parse_component(modeling&       mod,
                              component&      compo,
                              cache_rw&       cache,
                              std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    irt_check(parse_json_data(buffer, doc));
    irt_check(parse_json_component(mod, compo, cache, doc));

    return success();
}

status json_archiver::component_load(modeling&  mod,
                                     component& compo,
                                     cache_rw&  cache,
                                     file&      f) noexcept
{
    return parse_component(mod, compo, cache, f);
}

status json_archiver::component_load(modeling&       mod,
                                     component&      compo,
                                     cache_rw&       cache,
                                     std::span<char> buffer) noexcept
{
    return parse_component(mod, compo, cache, buffer);
}

template<typename Writer>
static void write_constant_sources(cache_rw& /*cache*/,
                                   const external_source& srcs,
                                   Writer&                w) noexcept
{
    w.Key("constant-sources");
    w.StartArray();

    const constant_source* src = nullptr;
    while (srcs.constant_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.constant_sources.get_id(*src)));
        w.Key("parameters");

        w.StartArray();
        for (const auto elem : src->buffer)
            w.Double(elem);
        w.EndArray();

        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_binary_file_sources(cache_rw& /*cache*/,
                                      const external_source& srcs,
                                      Writer&                w) noexcept
{
    w.Key("binary-file-sources");
    w.StartArray();

    const binary_file_source* src = nullptr;
    std::string               filepath;

    while (srcs.binary_file_sources.next(src)) {
        filepath = src->file_path.string();

        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
        w.Key("max-clients");
        w.Uint(src->max_clients);
        w.Key("path");
        w.String(filepath.data(),
                 static_cast<rapidjson::SizeType>(filepath.size()));
        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_text_file_sources(cache_rw& /*cache*/,
                                    const external_source& srcs,
                                    Writer&                w) noexcept
{
    w.Key("text-file-sources");
    w.StartArray();

    const text_file_source* src = nullptr;
    std::string             filepath;

    while (srcs.text_file_sources.next(src)) {
        filepath = src->file_path.string();

        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
        w.Key("path");
        w.String(filepath.data(),
                 static_cast<rapidjson::SizeType>(filepath.size()));
        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_random_sources(cache_rw& /*cache*/,
                                 const external_source& srcs,
                                 Writer&                w) noexcept
{
    w.Key("random-sources");
    w.StartArray();

    const random_source* src = nullptr;
    while (srcs.random_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.random_sources.get_id(*src)));
        w.Key("type");
        w.String(distribution_str(src->distribution));

        switch (src->distribution) {
        case distribution_type::uniform_int:
            w.Key("a");
            w.Int(src->a32);
            w.Key("b");
            w.Int(src->b32);
            break;

        case distribution_type::uniform_real:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::bernouilli:
            w.Key("p");
            w.Double(src->p);
            ;
            break;

        case distribution_type::binomial:
            w.Key("t");
            w.Int(src->t32);
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::negative_binomial:
            w.Key("t");
            w.Int(src->t32);
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::geometric:
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::poisson:
            w.Key("mean");
            w.Double(src->mean);
            break;

        case distribution_type::exponential:
            w.Key("lambda");
            w.Double(src->lambda);
            break;

        case distribution_type::gamma:
            w.Key("alpha");
            w.Double(src->alpha);
            w.Key("beta");
            w.Double(src->beta);
            break;

        case distribution_type::weibull:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::exterme_value:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::normal:
            w.Key("mean");
            w.Double(src->mean);
            w.Key("stddev");
            w.Double(src->stddev);
            break;

        case distribution_type::lognormal:
            w.Key("m");
            w.Double(src->m);
            w.Key("s");
            w.Double(src->s);
            break;

        case distribution_type::chi_squared:
            w.Key("n");
            w.Double(src->n);
            break;

        case distribution_type::cauchy:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::fisher_f:
            w.Key("m");
            w.Double(src->m);
            w.Key("n");
            w.Double(src->n);
            break;

        case distribution_type::student_t:
            w.Key("n");
            w.Double(src->n);
            break;
        }

        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_child_component_path(Writer&               w,
                                       const registred_path& reg,
                                       const dir_path&       dir,
                                       const file_path&      file) noexcept
{
    w.Key("name");
    w.String(reg.name.begin(), reg.name.size());

    w.Key("directory");
    w.String(dir.path.begin(), dir.path.size());

    w.Key("file");
    w.String(file.path.begin(), file.path.size());
}

template<typename Writer>
static void write_child_component_path(const modeling&  mod,
                                       const component& compo,
                                       Writer&          w) noexcept
{
    auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
    debug::ensure(reg);
    debug::ensure(not reg->path.empty());
    debug::ensure(not reg->name.empty());

    auto* dir = mod.dir_paths.try_to_get(compo.dir);
    debug::ensure(dir);
    debug::ensure(not dir->path.empty());

    auto* file = mod.file_paths.try_to_get(compo.file);
    debug::ensure(file);
    debug::ensure(not file->path.empty());

    if (reg and dir and file)
        write_child_component_path(w, *reg, *dir, *file);
}

template<typename Writer>
static void write_child_component(const modeling&    mod,
                                  const component_id compo_id,
                                  Writer&            w) noexcept
{
    if (auto* compo = mod.components.try_to_get(compo_id); compo) {
        w.Key("component-type");
        w.String(component_type_names[ordinal(compo->type)]);

        switch (compo->type) {
        case component_type::none:
            w.Key("component-type");
            w.String(component_type_names[ordinal(component_type::none)]);
            break;
        case component_type::internal:
            w.Key("parameter");
            w.String(internal_component_names[ordinal(compo->id.internal_id)]);
            break;
        case component_type::grid:
            write_child_component_path(mod, *compo, w);
            break;
        case component_type::graph:
            write_child_component_path(mod, *compo, w);
            break;
        case component_type::simple:
            write_child_component_path(mod, *compo, w);
            break;
        case component_type::hsm:
            write_child_component_path(mod, *compo, w);
            break;
        }
    } else {
        w.Key("component-type");
        w.String(component_type_names[ordinal(component_type::none)]);
    }
}

template<typename Writer>
static void write_child_model(model& mdl, Writer& w) noexcept
{
    w.Key("dynamics");

    dispatch(mdl,
             [&]<typename Dynamics>(Dynamics& dyn) noexcept { write(w, dyn); });
}

template<typename Writer>
static void write_child(const modeling&          mod,
                        const generic_component& gen,
                        const child&             ch,
                        const u64                unique_id,
                        Writer&                  w) noexcept
{
    const auto child_id  = gen.children.get_id(ch);
    const auto child_idx = get_index(child_id);

    w.StartObject();
    w.Key("id");
    w.Uint64(get_index(child_id));

    if (unique_id != 0) {
        w.Key("unique-id");
        w.Uint64(unique_id);
    }

    w.Key("x");
    w.Double(gen.children_positions[child_idx].x);
    w.Key("y");
    w.Double(gen.children_positions[child_idx].y);
    w.Key("name");
    w.String(gen.children_names[child_idx].c_str());

    w.Key("configurable");
    w.Bool(ch.flags[child_flags::configurable]);

    w.Key("observable");
    w.Bool(ch.flags[child_flags::observable]);

    if (ch.type == child_type::component) {
        const auto compo_id = ch.id.compo_id;
        if (auto* compo = mod.components.try_to_get(compo_id); compo) {
            w.Key("type");
            w.String("component");

            write_child_component(mod, compo_id, w);
        }
    } else {
        model mdl;
        mdl.type = ch.id.mdl_type;

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
            std::construct_at<Dynamics>(&dyn);
        });

        gen.children_parameters[child_idx].copy_to(mdl);

        w.Key("type");
        w.String(dynamics_type_names[ordinal(ch.id.mdl_type)]);

        write_child_model(mdl, w);
    }

    w.EndObject();
}

template<typename Writer>
static void write_generic_component_children(
  cache_rw& /*cache*/,
  const modeling&          mod,
  const generic_component& simple_compo,
  Writer&                  w) noexcept
{
    w.Key("children");
    w.StartArray();

    for_each_data(simple_compo.children, [&](auto& c) noexcept {
        write_child(mod,
                    simple_compo,
                    c,
                    c.unique_id == 0 ? simple_compo.make_next_unique_id()
                                     : c.unique_id,
                    w);
    });

    w.EndArray();
}

template<typename Writer>
static void write_component_ports(cache_rw& /*cache*/,
                                  const component& compo,
                                  Writer&          w) noexcept
{
    if (!compo.x_names.empty()) {
        w.Key("x");
        w.StartArray();

        for (const auto id : compo.x)
            w.String(compo.x_names[get_index(id)].c_str());

        w.EndArray();
    }

    if (!compo.y_names.empty()) {
        w.Key("y");
        w.StartArray();

        for (const auto id : compo.y)
            w.String(compo.y_names[get_index(id)].c_str());

        w.EndArray();
    }
}

template<typename Writer>
static void write_input_connection(const modeling&          mod,
                                   const component&         parent,
                                   const generic_component& gen,
                                   const port_id            x,
                                   const child&             dst,
                                   const connection::port   dst_x,
                                   Writer&                  w) noexcept
{
    w.StartObject();
    w.Key("type");
    w.String("input");
    w.Key("port");
    w.String(parent.x_names[get_index(x)].c_str());
    w.Key("destination");
    w.Uint64(get_index(gen.children.get_id(dst)));
    w.Key("port-destination");

    if (dst.type == child_type::model) {
        w.Int(dst_x.model);
    } else {
        const auto* compo_child = mod.components.try_to_get(dst.id.compo_id);
        if (compo_child and compo_child->x.exists(dst_x.compo))
            w.String(compo_child->x_names[get_index(dst_x.compo)].c_str());
    }

    w.EndObject();
}

template<typename Writer>
static void write_output_connection(const modeling&          mod,
                                    const component&         parent,
                                    const generic_component& gen,
                                    const port_id            y,
                                    const child&             src,
                                    const connection::port   src_y,
                                    Writer&                  w) noexcept
{
    w.StartObject();
    w.Key("type");
    w.String("output");
    w.Key("port");
    w.String(parent.y_names[get_index(y)].c_str());
    w.Key("source");
    w.Uint64(get_index(gen.children.get_id(src)));
    w.Key("port-source");

    if (src.type == child_type::model) {
        w.Int(src_y.model);
    } else {
        const auto* compo_child = mod.components.try_to_get(src.id.compo_id);
        if (compo_child and compo_child->y.exists(src_y.compo)) {
            w.String(compo_child->y_names[get_index(src_y.compo)].c_str());
        }
    }
    w.EndObject();
}

template<typename Writer>
static void write_internal_connection(const modeling&          mod,
                                      const generic_component& gen,
                                      const child&             src,
                                      const connection::port   src_y,
                                      const child&             dst,
                                      const connection::port   dst_x,
                                      Writer&                  w) noexcept
{
    const char* src_str = nullptr;
    const char* dst_str = nullptr;
    int         src_int = -1;
    int         dst_int = -1;

    if (src.type == child_type::component) {
        auto* compo = mod.components.try_to_get(src.id.compo_id);
        if (compo and compo->y.exists(src_y.compo))
            src_str = compo->y_names[get_index(src_y.compo)].c_str();
    } else
        src_int = src_y.model;

    if (dst.type == child_type::component) {
        auto* compo = mod.components.try_to_get(dst.id.compo_id);
        if (compo and compo->x.exists(dst_x.compo))
            dst_str = compo->x_names[get_index(dst_x.compo)].c_str();
    } else
        dst_int = dst_x.model;

    if ((src_str or src_int >= 0) and (dst_str or dst_int >= 0)) {
        w.StartObject();
        w.Key("type");
        w.String("internal");
        w.Key("source");
        w.Uint64(get_index(gen.children.get_id(src)));
        w.Key("port-source");
        if (src_str)
            w.String(src_str);
        else
            w.Int(src_int);

        w.Key("destination");
        w.Uint64(get_index(gen.children.get_id(dst)));
        w.Key("port-destination");
        if (dst_str)
            w.String(dst_str);
        else
            w.Int(dst_int);
        w.EndObject();
    }
}

template<typename Writer>
static void write_generic_component_connections(cache_rw& /*cache*/,
                                                const modeling&          mod,
                                                const component&         compo,
                                                const generic_component& gen,
                                                Writer& w) noexcept
{
    w.Key("connections");
    w.StartArray();

    for (const auto& con : gen.connections)
        if (auto* c_src = gen.children.try_to_get(con.src); c_src)
            if (auto* c_dst = gen.children.try_to_get(con.dst); c_dst)
                write_internal_connection(
                  mod, gen, *c_src, con.index_src, *c_dst, con.index_dst, w);

    for (const auto& con : gen.input_connections)
        if (auto* c = gen.children.try_to_get(con.dst); c)
            if (compo.x.exists(con.x))
                write_input_connection(mod, compo, gen, con.x, *c, con.port, w);

    for (const auto& con : gen.output_connections)
        if (auto* c = gen.children.try_to_get(con.src); c)
            if (compo.y.exists(con.y))
                write_output_connection(
                  mod, compo, gen, con.y, *c, con.port, w);

    w.EndArray();
}

template<typename Writer>
static void write_generic_component(cache_rw&                cache,
                                    const modeling&          mod,
                                    const component&         compo,
                                    const generic_component& s_compo,
                                    Writer&                  w) noexcept
{
    w.String("next-unique-id");
    w.Uint64(s_compo.next_unique_id);

    write_generic_component_children(cache, mod, s_compo, w);
    write_generic_component_connections(cache, mod, compo, s_compo, w);
}

template<typename Writer>
static void write_grid_component(cache_rw& /*cache*/,
                                 const modeling&       mod,
                                 const grid_component& grid,
                                 Writer&               w) noexcept
{
    w.Key("rows");
    w.Int(grid.row);
    w.Key("columns");
    w.Int(grid.column);
    w.Key("in-connection-type");
    w.Int(ordinal(grid.in_connection_type));
    w.Key("out-connection-type");
    w.Int(ordinal(grid.out_connection_type));

    w.Key("children");
    w.StartArray();
    for (auto& elem : grid.children) {
        w.StartObject();
        write_child_component(mod, elem, w);
        w.EndObject();
    }
    w.EndArray();
}

template<typename Writer>
static void write_graph_component_param(const modeling&        mod,
                                        const graph_component& g,
                                        Writer&                w) noexcept
{
    switch (g.g_type) {
    case graph_component::graph_type::dot_file: {
        w.String("dot-file");
        auto& p = g.param.dot;

        if (auto* dir = mod.dir_paths.try_to_get(p.dir); dir) {
            w.Key("dir");
            w.String(dir->path.begin(), dir->path.size());
        }

        if (auto* file = mod.file_paths.try_to_get(p.file); file) {
            w.Key("file");
            w.String(file->path.begin(), file->path.size());
        }
        break;
    }

    case graph_component::graph_type::scale_free: {
        w.String("scale-free");
        w.Key("alpha");
        w.Double(g.param.scale.alpha);
        w.Key("beta");
        w.Double(g.param.scale.beta);
        break;
    }

    case graph_component::graph_type::small_world: {
        w.String("small-world");
        w.Key("probability");
        w.Double(g.param.small.probability);
        w.Key("k");
        w.Int(g.param.small.k);
        break;
    }
    }
}

template<typename Writer>
static void write_graph_component(cache_rw& /*cache*/,
                                  const modeling&        mod,
                                  const graph_component& graph,
                                  Writer&                w) noexcept
{
    w.Key("graph-type");
    write_graph_component_param(mod, graph, w);

    w.Key("children");
    w.StartArray();
    for (auto& elem : graph.children) {
        w.StartObject();
        write_child_component(mod, elem.id, w);
        w.EndObject();
    }
    w.EndArray();
}

template<typename Writer>
static void write_hsm_component(const hierarchical_state_machine& hsm,
                                Writer&                           w) noexcept
{
    w.Key("states");
    w.StartArray();

    constexpr auto length  = hierarchical_state_machine::max_number_of_state;
    constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

    std::array<bool, length> states_to_write;
    states_to_write.fill(false);

    if (hsm.top_state != invalid)
        states_to_write[hsm.top_state] = true;

    for (auto i = 0; i != length; ++i) {
        if (hsm.states[i].if_transition != invalid)
            states_to_write[hsm.states[i].if_transition] = true;
        if (hsm.states[i].else_transition != invalid)
            states_to_write[hsm.states[i].else_transition] = true;
        if (hsm.states[i].super_id != invalid)
            states_to_write[hsm.states[i].super_id] = true;
        if (hsm.states[i].sub_id != invalid)
            states_to_write[hsm.states[i].sub_id] = true;
    }

    for (auto i = 0; i != length; ++i) {
        if (states_to_write[i]) {
            w.StartObject();
            w.Key("id");
            w.Uint(i);
            write(w, "enter", hsm.states[i].enter_action);
            write(w, "exit", hsm.states[i].exit_action);
            write(w, "if", hsm.states[i].if_action);
            write(w, "else", hsm.states[i].else_action);
            write(w, "condition", hsm.states[i].condition);

            w.Key("if-transition");
            w.Int(hsm.states[i].if_transition);
            w.Key("else-transition");
            w.Int(hsm.states[i].else_transition);
            w.Key("super-id");
            w.Int(hsm.states[i].super_id);
            w.Key("sub-id");
            w.Int(hsm.states[i].sub_id);
            w.EndObject();
        }
    }
    w.EndArray();

    w.Key("top");
    w.Uint(hsm.top_state);
}

template<typename Writer>
static void write_internal_component(cache_rw& /*cache*/,
                                     const modeling& /* mod */,
                                     const internal_component id,
                                     Writer&                  w) noexcept
{
    w.Key("component");
    w.String(internal_component_names[ordinal(id)]);
}

template<typename Writer>
static void do_component_save(Writer&    w,
                              modeling&  mod,
                              component& compo,
                              cache_rw&  cache) noexcept
{
    w.StartObject();

    w.Key("name");
    w.String(compo.name.c_str());

    write_constant_sources(cache, mod.srcs, w);
    write_binary_file_sources(cache, mod.srcs, w);
    write_text_file_sources(cache, mod.srcs, w);
    write_random_sources(cache, mod.srcs, w);

    w.Key("colors");
    w.StartArray();
    auto& color = mod.component_colors[get_index(mod.components.get_id(compo))];
    w.Double(color[0]);
    w.Double(color[1]);
    w.Double(color[2]);
    w.Double(color[3]);
    w.EndArray();

    write_component_ports(cache, compo, w);

    w.Key("type");
    w.String(component_type_names[ordinal(compo.type)]);

    switch (compo.type) {
    case component_type::none:
        break;

    case component_type::internal:
        write_internal_component(cache, mod, compo.id.internal_id, w);
        break;

    case component_type::simple: {
        auto* p = mod.generic_components.try_to_get(compo.id.generic_id);
        if (p)
            write_generic_component(cache, mod, compo, *p, w);
    } break;

    case component_type::grid: {
        auto* p = mod.grid_components.try_to_get(compo.id.grid_id);
        if (p)
            write_grid_component(cache, mod, *p, w);
    } break;

    case component_type::graph: {
        auto* p = mod.graph_components.try_to_get(compo.id.graph_id);
        if (p)
            write_graph_component(cache, mod, *p, w);
    } break;

    case component_type::hsm: {
        auto* p = mod.hsm_components.try_to_get(compo.id.hsm_id);
        if (p)
            write_hsm_component(*p, w);
    } break;
    }

    w.EndObject();
}

status json_archiver::component_save(modeling&                   mod,
                                     component&                  compo,
                                     cache_rw&                   cache,
                                     const char*                 filename,
                                     json_archiver::print_option print) noexcept
{
    auto f = file::open(filename, open_mode::write);
    if (!f)
        return f.error();

    FILE* fp = reinterpret_cast<FILE*>(f->get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    switch (print) {
    case json_archiver::print_option::indent_2:
        w.SetIndent(' ', 2);
        do_component_save(w, mod, compo, cache);
        break;

    case json_archiver::print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        do_component_save(w, mod, compo, cache);
        break;

    default:
        do_component_save(w, mod, compo, cache);
        break;
    }

    return success();
}

status json_archiver::component_save(modeling&                   mod,
                                     component&                  compo,
                                     cache_rw&                   cache,
                                     vector<char>&               out,
                                     json_archiver::print_option print) noexcept
{
    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    switch (print) {
    case json_archiver::print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        do_component_save(w, mod, compo, cache);
    } break;

    case json_archiver::print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        do_component_save(w, mod, compo, cache);
    } break;

    default: {
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        do_component_save(w, mod, compo, cache);
    } break;
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return success();
}

/*****************************************************************************
 *
 * Simulation file read part
 *
 ****************************************************************************/

template<typename Writer>
static void write_simulation_model(const simulation& sim, Writer& w) noexcept
{
    w.Key("hsms");
    w.StartArray();

    for (auto& machine : sim.hsms) {
        w.StartObject();
        w.Key("hsm");
        w.Uint64(ordinal(sim.hsms.get_id(machine)));
        write_hsm_component(machine, w);
        w.EndObject();
    }
    w.EndArray();

    w.Key("models");
    w.StartArray();

    for (auto& mdl : sim.models) {
        const auto mdl_id = sim.models.get_id(mdl);

        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(mdl_id));
        w.Key("type");
        w.String(dynamics_type_names[ordinal(mdl.type)]);
        w.Key("dynamics");

        dispatch(mdl, [&w]<typename Dynamics>(const Dynamics& dyn) noexcept {
            write(w, dyn);
        });

        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_simulation_connections(const simulation& sim,
                                         Writer&           w) noexcept
{
    w.Key("connections");
    w.StartArray();

    const model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        dispatch(*mdl, [&sim, &mdl, &w]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_output_port<Dynamics>) {
                for (auto i = 0, e = length(dyn.y); i != e; ++i) {
                    if (auto* lst = sim.nodes.try_to_get(dyn.y[i]); lst) {
                        for (const auto& cnt : *lst) {
                            auto* dst = sim.models.try_to_get(cnt.model);
                            if (dst) {
                                w.StartObject();
                                w.Key("source");
                                w.Uint64(ordinal(sim.models.get_id(*mdl)));
                                w.Key("port-source");
                                w.Uint64(static_cast<u64>(i));
                                w.Key("destination");
                                w.Uint64(ordinal(cnt.model));
                                w.Key("port-destination");
                                w.Uint64(static_cast<u64>(cnt.port_index));
                                w.EndObject();
                            }
                        }
                    }
                }
            }
        });
    }

    w.EndArray();
}

template<typename Writer>
static void do_simulation_save(Writer&           w,
                               const simulation& sim,
                               cache_rw&         cache) noexcept
{
    w.StartObject();

    write_constant_sources(cache, sim.srcs, w);
    write_binary_file_sources(cache, sim.srcs, w);
    write_text_file_sources(cache, sim.srcs, w);
    write_random_sources(cache, sim.srcs, w);
    write_simulation_model(sim, w);
    write_simulation_connections(sim, w);

    w.EndObject();
}

status json_archiver::simulation_save(const simulation& sim,
                                      cache_rw&         cache,
                                      const char*       filename,
                                      print_option      print_options) noexcept
{
    auto f = file::open(filename, open_mode::write);
    if (!f)
        return f.error();

    FILE* fp = reinterpret_cast<FILE*>(f->get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    switch (print_options) {
    case print_option::indent_2:
        w.SetIndent(' ', 2);
        do_simulation_save(w, sim, cache);
        break;

    case print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        do_simulation_save(w, sim, cache);
        break;

    default:
        do_simulation_save(w, sim, cache);
        break;
    }

    return success();
}

status json_archiver::simulation_save(const simulation& sim,
                                      cache_rw&         cache,
                                      vector<char>&     out,
                                      print_option      print_options) noexcept
{
    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    switch (print_options) {
    case print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        do_simulation_save(w, sim, cache);
        break;
    }

    case print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        do_simulation_save(w, sim, cache);
        break;
    }

    default: {
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        do_simulation_save(w, sim, cache);
        break;
    }
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return success();
}

static status parse_json_simulation(simulation&                sim,
                                    cache_rw&                  cache,
                                    const rapidjson::Document& doc) noexcept
{
    sim.clear();

    reader r{ cache, sim };
    if (r.read_simulation(doc.GetObject()))
        return success();

    debug_logi(
      2, "read simulation fail with {}\n", error_id_names[ordinal(r.error)]);

    for (auto i = 0u; i < r.stack.size(); ++i)
        debug_logi(2,
                   "  {}: {}\n",
                   static_cast<int>(i),
                   stack_id_names[ordinal(r.stack[i])]);

    return new_error(json_archiver::part::simulation_parser);
}

static status parse_simulation(simulation& sim,
                               cache_rw&   cache,
                               file&       f) noexcept
{
    rapidjson::Document doc;

    irt_check(read_file_to_buffer(cache, f));
    irt_check(parse_json_data(
      std::span(cache.buffer.data(), cache.buffer.size()), doc));

    irt_check(parse_json_simulation(sim, cache, doc));

    return success();
}

static status parse_simulation(simulation&     sim,
                               cache_rw&       cache,
                               std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    irt_check(parse_json_data(buffer, doc));
    irt_check(parse_json_simulation(sim, cache, doc));

    return success();
}

status json_archiver::simulation_load(simulation& sim,
                                      cache_rw&   cache,
                                      file&       f) noexcept
{
    return parse_simulation(sim, cache, f);
}

status json_archiver::simulation_load(simulation&     sim,
                                      cache_rw&       cache,
                                      std::span<char> buffer) noexcept
{
    return parse_simulation(sim, cache, buffer);
}

/*****************************************************************************
 *
 * Project file read part
 *
 ****************************************************************************/

static status parse_json_project(project&                   pj,
                                 modeling&                  mod,
                                 simulation&                sim,
                                 cache_rw&                  cache,
                                 const rapidjson::Document& doc) noexcept
{
    pj.clear();
    sim.clear();

    reader r{ cache, mod, sim, pj };
    if (r.read_project(doc.GetObject()))
        return success();

    debug_log("read project fail with {}\n", error_id_names[ordinal(r.error)]);

    for (auto i = 0u; i < r.stack.size(); ++i)
        debug_logi(2,
                   "  {}: {}\n",
                   static_cast<int>(i),
                   stack_id_names[ordinal(r.stack[i])]);

    return new_error(json_archiver::part::project_parser);
}

static status parse_project(project&    pj,
                            modeling&   mod,
                            simulation& sim,
                            cache_rw&   cache,
                            file&       f) noexcept
{
    rapidjson::Document doc;

    irt_check(read_file_to_buffer(cache, f));
    irt_check(parse_json_data(
      std::span(cache.buffer.data(), cache.buffer.size()), doc));
    irt_check(parse_json_project(pj, mod, sim, cache, doc));

    return success();
}

static status parse_project(project&        pj,
                            modeling&       mod,
                            simulation&     sim,
                            cache_rw&       cache,
                            std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    irt_check(parse_json_data(buffer, doc));
    irt_check(parse_json_project(pj, mod, sim, cache, doc));

    return success();
}

status json_archiver::project_load(project&    pj,
                                   modeling&   mod,
                                   simulation& sim,
                                   cache_rw&   cache,
                                   file&       f) noexcept
{
    return parse_project(pj, mod, sim, cache, f);
}

status json_archiver::project_load(project&        pj,
                                   modeling&       mod,
                                   simulation&     sim,
                                   cache_rw&       cache,
                                   std::span<char> buffer) noexcept
{
    return parse_project(pj, mod, sim, cache, buffer);
}

/*****************************************************************************
 *
 * project file write part
 *
 ****************************************************************************/

template<typename Writer>
static void write_color(Writer& w, std::array<u8, 4> color) noexcept
{
    w.StartArray();
    w.Uint(color[0]);
    w.Uint(color[1]);
    w.Uint(color[2]);
    w.Uint(color[3]);
    w.EndArray();
}

template<typename Writer>
static void write_project_unique_id_path(Writer&               w,
                                         const unique_id_path& path) noexcept
{
    w.StartArray();
    for (auto elem : path)
        w.Uint64(elem);
    w.EndArray();
}

constexpr static std::array<u8, 4> color_white{ 255, 255, 255, 0 };

template<typename Writer>
static status do_project_save_parameters(Writer& w, project& pj) noexcept
{
    w.Key("parameters");

    w.StartObject();
    irt_check(do_project_save_global_parameters(w, pj));
    w.EndObject();

    return success();
}

template<typename Writer>
static status do_project_save_plot_observations(Writer& w, project& pj) noexcept
{
    w.Key("global");
    w.StartArray();

    for_each_data(pj.variable_observers, [&](auto& plot) noexcept {
        unique_id_path path;

        w.StartObject();
        w.Key("name");
        w.String(plot.name.begin(), plot.name.size());

        plot.for_each_tn_mdl([&](const auto tn_id, const auto mdl_id) noexcept {
            w.Key("access");
            pj.build_unique_id_path(tn_id, mdl_id, path);
            write_project_unique_id_path(w, path);
        });

        w.Key("color");
        write_color(w, color_white);

        w.Key("type");
        w.String("line");
        w.EndObject();
    });

    w.EndArray();

    return success();
}

template<typename Writer>
static status do_project_save_grid_observations(Writer& w, project& pj) noexcept
{
    w.Key("grid");
    w.StartArray();

    for_each_data(pj.grid_observers, [&](auto& grid) noexcept {
        w.StartObject();
        w.Key("name");
        w.String(grid.name.begin(), grid.name.size());

        unique_id_path path;
        w.Key("grid");
        write_project_unique_id_path(w, path);
        pj.build_unique_id_path(grid.parent_id, path);

        w.Key("access");
        pj.build_unique_id_path(grid.tn_id, grid.mdl_id, path);

        w.EndObject();
    });

    w.EndArray();

    return success();
}

template<typename Writer>
static status do_project_save_observations(Writer& w, project& pj) noexcept
{
    w.Key("observations");

    w.StartObject();
    irt_check(do_project_save_plot_observations(w, pj));
    irt_check(do_project_save_grid_observations(w, pj));
    w.EndObject();

    return success();
}

template<typename Writer>
static void write_parameter(Writer& w, const parameter& param) noexcept
{
    w.Key("parameter");
    w.StartObject();
    w.Key("real");
    w.StartArray();
    for (auto elem : param.reals)
        w.Double(elem);
    w.EndArray();
    w.Key("integer");
    w.StartArray();
    for (auto elem : param.integers)
        w.Int64(elem);
    w.EndArray();
}

template<typename Writer>
static status do_project_save_global_parameters(Writer& w, project& pj) noexcept
{
    w.Key("global");
    w.StartArray();

    pj.parameters.for_each([&](const auto /*id*/,
                               const auto& name,
                               const auto  tn_id,
                               const auto  mdl_id,
                               const auto& p) noexcept {
        unique_id_path path;
        w.Key("access");
        pj.build_unique_id_path(tn_id, mdl_id, path);

        w.StartObject();
        w.Key("name");
        w.String(name.begin(), name.size());

        w.Key("access");
        write_project_unique_id_path(w, path);
        write_parameter(w, p);

        w.EndObject();
    });

    w.EndArray();

    return success();
}

template<typename Writer>
static status do_project_save_component(Writer&               w,
                                        component&            compo,
                                        const registred_path& reg,
                                        const dir_path&       dir,
                                        const file_path&      file) noexcept
{
    w.Key("component-type");
    w.String(component_type_names[ordinal(compo.type)]);

    switch (compo.type) {
    case component_type::internal:
        break;

    case component_type::simple:
    case component_type::grid:
    case component_type::hsm:
        w.Key("component-path");
        w.String(reg.name.c_str(), reg.name.size(), false);

        w.Key("component-directory");
        w.String(dir.path.c_str(), dir.path.size(), false);

        w.Key("component-file");
        w.String(file.path.c_str(), file.path.size(), false);
        break;

    default:
        break;
    };

    return success();
}

template<typename Writer>
static status do_project_save(Writer&    w,
                              project&   pj,
                              modeling&  mod,
                              component& compo,
                              cache_rw& /* cache */) noexcept
{
    auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
    if (!reg)
        return new_error(project::error::registred_path_access_error);
    if (reg->path.empty())
        return new_error(project::error::registred_path_access_error);
    if (reg->name.empty())
        return new_error(project::error::registred_path_access_error);

    auto* dir = mod.dir_paths.try_to_get(compo.dir);
    if (!dir)
        return new_error(project::error::directory_access_error);
    if (dir->path.empty())
        return new_error(project::error::directory_access_error);

    auto* file = mod.file_paths.try_to_get(compo.file);
    if (!file)
        return new_error(project::error::file_access_error);
    if (file->path.empty())
        return new_error(project::error::file_access_error);

    w.StartObject();
    irt_check(do_project_save_component(w, compo, *reg, *dir, *file));
    irt_check(do_project_save_parameters(w, pj));
    irt_check(do_project_save_observations(w, pj));
    w.EndObject();

    return success();
}

status json_archiver::project_save(project&  pj,
                                   modeling& mod,
                                   simulation& /* sim */,
                                   cache_rw&    cache,
                                   file&        io,
                                   print_option print_options) noexcept
{
    debug::ensure(io.is_open());

    auto* compo  = mod.components.try_to_get(pj.head());
    auto* parent = pj.tn_head();

    if (!(compo && parent))
        return new_error(project::error::empty_project);

    debug::ensure(mod.components.get_id(compo) == parent->id);

    auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
    if (!reg)
        return new_error(project::error::registred_path_access_error);

    auto* dir = mod.dir_paths.try_to_get(compo->dir);
    if (!dir)
        return new_error(project::error::directory_access_error);

    auto* file = mod.file_paths.try_to_get(compo->file);
    if (!file)
        return new_error(project::error::file_access_error);

    auto* fp = reinterpret_cast<FILE*>(io.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    switch (print_options) {
    case print_option::indent_2:
        w.SetIndent(' ', 2);
        irt_check(do_project_save(w, pj, mod, *compo, cache));
        break;

    case print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_check(do_project_save(w, pj, mod, *compo, cache));
        break;

    default:
        irt_check(do_project_save(w, pj, mod, *compo, cache));
        break;
    }

    return success();
}

status json_archiver::project_save(project&  pj,
                                   modeling& mod,
                                   simulation& /* sim */,
                                   cache_rw&     cache,
                                   vector<char>& out,
                                   print_option  print_options) noexcept
{
    auto* compo  = mod.components.try_to_get(pj.head());
    auto* parent = pj.tn_head();

    if (!(compo && parent))
        return new_error(project::error::empty_project);

    debug::ensure(mod.components.get_id(compo) == parent->id);

    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    switch (print_options) {
    case print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        irt_check(do_project_save(w, pj, mod, *compo, cache));
    } break;

    case print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_check(do_project_save(w, pj, mod, *compo, cache));
    } break;

    default: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        irt_check(do_project_save(w, pj, mod, *compo, cache));
    } break;
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return success();
}

} //  irt
