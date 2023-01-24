// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <optional>

namespace irt {

struct file_header
{
    enum class mode_type : i32
    {
        none = 0,
        all  = 1,
    };

    i32       code    = 0x11223344;
    i32       length  = sizeof(file_header);
    i32       version = 1;
    mode_type type    = mode_type::all;
};

//! A structure use to cache data when read or write json component.
//! - @c buffer is used to store the full file content or output buffer.
//! - @c string_buffer is used when reading string.
//! - @c stack is used when parsing project file.
//! - other variable are used to link file identifier with new identifier.
struct json_cache
{
    vector<char> buffer;
    std::string  string_buffer;

    table<u64, u64> model_mapping;
    table<u64, u64> constant_mapping;
    table<u64, u64> binary_file_mapping;
    table<u64, u64> random_mapping;
    table<u64, u64> text_file_mapping;

    vector<i32> stack;

    void clear() noexcept;
};

//! Control the json output stream (memory or file) pretty print.
enum class json_pretty_print
{
    off,                    //! disable pretty print.
    indent_2,               //! enable pretty print, use 2 spaces as indent.
    indent_2_one_line_array //! idem but merge simple array in one line.
};

//! Load a simulation structure from a json file.
status simulation_load(simulation&      sim,
                       external_source& srcs,
                       json_cache&      cache,
                       const char*      filename) noexcept;

//! Load a simulation structure from a json memory buffer. This function is
//! mainly used in unit-test to check i/o functions.
status simulation_load(simulation&      sim,
                       external_source& srcs,
                       json_cache&      cache,
                       std::span<char>  in) noexcept;

//! Save a component structure into a json memory buffer. This function is
//! mainly used in unit-test to check i/o functions.
status simulation_save(
  const simulation&      sim,
  const external_source& srcs,
  json_cache&            cache,
  vector<char>&          out,
  json_pretty_print      print_options = json_pretty_print::off) noexcept;

//! Save a component structure into a json file.
status simulation_save(
  const simulation&      sim,
  const external_source& srcs,
  json_cache&            cache,
  const char*            filename,
  json_pretty_print      print_options = json_pretty_print::off) noexcept;

//! Load a component structure from a json file.
status component_load(modeling&   mod,
                      component&  compo,
                      json_cache& cache,
                      const char* filename) noexcept;

//! Save a component structure into a json file.
status component_save(
  const modeling&   mod,
  const component&  compo,
  json_cache&       cache,
  const char*       filename,
  json_pretty_print print_options = json_pretty_print::off) noexcept;

//! Load a project from a project json file.
status project_load(modeling&   mod,
                    json_cache& cache,
                    const char* filename) noexcept;

//! Save a project from the current modeling.
status project_save(
  modeling&         mod,
  json_cache&       cache,
  const char*       filename,
  json_pretty_print print_options = json_pretty_print::off) noexcept;

struct binary_cache
{
    table<u32, model_id>              to_models;
    table<u32, hsm_id>                to_hsms;
    table<u32, constant_source_id>    to_constant;
    table<u32, binary_file_source_id> to_binary;
    table<u32, text_file_source_id>   to_text;
    table<u32, random_source_id>      to_random;

