// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"

#include <cstring>

namespace irt {

constexpr auto child_tag  = 0b11;
constexpr auto input_tag  = 0b10;
constexpr auto output_tag = 0b01;
constexpr auto shift_tag  = 2;
constexpr auto mask_tag   = 0b11;

static inline bool is_node_child(const int node) noexcept
{
    return (mask_tag & node) == child_tag;
}

static inline bool is_node_X(const int node) noexcept
{
    return (mask_tag & node) == input_tag;
}

static inline bool is_node_Y(const int node) noexcept
{
    return (mask_tag & node) == output_tag;
}

static inline int pack_node_child(const child_id child) noexcept
{
    return static_cast<int>((get_index(child) << shift_tag) | child_tag);
}

static inline int unpack_node_child(const int node) noexcept
{
    debug::ensure(is_node_child(node));
    return node >> shift_tag;
}

static inline int pack_node_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | input_tag);
}

static inline int unpack_node_X(const int node) noexcept
{
    debug::ensure(is_node_X(node));
    return node >> shift_tag;
}

static inline int pack_node_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | output_tag);
}

static inline int unpack_node_Y(const int node) noexcept
{
    debug::ensure(is_node_Y(node));
    return node >> shift_tag;
}

constexpr auto mask_port               = 0b111;
constexpr auto input_component_port    = 0b111;
constexpr auto input_child_model_port  = 0b110;
constexpr auto input_child_compo_port  = 0b101;
constexpr auto output_component_port   = 0b011;
constexpr auto output_child_model_port = 0b010;
constexpr auto output_child_compo_port = 0b001;
constexpr auto shift_port              = 3;
constexpr auto shift_child_port        = 16;

static inline bool is_input_X(const int attribute) noexcept
{
    return (attribute & mask_port) == input_component_port;
}

static inline bool is_output_Y(const int attribute) noexcept
{
    return (attribute & mask_port) == output_component_port;
}

static inline bool is_input_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_model_port;
}

static inline bool is_input_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_compo_port;
}

static inline bool is_output_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_model_port;
}

static inline bool is_output_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_compo_port;
}

static inline int pack_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>((get_index(port) << shift_port) |
                            input_component_port);
}

