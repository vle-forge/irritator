// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

int pack_in(const child_id id, const i8 port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    u32 port_index = static_cast<u32>(port);
    u32 index      = get_index(id);

    return static_cast<int>((index << 5u) | port_index);
}

int pack_out(const child_id id, const i8 port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    u32 port_index = 8u + static_cast<u32>(port);
    u32 index      = get_index(id);

    return static_cast<int>((index << 5u) | port_index);
}

void unpack_in(const int node_id, u32* index, i8* port) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    *port  = static_cast<i8>(real_node_id & 7u);
    *index = static_cast<u32>(real_node_id >> 5u);

    irt_assert((real_node_id & 8u) == 0);
}

void unpack_out(const int node_id, u32* index, i8* port) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    *port  = static_cast<i8>(real_node_id & 7u);
    *index = static_cast<u32>(real_node_id >> 5u);

    irt_assert((real_node_id & 8u) != 0);
}

int pack_node(const child_id id) noexcept
{
    return static_cast<int>(get_index(id));
}

child* unpack_node(const int                          node_id,
                   const data_array<child, child_id>& data) noexcept
{
    return data.try_to_get(static_cast<u32>(node_id));
}

static ImVec4 operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

static void add_component_to_current(component_editor& ed,
                                     component&        compo) noexcept
{
    component_ref* parent =
      ed.mod.component_refs.try_to_get(ed.selected_component);
    if (!parent)
        return;

    component* parent_compo = ed.mod.components.try_to_get(parent->id);
    if (!parent_compo)
        return;

    if (!ed.mod.component_refs.can_alloc(1))
        return;

    auto& new_ref    = ed.mod.component_refs.alloc();
    auto  new_ref_id = ed.mod.component_refs.get_id(new_ref);
    new_ref.id       = ed.mod.components.get_id(compo);
    new_ref.tree.set_id(&new_ref);
    new_ref.tree.parent_to(parent->tree);

    irt_assert(new_ref.tree.parented_by(parent->tree));

    auto& child    = ed.mod.children.alloc(new_ref_id);
    auto  child_id = ed.mod.children.get_id(child);

    parent_compo->children.emplace_back(child_id);
}

