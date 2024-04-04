// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "dot-parser.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <math.h>
#include <numeric>
#include <optional>
#include <random>
#include <utility>

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

#include <cstdint>

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

static auto build_graph_children(
  const data_array<component, component_id>& components,
  const data_array<graph_component::vertex, graph_component::vertex_id>&
                               vertices,
  data_array<child, child_id>& out,
  vector<child_position>&      positions,
  i32                          upper_limit,
  i32                          left_limit,
  i32                          space_x,
  i32 space_y) noexcept -> result<table<graph_component::vertex_id, child_id>>
{
    if (not out.can_alloc(vertices.size())) {
        out.reserve(vertices.size());
        if (not out.can_alloc(vertices.size()))
            return new_error(project::error::not_enough_memory);
    }

    positions.resize(vertices.size());

    table<graph_component::vertex_id, child_id> tr;

    tr.data.reserve(vertices.ssize());

    const auto sq = std::sqrt(static_cast<float>(vertices.size()));
    const auto gr = static_cast<i32>(sq);

    i32 x = 0;
    i32 y = 0;

    for (const auto& vertex : vertices) {
        child_id new_id = undefined<child_id>();

        if (auto* c = components.try_to_get(vertex.id); c) {
            auto& new_ch     = out.alloc(vertex.id);
            new_id           = out.get_id(new_ch);
            new_ch.unique_id = static_cast<u64>(vertices.get_id(vertex));

            positions[get_index(new_id)].x =
              static_cast<float>(((space_x * x) + left_limit));
            positions[get_index(new_id)].y =
              static_cast<float>(((space_y * y) + upper_limit));
        }

        if (x++ > gr) {
            x = 0;
            y++;
        }

        tr.data.emplace_back(vertices.get_id(vertex), new_id);
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
                const auto p_src = mod.get_y_index(*c_src, "out");

                if (dst->type == child_type::component) {
                    auto* c_dst = mod.components.try_to_get(dst->id.compo_id);
                    if (c_dst) {
                        const auto p_dst = mod.get_x_index(*c_dst, "in");

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
                auto p_src = mod.get_y_index(*c_src, "out");

                if (dst->type == child_type::component) {
                    auto* c_dst = mod.components.try_to_get(dst->id.compo_id);
                    if (c_dst) {
                        auto p_dst = mod.get_x_index(*c_dst, "in");

                        auto     sz_src = c_src->x_names.ssize();
                        auto     sz_dst = c_dst->y_names.ssize();
                        port_str temp;

                        format(temp, "{}", sz_src);
                        p_src = mod.get_x_index(*c_src, temp.sv());

                        format(temp, "{}", sz_dst);
                        p_dst = mod.get_y_index(*c_dst, temp.sv());

                        if (is_defined(p_src) and is_defined(p_dst))
                            compo.cache_connections.alloc(
                              src_id, p_src, dst_id, p_dst);
                    }
                }
            }
        }
    }
}

static void build_dot_file_edges(
  graph_component& graph,
  const graph_component::dot_file_param& /*params*/) noexcept
{
    if (auto ret = parse_dot_file(graph); not ret) {
        debug_log("parse_dot_file error");
    }
}

