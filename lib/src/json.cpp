// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/file.hpp>
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
#include <variant>

#ifdef IRRITATOR_ENABLE_DEBUG
#include <fmt/format.h>
#endif

#include <climits>
#include <cstdint>

using namespace std::literals;

namespace irt {

enum class stack_id
{
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
    component_children,
    component_generic,
    component_generic_connections,
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
    dynamics_integrator,
    dynamics_quantifier,
    dynamics_adder_2,
    dynamics_adder_3,
    dynamics_adder_4,
    dynamics_mult_2,
    dynamics_mult_3,
    dynamics_mult_4,
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
    dynamics_cross,
    dynamics_accumulator_2,
    dynamics_time_func,
    dynamics_filter,
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
    "component_children",
    "component_generic",
    "component_generic_connections",
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
    "dynamics_integrator",
    "dynamics_quantifier",
    "dynamics_adder_2",
    "dynamics_adder_3",
    "dynamics_adder_4",
    "dynamics_mult_2",
    "dynamics_mult_3",
    "dynamics_mult_4",
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
    "dynamics_cross",
    "dynamics_accumulator_2",
    "dynamics_time_func",
    "dynamics_filter",
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

enum class error_id
{
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
    project_plot_observers_not_enough,
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
    "project_plot_observers_not_enough",
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
        return false;                                                          \
    } while (0)

struct reader
{
    template<typename Function, typename... Args>
    bool for_each_array(const rapidjson::Value& array,
                        Function&&              f,
                        Args... args) noexcept
    {
        if (!array.IsArray())
            report_json_error(error_id::value_not_array);

        rapidjson::SizeType i = 0, e = array.GetArray().Size();
        for (; i != e; ++i) {
            // fmt::print("for-array: {}/{}\n", i, e);
            if (!f(i, array.GetArray()[i], args...))
                return false;
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
            // fmt::print("for-member: {}\n", it->name.GetString());
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

    bool copy_to(std::filesystem::path& path) noexcept
    {
        try {
            path = temp_string;
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

    bool copy_to(connection::connection_type& type) noexcept
    {
        if (temp_string == "internal"sv)
            type = connection::connection_type::internal;
        else if (temp_string == "output"sv)
            type = connection::connection_type::output;
        else if (temp_string == "input"sv)
            type = connection::connection_type::input;
        else
            report_json_error(error_id::missing_connection_type);

        return true;
    }

    bool copy_to(quantifier::adapt_state& dst) noexcept
    {
        if (temp_string == "possible"sv)
            dst = quantifier::adapt_state::possible;
        else if (temp_string == "impossible"sv)
            dst = quantifier::adapt_state::impossible;
        else if (temp_string == "done"sv)
            dst = quantifier::adapt_state::done;
        else
            report_json_error(error_id::missing_quantifier_adapt_state);

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
        if (!(INT32_MIN <= temp_integer && temp_integer < INT32_MAX))
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
        if (!(0 <= temp_integer && temp_integer < INT8_MAX))
            report_json_error(error_id::integer_to_i8_error);

        dst = static_cast<i8>(temp_integer);

        return true;
    }

    bool copy_to(u8& dst) noexcept
    {
        if (!(0 <= temp_integer && temp_integer < UINT8_MAX))
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
        if (!(0 <= temp_integer && temp_integer < INT8_MAX))
            report_json_error(error_id::integer_to_i8_error);

        dst = static_cast<i8>(temp_integer);

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
        if (!pj().global_parameters.can_alloc(i))
            report_json_error(error_id::project_global_parameters_not_enough);

        return true;
    }

    bool project_grid_parameters_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().grid_parameters.can_alloc(i))
            report_json_error(error_id::project_grid_parameters_not_enough);

        return true;
    }

    bool project_plot_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().plot_observers.can_alloc(i))
            report_json_error(error_id::project_plot_observers_not_enough);

        return true;
    }

    bool project_grid_observers_can_alloc(std::integral auto i) noexcept
    {
        if (!pj().grid_observers.can_alloc(i))
            report_json_error(error_id::project_grid_observers_not_enough);

        return true;
    }

    bool modeling_can_alloc(std::integral auto i) noexcept
    {
        if (!mod().children.can_alloc(i))
            report_json_error(error_id::modeling_not_enough_children);

        return true;
    }

    bool modeling_can_alloc_model(std::integral auto i) noexcept
    {
        if (!mod().models.can_alloc(i))
            report_json_error(error_id::modeling_not_enough_model);

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
        irt_assert(val.IsArray());

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
        irt_assert(val.IsArray());

        dst = static_cast<i64>(val.GetArray().Size());

        return true;
    }

    bool is_value_array_size_less(const rapidjson::Value& val,
                                  std::integral auto      i) noexcept
    {
        irt_assert(val.IsArray());

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

    bool affect_configurable_to(child_flags& flag) noexcept
    {
        flag |= child_flags_configurable;

        return true;
    }

    bool affect_observable_to(child_flags& flag) noexcept
    {
        flag |= child_flags_observable;

        return true;
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&        val,
                       abstract_integrator<QssLevel>& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_integrator);

        return for_each_member(
          val, [&](const auto name, const auto& val) noexcept -> bool {
              if ("X"sv == name)
                  return read_temp_real(val) and copy_to(dyn.default_X);
              if ("dQ"sv == name)
                  return read_temp_real(val) and copy_to(dyn.default_dQ);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value& /*val*/,
                       abstract_multiplier<QssLevel>& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_multiplier);

        return true;
    }

    template<int QssLevel, int PortNumber>
    bool read_dynamics(
      const rapidjson::Value& /*val*/,
      const abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_sum);

        return true;
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_wsum<QssLevel, 2> dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_2);

        return for_each_member(
          val, [&](const auto name, const auto& val) noexcept -> bool {
              if ("coeff-0"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[1]);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_wsum<QssLevel, 3> dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_3);

        return for_each_member(
          val, [&](const auto name, const auto& val) noexcept -> bool {
              if ("coeff-0"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[2]);

              report_json_error(error_id::unknown_element);
          });
    }

    template<int QssLevel>
    bool read_dynamics(const rapidjson::Value&    val,
                       abstract_wsum<QssLevel, 4> dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_qss_wsum_4);

