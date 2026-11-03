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
#include "internal.hpp"

#include <cstring>

namespace irt {

using editor_data = generic_component_editor_data;
using node_kind   = editor_data::node::kind;
using link_kind   = editor_data::link::kind;

static constexpr auto no_node = std::numeric_limits<u32>::max();

template<typename Id, typename Fetch>
static ImVec2& get_position(editor_data::positions<Id>& p,
                            const Id                    id,
                            Fetch&&                     fetch) noexcept
{
    const auto idx = get_index(id);

    if (idx >= p.ids.size()) {
        p.ids.resize(idx + 1, undefined<Id>());
        p.values.resize(idx + 1, ImVec2());
    }

    // An identifier reused after a deletion keeps its index: the generation
    // in the identifier tells the cached position is stale.
    if (p.ids[idx] != id) {
        p.ids[idx]    = id;
        p.values[idx] = fetch();
    }

    return p.values[idx];
}

template<typename Id>
static const ImVec2* find_position(const editor_data::positions<Id>& p,
                                   const Id id) noexcept
{
    const auto idx = get_index(id);

    return idx < p.ids.size() and p.ids[idx] == id ? &p.values[idx] : nullptr;
}

/// The ports of a component are shown in their iteration order.
static port_id nth_port(const component::port_type& ports, const u32 n) noexcept
{
    u32 i = 0;
    for (const auto id : ports)
        if (i++ == n)
            return id;

    return undefined<port_id>();
}

static std::optional<u32> port_ordinal(const component::port_type& ports,
                                       const port_id               id) noexcept
{
    u32 i = 0;
    for (const auto p : ports) {
        if (p == id)
            return i;
        ++i;
    }

    return std::nullopt;
}

static const simulation_component* get_wrapped_simulation(
  const component_access&  ids,
  const generic_component& gen,
  const child_id           id) noexcept
{
    const auto& param    = gen.children_parameters[get_index(id)];
    const auto  compo_id = enum_cast<component_id>(
      param.integers[simulation_wrapper_tag::id]);

    if (not ids.exists(compo_id) or
        ids.components[compo_id].type != component_type::simulation)
        return nullptr;

    return ids.sim_components.try_to_get(ids.components[compo_id].id.sim_id);
}

static u32 child_input_count(const component_access&         ids,
                             const generic_component&        gen,
                             const generic_component::child& c) noexcept
{
    const auto id = gen.children.get_id(c);

    if (c.type == child_type::component)
        return ids.exists(c.id.compo_id)
                 ? static_cast<u32>(ids.components[c.id.compo_id].x.size())
                 : 0u;

    if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
        const auto* sim = get_wrapped_simulation(ids, gen, id);
        return 2u + (sim ? static_cast<u32>(sim->factors.size()) : 0u);
    }

    return static_cast<u32>(get_input_port_names(c.id.mdl_type).size());
}

static u32 child_output_count(const component_access&         ids,
                              const generic_component&        gen,
                              const generic_component::child& c) noexcept
{
    const auto id = gen.children.get_id(c);

    if (c.type == child_type::component)
        return ids.exists(c.id.compo_id)
                 ? static_cast<u32>(ids.components[c.id.compo_id].y.size())
                 : 0u;

    if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
        const auto* sim = get_wrapped_simulation(ids, gen, id);
        return sim ? static_cast<u32>(sim->factors.size() +
                                      sim->selections.size())
                   : 0u;
    }

    return static_cast<u32>(get_output_port_names(c.id.mdl_type).size());
}

/// The pin of a connection port of a child, or nothing when the port does not
/// exist anymore.
static std::optional<u32> child_pin(const component_access&         ids,
                                    const generic_component&        gen,
                                    const generic_component::child& c,
                                    const connection::port          p,
                                    const bool is_input) noexcept
{
    if (c.type == child_type::component) {
        if (not ids.exists(c.id.compo_id))
            return std::nullopt;

        const auto& compo = ids.components[c.id.compo_id];
        return port_ordinal(is_input ? compo.x : compo.y, p.compo);
    }

    // Only the ports of the simulation_wrapper depend on other data: the
    // connections of the other models are kept even out of range.
    if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
        const auto count = is_input ? child_input_count(ids, gen, c)
                                    : child_output_count(ids, gen, c);
        if (p.model < 0 or std::cmp_greater_equal(p.model, count))
            return std::nullopt;
    }

    return static_cast<u32>(p.model);
}

