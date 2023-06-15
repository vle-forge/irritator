// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <algorithm>
#include <cwchar>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <array>
#include <optional>
#include <utility>
#include <variant>

namespace irt {

enum class component_id : u64;
enum class simple_component_id : u64;
enum class grid_component_id : u64;
enum class tree_node_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;
enum class child_id : u64;
enum class connection_id : u64;
enum class registred_path_id : u64;
enum class plot_observer_id : u32;
enum class grid_observer_id : u32;
enum class global_parameter_id : u32;
enum class grid_parameter_id : u32;

constexpr i32 max_component_dirs = 64;

enum class child_type : i8
{
    model,
    component
};

enum class description_status
{
    unread,
    read_only,
    modified,
    unmodified,
};

enum class internal_component
{
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

enum class component_type
{
    none, //! The component does not reference any container.
    internal,
    simple,
    grid
    // graph
};

enum class component_status
{
    unread,     //!< The component is not read (It is referenced by another
                //!< component).
    read_only,  //!< The component file is in read-only.
    modified,   //!< The component is not saved.
    unmodified, //!< or you show an internal component.
    unreadable  //!< When an error occurred during load-component.
};

enum class modeling_status
{
    modified,
    unmodified
};

enum class observable_type
{
    none,
    file,
    plot,
    graph,
    grid,
};

class project;

struct connection;
struct child;
struct generic_component;
struct modeling;
struct description;
struct io_cache;
struct tree_node;
struct plot_observer;
struct grid_observer;

//! A structure use to cache data when read or write json component.
//! - @c buffer is used to store the full file content or output buffer.
//! - @c string_buffer is used when reading string.
//! - @c stack is used when parsing project file.
//! - other variable are used to link file identifier with new identifier.
struct io_cache
{
    vector<char> buffer;
    std::string  string_buffer;

    table<u64, u64> model_mapping;
    table<u64, u64> constant_mapping;
    table<u64, u64> binary_file_mapping;
    table<u64, u64> random_mapping;
    table<u64, u64> text_file_mapping;

    vector<i32> stack;

    void clear() noexcept;
};

//! Description store the description of a compenent in a text way. A
//! description is attached to only one component (@c description_id). The
//! filename is the same than the component
//! @c file_path but with the extension ".txt".
//!
//! @note  The size of the buffer is static for now
struct description
{
    small_string<1022> data;
    description_status status = description_status::unread;
};

using child_flags = u8;

enum child_flags_ : u8
{
    child_flags_none         = 0,
    child_flags_configurable = 1 << 0,
    child_flags_observable   = 1 << 1,
    child_flags_both = child_flags_configurable | child_flags_observable,
};

struct child
{
    child() noexcept;
    child(model_id model) noexcept;
    child(component_id component) noexcept;

    union
    {
        model_id     mdl_id;
        component_id compo_id;
    } id;

    child_type  type  = child_type::model;
    child_flags flags = child_flags_none;

    //! An identifier provided by the component parent to easily found a child
    //! in project. The value 0 means unique_id is undefined.
    u64 unique_id = 0;
};

struct child_position
{
    float x = 0.f;
    float y = 0.f;
};

struct connection
{
    connection() noexcept = default;

    enum class connection_type : i8
    {
        internal,
        input,
        output
    };

    struct internal_t
    {
        child_id src;
        child_id dst;
        i8       index_src;
        i8       index_dst;
    };

    struct input_t
    {
        child_id dst;
        i8       index;
        i8       index_dst;
    };

    struct output_t
    {
        child_id src;
        i8       index;
        i8       index_src;
    };

    connection_type type;

    union
    {
        internal_t internal;
        input_t    input;
        output_t   output;
    };
};

struct generic_component
{
    vector<child_id>      children;
    vector<connection_id> connections;

    //! Use to affect @c child::unique_id when the component is saved. The value
    //! 0 means unique_id is undefined. Mutable variable to allow function @c
    //! make_next_unique_id to be const and called in json const functions.
    mutable u64 next_unique_id = 1;

    u64 make_next_unique_id() const noexcept { return next_unique_id++; }
};

struct grid_component
{
    static inline constexpr i32 row_max    = 1024;
    static inline constexpr i32 column_max = 1024;

    i32 row    = 1;
    i32 column = 1;

    enum class options : i8
    {
        none = 0,
        row_cylinder,
        column_cylinder,
        torus
    };

