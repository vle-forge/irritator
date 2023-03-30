// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

using component_popup =
  std::variant<std::monostate, component*, internal_component>;

static void show_component_popup_menu(irt::component_editor& ed,
                                      component_popup        selection) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New generic component")) {
            auto& compo = app->mod.alloc_simple_component();

            if (app->generics.can_alloc()) {
                auto& wnd = app->generics.alloc();
                wnd.id    = app->mod.components.get_id(compo);
                app->component_ed.request_to_open = wnd.id;
            }
        }

        if (ImGui::MenuItem("New grid component")) {
            auto& compo = app->mod.alloc_grid_component();
            if (app->grids.can_alloc()) {
                auto& wnd = app->grids.alloc();
                wnd.id    = app->mod.components.get_id(compo);
                app->component_ed.request_to_open = wnd.id;
            }
        }

        if (auto** compo = std::get_if<component*>(&selection); compo) {
            if (ImGui::MenuItem("Copy")) {
                if (app->mod.components.can_alloc()) {
                    auto& new_c = app->mod.components.alloc();
                    new_c.type  = component_type::simple;
                    new_c.name  = (*compo)->name;
                    new_c.state = component_status::modified;
                    app->mod.copy(*(*compo), new_c);
                } else {
                    auto& n = app->notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Can not alloc a new component";
                    app->notifications.enable(n);
                }
            }

            if (ImGui::MenuItem("Set at main project model")) {
                tree_node_id out = undefined<tree_node_id>();

                if (auto ret = project_init(app->main, app->mod, **compo);
                    is_bad(ret)) {
                    auto& n = app->notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Fail to build tree";
                    app->notifications.enable(n);
                } else {
                    app->pj.m_parent = out;
                    app->pj.m_compo  = app->mod.components.get_id(**compo);
                    app->simulation_ed.simulation_copy_modeling();
                }
            }
        }

        if (auto* compo = std::get_if<internal_component>(&selection); compo) {
            if (ImGui::MenuItem("Copy")) {
                if (app->mod.components.can_alloc()) {
                    auto& new_c = app->mod.components.alloc();
                    new_c.type  = component_type::simple;
                    new_c.name  = internal_component_names[ordinal(*compo)];
                    new_c.state = component_status::modified;
                    app->mod.copy(*compo, new_c);
                } else {
                    auto& n = app->notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Can not alloc a new component";
                    app->notifications.enable(n);
                }
            }
        }

        ImGui::EndPopup();
    }
}

template<typename T, typename ID>
static bool is_already_open(const data_array<T, ID>& data,
                            component_id             id) noexcept
{
    typename data_array<T, ID>::value_type* g = nullptr;

    while (data.next(g))
        if (g->id == id)
            return true;

    return false;
}

static void open_component(application& app, component_id id) noexcept
{
    if (auto* compo = app.mod.components.try_to_get(id); compo) {
        switch (compo->type) {
        case component_type::none:
            break;

        case component_type::simple: {
            if (!is_already_open(app.generics, id)) {
                auto* gen =
                  app.mod.simple_components.try_to_get(compo->id.simple_id);
                if (gen && app.generics.can_alloc()) {
                    auto& g = app.generics.alloc();
                    g.id    = id;
                }
            }
            app.component_ed.request_to_open = id;
        } break;

        case component_type::grid: {
            if (!is_already_open(app.grids, id)) {
                auto* grid =
                  app.mod.grid_components.try_to_get(compo->id.grid_id);
                if (grid && app.grids.can_alloc()) {
                    auto& g = app.grids.alloc();
                    g.load(id, *grid);
                }
            }
            app.component_ed.request_to_open = id;
        } break;

        case component_type::internal:
            break;
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
    auto*      app        = container_of(&ed, &application::component_ed);
    const auto id         = app->mod.components.get_id(c);
    const bool selected   = head ? id == head->id : false;
    bool       is_deleted = false;

    if (ImGui::Selectable(file.path.c_str(), selected))
        open_component(*app, id);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Copy")) {
            if (app->mod.components.can_alloc()) {
                auto& new_c = app->mod.components.alloc();
                new_c.type  = component_type::simple;
                new_c.name  = c.name;
                new_c.state = component_status::modified;
                app->mod.copy(c, new_c);
            } else {
                auto& n = app->notifications.alloc();
                n.level = log_level::error;
                n.title = "Can not alloc a new component";
                app->notifications.enable(n);
            }
        }

        if (ImGui::MenuItem("Set at main project model")) {
            tree_node_id out = undefined<tree_node_id>();

            if (auto ret = project_init(app->main, app->mod, c); is_bad(ret)) {
                auto& n = app->notifications.alloc();
                n.level = log_level::error;
                n.title = "Fail to build tree";
                app->notifications.enable(n);
            } else {
                app->pj.m_parent = out;
                app->pj.m_compo  = app->mod.components.get_id(c);
            }
        }

        if (ImGui::MenuItem("Delete file")) {
            auto& n = app->notifications.alloc();
            n.level = log_level::info;
            format(n.title = "Remove file `{}'", file.path.sv());

            app->mod.remove_file(reg, dir, file);
            app->mod.free(c);
            is_deleted = true;
        }

        ImGui::EndPopup();
    }

    if (!is_deleted) {
        switch (c.state) {
        case component_status::unread:
            ImGui::SameLine();
            ImGui::TextUnformatted(" (unread)");
            break;
        case component_status::read_only:
            ImGui::SameLine();
            ImGui::TextUnformatted(" (read-only)");
            break;
        case component_status::modified:
            ImGui::SameLine();
            ImGui::TextUnformatted(" (modified)");
            break;
        case component_status::unmodified:
            break;
        case component_status::unreadable:
            ImGui::SameLine();
            ImGui::TextUnformatted(" (unreadable)");
            break;
        }
    }

    return is_deleted;
}

