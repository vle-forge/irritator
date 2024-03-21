// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/error.hpp"
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <optional>
#include <utility>

#include <cstdint>

namespace irt {

enum class p_in_out { in = 0, out };

enum class p_4x4 { north = 0, south, west, east };

enum class p_8x8 {
    north = 0,
    south,
    west,
    east,
    north_east,
    south_east,
    north_west,
    south_west
};

constexpr std::string_view p_in_out_names[] = { "in", "out" };
constexpr std::string_view p_4x4_names[]    = { "N", "S", "W", "E" };
constexpr std::string_view p_8x8_names[]    = { "N",  "S",  "W",  "E",
                                                "NE", "SE", "NW", "SW" };

static auto build_grid_children(modeling&         mod,
                                grid_component&   grid,
                                vector<child_id>& ids,
                                i32               upper_limit,
                                i32               left_limit,
                                i32               space_x,
                                i32               space_y) noexcept -> status
{
    irt_assert(grid.row && grid.column);

    if (!mod.children.can_alloc(grid.row * grid.column))
        return new_error(project::error::not_enough_memory);

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

    return success();
}

static void connection_add(modeling&              mod,
                           vector<connection_id>& cnts,
                           child_id               src,
                           p_in_out               port_src,
                           child_id               dst,
                           p_in_out               port_dst) noexcept
{
    port_id port_src_real = undefined<port_id>();
    port_id port_dst_real = undefined<port_id>();

    if_child_is_component_do(
      mod, src, [&](auto& /*child*/, auto& compo) noexcept {
          port_src_real =
            mod.get_y_index(compo, p_in_out_names[ordinal(port_src)]);
      });

    if_child_is_component_do(
      mod, dst, [&](auto& /*child*/, auto& compo) noexcept {
          port_dst_real =
            mod.get_x_index(compo, p_in_out_names[ordinal(port_dst)]);
      });

    if (is_defined(port_src_real) && is_defined(port_dst_real)) {
        auto& c = mod.connections.alloc(src, port_src_real, dst, port_dst_real);
        auto  c_id = mod.connections.get_id(c);
        cnts.emplace_back(c_id);
    }
}

static void connection_add(modeling&              mod,
                           vector<connection_id>& cnts,
                           child_id               src,
                           p_4x4                  port_src,
                           child_id               dst,
                           p_4x4                  port_dst) noexcept
{
    port_id port_src_real = undefined<port_id>();
    port_id port_dst_real = undefined<port_id>();

    if_child_is_component_do(
      mod, src, [&](auto& /*child*/, auto& compo) noexcept {
          port_src_real =
            mod.get_x_index(compo, p_4x4_names[ordinal(port_src)]);
      });

    if_child_is_component_do(
      mod, src, [&](auto& /*child*/, auto& compo) noexcept {
          port_dst_real =
            mod.get_y_index(compo, p_4x4_names[ordinal(port_dst)]);
      });

    if (is_defined(port_src_real) && is_defined(port_dst_real)) {
        auto& c = mod.connections.alloc(src, port_src_real, dst, port_dst_real);
        auto  c_id = mod.connections.get_id(c);
        cnts.emplace_back(c_id);
    }
}

static void connection_add(modeling&              mod,
                           vector<connection_id>& cnts,
                           child_id               src,
                           p_8x8                  port_src,
                           child_id               dst,
                           p_8x8                  port_dst) noexcept
{
    port_id port_src_real = undefined<port_id>();
    port_id port_dst_real = undefined<port_id>();

    if_child_is_component_do(
      mod, src, [&](auto& /*child*/, auto& compo) noexcept {
          port_src_real =
            mod.get_x_index(compo, p_8x8_names[ordinal(port_src)]);
      });

    if_child_is_component_do(
      mod, src, [&](auto& /*child*/, auto& compo) noexcept {
          port_dst_real =
            mod.get_y_index(compo, p_8x8_names[ordinal(port_dst)]);
      });

    if (is_defined(port_src_real) && is_defined(port_dst_real)) {
        auto& c = mod.connections.alloc(src, port_src_real, dst, port_dst_real);
        auto  c_id = mod.connections.get_id(c);
        cnts.emplace_back(c_id);
    }
}

static void build_name_grid_connections_4(modeling&              mod,
                                          grid_component&        grid,
                                          vector<child_id>&      ids,
                                          vector<connection_id>& cnts,
                                          int                    row,
                                          int                    col,
                                          int old_size) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[old_size + grid.pos(row_min, col)];
        connection_add(mod, cnts, src, p_4x4::south, dst, p_4x4::north);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(mod, cnts, src, p_4x4::south, dst, p_4x4::north);
    }

    if (row_max != row) {
        const auto dst = ids[old_size + grid.pos(row_max, col)];
        connection_add(mod, cnts, src, p_4x4::north, dst, p_4x4::south);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(0, col)];
        connection_add(mod, cnts, src, p_4x4::north, dst, p_4x4::south);
    }

    if (col_min != col) {
        const auto dst = ids[old_size + grid.pos(row, col_min)];
        connection_add(mod, cnts, src, p_4x4::east, dst, p_4x4::west);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_4x4::east, dst, p_4x4::west);
    }

    if (col_max != col) {
        const auto dst = ids[old_size + grid.pos(row, col_max)];
        connection_add(mod, cnts, src, p_4x4::west, dst, p_4x4::east);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, 0)];
        connection_add(mod, cnts, src, p_4x4::west, dst, p_4x4::east);
    }
}

