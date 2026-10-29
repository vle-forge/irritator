// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/simulation-player.hpp>

namespace irt {

simulation_player::simulation_player(simulation&   sim,
                                     const options opt) noexcept
  : m_sim(sim)
  , m_interval(std::max<u32>(opt.keyframe_interval, 1u))
  , m_max_keyframes(std::max<u32>(opt.max_keyframes, 2u))
  , m_chunk(std::max<u32>(opt.observers_per_chunk, 1u))
{}

simulation_player::simulation_player(simulation&   sim,
                                     task_manager& tm,
                                     const size_t  list_index,
                                     const options opt) noexcept
  : simulation_player(sim, opt)
{
    set_task_list(tm, list_index);
}

void simulation_player::set_task_list(task_manager& tm,
                                      const size_t  list_index) noexcept
{
    if (list_index < tm.list_size()) {
        m_utl     = &tm.unordered(list_index);
        m_helpers = static_cast<u32>(tm.unordered_worker_size());
    } else {
        m_utl     = nullptr;
        m_helpers = 0;
    }
}

status simulation_player::start() noexcept
{
    // simulation::finalize() empties the sources of the models (a constant
    // source forgets its definition): initialize() can not start again from a
    // finalized state. The first keyframe is a state before finalize, with the
    // sources of the models: it is the way back to a simulation that can start.
    if (m_finalized and not m_keyframes.empty() and
        not m_keyframes.front().finalized)
        irt_check(restore_from(m_keyframes.front().data));

    reset();

    irt_check(m_sim.initialize());

    if (m_sink)
        m_sink->rewind(m_sim);

    irt_check(push_keyframe());

    m_started = true;

    // A simulation can be finished at the beginning (end <= begin).
    return finalize_if_finished();
}

void simulation_player::reset() noexcept
{
    m_started   = false;
    m_finalized = false;
    m_keyframes.clear();
    m_window.clear();
    m_window_first = 0;
    m_step         = 0;
    m_furthest     = 0;
    m_floor        = 0;
}

status simulation_player::step_forward() noexcept
{
    // Not finished() only: a simulation without end and without event must
    // not call run() (it would set the time to infinity).
    if (not can_step_forward())
        return success();

    irt_check(do_step());
    irt_check(keyframe_if_needed()); // before finalize: keyframes are replayable

    return finalize_if_finished();
}

status simulation_player::step_back() noexcept
{
    if (not can_step_back())
        return success();

    return rewind_to(m_step - 1, true);
}

status simulation_player::go_to(const step_type n) noexcept
{
    if (not m_started)
        return success();

    if (n < m_step) {
        // Nothing to rewind when the floor is the current step (no history).
        const auto target = std::max(n, m_floor);

        if (target < m_step)
            irt_check(rewind_to(target, false));
    }

    while (m_step < n and can_step_forward())
        irt_check(step_forward());

    return success();
}

status simulation_player::advance(const std::chrono::nanoseconds budget,
                                  const step_type max_steps) noexcept
{
    const auto end = std::chrono::steady_clock::now() + budget;

    for (step_type i = 0; i < max_steps; ++i) {
        if (not can_step_forward())
            break;

        irt_check(step_forward());

        if (std::chrono::steady_clock::now() >= end)
            break;
    }

    return success();
}

status simulation_player::retreat(const std::chrono::nanoseconds budget,
                                  const step_type max_steps) noexcept
{
    const auto end = std::chrono::steady_clock::now() + budget;

    for (step_type i = 0; i < max_steps; ++i) {
        if (not can_step_back())
            break;

        irt_check(step_back());

        if (std::chrono::steady_clock::now() >= end)
            break;
    }

    return success();
}

status simulation_player::forget_future() noexcept
{
    if (not m_started)
        return success();

    while (not m_keyframes.empty() and m_keyframes.back().step > m_step)
        m_keyframes.pop_back();

    m_window.clear();
    m_furthest = m_step;

    if (not m_keyframes.empty() and m_keyframes.back().step == m_step) {
        m_keyframes.back().finalized = m_finalized;
        return snapshot_to(m_keyframes.back().data);
    }

    return push_keyframe();
}

time simulation_player::next_time() const noexcept
{
    if (not m_started or m_sim.sched.empty())
        return time_domain<time>::infinity;

    return std::min(m_sim.sched.tn(), m_sim.limits.end());
}

status simulation_player::advance_until(const time                     target,
                                        const std::chrono::nanoseconds budget,
                                        const step_type max_steps) noexcept
{
    const auto end  = std::chrono::steady_clock::now() + budget;
    bool       idle = false; // nothing is due: the clock can move.

    for (step_type i = 0; i < max_steps; ++i) {
        if (not m_started or finished())
            break;

        const auto next = next_time();

        if (time_domain<time>::is_infinity(next)) {
            if (is_live())
                idle = true; // wait for an edition
            else
                irt_check(step_forward()); // no more event: finishes
            break;
        }

        if (next > target) {
            idle = true;
            break;
        }

        irt_check(step_forward());

        if (std::chrono::steady_clock::now() >= end)
            break;
    }

    // Nothing is due: the simulation is at the date `target`. The clock moves,
    // so that an edition (model created, value changed) is dated `target`.
    if (idle and not finished() and target > m_sim.current_time())
        m_sim.current_time(target); // no effect outside of the limits

    return success();
}

status simulation_player::forget_history() noexcept
{
    if (not m_started)
        return success();

    m_keyframes.clear();
    m_window.clear();
    m_floor    = m_step;
    m_furthest = m_step;

    return push_keyframe();
}

status simulation_player::keep_history(const bool value) noexcept
{
    if (value == m_keep_history)
        return success();

    m_keep_history = value;

    if (not m_started)
        return success();

    // Nothing to forget: a single keyframe, the one of the current step.
    if (m_keyframes.size() == 1u and m_keyframes.back().step == m_step) {
        m_window.clear();
        m_floor    = m_step;
        m_furthest = m_step;
        return success();
    }

    return forget_history();
}

bool simulation_player::finished() const noexcept
{
    return not m_started or m_finalized or
           time_domain<time>::is_infinity(m_sim.current_time()) or
           m_sim.current_time_expired();
}

std::size_t simulation_player::memory_usage() const noexcept
{
    std::size_t ret = 0;

    for (const auto& k : m_keyframes)
        ret += static_cast<std::size_t>(k.data.size());
    for (const auto& w : m_window)
        ret += static_cast<std::size_t>(w.size());

    return ret;
}

/*
 * Observers
 */

void simulation_player::tick_job::work() noexcept
{
    for (;;) {
        const auto i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= chunk_count)
            return;

        const auto first = i * chunk;
        const auto last  = std::min(count, first + chunk);

        sim->tick_resamplers(
          std::span<const observer_id>(ids + first, last - first), now);
    }
}