    void clear() noexcept;
};

status simulation_save(simulation&      sim,
                       external_source& srcs,
                       file&            io) noexcept;

status simulation_save(simulation&      sim,
                       external_source& srcs,
                       memory&          io) noexcept;

status simulation_load(simulation&      sim,
                       external_source& srcs,
                       file&            io,
                       binary_cache&    cache) noexcept;

status simulation_load(simulation&      sim,
                       external_source& srcs,
                       memory&          io,
                       binary_cache&    cache) noexcept;

//! Return the description string for each status.
const char* status_string(const status s) noexcept;

static constexpr inline const char* dynamics_type_names[] = {
    "qss1_integrator", "qss1_multiplier", "qss1_cross",    "qss1_filer",
    "qss1_power",      "qss1_square",     "qss1_sum_2",    "qss1_sum_3",
    "qss1_sum_4",      "qss1_wsum_2",     "qss1_wsum_3",   "qss1_wsum_4",
    "qss2_integrator", "qss2_multiplier", "qss2_cross",    "qss2_filter",
    "qss2_power",      "qss2_square",     "qss2_sum_2",    "qss2_sum_3",
    "qss2_sum_4",      "qss2_wsum_2",     "qss2_wsum_3",   "qss2_wsum_4",
    "qss3_integrator", "qss3_multiplier", "qss3_cross",    "qss3_filter",
    "qss3_power",      "qss3_square",     "qss3_sum_2",    "qss3_sum_3",
    "qss3_sum_4",      "qss3_wsum_2",     "qss3_wsum_3",   "qss3_wsum_4",
    "integrator",      "quantifier",      "adder_2",       "adder_3",
    "adder_4",         "mult_2",          "mult_3",        "mult_4",
    "counter",         "queue",           "dynamic_queue", "priority_queue",
    "generator",       "constant",        "cross",         "time_func",
    "accumulator_2",   "filter",          "logical_and_2", "logical_and_3",
    "logical_invert",  "logical_or_2",    "logical_or_3",  "hsm"
};

inline auto convert(std::string_view dynamics_name) noexcept
  -> std::optional<dynamics_type>
{
    struct string_to_type
    {
        constexpr string_to_type(const std::string_view n,
                                 const dynamics_type    t) noexcept
          : name(n)
          , type(t)
        {
        }

        std::string_view name;
        dynamics_type    type;
    };

    static constexpr string_to_type table[] = {
        { "accumulator_2", dynamics_type::accumulator_2 },
        { "adder_2", dynamics_type::adder_2 },
        { "adder_3", dynamics_type::adder_3 },
        { "adder_4", dynamics_type::adder_4 },
        { "constant", dynamics_type::constant },
        { "counter", dynamics_type::counter },
        { "cross", dynamics_type::cross },
        { "dynamic_queue", dynamics_type::dynamic_queue },
        { "filter", dynamics_type::filter },
        { "generator", dynamics_type::generator },
        { "hsm", dynamics_type::hsm_wrapper },
        { "integrator", dynamics_type::integrator },
        { "logical_and_2", dynamics_type::logical_and_2 },
        { "logical_and_3", dynamics_type::logical_and_3 },
        { "logical_invert", dynamics_type::logical_invert },
        { "logical_or_2", dynamics_type::logical_or_2 },
        { "logical_or_3", dynamics_type::logical_or_3 },
        { "mult_2", dynamics_type::mult_2 },
        { "mult_3", dynamics_type::mult_3 },
        { "mult_4", dynamics_type::mult_4 },
        { "priority_queue", dynamics_type::priority_queue },
        { "qss1_cross", dynamics_type::qss1_cross },
        { "qss1_filter", dynamics_type::qss1_filter },
        { "qss1_integrator", dynamics_type::qss1_integrator },
        { "qss1_multiplier", dynamics_type::qss1_multiplier },
        { "qss1_power", dynamics_type::qss1_power },
        { "qss1_square", dynamics_type::qss1_square },
        { "qss1_sum_2", dynamics_type::qss1_sum_2 },
        { "qss1_sum_3", dynamics_type::qss1_sum_3 },
        { "qss1_sum_4", dynamics_type::qss1_sum_4 },
        { "qss1_wsum_2", dynamics_type::qss1_wsum_2 },
        { "qss1_wsum_3", dynamics_type::qss1_wsum_3 },
        { "qss1_wsum_4", dynamics_type::qss1_wsum_4 },
        { "qss2_cross", dynamics_type::qss2_cross },
        { "qss2_filter", dynamics_type::qss2_filter },
        { "qss2_integrator", dynamics_type::qss2_integrator },
        { "qss2_multiplier", dynamics_type::qss2_multiplier },
        { "qss2_power", dynamics_type::qss2_power },
        { "qss2_square", dynamics_type::qss2_square },
        { "qss2_sum_2", dynamics_type::qss2_sum_2 },
        { "qss2_sum_3", dynamics_type::qss2_sum_3 },
        { "qss2_sum_4", dynamics_type::qss2_sum_4 },
        { "qss2_wsum_2", dynamics_type::qss2_wsum_2 },
        { "qss2_wsum_3", dynamics_type::qss2_wsum_3 },
        { "qss2_wsum_4", dynamics_type::qss2_wsum_4 },
        { "qss3_cross", dynamics_type::qss3_cross },
        { "qss3_filter", dynamics_type::qss3_filter },
        { "qss3_integrator", dynamics_type::qss3_integrator },
        { "qss3_multiplier", dynamics_type::qss3_multiplier },
        { "qss3_power", dynamics_type::qss3_power },
        { "qss3_square", dynamics_type::qss3_square },
        { "qss3_sum_2", dynamics_type::qss3_sum_2 },
        { "qss3_sum_3", dynamics_type::qss3_sum_3 },
        { "qss3_sum_4", dynamics_type::qss3_sum_4 },
        { "qss3_wsum_2", dynamics_type::qss3_wsum_2 },
        { "qss3_wsum_3", dynamics_type::qss3_wsum_3 },
        { "qss3_wsum_4", dynamics_type::qss3_wsum_4 },
        { "quantifier", dynamics_type::quantifier },
        { "queue", dynamics_type::queue },
        { "time_func", dynamics_type::time_func }
    };

    static_assert(std::size(table) == static_cast<sz>(dynamics_type_size()));

    const auto it =
      std::lower_bound(std::begin(table),
                       std::end(table),
                       dynamics_name,
                       [](const string_to_type& l, const std::string_view r) {
                           return l.name < r;
                       });

    return (it != std::end(table) && it->name == dynamics_name)
             ? std::make_optional(it->type)
             : std::nullopt;
}

static_assert(std::size(dynamics_type_names) ==
              static_cast<sz>(dynamics_type_size()));

static inline const char* str_empty[]                 = { "" };
static inline const char* str_integrator[]            = { "x-dot", "reset" };
static inline const char* str_adaptative_integrator[] = { "quanta",
                                                          "x-dot",
                                                          "reset" };
static inline const char* str_in_1[]                  = { "in" };
static inline const char* str_in_2[]                  = { "in-1", "in-2" };
static inline const char* str_in_3[] = { "in-1", "in-2", "in-3" };
static inline const char* str_in_4[] = { "in-1", "in-2", "in-3", "in-4" };
static inline const char* str_value_if_else[] = { "value",
                                                  "if",
                                                  "else",
                                                  "threshold" };
static inline const char* str_in_2_nb_2[] = { "in-1", "in-2", "nb-1", "nb-2" };

template<typename Dynamics>
static constexpr const char** get_input_port_names() noexcept
{
    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss2_integrator> ||
                  std::is_same_v<Dynamics, qss3_integrator>)
        return str_integrator;

