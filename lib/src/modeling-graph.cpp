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
    for (const auto id : g.nodes)
        if (g.node_names[get_index(id)] == name)
            return true;

    return false;
}

name_str graph_component::make_unique_name_id(
  const graph_node_id v) const noexcept
{
    debug::ensure(g.nodes.exists(v));

    name_str ret;

    if (g_type == graph_type::dot_file) {
        format(ret, "{}", g.node_names[get_index(v)]);
    } else {
        format(ret, "{}", get_index(v));
    }

    return ret;
}

static auto build_graph_children(modeling& mod, graph_component& graph) noexcept
  -> table<graph_node_id, child_id>
{
    table<graph_node_id, child_id> tr;
    tr.data.reserve(graph.g.nodes.ssize());

    for (const auto node_id : graph.g.nodes) {
        child_id   new_id   = undefined<child_id>();
        const auto compo_id = graph.g.node_components[get_index(node_id)];

        if (auto* c = mod.components.try_to_get(compo_id); c) {
            auto& new_ch = graph.cache.alloc(compo_id);
            new_id       = graph.cache.get_id(new_ch);
        }

        tr.data.emplace_back(node_id, new_id);
    }

    graph.cache_names.resize(tr.size());

    if (graph.g_type == graph_component::graph_type::dot_file) {
        for (const auto x : tr.data) {
            graph.cache_names[get_index(x.value)] =
              graph.g.node_names[get_index(x.id)];
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

static void in_out_connection_add(graph_component& compo,
                                  const child_id   src_id,
                                  const child_id   dst_id,
                                  const component& src,
                                  const component& dst) noexcept
{
    if (not compo.cache_connections.can_alloc(1)) {
        compo.cache_connections.grow();
        if (not compo.cache_connections.can_alloc(1))
            return;
    }

    if (const auto p_src = src.get_y("out"); is_defined(p_src)) {
        if (const auto p_dst = dst.get_x("in"); is_defined(p_dst)) {
            compo.cache_connections.alloc(src_id, p_src, dst_id, p_dst);
        }
    }
}

static void named_connection_add(graph_component& compo,
                                 const child_id   src_id,
                                 const child_id   dst_id,
                                 const component& src,
                                 const component& dst) noexcept
{
    if (not compo.cache_connections.can_alloc(1)) {
        compo.cache_connections.grow();
        if (not compo.cache_connections.can_alloc(1))
            return;
    }
    src.y.for_each<port_str>([&](const auto sid, const auto& sname) noexcept {
        dst.x.for_each<port_str>(
          [&](const auto did, const auto& dname) noexcept {
              if (sname == dname)
                  compo.cache_connections.alloc(src_id, sid, dst_id, did);
          });
    });
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

static void named_suffix_connection_add(graph_component& compo,
                                        const child_id   src_id,
                                        const child_id   dst_id,
                                        const component& src,
                                        const component& dst) noexcept
{
    if (not compo.cache_connections.can_alloc(1)) {
        compo.cache_connections.grow();
        if (not compo.cache_connections.can_alloc(1))
            return;
    }

    src.y.for_each<port_str>([&](const auto sid, const auto& sname) noexcept {
        for (const auto did : dst.x) {
            const auto dname = dst.x.get<port_str>(did).sv();
            const auto dual  = split(dname, '_');
            if (dual.first == sname.sv()) {
                if (exists_connection(compo, src_id, sid, dst_id, did))
                    continue;

                compo.cache_connections.alloc(src_id, sid, dst_id, did);
                return;
            }
        }
    });
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

static expected<void> build_dot_file_edges(
  const modeling&                        mod,
  graph_component&                       graph,
  const graph_component::dot_file_param& params) noexcept
{
    if (auto file_opt = build_dot_filename(mod, params.file);
        file_opt.has_value()) {
        if (auto dot_graph = parse_dot_file(mod, *file_opt);
            dot_graph.has_value()) {

            graph.g.nodes = std::move(dot_graph->nodes);
            graph.g.edges = std::move(dot_graph->edges);

            graph.g.node_names      = std::move(dot_graph->node_names);
            graph.g.node_ids        = std::move(dot_graph->node_ids);
            graph.g.node_positions  = std::move(dot_graph->node_positions);
            graph.g.node_components = std::move(dot_graph->node_components);
            graph.g.node_areas      = std::move(dot_graph->node_areas);
            graph.g.edges_nodes     = std::move(dot_graph->edges_nodes);

            graph.g.buffer = std::move(dot_graph->buffer);
        } else
            return new_error_code(graph_component::errc::dot_file_access_error,
                                  category::graph_component);
    } else
        return new_error_code(graph_component::errc::dot_file_format_error,
                              category::graph_component);

    return expected<void>();
}

static constexpr auto edge_exists(const graph&        g,
                                  const graph_node_id src,
                                  const graph_node_id dst) noexcept -> bool
{
    for (auto id : g.edges) {
        const auto idx = get_index(id);

        if (g.edges_nodes[idx][0] == src and g.edges_nodes[idx][1] == dst)
            return true;
    }

    return false;
}

static expected<void> build_scale_free_edges(
  graph_component&                         graph,
  const graph_component::scale_free_param& params) noexcept
{
    graph.resize(params.nodes, params.id);

    if (const unsigned n = graph.g.nodes.max_used(); n > 1) {
        local_rng r(std::span<const u64>(graph.seed),
                    std::span<const u64>(graph.key));
        std::uniform_int_distribution<unsigned> d(0u, n - 1);

        auto first = graph.g.nodes.begin();
        bool stop  = false;

        while (not stop) {
            unsigned xv = d(r);
            unsigned degree =
              (xv == 0 ? 0
                       : unsigned(params.beta * std::pow(xv, -params.alpha)));

            while (degree == 0) {
                ++first;
                if (first == graph.g.nodes.end()) {
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
                second         = graph.g.nodes.get_from_index(idx);
            } while (not is_defined(second) or *first == second or
                     edge_exists(graph.g, *first, second));
            --degree;

            if (not graph.g.edges.can_alloc(1)) {
                graph.g.edges.grow<3, 2>();
                graph.g.edges_nodes.resize(graph.g.edges.capacity());

                if (not graph.g.edges.can_alloc(1))
                    return new_error_code(
                      graph_component::errc::edges_container_full,
                      category::graph_component);
            }

            auto       new_edge_id  = graph.g.edges.alloc();
            const auto new_edge_idx = get_index(new_edge_id);

            graph.g.edges_nodes[new_edge_idx] = { *first, second };
        }
    }

    return expected<void>();
}

static expected<void> build_small_world_edges(
  graph_component&                          graph,
  const graph_component::small_world_param& params) noexcept
{
    graph.resize(params.nodes, params.id);

    if (const auto n = graph.g.nodes.ssize(); n > 1) {
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

            auto vertex_first = graph.g.nodes.begin();
            for (int i = 0; i <= first; ++i)
                ++vertex_first;

            auto vertex_second = graph.g.nodes.begin();
            for (int i = 0; i <= second; ++i)
                ++vertex_second;

            if (not graph.g.edges.can_alloc(1)) {
                graph.g.edges.grow<3, 2>();
                graph.g.edges_nodes.resize(graph.g.edges.capacity());

                if (not graph.g.edges.can_alloc(1))
                    return new_error_code(
                      graph_component::errc::edges_container_full,
                      category::graph_component);
            }

            const auto new_edge_id  = graph.g.edges.alloc();
            const auto new_edge_idx = get_index(new_edge_id);

            if (not is_defined(*vertex_first) or
                not is_defined(*vertex_second) or
                vertex_first == vertex_second or
                edge_exists(graph.g, *vertex_first, *vertex_second))
                continue;

            graph.g.edges_nodes[new_edge_idx] = { *vertex_first,
                                                  *vertex_second };
        } while (source + 1 < n);
    }

    return expected<void>();
}

expected<void> graph_component::update(const modeling& mod) noexcept
{
    switch (g_type) {
    case graph_type::dot_file:
        if (auto ret = build_dot_file_edges(mod, *this, param.dot); not ret)
            return ret.error();
        break;
    case graph_type::scale_free:
        if (auto ret = build_scale_free_edges(*this, param.scale); not ret)
            return ret.error();
        break;
    case graph_type::small_world:
        if (auto ret = build_small_world_edges(*this, param.small); not ret)
            return ret.error();
        break;
    };

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

    return expected<void>();
}

void graph_component::resize(const i32          children_size,
                             const component_id cid) noexcept
{
    g.clear();
    g.reserve(children_size, children_size * 8);

    input_connections.clear();
    output_connections.clear();

    for (auto i = 0; i < children_size; ++i) {
        const auto id          = g.nodes.alloc();
        const auto idx         = get_index(id);
        g.node_components[idx] = cid;
    }
}

static void build_graph_connections(
  modeling&                             mod,
  graph_component&                      graph,
  const table<graph_node_id, child_id>& vertex) noexcept
{
    for (const auto id : graph.g.edges) {
        const auto idx  = get_index(id);
        const auto u_id = graph.g.edges_nodes[idx][0];
        const auto v_id = graph.g.edges_nodes[idx][1];

        if (graph.g.nodes.exists(u_id) and graph.g.nodes.exists(v_id)) {
            if (const auto u = vertex.get(u_id)) {
                if (const auto v = vertex.get(v_id)) {
                    if (auto* src = graph.cache.try_to_get(*u);
                        src and src->type == child_type::component) {
                        if (auto* dst = graph.cache.try_to_get(*u);
                            dst and dst->type == child_type::component) {

                            const auto* c_src =
                              mod.components.try_to_get(src->id.compo_id);
                            const auto* c_dst =
                              mod.components.try_to_get(dst->id.compo_id);

                            switch (graph.type) {
                            case graph_component::connection_type::in_out:
                                graph.cache_connections.reserve(
                                  graph.g.edges.size());
                                in_out_connection_add(
                                  graph, *u, *v, *c_src, *c_dst);
                                break;
                            case graph_component::connection_type::name:
                                graph.cache_connections.reserve(
                                  graph.g.edges.size() * 4);
                                named_connection_add(
                                  graph, *u, *v, *c_src, *c_dst);
                                break;
                            case graph_component::connection_type::name_suffix:
                                graph.cache_connections.reserve(
                                  graph.g.edges.size() * 4);
                                named_suffix_connection_add(
                                  graph, *u, *v, *c_src, *c_dst);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

expected<void> graph_component::build_cache(modeling& mod) noexcept
{
    clear_cache();

    cache.reserve(g.nodes.size());
    if (not cache.can_alloc(g.nodes.size()))
        return new_error_code(graph_component::errc::nodes_container_full,
                              category::graph_component);

    const auto vec = build_graph_children(mod, *this);
    build_graph_connections(mod, *this, vec);

    return expected<void>{};
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
        return new_error(graph_component_errc::children_container_full);

    if (not generic.children.can_alloc(graph.cache.size()))
        return new_error(generic_component_errc::children_container_full);

    if (not generic.connections.can_alloc(graph.cache_connections.size()))
        return new_error(generic_component_errc::connection_container_full);

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

expected<input_connection_id> graph_component::connect_input(
  const port_id       x,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_input_connection(x, v, id))
        return new_error_code(errc::input_connection_already_exists,
                              category::graph_component);

    if (not input_connections.can_alloc(1))
        return new_error_code(errc::input_connection_full,
                              category::graph_component);

    return input_connections.get_id(input_connections.alloc(x, v, id));
}

expected<output_connection_id> graph_component::connect_output(
  const port_id       y,
  const graph_node_id v,
  const port_id       id) noexcept
{
    if (exists_output_connection(y, v, id))
        return new_error_code(errc::output_connection_already_exists,
                              category::graph_component);

    if (not output_connections.can_alloc(1))
        return new_error_code(errc::output_connection_full,
                              category::graph_component);

    return output_connections.get_id(output_connections.alloc(y, v, id));
}

} // namespace irt
