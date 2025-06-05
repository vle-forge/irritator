// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

static bool can_delete_component(application& app, component_id id) noexcept
{
    switch (app.library_wnd.is_component_deletable(app, id)) {
    case library_window::is_component_deletable_t::deletable:
        return true;

    case library_window::is_component_deletable_t::used_by_component:
        app.jn.push(log_level::info, [](auto& title, auto& msg) noexcept {
            title = "Can not delete this component";
            msg   = "This component is used in another component";
        });
        break;

    case library_window::is_component_deletable_t::used_by_project:
        app.jn.push(log_level::info, [](auto& title, auto& msg) noexcept {
            title = "Can not delete this component";
            msg   = "This component is used in project";
        });
        break;
    }

    return false;
}

static void show_component_popup_menu(application& app, component& sel) noexcept
{
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("New generic component"))
            app.component_ed.add_generic_component_data();

        if (ImGui::MenuItem("New grid component"))
            app.component_ed.add_grid_component_data();

        if (ImGui::MenuItem("New graph component"))
            app.component_ed.add_graph_component_data();

        if (ImGui::MenuItem("New HSM component"))
            app.component_ed.add_hsm_component_data();

        ImGui::Separator();

        if (sel.type != component_type::internal) {
            if (ImGui::MenuItem("Copy")) {
                if (app.mod.components.can_alloc(1)) {
                    auto  new_c_id = app.mod.components.alloc();
                    auto& new_c = app.mod.components.get<component>(new_c_id);
                    new_c.type  = component_type::generic;
                    new_c.name  = sel.name;
                    new_c.state = component_status::modified;

                    if (auto ret = app.mod.copy(sel, new_c); !ret) {
                        app.jn.push(log_level::error,
                                    [](auto& title, auto& msg) noexcept {
                                        title = "Library";
                                        msg   = "Fail to copy model";
                                    });
                    }

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
                } else {
                    app.jn.push(log_level::error,
                                [&](auto& title, auto& msg) noexcept {
                                    title = "Library";
                                    format(msg,
                                           "Can not alloc a new component ({})",
                                           app.mod.components.capacity());
                                });
                }
            }

            if (ImGui::MenuItem("Set as main project model")) {
                const auto compo_id = app.mod.components.get_id(sel);
                app.library_wnd.try_set_component_as_project(app, compo_id);
            }

            if (auto* file = app.mod.file_paths.try_to_get(sel.file); file) {
                if (ImGui::MenuItem("Delete component and file")) {
                    const auto id = app.mod.components.get_id(sel);
                    if (can_delete_component(app, id)) {
                        app.component_ed.close(app.mod.components.get_id(sel));

                        app.add_gui_task([&app, id = sel.file]() noexcept {
                            if (auto* f = app.mod.file_paths.try_to_get(id)) {
                                const auto compo_id = f->component;

                                if (app.mod.components.exists(compo_id)) {
                                    const auto name =
                                      app.mod.components
                                        .get<component>(compo_id)
                                        .name.sv();

                                    app.jn.push(
                                      log_level::notice,
                                      [&](auto& title, auto& msg) noexcept {
                                          title = "Remove component file";
                                          format(msg,
                                                 "File `{}' and component {} "
                                                 "removed",
                                                 f->path.sv(),
                                                 name);
                                      });
                                } else {
                                    app.jn.push(
                                      log_level::notice,
                                      [&](auto& title, auto& msg) noexcept {
                                          title = "Remove component file";
                                          format(msg,
                                                 "File `{}' removed",
                                                 f->path.sv());
                                      });
                                }

                                app.mod.remove_file(*f);
                                app.mod.components.free(compo_id);
                            }
                        });

                        app.add_gui_task(
                          [&app]() noexcept { app.component_sel.update(); });
                    }
                }
            } else {
                if (ImGui::MenuItem("Delete component")) {
                    const auto id = app.mod.components.get_id(sel);
                    if (can_delete_component(app, id)) {
                        const auto compo_id = app.mod.components.get_id(sel);
                        app.component_ed.close(compo_id);

                        app.add_gui_task(
                          [&app, compo_id, id = sel.file]() noexcept {
                              app.jn.push(
                                log_level::notice,
                                [&](auto& title, auto& /*msg*/) noexcept {
                                    title = "Remove component";
                                });

                              if (auto* f = app.mod.file_paths.try_to_get(id))
                                  app.mod.remove_file(*f);

                              if (auto* c =
                                    app.mod.components.try_to_get<component>(
                                      compo_id))
                                  app.mod.free(*c);
                          });

                        app.add_gui_task(
                          [&app]() noexcept { app.component_sel.update(); });
                    }
                }
            }
        } else {
            if (ImGui::MenuItem("Copy in generic component")) {
                if (app.mod.components.can_alloc(1)) {
                    auto  new_c_id = app.mod.components.alloc();
                    auto& new_c = app.mod.components.get<component>(new_c_id);
                    new_c.type  = component_type::generic;
                    new_c.name =
                      internal_component_names[ordinal(sel.id.internal_id)];
                    new_c.state = component_status::modified;
                    if (auto ret = app.mod.copy(
                          enum_cast<internal_component>(sel.id.internal_id),
                          new_c);
                        !ret) {
                        app.jn.push(log_level::error, [](auto& t, auto& m) {
                            t = "Library: copy in generic component";
                            m = "TODO";
                        });
                    }

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
                } else {
                    app.jn.push(log_level::error, [](auto& t, auto& m) {
                        t = "Library: copy in generic component";
                        m = "Can not allocate a new component";
                    });
                }
            }
        }

        ImGui::EndPopup();
    }
}

