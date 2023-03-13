// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <array>
#include <optional>

namespace irt {

enum class component_id : u64;
enum class tree_node_id : u64;
enum class parameter_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;
enum class child_id : u64;
enum class connection_id : u64;
enum class registred_path_id : u64;
enum class simulation_tree_node_id : u64;

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

enum class component_type
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
    file,
    memory,
};

enum class component_status
{
    unread,
    read_only,
    modified,
    unmodified,
};

enum class modeling_status
{
    modified,
    unmodified
};

enum class observable_type
{
    none,
    single,
    multiple,
    space,
};

struct connection;
struct child;
struct component;
struct modeling;
struct description;

static constexpr inline const char* component_type_names[] = {
    "qss1_izhikevich",
    "qss1_lif",
    "qss1_lotka_volterra",
    "qss1_negative_lif",
    "qss1_seirs",
    "qss1_van_der_pol",
    "qss2_izhikevich",
    "qss2_lif",
    "qss2_lotka_volterra",
    "qss2_negative_lif",
    "qss2_seirs",
    "qss2_van_der_pol",
    "qss3_izhikevich",
    "qss3_lif",
    "qss3_lotka_volterra",
    "qss3_negative_lif",
    "qss3_seirs",
    "qss3_van_der_pol",
    "file",
    "memory",
};

//! Try to get the component type from a string. If the string is unknown,
//! optional returns \c std::nullopt.
auto get_component_type(std::string_view name) noexcept
  -> std::optional<component_type>;

status add_cpp_component_ref(const char* buffer,
                             modeling&   mod,
                             component&  parent) noexcept;
status add_file_component_ref(const char* buffer,
                              modeling&   mod,
                              component&  parent) noexcept;

struct description
{
    small_string<1024> data;
    description_status status = description_status::unread;
};

struct child
{
    child(model_id model) noexcept;
    child(component_id component) noexcept;

    small_string<32> name;
    u64              id;
    child_type       type;

    float x            = 0.f;
    float y            = 0.f;
    bool  configurable = false; // true: is public initialization
    bool  observable   = false; // true: is public observable
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

struct component
{
    static inline constexpr int port_number = 8;

    component() noexcept;

    component(const component&)            = delete;
    component& operator=(const component&) = delete;

    void clear() noexcept;

    bool can_alloc(int place = 1) const noexcept;
    bool can_alloc(dynamics_type type, int place = 1) const noexcept;

    template<typename Dynamics>
    bool can_alloc_dynamics(int place = 1) const noexcept;

    data_array<model, model_id>                    models;
    data_array<hierarchical_state_machine, hsm_id> hsms;
    data_array<child, child_id>                    children;
    data_array<connection, connection_id>          connections;

    std::array<small_string<7>, port_number> x_names;
    std::array<small_string<7>, port_number> y_names;

    table<i32, child_id> child_mapping_io;

    description_id    desc     = description_id{ 0 };
    registred_path_id reg_path = registred_path_id{ 0 };
    dir_path_id       dir      = dir_path_id{ 0 };
    file_path_id      file     = file_path_id{ 0 };
    small_string<32>  name;

    component_type   type  = component_type::memory;
    component_status state = component_status::modified;
};

struct registred_path
{
    enum class state
    {
        none,
        read,
        unread,
        error,
    };

    small_string<256 * 16> path;
    small_string<32>       name;
    state                  status   = state::unread;
    i8                     priority = 0;

    bool make() const noexcept;
    bool exists() const noexcept;

    vector<dir_path_id> children;
};

struct dir_path
{
    enum class state
    {
        none,
        read,
        unread,
        error,
    };

    small_string<256> path;
    state             status = state::unread;
    registred_path_id parent{ 0 };

    bool make() const noexcept;
    bool exists() const noexcept;

    vector<file_path_id> children;
};

struct file_path
{
    small_string<256> path;
    dir_path_id       parent{ 0 };
    component_id      component{ 0 };
};

struct modeling_initializer
{
    i32 model_capacity              = 4096;
    i32 tree_capacity               = 32;
    i32 parameter_capacity          = 128;
    i32 description_capacity        = 128;
    i32 component_capacity          = 512;
    i32 dir_path_capacity           = 32;
    i32 file_path_capacity          = 512;
    i32 children_capacity           = 512;
    i32 connection_capacity         = 4096;
    i32 port_capacity               = 4096;
    i32 constant_source_capacity    = 16;
    i32 binary_file_source_capacity = 16;
    i32 text_file_source_capacity   = 16;
    i32 random_source_capacity      = 16;

    u64 random_generator_seed = 1234567890;

    bool is_fixed_window_placement = true;
};

struct simulation_tree_node
{
    vector<model_id> children;

    hierarchy<simulation_tree_node> tree;
};

struct tree_node
{
    tree_node(component_id id_, child_id id_in_parent_) noexcept;

    component_id            id;
    simulation_tree_node_id sim_tree_node =
      undefined<simulation_tree_node_id>();
    child_id id_in_parent;

    hierarchy<tree_node> tree;

    table<model_id, model_id>        parameters;
    table<model_id, observable_type> observables;

    table<model_id, model_id> sim;
};

//! Used to cache memory allocation when user import a model into simulation.
//! The memory cached can be reused using clear but memory cached can be
//! completely free using the destroy function.
struct modeling_to_simulation
{
    vector<tree_node*>              stack;
    vector<std::pair<model_id, i8>> inputs;
    vector<std::pair<model_id, i8>> outputs;