        return for_each_member(
          val, [&](const auto name, const auto& val) noexcept -> bool {
              if ("coeff-0"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[2]);
              if ("coeff-3"sv == name)
                  return read_temp_real(val) &&
                         copy_to(dyn.default_input_coeffs[3]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, integrator& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_integrator);

        return for_each_member(
          val, [&](const auto name, const auto& val) noexcept -> bool {
              if ("value"sv == name)
                  return read_temp_real(val) and
                         copy_to(dyn.default_current_value);
              if ("reset"sv == name)
                  return read_temp_real(val) and
                         copy_to(dyn.default_reset_value);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, quantifier& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_quantifier);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("adapt-state"sv == name)
                  return read_temp_string(value) &&
                         copy_to(dyn.default_adapt_state);
              if ("step-size"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_step_size);
              if ("past-length"sv == name)
                  return read_temp_integer(value) &&
                         copy_to(dyn.default_past_length);
              if ("zero-init-offset"sv == name)
                  return read_temp_bool(value) &&
                         copy_to(dyn.default_zero_init_offset);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, adder_2& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_adder_2);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, adder_3& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_adder_3);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[2]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[2]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, adder_4& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_adder_4);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[2]);
              if ("value-3"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[3]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[2]);
              if ("coeff-3"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[3]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, mult_2& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_mult_2);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, mult_3& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_mult_3);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[2]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[2]);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_dynamics(const rapidjson::Value& val, mult_4& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_mult_4);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("value-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[0]);
              if ("value-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[1]);
              if ("value-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[2]);
              if ("value-3"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_values[3]);
              if ("coeff-0"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[0]);
              if ("coeff-1"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[1]);
              if ("coeff-2"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[2]);
              if ("coeff-3"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_input_coeffs[3]);

              report_json_error(error_id::unknown_element);
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

    bool read_dynamics(const rapidjson::Value& val, cross& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_cross);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("threshold"sv == name)
                  return read_temp_real(value) &&
                         copy_to(dyn.default_threshold);

              report_json_error(error_id::unknown_element);
          });
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

    bool read_dynamics(const rapidjson::Value& val, filter& dyn) noexcept
    {
        auto_stack a(this, stack_id::dynamics_filter);

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

    bool read_hsm_state(const rapidjson::Value&            val,
                        hierarchical_state_machine::state& s) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_state);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
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
                  return read_temp_integer(value) && copy_to(s.if_transition);

              if ("else-transition"sv == name)
                  return read_temp_integer(value) && copy_to(s.else_transition);

              if ("super-id"sv == name)
                  return read_temp_integer(value) && copy_to(s.super_id);

              if ("sub-id"sv == name)
                  return read_temp_integer(value) && copy_to(s.sub_id);

              report_json_error(error_id::unknown_element);
          });
    }

    bool read_hsm_states(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_states);

        return for_each_array(
          val, [&](const auto i, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states[i]);
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

    bool read_dynamics(const rapidjson::Value&     val,
                       hierarchical_state_machine& hsm) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("states"sv == name)
                  return read_hsm_states(value, hsm.states);
              if ("outputs"sv == name)
                  return read_hsm_outputs(value, hsm.outputs);

              report_json_error(error_id::unknown_element);
          });
    }

    ////

    bool read_child(const rapidjson::Value& val,
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
                                copy_to(unique_id);
                     if ("x"sv == name)
                         return read_temp_real(value) and
                                copy_to(
                                  mod().children_positions[get_index(c_id)].x);
                     if ("y"sv == name)
                         return read_temp_real(value) and
                                copy_to(
                                  mod().children_positions[get_index(c_id)].y);
                     if ("name"sv == name)
                         return read_temp_string(value) and
                                copy_to(mod().children_names[get_index(c_id)]);
                     if ("configurable"sv == name)
                         return read_temp_bool(value) and
                                affect_configurable_to(c.flags);
                     if ("observable"sv == name)
                         return read_temp_bool(value) and
                                affect_observable_to(c.flags);

                     return true;
                 }) &&
               optional_has_value(id) &&
               cache_model_mapping_add(*id, ordinal(c_id)) &&
               optional_has_value(unique_id);
    }

    bool read_child_model_dynamics(const rapidjson::Value& val,
                                   model&                  mdl) noexcept
    {
        auto_stack a(this, stack_id::child_model_dynamics);

        return for_first_member(
          val, "dynamics"sv, [&](const auto& value) noexcept -> bool {
              return dispatch(
                mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept -> bool {
                    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                        if (auto* hsm = mod().hsms.try_to_get(dyn.id); hsm)
                            return read_dynamics(value, *hsm);

                        report_json_error(error_id::modeling_hsm_id_error);
                    } else {
                        return read_dynamics(value, dyn);
                    }
                });
          });
    }

    bool read_child_model(const rapidjson::Value& val,
                          child&                  c,
                          dynamics_type           type) noexcept
    {
        auto_stack a(this, stack_id::child_model);

        auto& mdl    = mod().models.alloc();
        auto  mdl_id = mod().models.get_id(mdl);
        mdl.type     = type;
        mdl.handle   = nullptr;
        c.type       = child_type::model;
        c.id.mdl_id  = mdl_id;

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
            new (&dyn) Dynamics{};

            if constexpr (has_input_port<Dynamics>)
                for (int i = 0, e = length(dyn.x); i != e; ++i)
                    dyn.x[i] = static_cast<u64>(-1);

            if constexpr (has_output_port<Dynamics>)
                for (int i = 0, e = length(dyn.y); i != e; ++i)
                    dyn.y[i] = static_cast<u64>(-1);

            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                irt_assert(mod().hsms.can_alloc());

                auto& machine = mod().hsms.alloc();
                dyn.id        = mod().hsms.get_id(machine);
            }
        });

        return read_child_model_dynamics(val, mdl);
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

    auto search_dir_in_reg(registred_path& reg, std::string_view name) noexcept
      -> dir_path*
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

    auto search_file(dir_path& dir, std::string_view name) noexcept
      -> file_path*
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

    bool modeling_copy_component_id(
      const small_string<31>&                         reg,
      const small_string<dir_path::path_buffer_len>&  dir,
      const small_string<file_path::path_buffer_len>& file,
      component_id&                                   c_id)
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

        auto* c = search_component(file.sv());
        if (c) {
            c_id = mod().components.get_id(*c);
            return true;
        }

        report_json_error(error_id::modeling_component_missing);
    }

    bool read_child_simple_or_grid_component(const rapidjson::Value& val,
                                             component_id& c_id) noexcept
    {
        auto_stack a(this, stack_id::child_simple_or_grid_component);

        small_string<31>                         reg_name;
        small_string<dir_path::path_buffer_len>  dir_path;
        small_string<file_path::path_buffer_len> file_path;

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

    bool read_child_model(const rapidjson::Value& val,
                          dynamics_type           type,
                          model_id&               c) noexcept
    {
        auto_stack a(this, stack_id::child_model);

        auto& mdl  = mod().models.alloc();
        c          = mod().models.get_id(mdl);
        mdl.type   = type;
        mdl.handle = nullptr;

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
            new (&dyn) Dynamics{};

            if constexpr (has_input_port<Dynamics>)
                for (int i = 0, e = length(dyn.x); i != e; ++i)
                    dyn.x[i] = static_cast<u64>(-1);

            if constexpr (has_output_port<Dynamics>)
                for (int i = 0, e = length(dyn.y); i != e; ++i)
                    dyn.y[i] = static_cast<u64>(-1);

            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                irt_assert(mod().hsms.can_alloc());

                auto& machine = mod().hsms.alloc();
                dyn.id        = mod().hsms.get_id(machine);
            }
        });

        return for_first_member(
          val, "dynamics", [&](const auto& value) noexcept -> bool {
              return dispatch(
                mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept -> bool {
                    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                        if (auto* hsm = mod().hsms.try_to_get(dyn.id); hsm)
                            return read_dynamics(value, *hsm);

                        report_json_error(error_id::modeling_hsm_id_error);
                    } else {
                        return read_dynamics(value, dyn);
                    }
                });
          });
    }

    bool dispatch_child_component_or_model(const rapidjson::Value& val,
                                           dynamics_type           d_type,
                                           child&                  c) noexcept
    {
        auto_stack a(this, stack_id::dispatch_child_component_or_model);

        return c.type == child_type::component
                 ? read_child_component(val, c.id.compo_id)
                 : modeling_can_alloc_model(1) &&
                     read_child_model(val, d_type, c.id.mdl_id);
    }

    bool read_child_component_or_model(const rapidjson::Value& val,
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
               dispatch_child_component_or_model(val, type, c);
    }

    bool read_children_array(const rapidjson::Value& val,
                             generic_component&      generic) noexcept
    {
        auto_stack a(this, stack_id::children_array);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               modeling_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& new_child    = mod().children.alloc();
                     auto  new_child_id = mod().children.get_id(new_child);
                     generic.children.emplace_back(new_child_id);
                     return read_child(value, new_child, new_child_id) &&
                            read_child_component_or_model(value, new_child);
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
                                             copy_to(text.file_path);

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
                                             copy_to(text.file_path);

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

        irt_unreachable();
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

    bool modeling_connect(generic_component& compo,
                          child_id           src_id,
                          i8                 src_port,
                          child_id           dst_id,
                          i8                 dst_port) noexcept
    {
        auto& con              = mod().connections.alloc();
        auto  con_id           = mod().connections.get_id(con);
        con.internal.src       = src_id;
        con.internal.dst       = dst_id;
        con.internal.index_src = src_port;
        con.internal.index_dst = dst_port;
        con.type               = connection::connection_type::internal;
        compo.connections.emplace_back(con_id);

        return true;
    }

    bool modeling_connect_input(generic_component& compo,
                                i8                 src_port,
                                child_id           dst_id,
                                i8                 port) noexcept
    {
        auto& con           = mod().connections.alloc();
        auto  con_id        = mod().connections.get_id(con);
        con.input.dst       = dst_id;
        con.input.index     = src_port;
        con.input.index_dst = port;
        con.type            = connection::connection_type::input;
        compo.connections.emplace_back(con_id);

        return true;
    }

    bool modeling_connect_output(generic_component& compo,
                                 child_id           src_id,
                                 i8                 src_port,
                                 i8                 port) noexcept
    {
        auto& con            = mod().connections.alloc();
        auto  con_id         = mod().connections.get_id(con);
        con.output.src       = src_id;
        con.output.index_src = src_port;
        con.output.index     = port;
        con.type             = connection::connection_type::output;
        compo.connections.emplace_back(con_id);

        return true;
    }

    bool cache_model_mapping_to(child_id& dst) noexcept
    {
        if (auto* elem = cache().model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool read_internal_connection(const rapidjson::Value& val,
                                  generic_component&      compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic_internal_connection);

        child_id          src_id;
        child_id          dst_id;
        std::optional<i8> src_port;
        std::optional<i8> dst_port;

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
                         return read_temp_integer(value) && copy_to(src_port);

                     if ("port-destination"sv == name)
                         return read_temp_integer(value) && copy_to(dst_port);

                     return true;
                 }) &&
               optional_has_value(src_port) && optional_has_value(dst_port) &&
               modeling_connect_internal_can_alloc() &&
               modeling_connect(compo, src_id, *src_port, dst_id, *dst_port);
    }

    bool modeling_connect_internal_can_alloc() noexcept
    {
        if (mod().connections.can_alloc())
            return true;

        report_json_error(error_id::modeling_connect_error);
    }

    bool modeling_connect_output_can_alloc() noexcept
    {
        if (mod().connections.can_alloc())
            return true;

        report_json_error(error_id::modeling_connect_output_error);
    }

    bool modeling_connect_input_can_alloc() noexcept
    {
        if (mod().connections.can_alloc())
            return true;

        report_json_error(error_id::modeling_connect_input_error);
    }

    bool read_output_connection(const rapidjson::Value& val,
                                generic_component&      compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic_output_connection);

        child_id src_id   = undefined<child_id>();
        i8       src_port = 0;
        i8       port     = 0;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("source"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(src_id);
                     if ("port-source"sv == name)
                         return read_temp_integer(value) && copy_to(src_port);
                     if ("port"sv == name)
                         return read_temp_integer(value) && copy_to(port);

                     return true;
                 }) &&
               modeling_connect_output_can_alloc() &&
               modeling_connect_output(compo, src_id, src_port, port);
    }

    bool read_input_connection(const rapidjson::Value& val,
                               generic_component&      compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic_input_connection);

        child_id dst_id   = undefined<child_id>();
        i8       dst_port = 0;
        i8       port     = 0;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("destination"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                cache_model_mapping_to(dst_id);
                     if ("port-destination"sv == name)
                         return read_temp_integer(value) && copy_to(dst_port);

                     if ("port"sv == name)
                         return read_temp_integer(value) && copy_to(port);

                     return true;
                 }) &&
               modeling_connect_input_can_alloc() &&
               modeling_connect_input(compo, port, dst_id, dst_port);
    }

    bool dispatch_connection_type(const rapidjson::Value&     val,
                                  connection::connection_type type,
                                  generic_component&          compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic_dispatch_connection);

        switch (type) {
        case connection::connection_type::internal:
            return read_internal_connection(val, compo);
        case connection::connection_type::output:
            return read_output_connection(val, compo);
        case connection::connection_type::input:
            return read_input_connection(val, compo);
        }

        irt_unreachable();
    }

    bool read_connections(const rapidjson::Value& val,
                          generic_component&      compo) noexcept
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
                               connection::connection_type type =
                                 connection::connection_type::internal;
                               return read_temp_string(value) &&
                                      copy_to(type) &&
                                      dispatch_connection_type(
                                        val_con, type, compo);
                           }

                           return true;
                       });
                 });
    }

    bool read_simple_component(const rapidjson::Value& val,
                               component&              compo) noexcept
    {
        auto_stack s(this, stack_id::component_generic);

        auto& generic      = mod().simple_components.alloc();
        compo.type         = component_type::simple;
        compo.id.simple_id = mod().simple_components.get_id(generic);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("next-unique-id"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(generic.next_unique_id);

              if ("children"sv == name)
                  return read_children(value, generic);

              if ("connections"sv == name)
                  return read_connections(value, generic);

              return true;
          });
    }

    bool grid_children_add(vector<component_id>& out,
                           component_id          c_id) noexcept
    {
        out.emplace_back(c_id);

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

        irt_assert(name.IsString());

        if ("dot-file"sv == name.GetString()) {
            graph.param = graph_component::dot_file_param{};
            return read_graph_param(
              val, *std::get_if<graph_component::dot_file_param>(&graph.param));
        }

        if ("scale-free"sv == name.GetString()) {
            graph.param = graph_component::scale_free_param{};
            return read_graph_param(
              val,
              *std::get_if<graph_component::scale_free_param>(&graph.param));
        }

        if ("small-world"sv == name.GetString()) {
            graph.param = graph_component::small_world_param{};
            return read_graph_param(
              val,
              *std::get_if<graph_component::small_world_param>(&graph.param));
        }

        report_json_error(error_id::graph_component_type_error);
    }

    bool read_graph_param(const rapidjson::Value&          val,
                          graph_component::dot_file_param& p) noexcept
    {
        auto_stack s(this, stack_id::component_graph_param);

        small_string<dir_path::path_buffer_len>  dir_path;
        small_string<file_path::path_buffer_len> file_path;

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

                     if ("connection-type"sv == name)
                         return read_temp_integer(value) &&
                                copy_to(grid.connection_type);

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

    bool dispatch_component_type(const rapidjson::Value& val,
                                 component&              compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            return true;

        case component_type::internal:
            return read_internal_component(val, compo);

        case component_type::simple:
            return read_simple_component(val, compo);

        case component_type::grid:
            return read_grid_component(val, compo);

        case component_type::graph:
            return read_graph_component(val, compo);
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

    bool read_ports(
      const rapidjson::Value&                              val,
      std::array<small_string<7>, component::port_number>& names) noexcept
    {
        auto_stack s(this, stack_id::component_ports);

        return is_value_array(val) &&
               is_value_array_size_equal(val, component::port_number) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_string(value) && copy_to(names[i]);
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
              if ("type"sv == name)
                  return read_temp_string(value) &&
                         convert_to_component(compo) &&
                         dispatch_component_type(val, compo);
              if ("colors"sv == name)
                  return read_component_colors(
                    value,
                    mod().component_colors[get_index(
                      mod().components.get_id(compo))]);
              if ("x"sv == name)
                  return read_ports(value, compo.x_names);
              if ("y"sv == name)
                  return read_ports(value, compo.y_names);

              return true;
          });
    }

    bool read_simulation_model_dynamics(const rapidjson::Value& val,
                                        model&                  mdl) noexcept
    {
        auto_stack s(this, stack_id::simulation_model_dynamics);

        return for_first_member(
          val, "dynamics"sv, [&](const auto& value) -> bool {
              dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
                  new (&dyn) Dynamics{};

                  if constexpr (has_input_port<Dynamics>)
                      for (int i = 0, e = length(dyn.x); i != e; ++i)
                          dyn.x[i] = static_cast<u64>(-1);

                  if constexpr (has_output_port<Dynamics>)
                      for (int i = 0, e = length(dyn.y); i != e; ++i)
                          dyn.y[i] = static_cast<u64>(-1);

                  if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                      irt_assert(sim().hsms.can_alloc());
                      auto& machine = sim().hsms.alloc();
                      dyn.id        = sim().hsms.get_id(machine);
                  }
              });

              return dispatch(
                mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept -> bool {
                    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                        if (auto* hsm = sim().hsms.try_to_get(dyn.id); hsm)
                            return read_dynamics(value, *hsm);

                        report_json_error(error_id::modeling_hsm_id_error);
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

    bool sim_models_can_alloc(std::integral auto i) noexcept
    {
        if (sim().models.can_alloc(i))
            return true;

        report_json_error(error_id::simulation_models_not_enough);
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
                     mdl.handle = nullptr;

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

        output_port* out = nullptr;
        input_port*  in  = nullptr;

        if (is_bad(get_output_port(*mdl_src, port_src, out)))
            report_json_error(error_id::simulation_connect_src_port_unknown);

        if (is_bad(get_input_port(*mdl_dst, port_dst, in)))
            report_json_error(error_id::simulation_connect_dst_port_unknown);

        if (is_bad(sim().connect(*mdl_src, port_src, *mdl_dst, port_dst)))
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
            if (is_success(pj().set(mod(), sim(), *compo)))
                return true;
            else
                report_json_error(error_id::project_set_error);
        } else
            report_json_error(error_id::project_set_no_head);
    }

    bool read_project_top_component(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_top_component);

        small_string<31>                         reg_name;
        small_string<dir_path::path_buffer_len>  dir_path;
        small_string<file_path::path_buffer_len> file_path;
        component_id c_id = undefined<component_id>();

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
                             std::array<real, 4>&    reals) noexcept
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

    bool read_global_parameter_child(const rapidjson::Value& val,
                                     global_parameter&       param) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameter_child);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              project::unique_id_path path;

              if ("access"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_model_ids(path,
                                                 param.children.back().tn_id,
                                                 param.children.back().mdl_id);

              if ("parameter"sv == name)
                  return read_parameter(value, param.params.back());

              return false;
          });
    }

    bool read_global_parameter_children(const rapidjson::Value& val,
                                        global_parameter&       param) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameter_children);

        return is_value_array(val) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     param.children.emplace_back();
                     param.params.emplace_back();
                     return read_global_parameter_child(value, param);
                 });
    }

    bool read_global_parameter(const rapidjson::Value& val,
                               global_parameter&       param) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameter);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(param.name);

              if ("models"sv == name)
                  return read_global_parameter_children(value, param);

              return true;
          });
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
                     auto& param = pj().global_parameters.alloc();
                     return read_global_parameter(value, param);
                 });
    }

    bool read_grid_parameter(const rapidjson::Value& val,
                             grid_parameter&         param) noexcept
    {
        auto_stack s(this, stack_id::project_grid_parameter);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              project::unique_id_path path;
              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(param.name);

              if ("grid"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_id(path, param.child.parent_id);

              if ("access"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_model_ids(
                           path, param.child.tn_id, param.child.mdl_id);

              if ("param"sv == name)
                  return read_parameter(value, param.param);

              return true;
          });
    }

    bool read_grid_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_grid_parameters);

        i64 size = 0;

        return is_value_array(val) && copy_array_size(val, size) &&
               project_grid_parameters_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& param = pj().grid_parameters.alloc();
                     return read_grid_parameter(value, param);
                 });
    }

    bool read_project_parameters(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project_parameters);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("global"sv == name)
                  return read_global_parameters(value);

              if ("grid"sv == name)
                  return read_grid_parameters(value);

              return false;
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

              return false;
          });
    }

    bool convert_to_tn_model_ids(const project::unique_id_path& path,
                                 tree_node_id&                  parent_id,
                                 model_id&                      mdl_id) noexcept
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

    bool convert_to_tn_id(const project::unique_id_path& path,
                          tree_node_id&                  tn_id) noexcept
    {
        auto_stack s(this, stack_id::project_convert_to_tn_id);

        if (auto ret_opt = pj().get_tn_id(path); ret_opt.has_value()) {
            tn_id = *ret_opt;
            return true;
        }

        report_json_error(error_id::project_fail_convert_access_to_tn_id);
    }

    bool read_project_unique_id_path(const rapidjson::Value&  val,
                                     project::unique_id_path& out) noexcept
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

    bool read_color(const rapidjson::Value& val, color& c) noexcept
    {
        auto_stack s(this, stack_id::load_color);

        return is_value_array(val) && is_value_array_size_equal(val, 4) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_unsigned_integer(value) && copy_to(c[i]);
                 });
    }

    bool read_project_plot_observation_child(const rapidjson::Value& val,
                                             plot_observer& plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation_child);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              project::unique_id_path path;

              if ("access"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_model_ids(path,
                                                 plot.children.back().tn_id,
                                                 plot.children.back().mdl_id);

              if ("color"sv == name)
                  return read_color(value, plot.colors.back());

              if ("type"sv == name)
                  return read_temp_string(value) && copy_to(plot.types.back());

              return false;
          });
    }

    bool copy_to(plot_observer::type& type) noexcept
    {
        if (temp_string == "line")
            type = plot_observer::type::line;

        if (temp_string == "dash")
            type = plot_observer::type::dash;

        return false;
    }

    bool read_project_plot_observation_children(const rapidjson::Value& val,
                                                plot_observer& plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation_children);

        return is_value_array(val) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     plot.children.emplace_back();
                     plot.colors.emplace_back();
                     plot.types.emplace_back();
                     return read_project_plot_observation_child(value, plot);
                 });
    }

    bool read_project_plot_observation(const rapidjson::Value& val,
                                       plot_observer&          plot) noexcept
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
               project_plot_observers_can_alloc(size) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& plot = pj().plot_observers.alloc();
                     return read_project_plot_observation(value, plot);
                 });
    }

    bool read_project_grid_observation(const rapidjson::Value& val,
                                       grid_observer&          grid) noexcept
    {
        auto_stack s(this, stack_id::project_grid_observation);

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              project::unique_id_path path;

              if ("name"sv == name)
                  return read_temp_string(value) && copy_to(grid.name);

              if ("grid"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_id(path, grid.child.parent_id);

              if ("access"sv == name)
                  return read_project_unique_id_path(val, path) &&
                         convert_to_tn_model_ids(
                           path, grid.child.tn_id, grid.child.mdl_id);

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

    io_cache*   m_cache = nullptr;
    modeling*   m_mod   = nullptr;
    simulation* m_sim   = nullptr;
    project*    m_pj    = nullptr;

    i64         temp_integer = 0;
    u64         temp_u64     = 0;
    double      temp_double  = 0.0;
    bool        temp_bool    = false;
    std::string temp_string;

    small_vector<stack_id, 16> stack;

    error_id error = error_id::none;

    reader(io_cache& cache_, modeling& mod_) noexcept
      : m_cache(&cache_)
      , m_mod(&mod_)
    {
    }

    reader(io_cache& cache_, simulation& mod_) noexcept
      : m_cache(&cache_)
      , m_sim(&mod_)
    {
    }

    reader(io_cache&   cache_,
           modeling&   mod_,
           simulation& sim_,
           project&    pj_) noexcept
      : m_cache(&cache_)
      , m_mod(&mod_)
      , m_sim(&sim_)
      , m_pj(&pj_)
    {
    }

    struct auto_stack
    {
        auto_stack(reader* r_, const stack_id id) noexcept
          : r(r_)
        {
            irt_assert(r->stack.can_alloc(1));
            r->stack.emplace_back(id);
        }

        ~auto_stack() noexcept
        {
            if (!r->have_error()) {
                irt_assert(!r->stack.empty());
                r->stack.pop_back();
            }
        }

        reader* r = nullptr;
    };

    bool have_error() const noexcept { return error != error_id::none; }

    modeling& mod() const noexcept
    {
        irt_assert(m_mod);
        return *m_mod;
    }

    simulation& sim() const noexcept
    {
        irt_assert(m_sim);
        return *m_sim;
    }

    project& pj() const noexcept
    {
        irt_assert(m_pj);
        return *m_pj;
    }

    io_cache& cache() const noexcept
    {
        irt_assert(m_cache);
        return *m_cache;
    }
};

// Helper functions to read, parse files.
//

static bool copy_filename_to(const char*            filename,
                             std::filesystem::path& dst) noexcept
{
    try {
        dst = std::filesystem::path(filename);
        return true;
    } catch (...) {
        return false;
    }
}

static bool file_exists(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

static bool file_size(const std::filesystem::path& path, u64& len) noexcept
{
    std::error_code ec;
    if (auto size = std::filesystem::file_size(path, ec); !ec) {
        if (is_numeric_castable<u64>(size)) {
            len = size;
            return true;
        }
    }

    return false;
}

static bool buffer_resive(u64 len, vector<char>& vec) noexcept
{
    vec.resize(len);
    return true;
}

static bool file_open(const std::filesystem::path& path,
                      std::ifstream&               ifs) noexcept
{
    ifs.open(path);
    return ifs.is_open() && ifs.good();
}

static bool buffer_fill(std::ifstream& ifs, vector<char>& vec) noexcept
{
    return ifs.read(vec.data(), vec.size()).good();
}

static bool read_file_to_buffer(io_cache& cache, const char* filename) noexcept
{
    std::filesystem::path path;
    std::ifstream         ifs;
    u64                   size = 0;

    return copy_filename_to(filename, path) && file_exists(path) &&
           file_size(path, size) && buffer_resive(size, cache.buffer) &&
           file_open(path, ifs) && buffer_fill(ifs, cache.buffer);
}

static bool parse_json_data(const std::span<char>& buffer,
                            const char*            filename,
                            rapidjson::Document&   doc) noexcept
{
    doc.Parse(buffer.data(), buffer.size());

    if (!doc.HasParseError())
        return true;

#ifdef IRRITATOR_ENABLE_DEBUG
    if (filename)
        fmt::print(stderr,
                   "Fail to parse {}. Error `{}' at offset {}\n",
                   filename,
                   rapidjson::GetParseError_En(doc.GetParseError()),
                   doc.GetErrorOffset());
    else
        fmt::print(stderr,
                   "Fail to parse buffer. Error `{}' at offset {}\n",
                   rapidjson::GetParseError_En(doc.GetParseError()),
                   doc.GetErrorOffset());
    return false;
#endif
}

//
//

template<typename Writer, int QssLevel>
status write(Writer& writer, const abstract_integrator<QssLevel>& dyn) noexcept
{
    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
    return status::success;
}

template<typename Writer, int QssLevel>
status write(Writer& writer,
             const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer, int QssLevel, int PortNumber>
status write(Writer& writer,
             const abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_sum_2& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_sum_3& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_sum_4& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_sum_2& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_sum_3& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_sum_4& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const integrator& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_current_value);
    writer.Key("reset");
    writer.Double(dyn.default_reset_value);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const quantifier& dyn) noexcept
{
    writer.StartObject();
    writer.Key("step-size");
    writer.Double(dyn.default_step_size);
    writer.Key("past-length");
    writer.Int(dyn.default_past_length);
    writer.Key("adapt-state");
    writer.String((dyn.default_adapt_state == quantifier::adapt_state::possible)
                    ? "possible"
                  : dyn.default_adapt_state ==
                      quantifier::adapt_state::impossible
                    ? "impossibe"
                    : "done");
    writer.Key("zero-init-offset");
    writer.Bool(dyn.default_zero_init_offset);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const adder_2& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const adder_3& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const adder_4& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const mult_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const mult_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const mult_4& dyn) noexcept
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
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_values[3]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const counter& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const dynamic_queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const priority_queue& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const generator& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const constant& dyn) noexcept
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
        writer.Double(dyn.port);
        break;
    case constant::init_type::outcoming_component_n:
        writer.String("outcoming_component_n");
        writer.Key("port");
        writer.Double(dyn.port);
        break;
    }
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_filter& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_filter& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_filter& dyn) noexcept
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss1_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss2_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const qss3_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
    return status::success;
}

