// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022
#define ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022

#include <irritator/core.hpp>

namespace irt {

template<int QssLevel>
constexpr auto compute_value(const observation_message& msg,
                             const time elapsed) noexcept -> real
{
    if constexpr (QssLevel == 1)
        return msg[1] + msg[2] * elapsed;

    if constexpr (QssLevel == 2)
        return msg[1] + (msg[2] * elapsed) + (msg[3] * elapsed * elapsed / two);

    if constexpr (QssLevel == 3)
        return msg[1] + (msg[2] * elapsed) +
               (msg[3] * elapsed * elapsed / two) +
               (msg[4] * elapsed * elapsed * elapsed / three);

    return msg[1];
}

constexpr auto compute_interpolate_size(const time t,
                                        const time until,
                                        const time time_step) noexcept -> int
{
    return 1 + static_cast<int>(((until - t) / time_step));
}

template<typename OutputIterator>
auto write_raw_data(observer& obs, OutputIterator it) noexcept -> void
{
    debug::ensure(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    while (head != tail) {
        *it++ = { head->data()[0], head->data()[1] };

        obs.buffer.pop_head();
        head = obs.buffer.head();
    }
}

template<typename OutputIterator>
auto flush_raw_data(observer& obs, OutputIterator it) noexcept -> void
{
    while (obs.buffer.ssize() >= 2)
        write_raw_data(obs, it);

    if (!obs.buffer.empty()) {
        auto head = obs.buffer.head();
        *it++     = { head->data()[0], head->data()[1] };
    }

    obs.clear();
}

template<int QssLevel, typename OutputIterator>
auto compute_interpolate(const observation_message& msg,
                         OutputIterator             it,
                         const time                 until,
                         const time                 time_step) noexcept -> void
{
    static_assert(1 <= QssLevel && QssLevel <= 3);

    *it++ = { msg[0], compute_value<QssLevel>(msg, 0) };

    auto duration = until - msg[0] - time_step;
    if (duration > 0) {
        time elapsed = time_step;
        for (; elapsed < duration; elapsed += time_step) {
            *it++ = { msg[0] + elapsed, compute_value<QssLevel>(msg, elapsed) };
        }

        if (duration < elapsed) {
            auto limit = duration - std::numeric_limits<real>::epsilon();
            *it++ = { msg[0] + limit, compute_value<QssLevel>(msg, limit) };
        }
    }
}

inline auto write_raw_data(observer& obs) noexcept -> void
{
    debug::ensure(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    while (head != tail) {
        obs.linearized_buffer.force_emplace_tail(head->data()[0],
                                                 head->data()[1]);
        obs.buffer.pop_head();
        head = obs.buffer.head();
    }
}

inline auto flush_raw_data(observer& obs) noexcept -> void
{
    while (obs.buffer.ssize() >= 2)
        write_raw_data(obs);

    if (!obs.buffer.empty()) {
        auto head = obs.buffer.head();
        obs.linearized_buffer.force_emplace_tail(head->data()[0],
                                                 head->data()[1]);
    }
}

template<int QssLevel>
auto compute_interpolate(const observation_message& msg,
                         ring_buffer<observation>&  out,
                         const time                 until,
                         const time                 time_step) noexcept -> void
{
    static_assert(1 <= QssLevel && QssLevel <= 3);

    out.force_emplace_tail(msg[0], compute_value<QssLevel>(msg, 0));

    const auto duration = until - msg[0] - time_step;
    if (duration > 0) {
        time elapsed = time_step;
        while (elapsed < duration) {
            out.force_emplace_tail(msg[0] + elapsed,
                                   compute_value<QssLevel>(msg, elapsed));
            elapsed += time_step;
        }

        if (duration < elapsed) {
            const auto limit = duration - std::numeric_limits<real>::epsilon();
            out.force_emplace_tail(msg[0] + limit,
                                   compute_value<QssLevel>(msg, limit));
        }
    }
}

template<typename OutputIterator>
auto write_interpolate_data(observer&      obs,
                            OutputIterator it,
                            real           time_step) noexcept -> void
{
    debug::ensure(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    switch (obs.type) {
    case interpolate_type::none:
        while (head != tail) {
            *it++ = observation(head->data()[0], head->data()[1]);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss1:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<1>(*head, it, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss2:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<2>(*head, it, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss3:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<3>(*head, it, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    }
}

inline auto write_interpolate_data(observer& obs, real time_step) noexcept
  -> void
{
    debug::ensure(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    switch (obs.type) {
    case interpolate_type::none:
        while (head != tail) {
            obs.linearized_buffer.force_emplace_tail(
              observation(head->data()[0], head->data()[1]));
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss1:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<1>(
              *head, obs.linearized_buffer, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss2:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<2>(
              *head, obs.linearized_buffer, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss3:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<3>(
              *head, obs.linearized_buffer, next->data()[0], time_step);
            obs.buffer.pop_head();
            head = obs.buffer.head();
        }
        break;
    }
}

template<typename OutputIterator>
constexpr auto flush_interpolate_data(observer&      obs,
                                      OutputIterator it,
                                      real           time_step) noexcept -> void
{
    while (obs.buffer.ssize() >= 2)
        write_interpolate_data(obs, it, time_step);

    if (!obs.buffer.empty())
        flush_raw_data(obs, it);

    obs.buffer.clear();
}

inline constexpr auto flush_interpolate_data(observer& obs,
                                             real time_step) noexcept -> void
{
    while (obs.buffer.ssize() >= 2)
        write_interpolate_data(obs, time_step);

    if (!obs.buffer.empty())
        flush_raw_data(obs);

    obs.buffer.clear();
}

} // namespace irt

#endif