    enum class type : i8
    {
        number, //!< Only one port for all neighbor.
        name    //!< One, two, three or four ports according to neighbor.
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

    vector<component_id> children;

    vector<child_id>      cache;
    vector<connection_id> cache_connections;

    options opts            = options::none;
    type    connection_type = type::name;
};

using color           = std::array<u8, 4>;
using component_color = std::array<float, 4>;

struct component
{
    static inline constexpr int port_number = 8;

    component() noexcept;

    std::array<small_string<7>, port_number> x_names;
    std::array<small_string<7>, port_number> y_names;

    table<i32, child_id> child_mapping_io;

    description_id    desc     = description_id{ 0 };
    registred_path_id reg_path = registred_path_id{ 0 };
    dir_path_id       dir      = dir_path_id{ 0 };
    file_path_id      file     = file_path_id{ 0 };
    small_string<31>  name;

    union id
    {
        internal_component  internal_id;
        simple_component_id simple_id;
        grid_component_id   grid_id;
    } id;

    component_type   type  = component_type::none;
    component_status state = component_status::modified;
};

struct registred_path
{
    static inline constexpr int path_buffer_len = 256 * 16 - 2;

    enum class state
    {
        none,
        read,
        unread,
        error,
    };

    small_string<path_buffer_len> path;
    small_string<31>              name;
    state                         status   = state::unread;
    i8                            priority = 0;

    vector<dir_path_id> children;
};

struct dir_path
{
    static inline constexpr int path_buffer_len = 256 - 1;

    enum class state
    {
        none,
        read,
        unread,
        error,
    };

    small_string<path_buffer_len> path;
    state                         status = state::unread;
    registred_path_id             parent{ 0 };

    vector<file_path_id> children;
};

struct file_path
{
    static inline constexpr int path_buffer_len = 256 - 1;

    small_string<path_buffer_len> path;
    dir_path_id                   parent{ 0 };
    component_id                  component{ 0 };
};

struct modeling_initializer
{
    i32 model_capacity              = 1024 * 1024;
    i32 tree_capacity               = 1024;
    i32 parameter_capacity          = 128 * 128;
    i32 description_capacity        = 128;
    i32 component_capacity          = 1024;
    i32 dir_path_capacity           = 32;
    i32 file_path_capacity          = 512;
    i32 children_capacity           = 1024 * 1024;
    i32 connection_capacity         = 1024 * 256;
    i32 port_capacity               = 4096;
    i32 constant_source_capacity    = 16;
    i32 binary_file_source_capacity = 16;
    i32 text_file_source_capacity   = 16;
    i32 random_source_capacity      = 16;

    u64 random_generator_seed = 1234567890;

    bool is_fixed_window_placement = true;
};

struct tree_node
{
    tree_node(component_id id_, u64 unique_id_) noexcept;

    //! Reference the current component
    component_id id = undefined<component_id>();

    //! A unique identifier provided by component parent.
    u64 unique_id = 0;

    hierarchy<tree_node> tree;

    //! Cache of model-id with observable tag.
    vector<u64> parameters;

    //! Cache of model-id with observable tag.
    vector<u64> observables;

    //! Map component children into simulation model. Table build in @c
    //! project::set or @c project::rebuild functions.
    table<child_id, model_id> child_to_sim;

    using node_v = std::variant<tree_node_id, model_id>;

    table<u64, node_v> nodes_v;

    auto get_model_id(const node_v v) const noexcept -> std::optional<model_id>
    {
        auto* ptr = std::get_if<model_id>(&v);
        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_model_id(u64 unique_id) const noexcept -> std::optional<model_id>
    {
        auto* ptr = nodes_v.get(unique_id);
        return ptr ? get_model_id(*ptr) : std::nullopt;
    }

    auto get_tree_node_id(const node_v v) const noexcept
      -> std::optional<tree_node_id>
    {
        auto* ptr = std::get_if<tree_node_id>(&v);
        return ptr ? std::make_optional(*ptr) : std::nullopt;
    }

    auto get_tree_node_id(u64 unique_id) const noexcept
      -> std::optional<tree_node_id>
    {
        auto* ptr = nodes_v.get(unique_id);
        return ptr ? get_tree_node_id(*ptr) : std::nullopt;
    }

    auto get_unique_id(const model_id mdl_id) const noexcept -> u64
    {
        for (auto x : nodes_v.data)
            if (auto* ptr = std::get_if<model_id>(&x.value);
                ptr && *ptr == mdl_id)
                return x.id;

        return 0;
    }

    auto get_unique_id(const tree_node_id tn_id) const noexcept -> u64
    {
        for (auto x : nodes_v.data)
            if (auto* ptr = std::get_if<tree_node_id>(&x.value);
                ptr && *ptr == tn_id)
                return x.id;

        return 0;
    }

    union node
    {
        node() noexcept = default;
        node(tree_node* tn_) noexcept;
        node(model* mdl_) noexcept;

        tree_node* tn;
        model*     mdl; // model in simluation models.
    };

    //! Stores for each component in children list the identifier of the
    //! tree_node. This variable allows to quickly build the connection
    //! network at build time.
    table<child_id, node> child_to_node;
};

struct global_access
{
    tree_node_id tn_id;  //< @c tree_node identifier parent of the model.
    model_id     mdl_id; //< @c model to observe.

