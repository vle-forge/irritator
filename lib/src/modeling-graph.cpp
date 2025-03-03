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
#include <random>

#ifndef R123_USE_CXX11
#define R123_USE_CXX11 1
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <Random123/philox.h>
#include <Random123/uniform.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace irt {

struct local_rng {
    using rng          = r123::Philox4x64;
    using counter_type = r123::Philox4x64::ctr_type;
    using key_type     = r123::Philox4x64::key_type;
    using result_type  = counter_type::value_type;

    static_assert(counter_type::static_size == 4);
    static_assert(key_type::static_size == 2);

    result_type operator()() noexcept
    {
        if (last_elem == 0) {
            c.incr();

            rng b{};
            rdata = b(c, k);

            n++;
            last_elem = rdata.size();
        }

        return rdata[--last_elem];
    }

    local_rng(std::span<const u64> c0, std::span<const u64> uk) noexcept
      : n(0)
      , last_elem(0)
    {
        std::copy_n(c0.data(), c0.size(), c.data());
        std::copy_n(uk.data(), uk.size(), k.data());
    }

    constexpr static result_type min R123_NO_MACRO_SUBST() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    constexpr static result_type max R123_NO_MACRO_SUBST() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

    counter_type c;
    key_type     k;
    counter_type rdata{ 0u, 0u, 0u, 0u };
    u64          n;
    sz           last_elem;
};

bool graph_component::exists_child(const std::string_view name) const noexcept
{
    for (const auto id : nodes)
        if (node_names[get_index(id)] == name)
            return true;

    return false;
}

name_str graph_component::make_unique_name_id(
  const graph_node_id v) const noexcept
{
    debug::ensure(nodes.exists(v));

    name_str ret;

    if (g_type == graph_type::dot_file) {
        format(ret, "{}", node_names[get_index(v)]);
    } else {
        format(ret, "{}", get_index(v));
    }

    return ret;
}

static auto build_graph_children(modeling& mod, graph_component& graph) noexcept
  -> table<graph_node_id, child_id>
{
    graph.positions.resize(graph.nodes.size());
    table<graph_node_id, child_id> tr;
    tr.data.reserve(graph.nodes.ssize());

    const auto sq = std::sqrt(static_cast<float>(graph.nodes.size()));
    const auto gr = static_cast<i32>(sq);

    i32 x = 0;
    i32 y = 0;

    for (const auto node_id : graph.nodes) {
        child_id   new_id   = undefined<child_id>();
        const auto compo_id = graph.node_components[get_index(node_id)];

        if (auto* c = mod.components.try_to_get(compo_id); c) {
            auto& new_ch   = graph.cache.alloc(compo_id);
            new_id         = graph.cache.get_id(new_ch);
            const auto idx = get_index(new_id);

            graph.positions[idx].x =
              static_cast<float>(((graph.space_x * x) + graph.left_limit));
            graph.positions[idx].y =
              static_cast<float>(((graph.space_y * y) + graph.upper_limit));
        }

        if (x++ > gr) {
            x = 0;
            y++;
        }

        tr.data.emplace_back(node_id, new_id);
    }

    graph.cache_names.resize(tr.size());

    if (graph.g_type == graph_component::graph_type::dot_file) {
        for (const auto x : tr.data) {
            graph.cache_names[get_index(x.value)] =
              graph.node_names[get_index(x.id)];
        }
    } else {
        for (const auto x : tr.data) {
            graph.cache_names[get_index(x.value)] =
              graph.make_unique_name_id(x.id);
        }
    }

    tr.sort();

    return tr;
}

static void in_out_connection_add(modeling&        mod,
                                  graph_component& compo,
                                  child_id         src_id,
                                  child_id         dst_id) noexcept
{
    auto* src = compo.cache.try_to_get(src_id);
    auto* dst = compo.cache.try_to_get(dst_id);

    if (src and dst) {
        if (src->type == child_type::component) {
            auto* c_src = mod.components.try_to_get(src->id.compo_id);
            if (c_src) {
                const auto p_src = c_src->get_y("out");

                if (dst->type == child_type::component) {
                    auto* c_dst = mod.components.try_to_get(dst->id.compo_id);
                    if (c_dst) {
                        const auto p_dst = c_dst->get_x("in");

                        if (is_defined(p_src) and is_defined(p_dst)) {
                            compo.cache_connections.alloc(
                              src_id, p_src, dst_id, p_dst);
                        }
                    }
                }
            }
        }
    }
}