static void show_notsaved_components(irt::component_editor& ed,
                                     irt::tree_node*        head) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    component* compo = nullptr;
    while (app->mod.components.next(compo)) {
        const auto is_not_saved =
          app->mod.file_paths.try_to_get(compo->file) == nullptr;

        if (is_not_saved) {
            const auto id       = app->mod.components.get_id(*compo);
            const bool selected = head ? id == head->id : false;

            ImGui::PushID(compo);
            if (ImGui::Selectable(compo->name.c_str(), selected)) {
                open_component(*app, id);
            }
            ImGui::PopID();

            show_component_popup_menu(ed, compo);
        }
    }
}

static void show_internal_components(irt::component_editor& ed) noexcept
{
    constexpr int nb = length(internal_component_names);
    for (int i = 0; i < nb; ++i) {
        ImGui::Selectable(internal_component_names[i]);

        show_component_popup_menu(ed, enum_cast<internal_component>(i));
    }
}

static void show_dirpath_component(irt::component_editor& ed,
                                   registred_path&        reg,
                                   dir_path&              dir,
                                   irt::tree_node*        head) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    if (ImGui::TreeNodeEx(dir.path.c_str())) {
        int j = 0;
        while (j < dir.children.ssize()) {
            auto  file_id = dir.children[j];
            auto* file    = app->mod.file_paths.try_to_get(file_id);

            if (file) {
                auto  compo_id = file->component;
                auto* compo    = app->mod.components.try_to_get(compo_id);

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

static void show_component_library(component_editor& c_editor,
                                   irt::tree_node*   tree) noexcept
{
    if (ImGui::CollapsingHeader("Component library",
                                ImGuiTreeNodeFlags_CollapsingHeader |
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* app = container_of(&c_editor, &application::component_ed);

        if (ImGui::TreeNodeEx("Internal")) {
            show_internal_components(c_editor);
            ImGui::TreePop();
        }

        for (auto id : app->mod.component_repertories) {
            small_string<32>  s;
            small_string<32>* select;

            auto* reg_dir = app->mod.registred_paths.try_to_get(id);
            if (!reg_dir || reg_dir->status == registred_path::state::error)
                continue;

            if (reg_dir->name.empty()) {
                format(s, "{}", ordinal(id));
                select = &s;
            } else {
                select = &reg_dir->name;
            }

            ImGui::PushID(reg_dir);
            if (ImGui::TreeNodeEx(select->c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
                int i = 0;
                while (i != reg_dir->children.ssize()) {
                    auto  dir_id = reg_dir->children[i];
                    auto* dir    = app->mod.dir_paths.try_to_get(dir_id);

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

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_notsaved_components(c_editor, tree);
            ImGui::TreePop();
        }
    }
}

void library_window::show() noexcept
{
    auto* app = container_of(this, &application::library_wnd);

    if (!ImGui::Begin(library_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto* tree = app->main.tree_nodes.try_to_get(app->main.tn_head);

    show_component_library(app->component_ed, tree);

    // @TODO hidden part. Need to be fix to enable configure/observation of
    // project.
    // ImGui::Separator();
    // show_input_output(*this);
    // ImGui::Separator();
    // show_selected_children(*this);

    ImGui::End();
}

} // namespace irt