static void show_file_component(application& app,
                                file_path&   file,
                                component&   c) noexcept
{
    if (std::unique_lock lock(file.mutex, std::try_to_lock); lock.owns_lock()) {
        const auto id       = app.mod.components.get_id(c);
        const bool selected = app.component_ed.is_component_open(id);
        const auto state    = c.state;

        small_string<254> buffer;

        ImGui::PushID(&c);

        const auto col = get_component_color(app, id);
        auto       im  = std::array<float, 4>{ col[0], col[1], col[2], col[3] };

        if (ImGui::ColorEdit4("Color selection",
                              im.data(),
                              ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel)) {
            if (app.mod.components.exists(id)) {
                auto& data = app.mod.components.get<component_color>(id);
                data       = im;
            }
        }

        format(buffer, "{} ({})", c.name.sv(), file.path.sv());
        ImGui::SameLine(75.f);
        if (state == component_status::unreadable) {
            ImGui::TextDisabled("%s", buffer.c_str());
        } else {
            if (ImGui::Selectable(buffer.c_str(),
                                  selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0))
                    app.library_wnd.try_set_component_as_project(app, id);
                else
                    app.component_ed.request_to_open(id);
            }
            show_component_popup_menu(app, c);
        }
        ImGui::PopID();

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
    } else {
        ImGui::TextDisabled("file is being updated");
    }
}

static void show_internal_components(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto& vec = app.mod.components.get<component>();
    for (const auto id : app.mod.components) {
        auto&      compo       = vec[id];
        const auto is_internal = compo.type == component_type::internal;

        if (is_internal) {
            ImGui::PushID(get_index(id));
            ImGui::Selectable(
              internal_component_names[ordinal(compo.id.internal_id)]);
            ImGui::PopID();

            show_component_popup_menu(app, compo);
        }
    }
}

static void show_notsaved_components(irt::component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto& compos = app.mod.components.get<component>();
    auto& colors = app.mod.components.get<component_color>();
    for (const auto id : app.mod.components) {
        auto& compo = compos[id];

        const auto is_not_saved =
          compo.type != component_type::internal &&
          app.mod.file_paths.try_to_get(compo.file) == nullptr;

        if (is_not_saved) {
            auto&      color    = colors[id];
            const bool selected = app.component_ed.is_component_open(id);

            ImGui::PushID(reinterpret_cast<const void*>(&compo));
            ImGui::ColorEdit4("Color selection",
                              to_float_ptr(color),
                              ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel);

            ImGui::SameLine(50.f);
            if (ImGui::Selectable(compo.name.c_str(),
                                  selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0))
                    app.library_wnd.try_set_component_as_project(app, id);
                else
                    app.component_ed.request_to_open(id);
            }
            ImGui::PopID();

            show_component_popup_menu(app, compo);
        }
    }
}