static inline u32 unpack_X(const int attribute) noexcept
{
    debug::ensure(is_input_X(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

static inline int pack_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>(
      (get_index(port) << shift_port | output_component_port));
}

static inline u32 unpack_Y(const int attribute) noexcept
{
    debug::ensure(is_output_Y(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

static inline int pack_in(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_model_port;
    return ret;
}

static inline int pack_in(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_compo_port;
    return ret;
}

static inline std::pair<u32, u32> unpack_in(const int attribute) noexcept
{
    debug::ensure(!is_input_X(attribute));
    debug::ensure(is_input_child_model(attribute) ||
                  is_input_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

static inline int pack_out(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_model_port;
    return ret;
}

static inline int pack_out(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_compo_port;
    return ret;
}

static inline std::pair<u32, u32> unpack_out(const int attribute) noexcept
{
    debug::ensure(!is_output_Y(attribute));
    debug::ensure(is_output_child_model(attribute) ||
                  is_output_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

static void add_input_attribute(const std::string_view name,
                                const child_id         id,
                                const int              idx) noexcept
{
    ImNodes::BeginInputAttribute(pack_in(id, idx),
                                 ImNodesPinShape_TriangleFilled);
    ImGui::TextFormat("{}", name);
    ImNodes::EndInputAttribute();
}

static void add_input_attribute(const std::span<const std::string_view> names,
                                const child_id id) noexcept
{
    for (auto i = 0, e = length(names); i != e; ++i)
        add_input_attribute(names[i], id, i);
}

static void add_output_attribute(const std::string_view name,
                                 const child_id         id,
                                 const int              idx) noexcept
{
    ImNodes::BeginOutputAttribute(pack_out(id, idx),
                                  ImNodesPinShape_TriangleFilled);
    ImGui::TextFormat("{}", name);
    ImNodes::EndOutputAttribute();
}

static void add_output_attribute(const std::span<const std::string_view> names,
                                 const child_id id) noexcept
{
    for (auto i = 0, e = length(names); i != e; ++i)
        add_output_attribute(names[i], id, i);
}

static bool delete_link(generic_component& gen, int link_id) noexcept
{
    if (link_id >= 8192) {
        const auto id = static_cast<u32>(link_id - 8192);
        if (auto* con = gen.output_connections.try_to_get_from_pos(id); con) {
            gen.output_connections.free(*con);
            return true;
        }
    } else if (link_id >= 4096) {
        const auto id = static_cast<u32>(link_id - 4096);
        if (auto* con = gen.input_connections.try_to_get_from_pos(id); con) {
            gen.input_connections.free(*con);
            return true;
        }
    } else {
        const auto id = static_cast<u32>(link_id);
        if (auto* con = gen.connections.try_to_get_from_pos(id); con) {
            gen.connections.free(*con);
            return true;
        }
    }

    return false;
}

static bool remove_nodes(generic_component&             gen,
                         generic_component_editor_data& data) noexcept
{
    auto u = 0;

    for (int i = 0, e = data.selected_nodes.size(); i != e; ++i) {
        if (is_node_child(data.selected_nodes[i])) {
            const auto idx = unpack_node_child(data.selected_nodes[i]);

            if (auto* child = gen.children.try_to_get_from_pos(idx)) {
                gen.children.free(*child);
                ++u;
            }
        }
    }

    data.selected_nodes.clear();
    ImNodes::ClearNodeSelection();

    return u > 0;
}

static bool remove_links(generic_component_editor_data& data,
                         generic_component&             s_parent) noexcept
{
    std::sort(data.selected_links.begin(),
              data.selected_links.end(),
              std::greater<int>());

    int update = false;
    for (i32 i = 0, e = data.selected_links.size(); i != e; ++i) {
        if (delete_link(s_parent, data.selected_links[i]))
            ++update;
    }

    data.selected_links.clear();
    ImNodes::ClearLinkSelection();

    return update;
}

static void remove_component_input_output(ImVector<int>& v) noexcept
{
    int i = 0;
    while (i < v.size()) {
        if (!is_node_child(v[i]))
            v.erase(v.Data + i);
        else
            ++i;
    };
}

static std::optional<int> get_y_port(const component_access&         ids,
                                     const generic_component&        gen,
                                     const generic_component::child& src,
                                     const child_id                  src_id,
                                     const connection::port& index_src) noexcept
{
    if (src.type == child_type::model) {
        if (src.id.mdl_type == dynamics_type::simulation_wrapper) {
            const auto& param     = gen.children_parameters[src_id];
            const auto  compo_idx = param.integers[simulation_wrapper_tag::id];
            const auto  compo_id  = enum_cast<component_id>(compo_idx);

            if (not ids.ids.exists(compo_id))
                return std::nullopt;

            const auto  sim_compo_id = ids.components[compo_id].id.sim_id;
            const auto* sim_compo = ids.sim_components.try_to_get(sim_compo_id);
            if (not sim_compo)
                return std::nullopt;

            if (not(std::cmp_greater_equal(index_src.model, 0) and
                    std::cmp_less(index_src.model,
                                  sim_compo->factors.size() + 2)))
                return std::nullopt;

            return pack_out(src_id, index_src.model);
        } else {
            return pack_out(src_id, index_src.model);
        }
    } else {
        return pack_out(src_id, index_src.compo);
    }
}

static std::optional<int> get_x_port(const component_access&         ids,
                                     const generic_component&        gen,
                                     const generic_component::child& dst,
                                     const child_id                  dst_id,
                                     const connection::port& index_dst) noexcept
{
    if (dst.type == child_type::model) {
        if (dst.id.mdl_type == dynamics_type::simulation_wrapper) {
            const auto& param     = gen.children_parameters[dst_id];
            const auto  compo_idx = param.integers[simulation_wrapper_tag::id];
            const auto  compo_id  = enum_cast<component_id>(compo_idx);

            if (not ids.ids.exists(compo_id))
                return std::nullopt;

            const auto  sim_compo_id = ids.components[compo_id].id.sim_id;
            const auto* sim_compo = ids.sim_components.try_to_get(sim_compo_id);
            if (not sim_compo)
                return std::nullopt;

            if (not(std::cmp_greater_equal(index_dst.model, 0) and
                    std::cmp_less(index_dst.model,
                                  sim_compo->factors.size() +
                                    sim_compo->selections.size())))
                return std::nullopt;

            return pack_in(dst_id, index_dst.model);
        } else {
            return pack_in(dst_id, index_dst.model);
        }
    } else {
        return pack_in(dst_id, index_dst.compo);
    }
}

static bool show_input_connection(
  const component_access&                    ids,
  const component&                           compo,
  const generic_component&                   gen,
  const generic_component::input_connection& con) noexcept
{
    const auto id     = gen.input_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 4096 + static_cast<int>(idx);

    if (compo.x.exists(con.x)) {
        if (auto* c = gen.children.try_to_get(con.dst)) {
            const auto dst = get_x_port(ids, gen, *c, con.dst, con.port);

            if (dst.has_value()) {
                const auto id_src = pack_X(con.x);
                ImNodes::Link(con_id, id_src, *dst);
                return false;
            }
        }
    }

    return true;
}

static bool show_output_connection(
  const component_access&                     ids,
  const component&                            compo,
  const generic_component&                    gen,
  const generic_component::output_connection& con) noexcept
{
    const auto id     = gen.output_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 8192 + static_cast<int>(idx);

    if (compo.y.exists(con.y)) {
        if (auto* c = gen.children.try_to_get(con.src); c) {
            const auto src = get_y_port(ids, gen, *c, con.src, con.port);

            if (src.has_value()) {
                const auto id_dst = pack_Y(con.y);
                ImNodes::Link(con_id, *src, id_dst);
                return false;
            }
        }
    }

    return true;
}

static bool show_connection(const component_access&  ids,
                            const generic_component& compo,
                            const connection&        con) noexcept
{
    const auto id     = compo.connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = static_cast<int>(idx);

    if (auto* s = compo.children.try_to_get(con.src)) {
        if (auto* d = compo.children.try_to_get(con.dst)) {
            const auto src = get_y_port(ids, compo, *s, con.src, con.index_src);
            const auto dst = get_x_port(ids, compo, *d, con.dst, con.index_dst);

            if (src.has_value() and dst.has_value()) {
                ImNodes::Link(con_id, *src, *dst);
                return false;
            }
        }
    }

    return true;
}

static bool show_node(const component_access&   ids,
                      component_editor&         ed,
                      component_id              compo_id,
                      component&                compo,
                      generic_component&        gen,
                      generic_component::child& c) noexcept
{
    const auto id  = gen.children.get_id(c);
    const auto idx = get_index(id);
    auto       up  = 0;

    auto& app              = container_of(&ed, &application::component_ed);
    u32   node_2_c         = 0u;
    u32   node_2_hovered_c = 0u;
    u32   node_2_active_c  = 0u;

    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          node_2_c         = to_ImU32(colors[style_color::node_2]);
          node_2_hovered_c = to_ImU32(colors[style_color::node_2_hovered]);
          node_2_active_c  = to_ImU32(colors[style_color::node_2_active]);
      });

    ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_2_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_2_hovered_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_2_active_c);

    ImNodes::BeginNode(pack_node_child(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      gen.children_names[idx].sv(),
                      dynamics_type_names[ordinal(c.id.mdl_type)]);
    ImNodes::EndNodeTitleBar();

    if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
        const auto& child_param = gen.children_parameters[idx];
        const auto compo_idx = child_param.integers[simulation_wrapper_tag::id];
        const auto compo_id  = enum_cast<component_id>(compo_idx);

        if (ids.ids.exists(compo_id) and
            ids.components[compo_id].type == component_type::simulation and
            ids.sim_components.exists(ids.components[compo_id].id.sim_id)) {
            const auto  sim_id          = ids.components[compo_id].id.sim_id;
            const auto& sim             = ids.sim_components.get(sim_id);
            const auto& factor_names    = sim.factors.get<name_str>();
            const auto& selection_names = sim.selections.get<name_str>();

            add_input_attribute("init", id, 0);
            add_input_attribute("run", id, 1);

            for (sz i = 0, e = sim.factors.size(); i != e; ++i) {
                add_input_attribute(
                  factor_names[i].sv(), id, static_cast<int>(i) + 2);
            }

            ImGui::PushItemWidth(120.0f);
            up += show_parameter_editor(
              app, compo.srcs, c.id.mdl_type, gen.children_parameters[idx]);
            ImGui::PopItemWidth();

            sz port_index = 0;

            for (sz i = 0, e = sim.factors.size(); i < e; ++i) {
                add_output_attribute(
                  factor_names[i].sv(), id, static_cast<int>(port_index));

                ++port_index;
            }

            for (sz i = 0, e = sim.selections.size(); i < e; ++i) {
                add_output_attribute(
                  selection_names[i].sv(), id, static_cast<int>(port_index));

                ++port_index;
            }
        } else {
            add_input_attribute("init", id, 0);
            add_input_attribute("run", id, 1);

            ImGui::PushItemWidth(120.0f);
            up += show_parameter_editor(
              app, compo.srcs, c.id.mdl_type, gen.children_parameters[idx]);
            ImGui::PopItemWidth();
        }
    } else {
        const auto X = get_input_port_names(c.id.mdl_type);
        const auto Y = get_output_port_names(c.id.mdl_type);

        add_input_attribute(X, id);
        ImGui::PushItemWidth(120.0f);
        if (c.id.mdl_type == dynamics_type::hsm_wrapper)
            up +=
              show_extented_hsm_parameter(app, gen.children_parameters[idx]);

        up += show_parameter_editor(
          app, compo.srcs, c.id.mdl_type, gen.children_parameters[idx]);

        if (c.id.mdl_type == dynamics_type::constant)
            up += show_extented_constant_parameter(
              ids, compo_id, gen.children_parameters[idx]);

        ImGui::PopItemWidth();
        add_output_attribute(Y, id);
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();

    return up > 0;
}

static void show_input_an_output_ports(const component& compo,
                                       const child_id   c_id) noexcept
{
    compo.x.for_each([&](const auto  id,
                         const auto  type,
                         const auto& name,
                         const auto& /*pos*/) noexcept {
        const auto pack_id = pack_in(c_id, id);

        ImNodes::BeginInputAttribute(pack_id, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TextUnformatted(port_option_names[ordinal(type)]);
        ImNodes::EndInputAttribute();
    });

    compo.y.for_each([&](const auto  id,
                         const auto  type,
                         const auto& name,
                         const auto& /*pos*/) noexcept {
        const auto pack_id = pack_out(c_id, id);

        ImNodes::BeginOutputAttribute(pack_id, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TextUnformatted(port_option_names[ordinal(type)]);
        ImNodes::EndOutputAttribute();
    });
}

static void show_generic_node(application&           app,
                              const std::string_view name,
                              const component&       compo,
                              const generic_component& /*s_compo*/,
                              const child_id c_id,
                              const generic_component::child& /*c*/) noexcept
{
    u32 node_c         = 0u;
    u32 node_hovered_c = 0u;
    u32 node_active_c  = 0u;

    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          node_c         = to_ImU32(colors[style_color::node]);
          node_hovered_c = to_ImU32(colors[style_color::node_hovered]);
          node_active_c  = to_ImU32(colors[style_color::node_active]);
      });

    ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_hovered_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_active_c);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.c_str());
    ImNodes::EndNodeTitleBar();
    if (compo.x.empty() and compo.y.empty()) {
        ImGui::TextUnformatted("No ports defined.");
    } else {
        show_input_an_output_ports(compo, c_id);
    }
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_grid_node(application&          app,
                           std::string_view      name,
                           const component&      compo,
                           const grid_component& grid,
                           child_id              c_id) noexcept
{
    u32 node_c         = 0u;
    u32 node_hovered_c = 0u;
    u32 node_active_c  = 0u;

    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          node_c         = to_ImU32(colors[style_color::node]);
          node_hovered_c = to_ImU32(colors[style_color::node_hovered]);
          node_active_c  = to_ImU32(colors[style_color::node_active]);
      });

    ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_hovered_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_active_c);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.sv());
    ImGui::TextFormat("{}x{}", grid.row(), grid.column());
    ImNodes::EndNodeTitleBar();
    if (compo.x.empty() and compo.y.empty()) {
        ImGui::TextUnformatted("No ports defined.");
    } else {
        show_input_an_output_ports(compo, c_id);
    }
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_graph_node(application&           app,
                            std::string_view       name,
                            const component&       compo,
                            const graph_component& graph,
                            child_id               c_id) noexcept
{
    u32 node_c         = 0u;
    u32 node_hovered_c = 0u;
    u32 node_active_c  = 0u;

    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          node_c         = to_ImU32(colors[style_color::node]);
          node_hovered_c = to_ImU32(colors[style_color::node_hovered]);
          node_active_c  = to_ImU32(colors[style_color::node_active]);
      });

    ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_hovered_c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_active_c);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.sv());
    ImGui::TextFormat("{}v {}e", graph.g.nodes.size(), graph.g.edges.size());
    ImNodes::EndNodeTitleBar();
    if (compo.x.empty() and compo.y.empty()) {
        ImGui::TextUnformatted("No ports defined.");
    } else {
        show_input_an_output_ports(compo, c_id);
    }
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static bool show_graph(const component_access& ids,
                       component_editor&       ed,
                       component_id            parent_id,
                       component&              parent,
                       generic_component&      s_parent) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto u = 0u;

    u32 node_c         = 0u;
    u32 node_hovered_c = 0u;
    u32 node_active_c  = 0u;

    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          node_c         = to_ImU32(colors[style_color::node]);
          node_hovered_c = to_ImU32(colors[style_color::node_hovered]);
          node_active_c  = to_ImU32(colors[style_color::node_active]);
      });

    parent.x.for_each<port_str>([&](auto id, const auto& name) noexcept {
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_c);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_hovered_c);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_active_c);

        ImNodes::BeginNode(pack_node_X(id));
        ImNodes::BeginOutputAttribute(pack_X(id),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();
    });

    parent.y.for_each<port_str>([&](auto id, const auto& name) noexcept {
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, node_c);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, node_hovered_c);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, node_active_c);

        ImNodes::BeginNode(pack_node_Y(id));
        ImNodes::BeginInputAttribute(pack_Y(id),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndInputAttribute();
        ImNodes::EndNode();
    });

    for (auto& c : s_parent.children) {
        if (c.type == child_type::model) {
            u += show_node(ids, ed, parent_id, parent, s_parent, c);
        } else {
            const auto cid  = s_parent.children.get_id(c);
            const auto cidx = get_index(cid);
            auto       id   = c.id.compo_id;

            if (ids.exists(id)) {
                auto& compo = ids.components[id];
                switch (compo.type) {
                case component_type::none:
                    break;

                case component_type::generic:
                    if (auto* s_compo = ids.generic_components.try_to_get(
                          compo.id.generic_id)) {
                        show_generic_node(app,
                                          s_parent.children_names[cidx].sv(),
                                          compo,
                                          *s_compo,
                                          cid,
                                          c);
                    }
                    break;

                case component_type::grid:
                    if (auto* s_compo =
                          ids.grid_components.try_to_get(compo.id.grid_id)) {
                        show_grid_node(app,
                                       s_parent.children_names[cidx].sv(),
                                       compo,
                                       *s_compo,
                                       cid);
                    }
                    break;

                case component_type::graph:
                    if (auto* s_compo =
                          ids.graph_components.try_to_get(compo.id.graph_id)) {
                        show_graph_node(app,
                                        s_parent.children_names[cidx].sv(),
                                        compo,
                                        *s_compo,
                                        cid);
                    }
                    break;

                case component_type::hsm:
                    break;

                case component_type::simulation:
                    break;
                }
            }
        }
    }

    for (auto& con : s_parent.connections)
        if (show_connection(ids, s_parent, con))
            s_parent.connections.free(con);

    for (auto& con : s_parent.input_connections)
        if (show_input_connection(ids, parent, s_parent, con))
            s_parent.input_connections.free(con);

    for (auto& con : s_parent.output_connections)
        if (show_output_connection(ids, parent, s_parent, con))
            s_parent.output_connections.free(con);

    return u > 0;
}

