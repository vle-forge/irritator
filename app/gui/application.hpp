// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/file.hpp>
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

template<class T, class M>
constexpr std::ptrdiff_t offset_of(const M T::* member)
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
//!     line& ptr = container_of(&p, &line::p1);
//!     ...
//! }
//! @endcode
template<class T, class M>
constexpr T& container_of(M* ptr, const M T::* member)
{
    return *reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                                 offset_of(member));
}

struct application;
class component_editor;

enum class notification_id : u32;
enum class plot_copy_id : u32;

enum class simulation_task_id : u64;
enum class gui_task_id : u64;

enum class grid_editor_data_id : u32;
enum class graph_editor_data_id : u32;
enum class generic_editor_data_id : u32;
enum class hsm_editor_data_id : u32;

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

void show_menu_external_sources(external_source& srcs,
                                const char*      title,
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
    void show(application& app) noexcept;

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

    void save_obs(const variable_observer_id      vobs,
                  const variable_observer::sub_id svobs) noexcept;
    void save_copy(const plot_copy_id id) noexcept;

private:
    ImPlotContext* m_ctx = nullptr;

    std::filesystem::path m_file;

    plot_copy_id              m_copy_id = undefined<plot_copy_id>();
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

    vector<ImVec2> positions;
    vector<ImVec2> displacements;

    vector<graph_component::vertex_id> selected_nodes;
    vector<graph_component::edge_id>   selected_edges;

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

private:
    component_id m_id = undefined<component_id>();
};

class hsm_component_editor_data
{
public:
    constexpr static inline const char* name = "HSM Editor";

    constexpr static inline auto max_number_of_state =
      hierarchical_state_machine::max_number_of_state;

    enum class test_status { none, being_processed, done, failed };

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

    // void load(component_id c_id, model_id m_id) noexcept;
    // void load(model_id m_id) noexcept;
    // void save() noexcept;

private:
    void show_hsm(hsm_component& hsm) noexcept;
    void show_menu(hsm_component& hsm) noexcept;
    void show_graph(hsm_component& hsm) noexcept;
    void show_panel(hsm_component& hsm) noexcept;
    void clear(hsm_component& hsm) noexcept;
    bool valid(hsm_component& hsm) noexcept;

    ImNodesEditorContext* m_context = nullptr;

    ImVector<int>                                  m_selected_links;
    ImVector<int>                                  m_selected_nodes;
    ImVector<hierarchical_state_machine::state_id> m_stack;

    std::array<ImVec2, max_number_of_state>  m_position;
    std::array<bool, max_number_of_state>    m_enabled;
    small_ring_buffer<small_string<127>, 10> m_messages;

    test_status m_test = test_status::none;

private:
    component_id     m_id     = undefined<component_id>();
    hsm_component_id m_hsm_id = undefined<hsm_component_id>();
};

class generic_component_editor_data
{
public:
    generic_component_editor_data(const component_id id) noexcept;
    ~generic_component_editor_data() noexcept;

    void show(component_editor& ed) noexcept;
    void show_selected_nodes(component_editor& ed) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    //! Before running any ImNodes functions, pre-move all children to force
    //! position for all new children.
    void update_position() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    ImNodesEditorContext* context = nullptr;

public:
    bool show_minimap            = true;
    bool show_input_output       = true;
    bool first_show_input_output = true;
    bool fix_input_output        = false;
    bool force_update_position   = false;

    ImVector<int>    selected_links;
    ImVector<int>    selected_nodes;
    vector<child_id> update_position_list;

private:
    component_id m_id = undefined<component_id>();
};

struct grid_simulation_editor {
    ImVec2            show_position{ 0.f, 0.f };
    ImVec2            disp{ 1000.f, 1000.f };
    float             scale           = 10.f;
    bool              start_selection = false;
    grid_component_id current_id      = undefined<grid_component_id>();

    component_id selected_setting_component = undefined<component_id>();
    model_id     selected_setting_model     = undefined<model_id>();

    component_id selected_observation_component = undefined<component_id>();
    model_id     selected_observation_model     = undefined<model_id>();
    tree_node*   selected_tn                    = nullptr;

    std::optional<std::pair<int, int>> selected_position;

    vector<bool>         selected;
    vector<component_id> children_class;

    void clear() noexcept;

