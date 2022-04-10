// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static void show_output_widget([[maybe_unused]] component_editor& ed) noexcept
{
    // if (ImGui::BeginTable("Observations", 5)) {
    //     ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
    //     ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
    //     ImGui::TableSetupColumn("time-step",
    //     ImGuiTableColumnFlags_WidthFixed); ImGui::TableSetupColumn("size",
    //     ImGuiTableColumnFlags_WidthFixed);
    //     ImGui::TableSetupColumn("capacity",
    //     ImGuiTableColumnFlags_WidthFixed);

    //     ImGui::TableHeadersRow();
    //     memory_output* out = nullptr;
    //     while (ed.outputs.next(out)) {
    //         ImGui::TableNextRow();
    //         ImGui::TableNextColumn();

    //         ImGui::TextFormat("{}", ordinal(ed.outputs.get_id(*out)));
    //         ImGui::TableNextColumn();
    //         ImGui::TextUnformatted(out->name.c_str());
    //         ImGui::TableNextColumn();
    //         ImGui::TextFormat("{}", out->time_step);
    //         ImGui::TableNextColumn();
    //         ImGui::TextFormat("{}", out->xs.size());
    //         ImGui::TableNextColumn();
    //         ImGui::TextFormat("{}", out->xs.capacity());
    //     }

    //     ImGui::EndTable();
    // }

    // if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen)) {
    //     if (ImPlot::BeginPlot("simulation", "t", "s")) {
    //         ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
    //         ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

    //         memory_output* obs = nullptr;
    //         while (ed.outputs.next(obs)) {
    //             const auto sz = obs->ys.size();

    //             if (sz) {
    //                 if (obs->interpolate) {
    //                     ImPlot::PlotLine(obs->name.c_str(),
    //                                      obs->xs.begin(),
    //                                      obs->ys.begin(),
    //                                      sz);

    //                 } else {
    //                     ImPlot::PlotScatter(obs->name.c_str(),
    //                                         obs->xs.begin(),
    //                                         obs->ys.begin(),
    //                                         sz);
    //                 }
    //             }
    //         }

    //         ImPlot::PopStyleVar(2);
    //         ImPlot::EndPlot();
    //     }
    // }
}

void application::show_output_editor_widget() noexcept
{
    show_output_widget(c_editor);
}

} // namespace irt
