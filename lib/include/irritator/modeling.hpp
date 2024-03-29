// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include "irritator/helpers.hpp"
#include "irritator/macros.hpp"
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/ext.hpp>
#include <irritator/thread.hpp>

#include <bitset>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace irt {

enum class port_id : u64;
enum class component_id : u64;
enum class hsm_component_id : u64;
enum class generic_component_id : u64;
enum class graph_component_id : u64;
enum class grid_component_id : u64;
enum class tree_node_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;
enum class child_id : u64;
enum class connection_id : u64;
enum class registred_path_id : u64;
enum class variable_observer_id : u64;
enum class grid_observer_id : u64;
enum class graph_observer_id : u64;
enum class global_parameter_id : u64;

using port_str           = small_string<7>;
using name_str           = small_string<31>;
using description_str    = small_string<1022>;
using registred_path_str = small_string<256 * 16 - 2>;
using directory_path_str = small_string<512 - 2>;
using file_path_str      = small_string<512 - 2>;
using log_str            = small_string<512 - 2>;

/// Maximum deepth of the component tree.
constexpr i32 max_component_stack_size = 16;

/// Stores the path from the head of the project to the model by following
/// the path of tree_node and/or component @c unique_id.
using unique_id_path = small_vector<u64, max_component_stack_size>;

/// Stores the path from the @c tree_node_id @c tn to the model.
/// Use the function in class project to easily build instances:
///
///    const auto rel_path = project.build_relative_path(tn, mdl);
///    ...
///    const auto [tn_id, mdl_id] = project.get_model(rel_path);
struct relative_id_path {
    tree_node_id   tn;
    unique_id_path ids;
};

enum class child_type : u8 { model, component };

enum class description_status {
    unread,
    read_only,
    modified,
    unmodified,
};

enum class internal_component {
    qss1_izhikevich,
    qss1_lif,
    qss1_lotka_volterra,
    qss1_negative_lif,
    qss1_seirs,
    qss1_van_der_pol,
    qss2_izhikevich,
    qss2_lif,
    qss2_lotka_volterra,
    qss2_negative_lif,
    qss2_seirs,
    qss2_van_der_pol,
    qss3_izhikevich,
    qss3_lif,
    qss3_lotka_volterra,
    qss3_negative_lif,
    qss3_seirs,
    qss3_van_der_pol,
};

constexpr int internal_component_count =
  ordinal(internal_component::qss3_van_der_pol) + 1;

enum class component_type {
    none,     ///< The component does not reference any container.
    internal, ///< The component reference a c++ code.
    simple,   ///< A classic component-model graph coupling.
    grid,     ///< Grid with 4, 8 neighbourhood.
    graph,    ///< Random graph generator
    hsm       ///< HSM component
};

enum class component_status {
    unread,     ///< The component is not read (It is referenced by another
                ///< component).
    read_only,  ///< The component file is in read-only.
    modified,   ///< The component is not saved.
    unmodified, ///< or you show an internal component.
    unreadable  ///< When an error occurred during load-component.
};

enum class modeling_status { modified, unmodified };

enum class observable_type {
    none,
    file,
    plot,
    graph,
    grid,
};

class project;

struct connection;
struct child;
class generic_component;
class modeling;
struct description;
struct cache_rw;
struct tree_node;
class variable_observer;
class grid_observer;
class graph_observer;

/// A structure use to cache data when read or write json component.
/// - @c buffer is used to store the full file content or output buffer.
/// - @c string_buffer is used when reading string.
/// - @c stack is used when parsing project file.
/// - other variable are used to link file identifier with new identifier.
struct cache_rw {
    vector<char> buffer;
    vector<i32>  stack;

    table<u64, u64>    model_mapping;
    table<u64, u64>    constant_mapping;
    table<u64, u64>    binary_file_mapping;
    table<u64, u64>    random_mapping;
    table<u64, u64>    text_file_mapping;
    table<u64, hsm_id> sim_hsms_mapping;

    small_function<1, void(std::string_view, int level)> warning_cb;

    //! Clear @c resize(0) all vector, table and string.
    //! @attention @c warning_cb and @c error_cb are unmodified.
    void clear() noexcept;

    //! Delete buffers for all vector, table and string.
    //! @attention @c warning_cb and @c error_cb are unmodified.
    void destroy() noexcept;
};

/// Description store the description of a component in a text way. A
/// description is attached to only one component (@c description_id). The
/// filename is the same than the component
/// @c file_path but with the extension ".txt".
///
/// @note  The size of the buffer is static for now
struct description {
    description_str    data;
    description_status status = description_status::unread;
};

enum class child_flags : u8 {
    none         = 0,
    configurable = 1 << 0,
    observable   = 1 << 1,
    Count
};

struct child {
    child() noexcept;
    child(dynamics_type type) noexcept;
    child(component_id component) noexcept;

    union {
        dynamics_type mdl_type;
        component_id  compo_id;
    } id;

    /// An identifier provided by the component parent to easily found a child
    /// in project. The value 0 means unique_id is undefined. @c grid-component
    /// stores a double word (row x column), graph-component stores the nth
    /// vertex, @c generic-component stores a incremental integer.
    u64 unique_id = 0;

