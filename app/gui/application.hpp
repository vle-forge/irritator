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
enum class simulation_editor_data_id : u32;

enum class graph_editor_id : u32;

enum class task_status : u8 { not_started, started, finished };
enum class main_task : u8 { simulation_0 = 0, simulation_1, simulation_2, gui };

enum class simulation_status : u8 {
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

enum class simulation_plot_type : u8 { none, plotlines, plotscatters };

/// @class request_buffer
///
/// @brief A thread-safe asynchronous mailbox with a built-in anti-spam
/// mechanism.
///
/// esigned for a "Request-Fulfill" pattern between a high-frequency
/// GUI thread and asynchronous worker threads. It prevents "Request Storms" by
/// ensuring only one computation task is active at any given time.
///
/// @tparam T The type of data being transferred. Supports complex types
/// (strings, vectors).
///
/// @code
/// void update_gui() {
///     // 1. We are trying to retrieve data that has just finished
///     if (auto result = my_buffer.try_take())
///         this->data = *result; // update the gui
///
///     // 2. If the data is still missing AND no task is in progress
///     if (not this->has_data() && my_buffer.should_request()) {
///         // This line will only be executed ONCE until fulfill() is called.
///         add_gui_task([]() {
///             auto data = expensive_calculation();
///             my_buffer.fulfill(std::move(data));
///         });
///     }
/// }
/// @endcode
template<typename T>
class request_buffer
{
public:
    enum class state : u8 { idle, pending, ready };

    request_buffer() noexcept = default;

    request_buffer(const request_buffer&)            = delete;
    request_buffer& operator=(const request_buffer&) = delete;

    /// Checks if a new request should be sent.
    /// @return true if state was Idle (now pending), false if a task is already
    /// running.
    bool should_request() noexcept
    {
        state expected = state::idle;
        return m_state.compare_exchange_strong(
          expected, state::pending, std::memory_order_acq_rel);
    }

    /// Delivers the result from the worker thread.
    void fulfill(T&& data) noexcept
    {
        {
            std::lock_guard lock(m_mutex);
            m_value.emplace(std::forward<T>(data));
        }
        m_state.store(state::ready, std::memory_order_release);
    }

    /// Attempts to consume the result in the GUI thread.
    /// @return std::optional containing the data if ready, otherwise
    /// std::nullopt.
    std::optional<T> try_take() noexcept
    {
        if (m_state.load(std::memory_order_acquire) == state::ready) {
            std::unique_lock lock(m_mutex, std::try_to_lock);
            if (lock.owns_lock()) {
                std::optional<T> out = std::move(m_value);
                m_value.reset();
                m_state.store(state::idle, std::memory_order_release);
                return out;
            }
        }
        return std::nullopt;
    }

    /// Resets the buffer to idle. Use this if a task fails or times out.
    void cancel() noexcept
    {
        std::lock_guard lock(m_mutex);
        m_value.reset();
        m_state.store(state::idle, std::memory_order_release);
    }

private:
    std::atomic<state> m_state{ state::idle };
    mutable std::mutex m_mutex;
    std::optional<T>   m_value;
};

/// @class atomic_request_buffer
///
/// @brief An ultra-fast, lock-free asynchronous mailbox for small data types.
///
/// @details Optimized version using purely atomic operations. Ideal for IDs,
/// Enums, or small numeric results. T must be Trivially Copyable.
///
/// @code
/// void on_render_frame() {
///     // 1. try to read
///     if (auto score = score_buffer.try_take()) {
///         this->current_score = *score;
///     }
///
///     // 2. If we need information and haven't already started the task
///     if (needs_update && score_buffer.should_request()) {
///         std::thread([&](){
///             int res = compute_heavy_score();
///             score_buffer.fulfill(res);
///         }).detach();
///     }
/// }
/// @endcode
template<typename T>
class atomic_request_buffer
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for atomic_request_buffer");

public:
    enum class state : u8 { idle, pending, ready };

    atomic_request_buffer() noexcept = default;

    atomic_request_buffer(const atomic_request_buffer&)            = delete;
    atomic_request_buffer& operator=(const atomic_request_buffer&) = delete;

    atomic_request_buffer(atomic_request_buffer&& other) noexcept
    {
        m_state.store(other.m_state.load(std::memory_order_acquire));
        m_value.store(other.m_value.load(std::memory_order_relaxed));
        other.m_state.store(state::idle, std::memory_order_release);
    }

    atomic_request_buffer& operator=(atomic_request_buffer&& other) noexcept
    {
        if (this != &other) {
            m_state.store(other.m_state.load(std::memory_order_acquire));
            m_value.store(other.m_value.load(std::memory_order_relaxed));
            other.m_state.store(state::idle, std::memory_order_release);
        }
        return *this;
    }

    bool should_request() noexcept
    {
        state expected = state::idle;
        return m_state.compare_exchange_strong(
          expected, state::pending, std::memory_order_acq_rel);
    }

    void fulfill(T data) noexcept
    {
        m_value.store(data, std::memory_order_relaxed);
        m_state.store(state::ready, std::memory_order_release);
    }

    std::optional<T> try_take() noexcept
    {
        if (m_state.load(std::memory_order_acquire) == state::ready) {
            T result = m_value.load(std::memory_order_relaxed);
            m_state.store(state::idle, std::memory_order_release);
            return result;
        }
        return std::nullopt;
    }

    void cancel() noexcept
    {
        m_state.store(state::idle, std::memory_order_release);
    }

private:
    std::atomic<state> m_state{ state::idle };
    std::atomic<T>     m_value{ T{} };
};

