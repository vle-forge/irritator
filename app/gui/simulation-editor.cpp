// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <imgui.h>

#include "imnodes.h"
#include "implot.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/timeline.hpp>

namespace irt {

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
        auto* app = container_of(&ed, &application::simulation_ed);

        const auto** names  = get_input_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = app->sim.models.get_id(mdl);
        const auto   e      = length(dyn.x);

        irt_assert(names != nullptr);
        irt_assert(app->sim.models.try_to_get(mdl_id) == &mdl);
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
        auto* app = container_of(&ed, &application::simulation_ed);

        const auto** names  = get_output_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = app->sim.models.get_id(mdl);
        const auto   e      = length(dyn.y);

        irt_assert(names != nullptr);
        irt_assert(app->sim.models.try_to_get(mdl_id) == &mdl);
        irt_assert(0 <= e && e < 8);

        for (int i = 0; i != e; ++i) {
            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_id, i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

struct gport
{
    gport() noexcept = default;

    gport(irt::model* model_, const int port_index_) noexcept
      : model(model_)
      , port_index(port_index_)
    {
    }

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
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
    }
}

static void show_dynamics_values(simulation& sim, const dynamic_queue& dyn)
{
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
    }
}

static void show_dynamics_values(simulation& sim, const priority_queue& dyn)
{
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
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
    auto* app = container_of(&ed, &application::simulation_ed);

    dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
        add_input_attribute(ed, dyn);
        if (ed.show_internal_values) {
            ImGui::PushItemWidth(120.0f);
            show_dynamics_values(app->sim, dyn);
            ImGui::PopItemWidth();
        }

        if (ed.allow_user_changes) {
            auto* app = container_of(&ed, &application::simulation_ed);
            ImGui::PushItemWidth(120.0f);

            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                auto* machine = app->sim.hsms.try_to_get(dyn.id);
                irt_assert(machine != nullptr);

                show_dynamics_inputs(
                  *app, app->sim.models.get_id(mdl), *machine);
            } else {
                show_dynamics_inputs(app->mod.srcs, dyn);
            }

            ImGui::PopItemWidth();
        }

        add_output_attribute(ed, dyn);
    });
}

static void show_top(simulation_editor& ed) noexcept
{
    auto* app = container_of(&ed, &application::simulation_ed);

    if (ed.show_identifiers) {
        model* mdl = nullptr;
        while (app->sim.models.next(mdl)) {
            const auto mdl_id    = app->sim.models.get_id(mdl);
            const auto mdl_index = static_cast<u32>(get_index(mdl_id));

            ImNodes::BeginNode(mdl_index);
            ImNodes::BeginNodeTitleBar();

            ImGui::TextFormat("{}\n{}",
                              mdl_index,
                              dynamics_type_names[static_cast<int>(mdl->type)]);

            ImNodes::EndNodeTitleBar();
            show_model_dynamics(ed, *mdl);
            ImNodes::EndNode();
        }
    } else {
        model* mdl = nullptr;
        while (app->sim.models.next(mdl)) {
            const auto mdl_id    = app->sim.models.get_id(mdl);
            const auto mdl_index = static_cast<u32>(get_index(mdl_id));

            ImNodes::BeginNode(mdl_index);
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(
              dynamics_type_names[static_cast<int>(mdl->type)]);
            ImNodes::EndNodeTitleBar();
            show_model_dynamics(ed, *mdl);
            ImNodes::EndNode();
        }
    }
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
        auto* app = container_of(&ed, &application::simulation_ed);

        app->add_simulation_task(task_simulation_model_add,
                                 ordinal(type),
                                 static_cast<u64>(click_pos.x),
                                 static_cast<u64>(click_pos.y));
    }
}

// @todo DEBUG MODE: Prefer user settings or better timeline constructor
simulation_editor::simulation_editor() noexcept
  : tl(32768, 4096, 65536, 65536, 32768, 32768)
{
    output_context = ImPlot::CreateContext();
    context        = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;
}

