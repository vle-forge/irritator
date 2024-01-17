// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <optional>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/timeline.hpp>

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include "imnodes.h"
#include "implot.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace irt {

const char* observable_type_names[] = {
    "none", "file", "plot", "graph", "grid",
};

static int make_input_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(irt::get_index(mdl));
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

static int make_output_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(irt::get_index(mdl));
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

static std::pair<irt::u32, irt::u32> get_model_input_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

static std::pair<irt::u32, irt::u32> get_model_output_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port >= 8u && port < 16u);
    port -= 8u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

template<typename Dynamics>
static void add_input_attribute(simulation_editor& ed,
                                const Dynamics&    dyn) noexcept
{
    if constexpr (has_input_port<Dynamics>) {
        auto& app = container_of(&ed, &application::simulation_ed);

        const auto** names  = get_input_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = app.sim.models.get_id(mdl);
        const auto   e      = length(dyn.x);

        irt_assert(names != nullptr);
        irt_assert(app.sim.models.try_to_get(mdl_id) == &mdl);
        irt_assert(0 <= e && e < 8);

        for (int i = 0; i != e; ++i) {
            ImNodes::BeginInputAttribute(make_input_node_id(mdl_id, i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(simulation_editor& ed,
                                 const Dynamics&    dyn) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        auto& app = container_of(&ed, &application::simulation_ed);

        const auto** names  = get_output_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = app.sim.models.get_id(mdl);
        const auto   e      = length(dyn.y);

        irt_assert(names != nullptr);
        irt_assert(app.sim.models.try_to_get(mdl_id) == &mdl);
        irt_assert(0 <= e && e < 8);

        for (int i = 0; i != e; ++i) {
            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_id, i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

struct gport {
    gport() noexcept = default;

    gport(irt::model* model_, const int port_index_) noexcept
      : model(model_)
      , port_index(port_index_)
    {}

    irt::model* model      = nullptr;
    int         port_index = 0;
};

gport get_in(simulation& sim, const int index) noexcept
{
    const auto model_index_port = get_model_input_port(index);
    auto*      mdl              = sim.models.try_to_get(model_index_port.first);

    return { mdl, static_cast<int>(model_index_port.second) };
}

gport get_out(simulation& sim, const int index) noexcept
{
    const auto model_index_port = get_model_output_port(index);
    auto*      mdl              = sim.models.try_to_get(model_index_port.first);

    return { mdl, static_cast<int>(model_index_port.second) };
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss1_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss2_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss3_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_sum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_sum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_sum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss1_multiplier& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_wsum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_wsum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_wsum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss2_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const qss3_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(simulation& /*sim*/, const integrator& dyn)
{
    ImGui::TextFormat("value {}", dyn.current_value);
}

static void show_dynamics_values(simulation& /*sim*/, const quantifier& dyn)
{
    ImGui::TextFormat("up threshold {}", dyn.m_upthreshold);
    ImGui::TextFormat("down threshold {}", dyn.m_downthreshold);
}

static void show_dynamics_values(simulation& /*sim*/, const adder_2& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const adder_3& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const adder_4& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::TextFormat("{} * {}", dyn.values[3], dyn.input_coeffs[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const mult_2& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const mult_3& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const mult_4& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::TextFormat("{} * {}", dyn.values[3], dyn.input_coeffs[3]);
}

static void show_dynamics_values(simulation& /*sim*/, const counter& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
}

static void show_dynamics_values(simulation& sim, const queue& dyn)
{
    auto* ar = sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front().data[0]);
        ImGui::TextFormat("next value {}", ar->front().data[1]);
    }
}

static void show_dynamics_values(simulation& sim, const dynamic_queue& dyn)
{
    auto* ar = sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front().data[0]);
        ImGui::TextFormat("next value {}", ar->front().data[1]);
    }
}

static void show_dynamics_values(simulation& sim, const priority_queue& dyn)
{
    auto* ar = sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front().data[0]);
        ImGui::TextFormat("next value {}", ar->front().data[1]);
    }
}

static void show_dynamics_values(simulation& /*sim*/, const generator& dyn)
{
    ImGui::TextFormat("next {}", dyn.sigma);
}

static void show_dynamics_values(simulation& /*sim*/, const constant& dyn)
{
    ImGui::TextFormat("next {}", dyn.sigma);
    ImGui::TextFormat("value {}", dyn.value);

    // @todo reenable
    // if (ImGui::Button("Send now")) {
    //     dyn.value = dyn.default_value;
    //     dyn.sigma = dyn.default_offset;

    //     auto& mdl = get_model(dyn);
    //     mdl.tl    = ed.simulation_current;
    //     mdl.tn    = ed.simulation_current + dyn.sigma;
    //     if (dyn.sigma && mdl.tn == ed.simulation_current)
    //         mdl.tn = std::nextafter(ed.simulation_current,
    //                                 ed.simulation_current + to_real(1.));

    //     ed.sim.sched.update(mdl, mdl.tn);
    // }
}

template<int QssLevel>
static void show_dynamics_values(simulation& /*sim*/,
                                 const abstract_cross<QssLevel>& dyn)
{
    ImGui::TextFormat("threshold: {}", dyn.threshold);
    ImGui::TextFormat("value: {}", dyn.value[0]);
    ImGui::TextFormat("if-value: {}", dyn.if_value[0]);
    ImGui::TextFormat("else-value: {}", dyn.else_value[0]);

    if (dyn.detect_up)
        ImGui::TextFormat("up detection");
    else
        ImGui::TextFormat("down detection");
}

template<int QssLevel>
static void show_dynamics_values(simulation& /*sim*/,
                                 const abstract_filter<QssLevel>& dyn)
{
    ImGui::TextFormat("value: {}", dyn.value[0]);
    ImGui::TextFormat("lower-threshold: {}", dyn.lower_threshold);
    ImGui::TextFormat("upper-threshold: {}", dyn.upper_threshold);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_power& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_power& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_power& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss1_square& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss2_square& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const qss3_square& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const cross& dyn)
{
    ImGui::TextFormat("threshold: {}", dyn.threshold);
    ImGui::TextFormat("value: {}", dyn.value);
    ImGui::TextFormat("if-value: {}", dyn.if_value);
    ImGui::TextFormat("else-value: {}", dyn.else_value);
}

static void show_dynamics_values(simulation& /*sim*/, const accumulator_2& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
    ImGui::TextFormat("- 0: {}", dyn.numbers[0]);
    ImGui::TextFormat("- 1: {}", dyn.numbers[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const filter& dyn)
{
    ImGui::TextFormat("value {}", dyn.inValue[0]);
}

static void show_dynamics_values(simulation& /*sim*/, const time_func& dyn)
{
    ImGui::TextFormat("value {}", dyn.value);
}

static void show_dynamics_values(simulation& /*sim*/, const logical_and_2& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const logical_or_2& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
}

static void show_dynamics_values(simulation& /*sim*/, const logical_and_3& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
    ImGui::TextFormat("- 2 {}", dyn.values[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const logical_or_3& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
    ImGui::TextFormat("- 2 {}", dyn.values[2]);
}

static void show_dynamics_values(simulation& /*sim*/, const logical_invert& dyn)
{
    ImGui::TextFormat("value {}", dyn.value);
}

static void show_dynamics_values(simulation& /*sim*/,
                                 const hsm_wrapper& /*dyn*/)
{
    ImGui::TextFormat("no data");
}

static void show_model_dynamics(simulation_editor& ed, model& mdl) noexcept
{
    auto& app = container_of(&ed, &application::simulation_ed);

    dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
        add_input_attribute(ed, dyn);
        if (ed.show_internal_values) {
            ImGui::PushItemWidth(120.0f);
            show_dynamics_values(app.sim, dyn);
            ImGui::PopItemWidth();
        }

        if (ed.allow_user_changes) {
            auto& app = container_of(&ed, &application::simulation_ed);
            ImGui::PushItemWidth(120.0f);
            show_dynamics_inputs(app.mod.srcs, dyn);
            ImGui::PopItemWidth();
        }

        add_output_attribute(ed, dyn);
    });
}

void show_top_with_identifier(application& app)
{
    for_each_data(app.sim.models, [&](model& mdl) noexcept -> void {
        const auto mdl_id    = app.sim.models.get_id(mdl);
        const auto mdl_index = static_cast<u32>(get_index(mdl_id));

        ImNodes::BeginNode(mdl_index);
        ImNodes::BeginNodeTitleBar();

        ImGui::TextFormat(
          "{}\n{}", mdl_index, dynamics_type_names[ordinal(mdl.type)]);

        ImNodes::EndNodeTitleBar();
        show_model_dynamics(app.simulation_ed, mdl);
        ImNodes::EndNode();
    });
}

void show_top_without_identifier(application& app)
{
    for_each_data(app.sim.models, [&](model& mdl) noexcept -> void {
        const auto mdl_id    = app.sim.models.get_id(mdl);
        const auto mdl_index = static_cast<u32>(get_index(mdl_id));

        ImNodes::BeginNode(mdl_index);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(dynamics_type_names[ordinal(mdl.type)]);
        ImNodes::EndNodeTitleBar();
        show_model_dynamics(app.simulation_ed, mdl);
        ImNodes::EndNode();
    });
}

static void show_top(application& app) noexcept
{
    if (app.simulation_ed.show_identifiers)
        show_top_with_identifier(app);
    else
        show_top_without_identifier(app);
}

static void add_popup_menuitem(simulation_editor& ed,
                               bool               enable_menu_item,
                               dynamics_type      type,
                               ImVec2             click_pos) noexcept
{
    if (ImGui::MenuItem(dynamics_type_names[static_cast<i8>(type)],
                        nullptr,
                        nullptr,
                        enable_menu_item)) {
        auto& app = container_of(&ed, &application::simulation_ed);

        app.add_simulation_task(task_simulation_model_add,
                                ordinal(type),
                                static_cast<u64>(click_pos.x),
                                static_cast<u64>(click_pos.y));
    }
}

static status copy_port(simulation&                      sim,
                        const table<model_id, model_id>& mapping,
                        node_id&                         src,
                        node_id&                         dst) noexcept
{
    if (is_undefined(src)) {
        dst = src;
        return success();
    }

    auto* src_list = sim.nodes.try_to_get(src);
    auto* dst_list = sim.nodes.try_to_get(dst);

    auto it = src_list->begin();
    auto et = src_list->end();

    while (it != et) {
        if (auto* found = mapping.get(it->model); found) {
            if (!sim.can_connect(1u))
                return new_error(simulation::part::messages,
                                 container_full_error{});
            dst_list->emplace_back(*found, it->port_index);
        } else {
            if (model* mdl = sim.models.try_to_get(it->model); mdl) {
                if (!sim.can_connect(1u))
                    return new_error(simulation::part::messages,
                                     container_full_error{});

                dst_list->emplace_back(it->model, it->port_index);
            }
        }

        ++it;
    }

    return success();
}

static status copy(simulation_editor& ed, const ImVector<int>& nodes) noexcept
{
    auto& app = container_of(&ed, &application::simulation_ed);

    table<model_id, model_id> mapping;
    mapping.data.reserve(nodes.size());

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        auto* src_mdl = app.sim.models.try_to_get(nodes[i]);
        if (!src_mdl)
            continue;

        if (!app.sim.can_alloc(1))
            return new_error(simulation::part::models, container_full_error{});

        auto& dst_mdl    = app.sim.clone(*src_mdl);
        auto  src_mdl_id = app.sim.models.get_id(src_mdl);
        auto  dst_mdl_id = app.sim.models.get_id(dst_mdl);

        irt_check(app.sim.make_initialize(dst_mdl, ed.simulation_current));

        mapping.data.emplace_back(src_mdl_id, dst_mdl_id);
    }

    mapping.sort();

    for (int i = 0, e = length(mapping.data); i != e; ++i) {
        auto& src_mdl = app.sim.models.get(mapping.data[i].id);
        auto& dst_mdl = app.sim.models.get(mapping.data[i].value);

        if (auto ret = dispatch(
              src_mdl,
              [&app, &mapping, &dst_mdl]<typename Dynamics>(
                Dynamics& dyn) noexcept -> status {
                  if constexpr (has_output_port<Dynamics>) {
                      for (int i = 0, e = length(dyn.y); i != e; ++i) {
                          auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);

                          if (auto ret = copy_port(
                                app.sim, mapping, dyn.y[i], dst_dyn.y[i]);
                              !ret)
                              return ret.error();
                      }
                  }
                  return success();
              });
            !ret)
            return ret.error();
    }

    return success();
}

static void free_children(simulation_editor&   ed,
                          const ImVector<int>& nodes) noexcept
{
    auto& app = container_of(&ed, &application::simulation_ed);

    const auto tasks = std::min(nodes.size(), task_list_tasks_number);

    for (int i = 0; i < tasks; ++i) {
        if (const auto* mdl = app.sim.models.try_to_get(nodes[i]); mdl) {
            const auto mdl_id  = app.sim.models.get_id(mdl);
            const auto mdl_idx = ordinal(mdl_id);
            app.add_simulation_task(task_simulation_model_del, mdl_idx);
        }
    }
}

static int show_connection(simulation_editor& ed, model& mdl, int connection_id)
{
    dispatch(
      mdl, [&ed, &connection_id]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (has_output_port<Dynamics>) {
              auto& app = container_of(&ed, &application::simulation_ed);

              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                  if (auto* list = app.sim.nodes.try_to_get(dyn.y[i]); list) {
                      auto it = list->begin();
                      auto et = list->end();

                      while (it != et) {
                          if (auto* mdl_dst =
                                app.sim.models.try_to_get(it->model);
                              mdl_dst) {
                              int out =
                                make_output_node_id(app.sim.get_id(dyn), i);
                              int in =
                                make_input_node_id(it->model, it->port_index);
                              ImNodes::Link(connection_id++, out, in);
                              ++it;
                          } else {
                              it = list->erase(it);
                          }
                      }
                  }
              }
          }
      });

    return connection_id;
}

static void show_connections(simulation_editor& ed) noexcept
{
    auto& app           = container_of(&ed, &application::simulation_ed);
    auto  connection_id = 0;

    for (model* mdl = nullptr; app.sim.models.next(mdl);)
        connection_id = show_connection(ed, *mdl, connection_id);
}

static void compute_connection_distance(const model_id     src,
                                        const model_id     dst,
                                        simulation_editor& ed,
                                        const float        k) noexcept
{
    const auto u     = static_cast<u32>(get_index(dst));
    const auto v     = static_cast<u32>(get_index(src));
    const auto u_pos = ImNodes::GetNodeEditorSpacePos(u);
    const auto v_pos = ImNodes::GetNodeEditorSpacePos(v);

    const float dx = v_pos.x - u_pos.x;
    const float dy = v_pos.y - u_pos.y;
    if (dx && dy) {
        const float d2    = dx * dx / dy * dy;
        const float coeff = std::sqrt(d2) / k;

        ed.displacements[v].x -= dx * coeff;
        ed.displacements[v].y -= dy * coeff;
        ed.displacements[u].x += dx * coeff;
        ed.displacements[u].y += dy * coeff;
    }
}

static void compute_connection_distance(const model&       mdl,
                                        simulation_editor& ed,
                                        const float        k) noexcept
{
    dispatch(mdl, [&mdl, &ed, k]<typename Dynamics>(Dynamics& dyn) -> void {
        if constexpr (has_output_port<Dynamics>) {
            auto& app = container_of(&ed, &application::simulation_ed);

            for (const auto elem : dyn.y) {
                if (auto* lst = app.sim.nodes.try_to_get(elem); lst) {
                    for (auto& dst : *lst)
                        compute_connection_distance(
                          app.sim.get_id(mdl), dst.model, ed, k);
                }
            }
        }
    });
}

static void compute_automatic_layout(simulation_editor& ed) noexcept
{
    auto& app      = container_of(&ed, &application::simulation_ed);
    auto& settings = app.settings_wnd;

    /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
       Fruchterman and Edward M. Reingold in Software--Pratice and
       Experience, Vol. 21(1 1), 1129-1164 (november 1991).
       */

    const auto orig_size = app.sim.models.ssize();

    if (orig_size == 0)
        return;

    const auto size      = static_cast<int>(orig_size);
    const auto tmp       = std::sqrt(size);
    const auto column    = static_cast<int>(tmp);
    auto       line      = column;
    auto       remaining = static_cast<int>(size) - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const float W =
      static_cast<float>(column) * settings.automatic_layout_x_distance;
    const float L =
      static_cast<float>(line) +
      ((remaining > 0) ? settings.automatic_layout_y_distance : 0.f);
    const float area     = W * L;
    const float k_square = area / static_cast<float>(app.sim.models.size());
    const float k        = std::sqrt(k_square);

    // float t = 1.f - static_cast<float>(iteration) /
    //                   static_cast<float>(automatic_layout_iteration_limit);
    // t *= t;

    ed.displacements.resize(size);

    float t =
      1.f - 1.f / static_cast<float>(settings.automatic_layout_iteration_limit);

    for (int i_v = 0; i_v < size; ++i_v) {
        const int v = i_v;

        ed.displacements[v].x = 0.f;
        ed.displacements[v].y = 0.f;

        for (int i_u = 0; i_u < size; ++i_u) {
            const int u = i_u;

            if (u != v) {
                const auto u_pos = ImNodes::GetNodeEditorSpacePos(u);
                const auto v_pos = ImNodes::GetNodeEditorSpacePos(v);
                const auto delta =
                  ImVec2{ v_pos.x - u_pos.x, v_pos.y - u_pos.y };

                if (delta.x && delta.y) {
                    const float d2    = delta.x * delta.x + delta.y * delta.y;
                    const float coeff = k_square / d2;

                    ed.displacements[v].x += coeff * delta.x;
                    ed.displacements[v].y += coeff * delta.y;
                }
            }
        }
    }

    for_each_data(app.sim.models, [&](auto& mdl) noexcept -> void {
        compute_connection_distance(mdl, ed, k);
    });

    model* mdl = nullptr;
    for (int i_v = 0; i_v < size; ++i_v) {
        irt_assert(app.sim.models.next(mdl));
        const int v = i_v;

        const float d2 = ed.displacements[v].x * ed.displacements[v].x +
                         ed.displacements[v].y * ed.displacements[v].y;
        const float d = std::sqrt(d2);

        if (d > t) {
            const float coeff = t / d;
            ed.displacements[v].x *= coeff;
            ed.displacements[v].y *= coeff;
        }

        auto v_pos = ImNodes::GetNodeEditorSpacePos(v);
        v_pos.x += ed.displacements[v].x;
        v_pos.y += ed.displacements[v].y;

        const auto mdl_id    = app.sim.models.get_id(mdl);
        const auto mdl_index = static_cast<u32>(get_index(mdl_id));

        ImNodes::SetNodeEditorSpacePos(mdl_index, v_pos);
    }
}

static void compute_grid_layout(simulation_editor& ed) noexcept
{
    auto& app      = container_of(&ed, &application::simulation_ed);
    auto& settings = app.settings_wnd;

    const auto size  = app.sim.models.max_size();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return;

    const auto column    = std::floor(std::sqrt(fsize));
    auto       line      = column;
    auto       remaining = fsize - (column * line);

    const auto panning = ImNodes::EditorContextGetPanning();
    auto       new_pos = panning;

    model* mdl = nullptr;
    for (float i = 0.f; i < line; ++i) {
        new_pos.y = panning.y + i * settings.grid_layout_y_distance;

        for (float j = 0.f; j < column; ++j) {
            if (!app.sim.models.next(mdl))
                break;

            const auto mdl_id    = app.sim.models.get_id(mdl);
            const auto mdl_index = static_cast<u32>(get_index(mdl_id));

            new_pos.x = panning.x + j * settings.grid_layout_x_distance;
            ImNodes::SetNodeEditorSpacePos(mdl_index, new_pos);
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + column * settings.grid_layout_y_distance;

    for (float j = 0.f; j < remaining; ++j) {
        if (!app.sim.models.next(mdl))
            break;

        const auto mdl_id    = app.sim.models.get_id(mdl);
        const auto mdl_index = static_cast<u32>(get_index(mdl_id));

        new_pos.x = panning.x + j * settings.grid_layout_x_distance;
        ImNodes::SetNodeEditorSpacePos(mdl_index, new_pos);
    }
}

static void show_simulation_graph_editor_edit_menu(application& app,
                                                   ImVec2 click_pos) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::MenuItem("Force grid layout")) {
            compute_grid_layout(app.simulation_ed);
        }

        if (ImGui::MenuItem("Force automatic layout")) {
            app.simulation_ed.automatic_layout_iteration =
              app.settings_wnd.automatic_layout_iteration_limit;
        }

        ImGui::MenuItem(
          "Show internal values", "", &app.simulation_ed.show_internal_values);
        ImGui::MenuItem("Show internal parameters",
                        "",
                        &app.simulation_ed.show_internal_inputs);
        ImGui::MenuItem(
          "Show identifiers", "", &app.simulation_ed.show_identifiers);

        ImGui::Separator();

        const auto can_edit = app.simulation_ed.can_edit();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = static_cast<int>(dynamics_type::qss1_integrator);
            const auto e = static_cast<int>(dynamics_type::qss1_wsum_4) + 1;
            for (; i != e; ++i)
                add_popup_menuitem(app.simulation_ed,
                                   can_edit,
                                   static_cast<dynamics_type>(i),
                                   click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = static_cast<int>(dynamics_type::qss2_integrator);
            const auto e = static_cast<int>(dynamics_type::qss2_wsum_4) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(app.simulation_ed,
                                   can_edit,
                                   static_cast<dynamics_type>(i),
                                   click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = static_cast<int>(dynamics_type::qss3_integrator);
            const auto e = static_cast<int>(dynamics_type::qss3_wsum_4) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(app.simulation_ed,
                                   can_edit,
                                   static_cast<dynamics_type>(i),
                                   click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::integrator,
                               click_pos);
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::quantifier,
                               click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::adder_2, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::adder_3, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::adder_4, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::mult_2, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::mult_3, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::mult_4, click_pos);
            add_popup_menuitem(
              app.simulation_ed, can_edit, dynamics_type::cross, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::logical_and_2,
                               click_pos);
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::logical_or_2,
                               click_pos);
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::logical_and_3,
                               click_pos);
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::logical_or_3,
                               click_pos);
            add_popup_menuitem(app.simulation_ed,
                               can_edit,
                               dynamics_type::logical_invert,
                               click_pos);
            ImGui::EndMenu();
        }

        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::counter, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::queue, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::dynamic_queue, click_pos);
        add_popup_menuitem(app.simulation_ed,
                           can_edit,
                           dynamics_type::priority_queue,
                           click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::generator, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::constant, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::time_func, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::accumulator_2, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::filter, click_pos);
        add_popup_menuitem(
          app.simulation_ed, can_edit, dynamics_type::hsm_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void try_create_connection(application& app) noexcept
{
    int start = 0, end = 0;

    if (!(ImNodes::IsLinkCreated(&start, &end) && app.simulation_ed.can_edit()))
        return;

    const gport out = get_out(app.sim, start);
    const gport in  = get_in(app.sim, end);

    if (!out.model && in.model && app.sim.can_connect(1))
        return;

    if (!is_ports_compatible(
          *out.model, out.port_index, *in.model, in.port_index))
        return;

    attempt_all(
      [&]() noexcept -> status {
          irt_check(app.sim.connect(
            *out.model, out.port_index, *in.model, in.port_index));
          return success();
      },

      [&app](const simulation::part s) noexcept -> void {
          auto& notif = app.notifications.alloc(log_level::warning);
          notif.title = "Fail to create connection";
          format(notif.message, "Error: {}", ordinal(s));
          app.notifications.enable(notif);
      },

      [&app]() noexcept -> void {
          auto& notif   = app.notifications.alloc(log_level::warning);
          notif.title   = "Fail to create connection";
          notif.message = "Error: Unknown";
          app.notifications.enable(notif);
      });
}

void show_simulation_editor(application& app) noexcept
{
    ImNodes::EditorContextSet(app.simulation_ed.context);

    ImNodes::BeginNodeEditor();

    if (app.simulation_ed.automatic_layout_iteration > 0) {
        compute_automatic_layout(app.simulation_ed);
        --app.simulation_ed.automatic_layout_iteration;
    }

    show_top(app);
    show_connections(app.simulation_ed);

    ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
    show_simulation_graph_editor_edit_menu(app, click_pos);

    if (app.simulation_ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    try_create_connection(app);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();

    if (num_selected_nodes == 0) {
        app.simulation_ed.selected_nodes.clear();
        ImNodes::ClearNodeSelection();
    }

    if (num_selected_links == 0) {
        app.simulation_ed.selected_links.clear();
        ImNodes::ClearLinkSelection();
    }

    if (app.simulation_ed.can_edit() && num_selected_nodes > 0) {
        app.simulation_ed.selected_nodes.resize(num_selected_nodes, -1);
        ImNodes::GetSelectedNodes(app.simulation_ed.selected_nodes.begin());

        if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
            free_children(app.simulation_ed, app.simulation_ed.selected_nodes);
            app.simulation_ed.selected_nodes.clear();
            num_selected_nodes = 0;
            ImNodes::ClearNodeSelection();
        } else if (ImGui::IsKeyReleased(ImGuiKey_D)) {
            if (auto ret =
                  copy(app.simulation_ed, app.simulation_ed.selected_nodes);
                !ret) {
                auto& n = app.notifications.alloc();
                n.title = "Fail to copy selected nodes";
                app.notifications.enable(n);
            }
            app.simulation_ed.selected_nodes.clear();
            num_selected_nodes = 0;
            ImNodes::ClearNodeSelection();
        }
    } else if (app.simulation_ed.can_edit() && num_selected_links > 0) {
        app.simulation_ed.selected_links.resize(num_selected_links);

        if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
            std::fill_n(app.simulation_ed.selected_links.begin(),
                        app.simulation_ed.selected_links.size(),
                        -1);
            ImNodes::GetSelectedLinks(app.simulation_ed.selected_links.begin());
            std::sort(app.simulation_ed.selected_links.begin(),
                      app.simulation_ed.selected_links.end(),
                      std::less<int>());

            int link_id_to_delete = app.simulation_ed.selected_links[0];
            int current_link_id   = 0;
            int i                 = 0;

            auto selected_links_ptr  = app.simulation_ed.selected_links.Data;
            auto selected_links_size = app.simulation_ed.selected_links.Size;

            model* mdl = nullptr;
            while (app.sim.models.next(mdl) && link_id_to_delete != -1) {
                dispatch(
                  *mdl,
                  [&app,
                   &i,
                   &current_link_id,
                   selected_links_ptr,
                   selected_links_size,
                   &link_id_to_delete]<typename Dynamics>(Dynamics& dyn) {
                      if constexpr (has_output_port<Dynamics>) {
                          const int e = length(dyn.y);
                          int       j = 0;

                          while (j < e && link_id_to_delete != -1) {
                              auto* list = app.sim.nodes.try_to_get(dyn.y[j]);
                              if (list) {
                                  auto it = list->begin();
                                  auto et = list->end();

                                  while (it != et && link_id_to_delete != -1) {
                                      if (current_link_id ==
                                          link_id_to_delete) {

                                          it = list->erase(it);

                                          ++i;

                                          if (i != selected_links_size)
                                              link_id_to_delete =
                                                selected_links_ptr[i];
                                          else
                                              link_id_to_delete = -1;
                                      } else {
                                          ++it;
                                      }

                                      ++current_link_id;
                                  }
                              }
                              ++j;
                          }
                      }
                  });
            }

            num_selected_links = 0;
            app.simulation_ed.selected_links.resize(0);
            ImNodes::ClearLinkSelection();
        }
    }
}

} // namespace irt
