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

enum class p_id {
    in,
    out,
    N,
    S,
    W,
    E,
    NE,
    SE,
    NW,
    SW,
    n4,
    n5,
    n6,
    n44,
    n45,
    n46,
    n54,
    n55,
    n56,
    n64,
    n65,
    n66
};

constexpr static inline std::string_view p_names[] = {
    "in", "out", "N",  "S",  "W",  "E",  "NE", "SE", "NW", "SW", "4",
    "5",  "6",   "44", "45", "46", "54", "55", "56", "64", "65", "66"
};

constexpr static int compute_grid_connections_size(
  const grid_component& grid) noexcept
{
    const int children = grid.cells_number();

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

    // Replace 8x with the number of input/output connections for each children
    // components.

    return 8 * children * children_mult + grid.column() * col_mult +
           grid.row() * row_mult;
}

static bool can_alloc_grid_children_and_connections(
  grid_component& grid) noexcept
{
    const auto children    = grid.cells_number();
    const auto connections = compute_grid_connections_size(grid);

    grid.cache.reserve(children);
    grid.cache_connections.reserve(connections);

    return grid.cache.can_alloc(children) and
           grid.cache_connections.can_alloc(connections);
}

static auto build_grid_children(modeling& mod, grid_component& grid) noexcept
  -> vector<child_id>
{
    const auto children_nb = grid.cells_number();

    vector<child_id> ret;

    ret.resize(children_nb, undefined<child_id>());
    grid.cache.reserve(children_nb);
    grid.cache_names.resize(children_nb);

    for (int row = 0; row < grid.row(); ++row) {
        for (int col = 0; col < grid.column(); ++col) {
            const auto index    = grid.pos(row, col);
            const auto compo_id = grid.children()[index];

            if (mod.components.exists(compo_id)) {
                auto& ch             = grid.cache.alloc(compo_id, row, col);
                auto  id             = grid.cache.get_id(ch);
                grid.cache_names[id] = grid.make_unique_name_id(row, col);
                ret[index]           = id;
            }
        }
    }

    return ret;
}

struct split_name {
    split_name(std::string_view str) noexcept
    {
        const auto pos = str.find_last_of('_');
        if (pos != std::string_view::npos and
            std::cmp_less(pos + 1, str.size())) {
            left  = str.substr(0, pos);
            right = str.substr(pos + 1, std::string::npos);
        } else {
            left = str;
        }
    }

    std::string_view left;
    std::string_view right;
};

static void connection_add(modeling&        mod,
                           grid_component&  grid,
                           child_id         src,
                           std::string_view port_src,
                           child_id         dst,
                           std::string_view port_dst) noexcept
{
    auto* child_src = grid.cache.try_to_get(src);
    auto* child_dst = grid.cache.try_to_get(dst);

    debug::ensure(child_src and child_dst);

    if (not child_src or not child_dst)
        return;

    auto* compo_src = mod.components.try_to_get<component>(child_src->compo_id);
    auto* compo_dst = mod.components.try_to_get<component>(child_dst->compo_id);
    if (not compo_src or not compo_dst)
        return;

    compo_src->y.for_each<port_str>(
      [&](const auto sid, const auto& sname) noexcept {
          split_name p_src(sname.sv());

          if (port_src == p_src.left) {
              compo_dst->x.for_each<port_str>(
                [&](const auto did, const auto dname) noexcept {
                    split_name p_dst(dname.sv());

                    if (port_dst == p_dst.left) {
                        if (p_src.right == p_dst.right)
                            grid.cache_connections.alloc(src, sid, dst, did);
                    }
                });
          }
      });
}

