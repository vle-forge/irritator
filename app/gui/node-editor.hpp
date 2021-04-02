// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2020

#include <irritator/core.hpp>

#include <filesystem>
#include <fstream>
#include <thread>
#include <variant>
#include <vector>

#include "imnodes.hpp"
#include <imgui.h>

namespace irt {

inline const char*
status_string(const status s) noexcept
{
    static const char* str[] = {
        "success",
        "unknown_dynamics",
        "block_allocator_bad_capacity",
        "block_allocator_not_enough_memory",
        "head_allocator_bad_capacity",
        "head_allocator_not_enough_memory",
        "simulation_not_enough_model",
        "simulation_not_enough_memory_message_list_allocator",
        "simulation_not_enough_memory_input_port_list_allocator",
        "simulation_not_enough_memory_output_port_list_allocator",
        "data_array_init_capacity_error",
        "data_array_not_enough_memory",
        "data_array_archive_init_capacity_error",
        "data_array_archive_not_enough_memory",
        "array_init_capacity_zero",
        "array_init_capacity_too_big",
        "array_init_not_enough_memory",
        "vector_init_capacity_zero",
        "vector_init_capacity_too_big",
        "vector_init_not_enough_memory",
        "dynamics_unknown_id",
        "dynamics_unknown_port_id",
        "dynamics_not_enough_memory",
        "model_connect_output_port_unknown",
        "model_connect_input_port_unknown",
        "model_connect_already_exist",
        "model_connect_bad_dynamics",
        "model_integrator_dq_error",
        "model_integrator_X_error",
        "model_integrator_internal_error",
        "model_integrator_output_error",
        "model_integrator_running_without_x_dot",
        "model_integrator_ta_with_bad_x_dot",
        "model_quantifier_bad_quantum_parameter",
        "model_quantifier_bad_archive_length_parameter",
        "model_quantifier_shifting_value_neg",
        "model_quantifier_shifting_value_less_1",
        "model_time_func_bad_init_message",
        "model_flow_bad_samplerate",
        "model_flow_bad_data",
        "gui_not_enough_memory",
        "io_not_enough_memory",
        "io_file_format_error",
        "io_file_format_model_error",
        "io_file_format_model_number_error",
        "io_file_format_model_unknown",
        "io_file_format_dynamics_unknown",
        "io_file_format_dynamics_limit_reach",
        "io_file_format_dynamics_init_error"
    };

    static_assert(std::size(str) == status_size());

    return str[static_cast<int>(s)];
}

template<class C>
constexpr int
length(const C& c) noexcept
{
    return static_cast<int>(c.size());
}

template<class T, size_t N>
constexpr int
length(const T (&array)[N]) noexcept
{
    (void)array;

    return static_cast<int>(N);
}

template<typename Identifier>
constexpr Identifier
undefined() noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return static_cast<Identifier>(0);
}

enum class editor_id : u64;
enum class cluster_id : u64;

using child_id = std::variant<model_id, cluster_id>;

enum class editor_status
{
    editing,
    initializing,
    running_debug,
    running_thread,
    running_thread_need_join
};

static inline constexpr int not_found = -1;

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

struct cluster
{
    cluster() = default;

    small_string<16> name;
    std::vector<child_id> children;
    std::vector<input_port_id> input_ports;
    std::vector<output_port_id> output_ports;

    int get(const child_id id) const noexcept
    {
        auto it = std::find(std::begin(children), std::end(children), id);
        if (it == std::end(children))
            return not_found;

        return static_cast<int>(std::distance(std::begin(children), it));
    }
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

static inline window_logger log_w;

struct editor;

enum class plot_output_id : u64;
enum class file_output_id : u64;

struct plot_output
{
    plot_output() = default;

    plot_output(std::string_view name_)
      : name(name_)
    {}

    void operator()(const irt::observer& obs,
                    const irt::dynamics_type /*type*/,
                    const irt::time t,
                    const irt::observer::status s);

