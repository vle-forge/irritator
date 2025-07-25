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

class application;
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

enum class main_task : i8 { simulation_0 = 0, simulation_1, simulation_2, gui };

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

/**
 * @brief  Display ComboBox to select a file from a directory.
 *
 * If the @a dir_id identifier is undefined, the function does not display
 * combobox. Only not `.irt` or `.txt` extension file can be selected with this
 * function.
 *
 * @c dir_id The directory path to search a file.
 * @c file_id The current file selected.
 *
 * @return @a undefined<file_id>() if the @a dir_id is undefined or if the user
 * select nothing otherwise return a valid @a file_path_id.
 */
auto show_data_file_input(const modeling&    mod,
                          const dir_path_id  dir_id,
                          const file_path_id file_id) noexcept -> file_path_id;

/** Display combobox and input real, input integer for each type of random
 * distribution. The @c random_source @c src is updated according to user
 * selection.
 * @return True if the user change the distribution type or the parameters.
 */
bool show_random_distribution_input(random_source& src) noexcept;

/** @brief Get the color of the component in the @a float[4] format.
 * If the @a component identifier @a id is undefined this function returns the
 * @a default_component_color.
 *
 * @param mod To access components array.
 * @param id The ID to test and get.
 * @return A @a component_color.
 */
auto get_component_color(const application& app, const component_id id) noexcept
  -> const std::span<const float, 4>;

/** @brief Get the color of the component in the @a ImU32 format.
 * If the @a component identifier @a id is undefined this function returns the
 * @a default_component_color.
 *
 * @param mod To access components array.
 * @param id The ID to test and get.
 * @return A undefined integer ImU32.
 */
auto get_component_u32color(const application& app,
                            const component_id id) noexcept -> ImU32;

enum class file_path_selector_option {
    none,
    force_dot_extension,
    force_irt_extension
};

/**
 * Display @a ImGui::ComboBox widgets to select file, dir and recorded paths.
 * @param app @a application to get @a modeling, @a notifications etc.
 * @param opt Options to force file path extension.
 * @param [in-out] reg_id @a recorded_path identifier.
 * @param [in-out] dir_id @a dir_path identifier.
 * @param [in-out] file_id @a file_path identifier.
 * @return @a true if a change occured in idenfiers parameters.
 */
auto file_path_selector(application&              app,
                        file_path_selector_option opt,
                        registred_path_id&        reg_id,
                        dir_path_id&              dir_id,
                        file_path_id&             file_id) noexcept -> bool;

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

    void show() noexcept;

    void enqueue(log_level        l,
                 std::string_view t,
                 std::string_view m,
                 u64              date) noexcept;

private:
    data_array<notification, notification_id> m_data;
    ring_buffer<notification_id>              m_enabled_ids;

    spin_mutex m_mutex;
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

    string_t& enqueue() noexcept;
    void      show() noexcept;

    bool is_open = true;

private:
    ring_t entries;

    bool auto_scroll      = true;
    bool scroll_to_bottom = false;

    spin_mutex m_mutex;
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
    void show(application&    app,
              project_editor& ed,
              graph_observer& graph,
              const ImVec2&   size) noexcept;