static void build_simple_grid_connections_4(modeling&              mod,
                                            grid_component&        grid,
                                            vector<child_id>&      ids,
                                            vector<connection_id>& cnts,
                                            int                    row,
                                            int                    col,
                                            int old_size) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[old_size + grid.pos(row_min, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row) {
        const auto dst = ids[old_size + grid.pos(row_max, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(0, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_min != col) {
        const auto dst = ids[old_size + grid.pos(row, col_min)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_max != col) {
        const auto dst = ids[old_size + grid.pos(row, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, 0)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }
}

static void build_name_grid_connections_8(modeling&              mod,
                                          grid_component&        grid,
                                          vector<child_id>&      ids,
                                          vector<connection_id>& cnts,
                                          int                    row,
                                          int                    col,
                                          int old_size) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    build_name_grid_connections_4(mod, grid, ids, cnts, row, col, old_size);

    if (row_min != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_min)];
        connection_add(
          mod, cnts, src, p_8x8::north_west, dst, p_8x8::south_east);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(
          mod, cnts, src, p_8x8::north_west, dst, p_8x8::south_east);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_min)];
        connection_add(
          mod, cnts, src, p_8x8::north_east, dst, p_8x8::south_west);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(
          mod, cnts, src, p_8x8::north_east, dst, p_8x8::south_west);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_max)];
        connection_add(
          mod, cnts, src, p_8x8::south_west, dst, p_8x8::north_east);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(
          mod, cnts, src, p_8x8::south_west, dst, p_8x8::north_east);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_max)];
        connection_add(
          mod, cnts, src, p_8x8::south_east, dst, p_8x8::north_west);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(
          mod, cnts, src, p_8x8::south_east, dst, p_8x8::north_west);
    }
}

static void build_simple_grid_connections_8(modeling&              mod,
                                            grid_component&        grid,
                                            vector<child_id>&      ids,
                                            vector<connection_id>& cnts,
                                            int                    row,
                                            int                    col,
                                            int old_size) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[old_size + grid.pos(row, col)];

    build_simple_grid_connections_4(mod, grid, ids, cnts, row, col, old_size);

    if (row_min != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_min)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_min)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }
}

static auto build_grid_connections(modeling&              mod,
                                   grid_component&        grid,
                                   vector<child_id>&      ids,
                                   vector<connection_id>& cnts,
                                   int old_size) noexcept -> status
{
    if (auto ret = mod.connections.can_alloc(grid.row * grid.column * 4); !ret)
        return new_error(project::error::not_enough_memory);

    if (grid.connection_type == grid_component::type::number) {
        switch (grid.neighbors) {
        case grid_component::neighborhood::four:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[old_size + grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_simple_grid_connections_4(
                      mod, grid, ids, cnts, row, col, old_size);
                }
            }
            break;
        case grid_component::neighborhood::eight:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[old_size + grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_simple_grid_connections_8(
                      mod, grid, ids, cnts, row, col, old_size);
                }
            }
            break;

        default:
            unreachable();
        };
    } else {
        switch (grid.neighbors) {
        case grid_component::neighborhood::four:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[old_size + grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_name_grid_connections_4(
                      mod, grid, ids, cnts, row, col, old_size);
                }
            }
            break;

        case grid_component::neighborhood::eight:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[old_size + grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_name_grid_connections_8(
                      mod, grid, ids, cnts, row, col, old_size);
                }
            }
            break;

        default:
            unreachable();
        }
    }

    return success();
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
    // Use to compute grid access with existing children in ids vector.
    const auto old_size = ids.ssize();

    if (auto ret = build_grid_children(
          *this, grid, ids, upper_limit, left_limit, space_x, space_y);
        !ret)
        return ret.error();

    if (auto ret = build_grid_connections(*this, grid, ids, cnts, old_size);
        !ret)
        return ret.error();

    return success();
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

bool grid_component::exist_input_connection(const port_id x,
                                            const i32     row,
                                            const i32     col,
                                            const port_id id) const noexcept
{
    for (int i = 0, e = input_connections.ssize(); i != e; ++i)
        if (x == input_connections[i].x and row == input_connections[i].row and
            col == input_connections[i].col and id == input_connections[i].id)
            return true;

    return false;
}

bool grid_component::exist_output_connection(const port_id y,
                                             const i32     row,
                                             const i32     col,
                                             const port_id id) const noexcept
{
    for (int i = 0, e = output_connections.ssize(); i != e; ++i)
        if (y == output_connections[i].y and
            row == output_connections[i].row and
            col == output_connections[i].col and id == output_connections[i].id)
            return true;

    return false;
}

void grid_component::add_input_connection(const port_id x,
                                          const i32     row,
                                          const i32     col,
                                          const port_id id) noexcept
{
    if (not exist_input_connection(x, row, col, id))
        input_connections.push_back(input_connection{ x, row, col, id });
}

void grid_component::add_output_connection(const port_id y,
                                           const i32     row,
                                           const i32     col,
                                           const port_id id) noexcept
{
    if (not exist_output_connection(y, row, col, id))
        output_connections.push_back(output_connection{ y, row, col, id });
}

} // namespace irt