static void named_connection_add(modeling&        mod,
                                 graph_component& compo,
                                 child_id         src_id,
                                 child_id         dst_id) noexcept
{
    auto* src = compo.cache.try_to_get(src_id);
    auto* dst = compo.cache.try_to_get(dst_id);

    if (src and dst) {
        if (src->type == child_type::component) {
            auto* c_src = mod.components.try_to_get(src->id.compo_id);
            if (c_src) {
                if (dst->type == child_type::component) {
                    auto* c_dst = mod.components.try_to_get(dst->id.compo_id);
                    if (c_dst) {
                        auto     sz_src = c_src->x.ssize();
                        auto     sz_dst = c_dst->y.ssize();
                        port_str temp;

                        format(temp, "{}", sz_src);
                        const auto p_src = c_src->get_x(temp.sv());

                        format(temp, "{}", sz_dst);
                        const auto p_dst = c_dst->get_y(temp.sv());

                        if (is_defined(p_src) and is_defined(p_dst))
                            compo.cache_connections.alloc(
                              src_id, p_src, dst_id, p_dst);
                    }
                }
            }
        }
    }
}

static std::optional<std::filesystem::path> build_dot_filename(
  const modeling&    mod,
  const file_path_id id) noexcept
{
    try {
        if (auto* f = mod.file_paths.try_to_get(id); f) {
            if (auto* d = mod.dir_paths.try_to_get(f->parent); d) {
                if (auto* r = mod.registred_paths.try_to_get(d->parent); r) {
                    return std::filesystem::path(r->path.sv()) / d->path.sv() /
                           f->path.sv();
                } else
                    debug_log("registred_path not found");
            } else
                debug_log("dir_path not found");
        } else
            debug_log("file_path not found");
    } catch (...) {
        debug_log("not enough memory");
    }

    return std::nullopt;
}

static void build_dot_file_edges(
  const modeling&                        mod,
  graph_component&                       graph,
  const graph_component::dot_file_param& params) noexcept
{
    if (auto file_opt = build_dot_filename(mod, params.file);
        file_opt.has_value()) {
        if (auto dot_graph = parse_dot_file(mod, *file_opt);
            dot_graph.has_value()) {

            graph.nodes = std::move(dot_graph->nodes);
            graph.edges = std::move(dot_graph->edges);

            graph.node_names     = std::move(dot_graph->node_names);
            graph.node_ids       = std::move(dot_graph->node_ids);
            graph.node_positions = std::move(dot_graph->node_positions);
            graph.node_areas     = std::move(dot_graph->node_areas);
            graph.edges_nodes    = std::move(dot_graph->edges_nodes);

            graph.buffer = std::move(dot_graph->buffer);
        } else
            debug_log("parse_dot_file error");
    } else
        debug_log("file_dot_file error");
}

static void build_scale_free_edges(
  graph_component&                         graph,
  const graph_component::scale_free_param& params) noexcept
{
    graph.resize(params.nodes, params.id);

    if (const unsigned n = graph.nodes.max_used(); n > 1) {
        local_rng r(std::span<const u64>(graph.seed),
                    std::span<const u64>(graph.key));
        std::uniform_int_distribution<unsigned> d(0u, n - 1);

        auto first = graph.nodes.begin();
        bool stop  = false;

        while (not stop) {
            unsigned xv = d(r);
            unsigned degree =
              (xv == 0 ? 0
                       : unsigned(params.beta * std::pow(xv, -params.alpha)));

            while (degree == 0) {
                ++first;
                if (first == graph.nodes.end()) {
                    stop = true;
                    break;
                }

                xv = d(r);
                degree =
                  (xv == 0
                     ? 0
                     : unsigned(params.beta * std::pow(xv, -params.alpha)));
            }

            if (stop)
                break;

            auto second = undefined<graph_node_id>();
            do {
                const auto idx = d(r);
                second         = graph.nodes.get_from_index(idx);
            } while (not is_defined(second) or *first == second);
            --degree;

            if (not graph.edges.can_alloc(1)) {
                graph.edges.reserve(graph.edges.capacity() * 2);
                graph.edges_nodes.resize(graph.edges.capacity());

                if (not graph.edges.can_alloc(1))
                    return;
            }

            auto       new_edge_id  = graph.edges.alloc();
            const auto new_edge_idx = get_index(new_edge_id);

            graph.edges_nodes[new_edge_idx] = { *first, second };
        }
    }
}

