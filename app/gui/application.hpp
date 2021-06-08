// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021
#define ORG_VLEPROJECT_IRRITATOR_APP_APPLICATION_2021

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>

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
struct cluster;
struct editor;
struct top_cluster;
struct window_logger;
struct plot_output;
struct file_output;
struct file_discrete_output;

static inline constexpr int not_found = -1;

enum class editor_id : u64;
enum class cluster_id : u64;
enum class plot_output_id : u64;
enum class file_output_id : u64;
enum class file_discrete_output_id : u64;

using child_id = std::variant<model_id, cluster_id>;

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

    void operator()(const irt::observer& obs,
                    const irt::dynamics_type /*type*/,
                    const irt::time tl,
                    const irt::time t,
                    const irt::observer::status s);

    editor* ed = nullptr;
    std::vector<float> xs;
    std::vector<float> ys;
    small_string<24u> name;
    double tl = 0.0;
    double time_step = 0.01;
};

struct file_output
{
    file_output() = default;

    file_output(std::string_view name_)
      : name(name_)
    {}

    void operator()(const irt::observer& obs,
                    const irt::dynamics_type type,
                    const irt::time tl,
                    const irt::time t,
                    const irt::observer::status s);

    editor* ed = nullptr;
    std::ofstream ofs;
    small_string<24u> name;
};

struct file_discrete_output
{
    file_discrete_output() = default;

    file_discrete_output(std::string_view name_)
      : name(name_)
    {}

    void operator()(const irt::observer& obs,
                    const irt::dynamics_type type,
                    const irt::time tl,
                    const irt::time t,
                    const irt::observer::status s);

    editor* ed = nullptr;
    std::ofstream ofs;
    small_string<24u> name;
    double tl = 0.0;
    double time_step = 0.01;
};

struct cluster
{
    cluster() = default;

    small_string<16> name;
    std::vector<child_id> children;
    std::vector<int> input_ports;
    std::vector<int> output_ports;

    int get(const child_id id) const noexcept
    {
        auto it = std::find(std::begin(children), std::end(children), id);
        if (it == std::end(children))
            return not_found;

        return static_cast<int>(std::distance(std::begin(children), it));
    }
};

struct top_cluster
{
    std::vector<std::pair<child_id, int>> children;
    int next_node_id = 0;

    static inline constexpr int not_found = -1;

    status init(size_t models) noexcept
    {
        try {
            children.reserve(models);
        } catch (const std::bad_alloc&) {
            std::vector<std::pair<child_id, int>>().swap(children);
            irt_bad_return(status::gui_not_enough_memory);
        }

        return status::success;
    }

    int get_index(const child_id id) const noexcept
    {
        for (int i = 0, e = length(children); i != e; ++i)
            if (children[i].first == id)
                return i;

        return not_found;
    }

    int get_index(const int node) const noexcept
    {
        for (int i = 0, e = length(children); i != e; ++i)
            if (children[i].second == node)
                return i;

        return not_found;
    }

    void clear() noexcept
    {
        children.clear();
    }

    void pop(const int index) noexcept
    {
        std::swap(children[index], children.back());
        children.pop_back();
    }

    int emplace_back(const child_id id)
    {
        int ret = next_node_id++;

        children.emplace_back(id, ret);

        return ret;
    }
};

int
make_input_node_id(const irt::model_id mdl, const int port) noexcept;

int
make_output_node_id(const irt::model_id mdl, const int port) noexcept;

std::pair<irt::u32, irt::u32>
get_model_input_port(const int node_id) noexcept;

std::pair<irt::u32, irt::u32>
get_model_output_port(const int node_id) noexcept;

struct editor
{
    small_string<16> name;
    std::filesystem::path path;
    ImNodesEditorContext* context = nullptr;
    bool show = true;

    simulation sim;
    external_source srcs;

    double simulation_begin = 0.0;
    double simulation_end = 10.0;
    double simulation_current = 10.0;
    double simulation_next_time = 0.0;
    long simulation_bag_id = 0;
    int step_by_step_bag = 0;

    double simulation_during_date;
    int simulation_during_bag;

    editor_status st = editor_status::editing;
    status sim_st = status::success;

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
    bool stop = false;

    data_array<plot_output, plot_output_id> plot_outs;
    data_array<file_output, file_output_id> file_outs;
    data_array<file_discrete_output, file_discrete_output_id>
      file_discrete_outs;
    std::vector<observation_output> observation_outputs;

    void show_sources_window(bool* is_show);
    void show_menu_sources(const char* title, source& src);

