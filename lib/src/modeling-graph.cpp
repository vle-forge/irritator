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

static auto build_graph_children(modeling& mod, graph_component& graph) noexcept
  -> table<graph_component::vertex_id, child_id>
{
    graph.positions.resize(graph.children.size());
    table<graph_component::vertex_id, child_id> tr;
    tr.data.reserve(graph.children.ssize());

    const auto sq = std::sqrt(static_cast<float>(graph.children.size()));
    const auto gr = static_cast<i32>(sq);

    i32 x = 0;
    i32 y = 0;

    for (const auto& vertex : graph.children) {
        child_id new_id = undefined<child_id>();

        if (auto* c = mod.components.try_to_get(vertex.id); c) {
            auto& new_ch     = graph.cache.alloc(vertex.id);
            new_id           = graph.cache.get_id(new_ch);
            new_ch.unique_id = static_cast<u64>(graph.children.get_id(vertex));

            graph.positions[get_index(new_id)].x =
              static_cast<float>(((graph.space_x * x) + graph.left_limit));
            graph.positions[get_index(new_id)].y =
              static_cast<float>(((graph.space_y * y) + graph.upper_limit));
        }

        if (x++ > gr) {
            x = 0;
            y++;
        }

        tr.data.emplace_back(graph.children.get_id(vertex), new_id);
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
                        auto     sz_src = c_src->x_names.ssize();
                        auto     sz_dst = c_dst->y_names.ssize();
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

status graph_component::build_cache(modeling& mod) noexcept
{
    clear_cache();

    cache.reserve(children.size());
    if (not cache.can_alloc(children.size()))
        return new_error(project::error::not_enough_memory);

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

bool graph_component::exists_input_connection(const port_id   x,
                                              const vertex_id v,
                                              const port_id   id) const noexcept
{
    for (const auto& con : input_connections)
        if (con.id == id and con.x == x and con.v == v)
            return true;

    return false;
}

bool graph_component::exists_output_connection(const port_id   y,
                                               const vertex_id v,
                                               const port_id id) const noexcept
{
    for (const auto& con : output_connections)
        if (con.id == id and con.y == y and con.v == v)
            return true;

    return false;
}

result<input_connection_id> graph_component::connect_input(
  const port_id   x,
  const vertex_id v,
  const port_id   id) noexcept
{
    if (exists_input_connection(x, v, id))
        return new_error(modeling::part::connections);

    if (not input_connections.can_alloc(1))
        return new_error(modeling::part::connections);

    return input_connections.get_id(input_connections.alloc(x, v, id));
}

result<output_connection_id> graph_component::connect_output(
  const port_id   y,
  const vertex_id v,
  const port_id   id) noexcept
{
    if (exists_output_connection(y, v, id))
        return new_error(modeling::part::connections);

    if (not output_connections.can_alloc(1))
        return new_error(modeling::part::connections);

    return output_connections.get_id(output_connections.alloc(y, v, id));
}

} // namespace irt
