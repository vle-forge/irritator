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

static void add_component_to_current(component_editor& ed, component& compo)
{
    auto* ref = ed.mod.component_refs.try_to_get(ed.selected_component);
    if (!ref) {
        if (auto* head = ed.mod.components.try_to_get(ed.mod.head); head) {
            auto& new_ref = ed.mod.component_refs.alloc();
            new_ref.id    = ed.mod.components.get_id(compo);
            head->children.emplace_back(ed.mod.component_refs.get_id(new_ref));
        }
    } else {
        if (auto* compo = ed.mod.components.try_to_get(ref->id); compo) {
            auto& new_ref = ed.mod.component_refs.alloc();
            new_ref.id    = ed.mod.components.get_id(compo);
            compo->children.emplace_back(ed.mod.component_refs.get_id(new_ref));
        }
    }
}

static void show_all_components(component_editor& ed)
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    static component*     selected_compo = nullptr;
    static component_type selected_type  = component_type::file;

    if (ImGui::CollapsingHeader("Components", flags)) {
        if (ImGui::TreeNodeEx("Internal", ImGuiTreeNodeFlags_DefaultOpen)) {
            component* compo = nullptr;
            while (ed.mod.components.next(compo)) {
                if (compo->type != component_type::file &&
                    compo->type != component_type::memory) {
                    ImGui::Selectable(compo->name.c_str(),
                                      false,
                                      ImGuiSelectableFlags_AllowDoubleClick);

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseDoubleClicked(
                              ImGuiMouseButton_Left)) {
                            add_component_to_current(ed, *compo);
                        } else if (ImGui::IsMouseClicked(
                                     ImGuiMouseButton_Right)) {
                            selected_compo = compo;
                            selected_type  = compo->type;
                            ImGui::OpenPopup("Component Menu");
                        }
                    }
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("File", ImGuiTreeNodeFlags_DefaultOpen)) {
            component* compo = nullptr;
            while (ed.mod.components.next(compo)) {
                if (compo->type == component_type::file) {
                    ImGui::Selectable(compo->name.c_str(),
                                      false,
                                      ImGuiSelectableFlags_AllowDoubleClick);

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseDoubleClicked(
                              ImGuiMouseButton_Left)) {
                            add_component_to_current(ed, *compo);
                        } else if (ImGui::IsMouseClicked(
                                     ImGuiMouseButton_Right)) {
                            selected_compo = compo;
                            selected_type  = compo->type;
                            ImGui::OpenPopup("Component Menu");
                        }
                    }
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            component* compo = nullptr;
            while (ed.mod.components.next(compo)) {
                if (compo->type == component_type::memory) {
                    ImGui::Selectable(compo->name.c_str(),
                                      false,
                                      ImGuiSelectableFlags_AllowDoubleClick);

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseDoubleClicked(
                              ImGuiMouseButton_Left)) {
                            add_component_to_current(ed, *compo);
                        } else if (ImGui::IsMouseClicked(
                                     ImGuiMouseButton_Right)) {
                            selected_compo = compo;
                            selected_type  = compo->type;
                            ImGui::OpenPopup("Component Menu");
                        }
                    }
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextWindow("Component Menu")) {
            if (ImGui::MenuItem("Open")) {
                printf("open\n");
            }

            if (ImGui::MenuItem("Open as main")) {
                printf("open as main\n");
            }

            if (ImGui::MenuItem("Copy")) {
                if (ed.mod.components.can_alloc()) {
                    auto& new_c = ed.mod.components.alloc();
                    new_c.type  = component_type::memory;
                    new_c.name  = selected_compo->name;
                    ed.mod.copy(*selected_compo, new_c);
                } else {
                    log_w.log(3, "Can not alloc a new component");
                }
            }

            if (ImGui::MenuItem("Delete")) {
                if (selected_type == component_type::memory) {
                    ed.mod.free(*selected_compo);
                    selected_compo = nullptr;
                }
            }
            ImGui::EndPopup();
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

    if (ImGui::TreeNodeEx(compo, 0, "%s", compo->name.c_str())) {
        for (int j = 0, e = compo->children.ssize(); j != e; ++j) {
            ImGui::PushID(j);
            if (compo->children[j].type == child_type::model)
                show_component_hierarchy_model(ed, *compo, j);
            else
                show_component_hierarchy_component(ed, *compo, j);
            ImGui::PopID();
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
                ImGui::PushID(i);
                if (compo->children[i].type == child_type::model) {
                    show_component_hierarchy_model(ed, *compo, i);
                } else {
                    show_component_hierarchy_component(ed, *compo, i);
                }
                ImGui::PopID();
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
    int push_id_number = 0;
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

static void show_opened_component_ref(component_editor& ed,
                                      component& /*head*/,
                                      component_ref& /*ref*/,
                                      component& compo) noexcept
{
    for (int i = 0, e = compo.children.ssize(); i != e; ++i) {
        if (compo.children[i].type == child_type::model) {
            auto id = enum_cast<model_id>(compo.children[i].id);
            if (auto* mdl = ed.mod.models.try_to_get(id); mdl) {
                show(ed, *mdl, i);
            }
        } else {
            auto id = enum_cast<component_ref_id>(compo.children[i].id);
            if (auto* c_ref = ed.mod.component_refs.try_to_get(id); c_ref) {
                if (auto* c = ed.mod.components.try_to_get(c_ref->id); c) {
                    show(ed, *c, i);
                }
            }
        }
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               dynamics_type     type,
                               int*              new_model)
{
    if (!ed.mod.models.can_alloc(1)) {
        log_w.log(2, "can not allocate a new model");
        return;
    }

    if (ImGui::MenuItem(get_dynamics_type_name(type))) {
        auto& mdl  = ed.mod.alloc(parent, type);
        *new_model = parent.children.ssize() - 1;

        log_w.log(7, "new model %zu\n", ordinal(ed.mod.get_id(mdl)));
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               int               type,
                               int*              new_model)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, parent, d_type, new_model);
}

static void show_popup_menuitem(component_editor& ed,
                                component&        parent,
                                ImVec2*           click_pos,
                                int*              new_model) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    *new_model = -1;
    *click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i != e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i != e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i != e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              ed, parent, dynamics_type::integrator, new_model);
            add_popup_menuitem(
              ed, parent, dynamics_type::quantifier, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_2, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_3, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_4, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_2, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_3, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_4, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::cross, new_model);
            ImGui::EndMenu();
        }

        add_popup_menuitem(ed, parent, dynamics_type::counter, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::queue, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::dynamic_queue, new_model);
        add_popup_menuitem(
          ed, parent, dynamics_type::priority_queue, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::generator, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::constant, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::time_func, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::accumulator_2, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::filter, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::flow, new_model);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

static void show_opened_component(component_editor& ed)
{
    auto* head = ed.mod.components.try_to_get(ed.mod.head);
    irt_assert(head);

    ImVec2 click_pos;
    int    new_model = -1;

    ImNodes::EditorContextSet(ed.context);
    ImNodes::BeginNodeEditor();

    auto*      ref   = ed.mod.component_refs.try_to_get(ed.selected_component);
    component* compo = nullptr;
    if (!ref) {
        show_opened_component_top(ed, *head);
        show_popup_menuitem(ed, *head, &click_pos, &new_model);
    } else {
        if (auto* compo = ed.mod.components.try_to_get(ref->id); compo) {
            show_opened_component_ref(ed, *head, *ref, *compo);
            show_popup_menuitem(ed, *compo, &click_pos, &new_model);
        }
    }

    if (ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    if (new_model != -1) {
        auto* parent = ref ? compo : head;

        ImNodes::SetNodeScreenSpacePos(new_model, click_pos);
        parent->children[new_model].x = click_pos.x;
        parent->children[new_model].y = click_pos.y;
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
        show_all_components(*this);
        ImGui::End();
    }
}

} // irt
