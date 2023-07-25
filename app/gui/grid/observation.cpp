// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

namespace irt {

static observer_id get_observer_id(application&         app,
                                   const tree_node&     tn,
                                   const std::span<u64> unique_ids,
                                   const model_id       mdl_id) noexcept
{
    const tree_node* ptr = &tn;

    for (const auto unique_id : unique_ids) {
        if (auto tree_node_id_opt = ptr->get_tree_node_id(unique_id);
            tree_node_id_opt.has_value()) {
            ptr = app.pj.tree_nodes.try_to_get(*tree_node_id_opt);
        }
    }

    if (auto mdl_id_opt = ptr->get_model_id(mdl_id); mdl_id_opt.has_value())
        if (auto* mdl = app.sim.models.try_to_get(*mdl_id_opt); mdl)
            return mdl->obs_id;

    return undefined<observer_id>();
}

static status grid_observation_widget_init(grid_observation_widget& grid_widget,
                                           application&             app,
                                           grid_observer&           grid,
                                           grid_component&          compo,
                                           tree_node& grid_parent) noexcept
{
    return if_data_exists_return(
      app.pj.tree_nodes,
      grid.child.tn_id,
      [&](auto& tn) noexcept {
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

                      grid_widget.observers[row * compo.column + col] =
                        get_observer_id(
                          app,
                          *child,
                          std::span(stack.begin(), stack.end() - 1),
                          grid.child.mdl_id);
                  }
              } while (child);
          }

          return status::success;
      },
      status::unknown_dynamics);
}

status grid_observation_widget::init(application&   app,
                                     grid_observer& grid) noexcept
{
    return if_data_exists_return(
      app.pj.tree_nodes,
      grid.child.parent_id,
      [&](auto& grid_tn) noexcept {
          return if_data_exists_return(
            app.mod.components,
            grid_tn.id,
            [&](auto& compo) noexcept {
                irt_assert(compo.type == component_type::grid);

                return if_data_exists_return(
                  app.mod.grid_components,
                  compo.id.grid_id,
                  [&](auto& grid_compo) noexcept {
                      const auto len = grid_compo.row * grid_compo.column;

                      observers.resize(len);
                      values.resize(len);

                      std::fill_n(
                        observers.data(), len, undefined<observer_id>());
                      std::fill_n(values.data(), len, none_value);

                      id = app.pj.grid_observers.get_id(grid);

                      return grid_observation_widget_init(
                        *this, app, grid, grid_compo, grid_tn);
                  },
                  status::unknown_dynamics); // unknown grid-compo
            },
            status::unknown_dynamics); // unknown compo
      },
      status::unknown_dynamics); // unknown_grid
}

void grid_observation_widget::resize(int rows_, int cols_) noexcept
{
    const auto len = rows_ * cols_;
    rows           = rows_;
    cols           = cols_;

    irt_assert(len > 0);

    observers.resize(len);
    values.resize(len);
    clear();
}

void grid_observation_widget::clear() noexcept
{
    std::fill_n(observers.data(), observers.size(), undefined<observer_id>());
    std::fill_n(values.data(), values.size(), none_value);
}

void grid_observation_widget::update(application& app) noexcept
{
    irt_assert(rows * cols == observers.ssize());

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            values[row * cols + col] = if_data_exists_return(
              app.sim.observers,
              observers[row * cols + col],
              [&](auto& obs) noexcept -> real {
                  return obs.linearized_buffer.back().y;
              },
              none_value);
        }
    }
}

void grid_observation_widget::show(application& app) noexcept
{
    if_data_exists_do(app.pj.grid_observers, id, [&](auto& grid_obs) noexcept {
        ImGui::PushID(this);
        if (ImPlot::BeginPlot(grid_obs.name.c_str(), ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::PlotHeatmap(
              grid_obs.name.c_str(), values.data(), rows, cols);

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
        ImGui::PopID();
    });
}

}