// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/timeline.hpp>

namespace irt {

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
  const int capacity) noexcept
{
    if (capacity > 0) {
        m_capacity = capacity + 1;
        m_front    = 0;
        m_back     = 0;

        ring.resize(capacity);
    }
}

bool simulation_snapshot_handler::reserve(const int capacity) noexcept
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

} // namespace irt