/**
 * @brief  Display ComboBox to select a file from a directory.
 *
 * If the @a dir_id identifier is undefined, the function does not display
 * combobox. Only not `.irt` or `.txt` extension file can be selected with this
 * function.
 *
 * @c fs To perform access to the file system.
 * @c dir_id The directory path to search a file.
 * @c file_id The current file selected.
 *
 * @return @a undefined<file_id>() if the @a dir_id is undefined or if the user
 * select nothing otherwise return a valid @a file_path_id.
 */
auto show_data_file_input(const file_access& fs,
                          const dir_path_id  dir_id,
                          const file_path_id file_id) noexcept -> file_path_id;

/** Display combobox and input real, input integer for each type of random
 * distribution. The @c random_source @c src is updated according to user
 * selection.
 * @return True if the user change the distribution type or the parameters.
 */
bool show_random_distribution_input(random_source& src) noexcept;

/** Display combobox and input real, input integer for each type of random
 * distribution. The @c random_source @c src is updated according to user
 * selection.
 * @return True if the user change the distribution type or the parameters.
 */
bool show_random_distribution_input(
  external_source_definition::random_source& src) noexcept;

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

/// Get access to the font icons ttf
std::pair<const void*, int> get_font_icons() noexcept;

enum class file_path_selector_option : u8 {
    none,
    force_dot_extension,
    force_irt_extension
};

/** Display two combox, one per line. The first to select the source type,
 * second to select the source identifier. */
void show_combobox_external_sources(external_source_definition&     srcs,
                                    external_source_definition::id& elem_id,
                                    const char* name = "source") noexcept;

//! @brief Show notification into a classical window in botton.
//!
//! @details It uses the same @c notification structure and copy it into a
//!  large ring-buffer of strings.
class window_logger
{
public:
    constexpr static inline const char* name = "Log";

    window_logger() noexcept = default;

    void show() noexcept;

    bool is_open = true;

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

/** A class to compute 3D projection of 3D points using three rotation
 * matrix.
 *
 * The three rotation matrix are computed using the @c angles vector that can
 * be changed by the user using ImGui widgets or dragging mouse with right
 * button.
 */
class projection_3d
{
public:
    /** Computes the new projection of the @c dots vector using the @c center
     * vector and the three rotation maxtrix @c rot_x, @c rot_y and @c rot_z.
     *
     * @param x The x coordinate of the point to project.
     * @param y The y coordinate of the point to project.
     * @param z The z coordinate of the point to project.
     * @return The new x, y and z coordinates of the projected point.
     */
    auto compute(float x, float y, float z) noexcept -> std::array<float, 3>;

    /** Compute the new x, y and z rotation matrix (repectively @c rot_x, rot_y
     * and rot_z) using the three angle parameters @c rx, @c ry and @c rz.
     *
     * @param angles The vector of three angles (rx, ry, rz) in radian.
     * @param center The center of the object to display.
     */
    auto update_matrices(const std::array<float, 3>& angles,
                         const std::array<float, 3>& center) noexcept -> void;

    /** Get the current angles used to compute the rotation matrix. */
    std::array<float, 3> angles() const noexcept { return m_angles; }

    /** Get the current center of the object to display. */
    std::array<float, 3> center() const noexcept { return m_center; }

private:
    /** Users can changes angles values from -M_PI to +M_PI using the ImGui
     * widgets or dragging mouse withg right button.
     *
     * @attention Attributes can be removed. The angles are in radian.
     */
    std::array<float, 3> m_angles{ 0, 0, 0 };

    /** The center of the object to display. */
    std::array<float, 3> m_center{ 0, 0, 0 };

    std::array<float, 9> m_rot_x{ 0, 0, 0 };
    std::array<float, 9> m_rot_y{ 0, 0, 0 };
    std::array<float, 9> m_rot_z{ 0, 0, 0 };
};

/// Use to display a grid_observation_system into ImGui widget. An instance of
/// this class is available in @c application::simulation_editor::grid_obs.
struct grid_observation_widget {
    /// Display the @c grid_observation_system into ImPlot::PlotHeatmap plot.
    void show(grid_observer& grid, const ImVec2& size) noexcept;
};

// Callback function use into ImPlot::Plot like functions that use ring_buffer
// to read a large buffer instead of a vector.
inline ImPlotPoint ring_buffer_getter(int idx, void* data)
{
    auto* ring  = reinterpret_cast<ring_buffer<observation>*>(data);
    auto  index = ring->index_from_begin(idx);

    return ImPlotPoint{ (*ring)[index].x, (*ring)[index].y };
};

/**
 * @brief The file_selector class display triple-combobox to select a file,
 * create new files or directories.
 */
class file_selector
{
public:
    enum class flag : u8 {
        show_save_button,
        show_load_button,
        show_cancel_button
    };

    file_selector() noexcept = default;

    file_selector(const file_access::full_file_access_result& fs) noexcept
      : reg_id_{ fs.reg_id }
      , dir_id_{ fs.dir_id }
      , file_id_{ fs.file_id }
    {}

    using flags = bitflags<flag>;

    constexpr static inline flags empty_option = flags{};

    struct combo_box_result {
        file_path_id file_id =
          undefined<file_path_id>(); //!< The file selected by the user. If
                                     //!< undefined, the user select nothing or
                                     //!< cancel.
        int close = false; //<! @c true if user click on save or close.
        int save  = false; //<! @c true if user click on save.
    };

