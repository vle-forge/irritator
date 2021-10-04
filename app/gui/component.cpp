// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <fmt/format.h>

namespace irt {

static void show_all_components(modeling& mod)
{
    constexpr ImGuiTreeNodeFlags flags =
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
}

// @todo : remove and move to public header name
static inline const char* dynamics_type_names[] = {
    "qss1_integrator", "qss1_multiplier", "qss1_cross",
    "qss1_power",      "qss1_square",     "qss1_sum_2",
    "qss1_sum_3",      "qss1_sum_4",      "qss1_wsum_2",
    "qss1_wsum_3",     "qss1_wsum_4",     "qss2_integrator",
    "qss2_multiplier", "qss2_cross",      "qss2_power",
    "qss2_square",     "qss2_sum_2",      "qss2_sum_3",
    "qss2_sum_4",      "qss2_wsum_2",     "qss2_wsum_3",
    "qss2_wsum_4",     "qss3_integrator", "qss3_multiplier",
    "qss3_cross",      "qss3_power",      "qss3_square",
    "qss3_sum_2",      "qss3_sum_3",      "qss3_sum_4",
    "qss3_wsum_2",     "qss3_wsum_3",     "qss3_wsum_4",
    "integrator",      "quantifier",      "adder_2",
    "adder_3",         "adder_4",         "mult_2",
    "mult_3",          "mult_4",          "counter",
    "queue",           "dynamic_queue",   "priority_queue",
    "generator",       "constant",        "cross",
    "time_func",       "accumulator_2",   "flow"
};

static void show_component_hierarchy_model(component_editor& ed,
                                           component&        c,
                                           int               i)
{
    auto  id  = enum_cast<model_id>(c.children[i].id);
    auto* mdl = ed.mod.models.try_to_get(id);

    if (!mdl) {
        c.children.swap_pop_back(i);
        return;
    }

    ImGui::Text("%d (%s)", i, dynamics_type_names[ordinal(mdl->type)]);
}

static void show_component_hierarchy_component(component_editor& ed,
                                               component&        c,
                                               int               i)
{
    auto  id    = enum_cast<component_ref_id>(c.children[i].id);
    auto* c_ref = ed.mod.component_refs.try_to_get(id);

    if (!c_ref) {
        c.children.swap_pop_back(i);
        return;
    }

    auto* compo = ed.mod.components.try_to_get(c_ref->id);
    if (!compo) {
        c.children.swap_pop_back(i);
        return;
    }

    if (ImGui::TreeNodeEx(compo->name.c_str())) {
        for (int j = 0, e = compo->children.ssize(); j != e; ++j) {
            if (compo->children[j].type == child_type::model)
                show_component_hierarchy_model(ed, *compo, j);
            else
                show_component_hierarchy_component(ed, *compo, j);
        }

        ImGui::TreePop();
    }
}

static void show_component_hierarchy(component_editor& ed)
{
    if (auto* compo = ed.mod.components.try_to_get(ed.mod.head); compo) {
        if (ImGui::TreeNodeEx(compo->name.c_str(),
                              ImGuiTreeNodeFlags_DefaultOpen)) {

            for (int i = 0, e = compo->children.ssize(); i != e; ++i) {
                if (compo->children[i].type == child_type::model) {
                    show_component_hierarchy_model(ed, *compo, i);
                } else {
                    show_component_hierarchy_component(ed, *compo, i);
                }
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("No component load");
    }
}

static void show_opened_component(component_editor& ed)
{
    if (auto* ref = ed.mod.components.try_to_get(ed.mod.head); ref) {

    } else {
        ImGui::Text("No component load");
    }
}

void component_editor::show(bool* /*is_show*/) noexcept
{
    constexpr ImGuiWindowFlags flag =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;

    const auto* viewport   = ImGui::GetMainViewport();
    const auto  region     = viewport->WorkSize;
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
    if (ImGui::Begin("Modeling component", 0, flag)) {
        show_component_hierarchy(*this);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(drawing_zone_pos);
    ImGui::SetNextWindowSize(drawing_zone_size);
    if (ImGui::Begin("Component editor", 0, flag)) {
        show_opened_component(*this);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(component_list_pos);
    ImGui::SetNextWindowSize(component_list_size);
    if (ImGui::Begin("Components list", 0, flag)) {
        show_all_components(mod);
        ImGui::End();
    }
}
}
