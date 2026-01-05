// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>

namespace irt {

bool graph_component::exists_child(const std::string_view name) const noexcept
{
    for (const auto id : g.nodes)
        if (g.node_names[id] == name)
            return true;

    return false;
}

name_str graph_component::make_unique_name_id(
  const graph_node_id v) const noexcept
{
    debug::ensure(g.nodes.exists(v));

    return format_n<31>("{}", get_index(v));
}

static auto build_graph_children(modeling& mod, graph_component& graph) noexcept
  -> table<graph_node_id, child_id>
{
    table<graph_node_id, child_id> tr;
    tr.data.reserve(graph.g.nodes.ssize());

    for (const auto node_id : graph.g.nodes) {
        const auto compo_id = graph.g.node_components[node_id];

        if (mod.components.exists(compo_id)) {
            auto& ch = graph.cache.alloc(compo_id, node_id);
            auto  id = graph.cache.get_id(ch);
            tr.data.emplace_back(node_id, id);
        } else {
            tr.data.emplace_back(node_id, undefined<child_id>());
        }
    }

    tr.sort();
    graph.cache_names.resize(tr.size());

    tr.data.for_each([&](const auto& x) {
        graph.cache_names[x.value] = graph.make_unique_name_id(x.id);
    });

    return tr;
}

struct get_edges_result {
    child_id         src;
    child_id         dst;
    component&       c_src;
    component&       c_dst;
    std::string_view p_src;
    std::string_view p_dst;
};

static auto get_edges(modeling&                             mod,
                      graph_component&                      graph,
                      const table<graph_node_id, child_id>& vertex,
                      const u16                             index) noexcept
  -> std::optional<get_edges_result>
{
    const auto u_id = graph.g.edges_nodes[index][0].first;
    const auto v_id = graph.g.edges_nodes[index][1].first;

    if (graph.g.nodes.exists(u_id) and graph.g.nodes.exists(v_id)) {
        const auto u = vertex.get(u_id);
        const auto v = vertex.get(v_id);

        if (auto* src = graph.cache.try_to_get(*u)) {
            if (auto* dst = graph.cache.try_to_get(*v)) {
                if (auto* c_src =
                      mod.components.try_to_get<component>(src->compo_id))
                    if (auto* c_dst =
                          mod.components.try_to_get<component>(dst->compo_id))
                        return get_edges_result{
                            .src   = *u,
                            .dst   = *v,
                            .c_src = *c_src,
                            .c_dst = *c_dst,
                            .p_src = graph.g.edges_nodes[index][0].second,
                            .p_dst = graph.g.edges_nodes[index][1].second,
                        };
            }
        }
    }

    return std::nullopt;
}

graph_component::graph_component() noexcept
  : rng(static_cast<u64>(reinterpret_cast<uintptr_t>(this)), 0x957864123u, 0u)
{}

void graph_component::update_position() noexcept
{
    reset_position();

    for (const auto id : g.nodes) {
        const auto idx = get_index(id);

        top_left_limit[0] = std::min(
          top_left_limit[0], g.node_positions[idx][0] - g.node_areas[idx]);
        top_left_limit[1] = std::min(
          top_left_limit[1], g.node_positions[idx][1] - g.node_areas[idx]);
        bottom_right_limit[0] = std::max(
          bottom_right_limit[0], g.node_positions[idx][0] + g.node_areas[idx]);
        bottom_right_limit[1] = std::max(
          bottom_right_limit[1], g.node_positions[idx][1] + g.node_areas[idx]);
    }

    if (top_left_limit[0] == bottom_right_limit[0]) {
        top_left_limit[0]     = -1.f;
        bottom_right_limit[0] = 1.f;
    }

    if (top_left_limit[1] == bottom_right_limit[1]) {
        top_left_limit[1]     = -1.f;
        bottom_right_limit[1] = 1.f;
    }
}

void graph_component::assign_grid_position(float distance_x,
                                           float distance_y) noexcept
{
    debug::ensure(not g.nodes.empty());
    debug::ensure(g_type != graph_component::graph_type::dot_file);

    const auto nb        = g.nodes.size();
    const auto sqrt      = std::sqrt(nb);
    const auto line      = static_cast<unsigned>(sqrt);
    const auto col       = nb / line;
    const auto remaining = nb - (col * line);

    auto it = g.nodes.begin();
    for (auto l = 0u; l < line; ++l) {
        for (auto c = 0u; c < col; ++c) {
            g.node_positions[get_index(*it)][0] =
              (distance_x + g.node_areas[get_index(*it)]) * c;
            g.node_positions[get_index(*it)][1] =
              (distance_y + g.node_areas[get_index(*it)]) * l;
            ++it;
        }
    }

    for (auto r = 0u; r < remaining; ++r) {
        g.node_positions[get_index(*it)][0] =
          (distance_x + g.node_areas[get_index(*it)]) * r;
        g.node_positions[get_index(*it)][1] =
          (distance_y + g.node_areas[get_index(*it)]) * line;
        ++it;
    }

    update_position();
}

void graph_component::reset_position() noexcept
{
    top_left_limit     = { +INFINITY, +INFINITY };
    bottom_right_limit = { -INFINITY, -INFINITY };
}

static constexpr auto exists_connection(const graph_component& graph,
                                        const child_id         src_id,
                                        const port_id          p_src,
                                        const child_id         dst_id,
                                        const port_id p_dst) noexcept -> bool
{
    for (const auto& elem : graph.cache_connections)
        if (elem.src == src_id and elem.dst == dst_id and
            elem.index_src.compo == p_src and elem.index_dst.compo == p_dst)
            return true;

    return false;
}

enum connection_add_result : u8 { done, nomem, noexist };

static connection_add_result connection_add(graph_component& compo,
                                            const child_id   src,
                                            const port_id    p_src,
                                            const child_id   dst,
                                            const port_id    p_dst) noexcept
{
    if (not compo.cache_connections.can_alloc(1) and
        not compo.cache_connections.grow<3, 2>())
        return connection_add_result::nomem;

    compo.cache_connections.alloc(src, p_src, dst, p_dst);
    return connection_add_result::done;
}

static connection_add_result in_out_connection_add(
  graph_component&        compo,
  const get_edges_result& edge) noexcept
{
    const auto oport = edge.p_src.empty() ? "out" : edge.p_src;
    const auto iport = edge.p_dst.empty() ? "in" : edge.p_dst;

    if (const auto p_src = edge.c_src.get_y(oport); is_defined(p_src))
        if (const auto p_dst = edge.c_dst.get_x(iport); is_defined(p_dst))
            return connection_add(compo, edge.src, p_src, edge.dst, p_dst);

    return connection_add_result::noexist;
}

static connection_add_result named_connection_add(
  graph_component&        compo,
  const get_edges_result& edge) noexcept
{
    // No input and output ports are defined. Trying to connect input and output
    // port with the same name.

    if (edge.p_src.empty() and edge.p_dst.empty()) {
        edge.c_src.y.for_each<port_str>(
          [&](const auto sid, const auto& sname) noexcept {
              edge.c_dst.x.for_each<port_str>([&](const auto  did,
                                                  const auto& dname) noexcept {
                  if (sname == dname) {
                      if (connection_add(compo, edge.src, sid, edge.dst, did) ==
                          connection_add_result::nomem)
                          return;
                  }
              });
          });

        if (not compo.g.flags[graph::option_flags::directed]) {
            edge.c_dst.y.for_each<port_str>(
              [&](const auto sid, const auto& sname) noexcept {
                  edge.c_src.x.for_each<port_str>(
                    [&](const auto did, const auto& dname) noexcept {
                        if (sname == dname) {
                            if (connection_add(
                                  compo, edge.dst, sid, edge.src, did) ==
                                connection_add_result::nomem)
                                return;
                        }
                    });
              });
        }

        return connection_add_result::done;
    }

    // At least one port is defined, trying to connect input and output port
    // with the same name. If the two ports are defined, force the connection
    // without searching input or output port with the sane name.

    auto p_src = edge.c_src.get_y(edge.p_src);
    auto p_dst = edge.c_dst.get_x(edge.p_dst);

    p_src = is_undefined(p_src) ? edge.c_src.get_y(edge.p_dst) : p_src;
    p_dst = is_undefined(p_dst) ? edge.c_dst.get_x(edge.p_src) : p_dst;

    return (is_defined(p_src) and is_defined(p_dst))
             ? connection_add(compo, edge.src, p_src, edge.dst, p_dst)
             : connection_add_result::noexist;
}

static connection_add_result named_suffix_connection_add(
  graph_component&        compo,
  const get_edges_result& edge) noexcept
{
    if (edge.p_src.empty() and edge.p_dst.empty()) {
        edge.c_src.y.for_each<port_str>([&](const auto  sid,
                                            const auto& sname) noexcept {
            for (const auto did : edge.c_dst.x) {
                const auto dname = edge.c_dst.x.get<port_str>(did).sv();
                const auto dual  = split(dname, '_');
                if (dual.first == sname.sv()) {
                    if (exists_connection(compo, edge.src, sid, edge.dst, did))
                        continue;

                    if (connection_add(compo, edge.src, sid, edge.dst, did) ==
                        connection_add_result::nomem)
                        return;
                }
            }
        });

        return connection_add_result::done;
    }

    // At least one port is defined, trying to connect input and output port
    // with the same name. If the two ports are defined, force the
    // connection without searching input or output port with the sane name.

    auto p_src = edge.c_src.get_y(edge.p_src);
    auto p_dst = edge.c_dst.get_x(edge.p_dst);

    p_src = is_undefined(p_src) ? edge.c_src.get_y(edge.p_dst) : p_src;
    p_dst = is_undefined(p_dst) ? edge.c_dst.get_x(edge.p_src) : p_dst;

    return (is_defined(p_src) and is_defined(p_dst))
             ? connection_add(compo, edge.src, p_src, edge.dst, p_dst)
             : connection_add_result::noexist;
}

static status build_graph_connections(
  modeling&                             mod,
  graph_component&                      graph,
  const table<graph_node_id, child_id>& vertex) noexcept
{
    if (not graph.cache_connections.reserve(graph.g.edges.capacity()) and
        not graph.cache_connections.grow<2, 1>())
        return new_error(modeling_errc::graph_connection_container_full);

    switch (graph.type) {
    case graph_component::connection_type::in_out:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(mod, graph, vertex, idx);

            if (edges_opt.has_value()) {
                in_out_connection_add(graph, *edges_opt);
            }
        }
        break;

    case graph_component::connection_type::name:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(mod, graph, vertex, idx);

            if (edges_opt.has_value()) {
                named_connection_add(graph, *edges_opt);
            }
        }
        break;

