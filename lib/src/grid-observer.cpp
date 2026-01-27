// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <charconv>

#include <fmt/format.h>

namespace irt {

static auto init_or_reuse_observer(simulation& sim,
                                   model&      mdl,
                                   std::integral auto /*row*/,
                                   std::integral auto /*col*/) noexcept
  -> observer_id
{
    if (auto* obs = sim.observers.try_to_get(mdl.obs_id); obs) {
        obs->init(observer::buffer_size_t(16),
                  observer::linearized_buffer_size_t(32),
                  0.01f);

        mdl.obs_id = sim.observers.get_id(*obs);
        sim.observe(mdl, *obs);
    } else {
        auto& new_obs = sim.observers.alloc();
        new_obs.init(observer::buffer_size_t(16),
                     observer::linearized_buffer_size_t(32),
                     0.01f);
        mdl.obs_id = sim.observers.get_id(new_obs);
        sim.observe(mdl, new_obs);
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

static void build_grid_observer(grid_observer&  grid_obs,
                                project&        pj,
                                modeling&       mod,
                                simulation&     sim,
                                tree_node&      grid_parent,
                                grid_component& grid_compo) noexcept
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
            auto*       mdl    = sim.models.try_to_get(tn_mdl.second);

            if (tn and mdl) {
                if (const auto w_opt = get_row_column(child->unique_id.sv());
                    w_opt.has_value()) {
                    const auto& w = *w_opt;
                    debug::ensure(grid_compo.is_coord_valid(w.first, w.second));
                    const auto index = grid_compo.pos(w.first, w.second);

                    debug::ensure(0 <= index);
                    debug::ensure(index < grid_obs.observers.ssize());

                    grid_obs.observers[index] =
                      init_or_reuse_observer(sim, *mdl, w.first, w.second);
                } else {
                    mod.journal.push(log_level::warning,
                                     [&](auto& t, auto& m) noexcept {
                                         t = "Grid observer error";
                                         format(m,
                                                "unique_id {} is not found",
                                                child->unique_id.sv());
                                     });
                }
            }
        }

        child = child->tree.get_sibling();
    }
}

void grid_observer::init(project& pj, modeling& mod, simulation& sim) noexcept
{
    observers.clear();

    values.write([&](auto& v) noexcept {
        v.clear();

        if (auto* tn = pj.tree_nodes.try_to_get(parent_id); tn) {
            if (auto* compo = mod.components.try_to_get<component>(tn->id);
                compo and compo->type == component_type::grid) {
                if (auto* grid =
                      mod.grid_components.try_to_get(compo->id.grid_id);
                    grid) {

                    const auto len = grid->cells_number();

                    observers.resize(len);
                    std::fill_n(
                      observers.data(), len, undefined<observer_id>());

                    v.resize(len, zero);

                    rows = grid->row();
                    cols = grid->column();

                    build_grid_observer(*this, pj, mod, sim, *tn, *grid);
                }
            }
        }
    });

    tn = sim.current_time() + static_cast<time>(time_step);
}

void grid_observer::clear() noexcept
{
    observers.clear();

    values.write([](auto& v) noexcept { v.clear(); });

    tn = 0;
}

void grid_observer::update(const simulation& sim) noexcept
{
    values.write([&](auto& v) noexcept {
        if (rows * cols != observers.ssize() or v.ssize() != observers.ssize())
            return;

        std::fill_n(v.data(), v.capacity(), 0.0);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const auto pos = col * rows + row;
                const auto id  = observers[pos];

                if (is_undefined(id))
                    continue;

                if (const auto* obs =
                      sim.observers.try_to_get(observers[pos])) {
                    if (obs->states[observer_flags::use_linear_buffer]) {
                        obs->linearized_buffer.read(
                          [](const auto& buf, const auto /*version*/, auto& v) {
                              v = not buf.empty() ? buf.back().y : zero;
                          },
                          v[pos]);

                    } else {
                        obs->buffer.read(
                          [](const auto& buf, const auto /*version*/, auto& v) {
                              v = not buf.empty() ? buf.back()[1] : zero;
                          },
                          v[pos]);
                    }
                }
            }
        }

        tn = sim.current_time() + static_cast<time>(time_step);
    });
}

} // namespace irt
