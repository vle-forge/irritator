// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/global.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/thread.hpp>
#include <irritator/timeline.hpp>

#include <filesystem>

#include "dialog.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imnodes.h"
#include "implot.h"
#include <imgui.h>

namespace irt {

struct application;
class component_editor;
struct project_editor;

enum class notification_id : u32;
enum class plot_copy_id : u32;

enum class simulation_task_id : u64;
enum class gui_task_id : u64;

enum class grid_editor_data_id : u32;
enum class graph_editor_data_id : u32;
enum class generic_editor_data_id : u32;
enum class hsm_editor_data_id : u32;

enum class project_id : u32;

enum class task_status { not_started, started, finished };

enum class simulation_status {
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

enum class simulation_plot_type { none, plotlines, plotscatters };

/** Display ComboBox to select a file (not `.irt` or `.txt` from the component
 * @c compo directory @c compo::dir.
 * @return True if the file path si selected.*/
auto show_data_file_input(const modeling&  mod,
                          const component& compo,
                          file_path_id&    id) noexcept -> bool;

/** Display combobox and input real, input integer for each type of random
 * distribution. The @c random_source @c src is updated according to user
 * selection.
 * @return True if the user change the distribution type or the parameters.
 */
bool show_random_distribution_input(random_source& src) noexcept;

void show_menu_external_sources(external_source& srcs,
                                const char*      title,
                                source&          src) noexcept;

/** Display two combox, one per line. The first to select the source type,
 * second to select the source identifier. */
void show_combobox_external_sources(external_source& srcs,
                                    source&          src) noexcept;

struct notification {
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

    template<typename Function>
    void try_insert(log_level level, Function&& fn) noexcept
    {
        std::unique_lock lock(m_mutex);

        if (m_data.can_alloc() and not m_enabled_ids.full()) {
            auto& d = m_data.alloc();
            d.level = level;
            lock.unlock();

            fn(d.title, d.message);

            enable(d);
        }
    }

private:
    data_array<notification, notification_id> m_data;
    ring_buffer<notification_id>              m_enabled_ids;

    spin_mutex m_mutex; /** @c alloc() and @c enable() functions lock the mutex
                           to fill @c data and @c r_buffer. The show function
                           only try to lock the mutex to display data. */
};

//! @brief Show notification into a classical window in botton.
//!
//! @details It uses the same @c notification structure and copy it into a
//!  large ring-buffer of strings.
class window_logger
{
public:
    constexpr static inline const char* name = "Log";

    static inline constexpr int string_length      = 510;
    static inline constexpr int ring_buffer_length = 64;

    using string_t = small_string<string_length>;
    using ring_t   = ring_buffer<string_t>;

    window_logger() noexcept;

    void      clear() noexcept;
    string_t& enqueue() noexcept;
    void      show() noexcept;

    ring_t entries;
    bool   is_open = true;

private:
    bool auto_scroll      = true;
    bool scroll_to_bottom = false;
};

class plot_observation_widget
{
public:
    plot_observation_widget() noexcept = default;

    /** Display all observers data using an ImPlot::PlotLineG or
     * ImPlot::PlotScatterG for continuous data or ImPlot::PlotStairsG or
     * ImPlot::PlotBarG for discrete data into and ImPlot::BeginPlot() /
     * ImPlot::EndPlot().
     */
    void show(project& app) noexcept;

