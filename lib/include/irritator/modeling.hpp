// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <irritator/container.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/ext.hpp>
#include <irritator/helpers.hpp>
#include <irritator/macros.hpp>
#include <irritator/thread.hpp>

#include <mutex>
#include <optional>

namespace irt {

enum class port_id : u32;
enum class input_connection_id : u32;
enum class output_connection_id : u32;
enum class component_id : u64;
enum class hsm_component_id : u64;
enum class generic_component_id : u64;
enum class graph_component_id : u64;
enum class grid_component_id : u64;
enum class tree_node_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;
enum class child_id : u32;
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
using color              = u32;
using component_color    = std::array<float, 4>;

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

class project;
class log_manager;
struct log_entry;
struct parameter;
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

//! Stores default values for all @c irt::dynamics.
//!
//! Mainly used to override default values of @c irt::component models into
//! the @c irt::tree_node objects of the @c irt::project class.
struct parameter {
    parameter() noexcept = default;

    //! Import values from the model @c mdl according to the underlying @c
    //! irt::dynamics_type.
    parameter(const model& mdl) noexcept;

    //! Initialize values from the default dynamics type.
    parameter(const dynamics_type type) noexcept;

    //! Copy data from the vectors to the simulation model.
    void copy_to(model& mdl) const noexcept;

    //! Copy data from model to the vectors of this parameter.
    void copy_from(const model& mdl) noexcept;

    //! Initialize data from dynamics type default values.
    void init_from(const dynamics_type type) noexcept;

    //! Assign @c 0 to reals and integers arrays.
    void clear() noexcept;

    std::array<real, 8> reals;
    std::array<i64, 4>  integers;
};

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
    struct port {
        port_id compo = port_id{};
        int     model = 0;

        friend bool operator==(const port& left, const port& right) noexcept
        {
            return left.compo == right.compo and left.model == right.model;
        }
    };

    child_id src;
    child_id dst;
    port     index_src;
    port     index_dst;

    connection(child_id src_,
               port     index_src_,
               child_id dst_,
               port     index_dst_) noexcept;
    connection(child_id src,
               port_id  p_src,
               child_id dst,
               port_id  p_dst) noexcept;
    connection(child_id src, port_id p_src, child_id dst, int p_dst) noexcept;
    connection(child_id src, int p_src, child_id dst, port_id p_dst) noexcept;
    connection(child_id src, int p_src, child_id dst, int p_dst) noexcept;

    friend bool operator==(const connection& left,
                           const connection& right) noexcept
    {
        return left.src == right.src and left.dst == right.dst and
               left.index_src == right.index_src and
               left.index_dst == right.index_dst;
    }
};

/// A wrapper to the simulation @c hierarchical_state_machine class.
///
/// This component is different from others. It does not have any child nor
/// connection. @c project class during import, the @c
/// hierarchical_state_machine is copied into the simulation HSM data array. The
/// parameter @c a and @c b are store into the @c children_parameters of the @c
/// generic_component.
using hsm_component = hierarchical_state_machine;

class generic_component
{
public:
    using child_limiter      = static_limiter<i32, 64, 64 * 16>;
    using connection_limiter = static_limiter<i32, 64 * 4, 64 * 16 * 4>;

    struct children_error {};
    struct connection_error {};
    struct input_connection_error {};
    struct output_connection_error {};

    generic_component() noexcept;

    generic_component(const child_limiter      child_limit,
                      const connection_limiter connection_limit) noexcept;

    struct input_connection {
        input_connection(const port_id          x_,
                         const child_id         dst_,
                         const connection::port port_) noexcept
          : x(x_)
          , dst(dst_)
          , port(port_)
        {}

        port_id          x; // The port_id in this component.
        child_id         dst;
        connection::port port;
    };

    struct output_connection {
        output_connection(port_id          y_,
                          child_id         src_,
                          connection::port port_) noexcept

          : y(y_)
          , src(src_)
          , port(port_)
        {}

        port_id          y; // The port_id in this component.
        child_id         src;
        connection::port port;
    };