    global_access() noexcept = default;

    void clear() noexcept;
    bool is_defined() const noexcept;
};

struct parent_access
{
    tree_node_id parent_id; //< @c tree_node identifier ancestor of the model.
    tree_node_id tn_id;     //< @c tree_node identifier parent of the model.
    model_id     mdl_id;    //< @c model to observe.

    parent_access() noexcept = default;

    void clear() noexcept;
    bool is_defined() const noexcept;
};

struct parameter
{
    std::array<real, 4> reals;
    std::array<i64, 4>  integers;

    //! Copy data from the vectors of this parameter to the simulation model.
    status init(model& mdl) const noexcept;

    //! Copy data from model to the vectors of this parameter.
    void init(const model& mdl) noexcept;

    void clear() noexcept;
};

struct grid_observer
{
    small_string<31> name;

    parent_access child;
};

struct plot_observer
{
    enum class type
    {
        line,
        dash,
    };

    small_string<31>      name;
    vector<global_access> children;
    vector<color>         colors;
    vector<type>          types;
};

struct grid_parameter
{
    small_string<31> name;

    parent_access child;
    parameter     param;
};

struct global_parameter
{
    small_string<31> name;

    vector<global_access> children;
    vector<parameter>     params;
};

struct log_entry
{
    constexpr static int buffer_size = 254;

    using string_t = small_string<buffer_size>;

    string_t  buffer;
    log_level level;
    status    st;
};

struct modeling
{
    data_array<description, description_id>            descriptions;
    data_array<generic_component, simple_component_id> simple_components;
    data_array<grid_component, grid_component_id>      grid_components;
    data_array<component, component_id>                components;
    data_array<registred_path, registred_path_id>      registred_paths;
    data_array<dir_path, dir_path_id>                  dir_paths;
    data_array<file_path, file_path_id>                file_paths;
    data_array<model, model_id>                        parameters;
    data_array<model, model_id>                        models;
    data_array<hierarchical_state_machine, hsm_id>     hsms;
    data_array<child, child_id>                        children;
    data_array<connection, connection_id>              connections;

    vector<child_position>   children_positions;
    vector<small_string<31>> children_names;
    vector<component_color>  component_colors;

    small_vector<registred_path_id, max_component_dirs> component_repertories;
    external_source                                     srcs;

    modeling_status state = modeling_status::unmodified;

    modeling() noexcept;

    status init(modeling_initializer& params) noexcept;

    status fill_internal_components() noexcept;
    status fill_components() noexcept;
    status fill_components(registred_path& path) noexcept;

    //! Clean data used as cache for simulation.
    void clean_simulation() noexcept;

    //! If the @c child references a model, model is freed, otherwise, do
    //! nothing. This function is useful to replace the content of a existing @c
    //! child
    void clear(child& c) noexcept;

    //! Deletes the component, the file (@c file_path_id) and the description
    //! (@c description_id) objects attached.
    void free(component& c) noexcept;
    void free(child& c) noexcept;
    void free(connection& c) noexcept;

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
    bool can_alloc_simple_component() const noexcept;

    component& alloc_grid_component() noexcept;
    component& alloc_simple_component() noexcept;

    //! For grid_component, build the children and connections
    //! based on children vectors and grid_component options (torus, cylinder
    //! etc.). The newly allocated child and connection are append to the output
    //! vectors. The vectors are not cleared.
    status build_grid_children_and_connections(grid_component&        grid,
                                               vector<child_id>&      ids,
                                               vector<connection_id>& cnts,
                                               i32 upper_limit = 0,
                                               i32 left_limit  = 0,
                                               i32 space_x     = 30,
                                               i32 space_y     = 50) noexcept;

