// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

struct application;

void show_dynamics_inputs(external_source& srcs, qss1_integrator& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_integrator& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_integrator& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_multiplier& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_sum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_sum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_sum_4& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_wsum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_wsum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_wsum_4& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_multiplier& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_sum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_sum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_sum_4& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_wsum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_wsum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_wsum_4& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_multiplier& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_sum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_sum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_sum_4& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_wsum_2& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_wsum_3& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_wsum_4& dyn);
void show_dynamics_inputs(external_source& srcs, integrator& dyn);
void show_dynamics_inputs(external_source& srcs, quantifier& dyn);
void show_dynamics_inputs(external_source& srcs, adder_2& dyn);
void show_dynamics_inputs(external_source& srcs, adder_3& dyn);
void show_dynamics_inputs(external_source& srcs, adder_4& dyn);
void show_dynamics_inputs(external_source& srcs, mult_2& dyn);
void show_dynamics_inputs(external_source& srcs, mult_3& dyn);
void show_dynamics_inputs(external_source& srcs, mult_4& dyn);
void show_dynamics_inputs(external_source& srcs, counter& dyn);
void show_dynamics_inputs(external_source& srcs, queue& dyn);
void show_dynamics_inputs(external_source& srcs, dynamic_queue& dyn);
void show_dynamics_inputs(external_source& srcs, priority_queue& dyn);
void show_dynamics_inputs(external_source& srcs, generator& dyn);
void show_dynamics_inputs(external_source& srcs, constant& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_cross& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_cross& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_cross& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_filter& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_filter& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_filter& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_power& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_power& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_power& dyn);
void show_dynamics_inputs(external_source& srcs, qss1_square& dyn);
void show_dynamics_inputs(external_source& srcs, qss2_square& dyn);
void show_dynamics_inputs(external_source& srcs, qss3_square& dyn);
void show_dynamics_inputs(external_source& srcs, cross& dyn);
void show_dynamics_inputs(external_source& srcs, accumulator_2& dyn);
void show_dynamics_inputs(external_source& srcs, filter& dyn);
void show_dynamics_inputs(external_source& srcs, logical_and_2& dyn);
void show_dynamics_inputs(external_source& srcs, logical_or_2& dyn);
void show_dynamics_inputs(external_source& srcs, logical_and_3& dyn);
void show_dynamics_inputs(external_source& srcs, logical_or_3& dyn);
void show_dynamics_inputs(external_source& srcs, logical_invert& dyn);
void show_dynamics_inputs(external_source& app, hsm_wrapper& dyn);
void show_dynamics_inputs(external_source& srcs, time_func& dyn);

/**
 * @brief Display ImGui widgets acconding to the dynamics in model.
 * @details [long description]
 *
 * @param app Global application to get settings, external sources, etc.
 * @param mdl Model to display dynamics widgets.
 * @param p [in,out] to read/write dynamics parameters.
 *
 * @return true is data under the parameter @c p are changed.
 */
bool show_parameter_editor(application& app, model& mdl, parameter& p) noexcept;

bool show_parameter_editor(application&  app,
                           dynamics_type type,
                           parameter&    p) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept;

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 src_id,
                                 source::source_type& src_type) noexcept;

