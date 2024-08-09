// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
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

    app.add_gui_task([&app]() noexcept {
        std::scoped_lock lock(app.mod_mutex);
        app.component_sel.update();
    });
}

static void add_grid_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_grid_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.grids.alloc(compo_id, compo.id.grid_id);
    app.component_ed.request_to_open(compo_id);
    app.add_gui_task([&app]() noexcept {
        std::scoped_lock lock(app.mod_mutex);
        app.component_sel.update();
    });
}

static void add_graph_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_graph_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.graphs.alloc(compo_id, compo.id.graph_id);
    app.component_ed.request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept {
        std::scoped_lock lock(app.mod_mutex);
        app.component_sel.update();
    });
}

static void add_hsm_component_data(application& app) noexcept
{
    auto& compo    = app.mod.alloc_hsm_component();
    auto  compo_id = app.mod.components.get_id(compo);
    app.hsms.alloc(
      compo_id, compo.id.hsm_id, app.mod.hsm_components.get(compo.id.hsm_id));
    app.component_ed.request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept {
        std::scoped_lock lock(app.mod_mutex);
        app.component_sel.update();
    });
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

        if (app.mod.can_alloc_hsm_component() && app.hsms.can_alloc() &&
            ImGui::MenuItem("New HSM component"))
            add_hsm_component_data(app);

        ImGui::Separator();

        if (sel.type != component_type::internal) {
            if (ImGui::MenuItem("Copy")) {
                if (app.mod.components.can_alloc()) {
                    auto& new_c = app.mod.components.alloc();
                    new_c.type  = component_type::simple;
                    new_c.name  = sel.name;
                    new_c.state = component_status::modified;

                    if (auto ret = app.mod.copy(sel, new_c); !ret) {
                        app.notifications.try_insert(
                          log_level::error,
                          [](auto& title, auto& msg) noexcept {
                              title = "Library";
                              msg   = "Fail to copy model";
                          });
                    }

                    app.add_gui_task([&app]() noexcept {
                        std::scoped_lock lock(app.mod_mutex);
                        app.component_sel.update();
                    });
                } else {
                    app.notifications.try_insert(
                      log_level::error, [&](auto& title, auto& msg) noexcept {
                          title = "Library";
                          format(msg,
                                 "Can not alloc a new component ({})",
                                 app.mod.components.capacity());
                      });
                }
            }

            if (ImGui::MenuItem("Set as main project model")) {
                const auto compo_id = app.mod.components.get_id(sel);
                app.library_wnd.try_set_component_as_project(compo_id);
            }

            if (auto* file = app.mod.file_paths.try_to_get(sel.file); file) {
                if (ImGui::MenuItem("Delete file")) {
                    app.add_gui_task([&app,
                                      r = sel.reg_path,
                                      d = sel.dir,
                                      f = sel.file]() noexcept {
                        auto& n = app.notifications.alloc(log_level::notice);
                        n.title = "Remove file";

                        {
                            auto* reg  = app.mod.registred_paths.try_to_get(r);
                            auto* dir  = app.mod.dir_paths.try_to_get(d);
                            auto* file = app.mod.file_paths.try_to_get(f);

                            if (reg and dir and file) {
                                std::scoped_lock lock(dir->mutex);

                                format(n.message,
                                       "File `{}' removed",
                                       file->path.sv());
                                app.mod.remove_file(*reg, *dir, *file);
                            }
                        }

                        app.notifications.enable(n);
                    });

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
                }
            } else {
                if (ImGui::MenuItem("Delete component")) {
                    app.add_gui_task([&]() noexcept {
                        const auto compo_id = app.mod.components.get_id(sel);
                        auto& n = app.notifications.alloc(log_level::notice);
                        n.title = "Remove component";

                        if_data_exists_do(
                          app.mod.components, compo_id, [&](auto& compo) {
                              app.mod.free(compo);
                          });

                        app.notifications.enable(n);
                    });

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
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

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
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

        case component_type::hsm:
            if (!is_already_open(app.hsms, id) && app.hsms.can_alloc())
                if (auto* h =
                      app.mod.hsm_components.try_to_get(compo.id.hsm_id);
                    h)
                    app.hsms.alloc(id, compo.id.hsm_id, *h);
            break;

        case component_type::internal:
            break;
        }

        app.component_ed.request_to_open(id);
    });
}