    data_array<child, child_id>                         children;
    data_array<connection, connection_id>               connections;
    data_array<input_connection, input_connection_id>   input_connections;
    data_array<output_connection, output_connection_id> output_connections;

    vector<child_position> children_positions;
    vector<name_str>       children_names;
    vector<parameter>      children_parameters;

    //! Use to affect @c child::unique_id when the component is saved. The value
    //! 0 means unique_id is undefined. Mutable variable to allow function @c
    //! make_next_unique_id to be const and called in json const functions.
    mutable u64 next_unique_id = 1;

    bool exists_input_connection(const port_id          x,
                                 const child&           dst,
                                 const connection::port port) const noexcept;
    bool exists_output_connection(const port_id          y,
                                  const child&           src,
                                  const connection::port port) const noexcept;

    bool exists(const child&           src,
                const connection::port p_src,
                const child&           dst,
                const connection::port p_dst) const noexcept;

    //! @brief Add a new connection into `connections` data_array.
    //!
    //! This function checks if none connection already exists with the same
    //! parameter and if the child are compatible (at `model` and `component`
    //! level).
    //!
    //! @param components
    //! @param src
    //! @param p_src
    //! @param dst
    //! @param p_dst
    //!
    //! @return `success()` or `already_exists_connection_error` or
    //! `incompatible_connection_error`.
    status connect(const modeling&        mod,
                   const child&           src,
                   const connection::port p_src,
                   const child&           dst,
                   const connection::port p_dst) noexcept;
    status connect_input(const port_id          x,
                         const child&           dst,
                         const connection::port port) noexcept;
    status connect_output(const port_id          y,
                          const child&           src,
                          const connection::port port) noexcept;

    /// @brief Copy a child into another `generic_component`.
    ///
    /// @param c [in] The identifier to copy.
    /// @param dst [in,out] The `generic_component` destination.
    ///
    /// @return Identifier of the newly allocated child. Can return
    /// error if `dst` can not allocate a new child.
    result<child_id> copy_to(const child&       c,
                             generic_component& dst) const noexcept;

    /// @brief Import children, connections and optionaly properties.
    ///
    /// @details Copy children and connections into the this
    /// `generic_component` and initialize of copy properties if exsits.
    ///
    /// @return `success()` or `modeling::connection_error`,
    /// `modeling::child_error`.
    status import(const data_array<child, child_id>&           children,
                  const data_array<connection, connection_id>& connections,
                  const std::span<child_position>              positions = {},
                  const std::span<name_str>                    names     = {},
                  const std::span<parameter> parameters = {}) noexcept;

    u64 make_next_unique_id() const noexcept { return next_unique_id++; }

    static auto build_error_handlers(log_manager& l) noexcept;
    static void format_connection_error(log_entry& e) noexcept;
    static void format_connection_full_error(log_entry& e) noexcept;
    static void format_input_connection_error(log_entry& e) noexcept;
    static void format_input_connection_full_error(log_entry& e) noexcept;
    static void format_output_connection_error(log_entry& e) noexcept;
    static void format_output_connection_full_error(log_entry& e) noexcept;
    static void format_children_error(log_entry& e) noexcept;
};

struct grid_component {
    static inline constexpr i32 row_max    = 1024;
    static inline constexpr i32 column_max = 1024;

    struct input_connection_error {};
    struct output_connection_error {};
    struct children_connection_error {};

    i32 row    = 1;
    i32 column = 1;

    enum class options : i8 { none = 0, row_cylinder, column_cylinder, torus };

    enum class type : i8 {
        in_out, //!< Only one port "in" or "out".
        name,   //!< Cardinal points according to neighbor: "N", "S", "W", "E",
                //!< "NE", ...
        number  //!< A tuple of integers that represent neighborhood for
                //!< examples: (5,5,5) the middle in 3D and the (4,4,5) the
                //!< top-left cell. (5) the middle in 1D and (6) the
                //!< righ-cell.
    };

    enum class neighborhood : i8 {
        four,
        eight,
    };

    void resize(i32 row_, i32 col_, component_id id) noexcept
    {
        debug::ensure(row_ > 0 && col_ > 0);

        row    = row_;
        column = col_;

        children.resize(row_ * col_);
        std::fill_n(children.data(), children.size(), id);
    }