struct dynamics_qss1_integrator_tag
{};
struct dynamics_qss1_multiplier_tag
{};
struct dynamics_qss1_cross_tag
{};
struct dynamics_qss1_filter_tag
{};
struct dynamics_qss1_power_tag
{};
struct dynamics_qss1_square_tag
{};
struct dynamics_qss1_sum_2_tag
{};
struct dynamics_qss1_sum_3_tag
{};
struct dynamics_qss1_sum_4_tag
{};
struct dynamics_qss1_wsum_2_tag
{};
struct dynamics_qss1_wsum_3_tag
{};
struct dynamics_qss1_wsum_4_tag
{};
struct dynamics_qss2_integrator_tag
{};
struct dynamics_qss2_multiplier_tag
{};
struct dynamics_qss2_cross_tag
{};
struct dynamics_qss2_filter_tag
{};
struct dynamics_qss2_power_tag
{};
struct dynamics_qss2_square_tag
{};
struct dynamics_qss2_sum_2_tag
{};
struct dynamics_qss2_sum_3_tag
{};
struct dynamics_qss2_sum_4_tag
{};
struct dynamics_qss2_wsum_2_tag
{};
struct dynamics_qss2_wsum_3_tag
{};
struct dynamics_qss2_wsum_4_tag
{};
struct dynamics_qss3_integrator_tag
{};
struct dynamics_qss3_multiplier_tag
{};
struct dynamics_qss3_cross_tag
{};
struct dynamics_qss3_filter_tag
{};
struct dynamics_qss3_power_tag
{};
struct dynamics_qss3_square_tag
{};
struct dynamics_qss3_sum_2_tag
{};
struct dynamics_qss3_sum_3_tag
{};
struct dynamics_qss3_sum_4_tag
{};
struct dynamics_qss3_wsum_2_tag
{};
struct dynamics_qss3_wsum_3_tag
{};
struct dynamics_qss3_wsum_4_tag
{};
struct dynamics_integrator_tag
{};
struct dynamics_quantifier_tag
{};
struct dynamics_adder_2_tag
{};
struct dynamics_adder_3_tag
{};
struct dynamics_adder_4_tag
{};
struct dynamics_mult_2_tag
{};
struct dynamics_mult_3_tag
{};
struct dynamics_mult_4_tag
{};
struct dynamics_counter_tag
{};
struct dynamics_queue_tag
{};
struct dynamics_dynamic_queue_tag
{};
struct dynamics_priority_queue_tag
{};
struct dynamics_generator_tag
{};
struct dynamics_constant_tag
{};
struct dynamics_cross_tag
{};
struct dynamics_accumulator_2_tag
{};
struct dynamics_time_func_tag
{};
struct dynamics_filter_tag
{};
struct dynamics_logical_and_2_tag
{};
struct dynamics_logical_and_3_tag
{};
struct dynamics_logical_or_2_tag
{};
struct dynamics_logical_or_3_tag
{};
struct dynamics_logical_invert_tag
{};
struct dynamics_hsm_wrapper_tag
{};

