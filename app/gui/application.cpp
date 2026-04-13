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

void file_selector::combobox_reg(const file_access& fs) noexcept
{
    const auto* r       = fs.registred_paths.try_to_get(reg_id_);
    const auto* preview = r ? r->path.c_str() : empty;

    if (ImGui::BeginCombo("Path", preview)) {
        if (ImGui::Selectable(empty, r == nullptr)) {
            reg_id_  = undefined<registred_path_id>();
            dir_id_  = undefined<dir_path_id>();
            file_id_ = undefined<file_path_id>();
        }

        const registred_path* list = nullptr;
        while (fs.registred_paths.next(list)) {
            if (list->status == registred_path::state::error)
                continue;

            ImGui::PushID(list);
            if (ImGui::Selectable(list->path.c_str(), r == list)) {
                if (r != list) {
                    reg_id_  = fs.registred_paths.get_id(list);
                    dir_id_  = undefined<dir_path_id>();
                    file_id_ = undefined<file_path_id>();
                }
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
}

dir_path_id get_dir_id(atomic_request_buffer<dir_path_id>& dir_ptr,
                       const dir_path_id                   other_id) noexcept
{
    if (const auto dir_opt = dir_ptr.try_take(); dir_opt.has_value())
        return *dir_opt;
    else
        return other_id;
}

static void new_file_task(application&                         app,
                          dir_path_id                          dir_id,
                          file_path::file_type                 type,
                          std::unique_ptr<directory_path_str>  filename,
                          atomic_request_buffer<file_path_id>& file_b) noexcept
{
    app.add_gui_task(
      [&app, dir_id, name = std::move(filename), type, &file_b]() {
          app.mod.files.write([&](auto& fs) {
              auto file_id = fs.alloc_file(dir_id, name->sv(), type);
              if (is_undefined(file_id)) {
                  app.jn.push(log_level::error, [&](auto& t, auto& /*m*/) {
                      t = "Fail to allocate a new directory";
                  });
              } else {
                  file_b.fulfill(file_id);
              }
          });
      });
}

static void new_directory_task(
  application&                        app,
  registred_path_id                   reg_id,
  std::unique_ptr<directory_path_str> dirname,
  atomic_request_buffer<dir_path_id>& dir_b) noexcept
{
    app.add_gui_task([&app, reg_id, name = std::move(dirname), &dir_b]() {
        app.mod.files.write([&](auto& fs) {
            auto dir_id = fs.alloc_dir(reg_id, name->sv());
            if (is_undefined(dir_id)) {
                app.jn.push(log_level::error, [&](auto& t, auto& /*m*/) {
                    t = "Fail to allocate a new directory";
                });
            } else {
                if (not fs.create_directories(dir_id)) {
                    app.jn.push(log_level::error, [&](auto& t, auto& m) {
                        t = "Fail to create a new directory";
                        format(m,
                               "Fail to open and create directory {}",
                               name->sv());
                    });

                    fs.dir_paths.free(dir_id);
                    dir_id = undefined<dir_path_id>();
                }
            }

            dir_b.fulfill(dir_id);
        });
    });
}

void file_selector::combobox_dir(application&       app,
                                 const file_access& fs) noexcept
{
    debug::ensure(fs.registred_paths.try_to_get(reg_id_));

    dir_id_             = get_dir_id(new_dir_, dir_id_);
    const auto& r       = fs.registred_paths.get(reg_id_);
    const auto* d       = fs.dir_paths.try_to_get(dir_id_);
    const auto* preview = d ? d->path.c_str() : empty;
    auto        buffer  = directory_path_str{};

    if (ImGui::BeginCombo("Directory", preview)) {
        if (ImGui::Selectable(empty, d == nullptr)) {
            dir_id_  = undefined<dir_path_id>();
            file_id_ = undefined<file_path_id>();
        }

        for (const auto d_id : r.children) {
            if (const auto* dir = fs.dir_paths.try_to_get(d_id)) {
                ImGui::PushID(dir);
                if (ImGui::Selectable(dir->path.c_str(), d_id == dir_id_)) {
                    if (dir_id_ != d_id) {
                        dir_id_  = d_id;
                        file_id_ = undefined<file_path_id>();
                    }
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    if (is_undefined(dir_id_)) {
        if (ImGui::InputFilteredString("New directory", buffer)) {
            const auto already_exist = [&]() {
                for (const auto d_id : r.children)
                    if (const auto* dir = fs.dir_paths.try_to_get(d_id))
                        if (dir->path == buffer.sv())
                            return true;

                return false;
            }();

            if (not already_exist) {
                if (new_dir_.should_request()) {
                    new_directory_task(
                      app,
                      reg_id_,
                      std::make_unique<directory_path_str>(buffer),
                      new_dir_);
                }
            }
        }
    }
}

void file_selector::combobox_dir_ro(const file_access& fs) noexcept
{
    debug::ensure(fs.registred_paths.try_to_get(reg_id_));

    const auto& r       = fs.registred_paths.get(reg_id_);
    const auto* d       = fs.dir_paths.try_to_get(dir_id_);
    const auto* preview = d ? d->path.c_str() : empty;

    if (ImGui::BeginCombo("Directory", preview)) {
        if (ImGui::Selectable(empty, d == nullptr)) {
            dir_id_  = undefined<dir_path_id>();
            file_id_ = undefined<file_path_id>();
        }

        for (const auto d_id : r.children) {
            if (const auto* dir = fs.dir_paths.try_to_get(d_id)) {
                ImGui::PushID(dir);
                if (ImGui::Selectable(dir->path.c_str(), d_id == dir_id_)) {
                    if (dir_id_ != d_id) {
                        dir_id_  = d_id;
                        file_id_ = undefined<file_path_id>();
                    }
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }
}

file_path_id get_file_id(atomic_request_buffer<file_path_id>& file_ptr,
                         const file_path_id                   other_id) noexcept
{
    if (const auto file_opt = file_ptr.try_take(); file_opt.has_value())
        return *file_opt;
    else
        return other_id;
}

void file_selector::combobox_file(application&               app,
                                  const file_access&         fs,
                                  const file_path::file_type type) noexcept
{
    debug::ensure(fs.registred_paths.try_to_get(reg_id_) != nullptr);
    debug::ensure(fs.dir_paths.try_to_get(dir_id_) != nullptr);

    file_id_            = get_file_id(new_file_, file_id_);
    const auto& d       = fs.dir_paths.get(dir_id_);
    const auto* f       = fs.file_paths.try_to_get(file_id_);
    const auto* preview = f ? f->path.c_str() : empty;
    auto        buffer  = file_path_str{};

    if (ImGui::BeginCombo("File", preview)) {
        if (ImGui::Selectable(empty, f == nullptr))
            file_id_ = undefined<file_path_id>();

        for (const auto f_id : d.children) {
            if (const auto* file = fs.file_paths.try_to_get(f_id)) {
                if (file->type == type) {
                    ImGui::PushID(file);
                    if (ImGui::Selectable(file->path.c_str(), f_id == file_id_))
                        file_id_ = f_id;
                    ImGui::PopID();
                }
            }
        }

        ImGui::EndCombo();
    }

    if (is_undefined(file_id_)) {
        if (ImGui::InputFilteredString("New file", buffer)) {
            if (not has_extension(buffer.sv(), type))
                add_extension(buffer, type);

            if (new_file_.should_request()) {
                new_file_task(app,
                              dir_id_,
                              type,
                              std::make_unique<file_path_str>(buffer),
                              new_file_);
            }
        }
    }
}

void file_selector::combobox_file_ro(const file_access&         fs,
                                     const file_path::file_type type) noexcept
{
    debug::ensure(fs.registred_paths.try_to_get(reg_id_) != nullptr);
    debug::ensure(fs.dir_paths.try_to_get(dir_id_) != nullptr);

    const auto& d       = fs.dir_paths.get(dir_id_);
    const auto* f       = fs.file_paths.try_to_get(file_id_);
    const auto* preview = f ? f->path.c_str() : empty;

    if (ImGui::BeginCombo("File", preview)) {
        if (ImGui::Selectable(empty, f == nullptr))
            file_id_ = undefined<file_path_id>();

        for (const auto f_id : d.children) {
            if (const auto* file = fs.file_paths.try_to_get(f_id)) {
                if (file->type == type) {
                    ImGui::PushID(file);
                    if (ImGui::Selectable(file->path.c_str(), f_id == file_id_))
                        file_id_ = f_id;
                    ImGui::PopID();
                }
            }
        }

        ImGui::EndCombo();
    }
}

auto file_selector::combobox(application&               app,
                             const file_access&         fs,
                             const file_path::file_type type,
                             const flags flags) noexcept -> combo_box_result
{
    auto save  = false;
    auto close = false;

    combobox_reg(fs);

    if (is_defined(reg_id_)) {
        combobox_dir(app, fs);

        if (is_defined(dir_id_)) {
            combobox_file(app, fs, type);

            if (is_defined(file_id_)) {
                const auto have_cancel = flags[flag::show_cancel_button];
                const auto buttons     = have_cancel ? 2 : 1;
                const auto size        = ImGui::ComputeButtonSize(buttons);
                const auto button_name = flags[flag::show_load_button] ? "load"
                                         : flags[flag::show_save_button]
                                           ? "save"
                                           : "select";

                if (ImGui::Button(button_name, size)) {
                    save  = true;
                    close = true;
                }

                ImGui::SameLine();

                if (have_cancel) {
                    if (ImGui::Button("cancel", size)) {
                        save  = false;
                        close = true;
                    }
                }
            }
        }
    }

    return combo_box_result{ .file_id = file_id_,
                             .close   = close,
                             .save    = save };
}

auto file_selector::combobox_ro(const file_access&         fs,
                                const file_path::file_type type,
                                const flags flags) noexcept -> combo_box_result
{
    auto save  = false;
    auto close = false;

    combobox_reg(fs);

    if (is_defined(reg_id_)) {
        combobox_dir_ro(fs);

        if (is_defined(dir_id_)) {
            combobox_file_ro(fs, type);

            if (is_defined(file_id_)) {
                const auto have_cancel = flags[flag::show_cancel_button];
                const auto buttons     = have_cancel ? 2 : 1;
                const auto size        = ImGui::ComputeButtonSize(buttons);
                const auto button_name = flags[flag::show_load_button] ? "load"
                                         : flags[flag::show_save_button]
                                           ? "save"
                                           : "select";

                if (ImGui::Button(button_name, size)) {
                    save  = true;
                    close = true;
                }

                ImGui::SameLine();

                if (have_cancel) {
                    if (ImGui::Button("cancel", size)) {
                        save  = false;
                        close = true;
                    }
                }
            }
        }
    }

    return combo_box_result{ .file_id = file_id_,
                             .close   = close,
                             .save    = save };
}

auto get_component_color(const application& app, const component_id id) noexcept
  -> const std::span<const float, 4>
{
    return app.config.vars.colors.read(
      [&](const auto& colors,
          const auto /*version*/) noexcept -> const std::span<const float, 4> {
          return app.mod.ids.read([&](const auto& ids, auto) noexcept
                                    -> const std::span<const float, 4> {
              return ids.exists(id) ? ids.component_colors[id]
                                    : colors[style_color::component_undefined];
          });
      });
}

auto get_component_u32color(const application& app,
                            const component_id id) noexcept -> ImU32
{
    return to_ImU32(get_component_color(app, id));
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
    settings_wnd.apply_style(config.vars.theme.load());

    task_mgr.start();

    jn.push(log_level::info, [&](auto& t, auto& m) noexcept {
        t = "irritator initialized";
        format(m,
               "Starting with {} ordered list {} unordered list and {} "
               "threads\n",
               task_mgr.ordered_size(),
               task_mgr.unordered_size(),
               task_mgr.ordered_size() + task_mgr.unordered_size());
    });
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
    config.vars.rec_paths.read(
      [&](const auto& paths, const auto /*vers*/) noexcept {
          mod.files.write([&](auto& fs) {
              for (const auto id : paths.ids) {
                  const auto idx = get_index(id);

                  auto& new_dir    = fs.registred_paths.alloc();
                  auto  new_dir_id = fs.registred_paths.get_id(new_dir);
                  new_dir.name     = paths.names[idx].sv();
                  new_dir.path     = paths.paths[idx].sv();
                  new_dir.priority = paths.priorities[idx];

                  jn.push(log_level::info,
                          [&new_dir](auto& title, auto& msg) noexcept {
                              title = "New directory registred";
                              format(msg,
                                     "{} registred as path `{}' priority: {}",
                                     new_dir.name.sv(),
                                     new_dir.path.sv(),
                                     new_dir.priority);
                          });

                  fs.recorded_paths.emplace_back(new_dir_id);
              }
          });
      });

    if (auto ret = mod.fill_components(); ret.has_error()) {
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

    if (memory_wnd.is_open)
        memory_wnd.show();

    if (task_wnd.is_open)
        task_wnd.show();

    if (settings_wnd.is_open)
        settings_wnd.show();

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

        app.mod.ids.read([&](const auto& ids, auto) {
            if (ids.exists(cur->id))
                if (auto it = std::find(ret.begin(), ret.end(), cur->id);
                    it == ret.end())
                    ret.emplace_back(cur->id);
        });

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
        mod.ids.read([&](const auto& ids, auto) noexcept {
            if (ids.exists(id)) {
                const auto& compo = ids.components[id];

                if (auto ret = mod.save(id); not ret) {
                    jn.push(log_level::error, [&](auto& title, auto& msg) {
                        title = "Component save error";
                        format(msg,
                               "Fail to save {} (part: {} {}",
                               compo.name.sv(),
                               ordinal(ret.error().cat()),
                               ret.error().value());
                    });
                } else {
                    jn.push(log_level::notice, [&](auto& title, auto& msg) {
                        title = "Component save";
                        format(msg, "Save {} success", compo.name.sv());
                    });
                }
            }
        });
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