    static inline constexpr auto type_count = 2;

    constexpr int pos(int row_, int col_) const noexcept
    {
        return col_ * row + row_;
    }

    constexpr std::pair<int, int> pos(int pos_) const noexcept
    {
        return std::make_pair(pos_ / row, pos_ % row);
    }

    constexpr u64 unique_id(int pos_) const noexcept
    {
        auto pair = pos(pos_);

        return make_doubleword(static_cast<u32>(pair.first),
                               static_cast<u32>(pair.second));
    }

    constexpr std::pair<int, int> unique_id(u64 id) const noexcept
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
        debug::ensure(is_numeric_castable<u32>(row));
        debug::ensure(is_numeric_castable<u32>(col));

        return make_doubleword(static_cast<u32>(row), static_cast<u32>(col));
    }

    struct input_connection {
        input_connection(port_id x_, i32 row_, i32 col_, port_id id_) noexcept
          : x(x_)
          , row(row_)
          , col(col_)
          , id(id_)
        {}

        port_id x;   // The port_id in this component.
        i32     row; // The row in children vector.
        i32     col; // The col in children vector.
        port_id id;  // The port_id of the @c children[idx].
    };

    struct output_connection {
        output_connection(port_id y_, i32 row_, i32 col_, port_id id_) noexcept
          : y(y_)
          , row(row_)
          , col(col_)
          , id(id_)
        {}

        port_id y;   // The port_id in this component.
        i32     row; // The row in children vector.
        i32     col; // The col in children vector.
        port_id id;  // The port_id of the @c children[idx].
    };

    //! @brief Check if the input connection already exits.
    bool exists_input_connection(const port_id x,
                                 const i32     row,
                                 const i32     col,
                                 const port_id id) const noexcept;

    //! @brief Check if the output connection already exits.
    bool exists_output_connection(const port_id x,
                                  const i32     row,
                                  const i32     col,
                                  const port_id id) const noexcept;

    //! @brief Tries to add this input connection if it does not already exist.
    //! @return `success()` or `connection_already_exists`.
    result<input_connection_id> connect_input(const port_id x,
                                              const i32     row,
                                              const i32     col,
                                              const port_id id) noexcept;

    //! @brief Tries to add this output connection if it does not already exist.
    //! @return `success()` or `connection_already_exists`.
    result<output_connection_id> connect_output(const port_id y,
                                                const i32     row,
                                                const i32     col,
                                                const port_id id) noexcept;

    vector<component_id>                                children;
    data_array<input_connection, input_connection_id>   input_connections;
    data_array<output_connection, output_connection_id> output_connections;

    data_array<child, child_id>           cache;
    data_array<connection, connection_id> cache_connections;

    //! clear the @c cache and @c cache_connection data_array.
    void clear_cache() noexcept;

    //! build the @c cache and @c cache_connection data_array according to
    //! current attributes @c row, @c column, @c opts, @c connection_type and @c
    //! neighbors.
    //!
    //! @param mod Necessary to read, check and build components and
    //! connections.
    //! @return success() or @c project::error::not_enough_memory.
    status build_cache(modeling& mod) noexcept;

    options      opts                = options::none;
    type         in_connection_type  = type::name;
    type         out_connection_type = type::name;
    neighborhood neighbors           = neighborhood::four;

    static auto build_error_handlers(log_manager& l) noexcept;
    static void format_input_connection_error(log_entry& e) noexcept;
    static void format_output_connection_error(log_entry& e) noexcept;
    static void format_children_connection_error(log_entry& e,
                                                 e_memory   mem) noexcept;
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

    struct input_connection_error {};
    struct output_connection_error {};
    struct children_error {};

    enum class vertex_id : u32;
    enum class edge_id : u32;

    enum class graph_type { dot_file, scale_free, small_world };

    struct input_connection {
        input_connection(port_id x_, vertex_id v_, port_id id_) noexcept
          : x(x_)
          , v(v_)
          , id(id_)
        {}

        port_id   x;  // The port_id in this component.
        vertex_id v;  // The index in children vector.
        port_id   id; // The port_id of the @c children[idx].
    };

    struct output_connection {
        output_connection(port_id y_, vertex_id v_, port_id id_) noexcept
          : y(y_)
          , v(v_)
          , id(id_)
        {}

        port_id   y;  // The port_id in this component.
        vertex_id v;  // The index in children vector.
        port_id   id; // The port_id of the @c children[idx].
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

    union random_graph_param {
        dot_file_param    dot;
        scale_free_param  scale;
        small_world_param small;
    };

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

    //! @brief Check if the input connection already exits.
    bool exists_input_connection(const port_id   x,
                                 const vertex_id v,
                                 const port_id   id) const noexcept;

    //! @brief Check if the output connection already exits.
    bool exists_output_connection(const port_id   y,
                                  const vertex_id v,
                                  const port_id   id) const noexcept;

    //! @brief Tries to add this input connection if it does not already exist.
    //! @return `success()` or `connection_already_exists`.
    result<input_connection_id> connect_input(const port_id   x,
                                              const vertex_id v,
                                              const port_id   id) noexcept;

    //! @brief Tries to add this output connection if it does not already exist.
    //! @return `success()` or `connection_already_exists`.
    result<output_connection_id> connect_output(const port_id   y,
                                                const vertex_id v,
                                                const port_id   id) noexcept;

    data_array<vertex, vertex_id>                       children;
    data_array<edge, edge_id>                           edges;
    data_array<input_connection, input_connection_id>   input_connections;
    data_array<output_connection, output_connection_id> output_connections;

    random_graph_param param   = { .scale = scale_free_param{} };
    graph_type         g_type  = graph_type::scale_free;
    u64                seed[4] = { 0u, 0u, 0u, 0u };
    u64                key[2]  = { 0u, 0u };

    data_array<child, child_id>           cache;
    data_array<connection, connection_id> cache_connections;
    vector<child_position>                positions;

    int space_x     = 100;
    int space_y     = 100;
    int left_limit  = 0;
    int upper_limit = 0;

    //! clear the @c cache and @c cache_connection data_array.
    void clear_cache() noexcept;

    //! build the @c cache and @c cache_connection data_array according to
    //! current attributes @c children, @c edges and @c random_graph_param.
    //!
    //! @param mod Necessary to read, check and build components and
    //! connections.
    //! @return success() or @c project::error::not_enough_memory.
    status build_cache(modeling& mod) noexcept;

    connection_type type = connection_type::name;

    static auto build_error_handlers(log_manager& l) noexcept;
    static void format_input_connection_error(log_entry& e) noexcept;
    static void format_input_connection_full_error(log_entry& e) noexcept;
    static void format_output_connection_error(log_entry& e) noexcept;
    static void format_output_connection_full_error(log_entry& e) noexcept;
    static void format_children_error(log_entry& e, e_memory mem) noexcept;
};

