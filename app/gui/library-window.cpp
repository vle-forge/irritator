// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

static void remove_component_and_file_task(application&       app,
                                           const file_path_id id) noexcept
{
    app.add_gui_task([&app, id]() noexcept {
        app.mod.files.write([&](auto& fs) noexcept {
            if (const auto* f = fs.file_paths.try_to_get(id)) {
                const auto compo_id = f->component;

                if (app.mod.components.exists(compo_id)) {
                    const auto name =
                      app.mod.components.get<component>(compo_id).name.sv();

                    app.jn.push(log_level::notice,
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
                      log_level::notice, [&](auto& title, auto& msg) noexcept {
                          title = "Remove component file";
                          format(msg, "File `{}' removed", f->path.sv());
                      });
                }

                fs.remove_file(id);
                app.mod.components.free(compo_id);
            }
        });
    });

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

static void remove_component_task(application&       app,
                                  const component_id compo_id) noexcept
{
    app.add_gui_task([&app, compo_id]() noexcept {
        app.jn.push(log_level::notice,
                    [&](auto& title, auto& /*msg*/) noexcept {
                        title = "Remove component";
                    });

        if (auto* c = app.mod.components.try_to_get<component>(compo_id))
            app.mod.free(*c);
    });

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

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

static void show_component_popup_menu(application&                 app,
                                      const modeling::file_access& fs,
                                      const component&             sel) noexcept
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
            if (app.mod.components.can_alloc(1)) {
                auto new_c_id = app.mod.components.alloc_id();
                app.mod.components.get<component_color>(
                  new_c_id) = { 1.f, 1.f, 1.f, 1.f };
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

        if (const auto* file = fs.file_paths.try_to_get(sel.file); file) {
            if (ImGui::MenuItem("Delete component and file")) {
                const auto id = app.mod.components.get_id(sel);
                if (can_delete_component(app, id)) {
                    app.component_ed.close(app.mod.components.get_id(sel));
                    remove_component_and_file_task(app, sel.file);
                }
            }
        } else {
            if (ImGui::MenuItem("Delete component")) {
                const auto id = app.mod.components.get_id(sel);
                if (can_delete_component(app, id)) {
                    const auto compo_id = app.mod.components.get_id(sel);
                    app.component_ed.close(compo_id);
                    remove_component_task(app, compo_id);
                }
            }
        }

        ImGui::EndPopup();
    }
}

void library_window::show_file_project(const modeling::file_access& fs,
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

void library_window::show_file_component(const modeling::file_access& fs,
                                         const file_path&             file,
                                         const component& c) noexcept
{
    auto&      app      = container_of(this, &application::library_wnd);
    const auto id       = app.mod.components.get_id(c);
    const bool selected = app.component_ed.is_component_open(id);
    const auto state    = c.state;

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

    ImGui::SameLine();
    const auto name    = format_n<63>("{} ({})", c.name.sv(), file.path.sv());
    const auto disable = state == component_status::unreadable;
    const auto flags   = disable ? ImGuiSelectableFlags_Disabled
                                 : ImGuiSelectableFlags_AllowDoubleClick;

    if (ImGui::Selectable(name.c_str(), selected, flags)) {
        if (ImGui::IsMouseDoubleClicked(0))
            app.library_wnd.try_set_component_as_project(app, id);
        else
            app.component_ed.request_to_open(id);
    }

    show_component_popup_menu(app, fs, c);
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
  const modeling::file_access& fs,
  const bitflags<file_type>    flags) noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (flags[file_type::component]) {
        auto& compos = app.mod.components.get<component>();
        auto& colors = app.mod.components.get<component_color>();

        for (const auto id : app.mod.components) {
            auto& compo = compos[id];

            const auto is_not_saved =
              app.mod.files.read([&](const auto& fs, const auto /*vers*/) {
                  return fs.file_paths.try_to_get(compo.file) == nullptr;
              });

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

                show_component_popup_menu(app, fs, compo);
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
  const modeling::file_access& fs,
  const dir_path&              dir,
  const bitflags<file_type>    flags) noexcept
{
    auto& app = container_of(this, &application::library_wnd);

    if (dir.status == dir_path::state::error) {
        ImGui::TextFormatDisabled("{} (error)", dir.path.sv());
        return;
    }

    if (ImGui::TreeNodeEx(&dir,
                          ImGuiTreeNodeFlags_DefaultOpen,
                          "%.*s",
                          dir.path.ssize(),
                          dir.path.data())) {
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
                if (flags[file_type::component]) {
                    if (auto* compo = app.mod.components.try_to_get<component>(
                          file->component))
                        show_file_component(fs, *file, *compo);
                }
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
  const modeling::file_access& fs,
  const bitflags<file_type>    flags) noexcept
{
    for (const auto id : fs.component_repertories) {
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
                    show_dirpath_content(fs, *dir, flags);
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
            if (auto* c = app.mod.components.try_to_get<component>(compo_id)) {
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
                    if (app.mod.components.can_alloc(1) and
                        app.mod.generic_components.can_alloc(1)) {
                        auto&      compo = app.mod.alloc_generic_component();
                        const auto generic_id = compo.id.generic_id;
                        auto& g = app.mod.generic_components.get(generic_id);

                        compo.name  = internal_component_names[i];
                        compo.state = component_status::modified;

                        if (auto ret = app.mod.copy(
                              enum_cast<internal_component>(i), compo, g);
                            !ret) {
                            app.jn.push(log_level::error, [](auto& t, auto& m) {
                                t = "Library: copy in "
                                    "generic component";
                                m = "TODO";
                            });
                        }

                        app.add_gui_task(
                          [&app]() noexcept { app.component_sel.update(); });
                    } else {
                        app.jn.push(log_level::error, [](auto& t, auto& m) {
                            t = "Library: copy in generic "
                                "component";
                            m = "Can not allocate a new component";
                        });
                    }
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void library_window::show_file_treeview(
  const modeling::file_access& fs,
  const bitflags<file_type>    flags) noexcept
{
    if (not ImGui::BeginChild("##library", ImGui::GetContentRegionAvail())) {
        ImGui::EndChild();
    } else {
        show_repertories_content(fs, flags);

        if (ImGui::TreeNodeEx("Not saved", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_notsaved_content(fs, flags);
            ImGui::TreePop();
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
        if (ImGui::BeginTabBar("Library")) {
            if (ImGui::BeginTabItem("Components")) {
                show_file_treeview(fs,
                                   bitflags<file_type>(file_type::component));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Projects")) {
                show_file_treeview(fs, bitflags<file_type>(file_type::project));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Files")) {
                show_file_treeview(fs, flags);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    });

    ImGui::End();
}

} // namespace irt
