// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

void grid_observation_widget::show(grid_observer& grid) noexcept
{
    auto& sim_ed = container_of(this, &simulation_editor::grid_obs);
    auto& app    = container_of(&sim_ed, &application::simulation_ed);

    ImGui::PushID(reinterpret_cast<void*>(&grid));

    grid.update(app.sim);

    if (not grid.values.empty()) {
        ImPlot::PushColormap(grid.color_map);
        if (ImPlot::BeginPlot(grid.name.c_str(),
                              ImVec2(-1, -1),
                              ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
            ImPlot::PlotHeatmap(grid.name.c_str(),
                                grid.values.data(),
                                grid.rows,
                                grid.cols,
                                grid.scale_min,
                                grid.scale_max);
            ImPlot::EndPlot();
        }
    }

    ImGui::PopID();
}

} // namespace irt
