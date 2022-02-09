// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static component_id add_empty_component(component_editor& ed) noexcept
{
    auto ret = undefined<component_id>();

    if (ed.mod.components.can_alloc()) {
        auto& new_compo = ed.mod.components.alloc();
        new_compo.name.assign("New component");
        new_compo.type  = component_type::memory;
        new_compo.state = component_status::modified;

        ret = ed.mod.components.get_id(new_compo);
    } else {
        auto* app   = container_of(&ed, &application::c_editor);
        auto& notif = app->notifications.alloc(notification_type::error);
        notif.title = "Can not allocate new container";
        format(notif.message,
               "Components allocated: {}\nTree nodes allocated: {}",
               ed.mod.components.size(),
               ed.mod.tree_nodes.size());
    }

    return ret;
}

static component* show_component(component_editor& ed, component& c) noexcept
{
    component* ret  = nullptr;
    auto*      file = ed.mod.file_paths.try_to_get(c.file);
    irt_assert(file);

    if (ImGui::Selectable(file->path.c_str())) {
        auto id = ed.mod.components.get_id(c);
        ed.open_as_main(id);
    }

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ret = &c;
        ImGui::OpenPopup("Component Menu");
    }

    if (c.state == component_status::modified) {
        ImGui::SameLine();
        ImGui::TextUnformatted("(modified)");
    }

    return ret;
}

static component* show_notsaved_components(irt::component_editor& ed) noexcept
{
    component* ret   = nullptr;
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        const auto is_notsaved = match(compo->type, component_type::memory);

        if (is_notsaved) {
            if (ImGui::Selectable(compo->name.c_str())) {
                auto id = ed.mod.components.get_id(*compo);
                ed.open_as_main(id);
            }

            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ret = compo;
                ImGui::OpenPopup("Component Menu");
            }
        }
    }

    return ret;
}

static component* show_internal_components(irt::component_editor& ed) noexcept
{
    component* ret   = nullptr;
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        const auto is_internal =
          !match(compo->type, component_type::file, component_type::memory);

        if (is_internal) {
            if (ImGui::Selectable(compo->name.c_str())) {
                auto id = ed.mod.components.get_id(*compo);
                ed.open_as_main(id);
            }

            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ret = compo;
                ImGui::OpenPopup("Component Menu");
            }
        }
    }

    return ret;
}

void application::show_components_window() noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;
    component* selected = nullptr;

    if (ImGui::CollapsingHeader("Component library", flags)) {
        if (ImGui::TreeNodeEx("Internal")) {
            if (auto* ret = show_internal_components(c_editor); ret)
                selected = ret;

            ImGui::TreePop();
        }

        for (auto id : c_editor.mod.component_repertories) {
            static small_string<32> s; //! @TODO remove this variable
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

                            if (auto* ret = show_component(c_editor, compo);
                                ret)
                                selected = ret;
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::TreeNodeEx("Not saved")) {
            if (auto* ret = show_notsaved_components(c_editor); ret)
                selected = ret;
            ImGui::TreePop();
        }

        if (ImGui::BeginPopupContextWindow("Component Menu")) {
            if (ImGui::MenuItem("New component")) {
                log_w.log(7, "adding a new component");
                auto id = add_empty_component(c_editor);
                c_editor.open_as_main(id);
            }

            if (selected) {
                if (ImGui::MenuItem("Open as main")) {
                    auto id = c_editor.mod.components.get_id(*selected);
                    c_editor.open_as_main(id);
                }

                if (ImGui::MenuItem("Copy")) {
                    if (c_editor.mod.components.can_alloc()) {
                        auto& new_c = c_editor.mod.components.alloc();
                        new_c.type  = component_type::memory;
                        new_c.name  = selected->name;
                        new_c.state = component_status::modified;
                        c_editor.mod.copy(*selected, new_c);
                    } else {
                        log_w.log(3, "Can not alloc a new component");
                    }
                }

                if (selected->type == component_type::memory &&
                    ImGui::MenuItem("Delete")) {
                    c_editor.mod.free(*selected);
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