simulation_editor::~simulation_editor() noexcept
{
    if (output_context) {
        ImPlot::DestroyContext(output_context);
    }

    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void simulation_editor::remove_simulation_observation_from(
  model_id mdl_id) noexcept
{
    simulation_observation* obs = nullptr;
    while (sim_obs.next(obs)) {
        if (obs->model == mdl_id) {
            obs->clear();

            auto obs_id = sim_obs.get_id(*obs);
            sim_obs.free(obs_id);
            break;
        }
    }

    auto* app = container_of(this, &application::simulation_ed);
    if (auto* mdl = app->sim.models.try_to_get(mdl_id); mdl)
        app->sim.unobserve(*mdl);
}

void simulation_editor::add_simulation_observation_for(std::string_view name,
                                                       model_id mdl_id) noexcept
{
    auto* app = container_of(this, &application::simulation_ed);

    if (auto* mdl = app->sim.models.try_to_get(mdl_id); mdl) {
        if (app->sim.observers.can_alloc(1) && sim_obs.can_alloc(1)) {
            auto& sobs    = sim_obs.alloc(mdl_id, 32768);
            auto  sobs_id = sim_obs.get_id(sobs);
            sobs.name     = name;

            auto& obs = app->sim.observers.alloc(name, ordinal(sobs_id), 0);
            app->sim.observe(*mdl, obs);
        } else {
            if (!app->sim.observers.can_alloc(1)) {
                auto* app = container_of(this, &application::simulation_ed);
                auto& n   = app->notifications.alloc(log_level::error);
                n.title   = "Too many observer in simulation";
                app->notifications.enable(n);
            }

            if (!sim_obs.can_alloc(1)) {
                auto* app = container_of(this, &application::simulation_ed);
                auto& n   = app->notifications.alloc(log_level::error);
                n.title   = "Too many simulation observation in simulation";
                app->notifications.enable(n);
            }
        }
    }
}

void simulation_editor::select(tree_node_id id) noexcept
{
    auto* app = container_of(this, &application::simulation_ed);

    if (auto* tree = app->main.tree_nodes.try_to_get(id); tree) {
        unselect();

        head    = id;
        current = id;
    }
}

void simulation_editor::unselect() noexcept
{
    head    = undefined<tree_node_id>();
    current = undefined<tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();

    selected_links.clear();
    selected_nodes.clear();
}

void simulation_editor::clear() noexcept
{
    auto* app = container_of(this, &application::simulation_ed);

    unselect();

    force_pause           = false;
    force_stop            = false;
    show_minimap          = true;
    allow_user_changes    = true;
    store_all_changes     = false;
    infinity_simulation   = false;
    real_time             = false;
    have_use_back_advance = false;

    app->sim.clear();
    tl.reset();

    simulation_begin   = 0;
    simulation_end     = 100;
    simulation_current = 0;

    simulation_real_time_relation = 1000000;

    head    = undefined<tree_node_id>();
    current = undefined<tree_node_id>();
    mode    = visualization_mode::flat;

    simulation_state = simulation_status::not_started;

    sim_obs.clear();

    selected_links.clear();
    selected_nodes.clear();

    automatic_layout_iteration = 0;
    displacements.clear();
}

static status copy_port(simulation&                      sim,
                        const table<model_id, model_id>& mapping,
                        output_port&                     src,
                        output_port&                     dst) noexcept
{
    if (src == static_cast<u64>(-1)) {
        dst = src;
        return status::success;
    }

    auto src_list = get_node(sim, src);
    auto dst_list = append_node(sim, dst);

    auto it = src_list.begin();
    auto et = src_list.end();

    while (it != et) {
        if (auto* found = mapping.get(it->model); found) {
            irt_return_if_fail(sim.can_connect(1u),
                               status::simulation_not_enough_connection);
            dst_list.emplace_back(*found, it->port_index);
        } else {
            if (model* mdl = sim.models.try_to_get(it->model); mdl) {
                irt_return_if_fail(sim.can_connect(1u),
                                   status::simulation_not_enough_connection);

                dst_list.emplace_back(it->model, it->port_index);
            }
        }

        ++it;
    }

    return status::success;
}

static status copy(simulation_editor& ed, const ImVector<int>& nodes) noexcept
{
    auto* app = container_of(&ed, &application::simulation_ed);

    table<model_id, model_id> mapping;
    mapping.data.reserve(nodes.size());

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        auto* src_mdl = app->sim.models.try_to_get(nodes[i]);
        if (!src_mdl)
            continue;

        irt_return_if_fail(app->sim.can_alloc(1),
                           status::simulation_not_enough_model);

        auto& dst_mdl    = app->sim.clone(*src_mdl);
        auto  src_mdl_id = app->sim.models.get_id(src_mdl);
        auto  dst_mdl_id = app->sim.models.get_id(dst_mdl);

        app->sim.make_initialize(dst_mdl, ed.simulation_current);

        mapping.data.emplace_back(src_mdl_id, dst_mdl_id);
    }

    mapping.sort();

    for (int i = 0, e = length(mapping.data); i != e; ++i) {
        auto& src_mdl = app->sim.models.get(mapping.data[i].id);
        auto& dst_mdl = app->sim.models.get(mapping.data[i].value);

        dispatch(src_mdl,
                 [&app, &mapping, &dst_mdl]<typename Dynamics>(Dynamics& dyn) {
                     if constexpr (has_output_port<Dynamics>) {
                         for (int i = 0, e = length(dyn.y); i != e; ++i) {
                             auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);
                             irt_return_if_bad(copy_port(
                               app->sim, mapping, dyn.y[i], dst_dyn.y[i]));
                         }
                     }
                     return status::success;
                 });
    }

    return status::success;
}