    if constexpr (std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, qss3_multiplier> ||
                  std::is_same_v<Dynamics, qss3_sum_2> ||
                  std::is_same_v<Dynamics, qss3_wsum_2> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, mult_2> ||
                  std::is_same_v<Dynamics, logical_and_2> ||
                  std::is_same_v<Dynamics, logical_or_2>)
        return str_in_2;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss3_sum_3> ||
                  std::is_same_v<Dynamics, qss3_wsum_3> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, mult_3> ||
                  std::is_same_v<Dynamics, logical_and_3> ||
                  std::is_same_v<Dynamics, logical_or_3>)
        return str_in_3;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, qss3_sum_4> ||
                  std::is_same_v<Dynamics, qss3_wsum_4> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_4> ||
                  std::is_same_v<Dynamics, hsm_wrapper>)
        return str_in_4;

    if constexpr (std::is_same_v<Dynamics, integrator>)
        return str_adaptative_integrator;

    if constexpr (std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, filter> ||
                  std::is_same_v<Dynamics, queue> ||
                  std::is_same_v<Dynamics, dynamic_queue> ||
                  std::is_same_v<Dynamics, priority_queue> ||
                  std::is_same_v<Dynamics, qss1_filter> ||
                  std::is_same_v<Dynamics, qss2_filter> ||
                  std::is_same_v<Dynamics, qss3_filter> ||
                  std::is_same_v<Dynamics, qss1_power> ||
                  std::is_same_v<Dynamics, qss2_power> ||
                  std::is_same_v<Dynamics, qss3_power> ||
                  std::is_same_v<Dynamics, qss1_square> ||
                  std::is_same_v<Dynamics, qss2_square> ||
                  std::is_same_v<Dynamics, qss3_square> ||
                  std::is_same_v<Dynamics, logical_invert>)
        return str_in_1;

    if constexpr (std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, qss3_cross> ||
                  std::is_same_v<Dynamics, cross>)
        return str_value_if_else;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_in_2_nb_2;

    irt_unreachable();
}

