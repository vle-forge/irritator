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
{}

status simulation_player::start() noexcept
{
    m_started = false;
    m_keyframes.clear();
    m_window.clear();
    m_step     = 0;
    m_furthest = 0;

    irt_check(m_sim.initialize());
    irt_check(push_keyframe());

    m_started = true;
    return success();
}

status simulation_player::step_forward() noexcept
{
    if (not m_started or finished())
        return success();

    irt_check(do_step());

    return keyframe_if_needed();
}

status simulation_player::step_back() noexcept
{
    if (not m_started or m_step == 0)
        return success();

    return rewind_to(m_step - 1, true);
}

status simulation_player::go_to(const step_type n) noexcept
{
    if (not m_started)
        return success();

    if (n < m_step)
        irt_check(rewind_to(n, false));

    while (m_step < n and not finished())
        irt_check(step_forward());

    return success();
}

status simulation_player::advance(
  const std::chrono::nanoseconds budget,
  const step_type max_steps) noexcept
{
        const auto end = std::chrono::steady_clock::now() + budget;

        for (step_type i = 0; i < max_steps; ++i) {
            if (not m_started or finished())
                break;

            irt_check(step_forward());

            if (std::chrono::steady_clock::now() >= end)
                break;
        }

        return success();
}

status simulation_player::retreat(
  const std::chrono::nanoseconds budget,
  const step_type max_steps) noexcept
{
        const auto end = std::chrono::steady_clock::now() + budget;

        for (step_type i = 0; i < max_steps; ++i) {
            if (not m_started or m_step == 0)
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

    if (not m_keyframes.empty() and m_keyframes.back().step == m_step)
        return snapshot_to(m_keyframes.back().data);

    return push_keyframe();
}

bool simulation_player::finished() const noexcept
{
    return not m_started or
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

status simulation_player::do_step() noexcept
{
    irt_check(m_sim.run());
    m_sim.tick_resamplers();

    ++m_step;
    m_furthest = std::max(m_furthest, m_step);

    return success();
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

    return read_snapshot(r, m_sim);
}

status simulation_player::push_keyframe() noexcept
{
    if (m_keyframes.emplace_back() == nullptr)
        return make_error(simulation_errc::memory_error);

    m_keyframes.back().step = m_step;

    if (auto ret = snapshot_to(m_keyframes.back().data); ret.has_error()) {
        m_keyframes.pop_back();
        return ret;
    }

    return success();
}

status simulation_player::keyframe_if_needed() noexcept
{
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
        m_step = n;
        return success();
    }

    auto k = static_cast<std::size_t>(m_keyframes.size());
    while (k > 1 and m_keyframes[k - 1].step > n)
        --k;

    const auto& kf = m_keyframes[k - 1];
    irt_check(restore_from(kf.data));
    m_step = kf.step;

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

    return success();
}

} // namespace irt