struct component {
    component() noexcept;

    id_array<port_id> x;
    id_array<port_id> y;

    vector<port_str> x_names;
    vector<port_str> y_names;

    port_id get_x(std::string_view str) const noexcept
    {
        for (const auto id : x)
            if (str == x_names[get_index(id)].sv())
                return id;

        return undefined<port_id>();
    }

    port_id get_y(std::string_view str) const noexcept
    {
        for (const auto id : y)
            if (str == y_names[get_index(id)].sv())
                return id;

        return undefined<port_id>();
    }

    port_id get_or_add_x(std::string_view str) noexcept
    {
        const auto id = get_x(str);
        if (is_defined<port_id>(id))
            return id;

        if (not x.can_alloc(1))
            return undefined<port_id>();

        const auto new_id          = x.alloc();
        x_names[get_index(new_id)] = str;
        return new_id;
    }

    port_id get_or_add_y(std::string_view str) noexcept
    {
        const auto id = get_y(str);
        if (is_defined<port_id>(id))
            return id;

        if (not y.can_alloc(1))
            return undefined<port_id>();

        const auto new_id          = y.alloc();
        y_names[get_index(new_id)] = str;
        return new_id;
    }

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
    spin_mutex          mutex;
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
    spin_mutex          mutex;

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
    spin_mutex           mutex;
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

struct tree_node {
    tree_node(component_id id_, u64 unique_id_) noexcept;

