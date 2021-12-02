// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/external_source.hpp>

namespace irt {

enum class component_id : u64;
enum class tree_node_id : u64;
enum class parameter_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;
enum class child_id : u64;
enum class connection_id : u64;

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
    qss1_seir_lineaire,
    qss1_seir_nonlineaire,
    qss1_van_der_pol,
    qss2_izhikevich,
    qss2_lif,
    qss2_lotka_volterra,
    qss2_negative_lif,
    qss2_seir_lineaire,
    qss2_seir_nonlineaire,
    qss2_van_der_pol,
    qss3_izhikevich,
    qss3_lif,
    qss3_lotka_volterra,
    qss3_negative_lif,
    qss3_seir_lineaire,
    qss3_seir_nonlineaire,
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

struct connection;
struct child;
struct port;
struct component;
struct modeling;
struct description;

status add_cpp_component_ref(const char* buffer,
                             modeling&   mod,
                             component&  parent) noexcept;
status add_file_component_ref(const char* buffer,
                              modeling&   mod,
                              component&  parent) noexcept;

status build_simulation(const modeling& mod, simulation& sim) noexcept;

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
    bool  configurable = false;
    bool  observable   = false;
};

struct port
{
    port() noexcept = default;
    port(model_id id_, i8 port_) noexcept;

    model_id id;
    i8       index;
};

struct connection
{
    connection() noexcept = default;

    connection(child_id src_,
               i8       index_src_,
               child_id dst_,
               i8       index_dst_) noexcept;

    child_id src;
    child_id dst;
    i8       index_src;
    i8       index_dst;
};

struct component
{
    component() noexcept
    {
        models.init(64);
        children.init(80);
        connections.init(256);
    }

    data_array<model, model_id>           models;
    data_array<child, child_id>           children;
    data_array<connection, connection_id> connections;

    small_vector<port, 8> x;
    small_vector<port, 8> y;

    description_id   desc = description_id{ 0 };
    dir_path_id      dir  = dir_path_id{ 0 };
    file_path_id     file = file_path_id{ 0 };
    small_string<32> name;

    component_type   type  = component_type::memory;
    component_status state = component_status::modified;
};

struct dir_path
{
    enum class status_option
    {
        none,
        read,
        unread,
    };

    small_string<256 * 16> path;
    small_string<32>       name;
    status_option          status   = status_option::unread;
    i8                     priority = 0;
};

struct file_path
{
    small_string<256> path;
    dir_path_id       parent{ 0 };
};

struct modeling_initializer
{
    i32 model_capacity;
    i32 tree_capacity;
    i32 description_capacity;
    i32 component_capacity;
    i32 observer_capacity;
    i32 file_path_capacity;
    i32 children_capacity;
    i32 connection_capacity;
    i32 port_capacity;

    i32 constant_source_capacity;
    i32 binary_file_source_capacity;
    i32 text_file_source_capacity;
    i32 random_source_capacity;

    u64 random_generator_seed;
};

struct tree_node
{
    tree_node(component_id id_, child_id id_in_parent_) noexcept;

    component_id         id;
    child_id             id_in_parent;
    hierarchy<tree_node> tree;

    table<model_id, model_id> parameters;
    vector<model_id>          observables;

    table<model_id, model_id> sim;
};

struct modeling
{
    data_array<observer, observer_id>       observers;
    data_array<tree_node, tree_node_id>     tree_nodes;
    data_array<description, description_id> descriptions;
    data_array<component, component_id>     components;
    data_array<dir_path, dir_path_id>       dir_paths;
    data_array<file_path, file_path_id>     file_paths;

    small_vector<dir_path_id, 64> component_repertories;
    irt::external_source          srcs;
    tree_node_id                  head;

    status init(modeling_initializer& params) noexcept;

    component_id search_component(const char* name, const char* hint) noexcept;

    status fill_internal_components() noexcept;
    status fill_components() noexcept;
    status fill_components(dir_path& path) noexcept;

    void free(component& c) noexcept;
    void free(component& parent, child& c) noexcept;
    void free(component& parent, connection& c) noexcept;
    void free(tree_node& node) noexcept;
    void free(dir_path& dir) noexcept;

    child& alloc(component& parent, dynamics_type type) noexcept;

    status copy(component& src, component& dst) noexcept;

    status connect(component& parent,
                   child_id   src,
                   i8         port_src,
                   child_id   dst,
                   i8         port_dst) noexcept;

    /// Build the @c hierarchy<component_ref> of from the component @c id
    status make_tree_from(component& id, tree_node_id* out) noexcept;

    status clean(component& c) noexcept; // clean empty vectors
    status save(component& c) noexcept;  // will call clean(component&) first.
};

/*
 * Implementation
 */

inline connection::connection(child_id src_,
                              i8       index_src_,
                              child_id dst_,
                              i8       index_dst_) noexcept
  : src(src_)
  , dst(dst_)
  , index_src(index_src_)
  , index_dst(index_dst_)
{}

inline child::child(model_id model) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
{}

inline child::child(component_id component) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
{}

inline port::port(model_id id_, i8 port_) noexcept
  : id{ id_ }
  , index{ port_ }
{}

inline tree_node::tree_node(component_id id_, child_id id_in_parent_) noexcept
  : id(id_)
  , id_in_parent(id_in_parent_)
{}

inline child& modeling::alloc(component& parent, dynamics_type type) noexcept
{
    irt_assert(!parent.models.full());

    auto& mdl  = parent.models.alloc();
    mdl.type   = type;
    mdl.handle = nullptr;

    dispatch(mdl, []<typename Dynamics>(Dynamics& dyn) -> void {
        new (&dyn) Dynamics{};

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (is_detected_v<has_output_port_t, Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);
    });

    auto  mdl_id = parent.models.get_id(mdl);
    auto& child  = parent.children.alloc(mdl_id);

    return child;
}

} // namespace irt

#endif
