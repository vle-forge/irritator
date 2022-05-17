// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/modeling.hpp>
#include <irritator/thread.hpp>
#include <irritator/timeline.hpp>

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

enum class notification_id : u64;
enum class simulation_observation_id : u64;
enum class simulation_plot_id : u64;
enum class gui_task_id : u64;

using application_status   = u32;
using selected_main_window = i32;
using simulation_plot_type = i32;

enum class notification_type
{
    none,
    success,
    warning,
    error,
    information
};

enum class gui_task_status
{
    not_started,
    started,
    finished
};

enum class log_status
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
    debugged,
};

enum application_status_
{
    application_status_modeling             = 0,
    application_status_simulating           = 1 << 1,
    application_status_read_only_modeling   = 1 << 2,
    application_status_read_only_simulating = 1 << 3,
};

enum selected_main_window_
{
    selected_main_window_none       = 0,      //! All windows are hidden
    selected_main_window_modeling   = 1 << 1, //! Only modeling window
    selected_main_window_simulation = 1 << 2, //! Only simulation window
    selected_main_window_output     = 1 << 3, //! Only output window
    selected_main_window_all =
      selected_main_window_modeling | selected_main_window_simulation |
      selected_main_window_output //! All windows are displayed
};

enum simulation_plot_
{
    simulation_plot_type_none,
    simulation_plot_type_raw,
    simulation_plot_type_plotlines,
    simulation_plot_type_plotscatters
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
    data_array<notification, notification_id>        data;
    std::array<notification_id, notification_number> buffer;
    ring_buffer<notification_id>                     r_buffer;
    std::mutex                                       mutex;
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

struct simulation_observation
{
    struct raw_observation
    {
        raw_observation() noexcept = default;
        raw_observation(const observation_message& msg_,
                        const real                 t_) noexcept;

        observation_message msg;
        real                t;
    };

    model_id                     model = undefined<model_id>();
    dynamics_type                type  = dynamics_type::constant;
    u64                          plot_id;
    small_string<16u>            name;
    vector<raw_observation>      raw_outputs;
    vector<ImVec2>               linear_outputs;
    ImVector<ImVec2>             plot_outputs;
    ring_buffer<raw_observation> raw_ring_buffer;
    ring_buffer<ImVec2>          linear_ring_buffer;

    std::filesystem::path raw_file;
    std::filesystem::path linear_file;

    real min_time_step = to_real(0.0001);
    real max_time_step = to_real(1);
    real time_step     = to_real(0.01);

    simulation_plot_type plot_type = simulation_plot_type_none;

    simulation_observation(model_id      mdl,
                           dynamics_type type,
                           i32           default_raw_length,
                           i32           default_linear_length) noexcept;

    void clear() noexcept;

    void save_raw(const std::filesystem::path& file_path) noexcept;
    void save_interpolate(const std::filesystem::path& file_path) noexcept;

    void compute_interpolate(const real           until,
                             ring_buffer<ImVec2>& out) noexcept;
    void compute_interpolate(const real until, ImVector<ImVec2>& out) noexcept;
};

void simulation_observation_update(const observer&        obs,
                                   const dynamics_type    type,
                                   const time             tl,
                                   const time             t,
                                   const observer::status s) noexcept;

struct simulation_plot
{
    small_string<16u> name;
};

void save_component(void* param) noexcept;
void save_description(void* param) noexcept;
void load_project(void* param) noexcept;
void save_project(void* param) noexcept;
void task_simulation_back(void* param) noexcept;
void task_simulation_advance(void* param) noexcept;

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

    //! 0.1s between each run thread task.
    static constexpr i64 thread_frame_duration = 100000;

    simulation_editor() noexcept;
    ~simulation_editor() noexcept;

    void select(simulation_tree_node_id id) noexcept;
    void unselect() noexcept;
    void clear() noexcept;

    void simulation_update_state() noexcept;

    void simulation_copy_modeling() noexcept;
    void simulation_init() noexcept;
    void simulation_clear() noexcept;
    void simulation_start() noexcept;
    void simulation_start_1() noexcept;
    void simulation_pause() noexcept;
    void simulation_stop() noexcept;
    void simulation_advance() noexcept;
    void simulation_back() noexcept;
    void enable_or_disable_debug() noexcept;

