// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

// static void remove_component_and_file_task(application&       app,
//                                            const file_path_id id) noexcept
// {
//     app.add_gui_task([&app, id]() noexcept {
//         app.mod.files.write([&](auto& fs) noexcept {
//             if (const auto* f = fs.file_paths.try_to_get(id)) {
//                 fs.remove_file(id);

//                 app.jn.push(log_level::notice,
//                             [&](auto& title, auto& msg) noexcept {
//                                 title = "Remove component file";
//                                 format(msg, "File `{}' removed",
//                                 f->path.sv());
//                             });
//             }
//         });
//     });

//     app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
// }

// static void refresh_component_list_task(application& app) noexcept
// {
//     app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
// }

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

static void show_component_popup_menu(application& app,
                                      const file_access& /*fs*/,
                                      const component_id compo_id,
                                      const component&   sel) noexcept
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

        if (ImGui::MenuItem("New simulation component"))
            app.component_ed.add_simulation_component_data();

        ImGui::Separator();

        if (ImGui::MenuItem("Copy")) {
            if (app.mod.can_alloc_component(1)) {
                app.add_gui_task([&app, compo_id, name = sel.name]() noexcept {
                    app.mod.ids.write([&](auto& ids) noexcept {
                        const auto c = ids.copy(compo_id);
                        if (c.has_error())
                            app.jn.push(log_level::error,
                                        [](auto& title, auto& msg) noexcept {
                                            title = "Library";
                                            msg   = "Fail to copy model";
                                        });
                    });
                });

                app.add_gui_task(
                  [&app]() noexcept { app.component_sel.update(); });
            } else {
                app.jn.push(log_level::error,
                            [&](auto& title, auto& msg) noexcept {
                                title = "Library";
                                format(msg, "Can not alloc a new component");
                            });
            }
        }

        if (ImGui::MenuItem("Set as main project model")) {
            app.library_wnd.try_set_component_as_project(app, compo_id);
        }

        if (ImGui::MenuItem("Delete component")) {
            if (can_delete_component(app, compo_id)) {
                app.component_ed.close(compo_id);

                app.add_gui_task([&app, compo_id]() noexcept {
                    app.mod.ids.write([&](auto& ids) noexcept {
                        const auto& compo = ids.component_file_paths[compo_id];
                        if (is_undefined(compo.parent))
                            return;

                        if (compo.path.sv().empty())
                            return;

                        app.mod.files.write([&](auto& fs) noexcept {
                            const auto file = make_file(fs, compo);
                            if (file.has_value()) {
                                std::error_code ec;

                                if (std::filesystem::exists(*file, ec)) {
                                    std::filesystem::remove(*file, ec);
                                    app.jn.push(
                                      log_level::notice,
                                      [&](auto& title, auto& msg) noexcept {
                                          title = "Remove component file";
                                          format(msg,
                                                 "File `{}' removed",
                                                 file->string());
                                      });
                                }
                            }
                        });

                        ids.free(compo_id);
                    });
                });

                app.add_gui_task(
                  [&app, compo_id]() { app.component_sel.update(); });
            }
        }

        ImGui::EndPopup();
    }
}

