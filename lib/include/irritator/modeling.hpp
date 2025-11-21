// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <irritator/container.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/ext.hpp>
#include <irritator/file.hpp>
#include <irritator/global.hpp>
#include <irritator/helpers.hpp>
#include <irritator/macros.hpp>
#include <irritator/thread.hpp>

#include <optional>

namespace irt {

enum class port_id : u32;
enum class input_connection_id : u32;
enum class output_connection_id : u32;
enum class component_id : u32;
enum class hsm_component_id : u32;
enum class generic_component_id : u32;
enum class graph_component_id : u32;
enum class grid_component_id : u32;
enum class tree_node_id : u64;
enum class description_id : u64;
enum class child_id : u32;
enum class connection_id : u64;
enum class variable_observer_id : u64;
enum class grid_observer_id : u64;
enum class graph_observer_id : u64;
enum class global_parameter_id : u64;
enum class file_observer_id : u32;

enum class graph_node_id : irt::u32;
enum class graph_edge_id : irt::u32;

using port_str           = small_string<7>;
using description_str    = small_string<1022>;
using registred_path_str = small_string<256 * 16 - 2>;
using directory_path_str = small_string<512 - 2>;
using file_path_str      = small_string<512 - 2>;
using color              = u32;
using component_color    = std::array<float, 4>;

/// Maximum deepth of the component tree.
constexpr i32 max_component_stack_size = 16;

/// Stores the path from the head of the project to the model by following
/// the path of tree_node and/or component @c unique_id.
using unique_id_path = small_vector<name_str, max_component_stack_size>;

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

enum class description_status : u8 {
    unread,
    read_only,
    modified,
    unmodified,
};

enum class internal_component : u8 {
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

enum class component_type : u8 {
    none,    ///< The component does not reference any container.
    generic, ///< A classic component-model graph coupling.
    grid,    ///< Grid with 4, 8 neighbourhood.
    graph,   ///< Random graph generator
    hsm      ///< HSM component
};

constexpr int component_type_count = ordinal(component_type::hsm) + 1;

enum class component_status : u8 {
    unread,     ///< The component is not read (It is referenced by another
                ///< component).
    read_only,  ///< The component file is in read-only.
    modified,   ///< The component is not saved.
    unmodified, ///< or you show an internal component.
    unreadable  ///< When an error occurred during load-component.
};

enum class modeling_status : u8 { modified, unmodified };

class project;
struct connection;
class generic_component;
class grid_component;
class graph_component;
struct component;
class modeling;
struct cache_rw;
struct tree_node;
class variable_observer;
class grid_observer;
class graph_observer;

enum class child_flags : u8 {
    configurable,
    observable,
};

struct position {
    float x = 0.f;
    float y = 0.f;

    void reset() noexcept;
};

enum class port_option : u8 {
    classic, /**< Classic connection between two components. */
    sum,     /**< Sum of all inputs messages (Adding @a abstract_sum models to
                perform the sum for all input connections) between components. */
};

inline void position::reset() noexcept { x = y = 0.f; }

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

/**
   A wrapper to the simulation @c hierarchical_state_machine class.

   This component is different from others. It does not have any child nor
   connection. @c project class during import, the @c
   hierarchical_state_machine is copied into the simulation HSM data array. The
   parameter @c a and @c b are store into the @c children_parameters of the @c
   generic_component.
 */
class hsm_component
{
public:
    static constexpr auto max_size =
      hierarchical_state_machine::max_number_of_state;
    static constexpr auto invalid =
      hierarchical_state_machine::invalid_state_id;

    /**
      Clear the @c machine state, reinit constants, and reset the positions.
     */
    void clear() noexcept;

    hierarchical_state_machine     machine;
    std::array<position, max_size> positions;
    std::array<name_str, max_size> names;

    i32    i1      = 0;
    i32    i2      = 0;
    real   r1      = 0.0;
    real   r2      = 0.0;
    time   timeout = time_domain<time>::infinity;
    source src;
};

inline void hsm_component::clear() noexcept
{
    machine.clear();
    (void)machine.set_state(0); // @TODO Is it really necessary?

    std::fill_n(
      positions.begin(), positions.size(), position{ .x = 0.f, .y = 0.f });

    for (auto& str : names)
        str.clear();

    i1      = 0;
    i2      = 0;
    r1      = 0.0;
    r2      = 0.0;
    timeout = time_domain<time>::infinity;
    src.clear();
}

class generic_component
{
public:
    using child_limiter      = static_bounded_value<i32, 64, 64 * 16>;
    using connection_limiter = static_bounded_value<i32, 64 * 4, 64 * 16 * 4>;

    generic_component() noexcept;

    generic_component(const child_limiter      child_limit,
                      const connection_limiter connection_limit) noexcept;

    struct child {
        explicit child(dynamics_type type) noexcept;
        explicit child(component_id component) noexcept;

        union {
            dynamics_type mdl_type;
            component_id  compo_id;
        } id;

        child_type            type{ child_type::model };
        bitflags<child_flags> flags;
    };

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

    vector<position>  children_positions;
    vector<name_str>  children_names;
    vector<parameter> children_parameters;

    /** Grow the children @a data_array and resize theq
     *  @a children_positions, @a children_names and @a children_parameters
     *  vectors to match the new capacity.
     *
     *  @return `true` if the operation is successful, otherwise `false`.
     */
    [[nodiscard]] inline bool grow_children() noexcept
    {
        if (children.can_alloc(1))
            return true;

        return children.grow<2, 1>() and
               children_positions.resize(children.capacity()) and
               children_names.resize(children.capacity()) and
               children_parameters.resize(children.capacity());
    }

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
    expected<child_id> copy_to(const child&       c,
                               generic_component& dst) const noexcept;

    /// @brief Import children, connections and optionaly properties.
    ///
    /// @details Copy children and connections into the this
    /// `generic_component` and initialize of copy properties if exsits.
    ///
    /// @return `success()` or `modeling::connection_error`,
    /// `modeling::child_error`.
    status import(const modeling&            mod,
                  const component&           compo,
                  const std::span<position>  positions  = {},
                  const std::span<name_str>  names      = {},
                  const std::span<parameter> parameters = {}) noexcept;

    status import(const graph_component& graph) noexcept;
    status import(const grid_component& graph) noexcept;
    status import(const generic_component& graph) noexcept;