template<typename Function, typename... Args>
constexpr auto dispatcher(const dynamics_type type,
                          Function&&          f,
                          Args&&... args) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
        return f(dynamics_qss1_integrator_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_multiplier:
        return f(dynamics_qss1_multiplier_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_cross:
        return f(dynamics_qss1_cross_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_filter:
        return f(dynamics_qss1_filter_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_power:
        return f(dynamics_qss1_power_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_square:
        return f(dynamics_qss1_square_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_2:
        return f(dynamics_qss1_sum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_3:
        return f(dynamics_qss1_sum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_4:
        return f(dynamics_qss1_sum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_2:
        return f(dynamics_qss1_wsum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_3:
        return f(dynamics_qss1_wsum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_4:
        return f(dynamics_qss1_wsum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_integrator:
        return f(dynamics_qss2_integrator_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_multiplier:
        return f(dynamics_qss2_multiplier_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_cross:
        return f(dynamics_qss2_cross_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_filter:
        return f(dynamics_qss2_filter_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_power:
        return f(dynamics_qss2_power_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_square:
        return f(dynamics_qss2_square_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_2:
        return f(dynamics_qss2_sum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_3:
        return f(dynamics_qss2_sum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_4:
        return f(dynamics_qss2_sum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_2:
        return f(dynamics_qss2_wsum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_3:
        return f(dynamics_qss2_wsum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_4:
        return f(dynamics_qss2_wsum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_integrator:
        return f(dynamics_qss3_integrator_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_multiplier:
        return f(dynamics_qss3_multiplier_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_cross:
        return f(dynamics_qss3_cross_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_filter:
        return f(dynamics_qss3_filter_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_power:
        return f(dynamics_qss3_power_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_square:
        return f(dynamics_qss3_square_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_2:
        return f(dynamics_qss3_sum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_3:
        return f(dynamics_qss3_sum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_4:
        return f(dynamics_qss3_sum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_2:
        return f(dynamics_qss3_wsum_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_3:
        return f(dynamics_qss3_wsum_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_4:
        return f(dynamics_qss3_wsum_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::integrator:
        return f(dynamics_integrator_tag{}, std::forward<Args>(args)...);
    case dynamics_type::quantifier:
        return f(dynamics_quantifier_tag{}, std::forward<Args>(args)...);
    case dynamics_type::adder_2:
        return f(dynamics_adder_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::adder_3:
        return f(dynamics_adder_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::adder_4:
        return f(dynamics_adder_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::mult_2:
        return f(dynamics_mult_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::mult_3:
        return f(dynamics_mult_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::mult_4:
        return f(dynamics_mult_4_tag{}, std::forward<Args>(args)...);
    case dynamics_type::counter:
        return f(dynamics_counter_tag{}, std::forward<Args>(args)...);
    case dynamics_type::queue:
        return f(dynamics_queue_tag{}, std::forward<Args>(args)...);
    case dynamics_type::dynamic_queue:
        return f(dynamics_dynamic_queue_tag{}, std::forward<Args>(args)...);
    case dynamics_type::priority_queue:
        return f(dynamics_priority_queue_tag{}, std::forward<Args>(args)...);
    case dynamics_type::generator:
        return f(dynamics_generator_tag{}, std::forward<Args>(args)...);
    case dynamics_type::constant:
        return f(dynamics_constant_tag{}, std::forward<Args>(args)...);
    case dynamics_type::cross:
        return f(dynamics_cross_tag{}, std::forward<Args>(args)...);
    case dynamics_type::accumulator_2:
        return f(dynamics_accumulator_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::time_func:
        return f(dynamics_time_func_tag{}, std::forward<Args>(args)...);
    case dynamics_type::filter:
        return f(dynamics_filter_tag{}, std::forward<Args>(args)...);
    case dynamics_type::logical_and_2:
        return f(dynamics_logical_and_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::logical_and_3:
        return f(dynamics_logical_and_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::logical_or_2:
        return f(dynamics_logical_or_2_tag{}, std::forward<Args>(args)...);
    case dynamics_type::logical_or_3:
        return f(dynamics_logical_or_3_tag{}, std::forward<Args>(args)...);
    case dynamics_type::logical_invert:
        return f(dynamics_logical_invert_tag{}, std::forward<Args>(args)...);
    case dynamics_type::hsm_wrapper:
        return f(dynamics_hsm_wrapper_tag{}, std::forward<Args>(args)...);
    }

    irt_unreachable();
}

template<typename Dynamics, typename Function, typename... Args>
constexpr auto dispatcher(Function&& f, Args&&... args) noexcept
{
    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return f(dynamics_qss1_integrator_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return f(dynamics_qss1_multiplier_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return f(dynamics_qss1_cross_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_filter>)
        return f(dynamics_qss1_filter_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_power>)
        return f(dynamics_qss1_power_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_square>)
        return f(dynamics_qss1_square_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_2>)
        return f(dynamics_qss1_sum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_3>)
        return f(dynamics_qss1_sum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_sum_4>)
        return f(dynamics_qss1_sum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_2>)
        return f(dynamics_qss1_wsum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_3>)
        return f(dynamics_qss1_wsum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_4>)
        return f(dynamics_qss1_wsum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
        return f(dynamics_qss2_integrator_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
        return f(dynamics_qss2_multiplier_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_cross>)
        return f(dynamics_qss2_cross_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_filter>)
        return f(dynamics_qss2_filter_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_power>)
        return f(dynamics_qss2_power_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_square>)
        return f(dynamics_qss2_square_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_2>)
        return f(dynamics_qss2_sum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_3>)
        return f(dynamics_qss2_sum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_sum_4>)
        return f(dynamics_qss2_sum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_2>)
        return f(dynamics_qss2_wsum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_3>)
        return f(dynamics_qss2_wsum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_4>)
        return f(dynamics_qss2_wsum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_integrator>)
        return f(dynamics_qss3_integrator_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_multiplier>)
        return f(dynamics_qss3_multiplier_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_cross>)
        return f(dynamics_qss3_cross_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_filter>)
        return f(dynamics_qss3_filter_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_power>)
        return f(dynamics_qss3_power_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_square>)
        return f(dynamics_qss3_square_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_2>)
        return f(dynamics_qss3_sum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_3>)
        return f(dynamics_qss3_sum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_sum_4>)
        return f(dynamics_qss3_sum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_2>)
        return f(dynamics_qss3_wsum_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_3>)
        return f(dynamics_qss3_wsum_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_4>)
        return f(dynamics_qss3_wsum_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, integrator>)
        return f(dynamics_integrator_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, quantifier>)
        return f(dynamics_quantifier_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, adder_2>)
        return f(dynamics_adder_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, adder_3>)
        return f(dynamics_adder_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, adder_4>)
        return f(dynamics_adder_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, mult_2>)
        return f(dynamics_mult_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, mult_3>)
        return f(dynamics_mult_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, mult_4>)
        return f(dynamics_mult_4_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, counter>)
        return f(dynamics_counter_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, queue>)
        return f(dynamics_queue_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, dynamic_queue>)
        return f(dynamics_dynamic_queue_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, priority_queue>)
        return f(dynamics_priority_queue_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, generator>)
        return f(dynamics_generator_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, constant>)
        return f(dynamics_constant_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, cross>)
        return f(dynamics_cross_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return f(dynamics_accumulator_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, time_func>)
        return f(dynamics_time_func_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, filter>)
        return f(dynamics_filter_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, logical_and_2>)
        return f(dynamics_logical_and_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, logical_and_3>)
        return f(dynamics_logical_and_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, logical_or_2>)
        return f(dynamics_logical_or_2_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, logical_or_3>)
        return f(dynamics_logical_or_3_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, logical_invert>)
        return f(dynamics_logical_invert_tag{}, std::forward<Args>(args)...);
    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return f(dynamics_hsm_wrapper_tag{}, std::forward<Args>(args)...);

    irt_unreachable();
}

auto show_parameter(dynamics_qss1_integrator_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_multiplier_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_cross_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_filter_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_power_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_square_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_sum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_sum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_sum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_wsum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_wsum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss1_wsum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_integrator_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_multiplier_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_cross_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_filter_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_power_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_square_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_sum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_sum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_sum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_wsum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_wsum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss2_wsum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_integrator_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_multiplier_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_cross_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_filter_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_power_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_square_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_sum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_sum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_sum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_wsum_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_wsum_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_qss3_wsum_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_integrator_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_quantifier_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_adder_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_adder_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_adder_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_mult_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_mult_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_mult_4_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_counter_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_queue_tag, application& app, parameter& p) noexcept
  -> bool;
auto show_parameter(dynamics_dynamic_queue_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_priority_queue_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_generator_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_constant_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_cross_tag, application& app, parameter& p) noexcept
  -> bool;
auto show_parameter(dynamics_accumulator_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_time_func_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_filter_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_logical_and_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_logical_and_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_logical_or_2_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_logical_or_3_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_logical_invert_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;
auto show_parameter(dynamics_hsm_wrapper_tag,
                    application& app,
                    parameter&   p) noexcept -> bool;

auto get_dynamics_input_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_quantifier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_adder_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_adder_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_adder_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_mult_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_mult_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_mult_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_input_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>;

auto get_dynamics_output_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_integrator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_quantifier_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_adder_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_adder_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_adder_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_mult_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_mult_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_mult_4_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_cross_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_filter_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>;
auto get_dynamics_output_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>;

/**
 * @brief Get the list of input port for the specified dynamics.
 *
 * @param type
 * @return An empty @c std::span if input port does not exist.
 */
inline auto get_dynamics_input_names(const dynamics_type type) noexcept
  -> std::span<const std::string_view>
{
    return dispatcher(
      type, [](auto tag) noexcept -> std::span<const std::string_view> {
          return get_dynamics_input_names(tag);
      });
}

/**
 * @brief Get the list of input port for the specified dynamics.
 *
 * @param type
 * @return An empty @c std::span if input port does not exist.
 */
inline auto get_dynamics_output_names(const dynamics_type type) noexcept
  -> std::span<const std::string_view>
{
    return dispatcher(
      type, [](auto tag) noexcept -> std::span<const std::string_view> {
          return get_dynamics_output_names(tag);
      });
}

} // irt

#endif
