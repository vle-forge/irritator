// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

void grid_observation_widget::show(grid_observer& grid,
                                   const ImVec2&  size) noexcept
{
    ImGui::PushID(reinterpret_cast<void*>(&grid));

    if (not grid.values.empty()) {
        if (std::unique_lock lock(grid.mutex, std::try_to_lock);
            lock.owns_lock()) {
            ImPlot::PushColormap(grid.color_map);
            if (ImPlot::BeginPlot(grid.name.c_str(),
                                  size,
                                  ImPlotFlags_NoLegend |
                                    ImPlotFlags_NoMouseText)) {
                ImPlot::PlotHeatmap(grid.name.c_str(),
                                    grid.values.data(),
                                    grid.rows,
                                    grid.cols,
                                    grid.scale_min,
                                    grid.scale_max);
                ImPlot::EndPlot();
            }
        }
    }

    ImGui::PopID();
}

} // namespace irt