    /** Display the observer data using an ImPlot::PlotLineG or
     * ImPlot::PlotScatterG for continuous data or ImPlot::PlotStairsG or
     * ImPlot::PlotBarG for discrete data.
     *
     * @attention Use this function after the call to ImPlot::BeginPlot() and
     * before the call to ImPlot::EndPlot().
     */
    void show_plot_line(const observer&                       obs,
                        const variable_observer::type_options options,
                        const name_str&                       name) noexcept;
};

/// Use to display a grid_observation_system into ImGui widget. An instance of
/// this class is available in @c application::simulation_editor::grid_obs.
struct grid_observation_widget {
    /// Display the @c grid_observation_system into ImPlot::PlotHeatmap plot.
    void show(grid_observer& grid, const ImVec2& size) noexcept;
};

class graph_observation_widget
{
public:
    //! Display the values vector using the ImGui::PlotHeatMap function.
    // void show(graph_observation_system& graph) noexcept;
};

// Callback function use into ImPlot::Plot like functions that use ring_buffer
// to read a large buffer instead of a vector.
inline ImPlotPoint ring_buffer_getter(int idx, void* data)
{
    auto* ring  = reinterpret_cast<ring_buffer<observation>*>(data);
    auto  index = ring->index_from_begin(idx);

    return ImPlotPoint{ (*ring)[index].x, (*ring)[index].y };
};

struct plot_copy {
    small_string<16u>        name;
    ring_buffer<observation> linear_outputs;
    simulation_plot_type     plot_type = simulation_plot_type::none;
};

struct plot_copy_widget {
    /** Display all plot_copy data into a ImPlot widget. */
    void show(const char* name) noexcept;

    /** Display a @c plot_copy using @c ImPlot::PlogLineG or @c
     * ImPlot::PlotScatterG but without @c ImPlot::BeginPlot and @c
     * ImPlot::EndPlot */
    void show_plot_line(const plot_copy& id) noexcept;
};

class output_editor
{
public:
    constexpr static inline const char* name = "Output";

    output_editor() noexcept;
    ~output_editor() noexcept;

    void show() noexcept;

    bool is_open = true;

    void save_obs(const project_id                pj_id,
                  const variable_observer_id      vobs,
                  const variable_observer::sub_id svobs) noexcept;
    void save_copy(const plot_copy_id id) noexcept;

private:
    ImPlotContext* m_ctx = nullptr;

    std::filesystem::path m_file;

    plot_copy_id              m_copy_id = undefined<plot_copy_id>();
    project_id                m_pj_id   = undefined<project_id>();
    variable_observer_id      m_vobs_id = undefined<variable_observer_id>();
    variable_observer::sub_id m_sub_id = undefined<variable_observer::sub_id>();

    enum class save_option { none, copy, obs } m_need_save = save_option::none;
};

class grid_component_editor_data
{
public:
    grid_component_editor_data(const component_id      id,
                               const grid_component_id grid) noexcept;

    void clear() noexcept;

    void show(component_editor& ed) noexcept;
    void show_selected_nodes(component_editor& ed) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    ImVec2     distance{ 5.f, 5.f };
    ImVec2     size{ 30.f, 30.f };
    ImVec2     scrolling{ 0.f, 0.f };
    float      zoom[2]{ 1.f, 1.f };
    component* hovered_component = nullptr;
    int        row               = 0;
    int        col               = 0;

    component_id selected_id = undefined<component_id>();

    grid_component_id grid_id = undefined<grid_component_id>();

private:
    component_id m_id = undefined<component_id>();
};

class graph_component_editor_data
{
public:
    graph_component_editor_data(const component_id       id,
                                const graph_component_id graph) noexcept;

    void clear() noexcept;

    void show(component_editor& ed) noexcept;
    void show_selected_nodes(component_editor& ed) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

private:
    struct impl;

    vector<ImVec2>        displacements;
    vector<graph_node_id> selected_nodes;
    vector<graph_edge_id> selected_edges;

    ImVec2 distance{ 50.f, 25.f };
    ImVec2 size{ 30.f, 15.f };
    ImVec2 scrolling{ 0.f, 0.f };
    ImVec2 start_selection;
    ImVec2 end_selection;

    float zoom[2]{ 1.f, 1.f };
    int   iteration        = 0;
    int   iteration_limit  = 1000;
    bool  automatic_layout = false;
    bool  run_selection    = false;

    component_id       selected_id = undefined<component_id>();
    graph_component_id graph_id    = undefined<graph_component_id>();

    spin_mutex mutex;
    enum class status { none, update_required, updating } st = status::none;

    component_id m_id = undefined<component_id>();
};

class hsm_component_editor_data
{
public:
    constexpr static inline const char* name = "HSM Editor";

