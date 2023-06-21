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
#include <numeric>
#include <optional>
#include <utility>

namespace irt {

static auto build_grid_children(modeling&         mod,
                                grid_component&   grid,
                                vector<child_id>& ids,
                                i32               upper_limit,
                                i32               left_limit,
                                i32               space_x,
                                i32               space_y) noexcept -> status
{
    irt_return_if_fail(grid.row && grid.column, status::io_project_file_error);
    irt_return_if_fail(mod.children.can_alloc(grid.row * grid.column),
                       status::data_array_not_enough_memory);

    const auto old_size = ids.ssize();
    ids.reserve(grid.row * grid.column + old_size);

    for (int i = 0, e = grid.children.ssize(); i != e; ++i) {
        child_id new_id = undefined<child_id>();

        if (mod.components.try_to_get(grid.children[i])) {
            auto& new_ch = mod.children.alloc(grid.children[i]);
            new_id       = mod.children.get_id(new_ch);

            auto pos         = grid.pos(i);
            new_ch.unique_id = grid.unique_id(pos.first, pos.second);

            mod.children_positions[get_index(new_id)] =
              child_position{ .x = static_cast<float>(
                                ((space_x * i) / grid.row) + left_limit),
                              .y = static_cast<float>(
                                ((space_y * i) % grid.row) + upper_limit) };
        }

        ids.emplace_back(new_id);
    }

    return status::success;
}

static void connection_add(modeling&              mod,
                           vector<connection_id>& cnts,
                           child_id               src,
                           int                    port_src,
                           child_id               dst,
                           int                    port_dst) noexcept
{
    auto& c              = mod.connections.alloc();
    auto  c_id           = mod.connections.get_id(c);
    c.type               = connection::connection_type::internal;
    c.internal.src       = src;
    c.internal.index_src = port_src;
    c.internal.dst       = dst;
    c.internal.index_dst = port_dst;
    cnts.emplace_back(c_id);
}

static void build_grid_connections_4(modeling&              mod,
                                     grid_component&        grid,
                                     vector<child_id>&      ids,
                                     vector<connection_id>& cnts,
                                     int                    row,
                                     int                    col,
                                     int                    old_size,
                                     std::span<int>         src_port_index,
                                     std::span<int> dst_port_index) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[old_size + grid.pos(row_min, col)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[0], dst, dst_port_index[0]);
    }

    if (row_max != row) {
        const auto dst = ids[old_size + grid.pos(row_max, col)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[1], dst, src_port_index[1]);
    }

    if (col_min != col) {
        const auto dst = ids[old_size + grid.pos(row, col_min)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[2], dst, src_port_index[2]);
    }

    if (col_max != col) {
        const auto dst = ids[old_size + grid.pos(row, col_max)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[3], dst, src_port_index[3]);
    }
}

static void build_grid_connections_8(modeling&              mod,
                                     grid_component&        grid,
                                     vector<child_id>&      ids,
                                     vector<connection_id>& cnts,
                                     int                    row,
                                     int                    col,
                                     int                    old_size,
                                     std::span<int>         src_port_index,
                                     std::span<int> dst_port_index) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    build_grid_connections_4(
      mod, grid, ids, cnts, row, col, old_size, src_port_index, dst_port_index);

    if (row_min != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_min)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[4], dst, dst_port_index[4]);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_min)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[5], dst, dst_port_index[5]);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_max)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[6], dst, dst_port_index[6]);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_max)];
        if (is_defined(dst))
            connection_add(
              mod, cnts, src, src_port_index[7], dst, dst_port_index[7]);
    }
}

static auto build_grid_default_connections(modeling&              mod,
                                           grid_component&        grid,
                                           vector<child_id>&      ids,
                                           vector<connection_id>& cnts,
                                           int old_size) noexcept -> status
{
    irt_return_if_fail(mod.connections.can_alloc(grid.row * grid.column * 4),
                       status::data_array_not_enough_memory);

    std::array<int, 8> src_index;
    std::array<int, 8> dst_index;

    if (grid.connection_type == grid_component::type::number) {
        src_index.fill(0);
        dst_index.fill(0);
    } else {
        std::iota(src_index.begin(), src_index.end(), 0);
        std::iota(dst_index.begin(), dst_index.end(), 0);
    }

    for (int row = 0; row < grid.row; ++row) {
        for (int col = 0; col < grid.column; ++col) {
            const auto src_id = ids[old_size + grid.pos(row, col)];
            if (src_id == undefined<child_id>())
                continue;

            switch (grid.neighbors) {
            case grid_component::neighborhood::four:
                build_grid_connections_4(mod,
                                         grid,
                                         ids,
                                         cnts,
                                         row,
                                         col,
                                         old_size,
                                         std::span<int>(src_index),
                                         std::span<int>(dst_index));
                break;
            case grid_component::neighborhood::eight:
                build_grid_connections_8(mod,
                                         grid,
                                         ids,
                                         cnts,
                                         row,
                                         col,
                                         old_size,
                                         std::span<int>(src_index),
                                         std::span<int>(dst_index));
                break;
            default:
                break;
            };
        }
    }

    return status::success;
}

