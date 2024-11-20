// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"
#include "irritator/archiver.hpp"
#include "irritator/modeling.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <irritator/file.hpp>
#include <mutex>

namespace irt {

application::application() noexcept
  : task_mgr{}
  , config(get_config_home(true))
{
    settings_wnd.apply_style(undefined<gui_theme_id>());

    log_w(*this, log_level::info, "Irritator start\n");

    log_w(*this,
          log_level::info,
          "Starting with {} main threads and {} generic workers\n",
          task_mgr.main_workers.ssize(),
          task_mgr.temp_workers.ssize());

    task_mgr.start();

    log_w(*this, log_level::info, "Initialization successfull");
}

application::~application() noexcept
{
    log_w(*this, log_level::info, "Task manager shutdown\n");
    task_mgr.finalize();
    log_w(*this, log_level::info, "Application shutdown\n");
}

std::optional<project_id> application::alloc_project_window() noexcept
{
    if (not pjs.can_alloc(1))
        pjs.grow();

    if (not pjs.can_alloc(1)) {
        notifications.try_insert(
          log_level::error, [&](auto& title, auto& msg) noexcept {
              title = "Fail to allocate another project";
              format(msg,
                     "There is {} projects opened. Close one before.",
                     pjs.size());
          });

        return std::nullopt;
    }

    name_str temp;
    format(temp, "project {}", pjs.next_key());

    auto& pj = pjs.alloc(temp.sv());

    if (not pj.pj.init(modeling_initializer{})) {
        notifications.try_insert(
          log_level::error, [&](auto& title, auto& msg) noexcept {
              title = "Fail to initialize project";
              format(msg,
                     "There is {} projects opened. Close one before.",
                     pjs.size());
          });
    }

    return pjs.get_id(pj);
}

bool application::init() noexcept
{
    if (!mod.init(mod_init)) {
        log_w(*this,
              log_level::error,
              "Fail to initialize modeling components: {}\n");
        return false;
    }

    pjs.reserve(8);

    if (mod.registred_paths.size() == 0) {
        attempt_all(
          [&]() -> result<void> {
              auto path = get_system_component_dir();
              if (!path)
                  return path.error();

              auto& new_dir    = mod.registred_paths.alloc();
              auto  new_dir_id = mod.registred_paths.get_id(new_dir);
              new_dir.name     = "System directory";
              new_dir.path     = path.value().string().c_str();
              log_w(*this,
                    log_level::info,
                    "Add system directory: {}\n",
                    new_dir.path.c_str());

              mod.component_repertories.emplace_back(new_dir_id);
              return success();
          },
          [&](fs_error /*code*/) -> void {
              log_w(*this,
                    log_level::error,
                    "Fail to use the system directory path");
          },
          [&]() -> void {
              log_w(*this,
                    log_level::error,
                    "Fail to use the system directory path");
          });

        attempt_all(
          [&]() -> result<void> {
              auto path = get_system_prefix_component_dir();
              if (!path)
                  return path.error();

              auto& new_dir    = mod.registred_paths.alloc();
              auto  new_dir_id = mod.registred_paths.get_id(new_dir);
              new_dir.name     = "System directory";
              new_dir.path     = path.value().string().c_str();
              log_w(*this,
                    log_level::info,
                    "Add system directory: {}\n",
                    new_dir.path.c_str());

              mod.component_repertories.emplace_back(new_dir_id);
              return success();
          },
          [&](fs_error /*code*/) -> void {
              log_w(*this,
                    log_level::error,
                    "Fail to use the system directory path");
          },
          [&]() -> void {
              log_w(*this,
                    log_level::error,
                    "Fail to use the system directory path");
          });

        attempt_all(
          [&]() -> result<void> {
              auto path = get_default_user_component_dir();
              if (!path)
                  return path.error();

              auto& new_dir    = mod.registred_paths.alloc();
              auto  new_dir_id = mod.registred_paths.get_id(new_dir);
              new_dir.name     = "User directory";
              new_dir.path     = path.value().string().c_str();
              log_w(*this,
                    log_level::info,
                    "Add user directory: {}\n",
                    new_dir.path.c_str());

              mod.component_repertories.emplace_back(new_dir_id);
              return success();
          },
          [&](fs_error /*code*/) -> void {
              log_w(
                *this, log_level::error, "Fail to use the user directory path");
          },
          [&]() -> void {
              log_w(
                *this, log_level::error, "Fail to use the user directory path");
          });
    }

    if (auto ret = save_settings(); !ret) {
        log_w(*this, log_level::error, "Fail to save settings files.\n");
    }

    if (auto ret = mod.fill_internal_components(); !ret) {
        log_w(*this,
              log_level::error,
              "Fail to fill internal component list: {}\n");
    }

    graphs.reserve(32);
    grids.reserve(32);
    generics.reserve(32);
    hsms.reserve(32);

    if (auto ret = mod.fill_components(); !ret)
        log_w(*this, log_level::error, "Fail to read all components\n");

    // Update the component selector in task.
    add_gui_task([&]() noexcept { this->component_sel.update(); });

    // @TODO at beggining, open a default generic component ?
    // auto id = component_ed.add_generic_component();
    // component_ed.open_as_main(id);

    return true;
}

template<typename DataArray>
static void cleanup_sim_or_gui_task(DataArray& d_array) noexcept
{
    typename DataArray::value_type* task   = nullptr;
    typename DataArray::value_type* to_del = nullptr;

    while (d_array.next(task)) {
        if (to_del) {
            d_array.free(*to_del);
            to_del = nullptr;
        }

        if (task->state == task_status::finished) {
            to_del = task;
        }
    }

    if (to_del)
        d_array.free(*to_del);
}

static void application_show_menu(application& app) noexcept
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("New project"))
                app.menu_new_project_file = true;

            if (ImGui::MenuItem("Open project"))
                app.menu_load_project_file = true;

            if (ImGui::MenuItem("Save project"))
                app.menu_save_project_file = true;

            if (ImGui::MenuItem("Save project as..."))
                app.menu_save_as_project_file = true;

            if (ImGui::MenuItem("Quit"))
                app.menu_quit = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show modeling editor", nullptr, &app.component_ed.is_open);
            ImGui::MenuItem(
              "Show output editor", nullptr, &app.output_ed.is_open);
            ImGui::MenuItem("Show data editor", nullptr, &app.data_ed.is_open);

            ImGui::MenuItem(
              "Show component hierarchy", nullptr, &app.library_wnd.is_open);

            ImGui::MenuItem(
              "Show memory usage", nullptr, &app.memory_wnd.is_open);
            ImGui::MenuItem("Show task usage", nullptr, &app.task_wnd.is_open);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("ImGui demo window", nullptr, &app.show_imgui_demo);
            ImGui::MenuItem(
              "ImPlot demo window", nullptr, &app.show_implot_demo);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &app.settings_wnd.is_open);

            if (ImGui::MenuItem("Load settings")) {
                attempt_all(
                  [&app]() noexcept -> status {
                      irt_check(app.load_settings());
                      return success();
                  },

                  [&app](const json_archiver::error_code s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Fail to load settings";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&app]() noexcept -> void {
                      auto& n   = app.notifications.alloc();
                      n.title   = "Fail to load settings";
                      n.message = "Unknown error";
                      app.notifications.enable(n);
                  });
            }

            if (ImGui::MenuItem("Save settings")) {
                attempt_all(
                  [&app]() noexcept -> status {
                      irt_check(app.save_settings());
                      return success();
                  },

                  [&app](const json_archiver::error_code s) noexcept -> void {
                      auto& n = app.notifications.alloc();
                      n.title = "Fail to save settings";
                      format(n.message, "Error: {}", ordinal(s));
                      app.notifications.enable(n);
                  },

                  [&app]() noexcept -> void {
                      auto& n   = app.notifications.alloc();
                      n.title   = "Fail to load settings";
                      n.message = "Unknown error";
                      app.notifications.enable(n);
                  });
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

static void application_manage_menu_action(application& app) noexcept
{
    if (app.menu_new_project_file) {
        app.alloc_project_window();
        app.menu_new_project_file = false;
        return;
    }

    if (app.menu_load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_load_file(title, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.mod.registred_paths.alloc();
                    auto  id   = app.mod.registred_paths.get_id(path);

                    if (auto opt = app.alloc_project_window(); opt.has_value())
                        app.start_load_project(id, *opt);
                }
            }

            app.f_dialog.clear();
            app.menu_load_project_file = false;
        }
    }

    if (app.menu_save_project_file) {
        const bool have_file = !app.project_file.empty();

        if (have_file) {
            auto  u8str = app.project_file.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (app.mod.registred_paths.can_alloc(1)) {
                auto& path = app.mod.registred_paths.alloc();
                auto  id   = app.mod.registred_paths.get_id(path);
                path.path  = str;

                debug::breakpoint();
                project_id pj_id;
                app.start_save_project(id, pj_id);
            }
        } else {
            app.menu_save_project_file    = false;
            app.menu_save_as_project_file = true;
        }
    }

    if (app.menu_save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_save_file(title, default_filename, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                app.project_file = app.f_dialog.result;
                auto  u8str      = app.project_file.u8string();
                auto* str        = reinterpret_cast<const char*>(u8str.c_str());

                if (app.mod.registred_paths.can_alloc(1)) {
                    auto& path = app.mod.registred_paths.alloc();
                    auto  id   = app.mod.registred_paths.get_id(path);
                    path.path  = str;

                    debug::breakpoint();
                    project_id pj_id;
                    app.start_save_project(id, pj_id);
                }
            }

            app.f_dialog.clear();
            app.menu_save_as_project_file = false;
        }
    }
}

