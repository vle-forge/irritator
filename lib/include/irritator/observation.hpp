// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022
#define ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022

#include <irritator/core.hpp>

namespace irt {

//! How to use @c observation_message and interpolate functions.
enum class interpolate_type
{
    none,
    qss1,
    qss2,
    qss3,
};

constexpr inline auto get_interpolate_type(const dynamics_type type) noexcept
  -> interpolate_type
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_cross:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
        return interpolate_type::qss1;

    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
        return interpolate_type::qss2;

    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_cross:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
        return interpolate_type::qss3;

    case dynamics_type::integrator:
    case dynamics_type::quantifier:
    case dynamics_type::adder_2:
    case dynamics_type::adder_3:
    case dynamics_type::adder_4:
    case dynamics_type::mult_2:
    case dynamics_type::mult_3:
    case dynamics_type::mult_4:
        return interpolate_type::qss1;

    default:
        return interpolate_type::none;
    }

    irt_unreachable();
}

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
    irt_assert(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    while (head != tail) {
        *it++ = head->data[0];
        *it++ = head->data[1];

        obs.buffer.pop_front();
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
        *it++     = head->data[0];
        *it++     = head->data[1];
    }

    obs.clear();
}

template<int QssLevel, typename OutputIterator>
auto compute_interpolate(const observation_message& msg,
                         OutputIterator             it,
                         time                       until,
                         time                       time_step) noexcept -> void
{
    static_assert(1 <= QssLevel && QssLevel <= 3);

    *it++ = msg[0];
    *it++ = compute_value<QssLevel>(msg, 0);

    auto duration = until - msg[0] - time_step;
    if (duration > 0) {
        time elapsed = time_step;
        for (; elapsed < duration; elapsed += time_step) {
            *it++ = msg[0] + elapsed;
            *it++ = compute_value<QssLevel>(msg, elapsed);
        }

        if (duration < elapsed) {
            auto limit = duration - std::numeric_limits<real>::epsilon();
            *it++      = msg[0] + limit;
            *it++      = compute_value<QssLevel>(msg, limit);
        }
    }
}

template<typename OutputIterator>
auto write_interpolate_data(observer&      obs,
                            OutputIterator it,
                            real           time_step) noexcept -> void
{
    irt_assert(obs.buffer.ssize() >= 2);

    auto head = obs.buffer.head();
    auto tail = obs.buffer.tail();

    switch (get_interpolate_type(obs.type)) {
    case interpolate_type::none:
        while (head != tail) {
            *it++ = head->data[0];
            *it++ = head->data[1];
            obs.buffer.pop_front();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss1:
        while (head != tail) {
            auto next = head;
            ++next;
            compute_interpolate<1>(*head, it, next->data[0], time_step);
            obs.buffer.pop_front();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss2:
        while (head != tail) {
            auto next = head;
            ++next;
            compute_interpolate<2>(*head, it, next->data[0], time_step);
            obs.buffer.pop_front();
            head = obs.buffer.head();
        }
        break;
    case interpolate_type::qss3:
        while (head != tail) {
            auto next = head;
            ++next;
            compute_interpolate<3>(*head, it, next->data[0], time_step);
            obs.buffer.pop_front();
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

    obs.clear();
}

} // namespace irt

#endif
