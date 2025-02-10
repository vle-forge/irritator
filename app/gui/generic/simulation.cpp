// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

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

namespace irt {

const char* observable_type_names[] = {
    "none", "file", "plot", "graph", "grid",
};

static int make_input_node_id(const irt::model_id mdl, const int port) noexcept
{
    debug::ensure(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(irt::get_index(mdl));
    debug::ensure(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

static int make_output_node_id(const irt::model_id mdl, const int port) noexcept
{
    debug::ensure(port >= 0 && port < 8);

    irt::u32 index = static_cast<u32>(irt::get_index(mdl));
    debug::ensure(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

static std::pair<irt::u32, irt::u32> get_model_input_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    debug::ensure(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

static std::pair<irt::u32, irt::u32> get_model_output_port(
  const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    debug::ensure(port >= 8u && port < 16u);
    port -= 8u;
    debug::ensure(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

template<typename Dynamics>
static void add_input_attribute(project_editor& ed,
                                const Dynamics& dyn) noexcept
{
    if constexpr (has_input_port<Dynamics>) {
        const auto** names  = get_input_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = ed.pj.sim.models.get_id(mdl);
        const auto   e      = length(dyn.x);

        debug::ensure(names != nullptr);
        debug::ensure(ed.pj.sim.models.try_to_get(mdl_id) == &mdl);
        debug::ensure(0 <= e && e < 8);

        for (int i = 0; i != e; ++i) {
            ImNodes::BeginInputAttribute(make_input_node_id(mdl_id, i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(project_editor& ed,
                                 const Dynamics& dyn) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        const auto** names  = get_output_port_names<Dynamics>();
        const auto&  mdl    = get_model(dyn);
        const auto   mdl_id = ed.pj.sim.models.get_id(mdl);
        const auto   e      = length(dyn.y);

        debug::ensure(names != nullptr);
        debug::ensure(ed.pj.sim.models.try_to_get(mdl_id) == &mdl);
        debug::ensure(0 <= e && e < 8);

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
    auto*      mdl = sim.models.try_to_get_from_pos(model_index_port.first);

    return { mdl, static_cast<int>(model_index_port.second) };
}

gport get_out(simulation& sim, const int index) noexcept
{
    const auto model_index_port = get_model_output_port(index);
    auto*      mdl = sim.models.try_to_get_from_pos(model_index_port.first);

    return { mdl, static_cast<int>(model_index_port.second) };
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.dQ);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.dQ);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.dQ);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss1_sum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss1_sum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss1_sum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_multiplier& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_wsum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_wsum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_wsum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss2_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss2_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss2_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss3_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss3_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss3_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void show_dynamics_values(project_editor& /*sim*/, const counter& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
}

static void show_dynamics_values(project_editor& sim, const queue& dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front()[0]);
        ImGui::TextFormat("next value {}", ar->front()[1]);
    }
}

static void show_dynamics_values(project_editor& sim, const dynamic_queue& dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front()[0]);
        ImGui::TextFormat("next value {}", ar->front()[1]);
    }
}

static void show_dynamics_values(project_editor& sim, const priority_queue& dyn)
{
    auto* ar = sim.pj.sim.dated_messages.try_to_get(dyn.fifo);

    if (not ar) {
        ImGui::TextFormat("empty");
    } else {
        ImGui::TextFormat("next ta {}", ar->front()[0]);
        ImGui::TextFormat("next value {}", ar->front()[1]);
    }
}

static void show_dynamics_values(project_editor& /*sim*/, const generator& dyn)
{
    ImGui::TextFormat("next {}", dyn.sigma);
}

static void show_dynamics_values(project_editor& sim, constant& dyn)
{
    ImGui::TextFormat("next ta {}", dyn.sigma);
    ImGui::InputDouble("value", &dyn.value);

    if (not sim.have_send_message.has_value() and
        ImGui::Button("Send value now")) {
        auto& mdl             = get_model(dyn);
        auto  mdl_id          = sim.pj.sim.models.get_id(mdl);
        sim.have_send_message = mdl_id;
    }
}

template<int QssLevel>
static void show_dynamics_values(project_editor& /*sim*/,
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
static void show_dynamics_values(project_editor& /*sim*/,
                                 const abstract_filter<QssLevel>& dyn)
{
    ImGui::TextFormat("value: {}", dyn.value[0]);
    ImGui::TextFormat("lower-threshold: {}", dyn.lower_threshold);
    ImGui::TextFormat("upper-threshold: {}", dyn.upper_threshold);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss1_power& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss2_power& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(project_editor& /*sim*/, const qss3_power& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss1_square& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss2_square& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const qss3_square& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const accumulator_2& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
    ImGui::TextFormat("- 0: {}", dyn.numbers[0]);
    ImGui::TextFormat("- 1: {}", dyn.numbers[1]);
}

static void show_dynamics_values(project_editor& /*sim*/, const time_func& dyn)
{
    ImGui::TextFormat("value {}", dyn.value);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const logical_and_2& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const logical_or_2& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const logical_and_3& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
    ImGui::TextFormat("- 2 {}", dyn.values[2]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const logical_or_3& dyn)
{
    ImGui::TextFormat("value {}", dyn.is_valid);
    ImGui::TextFormat("- 0 {}", dyn.values[0]);
    ImGui::TextFormat("- 1 {}", dyn.values[1]);
    ImGui::TextFormat("- 2 {}", dyn.values[2]);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const logical_invert& dyn)
{
    ImGui::TextFormat("value {}", dyn.value);
}

static void show_dynamics_values(project_editor& /*sim*/,
                                 const hsm_wrapper& dyn)
{
    ImGui::TextFormat("state={}",
                      static_cast<unsigned>(dyn.exec.current_state));
    ImGui::TextFormat("i1={}", dyn.exec.i1);
    ImGui::TextFormat("i2={}", dyn.exec.i2);
    ImGui::TextFormat("r1={}", dyn.exec.r1);
    ImGui::TextFormat("r2={}", dyn.exec.r2);
    ImGui::TextFormat("sigma={}", dyn.exec.timer);
}

struct generic_simulation_editor::impl {
    generic_simulation_editor& self;
    project_editor&            pj_ed;

    impl(generic_simulation_editor& ed) noexcept
      : self(ed)
      , pj_ed(container_of(&self, &project_editor::generic_sim))
    {}

    int add_popup_menuitem(application&    app,
                           project_editor& ed,
                           bool            enable_menu_item,
                           dynamics_type   type,
                           ImVec2          click_pos) noexcept
    {
        if (ImGui::MenuItem(dynamics_type_names[static_cast<i8>(type)],
                            nullptr,
                            nullptr,
                            enable_menu_item)) {
            // TODO Maybe we should add model and the update the vectors links
            // and nodes or just add an object "add model type at x, y"
            ed.start_simulation_model_add(app, type, click_pos.x, click_pos.y);
            return 1;
        }

        return 0;
    }

    status copy_port(simulation&                      sim,
                     const table<model_id, model_id>& mapping,
                     block_node_id&                   src,
                     block_node_id&                   dst) noexcept
    {
        if (is_undefined(src)) {
            dst = src;
            return success();
        }

        sim.for_each(src, [&](auto& mdl_src, int port_src) {
            const auto mdl_src_id = sim.get_id(mdl_src);
            if (auto* found = mapping.get(mdl_src_id); found) {
                // TODO
                (void)sim.connect(dst, *found, port_src);
            } else {
                (void)sim.connect(dst, mdl_src_id, port_src);
            }
        });

        return success();
    }

    bool is_in_node(const model_id mdl_id) const noexcept
    {
        for (auto i = 0, e = self.nodes_2nd.ssize(); i < e; ++i)
            if (self.nodes_2nd[i].mdl == mdl_id)
                return true;

        return false;
    }

    int get_index_from_nodes_2nd(const model_id mdl_id) const noexcept
    {
        for (auto i = 0, e = self.nodes_2nd.ssize(); i < e; ++i)
            if (mdl_id == self.nodes_2nd[i].mdl)
                return i;

        irt::unreachable();
    }

    void build_links(const simulation& sim, const tree_node& tn) noexcept
    {
        for (const auto& child : tn.children) {
            if (child.type == tree_node::child_node::type::model and
                child.mdl) {
                dispatch(
                  *child.mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
                      if constexpr (has_output_port<Dynamics>) {
                          const auto src_id  = sim.get_id(dyn);
                          const auto src_idx = get_index_from_nodes_2nd(src_id);
                          for (int i = 0, e = length(dyn.y); i != e; ++i) {
                              sim.for_each(
                                dyn.y[i], [&](auto& dst, auto dst_port) {
                                    const auto dst_id = sim.get_id(dst);
                                    if (is_in_node(dst_id)) {
                                        self.links_2nd.emplace_back(
                                          make_output_node_id(src_id, i),
                                          make_input_node_id(dst_id, dst_port),
                                          src_idx,
                                          get_index_from_nodes_2nd(dst_id));
                                    }
                                });
                          }
                      }
                  });
            }
        }
    }

    void build_nodes(const simulation& sim, const tree_node& tn) noexcept
    {
        for (const auto& child : tn.children) {
            if (child.type == tree_node::child_node::type::model) {
                if (child.mdl)
                    self.nodes_2nd.emplace_back(sim.get_id(*child.mdl));
            }
        }
    }

    void build_flat_links(const simulation& sim) noexcept
    {
        for (const auto& mdl : sim.models) {
            dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
                if constexpr (has_output_port<Dynamics>) {
                    const auto src_id  = sim.get_id(dyn);
                    const auto src_idx = get_index_from_nodes_2nd(src_id);
                    for (int i = 0, e = length(dyn.y); i != e; ++i) {
                        sim.for_each(dyn.y[i], [&](auto& dst, auto dst_port) {
                            const auto dst_id = sim.get_id(dst);
                            self.links_2nd.emplace_back(
                              make_output_node_id(src_id, i),
                              make_input_node_id(dst_id, dst_port),
                              src_idx,
                              get_index_from_nodes_2nd(dst_id));
                        });
                    }
                }
            });
        }
    }

    void build_flat_nodes(const simulation& sim) noexcept
    {
        for (const auto& mdl : sim.models)
            self.nodes_2nd.emplace_back(sim.get_id(mdl));
    }

    status copy(project_editor& ed, const vector<int>& nodes) noexcept
    {
        table<model_id, model_id> mapping;
        mapping.data.reserve(nodes.size());

        for (int i = 0, e = nodes.size(); i != e; ++i) {
            auto* src_mdl = ed.pj.sim.models.try_to_get_from_pos(nodes[i]);
            if (!src_mdl)
                continue;

            if (!ed.pj.sim.can_alloc(1))
                return new_error(simulation::part::models,
                                 container_full_error{});

            auto& dst_mdl    = ed.pj.sim.clone(*src_mdl);
            auto  src_mdl_id = ed.pj.sim.models.get_id(src_mdl);
            auto  dst_mdl_id = ed.pj.sim.models.get_id(dst_mdl);

            irt_check(ed.pj.sim.make_initialize(dst_mdl, ed.pj.sim.t));

            mapping.data.emplace_back(src_mdl_id, dst_mdl_id);
        }

        mapping.sort();

        for (const auto& m : mapping.data) {
            auto& src_mdl = ed.pj.sim.models.get(m.id);
            auto& dst_mdl = ed.pj.sim.models.get(m.value);

            if (auto ret = dispatch(
                  src_mdl,
                  [&]<typename Dynamics>(Dynamics& dyn) noexcept -> status {
                      if constexpr (has_output_port<Dynamics>) {
                          for (int i = 0, e = length(dyn.y); i != e; ++i) {
                              auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);

                              irt_check(copy_port(
                                ed.pj.sim, mapping, dyn.y[i], dst_dyn.y[i]));
                          }
                      }
                      return success();
                  });
                !ret)
                return ret.error();
        }

        return success();
    }

    void free_children(application&       app,
                       project_editor&    ed,
                       const vector<int>& nodes) noexcept
    {
        const auto tasks = std::min(nodes.ssize(), task_list_tasks_number);

        for (int i = 0; i < tasks; ++i) {
            if (const auto* mdl =
                  ed.pj.sim.models.try_to_get_from_pos(nodes[i]);
                mdl) {
                ed.start_simulation_model_del(app,
                                              ed.pj.sim.models.get_id(mdl));
            }
        }
    }

    void compute_connection_distance(const float       k,
                                     std::span<ImVec2> displacements) noexcept
    {
        for (const auto& link : self.links) {
            const auto out = get_model_output_port(link.out);
            const auto in  = get_model_input_port(link.in);

            const auto u_pos = ImNodes::GetNodeEditorSpacePos(out.first);
            const auto v_pos = ImNodes::GetNodeEditorSpacePos(in.first);

            const float dx = v_pos.x - u_pos.x;
            const float dy = v_pos.y - u_pos.y;

            if (dx and dy) {
                const float d2    = dx * dx / dy * dy;
                const float coeff = std::sqrt(d2) / k;

                displacements[link.mdl_out].x += dx * coeff;
                displacements[link.mdl_out].y += dy * coeff;
                displacements[link.mdl_in].x -= dx * coeff;
                displacements[link.mdl_in].y -= dy * coeff;
            }
        }
    }

    void compute_automatic_layout(application&    app,
                                  vector<ImVec2>& displacements) noexcept
    {
        if (self.nodes.empty())
            return;

        auto& settings = app.settings_wnd;

        /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
           Fruchterman and Edward M. Reingold in Software--Pratice and
           Experience, Vol. 21(1 1), 1129-1164 (november 1991).
           */

        const auto size      = self.nodes.ssize();
        const auto tmp       = std::sqrt(size);
        const auto column    = static_cast<int>(tmp);
        const auto line      = column;
        const auto remaining = size - (column * line);

        const float W =
          static_cast<float>(column) * settings.automatic_layout_x_distance;
        const float L =
          static_cast<float>(line) +
          ((remaining > 0) ? settings.automatic_layout_y_distance : 0.f);
        const float area     = W * L;
        const float k_square = area / static_cast<float>(size);
        const float k        = std::sqrt(k_square);

        // float t = 1.f - static_cast<float>(iteration) /
        //                   static_cast<float>(automatic_layout_iteration_limit);
        // t *= t;

        displacements.resize(size);

        float t = 1.f - 1.f / static_cast<float>(
                                settings.automatic_layout_iteration_limit);

        for (int i_v = 0; i_v < size; ++i_v) {
            const int v = get_index(self.nodes[i_v].mdl);

            displacements[i_v].x = 0.f;
            displacements[i_v].y = 0.f;

            for (int i_u = 0; i_u < size; ++i_u) {
                const int u = get_index(self.nodes[i_u].mdl);

                if (u != v) {
                    const auto u_pos = ImNodes::GetNodeEditorSpacePos(u);
                    const auto v_pos = ImNodes::GetNodeEditorSpacePos(v);
                    const auto delta =
                      ImVec2{ v_pos.x - u_pos.x, v_pos.y - u_pos.y };

                    if (delta.x && delta.y) {
                        const float d2 = delta.x * delta.x + delta.y * delta.y;
                        const float coeff = k_square / d2;

                        displacements[i_v].x += coeff * delta.x;
                        displacements[i_v].y += coeff * delta.y;
                    }
                }
            }
        }

        compute_connection_distance(
          k, std::span(displacements.data(), displacements.size()));

        for (int i_v = 0; i_v < size; ++i_v) {
            const int v = get_index(self.nodes[i_v].mdl);

            const float d2 = displacements[i_v].x * displacements[i_v].x +
                             displacements[i_v].y * displacements[i_v].y;
            const float d = std::sqrt(d2);

            if (d > t) {
                const float coeff = t / d;
                displacements[i_v].x *= coeff;
                displacements[i_v].y *= coeff;
            }

            auto v_pos = ImNodes::GetNodeEditorSpacePos(v);
            v_pos.x += displacements[i_v].x;
            v_pos.y += displacements[i_v].y;

            ImNodes::SetNodeEditorSpacePos(v, v_pos);
        }
    }

    void compute_grid_layout(application& app) noexcept
    {
        auto& settings = app.settings_wnd;

        const auto size  = self.nodes.ssize();
        const auto fsize = static_cast<float>(size);

        if (size == 0)
            return;

        const auto column    = static_cast<int>(std::floor(std::sqrt(fsize)));
        const auto line      = column;
        const auto remaining = size - (column * line);
        const auto panning   = ImNodes::EditorContextGetPanning();
        auto       new_pos   = panning;

        int index = 0;
        for (auto i = 0; i < line; ++i) {
            new_pos.y = panning.y +
                        static_cast<float>(i) * settings.grid_layout_y_distance;

            for (auto j = 0; j < column; ++j) {
                new_pos.x = panning.x + static_cast<float>(j) *
                                          settings.grid_layout_x_distance;
                ImNodes::SetNodeEditorSpacePos(get_index(self.nodes[index].mdl),
                                               new_pos);
                ++index;
            }
        }

        new_pos.x = panning.x;
        new_pos.y = panning.y + column * settings.grid_layout_y_distance;

        for (auto j = 0; j < remaining; ++j) {
            new_pos.x = panning.x +
                        static_cast<float>(j) * settings.grid_layout_x_distance;
            ImNodes::SetNodeEditorSpacePos(get_index(self.nodes[index].mdl),
                                           new_pos);
            ++index;
        }
    }

    int show_menu(application& app, ImVec2 click_pos) noexcept
    {
        const bool open_popup =
          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
          ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

        auto ret = 0;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
        if (!ImGui::IsAnyItemHovered() && open_popup)
            ImGui::OpenPopup("Context menu");

        if (ImGui::BeginPopup("Context menu")) {
            if (ImGui::MenuItem("Force grid layout")) {
                compute_grid_layout(app);
            }

            if (ImGui::MenuItem("Force automatic layout")) {
                self.automatic_layout_iteration =
                  app.settings_wnd.automatic_layout_iteration_limit;
            }

            ImGui::MenuItem(
              "Show internal values", "", &self.show_internal_values);
            ImGui::MenuItem(
              "Show internal parameters", "", &self.show_parameter_values);
            ImGui::MenuItem("Show identifiers", "", &self.show_identifiers);

            ImGui::Separator();

            const auto can_edit = pj_ed.can_edit();

            if (ImGui::BeginMenu("QSS1")) {
                auto       i = static_cast<int>(dynamics_type::qss1_integrator);
                const auto e = static_cast<int>(dynamics_type::qss1_wsum_4) + 1;
                for (; i != e; ++i)
                    ret += add_popup_menuitem(app,
                                              pj_ed,
                                              can_edit,
                                              static_cast<dynamics_type>(i),
                                              click_pos);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS2")) {
                auto       i = static_cast<int>(dynamics_type::qss2_integrator);
                const auto e = static_cast<int>(dynamics_type::qss2_wsum_4) + 1;

                for (; i != e; ++i)
                    ret += add_popup_menuitem(app,
                                              pj_ed,
                                              can_edit,
                                              static_cast<dynamics_type>(i),
                                              click_pos);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS3")) {
                auto       i = static_cast<int>(dynamics_type::qss3_integrator);
                const auto e = static_cast<int>(dynamics_type::qss3_wsum_4) + 1;

                for (; i != e; ++i)
                    ret += add_popup_menuitem(app,
                                              pj_ed,
                                              can_edit,
                                              static_cast<dynamics_type>(i),
                                              click_pos);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Logical")) {
                ret += add_popup_menuitem(app,
                                          pj_ed,
                                          can_edit,
                                          dynamics_type::logical_and_2,
                                          click_pos);
                ret += add_popup_menuitem(
                  app, pj_ed, can_edit, dynamics_type::logical_or_2, click_pos);
                ret += add_popup_menuitem(app,
                                          pj_ed,
                                          can_edit,
                                          dynamics_type::logical_and_3,
                                          click_pos);
                ret += add_popup_menuitem(
                  app, pj_ed, can_edit, dynamics_type::logical_or_3, click_pos);
                ret += add_popup_menuitem(app,
                                          pj_ed,
                                          can_edit,
                                          dynamics_type::logical_invert,
                                          click_pos);
                ImGui::EndMenu();
            }

            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::counter, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::queue, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::dynamic_queue, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::priority_queue, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::generator, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::constant, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::time_func, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::accumulator_2, click_pos);
            ret += add_popup_menuitem(
              app, pj_ed, can_edit, dynamics_type::hsm_wrapper, click_pos);

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        return ret;
    }

    int try_create_connection(application& app, project_editor& ed) noexcept
    {
        int start = 0, end = 0;

        if (!(ImNodes::IsLinkCreated(&start, &end) && ed.can_edit()))
            return 0;

        const gport out = get_out(ed.pj.sim, start);
        const gport in  = get_in(ed.pj.sim, end);

        if (!(out.model && in.model && ed.pj.sim.can_connect(1)))
            return 0;

        if (!is_ports_compatible(
              *out.model, out.port_index, *in.model, in.port_index))
            return 0;

        return attempt_all(
          [&]() noexcept -> result<int> {
              irt_check(ed.pj.sim.connect(
                *out.model, out.port_index, *in.model, in.port_index));
              return 1;
          },

          [&app](const simulation::part s) noexcept -> int {
              auto& notif = app.notifications.alloc(log_level::warning);
              notif.title = "Fail to create connection";
              format(notif.message, "Error: {}", ordinal(s));
              app.notifications.enable(notif);
              return 0;
          },

          [&app]() noexcept -> int {
              auto& notif   = app.notifications.alloc(log_level::warning);
              notif.title   = "Fail to create connection";
              notif.message = "Error: Unknown";
              app.notifications.enable(notif);
              return 0;
          });
    }

    template<typename Dynamics>
    void show_input_attribute(const Dynamics& dyn, const model_id id) noexcept
    {
        if constexpr (has_input_port<Dynamics>) {
            const auto** names = get_input_port_names<Dynamics>();
            const auto   e     = length(dyn.x);

            debug::ensure(names != nullptr);
            debug::ensure(0 <= e && e < 8);

            for (int i = 0; i != e; ++i) {
                ImNodes::BeginInputAttribute(make_input_node_id(id, i),
                                             ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted(names[i]);
                ImNodes::EndInputAttribute();
            }
        }
    }

    template<typename Dynamics>
    void show_output_attribute(const Dynamics& dyn, const model_id id) noexcept
    {
        if constexpr (has_output_port<Dynamics>) {
            const auto** names = get_output_port_names<Dynamics>();
            const auto   e     = length(dyn.y);

            debug::ensure(names != nullptr);
            debug::ensure(0 <= e && e < 8);

            for (int i = 0; i != e; ++i) {
                ImNodes::BeginOutputAttribute(make_output_node_id(id, i),
                                              ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted(names[i]);
                ImNodes::EndOutputAttribute();
            }
        }
    }

    template<typename Dynamics>
    void show_node_values(project_editor& ed, Dynamics& dyn) noexcept
    {
        ImGui::PushID(0);
        ImGui::PushItemWidth(120.0f);
        show_dynamics_values(ed, dyn);
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    void show_nodes(application&                                   app,
                    const vector<generic_simulation_editor::node>& nodes,
                    bool show_identifiers) noexcept
    {
        for (auto i = 0, e = nodes.ssize(); i != e; ++i) {
            if (auto* mdl = pj_ed.pj.sim.models.try_to_get(nodes[i].mdl)) {
                ImNodes::BeginNode(get_index(nodes[i].mdl));
                ImNodes::BeginNodeTitleBar();

                if (show_identifiers) {
                    ImGui::TextFormat(
                      "{}\n{}",
                      get_index(pj_ed.pj.sim.models.get_id(*mdl)),
                      dynamics_type_names[ordinal(mdl->type)]);
                } else {
                    ImGui::TextUnformatted(
                      dynamics_type_names[ordinal(mdl->type)]);
                }

                ImNodes::EndNodeTitleBar();

                const auto mdl_id = nodes[i].mdl;

                dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                    show_input_attribute(dyn, mdl_id);

                    if (self.show_internal_values) {
                        ImGui::PushID(0);
                        ImGui::PushItemWidth(120.0f);
                        show_dynamics_values(pj_ed, dyn);
                        ImGui::PopItemWidth();
                        ImGui::PopID();
                    }

                    if (self.can_edit_parameters) {
                        ImGui::PushID(1);
                        ImGui::PushItemWidth(120.0f);
                        show_parameter_editor(
                          app,
                          pj_ed.pj.sim.srcs,
                          mdl->type,
                          pj_ed.pj.sim.parameters[get_index(
                            pj_ed.pj.sim.models.get_id(mdl))]);
                        ImGui::PopItemWidth();
                        ImGui::PopID();
                    }

                    show_output_attribute(dyn, mdl_id);
                });
            }

            ImNodes::EndNode();
        }
    }

    void show_links(
      const vector<generic_simulation_editor::link>& links) noexcept
    {
        for (auto i = 0, e = links.ssize(); i != e; ++i) {
            ImNodes::Link(i, links[i].out, links[i].in);
        }
    }

    void rebuild(application& app) noexcept
    {
        self.enable_show = false;

        app.add_gui_task([&]() {
            self.links_2nd.clear();
            self.nodes_2nd.clear();

            if (auto* tn = pj_ed.pj.tree_nodes.try_to_get(self.current)) {
                build_nodes(pj_ed.pj.sim, *tn);
                build_links(pj_ed.pj.sim, *tn);
            } else {
                build_flat_nodes(pj_ed.pj.sim);
                build_flat_links(pj_ed.pj.sim);
            }

            if (std::unique_lock lock(self.mutex); lock.owns_lock()) {
                std::swap(self.links, self.links_2nd);
                std::swap(self.nodes, self.nodes_2nd);
            }

            self.enable_show = true;
        });
    }
};

generic_simulation_editor::generic_simulation_editor() noexcept
{
    context = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;
}

generic_simulation_editor::~generic_simulation_editor() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void generic_simulation_editor::init(application&     app,
                                     const tree_node& tn,
                                     component& /*compo*/,
                                     generic_component& /*gen*/) noexcept
{
    enable_show = false;

    app.add_gui_task([&]() {
        auto& pj_ed = container_of(this, &project_editor::generic_sim);
        generic_simulation_editor::impl impl(*this);

        links.clear();
        nodes.clear();

        const auto tn_id = pj_ed.pj.tree_nodes.get_id(tn);
        impl.build_nodes(pj_ed.pj.sim, tn);
        impl.build_links(pj_ed.pj.sim, tn);

        if (std::unique_lock lock(mutex); lock.owns_lock()) {
            std::swap(links, links_2nd);
            std::swap(nodes, nodes_2nd);
            current     = tn_id;
            enable_show = true;
        }
    });
}

void generic_simulation_editor::init(application& app,
                                     simulation& /*sim*/) noexcept
{
    enable_show = false;

    app.add_gui_task([&]() {
        auto& pj_ed = container_of(this, &project_editor::generic_sim);
        generic_simulation_editor::impl impl(*this);

        links.clear();
        nodes.clear();

        impl.build_flat_nodes(pj_ed.pj.sim);
        impl.build_flat_links(pj_ed.pj.sim);

        if (std::unique_lock lock(mutex); lock.owns_lock()) {
            std::swap(nodes, nodes_2nd);
            std::swap(links, links_2nd);
            current     = undefined<tree_node_id>();
            enable_show = true;
        }
    });
}

bool generic_simulation_editor::display(application& app) noexcept
{
    auto& pj_ed = container_of(this, &project_editor::generic_sim);
    generic_simulation_editor::impl impl(*this);

    if (std::unique_lock lock(mutex, std::try_to_lock); lock.owns_lock()) {
        if (enable_show) {
            ImNodes::EditorContextSet(context);
            ImNodes::BeginNodeEditor();

            auto changes = 0;

            if (automatic_layout_iteration > 0) {
                impl.compute_automatic_layout(app, displacements);
                --automatic_layout_iteration;
            }

            impl.show_nodes(app, nodes, show_identifiers);
            impl.show_links(links);

            auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
            changes += impl.show_menu(app, click_pos);

            if (show_minimap)
                ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

            ImNodes::EndNodeEditor();

            changes += impl.try_create_connection(app, pj_ed);

            auto num_selected_links = ImNodes::NumSelectedLinks();
            auto num_selected_nodes = ImNodes::NumSelectedNodes();

            if (num_selected_nodes == 0) {
                selected_nodes.clear();
                ImNodes::ClearNodeSelection();
            }

            if (num_selected_links == 0) {
                selected_links.clear();
                ImNodes::ClearLinkSelection();
            }

            if (can_edit and num_selected_nodes > 0) {
                selected_nodes.resize(num_selected_nodes, -1);
                ImNodes::GetSelectedNodes(selected_nodes.begin());

                if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
                    impl.free_children(app, pj_ed, selected_nodes);
                    selected_nodes.clear();
                    num_selected_nodes = 0;
                    ++changes;
                    ImNodes::ClearNodeSelection();
                } else if (ImGui::IsKeyReleased(ImGuiKey_D)) {
                    if (auto ret = impl.copy(pj_ed, selected_nodes); !ret) {
                        app.notifications.try_insert(
                          log_level::error,
                          [&](auto& title, auto& /*msg*/) noexcept {
                              title = "Fail to copy selected nodes";
                          });
                    }
                    selected_nodes.clear();
                    num_selected_nodes = 0;
                    ImNodes::ClearNodeSelection();
                    ++changes;
                }
            } else if (can_edit and num_selected_links > 0) {
                selected_links.resize(num_selected_links);

                if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
                    std::fill_n(
                      selected_links.begin(), selected_links.size(), -1);
                    ImNodes::GetSelectedLinks(selected_links.begin());

                    for (const auto link_index : selected_links) {
                        auto out = get_out(pj_ed.pj.sim, links[link_index].out);
                        auto in  = get_out(pj_ed.pj.sim, links[link_index].in);

                        if (out.model and in.model)
                            pj_ed.pj.sim.disconnect(*out.model,
                                                    out.port_index,
                                                    *in.model,
                                                    in.port_index);
                    }

                    selected_links.resize(0);
                    ImNodes::ClearLinkSelection();
                    ++changes;
                }
            }

            if (changes > 0)
                impl.rebuild(app);
        }
    }

    return false;
}

} // namespace irt
