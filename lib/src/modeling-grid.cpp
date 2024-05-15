// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <utility>

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

constexpr static inline std::string_view p_in_out_names[] = { "in", "out" };
constexpr static inline std::string_view p_4x4_names[] = { "N", "S", "W", "E" };
constexpr static inline std::string_view p_8x8_names[] = { "N",  "S",  "W",
                                                           "E",  "NE", "SE",
                                                           "NW", "SW" };

constexpr static int compute_grid_children_size(
  const grid_component& grid) noexcept
{
    return grid.row * grid.column;
}

constexpr static int compute_grid_connections_size(
  const grid_component& grid) noexcept
{
    const int children = compute_grid_children_size(grid);

    int row_mult = 0;
    int col_mult = 0;

    switch (grid.opts) {
    case grid_component::options::none:
        break;
    case grid_component::options::column_cylinder:
        col_mult = 1;
        break;
    case grid_component::options::row_cylinder:
        row_mult = 1;
        break;
    case grid_component::options::torus:
        col_mult = 1;
        row_mult = 1;
        break;
    }

    int children_mult = 1;

    switch (grid.neighbors) {
    case grid_component::neighborhood::eight:
        children_mult = 8;
        break;
    case irt::grid_component::neighborhood::four:
        children_mult = 4;
        break;
    }

    return children * children_mult + grid.column * col_mult +
           grid.row * row_mult;
}

static bool can_alloc_grid_children_and_connections(
  grid_component& grid) noexcept
{
    const auto children    = compute_grid_children_size(grid);
    const auto connections = compute_grid_connections_size(grid);

    grid.cache.reserve(children);
    grid.cache_connections.reserve(connections);

    return grid.cache.can_alloc(children) and
           grid.cache_connections.can_alloc(connections);
}

static auto build_grid_children(modeling& mod, grid_component& grid) noexcept
  -> vector<child_id>
{
    const auto children_nb = compute_grid_children_size(grid);

    vector<child_id> ret;

    ret.resize(children_nb);
    grid.cache.reserve(children_nb);

    for (int i = 0, e = grid.children.ssize(); i != e; ++i) {
        auto id = undefined<child_id>();

        if (mod.components.try_to_get(grid.children[i])) {
            auto& new_ch     = grid.cache.alloc(grid.children[i]);
            id               = grid.cache.get_id(new_ch);
            const auto pos   = grid.pos(i);
            new_ch.unique_id = grid.unique_id(pos.first, pos.second);
        };

        ret[i] = id;
    }

    return ret;
}

static void connection_add(modeling&        mod,
                           grid_component&  grid,
                           child_id         src,
                           std::string_view port_src,
                           child_id         dst,
                           std::string_view port_dst) noexcept
{
    auto port_src_real = undefined<port_id>();
    if_data_exists_do(grid.cache, src, [&](auto& child) noexcept {
        debug::ensure(child.type == child_type::component);

        if (child.type == child_type::component) {
            if_data_exists_do(
              mod.components, child.id.compo_id, [&](auto& compo) noexcept {
                  port_src_real = compo.get_y(port_src);
              });
        }
    });

    auto port_dst_real = undefined<port_id>();
    if_data_exists_do(grid.cache, dst, [&](auto& child) noexcept {
        debug::ensure(child.type == child_type::component);

        if (child.type == child_type::component) {
            if_data_exists_do(
              mod.components, child.id.compo_id, [&](auto& compo) noexcept {
                  port_dst_real = compo.get_x(port_dst);
              });
        }
    });

    if (is_defined(port_src_real) && is_defined(port_dst_real))
        grid.cache_connections.alloc(src, port_src_real, dst, port_dst_real);
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_in_out        port_src,
                           child_id        dst,
                           p_in_out        port_dst) noexcept
{
    connection_add(mod,
                   grid,
                   src,
                   p_in_out_names[ordinal(port_src)],
                   dst,
                   p_in_out_names[ordinal(port_dst)]);
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_4x4           port_src,
                           child_id        dst,
                           p_4x4           port_dst) noexcept
{
    connection_add(mod,
                   grid,
                   src,
                   p_4x4_names[ordinal(port_src)],
                   dst,
                   p_4x4_names[ordinal(port_dst)]);
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_8x8           port_src,
                           child_id        dst,
                           p_8x8           port_dst) noexcept
{
    connection_add(mod,
                   grid,
                   src,
                   p_8x8_names[ordinal(port_src)],
                   dst,
                   p_8x8_names[ordinal(port_dst)]);
}