    bool show_settings(tree_node&      tn,
                       component&      compo,
                       grid_component& grid) noexcept;

    bool show_observations(tree_node&      tn,
                           component&      compo,
                           grid_component& grid) noexcept;
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

struct grid_editor_dialog {
    constexpr static inline const char* name = "Grid generator";

    grid_editor_dialog() noexcept;

    grid_component     grid;
    application*       app        = nullptr;
    generic_component* compo      = nullptr;
    bool               is_running = false;
    bool               is_ok      = false;

    void load(application& app, generic_component* compo) noexcept;
    void save() noexcept;
    void show() noexcept;
};

struct graph_editor_dialog {
    constexpr static inline const char* name = "Graph generator";

    graph_editor_dialog() noexcept;

    graph_component    graph;
    application*       app        = nullptr;
    generic_component* compo      = nullptr;
    bool               is_running = false;
    bool               is_ok      = false;

    void load(application& app, generic_component* compo) noexcept;
    void save() noexcept;
    void show() noexcept;
};

struct simulation_editor {
    constexpr static inline const char* name = "Simulation editor";

    enum class visualization_mode { flat, tree };

    simulation_editor() noexcept;
    ~simulation_editor() noexcept;

    void show() noexcept;

    void select(tree_node_id id) noexcept;
    void unselect() noexcept;
    void clear() noexcept;

    void start_simulation_update_state() noexcept;

    void start_simulation_copy_modeling() noexcept;
    void start_simulation_init() noexcept;
    void start_simulation_delete() noexcept;
    void start_simulation_clear() noexcept;
    void start_simulation_start() noexcept;
    void start_simulation_observation() noexcept;
    void start_simulation_live_run() noexcept;
    void start_simulation_static_run() noexcept;
    void start_simulation_start_1() noexcept;
    void start_simulation_pause() noexcept;
    void start_simulation_stop() noexcept;
    void start_simulation_finish() noexcept;
    void start_simulation_advance() noexcept;
    void start_simulation_back() noexcept;
    void start_enable_or_disable_debug() noexcept;

    void start_simulation_model_add(const dynamics_type type,
                                    const float         x,
                                    const float         y) noexcept;
    void start_simulation_model_del(const model_id id) noexcept;

    void remove_simulation_observation_from(const model_id id) noexcept;
    void add_simulation_observation_for(const model_id id) noexcept;

    bool can_edit() const noexcept;
    bool can_display_graph_editor() const noexcept;

    bool force_pause           = false;
    bool force_stop            = false;
    bool show_minimap          = true;
    bool allow_user_changes    = true;
    bool store_all_changes     = false;
    bool infinity_simulation   = false;
    bool real_time             = false;
    bool have_use_back_advance = false;
    bool display_graph         = true;

    bool show_internal_values = false;
    bool show_internal_inputs = false;
    bool show_identifiers     = false;

    bool is_open = true;

    timeline tl;

    real simulation_begin           = 0;
    real simulation_end             = 100;
    real simulation_display_current = 0;

    /** Number of microsecond to run 1 unit of simulation time. The default is
     * to run 1 unit of simulation per second. */
    i64 nb_microsecond_per_simulation_time = 1000000;

    /** Simulation duraction between each run task. The default is 100ms. */
    static inline constexpr i64 thread_frame_duration = 100000;

    tree_node_id head    = undefined<tree_node_id>();
    tree_node_id current = undefined<tree_node_id>();

    visualization_mode mode = visualization_mode::flat;

    simulation_status simulation_state = simulation_status::not_started;
    data_array<plot_copy, plot_copy_id> copy_obs;

    plot_observation_widget  plot_obs;
    grid_observation_widget  grid_obs;
    graph_observation_widget graph_obs;

    plot_copy_widget plot_copy_wgt;

    grid_simulation_editor  grid_sim;
    graph_simulation_editor graph_sim;

    ImNodesEditorContext* context        = nullptr;
    ImPlotContext*        output_context = nullptr;

    ImVector<int> selected_links;
    ImVector<int> selected_nodes;

    /** Number of column in the tree node observation. */
    constrained_value<int, 1, 100> tree_node_observation        = 1;
    float                          tree_node_observation_height = 200.f;

    //! Position of each node
    int              automatic_layout_iteration = 0;
    ImVector<ImVec2> displacements;