    constexpr static inline auto max_number_of_state =
      hierarchical_state_machine::max_number_of_state;

    hsm_component_editor_data(const component_id     id,
                              const hsm_component_id hid,
                              hsm_component&         hsm) noexcept;
    ~hsm_component_editor_data() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    void show(component_editor& ed) noexcept;
    void show_selected_nodes(component_editor& ed) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    /**
       Stores the @c hsm_component_editor_data hidden attributes into hsm
       component. For example the position of nodes (in ImNodes). This function
       is called before switching to another component or before saving the
       component.
     */
    void store(component_editor& ed) noexcept;

private:
    void show_hsm(hsm_component& hsm) noexcept;
    void show_menu(hsm_component& hsm) noexcept;
    void show_graph(hsm_component& hsm) noexcept;
    void show_panel(application& app, hsm_component& hsm) noexcept;
    void clear(hsm_component& hsm) noexcept;
    bool valid(hsm_component& hsm) noexcept;

    ImNodesEditorContext* m_context = nullptr;

    ImVector<int> m_selected_links;
    ImVector<int> m_selected_nodes;

    ImVector<hierarchical_state_machine::state_id> m_stack;

    std::array<bool, max_number_of_state> m_enabled;

    enum {
        display_action_label,
        display_condition_label,
    };

    std::bitset<2> m_options;

private:
    component_id     m_id     = undefined<component_id>();
    hsm_component_id m_hsm_id = undefined<hsm_component_id>();
};

class generic_component_editor_data
{
public:
    generic_component_editor_data(const component_id         id,
                                  component&                 compo,
                                  const generic_component_id gid,
                                  generic_component&         gen) noexcept;
    ~generic_component_editor_data() noexcept;

    void show(component_editor& ed) noexcept;
    void show_selected_nodes(component_editor& ed) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    /** Stores the @c generic_component_editor_data hidden attributes into
     * generic_component. For example the position of nodes (in ImNodes). This
     * function is called before switching to another component or before saving
     * the component. */
    void store(component_editor& ed) noexcept;

    //! Before running any ImNodes functions, pre-move all children to force
    //! position for all new children.
    // void update_position() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    ImNodesEditorContext* context = nullptr;

public:
    enum {
        show_minimap,
        show_input_output,
        first_show_input_output,
        fix_input_output,
        force_update_position,
    };

    std::bitset<6> options;

    ImVector<int>    selected_links;
    ImVector<int>    selected_nodes;
    vector<child_id> update_position_list;

private:
    component_id m_id = undefined<component_id>();
};

struct generic_simulation_editor {

    ImNodesEditorContext* context = nullptr;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    bool show_observations(tree_node&         tn,
                           component&         compo,
                           generic_component& generic) noexcept;
};

class grid_simulation_editor
{
public:
    /** Restore the variable to the default value. */
    void reset() noexcept;

    bool display(application&    app,
                 project_editor& ed,
                 tree_node&      tn,
                 component&      compo,
                 grid_component& grid) noexcept;

    grid_component_id current_id = undefined<grid_component_id>();

    ImVec2 zoom{ 1.f, 1.f };
    ImVec2 scrolling{ 1.f, 1.f };
    ImVec2 size{ 30.f, 30.f };
    ImVec2 distance{ 5.f, 5.f };
};

struct graph_simulation_editor {
    ImVec2             show_position{ 0.f, 0.f };
    ImVec2             disp{ 1000.f, 1000.f };
    float              scale           = 10.f;
    bool               start_selection = false;
    graph_component_id current_id      = undefined<graph_component_id>();

    component_id selected_setting_component = undefined<component_id>();
    model_id     selected_setting_model     = undefined<model_id>();

    component_id selected_observation_component = undefined<component_id>();
    model_id     selected_observation_model     = undefined<model_id>();
    tree_node*   selected_tn                    = nullptr;

    vector<component_id> children_class;

    void clear() noexcept;

    bool show_settings(tree_node&       tn,
                       component&       compo,
                       graph_component& graph) noexcept;

