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

template<typename Dynamics>
static void add_input_attribute(simulation_editor& ed,
                                const Dynamics&    dyn) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names  = get_input_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = ed.sim.models.get_id(mdl);

        sz i = 0;
        for ([[maybe_unused]] auto& elem : dyn.x) {
            irt_assert(i < 8u);
            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginInputAttribute(make_input_node_id(mdl_id, (int)i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
            ++i;
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(simulation_editor& ed,
                                 const Dynamics&    dyn) noexcept
{
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names  = get_output_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = ed.sim.models.get_id(mdl);

        sz i = 0;
        for ([[maybe_unused]] auto& elem : dyn.y) {
            irt_assert(i < 8u);

            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_id, (int)i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
            ++i;
        }
    }
}

struct gport
{
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

static void show_dynamics_values(simulation& /*sim*/, const flow& dyn)
{
    if (dyn.i < dyn.default_size)
        ImGui::TextFormat("value {}", dyn.default_data[dyn.i]);
    else
        ImGui::TextFormat("no data");
}

static void show_model_dynamics(simulation_editor& ed, model& mdl) noexcept
{
    dispatch(mdl, [&](const auto& dyn) {
        add_input_attribute(ed, dyn);
        ImGui::PushItemWidth(120.0f);
        show_dynamics_values(ed.sim, dyn);
        ImGui::PopItemWidth();
        add_output_attribute(ed, dyn);
    });

    // if (simulation_show_value && st != editor_status::editing) {
    //    dispatch(mdl, [&](const auto& dyn) {
    //        add_input_attribute(*this, dyn);
    //        ImGui::PushItemWidth(120.0f);
    //        show_dynamics_values(sim, dyn);
    //        ImGui::PopItemWidth();
    //        add_output_attribute(*this, dyn);
    //    });
    //} else {
    //    dispatch(mdl, [&](auto& dyn) {
    //        add_input_attribute(*this, dyn);
    //        ImGui::PushItemWidth(120.0f);

    //        if (settings.show_dynamics_inputs_in_editor)
    //            show_dynamics_inputs(this->srcs, dyn);
    //        ImGui::PopItemWidth();
    //        add_output_attribute(*this, dyn);
    //    });
    //}
}

static void show_top(simulation_editor& ed) noexcept
{
    model* mdl = nullptr;
    while (ed.sim.models.next(mdl)) {
        const auto mdl_id    = ed.sim.models.get_id(mdl);
        const auto mdl_index = get_index(mdl_id);

        // if (st != editor_status::editing &&
        // models_make_transition[mdl_index]) {

        //    ImNodes::PushColorStyle(ImNodesCol_TitleBar,
        //                            ImGui::ColorConvertFloat4ToU32(
        //                              settings.gui_model_transition_color));

        //    ImNodes::PushColorStyle(
        //      ImNodesCol_TitleBarHovered,
        //      settings.gui_hovered_model_transition_color);
        //    ImNodes::PushColorStyle(
        //      ImNodesCol_TitleBarSelected,
        //      settings.gui_selected_model_transition_color);
        //} else {
        // ImNodes::PushColorStyle(
        //  ImNodesCol_TitleBar,
        //  ImGui::ColorConvertFloat4ToU32(settings.gui_model_color));

        // ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
        //                        settings.gui_hovered_model_color);
        // ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
        //                        settings.gui_selected_model_color);
        //}

        ImNodes::BeginNode(mdl_index);
        ImNodes::BeginNodeTitleBar();

        // ImGui::TextUnformatted(mdl->name.c_str());
        // ImGui::OpenPopupOnItemClick("Rename model", 1);

        // bool is_rename = true;
        // ImGui::SetNextWindowSize(ImVec2(250, 200),
        // ImGuiCond_Always); if (ImGui::BeginPopupModal("Rename
        // model", &is_rename)) {
        //    ImGui::InputText(
        //      "Name##edit-1", mdl->name.begin(),
        //      mdl->name.capacity());
        //    if (ImGui::Button("Close"))
        //        ImGui::CloseCurrentPopup();
        //    ImGui::EndPopup();
        //}

        ImGui::TextFormat("{}\n{}",
                          mdl_index,
                          dynamics_type_names[static_cast<int>(mdl->type)]);

        ImNodes::EndNodeTitleBar();
        show_model_dynamics(ed, *mdl);
        ImNodes::EndNode();

        // ImNodes::PopColorStyle();
        // ImNodes::PopColorStyle();
    }
}

static status add_popup_menuitem(simulation_editor& ed,
                                 dynamics_type      type,
                                 model_id*          new_model) noexcept
{
    if (!ed.sim.models.can_alloc(1))
        return status::data_array_not_enough_memory;

    if (ImGui::MenuItem(dynamics_type_names[static_cast<i8>(type)])) {
        auto& mdl  = ed.sim.alloc(type);
        *new_model = ed.sim.models.get_id(mdl);

        return ed.sim.make_initialize(mdl, ed.simulation_current);
    }

    return status::success;
}

simulation_editor::simulation_editor() noexcept
{
    context = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
}

simulation_editor::~simulation_editor() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
        context = nullptr;
    }
}

void simulation_editor::shutdown() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
        context = nullptr;
    }
}