static constexpr const char** get_input_port_names(
  const dynamics_type type) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss3_integrator:
        return str_integrator;

    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::adder_2:
    case dynamics_type::mult_2:
    case dynamics_type::logical_and_2:
    case dynamics_type::logical_or_2:
        return str_in_2;

    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::adder_3:
    case dynamics_type::mult_3:
    case dynamics_type::logical_and_3:
    case dynamics_type::logical_or_3:
        return str_in_3;

    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::adder_4:
    case dynamics_type::mult_4:
    case dynamics_type::hsm_wrapper:
        return str_in_4;

    case dynamics_type::integrator:
        return str_adaptative_integrator;

    case dynamics_type::quantifier:
    case dynamics_type::counter:
    case dynamics_type::filter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::qss1_filter:
    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
    case dynamics_type::qss1_power:
    case dynamics_type::qss2_power:
    case dynamics_type::qss3_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss2_square:
    case dynamics_type::qss3_square:
    case dynamics_type::logical_invert:
        return str_in_1;

    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
        return str_empty;

    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
    case dynamics_type::cross:
        return str_value_if_else;

    case dynamics_type::accumulator_2:
        return str_in_2_nb_2;
    }

    irt_unreachable();
}

static inline const char* str_out_1[] = { "out" };
static inline const char* str_out_4[] = { "out-1", "out-2", "out-3", "out-4" };
static inline const char* str_out_cross[]  = { "if-value",
                                               "else-value",
                                               "event" };
static inline const char* str_out_filter[] = { "value", "up", "down" };

template<typename Dynamics>
static constexpr const char** get_output_port_names() noexcept
{
    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_power> ||
                  std::is_same_v<Dynamics, qss1_square> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_integrator> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_power> ||
                  std::is_same_v<Dynamics, qss2_square> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, qss3_integrator> ||
                  std::is_same_v<Dynamics, qss3_multiplier> ||
                  std::is_same_v<Dynamics, qss3_power> ||
                  std::is_same_v<Dynamics, qss3_square> ||
                  std::is_same_v<Dynamics, qss3_sum_2> ||
                  std::is_same_v<Dynamics, qss3_sum_3> ||
                  std::is_same_v<Dynamics, qss3_sum_4> ||
                  std::is_same_v<Dynamics, qss3_wsum_2> ||
                  std::is_same_v<Dynamics, qss3_wsum_3> ||
                  std::is_same_v<Dynamics, qss3_wsum_4> ||
                  std::is_same_v<Dynamics, integrator> ||
                  std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_2> ||
                  std::is_same_v<Dynamics, mult_3> ||
                  std::is_same_v<Dynamics, mult_4> ||
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, queue> ||
                  std::is_same_v<Dynamics, dynamic_queue> ||
                  std::is_same_v<Dynamics, priority_queue> ||
                  std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, filter> ||
                  std::is_same_v<Dynamics, logical_and_2> ||
                  std::is_same_v<Dynamics, logical_and_3> ||
                  std::is_same_v<Dynamics, logical_or_2> ||
                  std::is_same_v<Dynamics, logical_or_3> ||
                  std::is_same_v<Dynamics, logical_invert>)
        return str_out_1;

    if constexpr (std::is_same_v<Dynamics, qss1_filter> ||
                  std::is_same_v<Dynamics, qss2_filter> ||
                  std::is_same_v<Dynamics, qss3_filter>)
        return str_out_filter;

    if constexpr (std::is_same_v<Dynamics, cross> ||
                  std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, qss3_cross>)
        return str_out_cross;

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return str_out_4;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_empty;

    irt_unreachable();
}

