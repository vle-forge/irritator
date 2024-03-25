// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static status build_grid(grid_simulation_observer& grid_system,
                         project&                  pj,
                         simulation&               sim,
                         tree_node&                grid_parent,
                         grid_component&           grid_compo,
                         grid_observer&            grid_obs) noexcept
{
    irt_assert(pj.tree_nodes.try_to_get(grid_obs.tn_id) != nullptr);

    const auto* to = pj.tree_nodes.try_to_get(grid_obs.tn_id);

    if (!to)
        return new_error(project::part::grid_observers, unknown_error{});

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
                irt_assert(w.first < static_cast<u32>(grid_compo.row));
                irt_assert(w.second < static_cast<u32>(grid_compo.column));

                const auto index =
                  static_cast<i32>(w.first) * grid_compo.column +
                  static_cast<i32>(w.second);

                irt_assert(0 <= index);
                irt_assert(index < grid_system.observers.ssize());

                auto&      obs    = sim.observers.alloc("tmp");
                const auto obs_id = sim.observers.get_id(obs);

                sim.observe(*mdl, obs);

                grid_system.observers[index] = obs_id;
            }
        }

        child = child->tree.get_sibling();
    }

    return success();
}

status grid_simulation_observer::init(project&       pj,
                                      modeling&      mod,
                                      simulation&    sim,
                                      grid_observer& grid_obs) noexcept
{
    return if_tree_node_is_grid_do(
      pj,
      mod,
      grid_obs.parent_id,
      [&](auto& grid_parent_tn, auto& compo, auto& grid) -> status {
          irt_assert(compo.type == component_type::grid);

          const auto len = grid.row * grid.column;
          rows           = grid.row;
          cols           = grid.column;

          observers.resize(len);
          values.resize(len);

          std::fill_n(observers.data(), len, undefined<observer_id>());
          std::fill_n(values.data(), len, none_value);

          id = pj.grid_observers.get_id(grid_obs);

          return build_grid(*this, pj, sim, grid_parent_tn, grid, grid_obs);
      });
}

void grid_simulation_observer::resize(int rows_, int cols_) noexcept
{
    const auto len = rows_ * cols_;
    rows           = rows_;
    cols           = cols_;

    irt_assert(len > 0);

    observers.resize(len);
    values.resize(len);
    clear();
}

void grid_simulation_observer::clear() noexcept
{
    observers.clear();
    values.clear();
}

void grid_simulation_observer::update(simulation& sim) noexcept
{
    irt_assert(rows * cols == observers.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const auto  id  = observers[row * cols + col];
            const auto* obs = sim.observers.try_to_get(id);

            values[row * cols + col] = obs && !obs->linearized_buffer.empty()
                                         ? obs->linearized_buffer.back().y
                                         : none_value;
        }
    }
}

} // namespace irt