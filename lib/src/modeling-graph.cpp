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

void graph_component::swap(graph_component& other) noexcept
{
    g.swap(other.g);
    input_connections.swap(other.input_connections);
    output_connections.swap(other.output_connections);
    top_left_limit.swap(other.top_left_limit);
    bottom_right_limit.swap(other.bottom_right_limit);

    std::swap(dot, other.dot);
    std::swap(scale, other.scale);
    std::swap(small, other.small);
    std::swap(g_type, other.g_type);

    std::swap(rng, other.rng);
    std::swap(type, other.type);
}

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

static auto build_graph_children(
  const component_access&                       ids,
  const graph_component&                        graph,
  data_array<graph_component::child, child_id>& cache,
  vector<name_str>& cache_names) noexcept -> table<graph_node_id, child_id>
{
    table<graph_node_id, child_id> tr;
    tr.data.reserve(graph.g.nodes.ssize());

    for (const auto node_id : graph.g.nodes) {
        const auto compo_id = graph.g.node_components[node_id];

        if (ids.exists(compo_id)) {
            auto& ch = cache.alloc(compo_id, node_id);
            auto  id = cache.get_id(ch);
            tr.data.emplace_back(node_id, id);
        } else {
            tr.data.emplace_back(node_id, undefined<child_id>());
        }
    }

    tr.sort();
    cache_names.resize(tr.size());

    for (const auto x : tr.data) {
        cache_names[x.value] = graph.make_unique_name_id(x.id);
    }

    return tr;
}

struct get_edges_result {
    child_id         src;
    child_id         dst;
    component_id     c_src;
    component_id     c_dst;
    std::string_view p_src;
    std::string_view p_dst;
};