    combo_box_result combobox(application&               app,
                              const file_access&         fs,
                              const file_path::file_type type,
                              const flags = empty_option) noexcept;

    combo_box_result combobox_ro(const file_access&         fs,
                                 const file_path::file_type type,
                                 const flags = empty_option) noexcept;

private:
    atomic_request_buffer<dir_path_id>  new_dir_;
    atomic_request_buffer<file_path_id> new_file_;

    registred_path_id reg_id_  = undefined<registred_path_id>();
    dir_path_id       dir_id_  = undefined<dir_path_id>();
    file_path_id      file_id_ = undefined<file_path_id>();

    void combobox_reg(const file_access& fs) noexcept;
    void combobox_dir(application& app, const file_access& fs) noexcept;
    void combobox_dir_ro(const file_access& fs) noexcept;
    void combobox_file(application&               app,
                       const file_access&         fs,
                       const file_path::file_type type) noexcept;
    void combobox_file_ro(const file_access&         fs,
                          const file_path::file_type type) noexcept;
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

    bool is_open = false;

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

    enum class save_option : u8 {
        none,
        copy,
        obs
    } m_need_save = save_option::none;
};

/** A class shared between modeling, simulation and observation layer.
 *
 * The @c graph_editor use a ImGui::BeginChild()/ImGui::EndChild() to display to
 * display the graph.
 */
class graph_editor
{
public:
    constexpr static inline auto selection_buffer_size = 128u;

    enum class show_result_type : u8 {
        none,            //!< No change - No close.
        edited,          //!< Component is changed - Update state.
        request_to_close //!< The window must be closed.
    };

    enum class option : u8 { show_grid, show_help };

    graph_editor() noexcept;

    /** Display the graph component directly into a @c ImGui::BeginChild. */
    void show(application&       app,
              component&         c,
              const component_id c_id,
              graph_component&   g) noexcept;

    /** Display the simulation @c tree-node and @c graph-observer into an
     * @c ImGui::BeginChild window.
     *
     * @return A status if the user close the window, edit the simulation or
     * keep open window.
     */
    show_result_type show(const char*     name,
                          application&    app,
                          project_editor& ed,
                          tree_node&      tn,
                          graph_observer& obs) noexcept;

    /** Display the simulation @c tree_node into a @c ImGui::BeginChild. */
    void show(application& app, project_editor& ed, tree_node& tn) noexcept;

    /** Display the simulation @c tree_node and @c graph_observer into a @c
     * ImGui::BeginChild. */
    void show(application&    app,
              project_editor& ed,
              tree_node&      tn,
              graph_observer& obs) noexcept;

    /** Thread-safe Copy and apply transformation of the graph @c g. A @c job
     * performs the task in a thread-safe way. Do not delete the @c g until the
     * job. */
    void update(application& app, const graph& g) noexcept;

private:
    /** A projection class to build @c data_type::nodes position in 2D from the
     * 3D position of the graph nodes. */
    projection_3d proj;

    /** Top left corner position in canvas. */
    ImVec2 scrolling       = { 0, 0 };
    ImVec2 canvas_sz       = { 0, 0 };
    ImVec2 start_selection = { 0, 0 };
    ImVec2 end_selection   = { 0, 0 };

    struct data_type {
        /** Each node stores the projection in 2D position @c (x,y) from the @c
         * graph node position 3D @c (x,y,z). */
        vector<std::array<float, 2>> nodes;

        std::array<float, 3> top_left;
        std::array<float, 3> bottom_right;
        std::array<float, 3> center;
    };

    shared_buffer<data_type> nodes_locker;

    vector<graph_node_id> selected_nodes;
    vector<graph_edge_id> selected_edges;

    graph_id g = undefined<graph_id>();

    float zoom      = { 1 };
    float grid_step = { 64 };

    bitflags<option> flags;

    bool run_selection = false;
    bool dock_init     = false;

    enum class popup_options : u8 {
        show_graph_modication,
        show_make_project_global_window
    };

    void auto_fit_camera() noexcept;
    void center_camera() noexcept;
    void reset_camera(application& app, const graph& g) noexcept;

    bool initialize_canvas(ImVec2 top_left,
                           ImVec2 bottom_right,
                           ImU32  color) noexcept;
    void draw_grid(ImVec2 top_left, ImVec2 bottom_right, ImU32 color) noexcept;
    void draw_graph(const graph&          g,
                    ImVec2                top_left,
                    ImU32                 color,
                    ImU32                 node_color,
                    ImU32                 edge_color,
                    const graph_observer& obs) noexcept;
    void draw_graph(const graph& g,
                    ImVec2       top_left,
                    ImU32        color,
                    ImU32        node_color,
                    ImU32        edge_color,
                    application& app) noexcept;
    bool draw_popup(application&                  app,
                    graph&                        g,
                    ImVec2                        top_left,
                    const bitflags<popup_options> opt = {}) noexcept;
    bool draw_popup(application&                  app,
                    const graph&                  g,
                    ImVec2                        top_left,
                    const bitflags<popup_options> opt = {}) noexcept;
    void draw_selection(const graph& g,
                        ImVec2       top_left,
                        ImU32        background_selection_color) noexcept;
};

class grid_component_editor_data
{
public:
    grid_component_editor_data(const component_id      id,
                               const grid_component_id grid) noexcept;

    void clear() noexcept;