void simulation_editor::select(simulation_tree_node_id id) noexcept
{
    if (auto* tree = tree_nodes.try_to_get(id); tree) {
        unselect();

        head    = id;
        current = id;
    }
}

void simulation_editor::unselect() noexcept
{
    head    = undefined<simulation_tree_node_id>();
    current = undefined<simulation_tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();

    selected_links.clear();
    selected_nodes.clear();
}

void simulation_editor::clear() noexcept
{
    unselect();

    tree_nodes.clear();
    sim.clear();
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
    table<model_id, model_id> mapping;
    mapping.data.reserve(nodes.size());

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        auto* src_mdl = ed.sim.models.try_to_get(nodes[i]);
        if (!src_mdl)
            continue;

        irt_return_if_fail(ed.sim.can_alloc(1),
                           status::simulation_not_enough_model);

        auto& dst_mdl    = ed.sim.clone(*src_mdl);
        auto  src_mdl_id = ed.sim.models.get_id(src_mdl);
        auto  dst_mdl_id = ed.sim.models.get_id(dst_mdl);

        ed.sim.make_initialize(dst_mdl, ed.simulation_current);

        mapping.data.emplace_back(src_mdl_id, dst_mdl_id);
    }

    mapping.sort();

    for (int i = 0, e = length(mapping.data); i != e; ++i) {
        auto& src_mdl = ed.sim.models.get(mapping.data[i].id);
        auto& dst_mdl = ed.sim.models.get(mapping.data[i].value);

        dispatch(
          src_mdl, [&ed, &mapping, &dst_mdl]<typename Dynamics>(Dynamics& dyn) {
              if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                  for (int i = 0, e = length(dyn.y); i != e; ++i) {
                      auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);
                      irt_return_if_bad(
                        copy_port(ed.sim, mapping, dyn.y[i], dst_dyn.y[i]));
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
    for (int i = 0, e = nodes.size(); i != e; ++i) {
        const auto* mdl = ed.sim.models.try_to_get(nodes[i]);
        if (!mdl)
            continue;

        const auto child_id = ed.sim.models.get_id(mdl);
        ed.sim.deallocate(child_id);

        // TODO observation
        // observation_dispatch(get_index(child_id),
        //                     [](auto& outs, const auto id) {
        //                     outs.free(id);
        //                     });

        // observation_outputs[get_index(child_id)] = std::monostate{};
    }
}

static int show_connection(simulation_editor& ed, model& mdl, int connection_id)
{
    dispatch(
      mdl, [&ed, &connection_id]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                  auto list = append_node(ed.sim, dyn.y[i]);
                  auto it   = list.begin();
                  auto et   = list.end();

                  while (it != et) {
                      if (auto* mdl_dst = ed.sim.models.try_to_get(it->model);
                          mdl_dst) {
                          int out = make_output_node_id(ed.sim.get_id(dyn), i);
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
    int connection_id = 0;

    for (model* mdl = nullptr; ed.sim.models.next(mdl);)
        connection_id = show_connection(ed, *mdl, connection_id);
}

static void compute_connection_distance(const model_id     src,
                                        const model_id     dst,
                                        simulation_editor& ed,
                                        const float        k) noexcept
{
    const auto u     = get_index(dst);
    const auto v     = get_index(src);
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
        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (const auto elem : dyn.y) {
                auto list = get_node(ed.sim, elem);

                for (auto& dst : list)
                    compute_connection_distance(
                      ed.sim.get_id(mdl), dst.model, ed, k);
            }
        }
    });
}

static void compute_automatic_layout(settings_manager&  settings,
                                     simulation_editor& ed) noexcept
{
    /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
       Fruchterman and Edward M. Reingold in Software--Pratice and
       Experience, Vol. 21(1 1), 1129-1164 (november 1991).
       */

    const auto orig_size = ed.sim.models.size();

    if (orig_size == 0 || orig_size > std::numeric_limits<int>::max())
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
    const float k_square = area / static_cast<float>(ed.sim.models.size());
    const float k        = std::sqrt(k_square);

    // float t = 1.f - static_cast<float>(iteration) /
    //                   static_cast<float>(automatic_layout_iteration_limit);
    // t *= t;

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
    while (ed.sim.models.next(mdl)) {
        compute_connection_distance(*mdl, ed, k);
    }

    mdl = nullptr;
    for (int i_v = 0; i_v < size; ++i_v) {
        irt_assert(ed.sim.models.next(mdl));
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

        const auto mdl_id    = ed.sim.models.get_id(mdl);
        const auto mdl_index = get_index(mdl_id);

        ImNodes::SetNodeEditorSpacePos(mdl_index, v_pos);
    }
}

static void compute_grid_layout(settings_manager&  settings,
                                simulation_editor& ed) noexcept
{
    const auto size  = ed.sim.models.max_size();
    const auto fsize = static_cast<float>(size);

    if (size == 0 || size > std::numeric_limits<u32>::max())
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
            if (!ed.sim.models.next(mdl))
                break;

            const auto mdl_id    = ed.sim.models.get_id(mdl);
            const auto mdl_index = get_index(mdl_id);

            new_pos.x = panning.x + j * settings.grid_layout_x_distance;
            ImNodes::SetNodeEditorSpacePos(mdl_index, new_pos);
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + column * settings.grid_layout_y_distance;

    for (float j = 0.f; j < remaining; ++j) {
        if (!ed.sim.models.next(mdl))
            break;

        const auto mdl_id    = ed.sim.models.get_id(mdl);
        const auto mdl_index = get_index(mdl_id);

        new_pos.x = panning.x + j * settings.grid_layout_x_distance;
        ImNodes::SetNodeEditorSpacePos(mdl_index, new_pos);
    }
}

static void show_simulation_graph_editor(application& app) noexcept
{
    ImNodes::EditorContextSet(app.s_editor.context);

    ImNodes::BeginNodeEditor();

    if (app.s_editor.automatic_layout_iteration > 0) {
        compute_automatic_layout(app.settings, app.s_editor);
        --app.s_editor.automatic_layout_iteration;
    }

    show_top(app.s_editor);
    show_connections(app.s_editor);

    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    model_id   new_model = undefined<model_id>();
    const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::MenuItem("Force grid layout")) {
            compute_grid_layout(app.settings, app.s_editor);
        }

        if (ImGui::MenuItem("Force automatic layout")) {
            app.s_editor.automatic_layout_iteration =
              app.settings.automatic_layout_iteration_limit;
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = static_cast<int>(dynamics_type::qss1_integrator);
            const auto e = static_cast<int>(dynamics_type::qss1_wsum_4) + 1;
            for (; i != e; ++i)
                add_popup_menuitem(
                  app.s_editor, static_cast<dynamics_type>(i), &new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = static_cast<int>(dynamics_type::qss2_integrator);
            const auto e = static_cast<int>(dynamics_type::qss2_wsum_4) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(
                  app.s_editor, static_cast<dynamics_type>(i), &new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = static_cast<int>(dynamics_type::qss3_integrator);
            const auto e = static_cast<int>(dynamics_type::qss3_wsum_4) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(
                  app.s_editor, static_cast<dynamics_type>(i), &new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              app.s_editor, dynamics_type::integrator, &new_model);
            add_popup_menuitem(
              app.s_editor, dynamics_type::quantifier, &new_model);
            add_popup_menuitem(
              app.s_editor, dynamics_type::adder_2, &new_model);
            add_popup_menuitem(
              app.s_editor, dynamics_type::adder_3, &new_model);
            add_popup_menuitem(
              app.s_editor, dynamics_type::adder_4, &new_model);
            add_popup_menuitem(app.s_editor, dynamics_type::mult_2, &new_model);
            add_popup_menuitem(app.s_editor, dynamics_type::mult_3, &new_model);
            add_popup_menuitem(app.s_editor, dynamics_type::mult_4, &new_model);
            add_popup_menuitem(app.s_editor, dynamics_type::cross, &new_model);
            ImGui::EndMenu();
        }

        add_popup_menuitem(app.s_editor, dynamics_type::counter, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::queue, &new_model);
        add_popup_menuitem(
          app.s_editor, dynamics_type::dynamic_queue, &new_model);
        add_popup_menuitem(
          app.s_editor, dynamics_type::priority_queue, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::generator, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::constant, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::time_func, &new_model);
        add_popup_menuitem(
          app.s_editor, dynamics_type::accumulator_2, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::filter, &new_model);
        add_popup_menuitem(app.s_editor, dynamics_type::flow, &new_model);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    if (app.s_editor.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    if (new_model != undefined<model_id>()) {
        const auto mdl_index = get_index(new_model);
        ImNodes::SetNodeEditorSpacePos(mdl_index, click_pos);
    }

    {
        auto& sim   = app.s_editor.sim;
        int   start = 0, end = 0;

        if (ImNodes::IsLinkCreated(&start, &end)) {
            const gport out = get_out(sim, start);
            const gport in  = get_in(sim, end);

            if (out.model && in.model && sim.can_connect(1)) {
                if (auto status = sim.connect(
                      *out.model, out.port_index, *in.model, in.port_index);
                    is_bad(status)) {
                    auto& notif =
                      app.notifications.alloc(notification_type::warning);
                    notif.title = "Not enough memory to connect model";
                    app.notifications.enable(notif);
                }
            }
        }
    }

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();

    if (num_selected_nodes > 0) {
        app.s_editor.selected_nodes.resize(num_selected_nodes, -1);
        ImNodes::GetSelectedNodes(app.s_editor.selected_nodes.begin());

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
            free_children(app.s_editor, app.s_editor.selected_nodes);
            app.s_editor.selected_nodes.clear();
            num_selected_nodes = 0;
            ImNodes::ClearNodeSelection();
        } else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('D')) {
            copy(app.s_editor, app.s_editor.selected_nodes);
            app.s_editor.selected_nodes.clear();
            num_selected_nodes = 0;
            ImNodes::ClearNodeSelection();
        }
    } else if (num_selected_links > 0) {
        app.s_editor.selected_links.resize(num_selected_links);

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
            std::fill_n(app.s_editor.selected_links.begin(),
                        app.s_editor.selected_links.size(),
                        -1);
            ImNodes::GetSelectedLinks(app.s_editor.selected_links.begin());
            std::sort(app.s_editor.selected_links.begin(),
                      app.s_editor.selected_links.end(),
                      std::less<int>());

            int link_id_to_delete = app.s_editor.selected_links[0];
            int current_link_id   = 0;
            int i                 = 0;

            auto selected_links_ptr  = app.s_editor.selected_links.Data;
            auto selected_links_size = app.s_editor.selected_links.Size;

            model* mdl = nullptr;
            while (app.s_editor.sim.models.next(mdl) &&
                   link_id_to_delete != -1) {
                dispatch(
                  *mdl,
                  [&app,
                   &i,
                   &current_link_id,
                   selected_links_ptr,
                   selected_links_size,
                   &link_id_to_delete]<typename Dynamics>(Dynamics& dyn) {
                      if constexpr (is_detected_v<has_output_port_t,
                                                  Dynamics>) {
                          const int e = length(dyn.y);
                          int       j = 0;

                          while (j < e && link_id_to_delete != -1) {
                              auto list =
                                append_node(app.s_editor.sim, dyn.y[j]);
                              auto it = list.begin();
                              auto et = list.end();

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
            app.s_editor.selected_links.resize(0);
            ImNodes::ClearLinkSelection();
        }
    }
}

void application::show_simulation_editor_widget() noexcept
{
    const bool can_be_initialized = !match(s_editor.simulation_state,
                                           simulation_status::not_started,
                                           simulation_status::finished,
                                           simulation_status::initialized,
                                           simulation_status::not_started);

    const bool can_be_started =
      !match(s_editor.simulation_state, simulation_status::initialized);

    const bool can_be_paused = !match(s_editor.simulation_state,
                                      simulation_status::running,
                                      simulation_status::run_requiring,
                                      simulation_status::paused);

    const bool can_be_restarted =
      !match(s_editor.simulation_state, simulation_status::pause_forced);

    const bool can_be_stopped = !match(s_editor.simulation_state,
                                       simulation_status::running,
                                       simulation_status::run_requiring,
                                       simulation_status::paused,
                                       simulation_status::pause_forced);

    ImGui::PushItemWidth(100.f);
    ImGui::InputReal("Begin", &s_editor.simulation_begin);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::Checkbox("Edit", &s_editor.allow_user_changes);

    ImGui::InputReal("End", &s_editor.simulation_end);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
    if (ImGui::Checkbox("Debug", &s_editor.store_all_changes)) {
        if (s_editor.store_all_changes &&
            s_editor.simulation_state == simulation_status::running) {
            s_editor.enable_or_disable_debug();
        }
    }

    ImGui::BeginDisabled(!s_editor.real_time);
    ImGui::InputScalar("Micro second for 1 unit time",
                       ImGuiDataType_S64,
                       &s_editor.simulation_real_time_relation);
    ImGui::EndDisabled();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::Checkbox("No time limit", &s_editor.infinity_simulation);

    ImGui::TextFormat("Current time {:.6f}", s_editor.simulation_current);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::Checkbox("Real time", &s_editor.real_time);

    ImGui::TextFormat("Simulation phase: {}",
                      ordinal(s_editor.simulation_state));

    ImGui::PopItemWidth();

    ImGui::BeginDisabled(can_be_initialized);
    if (ImGui::Button("Import model")) {
        s_editor.simulation_copy_modeling();
    }
    ImGui::SameLine();

    if (ImGui::Button("init")) {
        s_editor.simulation_init();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_started);
    if (ImGui::Button("start"))
        s_editor.simulation_start();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(can_be_paused);
    if (ImGui::Button("pause")) {
        s_editor.force_pause = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_restarted);
    if (ImGui::Button("continue")) {
        s_editor.simulation_start();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(can_be_stopped);
    if (ImGui::Button("stop")) {
        s_editor.force_stop = true;
    }
    ImGui::EndDisabled();

    const bool history_mode =
      (s_editor.store_all_changes || s_editor.allow_user_changes) &&
      (can_be_started || can_be_restarted);

    ImGui::BeginDisabled(!history_mode);

    if (s_editor.store_all_changes) {
        ImGui::SameLine();
        if (ImGui::Button("step-by-step"))
            s_editor.simulation_start_1();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!s_editor.tl.can_back());
    if (ImGui::Button("<"))
        s_editor.simulation_back();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!s_editor.tl.can_advance());
    if (ImGui::Button(">"))
        s_editor.simulation_advance();
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (s_editor.tl.current_bag != s_editor.tl.points.rend()) {
        ImGui::TextFormat(
          "debug bag: {}/{}", s_editor.tl.current_bag->bag, s_editor.tl.bag);
    } else {
        ImGui::TextFormat("debug bag: {}", s_editor.tl.bag);
    }

    ImGui::EndDisabled();

    show_simulation_graph_editor(*this);
}

} // namesapce irt
