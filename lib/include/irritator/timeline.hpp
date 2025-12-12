// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022
#define ORG_VLEPROJECT_IRRITATOR_TIMELINE_2022

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/thread.hpp>

namespace irt {

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

    /// Clear the ring-buffer without deallocation.
    void reset() noexcept;

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
    constexpr bool is_ptr_valid(const simulation_snapshot* it) const noexcept;

    constexpr const simulation_snapshot* ptr_from_index(
      std::integral auto idx) const noexcept;

private:
    vector<simulation_snapshot> ring;

    int m_front    = 0;
    int m_back     = 0;
    int m_capacity = 1;
};

} // namespace irt

#endif