static bool compute_grid_layout(generic_component& s_compo,
                                const float        grid_layout_y_distance,
                                const float grid_layout_x_distance) noexcept
{
    const auto size  = s_compo.children.ssize();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return false;

    const auto column  = static_cast<int>(std::floor(std::sqrt(fsize)));
    const auto panning = ImNodes::EditorContextGetPanning();

    auto i = 0;
    auto j = 0;

    for (const auto& c : s_compo.children) {
        const auto c_id = s_compo.children.get_id(c);
        const auto x    = panning.y + i * grid_layout_y_distance;
        const auto y    = panning.x + j * grid_layout_x_distance;

        s_compo.children_positions[c_id] = { .x = x, .y = y };

        ++j;
        if (j > column) {
            j = 0;
            ++i;
        }
    }

    for (const auto& c : s_compo.children) {
        const auto  c_id    = s_compo.children.get_id(c);
        const auto  node_id = pack_node_child(c_id);
        const auto& pos     = s_compo.children_positions[c_id];
        ImNodes::SetNodeEditorSpacePos(node_id, ImVec2(pos.x, pos.y));
    }

    return true;
}

static bool add_component_to_current(application&            app,
                                     const component_access& ids,
                                     const component_id      parent_id,
                                     generic_component&      parent_compo,
                                     const component_id      compo_to_add_id,
                                     ImVec2                  click_pos)
{
    if (not ids.can_add(parent_id, compo_to_add_id)) {
        app.jn.push(
          log_level::error, [&ids, compo_to_add_id](auto& t, auto& m) {
              t = "Fail to add component";
              format(m,
                     "Irritator does not accept recursive component {}",
                     ids.components[compo_to_add_id].name.sv());
          });

        return false;
    }

    if (not parent_compo.children.can_alloc(1) and
        not parent_compo.grow_children()) {
        app.jn.push(log_level::error, [](auto& t, auto& m) {
            t = "Generic component";
            m = "Can not allocate new model. Delete models or increase "
                "generic component default size.";
        });

        return false;
    }

    auto&      c     = parent_compo.children.alloc(compo_to_add_id);
    const auto c_id  = parent_compo.children.get_id(c);
    const auto c_idx = get_index(c_id);

    parent_compo.children_positions[c_idx].x = click_pos.x;
    parent_compo.children_positions[c_idx].y = click_pos.y;
    ImNodes::SetNodeScreenSpacePos(pack_node_child(c_id), click_pos);

    return true;
}

