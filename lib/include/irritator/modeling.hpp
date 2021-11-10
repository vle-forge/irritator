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

struct connection;
struct child;
struct port;
struct component;
// struct component_ref;
struct modeling;
struct description;

status add_cpp_component_ref(const char* buffer,
                             modeling&   mod,
                             component&  parent) noexcept;
status add_file_component_ref(const char* buffer,
                              modeling&   mod,
                              component&  parent) noexcept;

status build_simulation(const modeling& mod, simulation& sim) noexcept;

enum class description_status
{
    unread,
    read_only,
    modified,
    unmodified,
};

struct description
{
    small_string<1024> data;
    description_status status = description_status::unread;
};

template<typename DataArray, typename T, typename Function>
void for_each(const DataArray& data, vector<T>& vec, Function&& f)
{
    i64 i = 0;

    while (i != vec.ssize()) {
        if (auto* ptr = data.try_to_get(vec[i]); ptr) {
            f(*ptr);
            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }
}

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
    port(child_id child, i8 port) noexcept;

    child_id id;
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

struct component
{
    vector<child_id>      children;
    vector<connection_id> connections;

    small_vector<port, 8> x;
    small_vector<port, 8> y;

    description_id   desc = description_id{ 0 };
    dir_path_id      dir  = dir_path_id{ 0 };
    file_path_id     file = file_path_id{ 0 };
    small_string<32> name;

    component_type   type   = component_type::memory;
    component_status status = component_status::modified;
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
    int model_capacity;
    int tree_capacity;
    int description_capacity;
    int component_capacity;
    int observer_capacity;
    int dir_path_capacity;
    int file_path_capacity;
    int children_capacity;
    int connection_capacity;
    int port_capacity;

    int constant_source_capacity;
    int binary_file_source_capacity;
    int text_file_source_capacity;
    int random_source_capacity;

    u64 random_generator_seed;
};

struct tree_node
{
    tree_node(component_id id_) noexcept;

    component_id         id;
    hierarchy<tree_node> tree;

    table<model_id, model_id> parameters;
    vector<model_id>          observables;

    table<model_id, model_id> sim;
};

// static void refresh_component(void *param) noexcept
// {
//     auto *mod = reinterpret_cast<modeling*>(param);

//     // mod->status = modeling::read_only;
//     mod->status =
//     mod->refresh_component();
//     mod->status = modeling::done;
// }

struct modeling
{
    data_array<model, model_id>             models;
    data_array<tree_node, tree_node_id>     tree_nodes;
    data_array<description, description_id> descriptions;
    data_array<component, component_id>     components;
    data_array<observer, observer_id>       observers;
    data_array<dir_path, dir_path_id>       dir_paths;
    data_array<file_path, file_path_id>     file_paths;
    data_array<child, child_id>             children;
    data_array<connection, connection_id>   connections;

    vector<dir_path_id>  component_repertories;
    irt::external_source srcs;
    tree_node_id         head;

    status init(const modeling_initializer& params) noexcept;

    component_id search_component(const char* name, const char* hint) noexcept;

    status fill_internal_components() noexcept;
    status fill_components() noexcept;
    status fill_components(dir_path& path) noexcept;

    void free(component& c) noexcept;
    void free(component& parent, child& c) noexcept;
    void free(component& parent, connection& c) noexcept;
    void free(tree_node& node) noexcept;

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

inline port::port(child_id child_, i8 port_) noexcept
  : id{ child_ }
  , index{ port_ }
{}

inline tree_node::tree_node(component_id id_) noexcept
  : id(id_)
{}

inline child& modeling::alloc(component& parent, dynamics_type type) noexcept
{
    irt_assert(!models.full());

    auto& mdl  = models.alloc();
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

    auto  mdl_id = models.get_id(mdl);
    auto& child  = children.alloc(mdl_id);
    parent.children.emplace_back(children.get_id(child));

    return child;
}

} // namespace irt

#endif