    bool show_observations(tree_node&       tn,
                           component&       compo,
                           graph_component& graph) noexcept;
};

struct hsm_simulation_editor {
    bool show_observations(application&    app,
                           project_editor& ed,
                           tree_node&      tn,
                           component&      compo,
                           hsm_component&  hsm) noexcept;

    hsm_component_id current_id = undefined<hsm_component_id>();
};

class project_external_source_editor
{
public:
    enum class plot_status {
        empty,
        work_in_progress,
        data_available,
    };

    project_external_source_editor() noexcept;
    ~project_external_source_editor() noexcept;

    void show(application& app) noexcept;

    struct selection {
        void clear() noexcept;

        void select(constant_source_id id) noexcept;
        void select(text_file_source_id id) noexcept;
        void select(binary_file_source_id id) noexcept;
        void select(random_source_id id) noexcept;

        bool is(constant_source_id id) const noexcept;
        bool is(text_file_source_id id) const noexcept;
        bool is(binary_file_source_id id) const noexcept;
        bool is(random_source_id id) const noexcept;

        std::optional<source::source_type> type_sel;
        u64                                id_sel = 0;
    } sel;

    /** Fill the underlying @c plot vector with reals from the @c data
     * parameter. This operation is framed by the change of @c status:
     * - entering function, status is status::work_in_progress.
     * - exiting function, status is status::data_available.
     *
     * The @c mutex protect access to @c plot.
     */
    void fill_plot(std::span<double> data) noexcept;

private:
    vector<float>  plot;
    ImPlotContext* context          = nullptr;
    plot_status    m_status         = plot_status::empty;
    bool           show_file_dialog = false;

    spin_mutex mutex;
};

struct project_editor {
    enum class visualization_mode { flat, tree };

    project_editor(const std::string_view default_name) noexcept;
    ~project_editor() noexcept;

    project_editor(project_editor&&)            = default;
    project_editor& operator=(project_editor&&) = default;

    enum class show_result_t {
        success,          /**< Nothing to do. */
        request_to_close, /**< The window must be closed by the called. */
    };

    /** Display the project window in a @c ImGui::Begin/End window.
     *
     * @return project_editor next status enumeration: keep the window open or
     * try to close it. */
    show_result_t show(application& app) noexcept;

    void select(tree_node_id id) noexcept;
    void unselect() noexcept;
    void clear() noexcept;

    void start_simulation_update_state(application& app) noexcept;

    void start_simulation_copy_modeling(application& app) noexcept;
    void start_simulation_init(application& app) noexcept;
    void start_simulation_delete(application& app) noexcept;
    void start_simulation_clear(application& app) noexcept;
    void start_simulation_start(application& app) noexcept;
    void start_simulation_observation(application& app) noexcept;
    void stop_simulation_observation(application& app) noexcept;
    void start_simulation_live_run(application& app) noexcept;
    void start_simulation_static_run(application& app) noexcept;
    void start_simulation_start_1(application& app) noexcept;
    void start_simulation_pause(application& app) noexcept;
    void start_simulation_stop(application& app) noexcept;
    void start_simulation_finish(application& app) noexcept;
    void start_simulation_advance(application& app) noexcept;
    void start_simulation_back(application& app) noexcept;
    void start_enable_or_disable_debug(application& app) noexcept;

    void start_simulation_model_add(application&        app,
                                    const dynamics_type type,
                                    const float         x,
                                    const float         y) noexcept;
    void start_simulation_model_del(application&   app,
                                    const model_id id) noexcept;

    void remove_simulation_observation_from(application&   app,
                                            const model_id id) noexcept;
    void add_simulation_observation_for(application&   app,
                                        const model_id id) noexcept;

    bool can_edit() const noexcept;
    bool can_display_graph_editor() const noexcept;

    bool force_pause           = false;
    bool force_stop            = false;
    bool show_minimap          = true;
    bool allow_user_changes    = true;
    bool store_all_changes     = false;
    bool real_time             = false;
    bool have_use_back_advance = false;
    bool display_graph         = true;

    bool save_project_file    = false;
    bool save_as_project_file = false;

