// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>

namespace irt {

/// Stores a simulation-snapshot circular buffer.
///
/// This class stores simulation snapshot and build a timeline of simulations.
class simulation_snapshot_handler
{
public:
    constexpr simulation_snapshot_handler() noexcept = default;
    explicit simulation_snapshot_handler(const int capacity) noexcept;

    bool reserve(const int capacity) noexcept;
    void emplace_back(const simulation& sim) noexcept;

    /// Clear the ring-buffer without deallocation.
    constexpr void reset() noexcept;

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
    constexpr const simulation_snapshot* ptr_from_index(int idx) const noexcept;

private:
    vector<simulation_snapshot> ring;

    int m_front    = 0;
    int m_back     = 0;
    int m_capacity = 1;
};

// Implementation

inline constexpr void simulation_snapshot_handler::reset() noexcept
{
    m_front    = 0;
    m_back     = 0;
    m_capacity = ring.capacity() + 1;
}

inline constexpr simulation_snapshot*
simulation_snapshot_handler::front() noexcept
{
    return empty() ? nullptr : std::addressof(ring[m_front]);
}

inline constexpr const simulation_snapshot* simulation_snapshot_handler::front()
  const noexcept
{
    return empty() ? nullptr : std::addressof(ring[m_front]);
}

inline constexpr simulation_snapshot*
simulation_snapshot_handler::back() noexcept
{
    return empty()       ? nullptr
           : m_back == 0 ? std::addressof(ring[m_capacity - 1])
                         : std::addressof(ring[m_back - 1]);
}

inline constexpr const simulation_snapshot* simulation_snapshot_handler::back()
  const noexcept
{
    return empty()       ? nullptr
           : m_back == 0 ? std::addressof(ring[m_capacity - 1])
                         : std::addressof(ring[m_back - 1]);
}

inline constexpr bool simulation_snapshot_handler::next(
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

inline constexpr bool simulation_snapshot_handler::next(
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

inline constexpr bool simulation_snapshot_handler::previous(
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

inline constexpr bool simulation_snapshot_handler::previous(
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

inline constexpr void simulation_snapshot_handler::erase_after(
  const simulation_snapshot* snap) noexcept
{
    if (snap == nullptr)
        return;

    const auto idx = ring.index_from_ptr(snap);

    if ((idx + 1) % m_capacity == m_front)
        return;

    m_back = idx + 1;
}

inline constexpr unsigned simulation_snapshot_handler::capacity() const noexcept
{
    return m_capacity - 1;
}

inline constexpr unsigned simulation_snapshot_handler::size() const noexcept
{
    if (m_back >= m_front)
        return static_cast<unsigned>(m_back - m_front);

    return static_cast<unsigned>(m_capacity - (m_front - m_back));
}

inline constexpr int simulation_snapshot_handler::ssize() const noexcept
{
    if (m_back >= m_front)
        return m_back - m_front;

    return m_capacity - (m_front - m_back);
}

inline constexpr bool simulation_snapshot_handler::empty() const noexcept
{
    return m_front == m_back;
}

inline constexpr int simulation_snapshot_handler::index_from_ptr(
  const simulation_snapshot* ptr) const noexcept
{
    if (not(ring.data() <= ptr and ptr < (ring.data() + ring.size())))
        return -1;

    const auto distance = std::distance(ptr, ring.data());
    if (std::cmp_less_equal(0, distance) and
        std::cmp_less(distance, INTMAX_MAX))
        if (ptr_from_index(static_cast<int>(distance)))
            return static_cast<int>(distance);

    return -1;
}

inline constexpr const simulation_snapshot*
simulation_snapshot_handler::ptr_from_index(int idx) const noexcept
{
    if (idx < 0)
        return nullptr;

    if (m_front < m_back)
        return (m_front <= idx and idx < m_back) ? std::addressof(ring[idx])
                                                 : nullptr;

    return ((m_front <= idx and idx < m_capacity) or idx < m_back)
             ? std::addressof(ring[idx])
             : nullptr;
}

} // namespace irt

#endif