    case graph_component::connection_type::name_suffix:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(mod, graph, vertex, idx);

            if (edges_opt.has_value()) {
                named_suffix_connection_add(graph, *edges_opt);
            }
        }
        break;
    }

    return success();
}

expected<void> graph_component::build_cache(modeling& mod) noexcept
{
    clear_cache();

    cache.reserve(g.nodes.size());
    if (not cache.can_alloc(g.nodes.size()) and
        not cache.can_alloc(g.nodes.size()))
        return new_error(modeling_errc::graph_children_container_full);

    const auto vec = build_graph_children(mod, *this);
    return build_graph_connections(mod, *this, vec);
}

void graph_component::clear_cache() noexcept
{
    cache.clear();
    cache_connections.clear();
}

status modeling::copy(graph_component&   graph,
                      generic_component& generic) noexcept
{
    if (auto ret = graph.build_cache(*this); not ret.has_value())
        return new_error(modeling_errc::graph_children_container_full);

    if (not generic.children.can_alloc(graph.cache.size()))
        return new_error(modeling_errc::generic_children_container_full);

    if (not generic.connections.can_alloc(graph.cache_connections.size()))
        return new_error(modeling_errc::generic_connection_container_full);

    table<child_id, child_id> graph_to_generic;
    graph_to_generic.data.reserve(graph.cache.size());

    for (const auto& src : graph.cache) {
        const auto src_id = graph.cache.get_id(src);
        auto&      dst    = generic.children.alloc(src.compo_id);
        const auto dst_id = generic.children.get_id(dst);

        graph_to_generic.data.emplace_back(src_id, dst_id);
    }

    graph_to_generic.sort();

    for (const auto& src : graph.cache_connections) {
        auto c_src = graph_to_generic.get(src.src);
        auto c_dst = graph_to_generic.get(src.dst);
        if (c_src and c_dst) {
            generic.connections.alloc(
              *c_src, src.index_src, *c_dst, src.index_dst);
        }
    }

    return success();
}