template<typename Writer, int QssLevel>
status write(Writer& writer, const abstract_square<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.EndObject();
    return status::success;
}

template<typename Writer, int PortNumber>
status write(Writer& writer, const accumulator<PortNumber>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const time_func& dyn) noexcept
{
    writer.StartObject();
    writer.Key("function");
    writer.String(dyn.default_f == &time_function          ? "time"
                  : dyn.default_f == &square_time_function ? "square"
                                                           : "sin");
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(dyn.default_upper_threshold);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const logical_and_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const logical_and_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const logical_or_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const logical_or_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer& writer, const logical_invert& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
    return status::success;
}

template<typename Writer>
status write(Writer&                                         writer,
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
    return status::success;
}

template<typename Writer>
status write(Writer&                                             writer,
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
    return status::success;
}

template<typename Writer>
status write(Writer& writer,
             const hsm_wrapper& /*dyn*/,
             const hierarchical_state_machine& machine) noexcept
{
    writer.StartObject();
    writer.Key("states");
    writer.StartArray();

    constexpr auto length =
      to_unsigned(hierarchical_state_machine::max_number_of_state);
    constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

    std::array<bool, length> states_to_write;
    states_to_write.fill(false);

    for (unsigned i = 0; i != length; ++i) {
        if (machine.states[i].if_transition != invalid)
            states_to_write[machine.states[i].if_transition] = true;
        if (machine.states[i].else_transition != invalid)
            states_to_write[machine.states[i].else_transition] = true;
        if (machine.states[i].super_id != invalid)
            states_to_write[machine.states[i].super_id] = true;
        if (machine.states[i].sub_id != invalid)
            states_to_write[machine.states[i].sub_id] = true;
    }

    for (unsigned i = 0; i != length; ++i) {
        if (states_to_write[i]) {
            writer.Key("id");
            writer.Uint(i);
            write(writer, "enter", machine.states[i].enter_action);
            write(writer, "exit", machine.states[i].exit_action);
            write(writer, "if", machine.states[i].if_action);
            write(writer, "else", machine.states[i].else_action);
            write(writer, "condition", machine.states[i].condition);

            writer.Key("if-transition");
            writer.Int(machine.states[i].if_transition);
            writer.Key("else-transition");
            writer.Int(machine.states[i].else_transition);
            writer.Key("super-id");
            writer.Int(machine.states[i].super_id);
            writer.Key("sub-id");
            writer.Int(machine.states[i].sub_id);
        }
    }
    writer.EndArray();

    writer.Key("outputs");
    writer.StartArray();
    for (int i = 0, e = machine.outputs.ssize(); i != e; ++i) {
        writer.StartObject();
        writer.Key("port");
        writer.Int(machine.outputs[i].port);
        writer.Key("value");
        writer.Int(machine.outputs[i].value);
        writer.EndObject();
    }

    writer.EndArray();
    writer.EndObject();

    return status::success;
}