static std::optional<connection::port> child_port(
  const component_access&         ids,
  const generic_component::child& c,
  const u32                       pin,
  const bool                      is_input) noexcept
{
    if (c.type == child_type::model)
        return connection::port{ .model = static_cast<int>(pin) };

    if (not ids.exists(c.id.compo_id))
        return std::nullopt;

    const auto& compo = ids.components[c.id.compo_id];
    const auto  id    = nth_port(is_input ? compo.x : compo.y, pin);

    return is_defined(id) ? std::make_optional(connection::port{ .compo = id })
                          : std::nullopt;
}

static void build_nodes(const component&         compo,
                        const generic_component& gen,
                        editor_data&             data) noexcept
{
    vector<editor_data::node> nodes;
    nodes.reserve(compo.x.size() + compo.y.size() + gen.children.size());

    for (const auto id : compo.x)
        nodes.push_back({ .type = node_kind::input, .port = id });

    for (const auto id : compo.y)
        nodes.push_back({ .type = node_kind::output, .port = id });

    for (const auto& c : gen.children)
        nodes.push_back(
          { .type = node_kind::child, .child = gen.children.get_id(c) });

    if (std::ranges::equal(nodes, data.nodes))
        return;

    data.nodes = std::move(nodes);
    data.editor.reset();

    data.child_nodes.clear();
    data.x_nodes.clear();
    data.y_nodes.clear();
    data.child_nodes.resize(gen.children.capacity(), no_node);
    data.x_nodes.resize(compo.x.capacity(), no_node);
    data.y_nodes.resize(compo.y.capacity(), no_node);

    for (u32 i = 0, e = static_cast<u32>(data.nodes.size()); i != e; ++i) {
        const auto& n = data.nodes[i];

        switch (n.type) {
        case node_kind::input:
            data.x_nodes[get_index(n.port)] = i;
            break;
        case node_kind::output:
            data.y_nodes[get_index(n.port)] = i;
            break;
        case node_kind::child:
            data.child_nodes[get_index(n.child)] = i;
            break;
        }
    }
}

/// Builds the links of the graph and frees the connections to a port or a
/// child which does not exist anymore.
static bool build_links(const component_access& ids,
                        const component&        compo,
                        generic_component&      gen,
                        editor_data&            data) noexcept
{
    const auto old_size = data.links.size();
    auto       freed    = 0;

    data.links.clear();

    for (auto& con : gen.connections) {
        const auto* s   = gen.children.try_to_get(con.src);
        const auto* d   = gen.children.try_to_get(con.dst);
        const auto  src = s ? child_pin(ids, gen, *s, con.index_src, false)
                            : std::nullopt;
        const auto  dst = d ? child_pin(ids, gen, *d, con.index_dst, true)
                            : std::nullopt;

        if (not(src and dst)) {
            gen.connections.free(con);
            ++freed;
            continue;
        }

        data.links.push_back(
          { .type  = link_kind::internal,
            .index = static_cast<u32>(get_index(gen.connections.get_id(con))),
            .out_node = data.child_nodes[get_index(con.src)],
            .out_port = *src,
            .in_node  = data.child_nodes[get_index(con.dst)],
            .in_port  = *dst });
    }

    for (auto& con : gen.input_connections) {
        const auto* d   = gen.children.try_to_get(con.dst);
        const auto  dst = d ? child_pin(ids, gen, *d, con.port, true)
                            : std::nullopt;

        if (not(compo.x.exists(con.x) and dst)) {
            gen.input_connections.free(con);
            ++freed;
            continue;
        }

        data.links.push_back({ .type  = link_kind::input,
                               .index = static_cast<u32>(
                                 get_index(gen.input_connections.get_id(con))),
                               .out_node = data.x_nodes[get_index(con.x)],
                               .out_port = 0,
                               .in_node  = data.child_nodes[get_index(con.dst)],
                               .in_port  = *dst });
    }

    for (auto& con : gen.output_connections) {
        const auto* s   = gen.children.try_to_get(con.src);
        const auto  src = s ? child_pin(ids, gen, *s, con.port, false)
                            : std::nullopt;

        if (not(compo.y.exists(con.y) and src)) {
            gen.output_connections.free(con);
            ++freed;
            continue;
        }

        data.links.push_back({ .type  = link_kind::output,
                               .index = static_cast<u32>(
                                 get_index(gen.output_connections.get_id(con))),
                               .out_node = data.child_nodes[get_index(con.src)],
                               .out_port = *src,
                               .in_node  = data.y_nodes[get_index(con.y)],
                               .in_port  = 0 });
    }

    // The link indices of the selection refer to the previous links.
    if (data.links.size() != old_size)
        data.editor.clear_selection();

    return freed > 0;
}