    bool show_internal_values = false;
    bool show_internal_inputs = false;
    bool show_identifiers     = false;

    bool is_dock_init   = false;
    bool disable_access = true;

    /** Return true if a simulation is currently running.
     *
     * A simulation is running if and only if the simulation status is in state:
     * running, running_required or paused.
     */
    bool is_simulation_running() const noexcept;

    timeline tl;
    name_str name;

    real simulation_last_finite_t   = 0;
    real simulation_display_current = 0;

    //! Number of microsecond to run 1 unit of simulation time. The default is
    //! to run 1 unit of simulation per second.
    i64 nb_microsecond_per_simulation_time = 1000000;

    //! The duration of a simulation run task. The default is to run one task in
    //! 100ms.
    static inline constexpr i64 thread_frame_duration = 100000;

    using time_point =
      std::chrono::time_point<std::chrono::high_resolution_clock>;

    std::chrono::milliseconds simulation_time_duration{ 1000 };
    std::chrono::milliseconds simulation_task_duration{ 10 };

    //! Use in live modeling to store the time-point of the start of the
    //! simulation and allow to compute the next wakeup of the simulation task.
    time_point start;

    //! Use in live modeling to store the time-point of the next wakeup of the
    //! simulation.
    time_point wakeup;

    tree_node_id head    = undefined<tree_node_id>();
    tree_node_id current = undefined<tree_node_id>();

    visualization_mode mode = visualization_mode::flat;

    simulation_status simulation_state = simulation_status::not_started;
    data_array<plot_copy, plot_copy_id> copy_obs;

    plot_observation_widget  plot_obs;
    grid_observation_widget  grid_obs;
    graph_observation_widget graph_obs;

    plot_copy_widget plot_copy_wgt;

    generic_simulation_editor generic_sim;
    grid_simulation_editor    grid_sim;
    graph_simulation_editor   graph_sim;
    hsm_simulation_editor     hsm_sim;

    ImNodesEditorContext* context        = nullptr;
    ImPlotContext*        output_context = nullptr;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    /** Number of column in the tree node observation. */
    using tree_node_observation_t = constrained_value<int, 1, 100>;
    tree_node_observation_t tree_node_observation{ 1 };
    float                   tree_node_observation_height = 200.f;

    //! Position of each node
    int              automatic_layout_iteration = 0;
    ImVector<ImVec2> displacements;

    /**
     * @brief A live modeling tool to force a `constant` model to produce an
     * internal event.
     */
    std::optional<model_id> have_send_message;

    small_ring_buffer<std::pair<model_id, ImVec2>, 8>
      models_to_move; /**< Online simulation created models need to use ImNodes
                         API to move into the canvas. */

    registred_path_id project_file = undefined<registred_path_id>();
    project           pj;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(const modeling& mod, tree_node_id id) noexcept;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(const modeling& mod, tree_node& node) noexcept;

    //! Select a @C child in the modeling tree node. The existence of the
    //! underlying component is tested before assignment.
    void select(const modeling& mod, child_id id) noexcept;

    //! @return true if @c id is the selected @c tree_node.
    bool is_selected(tree_node_id id) const noexcept;

    //! @return true if @c id is the selected @c child.
    bool is_selected(child_id id) const noexcept;

    tree_node_id selected_tn() noexcept;
    child_id     selected_child() noexcept;

    tree_node_id m_selected_tree_node = undefined<tree_node_id>();
    child_id     m_selected_child     = undefined<child_id>();

    project_external_source_editor data_ed;
};

inline bool project_editor::is_simulation_running() const noexcept
{
    return any_equal(simulation_state,
                     simulation_status::paused,
                     simulation_status::running,
                     simulation_status::run_requiring);
}

class component_editor
{
public:
    constexpr static inline const char* name = "Component editor";

    /** Draw the editor into a ImGui::Begin/ImGui::End window. */
    void display() noexcept;

    void request_to_open(const component_id id) noexcept;
    bool need_to_open(const component_id id) const noexcept;
    void clear_request_to_open() noexcept;