    void remove_simulation_observation_from(model_id id) noexcept;
    void add_simulation_observation_for(model_id id) noexcept;

    bool force_pause           = false;
    bool force_stop            = false;
    bool show_minimap          = true;
    bool allow_user_changes    = false;
    bool store_all_changes     = false;
    bool infinity_simulation   = false;
    bool real_time             = false;
    bool have_use_back_advance = false;

    bool show_internal_values = false;
    bool show_identifiers     = false;

    simulation sim;
    timeline   tl;

    real simulation_begin   = 0;
    real simulation_end     = 100;
    real simulation_current = 0;

    //! Number of microsecond to run 1 unit of simulation time
    //! Default 1 unit of simulation in 1 second.
    i64 simulation_real_time_relation = 1000000;

    simulation_tree_node_id head    = undefined<simulation_tree_node_id>();
    simulation_tree_node_id current = undefined<simulation_tree_node_id>();
    visualization_mode      mode    = visualization_mode::flat;

    simulation_status simulation_state = simulation_status::not_started;

    data_array<simulation_tree_node, simulation_tree_node_id>     tree_nodes;
    data_array<simulation_observation, simulation_observation_id> sim_obs;
    data_array<simulation_plot, simulation_plot_id>               sim_plots;

    simulation_observation_id selected_sim_obs;

    ImNodesEditorContext* context        = nullptr;
    ImPlotContext*        output_context = nullptr;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    //! Position of each node
    int              automatic_layout_iteration = 0;
    ImVector<ImVec2> displacements;
};

struct project_hierarchy_selection
{
    tree_node_id parent = undefined<tree_node_id>();
    component_id compo  = undefined<component_id>();
    child_id     ch     = undefined<child_id>();

    void set(tree_node_id parent, component_id compo) noexcept;
    void set(tree_node_id parent, component_id compo, child_id ch) noexcept;
    void clear() noexcept;

    bool equal(tree_node_id parent,
               component_id compo,
               child_id     ch) const noexcept;
};

struct component_editor
{
    component_editor() noexcept;
    ~component_editor() noexcept;

    modeling        mod;
    external_source srcs;
    // data_array<memory_output, memory_output_id> outputs;
    tree_node_id          selected_component  = undefined<tree_node_id>();
    ImNodesEditorContext* context             = nullptr;
    bool                  is_saved            = true;
    bool                  show_minimap        = true;
    bool                  force_node_position = false;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    void select(tree_node_id id) noexcept;
    void new_project() noexcept;
    void open_as_main(component_id id) noexcept;
    void unselect() noexcept;

    void save_project(const char* filename) noexcept;
    void load_project(const char* filename) noexcept;

    void show(bool* is_show) noexcept;
};

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

struct application
{
    application() noexcept;
    ~application() noexcept;

    component_editor            c_editor;
    simulation_editor           s_editor;
    settings_manager            settings;
    project_hierarchy_selection project_selection;

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
    bool save_raw_file        = false;
    bool save_int_file        = false;
    bool menu_quit            = false;

    bool init() noexcept;
    void show() noexcept;

    void                 show_external_sources() noexcept;
    void                 show_simulation_window() noexcept;
    void                 show_simulation_observation_window() noexcept;
    void                 show_components_window() noexcept;
    void                 show_project_window() noexcept;
    selected_main_window show_main_as_tabbar(ImVec2           position,
                                             ImVec2           size,
                                             ImGuiWindowFlags window_flags,
                                             ImGuiCond        position_flags,
                                             ImGuiCond size_flags) noexcept;
    selected_main_window show_main_as_window(ImVec2 position,
                                             ImVec2 size) noexcept;
    void                 show_memory_box(bool* is_open) noexcept;

    void show_modeling_editor_widget() noexcept;
    void show_simulation_editor_widget() noexcept;
    void show_output_editor_widget() noexcept;

    status save_settings() noexcept;
    status load_settings() noexcept;

    window_logger log_w;
};

void task_remove_simulation_observation(application& app, model_id id) noexcept;
void task_add_simulation_observation(application& app, model_id id) noexcept;

char* get_imgui_filename() noexcept;

inline simulation_observation::raw_observation::raw_observation(
  const observation_message& msg_,
  const real                 t_) noexcept
  : msg(msg_)
  , t(t_)
{}

} // namespace irt

#endif
