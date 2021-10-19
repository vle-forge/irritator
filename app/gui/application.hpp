// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <variant>
#include <vector>

#include "imnodes.h"
#include "implot.h"
#include <imgui.h>

namespace irt {

// Forward declaration
struct application;
struct editor;
struct window_logger;
struct plot_output;
struct file_output;
struct file_discrete_output;
struct component_editor;

static inline constexpr int not_found = -1;

enum class editor_id : u64;
enum class plot_output_id : u64;
enum class file_output_id : u64;
enum class file_discrete_output_id : u64;
enum class component_editor_id : u64;

using observation_output = std::variant<std::monostate,
                                        plot_output_id,
                                        file_output_id,
                                        file_discrete_output_id>;

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

enum class editor_status
{
    editing,          // Default mode. No simulation run.
    running,          //
    running_1_step,   //
    running_10_step,  //
    running_100_step, //
    running_pause,    //
    running_stop,     //
    finalizing        // cleanup internal simulation data.
};

struct plot_output
{
    plot_output() = default;

    plot_output(std::string_view name_)
      : name(name_)
    {}

    editor*            ed = nullptr;
    std::vector<float> xs;
    std::vector<float> ys;
    small_string<24u>  name;
    real               tl        = zero;
    real               time_step = to_real(0.01);
};

void plot_output_callback(const irt::observer& obs,
                          const irt::dynamics_type /*type*/,
                          const irt::time             tl,
                          const irt::time             t,
                          const irt::observer::status s);

struct file_output
{
    file_output() = default;

    file_output(std::string_view name_)
      : name(name_)
    {}

    editor*           ed = nullptr;
    std::ofstream     ofs;
    small_string<24u> name;
};

void file_output_callback(const irt::observer&        obs,
                          const irt::dynamics_type    type,
                          const irt::time             tl,
                          const irt::time             t,
                          const irt::observer::status s);

struct file_discrete_output
{
    file_discrete_output() = default;

    file_discrete_output(std::string_view name_)
      : name(name_)
    {}

    editor*           ed = nullptr;
    std::ofstream     ofs;
    small_string<24u> name;
    real              tl        = irt::real(0);
    real              time_step = irt::real(0.01);
};

void file_discrete_output_callback(const irt::observer&        obs,
                                   const irt::dynamics_type    type,
                                   const irt::time             tl,
                                   const irt::time             t,
                                   const irt::observer::status s);

int make_input_node_id(const irt::model_id mdl, const int port) noexcept;

int make_output_node_id(const irt::model_id mdl, const int port) noexcept;

std::pair<irt::u32, irt::u32> get_model_input_port(const int node_id) noexcept;

std::pair<irt::u32, irt::u32> get_model_output_port(const int node_id) noexcept;

struct editor
{
    small_string<16>      name;
    std::filesystem::path path;
    ImNodesEditorContext* context      = nullptr;
    bool                  show         = true;
    bool                  show_minimap = true;

    simulation      sim;
    external_source srcs;

    irt::real simulation_begin     = 0;
    irt::real simulation_end       = 10;
    irt::real simulation_current   = 10;
    irt::real simulation_next_time = 0;
    long      simulation_bag_id    = 0;
    int       step_by_step_bag     = 0;

    real simulation_during_date;
    int  simulation_during_bag;

    editor_status st     = editor_status::editing;
    status        sim_st = status::success;

    editor() noexcept;
    ~editor() noexcept;

    bool is_running() const noexcept
    {
        return match(st,
                     editor_status::running,
                     editor_status::running_1_step,
                     editor_status::running_10_step,
                     editor_status::running_100_step);
    }

    bool simulation_show_value = false;
    bool stop                  = false;

    data_array<plot_output, plot_output_id> plot_outs;
    data_array<file_output, file_output_id> file_outs;
    data_array<file_discrete_output, file_discrete_output_id>
                                    file_discrete_outs;
    std::vector<observation_output> observation_outputs;

    template<typename Function, typename... Args>
    constexpr void observation_dispatch(const u32  index,
                                        Function&& f,
                                        Args... args) noexcept
    {
        switch (observation_outputs[index].index()) {
        case 1:
            f(plot_outs,
              std::get<plot_output_id>(observation_outputs[index]),
              args...);
            break;

        case 2:
            f(file_outs,
              std::get<file_output_id>(observation_outputs[index]),
              args...);
            break;

        case 3:
            f(file_discrete_outs,
              std::get<file_discrete_output_id>(observation_outputs[index]),
              args...);
            break;

        default:
            break;
        }
    }

    void observation_outputs_free(const u32 index) noexcept
    {
        observation_dispatch(
          index, [](auto& outs, auto out_id) { outs.free(out_id); });

        observation_outputs[index] = std::monostate{};
    }

    std::filesystem::path observation_directory;

    std::vector<bool> models_make_transition;

    ImVector<ImVec2> positions;
    ImVector<ImVec2> displacements;

    bool  use_real_time;
    bool  starting             = true;
    float synchronize_timestep = 1.f;

    std::string tooltip;

    bool show_load_file_dialog        = false;
    bool show_save_file_dialog        = false;
    bool show_select_directory_dialog = false;
    bool show_settings                = false;

