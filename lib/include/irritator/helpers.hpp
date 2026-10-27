// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_HELPERS_2023
#define ORG_VLEPROJECT_IRRITATOR_HELPERS_2023

#include <irritator/core.hpp>
#include <irritator/error.hpp>

#include <concepts>
#include <type_traits>

namespace irt {

enum port_type : u8 {
    qss,     //<! QSS port type with 2, 3 or 4 reals.
    scalar,  //<! Only one real
    boolean, //<! Only one boolean (0.0 or 1.0)
    ping,    //<! Just the event no data read
};

template<typename T>
concept have_output_port_type = requires(const T) {
    { T::y_names };
};

template<typename T>
concept have_input_port_type = requires(const T) {
    { T::x_names };
};

/**
 *  +-----------------------------------------------+
 *  |        |   qss   |  scalar | boolean |  ping  |
 *  |--------+---------+---------+---------+--------|
 *  |    qss |    v    |   (1)   |   (2)   |   v    |
 *  | scalar |   (3)   |    v    |   (2)   |   v    |
 *  |boolean |   (4)   |   (5)   |    v    |   v    |
 *  |   ping |    x    |    x    |    x    |   v    |
 *  +-----------------------------------------------+
 *
 *  (1) scalar equals message[0]
 *  (2) boolean equals TRUE if message[0] > 0 FALSE otherwise
 *  (3) qss x[0] = message[0] and x[1], x[2] = 0.0
 *  (4) qss x[0] = ONE if message[0] is TRUE, 0 otherwise
 *  (5) scalar equals ONE if message si TRUE, 0 otherwise
 */
static constexpr bool validity[16] = { true,  true,  true,  true, true, true,
                                       true,  true,  true,  true, true, true,
                                       false, false, false, true };

struct qss_integrator_tag {
    static constexpr std::string_view x_names[] = { "X_dot", "reset" };
    static constexpr std::string_view y_names[] = { "out" };
    static constexpr const port_type  x_types[] = { qss, scalar };
    static constexpr const port_type  y_types[] = { qss };

    enum parameter_names : u8 { X = 0, dQ };
};

struct qss_cross_tag {
    static constexpr std::string_view x_names[] = { "in", "threshold" };
    static constexpr std::string_view y_names[] = { "up", "down" };

    static constexpr const port_type x_types[] = { qss, scalar };
    static constexpr const port_type y_types[] = { scalar, scalar };

    enum parameter_names : u8 { threshold, up_value, bottom_value };
};

struct qss_min_hold_tag {
    static constexpr std::string_view x_names[] = { "in", "threshold" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, scalar };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_max_hold_tag {
    static constexpr std::string_view x_names[] = { "in", "threshold" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, scalar };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_multiplier_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_flipflop_tag {
    static constexpr std::string_view x_names[] = { "in", "event" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, ping };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_filter_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out", "up", "down" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss, scalar, scalar };

    enum parameter_names : u8 { lower_bound = 0, upper_bound };
};

struct qss_power_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { exponent = 0 };
};

struct qss_square_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_sum_2_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_sum_3_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_sum_4_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3",
                                                    "in-4" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss, qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_wsum_2_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { coeff1 = 0, coeff2 };
};

struct qss_wsum_3_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss, qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { coeff1 = 0, coeff2, coeff3 };
};

struct qss_wsum_4_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3",
                                                    "in-4" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss, qss, qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { coeff1 = 0, coeff2, coeff3, coeff4 };
};

struct qss_inverse_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_integer_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_compare_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { scalar };

    enum parameter_names : u8 { equal = 0, not_equal };
};

struct qss_gain_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { k = 0 };
};

struct qss_sin_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_cos_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_exp_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct qss_log_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct counter_tag {
    static constexpr std::string_view x_names[] = { "in" };

    static constexpr const port_type x_types[] = { scalar };

    enum parameter_names : u8 { i_obs_type = 0 };
};

struct queue_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { sigma = 0 };
};

struct dynamic_queue_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { source_ta = 0 };
};