/// The view of a generic component for the @c node_editor.
struct generic_component_editor_data::graph {
    application&            app;
    const component_access& ids;
    component_id            compo_id;
    component&              compo;
    generic_component&      gen;
    editor_data&            data;

    ImU32 model_colors[2]     = {};
    ImU32 component_colors[2] = {};

    small_string<127> buffer;
    int               changes = 0;

    graph(application&            app_,
          const component_access& ids_,
          const component_id      compo_id_,
          component&              compo_,
          generic_component&      gen_,
          editor_data&            data_) noexcept
      : app(app_)
      , ids(ids_)
      , compo_id(compo_id_)
      , compo(compo_)
      , gen(gen_)
      , data(data_)
    {
        app.config.vars.colors.read(
          [&](const auto& colors, const auto /*v*/) noexcept {
              model_colors[0] = to_ImU32(colors[style_color::node_2]);
              model_colors[1] = to_ImU32(colors[style_color::node_2_active]);
              component_colors[0] = to_ImU32(colors[style_color::node]);
              component_colors[1] = to_ImU32(colors[style_color::node_active]);
          });
    }

    const editor_data::node& node(const u32 i) const noexcept
    {
        return data.nodes[i];
    }

    generic_component::child& child(const u32 i) noexcept
    {
        return gen.children.get(node(i).child);
    }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(data.nodes.size());
    }

    ImVec2& position(const u32 i) noexcept
    {
        const auto& n = node(i);

        switch (n.type) {
        case node_kind::input:
            return get_position(data.x_positions, n.port, [&]() noexcept {
                const auto& p = compo.x.get<irt::position>(n.port);
                return ImVec2(p.x, p.y);
            });

        case node_kind::output:
            return get_position(data.y_positions, n.port, [&]() noexcept {
                const auto& p = compo.y.get<irt::position>(n.port);
                return ImVec2(p.x, p.y);
            });

        case node_kind::child:
            break;
        }

        return get_position(data.child_positions, n.child, [&]() noexcept {
            const auto& p = gen.children_positions[get_index(n.child)];
            return ImVec2(p.x, p.y);
        });
    }

    int layout_column(const u32 i) const noexcept
    {
        switch (node(i).type) {
        case node_kind::input:
            return -1;
        case node_kind::output:
            return 1;
        case node_kind::child:
            break;
        }

        return 0;
    }

    ImU32 title_color(const u32 i, const bool selected) noexcept
    {
        const auto is_model = node(i).type == node_kind::child and
                              child(i).type == child_type::model;

        return is_model ? model_colors[selected] : component_colors[selected];
    }

    std::string_view title(const u32 i) noexcept
    {
        const auto& n = node(i);

        if (n.type == node_kind::input)
            return compo.x.get<port_str>(n.port).sv();

        if (n.type == node_kind::output)
            return compo.y.get<port_str>(n.port).sv();

        const auto& c    = child(i);
        const auto  name = gen.children_names[get_index(n.child)].sv();
        const auto  type = c.type == child_type::model
                             ? std::string_view(
                                dynamics_type_names[ordinal(c.id.mdl_type)])
                           : ids.exists(c.id.compo_id)
                             ? ids.components[c.id.compo_id].name.sv()
                             : std::string_view("unknown component");

        if (name.empty())
            return type;

        format(buffer, "{} ({})", name, type);
        return buffer.sv();
    }

    u32 input_count(const u32 i) noexcept
    {
        switch (node(i).type) {
        case node_kind::input:
            return 0;
        case node_kind::output:
            return 1;
        case node_kind::child:
            break;
        }

        return child_input_count(ids, gen, child(i));
    }

    u32 output_count(const u32 i) noexcept
    {
        switch (node(i).type) {
        case node_kind::input:
            return 1;
        case node_kind::output:
            return 0;
        case node_kind::child:
            break;
        }

        return child_output_count(ids, gen, child(i));
    }

    std::string_view port_name(const component::port_type& ports,
                               const u32                   port) noexcept
    {
        const auto id = nth_port(ports, port);
        if (is_undefined(id))
            return {};

        const auto option = ports.get<port_option>(id);
        if (option == port_option::classic)
            return ports.get<port_str>(id).sv();

        format(buffer, "{} ({})", ports.get<port_str>(id).sv(),
               port_option_names[ordinal(option)]);
        return buffer.sv();
    }

    template<typename Ids>
    std::string_view nth_name(const Ids& names_ids, u32 n) noexcept
    {
        for (const auto id : names_ids)
            if (n-- == 0)
                return names_ids.template get<name_str>(id).sv();

        return {};
    }

    std::string_view input_name(const u32 i, const u32 port) noexcept
    {
        if (node(i).type != node_kind::child)
            return {};

        const auto& c = child(i);
        if (c.type == child_type::component)
            return ids.exists(c.id.compo_id)
                     ? port_name(ids.components[c.id.compo_id].x, port)
                     : std::string_view();

        if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
            constexpr std::string_view names[] = { "init", "run" };
            if (port < 2)
                return names[port];

            const auto* sim = get_wrapped_simulation(ids, gen, node(i).child);
            return sim ? nth_name(sim->factors, port - 2) : std::string_view();
        }

        const auto names = get_input_port_names(c.id.mdl_type);
        return port < names.size() ? names[port] : std::string_view();
    }

    std::string_view output_name(const u32 i, const u32 port) noexcept
    {
        if (node(i).type != node_kind::child)
            return {};

        const auto& c = child(i);
        if (c.type == child_type::component)
            return ids.exists(c.id.compo_id)
                     ? port_name(ids.components[c.id.compo_id].y, port)
                     : std::string_view();

        if (c.id.mdl_type == dynamics_type::simulation_wrapper) {
            // Same order as the simulation_wrapper: the factors, then the
            // selections.
            const auto* sim = get_wrapped_simulation(ids, gen, node(i).child);
            if (not sim)
                return {};

            const auto factors = static_cast<u32>(sim->factors.size());
            return port < factors ? nth_name(sim->factors, port)
                                  : nth_name(sim->selections, port - factors);
        }

        const auto names = get_output_port_names(c.id.mdl_type);
        return port < names.size() ? names[port] : std::string_view();
    }

    void content(const u32 i) noexcept
    {
        if (node(i).type != node_kind::child)
            return;

        const auto  id  = node(i).child;
        const auto  idx = get_index(id);
        const auto& c   = child(i);

        if (c.type == child_type::component) {
            if (not ids.exists(c.id.compo_id))
                return;

            const auto& sub = ids.components[c.id.compo_id];
            if (sub.type == component_type::grid) {
                if (const auto* g = ids.grid_components.try_to_get(
                      sub.id.grid_id))
                    ImGui::TextFormat("{}x{}", g->row(), g->column());
            } else if (sub.type == component_type::graph) {
                if (const auto* g = ids.graph_components.try_to_get(
                      sub.id.graph_id))
                    ImGui::TextFormat("{}v {}e", g->g.nodes.size(),
                                      g->g.edges.size());
            }

            if (sub.x.empty() and sub.y.empty())
                ImGui::TextUnformatted("No ports defined.");

            return;
        }

        auto& param = gen.children_parameters[idx];

        ImGui::PushItemWidth(120.0f);
        if (c.id.mdl_type == dynamics_type::hsm_wrapper)
            changes += show_extented_hsm_parameter(app, param);

        changes += show_parameter_editor(app, compo.srcs, c.id.mdl_type, param);

        if (c.id.mdl_type == dynamics_type::constant)
            changes += show_extented_constant_parameter(ids, compo_id, param);
        ImGui::PopItemWidth();
    }

    template<typename Fn>
    void for_each_link(Fn&& fn) noexcept
    {
        for (const auto& l : data.links)
            fn(l.out_node, l.out_port, l.in_node, l.in_port);
    }
};