bool graph_component::exists_input_connection(const port_id       x,
                                              const graph_node_id v,
                                              const port_id id) const noexcept
{
    for (const auto& con : input_connections)
        if (con.id == id and con.x == x and con.v == v)
            return true;

    return false;
}

bool graph_component::exists_output_connection(const port_id       y,
                                               const graph_node_id v,
                                               const port_id id) const noexcept
{
    for (const auto& con : output_connections)
        if (con.id == id and con.y == y and con.v == v)
            return true;

    return false;
}

expected<input_connection_id> graph_component::connect_input(
  const port_id       x,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_input_connection(x, v, id))
        return new_error(modeling_errc::graph_input_connection_already_exists);

    if (not input_connections.can_alloc(1))
        if (not input_connections.grow<2, 1>())
            return new_error(
              modeling_errc::graph_input_connection_container_full);

    return input_connections.get_id(input_connections.alloc(x, v, id));
}

expected<output_connection_id> graph_component::connect_output(
  const port_id       y,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_output_connection(y, v, id))
        return new_error(modeling_errc::graph_output_connection_already_exists);

    if (not output_connections.can_alloc(1))
        if (not output_connections.grow<2, 1>())
            return new_error(
              modeling_errc::graph_output_connection_container_full);

    return output_connections.get_id(output_connections.alloc(y, v, id));
}

} // namespace irt