void library_window::show_file_project(const file_access& fs,
                                       const file_path_id file_id) noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    const auto* file = fs.file_paths.try_to_get(file_id);
    const auto* name = file->path.c_str();
    auto*       pj   = app.pjs.try_to_get(file->pj_id);

    if (ImGui::Selectable(name, pj != nullptr, false)) {
        if (pj) {
            ImGui::SetWindowFocus(pj->title.c_str());
        } else {
            const auto pj_id = app.open_project_window(file_id);

            if (app.pjs.try_to_get(pj_id)) {
                app.add_gui_task([&app, pj_id, file_id]() noexcept {
                    auto* pj = app.pjs.try_to_get(pj_id);

                    app.mod.files.write([&](auto& fs) noexcept {
                        auto* file = fs.file_paths.try_to_get(file_id);
                        if (file) {
                            pj->pj.file = file_id;
                            file->pj_id = pj_id;
                        }
                    });

                    if (auto ret = pj->pj.load(app.mod); ret.has_error()) {
                        app.jn.push(log_level::error,
                                    [](auto& title, auto& msg) {
                                        title = "Project failure",
                                        msg   = "Fail to load project";
                                    });
                    } else {
                        pj->disable_access = false;
                    }
                });
            }
        }
    }

    if (ImGui::BeginPopupContextItem()) {
        if (pj) {
            if (ImGui::MenuItem("Close project")) {
                const auto pj_id = app.pjs.get_id(*pj);
                app.close_project_window(pj_id);
            }
        } else {
            if (ImGui::MenuItem("Open project")) {
                const auto pj_id = app.open_project_window(file_id);

                if (app.pjs.try_to_get(pj_id)) {
                    app.add_gui_task([&app, pj_id, file_id]() noexcept {
                        if (auto* pj = app.pjs.try_to_get(pj_id)) {
                            app.mod.files.write([&](auto& fs) noexcept {
                                auto* file  = fs.file_paths.try_to_get(file_id);
                                pj->pj.file = file_id;
                                file->pj_id = pj_id;

                                if (auto ret = pj->pj.load(app.mod);
                                    ret.has_error()) {
                                    app.jn.push(log_level::error,
                                                [](auto& title, auto& msg) {
                                                    title = "Project failure",
                                                    msg =
                                                      "Fail to load project";
                                                });
                                } else {
                                    pj->disable_access = false;
                                }
                            });
                        }
                    });
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete file")) {
                app.mod.files.read([&](const auto& fs, const auto /*vers*/) {
                    if (auto* file = fs.file_paths.try_to_get(file_id)) {
                        if (auto* pj = app.pjs.try_to_get(file->pj_id)) {
                            app.close_project_window(app.pjs.get_id(*pj));
                        }

                        app.add_gui_task([&app, &file_id]() noexcept {
                            app.mod.files.write([&](auto& fs) noexcept {
                                if (const auto file =
                                      fs.file_paths.try_to_get(file_id))
                                    fs.remove_file(file_id);
                            });
                        });
                    }
                });
            }
        }

        ImGui::EndPopup();
    }
}

void library_window::show_file_component(const file_access&      fs,
                                         const component_access& ids,
                                         const component_id      id) noexcept
{
    auto&       app      = container_of(this, &application::library_wnd);
    const bool  selected = app.component_ed.is_component_open(id);
    const auto& compo    = ids.components[id];
    const auto& file     = ids.component_file_paths[id];
    const auto  state    = compo.state;

    ImGui::PushID(ordinal(id));

    component_color im = ids.component_colors[id];

    if (ImGui::ColorEdit4("Color selection",
                          im.data(),
                          ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel)) {
        if (ids.exists(id)) {
            app.add_gui_task([&app, id, im]() noexcept {
                app.mod.ids.write([&](auto& ids) noexcept {
                    if (ids.exists(id)) {
                        ids.component_colors[id] = im;
                    }
                });
            });
        }
    }

    ImGui::SameLine();

    const auto& c       = ids.components[id];
    const auto  name    = format_n<63>("{} ({})", c.name.sv(), file.path.sv());
    const auto  disable = state == component_status::unreadable;
    const auto  flags   = disable ? ImGuiSelectableFlags_Disabled
                                  : ImGuiSelectableFlags_AllowDoubleClick;

    if (ImGui::Selectable(name.c_str(), selected, flags)) {
        if (ImGui::IsMouseDoubleClicked(0))
            app.library_wnd.try_set_component_as_project(app, id);
        else
            app.component_ed.request_to_open(id);
    }

    show_component_popup_menu(app, fs, id, c);
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
}