static void show_all_components(component_editor& ed)
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

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
                            ed.selected_component_list      = compo;
                            ed.selected_component_type_list = compo->type;
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
                            ed.selected_component_list      = compo;
                            ed.selected_component_type_list = compo->type;
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
                            ed.selected_component_list      = compo;
                            ed.selected_component_type_list = compo->type;
                            ImGui::OpenPopup("Component Menu");
                        }
                    }
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextWindow("Component Menu")) {
            if (ImGui::MenuItem("New component")) {
                log_w.log(7, "adding a new component");
                auto id = ed.add_empty_component();
                ed.select(id);
            }

            if (ImGui::MenuItem("Open as main")) {
                log_w.log(
                  7, "@todo be sure to save before opening a new component");

                auto id = ed.mod.components.get_id(*ed.selected_component_list);
                ed.open_as_main(id);
            }

            if (ImGui::MenuItem("Copy")) {
                if (ed.mod.components.can_alloc()) {
                    auto& new_c = ed.mod.components.alloc();
                    new_c.type  = component_type::memory;
                    new_c.name  = ed.selected_component_list->name;
                    ed.mod.copy(*ed.selected_component_list, new_c);
                } else {
                    log_w.log(3, "Can not alloc a new component");
                }
            }

            if (ImGui::MenuItem("Delete")) {
                if (ed.selected_component_type_list == component_type::memory) {
                    ed.mod.free(*ed.selected_component_list);
                    ed.selected_component_list = nullptr;
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Attributes", flags)) {
        auto* ref = ed.mod.component_refs.try_to_get(ed.selected_component);
        if (ref) {
            if (auto* compo = ed.mod.components.try_to_get(ref->id); compo) {
                ImGui::InputText(
                  "name", compo->name.begin(), compo->name.capacity());
                if (compo->type == component_type::memory ||
                    compo->type == component_type::file) {
                    static constexpr const char* empty = "";

                    auto*       dir = ed.mod.dir_paths.try_to_get(compo->dir);
                    const char* preview = dir ? dir->path.c_str() : empty;
                    if (ImGui::BeginCombo(
                          "Select directory", preview, ImGuiComboFlags_None)) {
                        dir_path* list = nullptr;
                        while (ed.mod.dir_paths.next(list)) {
                            if (ImGui::Selectable(list->path.c_str(),
                                                  preview == list->path.c_str(),
                                                  ImGuiSelectableFlags_None)) {
                                compo->dir = ed.mod.dir_paths.get_id(list);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    auto* file = ed.mod.file_paths.try_to_get(compo->file);
                    if (file) {
                        ImGui::InputText("File##text",
                                         file->path.begin(),
                                         file->path.capacity());
                    } else {
                        ImGui::Text("File not saved.");
                        if (ImGui::Button("Add file")) {
                            auto& f     = ed.mod.file_paths.alloc();
                            compo->file = ed.mod.file_paths.get_id(f);
                        }
                    }

                    auto* desc = ed.mod.descriptions.try_to_get(compo->desc);
                    if (!desc && ed.mod.descriptions.can_alloc(1)) {
                        if (ImGui::Button("Add description")) {
                            auto& new_desc = ed.mod.descriptions.alloc();
                            compo->desc = ed.mod.descriptions.get_id(new_desc);
                        }
                    } else {
                        constexpr ImGuiInputTextFlags flags =
                          ImGuiInputTextFlags_AllowTabInput;

                        ImGui::InputTextMultiline(
                          "##source",
                          desc->data.begin(),
                          desc->data.capacity(),
                          ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                          flags);

                        if (ImGui::Button("Remove")) {
                            ed.mod.descriptions.free(*desc);
                            compo->desc = undefined<description_id>();
                        }
                    }

                    if (file && dir) {
                        if (ImGui::Button("Save")) {
                            if (auto ret = ed.mod.save(*compo); is_bad(ret)) {
                                log_w.log(
                                  2,
                                  "Fail to save file %s in directory %s\n",
                                  file->path.c_str(),
                                  dir->path.c_str());
                            }
                        }
                    }
                }
            }
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Selected children", flags)) {
        for (int i = 0, e = ed.selected_nodes.ssize(); i != e; ++i) {
            auto* child = unpack_node(ed.selected_nodes[i], ed.mod.children);
            if (!child)
                continue;

            if (ImGui::TreeNodeEx(child,
                                  ImGuiTreeNodeFlags_DefaultOpen,
                                  "%d",
                                  ed.selected_nodes[i])) {
                ImGui::Text("position %f %f",
                            static_cast<double>(child->x),
                            static_cast<double>(child->y));
                ImGui::Checkbox("configurable", &child->configurable);
                ImGui::Checkbox("observables", &child->observable);
                ImGui::InputText(
                  "name", child->name.begin(), child->name.capacity());

                if (child->type == child_type::model) {
                    auto  child_id = enum_cast<model_id>(child->id);
                    auto& mdl      = ed.mod.models.get(child_id);

                    ImGui::Text("type: %s",
                                dynamics_type_names[ordinal(mdl.type)]);
                } else {
                    auto  c_ref_id = enum_cast<component_ref_id>(child->id);
                    auto& c_ref    = ed.mod.component_refs.get(c_ref_id);
                    auto* compo    = ed.mod.components.try_to_get(c_ref.id);

                    if (compo) {
                        ImGui::Text("type: %s",
                                    component_type_names[ordinal(compo->type)]);
                    }
                }

                ImGui::TreePop();
            }
        }
    }
}

static void show_component_hierarchy_model(component_editor& /*ed*/, model& mdl)
{
    ImGui::Text("%s", get_dynamics_type_name(mdl.type));
}

static void show_component_hierarchy_component(component_editor& ed,
                                               component_ref&    c_ref)
{
    auto* compo = ed.mod.components.try_to_get(c_ref.id);
    if (!compo)
        return;

    if (ImGui::TreeNodeEx(compo,
                          ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_OpenOnDoubleClick,
                          "%s",
                          compo->name.c_str())) {
        if (ImGui::IsItemHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ed.mod.component_refs.get_id(c_ref) != ed.selected_component) {
            ed.select(ed.mod.component_refs.get_id(c_ref));
        }

        for (int j = 0; j != compo->children.ssize(); ++j) {
            auto* subchild = ed.mod.children.try_to_get(compo->children[j]);
            if (!subchild)
                continue;

            ImGui::PushID(j);

            if (subchild->type == child_type::model) {
                auto  id  = enum_cast<model_id>(subchild->id);
                auto* mdl = ed.mod.models.try_to_get(id);
                if (mdl)
                    show_component_hierarchy_model(ed, *mdl);
            } else {
                auto  id = enum_cast<component_ref_id>(subchild->id);
                auto* c  = ed.mod.component_refs.try_to_get(id);
                if (c)
                    show_component_hierarchy_component(ed, *c);
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

static void show_component_hierarchy(component_editor& ed)
{
    if (auto* c_ref = ed.mod.component_refs.try_to_get(ed.mod.head); c_ref) {
        if (auto* compo = ed.mod.components.try_to_get(c_ref->id); compo) {
            if (ImGui::TreeNodeEx(compo->name.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen |
                                    ImGuiTreeNodeFlags_OpenOnDoubleClick)) {

                if (ImGui::IsItemHovered() &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                    ed.mod.head != ed.selected_component) {
                    ed.select(ed.mod.head);
                }

                for (int i = 0; i < compo->children.ssize(); ++i) {
                    auto* child =
                      ed.mod.children.try_to_get(compo->children[i]);
                    if (!child)
                        continue;

                    ImGui::PushID(i);
                    if (child->type == child_type::model) {
                        auto  id  = enum_cast<model_id>(child->id);
                        auto* mdl = ed.mod.models.try_to_get(id);
                        if (mdl)
                            show_component_hierarchy_model(ed, *mdl);
                    } else {
                        auto  id = enum_cast<component_ref_id>(child->id);
                        auto* c  = ed.mod.component_refs.try_to_get(id);
                        if (c)
                            show_component_hierarchy_component(ed, *c);
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::Text("No component load");
        }
    }
}

template<typename Dynamics>
static void add_input_attribute(const Dynamics& dyn, child_id id) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        irt_assert(length(dyn.x) < 8);

        for (int i = 0; i < length(dyn.x); ++i) {
            ImNodes::BeginInputAttribute(pack_in(id, static_cast<i8>(i)),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(const Dynamics& dyn, child_id id) noexcept
{
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();

        irt_assert(length(dyn.y) < 8);

        for (int i = 0; i < length(dyn.y); ++i) {
            ImNodes::BeginOutputAttribute(pack_out(id, static_cast<i8>(i)),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

static bool show_connection(component_editor& ed,
                            const component&  parent,
                            int               con_index) noexcept
{
    auto  con_id = parent.connections[con_index];
    auto* con    = ed.mod.connections.try_to_get(con_id);
    if (!con)
        return false;

    auto* src = ed.mod.children.try_to_get(con->src);
    if (!src) {
        ed.mod.connections.free(*con);
        return false;
    }

    auto* dst = ed.mod.children.try_to_get(con->dst);
    if (!dst) {
        ed.mod.connections.free(*con);
        return false;
    }

    ImNodes::Link(con_index,
                  pack_out(con->src, con->index_src),
                  pack_in(con->dst, con->index_dst));

    return true;
}

static void show(component_editor& ed,
                 model&            mdl,
                 child_id          id,
                 int               mdl_index) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_model_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", mdl_index, get_dynamics_type_name(mdl.type));
    ImNodes::EndNodeTitleBar();

    dispatch(mdl, [&ed, id](auto& dyn) {
        add_input_attribute(dyn, id);
        ImGui::PushItemWidth(120.0f);
        show_dynamics_inputs(ed.mod.srcs, dyn);
        ImGui::PopItemWidth();
        add_output_attribute(dyn, id);
    });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show(component_editor& ed,
                 component&        compo,
                 child_id          id,
                 int               mdl_index) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", mdl_index, compo.name.c_str());
    ImNodes::EndNodeTitleBar();

    irt_assert(length(compo.x) < INT8_MAX);
    irt_assert(length(compo.y) < INT8_MAX);

    for (int i = 0; i < length(compo.x); ++i) {
        ImNodes::BeginInputAttribute(pack_in(id, static_cast<i8>(i)),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    for (int i = 0; i < length(compo.y); ++i) {
        ImNodes::BeginOutputAttribute(pack_out(id, static_cast<i8>(i)),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

// static void show_opened_component_top(component_editor& ed,
//                                       component&        head) noexcept
// {
//     for (int i = 0; i < head.children.ssize(); ++i) {
//         const auto child_id = head.children[i];
//         auto*      child    = ed.mod.children.try_to_get(child_id);
//         if (!child)
//             continue;

//         if (child->type == child_type::model) {
//             auto id = enum_cast<model_id>(child->id);
//             if (auto* mdl = ed.mod.models.try_to_get(id); mdl) {
//                 show(ed, *mdl, child_id, i);
//                 if (ed.force_node_position) {
//                     ImNodes::SetNodeEditorSpacePos(pack_node(child_id),
//                                                    ImVec2(child->x,
//                                                    child->y));
//                 } else {
//                     auto pos =
//                       ImNodes::GetNodeEditorSpacePos(pack_node(child_id));
//                     child->x = pos.x;
//                     child->y = pos.y;
//                 }
//             }
//         } else {
//             auto id = enum_cast<component_ref_id>(child->id);
//             if (auto* c_ref = ed.mod.component_refs.try_to_get(id); c_ref) {
//                 if (auto* c = ed.mod.components.try_to_get(c_ref->id); c) {
//                     show(ed, *c, child_id, i);

//                     if (ed.force_node_position) {
//                         ImNodes::SetNodeEditorSpacePos(
//                           pack_node(child_id), ImVec2(child->x, child->y));
//                     } else {
//                         auto pos =
//                           ImNodes::GetNodeEditorSpacePos(pack_node(child_id));
//                         child->x = pos.x;
//                         child->y = pos.y;
//                     }
//                 }
//             }
//         }
//     }

//     ed.force_node_position = false;

//     int i = 0;
//     while (i < head.connections.ssize()) {
//         if (!show_connection(ed, head, i))
//             head.connections.swap_pop_back(i);
//         else
//             ++i;
//     }
// }

static void show_opened_component_ref(component_editor& ed,
                                      component_ref& /*ref*/,
                                      component& compo) noexcept
{
    for (int i = 0; i < compo.children.ssize(); ++i) {
        const auto child_id = compo.children[i];
        auto*      child    = ed.mod.children.try_to_get(child_id);
        if (!child)
            continue;

        if (child->type == child_type::model) {
            auto id = enum_cast<model_id>(child->id);
            if (auto* mdl = ed.mod.models.try_to_get(id); mdl) {
                show(ed, *mdl, child_id, i);

                if (ed.force_node_position) {
                    ImNodes::SetNodeEditorSpacePos(pack_node(child_id),
                                                   ImVec2(child->x, child->y));
                    ed.force_node_position = false;
                } else {
                    auto pos =
                      ImNodes::GetNodeEditorSpacePos(pack_node(child_id));
                    child->x = pos.x;
                    child->y = pos.y;
                }
            }
        } else {
            auto id = enum_cast<component_ref_id>(child->id);
            if (auto* c_ref = ed.mod.component_refs.try_to_get(id); c_ref) {
                if (auto* c = ed.mod.components.try_to_get(c_ref->id); c) {
                    show(ed, *c, child_id, i);

                    if (ed.force_node_position) {
                        ImNodes::SetNodeEditorSpacePos(
                          pack_node(child_id), ImVec2(child->x, child->y));
                        ed.force_node_position = false;
                    } else {
                        auto pos =
                          ImNodes::GetNodeEditorSpacePos(pack_node(child_id));
                        child->x = pos.x;
                        child->y = pos.y;
                    }
                }
            }
        }
    }

    int i = 0;
    while (i < compo.connections.ssize()) {
        if (!show_connection(ed, compo, i))
            compo.connections.swap_pop_back(i);
        else
            ++i;
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               dynamics_type     type,
                               child_id*         new_model)
{
    if (!ed.mod.models.can_alloc(1)) {
        log_w.log(2, "can not allocate a new model");
        return;
    }

    if (ImGui::MenuItem(get_dynamics_type_name(type))) {
        auto& child = ed.mod.alloc(parent, type);
        *new_model  = ed.mod.children.get_id(child);

        log_w.log(7, "new model %zu\n", ordinal(ed.mod.children.get_id(child)));
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               int               type,
                               child_id*         new_model)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, parent, d_type, new_model);
}

static void show_popup_menuitem(component_editor& ed,
                                component&        parent,
                                ImVec2*           click_pos,
                                child_id*         new_model) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    *new_model = undefined<child_id>();
    *click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i < e; ++i)
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

component* get_current_component(component_editor& ed) noexcept
{
    if (auto* ref = ed.mod.component_refs.try_to_get(ed.selected_component);
        ref)
        if (auto* compo = ed.mod.components.try_to_get(ref->id); compo)
            return compo;

    if (auto* ref = ed.mod.component_refs.try_to_get(ed.mod.head); ref)
        if (auto* compo = ed.mod.components.try_to_get(ref->id); compo)
            return compo;

    return nullptr;
}

static void is_link_created(component_editor& ed, component& parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        u32 index_src, index_dst;
        i8  port_src_index, port_dst_index;

        unpack_out(start, &index_src, &port_src_index);
        unpack_in(end, &index_dst, &port_dst_index);

        auto* child_src = ed.mod.children.try_to_get(index_src);
        auto* child_dst = ed.mod.children.try_to_get(index_dst);

        if (child_src != nullptr && child_dst != nullptr) {
            auto  child_src_id = ed.mod.children.get_id(*child_src);
            auto  child_dst_id = ed.mod.children.get_id(*child_dst);
            auto& con          = ed.mod.connections.alloc(
              child_src_id, port_src_index, child_dst_id, port_dst_index);
            auto con_id = ed.mod.connections.get_id(con);
            parent.connections.emplace_back(con_id);
        }

        // if (type_src == child_type::model) {
        //     if (auto* mdl = ed.mod.models.try_to_get(index_src); mdl)
        //         src = ordinal(ed.mod.models.get_id(*mdl));
        // } else {
        //     if (auto* c = ed.mod.component_refs.try_to_get(index_src); c)
        //         src = ordinal(ed.mod.component_refs.get_id(*c));
        // }

        // if (type_dst == child_type::model) {
        //     if (auto* mdl = ed.mod.models.try_to_get(index_dst); mdl)
        //         dst = ordinal(ed.mod.models.get_id(*mdl));
        // } else {
        //     if (auto* c = ed.mod.component_refs.try_to_get(index_dst); c)
        //         dst = ordinal(ed.mod.component_refs.get_id(*c));
        // }

        // if (src != 0 && dst != 0)
        //     parent.connections.emplace_back(
        //       src, dst, type_src, type_dst, port_src, port_dst);
    }
}

void remove_nodes(component_editor& ed, component& parent) noexcept
{
    for (i32 i = 0, e = ed.selected_nodes.ssize(); i != e; ++i) {
        if (auto* child = unpack_node(ed.selected_nodes[i], ed.mod.children);
            child)
            ed.mod.free(parent, *child);
    }

    ed.selected_nodes.clear();
    ImNodes::ClearNodeSelection();
}

void remove_links(component_editor& ed, component& parent) noexcept
{
    std::sort(
      ed.selected_links.begin(), ed.selected_links.end(), std::greater<int>());

    for (i32 i = 0, e = ed.selected_links.ssize(); i != e; ++i) {
        if (i < parent.connections.ssize()) {
            auto* con = ed.mod.connections.try_to_get(parent.connections[i]);
            if (con)
                ed.mod.connections.free(*con);
        }
    }

    std::sort(
      ed.selected_links.begin(), ed.selected_links.end(), std::greater<int>());

    for (i32 i = 0, e = ed.selected_links.ssize(); i != e; ++i)
        parent.connections.swap_pop_back(ed.selected_links[i]);

    ed.selected_links.clear();
    ImNodes::ClearLinkSelection();
}

static void show_opened_component(component_editor& ed) noexcept
{
    component_ref* c_ref =
      ed.mod.component_refs.try_to_get(ed.selected_component);
    if (!c_ref)
        return;

    component* compo = ed.mod.components.try_to_get(c_ref->id);
    if (!compo)
        return;

    ImNodes::EditorContextSet(ed.context);
    ImNodes::BeginNodeEditor();

    ImVec2   click_pos;
    child_id new_model;

    show_opened_component_ref(ed, *c_ref, *compo);
    show_popup_menuitem(ed, *compo, &click_pos, &new_model);

    if (ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    if (auto* child = ed.mod.children.try_to_get(new_model); child) {
        ImNodes::SetNodeScreenSpacePos(pack_node(new_model), click_pos);
        child->x = click_pos.x;
        child->y = click_pos.y;
    }

    is_link_created(ed, *compo);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        ed.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(ed.selected_nodes.begin());
    } else {
        ed.selected_nodes.clear();
    }

    if (num_selected_links > 0) {
        ed.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(ed.selected_links.begin());
    } else {
        ed.selected_links.clear();
    }

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
        if (num_selected_nodes > 0)
            remove_nodes(ed, *compo);
        else if (num_selected_links > 0)
            remove_links(ed, *compo);
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

void component_editor::show_memory_box(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Component memory", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::TextFormat("models: {} / {} / {}",
                      mod.models.size(),
                      mod.models.max_used(),
                      mod.models.capacity());
    ImGui::TextFormat("component_refs: {} / {} / {}",
                      mod.component_refs.size(),
                      mod.component_refs.max_used(),
                      mod.component_refs.capacity());
    ImGui::TextFormat("descriptions: {} / {} / {}",
                      mod.descriptions.size(),
                      mod.descriptions.max_used(),
                      mod.descriptions.capacity());
    ImGui::TextFormat("components: {} / {} / {}",
                      mod.components.size(),
                      mod.components.max_used(),
                      mod.components.capacity());
    ImGui::TextFormat("observers: {} / {} / {}",
                      mod.observers.size(),
                      mod.observers.max_used(),
                      mod.observers.capacity());
    ImGui::TextFormat("dir_paths: {} / {} / {}",
                      mod.dir_paths.size(),
                      mod.dir_paths.max_used(),
                      mod.dir_paths.capacity());
    ImGui::TextFormat("file_paths: {} / {} / {}",
                      mod.file_paths.size(),
                      mod.file_paths.max_used(),
                      mod.file_paths.capacity());
    ImGui::TextFormat("children: {} / {} / {}",
                      mod.children.size(),
                      mod.children.max_used(),
                      mod.children.capacity());
    ImGui::TextFormat("connections: {} / {} / {}",
                      mod.connections.size(),
                      mod.connections.max_used(),
                      mod.connections.capacity());

    if (ImGui::CollapsingHeader("Components")) {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            ImGui::PushID(compo);
            if (ImGui::TreeNode(compo->name.c_str())) {
                ImGui::Text("children: %d", compo->children.ssize());
                ImGui::Text("connections: %d", compo->connections.ssize());
                ImGui::Text("x: %d", compo->x.ssize());
                ImGui::Text("y: %d", compo->y.ssize());

                ImGui::Text("description: %d", ordinal(compo->desc));
                ImGui::Text("dir: %d", ordinal(compo->dir));
                ImGui::Text("file: %d", ordinal(compo->file));
                ImGui::Text("type: %s",
                            component_type_names[ordinal(compo->type)]);

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

component_ref_id component_editor::add_empty_component() noexcept
{
    if (mod.components.can_alloc()) {
        if (mod.component_refs.can_alloc(1)) {
            auto& new_compo = mod.components.alloc();
            new_compo.name.assign("New component");
            new_compo.type = component_type::memory;

            auto& new_c_ref = mod.component_refs.alloc();
            new_c_ref.id    = mod.components.get_id(new_compo);
            new_c_ref.tree.set_id(&new_c_ref);

            return mod.component_refs.get_id(new_c_ref);
        }
    }

    return undefined<component_ref_id>();
}

static void unselect_editor_component_ref(component_editor& ed) noexcept
{
    ed.selected_component = undefined<component_ref_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();
    ed.selected_links.clear();
    ed.selected_nodes.clear();
}

void component_editor::unselect() noexcept
{
    mod.head = undefined<component_ref_id>();

    unselect_editor_component_ref(*this);
}

void component_editor::select(component_ref_id id) noexcept
{
    if (auto* c_ref = mod.component_refs.try_to_get(id); c_ref) {
        if (auto* compo = mod.components.try_to_get(c_ref->id); compo) {
            unselect_editor_component_ref(*this);

            selected_component  = id;
            force_node_position = true;
        }
    }
}

void component_editor::open_as_main(component_id id) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        if (mod.component_refs.can_alloc(1)) {
            unselect();

            auto& c_ref    = mod.component_refs.alloc();
            auto  c_ref_id = mod.component_refs.get_id(c_ref);
            c_ref.id       = id;

            mod.head            = c_ref_id;
            selected_component  = c_ref_id;
            force_node_position = true;

            mod.make_tree_from(c_ref);
        }
    }
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

static void show_simulation(component_editor& ed) noexcept
{
    if (ImGui::CollapsingHeader("Simulation",
                                ImGuiTreeNodeFlags_CollapsingHeader)) {
        ImGui::InputReal("Begin", &ed.simulation_begin);
        ImGui::InputReal("End", &ed.simulation_end);
        ImGui::TextFormat("Current time {:.6f}", ed.simulation_begin);

        if (ImGui::Button("[]")) {
        }
        ImGui::SameLine();
        if (ImGui::Button("||")) {
        }
        ImGui::SameLine();
        if (ImGui::Button(">")) {
        }
        ImGui::SameLine();
        if (ImGui::Button("+1")) {
        }
        ImGui::SameLine();
        if (ImGui::Button("+10")) {
        }
        ImGui::SameLine();
        if (ImGui::Button("+100")) {
        }
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

    ImVec2 current_component_size(width_1_10 * 2.f, region.y / 2.f);
    ImVec2 simulation_size(width_1_10 * 2.f, region.y / 2.f);
    ImVec2 drawing_zone_size(width_1_10 * 6.f, region.y);
    ImVec2 component_list_size(width_1_10 * 2.f, region.y);

    ImVec2 current_component_pos(0.f, viewport->WorkPos.y);
    ImVec2 simulation_pos(0.f, viewport->WorkPos.y + region.y / 2.f);
    ImVec2 drawing_zone_pos(current_component_size.x, viewport->WorkPos.y);
    ImVec2 component_list_pos(current_component_size.x + drawing_zone_size.x,
                              viewport->WorkPos.y);

    ImGui::SetNextWindowPos(current_component_pos);
    ImGui::SetNextWindowSize(current_component_size);
    if (ImGui::Begin("Modeling component", 0, flag)) {
        show_component_hierarchy(*this);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(simulation_pos);
    ImGui::SetNextWindowSize(simulation_size);
    if (ImGui::Begin("Simulation", 0, flag)) {
        show_simulation(*this);
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

    if (show_memory)
        show_memory_box(&show_memory);
}

} // irt