static void free_children(simulation_editor&   ed,
                          const ImVector<int>& nodes) noexcept
{
    auto* app = container_of(&ed, &application::simulation_ed);

    const auto tasks = std::min(nodes.size(), task_list_tasks_number);

    for (int i = 0; i < tasks; ++i) {
        if (const auto* mdl = app->sim.models.try_to_get(nodes[i]); mdl) {
            const auto mdl_id  = app->sim.models.get_id(mdl);
            const auto mdl_idx = ordinal(mdl_id);
            app->add_simulation_task(task_simulation_model_del, mdl_idx);
        }
    }
}

static int show_connection(simulation_editor& ed, model& mdl, int connection_id)
{
    dispatch(
      mdl, [&ed, &connection_id]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (has_output_port<Dynamics>) {
              auto* app = container_of(&ed, &application::simulation_ed);

              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                  auto list = append_node(app->sim, dyn.y[i]);
                  auto it   = list.begin();
                  auto et   = list.end();

                  while (it != et) {
                      if (auto* mdl_dst = app->sim.models.try_to_get(it->model);
                          mdl_dst) {
                          int out =
                            make_output_node_id(app->sim.get_id(dyn), i);
                          int in =
                            make_input_node_id(it->model, it->port_index);
                          ImNodes::Link(connection_id++, out, in);
                          ++it;
                      } else {
                          it = list.erase(it);
                      }
                  }
              }
          }
      });

    return connection_id;
}

static void show_connections(simulation_editor& ed) noexcept
{
    auto* app           = container_of(&ed, &application::simulation_ed);
    auto  connection_id = 0;

    for (model* mdl = nullptr; app->sim.models.next(mdl);)
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
            auto* app = container_of(&ed, &application::simulation_ed);

            for (const auto elem : dyn.y) {
                auto list = get_node(app->sim, elem);

                for (auto& dst : list)
                    compute_connection_distance(
                      app->sim.get_id(mdl), dst.model, ed, k);
            }
        }
    });
}

static void compute_automatic_layout(simulation_editor& ed) noexcept
{
    auto* app      = container_of(&ed, &application::simulation_ed);
    auto& settings = app->settings_wnd;

    /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
       Fruchterman and Edward M. Reingold in Software--Pratice and
       Experience, Vol. 21(1 1), 1129-1164 (november 1991).
       */

    const auto orig_size = app->sim.models.ssize();

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
    const float k_square = area / static_cast<float>(app->sim.models.size());
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

    model* mdl = nullptr;
    while (app->sim.models.next(mdl)) {
        compute_connection_distance(*mdl, ed, k);
    }

    mdl = nullptr;
    for (int i_v = 0; i_v < size; ++i_v) {
        irt_assert(app->sim.models.next(mdl));
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

        const auto mdl_id    = app->sim.models.get_id(mdl);
        const auto mdl_index = static_cast<u32>(get_index(mdl_id));

        ImNodes::SetNodeEditorSpacePos(mdl_index, v_pos);
    }
}