    bool     exists_child(const std::string_view name) const noexcept;
    name_str make_unique_name_id(const child_id from_id) const noexcept;
};

class grid_component
{
public:
    using limit  = bounded_value<i32>;
    using slimit = static_bounded_value<i32, 1, 1024>;

private:
    i32                  m_row    = slimit::lower_bound();
    i32                  m_column = slimit::lower_bound();
    vector<component_id> m_children;

public:
    enum class options : u8 { none = 0, row_cylinder, column_cylinder, torus };

    struct child {
        child(const component_id compo_id_,
              const i32          r_,
              const i32          c_) noexcept;

        component_id compo_id;
        i32          row = 0;
        i32          col = 0;

        bitflags<child_flags> flags;
    };

    enum class type : u8 {
        in_out, //!< Only one port "in" or "out".
        name,   //!< Cardinal points according to neighbor: "N", "S", "W", "E",
                //!< "NE", ...
        number  //!< A tuple of integers that represent neighborhood for
                //!< examples: (5,5,5) the middle in 3D and the (4,4,5) the
                //!< top-left cell. (5) the middle in 1D and (6) the
                //!< righ-cell.
    };

    enum class neighborhood : u8 {
        four,
        eight,
    };

    constexpr i32 row() const noexcept { return m_row; }
    constexpr i32 column() const noexcept { return m_column; }

    constexpr std::span<const component_id> children() const noexcept
    {
        return std::span(m_children.data(), m_children.size());
    }

    constexpr std::span<component_id> children() noexcept
    {
        return std::span(m_children.data(), m_children.size());
    }

    void resize(slimit rows, slimit cols, component_id id) noexcept
    {
        m_row    = rows;
        m_column = cols;

        m_children.resize(m_row * m_column);
        std::fill_n(m_children.data(), m_children.size(), id);
    }

    static inline constexpr auto type_count = 2;

    name_str make_unique_name_id(int row, int col) const noexcept;

    /**
     * @brief Compute the number of cells in the grid.
     * @return A integer between @a limit::lower_bound() squared to @a
     * limit::upper_bound() squared.
     */
    constexpr i32 cells_number() const noexcept { return m_column * m_row; }

    constexpr bool is_coord_valid(std::integral auto r,
                                  std::integral auto c) const noexcept
    {
        return std::cmp_greater_equal(r, 0) and std::cmp_greater_equal(c, 0) and
               std::cmp_less(r, slimit::upper_bound()) and
               std::cmp_less(c, slimit::upper_bound());
    }

    constexpr int pos(std::integral auto r, std::integral auto c) const noexcept
    {
        debug::ensure(is_coord_valid(r, c));

        return c * m_row + r;
    }

    constexpr std::pair<i32, i32> pos(std::integral auto p) const noexcept
    {
        debug::ensure(is_coord_valid(p % m_row, p / m_row));

        return std::make_pair(p % m_row, p / m_row);
    }

    constexpr u64 unique_id(i32 pos_) const noexcept
    {
        auto pair = pos(pos_);

        return u32s_to_u64(static_cast<u32>(pair.first),
                           static_cast<u32>(pair.second));
    }

    constexpr std::pair<int, int> unique_id(u64 id) const noexcept
    {
        auto unpack = u64_to_u32s(id);

        return std::make_pair(static_cast<int>(unpack.first),
                              static_cast<int>(unpack.second));
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
    expected<input_connection_id> connect_input(const port_id x,
                                                const i32     row,
                                                const i32     col,
                                                const port_id id) noexcept;

    //! @brief Tries to add this output connection if it does not already exist.
    //! @return `success()` or `connection_already_exists`.
    expected<output_connection_id> connect_output(const port_id y,
                                                  const i32     row,
                                                  const i32     col,
                                                  const port_id id) noexcept;

    data_array<input_connection, input_connection_id>   input_connections;
    data_array<output_connection, output_connection_id> output_connections;

    data_array<child, child_id>           cache;
    data_array<connection, connection_id> cache_connections;
    vector<name_str>                      cache_names;

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
    type         out_connection_type = type::in_out;
    neighborhood neighbors           = neighborhood::four;
};

/**
 * A generic graph structure to stores nodes, edges, properties.
 */
class graph
{
public:
    using edge = std::pair<graph_node_id, std::string_view>;

    enum class option_flags : u8 {
        strict,
        directed,
    };

    graph() noexcept = default;

    explicit graph(const graph& other) noexcept;
    explicit graph(graph&& other) noexcept;

    graph& operator=(const graph& other) noexcept;
    graph& operator=(graph&& other) noexcept;

    id_array<graph_node_id> nodes;
    id_array<graph_edge_id> edges;

    vector<std::string_view>     node_names;
    vector<std::string_view>     node_ids;
    vector<std::array<float, 3>> node_positions;
    vector<component_id>         node_components;
    vector<float>                node_areas;
    vector<std::array<edge, 2>>  edges_nodes;

    std::string_view main_id;

    string_buffer buffer; /**< Stores all strings from @a node_names, @a
                             node_ids and @a main_id.*/

    file_path_id file = undefined<file_path_id>();

    /// Default an unstrict undirected graph.
    bitflags<option_flags> flags;

    /** Reserve memory for nodes @a data_array and resize memory for @c
     * vector for at least @a i nodes.
     * @param n Nodes number.
     * @param e Edges number.
     * @return @a success() or an @a error_code.
     */
    expected<void> reserve(int n, int e) noexcept;

    /** Call @a clear or @a resize(0) for each containers.
     */
    void clear() noexcept;

    /** Swap the content of the graph with @a other. */
    void swap(graph& g) noexcept;

    expected<void> init_scale_free_graph(double            alpha,
                                         double            beta,
                                         component_id      id,
                                         int               nodes,
                                         std::span<u64, 4> seed,
                                         std::span<u64, 2> key) noexcept;

    expected<void> init_small_world_graph(double            probability,
                                          i32               k,
                                          component_id      id,
                                          int               nodes,
                                          std::span<u64, 4> seed,
                                          std::span<u64, 2> key) noexcept;

    /**
     * Add a new node in graph. Grow containers if necessary.
     * @return
     */
    expected<graph_node_id> alloc_node() noexcept;

    /**
     * Add a new edge in graph if the @a src and @a dst exists and the edge
     * doe not already exist.
     * @param src
     * @param dst
     * @return
     */
    expected<graph_edge_id> alloc_edge(graph_node_id src,
                                       graph_node_id dst) noexcept;

