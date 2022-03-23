// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/timeline.hpp>

#include <fmt/format.h>

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

status timeline::init(i32 simulation_point_number,
                      i32 model_point_number,
                      i32 connection_point_number,
                      i32 timeline_point_number,
                      i32 model_number,
                      i32 message_number) noexcept
{
    irt_return_if_fail(
      simulation_point_number >= 0 && model_point_number >= 0 &&
        connection_point_number >= 0 && timeline_point_number >= 0 &&
        model_number >= 0 && message_number >= 0,
      status::gui_not_enough_memory);

    reset();

    sim_points.reserve(simulation_point_number);
    model_points.reserve(model_point_number);
    connection_points.reserve(connection_point_number);
    points_buffer.resize(timeline_point_number);
    points =
      ring_buffer(points_buffer.data(), static_cast<i32>(points_buffer.size()));

    max_models_number   = model_number;
    max_messages_number = message_number;

    return status::success;
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

    current_bag = points.rend();
    bag         = 0;
}

static status build_initial_simulation_point(timeline&   tl,
                                             simulation& sim,
                                             time        t) noexcept
{
    irt_return_if_fail(
      tl.can_alloc(timeline_point_type::simulation,
                   static_cast<i32>(sim.models.max_used()),
                   static_cast<i32>(sim.message_alloc.max_size())),
      status::simulation_not_enough_model);

    auto& sim_pt = tl.alloc_simulation_point();
    sim_pt.t     = t;
    sim_pt.models.reserve(sim.models.max_size());
    sim_pt.model_ids.reserve(sim.models.max_size());

    if (sim.message_alloc.max_size() > 0) {
        sim_pt.message_alloc.init(sim.message_alloc.max_size());
        sim.message_alloc.copy_to(sim_pt.message_alloc);
    }

    model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        auto& s_mdl = sim_pt.models.emplace_back();
        sim_pt.model_ids.emplace_back(sim.models.get_id(*mdl));

        std::copy_n(reinterpret_cast<std::byte*>(mdl),
                    sizeof(model),
                    reinterpret_cast<std::byte*>(&s_mdl));
    }

    tl.bag = 1;

    return status::success;
}

static status build_simulation_point(timeline&               tl,
                                     simulation&             sim,
                                     const vector<model_id>& imm,
                                     time                    t) noexcept
{
    irt_return_if_fail(
      tl.can_alloc(timeline_point_type::simulation,
                   imm.ssize(),
                   static_cast<i32>(sim.message_alloc.max_size())),
      status::simulation_not_enough_model);

    auto& sim_pt = tl.alloc_simulation_point();
    sim_pt.t     = t;
    sim_pt.models.reserve(imm.ssize());
    sim_pt.model_ids.reserve(imm.ssize());

    if (sim.message_alloc.max_size() > 0) {
        sim_pt.message_alloc.init(sim.message_alloc.max_size());
        sim.message_alloc.copy_to(sim_pt.message_alloc);
    }

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

    return status::success;
}

status initialize(timeline& tl, simulation& sim, time t) noexcept
{
    tl.reset();
    irt_return_if_bad(build_initial_simulation_point(tl, sim, t));

    return status::success;
}

static status apply(simulation& sim, simulation_point& sim_pt) noexcept
{
    sim.message_alloc.reset();
    sim_pt.message_alloc.swap(sim.message_alloc);

    for (i32 i = 0, e = sim_pt.models.ssize(); i != e; ++i) {
        auto* sim_model = sim.models.try_to_get(sim_pt.model_ids[i]);
        if (sim_model) {
            std::copy_n(reinterpret_cast<const std::byte*>(&sim_pt.models[i]),
                        sizeof(model),
                        reinterpret_cast<std::byte*>(sim_model));

            if (sim_model->handle) {
                sim.sched.update(*sim_model, sim_model->tn);
            } else {
                sim.sched.insert(
                  *sim_model, sim.models.get_id(*sim_model), sim_model->tn);
            }
        }
    }

    return status::success;
}

static status apply(simulation&                            sim,
                    const connection_point&                cnt,
                    const connection_point::operation_type type) noexcept
{
    auto* mdl_src = sim.models.try_to_get(cnt.src);
    irt_return_if_fail(mdl_src, status::unknown_dynamics);

    auto* mdl_dst = sim.models.try_to_get(cnt.dst);
    irt_return_if_fail(mdl_dst, status::unknown_dynamics);

    switch (type) {
    case connection_point::operation_type::add:
        irt_return_if_bad(
          sim.connect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst));
        break;

    case connection_point::operation_type::remove:
        irt_return_if_bad(
          sim.disconnect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst));
    }

    return status::success;
}

