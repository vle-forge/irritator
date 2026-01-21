// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <mutex>

namespace irt {

static const std::string_view extensions[] = { "", ".dot", ".irt" };

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
        ImGui::PushID(0);
        if (ImGui::Selectable("##empty-dir", is_undefined(in_out))) {
            in_out = undefined<dir_path_id>();
            ret    = true;
        }
        ImGui::PopID();

        ret =
          ret or
          reg.children.read([&](const auto& vec,
                                const auto /*version*/) noexcept -> bool {
              auto has_changes = false;
              for (const auto id : vec) {
                  if (auto* dir = app.mod.dir_paths.try_to_get(id)) {
                      ImGui::PushID(get_index(id));

                      if (ImGui::Selectable(dir->path.c_str(), in_out == id)) {
                          in_out      = id;
                          has_changes = true;
                      }

                      ImGui::PopID();
                  }
              }

              return has_changes;
          });
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

                reg.children.write(
                  [&](auto& vec) noexcept { vec.emplace_back(dir_id); });
                ret = true;

                if (not app.mod.create_directories(new_dir)) {
                    app.jn.push(
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
    auto* file = app.mod.file_paths.try_to_get(in_out);

    if (not file) {
        auto& f     = app.mod.file_paths.alloc();
        in_out      = app.mod.file_paths.get_id(f);
        f.component = undefined<component_id>();
        f.parent    = app.mod.dir_paths.get_id(dir);
        f.type      = file_path::file_type::dot_file;

        dir.children.write(
          [&](auto& vec) noexcept { vec.emplace_back(in_out); });

        file = &f;
    }

    if (ImGui::InputFilteredString("File##text", file->path)) {
        if (not end_with(file->path.sv(), opt)) {
            add_extension(file->path, extensions[ordinal(opt)]);
        }
    }

    return is_valid_dot_filename(file->path.sv());
}

auto get_component_color(const application& app, const component_id id) noexcept
  -> const std::span<const float, 4>
{
    return app.mod.components.exists(id)
             ? app.mod.components.get<component_color>(id)
             : app.config.colors[style_color::component_undefined];
}

auto get_component_u32color(const application& app,
                            const component_id id) noexcept -> ImU32
{
    return to_ImU32(get_component_color(app, id));
}

auto file_path_selector(application&              app,
                        file_path_selector_option opt,
                        registred_path_id&        reg_id,
                        dir_path_id&              dir_id,
                        file_path_id&             file_id) noexcept -> bool
{
    auto ret = false;
    combobox_reg(app, reg_id);

    if (auto* reg = app.mod.registred_paths.try_to_get(reg_id)) {
        combobox_dir(app, *reg, dir_id);

        if (auto* dir = app.mod.dir_paths.try_to_get(dir_id); not dir)
            combobox_newdir(app, *reg, dir_id);

        if (auto* dir = app.mod.dir_paths.try_to_get(dir_id))
            if (combobox_file(app, opt, *dir, file_id))
                ret = true;
    }

    return ret;
}

void simulation_to_cpp::show(const project_editor& ed) noexcept
{
    ImGui::SeparatorText("C++");

    static const char* names[] = { "models and connections",
                                   "and final tests",
                                   "and tests in progress" };

    debug::ensure(options <= 2u);

    auto opt = static_cast<int>(options);
    if (ImGui::Combo("##", &opt, names, length(names))) {
        if (opt != static_cast<int>(options)) {
            options = static_cast<u8>(opt);
            status  = status_type::none;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("generate")) {
        auto& app = container_of(this, &application::sim_to_cpp);

        app.add_gui_task([&]() {
            const auto ret = write_test_simulation(
              std::cout,
              ed.pj.name.sv(),
              ed.pj.sim,
              ed.pj.sim.limits.begin(),
              ed.pj.sim.limits.end(),
              enum_cast<write_test_simulation_options>(options));

            std::cout.flush();

            switch (ret) {
            case write_test_simulation_result::success:
                status = status_type::success;
                break;

            case write_test_simulation_result::output_error:
                status = status_type::output_error;
                break;

            case write_test_simulation_result::external_source_error:
                status = status_type::external_source_error;
                break;

            case write_test_simulation_result::hsm_error:
                status = status_type::hsm_error;
                break;

            default:
                status = status_type::none;
            }
        });
    }

    switch (status) {
    case status_type::none:
        break;

    case status_type::success:
        ImGui::TextUnformatted("write success");
        break;

    case status_type::output_error:
        ImGui::TextUnformatted("error writing simulation to output stream");
        break;

    case status_type::external_source_error:
        ImGui::TextUnformatted(
          "error in external source: only use constant-source");
        break;

    case status_type::hsm_error:
        ImGui::TextUnformatted("error in project: missing HSM definition");
        break;
    }
}

application::application(journal_handler& jn_) noexcept
  : task_mgr{ 4, 1 }
  , config(get_config_home(true))
  , mod{ jn_ }
  , jn{ jn_ }
  , pjs(16)
  , grids{ 16 }
  , graphs{ 16 }
  , generics{ 16 }
  , hsms{ 16 }
  , sims{ 16 }
  , graph_eds{ 16 }
  , copy_obs{ 16 }
  , m_journal_timestep{ journal_handler::get_tick_count_in_milliseconds() }
{
    settings_wnd.apply_style(config.theme);

    task_mgr.start();

    auto& msg = log_wnd.enqueue();
    format(msg,
           "Starting with {} ordered list {} unordered list and {} threads\n",
           task_mgr.ordered_size(),
           task_mgr.unordered_size(),
           task_mgr.ordered_size() + task_mgr.unordered_size());
}

application::~application() noexcept
{
    jn.push(log_level::info,
            [](auto&, auto& msg) { msg = "Task manager shutdown\n"; });

    task_mgr.shutdown();

    jn.push(log_level::info,
            [](auto&, auto& msg) { msg = "Application shutdown\n"; });
}

unordered_task_list& application::get_unordered_task_list() noexcept
{
    return task_mgr.unordered(0);
}

project_id application::alloc_project_window() noexcept
{
    if (not pjs.can_alloc(1)) {
        jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
            title = "Fail to allocate another project";
            format(msg,
                   "There is {} projects opened. Close one before.",
                   pjs.size());
        });

        return undefined<project_id>();
    }

    name_str temp;
    format(temp, "project-{}", pjs.next_key());

    auto& pj = pjs.alloc(temp.sv());

    return pjs.get_id(pj);
}

project_id application::open_project_window(const file_path_id file_id) noexcept
{
    auto* file = mod.file_paths.try_to_get(file_id);
    debug::ensure(file);
    debug::ensure(is_undefined(file->pj_id));

    if (not file or is_defined(file->pj_id) or not pjs.can_alloc(1))
        return undefined<project_id>();

    const auto pj_id = alloc_project_window();
    if (is_undefined(pj_id))
        return undefined<project_id>();

    auto& pj    = pjs.get(pj_id);
    file->pj_id = pj_id;

    const auto filename    = file->path.sv();
    const auto without_ext = filename.substr(0, filename.find_last_of('.'));

    pj.set_title_name(without_ext);

    return pj_id;
}

file_path_id application::close_project_window(const project_id pj_id) noexcept
{
    const auto* pj = pjs.try_to_get(pj_id);
    debug::ensure(pj);

    if (not pj)
        return undefined<file_path_id>();

    const auto file_id = pj->pj.file;
    auto*      file    = mod.file_paths.try_to_get(file_id);
    if (file)
        file->pj_id = undefined<project_id>();

    pjs.free(pj_id);

    return file_id;
}

void application::free_project_window(const project_id id) noexcept
{
    const auto file_id = close_project_window(id);
    if (is_defined(file_id))
        mod.file_paths.free(file_id);
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

              app.jn.push(log_level::info,
                          [&new_dir](auto& title, auto& msg) noexcept {
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
    init_registred_path(*this, config);

    if (auto ret = mod.fill_components(); !ret)
        jn.push(log_level::warning, [&](auto& title, auto& msg) noexcept {
            title = "Modeling initialization error";
            msg   = "Fail to fill read component list";
        });

    // Update the component selector in task.
    add_gui_task([&]() noexcept { this->component_sel.update(); });

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
        if (ImGui::BeginMenu("Irritator")) {
            if (ImGui::MenuItem("Quit"))
                ret = show_result_t::request_to_close;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
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

    component_ed.display();

    {
        project_editor* pj       = nullptr;
        project_editor* to_close = nullptr;
        while (pjs.next(pj)) {
            if (to_close) {
                free_project_window(pjs.get_id(*to_close));
                to_close = nullptr;
            }

            pj->start_simulation_update_state(*this);
            ImGui::PushID(pj);

            if (pj->show(*this) ==
                project_editor::show_result_t::request_to_close) {
                to_close = pj;
            }

            ImGui::PopID();
        }

        if (to_close)
            close_project_window(pjs.get_id(*to_close));
    }

    for_each_cond(sim_wnds, [&](const auto& v) noexcept {
        if (auto* pj = pjs.try_to_get(v.pj_id)) {
            auto* tn = pj->pj.tree_nodes.try_to_get(v.tn_id);
            auto* go = pj->pj.graph_observers.try_to_get(v.graph_obs_id);
            auto* ge = graph_eds.try_to_get(v.graph_ed_id);

            if (not(tn and go and ge))
                return true;

            small_string<64> name;
            format(
              name, "{}-graph-{}", pj->pj.name.sv(), get_index(v.graph_ed_id));

            if (ge->show(name.c_str(), *this, *pj, *tn, *go) ==
                graph_editor::show_result_type::request_to_close) {
                graph_eds.free(*ge);
                return true;
            }
        } else {
            return true;
        }

        return false;
    });

    if (output_ed.is_open)
        output_ed.show();

    if (log_wnd.is_open)
        log_wnd.show();

    if (library_wnd.is_open)
        library_wnd.show();
}

auto application::show() noexcept -> show_result_t
{
    auto ret = show_menu();
    show_dock();

    // Add a gui task to copy journal_handler entries into the log_window's
    // buffer once a time at 1.000 ms and flush the journal_handler.

    if (auto c = journal_handler::get_tick_count_in_milliseconds();
        (c - m_journal_timestep) > 1'000u) {
        m_journal_timestep = c;

        if (not journal_displayed.test_and_set()) {
            add_gui_task([&]() {
                jn.flush(
                  [](auto& ring,
                     auto& ids,
                     auto& titles,
                     auto& descriptions,
                     auto& notifications) noexcept {
                      for (const auto id : ring) {
                          if (ids.exists(id)) {
                              notifications.enqueue(ids[id].second,
                                                    titles[id].sv(),
                                                    descriptions[id].sv(),
                                                    ids[id].first);
                          }
                      }
                  },
                  notifications);

                journal_displayed.clear();
            });
        }
    }

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

        if (auto* compo = app.mod.components.try_to_get<component>(cur->id);
            compo) {
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

    if (ImGui::Button(button_label)) {
        debug::ensure(ed.pj.tree_nodes.get_id(tn) == access.parent_id);
        app.component_model_sel.update(ed.pj,
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

        if (const auto r = app.component_model_sel.combobox(
              "Select model to observe grid", ed.pj);
            r.has_value()) {
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                access.parent_id = r->parent_id;
                access.compo_id  = r->compo_id;
                access.tn_id     = r->tn_id;
                access.mdl_id    = r->mdl_id;
                ImGui::CloseCurrentPopup();
                ret = true;
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        } else {
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

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

    if (ImGui::Button(button_label)) {
        debug::ensure(ed.pj.tree_nodes.get_id(tn) == access.parent_id);
        app.component_model_sel.update(ed.pj,
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

        if (const auto r = app.component_model_sel.combobox(
              "Select model to observe graph", ed.pj);
            r.has_value()) {

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                access.parent_id = r->parent_id;
                access.compo_id  = r->compo_id;
                access.tn_id     = r->tn_id;
                access.mdl_id    = r->mdl_id;
                ImGui::CloseCurrentPopup();
                ret = true;
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        } else {
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    return ret;
}

std::optional<file> application::try_open_file(const char* filename,
                                               open_mode   mode) noexcept
{
    debug::ensure(filename);

    if (not filename)
        return std::nullopt;

    if (auto f = file::open(filename, mode); not f) {
        jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
            title = "File open error";
            format(msg, "Open file {}", filename);
        });

        return std::nullopt;
    } else
        return std::make_optional(std::move(*f));
}

void application::start_load_project(const project_id pj_id) noexcept
{
    add_gui_task([&, pj_id]() noexcept {
        auto* pj = pjs.try_to_get(pj_id);
        if (not pj)
            return;

        auto* file = mod.file_paths.try_to_get(pj->pj.file);
        if (not file)
            return;

        if (auto ret = pj->pj.load(mod); ret.has_value()) {
            pj->disable_access = false;

            jn.push(log_level::info, [&](auto& title, auto& /*msg*/) noexcept {
                format(
                  title, "Loading project file {} success", file->path.sv());
            });
        } else {
            jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                format(title, "Loading project file {} error", file->path.sv());

                if (ret.error().cat() == category::project) {
                    if (static_cast<project_errc>(ret.error().value()) ==
                        project_errc::file_access_error) {
                        msg = "Access error.";
                    }
                } else if (ret.error().cat() == category::json) {
                    switch (static_cast<json_errc>(ret.error().value())) {
                    case json_errc::memory_error:
                        msg = "json de-archiving memory error: not "
                              "enough memory\n";
                        break;

                    case json_errc::arg_error:
                        msg = "json de-archiving internal error\n";
                        break;

                    case json_errc::file_error:
                        msg = "json de-archiving memory error: not "
                              "enough memory\n";
                        break;

                    case json_errc::invalid_project_format:
                        format(msg,
                               "json de-archiving json format error "
                               "`{}' at offset "
                               "{}\n",
                               0,
                               0);
                        break;

                    default:
                        format(msg, "json de-archiving unknown error\n");
                    }
                }
            });
        }
    });
}

void application::start_save_project(const project_id pj_id) noexcept
{
    add_gui_task([&, pj_id]() noexcept {
        auto* pj_ed = pjs.try_to_get(pj_id);
        if (not pj_ed)
            return;

        if (auto ret = pj_ed->pj.save(mod); ret) {
            jn.push(log_level::info, [&](auto& title, auto& /*msg*/) noexcept {
                format(title,
                       "Saving project file {} success",
                       mod.file_paths.get(pj_ed->pj.file).path.sv());
            });
        } else {
            jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                const auto* f    = mod.file_paths.try_to_get(pj_ed->pj.file);
                const auto  name = f ? f->path.sv() : std::string_view{ "-" };

                format(title, "Saving project file {} error", name);

                if (ret.error().cat() == category::project) {
                    if (static_cast<project_errc>(ret.error().value()) ==
                        project_errc::file_access_error) {
                        msg = "Access error.";
                    }
                } else if (ret.error().cat() == category::json) {
                    switch (static_cast<json_errc>(ret.error().value())) {
                    case json_errc::memory_error:
                        format(msg,
                               "json archiving memory error: not "
                               "enough memory\n");
                        break;

                    case json_errc::arg_error:
                        format(msg, "json archiving internal error\n");
                        break;

                    case json_errc::file_error:
                        format(msg,
                               "json archiving memory error: not "
                               "enough memory\n");
                        break;

                    case json_errc::invalid_project_format:
                        format(msg,
                               "json archiving json format error "
                               "`{}' at offset "
                               "{}\n",
                               0,
                               0);
                        break;

                    default:
                        format(msg, "json de-archiving unknown error\n");
                    }
                }
            });
        }
    });
}

void application::start_save_component(const component_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        if (auto* c = mod.components.try_to_get<component>(id); c) {
            if (auto ret = mod.save(*c); not ret) {
                jn.push(log_level::error, [&](auto& title, auto& msg) {
                    title = "Component save error";
                    format(msg,
                           "Fail to save {} (part: {} {}",
                           c->name.sv(),
                           ordinal(ret.error().cat()),
                           ret.error().value());
                });
            } else {
                jn.push(log_level::notice, [&](auto& title, auto& msg) {
                    title = "Component save";
                    format(msg, "Save {} success", c->name.sv());
                });
            }
        }
    });
}

void application::start_init_source(const project_id  pj_id,
                                    const u64         id,
                                    const source_type type) noexcept
{
    debug::ensure(any_equal(type, source_type::constant, source_type::random));

    add_gui_task([this, pj_id, id, type]() noexcept {
        if (auto* sim_ed = pjs.try_to_get(pj_id)) {
            if (type == source_type::constant) {
                const auto constant_id = enum_cast<constant_source_id>(id);

                if (auto* c = sim_ed->pj.sim.srcs.constant_sources.try_to_get(
                      constant_id)) {
                    if (c->init().has_error())
                        return;

                    data_ed.fill_plot(std::span(c->buffer.data(), c->length));
                }
            } else {
                const auto random_id = enum_cast<random_source_id>(id);

                if (auto* c = sim_ed->pj.sim.srcs.random_sources.try_to_get(
                      random_id)) {

                    if (c->init().has_error())
                        return;

                    source      src;
                    source_data data;

                    src.index  = 0;
                    src.buffer = std::span(data.chunk_real);

                    data.chunk_id[0] = 0x648593178264597;
                    data.chunk_id[1] =
                      static_cast<u64>(reinterpret_cast<uintptr_t>(c));
                    data.chunk_id[2] = 0;
                    data.chunk_id[3] = 0;
                    data.chunk_id[4] = 0;
                    data.chunk_id[5] = 0;

                    chunk_type buffer;

                    c->fill(std::span(buffer), src, data);

                    data_ed.fill_plot(buffer);
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
            dir->status = dir_path::state::lock;
            dir->refresh(mod);
            dir->status = dir_path::state::read;
        }
    });
}

void application::start_dir_path_free(const dir_path_id id) noexcept
{
    add_gui_task([&, id]() noexcept {
        auto* dir = mod.dir_paths.try_to_get(id);

        if (dir and dir->status != dir_path::state::lock) {
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
            mod.remove_file(*reg, *dir, *file);

            jn.push(log_level::info, [&](auto& t, auto& m) {
                t = "File manager";
                format(m, "File `{}' was removed", file->path.sv());
            });
        }
    });
}

} // namespace irt