bool simulation_player::parallel_tick(const std::size_t n) const noexcept
{
    return m_utl != nullptr and m_helpers > 0 and n >= 2u * m_chunk;
}

void simulation_player::tick_parallel(const std::span<const observer_id> ids,
                                      const time now) noexcept
{
    tick_job job;
    job.sim         = &m_sim;
    job.now         = now;
    job.ids         = ids.data();
    job.count       = ids.size();
    job.chunk       = m_chunk;
    job.chunk_count = (ids.size() + m_chunk - 1u) / m_chunk;

    const auto helpers = std::min<std::size_t>(m_helpers, job.chunk_count - 1u);
    std::size_t added  = 0;

    for (std::size_t i = 0; i != helpers; ++i)
        if (m_utl->add([p = &job]() noexcept { p->work(); }))
            ++added; // refused (list stopping): the caller works alone

    if (added > 0)
        m_utl->submit();

    job.work(); // the calling thread does its share, or all of it

    // The batch is over when all the helpers are over: job can be destroyed.
    // They have nothing left to claim (or they finish the chunk they claimed).
    if (added > 0)
        m_utl->wait_completion();
}

status simulation_player::drain_observers(
  const std::span<const observer_id> ids) noexcept
{
    return m_sim.drain_observations(
      ids, [this](const observation_batch& b) noexcept -> status {
          return m_sink ? m_sink->consume(b) : success();
      });
}

status simulation_player::drain_all_observers() noexcept
{
    return m_sim.drain_observations(
      [this](const observation_batch& b) noexcept -> status {
          return m_sink ? m_sink->consume(b) : success();
      });
}

status simulation_player::tick_all_observers(const time now) noexcept
{
    const auto n = static_cast<std::size_t>(m_sim.observers.size());

    if (not parallel_tick(n)) {
        m_sim.tick_resamplers(now);
        return drain_all_observers();
    }

    m_obs_ids.clear();
    for (const auto& obs : m_sim.observers)
        m_obs_ids.emplace_back(m_sim.observers.get_id(obs));

    const auto ids = std::span<const observer_id>(m_obs_ids.data(),
                                                  m_obs_ids.size());

    tick_parallel(ids, now);

    // The consumer is called by this thread only: it needs no lock against
    // the helpers of the parallel tick.
    return drain_observers(ids);
}

status simulation_player::flush_observers() noexcept
{
    if (m_started)
        return tick_all_observers(m_sim.current_time());

    return success();
}

