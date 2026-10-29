// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "irritator/macros.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>

namespace irt {

static void show_discrete_plot_line(
  const plot_type_options                 options,
  const char*                             name,
  const std::span<const resampled_sample> lbuf) noexcept
{
    ImPlotSpec spec;
    spec.LineWeight = 1.f;
    spec.MarkerSize = 6.f;
    spec.Stride     = sizeof(resampled_sample);

    switch (options) {
    case plot_type_options::line:
        ImPlot::PlotStairs(name, &lbuf[0].t, &lbuf[0].value,
                           static_cast<int>(lbuf.size()), spec);
        break;

    case plot_type_options::dash:
        ImPlot::PlotBars(name, &lbuf[0].t, &lbuf[0].value,
                         static_cast<int>(lbuf.size()), 1.5, spec);
        break;

    default:
        break;
    }
}

static void show_continuous_plot_line(
  const plot_type_options                 options,
  const char*                             name,
  const std::span<const resampled_sample> lbuf) noexcept
{
    ImPlotSpec spec;
    spec.LineWeight = 1.f;
    spec.MarkerSize = 6.f;
    spec.Stride     = sizeof(resampled_sample);

    switch (options) {
    case plot_type_options::line:
        ImPlot::PlotLine(name, &lbuf[0].t, &lbuf[0].value,
                         static_cast<int>(lbuf.size()), spec);
        break;

    case plot_type_options::dash:
        ImPlot::PlotScatter(name, &lbuf[0].t, &lbuf[0].value,
                            static_cast<int>(lbuf.size()), spec);
        break;

    default:
        break;
    }
}

void plot_observation_widget::show_plot_line(
  const std::span<const resampled_sample> points,
  const plot_type_options                 options,
  const char*                             name) noexcept
{
    if (points.empty())
        return;

    ImGui::PushID(points.data());

    if (options == plot_type_options::dash)
        show_discrete_plot_line(options, name, points);
    else
        show_continuous_plot_line(options, name, points);

    ImGui::PopID();
}

status plot_series::consume(const observation_batch& batch) noexcept
{
    const auto ret = m_next.consume(batch);

    if (batch.points.empty())
        return ret;

    const auto n = static_cast<int>(batch.points.size());

    std::lock_guard lock(m_mutex);

    if ((not m_staged.can_alloc(1) and not m_staged.grow<2, 1>(1)) or
        (not m_staged_points.can_alloc(n) and
         not m_staged_points.grow<2, 1>(n)))
        return make_error(simulation_errc::memory_error);

    m_staged.emplace_back(
      message{ .id    = batch.id,
               .index = batch.sampler.output_index(),
               .first = static_cast<u32>(m_staged_points.size()),
               .count = static_cast<u32>(n) });

    m_staged_points.insert(m_staged_points.end(), batch.points.begin(),
                           batch.points.end());

    return ret;
}

void plot_series::rewind(const simulation& sim) noexcept
{
    m_next.rewind(sim);

    const auto n = static_cast<int>(sim.observers.size());

    std::lock_guard lock(m_mutex);

    if (not m_staged.can_alloc(n) and not m_staged.grow<2, 1>(n))
        return;

    for (const auto& obs : sim.observers) {
        const auto id = sim.observers.get_id(obs);

        m_staged.emplace_back(
          message{ .id    = id,
                   .index = sim.observers.get<resampler>(id).point_count(),
                   .first = 0,
                   .count = 0 });
    }
}

void plot_series::receive() noexcept
{
    {
        std::lock_guard lock(m_mutex);
        std::swap(m_staged, m_received);
        std::swap(m_staged_points, m_received_points);
    }

    for (const auto& msg : m_received) {
        const auto idx = static_cast<std::size_t>(get_index(msg.id));

        if (idx >= m_curves.size()) {
            const auto n = static_cast<int>(idx + 1u - m_curves.size());

            if (not m_curves.can_alloc(n) and not m_curves.grow<2, 1>(n))
                continue;

            while (m_curves.size() <= idx)
                m_curves.emplace_back();
        }

        auto& c = m_curves[idx];
        if (c.id != msg.id) {
            c.id = msg.id;
            c.points.clear();
        }

        if (std::cmp_less(msg.index, c.points.size()))
            c.points.resize(static_cast<int>(msg.index));

        const auto n = static_cast<int>(msg.count);
        if (n > 0 and (c.points.can_alloc(n) or c.points.grow<2, 1>(n))) {
            const auto first = m_received_points.begin() + msg.first;
            c.points.insert(c.points.end(), first, first + n);
        }
    }

    m_received.clear();
    m_received_points.clear();
}

std::span<const resampled_sample> plot_series::get(
  const observer_id id) const noexcept
{
    const auto idx = static_cast<std::size_t>(get_index(id));

    if (idx < m_curves.size() and m_curves[idx].id == id)
        return std::span(m_curves[idx].points.data(),
                         m_curves[idx].points.size());

    return {};
}

} // irt
