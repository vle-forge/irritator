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

struct local_rng
{
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

            rng b;
            rdata = b(c, k);

            n++;
            last_elem = rdata.size();
        }

        return rdata[--last_elem];
    }

    local_rng(const std::span<u64>& c0, const std::span<u64>& uk) noexcept
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
    counter_type rdata;
    u64          n;
    sz           last_elem;
};

static auto build_graph_children(modeling&         mod,
                                 graph_component&  graph,
                                 vector<child_id>& ids,
                                 i32               upper_limit,
                                 i32               left_limit,
                                 i32               space_x,
                                 i32               space_y) noexcept -> status
{
    irt_return_if_fail(mod.children.can_alloc(graph.children.size()),
                       status::data_array_not_enough_memory);

    const auto old_size = ids.ssize();
    ids.reserve(graph.children.ssize() + old_size);

    const auto sq = std::sqrt(static_cast<float>(graph.children.size()));
    const auto gr = static_cast<i32>(sq);

    i32 x = 0;
    i32 y = 0;

    for (int i = 0, e = graph.children.ssize(); i != e; ++i) {
        child_id new_id = undefined<child_id>();

        if (mod.components.try_to_get(graph.children[i])) {
            auto& new_ch     = mod.children.alloc(graph.children[i]);
            new_id           = mod.children.get_id(new_ch);
            new_ch.unique_id = static_cast<u64>(i);

            mod.children_positions[get_index(new_id)] = child_position{
                .x = static_cast<float>(((space_x * x) + left_limit)),
                .y = static_cast<float>(((space_y * y) + upper_limit))
            };
        }

        if (x++ > gr) {
            x = 0;
            y++;
        }

        ids.emplace_back(new_id);
    }

    return status::success;
}

static void connection_add(modeling&              mod,
                           vector<connection_id>& cnts,
                           child_id               src,
                           port_id                port_src,
                           child_id               dst,
                           port_id                port_dst) noexcept
{
    auto& c    = mod.connections.alloc(src, port_src, dst, port_dst);
    auto  c_id = mod.connections.get_id(c);
    cnts.emplace_back(c_id);
}

static void in_out_connection_add(modeling&              mod,
                                  vector<connection_id>& cnts,
                                  child_id               src,
                                  child_id               dst) noexcept
{
    port_id p_src = undefined<port_id>();
    port_id p_dst = undefined<port_id>();

    if_child_is_component_do(mod, src, [&](auto& /* ch */, auto& compo) {
        p_src = mod.get_y_index(compo, "out");
    });

    if_child_is_component_do(mod, dst, [&](auto& /* ch */, auto& compo) {
        p_dst = mod.get_x_index(compo, "in");
    });

    if (is_defined(p_src) && is_defined(p_dst))
        connection_add(mod, cnts, src, p_src, dst, p_dst);
}

static void named_connection_add(modeling&              mod,
                                 vector<connection_id>& cnts,
                                 child_id               src,
                                 child_id               dst) noexcept
{
    port_id p_src = undefined<port_id>();
    port_id p_dst = undefined<port_id>();

    if_child_is_component_do(mod, src, [&](auto& /* ch_src */, auto& compo_src) {
        if_child_is_component_do(mod, dst, [&](auto& /* ch_dst */, auto& compo_dst) {
            auto     sz_src = compo_src.x_names.ssize();
            auto     sz_dst = compo_dst.y_names.ssize();
            port_str temp;

            format(temp, "{}", sz_src);
            p_src = mod.get_x_index(compo_src, temp.sv());

            format(temp, "{}", sz_dst);
            p_dst = mod.get_y_index(compo_dst, temp.sv());
        });
    });

    if (is_defined(p_src) && is_defined(p_dst))
        connection_add(mod, cnts, src, p_src, dst, p_dst);
}

static bool get_dir(modeling& mod, dir_path_id id, dir_path*& out) noexcept
{
    return if_data_exists_return(
      mod.dir_paths,
      id,
      [&](auto& dir) noexcept {
          out = &dir;
          return true;
      },
      false);
}

static bool get_file(modeling& mod, file_path_id id, file_path*& out) noexcept
{
    return if_data_exists_return(
      mod.file_paths,
      id,
      [&](auto& file) noexcept {
          out = &file;
          return true;
      },
      false);
}

static bool open_file(dir_path& dir_p, file_path& file_p, file& out) noexcept
{
    try {
        std::filesystem::path p = dir_p.path.u8sv();
        p /= file_p.path.u8sv();

        std::u8string u8str = p.u8string();
        const char*   cstr  = reinterpret_cast<const char*>(u8str.c_str());

        out.open(cstr, open_mode::read);
        if (out.is_open())
            return true;
    } catch (...) {
    }

    return false;
}

static bool read_dot_file(modeling& /* mod */,
                          file& /* f */,
                          graph_component& /* graph */,
                          std::span<child_id> /* ids */,
                          vector<connection_id>& /* cnts */) noexcept
{
    return false;
}

