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

static const std::string_view extensions[] = { ".dot", ".irt" };

static constexpr const char* empty = "-";

static auto get_reg_preview(const application&      app,
                            const registred_path_id id) noexcept -> const char*
{
    if (auto* reg = app.mod.registred_paths.try_to_get(id)) {
        return reg->name.c_str();
    } else {
        return empty;
    }
}

static auto get_dir_preview(const application& app,
                            const dir_path_id  id) noexcept -> const char*
{
    if (auto* dir = app.mod.dir_paths.try_to_get(id)) {
        return dir->path.c_str();
    } else {
        return empty;
    }
}

static auto combobox_reg(application& app, registred_path_id& in_out) noexcept
  -> bool
{
    const char* reg_preview = get_reg_preview(app, in_out);
    auto        ret         = false;

    if (ImGui::BeginCombo("Path", reg_preview)) {
        int i = 0;
        for (auto& reg : app.mod.registred_paths) {
            if (reg.status == registred_path::state::error)
                continue;

            ImGui::PushID(i++);
            const auto id = app.mod.registred_paths.get_id(reg);
            if (ImGui::Selectable(
                  reg.path.c_str(), in_out == id, ImGuiSelectableFlags_None)) {
                in_out = id;
                ret    = true;
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return ret;
}

static auto combobox_dir(application&          app,
                         const registred_path& reg,
                         dir_path_id&          in_out) noexcept -> bool
{

    auto  ret         = false;
    auto* dir_preview = get_dir_preview(app, in_out);

    if (ImGui::BeginCombo("Dir", dir_preview)) {
        int i = 0;
        ImGui::PushID(i++);
        if (ImGui::Selectable("##empty-dir", is_undefined(in_out))) {
            in_out = undefined<dir_path_id>();
        }
        ImGui::PopID();

        for (auto id : reg.children) {
            if (auto* dir = app.mod.dir_paths.try_to_get(id)) {
                ImGui::PushID(i++);
                if (ImGui::Selectable(dir->path.c_str(), in_out == id)) {
                    in_out = id;
                    ret    = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndCombo();
    }

    return ret;
}

static auto combobox_newdir(application&    app,
                            registred_path& reg,
                            dir_path_id&    in_out) noexcept -> bool
{
    auto ret = false;

    if (auto* dir = app.mod.dir_paths.try_to_get(in_out); not dir) {
        directory_path_str dir_name;

        if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
            if (reg.exists(app.mod.dir_paths, dir_name.sv())) {
                auto& new_dir  = app.mod.dir_paths.alloc();
                auto  dir_id   = app.mod.dir_paths.get_id(new_dir);
                auto  reg_id   = app.mod.registred_paths.get_id(reg);
                new_dir.parent = reg_id;
                new_dir.path   = dir_name;
                new_dir.status = dir_path::state::unread;
                reg.children.emplace_back(dir_id);
                ret = true;

                if (not app.mod.create_directories(new_dir)) {
                    app.notifications.try_insert(
                      log_level::error, [&](auto& title, auto& msg) noexcept {
                          format(
                            title, "Fail to create directory ", dir_name.sv());
                          format(msg,
                                 "Error in recorded name `{}' path`{}'",
                                 reg.name.sv(),
                                 reg.path.sv());
                      });
                }
            }
        }
    }

    return ret;
}

static bool end_with(std::string_view v, file_path_selector_option opt) noexcept
{
    switch (opt) {
    case file_path_selector_option::none:
        return true;
    case file_path_selector_option::force_irt_extension:
        return v.ends_with(extensions[ordinal(opt)]);
    case file_path_selector_option::force_dot_extension:
        return v.ends_with(extensions[ordinal(opt)]);
    }

    irt::unreachable();
}

static void add_extension(file_path_str&   file,
                          std::string_view extension) noexcept
{
    const std::decay_t<decltype(file)> tmp(file);

    if (auto dot = tmp.sv().find_last_of('.'); dot != std::string_view::npos) {
        format(file, "{}{}", tmp.sv().substr(0, dot), extension);
    } else {
        format(file, "{}{}", tmp.sv(), extension);
    }
}

static auto combobox_file(application&              app,
                          file_path_selector_option opt,
                          dir_path&                 dir,
                          file_path_id&             in_out) noexcept -> bool
{
    auto  ret  = false;
    auto* file = app.mod.file_paths.try_to_get(in_out);

    if (not file) {
        auto& f     = app.mod.file_paths.alloc();
        auto  id    = app.mod.file_paths.get_id(f);
        f.component = undefined<component_id>();
        f.parent    = app.mod.dir_paths.get_id(dir);
        dir.children.emplace_back(id);
        file = &f;
    }

    if (ImGui::InputFilteredString("File##text", file->path)) {
        if (not end_with(file->path.sv(), opt)) {
            add_extension(file->path, extensions[ordinal(opt)]);
        }
        ret = true;
    }

    return ret;
}

auto file_path_selector(application&              app,
                        file_path_selector_option opt,
                        registred_path_id&        reg_id,
                        dir_path_id&              dir_id,
                        file_path_id&             file_id) noexcept -> bool
{
    auto ret = false;
    if (combobox_reg(app, reg_id))
        ret = true;

    if (auto* reg = app.mod.registred_paths.try_to_get(reg_id)) {
        if (combobox_dir(app, *reg, dir_id))
            ret = true;

        if (auto* dir = app.mod.dir_paths.try_to_get(dir_id); not dir) {
            if (combobox_newdir(app, *reg, dir_id))
                ret = true;
        }

        if (auto* dir = app.mod.dir_paths.try_to_get(dir_id)) {
            if (combobox_file(app, opt, *dir, file_id))
                ret = true;
        }
    }

    return ret;
}

application::application() noexcept
  : task_mgr{}
  , pjs(16)
  , config(get_config_home(true))
{
    settings_wnd.apply_style(undefined<gui_theme_id>());

    log_w(*this, log_level::info, "Irritator start\n");

    log_w(*this,
          log_level::info,
          "Starting with {} main threads and {} generic workers\n",
          task_mgr.ordered_task_workers.ssize(),
          task_mgr.unordered_task_workers.ssize());

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

void application::free_project_window(const project_id id) noexcept
{
    if (auto* pj = pjs.try_to_get(id); pj) {
        if (is_defined(pj->project_file))
            mod.registred_paths.free(pj->project_file);
    }

    pjs.free(id);
}

static void init_registred_path(application& app, config_manager& cfg) noexcept
{
    cfg.read_write(
      [](auto& vars, auto& app) noexcept {
          for (const auto id : vars.rec_paths.ids) {
              const auto idx = get_index(id);

              auto& new_dir    = app.mod.registred_paths.alloc();
              auto  new_dir_id = app.mod.registred_paths.get_id(new_dir);
              new_dir.name     = vars.rec_paths.names[idx].sv();
              new_dir.path     = vars.rec_paths.paths[idx].sv();
              new_dir.priority = vars.rec_paths.priorities[idx];

              app.notifications.try_insert(
                log_level::info, [&new_dir](auto& title, auto& msg) noexcept {
                    title = "New directory registred";
                    format(msg,
                           "{} registred as path `{}' priority: {}",
                           new_dir.name.sv(),
                           new_dir.path.sv(),
                           new_dir.priority);
                });

              app.mod.component_repertories.emplace_back(new_dir_id);
          }
      },
      app);
}

bool application::init() noexcept
{
    if (not mod.init(modeling_initializer{})) {
        log_w(*this,
              log_level::error,
              "Fail to initialize modeling components: {}\n");
        return false;
    }

    init_registred_path(*this, config);

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

static void open_project(application& app) noexcept
{
    const char*    title     = "Select project file path to load";
    const char8_t* filters[] = { u8".irt", nullptr };

    ImGui::OpenPopup(title);
    if (app.f_dialog.show_load_file(title, filters)) {
        if (app.f_dialog.state == file_dialog::status::ok) {
            auto  u8str = app.f_dialog.result.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (app.mod.registred_paths.can_alloc(1)) {
                auto& path = app.mod.registred_paths.alloc();
                auto  id   = app.mod.registred_paths.get_id(path);
                path.path  = str;

                if (auto opt = app.alloc_project_window(); opt.has_value()) {
                    auto& pj        = app.pjs.get(*opt);
                    pj.project_file = id;
                    app.start_load_project(id, *opt);
                }
            }
        }

        app.f_dialog.clear();
    }
}

void application::request_open_directory_dlg(
  const registred_path_id id) noexcept
{
    debug::ensure(not show_select_reg_path);

    show_select_reg_path = true;
    selected_reg_path    = id;
}

auto application::show_menu() noexcept -> show_result_t
{
    show_result_t ret = show_result_t::success;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New project"))
                alloc_project_window();

            if (ImGui::MenuItem("Open project"))
                menu_load_project_file = true;

            if (ImGui::MenuItem("Quit"))
                ret = show_result_t::request_to_close;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show modeling editor", nullptr, &component_ed.is_open);
            ImGui::MenuItem("Show output editor", nullptr, &output_ed.is_open);

            ImGui::MenuItem(
              "Show component hierarchy", nullptr, &library_wnd.is_open);

            ImGui::MenuItem("Show memory usage", nullptr, &memory_wnd.is_open);
            ImGui::MenuItem("Show task usage", nullptr, &task_wnd.is_open);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("ImGui demo window", nullptr, &show_imgui_demo);
            ImGui::MenuItem("ImPlot demo window", nullptr, &show_implot_demo);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &settings_wnd.is_open);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (menu_load_project_file)
        open_project(*this);

    if (show_imgui_demo)
        ImGui::ShowDemoWindow(&show_imgui_demo);

    if (show_implot_demo)
        ImPlot::ShowDemoWindow(&show_implot_demo);

    return ret;
}

void application::show_dock() noexcept
{
    static auto                  first_time = true;
    constexpr ImGuiDockNodeFlags dockspace_flags =
      ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    main_dock_id = ImGui::DockSpaceOverViewport(0, viewport, dockspace_flags);

    if (first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(main_dock_id);
        ImGui::DockBuilderAddNode(
          main_dock_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(main_dock_id, viewport->Size);

        right_dock_id = ImGui::DockBuilderSplitNode(
          main_dock_id, ImGuiDir_Right, 0.2f, nullptr, &main_dock_id);
        bottom_dock_id = ImGui::DockBuilderSplitNode(
          main_dock_id, ImGuiDir_Down, 0.2f, nullptr, &main_dock_id);

        ImGui::DockBuilderDockWindow(component_editor::name, main_dock_id);
        ImGui::DockBuilderDockWindow(output_editor::name, main_dock_id);

        ImGui::DockBuilderDockWindow(library_wnd.name, right_dock_id);

        ImGui::DockBuilderDockWindow(window_logger::name, bottom_dock_id);

        ImGui::DockBuilderFinish(main_dock_id);
    }

    if (component_ed.is_open)
        component_ed.display();

    project_editor* pj     = nullptr;
    project_editor* to_del = nullptr;
    while (pjs.next(pj)) {
        pj->start_simulation_update_state(*this);
        if (pj->show(*this) == project_editor::show_result_t::request_to_close)
            to_del = pj;
    }

    if (to_del)
        free_project_window(pjs.get_id(*to_del));

    if (output_ed.is_open)
        output_ed.show();

    if (log_wnd.is_open)
        log_wnd.show();

    if (library_wnd.is_open)
        library_wnd.show();
}

auto application::show() noexcept -> show_result_t
{
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

    auto ret = show_menu();
    show_dock();

    if (memory_wnd.is_open)
        memory_wnd.show();

    if (task_wnd.is_open)
        task_wnd.show();

    if (settings_wnd.is_open)
        settings_wnd.show();

    if (show_select_reg_path) {
        const char* title = "Select directory";
        ImGui::OpenPopup(title);
        if (f_dialog.show_select_directory(title)) {
            if (f_dialog.state == file_dialog::status::ok) {
                auto* dir_path =
                  mod.registred_paths.try_to_get(selected_reg_path);
                if (dir_path) {
                    auto str = f_dialog.result.string();
                    dir_path->path.assign(str);
                }

                show_select_reg_path = false;
                selected_reg_path    = undefined<registred_path_id>();
                f_dialog.result.clear();
            }

            f_dialog.clear();
            show_select_reg_path = false;
        }
    }

    notifications.show();

    return ret;
}

static void show_select_model_box_recursive(application&    app,
                                            project_editor& ed,
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

auto build_unique_component_vector(application& app, tree_node& tn)
  -> vector<component_id>
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
                           project_editor& ed,
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
                           project_editor& ed,
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
    return task_mgr
      .unordered_task_lists[std::clamp(idx, 0, unordered_task_worker_size - 1)];
}

std::optional<file> application::try_open_file(const char* filename,
                                               open_mode   mode) noexcept
{
    debug::ensure(filename);

    if (not filename)
        return std::nullopt;

    if (auto f = file::open(filename, mode); not f) {
        notifications.try_insert(log_level::error,
                                 [&](auto& title, auto& msg) noexcept {
                                     title = "File open error";
                                     format(msg, "Open file {}", filename);
                                 });

        return std::nullopt;
    } else
        return std::make_optional(std::move(*f));
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
    add_gui_task([&, id, pj_id]() noexcept {
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
            if (auto ret = mod.save(*c); not ret) {
                notifications.try_insert(
                  log_level::error, [&](auto& title, auto& msg) {
                      title = "Component save error";
                      format(msg,
                             "Fail to save {} (part: {} {}",
                             c->name.sv(),
                             ordinal(ret.error().cat()),
                             ret.error().value());
                  });
            } else {
                notifications.try_insert(
                  log_level::notice, [&](auto& title, auto& msg) {
                      title = "Component save";
                      format(msg, "Save {} success", c->name.sv());
                  });
            }
        }
    });
}

void application::start_init_source(const project_id          pj_id,
                                    const u64                 id,
                                    const source::source_type type) noexcept
{
    debug::ensure(any_equal(
      type, source::source_type::constant, source::source_type::random));

    add_gui_task([this, pj_id, id, type]() noexcept {
        if (auto* sim_ed = pjs.try_to_get(pj_id)) {
            if (type == source::source_type::constant) {
                if (auto* c = sim_ed->pj.sim.srcs.constant_sources.try_to_get(
                      enum_cast<constant_source_id>(id))) {
                    (void)c->init();
                    sim_ed->data_ed.fill_plot(
                      std::span(c->buffer.data(), c->length));
                }
            } else {
                if (auto* c = sim_ed->pj.sim.srcs.random_sources.try_to_get(
                      enum_cast<random_source_id>(id))) {
                    c->max_clients = 1;
                    (void)c->init();

                    chunk_type tmp;
                    source     src;
                    src.buffer = tmp;
                    src.id     = id;
                    src.type   = type;
                    (void)c->init(src);

                    sim_ed->data_ed.fill_plot(src.buffer);
                }
            }
        }
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
