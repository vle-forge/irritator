// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/core.hpp"
#include "irritator/error.hpp"
#include <irritator/format.hpp>
#include <irritator/timeline.hpp>

namespace irt {

// bool timeline::can_alloc(timeline_point_type type,
//                          i32                 models,
//                          i32                 messages) noexcept
//{
//     bool ret = false;
//
//     switch (type) {
//     case timeline_point_type ::simulation:
//         ret = sim_points.can_alloc(1) &&
//               current_models_number + models < max_models_number &&
//               current_messages_number + messages < max_messages_number;
//         break;
//     case timeline_point_type ::model:
//         ret = model_points.can_alloc(1);
//         break;
//     case timeline_point_type ::connection:
//         ret = connection_points.can_alloc(1);
//         break;
//     }
//
//     return ret;
// }
//
// timeline::timeline(i32 simulation_point_number,
//                    i32 model_point_number,
//                    i32 connection_point_number,
//                    i32 timeline_point_number,
//                    i32 model_number,
//                    i32 message_number) noexcept
//   : sim_points(simulation_point_number, irt::reserve_tag)
//   , model_points(model_point_number, irt::reserve_tag)
//   , connection_points(connection_point_number, irt::reserve_tag)
//   , points(timeline_point_number)
//{
//     debug::ensure(simulation_point_number >= 0 && model_point_number >= 0 &&
//                   connection_point_number >= 0 && timeline_point_number >= 0
//                   && model_number >= 0 && message_number >= 0);
//
//     max_models_number   = model_number;
//     max_messages_number = message_number;
// }
//
// simulation_point& timeline::alloc_simulation_point() noexcept
//{
//     std::lock_guard lock(mutex);
//
//     const auto nb = sim_points.ssize();
//
//     auto& back = sim_points.emplace_back();
//     points.force_emplace_enqueue(timeline_point_type::simulation, nb, bag);
//
//     return back;
// }
//
// model_point& timeline::alloc_model_point() noexcept
//{
//     std::lock_guard lock(mutex);
//
//     const auto nb = model_points.ssize();
//
//     auto& back = model_points.emplace_back();
//     points.force_emplace_enqueue(timeline_point_type::model, nb, bag);
//
//     return back;
// }
//
// connection_point& timeline::alloc_connection_point() noexcept
//{
//     std::lock_guard lock(mutex);
//
//     const auto nb = connection_points.ssize();
//
//     auto& back = connection_points.emplace_back();
//     points.force_emplace_enqueue(timeline_point_type::connection, nb, bag);
//
//     return back;
// }
//
// void timeline::reset() noexcept
//{
//     std::lock_guard lock(mutex);
//
//     sim_points.clear();
//     model_points.clear();
//     connection_points.clear();
//     points.clear();
//
//     current_models_number   = 0;
//     current_messages_number = 0;
//
//     current_bag.reset();
//     bag = 0;
// }
//
// void timeline::cleanup_after_current_bag() noexcept
//{
//     if (current_bag == points.end())
//         return;
//
//     const bool need_to_search_connections = !connection_points.empty();
//     const bool need_to_search_models      = !model_points.empty();
//     const bool need_to_search_simulations = !sim_points.empty();
//
//     i32  first_connection_index = -1;
//     i32  first_model_index      = -1;
//     i32  first_simulation_index = -1;
//     auto it{ current_bag };
//
//     for (++it; it != points.end(); ++it) {
//         switch (it->type) {
//         case timeline_point_type::connection:
//             if (first_connection_index == -1)
//                 first_connection_index = it->index;
//             break;
//         case timeline_point_type::model:
//             if (first_model_index == -1)
//                 first_model_index = it->index;
//             break;
//         case timeline_point_type::simulation:
//             if (first_simulation_index == -1)
//                 first_simulation_index = it->index;
//             break;
//         }
//
//         if ((first_connection_index != -1 || !need_to_search_connections) &&
//             (first_model_index != -1 || !need_to_search_models) &&
//             (first_simulation_index != -1 || !need_to_search_simulations))
//             break;
//     }
//
//     if (first_connection_index != -1)
//         connection_points.erase(
//           connection_points.data() + first_connection_index,
//           connection_points.data() + connection_points.size());
//
//     if (first_model_index != -1)
//         model_points.erase(model_points.data() + first_model_index,
//                            model_points.data() + model_points.size());
//
//     if (first_simulation_index != -1)
//         sim_points.erase(sim_points.data() + first_simulation_index,
//                          sim_points.data() + sim_points.size());
//
//     points.erase_before(current_bag);
// }
//
// static status build_initial_simulation_point(timeline&   tl,
//                                              simulation& sim) noexcept
//{
//     if (!tl.can_alloc(timeline_point_type::simulation,
//                       static_cast<i32>(sim.models.max_used()),
//                       static_cast<i32>(sim.nodes.max_size())))
//         return new_error(timeline_errc::memory_error);
//
//     auto& sim_pt = tl.alloc_simulation_point();
//     sim_pt.t     = sim.current_time();
//     sim_pt.models.reserve(sim.models.max_size());
//     sim_pt.model_ids.reserve(sim.models.max_size());
//
//     // if (sim.messages.max_size() > 0) {
//     //     irt_check(sim_pt.messages.init(sim.messages.max_size()));
//     //     irt_check(sim.message_alloc.copy_to(sim_pt.message_alloc));
//     // }
//
//     model* mdl = nullptr;
//     while (sim.models.next(mdl)) {
//         auto& s_mdl = sim_pt.models.emplace_back();
//         sim_pt.model_ids.emplace_back(sim.models.get_id(*mdl));
//
//         std::copy_n(reinterpret_cast<std::byte*>(mdl),
//                     sizeof(model),
//                     reinterpret_cast<std::byte*>(&s_mdl));
//     }
//
//     tl.bag = 1;
//
//     return success();
// }

// static status build_simulation_point(timeline&                  tl,
//                                      const simulation&          sim,
//                                      const std::span<model_id>& imm) noexcept
//{
//     // if (!tl.can_alloc(timeline_point_type::simulation,
//     //                   imm.ssize(),
//     //                   static_cast<i32>(sim.message_alloc.max_size())))
//     //     return new_error(simulation::part::messages,
//     container_full_error{});
//
//     auto& sim_pt = tl.alloc_simulation_point();
//     sim_pt.t     = sim.current_time();
//     sim_pt.models.reserve(imm.size());
//     sim_pt.model_ids.reserve(imm.size());
//
//     // if (sim.message_alloc.max_size() > 0) {
//     // irt_check(sim_pt.message_alloc.init(sim.message_alloc.max_size()));
//     //     irt_check(sim.message_alloc.copy_to(sim_pt.message_alloc));
//     // }
//
//     for (auto mdl_id : sim.immediate_models) {
//         if (auto* mdl = sim.models.try_to_get(mdl_id); mdl) {
//             auto& s_mdl = sim_pt.models.emplace_back();
//             auto& s_id  = sim_pt.model_ids.emplace_back();
//
//             std::copy_n(reinterpret_cast<std::byte*>(mdl),
//                         sizeof(model),
//                         reinterpret_cast<std::byte*>(&s_mdl));
//
//             s_id = mdl_id;
//         }
//     }
//
//     ++tl.bag;
//
//     return success();
// }

// status initialize(timeline& tl, simulation& sim) noexcept
//{
//     tl.reset();
//
//     irt_check(build_initial_simulation_point(tl, sim));
//
//     return success();
// }

// static status apply(simulation& sim, simulation_point& sim_pt) noexcept
//{
//     // sim.message_alloc.reset();
//     // sim_pt.message_alloc.swap(sim.message_alloc);
//
//     for (i32 i = 0, e = sim_pt.models.ssize(); i != e; ++i) {
//         auto* sim_model = sim.models.try_to_get(sim_pt.model_ids[i]);
//         if (sim_model) {
//             std::copy_n(reinterpret_cast<const
//             std::byte*>(&sim_pt.models[i]),
//                         sizeof(model),
//                         reinterpret_cast<std::byte*>(sim_model));
//
//             if (sim_model->handle == invalid_heap_handle) {
//                 sim.sched.update(*sim_model, sim_model->tn);
//             } else {
//                 sim.sched.alloc(
//                   *sim_model, sim.models.get_id(*sim_model), sim_model->tn);
//             }
//         }
//     }
//
//     return success();
// }
//
// static status apply(simulation&                            sim,
//                     const connection_point&                cnt,
//                     const connection_point::operation_type type) noexcept
//{
//     auto* mdl_src = sim.models.try_to_get(cnt.src);
//     if (!mdl_src)
//         return new_error(timeline_errc::apply_change_error);
//
//     auto* mdl_dst = sim.models.try_to_get(cnt.dst);
//     if (!mdl_dst)
//         return new_error(timeline_errc::apply_change_error);
//
//     switch (type) {
//     case connection_point::operation_type::add:
//         if (not sim.connect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst))
//             return new_error(timeline_errc::apply_change_error);
//         break;
//
//     case connection_point::operation_type::remove:
//         sim.disconnect(*mdl_src, cnt.port_src, *mdl_dst, cnt.port_dst);
//     }
//
//     return success();
// }
//
// static status apply(simulation&                       sim,
//                     const model_point&                mdl,
//                     const model_point::operation_type type) noexcept
//{
//     switch (type) {
//     case model_point::operation_type::add: {
//         if (!sim.models.can_alloc(1))
//             return new_error(timeline_errc::apply_change_error);
//
//         auto& new_mdl = sim.clone(mdl.mdl);
//         irt_check(sim.make_initialize(new_mdl, mdl.t));
//     } break;
//
//     case model_point::operation_type::change: {
//         auto* to_change_mdl = sim.models.try_to_get(mdl.id);
//         if (!to_change_mdl)
//             return new_error(timeline_errc::apply_change_error);
//
//         std::copy_n(reinterpret_cast<const std::byte*>(&mdl.mdl),
//                     sizeof(model),
//                     reinterpret_cast<std::byte*>(to_change_mdl));
//     } break;
//
//     case model_point::operation_type::remove:
//         sim.deallocate(mdl.id);
//         break;
//     }
//
//     return success();
// }
//
// static status advance(simulation& sim, const connection_point& cnt_pt)
// noexcept
//{
//     irt_check(apply(sim, cnt_pt, cnt_pt.type));
//
//     sim.current_time(cnt_pt.t);
//
//     return success();
// }
//
// static status advance(simulation& sim, model_point& mdl_pt) noexcept
//{
//     irt_check(apply(sim, mdl_pt, mdl_pt.type));
//
//     sim.current_time(mdl_pt.t);
//
//     return success();
// }
//
// static status advance(simulation& sim, simulation_point& sim_pt) noexcept
//{
//     irt_check(apply(sim, sim_pt));
//
//     sim.current_time(sim_pt.t);
//
//     return success();
// }
//
// status advance(timeline& tl, simulation& sim) noexcept
//{
//     if (tl.current_bag == tl.points.end())
//         return success();
//
//     --tl.current_bag;
//
//     if (tl.current_bag == tl.points.end())
//         return success();
//
//     auto& bag = *tl.current_bag;
//
//     switch (bag.type) {
//     case timeline_point_type::connection:
//         return advance(sim, tl.connection_points[bag.index]);
//     case timeline_point_type::model:
//         return advance(sim, tl.model_points[bag.index]);
//     case timeline_point_type::simulation:
//         return advance(sim, tl.sim_points[bag.index]);
//     }
//
//     unreachable();
// }
//
// static status back(simulation& sim, connection_point& cnt_pt) noexcept
//{
//     const auto overwrite_operation =
//       cnt_pt.type == connection_point::operation_type::add
//         ? connection_point::operation_type::remove
//         : connection_point::operation_type::add;
//
//     irt_check(apply(sim, cnt_pt, overwrite_operation));
//
//     sim.current_time(cnt_pt.t);
//
//     return success();
// }
//
// static status back(simulation& sim, model_point& mdl_pt) noexcept
//{
//     const auto overwrite_operation =
//       mdl_pt.type == model_point::operation_type::add
//         ? model_point::operation_type::remove
//         : (mdl_pt.type == model_point::operation_type::remove
//              ? model_point::operation_type::add
//              : model_point::operation_type::change);
//
//     irt_check(apply(sim, mdl_pt, overwrite_operation));
//
//     sim.current_time(mdl_pt.t);
//
//     return success();
// }
//
// static status back(simulation& sim, simulation_point& sim_pt) noexcept
//{
//     irt_check(apply(sim, sim_pt));
//
//     sim.current_time(sim_pt.t);
//
//     return success();
// }
//
// status back(timeline& tl, simulation& sim) noexcept
//{
//     if (tl.current_bag == tl.points.end())
//         return success();
//
//     ++tl.current_bag;
//
//     if (tl.current_bag == tl.points.end())
//         return success();
//
//     auto& bag = *tl.current_bag;
//
//     switch (bag.type) {
//     case timeline_point_type::connection:
//         return back(sim, tl.connection_points[bag.index]);
//     case timeline_point_type::model:
//         return back(sim, tl.model_points[bag.index]);
//     case timeline_point_type::simulation:
//         return back(sim, tl.sim_points[bag.index]);
//     }
//
//     unreachable();
// }
//
// status run(timeline& tl, simulation& sim) noexcept
//{
//     return sim.run_with_cb([&](const auto& sim, auto imm) noexcept {
//         build_simulation_point(tl, sim, imm);
//     });
// }
//
// status finalize(timeline& tl, simulation& sim) noexcept
//{
//     tl.current_bag = tl.points.tail();
//     return sim.finalize();
// }
//
// bool timeline::can_advance() const noexcept
//{
//     std::lock_guard lock(mutex);
//
//     if (current_bag == points.end())
//         return false;
//
//     auto next = current_bag;
//     --next;
//
//     if (next == points.end())
//         return false;
//
//     return true;
// }
//
// bool timeline::can_back() const noexcept
//{
//     std::lock_guard lock(mutex);
//
//     if (current_bag == points.end())
//         return false;
//
//     auto previous = current_bag;
//     ++previous;
//
//     if (previous == points.end())
//         return false;
//
//     return true;
// }

simulation_snapshot::simulation_snapshot(const simulation& sim) noexcept
  : models{ sim.models }
  , observers{ sim.observers }
  , nodes{ sim.nodes }
  , output_ports{ sim.output_ports }
  , dated_messages{ sim.dated_messages }
  , sched{ sim.sched }
  , t{ sim.current_time() }
{}

simulation_snapshot& simulation_snapshot::operator=(
  const simulation& sim) noexcept
{
    models         = sim.models;
    observers      = sim.observers;
    nodes          = sim.nodes;
    output_ports   = sim.output_ports;
    dated_messages = sim.dated_messages;
    sched          = sim.sched;
    t              = sim.current_time();

    return *this;
}

simulation_snapshot_handler::simulation_snapshot_handler(
  std::integral auto capacity) noexcept
{
    if (capacity > 0) {
        m_capacity = capacity + 1;
        m_front    = 0;
        m_back     = 0;

        ring.resize(capacity);
    }
}

bool simulation_snapshot_handler::reserve(std::integral auto capacity) noexcept
{
    if (std::cmp_less(m_capacity, capacity)) {
        vector<simulation_snapshot> new_buffer(capacity + 1, reserve_tag);
        if (std::cmp_less(new_buffer.capacity(), capacity))
            return false;

        simulation_snapshot* ptr = nullptr;
        while (next(ptr))
            new_buffer.push_back(*ptr);

        const auto new_capacity = capacity + 1;
        const auto new_front    = 0;
        const auto new_back     = ssize();

        m_capacity = new_capacity;
        m_front    = new_front;
        m_back     = new_back;
        ring       = std::move(new_buffer);
    }

    return true;
}

void simulation_snapshot_handler::emplace_back(const simulation& sim) noexcept
{
    if ((m_back + 1) % m_capacity == m_front)
        m_front = (m_front + 1) % m_capacity;

    ring[m_back] = sim;
    m_back       = (m_back + 1) % m_capacity;
}

void simulation_snapshot_handler::reset() noexcept
{
    m_front    = 0;
    m_back     = 0;
    m_capacity = ring.capacity() + 1;
}

constexpr simulation_snapshot* simulation_snapshot_handler::front() noexcept
{
    return empty() ? nullptr : std::addressof(ring[m_front]);
}

constexpr const simulation_snapshot* simulation_snapshot_handler::front()
  const noexcept
{
    return empty() ? nullptr : std::addressof(ring[m_front]);
}

constexpr simulation_snapshot* simulation_snapshot_handler::back() noexcept
{
    return empty()       ? nullptr
           : m_back == 0 ? std::addressof(ring[m_capacity - 1])
                         : std::addressof(ring[m_back - 1]);
}

constexpr const simulation_snapshot* simulation_snapshot_handler::back()
  const noexcept
{
    return empty()       ? nullptr
           : m_back == 0 ? std::addressof(ring[m_capacity - 1])
                         : std::addressof(ring[m_back - 1]);
}

constexpr bool simulation_snapshot_handler::next(
  simulation_snapshot*& snap) noexcept
{
    if (snap == nullptr) {
        if (empty())
            return false;

        snap = &ring[m_front]; // otherwise returns oldest element.
        return true;
    }

    const auto idx = ring.index_from_ptr(snap);

    if ((idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[idx + 1]);
    return true;
}

constexpr bool simulation_snapshot_handler::next(
  const simulation_snapshot*& snap) const noexcept
{
    if (snap == nullptr) {
        if (m_front == m_back) // ring is empty, returns false
            return false;

        snap = &ring[m_front]; // otherwise returns oldest element.
        return true;
    }

    const auto idx = ring.index_from_ptr(snap);

    if ((idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[idx + 1]);
    return true;
}

constexpr bool simulation_snapshot_handler::previous(
  simulation_snapshot*& snap) noexcept
{
    if (snap == nullptr) {
        if (empty())
            return false;

        snap = m_back == 0 ? std::addressof(ring[m_capacity - 1])
                           : std::addressof(ring[m_back - 1]);
        return true;
    }

    const auto idx     = ring.index_from_ptr(snap);
    const auto new_idx = idx == 0 ? m_capacity - 1 : m_back - 1;

    if (new_idx % m_capacity == m_back) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[new_idx]);
    return true;
}

constexpr bool simulation_snapshot_handler::previous(
  const simulation_snapshot*& snap) const noexcept
{
    if (snap == nullptr) {
        if (m_front == m_back) // ring is empty, returns false
            return false;

        snap = &ring[m_front]; // otherwise returns oldest element.
        return true;
    }

    const auto idx = ring.index_from_ptr(snap);

    if ((idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[idx + 1]);
    return true;
}

constexpr void simulation_snapshot_handler::erase_after(
  const simulation_snapshot* snap) noexcept
{
    if (snap == nullptr)
        return;

    const auto idx = ring.index_from_ptr(snap);

    if ((idx + 1) % m_capacity == m_front)
        return;

    m_back = idx + 1;
}

constexpr unsigned simulation_snapshot_handler::capacity() const noexcept
{
    return m_capacity - 1;
}

constexpr unsigned simulation_snapshot_handler::size() const noexcept
{
    if (m_back >= m_front)
        return static_cast<unsigned>(m_back - m_front);

    return static_cast<unsigned>(m_capacity - (m_front - m_back));
}

constexpr int simulation_snapshot_handler::ssize() const noexcept
{
    if (m_back >= m_front)
        return m_back - m_front;

    return m_capacity - (m_front - m_back);
}

constexpr bool simulation_snapshot_handler::empty() const noexcept
{
    return m_front == m_back;
}

constexpr int simulation_snapshot_handler::index_from_ptr(
  const simulation_snapshot* ptr) const noexcept
{
    if (not is_ptr_valid(ptr))
        return -1;

    return ring.index_from_ptr(ptr);
}

constexpr bool simulation_snapshot_handler::is_ptr_valid(
  const simulation_snapshot* ptr) const noexcept
{
    return ring.data() <= ptr and ptr < ring.data() + ring.size();
}

constexpr const simulation_snapshot* simulation_snapshot_handler::ptr_from_index(
    std::integral auto idx) const noexcept
{

 }

} // namespace irt