static void show_dirpath_component(irt::component_editor& ed,
                                   dir_path&              dir) noexcept
{
    if (std::unique_lock lock(dir.mutex, std::try_to_lock); lock.owns_lock()) {
        auto& app = container_of(&ed, &application::component_ed);

        if (ImGui::TreeNodeEx(dir.path.c_str())) {
            for_each_component(
              app.mod,
              dir,
              [&](auto& /*dir*/, auto& file, auto& compo) noexcept {
                  show_file_component(app, file, compo);
              });

            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("The directory is being updated");
    }
}

static void show_repertories_components(irt::application& app) noexcept
{
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
                    show_dirpath_component(app.component_ed, *dir);
                    ++i;
                } else {
                    reg_dir->children.swap_pop_back(i);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

static void show_component_library(application& app) noexcept
{
    if (not ImGui::BeginChild("##component_library",
                              ImGui::GetContentRegionAvail())) {
        ImGui::EndChild();
    } else {
        show_repertories_components(app);

        if (ImGui::TreeNodeEx("Internal")) {
            show_internal_components(app.component_ed);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_notsaved_components(app.component_ed);
            ImGui::TreePop();
        }

        ImGui::EndChild();
    }
}

void library_window::try_set_component_as_project(
  application&       app,
  const component_id compo_id) noexcept
{
    if (auto opt = app.alloc_project_window(); opt.has_value()) {
        const auto id = *opt;

        app.add_gui_task([&app, compo_id, id]() noexcept {
            auto& pj = app.pjs.get(id);
            if (auto* c = app.mod.components.try_to_get<component>(compo_id);
                c) {
                if (not pj.pj.set(app.mod, *c)) {
                    app.jn.push(log_level::error, [](auto& title, auto& msg) {
                        title = "Project failure",
                        msg   = "Fail to import component as project";
                    });
                }
                pj.disable_access = false;
            }
        });
    }
}

static auto is_component_used_in_components(const application& app,
                                            const component_id id) noexcept
  -> bool
{
    for (const auto c_id : app.mod.components) {
        const auto& c = app.mod.components.get<component>(c_id);

        switch (c.type) {
        case component_type::generic:
            if (const auto* g =
                  app.mod.generic_components.try_to_get(c.id.generic_id)) {
                if (std::ranges::any_of(
                      g->children, [id](const auto& ch) noexcept -> bool {
                          return ch.type == child_type::component and
                                 ch.id.compo_id == id;
                      }))
                    return true;
            }
            break;
        case component_type::grid:
            if (const auto* g =
                  app.mod.grid_components.try_to_get(c.id.grid_id)) {
                if (std::ranges::any_of(
                      g->children(),
                      [id](const auto c) noexcept -> bool { return c == id; }))
                    return true;
            }
            break;
        case component_type::graph:
            if (const auto* g =
                  app.mod.graph_components.try_to_get(c.id.graph_id)) {
                for (const auto i : g->g.nodes) {
                    if (g->g.node_components[i] == id)
                        return true;
                }
            }
            break;
        case component_type::hsm:
        case component_type::internal:
        case component_type::none:
            break;
        }
    }

    return false;
};

static auto is_component_used_in_projects(const application&  app,
                                          const component_id  id,
                                          vector<tree_node*>& stack) noexcept
  -> bool
{
    for (const auto& pj : app.pjs) {
        stack.clear();
        auto* head = pj.pj.tn_head();
        if (head->id == id)
            return true;

        stack.push_back(head);
        while (not stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();

            if (cur->id == id)
                return true;

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child(); child)
                stack.emplace_back(child);
        }
    }

    return false;
}

auto library_window::is_component_deletable(const application& app,
                                            const component_id id) noexcept
  -> is_component_deletable_t
{
    return is_component_used_in_components(app, id)
             ? is_component_deletable_t::used_by_component
           : is_component_used_in_projects(app, id, stack)
             ? is_component_deletable_t::used_by_project
             : is_component_deletable_t::deletable;
}

void library_window::show() noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (!ImGui::Begin(
          library_window::name, &is_open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("New")) {
            if (ImGui::MenuItem("generic component"))
                app.component_ed.add_generic_component_data();

            if (ImGui::MenuItem("grid component"))
                app.component_ed.add_grid_component_data();

            if (ImGui::MenuItem("graph component"))
                app.component_ed.add_graph_component_data();

            if (ImGui::MenuItem("hsm component"))
                app.component_ed.add_hsm_component_data();
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    show_component_library(app);

    ImGui::End();
}

} // namespace irt