static void show_file_component(application& app,
                                file_path&   file,
                                component&   c,
                                tree_node*   head) noexcept
{
    if (std::unique_lock lock(file.mutex, std::try_to_lock); lock.owns_lock()) {
        const auto id       = app.mod.components.get_id(c);
        const bool selected = head ? id == head->id : false;
        const auto state    = c.state;

        small_string<254> buffer;

        ImGui::PushID(&c);

        if (ImGui::ColorEdit4(
              "Color selection",
              to_float_ptr(app.mod.component_colors[get_index(id)]),
              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoAlpha))
            app.mod.component_colors[get_index(id)][3] = 1.f;

        format(buffer, "{} ({})", c.name.sv(), file.path.sv());

        ImGui::SameLine(75.f);
        if (ImGui::Selectable(buffer.c_str(),
                              selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsMouseDoubleClicked(0))
                app.library_wnd.try_set_component_as_project(id);
            else
                open_component(app, id);
        }
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
    } else {
        ImGui::TextDisabled("file is being updated");
    }
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
            if (ImGui::Selectable(compo.name.c_str(),
                                  selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0))
                    app.library_wnd.try_set_component_as_project(id);
                else
                    open_component(app, id);
            }
            ImGui::PopID();

            show_component_popup_menu(app, compo);
        }
    });
}

static void show_dirpath_component(irt::component_editor& ed,
                                   dir_path&              dir,
                                   irt::tree_node*        head) noexcept
{
    if (std::unique_lock lock(dir.mutex, std::try_to_lock); lock.owns_lock()) {
        auto& app = container_of(&ed, &application::component_ed);

        if (ImGui::TreeNodeEx(dir.path.c_str())) {
            for_each_component(
              app.mod,
              dir,
              [&](auto& /*dir*/, auto& file, auto& compo) noexcept {
                  show_file_component(app, file, compo, head);
              });

            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("The directory is being updated");
    }
}

static void show_component_library_new_component(application& app) noexcept
{
    const auto button_size = ImGui::ComputeButtonSize(4);

    if (ImGui::Button("+generic", button_size))
        add_generic_component_data(app);

    ImGui::SameLine();
    if (ImGui::Button("+grid", button_size))
        add_grid_component_data(app);

    ImGui::SameLine();
    if (ImGui::Button("+graph", button_size))
        add_graph_component_data(app);

    ImGui::SameLine();
    if (ImGui::Button("+hsm", button_size))
        add_hsm_component_data(app);
}

static void show_repertories_components(irt::application& app,
                                        irt::tree_node*   tree) noexcept
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
                    show_dirpath_component(app.component_ed, *dir, tree);
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

static void show_component_library(application&    app,
                                   irt::tree_node* tree) noexcept
{
    if (not ImGui::BeginChild("##component_library",
                              ImGui::GetContentRegionAvail())) {
        ImGui::EndChild();
    } else {
        show_repertories_components(app, tree);

        if (ImGui::TreeNodeEx("Internal")) {
            show_internal_components(app.component_ed);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_notsaved_components(app.component_ed, tree);
            ImGui::TreePop();
        }

        ImGui::EndChild();
    }
}

void library_window::try_set_component_as_project(
  const component_id compo_id) noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (not any_equal(app.simulation_ed.simulation_state,
                      simulation_status::finished,
                      simulation_status::not_started)) {
        app.notifications.try_insert(
          log_level::error, [&](auto& title, auto& msg) noexcept {
              title = "Project import error";
              msg =
                "A simulation is currently running. Stop the simulation "
                "before importing a new component as project main component.";
          });
    } else {
        // Perhaps add a new simulation status: to avoid import of a component.
        app.add_gui_task([&app, compo_id]() noexcept {
            std::scoped_lock lock{ app.sim_mutex, app.mod_mutex, app.pj_mutex };

            attempt_all(
              [&]() noexcept -> status {
                  if (auto* c = app.mod.components.try_to_get(compo_id); c) {
                      app.simulation_ed.clear();
                      return app.pj.set(app.mod, app.sim, *c);
                  }

                  return success();
              },

              [&](project::part part, project::error error) noexcept {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Project import error";
                  format(n.message,
                         "Error in {} failed with error: {}",
                         to_string(part),
                         to_string(error));
              },

              [&](project::part part) noexcept {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Project import error";
                  format(n.message, "Error in {}", to_string(part));
              },

              [&](project::error error) noexcept {
                  auto& n = app.notifications.alloc(log_level::error);
                  n.title = "Project import error";
                  format(n.message, "Error: {}", to_string(error));
              },

              [&]() noexcept {
                  auto& n = app.notifications.alloc();
                  n.level = log_level::error;
                  n.title = "Fail to build tree";
                  app.notifications.enable(n);
              });
        });
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

    show_component_library_new_component(app);
    show_component_library(app, tree);

    ImGui::End();
}

} // namespace irt