private:
    ImVec2 zoom{ 1.f, 1.f };
    ImVec2 scrolling{ 0.f, 0.f };
    ImVec2 distance{ 0.f, 0.f };
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

    int row = 10;
    int col = 10;

    component_id      selected_id = undefined<component_id>();
    grid_component_id grid_id     = undefined<grid_component_id>();

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

    graph_component::scale_free_param  psf{};
    graph_component::small_world_param psw{};
    graph_component::dot_file_param    pdf{};
    graph                              g;

    u64 seed[4];
    u64 key[2];

    vector<ImVec2> displacements;

    vector<graph_node_id> selected_nodes;
    vector<graph_edge_id> selected_edges;
    ImVec2                start_selection;
    ImVec2                end_selection;
    ImVec2                canvas_sz;

    ImVec2 distance{ 15.f, 15.f };
    ImVec2 scrolling{ 0.f, 0.f }; //!< top left position in canvas.
    ImVec2 zoom{ 1.f, 1.f };

    int iteration       = 0;
    int iteration_limit = 1000;

    graph_component_id graph_id = undefined<graph_component_id>();
    component_id       m_id     = undefined<component_id>();

    registred_path_id reg  = undefined<registred_path_id>();
    dir_path_id       dir  = undefined<dir_path_id>();
    file_path_id      file = undefined<file_path_id>();

    std::atomic_flag running = ATOMIC_FLAG_INIT;

    spin_mutex mutex;

    bool automatic_layout = false;
    bool run_selection    = false;

    enum class job : u8 {
        none,
        center_required,
        auto_fit_required,
        build_scale_free_required,
        build_small_world_required,
        build_dot_graph_required,
    } st = job::none;
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
                                  const generic_component&   gen) noexcept;
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

    ImVector<int>    selected_links;
    ImVector<int>    selected_nodes;
    vector<child_id> update_position_list;

    int   automatic_layout_iteration_limit = 2048;
    float automatic_layout_x_distance      = 350.f;
    float automatic_layout_y_distance      = 350.f;
    float grid_layout_x_distance           = 240.f;
    float grid_layout_y_distance           = 200.f;

    std::bitset<6> options;

private:
    component_id m_id = undefined<component_id>();
};

class generic_simulation_editor
{
public:
    generic_simulation_editor() noexcept;

    ~generic_simulation_editor() noexcept;

    /**
     * Build the @c links and @c nodes vectors from the project tree_node.
     *
     * This function lock the @c mutex to perform the computation. Use this
     * function in task.
     *
     * @param app Application to use thread task manager and log.
     * @param tn The tree node to display.
     * @param compo The component of the tree node.
     * @param gen The generic_component in the compo.
     */
    void init(application&       app,
              const tree_node&   tn,
              component&         compo,
              generic_component& gen) noexcept;

    /**
     * Build the @c links and @c nodes vectors from the entire simulation
     * models.
     *
     * This function lock the @c mutex to perform the computation. Use this
     * function in task.
     *
     *  @param app Application to use thread task manager and log.
     */
    void init(application& app) noexcept;

    /**
     * Add a rebuild @c nodes and @c links vectors task according to the current
     * @c tree_node.
     *
     * @param app Application to use thread task manager and log.
     */
    void reinit(application& app) noexcept;

    bool display(application& app) noexcept;

    struct link {
        link() noexcept = default;

        link(int out_, int in_, int mdl_out_, int mdl_in_) noexcept
          : out(out_)
          , in(in_)
          , mdl_out(mdl_out_)
          , mdl_in(mdl_in_)
        {}

        int out;     /**<  use get_model_output_port/make_output_node_id. */
        int in;      /**< use get_model_input_port/make_input_node_id. */
        int mdl_out; /**< output model in nodes index. */
        int mdl_in;  /**< input model in nodes index. */
    };

    struct node {
        node() noexcept = default;

        explicit node(const model_id mdl_) noexcept
          : mdl(mdl_)
        {}

        node(const model_id mdl_, const std::string_view name_) noexcept
          : mdl(mdl_)
          , name(name_)
        {}

        model_id mdl = undefined<model_id>();
        name_str name;
    };

private:
    struct impl;

    ImNodesEditorContext* context = nullptr;

    tree_node_id current = undefined<tree_node_id>();

    vector<int>    selected_links;
    vector<int>    selected_nodes;
    vector<link>   links;
    vector<node>   nodes;
    vector<link>   links_2nd;
    vector<node>   nodes_2nd;
    vector<ImVec2> displacements;

    int   automatic_layout_iteration       = 0;
    int   automatic_layout_iteration_limit = 2048;
    float automatic_layout_x_distance      = 350.f;
    float automatic_layout_y_distance      = 350.f;
    float grid_layout_x_distance           = 240.f;
    float grid_layout_y_distance           = 200.f;

    bool show_identifiers      = true;
    bool show_internal_values  = false;
    bool show_parameter_values = false;
    bool show_minimap          = true;
    bool can_edit_parameters   = true;