static bool show_menu(generic_component&  gen,
                      const dynamics_type type,
                      const int           offset,
                      const ImVec2        pos,
                      const char*         description = nullptr) noexcept
{
    const auto  idx      = ordinal(type) + offset;
    const auto  new_type = enum_cast<dynamics_type>(idx);
    const auto* name     = dynamics_type_names[idx];
    const auto  open     = ImGui::MenuItem(name);

    if (description)
        ImGui::SetItemTooltip("%s", description);

    if (open) {
        auto&      child = gen.children.alloc(new_type);
        const auto id    = gen.children.get_id(child);
        const auto idx   = get_index(id);

        gen.children_positions[idx].x = pos.x;
        gen.children_positions[idx].y = pos.y;
        gen.children_parameters[idx].init_from(new_type);
        ImNodes::SetNodeScreenSpacePos(pack_node_child(id), pos);

        return true;
    }

    return false;
}

static bool show_menu(generic_component&  gen,
                      const dynamics_type type,
                      const ImVec2        pos,
                      const char*         description = nullptr) noexcept
{
    const auto  idx  = ordinal(type);
    const auto* name = dynamics_type_names[idx];
    const auto  open = ImGui::MenuItem(name);

    if (description)
        ImGui::SetItemTooltip("%s", description);

    if (open) {
        auto&      child = gen.children.alloc(type);
        const auto id    = gen.children.get_id(child);
        const auto idx   = get_index(id);

        gen.children_positions[idx].x = pos.x;
        gen.children_positions[idx].y = pos.y;
        gen.children_parameters[idx].init_from(type);
        ImNodes::SetNodeScreenSpacePos(pack_node_child(id), pos);

        return true;
    }

    return false;
}

static constexpr const char* menu_names[] = { "QSS 1", "QSS 2", "QSS 3" };
static constexpr int         offsets[]    = { 0, 43, 86 };

template<std::size_t QssLevel>
static bool show_hybrid_continuous_discrete(generic_component& gen,
                                            const ImVec2       pos) noexcept
{
    static_assert(1 <= QssLevel and QssLevel <= 3);

    const auto* menu   = menu_names[QssLevel - 1];
    const auto  offset = offsets[QssLevel - 1];
    auto        u      = 0;

    if (not ImGui::BeginMenu(menu))
        return false;

    u += show_menu(gen,
                   dynamics_type::qss1_sample_hold,
                   offset,
                   pos,
                   "Samples the continuous input on an internal clock (period "
                   "ts) and holds each sample until the next tick");

    u += show_menu(gen,
                   dynamics_type::qss1_flipflop,
                   offset,
                   pos,
                   "Emits output continuous from a input continous when an "
                   "event arrives on event port");

    u += show_menu(gen,
                   dynamics_type::qss1_quantizer,
                   offset,
                   pos,
                   "y = q · round(x / q); emits an event each time the input "
                   "crosses a quantization level.");

    u += show_menu(gen,
                   dynamics_type::qss1_integer,
                   offset,
                   pos,
                   "y = round(x); emits an event each time the input "
                   "crosses a integer level.");

    u += show_menu(gen,
                   dynamics_type::qss1_threshold_crossing,
                   offset,
                   pos,
                   "Emits an event (±1) when the input crosses a level; a "
                   "decoupled zero-crossing detector");

    u += show_menu(gen,
                   dynamics_type::qss1_cross,
                   offset,
                   pos,
                   "Emits continuous event and an event "
                   "(±1) when the input crosses a level; a "
                   "decoupled zero-crossing detector");

    u += show_menu(gen,
                   dynamics_type::qss1_integrate_and_fire,
                   offset,
                   pos,
                   "Accumulates ∫x dt and emits a spike, then resets, when the "
                   "accumulator reaches θ");

    u += show_menu(gen,
                   dynamics_type::qss1_pwm,
                   offset,
                   pos,
                   "Converts a continuous duty-cycle input into a discrete "
                   "on/off square-wave output.");

    ImGui::EndMenu();

    return u > 0;
}