    template<typename Function, typename... Args>
    constexpr void observation_dispatch(const u32 index,
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

    data_array<cluster, cluster_id> clusters;
    std::vector<cluster_id> clusters_mapper; /* group per cluster_id */
    std::vector<cluster_id> models_mapper;   /* group per model_id */

    std::vector<bool> models_make_transition;

    ImVector<ImVec2> positions;
    ImVector<ImVec2> displacements;

    bool use_real_time;
    bool starting = true;
    float synchronize_timestep = 1.f;

    top_cluster top;

    std::string tooltip;

    bool show_load_file_dialog = false;
    bool show_save_file_dialog = false;
    bool show_select_directory_dialog = false;
    bool show_settings = false;
    bool show_sources = false;

    struct settings_manager
    {
        int kernel_model_cache = 1024;
        int kernel_message_cache = 32768;
        int gui_node_cache = 1024;
        ImVec4 gui_model_color{ .27f, .27f, .54f, 1.f };
        ImVec4 gui_model_transition_color{ .27f, .54f, .54f, 1.f };
        ImVec4 gui_cluster_color{ .27f, .54f, .27f, 1.f };

        ImU32 gui_hovered_model_color;
        ImU32 gui_selected_model_color;
        ImU32 gui_hovered_model_transition_color;
        ImU32 gui_selected_model_transition_color;
        ImU32 gui_hovered_cluster_color;
        ImU32 gui_selected_cluster_color;

        int automatic_layout_iteration_limit = 200;
        float automatic_layout_x_distance = 350.f;
        float automatic_layout_y_distance = 350.f;
        float grid_layout_x_distance = 250.f;
        float grid_layout_y_distance = 250.f;

        bool show_dynamics_inputs_in_editor = false;

        void compute_colors() noexcept;
        void show(bool* is_open);

    } settings;

    status initialize(u32 id) noexcept;
    void clear() noexcept;

    void group(const ImVector<int>& nodes) noexcept;
    void ungroup(const int node) noexcept;
    void free_group(cluster& group) noexcept;
    void free_children(const ImVector<int>& nodes) noexcept;
    status copy(const ImVector<int>& nodes) noexcept;

    void compute_grid_layout() noexcept;
    void compute_automatic_layout() noexcept;

    bool is_in_hierarchy(const cluster& group,
                         const cluster_id group_to_search) const noexcept;
    cluster_id ancestor(const child_id child) const noexcept;
    int get_top_group_ref(const child_id child) const noexcept;

    cluster_id parent(cluster_id child) const noexcept
    {
        return clusters_mapper[get_index(child)];
    }

    cluster_id parent(model_id child) const noexcept
    {
        return models_mapper[get_index(child)];
    }

    void parent(const cluster_id child, const cluster_id parent) noexcept
    {
        clusters_mapper[get_index(child)] = parent;
    }

    void parent(const model_id child, const cluster_id parent) noexcept
    {
        models_mapper[get_index(child)] = parent;
    }

    struct gport
    {
        gport() noexcept = default;

        gport(irt::model* model_, const int port_index_) noexcept
          : model(model_)
          , port_index(port_index_)
        {}

        irt::model* model = nullptr;
        int port_index = 0;
    };

    gport get_in(const int index) noexcept
    {
        const auto model_index_port = get_model_input_port(index);
        auto* mdl = sim.models.try_to_get(model_index_port.first);

        return { mdl, static_cast<int>(model_index_port.second) };
    }

    gport get_out(const int index) noexcept
    {
        const auto model_index_port = get_model_output_port(index);
        auto* mdl = sim.models.try_to_get(model_index_port.first);

        return { mdl, static_cast<int>(model_index_port.second) };
    }

    status add_lotka_volterra() noexcept;
    status add_izhikevitch() noexcept;

    void show_connections() noexcept;
    void show_model_dynamics(model& mdl) noexcept;
    void show_model_cluster(cluster& mdl) noexcept;
    void show_top() noexcept;

    bool show_editor() noexcept;
};

struct window_logger
{
    ImGuiTextBuffer buffer;
    ImGuiTextFilter filter;
    ImVector<int> line_offsets;

    bool auto_scroll = true;
    bool scroll_to_bottom = false;
    window_logger() = default;
    void clear() noexcept;

    void log(const int level, const char* fmt, ...) IM_FMTARGS(3);
    void log(const int level, const char* fmt, va_list args) IM_FMTLIST(3);
    void show(bool* is_show);
};

const char*
log_string(const log_status s) noexcept;

struct application
{
    data_array<editor, editor_id> editors;
    std::filesystem::path home_dir;
    std::filesystem::path executable_dir;
    std::vector<long long int> simulation_duration;

    bool show_log = true;
    bool show_simulation = true;
    bool show_demo = false;
    bool show_plot = true;
    bool show_settings = false;

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
    void free_editor(editor& ed);

    editor* make_combo_editor_name(editor_id& id) noexcept;
};

static inline window_logger log_w; // @todo remove this variable.

} // namespace irt

#endif