    child_type            type  = child_type::model;
    bitflags<child_flags> flags = child_flags::none;
};

struct child_position {
    float x = 0.f;
    float y = 0.f;
};

struct connection {
    connection(child_id src, port_id p_src, child_id dst, port_id p_dst);
    connection(child_id src, port_id p_src, child_id dst, int p_dst);
    connection(child_id src, int p_src, child_id dst, port_id p_dst);
    connection(child_id src, int p_src, child_id dst, int p_dst);

    connection(port_id p_src, child_id dst, port_id p_dst);
    connection(port_id p_src, child_id dst, int p_dst);

    connection(child_id src, port_id p_src, port_id p_dst);
    connection(child_id src, int p_src, port_id p_dst);

    enum class connection_type : i8 { internal, input, output };

    union port {
        port(const port_id compo_) noexcept
          : compo(compo_)
        {}

        port(const int model_) noexcept
          : model(model_)
        {}

        port_id compo;
        int     model;
    };

    struct internal_t {
        child_id src;
        child_id dst;
        port     index_src;
        port     index_dst;
    };

    struct input_t {
        child_id dst;
        port_id  index;
        port     index_dst;
    };

    struct output_t {
        child_id src;
        port_id  index;
        port     index_src;
    };

    connection_type type;

    union {
        internal_t internal;
        input_t    input;
        output_t   output;
    };
};

/// A wrapper to the simulation @c hierarchical_state_machine class.
///
/// This component is different from others. It does not have any child nor
/// connection. @c project class during import, the @c
/// hierarchical_state_machine is copied into the simulation HSM data array. The
/// parameter @c a and @c b are store into the @c children_parameters of the @c
/// generic_component.
struct hsm_component {
    hierarchical_state_machine machine;
};

class generic_component
{
public:
    struct internal_connection {
        child_id src;
        u64      p_src;
        child_id dst;
        u64      p_dst;
    };

    struct input_connection {
        port_id  x; // The port_id in this component.
        child_id dst;
        u64      port;
    };

    struct output_connection {
        port_id  y; // The port_id in this component.
        child_id src;
        u64      port;
    };

    vector<child_id>      children;
    vector<connection_id> connections;

    // vector<child_id>          children;
    // vector<connection_id>     connections;
    vector<input_connection>  input_connections;
    vector<output_connection> output_connections;

    /// Use to affect @c child::unique_id when the component is saved. The value
    /// 0 means unique_id is undefined. Mutable variable to allow function @c
    /// make_next_unique_id to be const and called in json const functions.
    mutable u64 next_unique_id = 1;

    u64 make_next_unique_id() const noexcept { return next_unique_id++; }
};

struct grid_component {
    static inline constexpr i32 row_max    = 1024;
    static inline constexpr i32 column_max = 1024;

    i32 row    = 1;
    i32 column = 1;

    enum class options : i8 { none = 0, row_cylinder, column_cylinder, torus };

    enum class type : i8 {
        number, ///< Only one port for all neighbor.
        name    ///< One, two, three or four ports according to neighbor.
    };

    enum class neighborhood : i8 {
        four,
        eight,
    };

    void resize(i32 row_, i32 col_, component_id id) noexcept
    {
        irt_assert(row_ > 0 && col_ > 0);

        row    = row_;
        column = col_;

        children.resize(row_ * col_);
        std::fill_n(children.data(), children.size(), id);
    }

    static inline constexpr auto type_count = 2;

    constexpr int pos(int row_, int col_) noexcept { return col_ * row + row_; }

    constexpr std::pair<int, int> pos(int pos_) noexcept
    {
        return std::make_pair(pos_ / row, pos_ % row);
    }

    constexpr u64 unique_id(int pos_) noexcept
    {
        auto pair = pos(pos_);

        return make_doubleword(static_cast<u32>(pair.first),
                               static_cast<u32>(pair.second));
    }

    constexpr std::pair<int, int> unique_id(u64 id) noexcept
    {
        auto unpack = unpack_doubleword(id);

        return std::make_pair(static_cast<int>(unpack.first),
                              static_cast<int>(unpack.second));
    }

    constexpr u64 unique_id(int row, int col) noexcept
    {
        return make_doubleword(static_cast<u32>(row), static_cast<u32>(col));
    }

    constexpr u64 make_next_unique_id(std::integral auto row,
                                      std::integral auto col) const noexcept
    {
        irt_assert(is_numeric_castable<u32>(row));
        irt_assert(is_numeric_castable<u32>(col));

        return make_doubleword(static_cast<u32>(row), static_cast<u32>(col));
    }

    struct input_connection {
        port_id x;   // The port_id in this component.
        i32     row; // The row in children vector.
        i32     col; // The col in children vector.
        port_id id;  // The port_id of the @c children[idx].
    };

    struct output_connection {
        port_id y;   // The port_id in this component.
        i32     row; // The row in children vector.
        i32     col; // The col in children vector.
        port_id id;  // The port_id of the @c children[idx].
    };

    vector<component_id>      children;
    vector<input_connection>  input_connections;
    vector<output_connection> output_connections;

    bool exist_input_connection(const port_id x,
                                const i32     row,
                                const i32     col,
                                const port_id id) const noexcept;

