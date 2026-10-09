// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_BINARY_SIMULATION_HPP
#define ORG_VLEPROJECT_IRRITATOR_BINARY_SIMULATION_HPP

#include <irritator/binary-containers.hpp>
#include <irritator/binary.hpp>
#include <irritator/core.hpp>

/// Binary persistence of a @c irt::simulation (the implementation is in
/// @c binary.cpp).
///
/// Two formats, both little endian, both with a header (magic number, version,
/// byte order mark):
///
/// - the snapshot (@c 'IRTS'): the part of the simulation that changes while it
///   runs, to roll back a simulation: the models with their dynamics, the
///   observers (raw samples, history and resampler), the connections, the
///   dated messages, the messages that wait for the next step, the scheduler
///   and the current time. The external
///   sources, the hierarchical state machines and the parameters are not
///   written: the snapshot is read in the simulation it comes from (or in a
///   simulation built the same way).
/// - the dump (@c 'IRTD'): everything that is needed to rebuild a simulation
///   in an empty @c irt::simulation: the snapshot, the limits, the external
///   sources (definitions and positions), the hierarchical state machines and
///   the parameters.
///
/// What is restored is the exact state of the containers (see
/// @c binary-containers.hpp): the models keep their identifiers, the
/// scheduler keeps its handles, so the next run is the same run.
///
/// Limits
/// - The simulation must be written between two steps of @c run(), never
///   during a step. The messages that were sent during a step and that the
///   models read in the next one (@c simulation::message_buffer and the
///   positions of the input ports) are part of the state and are written.
///   The writer checks the consistency of the simulation (scheduler,
///   observers, connections, input ports, references between models) and
///   refuses to write an inconsistent state.
/// - A simulation that holds embedded simulations (@c simulation::sims,
///   @c simulation_wrapper) is not supported.
/// - The files of the external sources are not part of the dump: the dump
///   stores their paths and positions, the reader opens the files again.
///
/// The readers are defensive: a corrupted or truncated input gives an error
/// and a cleared simulation (never an access outside the containers, an
/// infinite loop in the scheduler or a model that references something that
/// does not exist). Use @c binary_reader::set_allocation_limit for an input
/// that does not come from the program itself.
namespace irt {

/// 'IRTS': snapshot of a simulation.
inline constexpr u32 binary_snapshot_magic = binary_magic('I', 'R', 'T', 'S');

/// 'IRTD': dump of a simulation.
inline constexpr u32 binary_simulation_magic = binary_magic('I', 'R', 'T', 'D');

/// Version of both formats. A reader refuses another version.
inline constexpr u32 binary_simulation_version = 1u;

/// Writes the snapshot of the simulation.
///
/// Errors: @c simulation_errc::range_error if the simulation is not in a
/// consistent state or holds embedded simulations, @c
/// simulation_errc::memory_error if the writer can not grow.
status write_snapshot(binary_writer& w, const simulation& sim) noexcept;

/// Replaces the models, the observers, the connections, the dated messages,
/// the scheduler and the time of @c sim with the snapshot. The allocated
/// memory of @c sim is reused (it grows if the snapshot is bigger, it never
/// shrinks).
///
/// After an error, these parts of @c sim are cleared (the simulation is
/// valid and empty): the program must restore another snapshot or rebuild the
/// simulation.
///
/// Errors: @c simulation_errc::file_eof_error if the input is truncated,
/// @c simulation_errc::range_error if it is not a snapshot, if a value is not
/// valid or if the allocation limit of the reader is exceeded, or the error
/// of the external sources that read a file again. The memory error is only
/// reported by the writer.
status read_snapshot(binary_reader& r, simulation& sim) noexcept;

/// Writes the dump of the simulation (see the errors of @c write_snapshot).
status write_simulation(binary_writer& w, const simulation& sim) noexcept;

/// Reads a dump in @c sim. The previous content of @c sim (models, external
/// sources, state machines) is replaced. After an error @c sim is cleared.
status read_simulation(binary_reader& r, simulation& sim) noexcept;

/// Writes the dump of the simulation in the file @c p.
status save_simulation(const path& p, const simulation& sim) noexcept;

/// Reads the dump in the file @c p. @c max_allocation limits the memory used
/// by the containers, 0 means no limit.
status load_simulation(const path&  p,
                       simulation&  sim,
                       std::size_t  max_allocation = 0) noexcept;

} // namespace irt

#endif
