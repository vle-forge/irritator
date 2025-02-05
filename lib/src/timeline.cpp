// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/core.hpp"
#include "irritator/error.hpp"
#include <irritator/format.hpp>
#include <irritator/timeline.hpp>

namespace irt {

bool timeline::can_alloc(timeline_point_type type,
                         i32                 models,
                         i32                 messages) noexcept
{
    bool ret = false;

    switch (type) {
    case timeline_point_type ::simulation:
        ret = sim_points.can_alloc(1) &&
              current_models_number + models < max_models_number &&
              current_messages_number + messages < max_messages_number;
        break;
    case timeline_point_type ::model:
        ret = model_points.can_alloc(1);
        break;
    case timeline_point_type ::connection:
        ret = connection_points.can_alloc(1);
        break;
    }

    return ret;
}

timeline::timeline(i32 simulation_point_number,
                   i32 model_point_number,
                   i32 connection_point_number,
                   i32 timeline_point_number,
                   i32 model_number,
                   i32 message_number) noexcept
  : sim_points(simulation_point_number, irt::reserve_tag{})
  , model_points(model_point_number, irt::reserve_tag{})
  , connection_points(connection_point_number, irt::reserve_tag{})
  , points(timeline_point_number)
{
    debug::ensure(simulation_point_number >= 0 && model_point_number >= 0 &&
                  connection_point_number >= 0 && timeline_point_number >= 0 &&
                  model_number >= 0 && message_number >= 0);

    max_models_number   = model_number;
    max_messages_number = message_number;
}

simulation_point& timeline::alloc_simulation_point() noexcept
{
    const auto nb = sim_points.ssize();

    auto& back = sim_points.emplace_back();
    points.force_emplace_enqueue(timeline_point_type::simulation, nb, bag);

    return back;
}

model_point& timeline::alloc_model_point() noexcept
{
    const auto nb = model_points.ssize();

    auto& back = model_points.emplace_back();
    points.force_emplace_enqueue(timeline_point_type::model, nb, bag);

    return back;
}

connection_point& timeline::alloc_connection_point() noexcept
{
    const auto nb = connection_points.ssize();

    auto& back = connection_points.emplace_back();
    points.force_emplace_enqueue(timeline_point_type::connection, nb, bag);

    return back;
}

void timeline::reset() noexcept
{
    sim_points.clear();
    model_points.clear();
    connection_points.clear();
    points.clear();

    current_models_number   = 0;
    current_messages_number = 0;

    current_bag.reset();
    bag = 0;
}

void timeline::cleanup_after_current_bag() noexcept
{
    if (current_bag == points.end())
        return;

    const bool need_to_search_connections = !connection_points.empty();
    const bool need_to_search_models      = !model_points.empty();
    const bool need_to_search_simulations = !sim_points.empty();

    i32  first_connection_index = -1;
    i32  first_model_index      = -1;
    i32  first_simulation_index = -1;
    auto it{ current_bag };

    for (++it; it != points.end(); ++it) {
        switch (it->type) {
        case timeline_point_type::connection:
            if (first_connection_index == -1)
                first_connection_index = it->index;
            break;
        case timeline_point_type::model:
            if (first_model_index == -1)
                first_model_index = it->index;
            break;
        case timeline_point_type::simulation:
            if (first_simulation_index == -1)
                first_simulation_index = it->index;
            break;
        }

        if ((first_connection_index != -1 || !need_to_search_connections) &&
            (first_model_index != -1 || !need_to_search_models) &&
            (first_simulation_index != -1 || !need_to_search_simulations))
            break;
    }

    if (first_connection_index != -1)
        connection_points.erase(
          connection_points.data() + first_connection_index,
          connection_points.data() + connection_points.size());

    if (first_model_index != -1)
        model_points.erase(model_points.data() + first_model_index,
                           model_points.data() + model_points.size());

    if (first_simulation_index != -1)
        sim_points.erase(sim_points.data() + first_simulation_index,
                         sim_points.data() + sim_points.size());

    points.erase_before(current_bag);
}

static status build_initial_simulation_point(timeline&   tl,
                                             simulation& sim,
                                             time        t) noexcept
{
    if (!tl.can_alloc(timeline_point_type::simulation,
                      static_cast<i32>(sim.models.max_used()),
                      static_cast<i32>(sim.messages.max_size())))
        return new_error(simulation::part::models, container_full_error{});

    auto& sim_pt = tl.alloc_simulation_point();
    sim_pt.t     = t;
    sim_pt.models.reserve(sim.models.max_size());
    sim_pt.model_ids.reserve(sim.models.max_size());

    // if (sim.messages.max_size() > 0) {
    //     irt_check(sim_pt.messages.init(sim.messages.max_size()));
    //     irt_check(sim.message_alloc.copy_to(sim_pt.message_alloc));
    // }

    model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        auto& s_mdl = sim_pt.models.emplace_back();
        sim_pt.model_ids.emplace_back(sim.models.get_id(*mdl));

        std::copy_n(reinterpret_cast<std::byte*>(mdl),
                    sizeof(model),
                    reinterpret_cast<std::byte*>(&s_mdl));
    }