static void build_name_grid_affect_options(modeling&               mod,
                                           grid_component&         grid,
                                           const vector<child_id>& ids) noexcept
{
    if (any_equal(grid.opts,
                  grid_component::options::row_cylinder,
                  grid_component::options::torus)) {
        for (auto col = 0; col < grid.column; ++col) {
            const auto cell_1 = ids[grid.pos(grid.row - 1, col)];
            const auto cell_2 = ids[grid.pos(0, col)];
            connection_add(mod, grid, cell_1, p_4x4::west, cell_2, p_4x4::east);
            connection_add(mod, grid, cell_2, p_4x4::east, cell_1, p_4x4::west);
        }
    }

    if (any_equal(grid.opts,
                  grid_component::options::column_cylinder,
                  grid_component::options::torus)) {
        for (auto row = 0; row < grid.column; ++row) {
            const auto cell_1 = ids[grid.pos(row, grid.column - 1)];
            const auto cell_2 = ids[grid.pos(row, 0)];
            connection_add(
              mod, grid, cell_1, p_4x4::north, cell_2, p_4x4::south);
            connection_add(
              mod, grid, cell_2, p_4x4::south, cell_1, p_4x4::north);
        }
    }
}

static void build_simple_grid_affect_options(
  modeling&               mod,
  grid_component&         grid,
  const vector<child_id>& ids) noexcept
{
    if (any_equal(grid.opts,
                  grid_component::options::row_cylinder,
                  grid_component::options::torus)) {
        for (auto col = 0; col < grid.column; ++col) {
            const auto cell_1 = ids[grid.pos(grid.row - 1, col)];
            const auto cell_2 = ids[grid.pos(0, col)];
            connection_add(
              mod, grid, cell_1, p_in_out::out, cell_2, p_in_out::in);
            connection_add(
              mod, grid, cell_2, p_in_out::out, cell_1, p_in_out::in);
        }
    }

    if (any_equal(grid.opts,
                  grid_component::options::column_cylinder,
                  grid_component::options::torus)) {
        for (auto row = 0; row < grid.column; ++row) {
            const auto cell_1 = ids[grid.pos(row, grid.column - 1)];
            const auto cell_2 = ids[grid.pos(row, 0)];
            connection_add(
              mod, grid, cell_1, p_in_out::out, cell_2, p_in_out::in);
            connection_add(
              mod, grid, cell_2, p_in_out::out, cell_1, p_in_out::in);
        }
    }
}

static void build_name_grid_connections_4(modeling&               mod,
                                          grid_component&         grid,
                                          const vector<child_id>& ids,
                                          int                     row,
                                          int                     col) noexcept
{
    debug::ensure(0 <= row and row < grid.row);
    debug::ensure(0 <= col and col < grid.column);
    debug::ensure(row * col < ids.ssize());

    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[grid.pos(row_min, col)];
        connection_add(mod, grid, src, p_4x4::south, dst, p_4x4::north);
    }

    if (row_max != row) {
        const auto dst = ids[grid.pos(row_max, col)];
        connection_add(mod, grid, src, p_4x4::north, dst, p_4x4::south);
    }

    if (col_min != col) {
        const auto dst = ids[grid.pos(row, col_min)];
        connection_add(mod, grid, src, p_4x4::east, dst, p_4x4::west);
    }

    if (col_max != col) {
        const auto dst = ids[grid.pos(row, col_max)];
        connection_add(mod, grid, src, p_4x4::west, dst, p_4x4::east);
    }

    build_name_grid_affect_options(mod, grid, ids);
}

