// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static auto init_or_reuse_observer(simulation&        sim,
                                   model&             mdl,
                                   std::integral auto row,
                                   std::integral auto col) noexcept
  -> observer_id
{
    if (auto* obs = sim.observers.try_to_get(mdl.obs_id); obs) {
        const auto raw_buffer_size = std::max(obs->buffer.ssize(), 16);
        const auto linerized_buffer_size =
          std::max(obs->linearized_buffer.ssize(), 16);
        const auto time_step = std::clamp(
          obs->time_step, std::numeric_limits<float>::epsilon(), 0.01f);

        obs->init(raw_buffer_size, linerized_buffer_size, time_step);
        sim.observe(mdl, *obs);
    } else {
        auto& new_obs = sim.observers.alloc("");
        format(new_obs.name, "{}x{}", row, col);
        new_obs.init(16, 32, 0.01f);
        mdl.obs_id = sim.observers.get_id(new_obs);
        sim.observe(mdl, new_obs);
    }

    return mdl.obs_id;
}

static void build_grid_observer(grid_observer&  grid_obs,
                                project&        pj,
                                simulation&     sim,
                                tree_node&      grid_parent,
                                grid_component& grid_compo) noexcept
{
    const auto* to = pj.tree_nodes.try_to_get(grid_obs.tn_id);

    debug::ensure(pj.tree_nodes.try_to_get(grid_obs.tn_id) != nullptr);

    if (!to)
        return;

    const auto relative_path =
      pj.build_relative_path(grid_parent, *to, grid_obs.mdl_id);

    const auto* child = grid_parent.tree.get_child();
    while (child) {
        if (child->id == grid_obs.compo_id) {
            const auto  tn_mdl = pj.get_model(*child, relative_path);
            const auto* tn     = pj.tree_nodes.try_to_get(tn_mdl.first);
            auto*       mdl    = sim.models.try_to_get(tn_mdl.second);

            if (tn && mdl) {
                const auto w = unpack_doubleword(child->unique_id);
                debug::ensure(std::cmp_less(w.first, grid_compo.row));
                debug::ensure(std::cmp_less(w.second, grid_compo.column));

                const auto index =
                  static_cast<i32>(w.first) * grid_compo.column +
                  static_cast<i32>(w.second);

                debug::ensure(0 <= index);
                debug::ensure(index < grid_obs.observers.ssize());

                grid_obs.observers[index] =
                  init_or_reuse_observer(sim, *mdl, w.first, w.second);
            }
        }

        child = child->tree.get_sibling();
    }
}

void grid_observer::init(project& pj, modeling& mod, simulation& sim) noexcept
{
    observers.clear();
    values.clear();

    if_tree_node_is_grid_do(
      pj, mod, parent_id, [&](auto& tn, auto& compo, auto& g_compo) noexcept {
          debug::ensure(compo.type == component_type::grid);

          const auto len = g_compo.row * g_compo.column;
          rows           = g_compo.row;
          cols           = g_compo.column;

          observers.resize(len);
          values.resize(len);

          std::fill_n(observers.data(), len, undefined<observer_id>());
          std::fill_n(values.data(), len, zero);

          build_grid_observer(*this, pj, sim, tn, g_compo);
      });
}

void grid_observer::clear() noexcept
{
    observers.clear();
    values.clear();
}

void grid_observer::update(const simulation& sim) noexcept
{
    if (rows * cols != observers.ssize() or values.ssize() != observers.ssize())
        return;

    // debug::ensure(rows * cols == observers.ssize());
    // debug::ensure(values.ssize() == observers.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const auto pos = row * cols + col;
            const auto id  = observers[pos];

            if (is_defined(id)) {
                const auto* obs = sim.observers.try_to_get(observers[pos]);

                if (not obs) {
                    observers[pos] = undefined<observer_id>();
                    values[pos]    = zero;
                } else {
                    values[pos] = !obs->linearized_buffer.empty()
                                    ? obs->linearized_buffer.back().y
                                    : zero;
                }
            }
        }
    }
}

} // namespace irt