    tl.bag = 1;

    return success();
}

static status build_simulation_point(timeline&               tl,
                                     simulation&             sim,
                                     const vector<model_id>& imm,
                                     time                    t) noexcept
{
    // if (!tl.can_alloc(timeline_point_type::simulation,
    //                   imm.ssize(),
    //                   static_cast<i32>(sim.message_alloc.max_size())))
    //     return new_error(simulation::part::messages, container_full_error{});

    auto& sim_pt = tl.alloc_simulation_point();
    sim_pt.t     = t;
    sim_pt.models.reserve(imm.ssize());
    sim_pt.model_ids.reserve(imm.ssize());

    // if (sim.message_alloc.max_size() > 0) {
    //     irt_check(sim_pt.message_alloc.init(sim.message_alloc.max_size()));
    //     irt_check(sim.message_alloc.copy_to(sim_pt.message_alloc));
    // }

    for (auto mdl_id : sim.immediate_models) {
        if (auto* mdl = sim.models.try_to_get(mdl_id); mdl) {
            auto& s_mdl = sim_pt.models.emplace_back();
            auto& s_id  = sim_pt.model_ids.emplace_back();

            std::copy_n(reinterpret_cast<std::byte*>(mdl),
                        sizeof(model),
                        reinterpret_cast<std::byte*>(&s_mdl));

            s_id = mdl_id;
        }
    }

    ++tl.bag;

    return success();
}

status initialize(timeline& tl, simulation& sim, time t) noexcept
{
    tl.reset();

    irt_check(build_initial_simulation_point(tl, sim, t));

    return success();
}

static status apply(simulation& sim, simulation_point& sim_pt) noexcept
{
    // sim.message_alloc.reset();
    // sim_pt.message_alloc.swap(sim.message_alloc);

    for (i32 i = 0, e = sim_pt.models.ssize(); i != e; ++i) {
        auto* sim_model = sim.models.try_to_get(sim_pt.model_ids[i]);
        if (sim_model) {
            std::copy_n(reinterpret_cast<const std::byte*>(&sim_pt.models[i]),
                        sizeof(model),
                        reinterpret_cast<std::byte*>(sim_model));

            if (sim_model->handle == invalid_heap_handle) {
                sim.sched.update(*sim_model, sim_model->tn);
            } else {
                sim.sched.alloc(
                  *sim_model, sim.models.get_id(*sim_model), sim_model->tn);
            }
        }
    }

    return success();
}

static status apply(simulation&                            sim,
                    const connection_point&                cnt,
                    const connection_point::operation_type type) noexcept
{
    auto* mdl_src = sim.models.try_to_get(cnt.src);
    if (!mdl_src)
        return new_error(simulation::part::models, unknown_error{});

    auto* mdl_dst = sim.models.try_to_get(cnt.dst);
    if (!mdl_dst)
        return new_error(simulation::part::models, unknown_error{});

    switch (type) {
    case connection_point::operation_type::add:
        irt_check(sim.connect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst));
        break;

    case connection_point::operation_type::remove:
        sim.disconnect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst);
    }

    return success();
}

static status apply(simulation&                       sim,
                    const model_point&                mdl,
                    const model_point::operation_type type) noexcept
{
    switch (type) {
    case model_point::operation_type::add: {
        if (!sim.models.can_alloc(1))
            return new_error(simulation::part::models, container_full_error{});

        auto& new_mdl = sim.clone(mdl.mdl);
        irt_check(sim.make_initialize(new_mdl, mdl.t));
    } break;

    case model_point::operation_type::change: {
        auto* to_change_mdl = sim.models.try_to_get(mdl.id);
        if (!to_change_mdl)
            return new_error(simulation::part::models, unknown_error{});

        std::copy_n(reinterpret_cast<const std::byte*>(&mdl.mdl),
                    sizeof(model),
                    reinterpret_cast<std::byte*>(to_change_mdl));
    } break;

    case model_point::operation_type::remove:
        sim.deallocate(mdl.id);
        break;
    }

    return success();
}

static status advance(simulation&             sim,
                      time&                   t,
                      const connection_point& cnt_pt) noexcept
{
    irt_check(apply(sim, cnt_pt, cnt_pt.type));

    t = cnt_pt.t;

    return success();
}

