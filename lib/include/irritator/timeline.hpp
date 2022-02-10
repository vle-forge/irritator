// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>

namespace irt {

enum class simulation_timelime_point_id : u64;
enum class model_timelime_point_id : u64;
enum class connection_timelime_point_id : u64;
enum class timeline_point_id : u64;

enum class timeline_point_type
{
    simulation, //! a copy of state of model (basic - deep copy for now)
    model,      //! user add, remove or change value in a model
    connection  //! user add, remove or change value in a connection
};

enum class timeline_operation_type
{
    add,
    remove,
    change,
};

struct simulation_timelime_point
{
    time t;

    vector<model>                            models;
    vector<model_id>                         model_ids;
    block_allocator<list_view_node<message>> message_alloc;
};

struct model_timelime_point
{
    time t;

    model mdl;

    timeline_operation_type type = timeline_operation_type::add;
};

struct connection_timelime_point
{
    time t;

    model_id src;
    model_id dst;
    i8       port_src;
    i8       port_dst;

    timeline_operation_type type = timeline_operation_type::add;
};

struct timeline_point
{
    timeline_point(timeline_point_type type_,
                   u64                 id_,
                   i32                 global_identifier_) noexcept
      : id(id_)
      , type(type_)
      , global_identifier(global_identifier_)
    {}

    u64                 id;
    timeline_point_type type = timeline_point_type::simulation;
    i32                 global_identifier;

    timeline_point_id prev = undefined<timeline_point_id>();
    timeline_point_id next = undefined<timeline_point_id>();
};

struct timeline
{
    bool can_alloc(timeline_point_type type,
                   i32                 models   = 0,
                   i32                 messages = 0) noexcept;

    simulation_timelime_point& alloc_simulation_timelime_point() noexcept;
    model_timelime_point&      alloc_model_timelime_point() noexcept;
    connection_timelime_point& alloc_connection_timelime_point() noexcept;

    void push_back(timeline_point_type type,
                   u64                 id,
                   i32                 global_identifier) noexcept;
    void pop_front() noexcept;
    void pop_front_same_global_identifier() noexcept;

    void clear() noexcept;

    i32 make_next_global_identifier() noexcept;

    data_array<simulation_timelime_point, simulation_timelime_point_id>
                                                              sim_points;
    data_array<model_timelime_point, model_timelime_point_id> model_points;
    data_array<connection_timelime_point, connection_timelime_point_id>
                                                  connection_points;
    data_array<timeline_point, timeline_point_id> points;

