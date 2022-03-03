// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>

namespace irt {

enum class timeline_point_type
{
    simulation, //! a copy of state of model (basic - deep copy for now)
    model,      //! user add, remove or change value in a model
    connection  //! user add, remove or change value in a connection
};

struct simulation_point
{
    time t;

    vector<model>                            models;
    vector<model_id>                         model_ids;
    block_allocator<list_view_node<message>> message_alloc;
};

struct model_point
{
    time t;

    model    mdl;
    model_id id;

    enum class operation_type
    {
        add,
        remove,
        change,
    };

    operation_type type = operation_type::add;
};

struct connection_point
{
    time t;

    model_id src;
    model_id dst;
    i8       port_src;
    i8       port_dst;

    enum class operation_type
    {
        add,
        remove,
    };

    operation_type type = operation_type::add;
};

struct timeline_point
{
    timeline_point(timeline_point_type type_, i32 index_, i32 bag_) noexcept
      : index(index_)
      , bag(bag_)
      , type(type_)
    {}

    i32                 index;
    i32                 bag;
    timeline_point_type type = timeline_point_type::simulation;
};

struct timeline
{
    bool can_alloc(timeline_point_type type,
                   i32                 models   = 0,
                   i32                 messages = 0) noexcept;

    simulation_point& alloc_simulation_point() noexcept;
    model_point&      alloc_model_point() noexcept;
    connection_point& alloc_connection_point() noexcept;

    status init(i32 simulation_points,
                i32 model_points,
                i32 connection_points,
                i32 timeline_points,
                i32 model_number,
                i32 message_number) noexcept;

    void reset() noexcept;

    i32 make_next_bag() noexcept;

    vector<simulation_point> sim_points;
    vector<model_point>      model_points;
    vector<connection_point> connection_points;
    vector<timeline_point>   points;

    i32 max_models_number       = 0;
    i32 max_messages_number     = 0;
    i32 current_models_number   = 0;
    i32 current_messages_number = 0;

    i32 bag         = 0;  //! number of points @TODO replace with points.ssize()
    i32 current_bag = -1; //! current point
};

//! @brief Initialize simulation and store first state.
status initialize(timeline& tl, simulation&, time t) noexcept;

//! @brief Run one step of simulation and store state.
status run(timeline& tl, simulation&, time& t) noexcept;

//! @brief Finalize the simulation.
status finalize(timeline& tl, simulation&, time t) noexcept;

//! @brief  Advance the simulation by one step.
status advance(timeline& tl, simulation& sim, time& t) noexcept;

//! @brief  Back the simulation by one step.
status back(timeline& tl, simulation& sim, time& t) noexcept;

inline i32 timeline::make_next_bag() noexcept
{
    current_bag = bag++;

    return bag;
}

} // namespace irt

#endif