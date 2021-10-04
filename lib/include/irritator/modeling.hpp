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
enum class component_ref_id : u64;
enum class parameter_id : u64;
enum class description_id : u64;
enum class dir_path_id : u64;
enum class file_path_id : u64;

enum class child_type : i8
{
    model,
    component
};

struct connection;
struct child;
struct port;
struct component;
struct component_ref;
struct modeling;
struct description;

status add_cpp_component_ref(const char*    buffer,
                             modeling&      mod,
                             component&     parent,
                             component_ref& compo_ref) noexcept;
status add_file_component_ref(const char*    buffer,
                              modeling&      mod,
                              component&     parent,
                              component_ref& compo_ref) noexcept;

status build_simulation(const modeling& mod, simulation& sim) noexcept;

struct description
{
    small_string<1024> data;
};

template<typename DataArray, typename T, typename Function>
void for_each(const DataArray& data, vector<T>& vec, Function&& f)
{
    i64 i = 0, e = vec.size();

    while (i != e) {
        if (auto* ptr = data.try_to_get(vec[i]); ptr) {
            f(*ptr);
            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }
}

struct component_ref
{
    component_id              id; // the component reference
    table<model_id, model_id> mappers;
    hierarchy<component_ref>  tree;
};

struct child
{
    child(model_id model) noexcept;
    child(component_ref_id component) noexcept;

    u64        id;
    child_type type;

    float x            = 0.f;
    float y            = 0.f;
    bool  configurable = false;
    bool  observable   = false;
};

struct port
{
    port(model_id model, i8 port) noexcept;
    port(component_ref_id component, i8 port) noexcept;
    port(u64 id_, child_type type_, i8 port_) noexcept;

    u64        id;    // model_id or component_ref_id
    child_type type;  // allow to choose id
    i8         index; // index of the port
};

struct connection
{
    connection() noexcept = default;

    template<typename Src, typename Dst>
    connection(Src src, i8 port_src_, Dst dst, i8 port_dst_) noexcept;

    connection(child src_, i8 port_src_, child dst_, i8 port_dst_) noexcept;

    u64        src;      // model_id or component_id
    u64        dst;      // model_id or component_id
    child_type type_src; // model or component
    child_type type_dst; // model or component
    i8         port_src; // output port index
    i8         port_dst; // input port index
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
    file
};

struct component
{
    vector<child>      children;
    vector<connection> connections;

    vector<port> x;
    vector<port> y;

    description_id   desc = description_id{ 0 };
    file_path_id     path = file_path_id{ 0 };
    small_string<16> name;
    component_type   type;
};

struct dir_path
{
    small_string<256 * 16> path;

    enum class status_option
    {
        none,
        read_in_progress,
        usable,
        unusable
    };

    status_option status = status_option::none;
};

struct file_path
{
    small_string<256> path;
    dir_path_id       parent{ 0 };

    enum class status_option
    {
        none,
        read_in_progress,
        readable,
        unreadable
    };

    status_option status = status_option::none;
};

struct modeling_initializer
{
    int model_capacity;
    int component_ref_capacity;
    int description_capacity;
    int component_capacity;
    int observer_capacity;
    int dir_path_capacity;
    int file_path_capacity;

    int constant_source_capacity;
    int binary_file_source_capacity;
    int text_file_source_capacity;
    int random_source_capacity;

    u64 random_generator_seed;
};

struct modeling
{
    data_array<model, model_id>                 models;
    data_array<component_ref, component_ref_id> component_refs;
    data_array<description, description_id>     descriptions;
    data_array<component, component_id>         components;
    data_array<observer, observer_id>           observers;
    data_array<dir_path, dir_path_id>           dir_paths;
    data_array<file_path, file_path_id>         file_paths;
    irt::external_source                        srcs;
    component_ref_id                            head{ 0 };

    status init(const modeling_initializer& params) noexcept;

    status fill_component() noexcept;

    template<typename Dynamics>
    Dynamics& alloc(component& c) noexcept;

    model& alloc(component& parent, dynamics_type type) noexcept;

    template<typename DynamicsSrc, typename DynamicsDst>
    status connect(component&   c,
                   DynamicsSrc& src,
                   i8           port_src,
                   DynamicsDst& dst,
                   i8           port_dst) noexcept;

    status connect(component& parent,
                   i32        src,
                   i8         port_src,
                   i32        dst,
                   i8         port_dst) noexcept;

    template<typename Dynamics>
    model_id get_id(const Dynamics& dyn) const;
};

/*
 * Implementation
 */

template<typename Src, typename Dst>
connection::connection(Src src, i8 port_src_, Dst dst, i8 port_dst_) noexcept
  : src(ordinal(src))
  , dst(ordinal(dst))
  , port_src(port_src_)
  , port_dst(port_dst_)
{
    if constexpr (std::is_same_v<Src, model_id>) {
        type_src = child_type::model;
    } else {
        type_src = child_type::component;
    }

    if constexpr (std::is_same_v<Dst, model_id>) {
        type_dst = child_type::model;
    } else {
        type_dst = child_type::component;
    }
}

inline connection::connection(child src_,
                              i8    port_src_,
                              child dst_,
                              i8    port_dst_) noexcept
  : src(src_.id)
  , dst(dst_.id)
  , type_src(src_.type)
  , type_dst(dst_.type)
  , port_src(port_src_)
  , port_dst(port_dst_)
{}

inline child::child(model_id model) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
{}

inline child::child(component_ref_id component) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
{}

inline port::port(model_id model, i8 port_) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
  , index{ port_ }
{}

inline port::port(component_ref_id component, i8 port_) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
  , index{ port_ }
{}

inline port::port(u64 id_, child_type type_, i8 port_) noexcept
  : id{ id_ }
  , type{ type_ }
  , index{ port_ }
{}

template<typename Dynamics>
Dynamics& modeling::alloc(component& parent) noexcept
{
    irt_assert(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = nullptr;

    new (&mdl.dyn) Dynamics{};
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (is_detected_v<has_input_port_t, Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = static_cast<u64>(-1);

    if constexpr (is_detected_v<has_output_port_t, Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = static_cast<u64>(-1);

    parent.children.emplace_back(models.get_id(mdl));

    return dyn;
}

inline auto modeling::alloc(component& parent, dynamics_type type) noexcept
  -> model&
{
    irt_assert(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = type;
    mdl.handle = nullptr;

    dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> void {
        new (&dyn) Dynamics{};

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (is_detected_v<has_output_port_t, Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);
    });

    parent.children.emplace_back(models.get_id(mdl));

    return mdl;
}

template<typename DynamicsSrc, typename DynamicsDst>
status modeling::connect(component&   c,
                         DynamicsSrc& src,
                         i8           port_src,
                         DynamicsDst& dst,
                         i8           port_dst) noexcept
{
    irt_assert(!c.connections.full());

    model&   src_model    = get_model(src);
    model&   dst_model    = get_model(dst);
    model_id model_src_id = get_id(dst);
    model_id model_dst_id = get_id(dst);

    irt_return_if_fail(
      is_ports_compatible(src_model, port_src, dst_model, port_dst),
      status::model_connect_bad_dynamics);

    c.connections.emplace_back(model_src_id, port_src, model_dst_id, port_dst);

    return status::success;
}

template<typename Dynamics>
model_id modeling::get_id(const Dynamics& dyn) const
{
    return models.get_id(get_model(dyn));
}

} // namespace irt

#endif
