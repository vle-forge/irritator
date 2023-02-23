// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
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

//! A helper function to get a pointer to the parent container from a member.
//! @code
//! struct point { float x; float y; };
//! struct line { point p1, p2; };
//! ...
//! line l;
//! ..
//!
//! void fn(point& p) {
//!     line *ptr = container_of(&p, &line::p1);
//!     ...
//! }
//! @endcode
template<class T, class M>
constexpr T* container_of(M* ptr, const M T::*member)
{
    return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                                offset_of(member));
}

class window_logger;
struct application;
struct component_editor;
struct simulation_editor;

enum class notification_id : u64;
enum class simulation_observation_id : u64;
enum class simulation_observation_copy_id : u64;
enum class simulation_task_id : u64;
enum class gui_task_id : u64;

enum class task_status
{
    not_started,
    started,
    finished
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

enum class simulation_plot_type
{
    none,
    plotlines,
    plotscatters
};

void show_menu_external_sources(external_source& srcs,
                                const char*      title,
                                source&          src) noexcept;

struct notification
{
    static inline constexpr int title_length   = 127;
    static inline constexpr int message_length = 510;

    using title_t   = small_string<title_length>;
    using message_t = small_string<message_length>;

    notification() noexcept;
    notification(log_level level_) noexcept;

    title_t   title;
    message_t message;
    u64       creation_time;
    log_level level;

    //! Is @c only_log is true, the notification is displayed only by the @c
    //! window_logger. If @c only_log boolean is
    //! false (default value), the notification is displayed both in the @c
    //! window_logger and in the @c notification_manager.
    bool only_log = false;
};

//! @brief Show notification into small auto destructible windows in bottom
//!  right/right.
//!
//! @details @c notification_manager is thread-safe to permit reporting of @c
//! notification from gui-task. All function are thread-safe. The caller is
//! blocked until a task releases the lock.
class notification_manager
{
public:
    static inline constexpr i32 notification_number   = 10;
    static inline constexpr u32 notification_duration = 3000;

    notification_manager() noexcept;

    notification& alloc() noexcept;
    notification& alloc(log_level level) noexcept;

    void enable(const notification& n) noexcept;
    void show() noexcept;

private:
    data_array<notification, notification_id> data;
    ring_buffer<notification_id>              r_buffer;
    std::mutex                                mutex;
};

//! @brief Show notification into a classical window in botton.
//!
//! @details It uses the same @c notification structure and copy it into a
//!  large ring-buffer of strings.
class window_logger
{
public:
    static inline constexpr int string_length      = 510;
    static inline constexpr int ring_buffer_length = 64;

    using string_t = small_string<string_length>;
    using ring_t   = ring_buffer<string_t>;

    window_logger() noexcept;

    void      clear() noexcept;
    string_t& enqueue() noexcept;
    void      show() noexcept;

    ring_t entries;

private:
    bool auto_scroll      = true;
    bool scroll_to_bottom = false;
};

struct raw_observation
{
    raw_observation() noexcept = default;
    raw_observation(const observation_message& msg_, const real t_) noexcept;

    observation_message msg;
    real                t;
};

struct simulation_observation
{
    using value_type = irt::real;

    model_id         model  = undefined<model_id>();
    interpolate_type i_type = interpolate_type::none;

    irt::small_vector<double, 2> output_vec;

    u64               plot_id;
    small_string<16u> name;

    ring_buffer<ImPlotPoint> linear_outputs;
    ImPlotRange limits; //! use in preview output simulation observation.

    std::filesystem::path file;

    real min_time_step = to_real(0.0001L);
    real max_time_step = to_real(1.L);
    real time_step     = to_real(0.01L);

    simulation_plot_type plot_type = simulation_plot_type::none;

    simulation_observation(model_id mdl, i32 buffer_capacity) noexcept;

    void clear() noexcept;

    void update(observer& obs) noexcept;
    void flush(observer& obs) noexcept;
    void push_back(real r) noexcept;

