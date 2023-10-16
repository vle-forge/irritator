// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

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

enum class p_in_out
{
    in = 0,
    out
};

enum class p_4x4
{
    north = 0,
    south,
    west,
    east
};

enum class p_8x8
{
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
    } else if (match(grid.opts,
                     grid_component::options::row_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(mod, cnts, src, p_4x4::south, dst, p_4x4::north);
    }

    if (row_max != row) {
        const auto dst = ids[old_size + grid.pos(row_max, col)];
        connection_add(mod, cnts, src, p_4x4::north, dst, p_4x4::south);
    } else if (match(grid.opts,
                     grid_component::options::row_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(0, col)];
        connection_add(mod, cnts, src, p_4x4::north, dst, p_4x4::south);
    }

    if (col_min != col) {
        const auto dst = ids[old_size + grid.pos(row, col_min)];
        connection_add(mod, cnts, src, p_4x4::east, dst, p_4x4::west);
    } else if (match(grid.opts,
                     grid_component::options::column_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_4x4::east, dst, p_4x4::west);
    }

    if (col_max != col) {
        const auto dst = ids[old_size + grid.pos(row, col_max)];
        connection_add(mod, cnts, src, p_4x4::west, dst, p_4x4::east);
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
                     grid_component::options::row_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(grid.row - 1, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row) {
        const auto dst = ids[old_size + grid.pos(row_max, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
                     grid_component::options::row_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(0, col)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_min != col) {
        const auto dst = ids[old_size + grid.pos(row, col_min)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
                     grid_component::options::column_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (col_max != col) {
        const auto dst = ids[old_size + grid.pos(row, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
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
    } else if (match(grid.opts,
                     grid_component::options::column_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_min != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_min)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
                     grid_component::options::column_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_min != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_min, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
                     grid_component::options::column_cylinder,
                     grid_component::options::torus)) {
        const auto dst = ids[old_size + grid.pos(row, grid.column - 1)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    }

    if (row_max != row && col_max != col) {
        const auto dst = ids[old_size + grid.pos(row_max, col_max)];
        connection_add(mod, cnts, src, p_in_out::out, dst, p_in_out::in);
    } else if (match(grid.opts,
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
    irt_return_if_fail(mod.connections.can_alloc(grid.row * grid.column * 4),
                       status::data_array_not_enough_memory);

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
            irt_unreachable();
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
            irt_unreachable();
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

//
//
//

static observer_id get_observer_id(project&             pj,
                                   simulation&          sim,
                                   const tree_node&     tn,
                                   const std::span<u64> unique_ids,
                                   const model_id       mdl_id) noexcept
{
    const tree_node* ptr = &tn;

    for (const auto unique_id : unique_ids) {
        if (auto tree_node_id_opt = ptr->get_tree_node_id(unique_id);
            tree_node_id_opt.has_value()) {
            ptr = pj.tree_nodes.try_to_get(*tree_node_id_opt);
        }
    }

    if (auto mdl_id_opt = ptr->get_model_id(mdl_id); mdl_id_opt.has_value())
        if (auto* mdl = sim.models.try_to_get(*mdl_id_opt); mdl)
            return mdl->obs_id;

    return undefined<observer_id>();
}

static status build_grid(grid_observation_system& grid_system,
                         project&                 pj,
                         simulation&              sim,
                         tree_node&               grid_parent,
                         grid_component&          grid_compo,
                         grid_observer&           grid_obs) noexcept
{
    status ret = status::unknown_dynamics;

    if_data_exists_do(pj.tree_nodes, grid_obs.tn_id, [&](auto& tn) noexcept {
        small_vector<u64, 16> stack;

        // First step, build the stack with unique_id from parent to
        // grid_parent.
        stack.emplace_back(tn.unique_id);
        auto* parent = tn.tree.get_parent();
        while (parent && parent != &grid_parent) {
            stack.emplace_back(parent->unique_id);
            parent = parent->tree.get_parent();
        }

        // Second step, from grid parent, search child
        if (const auto* child = grid_parent.tree.get_child(); child) {
            do {
                if (child->unique_id == stack.back()) {
                    auto [row, col] = unpack_doubleword(child->unique_id);

                    grid_system.observers[row * grid_compo.column + col] =
                      get_observer_id(pj,
                                      sim,
                                      *child,
                                      std::span(stack.begin(), stack.end() - 1),
                                      grid_obs.mdl_id);
                }
                child = child->tree.get_sibling();
            } while (child);
        }
        ret = status::success;
    });

    return ret;
}

status grid_observation_system::init(project&       pj,
                                     modeling&      mod,
                                     simulation&    sim,
                                     grid_observer& grid_obs) noexcept
{
    status ret = status::unknown_dynamics;

    if_tree_node_is_grid_do(
      pj,
      mod,
      grid_obs.parent_id,
      [&](auto& parent_tn, auto& /*compo*/, auto& grid) {
          const auto len = grid.row * grid.column;

          observers.resize(len);
          values.resize(len);

          std::fill_n(observers.data(), len, undefined<observer_id>());
          std::fill_n(values.data(), len, none_value);

          id = pj.grid_observers.get_id(grid_obs);

          ret = build_grid(*this, pj, sim, parent_tn, grid, grid_obs);
      });

    return ret;
}

void grid_observation_system::resize(int rows_, int cols_) noexcept
{
    const auto len = rows_ * cols_;
    rows           = rows_;
    cols           = cols_;

    irt_assert(len > 0);

    observers.resize(len);
    values.resize(len);
    clear();
}

void grid_observation_system::clear() noexcept
{
    std::fill_n(observers.data(), observers.size(), undefined<observer_id>());
    std::fill_n(values.data(), values.size(), none_value);
}

void grid_observation_system::update(simulation& sim) noexcept
{
    irt_assert(rows * cols == observers.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            values[row * cols + col] = if_data_exists_return(
              sim.observers,
              observers[row * cols + col],
              [&](auto& obs) noexcept -> real {
                  return obs.linearized_buffer.back().y;
              },
              none_value);
        }
    }
}

} // namespace irt