    //! For grid_component, build the real children and connections grid
    //! based on default_chidren and specific_children vectors and
    //! grid_component options (torus, cylinder etc.).
    status build_grid_component_cache(grid_component& grid) noexcept;

    //! Delete children and connections from @c modeling for the @c
    //! grid_component cache.
    void clear_grid_component_cache(grid_component& grid) noexcept;

    //! Checks if the child can be added to the parent to avoid recursive loop
    //! (ie. a component child which need the same component in sub-child).
    bool can_add(const component& parent,
                 const component& child) const noexcept;

    child& alloc(generic_component& parent, dynamics_type type) noexcept;
    child& alloc(generic_component& parent, component_id id) noexcept;
    child& alloc(generic_component& parent, model_id id) noexcept;

    status copy(const generic_component& src, generic_component& dst) noexcept;
    status copy(internal_component src, component& dst) noexcept;
    status copy(const component& src, component& dst) noexcept;
    status copy(grid_component& grid, component& dst) noexcept;
    status copy(grid_component& grid, generic_component& s) noexcept;

    /**
     * @brief Try to connect the component input port and a child (model or
     * component) in a component.
     * @details This function checks:
     * - if a connection slot is available in the parent,
     * - if the underlying models port index are defined,
     * - if the underlying models are compatibles to coupling.
     *
     * @param parent The component parent of @c src and @c dst.
     * @param port_src The port index of the source.
     * @param dst The child destination
     * @param port_dst The port index of the destination.
     * @return status::success or any other error.
     */
    status connect_input(generic_component& parent,
                         i8                 port_src,
                         child_id           dst,
                         i8                 port_dst) noexcept;

    /**
     * @brief Try to connect the component output port and a child (model or
     * component) in a component.
     * @details This function checks:
     * - if a connection slot is available in the parent,
     * - if the underlying models port index are defined,
     * - if the underlying models are compatibles to coupling.
     *
     * @param parent The component parent of @c src and @c dst.
     * @param src The child source.
     * @param port_src The port index of the source.
     * @param port_dst The port index of the destination.
     * @return status::success or any other error.
     */
    status connect_output(generic_component& parent,
                          child_id           src,
                          i8                 port_src,
                          i8                 port_dst) noexcept;

    /**
     * @brief Try to connect two child (model or component) in a component.
     * @details This function checks:
     * - if a connection slot is available in the parent,
     * - if the underlying models port index are defined,
     * - if the underlying models are compatibles to coupling.
     *
     * @param parent The component parent of @c src and @c dst.
     * @param src The child source.
     * @param port_src The port index of the source.
     * @param dst The child destination
     * @param port_dst The port index of the destination.
     * @return status::success or any other error.
     */
    status connect(generic_component& parent,
                   child_id           src,
                   i8                 port_src,
                   child_id           dst,
                   i8                 port_dst) noexcept;

    status save(component& c) noexcept; // will call clean(component&) first.

    ring_buffer<log_entry> log_entries;
};

class project
{
public:
    status init(int size) noexcept;

    status load(modeling&   mod,
                simulation& sim,
                io_cache&   cache,
                const char* filename) noexcept;

    status save(modeling&   mod,
                simulation& sim,
                io_cache&   cache,
                const char* filename) noexcept;

    //! Assign a new @c component head. The previously allocated tree_node
    //! hierarchy is removed and a newly one is allocated.
    status set(modeling& mod, simulation& sim, component& compo) noexcept;

    //! Build the complete @c tree_node hierarchy from the @c component head.
    status rebuild(modeling& mod, simulation& sim) noexcept;

    //! Remove @c tree_node hierarchy and clear the @c component head.
    void clear() noexcept;

    //! For all @c tree_node remove the simulation mapping between modelling and
    //! simulation part (ie @c tree_node::sim variable).
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

    //! Return the size and the capacity of the tree_nodes data_array.
    auto tree_nodes_size() const noexcept -> std::pair<int, int>;

    //! Clear all vector and table in @c cache.
    void clear_cache() noexcept;

    //! Release all memory for vectors and tables in @c cache.
    void destroy_cache() noexcept;

    //! Used to cache memory allocation when user import a model into
    //! simulation. The memory cached can be reused using clear but memory
    //! cached can be completely free using the @c destroy_cache function.
    struct cache
    {
        vector<tree_node*>            stack;
        vector<std::pair<model*, i8>> inputs;
        vector<std::pair<model*, i8>> outputs;