    bool enable_show =
      true; /**< @c false, the display of nodes and links are disabled. Mainly
               use during @c rebuild or @c init. */
    bool rebuild_wip = false; /**< @ true, a rebuild order is in work in
                                 progress. No other rebuild can occured. */

    void start_rebuild_task(application& app) noexcept;

    spin_mutex mutex;
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

class graph_simulation_editor
{
public:
    enum class action { camera_center, camera_auto_fit, Count };

    bool display(application&     app,
                 project_editor&  ed,
                 tree_node&       tn,
                 component&       compo,
                 graph_component& grid) noexcept;

    void reset() noexcept;

private:
    struct impl; /**< To access private part in implementation file. */

    graph_component_id current_id = undefined<graph_component_id>();

    ImVec2 distance{ 15.f, 15.f };
    ImVec2 scrolling{ 0.f, 0.f }; //!< top left position in canvas.
    ImVec2 zoom{ 1.f, 1.f };
    ImVec2 start_selection;
    ImVec2 end_selection;

    vector<graph_node_id> selected_nodes;
    bitflags<action>      actions;

    bool run_selection = false;
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

enum class command_type {
    none,
    new_model,
    free_model,
    copy_model,
    new_connection,
    free_connection,
    new_observer,
    free_observer,
    send_message
};

struct command {
    struct none_t {};

    struct new_model_t {
        tree_node_id  tn_id;
        dynamics_type type;
        float         x;
        float         y;
    };

    struct free_model_t {
        tree_node_id tn_id;
        model_id     mdl_id;
    };

    struct copy_model_t {
        tree_node_id tn_id;
        model_id     mdl_id;
    };

    struct new_connection_t {
        tree_node_id tn_id;
        model_id     mdl_src_id;
        model_id     mdl_dst_id;
        i8           port_src;
        i8           port_dst;
    };

    struct free_connection_t {
        tree_node_id tn_id;
        model_id     mdl_src_id;
        model_id     mdl_dst_id;
        i8           port_src;
        i8           port_dst;
    };

    struct new_observer_t {
        tree_node_id tn_id;
        model_id     mdl_id;
    };

    struct free_observer_t {
        tree_node_id tn_id;
        model_id     mdl_id;
    };

    struct send_message_t {
        model_id mdl_id;
    };

    union data_t {
        none_t            none;
        new_model_t       new_model;
        free_model_t      free_model;
        copy_model_t      copy_model;
        new_connection_t  new_connection;
        free_connection_t free_connection;
        new_observer_t    new_observer;
        free_observer_t   free_observer;
        send_message_t    send_message;
    };

    command_type type = command_type::none;
    data_t       data;
};

struct project_editor {
    enum class visualization_mode { flat, tree };

    explicit project_editor(const std::string_view default_name) noexcept;

    ~project_editor() noexcept;

    enum class show_result_t {
        success,          /**< Nothing to do. */
        request_to_close, /**< The window must be closed by the called. */
    };

    /** Display the project window in a @c ImGui::Begin/End window.
     *
     * @return project_editor next status enumeration: keep the window open or
     * try to close it. */
    show_result_t show(application& app) noexcept;

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

    bool can_edit() const noexcept;
    bool can_display_graph_editor() const noexcept;

    bool force_pause           = false;
    bool force_stop            = false;
    bool show_minimap          = true;
    bool store_all_changes     = false;
    bool real_time             = false;
    bool have_use_back_advance = false;
    bool display_graph         = true;

    bool allow_user_changes = false;

    bool save_project_file    = false;
    bool save_as_project_file = false;

    enum class raw_data_type : u8 { none, graph, binary, text };

    raw_data_type save_simulation_raw_data = raw_data_type::none;

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

    ImPlotContext* output_context = nullptr;

    /** Number of column in the tree node observation. */
    using tree_node_observation_t = constrained_value<int, 1, 100>;
    tree_node_observation_t tree_node_observation{ 1 };
    float                   tree_node_observation_height = 300.f;

    circular_buffer<command, 256> commands;

    registred_path_id project_file = undefined<registred_path_id>();
    project           pj;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(application& app, tree_node_id id) noexcept;

