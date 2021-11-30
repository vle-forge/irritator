// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

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
    ImGui::Selectable(
      c.name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            add_component_to_current(ed, c);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ed.selected_component_list      = &c;
            ed.selected_component_type_list = c.type;
            ImGui::OpenPopup("Component Menu");
        }
    }

    if (c.state == component_status::modified) {
        ImGui::SameLine();
        ImGui::TextUnformatted("(modified)");
    }
}

static void show_all_components(component_editor& ed)
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Components", flags)) {
        if (ImGui::TreeNodeEx("Internal")) {
            component* compo = nullptr;
            while (ed.mod.components.next(compo)) {
                if (compo->type != component_type::file &&
                    compo->type != component_type::memory) {
                    show_component(ed, *compo);
                }
            }

            ImGui::TreePop();
        }

        for (auto id : ed.mod.component_repertories) {
            small_string<32>  s;
            small_string<32>* select;

            auto& dir = ed.mod.dir_paths.get(id);
            if (dir.name.empty()) {
                format(s, "{}", id);
                select = &s;
            } else {
                select = &dir.name;
            }

            ImGui::PushID(&dir);
            if (ImGui::TreeNodeEx(select->c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
                component* compo = nullptr;
                while (ed.mod.components.next(compo))
                    if (compo->type == component_type::file && compo->dir == id)
                        show_component(ed, *compo);

                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            component* compo = nullptr;
            while (ed.mod.components.next(compo)) {
                if (compo->type == component_type::memory) {
                    show_component(ed, *compo);
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextWindow("Component Menu")) {
            if (ImGui::MenuItem("New component")) {
                log_w.log(7, "adding a new component");
                auto id = add_empty_component(ed);
                ed.open_as_main(id);
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
                    new_c.state = component_status::modified;
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

    if (ImGui::CollapsingHeader("Selected children", flags)) {
        auto* tree = ed.mod.tree_nodes.try_to_get(ed.selected_component);
        if (tree) {
            component* compo = ed.mod.components.try_to_get(tree->id);
            if (compo) {
                for (int i = 0, e = ed.selected_nodes.ssize(); i != e; ++i) {
                    auto* child =
                      unpack_node(ed.selected_nodes[i], compo->children);
                    if (!child)
                        continue;

                    if (ImGui::TreeNodeEx(child,
                                          ImGuiTreeNodeFlags_DefaultOpen,
                                          "%d",
                                          ed.selected_nodes[i])) {
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
                              ed.mod.components.try_to_get(compo_id);

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

void component_editor::show_components_window() noexcept
{
    show_all_components(*this);
}

} // namespace irt