// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include <cerrno>
#include <cstdint>

using namespace std::literals;

namespace irt {

template<typename Callback, typename ErrorCode, typename... Args>
static constexpr bool report_error(Callback  cb,
                                   ErrorCode ec,
                                   Args&&... args) noexcept
{
    if (cb) {
        if constexpr (sizeof...(args) == 0)
            cb(ec, std::monostate{});
        else
            cb(ec, std::forward<Args>(args)...);
    }

    return false;
}

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

struct json_dearchiver::impl {
    json_dearchiver&          self;
    json_dearchiver::error_cb cb;

    impl(json_dearchiver&          self_,
         modeling&                 mod_,
         json_dearchiver::error_cb cb_) noexcept
      : self(self_)
      , cb(cb_)
      , m_mod(&mod_)
    {}

    impl(json_dearchiver&          self_,
         simulation&               sim_,
         json_dearchiver::error_cb cb_) noexcept
      : self(self_)
      , cb(cb_)
      , m_sim(&sim_)
    {}

    impl(json_dearchiver&          self_,
         modeling&                 mod_,
         simulation&               sim_,
         project&                  pj_,
         json_dearchiver::error_cb cb_) noexcept
      : self(self_)
      , cb(cb_)
      , m_mod(&mod_)
      , m_sim(&sim_)
      , m_pj(&pj_)
    {}

    struct auto_stack {
        auto_stack(json_dearchiver::impl* r_, const stack_id id) noexcept
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

        json_dearchiver::impl* r = nullptr;
    };

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

    bool copy_to(bool& dst) const noexcept
    {
        dst = temp_bool;

        return true;
    }

    bool copy_to(double& dst) const noexcept
    {
        dst = temp_double;

        return true;
    }

    bool copy_to(i64& dst) const noexcept
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
        if (!(temp_u64 <= UINT8_MAX))
            report_json_error(error_id::integer_to_u8_error);

        dst = static_cast<u8>(temp_u64);

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

    bool copy_to(u64& dst) const noexcept
    {
        dst = temp_u64;

        return true;
    }

    bool copy_to(std::optional<double>& dst) noexcept
    {
        dst = temp_double;
        return true;
    }

    bool copy_to(std::optional<u8>& dst) noexcept
    {
        if (!(temp_u64 <= UINT8_MAX))
            report_json_error(error_id::integer_to_i8_error);

        dst = static_cast<u8>(temp_integer);
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

    bool copy_to(source::source_type& dst) const noexcept
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

    bool affect_configurable_to(bitflags<child_flags>& flag) const noexcept
    {
        flag.set(child_flags::configurable, temp_bool);

        return true;
    }

    bool affect_observable_to(bitflags<child_flags>& flag) const noexcept
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
        bool       stop_on_error = false;

        dyn.flags.reset();

        return for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
              if ("offset"sv == name)
                  return read_temp_real(value) &&
                         is_double_greater_equal_than(0.0) &&
                         copy_to(dyn.default_offset);

              if ("source-ta-type"sv == name) {
                  if (read_temp_integer(value) &&
                      is_int_greater_equal_than(0) &&
                      is_int_less_than(source::source_type_count) &&
                      copy_to(dyn.default_source_ta.type)) {
                      dyn.flags.set(generator::option::ta_use_source);
                      return true;
                  }
                  return false;
              }

              if ("source-ta-id"sv == name) {
                  if (read_temp_unsigned_integer(value) &&
                      copy_to(dyn.default_source_ta.id)) {
                      dyn.flags.set(generator::option::ta_use_source);
                      return true;
                  }
                  return false;
              }

              if ("source-value-type"sv == name) {
                  if (read_temp_integer(value) &&
                      is_int_greater_equal_than(0) &&
                      is_int_less_than(source::source_type_count) &&
                      copy_to(dyn.default_source_value.type)) {
                      dyn.flags.set(generator::option::value_use_source);
                      return true;
                  }
                  return false;
              }

              if ("source-value-id"sv == name) {
                  if (read_temp_unsigned_integer(value) &&
                      copy_to(dyn.default_source_value.id)) {
                      dyn.flags.set(generator::option::value_use_source);
                      return true;
                  }
                  return false;
              }

              if ("stop-on-error"sv == name) {
                  if (read_temp_bool(value) && copy_to(stop_on_error)) {
                      dyn.flags.set(generator::option::stop_on_error);
                      return true;
                  }
                  return false;
              }

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
        auto_stack          a(this, stack_id::dynamics_hsm_condition_action);
        std::optional<u8>   port;
        std::optional<u8>   mask;
        std::optional<i32>  i;
        std::optional<real> f;

        auto ret = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
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
                           hierarchical_state_machine::condition_type_count) &&
                         copy_to(s.type);

              if ("port"sv == name)
                  return read_temp_integer(value) && copy_to(port);

              if ("mask"sv == name)
                  return read_temp_integer(value) && copy_to(mask);

              if ("constant-i"sv == name)
                  return read_temp_integer(value) && copy_to(i);

              if ("constant-r"sv == name)
                  return read_temp_real(value) && copy_to(f);