    small_ring_buffer<std::pair<model_id, ImVec2>, 8>
      models_to_move; /**< Online simulation created models need to use ImNodes
                         API to move into the canvas. */
};

struct data_window {
    constexpr static inline const char* name = "Data";

    data_window() noexcept;
    ~data_window() noexcept;

    void show_widgets() noexcept;
    void show() noexcept;

    ImVector<ImVec2> plot;

    ImPlotContext* context = nullptr;

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

    bool show_file_dialog = false;
    bool plot_available   = false;
    bool is_open          = true;
};

class component_editor
{
public:
    constexpr static inline const char* name = "Component editor";

    void show() noexcept;
    void add_generic_component() noexcept;
    void add_grid_component() noexcept;

    void request_to_open(const component_id id) noexcept;
    bool need_to_open(const component_id id) const noexcept;
    void clear_request_to_open() noexcept;

    small_string<31> title;
    small_string<31> component_name;
    bool             is_open = true;

private:
    component_id m_request_to_open = undefined<component_id>();
};

class library_window
{
public:
    constexpr static inline const char* name = "Library";

    library_window() noexcept = default;

    void try_set_component_as_project(const component_id id) noexcept;

    void show() noexcept;

    bool is_open = true;
};

//! An ImGui Window used to display the project hierarchy (@c hierarchy of @c
//! tree_node). Configurable or observable widgets are provided for each @c
//! tree_node with public configurable or observable component.
class project_window
{
public:
    project_window() noexcept = default;

    //! Display the window if the @c application::pj head is defined.
    void show() noexcept;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(tree_node_id id) noexcept;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(tree_node& node) noexcept;

    //! Select a @C child in the modeling tree node. The existence of the
    //! underlying component is tested before assignment.
    void select(child_id id) noexcept;

    //! Clear the selected component and child.
    void clear() noexcept;

    //! @return true if @c id is the selected @c tree_node.
    bool is_selected(tree_node_id id) const noexcept;

    //! @return true if @c id is the selected @c child.
    bool is_selected(child_id id) const noexcept;

    tree_node_id selected_tn() noexcept;
    child_id     selected_child() noexcept;

private:
    tree_node_id m_selected_tree_node = undefined<tree_node_id>();
    child_id     m_selected_child     = undefined<child_id>();
};

struct settings_window {
    constexpr static inline const char* name = "Settings";

    settings_window() noexcept = default;

    //! @brief Compute selected and hovered colours from gui_model_color and
    //! gui_component_color.
    void update() noexcept;

    void show() noexcept;
    void apply_default_style() noexcept;

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

    //! @brief Update the component cache with added/removed component.
    void update() noexcept;

    //! @brief Show @c ImGui::ComboBox for all @c component_id
    //! @details Show all @c component_id in @c modeling.
    //!
    //! @param label The ImGui::ComboBox label.
    //! @param new_selected Output parameters.
    //! @return The underlying ImGui::ComboBox return.
    bool combobox(const char* label, component_id* new_selected) noexcept;

    //! @brief Show @c ImGui::ComboBox for all @c component_id plus hyphen.
    //! @details Show all @c component_id in @c modeling plus the hyphen symbol.
    //! This symbol means user let the @c component_id value untouch.
    //!
    //! @param label The ImGui::ComboBox label.
    //! @param[out] new_selected Store the selected @c component_id.
    //! @param[out] hyphen Store if the user select the hyphen
    //! @return The underlying ImGui::ComboBox return.
    bool combobox(const char*   label,
                  component_id* new_selected,
                  bool*         hyphen) noexcept;

    //! @brief Display a @c ImGui::Menu for all @c component_id.
    bool menu(const char* label, component_id* new_selected) noexcept;

private:
    vector<component_id>      ids;
    vector<small_string<254>> names;

    component_id      selected_id   = undefined<component_id>();
    small_string<254> selected_name = "undefined";

    int files   = 0; //! Number of component in registred directories
    int unsaved = 0; //! Number of unsaved component

    spin_mutex m_mutex; /** @c update() lock the class to read modeling data and
                           build the @c ids and @c names vectors. Other
                           functions try to lock. */
};

class component_model_selector
{
public:
    component_model_selector() noexcept = default;

    /// If @c id is not equal to @c current_tree_node the clear and rebuild
    /// vector and cache from the @c tree_node @c id.
    void select(const tree_node_id parent_id) noexcept;