    bool exist_output_connection(const port_id y,
                                 const i32     row,
                                 const i32     col,
                                 const port_id id) const noexcept;

    /** Tries to add this input connection if it does not already exist. */
    void add_input_connection(const port_id x,
                              const i32     row,
                              const i32     col,
                              const port_id id) noexcept;

    /** Tries to add this output connection if it does not already exist. */
    void add_output_connection(const port_id y,
                               const i32     row,
                               const i32     col,
                               const port_id id) noexcept;

    vector<child_id>      cache;
    vector<connection_id> cache_connections;

    options      opts            = options::none;
    type         connection_type = type::name;
    neighborhood neighbors       = neighborhood::four;
};

/// random-graph type:
/// - scale_free: graph typically has a very skewed degree distribution, where
///   few vertices have a very high degree and a large number of vertices have a
///   very small degree. Many naturally evolving networks, such as the World
///   Wide Web, are scale-free graphs, making these graphs a good model for
///   certain networking problems.
/// - small_world: consists of a ring graph (where each vertex is connected to
///   its k nearest neighbors). Edges in the graph are randomly rewired to
///   different vertices with a probability p.
class graph_component
{
public:
    static inline constexpr i32 children_max = 4096;

    enum class vertex_id : u32;
    enum class edge_id : u32;

    enum class graph_type { dot_file, scale_free, small_world };

    struct input_connection {
        port_id   x;   // The port_id in this component.
        vertex_id idx; // The index in children vector.
        port_id   id;  // The port_id of the @c children[idx].
    };

    struct output_connection {
        port_id   y;   // The port_id in this component.
        vertex_id idx; // The index in children vector.
        port_id   id;  // The port_id of the @c children[idx].
    };

    struct vertex {
        vertex() noexcept = default;

        vertex(component_id id_) noexcept
          : id{ id_ }
        {}

        small_string<23> name;
        component_id     id = undefined<component_id>();
    };

    struct edge {
        edge() noexcept = default;

        edge(const vertex_id src, const vertex_id dst) noexcept
          : u{ src }
          , v{ dst }
        {}

        vertex_id u = undefined<vertex_id>();
        vertex_id v = undefined<vertex_id>();
    };

    enum class connection_type : i8 {
        number, ///< Only one port for all neighbor.
        name    ///< One, two, three or four ports according to neighbor.
    };

    struct dot_file_param {
        dir_path_id  dir  = undefined<dir_path_id>();
        file_path_id file = undefined<file_path_id>();
    };

    struct scale_free_param {
        double alpha = 2.5;
        double beta  = 1.e3;
    };

    struct small_world_param {
        double probability = 3e-2;
        i32    k           = 6;
    };

    using random_graph_param =
      std::variant<dot_file_param, scale_free_param, small_world_param>;

    graph_component() noexcept;

    //! Resize `children` vector and clear the `edges`, `input_connections` and
    //! `output_connection`.
    void resize(const i32 children_size, const component_id id) noexcept;

    //! Update `edges` according to parameters.
    void update() noexcept;

    constexpr u64 unique_id(int pos_) noexcept
    {
        return static_cast<u64>(pos_);
    }

    data_array<vertex, vertex_id> children;
    data_array<edge, edge_id>     edges;

    vector<input_connection>  input_connections;
    vector<output_connection> output_connections;

    random_graph_param param = scale_free_param{};
    std::array<u64, 4> seed  = { 0u, 0u, 0u, 0u };
    std::array<u64, 2> key   = { 0u, 0u };

    vector<child_id>      cache;
    vector<connection_id> cache_connections;

    connection_type type = connection_type::name;
};

using color           = u32;
using component_color = std::array<float, 4>;

struct port {
    port() noexcept = default;

    port(const std::string_view name_, const component_id parent_) noexcept
      : name{ name_ }
      , parent{ parent_ }
    {}

    port_str     name;
    component_id parent;
};

struct component {
    static inline constexpr int port_number = 8;

    component() noexcept = default;

    vector<port_id> x_names;
    vector<port_id> y_names;

    table<i32, child_id> child_mapping_io;

    description_id    desc     = description_id{ 0 };
    registred_path_id reg_path = registred_path_id{ 0 };
    dir_path_id       dir      = dir_path_id{ 0 };
    file_path_id      file     = file_path_id{ 0 };
    name_str          name;

    union id {
        internal_component   internal_id;
        generic_component_id generic_id;
        grid_component_id    grid_id;
        graph_component_id   graph_id;
        hsm_component_id     hsm_id;
    } id = {};

    component_type   type  = component_type::none;
    component_status state = component_status::unread;
};

struct registred_path {
    enum class state : u8 {
        lock,   /**< `dir-path` is locked during I/O operation. Do not use this
                   class in writing mode. */
        read,   /**< underlying directory is read and the `children` vector is
                   filled. */
        unread, /**< underlying directory is not read. */
        error,  /**< an error occurred during the read. */
    };

    enum class reg_flags : u8 {
        none         = 0,
        access_error = 1 << 1,
        read_only    = 1 << 2,
        Count,
    };

    registred_path_str path; /**< Stores an absolute path in utf8 format. */
    name_str           name; /**< Stores a user name, the same name as in the
                                configuration file. */
    vector<dir_path_id> children;

