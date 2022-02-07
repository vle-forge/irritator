// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static void show_internal_components(irt::component_editor& ed);
static void show_notsaved_component(irt::component_editor& ed);

static bool can_add_this_component(const tree_node*   top,
                                   const component_id id) noexcept
{
    if (top->id == id)
        return false;

    if (auto* parent = top->tree.get_parent(); parent) {
        do {
            if (parent->id == id)
                return false;

            parent = parent->tree.get_parent();
        } while (parent);
    }

    return true;
}

static bool can_add_this_component(component_editor&  ed,
                                   const component_id id) noexcept
{
    auto* tree = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    if (!tree)
        return false;

    if (!can_add_this_component(tree, id))
        return false;

    return true;
}

static component_id add_empty_component(component_editor& ed) noexcept
{
    if (ed.mod.components.can_alloc()) {
        auto& new_compo    = ed.mod.components.alloc();
        auto  new_compo_id = ed.mod.components.get_id(new_compo);
        new_compo.name.assign("New component");
        new_compo.type  = component_type::memory;
        new_compo.state = component_status::modified;

        return new_compo_id;
    }

    return undefined<component_id>();
}

static status add_component_to_current(component_editor& ed,
                                       component&        compo) noexcept
{
    auto& parent       = ed.mod.tree_nodes.get(ed.selected_component);
    auto& parent_compo = ed.mod.components.get(parent.id);

    if (!can_add_this_component(ed, ed.mod.components.get_id(compo)))
        return status::gui_not_enough_memory;

    tree_node_id tree_id;
    irt_return_if_bad(ed.mod.make_tree_from(compo, &tree_id));

    auto& c = parent_compo.children.alloc(ed.mod.components.get_id(compo));
    parent_compo.state = component_status::modified;

    auto& tree        = ed.mod.tree_nodes.get(tree_id);
    tree.id_in_parent = parent_compo.children.get_id(c);
    tree.tree.set_id(&tree);
    tree.tree.parent_to(parent.tree);

    return status::success;
}

static void show_component(component_editor& ed, component& c) noexcept
{
    auto* file = ed.mod.file_paths.try_to_get(c.file);

    irt_assert(file);

    if (file) {
        ImGui::Selectable(
          file->path.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
    }

    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            add_component_to_current(ed, c);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ed.selected_component_list = &c;
            ImGui::OpenPopup("Component Menu");
        }
    }

    if (c.state == component_status::modified) {
        ImGui::SameLine();
        ImGui::TextUnformatted("(modified)");
    }
}

void show_notsaved_component(irt::component_editor& ed)
{
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        if (compo->type == component_type::memory) {

            ImGui::Selectable(compo->name.c_str(),
                              false,
                              ImGuiSelectableFlags_AllowDoubleClick);

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    add_component_to_current(ed, *compo);
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ed.selected_component_list = compo;
                    ImGui::OpenPopup("Component Menu");
                }
            }
        }
    }
}

void show_internal_components(irt::component_editor& ed)
{
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        if (compo->type != component_type::file &&
            compo->type != component_type::memory) {

            ImGui::Selectable(compo->name.c_str(),
                              false,
                              ImGuiSelectableFlags_AllowDoubleClick);

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    add_component_to_current(ed, *compo);
                } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ed.selected_component_list = compo;
                    ImGui::OpenPopup("Component Menu");
                }
            }
        }
    }
}

void application::show_components_window() noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Component library", flags)) {
        if (ImGui::TreeNodeEx("Internal")) {
            show_internal_components(c_editor);
            ImGui::TreePop();
        }

        for (auto id : c_editor.mod.component_repertories) {
            static small_string<32> s;
            small_string<32>*       select;

            auto& reg_dir = c_editor.mod.registred_paths.get(id);
            if (reg_dir.name.empty()) {
                format(s, "{}", ordinal(id));
                select = &s;
            } else {
                select = &reg_dir.name;
            }

            ImGui::PushID(&reg_dir);
            if (ImGui::TreeNodeEx(select->c_str())) {
                for (auto dir_id : reg_dir.children) {
                    auto& dir = c_editor.mod.dir_paths.get(dir_id);
                    if (ImGui::TreeNodeEx(dir.path.c_str())) {
                        for (auto file_id : dir.children) {
                            auto& file = c_editor.mod.file_paths.get(file_id);
                            auto& compo =
                              c_editor.mod.components.get(file.component);

                            show_component(c_editor, compo);
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::TreeNodeEx("Not saved")) {
            show_notsaved_component(c_editor);
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextWindow("Component Menu")) {
            if (ImGui::MenuItem("New component")) {
                log_w.log(7, "adding a new component");
                auto id = add_empty_component(c_editor);
                c_editor.open_as_main(id);
            }

            if (ImGui::MenuItem("Open as main")) {
                log_w.log(
                  7, "@todo be sure to save before opening a new component");

                auto id = c_editor.mod.components.get_id(
                  *c_editor.selected_component_list);

                c_editor.open_as_main(id);
            }

            if (ImGui::MenuItem("Copy")) {
                if (c_editor.mod.components.can_alloc()) {
                    auto& new_c = c_editor.mod.components.alloc();
                    new_c.type  = component_type::memory;
                    new_c.name  = c_editor.selected_component_list->name;
                    new_c.state = component_status::modified;
                    c_editor.mod.copy(*c_editor.selected_component_list, new_c);
                } else {
                    log_w.log(3, "Can not alloc a new component");
                }
            }

            if (ImGui::MenuItem("Delete")) {
                if (c_editor.selected_component_list->type ==
                    component_type::memory) {
                    c_editor.mod.free(*c_editor.selected_component_list);
                    c_editor.selected_component_list = nullptr;
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Selected children", flags)) {
        auto* tree =
          c_editor.mod.tree_nodes.try_to_get(c_editor.selected_component);
        if (tree) {
            component* compo = c_editor.mod.components.try_to_get(tree->id);
            if (compo) {
                for (int i = 0, e = c_editor.selected_nodes.size(); i != e;
                     ++i) {
                    auto* child = compo->children.try_to_get(
                      static_cast<u32>(c_editor.selected_nodes[i]));
                    if (!child)
                        continue;

                    if (ImGui::TreeNodeEx(child,
                                          ImGuiTreeNodeFlags_DefaultOpen,
                                          "%d",
                                          c_editor.selected_nodes[i])) {
                        bool is_modified = false;
                        ImGui::Text("position %f %f",
                                    static_cast<double>(child->x),
                                    static_cast<double>(child->y));
                        if (ImGui::Checkbox("configurable",
                                            &child->configurable))
                            is_modified = true;
                        if (ImGui::Checkbox("observables", &child->observable))
                            is_modified = true;
                        if (ImGui::InputSmallString("name", child->name))
                            is_modified = true;

                        if (is_modified)
                            compo->state = component_status::modified;

                        if (child->type == child_type::model) {
                            auto  child_id = enum_cast<model_id>(child->id);
                            auto* mdl      = compo->models.try_to_get(child_id);

                            if (mdl)
                                ImGui::Text(
                                  "type: %s",
                                  dynamics_type_names[ordinal(mdl->type)]);
                        } else {
                            auto  compo_id = enum_cast<component_id>(child->id);
                            auto* compo =
                              c_editor.mod.components.try_to_get(compo_id);

                            if (compo)
                                ImGui::Text(
                                  "type: %s",
                                  component_type_names[ordinal(compo->type)]);
                        }

                        ImGui::TreePop();
                    }
                }
            }
        }
    }
}

} // namespace irt
