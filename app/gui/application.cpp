// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

namespace irt {

template<i32 Size>
static void render_notifications(
  data_array<notification, notification_id>& data,
  ring_buffer<notification_id, Size>&        array) noexcept;

bool application::init()
{
    c_editor.init();

    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
        std::fprintf(
          stderr, "Fail to initialize irritator: %s\n", status_string(ret));
        return false;
    }

    mod_init = { .model_capacity              = 256 * 64 * 16,
                 .tree_capacity               = 256 * 16,
                 .parameter_capacity          = 256 * 8,
                 .description_capacity        = 256 * 16,
                 .component_capacity          = 256 * 128,
                 .file_path_capacity          = 256 * 256,
                 .children_capacity           = 256 * 64 * 16,
                 .connection_capacity         = 256 * 64,
                 .port_capacity               = 256 * 64,
                 .constant_source_capacity    = 16,
                 .binary_file_source_capacity = 16,
                 .text_file_source_capacity   = 16,
                 .random_source_capacity      = 16,
                 .random_generator_seed       = 123456789u };

    if (auto ret = c_editor.mod.dir_paths.init(max_component_dirs);
        is_bad(ret)) {
        log_w.log(2, "Fail to initialize dir paths");
    }

    if (auto path = get_system_component_dir(); path) {
        auto& new_dir = c_editor.mod.dir_paths.alloc();
        new_dir.name  = "System directory";
        new_dir.path  = path.value().string().c_str();
        log_w.log(7, "Add system directory: %s\n", new_dir.path.c_str());
    }

    if (auto path = get_default_user_component_dir(); path) {
        auto& new_dir = c_editor.mod.dir_paths.alloc();
        new_dir.name  = "User directory";
        new_dir.path  = path.value().string().c_str();
        log_w.log(7, "Add user directory: %s\n", new_dir.path.c_str());
    }

    if (auto ret = load_settings(); is_bad(ret)) {
        log_w.log(2, "Fail to read settings files. Default parameters used\n");
    }

    {
        // Initialize component_repertories used into coponents-window

        dir_path* dir = nullptr;
        while (c_editor.mod.dir_paths.next(dir)) {
            auto id = c_editor.mod.dir_paths.get_id(*dir);
            c_editor.mod.component_repertories.emplace_back(id);
        }
    }

    if (auto ret = c_editor.mod.init(mod_init); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize modeling components: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = save_settings(); is_bad(ret)) {
        log_w.log(2, "Fail to save settings files.\n");
    }

    if (auto ret = c_editor.sim.init(mod_init.model_capacity,
                                     mod_init.model_capacity * 256);
        is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize simulation components: %s\n",
                  status_string(ret));
        return false;
    }

    if (auto ret = notifications.init(10); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize notifications: %s\n", status_string(ret));
        return false;
    }

    if (auto ret = c_editor.outputs.init(32); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize memory output: %s\n", status_string(ret));
        return false;
    }

    if (auto ret = c_editor.srcs.init(50); is_bad(ret)) {
        log_w.log(
          2, "Fail to initialize external sources: %s\n", status_string(ret));
        return false;
    } else {
        c_editor.sim.source_dispatch = c_editor.srcs;
    }

    if (auto ret = c_editor.mod.fill_internal_components(); is_bad(ret)) {
        log_w.log(2, "Fail to fill component list: %s\n", status_string(ret));
    }

    c_editor.mod.fill_components();
    c_editor.mod.head           = undefined<tree_node_id>();
    c_editor.selected_component = undefined<tree_node_id>();

    try {
        simulation_duration.resize(editors.capacity(), 0);
    } catch (const std::bad_alloc& /*e*/) {
        return false;
    }

    return true;
}

