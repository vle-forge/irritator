// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

observer::observer(const model_id mdl) noexcept
  : m_model(mdl)
{}

buffer_status observer::observe(const raw_sample& s) noexcept
{
    debug::ensure(s.t >= m_last_t and
                  "observe() called with a date earlier than a previous "
                  "observation -- violates DEVS causality");
    m_last_t = s.t;

    if (not m_raw.push(s))
        return buffer_status::overflow;

    return raw_fill_status();
}

void observer::reset() noexcept
{
    m_raw.clear();
    m_last_t = -std::numeric_limits<real>::infinity();
    m_history.reset();
}

resampler::resampler() noexcept
  : m_dt(1)
  , m_interp(interpolate_type::none)
{}

resampler::resampler(const real dt, const interpolate_type order) noexcept
  : m_dt(dt)
  , m_interp(order)
{}

void resampler::tick(observer& obs, const real now) noexcept
{
    raw_sample s;
    while (obs.raw_buffer().pop(s))
        ingest(s);

    if (m_has_pending && now > m_pending.t) {
        commit_pending();
        m_has_pending = false;
    }

    if (std::isinf(now))
        finalize();

    flush_batch(obs);
}

void resampler::ingest(const raw_sample& s) noexcept
{
    if (m_has_pending && s.t == m_pending.t) {
        m_pending = s;
        return;
    }

    if (m_has_pending)
        commit_pending();

    m_pending     = s;
    m_has_pending = true;
}

void resampler::commit_pending() noexcept
{
    if (!m_has_prev) {
        m_prev             = m_pending;
        m_has_prev         = true;
        m_next_sample_time = m_pending.t;
        push_resampled(m_pending.t, m_pending.value);
        m_next_sample_time += m_dt;
        return;
    }

    while (m_next_sample_time <= m_pending.t) {
        double v = m_interp.evaluate(m_prev, m_pending, m_next_sample_time);
        push_resampled(m_next_sample_time, v);
        m_next_sample_time += m_dt;
    }

    m_prev = m_pending;
}

void resampler::finalize() noexcept
{
    if (m_has_pending) {
        commit_pending();
        m_has_pending = false;
    }

    if (!m_has_prev || std::isinf(m_next_sample_time))
        return;

    while (m_next_sample_time <= m_prev.t) {
        double v = m_interp.extrapolate(m_prev, m_next_sample_time);
        push_resampled(m_next_sample_time, v);
        m_next_sample_time += m_dt;
    }

    push_resampled(m_prev.t, m_prev.value);
    m_next_sample_time = std::numeric_limits<double>::infinity();
}

// Dedup: identical value since last publish -> skip (reader fills the
// gap). Accumulates into a local batch, NOT written directly -- see
// flush_batch(), called once per tick() rather than once per point.
void resampler::push_resampled(double t, double value) noexcept
{
    if (m_has_last_pushed && value == m_last_pushed_value)
        return;

    m_has_last_pushed   = true;
    m_last_pushed_value = value;
    m_batch.push_back(resampled_sample{ t, value });
}

void resampler::flush_batch(observer& obs) noexcept
{
    if (m_batch.empty())
        return;

    obs.write_history(
      [&](auto& history) {
          const auto n = m_batch.size();

          if (not history.can_alloc(n) and not history.template grow<2, 1>(n)) {
              log(log_level::debug, [&](auto& t, auto& m) {
                  t = "Simulation observation";
                  m = "fail to allocate more observer history";
              });
              return;
          }

          history.insert(history.end(), m_batch.begin(), m_batch.end());
      },
      observer::write_key{});

    m_batch.clear();
}

}