        table<u64, constant_source_id>    constants;
        table<u64, binary_file_source_id> binary_files;
        table<u64, text_file_source_id>   text_files;
        table<u64, random_source_id>      randoms;
    };

    enum class observation_id : u32;

    /// Stores the path from the head of the project to the model by following
    /// the path of tree_node and/or component @c unique_id.
    using unique_id_path = small_vector<u64, 16>;

    void build_unique_id_path(const tree_node_id tn_id,
                              const model_id     mdl_id,
                              unique_id_path&    out) noexcept;

    void build_unique_id_path(const tree_node_id tn_id,
                              unique_id_path&    out) noexcept;

    void build_unique_id_path(const tree_node& model_unique_id_parent,
                              const u64        model_unique_id,
                              unique_id_path&  out) noexcept;

    auto get_model_path(u64 id) noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    auto get_model_path(const unique_id_path& path) noexcept
      -> std::optional<std::pair<tree_node_id, model_id>>;

    auto get_tn_id(const unique_id_path& path) noexcept
      -> std::optional<tree_node_id>;

    data_array<tree_node, tree_node_id>               m_tree_nodes;
    data_array<plot_observer, plot_observer_id>       plot_observers;
    data_array<grid_observer, grid_observer_id>       grid_observers;
    data_array<global_parameter, global_parameter_id> global_parameters;
    data_array<grid_parameter, grid_parameter_id>     grid_parameters;

private:
    component_id m_head    = undefined<component_id>();
    tree_node_id m_tn_head = undefined<tree_node_id>();

    cache m_cache;
};

inline void global_access::clear() noexcept
{
    tn_id  = undefined<tree_node_id>();
    mdl_id = undefined<model_id>();
}

inline bool global_access::is_defined() const noexcept
{
    return tn_id != undefined<tree_node_id>() and
           mdl_id != undefined<model_id>();
}

inline void parent_access::clear() noexcept
{
    parent_id = undefined<tree_node_id>();
    tn_id     = undefined<tree_node_id>();
    mdl_id    = undefined<model_id>();
}

inline bool parent_access::is_defined() const noexcept
{
    return parent_id != undefined<tree_node_id>() and
           tn_id != undefined<tree_node_id>() and
           mdl_id != undefined<model_id>();
}

/* ------------------------------------------------------------------
   Child part
 */

inline child::child() noexcept
  : id{ undefined<model_id>() }
  , type{ child_type::model }
{
}

inline child::child(model_id model) noexcept
  : type{ child_type::model }
{
    id.mdl_id = model;
}

inline child::child(component_id component) noexcept
  : type{ child_type::component }
{
    id.compo_id = component;
}

inline tree_node::tree_node(component_id id_, u64 unique_id_) noexcept
  : id(id_)
  , unique_id(unique_id_)
{
}

inline tree_node::node::node(tree_node* tn_) noexcept
  : tn(tn_)
{
}

inline tree_node::node::node(model* mdl_) noexcept
  : mdl(mdl_)
{
}

/*
   Project part
 */

inline auto project::head() const noexcept -> component_id { return m_head; }

inline auto project::tn_head() const noexcept -> tree_node*
{
    return m_tree_nodes.try_to_get(m_tn_head);
}

inline auto project::node(tree_node_id id) const noexcept -> tree_node*
{
    return m_tree_nodes.try_to_get(id);
}

inline auto project::node(tree_node& node) const noexcept -> tree_node_id
{
    return m_tree_nodes.get_id(node);
}

inline auto project::node(const tree_node& node) const noexcept -> tree_node_id
{
    return m_tree_nodes.get_id(node);
}

template<typename Function, typename... Args>
inline auto project::for_all_tree_nodes(Function&& f, Args... args) noexcept
{
    tree_node* tn = nullptr;
    while (m_tree_nodes.next(tn))
        return f(*tn, args...);
}

template<typename Function, typename... Args>
inline auto project::for_all_tree_nodes(Function&& f,
                                        Args... args) const noexcept
{
    const tree_node* tn = nullptr;
    while (m_tree_nodes.next(tn))
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

inline auto project::tree_nodes_size() const noexcept -> std::pair<int, int>
{
    return std::make_pair(m_tree_nodes.ssize(), m_tree_nodes.capacity());
}

} // namespace irt

#endif