    /// Intrusive hierarchy to the children, sibling and parent @c tree_node.
    hierarchy<tree_node> tree;

    /// Reference the current component
    component_id id = undefined<component_id>();

    struct child_node {
        union {
            model*     mdl{}; /* `mdl_id` Always valid in `data_array`. */
            tree_node* tn;    /* `tn_id` is valid in `data_array`. */
        };

        enum class type { empty, model, tree_node } type = type::empty;

        void set(model* id) noexcept
        {
            mdl  = id;
            type = type::model;
        }

        void set(tree_node* id) noexcept
        {
            tn   = id;
            type = type::tree_node;
        }
    };

    //! Filled during the `project::set` or `project::rebuild` functions, the
    //! size of this vector is the same as `generic_component::children`,
    //! `grid_component::cache` or `graph_component::cache` capacity. Use the
    //! `get_index(child_id)` to get the correct value.
    vector<child_node> children;

    bool is_model(const child_id id) const noexcept
    {
        return children[get_index(id)].type == child_node::type::model;
    }

    bool is_tree_node(const child_id id) const noexcept
    {
        return children[get_index(id)].type == child_node::type::tree_node;
    }

    /// A unique identifier provided by component parent.
    u64 unique_id = 0;

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
};

class grid_observer
{
public:
    name_str name;

    tree_node_id parent_id; //!< @c tree_node identifier ancestor of the model a
                            //!< grid component.
    component_id compo_id;  //!< @c component in the grid to observe.
    tree_node_id tn_id;     //!< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //!< @c model to observe.

    vector<observer_id> observers;
    vector<real>        values;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    void init(project& pj, modeling& mod, simulation& sim) noexcept;

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

    vector<observer_id> observers;
    vector<real>        values;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    void init(project& pj, modeling& mod, simulation& sim) noexcept;

    // Clear the `observers` and `values` vectors.
    void clear() noexcept;

    // For each `observer`, get the latest observation value and fill the values
    // vector.
    void update(const simulation& sim) noexcept;

    float scale_min = -100.f;
    float scale_max = +100.f;
    i32   nodes     = 0;
};

class variable_observer
{
public:
    enum class type_options {
        line,
        dash,
    };

    name_str                         name;
    static_limiter<i32, 8, 64>       max_observers           = 8;
    static_limiter<i32, 8, 512>      raw_buffer_size         = 64;
    static_limiter<i32, 1024, 65536> linearized_buffer_size  = 32768;
    floating_point_limiter<float, 1, 10000, 1, 10> time_step = .01f;

    enum class sub_id : u32;

private:
    id_array<sub_id>     m_ids;
    vector<tree_node_id> m_tn_ids;  //!< `tree_node` parent of the model.
    vector<model_id>     m_mdl_ids; //!< `model` to observe.
    vector<observer_id>  m_obs_ids; //!< `observer` connected to `model`.
    vector<color>        m_colors;  //!< Colors used for observers.
    vector<type_options> m_options; //!< Line, dash etc. for observers.
    vector<name_str>     m_names;   //!< Name of the observation.

public:
    //! @brief Fill the `observer_id` vector and initialize buffers.
    //!
    //! @details Build or reuse existing observer in `obs_id` vector for each
    //! pair `tn_id` and `mdl_id` and (re)initialize buffer reserve buffer.
    status init(project& pj, simulation& sim) noexcept;

    //! @brief Fill the `observer_id` vector with undefined value.
    void clear() noexcept;

    //! @brief Search a `sub_id`  `tn` and `mdl`.
    sub_id find(const tree_node_id tn, const model_id mdl) noexcept;

    //! @brief Remove `sub_id` for all ` m_tn_ids` equal to `tn` and `mdl_ids`
    //! equal to `mdl`.
    //!
    //! @details Do nothing if `tn` and `mdl` is not found.
    void erase(const tree_node_id tn, const model_id mdl) noexcept;