    table<u64, constant_source_id>    constants;
    table<u64, binary_file_source_id> binary_files;
    table<u64, text_file_source_id>   text_files;
    table<u64, random_source_id>      randoms;

    data_array<simulation_tree_node, simulation_tree_node_id> sim_tree_nodes;

    simulation_tree_node_id head = undefined<simulation_tree_node_id>();

    void clear() noexcept;
    void destroy() noexcept;
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
    data_array<tree_node, tree_node_id>           tree_nodes;
    data_array<description, description_id>       descriptions;
    data_array<component, component_id>           components;
    data_array<registred_path, registred_path_id> registred_paths;
    data_array<dir_path, dir_path_id>             dir_paths;
    data_array<file_path, file_path_id>           file_paths;
    data_array<model, model_id>                   parameters;

    small_vector<registred_path_id, max_component_dirs> component_repertories;
    external_source                                     srcs;

    tree_node_id head = undefined<tree_node_id>();

    modeling_status state = modeling_status::unmodified;

    modeling() noexcept;

    status init(modeling_initializer& params) noexcept;

    component_id search_component(const char* directory,
                                  const char* filename) noexcept;

    component_id search_internal_component(component_type type) noexcept;

    status fill_internal_components() noexcept;
    status fill_components() noexcept;
    status fill_components(registred_path& path) noexcept;

    //! Deletes the component, the file (@c file_path_id) and the description
    //! (@c description_id) objects attached.
    void free(component& c) noexcept;
    void free(component& parent, child& c) noexcept;
    void free(component& parent, connection& c) noexcept;
    void free(tree_node& node) noexcept;

    bool can_alloc_file(i32 number = 1) const noexcept;
    bool can_alloc_dir(i32 number = 1) const noexcept;
    bool can_alloc_registred(i32 number = 1) const noexcept;

    file_path&      alloc_file(dir_path& dir) noexcept;
    dir_path&       alloc_dir(registred_path& reg) noexcept;
    registred_path& alloc_registred() noexcept;

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

    child& alloc(component& parent, dynamics_type type) noexcept;

    status copy(component& src, component& dst) noexcept;

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
    status connect_input(component& parent,
                         i8         port_src,
                         child_id   dst,
                         i8         port_dst) noexcept;

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
    status connect_output(component& parent,
                          child_id   src,
                          i8         port_src,
                          i8         port_dst) noexcept;

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
    status connect(component& parent,
                   child_id   src,
                   i8         port_src,
                   child_id   dst,
                   i8         port_dst) noexcept;

    //! Initialize a project with the specified \c component as head.
    //!
    //! Before initializing, the current project is cleared: all tree_nodes and
    //! component tree are cleared.
    void init_project(component& compo) noexcept;

    //! Build the @c hierarchy<component_ref> of from the component @c id
    status make_tree_from(component& id, tree_node_id* out) noexcept;

    status save(component& c) noexcept; // will call clean(component&) first.

    status load_project(const char* filename) noexcept;
    status save_project(const char* filename) noexcept;
    void   clear_project() noexcept;

    bool   can_export_to(modeling_to_simulation& cache,
                         const simulation&       sim) const noexcept;
    status export_to(modeling_to_simulation& cache, simulation& sim) noexcept;

    ring_buffer<log_entry> log_entries;
};

/*
 * Implementation
 */

inline child::child(model_id model) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
{
}

inline child::child(component_id component) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
{
}

inline tree_node::tree_node(component_id id_, child_id id_in_parent_) noexcept
  : id(id_)
  , id_in_parent(id_in_parent_)
{
}

inline component::component() noexcept
{
    static const std::string_view port_names[] = { "0", "1", "2", "3",
                                                   "4", "5", "6", "7" };

    for (int i = 0; i < length(port_names); ++i) {
        x_names[i] = port_names[i];
        y_names[i] = port_names[i];
    }

    std::array<small_string<7>, port_number> x_names;

    models.init(64); // @TODO replace these constants
    hsms.init(8);    // with variables from init files
    children.init(80);
    connections.init(256);
}

inline bool component::can_alloc(int place) const noexcept
{
    return models.can_alloc(place);
}

inline bool component::can_alloc(dynamics_type type, int place) const noexcept
{
    if (type == dynamics_type::hsm_wrapper)
        return models.can_alloc(place) && hsms.can_alloc(place);
    else
        return models.can_alloc(place);
}

template<typename Dynamics>
inline bool component::can_alloc_dynamics(int place) const noexcept
{
    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return models.can_alloc(place) && hsms.can_alloc(place);
    else
        return models.can_alloc(place);
}

inline child& modeling::alloc(component& parent, dynamics_type type) noexcept
{
    irt_assert(!parent.models.full());

    auto& mdl  = parent.models.alloc();
    mdl.type   = type;
    mdl.handle = nullptr;

    dispatch(mdl, [&parent]<typename Dynamics>(Dynamics& dyn) -> void {
        new (&dyn) Dynamics{};

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            irt_assert(parent.hsms.can_alloc());

            auto& machine = parent.hsms.alloc();
            dyn.id        = parent.hsms.get_id(machine);
        }
    });

    auto  mdl_id = parent.models.get_id(mdl);
    auto& child  = parent.children.alloc(mdl_id);

    return child;
}

} // namespace irt

#endif