static void compute_grid_layout(simulation_editor& ed) noexcept
{
    auto* app      = container_of(&ed, &application::simulation_ed);
    auto& settings = app->settings_wnd;

    const auto size  = app->sim.models.max_size();
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
            if (!app->sim.models.next(mdl))
                break;

            const auto mdl_id    = app->sim.models.get_id(mdl);
            const auto mdl_index = static_cast<u32>(get_index(mdl_id));

            new_pos.x = panning.x + j * settings.grid_layout_x_distance;
            ImNodes::SetNodeEditorSpacePos(mdl_index, new_pos);
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + column * settings.grid_layout_y_distance;

    for (float j = 0.f; j < remaining; ++j) {
        if (!app->sim.models.next(mdl))
            break;

        const auto mdl_id    = app->sim.models.get_id(mdl);
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

static void show_simulation_graph_editor(application& app) noexcept
{
    ImNodes::EditorContextSet(app.simulation_ed.context);

    ImNodes::BeginNodeEditor();

    if (app.simulation_ed.automatic_layout_iteration > 0) {
        compute_automatic_layout(app.simulation_ed);
        --app.simulation_ed.automatic_layout_iteration;
    }

    show_top(app.simulation_ed);
    show_connections(app.simulation_ed);

    ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
    show_simulation_graph_editor_edit_menu(app, click_pos);

    if (app.simulation_ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    {
        auto& sim   = app.sim;
        int   start = 0, end = 0;

        if (ImNodes::IsLinkCreated(&start, &end) &&
            app.simulation_ed.can_edit()) {
            const gport out = get_out(sim, start);
            const gport in  = get_in(sim, end);

            if (out.model && in.model && sim.can_connect(1)) {
                if (is_ports_compatible(
                      *out.model, out.port_index, *in.model, in.port_index)) {
                    if (auto status = sim.connect(
                          *out.model, out.port_index, *in.model, in.port_index);
                        is_bad(status)) {
                        auto& notif =
                          app.notifications.alloc(log_level::warning);
                        notif.title = "Not enough memory to connect model";
                        app.notifications.enable(notif);
                    }
                }
            }
        }
    }

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
            copy(app.simulation_ed, app.simulation_ed.selected_nodes);
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
                              auto list = append_node(app.sim, dyn.y[j]);
                              auto it   = list.begin();
                              auto et   = list.end();

                              while (it != et && link_id_to_delete != -1) {
                                  if (current_link_id == link_id_to_delete) {

                                      it = list.erase(it);

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

static void show_simulation_action_buttons(simulation_editor& ed,
                                           bool can_be_initialized,
                                           bool can_be_started,
                                           bool can_be_paused,
                                           bool can_be_restarted,
                                           bool can_be_stopped) noexcept
{
    if (ImGui::Button("clear"))
        ed.simulation_clear();
    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_initialized);
    if (ImGui::Button("import"))
        ed.simulation_copy_modeling();
    ImGui::SameLine();

    if (ImGui::Button("init")) {
        ed.simulation_init();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_started);
    if (ImGui::Button("start"))
        ed.simulation_start();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_paused);
    if (ImGui::Button("pause")) {
        ed.force_pause = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_restarted);
    if (ImGui::Button("continue")) {
        ed.simulation_start();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_stopped);
    if (ImGui::Button("stop")) {
        ed.force_stop = true;
    }
    ImGui::EndDisabled();

    const bool history_mode = (ed.store_all_changes || ed.allow_user_changes) &&
                              (can_be_started || can_be_restarted);

    ImGui::BeginDisabled(!history_mode);

    if (ed.store_all_changes) {
        ImGui::SameLine();
        if (ImGui::Button("step-by-step"))
            ed.simulation_start_1();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_back());
    if (ImGui::Button("<"))
        ed.simulation_back();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!ed.tl.can_advance());
    if (ImGui::Button(">"))
        ed.simulation_advance();
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ed.tl.current_bag != ed.tl.points.end()) {
        ImGui::TextFormat(
          "debug bag: {}/{}", ed.tl.current_bag->bag, ed.tl.bag);
    } else {
        ImGui::TextFormat("debug bag: {}", ed.tl.bag);
    }

    ImGui::EndDisabled();
}

static void show_simulation_parameters(simulation_editor& ed) noexcept
{
    if (ImGui::CollapsingHeader("parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(100.f);
        ImGui::InputReal("Begin", &ed.simulation_begin);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::Checkbox("Edit", &ed.allow_user_changes);

        ImGui::InputReal("End", &ed.simulation_end);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::Checkbox("Debug", &ed.store_all_changes)) {
            if (ed.store_all_changes &&
                ed.simulation_state == simulation_status::running) {
                ed.enable_or_disable_debug();
            }
        }

        ImGui::BeginDisabled(!ed.real_time);
        ImGui::InputScalar("Micro second for 1 unit time",
                           ImGuiDataType_S64,
                           &ed.simulation_real_time_relation);
        ImGui::EndDisabled();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::Checkbox("No time limit", &ed.infinity_simulation);

        ImGui::TextFormat("Current time {:.6f}", ed.simulation_current);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::Checkbox("Real time", &ed.real_time);

        ImGui::TextFormat("Simulation phase: {}", ordinal(ed.simulation_state));

        ImGui::PopItemWidth();
    }
}

void simulation_editor::show() noexcept
{
    if (!ImGui::Begin(simulation_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    const bool can_be_initialized = !match(simulation_state,
                                           simulation_status::not_started,
                                           simulation_status::finished,
                                           simulation_status::initialized,
                                           simulation_status::not_started);

    const bool can_be_started =
      !match(simulation_state, simulation_status::initialized);

    const bool can_be_paused = !match(simulation_state,
                                      simulation_status::running,
                                      simulation_status::run_requiring,
                                      simulation_status::paused);

    const bool can_be_restarted =
      !match(simulation_state, simulation_status::pause_forced);

    const bool can_be_stopped = !match(simulation_state,
                                       simulation_status::running,
                                       simulation_status::run_requiring,
                                       simulation_status::paused,
                                       simulation_status::pause_forced);

    show_simulation_action_buttons(*this,
                                   can_be_initialized,
                                   can_be_started,
                                   can_be_paused,
                                   can_be_restarted,
                                   can_be_stopped);

    if (ImGui::BeginTable("##ed", 2, flags)) {
        ImGui::TableSetupColumn(
          "Hierarchy", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn(
          "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);

        auto* app = container_of(this, &application::simulation_ed);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        app->pj.show();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("##s-c", ImVec2(0, 0), false)) {
            if (ImGui::BeginTabBar("##SimulationTabBar")) {
                if (ImGui::BeginTabItem("Parameters")) {
                    show_simulation_parameters(*this);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("graph")) {
                    if (app->simulation_ed.can_display_graph_editor())
                        show_simulation_graph_editor(*app);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::End();
}

//
// simulation tasks
//

/* Task: add a model into current simulation (running, started etc.). */
void task_simulation_model_add(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& sim = task->app->sim;

    if (!sim.can_alloc(1)) {
        auto& n = task->app->notifications.alloc(log_level::error);
        n.title = "To many model in simulation editor";
        task->app->notifications.enable(n);
        task->state = task_status::finished;
        return;
    }

    auto& mdl    = sim.alloc(enum_cast<dynamics_type>(task->param_1));
    auto  mdl_id = sim.models.get_id(mdl);

    auto ret =
      sim.make_initialize(mdl, task->app->simulation_ed.simulation_current);
    if (is_bad(ret)) {
        sim.deallocate(mdl_id);

        auto& n = task->app->notifications.alloc(log_level::error);
        n.title = "Fail to initialize model";
        task->app->notifications.enable(n);
        task->state = task_status::finished;
        return;
    }

    const auto   x = static_cast<float>(task->param_2);
    const auto   y = static_cast<float>(task->param_3);
    const ImVec2 pos{ x, y };
    task->app->simulation_ed.models_to_move.push(
      simulation_editor::model_to_move{ .id = mdl_id, .position = pos });

    task->state = task_status::finished;
}

void task_simulation_model_del(void* param) noexcept
{
    auto* task  = reinterpret_cast<simulation_task*>(param);
    task->state = task_status::started;

    auto& sim    = task->app->sim;
    auto  mdl_id = enum_cast<model_id>(task->param_1);
    sim.deallocate(mdl_id);

    task->state = task_status::finished;
}

} // namesapce irt
