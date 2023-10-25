// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include "irritator/modeling.hpp"
#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/io.hpp>

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

// void show_dynamics_inputs(application&                app,
//                           model_id                    id,
//                           hierarchical_state_machine& machine);
// void show_dynamics_inputs(application&                app,
//                           component_id                compo,
//                           model_id                    id,
//                           hierarchical_state_machine& machine);

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

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept;

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 src_id,
                                 source::source_type& src_type) noexcept;

} // irt

#endif