static void build_grid_connections(modeling&               mod,
                                   grid_component&         grid,
                                   const vector<child_id>& ids,
                                   const int               row,
                                   const int               col) noexcept
{
    struct destination {
        int  r; // row index [0..grid.row[
        int  c; // column index [0..grid.column[
        p_id p; // input port identifier.
    };

    std::array<p_id, 8>        srcs;
    std::array<destination, 8> dests;
    std::array<bool, 8>        valids;

    switch (grid.out_connection_type) {
    case grid_component::type::in_out:
        srcs.fill(p_id::out);
        break;

    case grid_component::type::name:
        srcs[0] = { p_id::NE };
        srcs[1] = { p_id::NW };
        srcs[2] = { p_id::SE };
        srcs[3] = { p_id::SW };
        srcs[4] = { p_id::N };
        srcs[5] = { p_id::S };
        srcs[6] = { p_id::E };
        srcs[7] = { p_id::W };
        break;

    case grid_component::type::number:
        srcs[0] = { p_id::n44 };
        srcs[1] = { p_id::n46 };
        srcs[2] = { p_id::n64 };
        srcs[3] = { p_id::n66 };
        srcs[4] = { p_id::n45 };
        srcs[5] = { p_id::n54 };
        srcs[6] = { p_id::n56 };
        srcs[7] = { p_id::n65 };
        break;
    }

    switch (grid.in_connection_type) {
    case grid_component::type::in_out:
        dests[0] = { row - 1, col - 1, p_id::in };
        dests[1] = { row - 1, col + 1, p_id::in };
        dests[2] = { row + 1, col - 1, p_id::in };
        dests[3] = { row + 1, col + 1, p_id::in };
        dests[4] = { row - 1, col, p_id::in };
        dests[5] = { row + 1, col, p_id::in };
        dests[6] = { row, col - 1, p_id::in };
        dests[7] = { row, col + 1, p_id::in };
        break;

    case grid_component::type::name:
        dests[0] = { row - 1, col - 1, p_id::SW };
        dests[1] = { row - 1, col + 1, p_id::SE };
        dests[2] = { row + 1, col - 1, p_id::NW };
        dests[3] = { row + 1, col + 1, p_id::NE };
        dests[4] = { row - 1, col, p_id::S };
        dests[5] = { row + 1, col, p_id::N };
        dests[6] = { row, col - 1, p_id::W };
        dests[7] = { row, col + 1, p_id::E };
        break;

    case grid_component::type::number:
        dests[0] = { row - 1, col - 1, p_id::n66 };
        dests[1] = { row + 1, col - 1, p_id::n64 };
        dests[2] = { row + 1, col, p_id::n46 };
        dests[3] = { row, col + 1, p_id::n44 };
        dests[4] = { row - 1, col + 1, p_id::n65 };
        dests[5] = { row + 1, col + 1, p_id::n56 };
        dests[6] = { row - 1, col, p_id::n54 };
        dests[7] = { row, col - 1, p_id::n45 };
        break;
    }

    if (grid.neighbors == grid_component::neighborhood::eight) {
        std::fill_n(valids.begin(), 8u, true);
    } else {
        std::fill_n(valids.begin(), 4, false);
        std::fill_n(valids.begin() + 4, 4, true);
    }

    if (any_equal(grid.opts,
                  grid_component::options::column_cylinder,
                  grid_component::options::torus)) {
        for (std::size_t i = 0; i < valids.size(); ++i) {
            if (valids[i]) {
                if (dests[i].c < 0)
                    dests[i].c = grid.column() - 1;
                else if (dests[i].c >= grid.column())
                    dests[i].c = 0;
            }
        }
    } else {
        for (std::size_t i = 0; i < valids.size(); ++i)
            if (valids[i])
                valids[i] = 0 <= dests[i].c and dests[i].c < grid.column();
    }

    if (any_equal(grid.opts,
                  grid_component::options::row_cylinder,
                  grid_component::options::torus)) {
        for (std::size_t i = 0; i < valids.size(); ++i) {
            if (valids[i]) {
                if (dests[i].r < 0)
                    dests[i].r = grid.row() - 1;
                else if (dests[i].r >= grid.row())
                    dests[i].r = 0;
            }
        }
    } else {
        for (std::size_t i = 0; i < valids.size(); ++i)
            if (valids[i])
                valids[i] = 0 <= dests[i].r and dests[i].r < grid.row();
    }

    if (const auto c_src = ids[grid.pos(row, col)]; is_defined(c_src)) {
        for (std::size_t i = 0; i < valids.size(); ++i) {
            if (valids[i]) {
                debug::ensure(0 <= dests[i].r and dests[i].r < grid.row());
                debug::ensure(0 <= dests[i].c and dests[i].c < grid.column());

                if (const auto c_dst = ids[grid.pos(dests[i].r, dests[i].c)];
                    is_defined(c_dst)) {

                    connection_add(mod,
                                   grid,
                                   c_src,
                                   p_names[ordinal(srcs[i])],
                                   c_dst,
                                   p_names[ordinal(dests[i].p)]);
                }
            }
        }
    }
}

static void build_grid_connections(modeling&               mod,
                                   grid_component&         grid,
                                   const vector<child_id>& ids) noexcept
{
    for (int row = 0; row < grid.row(); ++row)
        for (int col = 0; col < grid.column(); ++col)
            build_grid_connections(mod, grid, ids, row, col);
}

status modeling::copy(grid_component& grid, generic_component& s) noexcept
{
    irt_check(grid.build_cache(*this));

    return s.import(grid);
}

name_str grid_component::make_unique_name_id(int row, int col) const noexcept
{
    debug::ensure(is_coord_valid(row, col));

    name_str ret;
    format(ret, "{},{}", row, col);

    return ret;
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

expected<input_connection_id> grid_component::connect_input(
  const port_id x,
  const i32     row,
  const i32     col,
  const port_id id) noexcept
{
    for (const auto& con : input_connections)
        if (x == con.x and row == con.row and col == con.col and id == con.id)
            return input_connections.get_id(con);

    if (not input_connections.can_alloc()) {
        const auto capacity = input_connections.capacity();
        const auto request  = capacity == 0 ? 8 : capacity * 2;

        input_connections.reserve(request);

        if (input_connections.capacity() == capacity)
            return new_error(
              modeling_errc::generic_input_connection_container_full);
    }

    return input_connections.get_id(input_connections.alloc(x, row, col, id));
}

expected<output_connection_id> grid_component::connect_output(
  const port_id y,
  const i32     row,
  const i32     col,
  const port_id id) noexcept
{
    for (const auto& con : output_connections)
        if (y == con.y and row == con.row and col == con.col and id == con.id)
            return output_connections.get_id(con);

    if (not output_connections.can_alloc()) {
        const auto capacity = output_connections.capacity();
        const auto request  = capacity == 0 ? 8 : capacity * 2;

        output_connections.reserve(request);

        if (output_connections.capacity() == capacity)
            return new_error(
              modeling_errc::generic_output_connection_container_full);
    }

    return output_connections.get_id(output_connections.alloc(y, row, col, id));
}

void grid_component::clear_cache() noexcept
{
    cache.clear();
    cache_connections.clear();
}

status grid_component::build_cache(modeling& mod) noexcept
{
    clear_cache();

    if (not can_alloc_grid_children_and_connections(*this))
        return new_error(modeling_errc::generic_children_container_full);

    const auto vec = build_grid_children(mod, *this);
    build_grid_connections(mod, *this, vec);

    return success();
}

} // namespace irt
