// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/helpers.hpp>

namespace irt {

void grid_observation_widget::show(application&             app,
                                   grid_observation_system& grid) noexcept
{

    if_data_exists_do(
      app.pj.grid_observers, grid.id, [&](auto& grid_obs) noexcept {
          ImGui::PushID(reinterpret_cast<void*>(&grid));

          if (ImPlot::BeginPlot(grid_obs.name.c_str(), ImVec2(-1, -1))) {
              ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
              ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

              ImPlot::PlotHeatmap(grid_obs.name.c_str(),
                                  grid.values.data(),
                                  grid.rows,
                                  grid.cols);

              ImPlot::PopStyleVar(2);
              ImPlot::EndPlot();
          }
          ImGui::PopID();
      });
}

} // namespace irt