    /** Force to close the opened component @c id. */
    void close(const component_id id) noexcept;

    small_string<31> title;
    small_string<31> component_name;
    bool             is_open = true;

private:
    struct impl;

    enum { tabitem_open_save, tabitem_open_in_out };

    std::bitset<2> tabitem_open;
    component_id   m_request_to_open = undefined<component_id>();
};

class library_window
{
public:
    constexpr static inline const char* name = "Library";

    library_window() noexcept = default;

    void try_set_component_as_project(application&       app,
                                      const component_id id) noexcept;

    enum class is_component_deletable_t {
        deletable,
        used_by_component,
        used_by_project
    };

    /** Test if the component @c id is not used in any other component and any
     * other projects. */
    is_component_deletable_t is_component_deletable(
      const application& app,
      const component_id id) const noexcept;

    void show() noexcept;

    bool is_open = true;
};

/** An ImGui TabBAr to display project settings, project hierarchy, simulation
 * settings etc. */
class project_settings_editor
{
public:
    project_settings_editor() noexcept = default;

    void show(project_editor& ed) noexcept;

private:
};

class settings_window
{
public:
    constexpr static inline const char* name = "Settings";

    settings_window() noexcept = default;

    void show() noexcept;
    void apply_style(gui_theme_id id) noexcept;

    u32 gui_model_color;
    u32 gui_component_color;
    u32 gui_hovered_model_color;
    u32 gui_selected_model_color;
    u32 gui_hovered_component_color;
    u32 gui_selected_component_color;

    int   automatic_layout_iteration_limit = 200;
    float automatic_layout_x_distance      = 350.f;
    float automatic_layout_y_distance      = 350.f;
    float grid_layout_x_distance           = 240.f;
    float grid_layout_y_distance           = 200.f;

    bool show_dynamics_inputs_in_editor = false;

    bool is_open = false;
};

struct task_window {
    constexpr static inline const char* name = "Tasks";

    task_window() noexcept = default;

    void show() noexcept;
    void show_widgets() noexcept;

    bool is_open = false;
};

struct memory_window {
    constexpr static inline const char* name = "Memory usage";

    memory_window() noexcept = default;

    void show() noexcept;

    bool is_open = false;
};

class component_selector
{
public:
    component_selector() noexcept = default;

    /** Update the components cache with added/renamed/removed component. This
       function lock the mutex to write new data.
     */
    void update() noexcept;

    /** Show @c ImGui::ComboBox for all @c component_id.

       Show all @c component_id in @c modeling. This function can lock the mutex
       if an update is in progress.

       @param label The ImGui::ComboBox label.
       @param new_selected Output parameters.
       @return The underlying ImGui::ComboBox return.
    */
    bool combobox(const char* label, component_id* new_selected) const noexcept;

    /** Show @c ImGui::ComboBox for all @c component_id plus hyphen. This symbol
       means user let the @c component_id value untouch.

       Show all @c component_id in @c modeling. This function can lock the mutex
       if an update is in progress.

       @param label The ImGui::ComboBox label.
       @param[out] new_selected Store the selected @c component_id.
       @param[out] hyphen Store if the user select the hyphen
       @return The underlying ImGui::ComboBox return.
    */
    bool combobox(const char*   label,
                  component_id* new_selected,
                  bool*         hyphen) const noexcept;

    /** Display a @c ImGui::Menu for all @c component_id.

      Show all @c component_id in @c modeling. This function can lock the mutex
      if an update is in progress.

    */
    bool menu(const char* label, component_id* new_selected) const noexcept;

private:
    vector<component_id>      ids;
    vector<small_string<254>> names;

    int files   = 0; //! Number of component in registred directories
    int unsaved = 0; //! Number of unsaved component

    mutable std::shared_mutex m_mutex; /**< @c update() lock the class to read
                           modeling data and build the @c ids and @c names
                           vectors. Other functions try to lock. */
};

class component_model_selector
{
public:
    struct access {
        tree_node_id parent_id;
        component_id compo_id;
        tree_node_id tn_id;
        model_id     mdl_id;
    };

