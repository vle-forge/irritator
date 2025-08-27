// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022
#define ORG_VLEPROJECT_IRRITATOR_OBSERVATION_HPP_2022

#include <irritator/core.hpp>

namespace irt {

inline auto write_raw_data(ring_buffer<observation_message>& buf,
                           ring_buffer<observation>& lbuf) noexcept -> void
{
    debug::ensure(buf.ssize() >= 2);

    auto head = buf.head();
    auto tail = buf.tail();

    while (head != tail) {
        lbuf.force_emplace_tail(head->data()[0], head->data()[1]);
        buf.pop_head();
        head = buf.head();
    }
}

template<typename Fn>
inline auto write_raw_data(ring_buffer<observation_message>& buf,
                           Fn&& fn) noexcept -> void
{
    debug::ensure(buf.ssize() >= 2);

    auto head = buf.head();
    auto tail = buf.tail();

    while (head != tail) {
        fn(head->data()[0], head->data()[1]);
        buf.pop_head();
        head = buf.head();
    }
}

inline auto write_raw_data(observer& obs) noexcept -> void
{
    obs.buffer.read_write(
      [](auto& buf, observer& obs) {
          debug::ensure(buf.ssize() >= 2);

          obs.linearized_buffer.read_write(
            [](auto& lbuf, auto& buf) { write_raw_data(buf, lbuf); }, buf);
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs);
}

inline auto flush_raw_data(ring_buffer<observation_message>& buf,
                           ring_buffer<observation>& lbuf) noexcept -> void
{
    debug::ensure(buf.ssize() == 1);

    auto head = buf.head();
    lbuf.force_emplace_tail(head->data()[0], head->data()[1]);
}

template<typename Fn>
inline auto flush_raw_data(ring_buffer<observation_message>& buf,
                           Fn&& fn) noexcept -> void
{
    debug::ensure(buf.ssize() == 1);

    auto head = buf.head();
    fn(head->data()[0], head->data()[1]);
}

inline auto flush_raw_data(observer& obs) noexcept -> void
{
    obs.buffer.read_write(
      [](auto& buf, observer& obs) {
          obs.linearized_buffer.read_write(
            [](auto& lbuf, auto& buf) {
                if (buf.ssize() > 2)
                    write_raw_data(buf, lbuf);
                else if (buf.ssize() == 1)
                    flush_raw_data(buf, lbuf);
            },
            buf);

          buf.clear();
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs);
}

template<typename Fn>
inline auto flush_raw_data(observer& obs, Fn&& fn) noexcept -> void
{
    obs.buffer.read_write(
      [](auto& buf, auto& obs, auto& fn) {
          if (buf.ssize() >= 2)
              write_raw_data(buf, fn);
          else if (buf.ssize() == 1)
              flush_raw_data(buf, fn);

          buf.clear();
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs,
      fn);
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

template<int QssLevel>
auto compute_interpolate(const observation_message& msg,
                         ring_buffer<observation>&  out,
                         const time                 until,
                         const time                 time_step) noexcept -> void
{
    static_assert(1 <= QssLevel && QssLevel <= 3);

    out.force_emplace_tail(msg[0], compute_value<QssLevel>(msg, 0));

    const auto duration = until - msg[0] - time_step;

    if (duration > 100.0)
        debug::breakpoint();

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

template<int QssLevel, typename Fn>
auto compute_interpolate(const observation_message& msg,
                         const time                 until,
                         const time                 time_step,
                         Fn&&                       fn) noexcept -> void
{
    static_assert(1 <= QssLevel && QssLevel <= 3);

    fn(msg[0], compute_value<QssLevel>(msg, 0));

    const auto duration = until - msg[0] - time_step;
    if (duration > 0) {
        time elapsed = time_step;
        while (elapsed < duration) {
            fn(msg[0] + elapsed, compute_value<QssLevel>(msg, elapsed));
            elapsed += time_step;
        }

        if (duration < elapsed) {
            const auto limit = duration - std::numeric_limits<real>::epsilon();
            fn(msg[0] + limit, compute_value<QssLevel>(msg, limit));
        }
    }
}

inline auto write_interpolate_data(ring_buffer<observation_message>& buf,
                                   ring_buffer<observation>&         lbuf,
                                   const real                        time_step,
                                   const interpolate_type type) noexcept
{
    debug::ensure(buf.ssize() >= 2);

    auto head = buf.head();
    auto tail = buf.tail();

    observation_message data[2];

    switch (type) {
    case interpolate_type::none:
        while (head != tail) {
            lbuf.force_emplace_tail(
              observation(head->data()[0], head->data()[1]));
            buf.pop_head();
            head = buf.head();
        }
        break;

    case interpolate_type::qss1:
        data[0] = *buf.head();
        buf.pop_head();
        data[1] = *buf.head();
        buf.pop_head();

        for (;;) {
            compute_interpolate<1>(data[0], lbuf, data[1][0], time_step);

            if (buf.empty())
                break;
            data[0] = data[1];
            data[1] = *buf.head();
            buf.pop_head();
        }
        break;

    case interpolate_type::qss2:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<2>(*head, lbuf, next->data()[0], time_step);
            buf.pop_head();
            head = buf.head();
        }
        break;

    case interpolate_type::qss3:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<3>(*head, lbuf, next->data()[0], time_step);
            buf.pop_head();
            head = buf.head();
        }
        break;
    }
}

template<typename Fn>
inline auto write_interpolate_data(ring_buffer<observation_message>& buf,
                                   const real                        time_step,
                                   const interpolate_type            type,
                                   Fn&& fn) noexcept
{
    debug::ensure(buf.ssize() >= 2);

    auto head = buf.head();
    auto tail = buf.tail();

    switch (type) {
    case interpolate_type::none:
        while (head != tail) {
            fn(head->data()[0], head->data()[1]);
            buf.pop_head();
            head = buf.head();
        }
        break;

    case interpolate_type::qss1:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<1>(*head, next->data()[0], time_step, fn);
            buf.pop_head();
            head = buf.head();
        }
        break;

    case interpolate_type::qss2:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<2>(*head, next->data()[0], time_step, fn);
            buf.pop_head();
            head = buf.head();
        }
        break;

    case interpolate_type::qss3:
        while (head != tail) {
            auto next{ head };
            ++next;
            compute_interpolate<3>(*head, next->data()[0], time_step, fn);
            buf.pop_head();
            head = buf.head();
        }
        break;
    }
}

inline auto write_interpolate_data(observer& obs, const real time_step) noexcept
  -> void
{
    obs.buffer.read_write(
      [](auto& buf, auto& obs, const auto time_step) {
          if (buf.ssize() > 2)
              obs.linearized_buffer.read_write(
                [](auto&      lbuf,
                   auto&      buf,
                   const auto time_step,
                   const auto type) {
                    write_interpolate_data(buf, lbuf, time_step, type);
                },
                buf,
                time_step,
                obs.type);

          obs.states.set(observer_flags::buffer_full, false);
      },
      obs,
      time_step);
}

template<typename Fn>
inline auto write_interpolate_data(observer&  obs,
                                   const real time_step,
                                   Fn&&       fn) noexcept -> void
{
    obs.buffer.read_write(
      [](auto& buf, auto& obs, const auto time_step, auto& fn) {
          if (buf.ssize() > 2)
              write_interpolate_data(buf, time_step, obs.type, fn);
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs,
      time_step,
      fn);
}

inline auto flush_interpolate_data(observer& obs, const real time_step) noexcept
  -> void
{
    obs.buffer.read_write(
      [](auto& buf, auto& obs, const auto time_step) {
          obs.linearized_buffer.read_write(
            [](auto& lbuf, auto& buf, const auto time_step, const auto type) {
                if (buf.ssize() >= 2)
                    write_interpolate_data(buf, lbuf, time_step, type);

                if (buf.ssize() == 1)
                    flush_raw_data(buf, lbuf);
            },
            buf,
            time_step,
            obs.type);

          buf.clear();
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs,
      time_step);
}

template<typename Fn>
inline auto flush_interpolate_data(observer&  obs,
                                   const real time_step,
                                   Fn&&       fn) noexcept
{
    obs.buffer.read_write(
      [](auto& buf, auto& obs, const auto time_step, auto& fn) {
          if (buf.ssize() >= 2)
              write_interpolate_data(buf, time_step, obs.type, fn);

          if (buf.ssize() == 1)
              flush_raw_data(buf, fn);

          buf.clear();
          obs.states.set(observer_flags::buffer_full, false);
      },
      obs,
      time_step,
      fn);
}

} // namespace irt

#endif
