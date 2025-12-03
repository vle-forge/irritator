// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/modeling.hpp>

#include <optional>
#include <string_view>
#include <type_traits>

namespace irt {

static inline constexpr const std::string_view component_status_string[] = {
    "unread",
    "read_only",
    "modified",
    "unmodified",
    "unreadable"
};

static constexpr inline const char* component_type_names[] = { "none",
                                                               "simple",
                                                               "grid",
                                                               "graph",
                                                               "hsm" };

static_assert(component_type_count == length(component_type_names));

static constexpr inline const char* port_option_names[] = { "classic",
                                                            "sum",
                                                            "wsum" };

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
    "qss1_integrator", "qss1_multiplier", "qss1_cross",      "qss1_flipflop",
    "qss1_filter",     "qss1_power",      "qss1_square",     "qss1_sum_2",
    "qss1_sum_3",      "qss1_sum_4",      "qss1_wsum_2",     "qss1_wsum_3",
    "qss1_wsum_4",     "qss1_inverse",    "qss1_integer",    "qss1_compare",
    "qss1_gain",       "qss1_sin",        "qss1_cos",        "qss1_log",
    "qss1_exp",        "qss2_integrator", "qss2_multiplier", "qss2_cross",
    "qss2_flipflop",   "qss2_filter",     "qss2_power",      "qss2_square",
    "qss2_sum_2",      "qss2_sum_3",      "qss2_sum_4",      "qss2_wsum_2",
    "qss2_wsum_3",     "qss2_wsum_4",     "qss2_inverse",    "qss2_integer",
    "qss2_compare",    "qss2_gain",       "qss2_sin",        "qss2_cos",
    "qss2_log",        "qss2_exp",        "qss3_integrator", "qss3_multiplier",
    "qss3_cross",      "qss3_flipflop",   "qss3_filter",     "qss3_power",
    "qss3_square",     "qss3_sum_2",      "qss3_sum_3",      "qss3_sum_4",
    "qss3_wsum_2",     "qss3_wsum_3",     "qss3_wsum_4",     "qss3_inverse",
    "qss3_integer",    "qss3_compare",    "qss3_gain",       "qss3_sin",
    "qss3_cos",        "qss3_log",        "qss3_exp",        "counter",
    "queue",           "dynamic_queue",   "priority_queue",  "generator",
    "constant",        "time_func",       "accumulator_2",   "logical_and_2",
    "logical_and_3",   "logical_or_2",    "logical_or_3",    "logical_invert",
    "hsm_wrapper"
};

//! Try to get the dymamics type from a string. If the string is unknown,
//! optional returns \c std::nullopt.
auto get_dynamics_type(std::string_view dynamics_name) noexcept
  -> std::optional<dynamics_type>;

static_assert(std::size(dynamics_type_names) ==
              static_cast<sz>(dynamics_type_size()));

static inline const char* external_source_type_string[] = { "constant",
                                                            "binary-file",
                                                            "text-file",
                                                            "random" };

inline const char* external_source_str(const source_type type) noexcept
{
    return external_source_type_string[ordinal(type)];
}