    component_model_selector() noexcept = default;

    /** Get the parent, component, treenode and model identifiers store in
     * the last call to the @c update function. */
    const access& get_update_access() noexcept { return m_access; }

    bool combobox(const char*   label,
                  project&      pj,
                  tree_node_id& parent_id,
                  component_id& compo_id,
                  tree_node_id& tn_id,
                  model_id&     mdl_id) const noexcept;

    /** A boolean to indicate that an update task is in progress. To be use to
     * disable control to avoid double update. */
    bool update_in_progress() const noexcept { return task_in_progress; }

    void start_update(const project&     pj,
                      const tree_node_id parent_id,
                      const component_id compo_id,
                      const tree_node_id tn_id,
                      const model_id     mdl_id) noexcept;

private:
    // Used in the component ComboBox to select the grid element.
    vector<std::pair<tree_node_id, component_id>> components;
    vector<small_string<254>>                     names;

    access m_access;

    mutable int component_selected = -1;
    bool        task_in_progress   = false;

    /** Clean and rebuild the components and names vector to be used with the @c
     * combobox function. */
    void update(const tree_node_id parent_id,
                const component_id compo_id,
                const tree_node_id tn_id,
                const model_id     mdl_id) noexcept;

    bool component_comboxbox(const char*   label,
                             component_id& compo_id,
                             tree_node_id& tn_id) const noexcept;

    bool observable_model_treenode(const project& pj,
                                   component_id&  compo_id,
                                   tree_node_id&  tn_id,
                                   model_id&      mdl_id) const noexcept;

    bool observable_model_treenode(const project& pj,
                                   tree_node&     tn,
                                   component_id&  compo_id,
                                   tree_node_id&  tn_id,
                                   model_id&      mdl_id) const noexcept;

    mutable std::shared_mutex m_mutex; /**< @c update() lock the class to read
                           modeling data and build the @c ids and @c names
                           vectors. Other functions try to lock. */
};

struct application {
    application() noexcept;
    ~application() noexcept;

    unsigned int main_dock_id;
    unsigned int right_dock_id;
    unsigned int bottom_dock_id;

    modeling mod;

    data_array<project_editor, project_id> pjs;

    /** Try to allocate a project and affect a new name to the newly allocated
     * project_window. */
    std::optional<project_id> alloc_project_window() noexcept;

    /** Free the @c id project and allo sub objects (@c registered_path etc.) */
    void free_project_window(const project_id id) noexcept;

    spin_mutex mod_mutex;
    spin_mutex sim_mutex;
    spin_mutex pj_mutex;

    component_selector       component_sel;
    component_model_selector component_model_sel;

    project_settings_editor project_wnd;

    component_editor component_ed;
    output_editor    output_ed;

    file_dialog f_dialog;

    settings_window settings_wnd;
    library_window  library_wnd;
    memory_window   memory_wnd;
    window_logger   log_wnd;
    task_window     task_wnd;

    registred_path_id select_dir_path = undefined<registred_path_id>();

    task_manager task_mgr;

    data_array<grid_component_editor_data, grid_editor_data_id>       grids;
    data_array<graph_component_editor_data, graph_editor_data_id>     graphs;
    data_array<generic_component_editor_data, generic_editor_data_id> generics;
    data_array<hsm_component_editor_data, hsm_editor_data_id>         hsms;

    std::filesystem::path select_directory;

    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;

    modeling_initializer mod_init;

    notification_manager notifications;

    config_manager config;

#ifdef IRRITATOR_USE_TTF
    ImFont* ttf = nullptr;
#endif

    bool show_select_directory_dialog = false;

    bool show_imgui_demo  = false;
    bool show_implot_demo = false;

    bool menu_new_project_file  = false;
    bool menu_load_project_file = false;
    bool menu_quit              = false;

    bool init() noexcept;
    void show() noexcept;

    status save_settings() noexcept;
    status load_settings() noexcept;

