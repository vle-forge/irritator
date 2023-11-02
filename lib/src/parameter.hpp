// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_SRC_PARAMETER_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_SRC_PARAMETER_HPP

#include <irritator/core.hpp>

#include <limits>

#include <cmath>

namespace irt {

template<int QssLevel>
struct abstract_integrator_parameters
{
    real X;
    real dQ;

    void correct() noexcept
    {
        X  = std::isfinite(X) ? X : 0.0;
        dQ = std::isnormal(dQ) ? dQ : 0.001;
    }
};

using qss1_integrator_parameters = abstract_integrator_parameters<1>;
using qss2_integrator_parameters = abstract_integrator_parameters<2>;
using qss3_integrator_parameters = abstract_integrator_parameters<3>;

template<int QssLevel>
struct abstract_power_parameters
{
    real n;

    void correct() noexcept { n = std::isnormal(n) ? n : 1.0; }
};

using qss1_power_parameters = abstract_power_parameters<1>;
using qss2_power_parameters = abstract_power_parameters<2>;
using qss3_power_parameters = abstract_power_parameters<3>;

template<int QssLevel>
struct abstract_square_parameters
{};

using qss1_square_parameters = abstract_square_parameters<1>;
using qss2_square_parameters = abstract_square_parameters<2>;
using qss3_square_parameters = abstract_square_parameters<3>;

template<int QssLevel, int PortNumber>
struct abstract_sum_parameters
{};

using qss1_sum_2_parameters = abstract_sum_parameters<1, 2>;
using qss2_sum_2_parameters = abstract_sum_parameters<2, 2>;
using qss3_sum_2_parameters = abstract_sum_parameters<3, 2>;
using qss1_sum_3_parameters = abstract_sum_parameters<1, 3>;
using qss2_sum_3_parameters = abstract_sum_parameters<2, 3>;
using qss3_sum_3_parameters = abstract_sum_parameters<3, 3>;
using qss1_sum_4_parameters = abstract_sum_parameters<1, 4>;
using qss2_sum_4_parameters = abstract_sum_parameters<2, 4>;
using qss3_sum_4_parameters = abstract_sum_parameters<3, 4>;

template<int QssLevel, int PortNumber>
struct abstract_wsum_parameters
{
    real coeffs[PortNumber];
};

using qss1_wsum_2_parameters = abstract_wsum_parameters<1, 2>;
using qss2_wsum_2_parameters = abstract_wsum_parameters<2, 2>;
using qss3_wsum_2_parameters = abstract_wsum_parameters<3, 2>;
using qss1_wsum_3_parameters = abstract_wsum_parameters<1, 3>;
using qss2_wsum_3_parameters = abstract_wsum_parameters<2, 3>;
using qss3_wsum_3_parameters = abstract_wsum_parameters<3, 3>;
using qss1_wsum_4_parameters = abstract_wsum_parameters<1, 4>;
using qss2_wsum_4_parameters = abstract_wsum_parameters<2, 4>;
using qss3_wsum_4_parameters = abstract_wsum_parameters<3, 4>;

template<int QssLevel>
struct abstract_multiplier_parameters
{};

using qss1_multiplier_parameters = abstract_multiplier_parameters<1>;
using qss2_multiplier_parameters = abstract_multiplier_parameters<2>;
using qss3_multiplier_parameters = abstract_multiplier_parameters<3>;

template<int QssLevel>
struct abstract_filter_parameters
{
    real lower_threshold = -std::numeric_limits<real>::infinity();
    real upper_threshold = +std::numeric_limits<real>::infinity();

    void correct() noexcept
    {
        if (lower_threshold > upper_threshold)
            std::swap(lower_threshold, upper_threshold);

        lower_threshold = std::isnan(lower_threshold)
                            ? -std::numeric_limits<real>::infinity()
                            : lower_threshold;

        upper_threshold = std::isnan(upper_threshold)
                            ? -std::numeric_limits<real>::infinity()
                            : upper_threshold;
    }
};

using qss1_filter_parameters = abstract_filter_parameters<1>;
using qss2_filter_parameters = abstract_filter_parameters<2>;
using qss3_filter_parameters = abstract_filter_parameters<3>;

template<int QssLevel>
struct abstract_cross_parameters
{
    real threshold = zero;
    bool detect_up = true;

    void correct() noexcept
    {
        threshold = std::isfinite(threshold) ? threshold : 0.0;
    }
};

using qss1_cross_parameters = abstract_cross_parameters<1>;
using qss2_cross_parameters = abstract_cross_parameters<2>;
using qss3_cross_parameters = abstract_cross_parameters<3>;

struct aqss_integrator_parameters
{
    real value = 0.0;
    real reset = 0.0;

    void correct() noexcept
    {
        value = std::isfinite(value) ? value : 0.0;
        reset = std::isfinite(reset) ? reset : 0.0;
    }
};

struct aqss_quantifier_parameters
{
    real                    step_size   = real(0.001);
    i32                     past_length = 3;
    quantifier::adapt_state adapt_state = quantifier::adapt_state::possible;
    bool                    zero_init_offset = false;