static void build_small_world_edges(
  graph_component&                          graph,
  const graph_component::small_world_param& params) noexcept
{
    graph.resize(params.nodes, params.id);

    if (const auto n = graph.nodes.ssize(); n > 1) {
        local_rng r(std::span<const u64>(graph.seed),
                    std::span<const u64>(graph.key));

        std::uniform_real_distribution<>   dr(0.0, 1.0);
        std::uniform_int_distribution<int> di(0, n - 1);

        int first  = 0;
        int second = 1;
        int source = 0;
        int target = 1;

        do {
            target = (target + 1) % n;
            if (target == (source + params.k / 2 + 1) % n) {
                ++source;
                target = (source + 1) % n;
            }
            first = source;

            double x = dr(r);
            if (x < params.probability) {
                auto lower = (source + n - params.k / 2) % n;
                auto upper = (source + params.k / 2) % n;
                do {
                    second = di(r);
                } while (
                  (second >= lower && second <= upper) ||
                  (upper < lower && (second >= lower || second <= upper)));
            } else {
                second = target;
            }

            debug::ensure(first >= 0 && first < n);
            debug::ensure(second >= 0 && second < n);

            auto vertex_first = graph.nodes.begin();
            for (int i = 0; i <= first; ++i)
                ++vertex_first;

            auto vertex_second = graph.nodes.begin();
            for (int i = 0; i <= second; ++i)
                ++vertex_second;

            if (not graph.edges.can_alloc(1)) {
                graph.edges.reserve(graph.edges.capacity() * 2);
                graph.edges_nodes.resize(graph.edges.capacity());

                if (not graph.edges.can_alloc(1))
                    return;
            }

            const auto new_edge_id  = graph.edges.alloc();
            const auto new_edge_idx = get_index(new_edge_id);

            graph.edges_nodes[new_edge_idx] = { *vertex_first, *vertex_second };
        } while (source + 1 < n);
    }
}

graph_component::graph_component(const graph_component& other) noexcept
  : nodes{ other.nodes }
  , edges{ other.edges }
  , node_ids{ other.node_ids }
  , node_positions{ other.node_positions }
  , node_areas{ other.node_areas }
  , node_components{ other.node_components }
  , edges_nodes{ other.edges_nodes }
{
    node_names.resize(other.node_names.capacity());

    for (const auto id : other.nodes) {
        const auto idx  = get_index(id);
        node_names[idx] = buffer.append(other.node_names[idx]);
    }
}

void graph_component::update(const modeling& mod) noexcept
{
    switch (g_type) {
    case graph_type::dot_file:
        build_dot_file_edges(mod, *this, param.dot);
        return;
    case graph_type::scale_free:
        build_scale_free_edges(*this, param.scale);
        return;
    case graph_type::small_world:
        build_small_world_edges(*this, param.small);
        return;
    };

    irt::unreachable();
}

void graph_component::resize(const i32          children_size,
                             const component_id cid) noexcept
{
    nodes.clear();
    edges.clear();
    nodes.reserve(children_size);
    edges.reserve(children_size);
    input_connections.clear();
    output_connections.clear();

    node_names.resize(nodes.capacity());
    node_ids.resize(nodes.capacity());
    node_positions.resize(nodes.capacity());
    node_areas.resize(nodes.capacity());
    node_components.resize(nodes.capacity());

    edges_nodes.resize(edges.capacity());

    for (auto i = 0; i < children_size; ++i) {
        const auto id        = nodes.alloc();
        const auto idx       = get_index(id);
        node_components[idx] = cid;
    }
}