    void write(const std::filesystem::path& file_path) noexcept;
};

// Callback function use into ImPlot::Plot like functions that use ring_buffer
// to read a large buffer instead of a vector.
inline ImPlotPoint ring_buffer_getter(void* data, int idx) noexcept
{
    auto* ring  = reinterpret_cast<ring_buffer<ImPlotPoint>*>(data);
    auto  index = ring->index_from_begin(idx);

    return ImPlotPoint{ (*ring)[index].x, (*ring)[index].y };
};

struct simulation_observation_copy
{
    small_string<16u>        name;
    ring_buffer<ImPlotPoint> linear_outputs;
    simulation_plot_type     plot_type = simulation_plot_type::none;
};

void task_save_component(void* param) noexcept;
void task_save_description(void* param) noexcept;
void task_load_project(void* param) noexcept;
void task_save_project(void* param) noexcept;
void task_simulation_model_add(void* param) noexcept;
void task_simulation_model_del(void* param) noexcept;
void task_simulation_back(void* param) noexcept;
void task_simulation_advance(void* param) noexcept;
void task_build_observation_output(void* param) noexcept;
void task_remove_simulation_observation(application& app, model_id id) noexcept;
void task_add_simulation_observation(application& app, model_id id) noexcept;

struct gui_task
{
    u64          param_1 = 0;
    u64          param_2 = 0;
    void*        param_3 = nullptr;
    application* app     = nullptr;
    task_status  state   = task_status::not_started;
};

struct simulation_task
{
    u64          param_1 = 0;
    u64          param_2 = 0;
    u64          param_3 = 0;
    application* app     = nullptr;
    task_status  state   = task_status::not_started;
};

struct output_editor
{
    output_editor() noexcept;
    ~output_editor() noexcept;

    void show() noexcept;

    ImPlotContext* implot_context = nullptr;

    bool write_output = false;
};

//! An ImNodes editor for HSM.
//! Default, node 0 is the top state.
class hsm_editor
{
public:
    constexpr static inline auto max_number_of_state =
      hierarchical_state_machine::max_number_of_state;

    enum class state
    {
        show,
        ok,
        cancel,
        hide
    };

    enum class test_status
    {
        none,
        being_processed,
        done,
        failed
    };

    hsm_editor() noexcept;
    ~hsm_editor() noexcept;

    //! This function clear the current hierarchical state machine: suppress all
    //! states, transitions and actions. Assign the state-0 as initial state.
    void clear() noexcept;

    void load(component_id c_id, model_id m_id) noexcept;
    void load(model_id m_id) noexcept;
    void save() noexcept;

    bool show(const char* title) noexcept;
    bool valid() noexcept;

    bool state_ok() const noexcept { return m_state == state::ok; }
    void hide() noexcept { m_state = state::hide; }

private:
    void show_hsm() noexcept;
    void show_menu() noexcept;
    void show_graph() noexcept;
    void show_panel() noexcept;

    ImNodesEditorContext* m_context = nullptr;

    hierarchical_state_machine m_hsm;
    component_id               m_compo_id;
    model_id                   m_model_id;

    ImVector<int>                                  m_selected_links;
    ImVector<int>                                  m_selected_nodes;
    ImVector<hierarchical_state_machine::state_id> m_stack;

    std::array<ImVec2, max_number_of_state>        m_position;
    std::array<bool, max_number_of_state>          m_enabled;
    thread_safe_ring_buffer<small_string<127>, 10> m_messages;

    test_status m_test  = test_status::none;
    state       m_state = state::hide;
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
    void add_simulation_observation_for(std::string_view name,
                                        model_id         id) noexcept;

    // Used to add GUI task @c task_build_observation_output, one per
    // observer_id, from the simulation immediate observers.
    void build_observation_output() noexcept;

    bool can_edit() const noexcept;

    bool force_pause           = false;
    bool force_stop            = false;
    bool show_minimap          = true;
    bool allow_user_changes    = true;
    bool store_all_changes     = false;
    bool infinity_simulation   = false;
    bool real_time             = false;
    bool have_use_back_advance = false;

    bool show_internal_values = false;
    bool show_internal_inputs = false;
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

    modeling_to_simulation                                        cache;
    data_array<simulation_tree_node, simulation_tree_node_id>     tree_nodes;
    data_array<simulation_observation, simulation_observation_id> sim_obs;
    data_array<simulation_observation_copy, simulation_observation_copy_id>
      copy_obs;

    simulation_observation_id selected_sim_obs;
    double                    preview_history   = 10.;
    bool                      preview_scrolling = true;

    output_editor output_ed;

    ImNodesEditorContext* context        = nullptr;
    ImPlotContext*        output_context = nullptr;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    //! Position of each node
    int              automatic_layout_iteration = 0;
    ImVector<ImVec2> displacements;

    struct model_to_move
    {
        model_id id;
        ImVec2   position;
    };

    thread_safe_ring_buffer<model_to_move, 8> models_to_move;
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

struct data_window
{
    data_window() noexcept;
    ~data_window() noexcept;

    void show() noexcept;

    ImVector<ImVec2> plot;

    ImPlotContext* context = nullptr;

    irt::constant_source*    constant_ptr      = nullptr;
    irt::binary_file_source* binary_file_ptr   = nullptr;
    irt::text_file_source*   text_file_ptr     = nullptr;
    irt::random_source*      random_source_ptr = nullptr;