    void correct() noexcept
    {
        step_size   = step_size > 0 ? step_size : 0.001;
        past_length = past_length > 1 ? past_length : 3;
    }
};

template<int PortNumber>
struct aqss_adder_parameters
{
    real values[PortNumber];
    real coeffs[PortNumber];

    void correct() noexcept
    {
        for (auto& v : values)
            v = std::isfinite(v) ? v : 0.0;

        for (auto& c : coeffs)
            c = std::isfinite(c) ? c : 1.0;
    }
};

using aqss_adder_2_parameters = aqss_adder_parameters<2>;
using aqss_adder_3_parameters = aqss_adder_parameters<3>;
using aqss_adder_4_parameters = aqss_adder_parameters<4>;

template<int PortNumber>
struct aqss_mult_parameters
{
    real values[PortNumber];
    real coeffs[PortNumber];

    void correct() noexcept
    {
        for (auto& v : values)
            v = std::isfinite(v) ? v : 0.0;

        for (auto& c : coeffs)
            c = std::isfinite(c) ? c : 1.0;
    }
};

using aqss_mult_2_parameters = aqss_mult_parameters<2>;
using aqss_mult_3_parameters = aqss_mult_parameters<3>;
using aqss_mult_4_parameters = aqss_mult_parameters<4>;

struct aqss_filter_parameters
{
    real lower_threshold = -std::numeric_limits<real>::infinity();
    real upper_threshold = +std::numeric_limits<real>::infinity();

    void correct() noexcept
    {
        if (lower_threshold > upper_threshold)
            std::swap(lower_threshold, upper_threshold);

        lower_threshold = std::isnan(lower_threshold)
                            ? -std::numeric_limits<real>::infinity()
                            : lower_threshold;

        upper_threshold = std::isnan(upper_threshold)
                            ? -std::numeric_limits<real>::infinity()
                            : upper_threshold;
    }
};

struct aqss_cross_parameters
{
    real threshold = 0.0;

    void correct() noexcept
    {
        threshold = std::isfinite(threshold) ? threshold : 0.0;
    }
};

template<typename AbstractLogicalTester, int PortNumber>
struct abstract_logical_parameters
{
    bool default_values[PortNumber];
};

using logical_and_2_parameters =
  abstract_logical_parameters<abstract_and_check, 2>;
using logical_and_3_parameters =
  abstract_logical_parameters<abstract_and_check, 3>;
using logical_or_2_parameters =
  abstract_logical_parameters<abstract_or_check, 2>;
using logical_or_3_parameters =
  abstract_logical_parameters<abstract_or_check, 3>;

struct logical_invert_parameters
{
    bool value = false;
};

template<int PortNumber>
struct accumulator_parameters
{};

using accumulator_2_parameters = accumulator_parameters<2>;

struct time_func_parameters
{
    real offset = 0.0;
    i32  f      = 0;

    void correct() noexcept
    {
        offset = std::isfinite(offset) ? offset : 0.0;
        f      = std::clamp(f, 0, 3);
    }
};

struct counter_parameters
{};

struct generator_parameters
{
    real   offset = 0.0;
    source source_ta;
    source source_value;
    bool   stop_on_error = false;

    void correct() noexcept { offset = std::isfinite(offset) ? offset : 0.0; }
};

struct queue_parameters
{
    real ta = 1.0;

    void correct() noexcept { ta = ta > 0 ? ta : 1.0; }
};

struct dynamic_queue_parameters
{
    source source_ta;
    bool   stop_on_error = false;
};

struct priority_queue_parameters
{
    real   ta = 1.0;
    source source_ta;
    bool   stop_on_error = false;

    void correct() noexcept { ta = ta > 0 ? ta : 1.0; }
};

struct constant_parameters
{
    real value  = 0.0;
    real offset = 0.0;

    void correct() noexcept
    {
        value  = std::isfinite(value) ? value : 0.0;
        offset = std::isfinite(offset) ? offset : 0.0;
    }
};

struct hsm_wapper_parameters
{
    i32 a  = 0;
    i32 b  = 0;
    u64 id = 0u;
};

enum class generator_parameter_indices
{
    stop_on_error = 0,
    ta_id,
    ta_type,
    value_id,
    value_type
};

enum class abstract_wsum_parameter_indices
{
    coeffs_0,
    coeffs_1,
    coeffs_2,
    coeffs_3,
};

enum class abstract_cross_parameter_indices
{
    threshold,
    detect_up,
};

enum class abstract_filter_parameter_indices
{
    lower_threshold,
    upper_threshold,
};

enum class abstract_power_parameter_indices
{
    n,
};

enum class abstract_logical_parameter_indices
{
    value_0,
    value_1,
    value_2
};

enum class hsm_wrapper_parameter_indices
{
    a,
    b
};

enum class time_func_parameter_indices
{
    f
};

enum class dynamic_queue_parameter_indices
{
    ta_id = 1,
    ta_type
};

enum class priority_queue_parameter_indices
{
    ta_id = 1,
    ta_type
};

} // namespace irt

#endif