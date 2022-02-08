// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/modeling.hpp>
#include <irritator/thread.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <variant>
#include <vector>

#include "dialog.hpp"

#include "imnodes.h"
#include "implot.h"
#include <imgui.h>

namespace irt {

template<class T, class M>
constexpr std::ptrdiff_t offset_of(const M T::*member)
{
    return reinterpret_cast<std::ptrdiff_t>(
      &(reinterpret_cast<T*>(0)->*member));
}

template<class T, class M>
constexpr T* container_of(M* ptr, const M T::*member)
{
    return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                                offset_of(member));
}

static inline constexpr int not_found = -1;

struct application;
struct window_logger;
struct component_editor;
struct simulation_editor;

enum class notification_type
{
    none,
    success,
    warning,
    error,
    information
};

enum class log_status : int
{
    emergency,
    alert,
    critical,
    error,
    warning,
    notice,
    info,
    debug
};

void show_menu_external_sources(external_source& srcs,
                                const char*      title,
                                source&          src) noexcept;

struct notification
{
    notification(notification_type type_) noexcept;

    small_string<128>  title;
    small_string<1022> message;
    notification_type  type;
    u64                creation_time;
};

enum class notification_id : u64;

//! @brief Show notification windows in bottom right.
//!
//! @c notification_manager is thread-safe to permit reporting of @c
//! notification from gui-task. All function are thread-safe. The caller is
//! blocked until a task releases the lock.
class notification_manager
{
public:
    static inline constexpr i32 notification_number   = 10;
    static inline constexpr u32 notification_duration = 3000;

    notification_manager() noexcept;

    notification& alloc(
      notification_type type = notification_type::none) noexcept;

    void enable(const notification& n) noexcept;
    void show() noexcept;

private:
    data_array<notification, notification_id>         data;
    ring_buffer<notification_id, notification_number> buffer;
    std::mutex                                        mutex;
};

struct window_logger
{
    ImGuiTextBuffer buffer;
    ImGuiTextFilter filter;
    ImVector<int>   line_offsets;

    bool auto_scroll      = true;
    bool scroll_to_bottom = false;
    window_logger()       = default;
    void clear() noexcept;

    void log(const int level, const char* fmt, ...) IM_FMTARGS(3);
    void log(const int level, const char* fmt, va_list args) IM_FMTLIST(3);
    void show() noexcept;
};

const char* log_string(const log_status s) noexcept;

using application_status = u32;

enum class memory_output_id : u64;

struct memory_output
{
    ImVector<real>    xs;
    ImVector<real>    ys;
    small_string<24u> name;
    real              time_step   = one / to_real(10);
    bool              interpolate = true;
};

//! @brief Callback function used into simulation kernel
//! @param obs
//! @param type
//! @param tl
//! @param t
//! @param s
//! @return
void memory_output_update(const irt::observer&        obs,
                          const irt::dynamics_type    type,
                          const irt::time             tl,
                          const irt::time             t,
                          const irt::observer::status s) noexcept;

enum class simulation_status
{
    not_started,
    initializing,
    initialized,
    run_requiring,
    running,
    paused,
    pause_forced,
    finish_requiring,
    finishing,
    finished,
};

enum application_status_
{
    application_status_modeling             = 0,
    application_status_simulating           = 1 << 1,
    application_status_read_only_modeling   = 1 << 2,
    application_status_read_only_simulating = 1 << 3,
};

enum class gui_task_status
{
    not_started,
    started,
    finished
};

void save_component(void* param) noexcept;
void save_description(void* param) noexcept;
void load_project(void* param) noexcept;
void save_project(void* param) noexcept;

enum class gui_task_id : u64;

struct gui_task
{
    u64                param_1      = 0;
    u64                param_2      = 0;
    void*              param_3      = nullptr;
    application*       app          = nullptr;
    application_status editor_state = 0;
    gui_task_status    state        = gui_task_status::not_started;
};

struct simulation_editor
{
    enum class visualization_mode
    {
        flat,
        tree
    };

    simulation_editor() noexcept;
    ~simulation_editor() noexcept;
    void shutdown() noexcept;

    void select(simulation_tree_node_id id) noexcept;
    void unselect() noexcept;
    void clear() noexcept;

    void simulation_update_state() noexcept;

    void simulation_copy_modeling() noexcept;
    void simulation_init() noexcept;
    void simulation_clear() noexcept;
    void simulation_start() noexcept;
    void simulation_pause() noexcept;
    void simulation_stop() noexcept;

