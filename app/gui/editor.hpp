// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_EDITOR_2021

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/external_source.hpp>
#include <irritator/io.hpp>

namespace irt {

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
void show_dynamics_inputs(external_source&            srcs,
                          hsm_wrapper&                dyn,
                          hierarchical_state_machine& machine);
void show_dynamics_inputs(external_source& srcs, time_func& dyn);

void show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src);

} // irt

#endif