static void build_simple_grid_connections_4(modeling&               mod,
                                            grid_component&         grid,
                                            const vector<child_id>& ids,
                                            int                     row,
                                            int col) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[grid.pos(row_min, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row) {
        const auto dst = ids[grid.pos(row_max, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_min != col) {
        const auto dst = ids[grid.pos(row, col_min)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_max != col) {
        const auto dst = ids[grid.pos(row, col_max)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    build_simple_grid_affect_options(mod, grid, ids);
}

static void build_name_grid_connections_8(modeling&               mod,
                                          grid_component&         grid,
                                          const vector<child_id>& ids,
                                          int                     row,
                                          int                     col) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[grid.pos(row, col)];

    build_name_grid_connections_4(mod, grid, ids, row, col);

    if (row_min != row && col_min != col) {
        const auto dst = ids[grid.pos(row_min, col_min)];
        connection_add(
          mod, grid, src, p_8x8::north_west, dst, p_8x8::south_east);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(grid.row - 1, col)];
        connection_add(
          mod, grid, src, p_8x8::north_west, dst, p_8x8::south_east);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[grid.pos(row_max, col_min)];
        connection_add(
          mod, grid, src, p_8x8::north_east, dst, p_8x8::south_west);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(grid.row - 1, col)];
        connection_add(
          mod, grid, src, p_8x8::north_east, dst, p_8x8::south_west);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[grid.pos(row_min, col_max)];
        connection_add(
          mod, grid, src, p_8x8::south_west, dst, p_8x8::north_east);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(
          mod, grid, src, p_8x8::south_west, dst, p_8x8::north_east);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[grid.pos(row_max, col_max)];
        connection_add(
          mod, grid, src, p_8x8::south_east, dst, p_8x8::north_west);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(
          mod, grid, src, p_8x8::south_east, dst, p_8x8::north_west);
    }
}

static void build_simple_grid_connections_8(modeling&               mod,
                                            grid_component&         grid,
                                            const vector<child_id>& ids,
                                            int                     row,
                                            int col) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[grid.pos(row, col)];

    build_simple_grid_connections_4(mod, grid, ids, row, col);

    if (row_min != row && col_min != col) {
        const auto dst = ids[grid.pos(row_min, col_min)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[grid.pos(row_max, col_min)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[grid.pos(row_min, col_max)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[grid.pos(row_max, col_max)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }
}

static auto build_grid_connections(modeling&               mod,
                                   grid_component&         grid,
                                   const vector<child_id>& ids) noexcept
  -> status
{
    const auto connections_nb = compute_grid_connections_size(grid);

    grid.cache_connections.reserve(connections_nb);

    if (grid.connection_type == grid_component::type::number) {
        switch (grid.neighbors) {
        case grid_component::neighborhood::four:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_simple_grid_connections_4(mod, grid, ids, row, col);
                }
            }
            break;
        case grid_component::neighborhood::eight:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_simple_grid_connections_8(mod, grid, ids, row, col);
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
                    const auto src_id = ids[grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_name_grid_connections_4(mod, grid, ids, row, col);
                }
            }
            break;

        case grid_component::neighborhood::eight:
            for (int row = 0; row < grid.row; ++row) {
                for (int col = 0; col < grid.column; ++col) {
                    const auto src_id = ids[grid.pos(row, col)];
                    if (src_id == undefined<child_id>())
                        continue;

                    build_name_grid_connections_8(mod, grid, ids, row, col);
                }
            }
            break;

        default:
            unreachable();
        }
    }

    return success();
}

status modeling::copy(grid_component& grid, generic_component& s) noexcept
{
    irt_check(grid.build_cache(*this));

    return s.import(grid.cache, grid.cache_connections);
}

bool grid_component::exists_input_connection(const port_id x,
                                             const i32     row,
                                             const i32     col,
                                             const port_id id) const noexcept
{
    for (const auto& con : input_connections)
        if (x == con.x and row == con.row and col == con.col and id == con.id)
            return true;

    return false;
}

bool grid_component::exists_output_connection(const port_id y,
                                              const i32     row,
                                              const i32     col,
                                              const port_id id) const noexcept
{
    for (const auto& con : output_connections)
        if (y == con.y and row == con.row and col == con.col and id == con.id)
            return true;

    return false;
}

result<input_connection_id> grid_component::connect_input(
  const port_id x,
  const i32     row,
  const i32     col,
  const port_id id) noexcept
{
    if (exists_input_connection(x, row, col, id))
        return new_error(modeling::part::grid_components);

    return input_connections.get_id(input_connections.alloc(x, row, col, id));
}

result<output_connection_id> grid_component::connect_output(
  const port_id y,
  const i32     row,
  const i32     col,
  const port_id id) noexcept
{
    if (exists_output_connection(y, row, col, id))
        return new_error(modeling::part::grid_components);

    return output_connections.get_id(output_connections.alloc(y, row, col, id));
}

void grid_component::clear_cache() noexcept
{
    cache.clear();
    cache_connections.clear();
}

status grid_component::build_cache(modeling& mod) noexcept
{
    if (not can_alloc_grid_children_and_connections(*this))
        return new_error(project::error::not_enough_memory);

    clear_cache();
    const auto vec = build_grid_children(mod, *this);
    irt_check(build_grid_connections(mod, *this, vec));

    return success();
}

} // namespace irt