    bool show(component_editor& ed,
              const component_access&,
              component& compo) noexcept;
    bool show_selected_nodes(component_editor& ed,
                             const component_access&,
                             component& compo) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    ImVec2           distance{ 5.f, 5.f };
    ImVec2           size{ 30.f, 30.f };
    ImVec2           scrolling{ 0.f, 0.f };
    float            zoom[2]{ 1.f, 1.f };
    const component* hovered_component = nullptr;

    int row = 10;
    int col = 10;

    component_id      selected_id = undefined<component_id>();
    grid_component_id grid_id     = undefined<grid_component_id>();

private:
    component_id m_id = undefined<component_id>();

    void read(application& app, component&) noexcept;
    void write(application& app, component&) noexcept;

    grid_component m_grid;
    u64            m_version = std::numeric_limits<u64>::max();
};

class graph_component_editor_data
{
public:
    graph_component_editor_data(const component_id       id,
                                const graph_component_id graph) noexcept;

    void clear() noexcept;

    bool show(component_editor& ed,
              const component_access&,
              component& compo) noexcept;
    bool show_selected_nodes(component_editor& ed,
                             const component_access&,
                             component& compo) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

private:
    file_selector file_select;

    vector<graph_node_id> selected_nodes;
    vector<graph_edge_id> selected_edges;
    ImVec2                start_selection;
    ImVec2                end_selection;
    ImVec2                canvas_sz;
    bool                  run_selection  = false;
    bool                  is_initialized = false;

    ImVec2 distance{ 15.f, 15.f };
    ImVec2 scrolling{ 0.f, 0.f }; //!< top left position in canvas.
    ImVec2 zoom{ 1.f, 1.f };

    std::atomic_flag task_is_running; // Allow only one task (new graph or new
                                      // automatic layout at the same time).

    i32 iteration_limit     = 100'000;
    i32 time_limit_ms       = 1'000;
    i32 update_frequence_ms = 0'16;

    graph_component_id graph_id = undefined<graph_component_id>();
    component_id       m_id     = undefined<component_id>();

    void automatic_layout_task(application& app) noexcept;
    bool compute_automatic_layout(graph_component& graph,
                                  vector<ImVec2>&  displacements,
                                  int              iteration) const noexcept;

    void clear_file_access() noexcept;
    void center_camera() noexcept;
    void auto_fit_camera() noexcept;

    bool show(application&, component&) noexcept;
    bool show_scale_free_menu(application& app) noexcept;
    bool show_small_world_menu(application& app) noexcept;
    bool show_dot_file_menu(application& app) noexcept;
    bool show_graph(application&     app,
                    component&       compo,
                    graph_component& data) noexcept;

private:
    void read(application& app, component&) noexcept;
    void write(application& app, component&) noexcept;

    graph_component m_graph;

    //! m_graph can receives new graph from dot or random graphs.
    request_buffer<graph_component> m_graph_2nd;

    u64 m_version = std::numeric_limits<u64>::max();
};

class simulation_component_editor_data
{
public:
    simulation_component_editor_data(
      const component_id            id,
      const simulation_component_id sid,
      const simulation_component& /*sim*/) noexcept;

    bool show_selected_nodes(component_editor&,
                             const component_access&,
                             component&) noexcept;
    bool show(component_editor&, const component_access&, component&) noexcept;

    component_id            m_id     = undefined<component_id>();
    simulation_component_id m_sim_id = undefined<simulation_component_id>();
    registred_path_id       m_reg    = undefined<registred_path_id>();
    dir_path_id             m_dir    = undefined<dir_path_id>();
    file_path_id            m_file   = undefined<file_path_id>();

private:
    void read(application& app, component&) noexcept;
    void write(application& app, component&) noexcept;

    simulation_component m_sim;
    u64                  m_version = std::numeric_limits<u64>::max();
};

class hsm_component_editor_data
{
public:
    constexpr static inline const char* name = "HSM Editor";

    constexpr static inline auto max_number_of_state =
      hierarchical_state_machine::max_number_of_state;

    hsm_component_editor_data(const component_id     id,
                              const hsm_component_id hid,
                              const hsm_component&   hsm) noexcept;
    ~hsm_component_editor_data() noexcept;

    //! Get the underlying component_id.
    component_id get_id() const noexcept { return m_id; }

    bool show(component_editor& ed,
              const component_access&,
              component& compo) noexcept;
    bool show_selected_nodes(component_editor& ed,
                             const component_access&,
                             component& compo) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

private:
    void show_hsm() noexcept;
    bool show_menu() noexcept;
    bool show_graph() noexcept;
    bool show_panel(component&) noexcept;
    void clear() noexcept;
    bool valid() noexcept;

    void read(application& app, component&) noexcept;
    void write(application& app, component&) noexcept;

    ImNodesEditorContext* m_context = nullptr;

    /** An editable @c hsm_component. */
    hsm_component m_hsm;
    u64           m_version = std::numeric_limits<u64>::max();

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
                                  const component&           compo,
                                  const generic_component_id gid,
                                  const generic_component&   gen) noexcept;
    ~generic_component_editor_data() noexcept;

    bool show(component_editor& ed,
              const component_access&,
              component& compo) noexcept;
    bool show_selected_nodes(component_editor& ed,
                             const component_access&,
                             component& compo) noexcept;
    bool need_show_selected_nodes(component_editor& ed) noexcept;
    void clear_selected_nodes() noexcept;

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

    std::unique_ptr<component>         m_compo_buffer;
    std::unique_ptr<generic_component> m_generic_buffer;

    int   automatic_layout_iteration_limit = 2048;
    float automatic_layout_x_distance      = 350.f;
    float automatic_layout_y_distance      = 350.f;
    float grid_layout_x_distance           = 240.f;
    float grid_layout_y_distance           = 200.f;