struct priority_queue_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { sigma = 0, source_ta = 0 };
};

struct generator_tag {
    static constexpr std::string_view x_names[] = { "value", "t", "+", "x" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { scalar, scalar, scalar,
                                                   scalar };
    static constexpr const port_type y_types[] = { scalar };

    enum parameter_names : u8 {
        i_options = 0,
        source_ta,
        source_value,
        sigma = 0,
        value
    };
};

struct constant_tag {
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type y_types[] = { scalar };

    enum parameter_names : u8 { value = 0, offset, i_type = 0, i_port };
};

struct time_func_tag {
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type y_types[] = { scalar };

    enum parameter_names : u8 { offset = 0, timestep, i_type = 0 };
};

struct accumulator_2_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };

    static constexpr const port_type x_types[] = { scalar, scalar };
};

struct logical_and_2_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { boolean, boolean };
    static constexpr const port_type y_types[] = { boolean };
};

struct logical_and_3_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { boolean, boolean, boolean };
    static constexpr const port_type y_types[] = { boolean };
};

struct logical_or_2_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { boolean, boolean };
    static constexpr const port_type y_types[] = { boolean };
};

struct logical_or_3_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { boolean, boolean, boolean };
    static constexpr const port_type y_types[] = { boolean };
};

struct logical_invert_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { boolean };
    static constexpr const port_type y_types[] = { boolean };
};

struct hsm_wrapper_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2", "in-3",
                                                    "in-4" };
    static constexpr std::string_view y_names[] = { "out-1", "out-2", "out-3",
                                                    "out-4" };

    static constexpr const port_type x_types[] = { qss, qss, qss, qss };
    static constexpr const port_type y_types[] = { qss, qss, qss, qss };

    enum parameter_names : u8 {
        r1 = 0,
        r2,
        timer,
        id = 0,
        i1,
        i2,
        source_value
    };
};

struct simulation_wrapper_tag {
    static constexpr std::string_view x_names[] = { "init", "run" };

    static constexpr const port_type x_types[] = { qss, qss };

    enum parameter_names : u8 {
        run = 0,
        id,
        factors,
        selections,
    };
};

struct sample_hold_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { ts = 0 };
};

struct zero_order_hold_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct quantizer_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { step = 0 };
};

struct integrate_and_fire_tag {
    static constexpr std::string_view x_names[] = { "in", "reset" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { threshold = 0 };
};

struct threshold_crossing_tag {
    static constexpr std::string_view x_names[] = { "in", "threshold" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { level = 0 };
};

struct pwm_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { period = 0, amplitude };
};

struct sqrt_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct atan_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct tan_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct tanh_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct sigmoid_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct division_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct atan2_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct abs_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct sign_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };
};

struct minimum_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct maximum_tag {
    static constexpr std::string_view x_names[] = { "in-1", "in-2" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss, qss };
    static constexpr const port_type y_types[] = { qss };
};

struct saturation_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { lower = 0, upper };
};

struct dead_zone_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { lower = 0, upper };
};

struct hysteresis_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { lower = 0, upper, out_low, out_high };
};

struct wrap_tag {
    static constexpr std::string_view x_names[] = { "in" };
    static constexpr std::string_view y_names[] = { "out" };

    static constexpr const port_type x_types[] = { qss };
    static constexpr const port_type y_types[] = { qss };

    enum parameter_names : u8 { origin = 0, modulo };
};

/** Dispatch the callable @c f with @c args argument according to @c type.
 *
 * This function is useful to: (1) avoid using dynamic polymorphism (i.e.,
 * virtual) based on the @c dynamics_type variables and to (2) provide the
 * same source code for same dynamics type like abstract classes.
 *
 * @param type
 * @param f
 * @param args
 *
 * @verbatim
 * dispatch(mdl.type, []<typename Tag>(
 *     const Tag t, const float x, const float y) -> bool {
 *         if constexpr(std::is_same_v(t, hsm_wrapper_tag)) {
 *             // todo
 *         }
 *     }));
 * @endverbatim
 */