void io_cache::clear() noexcept
{
    buffer.clear();
    string_buffer.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
}

static bool parse_json_component(modeling&                  mod,
                                 component&                 compo,
                                 io_cache&                  cache,
                                 const rapidjson::Document& doc) noexcept
{
    reader r{ cache, mod };
    if (r.read_component(doc.GetObject(), compo))
        return true;

#ifdef IRRITATOR_ENABLE_DEBUG
    fmt::print(stderr,
               "read component fail with {}\n",
               error_id_names[ordinal(r.error)]);

    for (auto i = 0u; i < r.stack.size(); ++i)
        fmt::print(stderr,
                   "  {}: {}\n",
                   static_cast<int>(i),
                   stack_id_names[ordinal(r.stack[i])]);
#endif

    return false;
}

static bool parse_component(modeling&   mod,
                            component&  compo,
                            io_cache&   cache,
                            const char* filename) noexcept
{
    rapidjson::Document doc;

    return read_file_to_buffer(cache, filename) &&
           parse_json_data(std::span(cache.buffer.data(), cache.buffer.size()),
                           filename,
                           doc) &&
           parse_json_component(mod, compo, cache, doc);
}

static bool parse_component(modeling&       mod,
                            component&      compo,
                            io_cache&       cache,
                            std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    return parse_json_data(buffer, nullptr, doc) &&
           parse_json_component(mod, compo, cache, doc);
}