    /** Return true if an edge exists in vector.
     * The input and output ports names are not used in the comparison result.
     */
    bool exists_edge(graph_node_id src, graph_node_id dst) const noexcept
    {
        return std::any_of(
          edges.begin(), edges.end(), [&](const auto id) noexcept -> bool {
              return edges_nodes[id][0].first == src and
                     edges_nodes[id][1].first == dst;
          });
    }

    /**
     * @brief Build a @c irt::table from node name to node identifier.
     * This function use the @c node_names and @c nodes object, do not
     * change this object after building a toc.
     * @return A string-to-node table.
     */
    table<std::string_view, graph_node_id> make_toc() const noexcept;
};

/// random-graph type:
/// - scale_free: graph typically has a very skewed degree distribution,
///   where few vertices have a very high degree and a large number of vertices
///   have a very small degree. Many naturally evolving networks, such as
///   the World Wide Web, are scale-free graphs, making these graphs a good
///   model for certain networking problems.
/// - small_world: consists of a ring graph (where each vertex is connected to
///   its k nearest neighbors). Edges in the graph are randomly rewired to
///   different vertices with a probability p.
class graph_component
{
public:
    static inline constexpr i32 children_max = 4096;

    enum class graph_type : u8 { dot_file, scale_free, small_world };

    struct child {
        child(const component_id  compo_id_,
              const graph_node_id node_id_) noexcept;

        component_id  compo_id;
        graph_node_id node_id;

        bitflags<child_flags> flags;
    };

    struct input_connection {
        input_connection(port_id x_, graph_node_id v_, port_id id_) noexcept
          : x(x_)
          , v(v_)
          , id(id_)
        {}

        port_id       x;  // The port_id in this component.
        graph_node_id v;  // The index in children vector.
        port_id       id; // The port_id of the @c children[idx].
    };

    struct output_connection {
        output_connection(port_id y_, graph_node_id v_, port_id id_) noexcept
          : y(y_)
          , v(v_)
          , id(id_)
        {}

        port_id       y;  // The port_id in this component.
        graph_node_id v;  // The index in children vector.
        port_id       id; // The port_id of the @c children[idx].
    };

    enum class connection_type : u8 {
        in_out,      //!< Connect only output port 'out' to input port 'in'.
        name,        //!< Connect output port to input port with same name.
        name_suffix, //!< Connect output port to inport port with the same
                     //!< name and an integer.
    };

    struct dot_file_param {
        dir_path_id  dir  = undefined<dir_path_id>();
        file_path_id file = undefined<file_path_id>();

        void reset() noexcept
        {
            dir  = undefined<dir_path_id>();
            file = undefined<file_path_id>();
        }
    };

    struct scale_free_param {
        double       alpha = 2.5;
        double       beta  = 1.e3;
        component_id id    = undefined<component_id>();
        int          nodes = 32;

        void reset() noexcept
        {
            alpha = 2.5;
            beta  = 1.e3;
            id    = undefined<component_id>();
            nodes = 32;
        }
    };

    struct small_world_param {
        double       probability = 3e-2;
        i32          k           = 6;
        component_id id          = undefined<component_id>();
        int          nodes       = 32;

        void reset() noexcept
        {
            probability = 3e-2;
            k           = 6;
            id          = undefined<component_id>();
            nodes       = 32;
        }
    };

    union random_graph_param {
        dot_file_param    dot;
        scale_free_param  scale;
        small_world_param small;
    };

    graph g;

    data_array<input_connection, input_connection_id>   input_connections;
    data_array<output_connection, output_connection_id> output_connections;

    random_graph_param param   = { .scale = scale_free_param{} };
    graph_type         g_type  = graph_type::scale_free;
    u64                seed[4] = { 0u, 0u, 0u, 0u };
    u64                key[2]  = { 0u, 0u };

    data_array<child, child_id>           cache;
    data_array<connection, connection_id> cache_connections;
    vector<name_str>                      cache_names;

    std::array<float, 2> top_left_limit{ +INFINITY, +INFINITY };
    std::array<float, 2> bottom_right_limit{ -INFINITY, -INFINITY };

    connection_type type = connection_type::name;

    graph_component() noexcept                             = default;
    graph_component(const graph_component& other) noexcept = default;

    bool     exists_child(const std::string_view name) const noexcept;
    name_str make_unique_name_id(const graph_node_id v) const noexcept;

    /** Compute top-left and bottom-right limits according to the position and
     * the area of each nodes. */
    void update_position() noexcept;

    /** Assign for each node a position based on a grid.
     * @param distance_x The distance between node x position.
     * @param distance_y The distance between node y position.
     */
    void assign_grid_position(float distance_x = 10.f,
                              float distance_y = 10.f) noexcept;

    /** Assign top-left and bottom, right limits in infinity position. */
    void reset_position() noexcept;

    //! @brief Check if the input connection already exits.
    bool exists_input_connection(const port_id       x,
                                 const graph_node_id v,
                                 const port_id       id) const noexcept;

    //! @brief Check if the output connection already exits.
    bool exists_output_connection(const port_id       y,
                                  const graph_node_id v,
                                  const port_id       id) const noexcept;

    //! @brief Tries to add this input connection if it does not already
    //! exist.
    //! @return `success()` or `connection_already_exists`.
    expected<input_connection_id> connect_input(const port_id       x,
                                                const graph_node_id v,
                                                const port_id id) noexcept;

    //! @brief Tries to add this output connection if it does not already
    //! exist.
    //! @return `success()` or `connection_already_exists`.
    expected<output_connection_id> connect_output(const port_id       y,
                                                  const graph_node_id v,
                                                  const port_id id) noexcept;

    //! clear the @c cache and @c cache_connection data_array.
    void clear_cache() noexcept;

    //! build the @c cache and @c cache_connection data_array according to
    //! current attributes @c children, @c edges and @c random_graph_param.
    //!
    //! @param mod Necessary to read, check and build components and
    //! connections.
    //! @return success() or @c project::error::not_enough_memory.
    expected<void> build_cache(modeling& mod) noexcept;
};

/// A connection pack makes a link between a X or Y port of a component to a
/// pair of component identifier and port identifier in the children component.
struct connection_pack {
    /// The input or output port.
    port_id parent_port = undefined<port_id>();

    /// The component identifier to be search in component children (grid,
    /// graph, generic children vectors).
    port_id child_port = undefined<port_id>();