static void build_scale_free_edges(
  graph_component&                         graph,
  const graph_component::scale_free_param& params) noexcept
{
    using vertex = graph_component::vertex;

    graph.edges.clear();

    if (const unsigned n = graph.children.max_used(); n > 1) {
        local_rng r(std::span<const u64>(graph.seed),
                    std::span<const u64>(graph.key));
        std::uniform_int_distribution<unsigned> d(0u, n - 1);

        vertex* first  = nullptr;
        vertex* second = nullptr;
        bool    stop   = false;

        graph.children.next(first);

        while (not stop) {
            unsigned xv = d(r);
            unsigned degree =
              (xv == 0 ? 0
                       : unsigned(params.beta * std::pow(xv, -params.alpha)));

            while (degree == 0) {
                if (not graph.children.next(first)) {
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

            do {
                second = graph.children.try_to_get(d(r));
            } while (second == nullptr or first == second);
            --degree;

            if (not graph.edges.can_alloc()) {
                graph.edges.reserve(graph.edges.capacity() * 2);

                if (not graph.edges.can_alloc())
                    return;
            }

            graph.edges.alloc(graph.children.get_id(first),
                              graph.children.get_id(second));
        }
    }
}

static void build_small_world_edges(
  graph_component&                          graph,
  const graph_component::small_world_param& params) noexcept
{
    using vertex = graph_component::vertex;

    graph.edges.clear();

    if (const auto n = graph.children.ssize(); n > 1) {
        local_rng                          r(std::span<const u64>(graph.seed),
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

            vertex* vertex_first = nullptr;
            {
                int i = 0;
                do {
                    graph.children.next(vertex_first);
                    ++i;
                } while (i <= first);
            }

            vertex* vertex_second = nullptr;
            {
                int i = 0;
                do {
                    graph.children.next(vertex_second);
                    ++i;
                } while (i <= second);
            }

            if (not graph.edges.can_alloc()) {
                graph.edges.reserve(graph.edges.capacity() * 2);

                if (not graph.edges.can_alloc())
                    return;
            }

            graph.edges.alloc(graph.children.get_id(vertex_first),
                              graph.children.get_id(vertex_second));
        } while (source + 1 < n);
    }
}

graph_component::graph_component() noexcept
  : children{ 16 }
  , edges{ 32 }
{}

void graph_component::update() noexcept
{
    switch (param.index()) {
    case 0:
        build_dot_file_edges(
          *this, *std::get_if<graph_component::dot_file_param>(&param));
        return;
    case 1:
        build_scale_free_edges(
          *this, *std::get_if<graph_component::scale_free_param>(&param));
        return;
    case 2:
        build_small_world_edges(
          *this, *std::get_if<graph_component::small_world_param>(&param));
        return;
    };

    irt::unreachable();
}

void graph_component::resize(const i32          children_size,
                             const component_id id) noexcept
{
    children.clear();
    children.reserve(children_size);

    for (int i = 0; i < children_size; ++i)
        children.alloc(id);

    edges.clear();
    input_connections.clear();
    output_connections.clear();
}

static void build_graph_connections(
  modeling&                                          mod,
  graph_component&                                   graph,
  const table<graph_component::vertex_id, child_id>& vertex) noexcept
{
    for (const auto& edge : graph.edges) {
        const auto u = vertex.get(edge.u);
        const auto v = vertex.get(edge.v);

        if (v and u) {
            if (graph.type == graph_component::connection_type::name) {
                named_connection_add(mod, graph, *u, *v);
            } else {
                in_out_connection_add(mod, graph, *u, *v);
            }
        }
    }
}

status modeling::build_graph_children_and_connections(graph_component& graph,
                                                      i32 upper_limit,
                                                      i32 left_limit,
                                                      i32 space_x,
                                                      i32 space_y) noexcept
{
    graph.cache.clear();
    graph.cache_connections.clear();
    graph.positions.clear();

    auto tr = build_graph_children(components,
                                   graph.children,
                                   graph.cache,
                                   graph.positions,
                                   upper_limit,
                                   left_limit,
                                   space_x,
                                   space_y);
    if (not tr)
        return tr.error();

    build_graph_connections(*this, graph, *tr);

    return success();
}

status modeling::build_graph_component_cache(graph_component& graph) noexcept
{
    clear_graph_component_cache(graph);

    return build_graph_children_and_connections(graph);
}

void modeling::clear_graph_component_cache(graph_component& graph) noexcept
{
    graph.cache.clear();
    graph.cache_connections.clear();
}

status modeling::copy(graph_component&   graph,
                      generic_component& generic) noexcept
{
    irt_check(build_graph_children_and_connections(graph));

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
        switch (src.type) {
        case connection::connection_type::internal: {
            auto c_src = graph_to_generic.get(src.internal.src);
            auto c_dst = graph_to_generic.get(src.internal.dst);
            if (c_src and c_dst) {
                generic.connections.alloc(
                  connection::internal_t{ *c_src,
                                          *c_dst,
                                          src.internal.index_src,
                                          src.internal.index_dst });
            }
        } break;

        default:
            break;
        }
    }

    return success();
}

} // namespace irt