    bool force_pause = false;
    bool force_stop  = false;

    bool show_minimap = true;

    simulation sim;

    real simulation_begin   = 0;
    real simulation_end     = 100;
    real simulation_current = 0;

    simulation_tree_node_id head    = undefined<simulation_tree_node_id>();
    simulation_tree_node_id current = undefined<simulation_tree_node_id>();
    visualization_mode      mode    = visualization_mode::flat;

    simulation_status simulation_state = simulation_status::not_started;

    data_array<simulation_tree_node, simulation_tree_node_id> tree_nodes;

    ImNodesEditorContext* context = nullptr;
    ImVector<int>         selected_links;
    ImVector<int>         selected_nodes;
};

struct component_editor
{
    modeling                                    mod;
    external_source                             srcs;
    data_array<memory_output, memory_output_id> outputs;
    tree_node_id          selected_component = undefined<tree_node_id>();
    ImNodesEditorContext* context            = nullptr;
    bool                  is_saved           = true;
    bool                  show_minimap       = true;

    component* selected_component_list = nullptr;
    bool       force_node_position     = false;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    void select(tree_node_id id) noexcept;
    void new_project() noexcept;
    void open_as_main(component_id id) noexcept;
    void unselect() noexcept;

    void save_project(const char* filename) noexcept;
    void load_project(const char* filename) noexcept;

    void init() noexcept;
    void show(bool* is_show) noexcept;
    void shutdown() noexcept;
};

struct application
{
    struct settings_manager
    {
        ImVec4 gui_model_color{ .27f, .27f, .54f, 1.f };
        ImVec4 gui_component_color{ .54f, .27f, .27f, 1.f };
        ImU32  gui_hovered_model_color;
        ImU32  gui_selected_model_color;
        ImU32  gui_hovered_component_color;
        ImU32  gui_selected_component_color;

        //! @brief Compute selected and hovered colors from gui_model_color and
        //! gui_component_color.
        void update() noexcept;

        int   automatic_layout_iteration_limit = 200;
        float automatic_layout_x_distance      = 350.f;
        float automatic_layout_y_distance      = 350.f;
        float grid_layout_x_distance           = 240.f;
        float grid_layout_y_distance           = 200.f;

        bool show_dynamics_inputs_in_editor = false;

        void show(bool* is_open) noexcept;
    };

    application() noexcept;
    ~application() noexcept;

    component_editor  c_editor;
    simulation_editor s_editor;
    settings_manager  settings;

    bool show_select_directory_dialog = false;

    registred_path_id select_dir_path = undefined<registred_path_id>();

    data_array<gui_task, gui_task_id> gui_tasks;
    task_manager                      task_mgr;

    std::filesystem::path project_file;
    std::filesystem::path select_directory;

    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;

    modeling_initializer mod_init;

    notification_manager notifications;
    application_status   state = application_status_modeling;

    file_dialog f_dialog;

    bool is_fixed_window_placement = true;
    bool is_fixed_main_window      = true;

    bool show_modeling   = true;
    bool show_simulation = false;
    bool show_demo       = false;
    bool show_plot       = false;
    bool show_settings   = false;
    bool show_memory     = false;

    bool show_output_editor     = true;
    bool show_simulation_editor = true;
    bool show_modeling_editor   = true;

    bool new_project_file     = false; // rename menu_*
    bool load_project_file    = false; // rename menu_*
    bool save_project_file    = false; // rename menu_*
    bool save_as_project_file = false; // rename menu_*
    bool menu_quit            = false;

    bool init() noexcept;
    void show() noexcept;
    void shutdown() noexcept;

    void show_external_sources() noexcept;
    void show_simulation_window() noexcept;
    void show_components_window() noexcept;
    void show_project_window() noexcept;
    void show_main_as_tabbar(ImVec2           position,
                             ImVec2           size,
                             ImGuiWindowFlags window_flags,
                             ImGuiCond        position_flags,
                             ImGuiCond        size_flags) noexcept;
    void show_main_as_window(ImVec2 position, ImVec2 size) noexcept;
    void show_memory_box(bool* is_open) noexcept;

    void show_modeling_editor_widget() noexcept;
    void show_simulation_editor_widget() noexcept;
    void show_output_editor_widget() noexcept;

    status save_settings() noexcept;
    status load_settings() noexcept;

    window_logger log_w;
};

char* get_imgui_filename() noexcept;

} // namespace irt

#endif
