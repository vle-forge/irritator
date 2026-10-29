// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

observer::observer(const model_id mdl) noexcept
  : m_model(mdl)
{}

observer::observer(const observer& other) noexcept
  : m_model(other.m_model)
  , m_last_t(other.m_last_t)
  , m_last_value(other.last_value())
  , m_raw(other.m_raw)
{}

buffer_status observer::observe(const raw_sample& s) noexcept
{
    debug::ensure(s.t >= m_last_t and
                  "observe() called with a date earlier than a previous "
                  "observation -- violates DEVS causality");
    m_last_t = s.t;
    m_last_value.store(s.value, std::memory_order_relaxed);

    if (not m_raw.push(s))
        return buffer_status::overflow;

    return raw_fill_status();
}

void observer::reset() noexcept
{
    m_raw.clear();
    m_last_t = -std::numeric_limits<real>::infinity();
    m_last_value.store(zero, std::memory_order_relaxed);
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

    advance(now);
}

void resampler::tick(std::span<const raw_sample> samples,
                     const real                  now) noexcept
{
    for (const auto& s : samples)
        ingest(s);

    advance(now);
}

void resampler::advance(const real now) noexcept
{
    if (m_has_pending && now > m_pending.t) {
        commit_pending();
        m_has_pending = false;
    }

    if (std::isinf(now))
        finalize();
}

void resampler::reset() noexcept
{
    m_batch.clear();
    m_raw_batch.clear();
    m_point_count       = 0;
    m_raw_count         = 0;
    m_next_sample_time  = 0.0;
    m_last_pushed_value = 0.0;
    m_pending           = raw_sample{};
    m_prev              = raw_sample{};
    m_has_pending       = false;
    m_has_prev          = false;
    m_has_last_pushed   = false;
}

void resampler::clear_outputs() noexcept
{
    m_batch.clear();
    m_raw_batch.clear();
}

void resampler::ingest(const raw_sample& s) noexcept
{
    m_raw_batch.push_back(s);
    ++m_raw_count;

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
// gap).
void resampler::push_resampled(double t, double value) noexcept
{
    if (m_has_last_pushed && value == m_last_pushed_value)
        return;

    m_has_last_pushed   = true;
    m_last_pushed_value = value;
    m_batch.push_back(resampled_sample{ t, value });
    ++m_point_count;
}

}