status component_load(modeling&   mod,
                      component&  compo,
                      io_cache&   cache,
                      const char* filename) noexcept
{
    irt_return_if_fail(parse_component(mod, compo, cache, filename),
                       status::io_file_format_model_error);

    return status::success;
}

status component_load(modeling&       mod,
                      component&      compo,
                      io_cache&       cache,
                      std::span<char> buffer) noexcept
{
    irt_return_if_fail(parse_component(mod, compo, cache, buffer),
                       status::io_file_format_model_error);

    return status::success;
}

template<typename Writer>
static void write_constant_sources(io_cache& /*cache*/,
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
static void write_binary_file_sources(io_cache& /*cache*/,
                                      const external_source& srcs,
                                      Writer&                w) noexcept
{
    w.Key("binary-file-sources");
    w.StartArray();

    const binary_file_source* src = nullptr;
    while (srcs.binary_file_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
        w.Key("max-clients");
        w.Uint(src->max_clients);
        w.Key("path");
        w.String(src->file_path.string().c_str());
        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_text_file_sources(io_cache& /*cache*/,
                                    const external_source& srcs,
                                    Writer&                w) noexcept
{
    w.Key("text-file-sources");
    w.StartArray();

    const text_file_source* src = nullptr;
    while (srcs.text_file_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
        w.Key("path");
        w.String(src->file_path.string().c_str());
        w.EndObject();
    }

    w.EndArray();
}

template<typename Writer>
static void write_random_sources(io_cache& /*cache*/,
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
static status write_child_component_path(Writer&               w,
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

    return status::success;
}

template<typename Writer>
static status write_child_component_path(const modeling&  mod,
                                         const component& compo,
                                         Writer&          w) noexcept
{
    auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
    irt_return_if_fail(reg, status::io_project_file_component_directory_error);
    irt_return_if_fail(!reg->path.empty(),
                       status::io_project_file_component_directory_error);
    irt_return_if_fail(!reg->name.empty(),
                       status::io_project_file_component_directory_error);

    auto* dir = mod.dir_paths.try_to_get(compo.dir);
    irt_return_if_fail(dir, status::io_project_file_component_directory_error);
    irt_return_if_fail(!dir->path.empty(),
                       status::io_project_file_component_directory_error);

    auto* file = mod.file_paths.try_to_get(compo.file);
    irt_return_if_fail(file, status::io_project_file_error);
    irt_return_if_fail(!file->path.empty(), status::io_project_file_error);

    return write_child_component_path(w, *reg, *dir, *file);
}

template<typename Writer>
static status write_child_component(const modeling&    mod,
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
            return status::success;

        case component_type::internal:
            w.Key("parameter");
            w.String(internal_component_names[ordinal(compo->id.internal_id)]);
            return status::success;

        case component_type::grid:
            return write_child_component_path(mod, *compo, w);

        case component_type::graph:
            return write_child_component_path(mod, *compo, w);

        case component_type::simple:
            return write_child_component_path(mod, *compo, w);
        }
        irt_unreachable();

    } else {
        w.Key("component-type");
        w.String(component_type_names[ordinal(component_type::none)]);
        return status::success;
    }
}

template<typename Writer>
static status write_child_model(const modeling& mod,
                                model&          mdl,
                                Writer&         w) noexcept
{
    w.Key("dynamics");

    return dispatch(mdl,
                    [&mod, &w]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                            auto* hsm = mod.hsms.try_to_get(dyn.id);
                            irt_assert(hsm);
                            return write(w, dyn, *hsm);
                        } else {
                            return write(w, dyn);
                        }
                    });
}

