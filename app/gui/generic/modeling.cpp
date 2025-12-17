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

inline bool is_node_child(const int node) noexcept
{
    return (mask_tag & node) == child_tag;
}

inline bool is_node_X(const int node) noexcept
{
    return (mask_tag & node) == input_tag;
}

inline bool is_node_Y(const int node) noexcept
{
    return (mask_tag & node) == output_tag;
}

inline int pack_node_child(const child_id child) noexcept
{
    return static_cast<int>((get_index(child) << shift_tag) | child_tag);
}

inline int unpack_node_child(const int node) noexcept
{
    debug::ensure(is_node_child(node));
    return node >> shift_tag;
}

inline int pack_node_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | input_tag);
}

inline int unpack_node_X(const int node) noexcept
{
    debug::ensure(is_node_X(node));
    return node >> shift_tag;
}

inline int pack_node_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | output_tag);
}

inline int unpack_node_Y(const int node) noexcept
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

inline bool is_input_X(const int attribute) noexcept
{
    return (attribute & mask_port) == input_component_port;
}

inline bool is_output_Y(const int attribute) noexcept
{
    return (attribute & mask_port) == output_component_port;
}

inline bool is_input_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_model_port;
}

inline bool is_input_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_compo_port;
}

inline bool is_output_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_model_port;
}

inline bool is_output_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_compo_port;
}

inline int pack_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>((get_index(port) << shift_port) |
                            input_component_port);
}

