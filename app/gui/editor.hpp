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

inline const char* get_dynamics_type_name(dynamics_type type) noexcept
{
    static const char* names[] = {
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
        "logical_and_2",   "logical_and_3",   "logical_or_2",
        "logical_or_3",    "logical_invert",  "hsm"
    };

    return names[ordinal(type)];
}

inline int make_input_node_id(const i32 mdl_index, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(mdl_index);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline int make_output_node_id(const i32 mdl_index, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(mdl_index);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline int make_input_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline int make_output_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline std::pair<irt::u32, irt::u32> get_model_input_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

inline std::pair<irt::u32, irt::u32> get_model_output_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port >= 8u && port < 16u);
    port -= 8u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask = ~(15u << 28u);

    irt::u32 index = real_node_id & mask;

    return std::make_pair(index, port);
}

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