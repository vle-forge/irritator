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
    using size_type  = vector<simulation_snapshot>::size_type;
    using index_type = std::make_signed_t<size_type>;

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

    constexpr size_type  capacity() const noexcept;
    constexpr size_type  size() const noexcept;
    constexpr index_type ssize() const noexcept;
    constexpr bool       empty() const noexcept;

    constexpr int index_of(const simulation_snapshot* data) const noexcept;
    constexpr const simulation_snapshot* ptr_from_index(int idx) const noexcept;

private:
    vector<simulation_snapshot> ring;

    std::size_t m_front    = 0;
    std::size_t m_back     = 0;
    std::size_t m_capacity = 1;
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

    const auto idx = ring.index_of(snap);

    if ((*idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[*idx + 1]);
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

    const auto idx = ring.index_of(snap);

    if ((*idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[*idx + 1]);
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

    const auto idx     = ring.index_of(snap);
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

    const auto idx = ring.index_of(snap);

    if ((*idx + 1) % m_capacity == m_front) {
        snap = nullptr;
        return false;
    }

    snap = std::addressof(ring[*idx + 1]);
    return true;
}

inline constexpr void simulation_snapshot_handler::erase_after(
  const simulation_snapshot* snap) noexcept
{
    if (snap == nullptr)
        return;

    const auto idx = ring.index_of(snap);

    if ((*idx + 1) % m_capacity == m_front)
        return;

    m_back = *idx + 1;
}

inline constexpr simulation_snapshot_handler::size_type
simulation_snapshot_handler::capacity() const noexcept
{
    return m_capacity - 1;
}

inline constexpr simulation_snapshot_handler::size_type
simulation_snapshot_handler::size() const noexcept
{
    if (m_back >= m_front)
        return static_cast<size_type>(m_back - m_front);

    return static_cast<size_type>(m_capacity - (m_front - m_back));
}

inline constexpr simulation_snapshot_handler::index_type
simulation_snapshot_handler::ssize() const noexcept
{
    if (m_back >= m_front)
        return m_back - m_front;

    return static_cast<index_type>(m_capacity - (m_front - m_back));
}

inline constexpr bool simulation_snapshot_handler::empty() const noexcept
{
    return m_front == m_back;
}

inline constexpr int simulation_snapshot_handler::index_of(
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
    if (idx < 0 or std::cmp_greater(idx, m_capacity))
        return nullptr;

    const auto cidx = static_cast<simulation_snapshot_handler::size_type>(idx);

    if (m_front < m_back)
        return (m_front <= cidx and cidx < m_back) ? std::addressof(ring[cidx])
                                                   : nullptr;

    return ((m_front <= cidx and cidx < m_capacity) or cidx < m_back)
             ? std::addressof(ring[cidx])
             : nullptr;
}

} // namespace irt

#endif