template<std::size_t QssLevel>
static bool show_hybrid_stateful_switch(generic_component& gen,
                                        const ImVec2       pos) noexcept
{
    static_assert(1 <= QssLevel and QssLevel <= 3);

    const auto* menu   = menu_names[QssLevel - 1];
    const auto  offset = offsets[QssLevel - 1];
    auto        u      = 0;

    if (not ImGui::BeginMenu(menu))
        return false;

    u += show_menu(
      gen,
      dynamics_type::qss1_hysteresis,
      offset,
      pos,
      "Binary output that flips high when the input rises past upper and low "
      "when it falls below lower, holding its state in between");

    ImGui::EndMenu();

    return u > 0;
}

template<std::size_t QssLevel>
static bool show_continuous_stateful_detectors(generic_component& gen,
                                               const ImVec2       pos) noexcept
{
    static_assert(1 <= QssLevel and QssLevel <= 3);

    const auto* menu   = menu_names[QssLevel - 1];
    const auto  offset = offsets[QssLevel - 1];
    auto        u      = 0;

    if (not ImGui::BeginMenu(menu))
        return false;

    u += show_menu(gen,
                   dynamics_type::qss1_max_hold,
                   offset,
                   pos,
                   "Running peak detector: tracks the input while it sets a "
                   "new maximum, then holds that max value");

    u += show_menu(gen,
                   dynamics_type::qss1_min_hold,
                   offset,
                   pos,
                   "Running peak detector: tracks the input while it sets a "
                   "new minumum, then holds that min value");

    ImGui::EndMenu();

    return u > 0;
}

template<std::size_t QssLevel>
static bool show_continuous_maps(generic_component& gen,
                                 const ImVec2       pos) noexcept
{
    static_assert(1 <= QssLevel and QssLevel <= 3);

    const auto* menu   = menu_names[QssLevel - 1];
    const auto  offset = offsets[QssLevel - 1];
    auto        u      = 0;

    if (not ImGui::BeginMenu(menu))
        return false;

    u += show_menu(
      gen, dynamics_type::qss1_abs, offset, pos, "Absolute value y = |x|");

    u += show_menu(gen,
                   dynamics_type::qss1_saturation,
                   offset,
                   pos,
                   "Limits the signal to [lower, upper]; "
                   "passes through in-band, flat outside.");

    u += show_menu(gen,
                   dynamics_type::qss1_filter,
                   offset,
                   pos,
                   "Limits the signal to [lower, upper]; "
                   "sends 0 on one events.");

    u += show_menu(gen,
                   dynamics_type::qss1_dead_zone,
                   offset,
                   pos,
                   "Zero output inside [lower, upper], linear (shifted) "
                   "beyond; suppresses small signals");

    u += show_menu(gen,
                   dynamics_type::qss1_sign,
                   offset,
                   pos,
                   "Sign function y ∈ {−1, 0, +1}; value discontinuity at 0");

    u += show_menu(gen,
                   dynamics_type::qss1_maximum,
                   offset,
                   pos,
                   "Selects the larger of two continuous "
                   "inputs, switching at their crossing");

    u += show_menu(gen,
                   dynamics_type::qss1_minimum,
                   offset,
                   pos,
                   "Selects the smaller of two continuous "
                   "inputs, switching at their crossing");

    u += show_menu(gen,
                   dynamics_type::qss1_wrap,
                   offset,
                   pos,
                   "Folds the signal into [origin, origin + modulo), jumping "
                   "by  ±modulo at each boundary (e.g. angle wrapping)");

    u += show_menu(gen,
                   dynamics_type::qss1_compare,
                   offset,
                   pos,
                   "Compares two continous input and output constant value "
                   "when an input becomes greater than the other");

    ImGui::EndMenu();

    return u > 0;
}