    /// The port identifier in the component @a component.
    component_id child_component;
};

struct component {

    using port_type = id_data_array<void,
                                    port_id,
                                    allocator<new_delete_memory_resource>,
                                    port_option,
                                    port_str,
                                    position>;

    component() noexcept;

    /** Stores input ports with names and positions. */
    port_type x;

    /** Stores output ports with names and positions. */
    port_type y;

    /** Stores input connection pack (links input port with all component
     * children with @a component_port::component identifier and @a
     * component_port::port port identifier. */
    vector<connection_pack> input_connection_pack;

    /** Stores input connection pack (links input port with all component
     * children with @a component_port::component identifier and @a
     * component_port::port port identifier. */
    vector<connection_pack> output_connection_pack;

    /** Get the port identifier of the input port with the name @a str.
     * @param str The name of the input port.
     * @return The port identifier or @c undefined<port_id>() if not found.
     */
    port_id get_x(std::string_view str) const noexcept;

    /** Get the port identifier of the output port with the name @a str.
     * @param str The name of the input port.
     * @return The port identifier or @c undefined<port_id>() if not found.
     */
    port_id get_y(std::string_view str) const noexcept;

    /** Get or add the input port with the name @a str.
     * If the port does not exist, it is created and added to the @c x
     * data_array.
     * @param str The name of the input port.
     * @return The port identifier or @c undefined<port_id>() if allocation
     * fails.
     */
    port_id get_or_add_x(std::string_view str) noexcept;

    /** Get or add the output port with the name @a str.
     * If the port does not exist, it is created and added to the @c x
     * data_array.
     * @param str The name of the output port.
     * @return The port identifier or @c undefined<port_id>() if allocation
     * fails.
     */
    port_id get_or_add_y(std::string_view str) noexcept;

    description_id    desc     = description_id{ 0 };
    registred_path_id reg_path = registred_path_id{ 0 };
    dir_path_id       dir      = dir_path_id{ 0 };
    file_path_id      file     = file_path_id{ 0 };
    name_str          name;

    /**
      Checks if the component have registred_path, dir_path and file_path
      defined. @attention This function does not check if the file can be
      saved.

      @return True paths attributes are defined.
     */
    constexpr bool is_file_defined() const noexcept
    {
        return is_defined(reg_path) and is_defined(dir) and is_defined(file);
    }

    union id {
        generic_component_id generic_id;
        grid_component_id    grid_id;
        graph_component_id   graph_id;
        hsm_component_id     hsm_id;
    } id = {};

    component_type   type  = component_type::none;
    component_status state = component_status::unread;

    /**< Each component stores potential external source. */
    external_source srcs;
};

struct registred_path;
struct dir_path;
struct file_path;

struct registred_path {
    enum class state : u8 {
        lock,   /**< `dir-path` is locked during I/O operation. Do not use
                   this class in writing mode. */
        read,   /**< underlying directory is read and the `children` vector is
                   filled. */
        unread, /**< underlying directory is not read. */
        error,  /**< an error occurred during the read. */
    };

    enum class reg_flags : u8 {
        access_error,
        read_only,
    };

    /**
     * Linear search a directory with the name @a dir_name in the @a
     * children vector.
     * @param data A @a data_array of @a dir_path.
     * @param dir_name The name to search.
     * @return The directory identifier or @a undefined otherwise.
     */
    dir_path_id search(const data_array<dir_path, dir_path_id>& data,
                       const std::string_view dir_name) noexcept;

    /**
     * Returns true if a directory with the name @a dir_name in the @a
     * children vector exists in this @a registred_path.
     * @param data A @a data_array of @a dir_path.
     * @param dir_name The name to search.
     * @return @a true is the directory exists, false otherwise.
     */
    bool exists(const data_array<dir_path, dir_path_id>& data,
                const std::string_view                   dir_name) noexcept;

    registred_path_str path; /**< Stores an absolute path in utf8 format. */
    name_str           name; /**< Stores a user name, the same name as in the
                                configuration file. */
    vector<dir_path_id> children;

    state               status = state::unread;
    bitflags<reg_flags> flags;
    i8                  priority = 0;
    spin_mutex          mutex;
};

struct dir_path {
    enum class state : u8 {
        lock,   /**< `dir-path` is locked during I/O operation. Do not use
                   this class in writing mode. */
        read,   /**< underlying directory is read and the `children` vector is
                   filled. */
        unread, /**< underlying directory is not read. */
        error,  /**< an error occurred during the read. */
    };

    enum class dir_flags : u8 {
        too_many_file,
        access_error,
        read_only,
    };

    directory_path_str   path; /**< stores a directory name in utf8. */
    registred_path_id    parent{ 0 };
    vector<file_path_id> children;

    state               status = state::unread;
    bitflags<dir_flags> flags;
    spin_mutex          mutex;

    /**
     * Refresh the `children` vector with new file in the filesystem.
     *
     * Files that disappear from the filesystem are not remove from the
     * vector but a flag is added in the `file_path` to indicate an absence
     * of existence in the filesystem.
     */
    vector<file_path_id> refresh(modeling& mod) noexcept;
};

struct file_path {
    enum class state : u8 {
        lock,   /**< `file-path` is locked during I/O operation. Do not use
                   this   class in writing mode. */
        read,   /**< underlying file is read. */
        unread, /**< underlying file  is not read. */
    };

    enum class file_flags : u8 {
        access_error,
        read_only,
    };

    enum class file_type : u8 {
        undefined_file,
        irt_file,
        dot_file,
        txt_file,
        data_file
    };

    file_path_str path; /**< stores the file name as utf8 string. */
    dir_path_id   parent{ 0 };
    component_id  component{ 0 };

    file_type            type{ file_type::undefined_file };
    state                status = state::unread;
    bitflags<file_flags> flags;
    spin_mutex           mutex;
};

struct tree_node {
    tree_node(component_id id_, const std::string_view unique_id_) noexcept;

    /// Intrusive hierarchy to the children, sibling and parent @c
    /// tree_node.
    hierarchy<tree_node> tree;

    /// Reference the current component
    component_id id = undefined<component_id>();

    struct child_node {
        union {
            model_id   mdl; /* `mdl_id` Always valid in `data_array`. */
            tree_node* tn;  /* `tn_id` is valid in `data_array`. */
        };

        enum class type : u8 { empty, model, tree_node } type = type::empty;