    state               status   = state::unread;
    bitflags<reg_flags> flags    = reg_flags::none;
    i8                  priority = 0;
    spin_lock           mutex;
};

class dir_path
{
public:
    enum class state : u8 {
        lock,   /**< `dir-path` is locked during I/O operation. Do not use this
                   class in writing mode. */
        read,   /**< underlying directory is read and the `children` vector is
                   filled. */
        unread, /**< underlying directory is not read. */
        error,  /**< an error occurred during the read. */
    };

    enum class dir_flags : u8 {
        none          = 0,
        too_many_file = 1 << 0,
        access_error  = 1 << 1,
        read_only     = 1 << 2,
        Count,
    };

    directory_path_str   path; /**< stores a directory name in utf8. */
    registred_path_id    parent{ 0 };
    vector<file_path_id> children;

    state               status = state::unread;
    bitflags<dir_flags> flags  = dir_flags::none;
    spin_lock           mutex;

    /**
     * Refresh the `children` vector with new file in the filesystem.
     *
     * Files that disappear from the filesystem are not remove from the vector
     * but a flag is added in the `file_path` to indicate an absence of
     * existence in the filesystem.
     */
    vector<file_path_id> refresh(modeling& mod) noexcept;
};

struct file_path {
    enum class state : u8 {
        lock,   /**< `file-path` is locked during I/O operation. Do not use this
                   class in writing mode. */
        read,   /**< underlying file is read. */
        unread, /**< underlying file  is not read. */
    };

    enum class file_flags : u8 {
        none         = 0,
        access_error = 1 << 1,
        read_only    = 1 << 2,
        Count,
    };

    enum class file_type { undefined_file, irt_file, dot_file };

    file_path_str path; /**< stores the file name as utf8 string. */
    dir_path_id   parent{ 0 };
    component_id  component{ 0 };

    file_type            type{ file_type::undefined_file };
    state                status = state::unread;
    bitflags<file_flags> flags  = file_flags::none;
    spin_lock            mutex;
};

struct modeling_initializer {
    i32 model_capacity              = 32768;
    i32 tree_capacity               = 256;
    i32 parameter_capacity          = 4096;
    i32 description_capacity        = 128;
    i32 component_capacity          = 512;
    i32 dir_path_capacity           = 32;
    i32 file_path_capacity          = 512;
    i32 children_capacity           = 8192;
    i32 connection_capacity         = 16384;
    i32 port_capacity               = 32768;
    i32 constant_source_capacity    = 32;
    i32 binary_file_source_capacity = 32;
    i32 text_file_source_capacity   = 32;
    i32 random_source_capacity      = 32;

    u64 random_generator_seed = 1234567890;

    bool is_fixed_window_placement = true;
};

// class variable_simulation_observer
// {
// public:
//     status init(project&           pj,
//                 simulation&        sim,
//                 variable_observer& v_obs) noexcept;

//     void clear() noexcept;
//     void update(simulation& sim) noexcept;

//     vector<observer_id> observers;

//     static_limiter<i32, 8, 512>      raw_buffer_size         = 64;
//     static_limiter<i32, 1024, 65536> linearized_buffer_size  = 32768;
//     floating_point_limiter<float, 1, 10000, 1, 10> time_step = .01f;

//     variable_observer_id id = undefined<variable_observer_id>();
// };

//! A simulation structure to stores the matrix of @c observer_id identifier and
//! a matrix of the last value from each observers.
//!
//! @c grid_simulation_observer stores simulation informations and can be used
//! to dipslay or write data into files.
//!
//!    simulation              modeling
//!   +----------------+      +-----------------+
//!   | grid           |      | grid            |
//!   | simulation     |      | modeling        |
//!   | observer       |      | observer        |
//!   +----------------+      +-----------------+
//!   | vector<obs_id> |      | tree_node       |
//!   | vector<float>  |<-----+                 |
//!   |                |      | compo_id        |
//!   +-----+----------+      | tn_id           |
//!         |  ^  cols        | mdl_id          |
//!         |  |  * rows      |                 |
//!         v  |              +-----------------+
//!   +--------+--+
//!   | observer  |
//!   +-----------+
//!   | mdl_id    |
//!   +-----------+
//!
// class grid_simulation_observer
// {
// public:
//     /// @brief Clear, initialize the grid according to the @c grid_observer.
//     /// @details Clear the @c grid_observation_widget and use the @c
//     ///  grid_observer data to initialize all @c observer_id from the
//     ///  simulation layer.
//     ///
//     /// @return The status.
//     status init(project&       pj,
//                 modeling&      mod,
//                 simulation&    sim,
//                 grid_observer& grid) noexcept;
//
//     /// Assign a new size to children and remove all @c model_id.
//     void resize(int row, int col) noexcept;
//
//     /// Assign @c undefined<model_id> to all children.
//     void clear() noexcept;
//
//     /// Update the values vector with observation values from the simulation
//     /// observers object.
//     void update(simulation& pj) noexcept;
//
//     vector<observer_id> observers;
//     vector<real>        values;
//
//     real none_value = 0.f;
//     int  rows       = 0;
//     int  cols       = 0;
//
//     grid_observer_id id = undefined<grid_observer_id>();
// };