static status advance(simulation& sim, time& t, model_point& mdl_pt) noexcept
{
    irt_check(apply(sim, mdl_pt, mdl_pt.type));

    t = mdl_pt.t;

    return success();
}

static status advance(simulation&       sim,
                      time&             t,
                      simulation_point& sim_pt) noexcept
{
    irt_check(apply(sim, sim_pt));

    t = sim_pt.t;

    return success();
}

status advance(timeline& tl, simulation& sim, time& t) noexcept
{
    if (tl.current_bag == tl.points.end())
        return success();

    --tl.current_bag;

    if (tl.current_bag == tl.points.end())
        return success();

    auto& bag = *tl.current_bag;

    switch (bag.type) {
    case timeline_point_type::connection:
        return advance(sim, t, tl.connection_points[bag.index]);
    case timeline_point_type::model:
        return advance(sim, t, tl.model_points[bag.index]);
    case timeline_point_type::simulation:
        return advance(sim, t, tl.sim_points[bag.index]);
    }

    unreachable();
}

static status back(simulation& sim, time& t, connection_point& cnt_pt) noexcept
{
    const auto overwrite_operation =
      cnt_pt.type == connection_point::operation_type::add
        ? connection_point::operation_type::remove
        : connection_point::operation_type::add;

    irt_check(apply(sim, cnt_pt, overwrite_operation));

    t = cnt_pt.t;

    return success();
}

static status back(simulation& sim, time& t, model_point& mdl_pt) noexcept
{
    const auto overwrite_operation =
      mdl_pt.type == model_point::operation_type::add
        ? model_point::operation_type::remove
        : (mdl_pt.type == model_point::operation_type::remove
             ? model_point::operation_type::add
             : model_point::operation_type::change);

    irt_check(apply(sim, mdl_pt, overwrite_operation));

    t = mdl_pt.t;

    return success();
}

static status back(simulation& sim, time& t, simulation_point& sim_pt) noexcept
{
    irt_check(apply(sim, sim_pt));

    t = sim_pt.t;

    return success();
}

status back(timeline& tl, simulation& sim, time& t) noexcept
{
    if (tl.current_bag == tl.points.end())
        return success();

    ++tl.current_bag;

    if (tl.current_bag == tl.points.end())
        return success();

    auto& bag = *tl.current_bag;

    switch (bag.type) {
    case timeline_point_type::connection:
        return back(sim, t, tl.connection_points[bag.index]);
    case timeline_point_type::model:
        return back(sim, t, tl.model_points[bag.index]);
    case timeline_point_type::simulation:
        return back(sim, t, tl.sim_points[bag.index]);
    }

    unreachable();
}

status run(timeline& tl, simulation& sim, time& t) noexcept
{
    if (sim.sched.empty()) {
        t = time_domain<time>::infinity;
        return success();
    }

    if (t = sim.sched.tn(); time_domain<time>::is_infinity(t))
        return success();

    sim.immediate_models.clear();
    sim.sched.pop(sim.immediate_models);

    irt_check(build_simulation_point(tl, sim, sim.immediate_models, t));

    sim.emitting_output_ports.clear();
    for (const auto id : sim.immediate_models)
        if (auto* mdl = sim.models.try_to_get(id); mdl)
            irt_check(sim.make_transition(*mdl, t));

    for (int i = 0, e = length(sim.emitting_output_ports); i != e; ++i) {
        auto* mdl = sim.models.try_to_get(sim.emitting_output_ports[i].model);
        if (!mdl)
            continue;

        sim.sched.update(*mdl, t);

        if (!sim.messages.can_alloc(1))
            return new_error(simulation::part::messages,
                             container_full_error{});

        auto  port = sim.emitting_output_ports[i].port;
        auto& msg  = sim.emitting_output_ports[i].msg;

        dispatch(*mdl, [&sim, port, &msg]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_input_port<Dynamics>) {
                auto* lst = sim.messages.try_to_get(dyn.x[port]);
                if (not lst) {
                    auto& new_lst = sim.messages.alloc();
                    lst           = &new_lst;
                    dyn.x[port]   = sim.messages.get_id(new_lst);
                }

                lst->push_back(msg);
            }
        });
    }

    tl.current_bag = tl.points.tail();

    return success();
}

status finalize(timeline& tl, simulation& sim, time t) noexcept
{
    tl.current_bag = tl.points.tail();
    sim.t          = t;
    return sim.finalize();
}

bool timeline::can_advance() const noexcept
{
    if (current_bag == points.end())
        return false;

    auto next = current_bag;
    --next;

    if (next == points.end())
        return false;

    return true;
}

bool timeline::can_back() const noexcept
{
    if (current_bag == points.end())
        return false;

    auto previous = current_bag;
    ++previous;

    if (previous == points.end())
        return false;

    return true;
}

} // namespace irt
