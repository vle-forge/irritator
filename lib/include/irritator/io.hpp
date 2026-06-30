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
    "modified",
    "unmodified",
    "unreadable"
};

static constexpr inline const char* component_type_names[] = {
    "none", "simple", "grid", "graph", "hsm", "simulation"
};

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
    "qss1_integrator",
    "qss1_multiplier",
    "qss1_cross",
    "qss1_max_hold",
    "qss1_min_hold",
    "qss1_flipflop",
    "qss1_filter",
    "qss1_power",
    "qss1_square",
    "qss1_sum_2",
    "qss1_sum_3",
    "qss1_sum_4",
    "qss1_wsum_2",
    "qss1_wsum_3",
    "qss1_wsum_4",
    "qss1_inverse",
    "qss1_integer",
    "qss1_compare",
    "qss1_gain",
    "qss1_sin",
    "qss1_cos",
    "qss1_log",
    "qss1_exp",
    "qss1_sample_hold",
    "qss1_quantizer",
    "qss1_integrate_and_fire",
    "qss1_threshold_crossing",
    "qss1_pwm",
    "qss1_abs",
    "qss1_atan",
    "qss1_atan2",
    "qss1_dead_zone",
    "qss1_division",
    "qss1_hysteresis",
    "qss1_maximum",
    "qss1_minimum",
    "qss1_saturation",
    "qss1_sigmoid",
    "qss1_sign",
    "qss1_sqrt",
    "qss1_tan",
    "qss1_tanh",
    "qss1_wrap",
    "qss2_integrator",
    "qss2_multiplier",
    "qss2_cross",
    "qss2_max_hold",
    "qss2_min_hold",
    "qss2_flipflop",
    "qss2_filter",
    "qss2_power",
    "qss2_square",
    "qss2_sum_2",
    "qss2_sum_3",
    "qss2_sum_4",
    "qss2_wsum_2",
    "qss2_wsum_3",
    "qss2_wsum_4",
    "qss2_inverse",
    "qss2_integer",
    "qss2_compare",
    "qss2_gain",
    "qss2_sin",
    "qss2_cos",
    "qss2_log",
    "qss2_exp",
    "qss2_sample_hold",
    "qss2_quantizer",
    "qss2_integrate_and_fire",
    "qss2_threshold_crossing",
    "qss2_pwm",
    "qss2_abs",
    "qss2_atan",
    "qss2_atan2",
    "qss2_dead_zone",
    "qss2_division",
    "qss2_hysteresis",
    "qss2_maximum",
    "qss2_minimum",
    "qss2_saturation",
    "qss2_sigmoid",
    "qss2_sign",
    "qss2_sqrt",
    "qss2_tan",
    "qss2_tanh",
    "qss2_wrap",
    "qss3_integrator",
    "qss3_multiplier",
    "qss3_cross",
    "qss3_max_hold",
    "qss3_min_hold",
    "qss3_flipflop",
    "qss3_filter",
    "qss3_power",
    "qss3_square",
    "qss3_sum_2",
    "qss3_sum_3",
    "qss3_sum_4",
    "qss3_wsum_2",
    "qss3_wsum_3",
    "qss3_wsum_4",
    "qss3_inverse",
    "qss3_integer",
    "qss3_compare",
    "qss3_gain",
    "qss3_sin",
    "qss3_cos",
    "qss3_log",
    "qss3_exp",
    "qss3_sample_hold",
    "qss3_quantizer",
    "qss3_integrate_and_fire",
    "qss3_threshold_crossing",
    "qss3_pwm",
    "qss3_abs",
    "qss3_atan",
    "qss3_atan2",
    "qss3_dead_zone",
    "qss3_division",
    "qss3_hysteresis",
    "qss3_maximum",
    "qss3_minimum",
    "qss3_saturation",
    "qss3_sigmoid",
    "qss3_sign",
    "qss3_sqrt",
    "qss3_tan",
    "qss3_tanh",
    "qss3_wrap",
    "counter",
    "queue",
    "dynamic_queue",
    "priority_queue",
    "generator",
    "constant",
    "time_func",
    "accumulator_2",
    "logical_and_2",
    "logical_and_3",
    "logical_or_2",
    "logical_or_3",
    "logical_invert",
    "hsm_wrapper",
    "simulation_wrapper",
    "zero_order_hold",
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

static constexpr inline std::string_view
  optimization_method_names[] = { "weighted_sum" /*, "epsilon_constrained"*/ };

static constexpr inline std::string_view optimization_type_names[] = {
    "maximize",
    "minimize"
};

static constexpr inline std::string_view norm_type_names[] = { "min-max",
                                                               "z-score" };

template<typename RandomGenerator, typename Distribution>
inline int generate_random_file(std::FILE*             os,
                                RandomGenerator&       gen,
                                Distribution&          dist,
                                const std::size_t      size,
                                const random_file_type type) noexcept
{
    using result_type = typename Distribution::result_type;

    switch (type) {
    case random_file_type::text: {
        if (not os)
            return -1;

        if constexpr (std::is_integral_v<result_type>) {
            for (std::size_t sz = 0; sz < size; ++sz)
                std::fprintf(os, "%d\n", dist(gen));
        } else if constexpr (std::is_floating_point_v<result_type>) {
            for (std::size_t sz = 0; sz < size; ++sz)
                std::fprintf(os, "%.10f\n", dist(gen));
        }
    } break;

    case random_file_type::binary: {
        if (not os)
            return -1;

        for (std::size_t sz = 0; sz < size; ++sz) {
            const auto value = dist(gen);
            std::fwrite(
              reinterpret_cast<const char*>(&value), 1, sizeof(value), os);
        }
    } break;
    }

    return 0;
}

/// Write the DEVS graph simulation into the @a os stream.
///
/// The directed graph writes nodes only with integer index (not string) and the
/// edges by theirs names.
void write_dot_graph_simulation(std::FILE* os, const simulation& sim) noexcept;

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
auto write_test_simulation(std::FILE*                          os,
                           const std::string_view              name,
                           const simulation&                   sim,
                           const time                          begin,
                           const time                          end,
                           const write_test_simulation_options opts) noexcept
  -> write_test_simulation_result;

} // namespace irt

#endif
