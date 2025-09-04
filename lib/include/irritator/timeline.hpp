// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/thread.hpp>

namespace irt {

enum class timeline_point_type {
    simulation, //! a copy of state of model (basic - deep copy for now)
    model,      //! user add, remove or change value in a model
    connection  //! user add, remove or change value in a connection
};

struct simulation_point {
    simulation_point() noexcept = default;

    simulation_point(const simulation_point& rhs) noexcept = delete;
    simulation_point(simulation_point&& rhs) noexcept      = default;

    time t;

    vector<model>    models;
    vector<model_id> model_ids;
    vector<message>  message_alloc;
};

static_assert(std::is_move_constructible_v<simulation_point>);
static_assert(not std::is_move_assignable_v<simulation_point>);
static_assert(not std::is_copy_constructible_v<simulation_point>);
static_assert(not std::is_copy_assignable_v<simulation_point>);

struct model_point {
    time t;

    model    mdl;
    model_id id;

    enum class operation_type {
        add,
        remove,
        change,
    };

    operation_type type = operation_type::add;
};

struct connection_point {
    time t;

    model_id src;
    model_id dst;
    i8       port_src;
    i8       port_dst;

    enum class operation_type {
        add,
        remove,
    };

    operation_type type = operation_type::add;
};

struct timeline_point {
    timeline_point() noexcept = default;

    timeline_point(timeline_point_type type_, i32 index_, i32 bag_) noexcept
      : index(index_)
      , bag(bag_)
      , type(type_)
    {}

    i32                 index;
    i32                 bag;
    timeline_point_type type = timeline_point_type::simulation;
};

struct timeline {
    bool can_alloc(timeline_point_type type,
                   i32                 models   = 0,
                   i32                 messages = 0) noexcept;

    simulation_point& alloc_simulation_point() noexcept;
    model_point&      alloc_model_point() noexcept;
    connection_point& alloc_connection_point() noexcept;

    timeline(i32 simulation_points,
             i32 model_points,
             i32 connection_points,
             i32 timeline_points,
             i32 model_number,
             i32 message_number) noexcept;

    timeline(const timeline& other) noexcept = default;
    timeline(timeline&& other) noexcept      = default;

    void reset() noexcept;
    void cleanup_after_current_bag() noexcept;

    vector<simulation_point>    sim_points;
    vector<model_point>         model_points;
    vector<connection_point>    connection_points;
    ring_buffer<timeline_point> points;

    i32 max_models_number       = 0;
    i32 max_messages_number     = 0;
    i32 current_models_number   = 0;
    i32 current_messages_number = 0;

    bool can_advance() const noexcept;
    bool can_back() const noexcept;

    ring_buffer<timeline_point>::iterator current_bag;

    i32 bag;

    mutable spin_mutex mutex;
};

//! @brief Initialize simulation and store first state.
status initialize(timeline& tl, simulation&) noexcept;

//! @brief Run one step of simulation and store state.
status run(timeline& tl, simulation&) noexcept;

//! @brief Finalize the simulation.
status finalize(timeline& tl, simulation&) noexcept;

//! @brief  Advance the simulation by one step.
status advance(timeline& tl, simulation& sim) noexcept;

//! @brief  Back the simulation by one step.
status back(timeline& tl, simulation& sim) noexcept;

} // namespace irt

#endif
