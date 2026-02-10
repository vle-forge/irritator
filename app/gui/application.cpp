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
#include "internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irt {

static constexpr const char* empty = "-";

static auto get_reg_preview(const modeling::file_access& fs,
                            const registred_path_id id) noexcept -> const char*
{
    const auto* reg = fs.registred_paths.try_to_get(id);

    return reg ? reg->name.c_str() : empty;
}

static auto get_dir_preview(const modeling::file_access& fs,
                            const dir_path_id id) noexcept -> const char*
{
    const auto* dir = fs.dir_paths.try_to_get(id);

    return dir ? dir->path.c_str() : empty;
}

static auto combobox_reg(const modeling::file_access& fs,
                         const registred_path_id      reg_id) noexcept
  -> registred_path_id
{
    const char* preview = get_reg_preview(fs, reg_id);
    auto        ret     = undefined<registred_path_id>();

    if (ImGui::BeginCombo("Path", preview)) {
        for (const auto& reg : fs.registred_paths) {
            if (reg.status == registred_path::state::error)
                continue;

            ImGui::PushID(&reg);
            const auto id = fs.registred_paths.get_id(reg);
            if (ImGui::Selectable(
                  reg.path.c_str(), reg_id == id, ImGuiSelectableFlags_None))
                ret = id;
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return ret;
}

static auto combobox_dir(const modeling::file_access& fs,
                         const registred_path&        reg,
                         const dir_path_id dir_id) noexcept -> dir_path_id
{
    auto* dir_preview = get_dir_preview(fs, dir_id);
    auto  ret         = undefined<dir_path_id>();

    if (ImGui::BeginCombo("Dir", dir_preview)) {
        ImGui::PushID(0);
        if (ImGui::Selectable("##empty-dir", is_undefined(dir_id))) {
            ret = undefined<dir_path_id>();
        }
        ImGui::PopID();

        for (const auto id : reg.children) {
            if (const auto* dir = fs.dir_paths.try_to_get(id)) {
                ImGui::PushID(dir);
                if (ImGui::Selectable(dir->path.c_str(), dir_id == id))
                    ret = id;
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    return ret;
}

static void alloc_dir_task(application&                        app,
                           const registred_path_id             parent,
                           const std::string_view              name,
                           atomic_request_buffer<dir_path_id>& new_dir) noexcept
{
    app.add_gui_task([&app, &new_dir, parent, name]() {
        app.mod.files.write([&](auto& fs) {
            const auto newdir_id = fs.alloc_dir(parent, name);
            fs.create_directories(newdir_id);
            new_dir.fulfill(newdir_id);
        });
    });
}

static void combobox_newdir(application&                        app,
                            atomic_request_buffer<dir_path_id>& new_dir,
                            const modeling::file_access&        fs,
                            const registred_path_id             reg_id,
                            dir_path_id&                        in_out) noexcept
{
    debug::ensure(fs.dir_paths.try_to_get(in_out) == nullptr);

    if (fs.dir_paths.try_to_get(in_out) == nullptr) {
        if (const auto newdir_id = new_dir.try_take()) {
            in_out = *newdir_id;
        } else {
            directory_path_str dir_name;

            if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                if (fs.find_directory_in_registry(reg_id, dir_name.sv()) ==
                    undefined<dir_path_id>()) {
                    if (new_dir.should_request()) {
                        alloc_dir_task(app, reg_id, dir_name.sv(), new_dir);
                    }
                }
            }
        }
    }
}

static bool end_with(std::string_view v, file_path_selector_option opt) noexcept
{
    switch (opt) {
    case file_path_selector_option::none:
        return true;
    case file_path_selector_option::force_irt_extension:
        return has_extension(v, file_path::file_type::irt_file);
    case file_path_selector_option::force_dot_extension:
        return has_extension(v, file_path::file_type::dot_file);
    }

    irt::unreachable();
}

static void copy_file_task(application&       app,
                           const file_path&   file,
                           const file_path_id dst) noexcept
{
    app.add_gui_task([&app, file, dst]() noexcept {
        app.mod.files.write([&](auto& fs) noexcept {
            if (auto* file_dst = fs.file_paths.try_to_get(dst)) {
                *file_dst = file;
            }
        });
    });
}

static void alloc_file_task(
  application&                         app,
  const dir_path_id                    parent,
  atomic_request_buffer<file_path_id>& new_file) noexcept
{
    app.add_gui_task([&app, &new_file, parent]() {
        app.mod.files.write([&](auto& fs) {
            const auto newfile_id = fs.alloc_file(parent, std::string_view());

            new_file.fulfill(newfile_id);
        });
    });
}

static auto combobox_file(application&                         app,
                          atomic_request_buffer<file_path_id>& new_file,
                          const modeling::file_access&         fs,
                          const file_path_selector_option      opt,
                          const dir_path_id                    parent,
                          const file_path_id file_id) noexcept -> file_path_id
{
    auto ret = undefined<file_path_id>();

    if (is_undefined(file_id)) {
        if (const auto f = new_file.try_take()) {
            ret = *f;
        } else if (new_file.should_request()) {
            app.add_gui_task([&app, &new_file, parent]() {
                alloc_file_task(app, parent, new_file);
            });
        }
    } else {
        ret = file_id;
    }

    if (is_defined(ret)) {
        auto&     file = fs.file_paths.get(ret);
        file_path tmp(file);

        if (ImGui::InputFilteredString("File##text", tmp.path)) {
            if (not end_with(tmp.path.sv(), opt)) {
                if (opt == file_path_selector_option::force_irt_extension) {
                    add_extension(tmp.path, file_path::file_type::irt_file);
                    if (is_valid_dot_filename(tmp.path.sv()))
                        copy_file_task(app, tmp, ret);
                }

                if (opt == file_path_selector_option::force_dot_extension) {
                    add_extension(tmp.path, file_path::file_type::dot_file);
                    if (is_valid_dot_filename(tmp.path.sv()))
                        copy_file_task(app, tmp, ret);
                }
            }
        }
    }

    return ret;
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
    return app.mod.files.read(
      [&](const auto& fs, const auto /*vers*/) noexcept {
          auto ret = false;

          const auto newreg_id = combobox_reg(fs, reg_id);
          if (auto* reg = fs.registred_paths.try_to_get(newreg_id)) {
              reg_id = newreg_id;

              const auto newdir_id = combobox_dir(fs, *reg, dir_id);
              if (auto* dir = fs.dir_paths.try_to_get(newdir_id)) {
                  dir_id = newdir_id;

                  const auto newfile_id =
                    combobox_file(app, app.new_file, fs, opt, dir_id, file_id);
                  if (auto* file = fs.file_paths.try_to_get(newfile_id)) {
                      file_id = newfile_id;
                      return true;
                  }
              } else {
                  combobox_newdir(app, app.new_dir, fs, newreg_id, dir_id);
              }
          }

          return ret;
      });
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
              stdout,
              ed.pj.name.sv(),
              ed.pj.sim,
              ed.pj.sim.limits.begin(),
              ed.pj.sim.limits.end(),
              enum_cast<write_test_simulation_options>(options));

            std::fflush(stdout);

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
    if (not pjs.can_alloc(1))
        return undefined<project_id>();

    const auto pj_id = alloc_project_window();
    if (is_undefined(pj_id))
        return undefined<project_id>();

    return mod.files.write([&](auto& fs) {
        auto* file = fs.file_paths.try_to_get(file_id);

        debug::ensure(file);
        debug::ensure(is_undefined(file->pj_id));

        if (not file or is_defined(file->pj_id))
            return undefined<project_id>();

        auto& pj    = pjs.get(pj_id);
        file->pj_id = pj_id;

        const auto filename    = file->path.sv();
        const auto without_ext = filename.substr(0, filename.find_last_of('.'));

        pj.set_title_name(without_ext);

        return pj_id;
    });
}

file_path_id application::close_project_window(const project_id pj_id) noexcept
{
    const auto* pj = pjs.try_to_get(pj_id);
    debug::ensure(pj);

    if (not pj)
        return undefined<file_path_id>();

    const auto file_id = pj->pj.file;

    add_gui_task([&, file_id]() {
        mod.files.write([&](auto& fs) {
            if (auto* file = fs.file_paths.try_to_get(file_id))
                file->pj_id = undefined<project_id>();

            return file_id;
        });
    });

    pjs.free(pj_id);

    return file_id;
}

void application::free_project_window(const project_id pj_id) noexcept
{
    const auto* pj = pjs.try_to_get(pj_id);
    debug::ensure(pj);

    if (not pj)
        return;

    const auto file_id = pj->pj.file;

    add_gui_task([&, file_id]() {
        mod.files.write([&](auto& fs) {
            if (auto* file = fs.file_paths.try_to_get(file_id)) {
                file->pj_id = undefined<project_id>();
                fs.remove_file(file_id);
            }
        });
    });

    pjs.free(pj_id);
}

bool application::init() noexcept
{
    config.read_write([this](auto& vars) noexcept {
        mod.files.write([&](auto& fs) {
            for (const auto id : vars.rec_paths.ids) {
                const auto idx = get_index(id);

                auto& new_dir    = fs.registred_paths.alloc();
                auto  new_dir_id = fs.registred_paths.get_id(new_dir);
                new_dir.name     = vars.rec_paths.names[idx].sv();
                new_dir.path     = vars.rec_paths.paths[idx].sv();
                new_dir.priority = vars.rec_paths.priorities[idx];

                jn.push(log_level::info,
                        [&new_dir](auto& title, auto& msg) noexcept {
                            title = "New directory registred";
                            format(msg,
                                   "{} registred as path `{}' priority: {}",
                                   new_dir.name.sv(),
                                   new_dir.path.sv(),
                                   new_dir.priority);
                        });

                fs.component_repertories.emplace_back(new_dir_id);
            }
        });
    });

    mod.files.write([&](auto& fs) { fs.browse_registreds(jn); });

    if (auto ret = mod.fill_components()) {
        jn.push(log_level::warning, [&](auto& title, auto& msg) noexcept {
            title = "Modeling initialization error";
            msg   = "Fail to fill read component list";
        });
    }

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
            ImGui::MenuItem("Show Welcome window", nullptr, &show_welcome_wnd);

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

    if (show_welcome_wnd) {
        ImGui::SetNextWindowDockID(get_main_dock_id());

        if (not ImGui::Begin("Welcome", &show_welcome_wnd)) {
            ImGui::End();
        } else {
            ImGui::SetCursorPos(
              ImGui::GetCursorPos() +
              (ImGui::GetContentRegionAvail() - ImVec2(50.f, 15.f)) * 0.5f);
            ImGui::TextFormat("Welcome to Irritator {}.{}.{}",
                              VERSION_MAJOR,
                              VERSION_MINOR,
                              VERSION_PATCH);

            ImGui::End();
        }
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

    // if (show_select_reg_path) {
    //     const char* title = "Select directory";
    //     ImGui::OpenPopup(title);
    //     if (f_dialog.show_select_directory(title)) {
    //         if (f_dialog.state == file_dialog::status::ok) {
    //             auto* dir_path =
    //               mod.registred_paths.try_to_get(selected_reg_path);
    //             if (dir_path) {
    //                 auto str = f_dialog.result.string();
    //                 dir_path->path.assign(str);
    //             }

    //             show_select_reg_path = false;
    //             selected_reg_path    = undefined<registred_path_id>();
    //             f_dialog.result.clear();
    //         }

    //         f_dialog.clear();
    //         show_select_reg_path = false;
    //     }
    // }

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

void application::start_load_project(const project_id pj_id) noexcept
{
    add_gui_task([&, pj_id]() noexcept {
        auto* pj = pjs.try_to_get(pj_id);
        if (not pj or is_undefined(pj->pj.file))
            return;

        if (auto ret = pj->pj.load(mod); ret.has_value()) {
            pj->disable_access = false;

            jn.push(log_level::info, [&](auto& title, auto& /*msg*/) noexcept {
                mod.files.read([&](const auto& fs, const auto /*vesr*/) {
                    format(title,
                           "Loading project file {} success",
                           fs.file_paths.get(pj->pj.file).path.sv());
                });
            });
        } else {
            jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                mod.files.read([&](const auto& fs, const auto /*vesr*/) {
                    format(title,
                           "Loading project file {} error",
                           fs.file_paths.get(pj->pj.file).path.sv());
                });

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
                mod.files.read([&](const auto& fs, const auto /*vers*/) {
                    format(title,
                           "Saving project file {} success",
                           fs.file_paths.get(pj_ed->pj.file).path.sv());
                });
            });
        } else {
            jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                const small_string<127> name =
                  mod.files.read([&](const auto& fs, const auto /*vers*/) {
                      const auto* f = fs.file_paths.try_to_get(pj_ed->pj.file);
                      return f ? f->path.sv() : std::string_view{ "-" };
                  });

                format(title, "Saving project file {} error", name.sv());

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
        mod.files.write([&](auto& fs) { fs.refresh(id); });
    });
}

} // namespace irt