    struct settings_manager
    {
        int    kernel_model_cache   = 1024;
        int    kernel_message_cache = 32768;
        int    gui_node_cache       = 1024;
        ImVec4 gui_model_color{ .27f, .27f, .54f, 1.f };
        ImVec4 gui_model_transition_color{ .27f, .54f, .54f, 1.f };

        ImU32 gui_hovered_model_color;
        ImU32 gui_selected_model_color;
        ImU32 gui_hovered_component_color;
        ImU32 gui_selected_component_color;
        ImU32 gui_hovered_model_transition_color;
        ImU32 gui_selected_model_transition_color;

        int   automatic_layout_iteration_limit = 200;
        float automatic_layout_x_distance      = 350.f;
        float automatic_layout_y_distance      = 350.f;
        float grid_layout_x_distance           = 250.f;
        float grid_layout_y_distance           = 250.f;

        bool show_dynamics_inputs_in_editor = false;

        void compute_colors() noexcept;
        void show(bool* is_open);
    } settings;

    status initialize(u32 id) noexcept;
    void   clear() noexcept;

    void   free_children(const ImVector<int>& nodes) noexcept;
    status copy(const ImVector<int>& nodes) noexcept;

    void compute_grid_layout() noexcept;
    void compute_automatic_layout() noexcept;

    struct gport
    {
        gport() noexcept = default;

        gport(irt::model* model_, const int port_index_) noexcept
          : model(model_)
          , port_index(port_index_)
        {}

        irt::model* model      = nullptr;
        int         port_index = 0;
    };

    gport get_in(const int index) noexcept
    {
        const auto model_index_port = get_model_input_port(index);
        auto*      mdl = sim.models.try_to_get(model_index_port.first);

        return { mdl, static_cast<int>(model_index_port.second) };
    }

    gport get_out(const int index) noexcept
    {
        const auto model_index_port = get_model_output_port(index);
        auto*      mdl = sim.models.try_to_get(model_index_port.first);

        return { mdl, static_cast<int>(model_index_port.second) };
    }

    status add_lotka_volterra() noexcept;
    status add_izhikevitch() noexcept;

    void show_connections() noexcept;
    void show_model_dynamics(model& mdl) noexcept;
    void show_top() noexcept;
    void show_sources() noexcept;
    void show_editor() noexcept;
    void show_menu_sources(const char* title, source& src);

    bool show_window() noexcept;
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
    void show(bool* is_show);
};

const char* log_string(const log_status s) noexcept;

enum class component_editor_status
{
    modeling,
    simulating
};

struct component_editor
{
    struct settings_manager
    {
        ImVec4 gui_model_color{ .27f, .27f, .54f, 1.f };
        ImVec4 gui_component_color{ .54f, .27f, .27f, 1.f };
        ImVec4 gui_model_transition_color{ .27f, .54f, .54f, 1.f };
        ImU32  gui_hovered_model_color;
        ImU32  gui_selected_model_color;
        ImU32  gui_hovered_component_color;
        ImU32  gui_selected_component_color;
        ImU32  gui_hovered_model_transition_color;
        ImU32  gui_selected_model_transition_color;

        int   automatic_layout_iteration_limit = 200;
        float automatic_layout_x_distance      = 350.f;
        float automatic_layout_y_distance      = 350.f;
        float grid_layout_x_distance           = 250.f;
        float grid_layout_y_distance           = 250.f;

        bool show_dynamics_inputs_in_editor = false;

        void show(bool* is_open) noexcept;
    };

    settings_manager        settings;
    small_string<16>        name;
    modeling                mod;
    simulation              sim;
    tree_node_id            selected_component  = undefined<tree_node_id>();
    ImNodesEditorContext*   context             = nullptr;
    bool                    is_saved            = true;
    bool                    show_minimap        = true;
    bool                    show_settings       = false;
    bool                    show_memory         = false;
    bool                    force_node_position = false;
    component_editor_status status = component_editor_status::modeling;

    component*     selected_component_list      = nullptr;
    component_type selected_component_type_list = component_type::file;

    vector<int> selected_links;
    vector<int> selected_nodes;

    real simulation_begin;
    real simulation_end;

    void select(tree_node_id id) noexcept;
    void open_as_main(component_id id) noexcept;
    void unselect() noexcept;

    void init() noexcept;
    void show(bool* is_show) noexcept;
    void show_memory_box(bool* is_show) noexcept;
    void shutdown() noexcept;
};

struct application
{
    component_editor              c_editor;
    data_array<editor, editor_id> editors;
    std::filesystem::path         home_dir;
    std::filesystem::path         executable_dir;
    std::vector<long long int>    simulation_duration;

    bool show_modeling   = true;
    bool show_log        = false;
    bool show_simulation = false;
    bool show_demo       = false;
    bool show_plot       = false;
    bool show_settings   = false;

    bool init();
    bool show();
    void shutdown();

    // For each editor run the simulation. Use this function outside of the
    // ImGui::Render/NewFrame to not block the graphical user interface.
    void run_simulations();

    void show_plot_window();
    void show_simulation_window();
    void show_settings_window();

    editor* alloc_editor();
    void    free_editor(editor& ed);

    editor* make_combo_editor_name(editor_id& id) noexcept;
};

static inline window_logger log_w; // @todo remove this variable.

} // namespace irt

#endif
