// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

void memory_observation_widget::show(memory_observer& mem,
                                     const project&   pj,
                                     const ImVec2&    size) noexcept
{
    ImGui::PushID(reinterpret_cast<void*>(&mem));

    if (ImGui::BeginChild("memory", size)) {
        mem.update(pj);

        const auto span = mem.get_values();

        if (span.size() == static_cast<sz>(mem.rows * mem.cols)) {
            ImPlot::PushColormap(mem.color_map);
            if (ImPlot::BeginPlot(mem.name.c_str(), ImVec2(-1, -1),
                                  ImPlotFlags_NoLegend |
                                    ImPlotFlags_NoMouseText |
                                    ImPlotFlags_Equal)) {
                ImPlot::SetupAxes(nullptr, nullptr,
                                  ImPlotAxisFlags_NoDecorations,
                                  ImPlotAxisFlags_NoDecorations);
                ImPlot::PlotHeatmap(mem.name.c_str(), span.data(), mem.rows,
                                    mem.cols, mem.scale_min, mem.scale_max,
                                    nullptr);
                ImPlot::EndPlot();
            }
            ImPlot::PopColormap();
        } else {
            ImGui::TextUnformatted("Starts the simulation to draw the memory.");
        }
    }

    ImGui::EndChild();
    ImGui::PopID();
}

void grid_observation_widget::show(grid_observer& grid,
                                   const project& pj,
                                   const ImVec2&  size) noexcept
{
    ImGui::PushID(reinterpret_cast<void*>(&grid));

    if (ImGui::BeginChild("grid")) {
        grid.update(pj);

        auto span = grid.get_values();

        if (not span.empty()) {
            ImPlot::PushColormap(grid.color_map);
            if (ImPlot::BeginPlot(grid.name.c_str(),
                                  size,
                                  ImPlotFlags_NoLegend |
                                    ImPlotFlags_NoMouseText)) {
                ImPlot::PlotHeatmap(grid.name.c_str(),
                                    span.data(),
                                    grid.rows,
                                    grid.cols,
                                    grid.scale_min,
                                    grid.scale_max);
                ImPlot::EndPlot();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopID();
}

} // namespace irt
