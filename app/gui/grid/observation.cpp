// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/helpers.hpp>

#include <fmt/format.h>

namespace irt {

void grid_observation_widget::show(grid_observation_system& grid) noexcept
{
    constexpr ImPlotAxisFlags axes_flags = ImPlotAxisFlags_Lock |
                                           ImPlotAxisFlags_NoGridLines |
                                           ImPlotAxisFlags_NoTickMarks;

    auto& sim_ed = container_of(this, &simulation_editor::grid_obs);
    auto& app    = container_of(&sim_ed, &application::simulation_ed);

    if_data_exists_do(
      app.pj.grid_observers, grid.id, [&](auto& grid_obs) noexcept {
          ImGui::PushID(reinterpret_cast<void*>(&grid));

          grid.update(app.sim);

          ImPlot::PushColormap(grid_obs.color_map);
          if (ImPlot::BeginPlot(grid_obs.name.c_str(),
                                ImVec2(-1, -1),
                                ImPlotFlags_NoLegend |
                                  ImPlotFlags_NoMouseText)) {
              ImPlot::PlotHeatmap(grid_obs.name.c_str(),
                                  grid.values.data(),
                                  grid.rows,
                                  grid.cols,
                                  grid_obs.scale_min,
                                  grid_obs.scale_max);
              ImPlot::EndPlot();
          }
          ImGui::PopID();
      });
}

} // namespace irt
