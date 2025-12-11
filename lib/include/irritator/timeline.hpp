// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/thread.hpp>

namespace irt {

enum class timeline_point_type : u8 {
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

/// Stores a simulation-snapshot circular buffer.
///
/// This class stores simulation snapshot and build a timeline of simulations.
class simulation_snapshot_handler
{
public:
    constexpr simulation_snapshot_handler() noexcept = default;
    explicit simulation_snapshot_handler(std::integral auto capacity) noexcept;

    bool reserve(std::integral auto capacity) noexcept;
    void emplace_back(const simulation& sim) noexcept;

    /// Return nullptr if empty otherwise the oldest element.
    constexpr simulation_snapshot*       front() noexcept;
    constexpr const simulation_snapshot* front() const noexcept;

    /// Return nullptr if empty otherwise the latest element.
    constexpr simulation_snapshot*       back() noexcept;
    constexpr const simulation_snapshot* back() const noexcept;

    constexpr bool next(simulation_snapshot*& snap) noexcept;
    constexpr bool next(const simulation_snapshot*& snap) const noexcept;
    constexpr bool previous(simulation_snapshot*& snap) noexcept;
    constexpr bool previous(const simulation_snapshot*& snap) const noexcept;

    constexpr void erase_after(const simulation_snapshot* snap) noexcept;

    constexpr unsigned capacity() const noexcept;
    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr bool     empty() const noexcept;

    constexpr int index_from_ptr(
      const simulation_snapshot* data) const noexcept;
    constexpr bool is_ptr_valid(simulation_snapshot* it) const noexcept;

private:
    vector<simulation_snapshot> ring;

    int m_front    = 0;
    int m_back     = 0;
    int m_capacity = 1;
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
