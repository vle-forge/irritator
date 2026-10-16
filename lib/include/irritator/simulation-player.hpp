// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_SIMULATION_PLAYER_HPP
#define ORG_VLEPROJECT_IRRITATOR_SIMULATION_PLAYER_HPP

#include <irritator/binary-simulation.hpp>
#include <irritator/thread.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <span>
#include <thread>

namespace irt {

/// The observers part of a step, sequential version: after @c simulation::run(),
/// the observers whose raw buffer is nearly full are resampled
/// (@c simulation::immediate_observers) and, at the end of the simulation, all
/// the observers.
///
/// The @c simulation_player does the same thing (in parallel if it can), so
/// use this function to build a reference with a plain @c run() loop: a
/// loop of @c sim.run() + @c sim.tick_resamplers() (all the observers at each
/// step) does not give the same histories.
inline void tick_resamplers_after_run(simulation& sim) noexcept
{
    if (sim.current_time_expired()) {
        sim.tick_resamplers();
    } else if (not sim.immediate_observers.empty()) {
        sim.tick_resamplers(std::span<const observer_id>(
          sim.immediate_observers.data(),
          static_cast<std::size_t>(sim.immediate_observers.size())));
    }
}

/// Options of the @c simulation_player.
struct simulation_player_options {
    /// A snapshot every @c keyframe_interval steps.
    u32 keyframe_interval = 64;

    /// Maximum number of keyframes (at least 2).
    u32 max_keyframes = 256;

    /// Minimum number of observers in a chunk of work for the parallel tick.
    /// The tick is parallel only if there are at least two chunks: with fewer
    /// observers, the cost of a round trip in the task list is higher than
    /// the work.
    u32 observers_per_chunk = 512;

    /// Maximum number of helper tasks added in the @c unordered_task_list by
    /// tick (the calling thread works too). 0 disables the parallel tick.
    u32 max_helpers = 7;
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
///   simulation (value, model, connection or event) at the current step, call
///   @c forget_history() after the edit: the edition is not part of the replay
///   (it is not a step), so the steps before can not be replayed any more.
///   @c forget_future() only forgets the steps after the current one.
/// - The observers are resampled after each @c run() as
///   @c tick_resamplers_after_run() does. With a @c unordered_task_list and
///   enough observers the work is shared between the calling thread and the
///   workers of the list. The list works in batch (@c add(), @c submit(),
///   @c wait_completion()) for one producer: it must be used by this player
///   only (the helpers live on the stack of the tick, which waits for them),
///   and the player must never be called from a task of its own list (the
///   helpers could never start: deadlock). The task manager must not be shut
///   down during a tick.
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

    /// @param utl The task list used to resample the observers in parallel,
    /// @c nullptr for a sequential tick (the list must outlive the player).
    explicit simulation_player(simulation&          sim,
                               unordered_task_list* utl = nullptr,
                               const options        opt = options{}) noexcept;

    explicit simulation_player(simulation&          sim,
                               unordered_task_list& utl,
                               const options        opt = options{}) noexcept
      : simulation_player(sim, &utl, opt)
    {}

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
                   const step_type                max_steps =
                     std::numeric_limits<step_type>::max()) noexcept;

    /// Steps back until the budget is spent, @c max_steps steps are done or
    /// the step 0 is reached: the "play backward" mode (see @c advance() for
    /// @c max_steps).
    status retreat(const std::chrono::nanoseconds budget,
                   const step_type                max_steps =
                     std::numeric_limits<step_type>::max()) noexcept;

    /// Forgets the steps after the current one (the user can not go forward
    /// to them any more). It does NOT make the past consistent with an edition
    /// of the simulation: a rewind restores the whole state of the past
    /// (models, parameters, connections), without the edition. After an
    /// edition, use @c forget_history().
    status forget_future() noexcept;

    /// Date of the next event, @c infinity if there is none. It is capped by
    /// the end of the simulation: the "event" that finishes the simulation is
    /// the one at the end date.
    time next_time() const noexcept;

