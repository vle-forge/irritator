// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <fmt/format.h>

namespace irt {

static ImVec4 operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

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

    ImGui::Text("%d (%s)", i, get_dynamics_type_name(mdl->type));
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

template<typename Dynamics>
static void add_input_attribute(const Dynamics& dyn, int mdl_index) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        irt_assert(length(dyn.x) < 8);

        for (int i = 0, e = length(dyn.x); i != e; ++i) {
            ImNodes::BeginInputAttribute(make_input_node_id(mdl_index, i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(const Dynamics& dyn, int mdl_index) noexcept
{
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();

        irt_assert(length(dyn.y) < 8);

        for (int i = 0, e = length(dyn.y); i != e; ++i) {
            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_index, i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

static void show(component_editor& ed, model& mdl, int mdl_index)
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_model_color);

    ImNodes::BeginNode(mdl_index);
    ImNodes::BeginNodeTitleBar();

    ImGui::TextFormat("{}\n{}", mdl_index, get_dynamics_type_name(mdl.type));

    ImNodes::EndNodeTitleBar();

    dispatch(mdl, [&ed, mdl_index](auto& dyn) {
        add_input_attribute(dyn, mdl_index);
        ImGui::PushItemWidth(120.0f);
        show_dynamics_inputs(ed.mod.srcs, dyn);
        ImGui::PopItemWidth();
        add_output_attribute(dyn, mdl_index);
    });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show(component_editor& ed, component& compo, int mdl_index)
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_component_color);

    ImNodes::BeginNode(mdl_index);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", mdl_index, compo.name.c_str());
    ImNodes::EndNodeTitleBar();

    for (int i = 0, e = length(compo.x); i != e; ++i) {
        ImNodes::BeginInputAttribute(make_input_node_id(mdl_index, i),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    for (int i = 0, e = length(compo.y); i != e; ++i) {
        ImNodes::BeginInputAttribute(make_input_node_id(mdl_index, i),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_opened_component_top(component_editor& ed,
                                      component&        head) noexcept
{
    for (int i = 0, e = head.children.ssize(); i != e; ++i) {
        if (head.children[i].type == child_type::model) {
            auto id = enum_cast<model_id>(head.children[i].id);
            if (auto* mdl = ed.mod.models.try_to_get(id); mdl) {
                show(ed, *mdl, i);
            }
        } else {
            auto id = enum_cast<component_ref_id>(head.children[i].id);
            if (auto* c_ref = ed.mod.component_refs.try_to_get(id); c_ref) {
                if (auto* c = ed.mod.components.try_to_get(c_ref->id); c) {
                    show(ed, *c, i);
                }
            }
        }
    }
}

static void show_opened_component_ref(component_editor& /*ed*/,
                                      component& /*head*/,
                                      component_ref& /*ref*/) noexcept
{}

static void show_opened_component(component_editor& ed)
{
    if (auto* head = ed.mod.components.try_to_get(ed.mod.head); head) {
        ImNodes::EditorContextSet(ed.context);
        ImNodes::BeginNodeEditor();

        auto* ref = ed.mod.component_refs.try_to_get(ed.selected_component);
        if (!ref) {
            show_opened_component_top(ed, *head);
        } else {
            show_opened_component_ref(ed, *head, *ref);
        }

        ImGui::PopStyleVar();
        if (ed.show_minimap)
            ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);
        ImNodes::EndNodeEditor();
    }
}

static void settings_compute_colors(
  component_editor::settings_manager& settings) noexcept
{
    settings.gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color * 1.25f);
    settings.gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color * 1.5f);

    settings.gui_hovered_component_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color * 1.25f);
    settings.gui_selected_component_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color * 1.5f);

    settings.gui_hovered_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_transition_color *
                                     1.25f);
    settings.gui_selected_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_transition_color *
                                     1.5f);
}

void component_editor::settings_manager::show(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Component settings", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Graphics");
    if (ImGui::ColorEdit3(
          "model", (float*)&gui_model_color, ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);
    if (ImGui::ColorEdit3(
          "model", (float*)&gui_component_color, ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);
    if (ImGui::ColorEdit3("model",
                          (float*)&gui_model_transition_color,
                          ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);

    ImGui::Text("Automatic layout parameters");
    ImGui::DragInt(
      "max iteration", &automatic_layout_iteration_limit, 1.f, 0, 1000);
    ImGui::DragFloat(
      "a-x-distance", &automatic_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "a-y-distance", &automatic_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::Text("Grid layout parameters");
    ImGui::DragFloat(
      "g-x-distance", &grid_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "g-y-distance", &grid_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::End();
}

void component_editor::init() noexcept
{
    if (!context) {
        context = ImNodes::EditorContextCreate();
        ImNodes::PushAttributeFlag(
          ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodesIO& io                           = ImNodes::GetIO();
        io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

        settings_compute_colors(settings);
    }
}

void component_editor::shutdown() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
        context = nullptr;
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

} // irt
