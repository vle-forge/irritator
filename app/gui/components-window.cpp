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

static void show_component_popup_menu(irt::component_editor& ed,
                                      component*             compo) noexcept
{
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New")) {
            auto id = add_empty_component(ed);
            ed.open_as_main(id);
        }

        if (compo) {
            if (ImGui::MenuItem("Copy")) {
                if (ed.mod.components.can_alloc()) {
                    auto& new_c = ed.mod.components.alloc();
                    new_c.type  = component_type::memory;
                    new_c.name  = compo->name;
                    new_c.state = component_status::modified;
                    ed.mod.copy(*compo, new_c);
                } else {
                    auto* app = container_of(&ed, &application::c_editor);
                    app->log_w.log(4, "Can not alloc a new component");
                }
            }

            if (compo->type == component_type::memory) {
                if (ImGui::MenuItem("Delete")) {
                    ed.mod.components.free(*compo);
                    compo = nullptr;
                }
            }

            ImGui::EndPopup();
        }
    }
}

static bool show_component(component_editor& ed,
                           registred_path&   reg,
                           dir_path&         dir,
                           file_path&        file,
                           component&        c,
                           irt::tree_node*   head) noexcept
{
    auto*      app        = container_of(&ed, &application::c_editor);
    const auto id         = ed.mod.components.get_id(c);
    const bool selected   = head ? id == head->id : false;
    bool       is_deleted = false;

    if (ImGui::Selectable(file.path.c_str(), selected))
        ed.open_as_main(id);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New")) {
            auto id = add_empty_component(ed);
            ed.open_as_main(id);
        }

        if (ImGui::MenuItem("Copy")) {
            if (ed.mod.components.can_alloc()) {
                auto& new_c = ed.mod.components.alloc();
                new_c.type  = component_type::memory;
                new_c.name  = c.name;
                new_c.state = component_status::modified;
                ed.mod.copy(c, new_c);
            } else {
                app->log_w.log(4, "Can not alloc a new component\n");
            }
        }

        if (ImGui::MenuItem("Delete file")) {
            app->log_w.log(
              0, "Remove file %.*s\n", file.path.size(), file.path.begin());

            ed.mod.remove_file(reg, dir, file);
            ed.mod.free(c);
            is_deleted = true;
        }

        ImGui::EndPopup();
    }

    if (!is_deleted && c.state == component_status::modified) {
        ImGui::SameLine();
        ImGui::TextUnformatted(" (modified)");
    }

    return is_deleted;
}

static void show_notsaved_components(irt::component_editor& ed,
                                     irt::tree_node*        head) noexcept
{
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        const auto is_notsaved = match(compo->type, component_type::memory);

        if (is_notsaved) {
            const auto id       = ed.mod.components.get_id(*compo);
            const bool selected = head ? id == head->id : false;

            ImGui::PushID(compo);
            if (ImGui::Selectable(compo->name.c_str(), selected))
                ed.open_as_main(id);
            ImGui::PopID();

            show_component_popup_menu(ed, compo);
        }
    }
}

static void show_internal_components(irt::component_editor& ed,
                                     irt::tree_node*        head) noexcept
{
    component* compo = nullptr;
    while (ed.mod.components.next(compo)) {
        const auto is_internal =
          !match(compo->type, component_type::file, component_type::memory);

        if (is_internal) {
            const auto id       = ed.mod.components.get_id(*compo);
            const bool selected = head ? id == head->id : false;

            if (ImGui::Selectable(compo->name.c_str(), selected))
                ed.open_as_main(id);

            show_component_popup_menu(ed, compo);
        }
    }
}

static void show_dirpath_component(irt::component_editor& ed,
                                   registred_path&        reg,
                                   dir_path&              dir,
                                   irt::tree_node*        head) noexcept
{
    if (ImGui::TreeNodeEx(dir.path.c_str())) {
        int j = 0;
        while (j < dir.children.ssize()) {
            auto  file_id = dir.children[j];
            auto* file    = ed.mod.file_paths.try_to_get(file_id);

            if (file) {
                auto  compo_id = file->component;
                auto* compo    = ed.mod.components.try_to_get(compo_id);

                if (compo) {
                    auto is_deleted =
                      show_component(ed, reg, dir, *file, *compo, head);
                    if (is_deleted) {
                        dir.children.swap_pop_back(j);
                    } else {
                        ++j;
                    }
                } else {
                    file->component = undefined<component_id>();
                    dir.children.swap_pop_back(j);
                }
            } else {
                dir.children.swap_pop_back(j);
            }
        }
        ImGui::TreePop();
    }
}

void application::show_components_window() noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;
    component* selected = nullptr;

    auto*        tree = c_editor.mod.tree_nodes.try_to_get(c_editor.mod.head);
    component_id head_id = tree ? tree->id : undefined<component_id>();

    if (ImGui::CollapsingHeader("Component library", flags)) {
        if (ImGui::TreeNodeEx("Internal")) {
            show_internal_components(c_editor, tree);
            ImGui::TreePop();
        }

        for (auto id : c_editor.mod.component_repertories) {
            small_string<32>  s;
            small_string<32>* select;

            auto* reg_dir = c_editor.mod.registred_paths.try_to_get(id);
            if (!reg_dir)
                continue;

            if (reg_dir->name.empty()) {
                format(s, "{}", ordinal(id));
                select = &s;
            } else {
                select = &reg_dir->name;
            }

            ImGui::PushID(reg_dir);
            if (ImGui::TreeNodeEx(select->c_str())) {
                int i = 0;
                while (i != reg_dir->children.ssize()) {
                    auto  dir_id = reg_dir->children[i];
                    auto* dir    = c_editor.mod.dir_paths.try_to_get(dir_id);

                    if (dir) {
                        show_dirpath_component(c_editor, *reg_dir, *dir, tree);
                        ++i;
                    } else {
                        reg_dir->children.swap_pop_back(i);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::TreeNodeEx("Not saved")) {
            show_notsaved_components(c_editor, tree);
            ImGui::TreePop();
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