inline u32 unpack_X(const int attribute) noexcept
{
    debug::ensure(is_input_X(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

inline int pack_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>(
      (get_index(port) << shift_port | output_component_port));
}

inline u32 unpack_Y(const int attribute) noexcept
{
    debug::ensure(is_output_Y(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

inline int pack_in(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_model_port;
    return ret;
}

inline int pack_in(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_compo_port;
    return ret;
}

inline std::pair<u32, u32> unpack_in(const int attribute) noexcept
{
    debug::ensure(!is_input_X(attribute));
    debug::ensure(is_input_child_model(attribute) ||
                  is_input_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

inline int pack_out(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_model_port;
    return ret;
}

inline int pack_out(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_compo_port;
    return ret;
}

inline std::pair<u32, u32> unpack_out(const int attribute) noexcept
{
    debug::ensure(!is_output_Y(attribute));
    debug::ensure(is_output_child_model(attribute) ||
                  is_output_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

static void add_input_attribute(const std::span<const std::string_view> names,
                                const child_id id) noexcept
{
    for (size_t i = 0, e = names.size(); i != e; ++i) {
        ImNodes::BeginInputAttribute(pack_in(id, static_cast<int>(i)),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", names[i]);
        ImNodes::EndInputAttribute();
    }
}

static void add_output_attribute(const std::span<const std::string_view> names,
                                 const child_id id) noexcept
{
    for (size_t i = 0, e = names.size(); i != e; ++i) {
        ImNodes::BeginOutputAttribute(pack_out(id, static_cast<int>(i)),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", names[i]);
        ImNodes::EndOutputAttribute();
    }
}

static void delete_link(component&         compo,
                        generic_component& gen,
                        int                link_id) noexcept
{
    if (link_id >= 8192) {
        const auto id = static_cast<u32>(link_id - 8192);
        if (auto* con = gen.output_connections.try_to_get_from_pos(id); con) {
            gen.output_connections.free(*con);
            compo.state = component_status::modified;
        }
    } else if (link_id >= 4096) {
        const auto id = static_cast<u32>(link_id - 4096);
        if (auto* con = gen.input_connections.try_to_get_from_pos(id); con) {
            gen.input_connections.free(*con);
            compo.state = component_status::modified;
        }
    } else {
        const auto id = static_cast<u32>(link_id);
        if (auto* con = gen.connections.try_to_get_from_pos(id); con) {
            gen.connections.free(*con);
            compo.state = component_status::modified;
        }
    }
}

static void remove_nodes(modeling&                      mod,
                         generic_component_editor_data& data,
                         component&                     parent) noexcept
{
    if (parent.type == component_type::generic) {
        if_data_exists_do(
          mod.generic_components,
          parent.id.generic_id,
          [&](auto& generic) noexcept {
              for (i32 i = 0, e = data.selected_nodes.size(); i != e; ++i) {
                  if (is_node_child(data.selected_nodes[i])) {
                      auto idx = unpack_node_child(data.selected_nodes[i]);

                      if (auto* child =
                            generic.children.try_to_get_from_pos(idx);
                          child) {
                          generic.children.free(*child);
                          parent.state = component_status::modified;
                      }
                  }
              }
          });
    }

    data.selected_nodes.clear();
    ImNodes::ClearNodeSelection();
}

static void remove_links(generic_component_editor_data& data,
                         component&                     parent,
                         generic_component&             s_parent) noexcept
{
    std::sort(data.selected_links.begin(),
              data.selected_links.end(),
              std::greater<int>());

    for (i32 i = 0, e = data.selected_links.size(); i != e; ++i)
        delete_link(parent, s_parent, data.selected_links[i]);

    data.selected_links.clear();
    ImNodes::ClearLinkSelection();

    parent.state = component_status::modified;
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

static bool show_connection(
  const component&                           compo,
  const generic_component&                   gen,
  const generic_component::input_connection& con) noexcept
{
    const auto id     = gen.input_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 4096 + static_cast<int>(idx);

    if (compo.x.exists(con.x)) {
        if (auto* c = gen.children.try_to_get(con.dst); c) {
            const auto id_src = pack_X(con.x);
            const auto id_dst = c->type == child_type::model
                                  ? pack_in(con.dst, con.port.model)
                                  : pack_in(con.dst, con.port.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return false;
        }
    }

    return true;
}

static bool show_connection(
  const component&                            compo,
  const generic_component&                    gen,
  const generic_component::output_connection& con) noexcept
{
    const auto id     = gen.output_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 8192 + static_cast<int>(idx);

    if (compo.y.exists(con.y)) {
        if (auto* c = gen.children.try_to_get(con.src); c) {
            const auto id_dst = pack_Y(con.y);
            const auto id_src = c->type == child_type::model
                                  ? pack_out(con.src, con.port.model)
                                  : pack_out(con.src, con.port.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return false;
        }
    }

    return true;
}

static bool show_connection(const generic_component& compo,
                            const connection&        con) noexcept
{
    const auto id     = compo.connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = static_cast<int>(idx);

    if (auto* s = compo.children.try_to_get(con.src); s) {
        if (auto* d = compo.children.try_to_get(con.dst); d) {
            const auto id_src = s->type == child_type::model
                                  ? pack_out(con.src, con.index_src.model)
                                  : pack_out(con.src, con.index_src.compo);

            const auto id_dst = d->type == child_type::model
                                  ? pack_in(con.dst, con.index_dst.model)
                                  : pack_in(con.dst, con.index_dst.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return false;
        }
    }

    return true;
}

static bool show_node(component_editor&         ed,
                      component&                compo,
                      generic_component&        gen,
                      generic_component::child& c) noexcept
{
    const auto id  = gen.children.get_id(c);
    const auto idx = get_index(id);

    auto& app = container_of(&ed, &application::component_ed);

    ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                            to_ImU32(app.config.colors[style_color::node_2]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarHovered,
      to_ImU32(app.config.colors[style_color::node_2_hovered]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarSelected,
      to_ImU32(app.config.colors[style_color::node_2_active]));

    ImNodes::BeginNode(pack_node_child(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      gen.children_names[idx].sv(),
                      dynamics_type_names[ordinal(c.id.mdl_type)]);
    ImNodes::EndNodeTitleBar();

    const auto X   = get_input_port_names(c.id.mdl_type);
    const auto Y   = get_output_port_names(c.id.mdl_type);
    auto       ret = 0;

    add_input_attribute(X, id);
    ImGui::PushItemWidth(120.0f);
    if (c.id.mdl_type == dynamics_type::hsm_wrapper)
        ret += show_extented_hsm_parameter(app, gen.children_parameters[idx]);

    ret += show_parameter_editor(
      app, compo.srcs, c.id.mdl_type, gen.children_parameters[idx]);

    if (c.id.mdl_type == dynamics_type::constant)
        ret +=
          show_extented_constant_parameter(app.mod,
                                           app.mod.components.get_id(compo),
                                           gen.children_parameters[idx]);

    ImGui::PopItemWidth();
    add_output_attribute(Y, id);

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();

    return ret;
}

static void show_input_an_output_ports(component&     compo,
                                       const child_id c_id) noexcept
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

static void show_generic_node(application&     app,
                              std::string_view name,
                              component&       compo,
                              generic_component& /*s_compo*/,
                              const child_id c_id,
                              generic_component::child& /*c*/) noexcept
{
    ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                            to_ImU32(app.config.colors[style_color::node]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarHovered,
      to_ImU32(app.config.colors[style_color::node_hovered]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarSelected,
      to_ImU32(app.config.colors[style_color::node_active]));

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

static void show_grid_node(application&     app,
                           std::string_view name,
                           component&       compo,
                           grid_component&  grid,
                           child_id         c_id) noexcept
{
    ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                            to_ImU32(app.config.colors[style_color::node]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarHovered,
      to_ImU32(app.config.colors[style_color::node_hovered]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarSelected,
      to_ImU32(app.config.colors[style_color::node_active]));

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

static void show_graph_node(application&     app,
                            std::string_view name,
                            component&       compo,
                            graph_component& graph,
                            child_id         c_id) noexcept
{
    ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                            to_ImU32(app.config.colors[style_color::node]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarHovered,
      to_ImU32(app.config.colors[style_color::node_hovered]));
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBarSelected,
      to_ImU32(app.config.colors[style_color::node_active]));

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

static void show_graph(component_editor&  ed,
                       component&         parent,
                       generic_component& s_parent) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    parent.x.for_each<port_str>([&](auto id, const auto& name) noexcept {
        ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                                to_ImU32(app.config.colors[style_color::node]));
        ImNodes::PushColorStyle(
          ImNodesCol_TitleBarHovered,
          to_ImU32(app.config.colors[style_color::node_hovered]));
        ImNodes::PushColorStyle(
          ImNodesCol_TitleBarSelected,
          to_ImU32(app.config.colors[style_color::node_active]));

        ImNodes::BeginNode(pack_node_X(id));
        ImNodes::BeginOutputAttribute(pack_X(id),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();
    });

    parent.y.for_each<port_str>([&](auto id, const auto& name) noexcept {
        ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                                to_ImU32(app.config.colors[style_color::node]));
        ImNodes::PushColorStyle(
          ImNodesCol_TitleBarHovered,
          to_ImU32(app.config.colors[style_color::node_hovered]));
        ImNodes::PushColorStyle(
          ImNodesCol_TitleBarSelected,
          to_ImU32(app.config.colors[style_color::node_active]));

        ImNodes::BeginNode(pack_node_Y(id));
        ImNodes::BeginInputAttribute(pack_Y(id),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndInputAttribute();
        ImNodes::EndNode();
    });

    for (auto& c : s_parent.children) {
        if (c.type == child_type::model) {
            (void)show_node(ed, parent, s_parent, c);
        } else {
            const auto cid  = s_parent.children.get_id(c);
            const auto cidx = get_index(cid);
            auto       id   = c.id.compo_id;

            if (auto* compo = app.mod.components.try_to_get<component>(id)) {
                switch (compo->type) {
                case component_type::none:
                    break;

                case component_type::generic:
                    if (auto* s_compo = app.mod.generic_components.try_to_get(
                          compo->id.generic_id)) {
                        show_generic_node(app,
                                          s_parent.children_names[cidx].sv(),
                                          *compo,
                                          *s_compo,
                                          cid,
                                          c);
                    }
                    break;

                case component_type::grid:
                    if (auto* s_compo = app.mod.grid_components.try_to_get(
                          compo->id.grid_id)) {
                        show_grid_node(app,
                                       s_parent.children_names[cidx].sv(),
                                       *compo,
                                       *s_compo,
                                       cid);
                    }
                    break;

                case component_type::graph:
                    if (auto* s_compo = app.mod.graph_components.try_to_get(
                          compo->id.graph_id)) {
                        show_graph_node(app,
                                        s_parent.children_names[cidx].sv(),
                                        *compo,
                                        *s_compo,
                                        cid);
                    }
                    break;

                case component_type::hsm:
                    break;
                }
            }
        }
    }

    remove_data_if(s_parent.connections, [&](auto& con) noexcept -> bool {
        return show_connection(s_parent, con);
    });

    remove_data_if(s_parent.input_connections, [&](auto& con) noexcept -> bool {
        return show_connection(parent, s_parent, con);
    });

    remove_data_if(s_parent.output_connections,
                   [&](auto& con) noexcept -> bool {
                       return show_connection(parent, s_parent, con);
                   });
}

static void add_popup_menuitem(application&       app,
                               component&         parent,
                               generic_component& s_parent,
                               dynamics_type      type,
                               ImVec2             click_pos)
{
    if (not s_parent.children.can_alloc(1)) {
        app.jn.push(log_level::error, [](auto& title, auto& msg) noexcept {
            title = "Generic component";
            msg   = "Can not allocate new model. Delete models or increase "
                    "generic component default size.";
        });

        return;
    }

    if (ImGui::MenuItem(dynamics_type_names[ordinal(type)])) {
        auto&      child = s_parent.children.alloc(type);
        const auto id    = s_parent.children.get_id(child);
        const auto idx   = get_index(id);

        s_parent.children_positions[idx].x = click_pos.x;
        s_parent.children_positions[idx].y = click_pos.y;
        s_parent.children_parameters[idx].init_from(type);
        ImNodes::SetNodeScreenSpacePos(pack_node_child(id),
                                       ImVec2(click_pos.x, click_pos.y));

        parent.state = component_status::modified;

        app.jn.push(log_level::info, [type](auto& title, auto& msg) noexcept {
            title = "Generic component";
            format(
              msg, "New model {} added", dynamics_type_names[ordinal(type)]);
        });
    }
}

static void add_popup_menuitem(application&       app,
                               component&         parent,
                               generic_component& s_parent,
                               int                type,
                               ImVec2             click_pos)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(app, parent, s_parent, d_type, click_pos);
}

static void compute_grid_layout(generic_component& s_compo,
                                const float        grid_layout_y_distance,
                                const float grid_layout_x_distance) noexcept
{
    const auto size  = s_compo.children.ssize();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return;

    const auto column  = std::floor(std::sqrt(fsize));
    const auto panning = ImNodes::EditorContextGetPanning();
    auto       i       = 0.f;
    auto       j       = 0.f;

    for_each_data(s_compo.children, [&](auto& c) noexcept {
        const auto id  = s_compo.children.get_id(c);
        const auto idx = get_index(id);

        s_compo.children_positions[idx].x =
          panning.y + i * grid_layout_y_distance;
        s_compo.children_positions[idx].y =
          panning.x + j * grid_layout_x_distance;
        ++j;

        if (j >= static_cast<int>(column)) {
            j = 0.f;
            ++i;
        }
    });
}

static void add_component_to_current(application&       app,
                                     component&         parent,
                                     generic_component& parent_compo,
                                     component&         compo_to_add,
                                     ImVec2             click_pos = ImVec2())
{

    if (not app.mod.can_add(parent, compo_to_add)) {
        app.jn.push(log_level::error, [&compo_to_add](auto& t, auto& m) {
            t = "Fail to add component";
            format(m,
                   "Irritator does not accept recursive component {}",
                   compo_to_add.name.sv());
        });
    }

    if (not parent_compo.children.can_alloc(1) and
        not parent_compo.grow_children())
        app.jn.push(log_level::error, [](auto& t, auto& m) {
            t = "Generic component";
            m = "Can not allocate new model. Delete models or increase "
                "generic component default size.";
        });

    const auto compo_to_add_id = app.mod.components.get_id(compo_to_add);
    auto&      c               = parent_compo.children.alloc(compo_to_add_id);
    const auto c_id            = parent_compo.children.get_id(c);
    const auto c_idx           = get_index(c_id);

    parent_compo.children_positions[c_idx].x = click_pos.x;
    parent_compo.children_positions[c_idx].y = click_pos.y;
    ImNodes::SetNodeEditorSpacePos(pack_node_child(c_id), click_pos);
}

static void show_popup_menuitem(application&                   app,
                                generic_component_editor_data& data,
                                component&                     parent,
                                generic_component& s_parent) noexcept
{
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
                remove_links(data, parent, s_parent);

        if (not data.selected_nodes.empty())
            if (ImGui::MenuItem("Delete selected nodes"))
                remove_nodes(app.mod, data, parent);

        if (ImGui::MenuItem("Force grid layout")) {
            compute_grid_layout(s_parent,
                                data.grid_layout_x_distance,
                                data.grid_layout_y_distance);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Add grid component")) {
            if (!app.mod.grid_components.can_alloc() ||
                !app.mod.components.can_alloc(1) ||
                !s_parent.children.can_alloc()) {
                app.jn.push(log_level::error, [](auto& t, auto&) {
                    t = "can not allocate a new grid component";
                });
            } else {
                auto& compo    = app.mod.alloc_grid_component();
                auto& grid     = app.mod.grid_components.get(compo.id.grid_id);
                auto  compo_id = app.mod.components.get_id(compo);
                grid.resize(4, 4, undefined<component_id>());

                app.mod.components.get<component_color>(
                  compo_id) = { 1.f, 1.f, 1.f, 1.f };
                app.mod.components.get<component>(compo_id).name = "Grid";

                add_component_to_current(
                  app, parent, s_parent, compo, click_pos);
            }
        }

        ImGui::Separator();

        if (auto res = app.component_sel.menu("Choose component");
            res.is_done) {
            debug::ensure(is_defined(res.id));

            if (auto* c = app.mod.components.try_to_get<component>(res.id)) {
                if (c->type == component_type::hsm)
                    app.jn.push(
                      log_level::error, [](auto& title, auto& msg) noexcept {
                          title = "Component editor";
                          msg   = "Please, use the hsm_wrapper model to add a "
                                  "hierarchical state machine";
                      });
                else
                    add_component_to_current(app, parent, s_parent, *c);
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_exp) + 1;
            for (; i != e; ++i)
                add_popup_menuitem(app, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_exp) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(app, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_exp) + 1;

            for (; i != e; ++i)
                add_popup_menuitem(app, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            add_popup_menuitem(
              app, parent, s_parent, dynamics_type::logical_and_2, click_pos);
            add_popup_menuitem(
              app, parent, s_parent, dynamics_type::logical_or_2, click_pos);
            add_popup_menuitem(
              app, parent, s_parent, dynamics_type::logical_and_3, click_pos);
            add_popup_menuitem(
              app, parent, s_parent, dynamics_type::logical_or_3, click_pos);
            add_popup_menuitem(
              app, parent, s_parent, dynamics_type::logical_invert, click_pos);
            ImGui::EndMenu();
        }

        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::counter, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::queue, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::dynamic_queue, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::priority_queue, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::generator, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::constant, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::time_func, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::accumulator_2, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::hsm_wrapper, click_pos);
        add_popup_menuitem(
          app, parent, s_parent, dynamics_type::simulation_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
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

static void is_link_created(application& app,
                            generic_component_editor_data& /*data*/,
                            component&         parent,
                            generic_component& s_parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        if (!s_parent.connections.can_alloc()) {
            error_not_enough_connections(app, s_parent.connections.capacity());
            return;
        }

        if (is_input_X(start)) {
            auto port_idx = unpack_X(start);
            auto port_id  = parent.x.get_from_index(port_idx);
            debug::ensure(is_defined(port_id));

            if (is_output_Y(end)) {
                error_not_connection_auth(app);
                return;
            }

            auto  child_port = unpack_in(end);
            auto* child =
              s_parent.children.try_to_get_from_pos(child_port.first);
            debug::ensure(child);

            if (child->type == child_type::model) {
                auto port_in = static_cast<int>(child_port.second);

                if (auto ret = s_parent.connect_input(
                      port_id, *child, connection::port{ .model = port_in });
                    !ret)
                    debug_log("fail to create link");
                parent.state = component_status::modified;
            } else {
                const auto compo_dst =
                  app.mod.components.try_to_get<component>(child->id.compo_id);
                if (not compo_dst)
                    return;

                const auto port_dst_id =
                  compo_dst->x.get_from_index(child_port.second);
                if (is_undefined(port_dst_id))
                    return;

                if (auto ret = s_parent.connect_input(
                      port_id,
                      *child,
                      connection::port{ .compo = port_dst_id });
                    !ret)
                    debug_log("fail to create link\n");
                parent.state = component_status::modified;
            }
        } else {
            auto  ch_port_src = unpack_out(start);
            auto* ch_src =
              s_parent.children.try_to_get_from_pos(ch_port_src.first);
            debug::ensure(ch_src);

            if (is_output_Y(end)) {
                auto port_idx = unpack_Y(end);
                auto port_id  = parent.y.get_from_index(port_idx);
                debug::ensure(is_defined(port_id));

                if (ch_src->type == child_type::model) {
                    auto port_out = static_cast<int>(ch_port_src.second);

                    if (auto ret = s_parent.connect_output(
                          port_id,
                          *ch_src,
                          connection::port{ .model = port_out });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                } else {
                    const auto compo_src =
                      app.mod.components.try_to_get<component>(
                        ch_src->id.compo_id);
                    if (not compo_src)
                        return;

                    const auto port_out =
                      compo_src->y.get_from_index(ch_port_src.second);
                    if (is_undefined(port_out))
                        return;

                    if (auto ret = s_parent.connect_output(
                          port_id,
                          *ch_src,
                          connection::port{ .compo = port_id });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
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
                              app.mod,
                              *ch_src,
                              connection::port{ .model = port_out },
                              *ch_dst,
                              connection::port{ .model = port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    } else {
                        const auto compo_dst =
                          app.mod.components.try_to_get<component>(
                            ch_dst->id.compo_id);
                        if (not compo_dst)
                            return;

                        const auto port_dst_id =
                          compo_dst->x.get_from_index(ch_port_dst.second);
                        if (is_undefined(port_dst_id))
                            return;

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .model = port_out },
                              *ch_dst,
                              connection::port{ .compo = port_dst_id });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    }
                } else {
                    const auto compo_src =
                      app.mod.components.try_to_get<component>(
                        ch_src->id.compo_id);
                    if (not compo_src)
                        return;

                    const auto port_out =
                      compo_src->y.get_from_index(ch_port_src.second);
                    if (is_undefined(port_out))
                        return;

                    if (ch_dst->type == child_type::model) {
                        auto port_in = static_cast<int>(ch_port_dst.second);

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .compo = port_out },
                              *ch_dst,
                              connection::port{ .model = port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    } else {
                        const auto compo_dst =
                          app.mod.components.try_to_get<component>(
                            ch_dst->id.compo_id);
                        if (not compo_dst)
                            return;

                        const auto port_in =
                          compo_dst->x.get_from_index(ch_port_dst.second);
                        if (is_undefined(port_in))
                            return;

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .compo = port_out },
                              *ch_dst,
                              connection::port{ .compo = port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    }
                }
            }
        }
    }
}

static void is_link_destroyed(component&         parent,
                              generic_component& s_parent) noexcept
{
    int link_id;

    if (ImNodes::IsLinkDestroyed(&link_id))
        delete_link(parent, s_parent, link_id);
}

static void show_component_editor(component_editor&              ed,
                                  generic_component_editor_data& data,
                                  component&                     compo,
                                  generic_component& s_compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    ImNodes::EditorContextSet(data.context);
    ImNodes::BeginNodeEditor();

    const auto is_editor_hovered =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) and
      ImNodes::IsEditorHovered();

    show_popup_menuitem(app, data, compo, s_compo);
    show_graph(ed, compo, s_compo);

    if (data.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    is_link_created(app, data, compo, s_compo);
    is_link_destroyed(compo, s_compo);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        data.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(data.selected_nodes.begin());
        remove_component_input_output(data.selected_nodes);
    } else {
        data.selected_nodes.clear();
    }

    if (num_selected_links > 0) {
        data.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(data.selected_links.begin());
    } else {
        data.selected_links.clear();
    }

    if (is_editor_hovered and ImGui::IsKeyReleased(ImGuiKey_Delete) and
        not ImGui::IsAnyItemHovered()) {
        if (num_selected_nodes > 0)
            remove_nodes(app.mod, data, compo);
        else if (num_selected_links > 0)
            remove_links(data, compo, s_compo);
    }
}

generic_component_editor_data::generic_component_editor_data(
  const component_id id,
  component&         compo,
  const generic_component_id /*gid*/,
  const generic_component& gen) noexcept
  : options(5u)
  , m_id{ id }
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

void generic_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* compo = app.mod.components.try_to_get<component>(get_id())) {
        const auto s_id = compo->id.generic_id;

        if (auto* s = app.mod.generic_components.try_to_get(s_id))
            show_component_editor(ed, *this, *compo, *s);
    }
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

static bool show_selected_node(component&                compo,
                               generic_component&        gen,
                               generic_component::child& c) noexcept
{
    const auto id          = gen.children.get_id(c);
    const auto idx         = get_index(id);
    bool       is_modified = false;

    if (ImGui::TreeNodeEx(&c, ImGuiTreeNodeFlags_DefaultOpen, "%u", idx)) {
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

        if (ImGui::InputSmallString("name", gen.children_names[idx]))
            is_modified = true;

        update_unique_id(gen, c);

        ImGui::TextFormat("name: {}", compo.name.sv());
        ImGui::TreePop();
    }

    return is_modified;
}

void generic_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    if (selected_nodes.empty())
        return;

    auto& app = container_of(&ed, &application::component_ed);

    if_component_is_generic(
      app.mod, m_id, [&](component& compo, generic_component& gen) noexcept {
          bool is_modified = false;
          for (int i = 0, e = selected_nodes.size(); i != e; ++i) {
              if (is_node_X(selected_nodes[i]) || is_node_Y(selected_nodes[i]))
                  continue;

              const auto id = unpack_node_child(selected_nodes[i]);
              if (auto* child = gen.children.try_to_get_from_pos(id); child)
                  is_modified =
                    show_selected_node(compo, gen, *child) or is_modified;
          }

          if (is_modified)
              compo.state = component_status::modified;
      });
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

void generic_component_editor_data::store(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto* compo = app.mod.components.try_to_get<component>(m_id);
    if (not compo or compo->type != component_type::generic)
        return;

    auto* gen = app.mod.generic_components.try_to_get(compo->id.generic_id);
    if (not gen)
        return;

    for (auto& c : gen->children) {
        const auto id  = gen->children.get_id(c);
        const auto idx = get_index(id);

        const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_child(id));
        gen->children_positions[idx].x = pos.x;
        gen->children_positions[idx].y = pos.y;
    }

    compo->x.for_each<position>([&](const auto id, auto& position) noexcept {
        const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_X(id));
        position.x     = pos.x;
        position.y     = pos.y;
    });

    compo->y.for_each<position>([&](const auto id, auto& position) noexcept {
        const auto pos = ImNodes::GetNodeEditorSpacePos(pack_node_Y(id));
        position.x     = pos.x;
        position.y     = pos.y;
    });
}

} // namespace irt