static constexpr inline const char* distribution_type_string[] = {
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

enum class random_file_type : u8 {
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

constexpr static inline auto dot_output_names = std::to_array<std::string_view>(
  { "out", "out_1", "out_2", "out_3", "out_4", "up", "down", "value" });

constexpr static inline auto output_names = std::to_array<std::string_view>(
  { "out", "out-1", "out-2", "out-3", "out-4", "up", "down", "value" });

constexpr static inline auto dot_output_index =
  std::to_array<std::pair<unsigned char, unsigned char>>({
    { 0, 0 }, // 0. empty
    { 0, 1 }, // 1. out
    { 1, 4 }, // 2. out1, out2, out3, out4
    { 5, 2 }, // 3. up, down
  });

constexpr static inline auto dot_output_dyn_index =
  std::to_array<unsigned char>({
    1, // qss1_integrator,
    1, // qss1_multiplier,
    3, // qss1_cross,
    1, // qss1_flipflop,
    2, // qss1_filter,
    1, // qss1_power,
    1, // qss1_square,
    1, // qss1_sum_2,
    1, // qss1_sum_3,
    1, // qss1_sum_4,
    1, // qss1_wsum_2,
    1, // qss1_wsum_3,
    1, // qss1_wsum_4,
    1, // qss1_inverse,
    1, // qss1_integer,
    1, // qss1_compare
    1, // qss1_gain,
    1, // qss1_sin
    1, // qss1_cos
    1, // qss1_log
    1, // qss1_exp
    1, // qss2_integrator,
    1, // qss2_multiplier,
    3, // qss2_cross,
    1, // qss2_flipflop,
    2, // qss2_filter,
    1, // qss2_power,
    1, // qss2_square,
    1, // qss2_sum_2,
    1, // qss2_sum_3,
    1, // qss2_sum_4,
    1, // qss2_wsum_2,
    1, // qss2_wsum_3,
    1, // qss2_wsum_4,
    1, // qss2_inverse,
    1, // qss2_integer,
    1, // qss2_compare
    1, // qss2_gain,
    1, // qss2_sin
    1, // qss2_cos
    1, // qss2_log
    1, // qss2_exp
    1, // qss3_integrator,
    1, // qss3_multiplier,
    3, // qss3_cross,
    1, // qss3_flipflop,
    2, // qss3_filter,
    1, // qss3_power,
    1, // qss3_square,
    1, // qss3_sum_2,
    1, // qss3_sum_3,
    1, // qss3_sum_4,
    1, // qss3_wsum_2,
    1, // qss3_wsum_3,
    1, // qss3_wsum_4,
    1, // qss3_inverse,
    1, // qss3_integer,
    1, // qss3_compare
    1, // qss3_gain,
    1, // qss3_sin
    1, // qss3_cos
    1, // qss3_log
    1, // qss3_exp
    1, // counter,
    1, // queue,
    1, // dynamic_queue,
    1, // priority_queue,
    1, // generator,
    1, // constant,
    1, // time_func,
    0, // accumulator_2,
    1, // logical_and_2,
    1, // logical_and_3,
    1, // logical_or_2,
    1, // logical_or_3,
    1, // logical_invert,
    2, // hsm_wrapper
  });

inline constexpr std::span<const std::string_view> get_output_port_names(
  const std::pair<unsigned char, unsigned char> index_size,
  const std::span<const std::string_view>       names = output_names) noexcept
{
    return std::span<const std::string_view>(names.data() + index_size.first,
                                             index_size.second);
}

inline constexpr std::span<const std::string_view> get_output_port_names(
  const dynamics_type                     type,
  const std::span<const std::string_view> names = output_names) noexcept
{
    return get_output_port_names(
      dot_output_index[dot_output_dyn_index[ordinal(type)]], names);
}

template<typename RawDynamics>
constexpr std::span<const std::string_view> get_output_port_names(
  const std::span<const std::string_view> names = output_names) noexcept
{
    using Dynamics = std::decay_t<RawDynamics>;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return get_output_port_names(dot_output_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss1_flipflop>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_filter>)
        return get_output_port_names(dot_output_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_power>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_square>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_inverse>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_integer>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_compare>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_gain>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sin>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_cos>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_log>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_exp>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_cross>)
        return get_output_port_names(dot_output_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss2_flipflop>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_filter>)
        return get_output_port_names(dot_output_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_power>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_square>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_inverse>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_integer>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_compare>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_gain>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sin>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_cos>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_log>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_exp>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_integrator>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_multiplier>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_cross>)
        return get_output_port_names(dot_output_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss3_flipflop>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_filter>)
        return get_output_port_names(dot_output_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_power>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_square>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_4>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_inverse>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_integer>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_compare>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_gain>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sin>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_cos>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_log>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_exp>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, counter>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, queue>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, dynamic_queue>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, priority_queue>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, generator>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, constant>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, time_func>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return get_output_port_names(dot_output_index[0], names);
    if constexpr (std::is_same_v<Dynamics, logical_and_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, logical_and_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, logical_or_2>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, logical_or_3>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, logical_invert>)
        return get_output_port_names(dot_output_index[1], names);
    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return get_output_port_names(dot_output_index[2], names);

    unreachable();
}

constexpr static inline auto dot_input_names =
  std::to_array<std::string_view>({ "x_dot",
                                    "reset",
                                    "in",
                                    "in_1",
                                    "in_2",
                                    "in_3",
                                    "in_4",
                                    "in",
                                    "threshold",
                                    "in_1",
                                    "in_2",
                                    "nb_1",
                                    "nb_2",
                                    "value",
                                    "set_t",
                                    "add_tr",
                                    "mult_tr",
                                    "in",
                                    "event" });

constexpr static inline auto input_names =
  std::to_array<std::string_view>({ "x-dot",
                                    "reset",
                                    "in",
                                    "in-1",
                                    "in-2",
                                    "in-3",
                                    "in-4",
                                    "in",
                                    "threshold",
                                    "in-1",
                                    "in-2",
                                    "nb-1",
                                    "nb-2",
                                    "value",
                                    "set-t",
                                    "add-tr",
                                    "mult-tr",
                                    "in",
                                    "event" });

constexpr static inline auto dot_input_index =
  std::to_array<std::pair<unsigned char, unsigned char>>(
    { { 0, 0 },   // 0. empty
      { 0, 2 },   // 1. integrator
      { 2, 1 },   // 2. in
      { 3, 2 },   // 3. in-[1,2]
      { 3, 3 },   // 4. in-[1,..,3]
      { 3, 4 },   // 5. in-[1,..,4]
      { 7, 2 },   // 6. in..threshold
      { 9, 4 },   // 7. in-1..nb-2
      { 13, 4 },  // 8. value..mult-tr
      { 17, 2 } } // 9. in..event
  );

constexpr static inline auto dot_input_dyn_index =
  std::to_array<unsigned char>({
    1, // qss1_integrator,
    3, // qss1_multiplier,
    6, // qss1_cross,
    9, // qss1_flipflop,
    2, // qss1_filter,
    2, // qss1_power,
    2, // qss1_square,
    3, // qss1_sum_2,
    4, // qss1_sum_3,
    5, // qss1_sum_4,
    3, // qss1_wsum_2,
    4, // qss1_wsum_3,
    5, // qss1_wsum_4,
    2, // qss1_inverse,
    2, // qss1_integer,
    3, // qss1_compare
    2, // qss1_gain,
    2, // qss1_sin
    2, // qss1_cos
    2, // qss1_log
    2, // qss1_exp
    1, // qss2_integrator,
    3, // qss2_multiplier,
    6, // qss2_cross,
    9, // qss2_flipflop,
    2, // qss2_filter,
    2, // qss2_power,
    2, // qss2_square,
    3, // qss2_sum_2,
    4, // qss2_sum_3,
    5, // qss2_sum_4,
    3, // qss2_wsum_2,
    4, // qss2_wsum_3,
    5, // qss2_wsum_4,
    2, // qss2_inverse,
    2, // qss2_integer,
    3, // qss2_compare
    2, // qss2_gain,
    2, // qss2_sin
    2, // qss2_cos
    2, // qss2_log
    2, // qss2_exp
    1, // qss3_integrator,
    3, // qss3_multiplier,
    6, // qss3_cross,
    9, // qss3_flipflop,
    2, // qss3_filter,
    2, // qss3_power,
    2, // qss3_square,
    3, // qss3_sum_2,
    4, // qss3_sum_3,
    5, // qss3_sum_4,
    3, // qss3_wsum_2,
    4, // qss3_wsum_3,
    5, // qss3_wsum_4,
    2, // qss3_inverse,
    2, // qss3_integer,
    3, // qss3_compare
    2, // qss3_gain,
    2, // qss3_sin
    2, // qss3_cos
    2, // qss3_log
    2, // qss3_exp
    2, // counter,
    2, // queue,
    2, // dynamic_queue,
    2, // priority_queue,
    8, // generator,
    0, // constant,
    0, // time_func,
    7, // accumulator_2,
    3, // logical_and_2,
    4, // logical_and_3,
    3, // logical_or_2,
    4, // logical_or_3,
    2, // logical_invert,
    5, // hsm_wrapper
  });

inline constexpr std::span<const std::string_view> get_input_port_names(
  const std::pair<unsigned char, unsigned char> index_size,
  const std::span<const std::string_view>       names = input_names) noexcept
{
    return std::span<const std::string_view>(names.data() + index_size.first,
                                             index_size.second);
}

inline constexpr std::span<const std::string_view> get_input_port_names(
  const dynamics_type                     type,
  const std::span<const std::string_view> names = input_names) noexcept
{
    return get_input_port_names(
      dot_input_index[dot_input_dyn_index[ordinal(type)]], names);
}

template<typename RawDynamics>
constexpr std::span<const std::string_view> get_input_port_names(
  std::span<const std::string_view> names = input_names) noexcept
{
    using Dynamics = std::decay_t<RawDynamics>;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return get_input_port_names(dot_input_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return get_input_port_names(dot_input_index[6], names);
    if constexpr (std::is_same_v<Dynamics, qss1_flipflop>)
        return get_input_port_names(dot_input_index[9], names);
    if constexpr (std::is_same_v<Dynamics, qss1_filter>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_power>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_square>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss1_inverse>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_integer>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_compare>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss1_gain>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_sin>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_cos>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_log>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss1_exp>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
        return get_input_port_names(dot_input_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss2_cross>)
        return get_input_port_names(dot_input_index[6], names);
    if constexpr (std::is_same_v<Dynamics, qss2_flipflop>)
        return get_input_port_names(dot_input_index[9], names);
    if constexpr (std::is_same_v<Dynamics, qss2_filter>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_power>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_square>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss2_inverse>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_integer>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_compare>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss2_gain>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_sin>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_cos>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_log>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss2_exp>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_integrator>)
        return get_input_port_names(dot_input_index[1], names);
    if constexpr (std::is_same_v<Dynamics, qss3_multiplier>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss3_cross>)
        return get_input_port_names(dot_input_index[6], names);
    if constexpr (std::is_same_v<Dynamics, qss3_flipflop>)
        return get_input_port_names(dot_input_index[9], names);
    if constexpr (std::is_same_v<Dynamics, qss3_filter>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_power>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_square>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_4>)
        return get_input_port_names(dot_input_index[5], names);
    if constexpr (std::is_same_v<Dynamics, qss3_inverse>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_integer>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_compare>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, qss3_gain>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_sin>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_cos>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_log>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, qss3_exp>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, counter>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, queue>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, dynamic_queue>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, priority_queue>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, generator>)
        return get_input_port_names(dot_input_index[8], names);
    if constexpr (std::is_same_v<Dynamics, constant>)
        return get_input_port_names(dot_input_index[0], names);
    if constexpr (std::is_same_v<Dynamics, time_func>)
        return get_input_port_names(dot_input_index[0], names);
    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return get_input_port_names(dot_input_index[7], names);
    if constexpr (std::is_same_v<Dynamics, logical_and_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, logical_and_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, logical_or_2>)
        return get_input_port_names(dot_input_index[3], names);
    if constexpr (std::is_same_v<Dynamics, logical_or_3>)
        return get_input_port_names(dot_input_index[4], names);
    if constexpr (std::is_same_v<Dynamics, logical_invert>)
        return get_input_port_names(dot_input_index[2], names);
    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return get_input_port_names(dot_input_index[5], names);

    unreachable();
}

/// Write the DEVS graph simulation into the @a os stream.
///
/// The directed graph writes nodes only with integer index (not string) and the
/// edges by theirs names.
void write_dot_graph_simulation(std::ostream&     os,
                                const simulation& sim) noexcept;

enum class write_test_simulation_options : u8 {
    none,        //!< Write only the structure of the simulation.
    test_finish, //!< Add constant number and last-value tests at end of the
                 //!< simulation.
    test_trace,  //!< Add constant number and last-value tests during the
                 //!< simulation.
};

enum class write_test_simulation_result : u8 {
    success,
    output_error,          //!< Output stream internal error (not writable).
    external_source_error, //!< Can not convert binary/text files or random
                           //!< sources. Prefer using constant external source.
    hsm_error,             //!< Missing HSM definition in simulation.
};

/// Write a C++ code from the simulation into @a os output stream.
///
/// The simulation is written in a way that it can be used as a unit test and
/// can be directly store into unit-test.
/// @param os
/// @param sim
/// @param begin
/// @param end
/// @param opts
auto write_test_simulation(std::ostream&                       os,
                           const std::string_view              name,
                           const simulation&                   sim,
                           const time                          begin,
                           const time                          end,
                           const write_test_simulation_options opts) noexcept
  -> write_test_simulation_result;

} // namespace irt

#endif