    //! @brief Remove a `sub_id` from `id_array`.
    void erase(const sub_id id) noexcept;

    //! @brief Push data in all vectors if pair (`tn`, `mdl`) does not
    //! already exists.
    sub_id push_back(const tree_node_id tn,
                     const model_id     mdl,
                     const color          = 0xFe1a0Fe,
                     const type_options t = type_options::line) noexcept;

    unsigned size() const noexcept { return m_ids.size(); }
    int      ssize() const noexcept { return m_ids.ssize(); }

    template<typename Function>
    void if_exists_do(const sub_id id, Function&& fn) noexcept
    {
        if (m_ids.exists(id)) {
            const auto idx = get_index(id);

            fn(m_obs_ids[idx], m_colors[idx], m_options[idx], m_names[idx]);
        }
    }

    template<typename Function>
    void for_each_tn_mdl(Function&& f) const noexcept
    {
        for (const auto id : m_ids)
            f(m_tn_ids[get_index(id)], m_mdl_ids[get_index(id)]);
    }

    template<typename Function>
    void for_each_obs(Function&& f) const noexcept
    {
        for (const auto id : m_ids)
            f(m_obs_ids[get_index(id)],
              m_colors[get_index(id)],
              m_options[get_index(id)],
              m_names[get_index(id)]);
    }
};

struct log_entry {
    log_str   buffer;
    log_level level;
};

class log_manager
{
public:
    constexpr log_manager(constrained_value<int, 1, 64> value = 8)
      : m_data(value.value())
    {}

    log_manager(const log_manager& other) noexcept            = delete;
    log_manager& operator=(const log_manager& other) noexcept = delete;
    log_manager(log_manager&& other) noexcept                 = delete;
    log_manager& operator=(log_manager&& other) noexcept      = delete;

    template<typename Function>
    constexpr bool try_push(log_level l, Function&& fn) noexcept
    {
        if (ordinal(l) <= ordinal(m_minlevel)) {
            if (std::unique_lock _(m_mutex, std::try_to_lock); _.owns_lock()) {
                fn(m_data.force_emplace_enqueue());
                return true;
            }
        }

        return false;
    }

    template<typename Function>
    constexpr void push(log_level l, Function&& fn) noexcept
    {
        if (ordinal(l) <= ordinal(m_minlevel)) {
            m_mutex.lock();
            fn(m_data.force_emplace_enqueue());
            m_mutex.unlock();
        }
    }

    template<typename Function>
    constexpr bool try_consume(Function&& fn) noexcept
    {
        if (std::unique_lock _(m_mutex, std::try_to_lock); _.owns_lock()) {
            fn(m_data);
            return true;
        }

        return false;
    }

    template<typename Function>
    constexpr void consume(Function&& fn) noexcept
    {
        m_mutex.lock();
        fn(m_data);
        m_mutex.unlock();
    }

    constexpr bool have_entry() const noexcept { return m_data.ssize() > 0; }

    constexpr bool full() const noexcept { return m_data.full(); }

    //! Return true if the underlying container available space is low.
    //!
    //! @return true if the remaining entry is lower than 25% of the capacity,
    //! false otherwise.
    constexpr bool almost_full() const noexcept
    {
        return (m_data.capacity() - m_data.ssize()) <= m_data.capacity() >> 2;
    }

private:
    ring_buffer<log_entry> m_data;
    spin_mutex             m_mutex;
    log_level              m_minlevel = log_level::notice;
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
    data_array<component, component_id>                 components;
    data_array<registred_path, registred_path_id>       registred_paths;
    data_array<dir_path, dir_path_id>                   dir_paths;
    data_array<file_path, file_path_id>                 file_paths;
    data_array<hierarchical_state_machine, hsm_id>      hsms;

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
    bool can_alloc_hsm_component() const noexcept;

    component& alloc_grid_component() noexcept;
    component& alloc_generic_component() noexcept;
    component& alloc_graph_component() noexcept;
    component& alloc_hsm_component() noexcept;

    /// Checks if the child can be added to the parent to avoid recursive loop
    /// (ie. a component child which need the same component in sub-child).
    bool can_add(const component& parent,
                 const component& child) const noexcept;