        constexpr auto is_empty() const noexcept { return type == type::empty; }
        constexpr auto is_model() const noexcept { return type == type::model; }
        constexpr auto is_tree_node() const noexcept
        {
            return type == type::tree_node;
        }

        void disable() noexcept
        {
            mdl  = undefined<model_id>();
            type = type::empty;
        }

        void set(const model_id id) noexcept
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

    //! Filled during the `project::set` or `project::rebuild` functions,
    //! the size of this vector is the same as
    //! `generic_component::children`, `grid_component::cache` or
    //! `graph_component::cache` capacity. Use the `get_index(child_id)` to
    //! get the correct value.
    vector<child_node> children;

    bool is_model(const child_id id) const noexcept
    {
        return children[id].type == child_node::type::model;
    }

    bool is_tree_node(const child_id id) const noexcept
    {
        return children[id].type == child_node::type::tree_node;
    }

    /// A unique identifier provided by component parent.
    name_str unique_id;

    struct name_str_compare {
        bool operator()(const auto& left, const auto& right) const noexcept
        {
            if constexpr (std::is_same_v<decltype(left), name_str>) {
                if constexpr (std::is_same_v<decltype(right), name_str>) {
                    return left.sv() < right.sv();
                } else {
                    return left.sv() < right;
                }
            } else {
                if constexpr (std::is_same_v<decltype(right), name_str>) {
                    return left < right.sv();
                } else {
                    return left < right;
                }
            }
        }
    };

    table<name_str, tree_node_id, name_str_compare> unique_id_to_tree_node_id;
    table<name_str, model_id, name_str_compare>     unique_id_to_model_id;
    table<model_id, name_str>                       model_id_to_unique_id;

    table<name_str, global_parameter_id, name_str_compare> parameters_ids;
    table<name_str, variable_observer_id, name_str_compare>
      variable_observer_ids;

    vector<graph_observer_id> graph_observer_ids;
    vector<grid_observer_id>  grid_observer_ids;