static void application_show_windows(application& app) noexcept
{
    static auto                  first_time = true;
    constexpr ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    app.main_dock_id =
      ImGui::DockSpaceOverViewport(0, viewport, dockspace_flags);

    if (first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(app.main_dock_id);
        ImGui::DockBuilderAddNode(
          app.main_dock_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(app.main_dock_id, viewport->Size);

        app.right_dock_id = ImGui::DockBuilderSplitNode(
          app.main_dock_id, ImGuiDir_Right, 0.2f, nullptr, &app.main_dock_id);
        app.bottom_dock_id = ImGui::DockBuilderSplitNode(
          app.main_dock_id, ImGuiDir_Down, 0.2f, nullptr, &app.main_dock_id);

        ImGui::DockBuilderDockWindow(component_editor::name, app.main_dock_id);
        ImGui::DockBuilderDockWindow(output_editor::name, app.main_dock_id);
        ImGui::DockBuilderDockWindow(data_window::name, app.main_dock_id);

        ImGui::DockBuilderDockWindow(app.library_wnd.name, app.right_dock_id);

        ImGui::DockBuilderDockWindow(window_logger::name, app.bottom_dock_id);

        ImGui::DockBuilderFinish(app.main_dock_id);
    }

    if (app.component_ed.is_open)
        app.component_ed.display();

    for (auto& ed : app.pjs) {
        ed.start_simulation_update_state(app);
        ed.show(app);
    }

    if (app.output_ed.is_open)
        app.output_ed.show();

    if (app.log_wnd.is_open)
        app.log_wnd.show();

    if (app.library_wnd.is_open)
        app.library_wnd.show();
}

void application::show() noexcept
{
#ifdef IRRITATOR_USE_TTF
    if (ttf)
        ImGui::PushFont(ttf);
#endif

    if (mod.log_entries.have_entry()) {
        mod.log_entries.try_consume([&](auto& entries) noexcept {
            for (auto& w : entries) {
                auto& n = notifications.alloc();
                n.level = w.level;
                n.title = w.buffer.sv();
                notifications.enable(n);
            }

            entries.clear();
        });
    }

    application_show_menu(*this);
    application_manage_menu_action(*this);

    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);

    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);

    application_show_windows(*this);

    if (memory_wnd.is_open)
        memory_wnd.show();

    if (task_wnd.is_open)
        task_wnd.show();

    if (settings_wnd.is_open)
        settings_wnd.show();

    if (show_select_directory_dialog) {
        const char* title = "Select directory";
        ImGui::OpenPopup(title);
        if (f_dialog.show_select_directory(title)) {
            if (f_dialog.state == file_dialog::status::ok) {
                select_directory = f_dialog.result;
                auto* dir_path =
                  mod.registred_paths.try_to_get(select_dir_path);
                if (dir_path) {
                    auto str = select_directory.string();
                    dir_path->path.assign(str);
                }

                show_select_directory_dialog = false;
                select_dir_path              = undefined<registred_path_id>();
                select_directory.clear();
            }

            f_dialog.clear();
            show_select_directory_dialog = false;
        }
    }

    notifications.show();