template<std::size_t QssLevel>
static bool show_continuous_algebraic_menu(generic_component& gen,
                                           const ImVec2       pos) noexcept
{
    static_assert(1 <= QssLevel and QssLevel <= 3);

    const auto* menu   = menu_names[QssLevel - 1];
    const auto  offset = offsets[QssLevel - 1];
    auto        u      = 0;

    if (not ImGui::BeginMenu(menu))
        return false;

    u +=
      show_menu(gen, dynamics_type::qss1_integrator, offset, pos, "Integrator");

    u += show_menu(gen,
                   dynamics_type::qss1_sqrt,
                   offset,
                   pos,
                   "Square root, y = √x; guarded for x > 0.");

    u +=
      show_menu(gen, dynamics_type::qss1_gain, offset, pos, "Gaim, y = k(x)");

    u += show_menu(gen, dynamics_type::qss1_power, offset, pos, "Power y = xⁿ");

    u +=
      show_menu(gen, dynamics_type::qss1_square, offset, pos, "Square, y = x¹");

    u += show_menu(
      gen, dynamics_type::qss1_sum_2, offset, pos, "Sum, y = x[0] + x[1]");

    u += show_menu(gen,
                   dynamics_type::qss1_sum_3,
                   offset,
                   pos,
                   "Sum, y = x[0] + x[1] + x[2]");

    u += show_menu(gen,
                   dynamics_type::qss1_sum_4,
                   offset,
                   pos,
                   "Sum, y = x[0] + x[1] + x[2] + x[3]");

    u += show_menu(gen,
                   dynamics_type::qss1_wsum_2,
                   offset,
                   pos,
                   "Weighted sm, y = k_0 · x[0] + k_1 · x[1]");

    u += show_menu(gen,
                   dynamics_type::qss1_wsum_3,
                   offset,
                   pos,
                   "Weighted sm, y = k_0 · x[0] + k_1 · x[1] + k_2 · x[2]");

    u += show_menu(
      gen,
      dynamics_type::qss1_wsum_4,
      offset,
      pos,
      "Weighted sm, y = k_0 · x[0] + k_1 · x[1] + k_2 · x[2] + k_3 · x[3]");

    u += show_menu(
      gen, dynamics_type::qss1_inverse, offset, pos, "Inverse, y = 1 / x");

    u += show_menu(gen,
                   dynamics_type::qss1_multiplier,
                   offset,
                   pos,
                   "Multiplier, y = x[0] · x[1]");

    u +=
      show_menu(gen, dynamics_type::qss1_log, offset, pos, "Sinus, y = log(x)");

    u += show_menu(
      gen, dynamics_type::qss1_exp, offset, pos, "Cosinus, y = exp(x)");

    u +=
      show_menu(gen, dynamics_type::qss1_sin, offset, pos, "Sinus, y = sin(x)");

    u += show_menu(
      gen, dynamics_type::qss1_cos, offset, pos, "Cosinus, y = cos(x)");

    u += show_menu(gen,
                   dynamics_type::qss1_division,
                   offset,
                   pos,
                   "Ratio of two continuous signals, y = a "
                   "/ b; guarded for b ≠ 0.");

    u += show_menu(
      gen, dynamics_type::qss1_atan, offset, pos, "Arctangent, y = atan(x).");

    u += show_menu(gen,
                   dynamics_type::qss1_atan2,
                   offset,
                   pos,
                   "Two-argument arctangent y = atan2(yin, xin)");

    u += show_menu(
      gen, dynamics_type::qss1_tan, offset, pos, "Tangent, y = tan(x)");

    u += show_menu(
      gen, dynamics_type::qss1_tanh, offset, pos, "Hyperbolic tangent");

    u += show_menu(gen,
                   dynamics_type::qss1_sigmoid,
                   offset,
                   pos,
                   "Logistic y = 1 / (1 + e⁻ˣ)");

    ImGui::EndMenu();

    return u > 0;
}