    bool show_file_dialog = false;
    bool plot_available   = false;
};

struct component_editor
{
    component_editor() noexcept;
    ~component_editor() noexcept;

    modeling    mod;
    data_window data;

    tree_node_id          selected_component      = undefined<tree_node_id>();
    ImNodesEditorContext* context                 = nullptr;
    bool                  is_saved                = true;
    bool                  show_minimap            = true;
    bool                  force_node_position     = false;
    bool                  show_input_output       = true;
    bool                  first_show_input_output = true;
    bool                  fix_input_output        = false;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    component_id add_empty_component() noexcept;
    void         select(tree_node_id id) noexcept;
    void         open_as_main(component_id id) noexcept;
    void         unselect() noexcept;

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

    int   style_selector                   = 0;
    int   automatic_layout_iteration_limit = 200;
    float automatic_layout_x_distance      = 350.f;
    float automatic_layout_y_distance      = 350.f;
    float grid_layout_x_distance           = 240.f;
    float grid_layout_y_distance           = 200.f;

    bool show_dynamics_inputs_in_editor = false;

    //! @brief Compute selected and hovered colours from gui_model_color and
    //! gui_component_color.
    void update() noexcept;

    void show(bool* is_open) noexcept;
};

struct application
{
    application() noexcept;
    ~application() noexcept;

    component_editor            c_editor;
    simulation_editor           s_editor;
    hsm_editor                  h_editor;
    settings_manager            settings;
    project_hierarchy_selection project_selection;

    registred_path_id select_dir_path = undefined<registred_path_id>();

    window_logger log_window;

private:
    data_array<simulation_task, simulation_task_id> sim_tasks;
    data_array<gui_task, gui_task_id>               gui_tasks;
    task_manager                                    task_mgr;

public:
    std::filesystem::path project_file;
    std::filesystem::path select_directory;

    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;

    modeling_initializer mod_init;

    notification_manager notifications;

    file_dialog f_dialog;

#ifdef IRRITATOR_USE_TTF
    ImFont* ttf = nullptr;
#endif

    bool show_select_directory_dialog = false;

    bool show_imgui_demo  = false;
    bool show_implot_demo = false;
    bool show_settings    = false;
    bool show_memory      = false;

    bool show_project      = true;
    bool show_log          = true;
    bool show_data         = true;
    bool show_output       = true;
    bool show_simulation   = true;
    bool show_modeling     = true;
    bool show_hsm_editor   = false;
    bool show_observation  = true;
    bool show_components   = true;
    bool show_tasks_window = false;

    bool new_project_file     = false; // rename menu_*
    bool load_project_file    = false; // rename menu_*
    bool save_project_file    = false; // rename menu_*
    bool save_as_project_file = false; // rename menu_*
    bool menu_quit            = false;

    bool init() noexcept;
    void show() noexcept;

    void show_tasks_widgets() noexcept;
    void show_external_sources() noexcept;
    void show_log_window() noexcept;
    void show_simulation_observation_window() noexcept;
    void show_components_window() noexcept;
    void show_project_window() noexcept;
    void show_memory_box(bool* is_open) noexcept;

    void show_modeling_editor_widget() noexcept;
    void show_simulation_editor_widget() noexcept;
    void show_output_editor_widget() noexcept;

    status save_settings() noexcept;
    status load_settings() noexcept;

    //! Helpers function to add a @c simulation_task into the @c
    //! main_task_lists[simulation]. Task is added at tail of the @c
    //! ring_buffer and ensure linear operation.
    void add_simulation_task(task_function fn,
                             u64           param_1 = 0,
                             u64           param_2 = 0,
                             u64           param_3 = 0) noexcept;

    //! Helpers function to add a @c simulation_task into the @c
    //! main_task_lists[gui]. Task is added at tail of the @c ring_buffer and
    //! ensure linear operation.
    void add_gui_task(task_function fn,
                      u64           param_1 = 0,
                      u64           param_2 = 0,
                      void*         param_3 = 0) noexcept;

    //! Helpers function to get a @c unordered_task_list. Wait until the  task
    //! list is available.
    unordered_task_list& get_unordered_task_list(int idx) noexcept;
};

//! @brief Get the file path of the @c imgui.ini file saved in $HOME.
//! @return A pointer to a newly allocated memory.
char* get_imgui_filename() noexcept;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

inline raw_observation::raw_observation(const observation_message& msg_,
                                        const real                 t_) noexcept
  : msg(msg_)
  , t(t_)
{
}

inline bool simulation_editor::can_edit() const noexcept
{
    if (match(simulation_state,
              simulation_status::not_started,
              simulation_status::finished))
        return true;

    return allow_user_changes;
}

} // namespace irt

#endif