#ifdef IRRITATOR_USE_TTF
    if (ttf)
        ImGui::PopFont();
#endif
}

static void show_select_model_box_recursive(application&    app,
                                            project_window& ed,
                                            tree_node&      tn,
                                            grid_observer&  access) noexcept
{
    constexpr auto flags = ImGuiTreeNodeFlags_DefaultOpen;

    if_data_exists_do(app.mod.components, tn.id, [&](auto& compo) noexcept {
        small_string<64> str;

        switch (compo.type) {
        case component_type::simple:
            format(str, "{} generic", compo.name.sv());
            break;
        case component_type::grid:
            format(str, "{} grid", compo.name.sv());
            break;
        case component_type::graph:
            format(str, "{} graph", compo.name.sv());
            break;
        default:
            format(str, "{} unknown", compo.name.sv());
            break;
        }

        ImGui::PushID(&tn);
        if (ImGui::TreeNodeEx(str.c_str(), flags)) {
            for_each_model(
              ed.pj.sim,
              tn,
              [&](const std::string_view /*unique_id*/, auto& mdl) noexcept {
                  const auto mdl_id = ed.pj.sim.models.get_id(mdl);
                  ImGui::PushID(get_index(mdl_id));

                  const auto current_tn_id = ed.pj.node(tn);
                  str = dynamics_type_names[ordinal(mdl.type)];
                  if (ImGui::Selectable(str.c_str(),
                                        access.tn_id == current_tn_id &&
                                          access.mdl_id == mdl_id,
                                        ImGuiSelectableFlags_DontClosePopups)) {
                      access.tn_id  = current_tn_id;
                      access.mdl_id = mdl_id;
                  }

                  ImGui::PopID();
              });

            if (auto* child = tn.tree.get_child(); child)
                show_select_model_box_recursive(app, ed, *child, access);

            ImGui::TreePop();
        }
        ImGui::PopID();
    });

    if (auto* sibling = tn.tree.get_sibling(); sibling)
        show_select_model_box_recursive(app, ed, *sibling, access);
}