    std::bitset<6> options;

private:
    void read(application&, component&) noexcept;
    void write(application&, component&) noexcept;

    generic_component m_generic;
    u64               m_version = std::numeric_limits<u64>::max();
    component_id      m_id      = undefined<component_id>();
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
    void init(application&             app,
              project_editor&          pj_ed,
              const tree_node&         tn,
              const component&         compo,
              const generic_component& gen) noexcept;

    /**
     * Build the @c links and @c nodes vectors from the entire simulation
     * models.
     *
     * This function lock the @c mutex to perform the computation. Use this
     * function in task.
     *
     *  @param app Application to use thread task manager and log.
     */
    void init(application& app, project_editor& pj_ed) noexcept;

    /**
     * Add a rebuild @c nodes and @c links vectors task according to the current
     * @c tree_node.
     *
     * @param app Application to use thread task manager and log.
     */
    void reinit(application& app, project_editor& pj_ed) noexcept;

    bool display(application& app, project_editor& pj_ed) noexcept;

    struct link {
        int out     = 0; /**<  use get_model_output_port/make_output_node_id. */
        int in      = 0; /**< use get_model_input_port/make_input_node_id. */
        int mdl_out = 0; /**< output model in nodes index. */
        int mdl_in  = 0; /**< input model in nodes index. */
    };

    struct node {
        model_id mdl = undefined<model_id>();
        name_str name;
    };

private:
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

    void start_rebuild_task(application& app, project_editor& pj_ed) noexcept;
    void show_nodes(application& app, project_editor& pj_ed) noexcept;
    void show_links() noexcept;
    void compute_automatic_layout() noexcept;
    int  show_menu(application&    app,
                   project_editor& pj_ed,
                   const ImVec2    click_pos) noexcept;

    spin_mutex mutex;
};

class flat_simulation_editor
{
public:
    enum class action : u32 {
        camera_center,
        camera_auto_fit,
        use_grid,
        use_bezier,
    };

    bool display(application& app, project_editor& pj_ed) noexcept;
    void display_status() noexcept;

    void reset() noexcept;

private:
    struct data_type {
        vector<ImVec2> positions; ///< @c (x,y) per simulation models

        vector<ImVec2> tn_rects;   ///< @c (width,height) per tree-node
        vector<ImVec2> tn_centers; ///< @c (x,y) center position per tree-node
        vector<ImVec2> tn_factors; ///< @c (fx,fy) coefficient to adjust pos
        vector<ImU32>  tn_colors;  ///< @c (color) per tree-node

        vector<ImVec2> cache_positions;
    };

    void center_camera() noexcept;
    void auto_fit_camera() noexcept;

    void compute_rects(application&    app,
                       project_editor& pj_ed,
                       data_type&      d) noexcept;
    void rebuild(application& app, project_editor& pj_ed) noexcept;

    shared_buffer<data_type> data;

    ImVec2 distance{ 15.f, 15.f };      /// distance between two nodes
    ImVec2 tn_distance{ 100.f, 100.f }; /// distance between two tree nodes
    ImVec2 scrolling{ 0.f, 0.f };       /// top left position in canvas
    ImVec2 canvas_sz{ 0.f, 0.f };       /// canvas size
    float  zoom = 1.f;

    ImVec2 start_selection;
    ImVec2 end_selection;

    ImVec2 top_left;
    ImVec2 bottom_right;

    vector<model_id> selected_nodes;
    bitflags<action> actions;

    bool run_selection = false;

    std::atomic_flag is_ready = ATOMIC_FLAG_INIT;
};

class grid_simulation_editor
{
public:
    /** Restore the variable to the default value. */
    void reset() noexcept;

    bool display(application&      app,
                 project_editor&   ed,
                 tree_node&        tn,
                 grid_component_id grid_id) noexcept;

    grid_component_id current_id = undefined<grid_component_id>();

    ImVec2 zoom{ 1.f, 1.f };
    ImVec2 scrolling{ 1.f, 1.f };
    ImVec2 size{ 30.f, 30.f };
    ImVec2 distance{ 5.f, 5.f };
};

struct hsm_simulation_editor {
    bool show_observations(application&     app,
                           project_editor&  ed,
                           tree_node&       tn,
                           hsm_component_id hsm_id) noexcept;

    hsm_component_id current_id = undefined<hsm_component_id>();
};

class project_external_source_editor
{
public:
    project_external_source_editor() noexcept;

    ~project_external_source_editor() noexcept;

    void show(application& app, project_editor& pj) noexcept;

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

        std::optional<source_type> type_sel;
        u64                        id_sel = 0;
    } sel;

    /// Copy data into the plot shared buffer.
    void fill_plot(std::span<double> data) noexcept;