status simulation_player::tick_observers() noexcept
{
    if (m_sim.current_time_expired())
        return tick_all_observers(m_sim.current_time());

    if (m_sim.immediate_observers.empty())
        return success();

    const auto ids = std::span<const observer_id>(
      m_sim.immediate_observers.data(),
      static_cast<std::size_t>(m_sim.immediate_observers.size()));

    if (not parallel_tick(ids.size())) {
        m_sim.tick_resamplers(ids, m_sim.current_time());
    } else {
        // The identifiers of immediate_observers are unique: two chunks never
        // tick the same observer, and the vector is not changed during the
        // tick.
        tick_parallel(ids, m_sim.current_time());
    }

    return drain_observers(ids);
}

status simulation_player::do_step() noexcept
{
    irt_check(m_sim.run());
    irt_check(tick_observers());

    ++m_step;
    m_furthest = std::max(m_furthest, m_step);

    return success();
}

status simulation_player::finalize_if_finished() noexcept
{
    if (not m_started or m_finalized or not finished())
        return success();

    // Whatever happens, never finalize twice: the first call may have cleaned
    // a part of the simulation.
    m_finalized = true;

    // Without any more event the date is infinite, and simulation::finalize()
    // needs a date: the date of the last event.
    if (time_domain<time>::is_infinity(m_sim.current_time())) {
        m_sim.current_time(m_sim.last_time()); // no effect outside of the limits

        if (time_domain<time>::is_infinity(m_sim.current_time()))
            return success();
    }

    irt_check(m_sim.finalize());

    // The final observations (and the near full observers) go to the consumer.
    irt_check(tick_observers());

    // Without this tick the last resampled point of each series stays pending.
    return tick_all_observers(time_domain<time>::infinity);
}

status simulation_player::snapshot_to(vector<u8>& out) noexcept
{
    m_writer.clear();
    irt_check(write_snapshot(m_writer, m_sim));

    const auto bytes = m_writer.bytes();
    out.assign(bytes.begin(), bytes.end());

    if (out.size() != bytes.size())
        return make_error(simulation_errc::memory_error);

    return success();
}

status simulation_player::restore_from(const vector<u8>& data) noexcept
{
    binary_reader r(std::span<const u8>(data.data(), data.size()));

    {
        std::unique_lock lock(m_restore_mutex);
        irt_check(read_snapshot(r, m_sim));
    }

    if (m_sink)
        m_sink->rewind(m_sim);

    return success();
}

status simulation_player::push_keyframe() noexcept
{
    if (m_keyframes.emplace_back() == nullptr)
        return make_error(simulation_errc::memory_error);

    m_keyframes.back().step      = m_step;
    m_keyframes.back().finalized = m_finalized;

    if (auto ret = snapshot_to(m_keyframes.back().data); ret.has_error()) {
        m_keyframes.pop_back();
        return ret;
    }

    return success();
}

status simulation_player::keyframe_if_needed() noexcept
{
    if (not m_keep_history) {
        m_floor = m_step; // nothing to go back to
        return success();
    }

    if (m_step <= m_keyframes.back().step or m_step % m_interval != 0)
        return success();

    irt_check(push_keyframe());

    if (m_keyframes.size() > m_max_keyframes)
        thin_keyframes();

    return success();
}

void simulation_player::thin_keyframes() noexcept
{
    const auto n = m_keyframes.size();
    auto       w = std::size_t{ 1 };

    for (std::size_t r = 1; r + 1 < static_cast<std::size_t>(n); ++r) {
        if (r % 2 == 0) {
            m_keyframes[w] = std::move(m_keyframes[r]);
            ++w;
        }
    }

    m_keyframes[w] = std::move(m_keyframes[n - 1]);
    ++w;

    while (m_keyframes.size() > w)
        m_keyframes.pop_back();
}

status simulation_player::rewind_to(const step_type n,
                                    const bool      fill_window) noexcept
{
    if (fill_window and n >= m_window_first and
        n - m_window_first < m_window.size()) {
        irt_check(restore_from(m_window[n - m_window_first]));
        m_step      = n;
        m_finalized = false; // the window only has states before finalize

        return finalize_if_finished();
    }

    auto k = static_cast<std::size_t>(m_keyframes.size());
    while (k > 1 and m_keyframes[k - 1].step > n)
        --k;

    const auto& kf = m_keyframes[k - 1];
    irt_check(restore_from(kf.data));
    m_step      = kf.step;
    m_finalized = kf.finalized;

    m_window.clear();
    m_window_first = kf.step;

    if (fill_window) {
        if (m_window.emplace_back() == nullptr)
            return make_error(simulation_errc::memory_error);
        m_window.back().assign(kf.data.begin(), kf.data.end());
    }

    while (m_step < n) {
        irt_check(do_step());

        if (fill_window) {
            if (m_window.emplace_back() == nullptr)
                return make_error(simulation_errc::memory_error);
            irt_check(snapshot_to(m_window.back()));
        }
    }

    return finalize_if_finished();
}

} // namespace irt