bool application::show()
{
    static bool new_project_file     = false;
    static bool load_project_file    = false;
    static bool save_project_file    = false;
    static bool save_as_project_file = false;
    bool        ret                  = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("New project"))
                new_project_file = true;

            if (ImGui::MenuItem("Open project"))
                load_project_file = true;

            if (ImGui::MenuItem("Save project"))
                save_project_file = true;

            if (ImGui::MenuItem("Save project as..."))
                save_as_project_file = true;

            if (ImGui::MenuItem("Quit"))
                ret = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem(
              "Show component memory window", nullptr, &c_editor.show_memory);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);
            ImGui::Separator();
            ImGui::MenuItem(
              "Component settings", nullptr, &c_editor.show_settings);

            if (ImGui::MenuItem("Load settings"))
                load_settings();

            if (ImGui::MenuItem("Save settings"))
                save_settings();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Old menu")) {
            if (ImGui::MenuItem("New")) {
                if (auto* ed = alloc_editor(); ed)
                    ed->context = ImNodes::EditorContextCreate();
            }

            editor* ed = nullptr;
            while (editors.next(ed))
                ImGui::MenuItem(ed->name.c_str(), nullptr, &ed->show);

            ImGui::MenuItem("Simulation", nullptr, &show_simulation);
            ImGui::MenuItem("Plot", nullptr, &show_plot);
            ImGui::MenuItem("Settings", nullptr, &show_settings);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (new_project_file) {
        c_editor.mod.clear_project();
    }

    if (load_project_file) {
        const char*    title     = "Select project file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (f_dialog.show_load_file(title, filters)) {
            if (f_dialog.state == file_dialog::status::ok) {
                c_editor.project_file = f_dialog.result;
                auto  u8str           = c_editor.project_file.u8string();
                auto* str = reinterpret_cast<const char*>(u8str.c_str());

                if (auto ret = c_editor.mod.load_project(str); is_bad(ret)) {
                    auto  file = c_editor.project_file.generic_u8string();
                    auto* generic_str =
                      reinterpret_cast<const char*>(file.c_str());

                    auto& notif = push_notification(notification_type::error);
                    notif.title = "Save project";
                    format(notif.message,
                           "Error {} in project file loading ({})",
                           status_string(ret),
                           generic_str);
                } else {
                    auto& notif = push_notification(notification_type::success);
                    notif.title = "Load project";
                }
            }

            f_dialog.clear();
            load_project_file = false;
        }
    }

    if (save_project_file) {
        const bool have_file = !c_editor.project_file.empty();

        if (have_file) {
            auto  u8str = c_editor.project_file.u8string();
            auto* str   = reinterpret_cast<const char*>(u8str.c_str());

            if (auto ret = c_editor.mod.save_project(str); is_bad(ret)) {
                auto  file        = c_editor.project_file.generic_u8string();
                auto* generic_str = reinterpret_cast<const char*>(file.c_str());

                auto& notif = push_notification(notification_type::error);
                notif.title = "Save project";
                format(notif.message,
                       "Error {} in project file writing {}",
                       status_string(ret),
                       generic_str);
            } else {
                auto& notif = push_notification(notification_type::success);
                notif.title = "Save project";
            }
        } else {
            save_project_file    = false;
            save_as_project_file = true;
        }
    }

    if (save_as_project_file) {
        const char*              title = "Select project file path to save";
        const std::u8string_view default_filename = u8"filename.irt";
        const char8_t*           filters[]        = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (f_dialog.show_save_file(title, default_filename, filters)) {
            if (f_dialog.state == file_dialog::status::ok) {
                c_editor.project_file = f_dialog.result;
                auto  u8str           = c_editor.project_file.u8string();
                auto* str = reinterpret_cast<const char*>(u8str.c_str());

                if (auto ret = c_editor.mod.save_project(str); is_bad(ret)) {
                    auto  file = c_editor.project_file.generic_u8string();
                    auto* generic_str =
                      reinterpret_cast<const char*>(file.c_str());

                    auto& notif = push_notification(notification_type::error);
                    notif.title = "Save project";
                    format(notif.message,
                           "Error {} in project file writing {}",
                           status_string(ret),
                           generic_str);
                } else {
                    auto& notif = push_notification(notification_type::success);
                    notif.title = "Save project";
                }
            }

            f_dialog.clear();
            save_as_project_file = false;
        }
    }

    editor* ed = nullptr;
    while (editors.next(ed)) {
        if (ed->show) {
            if (!ed->show_window(log_w)) {
                editor* next = ed;
                editors.next(next);
                free_editor(*ed);
            } else {
                if (ed->show_settings)
                    ed->settings.show(&ed->show_settings);
            }
        }
    }

    if (show_simulation)
        show_simulation_window();

    if (show_plot)
        show_plot_window();

    if (show_settings)
        show_settings_window();

    if (show_demo)
        ImGui::ShowDemoWindow();

    if (show_modeling)
        c_editor.show(&show_modeling);

    render_notifications(notifications, notification_buffer);

    return ret;
}

editor* application::alloc_editor()
{
    if (!editors.can_alloc(1u)) {
        auto& notif = push_notification(notification_type::error);
        notif.title = "Too many open editor";
        return nullptr;
    }

    auto& ed = editors.alloc();
    if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret)) {
        auto& notif = push_notification(notification_type::error);
        notif.title = "Editor allocation error";
        format(notif.message, "Error: {}", status_string(ret));
        editors.free(ed);
        return nullptr;
    }

    log_w.log(5, "Open editor %s\n", ed.name.c_str());
    return &ed;
}