    child& alloc(generic_component& parent, dynamics_type type) noexcept;
    child& alloc(generic_component& parent, component_id id) noexcept;

    status copy(internal_component src, component& dst) noexcept;
    status copy(const component& src, component& dst) noexcept;
    status copy(grid_component& grid, component& dst) noexcept;

    status copy(const generic_component& src, generic_component& dst) noexcept;
    status copy(grid_component& grid, generic_component& s) noexcept;
    status copy(graph_component& grid, generic_component& s) noexcept;

    status save(component& c) noexcept;

    log_manager log_entries;

    spin_mutex reg_paths_mutex;
    spin_mutex dir_paths_mutex;
    spin_mutex file_paths_mutex;
};

class project
{
public:
    struct hsm_error {};

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

    auto head() const noexcept -> component_id;
    auto tn_head() const noexcept -> tree_node*;

    auto node(tree_node_id id) const noexcept -> tree_node*;
    auto node(tree_node& node) const noexcept -> tree_node_id;
    auto node(const tree_node& node) const noexcept -> tree_node_id;

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
        struct model_port {
            model_port() noexcept = default;

            model_port(model* mdl_, int port_) noexcept
              : mdl{ mdl_ }
              , port{ port_ }
            {}

            model* mdl{};
            int    port{};
        };

        vector<tree_node*> stack;
        vector<model_port> inputs;
        vector<model_port> outputs;

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

    id_data_array<global_parameter_id,
                  default_allocator,
                  name_str,
                  tree_node_id,
                  model_id,
                  parameter>
      parameters;

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

inline component::component() noexcept
{
    x.reserve(16);
    y.reserve(16);
    x_names.resize(16);
    y_names.resize(16);
}

inline connection::connection(child_id src_,
                              port     p_src_,
                              child_id dst_,
                              port     p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ p_src_ }
  , index_dst{ p_dst_ }
{}

inline connection::connection(child_id src_,
                              port_id  p_src_,
                              child_id dst_,
                              port_id  p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .compo = p_src_ }
  , index_dst{ .compo = p_dst_ }
{}

inline connection::connection(child_id src_,
                              int      p_src_,
                              child_id dst_,
                              port_id  p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .model = p_src_ }
  , index_dst{ .compo = p_dst_ }
{}

inline connection::connection(child_id src_,
                              port_id  p_src_,
                              child_id dst_,
                              int      p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .compo = p_src_ }
  , index_dst{ .model = p_dst_ }
{}

inline connection::connection(child_id src_,
                              int      p_src_,
                              child_id dst_,
                              int      p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .model = p_src_ }
  , index_dst{ .model = p_dst_ }
{}

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

/*
   Project part
 */

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

inline auto grid_component::build_error_handlers(log_manager& l) noexcept
{
    return std::make_tuple(
      [&](input_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](output_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_output_connection_error(e); });
      },
      [&](children_connection_error, e_memory mem) {
          l.push(log_level::error,
                 [&](auto& e) { format_children_connection_error(e, mem); });
      });
}

inline auto graph_component::build_error_handlers(log_manager& l) noexcept
{
    return std::make_tuple(
      [&](input_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](input_connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_full_error(e); });
      },
      [&](output_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_output_connection_error(e); });
      },
      [&](output_connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_output_connection_full_error(e); });
      },
      [&](children_error, e_memory mem) {
          l.push(log_level::error,
                 [&](auto& e) { format_children_error(e, mem); });
      });
}

inline auto generic_component::build_error_handlers(log_manager& l) noexcept
{
    return std::make_tuple(
      [&](connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](input_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_error(e); });
      },
      [&](input_connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_input_connection_full_error(e); });
      },
      [&](output_connection_error, already_exist_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_output_connection_error(e); });
      },
      [&](output_connection_error, container_full_error) {
          l.push(log_level::error,
                 [&](auto& e) { format_output_connection_full_error(e); });
      },
      [&](children_error, container_full_error) {
          l.push(log_level::error, [&](auto& e) { format_children_error(e); });
      });
}

} // namespace irt

#endif