private:
    shared_buffer<vector<float>> plot;

    ImPlotContext* context          = nullptr;
    bool           show_file_dialog = false;
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

    project_editor(const std::string_view default_name,
                   project&&              project_) noexcept;

    ~project_editor() noexcept;

    enum class show_result_t {
        success,          /**< Nothing to do. */
        request_to_close, /**< The window must be closed by the called. */
    };

    void set_title_name(const std::string_view name) noexcept;

    /** Display the project window in a @c ImGui::Begin/End window.
     *
     * @return project_editor next status enumeration: keep the window open or
     * try to close it. */
    show_result_t show(application& app) noexcept;

    /** Display the settings and simulation hierarchy in a @c
     * ImGui::BeginTabBar/End widget. */
    void show_settings_and_hierarchy(application& app) noexcept;

    void display_subwindows(application& app) noexcept;
    void close_subwindows(application& app) noexcept;

    void start_simulation_update_state(application& app) noexcept;

    void start_simulation_copy_modeling(application& app) noexcept;
    void start_simulation_init(application& app) noexcept;
    void start_simulation_start(application& app) noexcept;
    void start_simulation_observation(application& app) noexcept;
    void stop_simulation_observation(application& app) noexcept;
    void start_simulation_live_run(application& app) noexcept;
    void start_simulation_static_run(application& app) noexcept;
    void start_simulation_step_by_step(application& app) noexcept;
    void start_simulation_pause(application& app) noexcept;
    void start_simulation_stop(application& app) noexcept;
    void start_simulation_finish(application& app) noexcept;
    void start_simulation_advance(application& app) noexcept;
    void start_simulation_back(application& app) noexcept;

    bool can_edit() const noexcept;
    bool can_display_graph_editor() const noexcept;

    std::atomic_bool force_pause           = false;
    std::atomic_bool force_stop            = false;
    bool             show_minimap          = true;
    bool             store_all_changes     = false;
    bool             real_time             = false;
    bool             have_use_back_advance = false;
    bool             display_graph         = true;

    bool allow_user_changes = false;

    enum class raw_data_type : u8 { none, graph, binary, text };

    raw_data_type save_simulation_raw_data = raw_data_type::none;

    bool is_dock_init = false;

    /** Return true if a simulation is currently running.
     *
     * A simulation is running if and only if the simulation status is in state:
     * running, running_required or paused.
     */
    bool is_simulation_running() const noexcept;

    // timeline tl;

    /// Default stores 128 snapshots.
    ///
    /// @TODO Adds this parameters in global file settings.
    simulation_snapshot_handler snaps{ 128 };

    /// The index of the current selected
    int current_snap = -1;

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

    std::atomic<simulation_status> simulation_state =
      simulation_status::not_started;

    ImPlotContext* output_context = nullptr;

    /** Number of column in the tree node observation. */
    using tree_node_observation_t = constrained_value<int, 1, 100>;
    tree_node_observation_t tree_node_observation{ 1 };
    float                   tree_node_observation_height = 300.f;

    circular_buffer<command, 256> commands;

    project pj;

    //! Select a @c tree_node node in the modeling tree node. The existence of
    //! the underlying component is tested before assignment.
    void select(application& app, tree_node_id id) noexcept;

    //! @return true if @c id is the selected @c tree_node.
    bool is_selected(tree_node_id id) const noexcept;

    tree_node_id selected_tn() noexcept;

    tree_node_id m_selected_tree_node = undefined<tree_node_id>();

    /// For each simulation, and according to the @a save_simulation_raw_data,
    /// an output stream to store all model state during simulation.
    file raw_ofs;

    /** A local @c graph_editor to display the simulation graph component editor
     * (without observation). */
    graph_editor graph_ed;

    struct visualisation_editor {
        graph_editor_id   graph_ed_id  = undefined<graph_editor_id>();
        tree_node_id      tn_id        = undefined<tree_node_id>();
        graph_observer_id graph_obs_id = undefined<graph_observer_id>();
    };

    data_array<graph_editor, graph_editor_id> graph_eds;
    vector<visualisation_editor>              visualisation_eds;

    small_string<64> title;

    std::atomic_flag save_in_progress;
    file_selector    file_select;
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
    constexpr static inline const char* name    = "Component editor";
    constexpr static inline unsigned    max_tab = 32;

    enum class show_result_t {
        success,          ///< Nothing to do.
        request_to_close, ///< The window must be closed by the called.
    };

    component_editor() noexcept;

    /// Draw openend component into ImGui windows
    void display() noexcept;

    void request_to_open(const component_id id) noexcept;
    bool need_to_open(const component_id id) const noexcept;
    void clear_request_to_open() noexcept;

    /// Force to close the opened component @c id.
    void close(const component_id id) noexcept;

    void add_generic_component_data() noexcept;
    void add_grid_component_data() noexcept;
    void add_graph_component_data() noexcept;
    void add_hsm_component_data() noexcept;
    void add_simulation_component_data() noexcept;

    bool is_component_open(const component_id id) const noexcept;

    template<typename IdentifierType>
    auto try_to_get(const IdentifierType id) noexcept;

    template<typename T>
    auto get_id(const T& elem) const noexcept;

    enum class tab_id : u32;

    struct tab {
        tab(const component_id&                         compo_id,
            const component_type                        compo_type,
            const file_access::full_file_access_result& fs) noexcept
          : id{ compo_id }
          , type{ compo_type }
          , file_select{ fs }
        {}

        component_id        id;
        component_type      type;
        component           compo;
        component_file_path file;
        description_str     desc;

        file_selector file_select;

        // We used following attributes to define new input or output
        // connections for all type of components.

        generic_component::input_connection  input_gen_con;
        generic_component::output_connection output_gen_con;
        grid_component::input_connection     input_grid_con;
        grid_component::output_connection    output_grid_con;
        graph_component::input_connection    input_graph_con;
        graph_component::output_connection   output_graph_con;
        connection_pack                      input_pack_con;
        connection_pack                      output_pack_con;

        u64 version = std::numeric_limits<u64>::max();

        /// Stores the list of type of component_id children (used by
        /// connection_pack).
        shared_buffer<vector<component_id>> uniq_component_children;

        union {
            grid_editor_data_id       grid;
            graph_editor_data_id      graph;
            generic_editor_data_id    generic;
            hsm_editor_data_id        hsm;
            simulation_editor_data_id sim;
        } data;

        bool is_dock_init = false;
    };

    enum { tabitem_open_save, tabitem_open_in_out };

    /// List of component and tabulation opened in the @a ImGui::BeginTabBar.
    data_array<tab, tab_id> tabs;

    std::bitset<2> tabitem_open;
    component_id   m_request_to_open = undefined<component_id>();

    void          display_tabs() noexcept;
    show_result_t display_tab_content(tab& t) noexcept;
};

