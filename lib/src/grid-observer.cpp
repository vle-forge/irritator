// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <charconv>

#include <fmt/format.h>

namespace irt {

static auto init_or_reuse_observer(project&       pj,
                                   model&         mdl,
                                   const fraction timestep) noexcept
  -> observer_id
{
    if (auto* obs = pj.sim.observers.try_to_get(mdl.obs_id)) {
        obs->reset();
    } else {
        if (pj.sim.observe(mdl, timestep.to_double()).has_error()) {
            // @todo Handle error
        }
    }

    return mdl.obs_id;
}

std::optional<std::pair<int, int>> get_row_column(
  const std::string_view str) noexcept
{
    int row = 0, col = 0;

    const auto begin = str.data();
    const auto end   = str.data() + str.size();

    if (const auto ret_1 = std::from_chars(begin, end, row);
        ret_1.ec == std::errc{} and ret_1.ptr != str.data() + str.size()) {
        const auto begin_2 = ret_1.ptr + 1;

        if (const auto ret_2 = std::from_chars(begin_2, end, col);
            ret_2.ec == std ::errc{}) {
            return std::make_pair(row, col);
        }
    }

    return std::nullopt;
}

static void build_grid_observer(grid_observer&   grid_obs,
                                project&         pj,
                                journal_handler& jn,
                                simulation&      sim,
                                tree_node&       grid_parent,
                                grid_component&  grid_compo) noexcept
{
    const auto* to = pj.tree_nodes.try_to_get(grid_obs.tn_id);
    if (not to)
        return;

    const auto relative_path =
      pj.build_relative_path(grid_parent, *to, grid_obs.mdl_id);

    const auto* child = grid_parent.tree.get_child();
    while (child) {
        if (child->id == grid_obs.compo_id) {
            const auto  tn_mdl = pj.get_model(*child, relative_path);
            const auto* tn     = pj.tree_nodes.try_to_get(tn_mdl.first);
            auto*       mdl    = pj.sim.models.try_to_get(tn_mdl.second);

            if (not(tn and mdl))
                continue;

            if (const auto w_opt = get_row_column(child->unique_id.sv());
                w_opt.has_value()) {
                const auto& w = *w_opt;
                debug::ensure(grid_compo.is_coord_valid(w.first, w.second));
                const auto index = grid_compo.pos(w.first, w.second);

                debug::ensure(0 <= index);
                debug::ensure(std::cmp_less(index, grid_obs.observers.size()));

                grid_obs.observers[index] =
                  init_or_reuse_observer(pj, *mdl, grid_obs.timestep);
            } else {
                jn.push(log_level::warning, [&](auto& t, auto& m) noexcept {
                    t = "Grid observer error";
                    format(
                      m, "unique_id {} is not found", child->unique_id.sv());
                });
            }
        }

        child = child->tree.get_sibling();
    }
}

void grid_observer::init(project&         pj,
                         modeling&        mod,
                         simulation&      sim,
                         journal_handler& jn) noexcept
{
    observers.clear();
    values.clear();

    if (auto* tn = pj.tree_nodes.try_to_get(parent_id)) {
        mod.ids.read([&](const auto& ids, auto) noexcept {
            if (ids.exists(tn->id)) {
                if (ids.components[tn->id].type == component_type::grid) {
                    if (auto* grid = ids.grid_components.try_to_get(
                          ids.components[tn->id].id.grid_id)) {

                        const auto len = grid->cells_number();

                        observers.resize(len);
                        std::fill_n(
                          observers.data(), len, undefined<observer_id>());

                        values.resize(len, zero);

                        rows = grid->row();
                        cols = grid->column();

                        build_grid_observer(*this, pj, jn, sim, *tn, *grid);
                    }
                }
            }
        });
    }
}

void grid_observer::clear() noexcept
{
    observers.clear();
    values.clear();
}

void grid_observer::update(const project& pj) noexcept
{
    const auto len        = static_cast<std::size_t>(rows * cols);
    const auto obs_len    = observers.size();
    const auto values_len = values.size();

    if (debug::check(len == obs_len and len == values_len)) {
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const auto  pos    = col * rows + row;
                const auto  obs_id = observers[pos];
                const auto* obs    = pj.sim.observers.try_to_get(obs_id);

                if (obs) {
                    obs->read_history([&](const auto& h, const auto) {
                        if (not h.empty())
                            values[pos] = h.back().value;
                    });
                }
            }
        }
    }
}

} // namespace irt