static bool remove_nodes(generic_component& gen, editor_data& data) noexcept
{
    auto u = 0;

    for (const auto i : data.editor.selected_nodes()) {
        const auto& n = data.nodes[i];

        if (n.type == node_kind::child) {
            if (auto* child = gen.children.try_to_get(n.child)) {
                gen.children.free(*child);
                ++u;
            }
        }
    }

    return u > 0;
}

static bool remove_links(generic_component& gen, editor_data& data) noexcept
{
    auto u = 0;

    for (const auto i : data.editor.selected_links()) {
        const auto& l = data.links[i];

        switch (l.type) {
        case link_kind::internal:
            if (auto* con = gen.connections.try_to_get_from_pos(l.index)) {
                gen.connections.free(*con);
                ++u;
            }
            break;

        case link_kind::input:
            if (auto* con = gen.input_connections.try_to_get_from_pos(
                  l.index)) {
                gen.input_connections.free(*con);
                ++u;
            }
            break;

        case link_kind::output:
            if (auto* con = gen.output_connections.try_to_get_from_pos(
                  l.index)) {
                gen.output_connections.free(*con);
                ++u;
            }
            break;
        }
    }

    return u > 0;
}

static bool add_component_to_current(const component_access& ids,
                                     const component_id      parent_id,
                                     generic_component&      parent_compo,
                                     const component_id      compo_to_add_id,
                                     ImVec2                  click_pos)
{
    if (not ids.can_add(parent_id, compo_to_add_id)) {
        log(log_level::error, [&ids, compo_to_add_id](auto& m) {
            format(m,
                   "modeling: generic component does not accept recursive "
                   "component {}",
                   ids.components[compo_to_add_id].name.sv());
        });

        return false;
    }

    if (not parent_compo.children.can_alloc(1) and
        not parent_compo.grow_children()) {
        log(log_level::error, [](auto& m) {
            m = "modeling: can not allocate new model. Delete models or "
                "increase "
                "generic component default size.";
        });

        return false;
    }

    auto&      c     = parent_compo.children.alloc(compo_to_add_id);
    const auto c_id  = parent_compo.children.get_id(c);
    const auto c_idx = get_index(c_id);

    parent_compo.children_positions[c_idx].x = click_pos.x;
    parent_compo.children_positions[c_idx].y = click_pos.y;

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

    u += show_menu(gen, dynamics_type::qss1_sample_hold, offset, pos,
                   "Samples the continuous input on an internal clock (period "
                   "ts) and holds each sample until the next tick");

    u += show_menu(gen, dynamics_type::qss1_flipflop, offset, pos,
                   "Emits output continuous from a input continous when an "
                   "event arrives on event port");

    u += show_menu(gen, dynamics_type::qss1_quantizer, offset, pos,
                   "y = q · round(x / q); emits an event each time the input "
                   "crosses a quantization level.");

    u += show_menu(gen, dynamics_type::qss1_integer, offset, pos,
                   "y = round(x); emits an event each time the input "
                   "crosses a integer level.");

    u += show_menu(gen, dynamics_type::qss1_threshold_crossing, offset, pos,
                   "Emits an event (±1) when the input crosses a level; a "
                   "decoupled zero-crossing detector");

    u += show_menu(gen, dynamics_type::qss1_cross, offset, pos,
                   "Emits continuous event and an event "
                   "(±1) when the input crosses a level; a "
                   "decoupled zero-crossing detector");

    u += show_menu(gen, dynamics_type::qss1_integrate_and_fire, offset, pos,
                   "Accumulates ∫x dt and emits a spike, then resets, when the "
                   "accumulator reaches θ");

    u += show_menu(gen, dynamics_type::qss1_pwm, offset, pos,
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
      gen, dynamics_type::qss1_hysteresis, offset, pos,
      "Binary output that flips high when the input rises past upper and "
      "low "
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

    u += show_menu(gen, dynamics_type::qss1_max_hold, offset, pos,
                   "Running peak detector: tracks the input while it sets a "
                   "new maximum, then holds that max value");

    u += show_menu(gen, dynamics_type::qss1_min_hold, offset, pos,
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

    u += show_menu(gen, dynamics_type::qss1_abs, offset, pos,
                   "Absolute value y = |x|");

    u += show_menu(gen, dynamics_type::qss1_saturation, offset, pos,
                   "Limits the signal to [lower, upper]; "
                   "passes through in-band, flat outside.");

    u += show_menu(gen, dynamics_type::qss1_filter, offset, pos,
                   "Limits the signal to [lower, upper]; "
                   "sends 0 on one events.");

    u += show_menu(gen, dynamics_type::qss1_dead_zone, offset, pos,
                   "Zero output inside [lower, upper], linear (shifted) "
                   "beyond; suppresses small signals");

    u += show_menu(gen, dynamics_type::qss1_sign, offset, pos,
                   "Sign function y ∈ {−1, 0, +1}; value discontinuity at 0");

    u += show_menu(gen, dynamics_type::qss1_maximum, offset, pos,
                   "Selects the larger of two continuous "
                   "inputs, switching at their crossing");

    u += show_menu(gen, dynamics_type::qss1_minimum, offset, pos,
                   "Selects the smaller of two continuous "
                   "inputs, switching at their crossing");

    u += show_menu(gen, dynamics_type::qss1_wrap, offset, pos,
                   "Folds the signal into [origin, origin + modulo), jumping "
                   "by  ±modulo at each boundary (e.g. angle wrapping)");

    u += show_menu(gen, dynamics_type::qss1_compare, offset, pos,
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

    u += show_menu(gen, dynamics_type::qss1_integrator, offset, pos,
                   "Integrator");

    u += show_menu(gen, dynamics_type::qss1_sqrt, offset, pos,
                   "Square root, y = √x; guarded for x > 0.");

    u += show_menu(gen, dynamics_type::qss1_gain, offset, pos,
                   "Gaim, y = k(x)");

    u += show_menu(gen, dynamics_type::qss1_power, offset, pos, "Power y = xⁿ");

    u += show_menu(gen, dynamics_type::qss1_square, offset, pos,
                   "Square, y = x¹");

    u += show_menu(gen, dynamics_type::qss1_sum_2, offset, pos,
                   "Sum, y = x[0] + x[1]");

    u += show_menu(gen, dynamics_type::qss1_sum_3, offset, pos,
                   "Sum, y = x[0] + x[1] + x[2]");

    u += show_menu(gen, dynamics_type::qss1_sum_4, offset, pos,
                   "Sum, y = x[0] + x[1] + x[2] + x[3]");

    u += show_menu(gen, dynamics_type::qss1_wsum_2, offset, pos,
                   "Weighted sm, y = k_0 · x[0] + k_1 · x[1]");

    u += show_menu(gen, dynamics_type::qss1_wsum_3, offset, pos,
                   "Weighted sm, y = k_0 · x[0] + k_1 · x[1] + k_2 · x[2]");

    u += show_menu(gen, dynamics_type::qss1_wsum_4, offset, pos,
                   "Weighted sm, y = k_0 · x[0] + k_1 · x[1] + k_2 · "
                   "x[2] + k_3 · x[3]");

    u += show_menu(gen, dynamics_type::qss1_inverse, offset, pos,
                   "Inverse, y = 1 / x");

    u += show_menu(gen, dynamics_type::qss1_multiplier, offset, pos,
                   "Multiplier, y = x[0] · x[1]");

    u += show_menu(gen, dynamics_type::qss1_log, offset, pos,
                   "Sinus, y = log(x)");

    u += show_menu(gen, dynamics_type::qss1_exp, offset, pos,
                   "Cosinus, y = exp(x)");

    u += show_menu(gen, dynamics_type::qss1_sin, offset, pos,
                   "Sinus, y = sin(x)");

    u += show_menu(gen, dynamics_type::qss1_cos, offset, pos,
                   "Cosinus, y = cos(x)");

    u += show_menu(gen, dynamics_type::qss1_division, offset, pos,
                   "Ratio of two continuous signals, y = a "
                   "/ b; guarded for b ≠ 0.");

    u += show_menu(gen, dynamics_type::qss1_atan, offset, pos,
                   "Arctangent, y = atan(x).");

    u += show_menu(gen, dynamics_type::qss1_atan2, offset, pos,
                   "Two-argument arctangent y = atan2(yin, xin)");

    u += show_menu(gen, dynamics_type::qss1_tan, offset, pos,
                   "Tangent, y = tan(x)");

    u += show_menu(gen, dynamics_type::qss1_tanh, offset, pos,
                   "Hyperbolic tangent");

    u += show_menu(gen, dynamics_type::qss1_sigmoid, offset, pos,
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

    const bool open_popup = ImGui::IsWindowFocused(
                              ImGuiFocusedFlags_RootAndChildWindows) and
                            data.editor.is_background_hovered() and
                            ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        const auto click_pos = data.editor.to_world(
          ImGui::GetMousePosOnOpeningCurrentPopup());

        if (not data.editor.selected_links().empty())
            if (ImGui::MenuItem("Delete selected links")) {
                u += remove_links(gen, data);
                data.editor.clear_selection();
            }

        if (not data.editor.selected_nodes().empty())
            if (ImGui::MenuItem("Delete selected nodes")) {
                u += remove_nodes(gen, data);
                data.editor.clear_selection();
            }

        if (ImGui::MenuItem("Grid layout"))
            data.editor.request_grid_layout();

        if (ImGui::MenuItem("Automatic layout"))
            data.editor.request_force_layout();

        ImGui::MenuItem("Show minimap", "", &data.editor.show_minimap);

        ImGui::Separator();

        if (auto res = app.component_sel.menu("Choose component");
            res.is_done) {
            debug::ensure(is_defined(res.id));

            if (ids.exists(res.id)) {
                auto& c = ids.components[res.id];
                if (c.type == component_type::hsm)
                    log(log_level::error, [](auto& msg) noexcept {
                        msg = "modeling: please, use the hsm_wrapper model to "
                              "add a hierarchical state machine";
                    });
                else
                    u += add_component_to_current(ids, parent_id, gen, res.id,
                                                  click_pos);
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
              "continuous out, memoryless, with internal state events at "
              "the "
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
              "Holds the last received discrete event value as a "
              "constant "
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

static void error_not_enough_connections(sz capacity) noexcept
{
    log(log_level::error, [&](auto& m) {
        format(m,
               "modeling: not enough connection slot in ths component. All "
               "connections slots ({}) are used.",
               capacity);
    });
}

static void error_not_connection_auth() noexcept
{
    log(log_level::error, [&](auto& m) {
        m = "modeling: can not connect component input on output ports";
    });
}

static bool connect(const component_access&         ids,
                    generic_component&              gen,
                    editor_data&                    data,
                    const node_editor::link_request req) noexcept
{
    const auto& out = data.nodes[req.out.node];
    const auto& in  = data.nodes[req.in.node];

    if (out.type == node_kind::input and in.type == node_kind::output) {
        error_not_connection_auth();
        return false;
    }

    if (not gen.connections.can_alloc() and not gen.connections.grow<2, 1>()) {
        error_not_enough_connections(gen.connections.capacity());
        return false;
    }

    if (out.type == node_kind::input) {
        auto&      dst  = gen.children.get(in.child);
        const auto port = child_port(ids, dst, req.in.port, true);

        return port and gen.connect_input(out.port, dst, *port).has_value();
    }

    auto&      src      = gen.children.get(out.child);
    const auto port_src = child_port(ids, src, req.out.port, false);
    if (not port_src)
        return false;

    if (in.type == node_kind::output)
        return gen.connect_output(in.port, src, *port_src).has_value();

    auto&      dst      = gen.children.get(in.child);
    const auto port_dst = child_port(ids, dst, req.in.port, true);

    return port_dst and gen.connect(src, *port_src, dst, *port_dst).has_value();
}

static bool show_component_editor(component_editor&       ed,
                                  const component_access& ids,
                                  editor_data&            data,
                                  component_id            compo_id,
                                  component&              compo,
                                  generic_component&      gen) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto  u   = 0;

    build_nodes(compo, gen, data);
    u += build_links(ids, compo, gen, data);

    auto       g  = editor_data::graph(app, ids, compo_id, compo, gen, data);
    const auto ev = data.editor.show("generic-graph", g);
    u += g.changes;

    u += show_popup_menuitem(ids, app, data, compo_id, gen);

    if (ev.link_created)
        u += connect(ids, gen, data, *ev.link_created);

    if (ev.delete_selection) {
        u += remove_links(gen, data);
        u += remove_nodes(gen, data);
        data.editor.clear_selection();
    }

    if (ev.nodes_moved)
        ++u;

    return u > 0;
}

generic_component_editor_data::generic_component_editor_data(
  const component_id id,
  const component& /*compo*/,
  const generic_component_id /*gid*/,
  const generic_component& /*gen*/) noexcept
  : options(5u)
  , m_id(id)
{}

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
        ImGui::TextFormat("position {},{}", gen.children_positions[idx].x,
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
    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);

    auto selection_option = bitflags<child_flags>{};
    auto force_selection  = false;

    if (ImGui::BeginMenu("Selection")) {
        if (ImGui::MenuItem("None"))
            force_selection = true;

        if (ImGui::MenuItem("Observables")) {
            selection_option.set(child_flags::observable, true);
            force_selection = true;
        }

        if (ImGui::MenuItem("Configurables")) {
            selection_option.set(child_flags::configurable, true);
            force_selection = true;
        }

        if (ImGui::MenuItem("Both")) {
            selection_option = bitflags<child_flags>(child_flags::configurable,
                                                     child_flags::observable);
            force_selection  = true;
        }
        ImGui::EndMenu();
    }

    if (force_selection) {
        editor.clear_selection();

        if (selection_option.any()) {
            for (u32 i = 0, e = static_cast<u32>(nodes.size()); i != e; ++i) {
                if (nodes[i].type != node::kind::child)
                    continue;

                if (const auto* c = m_generic.children.try_to_get(
                      nodes[i].child);
                    c and (c->flags & selection_option).any())
                    editor.select(i);
            }
        }
    }

    int u = 0;
    for (const auto i : editor.selected_nodes()) {
        if (nodes[i].type != node::kind::child)
            continue;

        if (auto* child = m_generic.children.try_to_get(nodes[i].child))
            u += show_selected_node(m_generic, *child);
    }

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

void generic_component_editor_data::read(application& app,
                                         component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        if (compo.type == component_type::generic) {
            const auto gen_id = compo.id.generic_id;

            if (auto* gen = ids.generic_components.try_to_get(gen_id))
                m_generic = *gen;
        }
    });
}

void generic_component_editor_data::write(application& app,
                                          component&   compo) noexcept
{
    for (const auto& c : m_generic.children) {
        const auto id = m_generic.children.get_id(c);

        if (const auto* pos = find_position(child_positions, id)) {
            m_generic.children_positions[get_index(id)].x = pos->x;
            m_generic.children_positions[get_index(id)].y = pos->y;
        }
    }

    for (const auto id : compo.x) {
        if (const auto* pos = find_position(x_positions, id)) {
            compo.x.get<position>(id).x = pos->x;
            compo.x.get<position>(id).y = pos->y;
        }
    }

    for (const auto id : compo.y) {
        if (const auto* pos = find_position(y_positions, id)) {
            compo.y.get<position>(id).x = pos->x;
            compo.y.get<position>(id).y = pos->y;
        }
    }

    auto c_ptr = std::make_unique<component>(compo);
    auto g_ptr = std::make_unique<generic_component>(m_generic);

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (c->type == component_type::generic) {
                  const auto gen_id = c->id.generic_id;

                  if (auto* gen = ids.generic_components.try_to_get(gen_id)) {
                      *gen = *g;
                  }
              }
          });
      });
}

} // namespace irt