              report_json_error(error_id::unknown_element);
          });

        if (not ret)
            return ret;

        if (port.has_value() and mask.has_value())
            s.set(*port, *mask);
        else if (i.has_value())
            s.constant.i = *i;
        else if (f.has_value())
            s.constant.f = static_cast<float>(*f);

        return true;
    }

    bool read_hsm_state_action(
      const rapidjson::Value&                   val,
      hierarchical_state_machine::state_action& s) noexcept
    {
        auto_stack          a(this, stack_id::dynamics_hsm_state_action);
        std::optional<i32>  i;
        std::optional<real> f;

        auto ret = for_each_member(
          val, [&](const auto name, const auto& value) noexcept -> bool {
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

              if ("constant-i"sv == name)
                  return read_temp_integer(value) && copy_to(i);

              if ("constant-r"sv == name)
                  return read_temp_real(value) && copy_to(f);

              report_json_error(error_id::unknown_element);
          });

        if (not ret)
            return ret;

        if (i.has_value())
            s.constant.i = *i;
        else if (f.has_value())
            s.constant.f = static_cast<float>(*f);

        return true;
    }

    bool read_hsm_state(
      const rapidjson::Value&                                      val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>& states,
      std::array<name_str, hierarchical_state_machine::max_number_of_state>&
        names,
      std::array<position, hierarchical_state_machine::max_number_of_state>&
        positions) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_state);

        std::optional<int>                id;
        hierarchical_state_machine::state s;
        name_str                          state_name;
        position                          pos;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("id"sv == name)
                      return read_temp_integer(value) and copy_to(id);

                  if ("name"sv == name)
                      return read_temp_string(value) and copy_to(state_name);

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
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.if_transition);

                  if ("else-transition"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.else_transition);

                  if ("super-id"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.super_id);

                  if ("sub-id"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.sub_id);

                  if ("x"sv == name)
                      return read_temp_real(value) && copy_to(pos.x);

                  if ("y"sv == name)
                      return read_temp_real(value) && copy_to(pos.y);

                  report_json_error(error_id::unknown_element);
              }) or
            not id.has_value())
            return false;

        states[*id]    = s;
        names[*id]     = state_name;
        positions[*id] = pos;

        return true;
    }

    bool read_hsm_state(
      const rapidjson::Value& val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>&
        states) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_state);

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
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.if_transition);

                  if ("else-transition"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.else_transition);

                  if ("super-id"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.super_id);

                  if ("sub-id"sv == name)
                      return read_temp_unsigned_integer(value) &&
                             copy_to(s.sub_id);

                  report_json_error(error_id::unknown_element);
              }) or
            not id.has_value())
            return false;

        states[*id] = s;

        return true;
    }

    bool read_hsm_states(
      const rapidjson::Value&                                      val,
      std::array<hierarchical_state_machine::state,
                 hierarchical_state_machine::max_number_of_state>& states,
      std::array<name_str, hierarchical_state_machine::max_number_of_state>&
        names,
      std::array<position, hierarchical_state_machine::max_number_of_state>&
        positions) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm_states);

        return for_each_array(
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states, names, positions);
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
          val, [&](const auto /*i*/, const auto& value) noexcept -> bool {
              return read_hsm_state(value, states);
          });
    }

    bool read_simulation_dynamics(const rapidjson::Value& val,
                                  hsm_wrapper&            wrapper) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm);

        static constexpr std::string_view n[] = { "hsm", "i1", "i2",
                                                  "r1",  "r2", "timeout" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0: {
                u64 id_in_file = 0;

                return read_u64(value, id_in_file) &&
                       ((std::cmp_greater(id_in_file, 0) &&
                         sim_hsms_mapping_get(id_in_file, wrapper.id)) ||
                        (copy<hsm_id>(0, wrapper.id) &&
                         warning("hsm_wrapper does not reference a valid hsm",
                                 log_level::error)));
            }
            case 1:
                return read_temp_integer(value) && copy_to(wrapper.exec.i1);
            case 2:
                return read_temp_integer(value) && copy_to(wrapper.exec.i2);
            case 3:
                return read_temp_real(value) && copy_to(wrapper.exec.r1);
            case 4:
                return read_temp_real(value) && copy_to(wrapper.exec.r2);
            case 5:
                return read_temp_real(value) && copy_to(wrapper.exec.timer);
            default:
                report_json_error(error_id::unknown_element);
            }
        });
    }

    bool read_modeling_dynamics(const rapidjson::Value& val,
                                hsm_wrapper&            wrapper) noexcept
    {
        auto_stack a(this, stack_id::dynamics_hsm);

        static constexpr std::string_view n[] = { "hsm", "i1", "i2",
                                                  "r1",  "r2", "timeout" };

        return for_members(val, n, [&](auto idx, const auto& value) noexcept {
            switch (idx) {
            case 0: {
                component_id c;
                return read_child_simple_or_grid_component(value, c) &&
                       copy<component_id>(c, wrapper.compo_id);
            }
            case 1:
                return read_temp_integer(value) && copy_to(wrapper.exec.i1);
            case 2:
                return read_temp_integer(value) && copy_to(wrapper.exec.i2);
            case 3:
                return read_temp_real(value) && copy_to(wrapper.exec.r1);
            case 4:
                return read_temp_real(value) && copy_to(wrapper.exec.r2);
            case 5:
                return read_temp_real(value) && copy_to(wrapper.exec.timer);
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

    bool fix_child_name(generic_component& generic, child& c) noexcept
    {
        if (c.flags[child_flags::configurable] or
            c.flags[child_flags::observable]) {
            const auto id  = generic.children.get_id(c);
            const auto idx = get_index(id);

            if (generic.children_names[idx].empty())
                generic.children_names[idx] = generic.make_unique_name_id(id);
        }

        return true;
    }

    bool read_child(const rapidjson::Value& val,
                    generic_component&      generic,
                    child&                  c,
                    child_id                c_id) noexcept
    {
        auto_stack a(this, stack_id::child);

        std::optional<u64> id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("id"sv == name)
                         return read_temp_unsigned_integer(value) &&
                                copy_to(id);
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
               cache_model_mapping_add(*id, ordinal(c_id)) &&
               fix_child_name(generic, c);
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

    auto search_dir(std::string_view name) const noexcept -> dir_path*
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

    auto search_component(std::string_view name) const noexcept -> component*
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
            debug::ensure(mod().components.try_to_get(c_id) != nullptr);

            if (auto* c = mod().components.try_to_get(c_id); c) {
                if (c->state == component_status::unmodified)
                    return true;

                if (auto ret = mod().load_component(*c); ret)
                    return true;
            }
        }

        // @TODO Really necessary?
        // if (auto* c = search_component(file.sv()); c) {
        //     c_id = mod().components.get_id(*c);
        //     if (c->state == component_status::unmodified)
        //         return true;
        //     append_dependency(c_id);
        // }

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
        self.model_mapping.sort();

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
            std::cmp_less_equal(i, external_source_chunk_size))
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
                                    constant_source_id id) const noexcept
    {
        self.constant_mapping.data.emplace_back(id_in_file, ordinal(id));

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
                       std::string        name;

                       return for_each_member(
                                value,
                                [&](const auto  name,
                                    const auto& value) noexcept -> bool {
                                    if ("id"sv == name)
                                        return read_temp_unsigned_integer(
                                                 value) &&
                                               copy_to(id_in_file);

                                    if ("name"sv == name)
                                        return read_temp_string(value) &&
                                               copy_to(cst.name);

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
        self.text_file_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool search_file_from_dir_component(const component& compo,
                                        file_path_id&    out) noexcept
    {
        out = get_file_from_component(mod(), compo, temp_string);

        return is_defined(out);
    }

    bool read_text_file_sources(const rapidjson::Value& val,
                                component&              compo) noexcept
    {
        auto_stack s(this, stack_id::srcs_text_file_sources);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               text_file_sources_can_alloc(compo.srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& text = compo.srcs.text_file_sources.alloc();
                     auto  id   = compo.srcs.text_file_sources.get_id(text);
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

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(text.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             search_file_from_dir_component(
                                               compo, text.file_id);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_text_file_mapping_add(*id_in_file, id);
                 });
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

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(text.name);

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
                                       binary_file_source_id id) const noexcept
    {
        self.binary_file_mapping.data.emplace_back(id_in_file, ordinal(id));

        return true;
    }

    bool read_binary_file_sources(const rapidjson::Value& val,
                                  component&              compo) noexcept
    {
        auto_stack s(this, stack_id::srcs_binary_file_sources);

        i64 len = 0;

        return is_value_array(val) && copy_array_size(val, len) &&
               binary_file_sources_can_alloc(compo.srcs, len) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     auto& bin = compo.srcs.binary_file_sources.alloc();
                     auto  id  = compo.srcs.binary_file_sources.get_id(bin);
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

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(bin.name);

                                  if ("path"sv == name)
                                      return read_temp_string(value) &&
                                             search_file_from_dir_component(
                                               compo, bin.file_id);

                                  return true;
                              }) &&
                            optional_has_value(id_in_file) &&
                            cache_binary_file_mapping_add(*id_in_file, id);
                 });
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

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(text.name);

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
        self.random_mapping.data.emplace_back(id_in_file, ordinal(id));

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

                                  if ("name"sv == name)
                                      return read_temp_string(value) &&
                                             copy_to(r.name);

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
        if (auto* elem = self.model_mapping.get(temp_u64); elem) {
            dst = enum_cast<child_id>(*elem);
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool cache_model_mapping_to(std::optional<child_id>& dst) noexcept
    {
        if (auto* elem = self.model_mapping.get(temp_u64); elem) {
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
        return grid.row * grid.column == grid.children.ssize();
    }

    bool read_grid_input_connection(const rapidjson::Value& val,
                                    const component&        compo,
                                    grid_component&         grid) noexcept
    {
        auto_stack s(this, stack_id::component_grid);

        std::optional<int>         row, col;
        std::optional<std::string> id;
        std::optional<std::string> x;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("row"sv == name)
                      return read_temp_integer(value) and copy_to(row);

                  if ("col"sv == name)
                      return read_temp_integer(value) and copy_to(col);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_to(id);

                  if ("x"sv == name)
                      return read_temp_string(value) and copy_to(x);

                  return true;
              }) and
            optional_has_value(row) and optional_has_value(col) and
            optional_has_value(id) and optional_has_value(x))
            report_json_error(error_id::grid_component_size_error);

        if (const auto pos = grid.pos(*row, *col);
            0 <= pos and pos < grid.children.ssize()) {
            const auto c_compo_id = grid.children[pos];
            if (const auto* c = mod().components.try_to_get(c_compo_id); c) {
                const auto con_id = c->get_x(*id);
                const auto con_x  = compo.get_x(*x);

                if (is_defined(con_id) and is_defined(con_x)) {
                    if (auto ret =
                          grid.connect_input(con_x, *row, *col, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return false;
    }

    bool read_grid_output_connection(const rapidjson::Value& val,
                                     const component&        compo,
                                     grid_component&         grid) noexcept
    {
        auto_stack s(this, stack_id::component_grid);

        std::optional<int>         row, col;
        std::optional<std::string> id;
        std::optional<std::string> y;

        if (not for_each_member(
              val,
              [&](const auto name, const auto& value) noexcept -> bool {
                  if ("row"sv == name)
                      return read_temp_integer(value) and copy_to(row);

                  if ("col"sv == name)
                      return read_temp_integer(value) and copy_to(col);

                  if ("id"sv == name)
                      return read_temp_string(value) and copy_to(id);

                  if ("y"sv == name)
                      return read_temp_string(value) and copy_to(y);

                  return true;
              }) and
            optional_has_value(row) and optional_has_value(col) and
            optional_has_value(id) and optional_has_value(y))
            report_json_error(error_id::grid_component_size_error);

        if (const auto pos = grid.pos(*row, *col);
            0 <= pos and pos < grid.children.ssize()) {
            const auto c_compo_id = grid.children[pos];
            if (const auto* c = mod().components.try_to_get(c_compo_id); c) {
                const auto con_id = c->get_x(*id);
                const auto con_y  = compo.get_y(*y);

                if (is_defined(con_id) and is_defined(con_y)) {
                    if (auto ret =
                          grid.connect_output(con_y, *row, *col, con_id);
                        !!ret)
                        return true;
                }
            }
        }

        return false;
    }

    bool read_grid_connection(const rapidjson::Value& val,
                              const component&        compo,
                              grid_component&         grid) noexcept
    {
        auto_stack s(this, stack_id::component_grid);

        return for_first_member(
          val, "type", [&](const auto& value) noexcept -> bool {
              return read_temp_string(value) and
                     ((temp_string == "input" and
                       read_grid_input_connection(val, compo, grid)) or
                      (temp_string == "output" and
                       read_grid_output_connection(val, compo, grid)));
          });
    }

    bool read_grid_connections(const rapidjson::Value& val,
                               const component&        compo,
                               grid_component&         grid) noexcept
    {
        auto_stack s(this, stack_id::component_grid);

        return is_value_array(val) and
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return is_value_object(value) and
                            read_grid_connection(value, compo, grid);
                 });
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

                     if ("connections"sv == name)
                         return read_grid_connections(value, compo, grid);

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
                  return read_hsm_states(
                    value, hsm.machine.states, hsm.names, hsm.positions);

              if ("top"sv == name)
                  return read_temp_unsigned_integer(value) &&
                         copy_to(hsm.machine.top_state);

              if ("i1"sv == name)
                  return read_temp_integer(value) && copy_to(hsm.i1);

              if ("i2"sv == name)
                  return read_temp_integer(value) && copy_to(hsm.i2);

              if ("r1"sv == name)
                  return read_temp_real(value) && copy_to(hsm.r1);

              if ("r2"sv == name)
                  return read_temp_real(value) && copy_to(hsm.r2);

              if ("timeout"sv == name)
                  return read_temp_real(value) && copy_to(hsm.timeout);

              if ("constants"sv == name)
                  return value.IsArray() and
                           std::cmp_less_equal(
                             value.GetArray().Size(),
                             hierarchical_state_machine::max_constants),
                         for_each_array(
                           value,
                           [&](const auto i, const auto& val) noexcept -> bool {
                               return read_temp_real(val) &&
                                      copy_to(hsm.machine.constants[i]);
                           });

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

    bool read_port(
      const rapidjson::Value& val,
      id_data_array<port_id, default_allocator, port_str, position>&
        port) noexcept
    {
        port_str port_name;
        double   x = 0, y = 0;

        if (not for_each_member(
              val, [&](const auto name, const auto& value) noexcept -> bool {
                  if ("name"sv == name)
                      return read_temp_string(value) && copy_to(port_name);
                  if ("x"sv == name)
                      return read_temp_real(value) && copy_to(x);
                  if ("y"sv == name)
                      return read_temp_real(value) && copy_to(y);

                  return true;
              }))
            return false;

        if (not port.can_alloc(1))
            return false;

        port.alloc([&](auto /*id*/, auto& str, auto& position) noexcept {
            str        = port_name.sv();
            position.x = static_cast<float>(x);
            position.y = static_cast<float>(y);
        });

        return true;
    }

    bool read_ports(
      const rapidjson::Value& val,
      id_data_array<port_id, default_allocator, port_str, position>&
        port) noexcept
    {
        auto_stack s(this, stack_id::component_ports);

        return is_value_array(val) && port.can_alloc(val.GetArray().Size()) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     if (value.IsString()) {
                         if (read_temp_string(value)) {
                             port.alloc(
                               [&](auto /*id*/, auto& str, auto& pos) noexcept {
                                   str = temp_string;
                                   pos.reset();
                               });
                             return true;
                         }
                         return false;
                     } else if (value.IsObject()) {
                         return read_port(value, port);
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
                  return read_constant_sources(value, compo.srcs);
              if ("binary-file-sources"sv == name)
                  return read_binary_file_sources(value, compo);
              if ("text-file-sources"sv == name)
                  return read_text_file_sources(value, compo);
              if ("random-sources"sv == name)
                  return read_random_sources(value, compo.srcs);
              if ("x"sv == name)
                  return read_ports(value, compo.x);
              if ("y"sv == name)
                  return read_ports(value, compo.y);
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

    bool cache_model_mapping_add(u64 id_in_file, u64 id) const noexcept
    {
        self.model_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool sim_hsms_mapping_clear() noexcept
    {
        self.sim_hsms_mapping.data.clear();
        return true;
    }

    bool sim_hsms_mapping_add(const u64    id_in_file,
                              const hsm_id id) const noexcept
    {
        self.sim_hsms_mapping.data.emplace_back(id_in_file, id);
        return true;
    }

    bool sim_hsms_mapping_get(const u64 id_in_file, hsm_id& id) noexcept
    {
        if (auto* ptr = self.sim_hsms_mapping.get(id_in_file); ptr) {
            id = *ptr;
            return true;
        }

        report_json_error(error_id::cache_model_mapping_unfound);
    }

    bool sim_hsms_mapping_sort() noexcept
    {
        self.sim_hsms_mapping.sort();
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

        auto* mdl_src_id = self.model_mapping.get(src);
        if (!mdl_src_id)
            report_json_error(error_id::simulation_connect_src_unknown);

        auto* mdl_dst_id = self.model_mapping.get(dst);
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

        return is_value_array(val) && is_value_array_size_equal(val, 8) &&
               for_each_array(
                 val, [&](const auto i, const auto& value) noexcept -> bool {
                     return read_temp_real(value) && copy_to(reals[i]);
                 });
    }

    bool read_integer_parameter(const rapidjson::Value& val,
                                std::array<i64, 8>&     integers) noexcept
    {
        auto_stack s(this, stack_id::project_integer_parameter);

        return is_value_array(val) && is_value_array_size_equal(val, 8) &&
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

    bool global_parameter_init(const global_parameter_id id,
                               const tree_node_id        tn_id,
                               const model_id            mdl_id,
                               const parameter&          p) noexcept
    {
        pj().parameters.get<tree_node_id>(id) = tn_id;
        pj().parameters.get<model_id>(id)     = mdl_id;
        pj().parameters.get<parameter>(id)    = p;

        return true;
    }

    bool read_global_parameter(const rapidjson::Value&   val,
                               const global_parameter_id id) noexcept
    {
        auto_stack s(this, stack_id::project_global_parameter);

        std::optional<tree_node_id> tn_id_opt;
        std::optional<model_id>     mdl_id_opt;
        std::optional<parameter>    parameter_opt;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;

                     if ("name"sv == name)
                         return read_temp_string(value) &&
                                copy_to(pj().parameters.get<name_str>(id));

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, tn_id_opt, mdl_id_opt);

                     if ("parameter"sv == name) {
                         parameter p;
                         if (not read_parameter(value, p))
                             return false;

                         parameter_opt = p;
                         return true;
                     }

                     return true;
                 }) and
               optional_has_value(tn_id_opt) and
               optional_has_value(mdl_id_opt) and
               optional_has_value(parameter_opt) and
               global_parameter_init(
                 id, *tn_id_opt, *mdl_id_opt, *parameter_opt);
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
                     const auto id = pj().parameters.alloc();
                     return read_global_parameter(value, id);
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

    bool convert_to_tn_model_ids(const unique_id_path&        path,
                                 std::optional<tree_node_id>& parent_id,
                                 std::optional<model_id>&     mdl_id) noexcept
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

    bool convert_to_tn_id(const unique_id_path&        path,
                          std::optional<tree_node_id>& tn_id) noexcept
    {
        auto_stack s(this, stack_id::project_convert_to_tn_id);

        if (const auto id = pj().get_tn_id(path); is_defined(id)) {
            tn_id = id;
            return true;
        }

        report_json_error(error_id::project_fail_convert_access_to_tn_id);
    }

    template<typename T, int L>
    bool clear(small_vector<T, L>& out) noexcept
    {
        out.clear();

        return true;
    }

    template<typename T, int L>
    bool push_back_string(small_vector<T, L>& out) noexcept
    {
        if (out.ssize() + 1 < out.capacity()) {
            out.emplace_back(
              std::string_view{ temp_string.data(), temp_string.size() });
            return true;
        }
        return false;
    }

    bool read_project_unique_id_path(const rapidjson::Value& val,
                                     unique_id_path&         out) noexcept
    {
        auto_stack s(this, stack_id::project_unique_id_path);

        return is_value_array(val) &&
               is_value_array_size_less(val, out.capacity()) && clear(out) &&
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return read_temp_string(value) && push_back_string(out);
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

    bool plot_observation_init(variable_observer&                    plot,
                               const tree_node_id                    parent_id,
                               const model_id                        mdl_id,
                               const color                           c,
                               const variable_observer::type_options t,
                               const std::string_view name) noexcept
    {
        if (auto* tn = pj().tree_nodes.try_to_get(parent_id); tn) {
            plot.push_back(parent_id, mdl_id, c, t, name);
            return true;
        }

        return false;
    }

    bool read_project_plot_observation_child(const rapidjson::Value& val,
                                             variable_observer& plot) noexcept
    {
        auto_stack s(this, stack_id::project_plot_observation_child);

        name_str                    str;
        std::optional<tree_node_id> parent_id;
        std::optional<model_id>     mdl_id;

        auto c = color(0xff0ff);
        auto t = variable_observer::type_options::line;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;
                     if ("name"sv == name)
                         return read_temp_string(value) && copy_to(str);

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, parent_id, mdl_id);

                     if ("color"sv == name)
                         return read_color(value, c);

                     if ("type"sv == name)
                         return read_temp_string(value);

                     report_json_error(error_id::unknown_element);
                 }) &&
               optional_has_value(parent_id) and optional_has_value(mdl_id) and
               plot_observation_init(plot, *parent_id, *mdl_id, c, t, str.sv());
        return false;
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

        i64 size = 0;

        return is_value_array(val) and
               copy_array_size(val, size) and // @TODO check un can-alloc
               for_each_array(
                 val,
                 [&](const auto /*i*/, const auto& value) noexcept -> bool {
                     return is_value_object(value) and
                            read_project_plot_observation_child(value, plot);
                 });
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

    bool grid_observation_init(grid_observer& grid,
                               tree_node_id   parent_id,
                               tree_node_id   grid_tn_id,
                               model_id       mdl_id)
    {
        auto* parent     = pj().tree_nodes.try_to_get(parent_id);
        auto* mdl_parent = pj().tree_nodes.try_to_get(grid_tn_id);

        debug::ensure(parent and mdl_parent);

        if (parent and mdl_parent) {
            grid.parent_id = parent_id;
            grid.tn_id     = grid_tn_id;
            grid.mdl_id    = mdl_id;
            grid.compo_id  = mdl_parent->id;
            parent->grid_observer_ids.emplace_back(
              pj().grid_observers.get_id(grid));
            return true;
        }

        return false;
    }

    bool read_project_grid_observation(const rapidjson::Value& val,
                                       grid_observer&          grid) noexcept
    {
        auto_stack s(this, stack_id::project_grid_observation);

        std::optional<tree_node_id> parent_id;
        std::optional<tree_node_id> grid_tn_id;
        std::optional<model_id>     mdl_id;

        return for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     unique_id_path path;

                     if ("name"sv == name)
                         return read_temp_string(value) && copy_to(grid.name);

                     if ("grid"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_id(path, parent_id);

                     if ("access"sv == name)
                         return read_project_unique_id_path(value, path) &&
                                convert_to_tn_model_ids(
                                  path, grid_tn_id, mdl_id);

                     return true;
                 }) and
               optional_has_value(parent_id) and
               optional_has_value(grid_tn_id) and optional_has_value(mdl_id) and
               grid_observation_init(grid, *parent_id, *grid_tn_id, *mdl_id);
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

    bool project_time_limit_affect(const double b, const double e) noexcept
    {
        pj().t_limit.set_bound(b, e);

        return true;
    }

    bool read_project(const rapidjson::Value& val) noexcept
    {
        auto_stack s(this, stack_id::project);

        double begin = { 0 };
        double end   = { 100 };

        return read_project_top_component(val) &&
               for_each_member(
                 val,
                 [&](const auto name, const auto& value) noexcept -> bool {
                     if ("begin"sv == name)
                         return read_temp_real(value) and copy_to(begin);

                     if ("end"sv == name)
                         return read_temp_real(value) and copy_to(end);

                     if ("parameters"sv == name)
                         return read_project_parameters(value);

                     if ("observations"sv == name)
                         return read_project_observations(value);

                     return true;
                 }) and
               project_time_limit_affect(begin, end);
    }

    void clear() noexcept
    {
        temp_integer = 0;
        temp_u64     = 0;
        temp_double  = 0.0;
        temp_bool    = false;
        temp_string.clear();
        stack.clear();
        error = error_id::none;
    }

    modeling*   m_mod = nullptr;
    simulation* m_sim = nullptr;
    project*    m_pj  = nullptr;

    i64         temp_integer = 0;
    u64         temp_u64     = 0;
    double      temp_double  = 0.0;
    bool        temp_bool    = false;
    std::string temp_string;

    small_vector<stack_id, 16> stack;

    error_id error = error_id::none;

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

    bool return_true() const noexcept { return true; }
    bool return_false() const noexcept { return false; }

    bool warning(const std::string_view str,
                 const log_level        level) const noexcept
    {
        std::clog << log_level_names[ordinal(level)] << ' ' << str << '\n';

        return true;
    }

    bool parse_simulation(const rapidjson::Document& doc) noexcept
    {
        sim().clear();

        if (read_simulation(doc.GetObject()))
            return true;

        debug_logi(
          2, "read simulation fail with {}\n", error_id_names[ordinal(error)]);

        for (auto i = 0u; i < stack.size(); ++i)
            debug_logi(2,
                       "  {}: {}\n",
                       static_cast<int>(i),
                       stack_id_names[ordinal(stack[i])]);

        return report_error(cb, json_dearchiver::error_code::format_error);
    }

    bool parse_component(const rapidjson::Document& doc,
                         component&                 compo) noexcept
    {
        debug::ensure(compo.state != component_status::unmodified);

        if (read_component(doc.GetObject(), compo)) {
            compo.state = component_status::unmodified;
        } else {
            compo.state = component_status::unreadable;
            mod().clear(compo);

#ifdef IRRITATOR_ENABLE_DEBUG
            show_error();
#endif

            return report_error(cb,
                                json_dearchiver::error_code::dependency_error);
        }

        return true;
    }

    bool parse_project(const rapidjson::Document& doc) noexcept
    {
        pj().clear();
        sim().clear();

        if (read_project(doc.GetObject()))
            return true;

        debug_log("read project fail with {}\n",
                  error_id_names[ordinal(error)]);

        for (auto i = 0u; i < stack.size(); ++i)
            debug_logi(2,
                       "  {}: {}\n",
                       static_cast<int>(i),
                       stack_id_names[ordinal(stack[i])]);

        return report_error(cb, json_dearchiver::error_code::format_error);
    }
};

static bool read_file_to_buffer(vector<char>&             buffer,
                                file&                     f,
                                json_dearchiver::error_cb cb) noexcept
{
    debug::ensure(f.is_open());
    debug::ensure(f.get_mode() == open_mode::read);

    if (not f.is_open() or f.get_mode() != open_mode::read)
        return report_error(cb, json_dearchiver::error_code::arg_error);

    const auto len = f.length();
    if (std::cmp_less(len, 2))
        return report_error(
          cb, json_dearchiver::error_code::file_error, static_cast<sz>(len));

    buffer.resize(len);

    if (std::cmp_less(buffer.size(), len))
        return report_error(
          cb, json_dearchiver::error_code::memory_error, static_cast<sz>(len));

    if (not f.read(buffer.data(), len))
        return report_error(
          cb, json_dearchiver::error_code::read_error, static_cast<int>(errno));

    return true;
}

static bool parse_json_data(std::span<char>           buffer,
                            rapidjson::Document&      doc,
                            json_dearchiver::error_cb cb) noexcept
{
    doc.Parse<rapidjson::kParseNanAndInfFlag>(buffer.data(), buffer.size());

    if (doc.HasParseError())
        return report_error(
          cb,
          json_dearchiver::error_code::format_error,
          std::make_pair(doc.GetErrorOffset(),
                         GetParseError_En(doc.GetParseError())));

    return true;
}

//
//

struct json_archiver::impl {
    json_archiver&          self;
    json_archiver::error_cb cb;

    impl(json_archiver& self_, json_archiver::error_cb cb_) noexcept
      : self{ self_ }
      , cb{ cb_ }
    {}

    template<typename Writer, int QssLevel>
    void write(Writer&                              writer,
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
    void write(Writer& writer,
               const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss1_sum_2& dyn) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(dyn.default_values[0]);
        writer.Key("value-1");
        writer.Double(dyn.default_values[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss1_sum_3& dyn) noexcept
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
    void write(Writer& writer, const qss1_sum_4& dyn) noexcept
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
    void write(Writer& writer, const qss1_wsum_2& dyn) noexcept
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
    void write(Writer& writer, const qss1_wsum_3& dyn) noexcept
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
    void write(Writer& writer, const qss1_wsum_4& dyn) noexcept
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
    void write(Writer& writer, const qss2_sum_2& dyn) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(dyn.default_values[0]);
        writer.Key("value-1");
        writer.Double(dyn.default_values[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss2_sum_3& dyn) noexcept
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
    void write(Writer& writer, const qss2_sum_4& dyn) noexcept
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
    void write(Writer& writer, const qss2_wsum_2& dyn) noexcept
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
    void write(Writer& writer, const qss2_wsum_3& dyn) noexcept
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
    void write(Writer& writer, const qss2_wsum_4& dyn) noexcept
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
    void write(Writer& writer, const qss3_sum_2& dyn) noexcept
    {
        writer.StartObject();

        writer.Key("value-0");
        writer.Double(dyn.default_values[0]);
        writer.Key("value-1");
        writer.Double(dyn.default_values[1]);

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss3_sum_3& dyn) noexcept
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
    void write(Writer& writer, const qss3_sum_4& dyn) noexcept
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
    void write(Writer& writer, const qss3_wsum_2& dyn) noexcept
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
    void write(Writer& writer, const qss3_wsum_3& dyn) noexcept
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
    void write(Writer& writer, const qss3_wsum_4& dyn) noexcept
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
    void write(Writer& writer, const counter& /*dyn*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const queue& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("ta");
        writer.Double(dyn.default_ta);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const dynamic_queue& dyn) noexcept
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
    void write(Writer& writer, const priority_queue& dyn) noexcept
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
    void write(Writer& writer, const generator& dyn) noexcept
    {
        writer.StartObject();

        writer.Key("stop-on-error");
        writer.Bool(dyn.flags[generator::option::stop_on_error]);

        if (dyn.flags[generator::option::ta_use_source]) {
            writer.Key("offset");
            writer.Double(dyn.default_offset);
            writer.Key("source-ta-type");
            writer.Int(ordinal(dyn.default_source_ta.type));
            writer.Key("source-ta-id");
            writer.Uint64(dyn.default_source_ta.id);
        }

        if (dyn.flags[generator::option::value_use_source]) {
            writer.Key("source-value-type");
            writer.Int(ordinal(dyn.default_source_value.type));
            writer.Key("source-value-id");
            writer.Uint64(dyn.default_source_value.id);
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const constant& dyn) noexcept
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
    void write(Writer& writer, const qss1_cross& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(dyn.default_threshold);
        writer.Key("detect-up");
        writer.Bool(dyn.default_detect_up);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss2_cross& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(dyn.default_threshold);
        writer.Key("detect-up");
        writer.Bool(dyn.default_detect_up);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss3_cross& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("threshold");
        writer.Double(dyn.default_threshold);
        writer.Key("detect-up");
        writer.Bool(dyn.default_detect_up);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss1_filter& dyn) noexcept
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
    void write(Writer& writer, const qss2_filter& dyn) noexcept
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
    void write(Writer& writer, const qss3_filter& dyn) noexcept
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
    void write(Writer& writer, const qss1_power& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(dyn.default_n);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss2_power& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(dyn.default_n);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const qss3_power& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("n");
        writer.Double(dyn.default_n);
        writer.EndObject();
    }

    template<typename Writer, int QssLevel>
    void write(Writer& writer,
               const abstract_square<QssLevel>& /*dyn*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer, int PortNumber>
    void write(Writer& writer, const accumulator<PortNumber>& /*dyn*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const time_func& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("function");
        writer.String(dyn.default_f == &time_function          ? "time"
                      : dyn.default_f == &square_time_function ? "square"
                                                               : "sin");
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const logical_and_2& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(dyn.default_values[0]);
        writer.Key("value-1");
        writer.Bool(dyn.default_values[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const logical_and_3& dyn) noexcept
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
    void write(Writer& writer, const logical_or_2& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("value-0");
        writer.Bool(dyn.default_values[0]);
        writer.Key("value-1");
        writer.Bool(dyn.default_values[1]);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer& writer, const logical_or_3& dyn) noexcept
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
    void write(Writer& writer, const logical_invert& /*dyn*/) noexcept
    {
        writer.StartObject();
        writer.EndObject();
    }

    template<typename Writer>
    void write_simulation_dynamics(Writer&            writer,
                                   const hsm_wrapper& dyn) noexcept
    {
        writer.StartObject();
        writer.Key("hsm");
        writer.Uint64(get_index(dyn.id));
        writer.Key("i1");
        writer.Int(dyn.exec.i1);
        writer.Key("i2");
        writer.Int(dyn.exec.i2);
        writer.Key("r1");
        writer.Double(dyn.exec.r1);
        writer.Key("r2");
        writer.Double(dyn.exec.r2);
        writer.Key("timeout");
        writer.Double(dyn.exec.timer);
        writer.EndObject();
    }

    template<typename Writer>
    void write_modeling_dynamics(const modeling&    mod,
                                 Writer&            writer,
                                 const hsm_wrapper& dyn) noexcept
    {
        writer.StartObject();

        writer.Key("hsm");
        writer.StartObject();
        if_data_exists_do(
          mod.components,
          enum_cast<component_id>(dyn.compo_id),
          [&](auto& compo) { write_child_component_path(mod, compo, writer); });
        writer.EndObject();

        writer.Key("i1");
        writer.Int(dyn.exec.i1);
        writer.Key("i2");
        writer.Int(dyn.exec.i2);
        writer.Key("r1");
        writer.Double(dyn.exec.r1);
        writer.Key("r2");
        writer.Double(dyn.exec.r2);
        writer.Key("timeout");
        writer.Double(dyn.exec.timer);
        writer.EndObject();
    }

    template<typename Writer>
    void write(Writer&                                         writer,
               std::string_view                                name,
               const hierarchical_state_machine::state_action& state) noexcept
    {
        writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        writer.StartObject();
        writer.Key("var-1");
        writer.Int(static_cast<int>(state.var1));
        writer.Key("var-2");
        writer.Int(static_cast<int>(state.var2));
        writer.Key("type");
        writer.Int(static_cast<int>(state.type));

        if (state.var2 == hierarchical_state_machine::variable::constant_i) {
            writer.Key("constant-i");
            writer.Int(state.constant.i);
        } else if (state.var2 ==
                   hierarchical_state_machine::variable::constant_r) {
            writer.Key("constant-r");
            writer.Double(state.constant.f);
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write(
      Writer&                                             writer,
      std::string_view                                    name,
      const hierarchical_state_machine::condition_action& state) noexcept
    {
        writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
        writer.StartObject();
        writer.Key("var-1");
        writer.Int(static_cast<int>(state.var1));
        writer.Key("var-2");
        writer.Int(static_cast<int>(state.var2));
        writer.Key("type");
        writer.Int(static_cast<int>(state.type));

        if (state.type == hierarchical_state_machine::condition_type::port) {
            u8 port{}, mask{};
            state.get(port, mask);

            writer.Key("port");
            writer.Int(static_cast<int>(port));
            writer.Key("mask");
            writer.Int(static_cast<int>(mask));
        }

        if (state.var2 == hierarchical_state_machine::variable::constant_i) {
            writer.Key("constant-i");
            writer.Int(state.constant.i);
        } else if (state.var2 ==
                   hierarchical_state_machine::variable::constant_r) {
            writer.Key("constant-r");
            writer.Double(state.constant.f);
        }

        writer.EndObject();
    }

    template<typename Writer>
    void write_constant_sources(const external_source& srcs, Writer& w) noexcept
    {
        w.Key("constant-sources");
        w.StartArray();

        const constant_source* src = nullptr;
        while (srcs.constant_sources.next(src)) {
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.constant_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("parameters");

            w.StartArray();
            for (auto e = src->length, i = 0u; i != e; ++i)
                w.Double(src->buffer[i]);
            w.EndArray();

            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_binary_file_sources(
      const external_source&                     srcs,
      const data_array<file_path, file_path_id>& files,
      Writer&                                    w) noexcept
    {
        w.Key("binary-file-sources");
        w.StartArray();

        const binary_file_source* src = nullptr;
        while (srcs.binary_file_sources.next(src)) {
            if (const auto* f = files.try_to_get(src->file_id); f) {
                w.StartObject();
                w.Key("id");
                w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
                w.Key("name");
                w.String(src->name.c_str());
                w.Key("max-clients");
                w.Uint(src->max_clients);
                w.Key("path");
                w.String(f->path.data(),
                         static_cast<rapidjson::SizeType>(f->path.size()));
                w.EndObject();
            }
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_text_file_sources(
      const external_source&                     srcs,
      const data_array<file_path, file_path_id>& files,
      Writer&                                    w) noexcept
    {
        w.Key("text-file-sources");
        w.StartArray();

        const text_file_source* src = nullptr;
        std::string             filepath;

        while (srcs.text_file_sources.next(src)) {
            if (const auto* f = files.try_to_get(src->file_id); f) {
                w.StartObject();
                w.Key("id");
                w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
                w.Key("name");
                w.String(src->name.c_str());
                w.Key("path");
                w.String(f->path.data(),
                         static_cast<rapidjson::SizeType>(f->path.size()));
                w.EndObject();
            }
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_binary_file_sources(const external_source& srcs,
                                   Writer&                w) noexcept
    {
        w.Key("binary-file-sources");
        w.StartArray();

        const binary_file_source* src = nullptr;
        while (srcs.binary_file_sources.next(src)) {
            const auto str = src->file_path.string();

            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("max-clients");
            w.Uint(src->max_clients);
            w.Key("path");
            w.String(str.data(), static_cast<rapidjson::SizeType>(str.size()));
            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_text_file_sources(const external_source& srcs,
                                 Writer&                w) noexcept
    {
        w.Key("text-file-sources");
        w.StartArray();

        const text_file_source* src = nullptr;
        while (srcs.text_file_sources.next(src)) {
            const auto str = src->file_path.string();
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
            w.Key("path");
            w.String(str.data(), static_cast<rapidjson::SizeType>(str.size()));
            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_random_sources(const external_source& srcs, Writer& w) noexcept
    {
        w.Key("random-sources");
        w.StartArray();

        const random_source* src = nullptr;
        while (srcs.random_sources.next(src)) {
            w.StartObject();
            w.Key("id");
            w.Uint64(ordinal(srcs.random_sources.get_id(*src)));
            w.Key("name");
            w.String(src->name.c_str());
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
    void write_child_component_path(Writer&               w,
                                    const registred_path& reg,
                                    const dir_path&       dir,
                                    const file_path&      file) noexcept
    {
        w.Key("path");
        w.String(reg.name.begin(), reg.name.size());

        w.Key("directory");
        w.String(dir.path.begin(), dir.path.size());

        w.Key("file");
        w.String(file.path.begin(), file.path.size());
    }

    template<typename Writer>
    void write_child_component_path(const modeling&  mod,
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
    void write_child_component(const modeling&    mod,
                               const component_id compo_id,
                               Writer&            w) noexcept
    {
        if (auto* compo = mod.components.try_to_get(compo_id); compo) {
            w.Key("component-type");
            w.String(component_type_names[ordinal(compo->type)]);

            switch (compo->type) {
            case component_type::none:
                break;
            case component_type::internal:
                w.Key("parameter");
                w.String(
                  internal_component_names[ordinal(compo->id.internal_id)]);
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
    void write_child_model(const modeling& mod, model& mdl, Writer& w) noexcept
    {
        w.Key("dynamics");

        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                write_modeling_dynamics(mod, w, dyn);
            } else {
                write(w, dyn);
            }
        });
    }

    template<typename Writer>
    void write_child(const modeling&          mod,
                     const generic_component& gen,
                     const child&             ch,
                     Writer&                  w) noexcept
    {
        const auto child_id  = gen.children.get_id(ch);
        const auto child_idx = get_index(child_id);

        w.StartObject();
        w.Key("id");
        w.Uint64(get_index(child_id));

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

            write_child_model(mod, mdl, w);
        }

        w.EndObject();
    }

    template<typename Writer>
    void write_generic_component_children(const modeling&          mod,
                                          const generic_component& simple_compo,
                                          Writer&                  w) noexcept
    {
        w.Key("children");
        w.StartArray();

        for (const auto& c : simple_compo.children)
            write_child(mod, simple_compo, c, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_component_ports(const component& compo, Writer& w) noexcept
    {
        if (not compo.x.empty()) {
            w.Key("x");
            w.StartArray();

            compo.x.for_each(
              [&](auto /*id*/, const auto& str, auto& pos) noexcept {
                  w.StartObject();
                  w.Key("name");
                  w.String(str.c_str());
                  w.Key("x");
                  w.Double(pos.x);
                  w.Key("y");
                  w.Double(pos.y);
                  w.EndObject();
              });

            w.EndArray();
        }

        if (not compo.y.empty()) {
            w.Key("y");
            w.StartArray();

            compo.y.for_each(
              [&](auto /*id*/, const auto& str, auto& pos) noexcept {
                  w.StartObject();
                  w.Key("name");
                  w.String(str.c_str());
                  w.Key("x");
                  w.Double(pos.x);
                  w.Key("y");
                  w.Double(pos.y);
                  w.EndObject();
              });

            w.EndArray();
        }
    }

    template<typename Writer>
    void write_input_connection(const modeling&          mod,
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
        w.String(parent.x.get<port_str>(x).c_str());
        w.Key("destination");
        w.Uint64(get_index(gen.children.get_id(dst)));
        w.Key("port-destination");

        if (dst.type == child_type::model) {
            w.Int(dst_x.model);
        } else {
            const auto* compo_child =
              mod.components.try_to_get(dst.id.compo_id);
            if (compo_child and compo_child->x.exists(dst_x.compo))
                w.String(compo_child->x.get<port_str>(dst_x.compo).c_str());
        }

        w.EndObject();
    }

    template<typename Writer>
    void write_output_connection(const modeling&          mod,
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
        w.String(parent.y.get<port_str>(y).c_str());
        w.Key("source");
        w.Uint64(get_index(gen.children.get_id(src)));
        w.Key("port-source");

        if (src.type == child_type::model) {
            w.Int(src_y.model);
        } else {
            const auto* compo_child =
              mod.components.try_to_get(src.id.compo_id);
            if (compo_child and compo_child->y.exists(src_y.compo)) {
                w.String(compo_child->y.get<port_str>(src_y.compo).c_str());
            }
        }
        w.EndObject();
    }

    template<typename Writer>
    void write_internal_connection(const modeling&          mod,
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
                src_str = compo->y.get<port_str>(src_y.compo).c_str();
        } else
            src_int = src_y.model;

        if (dst.type == child_type::component) {
            auto* compo = mod.components.try_to_get(dst.id.compo_id);
            if (compo and compo->x.exists(dst_x.compo))
                dst_str = compo->x.get<port_str>(dst_x.compo).c_str();
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
    void write_generic_component_connections(const modeling&          mod,
                                             const component&         compo,
                                             const generic_component& gen,
                                             Writer& w) noexcept
    {
        w.Key("connections");
        w.StartArray();

        for (const auto& con : gen.connections)
            if (auto* c_src = gen.children.try_to_get(con.src); c_src)
                if (auto* c_dst = gen.children.try_to_get(con.dst); c_dst)
                    write_internal_connection(mod,
                                              gen,
                                              *c_src,
                                              con.index_src,
                                              *c_dst,
                                              con.index_dst,
                                              w);

        for (const auto& con : gen.input_connections)
            if (auto* c = gen.children.try_to_get(con.dst); c)
                if (compo.x.exists(con.x))
                    write_input_connection(
                      mod, compo, gen, con.x, *c, con.port, w);

        for (const auto& con : gen.output_connections)
            if (auto* c = gen.children.try_to_get(con.src); c)
                if (compo.y.exists(con.y))
                    write_output_connection(
                      mod, compo, gen, con.y, *c, con.port, w);

        w.EndArray();
    }

    template<typename Writer>
    void write_generic_component(const modeling&          mod,
                                 const component&         compo,
                                 const generic_component& s_compo,
                                 Writer&                  w) noexcept
    {
        write_generic_component_children(mod, s_compo, w);
        write_generic_component_connections(mod, compo, s_compo, w);
    }

    template<typename Writer>
    void write_grid_component(const modeling&       mod,
                              const component&      compo,
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

        w.Key("connections");
        w.StartArray();
        for (const auto& con : grid.input_connections) {
            const auto pos            = grid.pos(con.row, con.col);
            const auto child_compo_id = grid.children[pos];

            if (const auto* c = mod.components.try_to_get(child_compo_id); c) {
                if (c->x.exists(con.id)) {
                    w.StartObject();
                    w.Key("type");
                    w.String("input");
                    w.Key("row");
                    w.Int(con.row);
                    w.Key("col");
                    w.Int(con.col);
                    w.Key("id");
                    w.String(c->x.get<port_str>(con.id).c_str());
                    w.Key("x");
                    w.String(compo.x.get<port_str>(con.x).c_str());
                    w.EndObject();
                }
            }
        }

        for (const auto& con : grid.output_connections) {
            const auto pos            = grid.pos(con.row, con.col);
            const auto child_compo_id = grid.children[pos];

            if (const auto* c = mod.components.try_to_get(child_compo_id); c) {
                if (c->x.exists(con.id)) {
                    w.StartObject();
                    w.Key("type");
                    w.String("output");
                    w.Key("row");
                    w.Int(con.row);
                    w.Key("col");
                    w.Int(con.col);
                    w.Key("id");
                    w.String(c->y.get<port_str>(con.id).c_str());
                    w.Key("y");
                    w.String(compo.x.get<port_str>(con.y).c_str());
                    w.EndObject();
                }
            }
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_graph_component_param(const modeling&        mod,
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
    void write_graph_component(const modeling&        mod,
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
    void write_hierarchical_state_machine_state(
      const hierarchical_state_machine::state& state,
      const std::string_view&                  name,
      const position&                          pos,
      const std::integral auto                 index,
      Writer&                                  w) noexcept
    {
        w.StartObject();
        w.Key("id");
        w.Uint(static_cast<u32>(index));

        if (not name.empty()) {
            w.Key("name");
            w.String(name.data(),
                     static_cast<rapidjson::SizeType>(name.size()));
        }

        write(w, "enter", state.enter_action);
        write(w, "exit", state.exit_action);
        write(w, "if", state.if_action);
        write(w, "else", state.else_action);
        write(w, "condition", state.condition);

        w.Key("if-transition");
        w.Int(state.if_transition);
        w.Key("else-transition");
        w.Int(state.else_transition);
        w.Key("super-id");
        w.Int(state.super_id);
        w.Key("sub-id");
        w.Int(state.sub_id);

        w.Key("x");
        w.Double(pos.x);
        w.Key("y");
        w.Double(pos.y);

        w.EndObject();
    }

    template<typename Writer>
    void write_hierarchical_state_machine(const hierarchical_state_machine& hsm,
                                          std::span<const name_str> names,
                                          std::span<const position> positions,
                                          Writer&                   w) noexcept
    {
        w.Key("states");
        w.StartArray();

        constexpr auto length = hierarchical_state_machine::max_number_of_state;
        constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

        std::array<bool, length> states_to_write{};
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

        if (not positions.empty()) {
            for (auto i = 0; i != length; ++i) {
                if (states_to_write[i]) {
                    write_hierarchical_state_machine_state(
                      hsm.states[i], names[i].sv(), positions[i], i, w);
                }
            }
        }
        w.EndArray();

        w.Key("top");
        w.Uint(hsm.top_state);
    }

    template<typename Writer>
    void write_hsm_component(const hsm_component& hsm, Writer& w) noexcept
    {
        write_hierarchical_state_machine(
          hsm.machine,
          std::span{ hsm.names.begin(), hsm.names.size() },
          std::span{ hsm.positions.begin(), hsm.positions.size() },
          w);

        w.Key("i1");
        w.Int(hsm.i1);
        w.Key("i2");
        w.Int(hsm.i2);
        w.Key("r1");
        w.Double(hsm.r1);
        w.Key("r2");
        w.Double(hsm.r2);
        w.Key("timeout");
        w.Double(hsm.timeout);

        w.Key("constants");
        w.StartArray();
        for (const auto& c : hsm.machine.constants)
            w.Double(c);
        w.EndArray();
    }

    template<typename Writer>
    void write_internal_component(const modeling& /* mod */,
                                  const internal_component id,
                                  Writer&                  w) noexcept
    {
        w.Key("component");
        w.String(internal_component_names[ordinal(id)]);
    }

    template<typename Writer>
    void do_component_save(Writer& w, modeling& mod, component& compo) noexcept
    {
        w.StartObject();

        w.Key("name");
        w.String(compo.name.c_str());

        write_constant_sources(compo.srcs, w);
        write_binary_file_sources(compo.srcs, mod.file_paths, w);
        write_text_file_sources(compo.srcs, mod.file_paths, w);
        write_random_sources(compo.srcs, w);

        w.Key("colors");
        w.StartArray();
        auto& color =
          mod.component_colors[get_index(mod.components.get_id(compo))];
        w.Double(color[0]);
        w.Double(color[1]);
        w.Double(color[2]);
        w.Double(color[3]);
        w.EndArray();

        write_component_ports(compo, w);

        w.Key("type");
        w.String(component_type_names[ordinal(compo.type)]);

        switch (compo.type) {
        case component_type::none:
            break;

        case component_type::internal:
            write_internal_component(mod, compo.id.internal_id, w);
            break;

        case component_type::simple: {
            auto* p = mod.generic_components.try_to_get(compo.id.generic_id);
            if (p)
                write_generic_component(mod, compo, *p, w);
        } break;

        case component_type::grid: {
            auto* p = mod.grid_components.try_to_get(compo.id.grid_id);
            if (p)
                write_grid_component(mod, compo, *p, w);
        } break;

        case component_type::graph: {
            auto* p = mod.graph_components.try_to_get(compo.id.graph_id);
            if (p)
                write_graph_component(mod, *p, w);
        } break;

        case component_type::hsm: {
            auto* p = mod.hsm_components.try_to_get(compo.id.hsm_id);
            if (p)
                write_hsm_component(*p, w);
        } break;
        }

        w.EndObject();
    }

    /*****************************************************************************
     *
     * Simulation file read part
     *
     ****************************************************************************/

    template<typename Writer>
    void write_simulation_model(const simulation& sim, Writer& w) noexcept
    {
        w.Key("hsms");
        w.StartArray();

        for (auto& machine : sim.hsms) {
            w.StartObject();
            w.Key("hsm");
            w.Uint64(ordinal(sim.hsms.get_id(machine)));
            write_hierarchical_state_machine(
              machine, std::span<name_str>(), std::span<position>(), w);
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

            dispatch(mdl, [&]<typename Dynamics>(const Dynamics& dyn) noexcept {
                if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
                    write_simulation_dynamics(w, dyn);
                else
                    write(w, dyn);
            });

            w.EndObject();
        }

        w.EndArray();
    }

    template<typename Writer>
    void write_simulation_connections(const simulation& sim, Writer& w) noexcept
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
    void do_simulation_save(Writer& w, const simulation& sim) noexcept
    {
        w.StartObject();

        write_constant_sources(sim.srcs, w);
        write_binary_file_sources(sim.srcs, w);
        write_text_file_sources(sim.srcs, w);
        write_random_sources(sim.srcs, w);
        write_simulation_model(sim, w);
        write_simulation_connections(sim, w);

        w.EndObject();
    }

    /*****************************************************************************
     *
     * project file write part
     *
     ****************************************************************************/

    template<typename Writer>
    void write_color(Writer& w, std::array<u8, 4> color) noexcept
    {
        w.StartArray();
        w.Uint(color[0]);
        w.Uint(color[1]);
        w.Uint(color[2]);
        w.Uint(color[3]);
        w.EndArray();
    }

    template<typename Writer>
    void write_project_unique_id_path(Writer&               w,
                                      const unique_id_path& path) noexcept
    {
        w.StartArray();
        for (const auto& elem : path)
            w.String(elem.data(), elem.size());
        w.EndArray();
    }

    constexpr static std::array<u8, 4> color_white{ 255, 255, 255, 0 };

    template<typename Writer>
    void do_project_save_parameters(Writer& w, project& pj) noexcept
    {
        w.Key("parameters");

        w.StartObject();
        do_project_save_global_parameters(w, pj);
        w.EndObject();
    }

    template<typename Writer>
    void do_project_save_plot_observations(Writer& w, project& pj) noexcept
    {
        w.Key("global");
        w.StartArray();

        for_each_data(pj.variable_observers, [&](auto& plot) noexcept {
            unique_id_path path;

            w.StartObject();
            w.Key("name");
            w.String(plot.name.begin(), plot.name.size());

            w.Key("models");
            w.StartArray();
            plot.for_each([&](const auto id) noexcept {
                w.StartObject();
                const auto idx = get_index(id);
                const auto tn  = plot.get_tn_ids()[idx];
                const auto mdl = plot.get_mdl_ids()[idx];
                const auto str = plot.get_names()[idx];

                if (not str.empty()) {
                    w.Key("name");
                    w.String(str.data(), static_cast<int>(str.size()));
                }

                w.Key("access");
                pj.build_unique_id_path(tn, mdl, path);
                write_project_unique_id_path(w, path);

                w.Key("color");
                write_color(w, color_white);

                w.Key("type");
                w.String("line");
                w.EndObject();
            });
            w.EndArray();

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_grid_observations(Writer& w, project& pj) noexcept
    {
        w.Key("grid");
        w.StartArray();

        for_each_data(pj.grid_observers, [&](auto& grid) noexcept {
            w.StartObject();
            w.Key("name");
            w.String(grid.name.begin(), grid.name.size());

            unique_id_path path;
            w.Key("grid");
            pj.build_unique_id_path(grid.parent_id, path);
            write_project_unique_id_path(w, path);

            w.Key("access");
            pj.build_unique_id_path(grid.tn_id, grid.mdl_id, path);
            write_project_unique_id_path(w, path);

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_observations(Writer& w, project& pj) noexcept
    {
        w.Key("observations");

        w.StartObject();
        do_project_save_plot_observations(w, pj);
        do_project_save_grid_observations(w, pj);
        w.EndObject();
    }

    template<typename Writer>
    void write_parameter(Writer& w, const parameter& param) noexcept
    {
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
        w.EndObject();
    }

    template<typename Writer>
    void do_project_save_global_parameters(Writer& w, project& pj) noexcept
    {
        w.Key("global");
        w.StartArray();

        pj.parameters.for_each([&](const auto /*id*/,
                                   const auto& name,
                                   const auto  tn_id,
                                   const auto  mdl_id,
                                   const auto& p) noexcept {
            w.StartObject();
            w.Key("name");
            w.String(name.begin(), name.size());

            unique_id_path path;
            w.Key("access");
            pj.build_unique_id_path(tn_id, mdl_id, path);
            write_project_unique_id_path(w, path);

            w.Key("parameter");
            write_parameter(w, p);

            w.EndObject();
        });

        w.EndArray();
    }

    template<typename Writer>
    void do_project_save_component(Writer&               w,
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
    }

    template<typename Writer>
    bool do_project_save(Writer&    w,
                         project&   pj,
                         modeling&  mod,
                         component& compo) noexcept
    {
        auto* reg = mod.registred_paths.try_to_get(compo.reg_path);
        if (!reg)
            return report_error(cb, json_archiver::error_code::file_error);
        if (reg->path.empty())
            return report_error(cb, json_archiver::error_code::file_error);
        if (reg->name.empty())
            return report_error(cb, json_archiver::error_code::file_error);

        auto* dir = mod.dir_paths.try_to_get(compo.dir);
        if (!dir)
            return report_error(cb, json_archiver::error_code::file_error);
        if (dir->path.empty())
            return report_error(cb, json_archiver::error_code::file_error);

        auto* file = mod.file_paths.try_to_get(compo.file);
        if (!file)
            return report_error(cb, json_archiver::error_code::file_error);
        if (file->path.empty())
            return report_error(cb, json_archiver::error_code::file_error);

        w.StartObject();

        w.Key("begin");
        w.Double(pj.t_limit.begin());
        w.Key("end");
        w.Double(pj.t_limit.end());

        do_project_save_component(w, compo, *reg, *dir, *file);
        do_project_save_parameters(w, pj);
        do_project_save_observations(w, pj);
        w.EndObject();

        return true;
    }
};

void json_dearchiver::destroy() noexcept
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

void json_dearchiver::clear() noexcept
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

bool irt::json_dearchiver::set_buffer(const u32 buffer_size) noexcept
{
    if (std::cmp_less(buffer.capacity(), buffer_size)) {
        buffer.resize(buffer_size);

        if (std::cmp_less(buffer.capacity(), buffer_size))
            return false;
    }

    return true;
}

bool json_dearchiver::operator()(simulation& sim,
                                 file&       io,
                                 error_cb    cb) noexcept
{
    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::read);
    clear();

    rapidjson::Document doc;

    if (not(read_file_to_buffer(buffer, io, cb) and
            parse_json_data(std::span(buffer.data(), buffer.size()), doc, cb)))
        return false;

    json_dearchiver::impl i(*this, sim, cb);

    return i.parse_simulation(doc);
}

bool json_dearchiver::operator()(modeling&  mod,
                                 component& compo,
                                 file&      io,
                                 error_cb   err) noexcept
{
    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::read);
    clear();

    rapidjson::Document doc;

    if (not(read_file_to_buffer(buffer, io, err) and
            parse_json_data(std::span(buffer.data(), buffer.size()), doc, err)))
        return false;

    json_dearchiver::impl i(*this, mod, err);
    return i.parse_component(doc, compo);
}

bool json_dearchiver::operator()(project&    pj,
                                 modeling&   mod,
                                 simulation& sim,
                                 file&       io,
                                 error_cb    err) noexcept
{
    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::read);
    clear();

    rapidjson::Document doc;

    if (not(read_file_to_buffer(buffer, io, err) and
            parse_json_data(std::span(buffer.data(), buffer.size()), doc, err)))
        return false;

    json_dearchiver::impl i(*this, mod, sim, pj, err);
    return i.parse_project(doc);
}

bool json_dearchiver::operator()(simulation&     sim,
                                 std::span<char> io,
                                 error_cb        err) noexcept
{
    clear();
    rapidjson::Document doc;

    if (not parse_json_data(io, doc, err))
        return false;

    json_dearchiver::impl i(*this, sim, err);
    return i.parse_simulation(doc);
}

bool json_dearchiver::operator()(modeling&       mod,
                                 component&      compo,
                                 std::span<char> io,
                                 error_cb        err) noexcept
{
    clear();
    rapidjson::Document doc;

    if (not parse_json_data(io, doc, err))
        return false;

    json_dearchiver::impl i(*this, mod, err);
    return i.parse_component(doc, compo);
}

bool json_dearchiver::operator()(project&        pj,
                                 modeling&       mod,
                                 simulation&     sim,
                                 std::span<char> io,
                                 error_cb        err) noexcept
{
    clear();
    rapidjson::Document doc;

    if (not parse_json_data(io, doc, err))
        return false;

    json_dearchiver::impl i(*this, mod, sim, pj, err);
    return i.parse_project(doc);
}

void json_dearchiver::cerr(
  json_dearchiver::error_code ec,
  std::variant<std::monostate, sz, int, std::pair<sz, std::string_view>>
    v) noexcept
{
    switch (ec) {
    case json_dearchiver::error_code::memory_error:
        if (auto* ptr = std::get_if<sz>(&v); ptr) {
            fmt::print(std::cerr,
                       "json de-archiving memory error: not enough memory "
                       "(requested: {})\n",
                       *ptr);
        } else {
            fmt::print(std::cerr,
                       "json de-archiving memory error: not enough memory\n");
        }
        break;

    case json_dearchiver::error_code::arg_error:
        fmt::print(std::cerr, "json de-archiving internal error\n");
        break;

    case json_dearchiver::error_code::file_error:
        if (auto* ptr = std::get_if<sz>(&v); ptr) {
            fmt::print(std::cerr,
                       "json de-archiving file error: file too small {}\n",
                       *ptr);
        } else {
            fmt::print(std::cerr,
                       "json de-archiving memory error: not enough memory\n");
        }
        break;

    case json_dearchiver::error_code::read_error:
        if (auto* ptr = std::get_if<int>(&v); ptr and *ptr != 0) {
            fmt::print(
              std::cerr,
              "json de-archiving read error: internal system {} ({})\n",
              *ptr,
              std::strerror(*ptr));
        } else {
            fmt::print(std::cerr, "json de-archiving read error\n");
        }
        break;

    case json_dearchiver::error_code::format_error:
        if (auto* ptr = std::get_if<std::pair<sz, std::string_view>>(&v); ptr) {
            if (ptr->second.empty()) {
                fmt::print(std::cerr,
                           "json de-archiving json format error at offset {}\n",
                           ptr->first);
            } else {
                fmt::print(
                  std::cerr,
                  "json de-archiving json format error `{}' at offset {}\n",
                  ptr->second,
                  ptr->first);
            }
        } else {
            fmt::print(std::cerr, "json de-archiving json format error\n");
        }
        break;

    default:
        fmt::print(std::cerr, "json de-archiving unknown error\n");
    }
}

bool json_archiver::operator()(modeling&                   mod,
                               component&                  compo,
                               file&                       io,
                               json_archiver::print_option print,
                               error_cb                    err) noexcept
{
    clear();

    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::write);

    if (not(io.is_open() and io.get_mode() == open_mode::write))
        return report_error(err, json_archiver::error_code::arg_error);

    auto fp = reinterpret_cast<FILE*>(io.get_handle());
    buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, buffer.data(), buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream,
                            rapidjson::UTF8<>,
                            rapidjson::UTF8<>,
                            rapidjson::CrtAllocator,
                            rapidjson::kWriteNanAndInfFlag>
      w(os);

    json_archiver::impl i{ *this, err };

    switch (print) {
    case json_archiver::print_option::indent_2:
        w.SetIndent(' ', 2);
        i.do_component_save(w, mod, compo);
        break;

    case json_archiver::print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_component_save(w, mod, compo);
        break;

    default:
        i.do_component_save(w, mod, compo);
        break;
    }

    return true;
}

bool json_archiver::operator()(modeling&                   mod,
                               component&                  compo,
                               vector<char>&               out,
                               json_archiver::print_option print,
                               error_cb                    err) noexcept
{
    clear();

    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    json_archiver::impl i{ *this, err };

    switch (print) {
    case json_archiver::print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        i.do_component_save(w, mod, compo);
    } break;

    case json_archiver::print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_component_save(w, mod, compo);
    } break;

    default: {
        rapidjson::Writer<rapidjson::StringBuffer,
                          rapidjson::UTF8<>,
                          rapidjson::UTF8<>,
                          rapidjson::CrtAllocator,
                          rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        i.do_component_save(w, mod, compo);
    } break;
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return true;
}

bool json_archiver::operator()(const simulation&           sim,
                               file&                       io,
                               json_archiver::print_option print,
                               error_cb                    err) noexcept
{
    clear();

    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::write);

    if (not(io.is_open() and io.get_mode() == open_mode::write))
        return report_error(err, json_archiver::error_code::arg_error);

    auto fp = reinterpret_cast<FILE*>(io.get_handle());
    buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, buffer.data(), buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream,
                            rapidjson::UTF8<>,
                            rapidjson::UTF8<>,
                            rapidjson::CrtAllocator,
                            rapidjson::kWriteNanAndInfFlag>
                        w(os);
    json_archiver::impl i{ *this, err };

    switch (print) {
    case print_option::indent_2:
        w.SetIndent(' ', 2);
        i.do_simulation_save(w, sim);
        break;

    case print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_simulation_save(w, sim);
        break;

    default:
        i.do_simulation_save(w, sim);
        break;
    }

    return true;
}

bool json_archiver::operator()(const simulation& sim,
                               vector<char>&     out,
                               print_option      print_options,
                               error_cb          err) noexcept
{
    clear();

    rapidjson::StringBuffer buffer;
    buffer.Reserve(4096u);

    json_archiver::impl i{ *this, err };

    switch (print_options) {
    case print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        i.do_simulation_save(w, sim);
        break;
    }

    case print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_simulation_save(w, sim);
        break;
    }

    default: {
        rapidjson::Writer<rapidjson::StringBuffer,
                          rapidjson::UTF8<>,
                          rapidjson::UTF8<>,
                          rapidjson::CrtAllocator,
                          rapidjson::kWriteNanAndInfFlag>
          w(buffer);
        i.do_simulation_save(w, sim);
        break;
    }
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length));
    std::copy_n(str, length, out.data());

    return true;
}

bool json_archiver::operator()(project&  pj,
                               modeling& mod,
                               simulation& /* sim */,
                               file&        io,
                               print_option print_options,
                               error_cb     err) noexcept
{
    clear();

    debug::ensure(io.is_open());
    debug::ensure(io.get_mode() == open_mode::write);

    if (not(io.is_open() and io.get_mode() == open_mode::write))
        return report_error(err, json_archiver::error_code::arg_error);

    auto* compo  = mod.components.try_to_get(pj.head());
    auto* parent = pj.tn_head();

    if (not(compo and parent))
        return report_error(err, json_archiver::error_code::empty_project);

    debug::ensure(mod.components.get_id(compo) == parent->id);

    auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
    if (!reg)
        return report_error(err, json_archiver::error_code::file_error);

    auto* dir = mod.dir_paths.try_to_get(compo->dir);
    if (!dir)
        return report_error(err, json_archiver::error_code::file_error);

    auto* file = mod.file_paths.try_to_get(compo->file);
    if (!file)
        return report_error(err, json_archiver::error_code::file_error);

    auto fp = reinterpret_cast<FILE*>(io.get_handle());
    clear();
    buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, buffer.data(), buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream,
                            rapidjson::UTF8<>,
                            rapidjson::UTF8<>,
                            rapidjson::CrtAllocator,
                            rapidjson::kWriteNanAndInfFlag>
                        w(os);
    json_archiver::impl i{ *this, err };

    switch (print_options) {
    case print_option::indent_2:
        w.SetIndent(' ', 2);
        return i.do_project_save(w, pj, mod, *compo);

    case print_option::indent_2_one_line_array:
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        return i.do_project_save(w, pj, mod, *compo);

    default:
        return i.do_project_save(w, pj, mod, *compo);
    }
}

bool json_archiver::operator()(project&  pj,
                               modeling& mod,
                               simulation& /* sim */,
                               vector<char>& out,
                               print_option  print_options,
                               error_cb      err) noexcept
{
    clear();

    auto* compo  = mod.components.try_to_get(pj.head());
    auto* parent = pj.tn_head();

    if (!(compo && parent))
        return report_error(err, json_archiver::error_code::empty_project);

    debug::ensure(mod.components.get_id(compo) == parent->id);

    rapidjson::StringBuffer rbuffer;
    rbuffer.Reserve(4096u);
    json_archiver::impl i{ *this, err };

    switch (print_options) {
    case print_option::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        w.SetIndent(' ', 2);
        i.do_project_save(w, pj, mod, *compo);
    } break;

    case print_option::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer,
                                rapidjson::UTF8<>,
                                rapidjson::UTF8<>,
                                rapidjson::CrtAllocator,
                                rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        i.do_project_save(w, pj, mod, *compo);
    } break;

    default: {
        rapidjson::Writer<rapidjson::StringBuffer,
                          rapidjson::UTF8<>,
                          rapidjson::UTF8<>,
                          rapidjson::CrtAllocator,
                          rapidjson::kWriteNanAndInfFlag>
          w(rbuffer);
        i.do_project_save(w, pj, mod, *compo);
    } break;
    }

    auto length = rbuffer.GetSize();
    auto str    = rbuffer.GetString();
    out.resize(length + 1);
    std::copy_n(str, length, out.data());
    out.back() = '\0';

    return true;
}

void json_archiver::cerr(json_archiver::error_code             ec,
                         std::variant<std::monostate, sz, int> v) noexcept
{
    switch (ec) {
    case json_archiver::error_code::memory_error:
        if (auto* ptr = std::get_if<sz>(&v); ptr) {
            fmt::print(std::cerr,
                       "json archiving memory error: not enough memory "
                       "(requested: {})\n",
                       *ptr);
        } else {
            fmt::print(std::cerr,
                       "json archiving memory error: not enough memory\n");
        }
        break;

    case json_archiver::error_code::arg_error:
        fmt::print(std::cerr, "json archiving internal error\n");
        break;

    case json_archiver::error_code::empty_project:
        fmt::print(std::cerr, "json archiving empty project error\n");
        break;

    case json_archiver::error_code::file_error:
        fmt::print(std::cerr, "json archiving file access error\n");
        break;

    case json_archiver::error_code::format_error:
        fmt::print(std::cerr, "json archiving format error\n");
        break;

    case json_archiver::error_code::dependency_error:
        fmt::print(std::cerr, "json archiving format error\n");
        break;

    default:
        fmt::print(std::cerr, "json de-archiving unknown error\n");
    }
}

void json_archiver::destroy() noexcept
{
    buffer.destroy();

    model_mapping.data.destroy();
    constant_mapping.data.destroy();
    binary_file_mapping.data.destroy();
    random_mapping.data.destroy();
    text_file_mapping.data.destroy();
    sim_hsms_mapping.data.destroy();
}

void json_archiver::clear() noexcept
{
    buffer.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
    sim_hsms_mapping.data.clear();
}
} //  irt