class library_window
{
public:
    constexpr static inline const char* name                = "Library";
    constexpr static inline auto        open_file_list_size = 64;

    union file_target {
        project_id   pj_id = undefined<project_id>();
        component_id compo_id;
        graph_id     g_id;
    };

    library_window() noexcept
      : stack(max_component_stack_size, reserve_tag)
      , open_file_list(open_file_list_size, reserve_tag)
    {}

    enum class is_component_deletable_t : u8 {
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

    void add_open_file(const file_path_id, const project_id) noexcept;
    void add_open_file(const file_path_id, const component_id) noexcept;
    void add_open_file(const file_path_id, const graph_id) noexcept;
    std::optional<file_target> get_open_file(const file_path_id) noexcept;
    void                       close_open_file(const file_path_id) noexcept;

    bool is_open = true;

private:
    vector<tree_node*> stack;

    /// Stores the list of opened files.
    table<file_path_id, file_target> open_file_list;

    enum class file_type : u8 {
        none,
        component,
        project,
        txt,
        data,
        dot,
    };

    bitflags<file_type> flags = bitflags<file_type>(file_type::component,
                                                    file_type::project,
                                                    file_type::txt,
                                                    file_type::data,
                                                    file_type::dot);

    is_component_deletable_t is_component_deletable(
      const application&      app,
      const component_access& ids,
      const component_id      id) noexcept;

    void show_menu() noexcept;
    void show_file_treeview(const file_access&,
                            const component_access&,
                            const bitflags<file_type>) noexcept;
    void show_repertories_content(const file_access&,
                                  const component_access&,
                                  const bitflags<file_type>) noexcept;
    void show_dirpath_content(const file_access&,
                              const component_access&,
                              const dir_path&,
                              const bitflags<file_type>) noexcept;
    void show_notsaved_content(const file_access&,
                               const component_access&,
                               const bitflags<file_type>) noexcept;
    void show_file_component(const file_access&,
                             const component_access&,
                             const component_id) noexcept;
    void show_file_project(const file_access&,
                           const component_access&,
                           const file_path&,
                           const file_path_id) noexcept;
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

    /// Update the components cache with added/renamed/removed component.
    /// This function run the update process in the current thread.
    void update() noexcept;

    /// Update the components cache with added/renamed/removed component.
    /// The function run the update in a @c task from the gui @c task_list.
    void task_update() noexcept;

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
        data_type() noexcept = default;

        vector<std::pair<component_id, name_str>>      by_names;
        vector<std::pair<component_id, file_path_str>> by_files;

        vector<std::pair<component_id, name_str>> by_generics;
        vector<std::pair<component_id, name_str>> by_grids;
        vector<std::pair<component_id, name_str>> by_graphs;
        vector<std::pair<component_id, name_str>> by_hsms;
        vector<std::pair<component_id, name_str>> by_sims;

        void clear() noexcept;
        void update(const modeling& mod) noexcept;

        u64 ids_version = std::numeric_limits<u64>::max();
    };

    shared_buffer<data_type> data;
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

    /** Display a @c ImGui::ComboBox and a @c ImGui::TreeNode for the hierarchy
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

    /** Update the components cache with component. */
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

    shared_buffer<data_type> data;

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

struct global_simulation_window {
    project_id        pj_id        = undefined<project_id>();
    tree_node_id      tn_id        = undefined<tree_node_id>();
    graph_editor_id   graph_ed_id  = undefined<graph_editor_id>();
    graph_observer_id graph_obs_id = undefined<graph_observer_id>();
};

class application
{
public:
    explicit application(journal_handler& jn) noexcept;

    ~application() noexcept;

private:
    task_manager task_mgr;

public:
    ImFont* icons = nullptr;

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

    component_editor component_ed;
    output_editor    output_ed;

    file_dialog f_dialog;

    settings_window settings_wnd;
    library_window  library_wnd;
    memory_window   memory_wnd;
    window_logger   log_wnd;
    task_window     task_wnd;

    simulation_to_cpp sim_to_cpp;

    data_array<grid_component_editor_data, grid_editor_data_id>       grids;
    data_array<graph_component_editor_data, graph_editor_data_id>     graphs;
    data_array<generic_component_editor_data, generic_editor_data_id> generics;
    data_array<hsm_component_editor_data, hsm_editor_data_id>         hsms;
    data_array<simulation_component_editor_data, simulation_editor_data_id>
      sims;

    data_array<graph_editor, graph_editor_id> graph_eds;

    data_array<plot_copy, plot_copy_id> copy_obs;

    /** Stores the list of opened simulation windows. */
    vector<global_simulation_window> sim_wnds;

    plot_observation_widget plot_obs;
    grid_observation_widget grid_obs;

    plot_copy_widget plot_copy_wgt;

    flat_simulation_editor         flat_sim;
    generic_simulation_editor      generic_sim;
    grid_simulation_editor         grid_sim;
    hsm_simulation_editor          hsm_sim;
    project_external_source_editor data_ed;