static status apply(simulation&                       sim,
                    const model_point&                mdl,
                    const model_point::operation_type type) noexcept
{
    switch (type) {
    case model_point::operation_type::add: {
        irt_return_if_fail(sim.models.can_alloc(1),
                           status::simulation_not_enough_model);
        auto& new_mdl = sim.clone(mdl.mdl);
        sim.make_initialize(new_mdl, mdl.t);
    } break;

    case model_point::operation_type::change: {
        auto* to_change_mdl = sim.models.try_to_get(mdl.id);
        irt_return_if_fail(to_change_mdl, status::unknown_dynamics);

        std::copy_n(reinterpret_cast<const std::byte*>(&mdl.mdl),
                    sizeof(model),
                    reinterpret_cast<std::byte*>(to_change_mdl));
    } break;

    case model_point::operation_type::remove:
        irt_return_if_bad(sim.deallocate(mdl.id));
        break;
    }

    return status::success;
}

static status advance(simulation&             sim,
                      time&                   t,
                      const connection_point& cnt_pt) noexcept
{
    irt_return_if_bad(apply(sim, cnt_pt, cnt_pt.type));

    t = cnt_pt.t;

    return status::success;
}

static status advance(simulation& sim, time& t, model_point& mdl_pt) noexcept
{
    irt_return_if_bad(apply(sim, mdl_pt, mdl_pt.type));

    t = mdl_pt.t;

    return status::success;
}

static status advance(simulation&       sim,
                      time&             t,
                      simulation_point& sim_pt) noexcept
{
    irt_return_if_bad(apply(sim, sim_pt));

    t = sim_pt.t;

    return status::success;
}

status advance(timeline& tl, simulation& sim, time& t) noexcept
{
    if (tl.current_bag == tl.points.rend())
        return status::success;

    --tl.current_bag;

    if (tl.current_bag == tl.points.rend())
        return status::success;

    auto& bag = *tl.current_bag;

    switch (bag.type) {
    case timeline_point_type::connection:
        return advance(sim, t, tl.connection_points[bag.index]);
    case timeline_point_type::model:
        return advance(sim, t, tl.model_points[bag.index]);
    case timeline_point_type::simulation:
        return advance(sim, t, tl.sim_points[bag.index]);
    }

    irt_unreachable();
}

static status back(simulation& sim, time& t, connection_point& cnt_pt) noexcept
{
    const auto overwrite_operation =
      cnt_pt.type == connection_point::operation_type::add
        ? connection_point::operation_type::remove
        : connection_point::operation_type::add;

    irt_return_if_bad(apply(sim, cnt_pt, overwrite_operation));

    t = cnt_pt.t;

    return status::success;
}

static status back(simulation& sim, time& t, model_point& mdl_pt) noexcept
{
    const auto overwrite_operation =
      mdl_pt.type == model_point::operation_type::add
        ? model_point::operation_type::remove
        : (mdl_pt.type == model_point::operation_type::remove
             ? model_point::operation_type::add
             : model_point::operation_type::change);

    irt_return_if_bad(apply(sim, mdl_pt, overwrite_operation));

    t = mdl_pt.t;

    return status::success;
}

static status back(simulation& sim, time& t, simulation_point& sim_pt) noexcept
{
    irt_return_if_bad(apply(sim, sim_pt));

    t = sim_pt.t;

    return status::success;
}

status back(timeline& tl, simulation& sim, time& t) noexcept
{
    if (tl.current_bag == tl.points.rend())
        return status::success;

    ++tl.current_bag;

    if (tl.current_bag == tl.points.rend())
        return status::success;

    auto& bag = *tl.current_bag;

    switch (bag.type) {
    case timeline_point_type::connection:
        return back(sim, t, tl.connection_points[bag.index]);
    case timeline_point_type::model:
        return back(sim, t, tl.model_points[bag.index]);
    case timeline_point_type::simulation:
        return back(sim, t, tl.sim_points[bag.index]);
    }

    irt_unreachable();
}

status run(timeline& tl, simulation& sim, time& t) noexcept
{
    if (sim.sched.empty()) {
        t = time_domain<time>::infinity;
        return status::success;
    }

    if (t = sim.sched.tn(); time_domain<time>::is_infinity(t))
        return status::success;

    sim.immediate_models.clear();
    sim.sched.pop(sim.immediate_models);

    irt_return_if_bad(build_simulation_point(tl, sim, sim.immediate_models, t));

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

    tl.current_bag = tl.points.rbegin();

    return status::success;
}

status finalize(timeline& tl, simulation& sim, time t) noexcept
{
    tl.current_bag = tl.points.rbegin();
    return sim.finalize(t);
}

bool timeline::can_advance() const noexcept
{
    if (current_bag == points.rend())
        return false;

    auto next = current_bag;
    --next;

    if (next == points.rend())
        return false;

    return true;
}

bool timeline::can_back() const noexcept
{
    if (current_bag == points.rend())
        return false;

    auto previous = current_bag;
    ++previous;

    if (previous == points.rend())
        return false;

    return true;
}

} // namespace irt