    /// If @c id is not equal to @c current_tree_node the clear and rebuild
    /// vector and cache from the @c tree_node @c id. Component selected are
    /// read from the @c g_obs.
    void select(const tree_node_id parent_id,
                const component_id compo_id,
                const tree_node_id tn_id,
                const model_id     mdl_id) noexcept;

    bool combobox(const char*   label,
                  tree_node_id& parent_id,
                  component_id& compo_id,
                  tree_node_id& tn_id,
                  model_id&     mdl_id) noexcept;

private:
    // Used in the component ComboBox to select the grid element.
    vector<std::pair<tree_node_id, component_id>> components;
    vector<small_string<254>>                     names;

    // A cache to proceed recursive search.
    vector<tree_node*> stack_tree_nodes;

    tree_node_id current_tree_node;
    int          component_selected = -1;

    bool component_comboxbox(const char*   label,
                             component_id& compo_id,
                             tree_node_id& tn_id) noexcept;

    bool observable_model_treenode(component_id& compo_id,
                                   tree_node_id& tn_id,
                                   model_id&     mdl_id) noexcept;

    bool observable_model_treenode(tree_node&    tn,
                                   component_id& compo_id,
                                   tree_node_id& tn_id,
                                   model_id&     mdl_id) noexcept;
};

struct application {
    application() noexcept;
    ~application() noexcept;

    modeling   mod;
    simulation sim;
    project    pj;

    spin_mutex mod_mutex;
    spin_mutex sim_mutex;
    spin_mutex pj_mutex;

    component_selector       component_sel;
    component_model_selector component_model_sel;

    project_window project_wnd;

    component_editor  component_ed;
    simulation_editor simulation_ed;
    output_editor     output_ed;
    data_window       data_ed;

    grid_editor_dialog  grid_dlg;
    graph_editor_dialog graph_dlg;
    file_dialog         f_dialog;

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

    std::filesystem::path project_file;
    std::filesystem::path select_directory;

    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;

    modeling_initializer mod_init;

    notification_manager notifications;

#ifdef IRRITATOR_USE_TTF
    ImFont* ttf = nullptr;
#endif

    bool show_select_directory_dialog = false;

    bool show_imgui_demo  = false;
    bool show_implot_demo = false;

    bool menu_new_project_file     = false;
    bool menu_load_project_file    = false;
    bool menu_save_project_file    = false;
    bool menu_save_as_project_file = false;
    bool menu_quit                 = false;

    bool init() noexcept;
    void show() noexcept;

    void show_tasks_widgets() noexcept;
    void show_external_sources() noexcept;
    void show_log_window() noexcept;
    void show_simulation_observation_window() noexcept;
    void show_components_window() noexcept;
    void show_project_window() noexcept;
    void show_memory_box(bool* is_open) noexcept;

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

    void start_load_project(const registred_path_id file) noexcept;
    void start_save_project(const registred_path_id file) noexcept;
    void start_save_component(const component_id id) noexcept;
    void start_init_source(const u64                 id,
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
bool show_select_model_box(const char*    button_label,
                           const char*    popup_label,
                           application&   app,
                           tree_node&     tn,
                           grid_observer& access) noexcept;

/// Display dialog box to choose a @c model in a hierarchy of a @c tree_node
/// build from the @c tree_node @c tn that reference a graph component.
bool show_select_model_box(const char*     button_label,
                           const char*     popup_label,
                           application&    app,
                           tree_node&      tn,
                           graph_observer& access) noexcept;

bool show_local_observers(application&     app,
                          tree_node&       tn,
                          component&       compo,
                          graph_component& graph) noexcept;

void alloc_grid_observer(irt::application& app, irt::tree_node& tn);

bool show_local_observers(application&    app,
                          tree_node&      tn,
                          component&      compo,
                          grid_component& grid) noexcept;

void show_simulation_editor(application& app) noexcept;

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

inline tree_node_id project_window::selected_tn() noexcept
{
    return m_selected_tree_node;
}

inline child_id project_window::selected_child() noexcept
{
    return m_selected_child;
}

inline bool simulation_editor::can_edit() const noexcept
{
    if (any_equal(simulation_state,
                  simulation_status::not_started,
                  simulation_status::finished))
        return true;

    return allow_user_changes;
}

inline bool simulation_editor::can_display_graph_editor() const noexcept
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
