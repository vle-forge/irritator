// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

// The functions of the simulation which dispatch on all the dynamics: defined
// in the header, they were instantiated in each source including core.hpp.

namespace irt {

status simulation::watch(const model_id id) noexcept
{
    auto* mdl = models.try_to_get(id);
    if (not mdl)
        return make_error(simulation_errc::watch_model_error);

    const auto can_tell = dispatch(*mdl, []<typename Dynamics>(Dynamics&) {
        return has_output_port<Dynamics> or
               has_watched_value_function<Dynamics>;
    });

    if (not can_tell)
        return make_error(simulation_errc::watch_model_error);

    const auto index = get_index(id);
    const auto word  = index >> 6;

    if (std::cmp_greater_equal(word, m_watched.size()) and
        not m_watched.resize(word + 1u, u64{ 0 }))
        return make_error(simulation_errc::watched_events_full);

    const auto bit = u64{ 1 } << (index & 63u);
    if ((m_watched[word] & bit) == 0u) {
        m_watched[word] |= bit;
        ++m_watched_count;
    }

    return success();
}

status simulation::make_watched_lambda(model& mdl) noexcept
{
    const auto id = models.get_id(mdl);

    return dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_lambda_function<Dynamics> and
                      has_output_port<Dynamics>) {
            // A message to an output port without connection is dropped by
            // send_message(): the watched outputs get a port, even after a
            // rewind to a snapshot taken before the watch.
            for (auto& y : dyn.y) {
                if (not output_ports.try_to_get(y)) {
                    if (not output_ports.can_alloc(1) and
                        not output_ports.grow<2, 1>())
                        return make_error(
                          simulation_errc::output_ports_container_full);

                    y = output_ports.get_id(output_ports.alloc());
                }
            }

            const auto first = active_output_ports.size();
            irt_check(dyn.lambda(*this));

            for (auto i = first, e = active_output_ports.size(); i != e; ++i)
                for (i32 p = 0, n = length(dyn.y); p != n; ++p)
                    if (dyn.y[p] == active_output_ports[i])
                        irt_check(push_watched_event(
                          id, p, active_output_messages[i][0]));
        }

        return success();
    });
}

void simulation::deallocate(model_id id) noexcept
{
    auto* mdl = models.try_to_get(id);
    debug::ensure(mdl);

    unwatch(id);

    unobserve(*mdl);

    dispatch(*mdl, [this]<typename Dynamics>(Dynamics& dyn) {
        this->do_deallocate<Dynamics>(dyn);
    });

    sched.free(*mdl);
    models.free(*mdl);
}

status simulation::connect(model&       src,
                           int          port_src,
                           const model& dst,
                           int          port_dst) noexcept
{
    if (not is_ports_compatible(src, port_src, dst, port_dst))
        return make_error(simulation_errc::connection_incompatible);

    if (not can_connect(src, port_src, dst, port_dst))
        return make_error(simulation_errc::connection_already_exists);

    auto ret = dispatch(dst, [&]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_input_port<Dynamics>) {
            if (0 <= port_dst and port_dst < length(dyn.x))
                return success();

            return make_error(simulation_errc::input_port_error);
        }

        irt::unreachable();
    });

    if (ret.has_error())
        return ret.error();

    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_output_port<Dynamics>) {
            return connect(dyn.y[port_src], get_id(dst), port_dst);
        }

        irt::unreachable();
    });
}

bool simulation::can_connect(const model& src,
                             int          port_src,
                             const model& dst,
                             int          port_dst) const noexcept
{
    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
        if constexpr (has_output_port<Dynamics>) {
            if (const auto* y = output_ports.try_to_get(dyn.y[port_src])) {
                for (const auto& elem : y->connections)
                    if (elem.model == get_id(dst) and
                        elem.port_index == port_dst)
                        return false;

                for (const auto* block = nodes.try_to_get(y->next); block;
                     block             = nodes.try_to_get(block->next)) {

                    for (const auto& elem : block->nodes)
                        if (elem.model == get_id(dst) and
                            elem.port_index == port_dst)
                            return false;
                }
            }
        }

        return true;
    });
}

void simulation::disconnect(model& src,
                            int    port_src,
                            model& dst,
                            int    port_dst) noexcept
{
    dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        if constexpr (has_output_port<Dynamics>) {
            if (auto* y = output_ports.try_to_get(dyn.y[port_src])) {
                for (auto it = y->connections.begin();
                     it != y->connections.end();) {
                    if (it->model == models.get_id(dst) and
                        it->port_index == port_dst) {
                        y->connections.swap_pop_back(it);
                        return;
                    } else {
                        ++it;
                    }
                }

                block_node* prev = nullptr;

                for (auto* block = nodes.try_to_get(y->next); block;
                     block       = nodes.try_to_get(block->next)) {

                    auto connection_deleted = false;
                    for (auto it = block->nodes.begin();
                         it != block->nodes.end();) {
                        if (it->model == models.get_id(dst) &&
                            it->port_index == port_dst) {
                            block->nodes.swap_pop_back(it);
                            connection_deleted = true;
                            break;
                        } else {
                            ++it;
                        }
                    }

                    if (connection_deleted and block->nodes.empty()) {
                        if (prev != nullptr) {
                            prev->next = block->next;
                            nodes.free(*block);
                            block = prev;
                        } else {
                            if (auto* next_block = nodes.try_to_get(
                                  block->next)) {
                                block->nodes = next_block->nodes;
                                block->next  = next_block->next;
                                nodes.free(*next_block);
                            } else {
                                nodes.free(*block);
                                y->next = undefined<block_node_id>();
                                break;
                            }
                        }
                    } else {
                        prev = block;
                    }
                }
            }
        }
    });
}