template<typename Writer>
static status write_child(const modeling& mod,
                          const child&    ch,
                          const u64       unique_id,
                          Writer&         w) noexcept
{
    const auto child_id = mod.children.get_id(ch);

    w.StartObject();
    w.Key("id");
    w.Uint64(get_index(child_id));
    w.Key("unique-id");
    w.Uint64(unique_id);
    w.Key("x");
    w.Double(mod.children_positions[get_index(child_id)].x);
    w.Key("y");
    w.Double(mod.children_positions[get_index(child_id)].y);
    w.Key("name");
    w.String(mod.children_names[get_index(child_id)].c_str());
    w.Key("configurable");
    w.Bool(ch.flags & child_flags_configurable);
    w.Key("observable");
    w.Bool(ch.flags & child_flags_observable);

    if (ch.type == child_type::component) {
        const auto compo_id = ch.id.compo_id;
        if (auto* compo = mod.components.try_to_get(compo_id); compo) {
            w.Key("type");
            w.String("component");

            irt_return_if_bad(write_child_component(mod, compo_id, w));
        }
    } else {
        const auto mdl_id = ch.id.mdl_id;
        if (auto* mdl = mod.models.try_to_get(mdl_id); mdl) {
            w.Key("type");
            w.String(dynamics_type_names[ordinal(mdl->type)]);

            irt_return_if_bad(write_child_model(mod, *mdl, w));
        }
    }

    w.EndObject();

    return status::success;
}

