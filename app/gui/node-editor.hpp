// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2020

#include <irritator/core.hpp>

#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "imnodes.hpp"
#include <imgui.h>

namespace irt {

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

enum class simulation_status
{
    success,
    running,
    uninitialized,
    internal_error,
};

static inline constexpr int not_found = -1;

struct top_cluster
{
    std::vector<child_id> children;
    std::vector<int> nodes;
    int next_node_id = 0;

    static inline constexpr int not_found = -1;

    int get_index(const child_id id) const noexcept
    {
        auto it = std::find(std::begin(children), std::end(children), id);
        if (it == std::end(children))
            return not_found;

        return static_cast<int>(std::distance(std::begin(children), it));
    }

    int get_index(const int node) const noexcept
    {
        auto it = std::find(std::begin(nodes), std::end(nodes), node);
        if (it == std::end(nodes))
            return not_found;

        return static_cast<int>(std::distance(std::begin(nodes), it));
    }

    //const child_id get_child(const int node) const noexcept
    //{
    //    if (auto found = get_index(node); found != not_found)
    //        return children[found];

    //    return undefined<child_id>();
    //}

    //const int get_node(const child_id id) const noexcept
    //{
    //    if (auto found = get_index(id); found != not_found)
    //        return nodes[found];

    //    return not_found;
    //}

    void clear() noexcept
    {
        children.clear();
        nodes.clear();
    }

    void pop(const int index) noexcept
    {
        std::swap(children[index], children.back());
        children.pop_back();

        std::swap(nodes[index], nodes.back());
        nodes.pop_back();
    }

    void emplace_back(const child_id id)
    {
        children.emplace_back(id);
        nodes.emplace_back(next_node_id);
        ++next_node_id;
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
        for (size_t i = 0, e = children.size(); i != e; ++i)
            if (id == children[i])
                return static_cast<int>(i);

        return not_found;
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

struct observation_output
{
    enum class type
    {
        none,
        plot,
        file,
        both
    };

    observation_output() = default;

    observation_output(const char* name_)
      : name(name_)
    {}

    std::ofstream ofs;
    const char* name = nullptr;
    array<float> data;
    double tl = 0.0;
    float min = -1.f;
    float max = +1.f;
    int id = 0;
    type observation_type = type::none;
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
    std::thread simulation_thread;
    simulation_status st = simulation_status::uninitialized;
    bool simulation_show_value = false;
    bool stop = false;

    vector<observation_output> observation_outputs;
    array<observation_output::type> observation_types;
    std::filesystem::path observation_directory;

    data_array<cluster, cluster_id> clusters;
    array<cluster_id> clusters_mapper; /* group per cluster_id */
    array<cluster_id> models_mapper;   /* group per model_id */

    top_cluster top;

    status initialize(u32 id) noexcept;
    void clear() noexcept;
    void reorder() noexcept;

    void group(const ImVector<int>& nodes) noexcept;
    void ungroup(const int node) noexcept;
    void free_group(cluster& group) noexcept;
    void free_children(const ImVector<int>& nodes) noexcept;
    void copy_group(const child_id* sources,
                    const size_t size,
                    child_id* destination) noexcept;
    void reorder_subgroup(const size_t from,
                          const size_t length,
                          ImVec2 click_pos) noexcept;

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

    int get_in(input_port_id id) const noexcept
    {
        return static_cast<int>(get_index(id));
    }

    input_port_id get_in(int index) const noexcept
    {
        auto* port = sim.input_ports.try_to_get(static_cast<u32>(index));

        return port ? sim.input_ports.get_id(port) : undefined<input_port_id>();
    }

    int get_out(output_port_id id) const noexcept
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

} // namespace irt

#endif