    //! Helpers function to add a @c simulation_task into the @c
    //! main_task_lists[simulation]. Task is added at tail of the @c
    //! ring_buffer and ensure linear operation.
    template<typename Fn>
    void add_simulation_task(Fn&& fn) noexcept;

    //! Helpers function to add a @c simulation_task into the @c
    //! main_task_lists[gui]. Task is added at tail of the @c ring_buffer and
    //! ensure linear operation.
    template<typename Fn>
    void add_gui_task(Fn&& fn) noexcept;

    //! Helpers function to get a @c unordered_task_list. Wait until the task
    //! list is available.
    unordered_task_list& get_unordered_task_list(int idx) noexcept;

    void start_load_project(const registred_path_id file,
                            const project_id        pj_id) noexcept;
    void start_save_project(const registred_path_id file,
                            const project_id        pj_id) noexcept;
    void start_save_component(const component_id id) noexcept;
    void start_init_source(const project_id          pj_id,
                           const u64                 id,
                           const source::source_type type) noexcept;
    void start_hsm_test_start() noexcept;
    void start_dir_path_refresh(const dir_path_id id) noexcept;
    void start_dir_path_free(const dir_path_id id) noexcept;
    void start_reg_path_free(const registred_path_id id) noexcept;
    void start_file_remove(const registred_path_id r,
                           const dir_path_id       d,
                           const file_path_id      f) noexcept;

    std::optional<file> try_open_file(const char* filename,
                                      open_mode   mode) noexcept;
};

/// Display dialog box to choose a @c model in a hierarchy of a @c tree_node
/// build from the @c tree_node @c tn that reference a grid component.
bool show_select_model_box(const char*     button_label,
                           const char*     popup_label,
                           application&    app,
                           project_editor& ed,
                           tree_node&      tn,
                           grid_observer&  access) noexcept;

/// Display dialog box to choose a @c model in a hierarchy of a @c tree_node
/// build from the @c tree_node @c tn that reference a graph component.
bool show_select_model_box(const char*     button_label,
                           const char*     popup_label,
                           application&    app,
                           project_editor& ed,
                           tree_node&      tn,
                           graph_observer& access) noexcept;

bool show_local_observers(application&     app,
                          project_editor&  ed,
                          tree_node&       tn,
                          component&       compo,
                          graph_component& graph) noexcept;

void alloc_grid_observer(irt::application& app, irt::tree_node& tn);

bool show_local_observers(application&    app,
                          project_editor& ed,
                          tree_node&      tn,
                          component&      compo,
                          grid_component& grid) noexcept;

void show_simulation_editor(application& app, project_editor& ed) noexcept;

//! @brief Get the file path of the @c imgui.ini file saved in $HOME.
//! @return A pointer to a newly allocated memory.
char* get_imgui_filename() noexcept;

template<typename Fn>
void application::add_simulation_task(Fn&& fn) noexcept
{
    task_mgr.main_task_lists[ordinal(main_task::simulation)].add(fn);
    task_mgr.main_task_lists[ordinal(main_task::simulation)].submit();
}

template<typename Fn>
void application::add_gui_task(Fn&& fn) noexcept
{
    task_mgr.main_task_lists[ordinal(main_task::gui)].add(fn);
    task_mgr.main_task_lists[ordinal(main_task::gui)].submit();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

inline tree_node_id project_editor::selected_tn() noexcept
{
    return m_selected_tree_node;
}

inline child_id project_editor::selected_child() noexcept
{
    return m_selected_child;
}

inline bool project_editor::can_edit() const noexcept
{
    if (any_equal(simulation_state,
                  simulation_status::not_started,
                  simulation_status::finished))
        return true;

    return allow_user_changes;
}

inline bool project_editor::can_display_graph_editor() const noexcept
{
    return display_graph;
}

inline void component_editor::request_to_open(const component_id id) noexcept
{
    m_request_to_open = id;
}

inline bool component_editor::need_to_open(const component_id id) const noexcept
{
    return m_request_to_open == id;
};

inline void component_editor::clear_request_to_open() noexcept
{
    m_request_to_open = undefined<component_id>();
}

} // namespace irt

#endif