template<typename Writer>
static status write_simple_component_children(
  io_cache& /*cache*/,
  const modeling&          mod,
  const generic_component& simple_compo,
  Writer&                  w) noexcept
{
    w.Key("children");
    w.StartArray();

    for (auto child_id : simple_compo.children)
        if (auto* c = mod.children.try_to_get(child_id); c)
            irt_return_if_bad(write_child(mod,
                                          *c,
                                          c->unique_id == 0
                                            ? simple_compo.make_next_unique_id()
                                            : c->unique_id,
                                          w));

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_component_ports(io_cache& /*cache*/,
                                    const modeling& /*mod*/,
                                    const component& compo,
                                    Writer&          w) noexcept
{
    w.Key("x");
    w.StartArray();

    for (int i = 0; i != component::port_number; ++i)
        w.String(compo.x_names[static_cast<unsigned>(i)].c_str());

    w.EndArray();

    w.Key("y");
    w.StartArray();

    for (int i = 0; i != component::port_number; ++i)
        w.String(compo.y_names[static_cast<unsigned>(i)].c_str());

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_simple_component_connections(io_cache& /*cache*/,
                                                 const modeling&          mod,
                                                 const generic_component& compo,
                                                 Writer& w) noexcept
{
    w.Key("connections");
    w.StartArray();

    for (auto connection_id : compo.connections) {
        if (auto* c = mod.connections.try_to_get(connection_id); c) {
            w.StartObject();

            w.Key("type");

            switch (c->type) {
            case connection::connection_type::input:
                w.String("input");
                w.Key("port");
                w.Int(c->input.index);
                w.Key("destination");
                w.Uint64(get_index(c->input.dst));
                w.Key("port-destination");
                w.Int(c->input.index_dst);
                break;
            case connection::connection_type::internal:
                w.String("internal");
                w.Key("source");
                w.Uint64(get_index(c->internal.src));
                w.Key("port-source");
                w.Int(c->internal.index_src);
                w.Key("destination");
                w.Uint64(get_index(c->internal.dst));
                w.Key("port-destination");
                w.Int(c->internal.index_dst);
                break;
            case connection::connection_type::output:
                w.String("output");
                w.Key("source");
                w.Uint64(get_index(c->output.src));
                w.Key("port-source");
                w.Int(c->output.index_src);
                w.Key("port");
                w.Int(c->output.index);
                break;
            }

            w.EndObject();
        }
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_simple_component(io_cache&                cache,
                                     const modeling&          mod,
                                     const generic_component& s_compo,
                                     Writer&                  w) noexcept
{
    w.String("next-unique-id");
    w.Uint64(s_compo.next_unique_id);

    write_simple_component_children(cache, mod, s_compo, w);
    write_simple_component_connections(cache, mod, s_compo, w);

    return status::success;
}

template<typename Writer>
static status write_grid_component(io_cache& /*cache*/,
                                   const modeling&       mod,
                                   const grid_component& grid,
                                   Writer&               w) noexcept
{
    w.Key("rows");
    w.Int(grid.row);
    w.Key("columns");
    w.Int(grid.column);
    w.Key("connection-type");
    w.Int(ordinal(grid.connection_type));

    w.Key("children");
    w.StartArray();
    for (auto& elem : grid.children) {
        w.StartObject();
        write_child_component(mod, elem, w);
        w.EndObject();
    }
    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_graph_component_param(
  const modeling&                            mod,
  const graph_component::random_graph_param& param,
  Writer&                                    w) noexcept
{
    switch (param.index()) {
    case 0: {
        w.String("dot-file");
        auto& p = *std::get_if<graph_component::dot_file_param>(&param);

        if (auto* dir = mod.dir_paths.try_to_get(p.dir); dir) {
            w.Key("dir");
            w.String(dir->path.begin(), dir->path.size());
        }

        if (auto* file = mod.file_paths.try_to_get(p.file); file) {
            w.Key("file");
            w.String(file->path.begin(), file->path.size());
        }

        return status::success;
    }

    case 1: {
        w.String("scale-free");
        auto& p = *std::get_if<graph_component::scale_free_param>(&param);
        w.Key("alpha");
        w.Double(p.alpha);
        w.Key("beta");
        w.Double(p.beta);

        return status::success;
    }

    case 2: {
        w.String("small-world");
        auto& p = *std::get_if<graph_component::small_world_param>(&param);
        w.Key("probability");
        w.Double(p.probability);
        w.Key("k");
        w.Int(p.k);

        return status::success;
    }
    }

    static_assert(std::variant_size_v<graph_component::random_graph_param> ==
                  3);
    irt_unreachable();
}

template<typename Writer>
static status write_graph_component(io_cache& /*cache*/,
                                    const modeling&        mod,
                                    const graph_component& graph,
                                    Writer&                w) noexcept
{
    w.Key("graph-type");
    write_graph_component_param(mod, graph.param, w);

    w.Key("children");
    w.StartArray();
    for (auto& elem : graph.children) {
        w.StartObject();
        write_child_component(mod, elem, w);
        w.EndObject();
    }
    w.EndArray();

    return status::success;
}

template<typename Writer>
static void write_internal_component(io_cache& /*cache*/,
                                     const modeling& /* mod */,
                                     const internal_component id,
                                     Writer&                  w) noexcept
{
    w.Key("component");
    w.String(internal_component_names[ordinal(id)]);
}

template<typename Writer>
static status do_component_save(Writer&          w,
                                const modeling&  mod,
                                const component& compo,
                                io_cache&        cache) noexcept
{
    status ret = status::success;

    w.StartObject();

    w.Key("name");
    w.String(compo.name.c_str());

    write_constant_sources(cache, mod.srcs, w);
    write_binary_file_sources(cache, mod.srcs, w);
    write_text_file_sources(cache, mod.srcs, w);
    write_random_sources(cache, mod.srcs, w);

    w.Key("type");
    w.String(component_type_names[ordinal(compo.type)]);

    w.Key("colors");
    w.StartArray();
    auto& color = mod.component_colors[get_index(mod.components.get_id(compo))];
    w.Double(color[0]);
    w.Double(color[1]);
    w.Double(color[2]);
    w.Double(color[3]);
    w.EndArray();

    write_component_ports(cache, mod, compo, w);

    switch (compo.type) {
    case component_type::internal:
        write_internal_component(cache, mod, compo.id.internal_id, w);
        break;

    case component_type::simple: {
        ret = if_data_exists_return(
          mod.simple_components,
          compo.id.simple_id,
          [&](auto& generic) noexcept -> status {
              return write_simple_component(cache, mod, generic, w);
          },
          status::unknown_dynamics); // @TODO undefined component.
    } break;

    case component_type::grid: {
        ret = if_data_exists_return(
          mod.grid_components,
          compo.id.grid_id,
          [&](auto& grid) noexcept -> status {
              return write_grid_component(cache, mod, grid, w);
          },
          status::unknown_dynamics); // @TODO undefined component
    } break;

    case component_type::graph: {
        ret = if_data_exists_return(
          mod.graph_components,
          compo.id.graph_id,
          [&](auto& graph) noexcept -> status {
              return write_graph_component(cache, mod, graph, w);
          },
          status::unknown_dynamics); // @TODO undefined component
    } break;

    default:
        break;
    }

    w.EndObject();

    return ret;
}

status component_save(const modeling&   mod,
                      const component&  compo,
                      io_cache&         cache,
                      const char*       filename,
                      json_pretty_print print_options) noexcept
{
    file f{ filename, open_mode::write };

    irt_return_if_fail(f.is_open(), status::io_file_format_error);

    FILE* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    switch (print_options) {
    case json_pretty_print::indent_2:
        w.SetIndent(' ', 2);
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;

    case json_pretty_print::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;

    default:
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;
    }

    return status::success;
}

status component_save(const modeling&   mod,
                      const component&  compo,
                      io_cache&         cache,
                      vector<char>&     out,
                      json_pretty_print print_options) noexcept
{
    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    switch (print_options) {
    case json_pretty_print::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;
    }

    case json_pretty_print::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;
    }

    default: {
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        irt_return_if_bad(do_component_save(w, mod, compo, cache));
        break;
    }
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return status::success;
}

/*****************************************************************************
 *
 * Simulation file read part
 *
 ****************************************************************************/

template<typename Writer>
static status write_simulation_model(const simulation& sim, Writer& w) noexcept
{
    w.Key("models");
    w.StartArray();

    const model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        const auto mdl_id = sim.models.get_id(*mdl);

        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(mdl_id));
        w.Key("type");
        w.String(dynamics_type_names[ordinal(mdl->type)]);
        w.Key("dynamics");

        dispatch(*mdl,
                 [&w, &sim]<typename Dynamics>(const Dynamics& dyn) -> void {
                     if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                         auto* hsm = sim.hsms.try_to_get(dyn.id);
                         irt_assert(hsm);
                         write(w, dyn, *hsm);
                     } else {
                         write(w, dyn);
                     }
                 });

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_simulation_connections(const simulation& sim,
                                           Writer&           w) noexcept
{
    w.Key("connections");
    w.StartArray();

    const model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        dispatch(*mdl, [&sim, &mdl, &w]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_output_port<Dynamics>) {
                for (auto i = 0, e = length(dyn.y); i != e; ++i) {
                    auto list = get_node(sim, dyn.y[i]);
                    for (const auto& cnt : list) {
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
        });
    }

    w.EndArray();
    return status::success;
}

template<typename Writer>
status do_simulation_save(Writer&           w,
                          const simulation& sim,
                          io_cache&         cache) noexcept
{
    w.StartObject();

    write_constant_sources(cache, sim.srcs, w);
    write_binary_file_sources(cache, sim.srcs, w);
    write_text_file_sources(cache, sim.srcs, w);
    write_random_sources(cache, sim.srcs, w);

    write_simulation_model(sim, w);
    write_simulation_connections(sim, w);

    w.EndObject();

    return status::success;
}

status simulation_save(const simulation& sim,
                       io_cache&         cache,
                       const char*       filename,
                       json_pretty_print print_options) noexcept
{
    file f{ filename, open_mode::write };
    irt_return_if_fail(f.is_open(), status::io_filesystem_error);

    FILE* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    switch (print_options) {
    case json_pretty_print::indent_2:
        w.SetIndent(' ', 2);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;

    case json_pretty_print::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;

    default:
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }

    return status::success;
}

status simulation_save(const simulation& sim,
                       io_cache&         cache,
                       vector<char>&     out,
                       json_pretty_print print_options) noexcept
{
    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    switch (print_options) {
    case json_pretty_print::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }

    case json_pretty_print::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }

    default: {
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return status::success;
}

static bool parse_json_simulation(simulation&                sim,
                                  io_cache&                  cache,
                                  const rapidjson::Document& doc) noexcept
{
    sim.clear();

    reader r{ cache, sim };
    if (r.read_simulation(doc.GetObject()))
        return true;

#ifdef IRRITATOR_ENABLE_DEBUG
    fmt::print(stderr,
               "read simulation fail with {}\n",
               error_id_names[ordinal(r.error)]);

    for (auto i = 0u; i < r.stack.size(); ++i)
        fmt::print(stderr,
                   "  {}: {}\n",
                   static_cast<int>(i),
                   stack_id_names[ordinal(r.stack[i])]);
#endif

    return false;
}

static bool parse_simulation(simulation& sim,
                             io_cache&   cache,
                             const char* filename) noexcept
{
    rapidjson::Document doc;

    return read_file_to_buffer(cache, filename) &&
           parse_json_data(std::span(cache.buffer.data(), cache.buffer.size()),
                           filename,
                           doc) &&
           parse_json_simulation(sim, cache, doc);
}

static bool parse_simulation(simulation&     sim,
                             io_cache&       cache,
                             std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    return parse_json_data(buffer, nullptr, doc) &&
           parse_json_simulation(sim, cache, doc);
}

status simulation_load(simulation& sim,
                       io_cache&   cache,
                       const char* filename) noexcept
{
    irt_return_if_fail(parse_simulation(sim, cache, filename),
                       status::io_file_format_model_error);

    return status::success;
}

status simulation_load(simulation&     sim,
                       io_cache&       cache,
                       std::span<char> buffer) noexcept
{
    irt_return_if_fail(parse_simulation(sim, cache, buffer),
                       status::io_file_format_model_error);

    return status::success;
}

/*****************************************************************************
 *
 * Project file read part
 *
 ****************************************************************************/

static bool parse_json_project(project&                   pj,
                               modeling&                  mod,
                               simulation&                sim,
                               io_cache&                  cache,
                               const rapidjson::Document& doc) noexcept
{
    pj.clear();
    sim.clear();

    reader r{ cache, mod, sim, pj };
    if (r.read_project(doc.GetObject()))
        return true;

#ifdef IRRITATOR_ENABLE_DEBUG
    fmt::print(
      stderr, "read project fail with {}\n", error_id_names[ordinal(r.error)]);

    for (auto i = 0u; i < r.stack.size(); ++i)
        fmt::print(stderr,
                   "  {}: {}\n",
                   static_cast<int>(i),
                   stack_id_names[ordinal(r.stack[i])]);
#endif

    return false;
}

bool parse_project(project&    pj,
                   modeling&   mod,
                   simulation& sim,
                   io_cache&   cache,
                   const char* filename) noexcept
{
    rapidjson::Document doc;

    return read_file_to_buffer(cache, filename) &&
           parse_json_data(std::span(cache.buffer.data(), cache.buffer.size()),
                           filename,
                           doc) &&
           parse_json_project(pj, mod, sim, cache, doc);
}

bool parse_project(project&        pj,
                   modeling&       mod,
                   simulation&     sim,
                   io_cache&       cache,
                   std::span<char> buffer) noexcept
{
    rapidjson::Document doc;

    return parse_json_data(buffer, nullptr, doc) &&
           parse_json_project(pj, mod, sim, cache, doc);
}

status project_load(project&    pj,
                    modeling&   mod,
                    simulation& sim,
                    io_cache&   cache,
                    const char* filename) noexcept
{
    irt_return_if_fail(parse_project(pj, mod, sim, cache, filename),
                       status::io_file_format_error);

    return status::success;
}

status project_load(project&        pj,
                    modeling&       mod,
                    simulation&     sim,
                    io_cache&       cache,
                    std::span<char> buffer) noexcept
{
    irt_return_if_fail(parse_project(pj, mod, sim, cache, buffer),
                       status::io_file_format_error);

    return status::success;
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
static void write_project_unique_id_path(
  Writer&                        w,
  const project::unique_id_path& path) noexcept
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
    irt_return_if_bad(do_project_save_global_parameters(w, pj));
    irt_return_if_bad(do_project_save_grid_parameters(w, pj));
    w.EndObject();

    return status::success;
}

template<typename Writer>
static status do_project_save_plot_observations(Writer& w, project& pj) noexcept
{
    w.Key("global");
    w.StartArray();

    for_each_data(pj.plot_observers, [&](auto& plot) noexcept {
        w.StartObject();
        w.Key("name");
        w.String(plot.name.begin(), plot.name.size());
        w.Key("models");
        w.StartArray();

        project::unique_id_path path;
        for (auto node : plot.children) {
            w.StartObject();
            w.Key("access");
            pj.build_unique_id_path(node.tn_id, node.mdl_id, path);
            write_project_unique_id_path(w, path);

            w.Key("color");
            write_color(w, color_white);

            w.Key("type");
            w.String("line");
            w.EndObject();
        }

        w.EndArray();
        w.EndObject();
    });

    w.EndArray();

    return status::success;
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

        project::unique_id_path path;
        w.Key("grid");
        write_project_unique_id_path(w, path);
        pj.build_unique_id_path(grid.child.parent_id, path);

        w.Key("access");
        pj.build_unique_id_path(grid.child.tn_id, grid.child.mdl_id, path);

        w.EndObject();
    });

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status do_project_save_observations(Writer& w, project& pj) noexcept
{
    w.Key("observations");

    w.StartObject();
    irt_return_if_bad(do_project_save_plot_observations(w, pj));
    irt_return_if_bad(do_project_save_grid_observations(w, pj));
    w.EndObject();

    return status::success;
}

template<typename Writer>
static status write_parameter(Writer& w, const parameter& param) noexcept
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

    return status::success;
}

template<typename Writer>
static status do_project_save_global_parameter(Writer&           w,
                                               project&          pj,
                                               global_parameter& param) noexcept
{
    w.StartObject();
    w.Key("name");
    w.String(param.name.begin(), param.name.size());

    w.Key("models");
    w.StartArray();
    for (int i = 0, e = param.children.ssize(); i != e; ++i) {
        w.StartObject();
        project::unique_id_path path;
        w.Key("access");
        pj.build_unique_id_path(
          param.children[i].tn_id, param.children[i].mdl_id, path);
        write_project_unique_id_path(w, path);

        write_parameter(w, param.params[i]);
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();

    return status::success;
}

template<typename Writer>
static status do_project_save_global_parameters(Writer& w, project& pj) noexcept
{
    w.Key("global");
    w.StartArray();

    for_each_data(pj.global_parameters, [&](auto& param) noexcept {
        do_project_save_global_parameter(w, pj, param);
    });

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status do_project_save_grid_parameters(Writer& w, project& pj) noexcept
{
    w.Key("grid");
    w.StartArray();

    for_each_data(pj.grid_parameters, [&](auto& param) noexcept {
        w.StartObject();
        w.Key("name");
        w.String(param.name.begin(), param.name.size());

        project::unique_id_path path;
        w.Key("grid");
        write_project_unique_id_path(w, path);
        pj.build_unique_id_path(param.child.parent_id, path);

        w.Key("access");
        pj.build_unique_id_path(param.child.tn_id, param.child.mdl_id, path);

        write_parameter(w, param.param);

        w.EndObject();
    });

    w.EndArray();

    return status::success;
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

    return status::success;
}

template<typename Writer>
static status do_project_save(Writer&    w,
                              project&   pj,
                              modeling&  mod,
                              component& compo,
                              io_cache& /* cache */) noexcept
{
    auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
    irt_return_if_fail(reg, status::io_project_file_component_directory_error);
    irt_return_if_fail(!reg->path.empty(),
                       status::io_project_file_component_directory_error);
    irt_return_if_fail(!reg->name.empty(),
                       status::io_project_file_component_directory_error);

    auto* dir = mod.dir_paths.try_to_get(compo.dir);
    irt_return_if_fail(dir, status::io_project_file_component_directory_error);
    irt_return_if_fail(!dir->path.empty(),
                       status::io_project_file_component_directory_error);

    auto* file = mod.file_paths.try_to_get(compo.file);
    irt_return_if_fail(file, status::io_project_file_error);
    irt_return_if_fail(!file->path.empty(), status::io_project_file_error);

    w.StartObject();
    irt_return_if_bad(do_project_save_component(w, compo, *reg, *dir, *file));
    irt_return_if_bad(do_project_save_parameters(w, pj));
    irt_return_if_bad(do_project_save_observations(w, pj));
    w.EndObject();

    return status::success;
}

status project_save(project&  pj,
                    modeling& mod,
                    simulation& /* sim */,
                    io_cache&         cache,
                    const char*       filename,
                    json_pretty_print print_options) noexcept
{
    if (auto* compo = mod.components.try_to_get(pj.head()); compo) {
        if (auto* parent = pj.tn_head(); parent) {
            irt_assert(mod.components.get_id(compo) == parent->id);

            file f{ filename, open_mode::write };
            irt_return_if_fail(f.is_open(), status::io_project_file_error);

            auto* reg  = mod.registred_paths.try_to_get(compo->reg_path);
            auto* dir  = mod.dir_paths.try_to_get(compo->dir);
            auto* file = mod.file_paths.try_to_get(compo->file);
            irt_return_if_fail(reg && dir && file, status::io_filesystem_error);

            auto* fp = reinterpret_cast<FILE*>(f.get_handle());
            cache.clear();
            cache.buffer.resize(4096);

            rapidjson::FileWriteStream os(
              fp, cache.buffer.data(), cache.buffer.size());
            rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

            switch (print_options) {
            case json_pretty_print::indent_2:
                w.SetIndent(' ', 2);
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
                break;

            case json_pretty_print::indent_2_one_line_array:
                w.SetIndent(' ', 2);
                w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
                break;

            default:
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
                break;
            }

            return status::success;
        }
    }

    // @TODO head is not defined
    irt_bad_return(status::block_allocator_bad_capacity);
}

status project_save(project&  pj,
                    modeling& mod,
                    simulation& /* sim */,
                    io_cache&         cache,
                    vector<char>&     out,
                    json_pretty_print print_options) noexcept
{
    if (auto* compo = mod.components.try_to_get(pj.head()); compo) {
        if (auto* parent = pj.tn_head(); parent) {
            irt_assert(mod.components.get_id(compo) == parent->id);

            rapidjson::StringBuffer buffer;
            buffer.Reserve(4096u);

            switch (print_options) {
            case json_pretty_print::indent_2: {
                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
                w.SetIndent(' ', 2);
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
            } break;

            case json_pretty_print::indent_2_one_line_array: {
                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
                w.SetIndent(' ', 2);
                w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
            } break;

            default: {

                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
                irt_return_if_bad(do_project_save(w, pj, mod, *compo, cache));
            } break;
            }

            auto length = buffer.GetSize();
            auto str    = buffer.GetString();
            out.resize(static_cast<int>(length));
            std::copy_n(str, length, out.data());

            return status::success;
        }
    }

    // @TODO head is not defined
    irt_bad_return(status::block_allocator_bad_capacity);
}

} //  irt