    //! @return true if @c id is the selected @c tree_node.
    bool is_selected(tree_node_id id) const noexcept;

    tree_node_id selected_tn() noexcept;

    tree_node_id m_selected_tree_node = undefined<tree_node_id>();

    project_external_source_editor data_ed;

    /// For each simulation, and according to the @a save_simulation_raw_data,
    /// an output stream to store all model state during simulation.
    buffered_file raw_ofs;
};

inline bool project_editor::can_edit() const noexcept
{
    if (any_equal(simulation_state,
                  simulation_status::not_started,
                  simulation_status::finished))
        return true;

    return allow_user_changes;
}

class component_editor
{
public:
    constexpr static inline const char* name = "Component editor";

    component_editor() noexcept;

    /** Draw the editor into a ImGui::Begin/ImGui::End window. */
    void display() noexcept;

    void request_to_open(const component_id id) noexcept;
    bool need_to_open(const component_id id) const noexcept;
    void clear_request_to_open() noexcept;

    /** Force to close the opened component @c id. */
    void close(const component_id id) noexcept;

    small_string<31> title;
    small_string<31> component_name;

    bool is_open = true;

    void add_generic_component_data() noexcept;
    void add_grid_component_data() noexcept;
    void add_graph_component_data() noexcept;
    void add_hsm_component_data() noexcept;

    bool is_component_open(const component_id id) const noexcept;

    template<typename IdentifierType>
    auto try_to_get(const IdentifierType id) noexcept
    {
        if constexpr (std::is_same_v<IdentifierType, grid_editor_data_id>)
            return grids.try_to_get(id);
        if constexpr (std::is_same_v<IdentifierType, graph_editor_data_id>)
            return graphs.try_to_get(id);
        if constexpr (std::is_same_v<IdentifierType, generic_editor_data_id>)
            return generics.try_to_get(id);
        if constexpr (std::is_same_v<IdentifierType, hsm_editor_data_id>)
            return hsms.try_to_get(id);
    }

    template<typename T>
    auto get_id(const T& elem) const noexcept
    {
        if constexpr (std::is_same_v<T, grid_component_editor_data>)
            return grids.get_id(elem);
        if constexpr (std::is_same_v<T, graph_component_editor_data>)
            return graphs.get_id(elem);
        if constexpr (std::is_same_v<T, generic_component_editor_data>)
            return generics.get_id(elem);
        if constexpr (std::is_same_v<T, hsm_component_editor_data>)
            return hsms.get_id(elem);
    }

    struct tab {
        component_id   id;
        component_type type;

        union {
            grid_editor_data_id    grid;
            graph_editor_data_id   graph;
            generic_editor_data_id generic;
            hsm_editor_data_id     hsm;
        } data;
    };

private:
    struct impl;

    enum { tabitem_open_save, tabitem_open_in_out };

    data_array<grid_component_editor_data, grid_editor_data_id>       grids;
    data_array<graph_component_editor_data, graph_editor_data_id>     graphs;
    data_array<generic_component_editor_data, generic_editor_data_id> generics;
    data_array<hsm_component_editor_data, hsm_editor_data_id>         hsms;

    /// List of tabulation opened in the @a ImGui::BeginTabBar.
    vector<tab> tabs;

    vector<component_id> component_list;
    spin_mutex           component_list_mutex;
    std::atomic_flag     component_list_updating = ATOMIC_FLAG_INIT;

    std::bitset<2> tabitem_open;
    component_id   m_request_to_open = undefined<component_id>();
};

class library_window
{
public:
    constexpr static inline const char* name = "Library";

    library_window() noexcept
      : stack(max_component_stack_size, reserve_tag{})
    {}

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
      const component_id id) noexcept;

    void show() noexcept;

    bool is_open = true;

private:
    vector<tree_node*> stack;
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
    void apply_style(const int theme) noexcept;

    bool is_open = false;

    /** Stores the clock when settings are changed. If after five second user
     * does not change anything, a save settings task is launch. */
    std::chrono::time_point<std::chrono::steady_clock> last_change;