template<typename Function, typename... Args>
constexpr auto dispatch(const dynamics_type type,
                        Function&&          f,
                        Args&&... args) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss3_integrator:
        return std::invoke(std::forward<Function>(f), qss_integrator_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss3_multiplier:
        return std::invoke(std::forward<Function>(f), qss_multiplier_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
        return std::invoke(std::forward<Function>(f), qss_cross_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_max_hold:
    case dynamics_type::qss2_max_hold:
    case dynamics_type::qss3_max_hold:
        return std::invoke(std::forward<Function>(f), qss_max_hold_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_min_hold:
    case dynamics_type::qss2_min_hold:
    case dynamics_type::qss3_min_hold:
        return std::invoke(std::forward<Function>(f), qss_min_hold_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_flipflop:
    case dynamics_type::qss2_flipflop:
    case dynamics_type::qss3_flipflop:
        return std::invoke(std::forward<Function>(f), qss_flipflop_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_filter:
    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
        return std::invoke(std::forward<Function>(f), qss_filter_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_power:
    case dynamics_type::qss2_power:
    case dynamics_type::qss3_power:
        return std::invoke(std::forward<Function>(f), qss_power_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_square:
    case dynamics_type::qss2_square:
    case dynamics_type::qss3_square:
        return std::invoke(std::forward<Function>(f), qss_square_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss3_sum_2:
        return std::invoke(std::forward<Function>(f), qss_sum_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss3_sum_3:
        return std::invoke(std::forward<Function>(f), qss_sum_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss3_sum_4:
        return std::invoke(std::forward<Function>(f), qss_sum_4_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss3_wsum_2:
        return std::invoke(std::forward<Function>(f), qss_wsum_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_wsum_3:
        return std::invoke(std::forward<Function>(f), qss_wsum_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_wsum_4:
        return std::invoke(std::forward<Function>(f), qss_wsum_4_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_inverse:
    case dynamics_type::qss2_inverse:
    case dynamics_type::qss3_inverse:
        return std::invoke(std::forward<Function>(f), qss_inverse_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_integer:
    case dynamics_type::qss2_integer:
    case dynamics_type::qss3_integer:
        return std::invoke(std::forward<Function>(f), qss_integer_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_compare:
    case dynamics_type::qss2_compare:
    case dynamics_type::qss3_compare:
        return std::invoke(std::forward<Function>(f), qss_compare_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_gain:
    case dynamics_type::qss2_gain:
    case dynamics_type::qss3_gain:
        return std::invoke(std::forward<Function>(f), qss_gain_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_sin:
    case dynamics_type::qss2_sin:
    case dynamics_type::qss3_sin:
        return std::invoke(std::forward<Function>(f), qss_sin_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_cos:
    case dynamics_type::qss2_cos:
    case dynamics_type::qss3_cos:
        return std::invoke(std::forward<Function>(f), qss_cos_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_log:
    case dynamics_type::qss2_log:
    case dynamics_type::qss3_log:
        return std::invoke(std::forward<Function>(f), qss_log_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_exp:
    case dynamics_type::qss2_exp:
    case dynamics_type::qss3_exp:
        return std::invoke(std::forward<Function>(f), qss_exp_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::counter:
        return std::invoke(std::forward<Function>(f), counter_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::queue:
        return std::invoke(std::forward<Function>(f), queue_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::dynamic_queue:
        return std::invoke(std::forward<Function>(f), dynamic_queue_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::priority_queue:
        return std::invoke(std::forward<Function>(f), priority_queue_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::generator:
        return std::invoke(std::forward<Function>(f), generator_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::constant:
        return std::invoke(std::forward<Function>(f), constant_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::accumulator_2:
        return std::invoke(std::forward<Function>(f), accumulator_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::time_func:
        return std::invoke(std::forward<Function>(f), time_func_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::logical_and_2:
        return std::invoke(std::forward<Function>(f), logical_and_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_and_3:
        return std::invoke(std::forward<Function>(f), logical_and_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_2:
        return std::invoke(std::forward<Function>(f), logical_or_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_3:
        return std::invoke(std::forward<Function>(f), logical_or_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_invert:
        return std::invoke(std::forward<Function>(f), logical_invert_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::hsm_wrapper:
        return std::invoke(std::forward<Function>(f), hsm_wrapper_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::simulation_wrapper:
        return std::invoke(std::forward<Function>(f), simulation_wrapper_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sample_hold:
    case dynamics_type::qss2_sample_hold:
    case dynamics_type::qss3_sample_hold:
        return std::invoke(std::forward<Function>(f), sample_hold_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::zero_order_hold:
        return std::invoke(std::forward<Function>(f), zero_order_hold_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_quantizer:
    case dynamics_type::qss2_quantizer:
    case dynamics_type::qss3_quantizer:
        return std::invoke(std::forward<Function>(f), quantizer_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_integrate_and_fire:
    case dynamics_type::qss2_integrate_and_fire:
    case dynamics_type::qss3_integrate_and_fire:
        return std::invoke(std::forward<Function>(f), integrate_and_fire_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_threshold_crossing:
    case dynamics_type::qss2_threshold_crossing:
    case dynamics_type::qss3_threshold_crossing:
        return std::invoke(std::forward<Function>(f), threshold_crossing_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_pwm:
    case dynamics_type::qss2_pwm:
    case dynamics_type::qss3_pwm:
        return std::invoke(std::forward<Function>(f), pwm_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sqrt:
    case dynamics_type::qss2_sqrt:
    case dynamics_type::qss3_sqrt:
        return std::invoke(std::forward<Function>(f), sqrt_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_atan:
    case dynamics_type::qss2_atan:
    case dynamics_type::qss3_atan:
        return std::invoke(std::forward<Function>(f), atan_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_tan:
    case dynamics_type::qss2_tan:
    case dynamics_type::qss3_tan:
        return std::invoke(std::forward<Function>(f), tan_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_tanh:
    case dynamics_type::qss2_tanh:
    case dynamics_type::qss3_tanh:
        return std::invoke(std::forward<Function>(f), tanh_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sigmoid:
    case dynamics_type::qss2_sigmoid:
    case dynamics_type::qss3_sigmoid:
        return std::invoke(std::forward<Function>(f), sigmoid_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_division:
    case dynamics_type::qss2_division:
    case dynamics_type::qss3_division:
        return std::invoke(std::forward<Function>(f), division_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_atan2:
    case dynamics_type::qss2_atan2:
    case dynamics_type::qss3_atan2:
        return std::invoke(std::forward<Function>(f), atan2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_abs:
    case dynamics_type::qss2_abs:
    case dynamics_type::qss3_abs:
        return std::invoke(std::forward<Function>(f), abs_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sign:
    case dynamics_type::qss2_sign:
    case dynamics_type::qss3_sign:
        return std::invoke(std::forward<Function>(f), sign_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_minimum:
    case dynamics_type::qss2_minimum:
    case dynamics_type::qss3_minimum:
        return std::invoke(std::forward<Function>(f), minimum_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_maximum:
    case dynamics_type::qss2_maximum:
    case dynamics_type::qss3_maximum:
        return std::invoke(std::forward<Function>(f), maximum_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_saturation:
    case dynamics_type::qss2_saturation:
    case dynamics_type::qss3_saturation:
        return std::invoke(std::forward<Function>(f), saturation_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_dead_zone:
    case dynamics_type::qss2_dead_zone:
    case dynamics_type::qss3_dead_zone:
        return std::invoke(std::forward<Function>(f), dead_zone_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_hysteresis:
    case dynamics_type::qss2_hysteresis:
    case dynamics_type::qss3_hysteresis:
        return std::invoke(std::forward<Function>(f), hysteresis_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wrap:
    case dynamics_type::qss2_wrap:
    case dynamics_type::qss3_wrap:
        return std::invoke(std::forward<Function>(f), wrap_tag{},
                           std::forward<Args>(args)...);
    }

    unreachable();
}

template<typename Dymamics, typename Function, typename... Args>
constexpr auto dispatch_from_dyn(Function&& f, Args&&... args) noexcept
{
    const auto type = dynamics_typeof<std::decay_t<Dymamics>>();

    return dispatch(type, std::forward<Function>(f),
                    std::forward<Args>(args)...);
}

template<typename Tag>
concept not_simulation_wrapper = not std::is_same_v<Tag,
                                                    simulation_wrapper_tag>;

constexpr auto get_input_port_names(const dynamics_type type)
  -> std::span<const std::string_view>
{
    return dispatch(
      type,
      []<typename Tag>(Tag) noexcept -> std::span<const std::string_view> {
          if constexpr (have_input_port_type<Tag>)
              return { Tag::x_names };

          return {};
      });
}

constexpr auto get_output_port_names(const dynamics_type type)
  -> std::span<const std::string_view>
{
    return dispatch(
      type,
      []<typename Tag>(Tag) noexcept -> std::span<const std::string_view> {
          if constexpr (have_output_port_type<Tag>)
              return { Tag::y_names };

          return {};
      });
}

template<typename Dynamics>
constexpr auto get_input_port_names() -> std::span<const std::string_view>
{
    return dispatch_from_dyn<Dynamics>(
      []<typename Tag>(Tag) noexcept -> std::span<const std::string_view> {
          if constexpr (have_input_port_type<Tag>)
              return { Tag::x_names };

          return {};
      });
}

template<typename Dynamics>
constexpr auto get_output_port_names() -> std::span<const std::string_view>
{
    return dispatch_from_dyn<Dynamics>(
      []<typename Tag>(Tag) noexcept -> std::span<const std::string_view> {
          if constexpr (have_output_port_type<Tag>)
              return { Tag::y_names };

          return {};
      });
}

template<typename T>
    requires std::integral<T> or std::floating_point<T>
class bounded_value
{
    T m_lower;
    T m_upper;
    T m_value;

public:
    constexpr bounded_value(const T& lower_, const T& upper_) noexcept
      : m_lower{ lower_ }
      , m_upper{ upper_ }
    {
        debug::ensure(lower_ < upper_);
    }

    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }
};

template<typename T, T Lower, T Upper>
    requires std::integral<T>
class static_bounded_value
{
    static_assert(Lower < Upper);

    T m_value;

public:
    constexpr static_bounded_value(const T value) noexcept
      : m_value(std::clamp(value, Lower, Upper))
    {}

    constexpr T value() const noexcept { return m_value; }
    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }

    static constexpr T lower_bound() noexcept { return Lower; }
    static constexpr T upper_bound() noexcept { return Upper; }
};

template<typename T, int LowerNum, int LowerDenom, int UpperNum, int UpperDenom>
    requires std::floating_point<T>
class static_bounded_floating_point
{
public:
    static inline constexpr T lower = static_cast<T>(LowerNum) /
                                      static_cast<T>(LowerDenom);
    static inline constexpr T upper = static_cast<T>(UpperNum) /
                                      static_cast<T>(UpperDenom);
    static_assert(lower < upper);

private:
    T m_value;

public:
    constexpr static_bounded_floating_point(const T value) noexcept
      : m_value(std::clamp(value, lower, upper))
    {}

    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }
};

template<typename>
struct is_expected : std::false_type {};

template<typename T>
struct is_expected<::irt::expected<T>> : std::true_type {};

/** A for-each-condition function which reads each element of the vector @c
 * Vec and apply the function @c Fn. If the function @c Fn returns true, the
 * element is removed from the vector using the @c vector<T>::erase .
 *
 * @code
 *   for_each_cond(ed.visualisation_eds, [&](const auto v) noexcept -> bool
 * { if (v.tn_id == ed.pj.tree_nodes.get_id(tn)) { auto* ged =
 * app.graph_eds.try_to_get(v.graph_ed_id); auto* obs =
 * ed.pj.graph_observers.try_to_get(v.graph_obs_id);
 *
 *           if (not(ged and obs))
 *               return true;
 *
 *           ged->show(app, ed, tn, *obs);
 *       }
 *
 *       return false;
 *   });
 * @endcode
 */
template<typename Vec, typename Fn, typename... Args>
void for_each_cond(Vec& vec, Fn&& fn, Args&&... args) noexcept
{
    const auto [first,
                last] = std::ranges::remove_if(vec, [&](const auto& elem) {
        return std::invoke(std::forward<Fn>(fn), elem,
                           std::forward<Args>(args)...);
    });

    vec.erase(first, last);
}

//! @brief Apply the function @c f for all elements of the @c data_array.
template<typename Data, typename Function>
auto for_each_data(Data& d, Function&& f) noexcept -> void
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    for (auto& data : d)
        std::invoke(std::forward<Function>(f), data);
}

//! @brief Call function @c f if @c id exists in @c data_array.
template<typename Data, typename Function>
void if_data_exists_do(Data&                          d,
                       typename Data::identifier_type id,
                       Function&&                     f) noexcept
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    if (auto* ptr = d.try_to_get(id); ptr)
        std::invoke(std::forward<Function>(f), *ptr);
}

//! @brief Apply function @c f for all element in vector @c.
//!
//! For each element in vector @c (type must be @c typename @c
//! Data::identifier_type with @c Data a @c data_array) call the function
//! @c f. If the @c vec vector is no-const then, undefined element are
//! removed from the vector.
template<typename Data, typename Vector, typename Function>
void for_specified_data(Data& d, Vector& vec, Function&& f) noexcept
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    if constexpr (std::is_const_v<Vector>) {
        for (auto i = 0, e = vec.ssize(); i != e; ++i)
            if_data_exists_do(d, vec[i], std::forward<Function>(f));
    } else {
        sz i = 0;

        while (i < vec.size()) {
            if (auto* ptr = d.try_to_get(vec[i]); ptr) {
                std::invoke(std::forward<Function>(f), *ptr);
                ++i;
            } else {
                vec.swap_pop_back(i);
            }
        }
    }
}

constexpr inline bool is_valid_irt_filename(const std::string_view v) noexcept
{
    return !v.empty() && v[0] != '.' && v[0] != '-' && all_char_valid(v) &&
           v.ends_with(".irt");
}

constexpr inline source_type get_source_type(std::integral auto type) noexcept
{
    return (0 <= type and type < 4) ? enum_cast<source_type>(type)
                                    : source_type::constant;
}

constexpr inline u64 from_source(const source_type   type,
                                 const source_any_id id) noexcept
{
    switch (type) {
    case source_type::binary_file:
        return u32s_to_u64(ordinal(type), ordinal(id.binary_file_id));

    case source_type::constant:
        return u32s_to_u64(ordinal(type), ordinal(id.constant_id));

    case source_type::random:
        return u32s_to_u64(ordinal(type), ordinal(id.random_id));

    case source_type::text_file:
        return u32s_to_u64(ordinal(type), ordinal(id.text_file_id));
    }

    unreachable();
}

constexpr inline u64 from_source(const source& src) noexcept
{
    switch (src.type) {
    case source_type::binary_file:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.binary_file_id));

    case source_type::constant:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.constant_id));

    case source_type::random:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.random_id));

    case source_type::text_file:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.text_file_id));
    }

    unreachable();
}

constexpr inline std::pair<source_type, source_any_id> get_source(
  const u64 parameter) noexcept
{
    const auto    p_type = left(parameter);
    const auto    p_id   = right(parameter);
    const auto    type   = get_source_type(p_type);
    source_any_id id     = undefined<constant_source_id>();

    switch (type) {
    case source_type::constant:
        id.constant_id = enum_cast<constant_source_id>(p_id);
        break;

    case source_type::text_file:
        id.text_file_id = enum_cast<text_file_source_id>(p_id);
        break;

    case source_type::binary_file:
        id.binary_file_id = enum_cast<binary_file_source_id>(p_id);
        break;

    case source_type::random:
        id.random_id = enum_cast<random_source_id>(p_id);
        break;
    }

    return std::make_pair(type, id);
}

} // namespace irt

#endif
