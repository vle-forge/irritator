// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/io.hpp>

#include <fstream>

#include <fmt/format.h>

#include <cstdio>

static inline std::string_view status_str[] = {
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
    "io_file_format_dynamics_init_error",
};

enum action_type
{
    action_nothing,
    action_help,
    action_run,
    action_version
};

enum status_type
{
    status_success,
    status_unknown_action,
    status_missing_run_arguments,
    status_bad_begin_time_argument,
    status_bad_duration_time_argument,
    status_bad_models_argument,
    status_bad_messages_argument
};

struct main_action
{
    main_action(action_type      type_,
                std::string_view short_string_,
                std::string_view long_string_,
                int              argument_) noexcept;

    action_type      type;
    std::string_view short_string;
    std::string_view long_string;
    int              argument;

    bool operator<(const std::string_view other) const noexcept;
    bool operator==(const std::string_view other) const noexcept;
};

main_action actions[] = { { action_help, "h", "help", 0 },
                          { action_run, "r", "run", 2 },
                          { action_version, "v", "version", 0 } };

static inline std::string_view main_status_str[] = {
    "success",
    "unknown action",
    "missing run arguments",
    "bad begin time argument",
    "bad duration time argument",
    "status bad models argument",
    "status bad messages argument"
};

//! Show help message in console.
void show_help() noexcept;

//! Show current version in console.
void show_version() noexcept;

//! Run simulation from simulation file.
void run_simulation(irt::real   begin,
                    irt::real   duration,
                    int         models,
                    int         messages,
                    const char* file_name) noexcept;

struct main_parameters
{
    irt::real   begin    = irt::zero;
    irt::real   duration = irt::one;
    int         models   = 2048;
    int         messages = 4096;
    status_type status   = status_success;
    action_type action   = action_nothing;
    int         files    = 0;

    main_parameters() = default;

    static bool parse_real(const char* param, irt::real& out) noexcept;
    static bool parse_integer(const char* param, int& out) noexcept;
    bool        parse(int argc, char* argv[]) noexcept;
};

int main(int argc, char* argv[])
{
    main_parameters params;

    if (!params.parse(argc, argv)) {
        fmt::print(stderr,
                   "Fail to parse arguments. Error: {}\n",
                   main_status_str[irt::ordinal(params.status)]);

        return 0;
    }

    switch (params.action) {
    case action_nothing:
        break;
    case action_help:
        show_help();
        break;
    case action_run:
        for (; params.files < argc; ++params.files)
            run_simulation(params.begin,
                           params.duration,
                           params.models,
                           params.messages,
                           argv[params.files]);
        fmt::print("\n\n");
        break;
    case action_version:
        show_version();
        break;
    }

    return 0;
}

main_action::main_action(action_type      type_,
                         std::string_view short_string_,
                         std::string_view long_string_,
                         int              argument_) noexcept
  : type(type_)
  , short_string(short_string_)
  , long_string(long_string_)
  , argument(argument_)
{
}

bool main_action::operator<(const std::string_view other) const noexcept
{
    return long_string < other;
}

bool main_action::operator==(const std::string_view other) const noexcept
{
    return long_string == other;
}

bool main_parameters::parse_real(const char* param, irt::real& out) noexcept
{
    long double result = 0;
    if (auto read = std::sscanf(param, "%Lf", &result); read) {
        out = irt::to_real(result);
        return true;
    }

    return false;
}

bool main_parameters::parse_integer(const char* param, int& out) noexcept
{
    int result = 0;
    if (auto read = std::sscanf(param, "%d", &result); read) {
        out = result;
        return true;
    }

    return false;
}

bool main_parameters::parse(int argc, char* argv[]) noexcept
{
    if (argc <= 1)
        return true;

    auto str = argv[1];
    auto it  = irt::binary_find(std::begin(actions), std::end(actions), str);

    if (it == std::end(actions)) {
        status = status_unknown_action;
        return false;
    }

    action = it->type;

    if (it->argument == 0)
        return true;

    if (it->type == action_run) {
        if (5 < argc) {
            if (!parse_real(argv[2], begin)) {
                status = status_bad_begin_time_argument;
                return false;
            }

            if (!parse_real(argv[3], duration)) {
                status = status_bad_duration_time_argument;
                return false;
            }

            if (!parse_integer(argv[4], models) || models < 0) {
                status = status_bad_models_argument;
                return false;
            }

            if (!parse_integer(argv[5], messages) || messages < 0) {
                status = status_bad_messages_argument;
                return false;
            }
        } else {
            status = status_missing_run_arguments;
            return false;
        }
    }

    files = 6;
    return true;
}

void show_help() noexcept
{
    fmt::print(
      "irritator-cli action-name action-argument [files...]\n"
      "\n\n"
      "help        This help message\n"
      "version     Version of irritator-cli\n"
      "information Information about simulation files\n"
      "		 (no argument required)\n"
      "run 	 Run simulation files\n"
      "	Need parameters:\n"
      "	- [real] The begin date of the begin date of the simulation\n"
      "	- [real] The duration of the simulation\n"
      " - [integer] The number of models to pre-allocate\n"
      " - [integer] The number of messages to pre-allocate\n"
      "\n\n");
}

void show_version() noexcept
{
    fmt::print("irritator-cli {}.{}.{}-{}\n\n",
               VERSION_MAJOR,
               VERSION_MINOR,
               VERSION_PATCH,
               VERSION_TWEAK);
}

void run_simulation(irt::real   begin,
                    irt::real   duration,
                    int         models,
                    int         messages,
                    const char* file_name) noexcept
{
    fmt::print("Run simulation from `{}' to `{}' for file {}\n",
               begin,
               duration,
               file_name);

    irt::simulation      sim;
    irt::external_source srcs;

    if (irt::is_bad(sim.init(static_cast<unsigned>(models),
                             static_cast<unsigned>(messages)))) {
        fmt::print(stderr, "Fail to allocate {} models\n", models);
        return;
    }

    if (irt::is_bad(srcs.init(64u))) {
        fmt::print(stderr, "Fail to allocate 64 external sources\n");
        return;
    }

    std::ifstream ifs(file_name);
    if (ifs.bad()) {
        fmt::print(stderr, "Fail to open file `{}'\n", file_name);
        return;
    }

    irt::reader reader(ifs);
    if (auto ret = reader(sim, srcs); irt::is_bad(ret)) {
        fmt::print(stderr,
                   "Fail to read file `{}' ({})\n",
                   file_name,
                   status_str[irt::ordinal(ret)]);
        return;
    }

    irt::status ret;
    irt::time   t   = begin;
    irt::time   end = begin + duration;

    if (ret = srcs.prepare(); is_bad(ret)) {
        fmt::print(stderr,
                   "Fail to initialize external sources: {}\n",
                   status_str[irt::ordinal(ret)]);
        return;
    }

    if (ret = sim.initialize(t); is_bad(ret)) {
        fmt::print(stderr,
                   "Fail in initialize simulation: {}\n",
                   status_str[irt::ordinal(ret)]);
        return;
    }

    do {
        if (ret = sim.run(t); is_bad(ret)) {
            fmt::print(stderr,
                       "Fail in during simulation: {}\n",
                       status_str[irt::ordinal(ret)]);
            break;
        }

    } while (t < end);

    if (ret = sim.finalize(t); is_bad(ret)) {
        fmt::print(stderr,
                   "Fail in finalizing simulation operation: {}\n",
                   status_str[irt::ordinal(ret)]);
    }

    fmt::print("\n\n");
}
