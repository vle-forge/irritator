// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_SIMULATION_PLAYER_HPP
#define ORG_VLEPROJECT_IRRITATOR_SIMULATION_PLAYER_HPP

#include <irritator/binary-simulation.hpp>

#include <chrono>

namespace irt {

/// Options of the @c simulation_player.
struct simulation_player_options {
    /// A snapshot every @c keyframe_interval steps.
    u32 keyframe_interval = 64;

    /// Maximum number of keyframes (at least 2).
    u32 max_keyframes = 256;
};

/// Moves a simulation forward and backward in "steps" (one call of
/// @c simulation::run() followed by @c tick_resamplers()), for a user
/// interface: step forward, step back, go to a step, play, play backward.
///
/// How it works
/// - Going forward is the simulation itself.
/// - Going back restores a snapshot and replays the simulation forward.
/// - A snapshot (a keyframe) is kept every @c options::keyframe_interval
///   steps. Going back to a step costs at most the replay of one interval.
///   When @c options::max_keyframes is reached, every other keyframe is
///   dropped (the first and the last are kept): the oldest part of the history
///   becomes sparser, so going far back is slower, but the memory is bounded.
/// - The steps between a keyframe and the step where the user stops are kept
///   too (the "window"), so a series of @c step_back() is one restore each,
///   not a replay each.
///
/// Rules
/// - The player does not own the simulation and is not thread safe: call it
///   from the thread that owns the simulation.
/// - The simulation must only be changed by the player. If the user edits the
///   simulation (value, model or connection) at the current step, call
///   @c forget_future() after the edit: the steps after this one are no more
///   valid. The keyframes before are kept, and a replay from them uses the
///   edited simulation (the past that is shown after a rollback is the past
///   "as if" the edit had been there).
///
/// @TODO
/// - Simulations with @c simulation_wrapper are not supported (see
///   @c binary-simulation.hpp).
/// - Using @c hierarchy<keyframe> to more feature?
class simulation_player
{
public:
    using step_type = u64;
    using options   = simulation_player_options;

    explicit simulation_player(simulation&   sim,
                               const options opt = options{}) noexcept;

    simulation_player(const simulation_player&)            = delete;
    simulation_player& operator=(const simulation_player&) = delete;

    /* * * * * * * *
     *
     * simulation handler
     *
     * * * * * * * */

    /// Initializes the simulation (@c simulation::initialize()) and goes to
    /// the step 0. Use it again to restart.
    status start() noexcept;

    /// One step forward. Nothing happens when the simulation is finished.
    status step_forward() noexcept;

    /// One step back (nothing at the step 0).
    status step_back() noexcept;

    /// Goes to the step @c n, forward or backward. Forward, the simulation
    /// stops at the end of the simulation.
    status go_to(const step_type n) noexcept;

    /// Steps forward until the budget is spent, @c max_steps steps are done
    /// or the simulation is finished: call it at each frame of a "play" mode,
    /// with a budget smaller than the duration of a frame. Without
    /// @c max_steps the player runs as fast as it can, which is too fast to
    /// follow when a step is cheap: give a small @c max_steps (1 to 10) to
    /// control the speed of the playback.
    status advance(const std::chrono::nanoseconds budget,
        const step_type max_steps = std::numeric_limits<step_type>::max()) noexcept;

    /// Steps back until the budget is spent, @c max_steps steps are done or
    /// the step 0 is reached: the "play backward" mode (see @c advance() for
    /// @c max_steps).
    status retreat(const std::chrono::nanoseconds budget,
        const step_type max_steps = std::numeric_limits<step_type>::max()) noexcept;

    /// Call it after an edition of the simulation at the current step: the
    /// steps after the current one are forgotten.
    status forget_future() noexcept;

    /* * * * * * * *
     *
     * observers
     *
     * * * * * * * */

    /// Return true iff simulation started.
    bool started() const noexcept { return m_started; }

    /// Return the current step [0..furthest_step]: The user can go to it with
    /// @c got_to().
    step_type step() const noexcept { return m_step; }

    /// The last step that was reached: the user can go forward to it with
    /// @c go_to() without running new steps.
    step_type furthest_step() const noexcept { return m_furthest; }

    /// Return true iff simulation started() and step() greater than 0.
    bool can_step_back() const noexcept { return m_started and m_step != 0; }

    /// True if the simulation is at its end (time limit or no more events).
    bool finished() const noexcept;

    time current_time() const noexcept { return m_sim.current_time(); }

    std::size_t keyframe_number() const noexcept
    {
        return static_cast<std::size_t>(m_keyframes.size());
    }

    /// Bytes used by the keyframes and by the window.
    std::size_t memory_usage() const noexcept;

private:
    struct keyframe {
        step_type  step = 0;
        vector<u8> data;
    };

    simulation&        m_sim;
    binary_writer      m_writer;    // reused: no allocation for a snapshot
    vector<keyframe>   m_keyframes; // sorted by step, the first is the step 0
    vector<vector<u8>> m_window;    // snapshots of the steps
                                    // [m_window_first, m_window_first + size)
    step_type m_window_first = 0;
    step_type m_step         = 0;
    step_type m_furthest     = 0;
    u32       m_interval;
    u32       m_max_keyframes;
    bool      m_started = false;

    status do_step() noexcept;
    status snapshot_to(vector<u8>& out) noexcept;
    status restore_from(const vector<u8>& data) noexcept;
    status push_keyframe() noexcept;

    /// Takes a keyframe when the simulation goes beyond the last one (a
    /// replay of a known part of the history does not take any).
    status keyframe_if_needed() noexcept;

    /// Keeps the first, the last and one over two in between.
    void thin_keyframes() noexcept;

    /// Restores the nearest keyframe before the step @c n and replays the
    /// simulation up to @c n. If @c fill_window, the intermediate steps are
    /// kept for the next calls.
    status rewind_to(const step_type n, const bool fill_window) noexcept;
};

} // namespace irt

#endif