auto build_unique_component_vector(application& app,
                                   tree_node&   tn) -> vector<component_id>
{
    vector<component_id> ret;
    vector<tree_node*>   stack;

    if (auto* child = tn.tree.get_child(); child)
        stack.emplace_back(child);

    while (!stack.empty()) {
        auto* cur = stack.back();
        stack.pop_back();

        if (auto* compo = app.mod.components.try_to_get(cur->id); compo) {
            if (auto it = std::find(ret.begin(), ret.end(), cur->id);
                it == ret.end())
                ret.emplace_back(cur->id);
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    return ret;
}

bool show_select_model_box(const char*     button_label,
                           const char*     popup_label,
                           application&    app,
                           project_window& ed,
                           tree_node&      tn,
                           grid_observer&  access) noexcept
{
    auto ret = false;

    ImGui::BeginDisabled(app.component_model_sel.update_in_progress());
    if (ImGui::Button(button_label)) {
        debug::ensure(ed.pj.tree_nodes.get_id(tn) == access.parent_id);
        app.component_model_sel.start_update(ed.pj,
                                             access.parent_id,
                                             access.compo_id,
                                             access.tn_id,
                                             access.mdl_id);

        ImGui::OpenPopup(popup_label);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(
          popup_label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        if (not app.component_model_sel.update_in_progress()) {
            app.component_model_sel.combobox("Select model to observe grid",
                                             ed.pj,
                                             access.parent_id,
                                             access.compo_id,
                                             access.tn_id,
                                             access.mdl_id);
        } else {
            ImGui::Text("Computing observable children");
        }

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            ret = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            const auto& old_access =
              app.component_model_sel.get_update_access();
            access.parent_id = old_access.parent_id;
            access.compo_id  = old_access.compo_id;
            access.tn_id     = old_access.tn_id;
            access.mdl_id    = old_access.mdl_id;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::EndDisabled();

    return ret;
}

bool show_select_model_box(const char*     button_label,
                           const char*     popup_label,
                           application&    app,
                           project_window& ed,
                           tree_node&      tn,
                           graph_observer& access) noexcept
{
    auto ret = false;

    ImGui::BeginDisabled(app.component_model_sel.update_in_progress());
    if (ImGui::Button(button_label)) {
        debug::ensure(ed.pj.tree_nodes.get_id(tn) == access.parent_id);
        app.component_model_sel.start_update(ed.pj,
                                             access.parent_id,
                                             access.compo_id,
                                             access.tn_id,
                                             access.mdl_id);

        ImGui::OpenPopup(popup_label);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(
          popup_label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        if (not app.component_model_sel.update_in_progress()) {
            app.component_model_sel.combobox("Select model to observe graph",
                                             ed.pj,
                                             access.parent_id,
                                             access.compo_id,
                                             access.tn_id,
                                             access.mdl_id);
        } else {
            ImGui::Text("Computing observable children");
        }

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            ret = true;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            const auto& old_access =
              app.component_model_sel.get_update_access();
            access.parent_id = old_access.parent_id;
            access.compo_id  = old_access.compo_id;
            access.tn_id     = old_access.tn_id;
            access.mdl_id    = old_access.mdl_id;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return ret;
}

unordered_task_list& application::get_unordered_task_list(int idx) noexcept
{
    idx = idx < 0 ? 0
          : idx >= length(task_mgr.temp_task_lists)
            ? length(task_mgr.temp_task_lists) - 1
            : idx;

    return task_mgr.temp_task_lists[idx];
}

std::optional<file> application::try_open_file(const char* filename,
                                               open_mode   mode) noexcept
{
    debug::ensure(filename);

    if (not filename)
        return std::nullopt;

    auto f = file::open(filename, mode, [&](file::error_code ec) noexcept {
        notifications.try_insert(
          log_level::error, [&](auto& title, auto& msg) noexcept {
              format(title, "Open file {}", filename);

              std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, file::memory_error>) {
                        format(msg,
                               "Memory allocation error (requires {})",
                               arg.value);
                    } else if constexpr (std::is_same_v<T, file::arg_error>) {
                        msg = "Internal error (bad argument)";
                    } else if constexpr (std::is_same_v<T, file::eof_error>) {
                        msg = "End of file reached to quickly";
                    } else if constexpr (std::is_same_v<T, file::open_error>) {
                        if (arg.value)
                            format(msg,
                                   "Filesystem error ({})",
                                   std::strerror(arg.value));
                        else
                            msg = "Filesystem error";
                    } else
                        msg = "Internal error (non-exhaustive error report";
                },
                ec);
          });
    });

    return f;
}

void application::start_load_project(const registred_path_id id,
                                     const project_id        pj_id) noexcept
{
    add_gui_task([&, id]() noexcept {
        std::scoped_lock _(mod.reg_paths_mutex);

        auto* file = mod.registred_paths.try_to_get(id);
        if (not file)
            return;

        std::scoped_lock lock(file->mutex);

        auto f_opt = try_open_file(file->path.c_str(), open_mode::read);
        if (not f_opt)
            return;

        auto* sim_ed = pjs.try_to_get(pj_id);
        if (not sim_ed)
            return;

        json_dearchiver dearc;
        dearc(
          sim_ed->pj,
          mod,
          sim_ed->pj.sim,
          *f_opt,
          [&](
            json_dearchiver::error_code ec,
            std::
              variant<std::monostate, sz, int, std::pair<sz, std::string_view>>
                v) noexcept {
              notifications.try_insert(
                log_level::error, [&](auto& title, auto& msg) noexcept {
                    format(title,
                           "Loading project file {} error",
                           file->path.c_str());
                    switch (ec) {
                    case json_dearchiver::error_code::memory_error:
                        if (auto* ptr = std::get_if<sz>(&v); ptr) {
                            format(msg,
                                   "json de-archiving memory error: not "
                                   "enough memory "
                                   "(requested: {})\n",
                                   *ptr);
                        } else {
                            format(msg,
                                   "json de-archiving memory error: not "
                                   "enough memory\n");
                        }
                        break;

                    case json_dearchiver::error_code::arg_error:
                        format(msg, "json de-archiving internal error\n");
                        break;

                    case json_dearchiver::error_code::file_error:
                        if (auto* ptr = std::get_if<sz>(&v); ptr) {
                            format(msg,
                                   "json de-archiving file error: file too "
                                   "small {}\n",
                                   *ptr);
                        } else {
                            format(msg,
                                   "json de-archiving memory error: not "
                                   "enough memory\n");
                        }
                        break;

                    case json_dearchiver::error_code::read_error:
                        if (auto* ptr = std::get_if<int>(&v);
                            ptr and *ptr != 0) {
                            format(msg,
                                   "json de-archiving read error: internal "
                                   "system {} ({})\n",
                                   *ptr,
                                   std::strerror(*ptr));
                        } else {
                            format(msg, "json de-archiving read error\n");
                        }
                        break;

                    case json_dearchiver::error_code::format_error:
                        if (auto* ptr =
                              std::get_if<std::pair<sz, std::string_view>>(&v);
                            ptr) {
                            if (ptr->second.empty()) {
                                format(msg,
                                       "json de-archiving json format error at "
                                       "offset "
                                       "{}\n",
                                       ptr->first);

                            } else {
                                format(msg,
                                       "json de-archiving json format error "
                                       "`{}' at offset "
                                       "{}\n",
                                       ptr->second,
                                       ptr->first);
                            }
                        } else {
                            format(msg,
                                   "json de-archiving json format error\n");
                        }
                        break;

                    default:
                        format(msg, "json de-archiving unknown error\n");
                    }
                });
          });

        notifications.try_insert(
          log_level::info, [&](auto& title, auto& /*msg*/) noexcept {
              format(
                title, "Loading project file {} success", file->path.c_str());
          });
    });
}

void application::start_save_project(const registred_path_id id,
                                     const project_id        pj_id) noexcept
{
    add_gui_task([&, id]() noexcept {
        std::scoped_lock _(mod.reg_paths_mutex);

        auto* file = mod.registred_paths.try_to_get(id);
        if (not file)
            return;

        std::scoped_lock lock(file->mutex);

        auto f_opt = try_open_file(file->path.c_str(), open_mode::write);
        if (not f_opt)
            return;

        auto* sim_ed = pjs.try_to_get(pj_id);
        if (not sim_ed)
            return;

        json_archiver arc;
        arc(sim_ed->pj,
            mod,
            sim_ed->pj.sim,
            *f_opt,
            json_archiver::print_option::indent_2_one_line_array,
            [&](json_archiver::error_code             ec,
                std::variant<std::monostate, sz, int> v) noexcept {
                notifications.try_insert(
                  log_level::error, [&](auto& title, auto& msg) noexcept {
                      format(title,
                             "Saving project file {} error",
                             file->path.c_str());

                      switch (ec) {
                      case json_archiver::error_code::memory_error:
                          if (auto* ptr = std::get_if<sz>(&v); ptr) {
                              format(msg,
                                     "json archiving memory error: not "
                                     "enough memory "
                                     "(requested: {})\n",
                                     *ptr);
                          } else {
                              format(msg,
                                     "json archiving memory error: not "
                                     "enough memory\n");
                          }
                          break;

                      case json_archiver::error_code::arg_error:
                          format(msg, "json archiving internal error\n");
                          break;

                      case json_archiver::error_code::empty_project:
                          format(msg, "json archiving empty project error\n");
                          break;

                      case json_archiver::error_code::file_error:
                          format(msg, "json archiving file access error\n");
                          break;

                      case json_archiver::error_code::format_error:
                          format(msg, "json archiving format error\n");
                          break;

                      case json_archiver::error_code::dependency_error:
                          format(msg, "json archiving format error\n");
                          break;

                      default:
                          format(msg, "json de-archiving unknown error\n");
                      }
                  });
            });

        notifications.try_insert(
          log_level::info, [&](auto& title, auto& /*msg*/) noexcept {
              format(
                title, "Saving project file {} success", file->path.c_str());
          });
    });
}

void application::start_save_component(const component_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        if (auto* c = mod.components.try_to_get(id); c) {
            auto& n = notifications.alloc();
            n.title = "Component save";

            attempt_all(
              [&]() noexcept -> status {
                  irt_check(mod.save(*c));
                  format(n.message, "{} saved", c->name.sv());

                  return success();
              },

              [&](const irt::modeling::part s) noexcept -> void {
                  format(n.message,
                         "Fail to save {} (part: {})",
                         c->name.sv(),
                         ordinal(s));
              },

              [&]() noexcept -> void {
                  auto& n   = notifications.alloc();
                  n.message = "Unknown error";
              });

            notifications.enable(n);
        }
    });
}

void application::start_init_source(const project_id          pj_id,
                                    const u64                 id,
                                    const source::source_type type) noexcept
{
    add_gui_task([&, id, type]() noexcept {
        auto* sim_ed = pjs.try_to_get(pj_id);
        if (not sim_ed)
            return;

        source src;
        src.id   = id;
        src.type = type;

        attempt_all(
          [&]() noexcept -> status {
              if (sim_ed->pj.sim.srcs.dispatch(
                    src, source::operation_type::initialize)) {
                  data_ed.plot.clear();
                  for (sz i = 0, e = src.buffer.size(); i != e; ++i)
                      data_ed.plot.push_back(
                        ImVec2{ static_cast<float>(i),
                                static_cast<float>(src.buffer[i]) });
                  data_ed.plot_available = true;

                  if (!sim_ed->pj.sim.srcs.prepare())
                      notifications.try_insert(
                        log_level::error, [](auto& title, auto& msg) noexcept {
                            title = "Data error";
                            msg   = "Fail to prepare data from source.";
                        });
              }
              return success();
          },
          [&]() {
              notifications.try_insert(
                log_level::error, [](auto& title, auto& msg) noexcept {
                    title = "Data error";
                    msg   = "Fail to prepare data from source.";
                });
          });
    });
}

void application::start_dir_path_refresh(const dir_path_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        auto* dir = mod.dir_paths.try_to_get(id);

        if (dir and dir->status != dir_path::state::lock) {
            std::scoped_lock lock(dir->mutex);
            dir->status   = dir_path::state::lock;
            auto children = dir->refresh(mod);
            dir->children = std::move(children);
            dir->status   = dir_path::state::read;
        }
    });
}