    i32               global_identifier{};
    timeline_point_id first{};
    timeline_point_id last{};
};

status initialize(timeline& tl, simulation&, time t) noexcept;
status run(timeline& tl, simulation&, time& t) noexcept;
status finalize(timeline& tl, simulation&, time t) noexcept;

inline bool timeline::can_alloc(timeline_point_type  type,
                                [[maybe_unused]] i32 models,
                                [[maybe_unused]] i32 messages) noexcept
{
    bool ret = false;

    if (points.can_alloc(1)) {
        switch (type) {
        case timeline_point_type::simulation:
            ret = sim_points.can_alloc(1);
            // @TODO check models and messages allocator
            break;
        case timeline_point_type::model:
            ret = model_points.can_alloc(1);
            break;
        case timeline_point_type::connection:
            ret = connection_points.can_alloc(1);
            break;
        }
    }

    return ret;
}

inline void timeline::push_back(timeline_point_type type,
                                u64                 id,
                                i32                 global_identifier) noexcept
{
    irt_assert(points.can_alloc(1));

    auto& point    = points.alloc(type, id, global_identifier);
    auto  point_id = points.get_id(point);

    auto* last_point = points.try_to_get(last);
    irt_assert(last_point);

    last_point->next = point_id;
    point.prev       = last;
    point.next       = undefined<timeline_point_id>();
    last             = point_id;
}

inline simulation_timelime_point&
timeline::alloc_simulation_timelime_point() noexcept
{
    auto& ret    = sim_points.alloc();
    auto  ret_id = ordinal(sim_points.get_id(ret));

    push_back(timeline_point_type::simulation, ret_id, global_identifier);

    return ret;
}

inline model_timelime_point& timeline::alloc_model_timelime_point() noexcept
{
    auto& ret    = model_points.alloc();
    auto  ret_id = ordinal(model_points.get_id(ret));

    push_back(timeline_point_type::model, ret_id, global_identifier);

    return ret;
}

inline connection_timelime_point&
timeline::alloc_connection_timelime_point() noexcept
{
    auto& ret    = connection_points.alloc();
    auto  ret_id = ordinal(connection_points.get_id(ret));

    push_back(timeline_point_type::connection, ret_id, global_identifier);

    return ret;
}

inline void timeline::pop_front() noexcept
{
    if (auto* first_point = points.try_to_get(first); first_point) {
        if (auto* next = points.try_to_get(first_point->next); next) {
            next->prev = undefined<timeline_point_id>();
            first      = first_point->next;
        } else {
            first = undefined<timeline_point_id>();
            last  = undefined<timeline_point_id>();
        }

        switch (first_point->type) {
        case timeline_point_type::simulation:
            sim_points.free(
              enum_cast<simulation_timelime_point_id>(first_point->id));
            break;
        case timeline_point_type::model:
            model_points.free(
              enum_cast<model_timelime_point_id>(first_point->id));
            break;
        case timeline_point_type::connection:
            connection_points.free(
              enum_cast<connection_timelime_point_id>(first_point->id));
            break;
        }

        points.free(*first_point);
    }
}

inline void timeline::pop_front_same_global_identifier() noexcept
{
    if (auto* first_point = points.try_to_get(first); first_point) {
        auto global_identifier = first_point->global_identifier;

        pop_front();

        while (points.size() > 0) {
            if (first_point = points.try_to_get(first);
                first_point &&
                first_point->global_identifier == global_identifier)
                pop_front();
            else
                break;
        }
    }
}

inline void timeline::clear() noexcept
{
    points.clear();

    sim_points.clear();
    model_points.clear();
    connection_points.clear();

    first = undefined<timeline_point_id>();
    last  = undefined<timeline_point_id>();

    global_identifier = 0;
}

inline i32 timeline::make_next_global_identifier() noexcept
{
    return ++global_identifier;
}

inline status initialize(timeline& /*tl*/, simulation& sim, time t) noexcept
{
    return sim.finalize(t);
}

inline status run(timeline& tl, simulation& sim, time& t) noexcept
{
    if (sim.sched.empty()) {
        t = time_domain<time>::infinity;
        return status::success;
    }

    if (t = sim.sched.tn(); time_domain<time>::is_infinity(t))
        return status::success;

    sim.immediate_models.clear();
    sim.sched.pop(sim.immediate_models);

    tl.make_next_global_identifier();
    if (tl.can_alloc(timeline_point_type::simulation,
                     sim.immediate_models.ssize())) {

        auto& sim_pt = tl.alloc_simulation_timelime_point();
        sim_pt.t     = t;
        sim_pt.models.resize(sim.immediate_models.ssize());
        sim_pt.model_ids.resize(sim.immediate_models.ssize());
        sim_pt.message_alloc.init(sim.message_alloc.max_size());

        for (auto mdl_id : sim.immediate_models) {
            if (auto* mdl = sim.models.try_to_get(mdl_id); mdl) {

                auto& s_mdl = sim_pt.models.emplace_back();
                auto& s_id  = sim_pt.model_ids.emplace_back();

                std::copy_n(reinterpret_cast<std::byte*>(mdl),
                            sizeof(model),
                            reinterpret_cast<std::byte*>(&s_mdl));

                s_id = mdl_id;

                dispatch(
                  *mdl, [&sim, &sim_pt]<typename Dynamics>(Dynamics& dyn) {
                      if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                          for (i32 i = 0, e = length(dyn.x); i != e; ++i) {
                              auto const_list = get_message(sim, dyn.x[i]);
                              input_port p    = dyn.x[i];
                              auto       list =
                                list_view<message>(sim_pt.message_alloc, p);

                              auto it = const_list.begin();
                              auto et = const_list.end();

                              for (; it != et; ++it) {
                                  list.emplace_back(*it);
                              }
                          }
                      }
                  });
            }
        }
    }

    sim.emitting_output_ports.clear();
    for (const auto id : sim.immediate_models)
        if (auto* mdl = sim.models.try_to_get(id); mdl)
            irt_return_if_bad(sim.make_transition(*mdl, t));

    for (int i = 0, e = length(sim.emitting_output_ports); i != e; ++i) {
        auto* mdl = sim.models.try_to_get(sim.emitting_output_ports[i].model);
        if (!mdl)
            continue;

        sim.sched.update(*mdl, t);

        irt_return_if_fail(can_alloc_message(sim, 1),
                           status::simulation_not_enough_message);

        auto  port = sim.emitting_output_ports[i].port;
        auto& msg  = sim.emitting_output_ports[i].msg;

        dispatch(*mdl, [&sim, port, &msg]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                auto list = append_message(sim, dyn.x[port]);
                list.push_back(msg);
            }
        });
    }

    return status::success;
}

inline status finalize(timeline& /*tl*/, simulation& sim, time t) noexcept
{
    return sim.finalize(t);
}

} // namespace irt

#endif