void application::free_editor(editor& ed)
{
    log_w.log(5, "Close editor %s\n", ed.name.c_str());
    editors.free(ed);
}

void application::show_plot_window()
{
    ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Plot", &show_plot)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        if (ImPlot::BeginPlot("simulation", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);

            plot_output* out = nullptr;
            while (ed->plot_outs.next(out)) {
                if (!out->xs.empty() && !out->ys.empty())
                    ImPlot::PlotLine(out->name.c_str(),
                                     out->xs.data(),
                                     out->ys.data(),
                                     static_cast<int>(out->xs.size()));
            }

            ImPlot::PopStyleVar(1);
            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

void application::show_settings_window()
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Settings", &show_settings)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Home.......: %s",
                reinterpret_cast<const char*>(home_dir.u8string().c_str()));
    ImGui::Text(
      "Executable.: %s",
      reinterpret_cast<const char*>(executable_dir.u8string().c_str()));

    ImGui::End();
}

void application::shutdown()
{
    c_editor.shutdown();
    editors.clear();
    log_w.clear();
}

static void run_for(editor& ed, long long int duration_in_microseconds) noexcept
{
    namespace stdc = std::chrono;

    auto          start_at             = stdc::high_resolution_clock::now();
    long long int duration_since_start = 0;

    if (ed.simulation_bag_id < 0) {
        ed.sim.clean();
        ed.simulation_current     = ed.simulation_begin;
        ed.simulation_during_date = ed.simulation_begin;
        ed.models_make_transition.resize(ed.sim.models.capacity(), false);

        if (ed.sim_st = ed.sim.initialize(ed.simulation_current);
            irt::is_bad(ed.sim_st)) {
            return;
        }

        ed.simulation_next_time = ed.sim.sched.empty()
                                    ? time_domain<time>::infinity
                                    : ed.sim.sched.tn();
        ed.simulation_bag_id    = 0;
    }

    if (ed.st == editor_status::running) {
        do {
            ++ed.simulation_bag_id;
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                ed.sim.finalize(ed.simulation_end);
                return;
            }

            if (ed.simulation_current >= ed.simulation_end) {
                ed.st = editor_status::editing;
                ed.sim.finalize(ed.simulation_end);
                return;
            }

            auto end_at   = stdc::high_resolution_clock::now();
            auto duration = end_at - start_at;
            auto duration_cast =
              stdc::duration_cast<stdc::microseconds>(duration);
            duration_since_start = duration_cast.count();
        } while (duration_since_start < duration_in_microseconds);
        return;
    }

    const int loop = ed.st == editor_status::running_1_step    ? 1
                     : ed.st == editor_status::running_10_step ? 10
                                                               : 100;

    for (int i = 0; i < loop; ++i) {
        ++ed.simulation_bag_id;
        if (ed.sim_st = ed.sim.run(ed.simulation_current); is_bad(ed.sim_st)) {
            ed.st = editor_status::editing;
            ed.sim.finalize(ed.simulation_end);
            return;
        }

        if (ed.simulation_current >= ed.simulation_end) {
            ed.st = editor_status::editing;
            ed.sim.finalize(ed.simulation_end);
            return;
        }
    }

    ed.st = editor_status::running_pause;
}

void application::run_simulations()
{
    auto running_simulation = 0.f;

    editor* ed = nullptr;
    while (editors.next(ed))
        if (ed->is_running())
            ++running_simulation;

    if (!running_simulation)
        return;

    const auto duration = 10000.f / running_simulation;

    ed = nullptr;
    while (editors.next(ed))
        if (ed->is_running())
            run_for(
              *ed,
              static_cast<long long int>(duration * ed->synchronize_timestep));
}