struct tree_node {
    tree_node(component_id id_, u64 unique_id_) noexcept;

    /// Intrusive hierarchy to the children, sibling and parent @c tree_node.
    hierarchy<tree_node> tree;

    /// Reference the current component
    component_id id = undefined<component_id>();

    /// A unique identifier provided by component parent.
    u64 unique_id = 0;

    /// Map component children into simulation model. Table build in @c
    /// project::set or @c project::rebuild functions.
    table<child_id, model_id> child_to_sim;

    table<u64, tree_node_id> unique_id_to_tree_node_id;
    table<u64, model_id>     unique_id_to_model_id;

    table<u64, global_parameter_id>  parameters_ids;
    table<u64, variable_observer_id> variable_observer_ids;

    vector<graph_observer_id> graph_observer_ids;
    vector<grid_observer_id>  grid_observer_ids;

    auto get_model_id(const u64 u_id) const noexcept -> std::optional<model_id>
    {
        auto* ptr = unique_id_to_model_id.get(u_id);

        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_tree_node_id(const u64 u_id) const noexcept
      -> std::optional<tree_node_id>
    {
        auto* ptr = unique_id_to_tree_node_id.get(u_id);

        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_unique_id(const model_id mdl_id) const noexcept -> u64
    {
        auto it = std::find_if(
          unique_id_to_model_id.data.begin(),
          unique_id_to_model_id.data.end(),
          [mdl_id](const auto& elem) noexcept { return elem.value == mdl_id; });

        return it == unique_id_to_model_id.data.end() ? 0u : it->id;
    }

    auto get_unique_id(const tree_node_id tn_id) const noexcept -> u64
    {
        auto it = std::find_if(
          unique_id_to_tree_node_id.data.begin(),
          unique_id_to_tree_node_id.data.end(),
          [tn_id](const auto& elem) noexcept { return elem.value == tn_id; });

        return it == unique_id_to_tree_node_id.data.end() ? 0u : it->id;
    }

    union node {
        node() noexcept = default;
        node(tree_node* tn_) noexcept;
        node(model* mdl_) noexcept;

        tree_node* tn;
        model*     mdl; // model in simluation models.
    };

    /// Stores for each component in children list the identifier of the
    /// tree_node. This variable allows to quickly build the connection
    /// network at build time.
    table<child_id, node> child_to_node;
};

struct parameter {
    std::array<real, 8> reals;
    std::array<i64, 4>  integers;

    //! Copy data from the vectors to the simulation model.
    void copy_to(model& mdl) const noexcept;

    //! Copy data from model to the vectors of this parameter.
    void copy_from(const model& mdl) noexcept;

    //! Assign @c 0 to vector.
    void clear() noexcept;
};

class grid_observer
{
public:
    name_str name;

    tree_node_id parent_id; ///< @c tree_node identifier ancestor of the model
                            ///< A grid component.
    component_id compo_id;  //< @c component in the grid to observe.
    tree_node_id tn_id;     //< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //< @c model to observe.

    vector<observer_id> observers;
    vector<real>        values;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    status init(project& pj, modeling& mod, simulation& sim) noexcept;

    // Clear the `observers` and `values` vectors.
    void clear() noexcept;

    // For each `observer`, get the latest observation value and fill the values
    // vector.
    void update(const simulation& sim) noexcept;

    float scale_min = -100.f;
    float scale_max = +100.f;
    i32   color_map = 0;
    i32   rows      = 0;
    i32   cols      = 0;
};

class graph_observer
{
public:
    name_str name;

    tree_node_id parent_id; ///< @c tree_node identifier ancestor of the model.
                            ///< A graph component.
    component_id compo_id;  //< @c component in the graph to observe.
    tree_node_id tn_id;     //< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //< @c model to observe.
};

class variable_observer
{
public:
    enum class type_options {
        line,
        dash,
    };

    name_str                         name;
    static_limiter<i32, 8, 512>      raw_buffer_size         = 64;
    static_limiter<i32, 1024, 65536> linearized_buffer_size  = 32768;
    floating_point_limiter<float, 1, 10000, 1, 10> time_step = .01f;

    vector<tree_node_id> tn_id; //< @c tree_node identifier parent of the model.
    vector<model_id>     mdl_id; //< @c model to observe.
    vector<observer_id>  obs_ids;
    vector<color>        colors;
    vector<type_options> options;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    status init(project& pj, simulation& sim) noexcept;

    void clear() noexcept;

    void update(simulation& sim) noexcept;

    // Removes from `tn_id` and `mdl_id` vectors the pair (tn, mdl).
    void erase(const tree_node_id tn, const model_id mdl) noexcept;

    // Push into `tn_id` and `mdl_id` vectors the pair (tn, mdl) if the pair
    // does not already exsits.
    void push_back(const tree_node_id tn, const model_id mdl) noexcept;
};

struct global_parameter {
    name_str name;

    tree_node_id tn_id;  //< @c tree_node identifier parent of the model.
    model_id     mdl_id; //< @c model to observe.

    parameter param;
};

struct log_entry {
    log_str   buffer;
    log_level level;
};

class modeling
{
public:
    struct connection_error {};
    struct children_error {};

    enum class part {
        descriptions,
        generic_components,
        grid_components,
        graph_components,
        hsm_components,
        ports,
        components,
        registred_paths,
        dir_paths,
        file_paths,
        hsms,
        children,
        connections
    };

    data_array<description, description_id>             descriptions;
    data_array<generic_component, generic_component_id> generic_components;
    data_array<grid_component, grid_component_id>       grid_components;
    data_array<graph_component, graph_component_id>     graph_components;
    data_array<hsm_component, hsm_component_id>         hsm_components;
    data_array<port, port_id>                           ports;
    data_array<component, component_id>                 components;
    data_array<registred_path, registred_path_id>       registred_paths;
    data_array<dir_path, dir_path_id>                   dir_paths;
    data_array<file_path, file_path_id>                 file_paths;
    data_array<hierarchical_state_machine, hsm_id>      hsms;
    data_array<child, child_id>                         children;
    data_array<connection, connection_id>               connections;

    vector<child_position>  children_positions;
    vector<name_str>        children_names;
    vector<parameter>       children_parameters;
    vector<component_color> component_colors;

    vector<registred_path_id> component_repertories;
    external_source           srcs;

    modeling_status state = modeling_status::unmodified;

    modeling() noexcept;

    status init(modeling_initializer& params) noexcept;

    //! Add internal components to component lists.
    status fill_internal_components() noexcept;

    //! Reads all registered paths and search component files.
    status fill_components() noexcept;

    //! Adds a new path to read and search component files.
    status fill_components(registred_path& path) noexcept;

    /// Clean data used as cache for simulation.
    void clean_simulation() noexcept;

    /// If the @c child references a model, model is freed, otherwise, do
    /// nothing. This function is useful to replace the content of a existing @c
    /// child
    void clear(child& c) noexcept;

    /// Clear and free all dependencies of the component but let the component
    /// alive.
    void clear(component& c) noexcept;

    /// Deletes the component, the file (@c file_path_id) and the description
    /// (@c description_id) objects attached.
    void free(component& c) noexcept;
    void free(generic_component& c) noexcept;
    void free(graph_component& c) noexcept;
    void free(grid_component& c) noexcept;
    void free(hsm_component& c) noexcept;
    void free(child& c) noexcept;
    void free(connection& c) noexcept;

    void free_input_port(component& compo, port& idx) noexcept;
    void free_output_port(component& compo, port& idx) noexcept;

    bool can_alloc_file(i32 number = 1) const noexcept;
    bool can_alloc_dir(i32 number = 1) const noexcept;
    bool can_alloc_registred(i32 number = 1) const noexcept;

    file_path&      alloc_file(dir_path& dir) noexcept;
    dir_path&       alloc_dir(registred_path& reg) noexcept;
    registred_path& alloc_registred(std::string_view name,
                                    int              priority) noexcept;

    bool exists(const registred_path& dir) noexcept;
    bool exists(const dir_path& dir) noexcept;
    bool create_directories(const registred_path& dir) noexcept;
    bool create_directories(const dir_path& dir) noexcept;

    void remove_file(registred_path& reg,
                     dir_path&       dir,
                     file_path&      file) noexcept;

    void move_file(registred_path& reg,
                   dir_path&       from,
                   dir_path&       to,
                   file_path&      file) noexcept;

    void free(file_path& file) noexcept;
    void free(dir_path& dir) noexcept;
    void free(registred_path& dir) noexcept;

    bool can_alloc_grid_component() const noexcept;
    bool can_alloc_generic_component() const noexcept;
    bool can_alloc_graph_component() const noexcept;

    component& alloc_grid_component() noexcept;
    component& alloc_generic_component() noexcept;
    component& alloc_graph_component() noexcept;

    /// For grid_component, build the children and connections
    /// based on children vectors and grid_component options (torus, cylinder
    /// etc.). The newly allocated child and connection are append to the output
    /// vectors. The vectors are not cleared.
    status build_grid_children_and_connections(grid_component&        grid,
                                               vector<child_id>&      ids,
                                               vector<connection_id>& cnts,
                                               i32 upper_limit = 0,
                                               i32 left_limit  = 0,
                                               i32 space_x     = 30,
                                               i32 space_y     = 50) noexcept;

    /// For graph_component, build the children and connections
    /// based on children vectors and graph_component options (torus, cylinder
    /// etc.). The newly allocated child and connection are append to the output
    /// vectors. The vectors are not cleared.
    status build_graph_children_and_connections(graph_component&       graph,
                                                vector<child_id>&      ids,
                                                vector<connection_id>& cnts,
                                                i32 upper_limit = 0,
                                                i32 left_limit  = 0,
                                                i32 space_x     = 30,
                                                i32 space_y     = 50) noexcept;

    /// For grid_component, build the real children and connections grid
    /// based on default_chidren and specific_children vectors and
    /// grid_component options (torus, cylinder etc.).
    status build_grid_component_cache(grid_component& grid) noexcept;

    /// For graph_component, build the real children and connections graph
    /// based on default_chidren and specific_children vectors and
    /// graph_component options (torus, cylinder etc.).
    status build_graph_component_cache(graph_component& graph) noexcept;

    /// Delete children and connections from @c modeling for the @c
    /// grid_component cache.
    void clear_grid_component_cache(grid_component& grid) noexcept;

    /// Delete children and connections from @c modeling for the @c
    /// graph_component cache.
    void clear_graph_component_cache(graph_component& graph) noexcept;

    /// Checks if the child can be added to the parent to avoid recursive loop
    /// (ie. a component child which need the same component in sub-child).
    bool can_add(const component& parent,
                 const component& child) const noexcept;

    child& alloc(generic_component& parent, dynamics_type type) noexcept;
    child& alloc(generic_component& parent, component_id id) noexcept;

    status copy(const generic_component& src, generic_component& dst) noexcept;
    status copy(internal_component src, component& dst) noexcept;
    status copy(const component& src, component& dst) noexcept;
    status copy(grid_component& grid, component& dst) noexcept;
    status copy(grid_component& grid, generic_component& s) noexcept;
    status copy(graph_component& grid, generic_component& s) noexcept;

    port_id get_x_index(const component& c,
                        std::string_view name) const noexcept;
    port_id get_y_index(const component& c,
                        std::string_view name) const noexcept;

    port_id get_or_add_x_index(component& c, std::string_view name) noexcept;
    port_id get_or_add_y_index(component& c, std::string_view name) noexcept;

    status connect_input(generic_component& parent,
                         port&              x,
                         child&             c,
                         connection::port   p_c) noexcept;

    status connect_output(generic_component& parent,
                          child&             c,
                          connection::port   p_c,
                          port&              y) noexcept;

    status connect(generic_component& parent,
                   child&             src,
                   connection::port   y,
                   child&             dst,
                   connection::port   x) noexcept;

    status save(component& c) noexcept;

    ring_buffer<log_entry> log_entries;

    spin_lock reg_paths_mutex;
    spin_lock dir_paths_mutex;
    spin_lock file_paths_mutex;
};

class project
{
public:
    //! Used to report which part of the @c project have a problem with the @c
    //! new_error function.
    enum class part {
        tree_nodes,
        variable_observers,
        grid_observers,
        graph_observers,
        global_parameters
    };

    enum error {
        not_enough_memory,
        unknown_source,
        impossible_connection,
        empty_project,

        component_empty,
        component_type_error,
        file_error,
        file_component_type_error,

        registred_path_access_error,
        directory_access_error,
        file_access_error,
        file_open_error,

        file_parameters_error,
        file_parameters_access_error,
        file_parameters_type_error,
        file_parameters_init_error,
    };

    status init(const modeling_initializer& init) noexcept;

    status load(modeling&   mod,
                simulation& sim,
                cache_rw&   cache,
                const char* filename) noexcept;

    status save(modeling&   mod,
                simulation& sim,
                cache_rw&   cache,
                const char* filename) noexcept;

    /// Assign a new @c component head. The previously allocated tree_node
    /// hierarchy is removed and a newly one is allocated.
    status set(modeling& mod, simulation& sim, component& compo) noexcept;

    /// Build the complete @c tree_node hierarchy from the @c component head.
    status rebuild(modeling& mod, simulation& sim) noexcept;

    /// Remove @c tree_node hierarchy and clear the @c component head.
    void clear() noexcept;

    /// For all @c tree_node remove the simulation mapping between modelling and
    /// simulation part (ie @c tree_node::sim variable).
    void clean_simulation() noexcept;

    auto head() const noexcept -> component_id;
    auto tn_head() const noexcept -> tree_node*;

    auto node(tree_node_id id) const noexcept -> tree_node*;
    auto node(tree_node& node) const noexcept -> tree_node_id;
    auto node(const tree_node& node) const noexcept -> tree_node_id;

    template<typename Function, typename... Args>
    auto for_all_tree_nodes(Function&& f, Args... args) noexcept;

    template<typename Function, typename... Args>
    auto for_all_tree_nodes(Function&& f, Args... args) const noexcept;

    template<typename Function, typename... Args>
    void for_each_children(tree_node& tn, Function&& f, Args... args) noexcept;

    /// Return the size and the capacity of the tree_nodes data_array.
    auto tree_nodes_size() const noexcept -> std::pair<int, int>;

    /// Clear all vector and table in @c cache.
    void clear_cache() noexcept;

    /// Release all memory for vectors and tables in @c cache.
    void destroy_cache() noexcept;

    /// Used to cache memory allocation when user import a model into
    /// simulation. The memory cached can be reused using clear but memory
    /// cached can be completely free using the @c destroy_cache function.
    struct cache {
        vector<tree_node*>             stack;
        vector<std::pair<model*, int>> inputs;
        vector<std::pair<model*, int>> outputs;

        table<u64, constant_source_id>    constants;
        table<u64, binary_file_source_id> binary_files;
        table<u64, text_file_source_id>   text_files;
        table<u64, random_source_id>      randoms;
    };

    enum class observation_id : u32;

    /// Build a @c from is excluced from the relative_id_path
    auto build_relative_path(const tree_node& from,
                             const tree_node& to,
                             const model_id   mdl_id) noexcept
      -> relative_id_path;

    auto get_model(const relative_id_path& path) noexcept
      -> std::pair<tree_node_id, model_id>;

    auto get_model(const tree_node& tn, const relative_id_path& path) noexcept
      -> std::pair<tree_node_id, model_id>;

    void build_unique_id_path(const tree_node_id tn_id,
                              const model_id     mdl_id,
                              unique_id_path&    out) noexcept;

    void build_unique_id_path(const tree_node_id tn_id,
                              unique_id_path&    out) noexcept;

    void build_unique_id_path(const tree_node& model_unique_id_parent,
                              const u64        model_unique_id,
                              unique_id_path&  out) noexcept;

    auto get_model_path(u64 id) const noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    auto get_model_path(const unique_id_path& path) const noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    auto get_tn_id(const unique_id_path& path) const noexcept
      -> std::optional<tree_node_id>;

    data_array<tree_node, tree_node_id> tree_nodes;

    data_array<variable_observer, variable_observer_id> variable_observers;
    data_array<grid_observer, grid_observer_id>         grid_observers;
    data_array<graph_observer, graph_observer_id>       graph_observers;

    data_array<global_parameter, global_parameter_id> global_parameters;

    /** Use the index of the @c get_index<grid_observer_id>. */
    // vector<grid_simulation_observer> grid_observation_systems;

    /** Use the index of the @c get_index<graph_observer_id>. */
    // vector<graph_observation_system> graph_observation_systems;

private:
    component_id m_head    = undefined<component_id>();
    tree_node_id m_tn_head = undefined<tree_node_id>();

    cache m_cache;
};

std::string_view to_string(const project::part p) noexcept;
std::string_view to_string(const project::error e) noexcept;

/* ------------------------------------------------------------------
   Child part
 */

inline connection::connection(child_id src,
                              port_id  p_src,
                              child_id dst,
                              port_id  p_dst)
  : type{ connection_type::internal }
{
    internal.src             = src;
    internal.dst             = dst;
    internal.index_src.compo = p_src;
    internal.index_dst.compo = p_dst;
}

inline connection::connection(child_id src,
                              int      p_src,
                              child_id dst,
                              port_id  p_dst)
  : type{ connection_type::internal }
{
    internal.src             = src;
    internal.dst             = dst;
    internal.index_src.model = p_src;
    internal.index_dst.compo = p_dst;
}

inline connection::connection(child_id src,
                              port_id  p_src,
                              child_id dst,
                              int      p_dst)
  : type{ connection_type::internal }
{
    internal.src             = src;
    internal.dst             = dst;
    internal.index_src.compo = p_src;
    internal.index_dst.model = p_dst;
}

inline connection::connection(child_id src, int p_src, child_id dst, int p_dst)
  : type{ connection_type::internal }
{
    internal.src             = src;
    internal.dst             = dst;
    internal.index_src.model = p_src;
    internal.index_dst.model = p_dst;
}

inline connection::connection(port_id p_src, child_id dst, port_id p_dst)
  : type{ connection_type::input }
{
    input.dst             = dst;
    input.index           = p_src;
    input.index_dst.compo = p_dst;
}

inline connection::connection(port_id p_src, child_id dst, int p_dst)
  : type{ connection_type::input }
{
    input.dst             = dst;
    input.index           = p_src;
    input.index_dst.model = p_dst;
}

inline connection::connection(child_id src, port_id p_src, port_id p_dst)
  : type{ connection_type::output }
{
    output.src             = src;
    output.index           = p_dst;
    output.index_src.compo = p_src;
}

inline connection::connection(child_id src, int p_src, port_id p_dst)
  : type{ connection_type::output }
{
    output.src             = src;
    output.index           = p_dst;
    output.index_src.model = p_src;
}

inline child::child() noexcept
  : id{ .mdl_type = dynamics_type::constant }
  , type{ child_type::model }
{}

inline child::child(dynamics_type type) noexcept
  : id{ .mdl_type = type }
  , type{ child_type::model }
{}

inline child::child(component_id component) noexcept
  : id{ .compo_id = component }
  , type{ child_type::component }
{}

inline tree_node::tree_node(component_id id_, u64 unique_id_) noexcept
  : id(id_)
  , unique_id(unique_id_)
{}

inline tree_node::node::node(tree_node* tn_) noexcept
  : tn(tn_)
{}

inline tree_node::node::node(model* mdl_) noexcept
  : mdl(mdl_)
{}

/*
   Project part
 */

template<typename Function, typename... Args>
inline auto project::for_all_tree_nodes(Function&& f, Args... args) noexcept
{
    tree_node* tn = nullptr;
    while (tree_nodes.next(tn))
        return f(*tn, args...);
}

template<typename Function, typename... Args>
inline auto project::for_all_tree_nodes(Function&& f,
                                        Args... args) const noexcept
{
    const tree_node* tn = nullptr;
    while (tree_nodes.next(tn))
        return f(*tn, args...);
}

template<typename Function, typename... Args>
inline void project::for_each_children(tree_node& tn,
                                       Function&& f,
                                       Args... args) noexcept
{
    auto* child = tn.tree.get_child();
    if (!child)
        return;

    vector<tree_node*> stack;
    stack.emplace_back(child);
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        f(*child, args...);

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }
}

} // namespace irt

#endif
