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

constexpr std::string_view p_in_out_names[] = { "in", "out" };
constexpr std::string_view p_4x4_names[]    = { "N", "S", "W", "E" };
constexpr std::string_view p_8x8_names[]    = { "N",  "S",  "W",  "E",
                                                "NE", "SE", "NW", "SW" };

static auto build_grid_children(modeling&       mod,
                                grid_component& grid,
                                i32 /*upper_limit*/,
                                i32 /*left_limit*/,
                                i32 /*space_x*/,
                                i32 /*space_y*/) noexcept
  -> result<vector<child_id>>
{
    vector<child_id> ret;

    if (not grid.cache.can_alloc(grid.row * grid.column)) {
        grid.cache.reserve(grid.row * grid.column);
        if (not grid.cache.can_alloc(grid.row * grid.column)) {
            return new_error(project::error::not_enough_memory);
        }
    }

    for (int i = 0, e = grid.children.ssize(); i != e; ++i) {
        auto id = undefined<child_id>();

        if (mod.components.try_to_get(grid.children[i])) {
            auto& new_ch     = grid.cache.alloc(grid.children[i]);
            id               = grid.cache.get_id(new_ch);
            const auto pos   = grid.pos(i);
            new_ch.unique_id = grid.unique_id(pos.first, pos.second);
        };

        ret.emplace_back(id);
    }

    return ret;
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_in_out        port_src,
                           child_id        dst,
                           p_in_out        port_dst) noexcept
{
    auto port_src_real = undefined<port_id>();
    auto port_dst_real = undefined<port_id>();

    if_data_exists_do(grid.cache, src, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_src_real = compo->get_y(p_in_out_names[ordinal(port_src)]);
            }
        }
    });

    if_data_exists_do(grid.cache, dst, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_dst_real = compo->get_x(p_in_out_names[ordinal(port_dst)]);
            }
        }
    });

    if (is_defined(port_src_real) && is_defined(port_dst_real))
        grid.cache_connections.alloc(src, port_src_real, dst, port_dst_real);
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_4x4           port_src,
                           child_id        dst,
                           p_4x4           port_dst) noexcept
{
    port_id port_src_real = undefined<port_id>();
    port_id port_dst_real = undefined<port_id>();

    if_data_exists_do(grid.cache, src, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_src_real = compo->get_y(p_4x4_names[ordinal(port_src)]);
            }
        }
    });

    if_data_exists_do(grid.cache, dst, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_dst_real = compo->get_x(p_4x4_names[ordinal(port_dst)]);
            }
        }
    });

    if (is_defined(port_src_real) && is_defined(port_dst_real))
        grid.cache_connections.alloc(src, port_src_real, dst, port_dst_real);
}

static void connection_add(modeling&       mod,
                           grid_component& grid,
                           child_id        src,
                           p_8x8           port_src,
                           child_id        dst,
                           p_8x8           port_dst) noexcept
{
    port_id port_src_real = undefined<port_id>();
    port_id port_dst_real = undefined<port_id>();

    if_data_exists_do(grid.cache, src, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_src_real = compo->get_y(p_8x8_names[ordinal(port_src)]);
            }
        }
    });

    if_data_exists_do(grid.cache, dst, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if (auto* compo = mod.components.try_to_get(child.id.compo_id);
                compo) {
                port_dst_real = compo->get_x(p_8x8_names[ordinal(port_dst)]);
            }
        }
    });
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
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(grid.row - 1, col)];
        connection_add(mod, grid, src, p_4x4::south, dst, p_4x4::north);
    }

    if (row_max != row) {
        const auto dst = ids[grid.pos(row_max, col)];
        connection_add(mod, grid, src, p_4x4::north, dst, p_4x4::south);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(0, col)];
        connection_add(mod, grid, src, p_4x4::north, dst, p_4x4::south);
    }

    if (col_min != col) {
        const auto dst = ids[grid.pos(row, col_min)];
        connection_add(mod, grid, src, p_4x4::east, dst, p_4x4::west);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_4x4::east, dst, p_4x4::west);
    }

    if (col_max != col) {
        const auto dst = ids[grid.pos(row, col_max)];
        connection_add(mod, grid, src, p_4x4::west, dst, p_4x4::east);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, 0)];
        connection_add(mod, grid, src, p_4x4::west, dst, p_4x4::east);
    }
}

static void build_simple_grid_connections_4(modeling&         mod,
                                            grid_component&   grid,
                                            vector<child_id>& ids,
                                            int               row,
                                            int               col) noexcept
{
    const auto row_min = row == 0 ? 0 : row - 1;
    const auto row_max = row == grid.row - 1 ? row : row + 1;
    const auto col_min = col == 0 ? 0 : col - 1;
    const auto col_max = col == grid.column - 1 ? col : col + 1;
    const auto src     = ids[grid.pos(row, col)];

    if (row_min != row) {
        const auto dst = ids[grid.pos(row_min, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(grid.row - 1, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row) {
        const auto dst = ids[grid.pos(row_max, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::row_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(0, col)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_min != col) {
        const auto dst = ids[grid.pos(row, col_min)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, grid.column - 1)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_max != col) {
        const auto dst = ids[grid.pos(row, col_max)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    } else if (any_equal(grid.opts,
                         grid_component::options::column_cylinder,
                         grid_component::options::torus)) {
        const auto dst = ids[grid.pos(row, 0)];
        connection_add(mod, grid, src, p_in_out::out, dst, p_in_out::in);
    }
}

static void build_name_grid_connections_8(modeling&         mod,
                                          grid_component&   grid,
                                          vector<child_id>& ids,
                                          int               row,
                                          int               col) noexcept
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

static void build_simple_grid_connections_8(modeling&         mod,
                                            grid_component&   grid,
                                            vector<child_id>& ids,
                                            int               row,
                                            int               col) noexcept
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

static auto build_grid_connections(modeling&         mod,
                                   grid_component&   grid,
                                   vector<child_id>& ids) noexcept -> status
{
    if (not grid.cache_connections.can_alloc(grid.row * grid.column * 4)) {
        grid.cache_connections.reserve(grid.row * grid.column * 4);

        if (not grid.cache_connections.can_alloc(grid.row * grid.column * 4))
            return new_error(project::error::not_enough_memory);
    }

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

status modeling::build_grid_children_and_connections(grid_component& grid,
                                                     i32 upper_limit,
                                                     i32 left_limit,
                                                     i32 space_x,
                                                     i32 space_y) noexcept
{
    irt_auto(vec, build_grid_children(*this, grid, upper_limit, left_limit, space_x, space_y));
    irt_check(build_grid_connections(*this, grid, vec));

    return success();
}

status modeling::build_grid_component_cache(grid_component& grid) noexcept
{
    clear_grid_component_cache(grid);

    return build_grid_children_and_connections(grid);
}

void modeling::clear_grid_component_cache(grid_component& grid) noexcept
{
    grid.cache.clear();
    grid.cache_connections.clear();
}

status modeling::copy(grid_component& grid, generic_component& s) noexcept
{
    irt_check(build_grid_children_and_connections(grid));

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

} // namespace irt