    auto get_model_id(const std::string_view u_id) const noexcept
      -> std::optional<model_id>
    {
        auto* ptr = unique_id_to_model_id.get(u_id);

        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_tree_node_id(const std::string_view u_id) const noexcept
      -> std::optional<tree_node_id>
    {
        auto* ptr = unique_id_to_tree_node_id.get(u_id);

        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_unique_id(const model_id mdl_id) const noexcept -> std::string_view
    {
        auto it = std::find_if(
          unique_id_to_model_id.data.begin(),
          unique_id_to_model_id.data.end(),
          [mdl_id](const auto& elem) noexcept { return elem.value == mdl_id; });

        return it == unique_id_to_model_id.data.end() ? std::string_view{}
                                                      : it->id.sv();
    }

    auto get_unique_id(const tree_node_id tn_id) const noexcept
      -> std::string_view
    {
        auto it = std::find_if(
          unique_id_to_tree_node_id.data.begin(),
          unique_id_to_tree_node_id.data.end(),
          [tn_id](const auto& elem) noexcept { return elem.value == tn_id; });

        return it == unique_id_to_tree_node_id.data.end() ? std::string_view{}
                                                          : it->id.sv();
    }
};

class grid_observer
{
public:
    name_str name;

    tree_node_id parent_id; //!< @c tree_node identifier ancestor of the
                            //!< model a grid component.
    component_id compo_id;  //!< @c component in the grid to observe.
    tree_node_id tn_id;     //!< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //!< @c model to observe.

    vector<observer_id>         observers;
    shared_buffer<vector<real>> values;

    time tn = 0;

    static_bounded_floating_point<float, 1, 100, 1, 1> time_step = 0.1f;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    void init(project& pj, modeling& mod, simulation& sim) noexcept;

    // Clear the `observers` and `values` vectors.
    void clear() noexcept;

    /** Check if the simulation time is greater than wake up time @c tn. */
    bool can_update(const time t) const noexcept { return t > tn; }

    // For each `observer`, get the latest observation value and fill the
    // values vector.
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

    tree_node_id parent_id; ///< @c tree_node identifier ancestor of the
                            ///< model. A graph component.
    component_id compo_id;  //< @c component in the graph to observe.
    tree_node_id tn_id;     //< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //< @c model to observe.

    vector<observer_id>         observers;
    shared_buffer<vector<real>> values;

    time tn = 0;

    static_bounded_floating_point<float, 1, 100, 1, 1> time_step = 0.1f;

    // Build or reuse existing observer for each pair `tn_id`, `mdl_id` and
    // reinitialize all buffers.
    void init(project& pj, modeling& mod, simulation& sim) noexcept;

    // Clear the `observers` and `values` vectors.
    void clear() noexcept;

    /** Check if the simulation time is greater than wake up time @c tn. */
    bool can_update(const time t) const noexcept { return t > tn; }

    // For each `observer`, get the latest observation value and fill the
    // values vector.
    void update(const simulation& sim) noexcept;

    float scale_min = -100.f;
    float scale_max = +100.f;
    i32   color_map = 0;
};

class variable_observer
{
public:
    spin_mutex mutex; //!< To write-protect the swap between buffers
                      //!< (values and values_2nd).

    enum class type_options : u8 {
        none,
        line,
        dash,
    };

    name_str                               name;
    static_bounded_value<i32, 8, 64>       max_observers          = 8;
    static_bounded_value<i32, 8, 512>      raw_buffer_size        = 64;
    static_bounded_value<i32, 1024, 65536> linearized_buffer_size = 32768;
    static_bounded_floating_point<float, 1, 100, 1, 10> time_step = .01f;

    time tn = 0;

    enum class sub_id : u32;
    shared_buffer<vector<double>>
      values; //!< The last value of the observation.

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
    //! @details Build or reuse existing observer in `obs_id` vector for
    //! each pair `tn_id` and `mdl_id` and (re)initialize buffer reserve
    //! buffer.
    status init(project& pj, simulation& sim) noexcept;

    //! @brief Fill the `observer_id` vector with undefined value.
    void clear() noexcept;

    //! @brief Search a `sub_id`  `tn` and `mdl`.
    sub_id find(const tree_node_id tn, const model_id mdl) noexcept;
    bool   exists(const tree_node_id tn) noexcept;

    //! @brief Remove `sub_id` for all ` m_tn_ids` equal to `tn` and
    //! `mdl_ids` equal to `mdl`.
    //!
    //! @details Do nothing if `tn` and `mdl` is not found.
    void erase(const tree_node_id tn, const model_id mdl) noexcept;

    //! @brief Remove a `sub_id` from `id_array`.
    void erase(const sub_id id) noexcept;

    //! @brief Push data in all vectors if pair (`tn`, `mdl`) does not
    //! already exists.
    sub_id push_back(const tree_node_id tn,
                     const model_id     mdl,
                     const color                 = 0xFe1a0Fe,
                     const type_options     t    = type_options::line,
                     const std::string_view name = std::string_view{}) noexcept;

    bool     exists(const sub_id id) const noexcept { return m_ids.exists(id); }
    unsigned size() const noexcept { return m_ids.size(); }
    int      ssize() const noexcept { return m_ids.ssize(); }

    template<typename Function>
    void if_exists_do(const sub_id id, Function&& fn) noexcept
    {
        if (m_ids.exists(id))
            std::invoke(std::forward<Function>(fn), id);
    }

    template<typename Function>
    void for_each(Function&& fn) noexcept
    {
        for (const auto id : m_ids)
            std::invoke(std::forward<Function>(fn), id);
    }

    template<typename Function>
    void for_each(Function&& fn) const noexcept
    {
        for (const auto id : m_ids)
            std::invoke(std::forward<Function>(fn), id);
    }

    std::span<tree_node_id>       get_tn_ids() noexcept;
    std::span<const tree_node_id> get_tn_ids() const noexcept;
    std::span<model_id>           get_mdl_ids() noexcept;
    std::span<const model_id>     get_mdl_ids() const noexcept;
    std::span<observer_id>        get_obs_ids() noexcept;
    std::span<const observer_id>  get_obs_ids() const noexcept;
    std::span<name_str>           get_names() noexcept;
    std::span<const name_str>     get_names() const noexcept;
    std::span<color>              get_colors() noexcept;
    std::span<const color>        get_colors() const noexcept;
    std::span<type_options>       get_options() noexcept;
    std::span<const type_options> get_options() const noexcept;
};

struct modeling_reserve_definition {
    constrained_value<int, 512, INT_MAX> components{};
    constrained_value<int, 512, INT_MAX> grid_compos{};
    constrained_value<int, 512, INT_MAX> graph_compos{};
    constrained_value<int, 512, INT_MAX> generic_compos{};
    constrained_value<int, 512, INT_MAX> hsm_compos{};
    constrained_value<int, 16, INT_MAX>  regs{};
    constrained_value<int, 32, INT_MAX>  dirs{};
    constrained_value<int, 512, INT_MAX> files{};
};

class modeling
{
public:
    /** Stores the description of a component in a text. A description is
     * attached to only one component (@c description_id). The file name of
     * the description is the same than the component except the extension
     * ".desc".
     * @attention The size of the buffer is static for now. */
    id_data_array<void,
                  description_id,
                  allocator<new_delete_memory_resource>,
                  description_str,
                  description_status>
      descriptions;

    data_array<generic_component, generic_component_id> generic_components;
    data_array<grid_component, grid_component_id>       grid_components;
    data_array<graph_component, graph_component_id>     graph_components;
    data_array<hsm_component, hsm_component_id>         hsm_components;

    id_data_array<void,
                  component_id,
                  allocator<new_delete_memory_resource>,
                  component,
                  component_color>
                                                   components;
    data_array<registred_path, registred_path_id>  registred_paths;
    data_array<dir_path, dir_path_id>              dir_paths;
    data_array<file_path, file_path_id>            file_paths;
    data_array<hierarchical_state_machine, hsm_id> hsms;
    data_array<graph, graph_id>                    graphs;

    vector<registred_path_id> component_repertories;

    modeling_status state = modeling_status::unmodified;

    //! Ctor parameters are used to initialized the default stock of all
    //! data-array and observations objects.
    //!
    //! \param jnl
    //! \param components
    //! \param grid
    //! \param graph
    //! \param generic
    //! \param hsms
    //! \param regs
    //! \param dirs
    //! \param files
    //!
    modeling(journal_handler&                   jnl,
             const modeling_reserve_definition& res =
               modeling_reserve_definition()) noexcept;

    //! Reads the component @c compo and all dependencies recursively.
    status load_component(component& compo) noexcept;

    //! Reads all registered paths and search component files.
    status fill_components() noexcept;

    //! Adds a new path to read and search component files.
    status fill_components(registred_path& path) noexcept;

    /** Search a component from three string.
     *
     *  @return @c component_id found or @c undefined<component_id>();
     */
    component_id search_component_by_name(std::string_view reg,
                                          std::string_view dir,
                                          std::string_view file) const noexcept;

    /** Search a @a graph object into modeling.
     * @param dir_id Directory identifier where the dot file exists.
     * @param file_id File identifier of the dot file.
     * @return The found @a graph_id in modeling.
     */
    auto search_graph_id(const dir_path_id  dir_id,
                         const file_path_id file_id) const noexcept -> graph_id;

    /// Clear and free all dependencies of the component but let the
    /// component alive.
    void clear(component& c) noexcept;

    /// Deletes the component, the file (@c file_path_id) and the
    /// description
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
    void remove_file(const file_path& file) noexcept;

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

    /// Checks if the child can be added to the parent to avoid recursive
    /// loop (ie. a component child which need the same component in
    /// sub-child).
    bool can_add(const component& parent,
                 const component& other) const noexcept;

    generic_component::child& alloc(generic_component& parent,
                                    dynamics_type      type) noexcept;
    generic_component::child& alloc(generic_component& parent,
                                    component_id       id) noexcept;

    status copy(const internal_component src,
                component&               dst,
                generic_component&       g) noexcept;
    status copy(const component& src, component& dst) noexcept;

    status copy(const generic_component& src, generic_component& dst) noexcept;
    status copy(grid_component& grid, generic_component& s) noexcept;
    status copy(graph_component& grid, generic_component& s) noexcept;

    status save(component& c) noexcept;

    journal_handler& journal;

    spin_mutex reg_paths_mutex;
    spin_mutex dir_paths_mutex;
    spin_mutex file_paths_mutex;
};

class file_observers
{
public:
    /** Increases size of the @c id_array and all sub vectors. */
    void grow() noexcept;

    /** Clears the id_array and all buffers. After this function @c
     * ids.size() equals zero, the buffered files are are reseted. */
    void clear() noexcept;

    /** For each variable_observers, grid_observers and graph_observers from
     * @c project try to initialize the @c buffered_file in @c files. */
    void initialize(const simulation& sim,
                    project&          pj,
                    std::string_view  output_dir) noexcept;

    /** Check if the @c tn is lower than @c t. */
    bool can_update(const time t) const noexcept;

    /** For each @c file_observer_id, flush data into the open files */
    void update(const simulation& sim, const project& pj) noexcept;

    /** For each @c buffered_file in @c files, close the opening file. */
    void finalize() noexcept;

    template<typename T>
        requires(std::is_same_v<T, grid_observer_id> or
                 std::is_same_v<T, graph_observer_id> or
                 std::is_same_v<T, variable_observer_id>)
    bool alloc(const T id, bool enable = true) noexcept;

    union id_type {
        variable_observer_id var;
        grid_observer_id     grid;
        graph_observer_id    graph;
    };

    enum class type : u8 { variables, grid, graph };

    id_array<file_observer_id> ids;
    vector<buffered_file>      files;
    vector<id_type>            subids;
    vector<type>               types;
    vector<bool>               enables;

    static_bounded_floating_point<float, 1, 10000, 1, 1> time_step = 1.f;

    time tn = 0;
};

template<typename T>
    requires(std::is_same_v<T, grid_observer_id> or
             std::is_same_v<T, graph_observer_id> or
             std::is_same_v<T, variable_observer_id>)
inline bool file_observers::alloc(const T subobs_id, bool enable) noexcept
{
    if (not ids.can_alloc(1))
        grow();

    if (not ids.can_alloc(1))
        return false;

    const auto id  = ids.alloc();
    const auto idx = get_index(id);

    enables[idx] = enable;

    if constexpr (std::is_same_v<T, grid_observer_id>) {
        subids[idx].grid = subobs_id;
        types[idx]       = file_observers::type::grid;
    }

    if constexpr (std::is_same_v<T, graph_observer_id>) {
        subids[idx].graph = subobs_id;
        types[idx]        = file_observers::type::graph;
    }

    if constexpr (std::is_same_v<T, variable_observer_id>) {
        subids[idx].var = subobs_id;
        types[idx]      = file_observers::type::variables;
    }

    return true;
}

struct project_reserve_definition {
    constrained_value<int, 256, INT_MAX> nodes;
    constrained_value<int, 256, INT_MAX> grids;
    constrained_value<int, 256, INT_MAX> graphs;
    constrained_value<int, 256, INT_MAX> vars;
};

class project
{
public:
    //! Ctor parameters are used to initialized the default stock of
    //! simulation and observations objects.
    //!
    //! \param models Stocks of models in the simulation.
    //! \param nodes Stocks of simulation tree-nodes or coupled models.
    //! \param grids Stocks of grid observations.
    //! \param graphs Stocks of graph observations.
    //! \param vars Stocks of variables observations.
    //! \param parameters  Stocks of parameters in the simulation.
    //!
    project(
      const project_reserve_definition&    res = project_reserve_definition(),
      const simulation_reserve_definition& sim_res =
        simulation_reserve_definition(),
      const external_source_reserve_definition& srcs_res =
        external_source_reserve_definition()) noexcept;

    struct required_data {
        unsigned tree_node_nb{ 1u };
        unsigned model_nb{ 0u };
        unsigned hsm_nb{ 0u };

        constexpr friend required_data operator+(
          const required_data lhs,
          const required_data rhs) noexcept
        {
            return { lhs.tree_node_nb + rhs.tree_node_nb,
                     lhs.model_nb + rhs.model_nb,
                     lhs.hsm_nb + rhs.hsm_nb };
        }

        constexpr required_data& operator+=(const required_data other) noexcept
        {
            tree_node_nb += other.tree_node_nb;
            model_nb += other.model_nb;
            hsm_nb += other.hsm_nb;
            return *this;
        }

        /** Apply boundaries for all values. */
        constexpr void fix() noexcept
        {
            tree_node_nb = std::clamp(tree_node_nb, 1u, UINT32_MAX >> 16);
            model_nb     = std::clamp(model_nb, 16u, UINT32_MAX >> 2);
            hsm_nb       = std::clamp(hsm_nb, 0u, UINT32_MAX >> 2);
        }
    };

    name_str   name;
    simulation sim;

    /** Compute the number of @c tree_node required to load the component @c
     * into the @c project and the number of @c irt::model and @c
     * irt::hierarchical_state_machine to fill the @C irt::simulation
     * structures. */
    required_data compute_memory_required(const modeling&  mod,
                                          const component& c) const noexcept;

    /// Assign a new @c component head. The previously allocated tree_node
    /// hierarchy is removed and a newly one is allocated.
    status set(modeling& mod, component& compo) noexcept;

    /// Build the complete @c tree_node hierarchy from the @c component
    /// head.
    status rebuild(modeling& mod) noexcept;

    /// Remove @c tree_node hierarchy and clear the @c component head.
    void clear() noexcept;

    auto head() const noexcept -> component_id;
    auto tn_head() const noexcept -> tree_node*;

    auto node(tree_node_id id) const noexcept -> tree_node*;
    auto node(tree_node& node) const noexcept -> tree_node_id;
    auto node(const tree_node& node) const noexcept -> tree_node_id;

    template<typename Function, typename... Args>
    void for_each_children(tree_node& tn,
                           Function&& f,
                           Args&&... args) noexcept;

    /// Return the size and the capacity of the tree_nodes data_array.
    auto tree_nodes_size() const noexcept -> std::pair<int, int>;

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

    void build_unique_id_path(const tree_node&       model_unique_id_parent,
                              const std::string_view model_unique_id,
                              unique_id_path&        out) noexcept;

    /** Search a model with name attribute equals to @c id from the root
     * tree-node (the top of the hierarchy). */
    auto get_model_path(const std::string_view id) const noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    /** Search a model from the @c path. The first element is the root node
     * (top of the hierarchy), next element are tree nodes and finally, at
     * the last position in the @c unique_id_path, the name of the model. */
    auto get_model_path(const unique_id_path& path) const noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    auto get_tn_id(const unique_id_path& path) const noexcept -> tree_node_id;

    data_array<tree_node, tree_node_id> tree_nodes;

    data_array<variable_observer, variable_observer_id> variable_observers;
    data_array<grid_observer, grid_observer_id>         grid_observers;
    data_array<graph_observer, graph_observer_id>       graph_observers;

    file_observers file_obs;

    id_data_array<void,
                  global_parameter_id,
                  allocator<new_delete_memory_resource>,
                  name_str,
                  tree_node_id,
                  model_id,
                  parameter>
      parameters;

    /**
       @brief Alloc a new variable observer and assign a name.
       @return The new instance. Be carreful, use `can_alloc()` before
       running this function to ensure allocation is possible.
     */
    variable_observer& alloc_variable_observer() noexcept;

    /**
       @brief Alloc a new grid observer and assign a name.
       @return The new instance. Be carreful, use `can_alloc()` before
       running this function to ensure allocation is possible.
     */
    grid_observer& alloc_grid_observer() noexcept;

    /**
       @brief Alloc a new graph observer and assign a name.
       @return The new instance. Be carreful, use `can_alloc()` before
       running this function to ensure allocation is possible.
     */
    graph_observer& alloc_graph_observer() noexcept;

    /** Get the observation directory used by all text observation
     * files. If the @c observation_dir is undefined this function returns an
     * empty string.
     *
     * @param mod The modeling object to get the observation directory.
     * @return A string_view to the observation directory.
     */
    std::string_view get_observation_dir(
      const irt::modeling& mod) const noexcept;

    registred_path_id
      observation_dir; /**< The output directory used by all text observation
                          file. If undefined, the current repertory is used. */
private:
    component_id m_head    = undefined<component_id>();
    tree_node_id m_tn_head = undefined<tree_node_id>();
};

/* ------------------------------------------------------------------
   Child part
 */

inline component::component() noexcept
{
    x.reserve(16);
    y.reserve(16);

    srcs.constant_sources.reserve(4);
    srcs.binary_file_sources.reserve(4);
    srcs.text_file_sources.reserve(4);
    srcs.random_sources.reserve(4);
}

inline port_id component::get_x(std::string_view str) const noexcept
{
    const auto& vec_name = x.get<port_str>();
    for (const auto elem : x)
        if (str == vec_name[elem].sv())
            return elem;

    return undefined<port_id>();
}

inline port_id component::get_y(std::string_view str) const noexcept
{
    const auto& vec_name = y.get<port_str>();
    for (const auto elem : y)
        if (str == vec_name[elem].sv())
            return elem;

    return undefined<port_id>();
}

inline port_id component::get_or_add_x(std::string_view str) noexcept
{
    auto port_id = get_x(str);

    if (is_undefined(port_id)) {
        if (x.can_alloc(1)) {
            port_id                  = x.alloc_id();
            x.get<port_str>(port_id) = str;
            x.get<position>(port_id).reset();
        }
    }

    return port_id;
}

inline port_id component::get_or_add_y(std::string_view str) noexcept
{
    auto port_id = get_y(str);

    if (is_undefined(port_id)) {
        if (y.can_alloc(1)) {
            port_id                  = y.alloc_id();
            y.get<port_str>(port_id) = str;
            y.get<position>(port_id).reset();
        }
    }

    return port_id;
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
  , index_src{ .compo = p_src_, .model = 0 }
  , index_dst{ .compo = p_dst_, .model = 0 }
{}

inline connection::connection(child_id src_,
                              int      p_src_,
                              child_id dst_,
                              port_id  p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .compo = {}, .model = p_src_ }
  , index_dst{ .compo = p_dst_, .model = 0 }
{}

inline connection::connection(child_id src_,
                              port_id  p_src_,
                              child_id dst_,
                              int      p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .compo = p_src_, .model = 0 }
  , index_dst{ .compo = {}, .model = p_dst_ }
{}

inline connection::connection(child_id src_,
                              int      p_src_,
                              child_id dst_,
                              int      p_dst_) noexcept
  : src{ src_ }
  , dst{ dst_ }
  , index_src{ .compo = {}, .model = p_src_ }
  , index_dst{ .compo = {}, .model = p_dst_ }
{}

inline grid_component::child::child(const component_id compo_id_,
                                    const i32          r_,
                                    const i32          c_) noexcept
  : compo_id(compo_id_)
  , row(r_)
  , col(c_)
{}

inline graph_component::child::child(const component_id  compo_id_,
                                     const graph_node_id node_id_) noexcept
  : compo_id(compo_id_)
  , node_id(node_id_)
{}

inline generic_component::child::child(dynamics_type type) noexcept
  : id{ .mdl_type = type }
  , type{ child_type::model }
{}

inline generic_component::child::child(component_id component) noexcept
  : id{ .compo_id = component }
  , type{ child_type::component }
{}

inline std::span<tree_node_id> variable_observer::get_tn_ids() noexcept
{
    return m_tn_ids;
}

inline std::span<const tree_node_id> variable_observer::get_tn_ids()
  const noexcept
{
    return m_tn_ids;
}

inline std::span<model_id> variable_observer::get_mdl_ids() noexcept
{
    return m_mdl_ids;
}

inline std::span<const model_id> variable_observer::get_mdl_ids() const noexcept
{
    return m_mdl_ids;
}

inline std::span<observer_id> variable_observer::get_obs_ids() noexcept
{
    return m_obs_ids;
}

inline std::span<const observer_id> variable_observer::get_obs_ids()
  const noexcept
{
    return m_obs_ids;
}

inline std::span<name_str> variable_observer::get_names() noexcept
{
    return m_names;
}

inline std::span<const name_str> variable_observer::get_names() const noexcept
{
    return m_names;
}

inline std::span<color> variable_observer::get_colors() noexcept
{
    return m_colors;
}

inline std::span<const color> variable_observer::get_colors() const noexcept
{
    return m_colors;
}

inline std::span<variable_observer::type_options>
variable_observer::get_options() noexcept
{
    return m_options;
}

inline std::span<const variable_observer::type_options>
variable_observer::get_options() const noexcept
{
    return m_options;
}

inline tree_node::tree_node(component_id           id_,
                            const std::string_view uid) noexcept
  : id(id_)
  , unique_id(uid)
{}

/*
   Project part
 */

template<typename Function, typename... Args>
inline void project::for_each_children(tree_node& tn,
                                       Function&& f,
                                       Args&&... args) noexcept
{
    auto* child = tn.tree.get_child();
    if (!child)
        return;

    vector<tree_node*> stack;
    stack.emplace_back(child);
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        std::invoke(
          std::forward<Function>(f), *child, std::forward<Args>(args)...);

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }
}

} // namespace irt

#endif