    /// Real time mode: steps forward while the date of the next event is
    /// lower than or equal to @c target, until the budget is spent or
    /// @c max_steps steps are done.
    ///
    /// Every event with a date lower than or equal to @c target is done at
    /// the end of the call (if the budget is enough). The simulation clock
    /// (@c current_time()) is then moved to @c target: the state of the
    /// simulation is the state at the date @c target, and it is the right
    /// moment to edit the simulation (a model created now is initialized at
    /// @c target).
    ///
    /// If the simulation has no end (@c is_live()), the simulation does not
    /// finish when there is no more event: the clock goes on and the player
    /// waits for an edition. Otherwise, when there is no more event, the
    /// simulation is finished (as with @c step_forward()).
    status advance_until(const time                     target,
                         const std::chrono::nanoseconds budget,
                         const step_type                max_steps =
                           std::numeric_limits<step_type>::max()) noexcept;

    /// Call it after an edition of the simulation (value, model, connection,
    /// event) at the current step. The steps before can not be replayed: the
    /// edition is not part of the replay (it is not a step), so all the
    /// history is dropped and the current state is the new beginning:
    /// @c earliest_step() equals @c step().
    status forget_history() noexcept;

    /* * * * * * * *
     *
     * observers
     *
     * * * * * * * */

    /// Return true iff simulation started.
    bool started() const noexcept { return m_started; }

    /// Return the current step [earliest_step..furthest_step]: The user can go
    /// to it with @c go_to().
    step_type step() const noexcept { return m_step; }

    /// The last step that was reached: the user can go forward to it with
    /// @c go_to() without running new steps.
    step_type furthest_step() const noexcept { return m_furthest; }

    /// The first step that can be reached backward: 0 until the first
    /// @c forget_history().
    step_type earliest_step() const noexcept { return m_floor; }

    /// True if @c step_back() can go back: started and after the first step
    /// that can be reached (@c earliest_step()).
    bool can_step_back() const noexcept { return m_started and m_step > m_floor; }

    /// True if the simulation is at its end (time limit or no more events).
    bool finished() const noexcept;

    time current_time() const noexcept { return m_sim.current_time(); }

    std::size_t keyframe_number() const noexcept
    {
        return static_cast<std::size_t>(m_keyframes.size());
    }

    /// True if the simulation has no end date: it never finishes by itself.
    bool is_live() const noexcept
    {
        return time_domain<time>::is_infinity(m_sim.limits.end());
    }

    /// True if step_forward() can do a step: started, not finished and, for a
    /// simulation without end, something to run.
    bool can_step_forward() const noexcept
    {
        return m_started and not finished() and
               not(is_live() and m_sim.sched.empty());
    }

    /// Bytes used by the keyframes and by the window.
    std::size_t memory_usage() const noexcept;

private:
    struct keyframe {
        step_type  step = 0;
        vector<u8> data;
    };

    simulation&           m_sim;
    unordered_task_list* m_utl;

    binary_writer      m_writer;    // reused: no allocation for a snapshot
    vector<keyframe>   m_keyframes; // sorted by step, the first is the step 0
    vector<vector<u8>> m_window;    // snapshots of the steps
                                    // [m_window_first, m_window_first + size)
    step_type m_window_first = 0;
    step_type m_step         = 0;
    step_type m_furthest     = 0;
    step_type m_floor        = 0; // first reachable step (forget_history)
    u32       m_interval;
    u32       m_max_keyframes;
    u32       m_chunk;
    u32       m_helpers;
    bool      m_started = false;

    vector<observer_id> m_obs_ids; // work buffer of the parallel tick

    /// The work of one parallel tick, shared between the calling thread and
    /// the helpers: each one claims chunks until there is none. It lives on
    /// the stack of the tick, which waits for the end of the helpers.
    struct tick_job {
        simulation*              sim         = nullptr;
        const observer_id*       ids         = nullptr;
        std::size_t              count       = 0;
        std::size_t              chunk       = 1;
        std::size_t              chunk_count = 0;
        std::atomic<std::size_t> next{ 0 }; // next chunk to claim