static status build_dot_file_connections(
  modeling&                              mod,
  graph_component&                       graph,
  const graph_component::dot_file_param& params,
  std::span<child_id>                    ids,
  vector<connection_id>&                 cnts) noexcept
{
    dir_path*  dir_p  = nullptr;
    file_path* file_p = nullptr;
    file       f;

    return get_dir(mod, params.dir, dir_p) &&
               get_file(mod, params.file, file_p) &&
               open_file(*dir_p, *file_p, f) &&
               read_dot_file(mod, f, graph, ids, cnts)
             ? status::success
             : status::io_file_format_error;
}

static auto build_scale_free_connections(
  modeling&                                mod,
  graph_component&                         graph,
  const graph_component::scale_free_param& params,
  std::span<child_id>                      ids,
  vector<connection_id>&                   cnts) noexcept -> status
{
    const unsigned n = graph.children.size();
    irt_assert(n > 1);

    local_rng r(std::span<u64>(graph.seed), std::span<u64>(graph.key));
    std::uniform_int_distribution<unsigned> d(0u, n - 1);

    unsigned first{};
    unsigned second{};

    for (;;) {
        unsigned xv = d(r);
        unsigned degree =
          (xv == 0 ? 0 : unsigned(params.beta * std::pow(xv, -params.alpha)));

        while (degree == 0) {
            if (++first >= n)
                return status::success;

            xv = d(r);
            degree =
              (xv == 0 ? 0
                       : unsigned(params.beta * std::pow(xv, -params.alpha)));
        }

        do {
            second = d(r);
        } while (first == second);
        --degree;

        irt_return_if_fail(mod.connections.can_alloc(),
                           status::data_array_not_enough_memory);

        if (graph.type == graph_component::connection_type::name)
            named_connection_add(mod, cnts, ids[first], ids[second]);
        else
            in_out_connection_add(mod, cnts, ids[first], ids[second]);
    }

    return status::success;
}

static auto build_small_world_connections(
  modeling&                                 mod,
  graph_component&                          graph,
  const graph_component::small_world_param& params,
  std::span<child_id>                       ids,
  vector<connection_id>&                    cnts) noexcept -> status
{
    const int n = graph.children.ssize();
    irt_assert(n > 1);

    local_rng r(std::span<u64>(graph.seed), std::span<u64>(graph.key));
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
            } while ((second >= lower && second <= upper) ||
                     (upper < lower && (second >= lower || second <= upper)));
        } else {
            second = target;
        }

        irt_return_if_fail(mod.connections.can_alloc(),
                           status::data_array_not_enough_memory);

        irt_assert(first >= 0 && first < n);
        irt_assert(second >= 0 && second < n);

        if (graph.type == graph_component::connection_type::name)
            named_connection_add(mod,
                                 cnts,
                                 ids[static_cast<unsigned>(first)],
                                 ids[static_cast<unsigned>(second)]);
        else
            in_out_connection_add(mod,
                                  cnts,
                                  ids[static_cast<unsigned>(first)],
                                  ids[static_cast<unsigned>(second)]);
    } while (source + 1 < n);

    return status::success;
}

static auto build_graph_connections(modeling&              mod,
                                    graph_component&       graph,
                                    std::span<child_id>    ids,
                                    vector<connection_id>& cnts) noexcept
  -> status
{
    switch (graph.param.index()) {
    case 0:
        return build_dot_file_connections(
          mod,
          graph,
          *std::get_if<graph_component::dot_file_param>(&graph.param),
          ids,
          cnts);
    case 1:
        return build_scale_free_connections(
          mod,
          graph,
          *std::get_if<graph_component::scale_free_param>(&graph.param),
          ids,
          cnts);
    case 2:
        return build_small_world_connections(
          mod,
          graph,
          *std::get_if<graph_component::small_world_param>(&graph.param),
          ids,
          cnts);
    };

    return status::success;
}

status modeling::build_graph_children_and_connections(
  graph_component&       graph,
  vector<child_id>&      ids,
  vector<connection_id>& cnts,
  i32                    upper_limit,
  i32                    left_limit,
  i32                    space_x,
  i32                    space_y) noexcept
{
    // Use to compute graph access with existing children in ids
    // vector.
    const auto old_size = ids.ssize();

    irt_return_if_bad(build_graph_children(
      *this, graph, ids, upper_limit, left_limit, space_x, space_y));

    irt_return_if_bad(build_graph_connections(
      *this,
      graph,
      std::span<child_id>(ids.begin() + old_size, ids.end()),
      cnts));

    return status::success;
}

status modeling::build_graph_component_cache(graph_component& graph) noexcept
{
    clear_graph_component_cache(graph);

    return build_graph_children_and_connections(
      graph, graph.cache, graph.cache_connections);
}

void modeling::clear_graph_component_cache(graph_component& graph) noexcept
{
    for (auto id : graph.cache)
        children.free(id);

    for (auto id : graph.cache_connections)
        connections.free(id);

    graph.cache.clear();
    graph.cache_connections.clear();
}

status modeling::copy(graph_component& grid, generic_component& s) noexcept
{
    return build_graph_children_and_connections(
      grid, s.children, s.connections);
}

} // namespace irt