static void build_graph_connections(
  modeling&                             mod,
  graph_component&                      graph,
  const table<graph_node_id, child_id>& vertex) noexcept
{
    for (const auto id : graph.edges) {
        const auto idx  = get_index(id);
        const auto u_id = graph.edges_nodes[idx][0];
        const auto v_id = graph.edges_nodes[idx][1];

        if (graph.nodes.exists(u_id) and graph.nodes.exists(v_id)) {
            const auto u = vertex.get(u_id);
            const auto v = vertex.get(v_id);

            if (u and v) {
                if (graph.type == graph_component::connection_type::name) {
                    named_connection_add(mod, graph, *u, *v);
                } else {
                    in_out_connection_add(mod, graph, *u, *v);
                }
            }
        }
    }
}

status graph_component::build_cache(modeling& mod) noexcept
{
    clear_cache();

    cache.reserve(nodes.size());
    if (not cache.can_alloc(nodes.size()))
        return new_error(
          graph_component::children_error{},
          e_memory{ nodes.size(), static_cast<unsigned>(nodes.capacity()) });

    const auto vec = build_graph_children(mod, *this);
    build_graph_connections(mod, *this, vec);

    return success();
}

void graph_component::clear_cache() noexcept
{
    cache.clear();
    cache_connections.clear();
    positions.clear();
}

status modeling::copy(graph_component&   graph,
                      generic_component& generic) noexcept
{
    irt_check(graph.build_cache(*this));

    if (not generic.children.can_alloc(graph.cache.size()))
        return new_error(modeling::children_error{}, container_full_error{});

    if (not generic.connections.can_alloc(graph.cache_connections.size()))
        return new_error(modeling::connection_error{}, container_full_error{});

    table<child_id, child_id> graph_to_generic;
    graph_to_generic.data.reserve(graph.cache.size());

    for (const auto& src : graph.cache) {
        const auto src_id = graph.cache.get_id(src);

        if (src.type == child_type::model) {
            auto&      dst    = generic.children.alloc(src.id.mdl_type);
            const auto dst_id = generic.children.get_id(dst);

            graph_to_generic.data.emplace_back(src_id, dst_id);
        } else {
            auto&      dst    = generic.children.alloc(src.id.compo_id);
            const auto dst_id = generic.children.get_id(dst);

            graph_to_generic.data.emplace_back(src_id, dst_id);
        }
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

result<input_connection_id> graph_component::connect_input(
  const port_id       x,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_input_connection(x, v, id))
        return new_error(input_connection_error{}, already_exist_error{});

    if (not input_connections.can_alloc(1))
        return new_error(input_connection_error{}, container_full_error{});

    return input_connections.get_id(input_connections.alloc(x, v, id));
}

result<output_connection_id> graph_component::connect_output(
  const port_id       y,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_output_connection(y, v, id))
        return new_error(input_connection_error{}, already_exist_error{});

    if (not output_connections.can_alloc(1))
        return new_error(input_connection_error{}, container_full_error{});

    return output_connections.get_id(output_connections.alloc(y, v, id));
}

void graph_component::format_input_connection_error(log_entry& e) noexcept
{
    e.buffer = "Input connection already exists in this graph component";
    e.level  = log_level::notice;
}

void graph_component::format_input_connection_full_error(log_entry& e) noexcept
{
    e.buffer = "Input connection list is full in this graph component";
    e.level  = log_level::error;
}

void graph_component::format_output_connection_error(log_entry& e) noexcept
{
    e.buffer = "Input connection already exists in this graph component";
    e.level  = log_level::notice;
}

void graph_component::format_output_connection_full_error(log_entry& e) noexcept
{
    e.buffer = "Output connection list is full in this graph component";
    e.level  = log_level::error;
}

void graph_component::format_children_error(log_entry& e, e_memory mem) noexcept
{
    format(e.buffer,
           "Not enough available space for model "
           "in this grid component({}, {}) ",
           mem.request,
           mem.capacity);

    e.level = log_level::error;
}

} // namespace irt