    bool timer_started = false;
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

/**
 * @brief Display component selection widgets.
 * The @a update() function uses a @a unique_lock to protect write access to
 * component cache. The widget functions use a @a shared_lock and a @a
 * try_lock to protect component cache access.
 */
class component_selector
{
public:
    component_selector() noexcept = default;

    /** Update the components cache with added/renamed/removed component.
     * This function lock the @a shared_mutex to write new data using a
     * @a std::unique_lock.
     */
    void update() noexcept;

    struct result_t {
        component_id id;
        bool         is_done;

        operator bool() const noexcept { return is_done; }
    };

    /**
     * Dispay @c ImGui::ComboBox for all @c component.
     *
     * @param label The @a ImGui::ComboBox label.
     * @param current The current @a component_id to be selected in the
     * combobox.
     * @return a simple structure with a valid @a component_id if the structure
     * is valid.
     */
    result_t combobox(const char*        label,
                      const component_id current) const noexcept;

    /**
     * Dispay @c ImGui::ComboBox for @c component of @c type @c component_id.
     *
     * @param label The @a ImGui::ComboBox label.
     * @param type The @c component_type to be displayed in the ComboBox.
     * @param current The current @a component_id to be selected in the
     * combobox.
     * @return a simple structure with a valid @a component_id if the structure
     * is valid.
     */
    result_t combobox(const char*          label,
                      const component_type type,
                      const component_id   current) const noexcept;

    /**
     * Display a @c ImGui::MenuItem for all @c component_id in different
     * category. This function can lock the mutex if an update is in progress.
     *
     * @param label The @c ImGui::BeginMenu label
     * @return a simple structure with a valid @a component_id if the structure
     * is valid.
     */
    result_t menu(const char* label) const noexcept;

private:
    struct data_type {
        vector<std::pair<component_id, name_str>>      by_names;
        vector<std::pair<component_id, file_path_str>> by_files;
        vector<std::pair<component_id, name_str>>      by_generics;
        vector<std::pair<component_id, name_str>>      by_grids;
        vector<std::pair<component_id, name_str>>      by_graphs;
        vector<std::pair<component_id, name_str>>      by_hsms;
    };

    locker_2<data_type> data;
};

class component_model_selector
{
public:
    struct access {
        tree_node_id parent_id = undefined<tree_node_id>();
        component_id compo_id  = undefined<component_id>();
        tree_node_id tn_id     = undefined<tree_node_id>();
        model_id     mdl_id    = undefined<model_id>();
    };

    component_model_selector() noexcept = default;

    /**
     * Display a @c ImGui::ComboBox and a @c ImGui::TreeNode for the hierarchy
     * of @c tree_node given in update function. This function can lock the
     * mutex if an update is in progress.
     *
     * @param label The @a ImGui::ComboBox label.
     * @param pj The project to use to build the combobox.
     * @return An optional access structure with valid data if a component is
     * selected.
     */
    std::optional<access> combobox(const char*    label,
                                   const project& pj) noexcept;

    /**
     * Update the components cache with component. This function lock the @a
     * shared_mutex to write new data using a @a std::unique_lock.
     */
    void update(const project&     pj,
                const tree_node_id parent_id,
                const component_id compo_id,
                const tree_node_id tn_id,
                const model_id     mdl_id) noexcept;

private:
    struct data_type {
        vector<std::pair<tree_node_id, component_id>> components;
        vector<name_str>                              names;
        vector<tree_node*>                            stack_tree_nodes;
    };

    locker_2<data_type> data;

    tree_node_id parent_id = undefined<tree_node_id>();
    tree_node_id tn_id     = undefined<tree_node_id>();
    model_id     mdl_id    = undefined<model_id>();
    component_id compo_id  = undefined<component_id>();

    mutable int component_selected = -1;

    void component_comboxbox(const char* label, const data_type& data) noexcept;
    void observable_model_treenode(const project&   pj,
                                   const data_type& data) noexcept;
    void observable_model_treenode(const project& pj, tree_node& tn) noexcept;
};

class simulation_to_cpp
{
public:
    void show(const project_editor& ed) noexcept;

private:
    u8 options = 0u;