editor* application::make_combo_editor_name(editor_id& current) noexcept
{
    editor* first = editors.try_to_get(current);
    if (first == nullptr) {
        if (!editors.next(first)) {
            current = undefined<editor_id>();
            return nullptr;
        }
    }

    current = editors.get_id(first);

    if (ImGui::BeginCombo("Name", first->name.c_str())) {
        editor* ed = nullptr;
        while (editors.next(ed)) {
            const bool is_selected = current == editors.get_id(ed);

            if (ImGui::Selectable(ed->name.c_str(), is_selected))
                current = editors.get_id(ed);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return editors.try_to_get(current);
}

//
// Notification buffer
//

static u64 get_tick_count_in_milliseconds() noexcept
{
    namespace sc = std::chrono;

    return duration_cast<sc::milliseconds>(
             sc::steady_clock::now().time_since_epoch())
      .count();
}

constexpr u32   notification_duration          = 3000;
constexpr float notification_x_padding         = 20.f;
constexpr float notification_y_padding         = 20.f;
constexpr float notification_y_message_padding = 20.f;
constexpr u64   notification_fade_duration     = 150;

constexpr ImGuiWindowFlags notification_flags =
  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
  ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
  ImGuiWindowFlags_NoFocusOnAppearing;

enum class notification_state
{
    fadein,
    wait,
    fadeout,
    expired
};

static inline const ImVec4 notification_text_color[] = {
    { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f },
    { 0.46f, 0.78f, 0.59f, 1.f },
    { 0.78f, 0.49f, 0.46f, 1.f },
    { 0.46f, 0.59f, 0.78f, 1.f }
};

static inline const ImVec4 notification_color[] = {
    { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f },
    { 0.16f, 0.48f, 0.29f, 1.f },
    { 0.48f, 0.29f, 0.16f, 1.f },
    { 0.16f, 0.29f, 0.48f, 1.f }
};

static inline const char* notification_prefix[] = { { "" },
                                                    { "Success " },
                                                    { "Warning " },
                                                    { "Error " },
                                                    { "Information " } };

notification::notification(notification_type type_) noexcept
  : type(type_)
  , creation_time(get_tick_count_in_milliseconds())
{}

static u64 get_elapsed_time(const notification& n) noexcept
{
    return get_tick_count_in_milliseconds() - n.creation_time;
}

static notification_state get_state(const notification& n) noexcept
{
    const auto elapsed = get_elapsed_time(n);

    if (elapsed > notification_fade_duration + notification_duration +
                    notification_fade_duration)
        return notification_state::expired;

    if (elapsed > notification_fade_duration + notification_duration)
        return notification_state::fadeout;

    if (elapsed > notification_fade_duration)
        return notification_state::wait;

    return notification_state::fadein;
}

float get_fade_percent(const notification& n) noexcept
{
    const auto state   = get_state(n);
    const auto elapsed = get_elapsed_time(n);

    switch (state) {
    case notification_state::fadein:
        return static_cast<float>(elapsed) / notification_fade_duration;

    case notification_state::wait:
        return 1.f;

    case notification_state::fadeout:
        return 1.f -
               ((static_cast<float>(elapsed) - notification_fade_duration -
                 static_cast<float>(notification_duration)) /
                notification_fade_duration);

    case notification_state::expired:
        return 1.f;
    }

    irt_unreachable();
}

notification& application::push_notification(notification_type type) noexcept
{
    if (notification_buffer.full()) {
        auto id = notification_buffer.dequeue();
        notifications.free(id);
    }

    auto& notif = notifications.alloc(type);
    auto  id    = notifications.get_id(notif);
    notification_buffer.emplace_enqueue(id);

    return notif;
}

template<i32 Size>
static void render_notifications(
  data_array<notification, notification_id>& data,
  ring_buffer<notification_id, Size>&        array) noexcept
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);

    const auto vp_size = ImGui::GetMainViewport()->Size;
    auto       height  = 0.f;

    int i = 0;
    for (auto it = array.begin(), et = array.end(); it != et; ++it, ++i) {
        auto* notif = data.try_to_get(*it);
        if (!notif) {
            *it = undefined<notification_id>();
            continue;
        }

        if (get_state(*notif) == notification_state::expired) {
            data.free(*it);
            *it = undefined<notification_id>();
            continue;
        }

        const auto opacity    = get_fade_percent(*notif);
        auto       text_color = notification_text_color[ordinal(notif->type)];
        text_color.w          = opacity;

        ImGui::SetNextWindowBgAlpha(opacity);
        ImGui::SetNextWindowPos(
          ImVec2(vp_size.x - notification_x_padding,
                 vp_size.y - notification_y_padding - height),
          ImGuiCond_Always,
          ImVec2(1.0f, 1.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              notification_color[ordinal(notif->type)]);
        small_string<16> name;
        format(name, "##{}toast", i);
        ImGui::Begin(name.c_str(), nullptr, notification_flags);
        ImGui::PopStyleColor(1);

        ImGui::PushTextWrapPos(vp_size.x / 3.f);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::TextUnformatted(notification_prefix[ordinal(notif->type)]);
        ImGui::SameLine();
        ImGui::Text(notif->title.c_str());
        ImGui::PopStyleColor();

        if (!notif->message.empty()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
            ImGui::Separator();
            ImGui::Text(notif->message.c_str());
        }

        ImGui::PopTextWrapPos();
        height += ImGui::GetWindowHeight() + notification_y_message_padding;

        ImGui::End();
    }

    while (!array.empty() && array.front() == undefined<notification_id>())
        array.dequeue();

    ImGui::PopStyleVar();
}

} // namespace irt
