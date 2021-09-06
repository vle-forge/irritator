// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP
#define ORG_VLEPROJECT_IRRITATOR_2021_MODELING_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

namespace irt {

enum class component_id     = u64;
enum class component_ref_id = u64;
enum class parameter_id     = u64;

enum class vertice_type : i8
{
    model,
    component
};

enum class connection_type : i8
{
    internal,
    input,
    output
};

struct parameter;
struct connection;
struct child;
struct port;
struct component;
struct component_ref;
struct modeling;

status build_simulation(const modeling& mod, simulation& sim);

struct parameter
{
    u64  integer[8];
    real values[8];
};

struct connection
{
    u64             src;      // model_id or component_id
    u64             dst;      // model_id or component_id
    node_type       type_src; // model or component
    node_type       type_dst; // model or component
    i8              port_src; // output port index
    i8              port_dst; // input port index
    connection_type type;
};

struct component_ref
{
    component_id              id;         // the component reference
    vector<parameter_id>      parameters; // same number as configurables
    vector<observer_id>       observers;  // same number as observable
    table<model_id, model_id> mappers;
    hierarchy<component_ref>  tree;
};

struct child
{
    child(model_id model) noexcept;
    child(component_ref_id component) noexcept;

    u64       id;
    node_type type;
};

struct port
{
    port(model_id model, i8 port) noexcept;
    port(component_ref_id component, i8 port) noexcept;

    u64       id;
    node_type type;
    i8        port;
};

struct component
{
    vector<child>      children;
    vector<connection> connections;

    vector<port>  x;
    vector<port>  y;
    vector<child> configurables; // configurable model list
    vector<child> observables;   // observable model list
};

struct modeling
{
    data_array<model, model_id>                 models;
    data_array<component_ref, component_ref_id> components;
    data_array<component, component_id>         components;
    data_array<parameter, parameter_id>         parameters;
    data_array<observer, observer_id>           observers;

    component_ref_id head = component_ref_id{ 0 };
};

////

inline child::child(model_id model) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
{}

inline child::child(component_ref_id component) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
{}

inline port::port(model_id model, i8 port) noexcept
  : id{ ordinal(model) }
  , type{ child_type::model }
  , port(port_)
{}

inline port::port(component_ref_id component, i8 port) noexcept
  : id{ ordinal(component) }
  , type{ child_type::component }
  , port(port_)
{}

} // namespace irt

#endif