static auto get_edges(
  const component_access&                             ids,
  const graph_component&                              graph,
  const table<graph_node_id, child_id>&               vertex,
  const u16                                           index,
  const data_array<graph_component::child, child_id>& cache) noexcept
  -> std::optional<get_edges_result>
{
    const auto u_id = graph.g.edges_nodes[index][0].first;
    const auto v_id = graph.g.edges_nodes[index][1].first;

    if (graph.g.nodes.exists(u_id) and graph.g.nodes.exists(v_id)) {
        const auto u = vertex.get(u_id);
        const auto v = vertex.get(v_id);

        if (auto* src = cache.try_to_get(*u)) {
            if (auto* dst = cache.try_to_get(*v)) {
                if (ids.exists(src->compo_id) and ids.exists(dst->compo_id))
                    return get_edges_result{
                        .src   = *u,
                        .dst   = *v,
                        .c_src = src->compo_id,
                        .c_dst = dst->compo_id,
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

static constexpr auto exists_connection(
  const data_array<connection, connection_id>& cache_connections,
  const child_id                               src_id,
  const port_id                                p_src,
  const child_id                               dst_id,
  const port_id                                p_dst) noexcept -> bool
{
    for (const auto& elem : cache_connections)
        if (elem.src == src_id and elem.dst == dst_id and
            elem.index_src.compo == p_src and elem.index_dst.compo == p_dst)
            return true;

    return false;
}

enum connection_add_result : u8 { done, nomem, noexist };

static connection_add_result connection_add(
  const child_id                         src,
  const port_id                          p_src,
  const child_id                         dst,
  const port_id                          p_dst,
  data_array<connection, connection_id>& cache_connections) noexcept
{
    if (not cache_connections.can_alloc(1) and
        not cache_connections.grow<3, 2>())
        return connection_add_result::nomem;

    cache_connections.alloc(src, p_src, dst, p_dst);
    return connection_add_result::done;
}

static connection_add_result in_out_connection_add(
  const component_access&                ids,
  const get_edges_result&                edge,
  data_array<connection, connection_id>& cache_connections) noexcept
{
    const auto  oport = edge.p_src.empty() ? "out" : edge.p_src;
    const auto  iport = edge.p_dst.empty() ? "in" : edge.p_dst;
    const auto& c_src = ids.components[edge.c_src];
    const auto& c_dst = ids.components[edge.c_dst];

    if (const auto p_src = c_src.get_y(oport); is_defined(p_src))
        if (const auto p_dst = c_dst.get_x(iport); is_defined(p_dst))
            return connection_add(
              edge.src, p_src, edge.dst, p_dst, cache_connections);

    return connection_add_result::noexist;
}

static connection_add_result named_connection_add(
  const component_access&                ids,
  const graph_component&                 compo,
  const get_edges_result&                edge,
  data_array<connection, connection_id>& cache_connections) noexcept
{
    const auto& c_src = ids.components[edge.c_src];
    const auto& c_dst = ids.components[edge.c_dst];

    // No input and output ports are defined. Trying to connect input and output
    // port with the same name.

    if (edge.p_src.empty() and edge.p_dst.empty()) {
        c_src.y.template for_each<port_str>(
          [&](const auto sid, const auto& sname) noexcept {
              c_dst.x.for_each<port_str>([&](const auto  did,
                                             const auto& dname) noexcept {
                  if (sname == dname) {
                      if (connection_add(
                            edge.src, sid, edge.dst, did, cache_connections) ==
                          connection_add_result::nomem)
                          return;
                  }
              });
          });

        if (not compo.g.flags[graph::option_flags::directed]) {
            c_dst.y.for_each<port_str>(
              [&](const auto sid, const auto& sname) noexcept {
                  c_src.x.for_each<port_str>(
                    [&](const auto did, const auto& dname) noexcept {
                        if (sname == dname) {
                            if (connection_add(edge.dst,
                                               sid,
                                               edge.src,
                                               did,
                                               cache_connections) ==
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

    auto p_src = c_src.get_y(edge.p_src);
    auto p_dst = c_dst.get_x(edge.p_dst);

    p_src = is_undefined(p_src) ? c_src.get_y(edge.p_dst) : p_src;
    p_dst = is_undefined(p_dst) ? c_dst.get_x(edge.p_src) : p_dst;

    return (is_defined(p_src) and is_defined(p_dst))
             ? connection_add(
                 edge.src, p_src, edge.dst, p_dst, cache_connections)
             : connection_add_result::noexist;
}

static connection_add_result named_suffix_connection_add(
  const component_access&                ids,
  const get_edges_result&                edge,
  data_array<connection, connection_id>& cache_connections) noexcept
{
    const auto& c_src = ids.components[edge.c_src];
    const auto& c_dst = ids.components[edge.c_dst];

    if (edge.p_src.empty() and edge.p_dst.empty()) {
        c_src.y.template for_each<port_str>(
          [&](const auto sid, const auto& sname) noexcept {
              for (const auto did : c_dst.x) {
                  const auto dname = c_dst.x.get<port_str>(did).sv();
                  const auto dual  = split(dname, '_');
                  if (dual.first == sname.sv()) {
                      if (exists_connection(
                            cache_connections, edge.src, sid, edge.dst, did))
                          continue;

                      if (connection_add(
                            edge.src, sid, edge.dst, did, cache_connections) ==
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

    auto p_src = c_src.get_y(edge.p_src);
    auto p_dst = c_dst.get_x(edge.p_dst);

    p_src = is_undefined(p_src) ? c_src.get_y(edge.p_dst) : p_src;
    p_dst = is_undefined(p_dst) ? c_dst.get_x(edge.p_src) : p_dst;

    return (is_defined(p_src) and is_defined(p_dst))
             ? connection_add(
                 edge.src, p_src, edge.dst, p_dst, cache_connections)
             : connection_add_result::noexist;
}

static status build_graph_connections(
  const component_access&                             ids,
  const graph_component&                              graph,
  const table<graph_node_id, child_id>&               vertex,
  const data_array<graph_component::child, child_id>& cache,
  data_array<connection, connection_id>& cache_connections) noexcept
{
    if (not cache_connections.reserve(graph.g.edges.capacity()) and
        not cache_connections.grow<2, 1>())
        return new_error(modeling_errc::graph_connection_container_full);

    switch (graph.type) {
    case graph_component::connection_type::in_out:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(ids, graph, vertex, idx, cache);

            if (edges_opt.has_value()) {
                in_out_connection_add(ids, *edges_opt, cache_connections);
            }
        }
        break;

    case graph_component::connection_type::name:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(ids, graph, vertex, idx, cache);

            if (edges_opt.has_value()) {
                named_connection_add(ids, graph, *edges_opt, cache_connections);
            }
        }
        break;

    case graph_component::connection_type::name_suffix:
        for (const auto id : graph.g.edges) {
            const auto idx       = get_index(id);
            const auto edges_opt = get_edges(ids, graph, vertex, idx, cache);

            if (edges_opt.has_value()) {
                named_suffix_connection_add(ids, *edges_opt, cache_connections);
            }
        }
        break;
    }

    return success();
}

expected<void> graph_component::build_cache(
  const component_access&                       ids,
  data_array<graph_component::child, child_id>& cache,
  data_array<connection, connection_id>&        cache_connections,
  vector<name_str>&                             cache_names) const noexcept
{
    cache.clear();
    cache_connections.clear();
    cache_names.clear();

    cache.reserve(g.nodes.size());
    if (not cache.can_alloc(g.nodes.size()))
        return new_error(modeling_errc::graph_children_container_full);

    const auto vec = build_graph_children(ids, *this, cache, cache_names);
    return build_graph_connections(ids, *this, vec, cache, cache_connections);
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