static auto build_grid_connections(modeling&              mod,
                                   grid_component&        grid,
                                   vector<child_id>&      ids,
                                   vector<connection_id>& cnts,
                                   int old_size) noexcept -> status
{
    irt_return_if_fail(mod.connections.can_alloc(grid.row * grid.column * 4),
                       status::data_array_not_enough_memory);

    irt_return_if_bad(
      build_grid_default_connections(mod, grid, ids, cnts, old_size));

    const auto use_row_cylinder = match(grid.opts,
                                        grid_component::options::row_cylinder,
                                        grid_component::options::torus);

    if (use_row_cylinder) {
        for (int row = 0; row < grid.row; ++row) {
            const auto src_id = ids[old_size + grid.pos(row, 0)];
            const auto dst_id = ids[old_size + grid.pos(row, grid.column - 1)];

            auto& c1 = mod.connections.alloc();
            cnts.emplace_back(mod.connections.get_id(c1));
            c1.type               = connection::connection_type::internal;
            c1.internal.src       = src_id;
            c1.internal.index_src = 0;
            c1.internal.dst       = dst_id;
            c1.internal.index_dst = 0;

            auto& c2 = mod.connections.alloc();
            cnts.emplace_back(mod.connections.get_id(c2));
            c2.type               = connection::connection_type::internal;
            c2.internal.src       = dst_id;
            c2.internal.index_src = 0;
            c2.internal.dst       = src_id;
            c2.internal.index_dst = 0;
        }
    }

    const auto use_column_cylinder =
      match(grid.opts,
            grid_component::options::column_cylinder,
            grid_component::options::torus);

    if (use_column_cylinder) {
        for (int col = 0; col < grid.column; ++col) {
            const auto src_id = ids[old_size + grid.pos(0, col)];
            const auto dst_id = ids[old_size + grid.pos(grid.row - 1, col)];

            auto& c1 = mod.connections.alloc();
            cnts.emplace_back(mod.connections.get_id(c1));
            c1.internal.src       = src_id;
            c1.internal.index_src = 0;
            c1.internal.dst       = dst_id;
            c1.internal.index_dst = 0;

            auto& c2 = mod.connections.alloc();
            cnts.emplace_back(mod.connections.get_id(c2));
            c2.internal.src       = dst_id;
            c2.internal.index_src = 0;
            c2.internal.dst       = src_id;
            c2.internal.index_dst = 0;
        }
    }

    return status::success;
}

status modeling::build_grid_children_and_connections(
  grid_component&        grid,
  vector<child_id>&      ids,
  vector<connection_id>& cnts,
  i32                    upper_limit,
  i32                    left_limit,
  i32                    space_x,
  i32                    space_y) noexcept
{
    // Use to compute grid access with existing children in ids
    // vector.
    const auto old_size = ids.ssize();

    irt_return_if_bad(build_grid_children(
      *this, grid, ids, upper_limit, left_limit, space_x, space_y));
    irt_return_if_bad(build_grid_connections(*this, grid, ids, cnts, old_size));

    return status::success;
}

status modeling::build_grid_component_cache(grid_component& grid) noexcept
{
    clear_grid_component_cache(grid);

    return build_grid_children_and_connections(
      grid, grid.cache, grid.cache_connections);
}

void modeling::clear_grid_component_cache(grid_component& grid) noexcept
{
    for (auto id : grid.cache)
        children.free(id);

    for (auto id : grid.cache_connections)
        connections.free(id);

    grid.cache.clear();
    grid.cache_connections.clear();
}

status modeling::copy(grid_component& grid, generic_component& s) noexcept
{
    return build_grid_children_and_connections(grid, s.children, s.connections);
}

} // namespace irt