    editor* ed = nullptr;
    std::vector<float> xs;
    std::vector<float> ys;
    small_string<24u> name;
    double tl = 0.0;
    double time_step = 0.1;
    float min = -1.f;
    float max = +1.f;
};

struct file_output
{
    file_output() = default;

    file_output(std::string_view name_)
      : name(name_)
    {}

    void operator()(const irt::observer& obs,
                    const irt::dynamics_type /*type*/,
                    const irt::time t,
                    const irt::observer::status s);

    editor* ed = nullptr;
    std::ofstream ofs;
    small_string<24u> name;
    double tl = 0.0;
};

struct observation_output
{
    constexpr observation_output() = default;

    constexpr void clear() noexcept
    {
        plot_id = undefined<plot_output_id>();
        file_id = undefined<file_output_id>();
    }

    plot_output_id plot_id = undefined<plot_output_id>();
    file_output_id file_id = undefined<file_output_id>();
};

struct editor
{
    small_string<16> name;
    std::filesystem::path path;
    imnodes::EditorContext* context = nullptr;
    bool initialized = false;
    bool show = true;

    simulation sim;

    double simulation_begin = 0.0;
    double simulation_end = 10.0;
    double simulation_current = 10.0;
    double simulation_next_time = 0.0;
    long simulation_bag_id = 0;

    double simulation_during_date;
    int simulation_during_bag;

    std::thread simulation_thread;
    editor_status st = editor_status::editing;
    status sim_st = status::success;

    bool simulation_show_value = false;
    bool stop = false;

    data_array<plot_output, plot_output_id> plot_outs;
    data_array<file_output, file_output_id> file_outs;

    std::vector<observation_output> observation_outputs;
    std::filesystem::path observation_directory;

    data_array<cluster, cluster_id> clusters;
    std::vector<cluster_id> clusters_mapper; /* group per cluster_id */
    std::vector<cluster_id> models_mapper;   /* group per model_id */

    std::vector<bool> models_make_transition;

    ImVector<ImVec2> positions;
    ImVector<ImVec2> displacements;

    bool use_real_time;
    bool starting = true;
    double synchronize_timestep;

    top_cluster top;

    std::string tooltip;

    bool show_load_file_dialog = false;
    bool show_save_file_dialog = false;
    bool show_select_directory_dialog = false;
    bool show_settings = false;

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

    static int get_in(input_port_id id) noexcept
    {
        return static_cast<int>(get_index(id));
    }

    input_port_id get_in(int index) const noexcept
    {
        auto* port = sim.input_ports.try_to_get(static_cast<u32>(index));

        return port ? sim.input_ports.get_id(port) : undefined<input_port_id>();
    }

    static int get_out(output_port_id id) noexcept
    {
        constexpr u32 is_output = 1 << 31;
        u32 index = get_index(id);
        index |= is_output;

        return static_cast<int>(index);
    }

    output_port_id get_out(int index) const noexcept
    {
        constexpr u32 mask = ~(1 << 31); /* remove the first bit */
        index &= mask;

        auto* port = sim.output_ports.try_to_get(static_cast<u32>(index));

        return port ? sim.output_ports.get_id(port)
                    : undefined<output_port_id>();
    }

    status add_lotka_volterra() noexcept;
    status add_izhikevitch() noexcept;

    void show_connections() noexcept;
    void show_model_dynamics(model& mdl) noexcept;
    void show_model_cluster(cluster& mdl) noexcept;
    void show_top() noexcept;

    bool show_editor() noexcept;
};

void
show_simulation_box(editor& ed, bool* show_simulation);

struct application
{
    data_array<editor, editor_id> editors;

    struct settings_manager
    {
        settings_manager() noexcept;

        std::filesystem::path home_dir;
        std::filesystem::path executable_dir;
        std::vector<std::string> libraries_dir;

        void show(bool* is_open);
    } settings;

    bool show_log = true;
    bool show_simulation = true;
    bool show_demo = false;
    bool show_plot = true;
    bool show_settings = false;

    editor* alloc_editor();
    void free_editor(editor& ed);
};

static inline application app;

editor*
make_combo_editor_name(application& app, editor_id& current) noexcept;

} // namespace irt

#endif