void library_window::show_notsaved_content(
  const file_access&        fs,
  const component_access&   ids,
  const bitflags<file_type> flags) noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (flags[file_type::component]) {
        for (const auto id : ids) {
            const auto& compo        = ids.components[id];
            const auto& file         = ids.component_file_paths[id];
            const auto  is_not_saved = file.path.empty();

            if (is_not_saved) {
                component_color color = ids.component_colors[id];
                const auto selected   = app.component_ed.is_component_open(id);

                ImGui::PushID(reinterpret_cast<const void*>(&compo));
                if (ImGui::ColorEdit4("Color selection",
                                      color.data(),
                                      ImGuiColorEditFlags_NoInputs |
                                        ImGuiColorEditFlags_NoLabel)) {
                    app.add_gui_task([&app, id, color]() noexcept {
                        app.mod.ids.write([&](auto& ids) noexcept {
                            if (ids.exists(id)) {
                                ids.component_colors[id] = color;
                            }
                        });
                    });
                }

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

                show_component_popup_menu(app, fs, id, compo);
            }
        }
    }

    if (flags[file_type::project]) {
        for (const auto& pj : app.pjs) {
            const auto pj_id = app.pjs.get_id(pj);
            const auto have_file =
              app.mod.files.read([&](const auto& fs, const auto /*vers*/) {
                  return fs.file_paths.try_to_get(pj.pj.file) != nullptr;
              });

            if (not have_file) {
                ImGui::PushID(std::addressof(pj));
                if (ImGui::Selectable(pj.pj.name.c_str(), true)) {
                    ImGui::SetWindowFocus(pj.title.c_str());
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Close project")) {
                        app.close_project_window(pj_id);
                    }

                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
    }
}

void library_window::show_dirpath_content(
  const file_access&        fs,
  const component_access&   ids,
  const dir_path&           dir,
  const bitflags<file_type> flags) noexcept
{
    if (dir.status == dir_path::state::error) {
        ImGui::TextFormatDisabled("{} (error)", dir.path.sv());
        return;
    }

    if (ImGui::TreeNodeEx(&dir,
                          ImGuiTreeNodeFlags_DefaultOpen,
                          "%.*s",
                          dir.path.ssize(),
                          dir.path.data())) {
        const auto dir_id = fs.dir_paths.get_id(dir);
        if (flags[file_type::component]) {
            for (const auto id : ids.ids) {
                const auto& file = ids.component_file_paths[id];

                if (file.parent == dir_id) {
                    show_file_component(fs, ids, id);
                }
            }
        }

        for (const auto file_id : dir.children) {
            const auto* file = fs.file_paths.try_to_get(file_id);
            if (file == nullptr)
                return;

            ImGui::PushID(file);

            switch (file->type) {
            case file_path::file_type::data_file:
                break;

            case file_path::file_type::dot_file:
                break;

            case file_path::file_type::component_file:
                break;

            case file_path::file_type::txt_file:
                break;

            case file_path::file_type::undefined_file:
                break;

            case file_path::file_type::project_file:
                if (flags[file_type::project])
                    show_file_project(fs, file_id);
                break;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

void library_window::show_repertories_content(
  const file_access&        fs,
  const component_access&   ids,
  const bitflags<file_type> flags) noexcept
{
    for (const auto id : fs.recorded_paths) {
        small_string<31>        s;
        const small_string<31>* select;

        const auto* reg_dir = fs.registred_paths.try_to_get(id);
        if (not reg_dir or reg_dir->status == registred_path::state::error)
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
            for (const auto dir_id : reg_dir->children) {
                auto* dir = fs.dir_paths.try_to_get(dir_id);
                if (dir and dir->status != dir_path::state::error)
                    show_dirpath_content(fs, ids, *dir, flags);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void library_window::try_set_component_as_project(
  application&       app,
  const component_id compo_id) noexcept
{
    const auto pj_id = app.alloc_project_window();
    if (app.pjs.try_to_get(pj_id)) {
        app.add_gui_task([&app, compo_id, pj_id]() noexcept {
            auto& pj = app.pjs.get(pj_id);

            if (not pj.pj.set(app.mod, compo_id)) {
                app.jn.push(log_level::error, [](auto& title, auto& msg) {
                    title = "Project failure",
                    msg   = "Fail to import component as project";
                });
            }
            pj.disable_access = false;
        });
    }
}

static auto is_component_used_in_components(const component_access& ids,
                                            const component_id      id) noexcept
  -> bool
{
    for (const auto c_id : ids) {
        const auto& c = ids.components[c_id];

        switch (c.type) {
        case component_type::generic:
            if (const auto* g =
                  ids.generic_components.try_to_get(c.id.generic_id)) {
                if (std::ranges::any_of(
                      g->children, [id](const auto& ch) noexcept -> bool {
                          return ch.type == child_type::component and
                                 ch.id.compo_id == id;
                      }))
                    return true;
            }
            break;

        case component_type::grid:
            if (const auto* g = ids.grid_components.try_to_get(c.id.grid_id)) {
                if (std::ranges::any_of(
                      g->children(),
                      [id](const auto c) noexcept -> bool { return c == id; }))
                    return true;
            }
            break;

        case component_type::graph:
            if (const auto* g =
                  ids.graph_components.try_to_get(c.id.graph_id)) {
                for (const auto i : g->g.nodes) {
                    if (g->g.node_components[i] == id)
                        return true;
                }
            }
            break;

        case component_type::simulation:
        case component_type::hsm:
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

auto library_window::is_component_deletable(const application&      app,
                                            const component_access& ids,
                                            const component_id      id) noexcept
  -> is_component_deletable_t
{
    return is_component_used_in_components(ids, id)
             ? is_component_deletable_t::used_by_component
           : is_component_used_in_projects(app, id, stack)
             ? is_component_deletable_t::used_by_project
             : is_component_deletable_t::deletable;
}

auto library_window::is_component_deletable(const application& app,
                                            const component_id id) noexcept
  -> is_component_deletable_t
{
    return app.mod.ids.read(
      [&](const auto& ids, auto) noexcept -> is_component_deletable_t {
          return is_component_deletable(app, ids, id);
      });
}

void library_window::show_menu() noexcept
{
    auto& app = container_of(this, &application::library_wnd);

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

            if (ImGui::MenuItem("simulation component"))
                app.component_ed.add_simulation_component_data();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
            auto show_component = flags[file_type::component];
            if (ImGui::MenuItem("Display components", nullptr, &show_component))
                flags.set(file_type::component, show_component);
            auto show_project = flags[file_type::project];
            if (ImGui::MenuItem("Display projects", nullptr, &show_project))
                flags.set(file_type::project, show_project);
            auto show_txt = flags[file_type::txt];
            if (ImGui::MenuItem("Display text files", nullptr, &show_txt))
                flags.set(file_type::txt, show_txt);
            auto show_data = flags[file_type::data];
            if (ImGui::MenuItem("Display data files", nullptr, &show_data))
                flags.set(file_type::data, show_data);
            auto show_dot = flags[file_type::dot];
            if (ImGui::MenuItem("Display dot files", nullptr, &show_dot))
                flags.set(file_type::dot, show_dot);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Examples")) {
            for (auto i = 0; i < internal_component_count; ++i) {
                if (ImGui::MenuItem(internal_component_names[i])) {
                    app.add_gui_task([&]() noexcept {
                        app.mod.ids.write([&](auto& ids) noexcept {
                            const auto compo_id = ids.alloc_generic_component();

                            if (is_undefined(compo_id)) {
                                app.jn.push(
                                  log_level::error, [](auto& t, auto& m) {
                                      t = "Library: copy in generic "
                                          "component";
                                      m = "Can not allocate a new component";
                                  });
                                return;
                            }

                            const auto ret = ids.copy(
                              enum_cast<internal_component>(i), compo_id);

                            if (ret.has_value())
                                app.jn.push(log_level::error,
                                            [](auto& t, auto& m) {
                                                t = "Library: copy in "
                                                    "generic component";
                                                m = "TODO";
                                            });
                        });
                    });

                    app.add_gui_task(
                      [&app]() noexcept { app.component_sel.update(); });
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void library_window::show_file_treeview(
  const file_access&        fs,
  const component_access&   ids,
  const bitflags<file_type> flags) noexcept
{
    if (not ImGui::BeginChild("##library", ImGui::GetContentRegionAvail())) {
        ImGui::EndChild();
    } else {
        show_repertories_content(fs, ids, flags);

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& app = container_of(this, &application::library_wnd);
            app.mod.ids.read([&](const auto& ids, auto) noexcept {
                show_notsaved_content(fs, ids, flags);
                ImGui::TreePop();
            });
        }

        ImGui::EndChild();
    }
}

void library_window::show() noexcept
{
    if (!ImGui::Begin(
          library_window::name, &is_open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    show_menu();

    auto& app = container_of(this, &application::library_wnd);

    app.mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept {
        app.mod.ids.read([&](const auto& ids, auto) noexcept {
            if (ImGui::BeginTabBar("Library")) {
                if (ImGui::BeginTabItem("Components")) {
                    show_file_treeview(
                      fs, ids, bitflags<file_type>(file_type::component));
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Projects")) {
                    show_file_treeview(
                      fs, ids, bitflags<file_type>(file_type::project));
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Files")) {
                    show_file_treeview(fs, ids, flags);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        });
    });

    ImGui::End();
}

} // namespace irt