        void work() noexcept;
    };

    status do_step() noexcept;
    status snapshot_to(vector<u8>& out) noexcept;
    status restore_from(const vector<u8>& data) noexcept;
    status push_keyframe() noexcept;

    /// After @c run(): see @c tick_resamplers_after_run().
    void tick_observers() noexcept;

    /// True if the tick of @c n observers is worth the task list.
    bool parallel_tick(const std::size_t n) const noexcept;

    /// Resamples @c ids (without duplicates) with the task list.
    void tick_parallel(const std::span<const observer_id> ids) noexcept;

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

/// Maps the wall clock to the simulation time for the "real time" mode: one
/// unit of simulation time lasts @c unit_duration() of real time.
///
/// The pacer only does arithmetic (no thread, no sleep, no simulation): a
/// simulation is real time when it only runs the events whose date is lower
/// than or equal to @c target(). The wall clock is a parameter of every
/// function (default: @c steady_clock::now()) to be testable.
///
/// Not thread safe: use it from the task that runs the simulation.
class realtime_pacer
{
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration   = std::chrono::nanoseconds;

    explicit realtime_pacer(
      const duration unit = std::chrono::seconds(1)) noexcept
      : m_unit(std::max(unit, duration(1)))
    {}

    /// Real duration of one unit of simulation time.
    duration unit_duration() const noexcept { return m_unit; }

    /// Says that the simulation time @c sim_now is reached at @c wall_now.
    /// Use it at the start, after a pause (otherwise the simulation runs
    /// fast to catch up with the real time) and after a long delay.
    void rebase(const time       sim_now,
                const time_point wall_now = clock::now()) noexcept
    {
        m_sim_ref  = sim_now;
        m_wall_ref = wall_now;
    }

    /// Changes the real duration of one unit of simulation time. The target
    /// does not jump: the new speed applies from @c wall_now.
    void set_unit_duration(const duration   unit,
                           const time_point wall_now = clock::now()) noexcept
    {
        rebase(target(wall_now), wall_now);
        m_unit = std::max(unit, duration(1));
    }

    /// The simulation time that the simulation must have reached at @c
    /// wall_now: all the events with a date lower than or equal to it are
    /// due.
    time target(const time_point wall_now = clock::now()) const noexcept
    {
        const auto elapsed = wall_now - m_wall_ref;

        return m_sim_ref + static_cast<double>(elapsed.count()) /
                             static_cast<double>(m_unit.count());
    }

    /// The wall clock date of the simulation time @c t (to sleep until the
    /// next event for instance).
    time_point wall_of(const time t) const noexcept
    {
        const auto ns = (t - m_sim_ref) * static_cast<double>(m_unit.count());

        return m_wall_ref + std::chrono::duration_cast<duration>(
                              std::chrono::duration<double, std::nano>(ns));
    }

    /// How late is the event at @c next_event_time: zero if it is not due
    /// yet. The date of the last event is not a good parameter: nothing is
    /// late when there is no event.
    duration lateness(const time       next_event_time,
                      const time_point wall_now = clock::now()) const noexcept
    {
        const auto late = target(wall_now) - next_event_time;

        if (not(late > 0.0))
            return duration::zero();

        return std::chrono::duration_cast<duration>(
          std::chrono::duration<double, std::nano>(
            late * static_cast<double>(m_unit.count())));
    }

    /// If the next event is more than @c max_lateness late (the simulation is
    /// slower than the real time, or the process was suspended), forgets the
    /// delay: the simulation goes on from there at the real speed instead of
    /// running flat out to catch up. Returns true if the delay is forgotten.
    bool resync_if_late(const time       next_event_time,
                        const duration   max_lateness,
                        const time_point wall_now = clock::now()) noexcept
    {
        if (lateness(next_event_time, wall_now) <= max_lateness)
            return false;

        rebase(next_event_time, wall_now);
        return true;
    }

private:
    time       m_sim_ref  = 0.0;
    time_point m_wall_ref = clock::now();
    duration   m_unit;
};

} // namespace irt

#endif
