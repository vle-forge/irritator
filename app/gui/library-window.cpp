// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

static void add_generic_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_generic_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.generics.alloc(compo_id);
    app.component_ed.request_to_open(compo_id);
    app.component_sel.update();
}

static void add_grid_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_grid_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.grids.alloc(compo_id, compo.id.grid_id);
    app.component_ed.request_to_open(compo_id);
    app.component_sel.update();
}

static void add_graph_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_graph_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.graphs.alloc(compo_id, compo.id.graph_id);
    app.component_ed.request_to_open(compo_id);
    app.component_sel.update();
}

static void show_component_popup_menu(application& app, component& sel) noexcept
{
    if (ImGui::BeginPopupContextItem()) {
        if (app.mod.can_alloc_generic_component() && app.generics.can_alloc() &&
            ImGui::MenuItem("New generic component"))
            add_generic_component_data(app);

        if (app.mod.can_alloc_grid_component() && app.grids.can_alloc() &&
            ImGui::MenuItem("New grid component"))
            add_grid_component_data(app);

        if (app.mod.can_alloc_graph_component() && app.graphs.can_alloc() &&
            ImGui::MenuItem("New graph component"))
            add_graph_component_data(app);

        ImGui::Separator();

        if (sel.type != component_type::internal) {
            if (ImGui::MenuItem("Copy")) {
                if (app.mod.components.can_alloc()) {
                    auto& new_c = app.mod.components.alloc();
                    new_c.type  = component_type::simple;
                    new_c.name  = sel.name;
                    new_c.state = component_status::modified;
                    if (auto ret = app.mod.copy(sel, new_c); !ret) {
                        auto& n = app.notifications.alloc();
                        n.level = log_level::error;
                        n.title = "Can not alloc a new component";
                        app.notifications.enable(n);
                    }

                    app.component_sel.update();
                } else {
                    auto& n = app.notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Can not alloc a new component";
                    app.notifications.enable(n);
                }
            }

            if (ImGui::MenuItem("Set as main project model")) {
                if (auto ret = app.pj.set(app.mod, app.sim, sel); !ret) {
                    auto& n = app.notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Fail to build tree";
                    app.notifications.enable(n);
                }
            }

            if (auto* file = app.mod.file_paths.try_to_get(sel.file); file) {
                if (ImGui::MenuItem("Delete file")) {
                    auto& n = app.notifications.alloc();
                    n.level = log_level::info;
                    format(n.title = "Remove file `{}'", file->path.sv());

                    auto* reg =
                      app.mod.registred_paths.try_to_get(sel.reg_path);
                    auto* dir = app.mod.dir_paths.try_to_get(sel.dir);

                    if (reg && dir)
                        app.mod.remove_file(*reg, *dir, *file);
                    app.mod.free(sel);

                    app.component_sel.update();
                }
            }
        } else {
            if (ImGui::MenuItem("Copy in generic component")) {
                if (app.mod.components.can_alloc()) {
                    auto& new_c = app.mod.components.alloc();
                    new_c.type  = component_type::simple;
                    new_c.name =
                      internal_component_names[ordinal(sel.id.internal_id)];
                    new_c.state = component_status::modified;
                    if (auto ret = app.mod.copy(
                          enum_cast<internal_component>(sel.id.internal_id),
                          new_c);
                        !ret) {
                        auto& n = app.notifications.alloc();
                        n.level = log_level::error;
                        n.title = "Can not alloc a new component";
                        app.notifications.enable(n);
                    }

                    app.component_sel.update();
                } else {
                    auto& n = app.notifications.alloc();
                    n.level = log_level::error;
                    n.title = "Can not alloc a new component";
                    app.notifications.enable(n);
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
    const typename data_array<T, ID>::value_type* g = nullptr;

    while (data.next(g))
        if (g->get_id() == id)
            return true;

    return false;
}

static void open_component(application& app, component_id id) noexcept
{
    if_data_exists_do(app.mod.components, id, [&](auto& compo) noexcept {
        switch (compo.type) {
        case component_type::none:
            break;

        case component_type::simple:
            if (!is_already_open(app.generics, id) && app.generics.can_alloc())
                if (app.mod.generic_components.try_to_get(compo.id.generic_id))
                    app.generics.alloc(id);
            break;

        case component_type::grid:
            if (!is_already_open(app.grids, id) && app.grids.can_alloc())
                if (app.mod.grid_components.try_to_get(compo.id.grid_id))
                    app.grids.alloc(id, compo.id.grid_id);
            break;

        case component_type::graph:
            if (!is_already_open(app.graphs, id) && app.graphs.can_alloc())
                if (app.mod.graph_components.try_to_get(compo.id.graph_id))
                    app.graphs.alloc(id, compo.id.graph_id);
            break;

        case component_type::internal:
            break;

        default:
            irt_unreachable();
        }

        app.component_ed.request_to_open(id);
    });
}

static void show_file_component(application& app,
                                file_path&   file,
                                component&   c,
                                tree_node*   head) noexcept
{
    const auto id       = app.mod.components.get_id(c);
    const bool selected = head ? id == head->id : false;
    const auto state    = c.state;

    small_string<254> buffer;

    ImGui::PushID(&c);

    if (ImGui::ColorEdit4("Color selection",
                          to_float_ptr(app.mod.component_colors[get_index(id)]),
                          ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_NoAlpha))
        app.mod.component_colors[get_index(id)][3] = 1.f;

    format(buffer, "{} ({})", c.name.sv(), file.path.sv());

    ImGui::SameLine(75.f);
    if (ImGui::Selectable(buffer.c_str(), selected))
        open_component(app, id);
    ImGui::PopID();

    show_component_popup_menu(app, c);

    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);

    switch (state) {
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
        ImGui::TextUnformatted(" (not-saved)");
        break;
    case component_status::unmodified:
        ImGui::SameLine();
        ImGui::TextUnformatted(" (modified)");
        break;
    case component_status::unreadable:
        ImGui::SameLine();
        ImGui::TextUnformatted(" (unreadable)");
        break;
    }

    ImGui::PopStyleColor();
}

static void show_internal_components(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    for_each_data(app.mod.components, [&app](auto& compo) noexcept {
        const auto is_internal = compo.type == component_type::internal;

        if (is_internal) {
            ImGui::PushID(reinterpret_cast<const void*>(&compo));
            ImGui::Selectable(
              internal_component_names[ordinal(compo.id.internal_id)]);
            ImGui::PopID();

            show_component_popup_menu(app, compo);
        }
    });
}

static void show_notsaved_components(irt::component_editor& ed,
                                     irt::tree_node*        head) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    for_each_data(app.mod.components, [&](auto& compo) noexcept {
        const auto is_not_saved =
          compo.type != component_type::internal &&
          app.mod.file_paths.try_to_get(compo.file) == nullptr;

        if (is_not_saved) {
            const auto id       = app.mod.components.get_id(compo);
            const bool selected = head ? id == head->id : false;

            ImGui::PushID(reinterpret_cast<const void*>(&compo));
            if (ImGui::ColorEdit4(
                  "Color selection",
                  to_float_ptr(app.mod.component_colors[get_index(id)]),
                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                    ImGuiColorEditFlags_NoAlpha))
                app.mod.component_colors[get_index(id)][3] = 1.f;

            ImGui::SameLine(50.f);
            if (ImGui::Selectable(compo.name.c_str(), selected))
                open_component(app, id);
            ImGui::PopID();

            show_component_popup_menu(app, compo);
        }
    });
}

static void show_dirpath_component(irt::component_editor& ed,
                                   dir_path&              dir,
                                   irt::tree_node*        head) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (ImGui::TreeNodeEx(dir.path.c_str())) {
        for_each_component(
          app.mod, dir, [&](auto& /*dir*/, auto& file, auto& compo) noexcept {
              show_file_component(app, file, compo, head);
          });

        ImGui::TreePop();
    }
}

static void show_component_library(component_editor& c_editor,
                                   irt::tree_node*   tree) noexcept
{
    auto& app = container_of(&c_editor, &application::component_ed);

    const ImVec2 button_size = ImGui::ComputeButtonSize(3);

    if (ImGui::Button("+generic", button_size))
        add_generic_component_data(app);

    ImGui::SameLine();
    if (ImGui::Button("+grid", button_size))
        add_grid_component_data(app);

    ImGui::SameLine();
    if (ImGui::Button("+graph", button_size))
        add_graph_component_data(app);

    if (ImGui::CollapsingHeader("Components",
                                ImGuiTreeNodeFlags_CollapsingHeader |
                                  ImGuiTreeNodeFlags_DefaultOpen)) {

        for (auto id : app.mod.component_repertories) {
            small_string<31>  s;
            small_string<31>* select;

            auto* reg_dir = app.mod.registred_paths.try_to_get(id);
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
                    auto* dir    = app.mod.dir_paths.try_to_get(dir_id);

                    if (dir) {
                        show_dirpath_component(c_editor, *dir, tree);
                        ++i;
                    } else {
                        reg_dir->children.swap_pop_back(i);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (ImGui::TreeNodeEx("Internal", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_internal_components(c_editor);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_notsaved_components(c_editor, tree);
            ImGui::TreePop();
        }
    }
}

void library_window::show() noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (!ImGui::Begin(library_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto* tree = app.pj.tn_head();

    show_component_library(app.component_ed, tree);

    // @TODO hidden part. Need to be fix to enable configure/observation
    // of project. ImGui::Separator(); show_input_output(*this);
    // ImGui::Separator();
    // show_selected_children(*this);

    ImGui::End();
}

} // namespace irt