static bool show_popup_menuitem(const component_access&        ids,
                                application&                   app,
                                generic_component_editor_data& data,
                                const component_id             parent_id,
                                generic_component&             gen) noexcept
{
    auto u = 0;

    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        if (not data.selected_links.empty())
            if (ImGui::MenuItem("Delete selected links"))
                u += remove_links(data, gen);

        if (not data.selected_nodes.empty())
            if (ImGui::MenuItem("Delete selected nodes"))
                u += remove_nodes(gen, data);

        if (ImGui::MenuItem("Force grid layout"))
            u += compute_grid_layout(
              gen, data.grid_layout_x_distance, data.grid_layout_y_distance);

        ImGui::Separator();

        if (auto res = app.component_sel.menu("Choose component");
            res.is_done) {
            debug::ensure(is_defined(res.id));

            if (ids.exists(res.id)) {
                auto& c = ids.components[res.id];
                if (c.type == component_type::hsm)
                    app.jn.push(
                      log_level::error, [](auto& title, auto& msg) noexcept {
                          title = "Component editor";
                          msg   = "Please, use the hsm_wrapper model to add a "
                                  "hierarchical state machine";
                      });
                else
                    u += add_component_to_current(
                      app, ids, parent_id, gen, res.id, click_pos);
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Continuous algebraic")) {
            ImGui::SetItemTooltip("Continuous ‣ smooth algebraic "
                                  "functions\n(continuous trajectory "
                                  "in ‣ continuous trajectory out)");

            u += show_continuous_algebraic_menu<1>(gen, click_pos);
            u += show_continuous_algebraic_menu<2>(gen, click_pos);
            u += show_continuous_algebraic_menu<3>(gen, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Continuous maps")) {
            ImGui::SetItemTooltip(
              "Continuous - discontinuous static maps\n(continuous in ‣ "
              "continuous out, memoryless, with internal state events at the "
              "breakpoints)");

            u += show_continuous_maps<1>(gen, click_pos);
            u += show_continuous_maps<2>(gen, click_pos);
            u += show_continuous_maps<3>(gen, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Continuous stateful detectors")) {
            ImGui::SetItemTooltip(
              "Continuous in ‣ continuous out, with memory");

            u += show_continuous_stateful_detectors<1>(gen, click_pos);
            u += show_continuous_stateful_detectors<2>(gen, click_pos);
            u += show_continuous_stateful_detectors<3>(gen, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Hybrid - stateful switch")) {
            ImGui::SetItemTooltip(
              "Continuous in ‣ switched output, with memory");

            u += show_hybrid_stateful_switch<1>(gen, click_pos);
            u += show_hybrid_stateful_switch<2>(gen, click_pos);
            u += show_hybrid_stateful_switch<3>(gen, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Hybrid - continuous ‣ discrete")) {
            ImGui::SetItemTooltip("Turn a continuous signal into discrete "
                                  "events or a discrete-time signal");

            u += show_hybrid_continuous_discrete<1>(gen, click_pos);
            u += show_hybrid_continuous_discrete<2>(gen, click_pos);
            u += show_hybrid_continuous_discrete<3>(gen, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Hybrid - discrete ‣ continuous")) {
            ImGui::SetItemTooltip("Turn a continuous signal into discrete "
                                  "events or a discrete-time signal");

            u += show_menu(gen, dynamics_type::zero_order_hold, click_pos);

            ImGui::SetItemTooltip(
              "Holds the last received discrete event value as a constant "
              "continuous output until the next event arrives");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            u += show_menu(gen, dynamics_type::logical_and_2, click_pos);
            u += show_menu(gen, dynamics_type::logical_or_2, click_pos);
            u += show_menu(gen, dynamics_type::logical_and_3, click_pos);
            u += show_menu(gen, dynamics_type::logical_or_3, click_pos);
            u += show_menu(gen, dynamics_type::logical_invert, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Queue")) {
            u += show_menu(gen, dynamics_type::queue, click_pos);
            u += show_menu(gen, dynamics_type::dynamic_queue, click_pos);
            u += show_menu(gen, dynamics_type::priority_queue, click_pos);
            u += show_menu(gen, dynamics_type::generator, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Wrapper")) {
            u += show_menu(gen, dynamics_type::hsm_wrapper, click_pos);
            u += show_menu(gen, dynamics_type::simulation_wrapper, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Other")) {
            u += show_menu(gen, dynamics_type::counter, click_pos);
            u += show_menu(gen, dynamics_type::constant, click_pos);
            u += show_menu(gen, dynamics_type::time_func, click_pos);
            u += show_menu(gen, dynamics_type::accumulator_2, click_pos);
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    return u > 0;
}

static void error_not_enough_connections(application& app, sz capacity) noexcept
{
    app.jn.push(log_level::error, [&](auto& t, auto& m) {
        t = "Not enough connection slot in this component";
        format(m, "All connections slots ({}) are used.", capacity);
    });
}

static void error_not_connection_auth(application& app) noexcept
{
    app.jn.push(log_level::error, [&](auto& t, auto&) {
        t = "Can not connect component input on output ports";
    });
}

static bool is_link_created(application&            app,
                            const component_access& ids,
                            generic_component_editor_data& /*data*/,
                            component&         parent,
                            generic_component& s_parent) noexcept
{
    int start = 0, end = 0;

    if (not ImNodes::IsLinkCreated(&start, &end))
        return false;

    if (not s_parent.connections.can_alloc() and
        not s_parent.connections.grow<2, 1>()) {
        error_not_enough_connections(app, s_parent.connections.capacity());
        return false;
    }

    if (is_input_X(start)) {
        auto port_idx = unpack_X(start);
        auto port_id  = parent.x.get_from_index(port_idx);
        debug::ensure(is_defined(port_id));

        if (is_output_Y(end)) {
            error_not_connection_auth(app);
            return false;
        }

        auto  child_port = unpack_in(end);
        auto* child = s_parent.children.try_to_get_from_pos(child_port.first);
        debug::ensure(child);

        if (child->type == child_type::model) {
            auto port_in = static_cast<int>(child_port.second);

            if (auto ret = s_parent.connect_input(
                  port_id, *child, connection::port{ .model = port_in });
                !ret)
                debug_log("fail to create link");
            parent.state = component_status::modified;
            return true;
        } else {
            if (not ids.exists(child->id.compo_id))
                return false;

            const auto& compo_dst = ids.components[child->id.compo_id];

            const auto port_dst_id =
              compo_dst.x.get_from_index(child_port.second);
            if (is_undefined(port_dst_id))
                return false;

            if (auto ret = s_parent.connect_input(
                  port_id, *child, connection::port{ .compo = port_dst_id });
                !ret)
                debug_log("fail to create link\n");
            parent.state = component_status::modified;
            return true;
        }
    } else {
        auto  ch_port_src = unpack_out(start);
        auto* ch_src = s_parent.children.try_to_get_from_pos(ch_port_src.first);
        debug::ensure(ch_src);

        if (is_output_Y(end)) {
            auto port_idx = unpack_Y(end);
            auto port_id  = parent.y.get_from_index(port_idx);
            debug::ensure(is_defined(port_id));

            if (ch_src->type == child_type::model) {
                auto port_out = static_cast<int>(ch_port_src.second);

                if (auto ret = s_parent.connect_output(
                      port_id, *ch_src, connection::port{ .model = port_out });
                    !ret)
                    debug_log("fail to create link\n");
                parent.state = component_status::modified;
                return true;
            } else {
                if (not ids.exists(ch_src->id.compo_id))
                    return false;

                const auto& compo_src = ids.components[ch_src->id.compo_id];
                const auto  port_out =
                  compo_src.y.get_from_index(ch_port_src.second);
                if (is_undefined(port_out))
                    return false;

                if (auto ret = s_parent.connect_output(
                      port_id, *ch_src, connection::port{ .compo = port_id });
                    !ret)
                    debug_log("fail to create link\n");
                parent.state = component_status::modified;
                return true;
            }
        } else {
            auto  ch_port_dst = unpack_in(end);
            auto* ch_dst =
              s_parent.children.try_to_get_from_pos(ch_port_dst.first);
            debug::ensure(ch_dst);

            if (ch_src->type == child_type::model) {
                auto port_out = static_cast<int>(ch_port_src.second);
                if (ch_dst->type == child_type::model) {
                    auto port_in = static_cast<int>(ch_port_dst.second);

                    if (auto ret = s_parent.connect(
                          *ch_src,
                          connection::port{ .model = port_out },
                          *ch_dst,
                          connection::port{ .model = port_in });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                    return true;
                } else {
                    if (not ids.exists(ch_dst->id.compo_id))
                        return false;

                    const auto& compo_dst = ids.components[ch_dst->id.compo_id];
                    const auto  port_dst_id =
                      compo_dst.x.get_from_index(ch_port_dst.second);
                    if (is_undefined(port_dst_id))
                        return false;

                    if (auto ret = s_parent.connect(
                          *ch_src,
                          connection::port{ .model = port_out },
                          *ch_dst,
                          connection::port{ .compo = port_dst_id });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                    return true;
                }
            } else {
                if (not ids.exists(ch_src->id.compo_id))
                    return false;

                const auto& compo_src = ids.components[ch_src->id.compo_id];
                const auto  port_out =
                  compo_src.y.get_from_index(ch_port_src.second);

                if (is_undefined(port_out))
                    return false;

                if (ch_dst->type == child_type::model) {
                    auto port_in = static_cast<int>(ch_port_dst.second);

                    if (auto ret = s_parent.connect(
                          *ch_src,
                          connection::port{ .compo = port_out },
                          *ch_dst,
                          connection::port{ .model = port_in });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                    return true;
                } else {
                    if (not ids.exists(ch_dst->id.compo_id))
                        return false;

                    const auto& compo_dst = ids.components[ch_dst->id.compo_id];
                    const auto  port_in =
                      compo_dst.x.get_from_index(ch_port_dst.second);

                    if (is_undefined(port_in))
                        return false;

                    if (auto ret = s_parent.connect(
                          *ch_src,
                          connection::port{ .compo = port_out },
                          *ch_dst,
                          connection::port{ .compo = port_in });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                    return true;
                }
            }
        }
    }
}

static bool is_link_destroyed(generic_component& s_parent) noexcept
{
    int link_id;

    if (ImNodes::IsLinkDestroyed(&link_id))
        return delete_link(s_parent, link_id);

    return false;
}

static bool show_component_editor(component_editor&              ed,
                                  const component_access&        ids,
                                  generic_component_editor_data& data,
                                  component_id                   compo_id,
                                  component&                     compo,
                                  generic_component& s_compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto u = 0;

    ImNodes::EditorContextSet(data.context);
    ImNodes::BeginNodeEditor();

    const auto is_editor_hovered =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) and
      ImNodes::IsEditorHovered();

    u += show_popup_menuitem(ids, app, data, compo_id, s_compo);
    u += show_graph(ids, ed, compo_id, compo, s_compo);

    if (data.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    u += is_link_created(app, ids, data, compo, s_compo);
    u += is_link_destroyed(s_compo);

    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        data.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(data.selected_nodes.begin());
        remove_component_input_output(data.selected_nodes);
    } else {
        data.selected_nodes.clear();
    }

    int num_selected_links = ImNodes::NumSelectedLinks();
    if (num_selected_links > 0) {
        data.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(data.selected_links.begin());
    } else {
        data.selected_links.clear();
    }

    if (is_editor_hovered and ImGui::IsKeyReleased(ImGuiKey_Delete) and
        not ImGui::IsAnyItemHovered()) {
        if (num_selected_nodes > 0)
            u += remove_nodes(s_compo, data);
        else if (num_selected_links > 0)
            u += remove_links(data, s_compo);
    }

    return u > 0;
}

generic_component_editor_data::generic_component_editor_data(
  const component_id id,
  const component&   compo,
  const generic_component_id /*gid*/,
  const generic_component& gen) noexcept
  : options(5u)
  , m_id(id)
{
    context = ImNodes::EditorContextCreate();
    ImNodes::EditorContextSet(context);
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;

    ImVec2 top_left(FLT_MAX, FLT_MAX), bottom_right(FLT_MIN, FLT_MIN);

    for (auto& c : gen.children) {
        const auto id  = gen.children.get_id(c);
        const auto idx = get_index(id);
        const auto x   = gen.children_positions[idx].x;
        const auto y   = gen.children_positions[idx].y;

        top_left.x     = std::min(x, top_left.x);
        top_left.y     = std::min(y, top_left.y);
        bottom_right.x = std::max(x, bottom_right.x);
        bottom_right.y = std::max(y, bottom_right.y);

        ImNodes::SetNodeEditorSpacePos(pack_node_child(id), ImVec2(x, y));
    }

    compo.x.for_each<position>([&](auto id, const position& pos) noexcept {
        ImNodes::SetNodeEditorSpacePos(pack_node_X(id), ImVec2(pos.x, pos.y));
    });

    compo.y.for_each<position>([&](auto id, const position& pos) noexcept {
        ImNodes::SetNodeEditorSpacePos(pack_node_Y(id), ImVec2(pos.x, pos.y));
    });
}

generic_component_editor_data::~generic_component_editor_data() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

bool generic_component_editor_data::show(component_editor&       ed,
                                         const component_access& ids,
                                         component&              compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);
    if (show_component_editor(ed, ids, *this, m_id, compo, m_generic)) {
        write(app, compo);
        return true;
    }

    return false;
}

static void update_unique_id(generic_component&        gen,
                             generic_component::child& ch) noexcept
{
    const auto configurable = ch.flags[child_flags::configurable];
    const auto observable   = ch.flags[child_flags::observable];

    const auto ch_id  = gen.children.get_id(ch);
    const auto ch_idx = get_index(ch_id);

    if ((configurable or observable) and gen.children_names[ch_idx].empty())
        gen.children_names[ch_idx] = gen.make_unique_name_id(ch_id);
}

static bool show_selected_node(generic_component&        gen,
                               generic_component::child& c) noexcept
{
    const auto id          = gen.children.get_id(c);
    const auto idx         = get_index(id);
    const auto name        = format_n<32>("{}", idx);
    bool       is_modified = false;

    if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("position {},{}",
                          gen.children_positions[idx].x,
                          gen.children_positions[idx].y);

        bool configurable = c.flags[child_flags::configurable];
        if (ImGui::Checkbox("configurable", &configurable)) {
            c.flags.set(child_flags::configurable, configurable);
            is_modified = true;
        }

        bool observable = c.flags[child_flags::observable];
        if (ImGui::Checkbox("observables", &observable)) {
            c.flags.set(child_flags::observable, observable);
            is_modified = true;
        }

        name_str name = gen.children_names[idx];
        if (ImGui::InputFilteredString("name", name)) {
            gen.children_names[idx] = name;
            is_modified             = true;
        }

        update_unique_id(gen, c);
        ImGui::TreePop();
    }

    return is_modified;
}

bool generic_component_editor_data::show_selected_nodes(
  component_editor& ed,
  const component_access& /*ids*/,
  component& compo) noexcept
{
    if (selected_nodes.empty())
        return false;

    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);

    int u = 0;
    for (int i = 0, e = selected_nodes.size(); i != e; ++i) {
        if (is_node_X(selected_nodes[i]) || is_node_Y(selected_nodes[i]))
            continue;

        const auto id = unpack_node_child(selected_nodes[i]);
        if (auto* child = m_generic.children.try_to_get_from_pos(id); child)
            u += show_selected_node(m_generic, *child);
    }

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

bool generic_component_editor_data::need_show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    return not selected_nodes.empty();
}

void generic_component_editor_data::clear_selected_nodes() noexcept
{
    ImNodes::ClearNodeSelection();
    ImNodes::ClearLinkSelection();

    selected_links.clear();
    selected_nodes.clear();
}

void generic_component_editor_data::read(application& app,
                                         component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        if (debug::check(compo.type == component_type::generic)) {
            const auto gen_id = compo.id.generic_id;

            if (auto* gen = ids.generic_components.try_to_get(gen_id))
                m_generic = *gen;
        }
    });
}

void generic_component_editor_data::write(application& app,
                                          component&   compo) noexcept
{
    // Stores the ImNodes hidden attributes into generic_component position
    // of nodes.

    for (auto& c : m_generic.children) {
        const auto id  = m_generic.children.get_id(c);
        const auto idx = get_index(id);

        const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_child(id));
        m_generic.children_positions[idx].x = pos.x;
        m_generic.children_positions[idx].y = pos.y;
    }

    compo.x.template for_each<position>(
      [&](const auto id, auto& position) noexcept {
          const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_X(id));
          position.x     = pos.x;
          position.y     = pos.y;
      });

    compo.y.template for_each<position>(
      [&](const auto id, auto& position) noexcept {
          const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_Y(id));
          position.x     = pos.x;
          position.y     = pos.y;
      });

    auto c_ptr = std::make_unique<component>(compo);
    auto g_ptr = std::make_unique<generic_component>(m_generic);

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (debug::check(c->type == component_type::generic)) {
                  const auto gen_id = c->id.generic_id;

                  if (auto* gen = ids.generic_components.try_to_get(gen_id)) {
                      *gen = *g;
                  }
              }
          });
      });
}

} // namespace irt