void application::start_dir_path_free(const dir_path_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        auto* dir = mod.dir_paths.try_to_get(id);

        if (dir and dir->status != dir_path::state::lock) {
            std::scoped_lock lock(dir->mutex);
            dir->status = dir_path::state::lock;
            mod.dir_paths.free(*dir);
        }
    });
}

void application::start_reg_path_free(const registred_path_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        auto* reg = mod.registred_paths.try_to_get(id);

        if (reg and reg->status != registred_path::state::lock) {
            std::scoped_lock lock(reg->mutex);
            reg->status = registred_path::state::lock;
            mod.registred_paths.free(*reg);
        }
    });
};

void application::start_file_remove(const registred_path_id r,
                                    const dir_path_id       d,
                                    const file_path_id      f) noexcept
{
    add_gui_task([&, r, d, f]() noexcept {
        auto* reg  = mod.registred_paths.try_to_get(r);
        auto* dir  = mod.dir_paths.try_to_get(d);
        auto* file = mod.file_paths.try_to_get(f);

        if (reg and dir and file) {
            std::scoped_lock lock(dir->mutex);

            auto& n = notifications.alloc();
            n.title = "Start remove file";
            format(n.message, "File `{}' removed", file->path.sv());
            mod.remove_file(*reg, *dir, *file);
            notifications.enable(n);
        }
    });
}

} // namespace irt
