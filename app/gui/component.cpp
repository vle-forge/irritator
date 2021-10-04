// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <fmt/format.h>

namespace irt {

void component_editor::show(bool* /*is_show*/) noexcept
{
    // ImGuiWindowFlags flag = 0;
    ImGuiWindowFlags flag =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;

    auto*       viewport   = ImGui::GetMainViewport();
    auto        region     = viewport->WorkSize;
    const float width_1_10 = region.x / 10.f;

    ImVec2 current_component_size(width_1_10 * 2.f, region.y);
    ImVec2 drawing_zone_size(width_1_10 * 6.f, region.y);
    ImVec2 component_list_size(width_1_10 * 2.f, region.y);

    ImVec2 current_component_pos(0.f, viewport->WorkPos.y);
    ImVec2 drawing_zone_pos(current_component_size.x, viewport->WorkPos.y);
    ImVec2 component_list_pos(current_component_size.x + drawing_zone_size.x,
                              viewport->WorkPos.y);

    ImGui::SetNextWindowPos(current_component_pos);
    ImGui::SetNextWindowSize(current_component_size);
    if (ImGui::Begin("Modeling compoent", 0, flag)) {
        const ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::CollapsingHeader("Hierarchy", flags)) {
            ImGui::Text("first");
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(drawing_zone_pos);
    ImGui::SetNextWindowSize(drawing_zone_size);
    if (ImGui::Begin("Component editor", 0, flag)) {
        ImGui::Text("drawing_zone_pos");
        ImGui::Text("drawing_zone_pos");
        ImGui::Text("drawing_zone_pos");
    }
    ImGui::End();

    ImGui::SetNextWindowPos(component_list_pos);
    ImGui::SetNextWindowSize(component_list_size);
    if (ImGui::Begin("Components list", 0, flag)) {
        const ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::CollapsingHeader("Components", flags)) {
            if (ImGui::TreeNodeEx("Internal", ImGuiTreeNodeFlags_DefaultOpen)) {
                component* compo = nullptr;
                while (mod.components.next(compo))
                    if (compo->type != component_type::file)
                        ImGui::Text(compo->name.c_str());

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("File", ImGuiTreeNodeFlags_DefaultOpen)) {
                component* compo = nullptr;
                while (mod.components.next(compo))
                    if (compo->type == component_type::file)
                        ImGui::Text(compo->name.c_str());

                ImGui::TreePop();
            }
        }
        ImGui::End();
    }
}

}