    enum class status_type : u8 {
        none,
        success,
        output_error,
        external_source_error,
        hsm_error,
    } status = status_type::none;
};

class application
{
public:
    explicit application(journal_handler& jn) noexcept;

    ~application() noexcept;

private:
    task_manager<4, 1> task_mgr;

public:
    config_manager   config;
    modeling         mod;
    journal_handler& jn;

    data_array<project_editor, project_id> pjs;

    enum class show_result_t {
        success,          /**< Nothing to do. */
        request_to_close, /**< The window must be closed by the called. */
    };

    component_selector       component_sel;
    component_model_selector component_model_sel;

    project_settings_editor project_wnd;
    component_editor        component_ed;
    output_editor           output_ed;

    file_dialog f_dialog;

    settings_window settings_wnd;
    library_window  library_wnd;
    memory_window   memory_wnd;
    window_logger   log_wnd;
    task_window     task_wnd;

    simulation_to_cpp sim_to_cpp;

    notification_manager notifications;

    /**
     * Try to allocate a project and affect a new name to the newly allocated
     * project_window.
     */
    std::optional<project_id> alloc_project_window() noexcept;

    /**
     * Free the @a project according to the @a project_id and free all
     * subobjects (@a registered_path etc.).
     */
    void free_project_window(const project_id id) noexcept;

    bool          init() noexcept;
    show_result_t show() noexcept;

    /**
     * Helpers function to add a @c simulation_task into an @c
     * ordered_task_list. The task list used depends on the @c get_index or the
     * @c project_id. Task is added at tail of the @c ring_buffer and ensure
     * linear operation.
     *
     * @param id The identifier use to select the @a
     * task_manager::ordered_task_lists.
     * @param fn The function to run.
     */
    template<typename Fn>
    void add_simulation_task(const project_id id, Fn&& fn) noexcept;

    /**
     * Helpers function to add a @c gui_task into the @c ordered_task_list gui.
     * Task is added at tail of the @c ring_buffer and ensure linear operation.
     *
     * @param fn The function to run.
     */
    template<typename Fn>
    void add_gui_task(Fn&& fn) noexcept;

    /**
     * Helpers function to get an @a unordered_task_list according to the
     * integer modulo operation on @a idx index. This index can be produced from
     * project identifier.
     *
     * @param idx The index in range `[0, unordered_task_worker_size - 1]`.
     * @return The reference to the @a unordered_task_list
     */
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

    unsigned int get_main_dock_id() const noexcept { return main_dock_id; }

    void request_open_directory_dlg(const registred_path_id id) noexcept;

private:
    friend task_window;

    unsigned int main_dock_id;
    unsigned int right_dock_id;
    unsigned int bottom_dock_id;

    registred_path_id selected_reg_path    = undefined<registred_path_id>();
    bool              show_select_reg_path = false;

    bool show_imgui_demo  = false;
    bool show_implot_demo = false;

    show_result_t show_menu() noexcept;
    void          show_dock() noexcept;
    void          show_select_directory_dlg() noexcept;
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

//! @brief Get the file path of the @c imgui.ini file saved in $HOME.
//! @return A pointer to a newly allocated memory.
char* get_imgui_filename() noexcept;

template<typename Fn>
void application::add_simulation_task(const project_id id, Fn&& fn) noexcept
{
    const auto index = get_index(id) % 3;

    task_mgr.ordered_task_lists[index].add(fn);
    task_mgr.ordered_task_lists[index].submit();
}

template<typename Fn>
void application::add_gui_task(Fn&& fn) noexcept
{
    task_mgr.ordered_task_lists[ordinal(main_task::gui)].add(fn);
    task_mgr.ordered_task_lists[ordinal(main_task::gui)].submit();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

inline bool project_editor::can_display_graph_editor() const noexcept
{
    return display_graph;
}

inline bool project_editor::is_simulation_running() const noexcept
{
    return any_equal(simulation_state,
                     simulation_status::paused,
                     simulation_status::running,
                     simulation_status::run_requiring);
}

inline tree_node_id project_editor::selected_tn() noexcept
{
    return m_selected_tree_node;
}

} // namespace irt

#endif