static constexpr const char** get_output_port_names(
  const dynamics_type type) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::integrator:
    case dynamics_type::quantifier:
    case dynamics_type::adder_2:
    case dynamics_type::adder_3:
    case dynamics_type::adder_4:
    case dynamics_type::mult_2:
    case dynamics_type::mult_3:
    case dynamics_type::mult_4:
    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
    case dynamics_type::filter:
    case dynamics_type::logical_and_2:
    case dynamics_type::logical_or_2:
    case dynamics_type::logical_and_3:
    case dynamics_type::logical_or_3:
    case dynamics_type::logical_invert:
        return str_out_1;

    case dynamics_type::hsm_wrapper:
        return str_out_4;

    case dynamics_type::cross:
    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
        return str_out_cross;

    case dynamics_type::qss1_filter:
    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
        return str_out_filter;

    case dynamics_type::accumulator_2:
        return str_empty;
    }

    irt_unreachable();
}

static constexpr inline const char* status_string_names[] = {
    "success",
    "unknown dynamics",
    "block allocator bad capacity",
    "block allocator not enough memory",
    "head allocator bad capacity",
    "head allocator not enough memory",
    "simulation not enough model",
    "simulation not enough message",
    "simulation not enough connection",
    "vector init capacity error",
    "vector not enough memory",
    "data array init capacity error",
    "data array not enough memory",
    "source unknown",
    "source empty",
    "model connect output port unknown",
    "model connect already exist",
    "model connect bad dynamics",
    "model queue bad ta",
    "model queue full",
    "model dynamic queue source is null",
    "model dynamic queue full",
    "model priority queue source is null",
    "model priority queue full",
    "model integrator dq error",
    "model integrator X error",
    "model integrator internal error",
    "model integrator output error",
    "model integrator running without x dot",
    "model integrator ta with bad x dot",
    "model quantifier bad quantum parameter",
    "model quantifier bad archive length parameter",
    "model quantifier shifting value neg",
    "model quantifier shifting value less 1",
    "model time func bad init message",
    "model hsm bad top state",
    "model hsm bad next state",
    "model filter threshold condition not satisfied",
    "modeling too many description open",
    "modeling too many file open",
    "modeling too many directory open",
    "modeling registred path access error",
    "modeling directory access error",
    "modeling file access error",
    "modeling component save error",
    "gui not enough memory",
    "io not enough memory",
    "io filesystem error",
    "io filesystem make directory error",
    "io filesystem not directory error",
    "io project component empty",
    "io project component type error",
    "io project file error",
    "io project file component type error",
    "io project file component path error",
    "io project file component directory error",
    "io project file component file error",
    "io project file parameters error",
    "io project file parameters access error",
    "io project file parameters type error",
    "io project file parameters init error",
    "io file format error",
    "io file format source number error",
    "io file source full",
    "io file format model error",
    "io file format model number error",
    "io file format model unknown",
    "io file format dynamics unknown",
    "io file format dynamics limit reach",
    "io file format dynamics init error",
};

inline const char* status_string(const status s) noexcept
{
    static_assert(std::size(status_string_names) == status_size());

    return status_string_names[ordinal(s)];
}

} // namespace irt

#endif