    request_buffer<std::unique_ptr<project>> new_project_req;

    void request_alloc_project_editor(std::unique_ptr<project> pj) noexcept
    {
        if (new_project_req.should_request())
            new_project_req.fulfill(std::move(pj));
    }

    /// Open a new project window iff component @c id can be set as a project.
    void try_set_component_as_project(const file_access&      files,
                                      const component_access& ids,
                                      const component_id      id) noexcept;

    /// Open a new project window according to the @a file_id
    void try_open_project_window(const file_access&      files,
                                 const component_access& ids,
                                 const file_path_id      file_id) noexcept;

    /// Close a project window according to the @a project_id but keep the
    /// correspondiing @c project_editor::file @c file_path_id.
    void close_project_window(const project_id project_id) noexcept;

    /// Free the @a project according to the @a project_id
    void free_project_window(const project_id id) noexcept;

    bool          init() noexcept;
    show_result_t show() noexcept;

    /**
     * Helpers function to add a @c simulation_task into an @c
     * ordered_task_list. The task list used depends on the @c get_index or
     * the
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
     * Helpers function to add a @c gui_task into the @c ordered_task_list
     * gui. Task is added at tail of the @c ring_buffer and ensure linear
     * operation.
     *
     * @param fn The function to run.
     */
    template<typename Fn>
    void add_gui_task(Fn&& fn) noexcept;

    /**
     * Helpers function to get the current instance of the @a
     * unordered_task_list.
     *
     * @return The reference to the @a unordered_task_list
     */
    unordered_task_list& get_unordered_task_list() noexcept;

    /// Allocate a new @c project_editor and read the file @c file_path_id.
    void start_load_project(const project_id f_id) noexcept;

    /// Save the project @c pj_id.
    void start_save_project(const project_id pj_id) noexcept;
    void start_save_component(const component_id id) noexcept;
    void start_init_source(const project_id  pj_id,
                           const u64         id,
                           const source_type type) noexcept;
    void start_hsm_test_start() noexcept;
    void start_dir_path_refresh(const dir_path_id id) noexcept;

    unsigned int get_main_dock_id() const noexcept { return main_dock_id; }

    void request_open_directory_dlg(const registred_path_id id) noexcept;

    atomic_request_buffer<file_path_id> new_file;
    atomic_request_buffer<dir_path_id>  new_dir;

private:
    friend task_window;

    unsigned int main_dock_id;
    unsigned int right_dock_id;
    unsigned int bottom_dock_id;

    registred_path_id selected_reg_path    = undefined<registred_path_id>();
    bool              show_select_reg_path = false;

    bool show_welcome_wnd = true;
    bool show_imgui_demo  = false;
    bool show_implot_demo = false;

    show_result_t show_menu() noexcept;
    void          show_dock() noexcept;
    void          show_select_directory_dlg() noexcept;

    u64 m_journal_timestep;
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

bool show_local_observers(application&            app,
                          project_editor&         ed,
                          const component_access& ids,
                          tree_node&              tn,
                          const component&        compo,
                          const graph_component&  graph) noexcept;

void alloc_grid_observer(irt::application& app, irt::tree_node& tn);

bool show_local_observers(application&            app,
                          project_editor&         ed,
                          const component_access& ids,
                          tree_node&              tn,
                          const component&        compo,
                          const grid_component&   grid) noexcept;

//! @brief Get the file path of the @c imgui.ini file saved in $HOME.
//! @return A pointer to a newly allocated memory.
std::filesystem::path get_imgui_filename() noexcept;

template<typename Fn>
void application::add_simulation_task(const project_id id, Fn&& fn) noexcept
{
    const auto index = get_index(id) % 3;

    task_mgr.ordered(index).add(std::forward<Fn>(fn));
}

template<typename Fn>
void application::add_gui_task(Fn&& fn) noexcept
{
    task_mgr.ordered(ordinal(main_task::gui)).add(std::forward<Fn>(fn));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

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

template<typename IdentifierType>
auto component_editor::try_to_get(const IdentifierType id) noexcept
{
    auto& app = container_of(this, &application::component_ed);

    if constexpr (std::is_same_v<IdentifierType, grid_editor_data_id>)
        return app.grids.try_to_get(id);
    if constexpr (std::is_same_v<IdentifierType, graph_editor_data_id>)
        return app.graphs.try_to_get(id);
    if constexpr (std::is_same_v<IdentifierType, generic_editor_data_id>)
        return app.generics.try_to_get(id);
    if constexpr (std::is_same_v<IdentifierType, hsm_editor_data_id>)
        return app.hsms.try_to_get(id);
    if constexpr (std::is_same_v<IdentifierType, simulation_editor_data_id>)
        return app.sims.try_to_get(id);
}

template<typename T>
auto component_editor::get_id(const T& elem) const noexcept
{
    auto& app = container_of(this, &application::component_ed);

    if constexpr (std::is_same_v<T, grid_component_editor_data>)
        return app.grids.get_id(elem);
    if constexpr (std::is_same_v<T, graph_component_editor_data>)
        return app.graphs.get_id(elem);
    if constexpr (std::is_same_v<T, generic_component_editor_data>)
        return app.generics.get_id(elem);
    if constexpr (std::is_same_v<T, hsm_component_editor_data>)
        return app.hsms.get_id(elem);
    if constexpr (std::is_same_v<T, simulation_component_editor_data>)
        return app.sims.get_id(elem);
}

} // namespace irt

#endif
