// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <math.h>
#include <numeric>
#include <optional>
#include <utility>

namespace irt {

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

// static void connection_add(modeling&              mod,
//                            vector<connection_id>& cnts,
//                            child_id               src,
//                            i8                     port_src,
//                            child_id               dst,
//                            i8                     port_dst) noexcept
// {
//     auto& c              = mod.connections.alloc();
//     auto  c_id           = mod.connections.get_id(c);
//     c.type               = connection::connection_type::internal;
//     c.internal.src       = src;
//     c.internal.index_src = port_src;
//     c.internal.dst       = dst;
//     c.internal.index_dst = port_dst;
//     cnts.emplace_back(c_id);
// }

static auto build_graph_connections(modeling&        mod,
                                    graph_component& graph,
                                    vector<child_id>& /* ids */,
                                    vector<connection_id>& /* cnts */,
                                    int /* old_size */) noexcept -> status
{
    irt_return_if_fail(mod.connections.can_alloc(graph.children.size() * 4),
                       status::data_array_not_enough_memory);

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
    irt_return_if_bad(
      build_graph_connections(*this, graph, ids, cnts, old_size));

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