status simulation::initialize() noexcept
{
    last_valid_t = limits.begin();
    t            = limits.begin();

    clean();

    for (auto& h : hsms)
        h.flags.set(hierarchical_state_machine::option::use_source,
                    h.compute_is_using_source());

    for (auto& mdl : models)
        irt_check(make_initialize(mdl, t));

    for (auto& obs : observers) {
        obs.reset();
        observers.get<resampler>(observers.get_id(obs)).reset();

        if (auto* mdl = models.try_to_get(obs.model())) {
            dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                if constexpr (has_observation_function<Dynamics>) {
                    obs.observe(dyn.observation(t, t - mdl->tl));
                }
            });
        } else {
            observers.free(observers.get_id(obs));
        }
    }

    return success();
}

status simulation::make_initialize(model& mdl, time t) noexcept
{
    return dispatch(
      mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) -> status {
          return this->make_initialize(mdl, dyn, t);
      });
}

status simulation::make_transition(model& mdl, time t) noexcept
{
    return dispatch(mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) {
        return this->make_transition(mdl, dyn, t);
    });
}

status simulation::make_lambda(model& mdl) noexcept
{
    if (m_watched_count > 0 and is_watched_index(get_index(models.get_id(mdl))))
        return make_watched_lambda(mdl);

    return dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_lambda_function<Dynamics> and
                      has_output_port<Dynamics>)
            return dyn.lambda(*this);
        else
            return success();
    });
}

status simulation::finalize() noexcept
{
    debug::ensure(std::isfinite(t.load()));

    for (auto& mdl : models) {
        observer* obs = nullptr;
        if (is_defined(mdl.obs_id))
            obs = observers.try_to_get(mdl.obs_id);

        auto ret = dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
            return this->make_finalize(dyn, obs, t);
        });

        irt_check(ret);
    }

    return success();
}

status simulation::run() noexcept
{
    debug::ensure(std::isfinite(t.load()));

    immediate_models.clear();
    immediate_observers.clear();

    last_valid_t = t;
    t            = next_time();

    if (limits.expired(t)) {
        t = limits.end();
        return success();
    }

    for (const auto id : zero_ta_models)
        immediate_models.emplace_back(id);
    zero_ta_models.clear();

    if (not sched.empty() and sched.tn() == t)
        sched.pop(immediate_models);

    if (immediate_models.empty())
        return success();

    // A step of Parallel DEVS: the outputs of all the imminent models, the
    // routing of the messages, then one transition for each imminent or
    // receiving model with its bag.
    active_output_ports.clear();
    active_output_messages.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_lambda(*mdl));

    // First, we compute the input_port::capacity and record the input port of
    // each message. A receiver still in the scheduler is not imminent: it
    // leaves the scheduler to make its transition in this step.
    deliveries.clear();
    bool full = false;

    for (u32 i = 0, e = static_cast<u32>(active_output_ports.size()); i != e;
         ++i) {
        auto* y = output_ports.try_to_get(active_output_ports[i]);
        if (not y)
            continue;

        y->for_each(models, nodes, [&](model& mdl, const auto port_index) {
            dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                if constexpr (has_input_port<Dynamics>) {
                    auto& x = dyn.x[port_index];

                    // The ports of a model are reset by its transition: the
                    // first message of the step for a model not imminent. An
                    // imminent model has tn equal to t, a passive model is
                    // out of the scheduler.
                    if (x.capacity == 0 and mdl.tn != t and
                        std::ranges::all_of(dyn.x, [](const auto& p) {
                            return p.capacity == 0;
                        })) {
                        debug::ensure(mdl.handle != invalid_heap_handle);
                        if (sched.is_in_tree(mdl.handle))
                            sched.remove(mdl);

                        immediate_models.emplace_back(models.get_id(mdl));
                    }

                    x.capacity += 1u;
                    x.position = 0u;
                    x.size     = 0u;

                    if (not deliveries.emplace_back(
                          delivery{ .port = &x, .message = i }))
                        full = true;
                }
            });
        });
    }

    if (full)
        return make_error(simulation_errc::emitting_output_ports_full);

    // Then, we copy the @c active_output_messages on the @c message_buffer
    // according to the @c input_port::capacity: the messages of a port follow
    // each other.
    message_buffer.resize(deliveries.size());
    u32 global_position = 0;

    for (const auto& d : deliveries) {
        auto& x = *d.port;

        if (x.size == 0) {
            x.position = global_position;
            global_position += x.capacity;
        }

        message_buffer[x.position + x.size] = active_output_messages[d.message];
        ++x.size;

        if (x.size == x.capacity)
            detail::sort_bag(
              std::span(message_buffer.data() + x.position, x.size));
    }

    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_transition(*mdl, t));

    return success();
}

model& simulation::clone(const model& mdl) noexcept
{
    fatal::ensure(models.can_alloc(1));

    auto& new_mdl  = models.alloc();
    new_mdl.type   = mdl.type;
    new_mdl.handle = invalid_heap_handle;

    dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(mdl);
        std::construct_at(&dyn, src_dyn);

        parameters[models.get_id(new_mdl)] = parameters[models.get_id(mdl)];

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i].reset();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<output_port_id>();
    });

    return new_mdl;
}

model& simulation::alloc(dynamics_type type) noexcept
{
    fatal::ensure(models.can_alloc(1));

    auto& mdl  = models.alloc();
    mdl.type   = type;
    mdl.handle = invalid_heap_handle;

    dispatch(mdl, []<typename Dynamics>(Dynamics& dyn) -> void {
        std::construct_at(&dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i].reset();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<output_port_id>();

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            dyn.id = undefined<hsm_id>();
        }
    });

    return mdl;
}

} // namespace irt
