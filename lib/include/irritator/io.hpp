// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>

namespace irt {

static inline constexpr const std::string_view log_level_names[] = {
    "emergency", "alert",  "critical", "error",
    "warning",   "notice", "info",     "debug",
};

static inline constexpr const std::string_view component_status_string[] = {
    "unread",
    "read_only",
    "modified",
    "unmodified",
    "unreadable"
};

static constexpr inline const char* component_type_names[] = { "none",
                                                               "internal",
                                                               "simple",
                                                               "grid",
                                                               "graph" };

//! Try to get the component type from a string. If the string is unknown,
//! optional returns \c std::nullopt.
auto get_component_type(std::string_view name) noexcept
  -> std::optional<component_type>;

static constexpr inline const char* internal_component_names[] = {
    "qss1_izhikevich",   "qss1_lif",   "qss1_lotka_volterra",
    "qss1_negative_lif", "qss1_seirs", "qss1_van_der_pol",
    "qss2_izhikevich",   "qss2_lif",   "qss2_lotka_volterra",
    "qss2_negative_lif", "qss2_seirs", "qss2_van_der_pol",
    "qss3_izhikevich",   "qss3_lif",   "qss3_lotka_volterra",
    "qss3_negative_lif", "qss3_seirs", "qss3_van_der_pol"
};

//! Try to get internal component type from a string. If the string is unknown,
//! optional returns \c std::nullopt.
auto get_internal_component_type(std::string_view name) noexcept
  -> std::optional<internal_component>;

static constexpr inline const char* dynamics_type_names[] = {
    "qss1_integrator", "qss1_multiplier", "qss1_cross",    "qss1_filter",
    "qss1_power",      "qss1_square",     "qss1_sum_2",    "qss1_sum_3",
    "qss1_sum_4",      "qss1_wsum_2",     "qss1_wsum_3",   "qss1_wsum_4",
    "qss2_integrator", "qss2_multiplier", "qss2_cross",    "qss2_filter",
    "qss2_power",      "qss2_square",     "qss2_sum_2",    "qss2_sum_3",
    "qss2_sum_4",      "qss2_wsum_2",     "qss2_wsum_3",   "qss2_wsum_4",
    "qss3_integrator", "qss3_multiplier", "qss3_cross",    "qss3_filter",
    "qss3_power",      "qss3_square",     "qss3_sum_2",    "qss3_sum_3",
    "qss3_sum_4",      "qss3_wsum_2",     "qss3_wsum_3",   "qss3_wsum_4",
    "counter",         "queue",           "dynamic_queue", "priority_queue",
    "generator",       "constant",        "time_func",     "accumulator_2",
    "logical_and_2",   "logical_and_3",   "logical_or_2",  "logical_or_3",
    "logical_invert",  "hsm_wrapper",
};

//! Try to get the dymamics type from a string. If the string is unknown,
//! optional returns \c std::nullopt.
auto get_dynamics_type(std::string_view dynamics_name) noexcept
  -> std::optional<dynamics_type>;

static_assert(std::size(dynamics_type_names) ==
              static_cast<sz>(dynamics_type_size()));

static inline const char* str_empty[]      = { "" };
static inline const char* str_integrator[] = { "x-dot", "reset" };
static inline const char* str_in_1[]       = { "in" };
static inline const char* str_in_2[]       = { "in-1", "in-2" };
static inline const char* str_in_3[]       = { "in-1", "in-2", "in-3" };
static inline const char* str_in_4[]       = { "in-1", "in-2", "in-3", "in-4" };
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
                  std::is_same_v<Dynamics, logical_and_2> ||
                  std::is_same_v<Dynamics, logical_or_2>)
        return str_in_2;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss3_sum_3> ||
                  std::is_same_v<Dynamics, qss3_wsum_3> ||
                  std::is_same_v<Dynamics, logical_and_3> ||
                  std::is_same_v<Dynamics, logical_or_3>)
        return str_in_3;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, qss3_sum_4> ||
                  std::is_same_v<Dynamics, qss3_wsum_4> ||
                  std::is_same_v<Dynamics, hsm_wrapper>)
        return str_in_4;

    if constexpr (std::is_same_v<Dynamics, counter> ||
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
                  std::is_same_v<Dynamics, qss3_cross>)
        return str_value_if_else;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_in_2_nb_2;

    unreachable();
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
    case dynamics_type::logical_and_2:
    case dynamics_type::logical_or_2:
        return str_in_2;

    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::logical_and_3:
    case dynamics_type::logical_or_3:
        return str_in_3;

    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::hsm_wrapper:
        return str_in_4;

    case dynamics_type::counter:
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
        return str_value_if_else;

    case dynamics_type::accumulator_2:
        return str_in_2_nb_2;
    }

    unreachable();
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
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, queue> ||
                  std::is_same_v<Dynamics, dynamic_queue> ||
                  std::is_same_v<Dynamics, priority_queue> ||
                  std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
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

    if constexpr (std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, qss3_cross>)
        return str_out_cross;

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return str_out_4;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_empty;

    unreachable();
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
    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
    case dynamics_type::logical_and_2:
    case dynamics_type::logical_or_2:
    case dynamics_type::logical_and_3:
    case dynamics_type::logical_or_3:
    case dynamics_type::logical_invert:
        return str_out_1;

    case dynamics_type::hsm_wrapper:
        return str_out_4;

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

    unreachable();
}

static constexpr inline const char* status_string_names[] = {
    "io not enough memory",
    "io filesystem error",
    "io filesystem make directory error",
    "io filesystem not directory error"
};

static inline const char* external_source_type_string[] = { "binary_file",
                                                            "constant",
                                                            "random",
                                                            "text_file" };

inline const char* external_source_str(const source::source_type type) noexcept
{
    return external_source_type_string[ordinal(type)];
}

static const char* distribution_type_string[] = {
    "bernouilli",        "binomial", "cauchy",  "chi_squared", "exponential",
    "exterme_value",     "fisher_f", "gamma",   "geometric",   "lognormal",
    "negative_binomial", "normal",   "poisson", "student_t",   "uniform_int",
    "uniform_real",      "weibull"
};

inline const char* distribution_str(const distribution_type type) noexcept
{
    return distribution_type_string[static_cast<int>(type)];
}

//! Try to get the random source distribution type from a string. If the string
//! is unknwon, optional return @c std::nullopt.
auto get_distribution_type(std::string_view name) noexcept
  -> std::optional<distribution_type>;

enum class random_file_type {
    binary,
    text,
};

template<typename RandomGenerator, typename Distribution>
inline int generate_random_file(std::ostream&          os,
                                RandomGenerator&       gen,
                                Distribution&          dist,
                                const std::size_t      size,
                                const random_file_type type) noexcept
{
    switch (type) {
    case random_file_type::text: {
        if (!os)
            return -1;

        for (std::size_t sz = 0; sz < size; ++sz)
            if (!(os << dist(gen) << '\n'))
                return -2;
    } break;

    case random_file_type::binary: {
        if (!os)
            return -1;

        for (std::size_t sz = 0; sz < size; ++sz) {
            const double value = dist(gen);
            os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    } break;
    }

    return 0;
}

} // namespace irt

#endif
