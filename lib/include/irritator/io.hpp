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
#include <istream>
#include <ostream>
#include <streambuf>
#include <vector>

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

struct memory_requirement
{
    i32 constant_external_source = 0;
    i32 binary_external_source   = 0;
    i32 text_external_source     = 0;
    i32 random_external_source   = 0;
    i32 models                   = 0;
    i32 hsms                     = 0;
};

//! Callbacks call after read the number of models, hsms, observer and
//! messages to allocate. Use this function to preallocate the simulation @c
//! data_array.
using memory_callback = function_ref<bool(const memory_requirement&)>;

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

struct archiver
{
    status perform(simulation& sim, external_source& srcs, file& io) noexcept;
    status perform(simulation& sim, external_source& srcs, memory& io) noexcept;
};

struct dearchiver
{
    table<u32, model_id>              u32_to_models;
    table<u32, hsm_id>                u32_to_hsms;
    table<u32, constant_source_id>    u32_to_constant;
    table<u32, binary_file_source_id> u32_to_binary;
    table<u32, text_file_source_id>   u32_to_text;
    table<u32, random_source_id>      u32_to_random;

    data_array<model, model_id>*                    models = nullptr;
    data_array<hierarchical_state_machine, hsm_id>* hsms   = nullptr;

    status perform(simulation&      sim,
                   external_source& srcs,
                   file&            io,
                   memory_callback  callback) noexcept;

    status perform(simulation&      sim,
                   external_source& srcs,
                   memory&          io,
                   memory_callback  callback) noexcept;
};

//! Return the description string for each status.
const char* status_string(const status s) noexcept;

static constexpr inline const char* dynamics_type_names[] = {
    "qss1_integrator", "qss1_multiplier", "qss1_cross",
    "qss1_power",      "qss1_square",     "qss1_sum_2",
    "qss1_sum_3",      "qss1_sum_4",      "qss1_wsum_2",
    "qss1_wsum_3",     "qss1_wsum_4",     "qss2_integrator",
    "qss2_multiplier", "qss2_cross",      "qss2_power",
    "qss2_square",     "qss2_sum_2",      "qss2_sum_3",
    "qss2_sum_4",      "qss2_wsum_2",     "qss2_wsum_3",
    "qss2_wsum_4",     "qss3_integrator", "qss3_multiplier",
    "qss3_cross",      "qss3_power",      "qss3_square",
    "qss3_sum_2",      "qss3_sum_3",      "qss3_sum_4",
    "qss3_wsum_2",     "qss3_wsum_3",     "qss3_wsum_4",
    "integrator",      "quantifier",      "adder_2",
    "adder_3",         "adder_4",         "mult_2",
    "mult_3",          "mult_4",          "counter",
    "queue",           "dynamic_queue",   "priority_queue",
    "generator",       "constant",        "cross",
    "time_func",       "accumulator_2",   "filter",
    "logical_and_2",   "logical_and_3",   "logical_invert",
    "logical_or_2",    "logical_or_3",    "hsm"
};

inline bool convert(const std::string_view dynamics_name,
                    dynamics_type*         type) noexcept
{
    struct string_to_type
    {
        constexpr string_to_type(const std::string_view n,
                                 const dynamics_type    t) noexcept
          : name(n)
          , type(t)
        {
        }

        const std::string_view name;
        dynamics_type          type;
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

    if (it != std::end(table) && it->name == dynamics_name) {
        *type = it->type;
        return true;
    }

    return false;
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
static inline const char* str_out_cross[] = { "if-value",
                                              "else-value",
                                              "event" };

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

    case dynamics_type::accumulator_2:
        return str_empty;
    }

    irt_unreachable();
}

static constexpr inline const char* status_string_names[] = {
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
    "model_hsm_bad_top_state",
    "model_hsm_bad_next_state",

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

    "io_project_file_error",
    "io_project_file_component_path_error",
    "io_project_file_component_directory_error",
    "io_project_file_component_file_error",
    "io_project_file_parameters_error",
    "io_project_file_parameters_access_error",
    "io_project_file_parameters_type_error",
    "io_project_file_parameters_init_error",

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

inline const char* status_string(const status s) noexcept
{
    static_assert(std::size(status_string_names) == status_size());

    return status_string_names[ordinal(s)];
}

} // namespace irt

#endif
