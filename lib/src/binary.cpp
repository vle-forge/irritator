// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/binary-simulation.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>

#include <algorithm>
#include <bitset>
#include <cstring>
#include <memory>
#include <type_traits>

/// Binary serialization of the simulation, see @c binary-simulation.hpp for
/// the format and its limits.
///
/// How the dynamics are described. Every dynamics has a @c binary_serialize
/// function that lists its members field by field (see the generated block
/// below). The line
///
/// @code
/// auto& [x, y, value, sigma] = d;
/// @endcode
///
/// is a compile-time check: a structured binding fails to compile if the
/// number of members of the dynamics changes. The members of a dynamics are
/// public, so a new member can not be forgotten without the compiler telling
/// it. A member that changes its type (an enumeration, a source) is handled
/// by the type dispatch of the archiver. The generated block can be rebuilt
/// with the script @c gen_dyn.py.
namespace irt {

/* * * * * * * * * *
 *
 * Which types are dumped as a block of bytes
 *
 * * * * * * * * * */

// Trivial structures without padding and with floating point members: the
// author guarantees the layout (see binary_raw_layout).
template<>
inline constexpr bool binary_raw_layout<raw_sample> = true;
template<>
inline constexpr bool binary_raw_layout<resampled_sample> = true;
template<>
inline constexpr bool binary_raw_layout<parameter> = true;

// A message and a dated message are arrays of doubles.
template<>
inline constexpr bool binary_raw_layout<message> = true;
template<>
inline constexpr bool binary_raw_layout<dated_message> = true;

static_assert(sizeof(message) == 3 * sizeof(real));
static_assert(sizeof(dated_message) == 4 * sizeof(real));
static_assert(binary_raw<message> and binary_raw<dated_message>);
static_assert(sizeof(raw_sample) == 4 * sizeof(real));
static_assert(sizeof(resampled_sample) == 2 * sizeof(real));
static_assert(sizeof(parameter) ==
              4 * sizeof(real) + 4 * sizeof(i64));
static_assert(binary_raw<raw_sample> and binary_raw<resampled_sample> and
              binary_raw<parameter>);

// input_port is three integers without padding: a block of bytes.
static_assert(binary_raw<input_port>);

// A dynamics, a model, a path and the structures with a function below are
// never dumped as a block of bytes, even if their layout would allow it.
template<dynamics D>
inline constexpr bool binary_no_raw<D> = true;
template<>
inline constexpr bool binary_no_raw<model> = true;
template<>
inline constexpr bool binary_no_raw<path> = true;
template<>
inline constexpr bool binary_no_raw<node> = true;
template<>
inline constexpr bool binary_no_raw<block_node> = true;
template<>
inline constexpr bool binary_no_raw<output_port> = true;
template<>
inline constexpr bool binary_no_raw<source> = true;
template<>
inline constexpr bool binary_no_raw<source_data> = true;
template<>
inline constexpr bool binary_no_raw<hierarchical_state_machine> = true;
template<>
inline constexpr bool
  binary_no_raw<hierarchical_state_machine::state_action> = true;
template<>
inline constexpr bool
  binary_no_raw<hierarchical_state_machine::condition_action> = true;
template<>
inline constexpr bool binary_no_raw<hierarchical_state_machine::state> = true;
template<>
inline constexpr bool
  binary_no_raw<hierarchical_state_machine::execution> = true;
template<>
inline constexpr bool binary_no_raw<observer> = true;
template<>
inline constexpr bool binary_no_raw<resampler> = true;
template<>
inline constexpr bool binary_no_raw<constant_source> = true;
template<>
inline constexpr bool binary_no_raw<binary_file_source> = true;
template<>
inline constexpr bool binary_no_raw<text_file_source> = true;
template<>
inline constexpr bool binary_no_raw<random_source> = true;
template<typename T, typename A>
inline constexpr bool binary_no_raw<ring_buffer<T, A>> = true;

/// The reader builds an observer before it fills it. It has no default
/// constructor: the model is read afterward.
template<>
struct binary_construct<observer> {
    static observer* make(observer* p) noexcept
    {
        return std::construct_at(p, undefined<model_id>());
    }
};

/* * * * * * * * * *
 *
 * Helpers
 *
 * * * * * * * * * */

/// An enumeration with a limit: a value after @c last is an error.
template<typename Ar, typename E>
void io_enum(Ar& ar, E& value, const E last) noexcept
{
    ar(value);

    if constexpr (Ar::is_reader) {
        using underlying = std::underlying_type_t<E>;

        if (static_cast<underlying>(value) > static_cast<underlying>(last)) {
            ar.fail();
            value = E{};
        }
    }
}

/// A 32 bits union (the constant of an action or of a condition of a state
/// machine): its bytes, as an u32.
template<typename Ar, typename Union>
void io_union32(Ar& ar, Union& u) noexcept
{
    static_assert(sizeof(Union) == sizeof(u32));

    u32 bits = 0;

    if constexpr (Ar::is_writer)
        std::memcpy(&bits, &u, sizeof(bits));

    ar(bits);

    if constexpr (Ar::is_reader)
        std::memcpy(static_cast<void*>(&u), &bits, sizeof(bits));
}

/// Binds the buffer of a source to the buffer it views, the one of the
/// external source or the one of the dynamics (the random source views
/// @c source_data::chunk_real). The reader gets the external source with
/// its context.
inline bool bind_source(const external_source* srcs,
                        source&                src,
                        source_data&           data) noexcept
{
    if (srcs == nullptr)
        return false;

    switch (src.type) {
    case source_type::constant: {
        const auto* s = srcs->constant_sources.try_to_get(src.id.constant_id);
        if (s == nullptr or s->length > s->buffer.size())
            return false;

        src.buffer = std::span<double>(
          const_cast<double*>(s->buffer.data()), s->length);
        return true;
    }

    case source_type::binary_file: {
        const auto* s = srcs->binary_file_sources.try_to_get(
          src.id.binary_file_id);
        if (s == nullptr or data.chunk_id[0] >= s->buffers.size())
            return false;

        auto& chunk = const_cast<chunk_type&>(s->buffers[data.chunk_id[0]]);
        src.buffer  = std::span<double>(chunk.data(), chunk.size());
        return true;
    }

    case source_type::text_file: {
        const auto* s = srcs->text_file_sources.try_to_get(
          src.id.text_file_id);
        if (s == nullptr)
            return false;

        auto& chunk = const_cast<chunk_type&>(s->buffer);
        src.buffer  = std::span<double>(chunk.data(), chunk.size());
        return true;
    }

    case source_type::random: {
        if (srcs->random_sources.try_to_get(src.id.random_id) == nullptr)
            return false;

        src.buffer = std::span<double>(data.chunk_real);
        return true;
    }
    }

    return false;
}

/// A source: the type, the identifier, the position and if the source views a
/// buffer. The buffer itself is never written: the reader binds it again
/// (see @c bind_source). @c data is the @c source_data of the dynamics, read
/// before.
template<typename Ar>
void io_source(Ar& ar, source& src, source_data& data) noexcept
{
    if constexpr (Ar::is_writer) {
        u32 id = 0;

        switch (src.type) {
        case source_type::constant:
            id = static_cast<u32>(src.id.constant_id);
            break;
        case source_type::binary_file:
            id = static_cast<u32>(src.id.binary_file_id);
            break;
        case source_type::text_file:
            id = static_cast<u32>(src.id.text_file_id);
            break;
        case source_type::random:
            id = static_cast<u32>(src.id.random_id);
            break;
        }

        (void)data;
        ar(src.type, id, src.index, not src.buffer.empty());
    } else {
        source_type type  = source_type::constant;
        u32         id    = 0;
        u16         index = 0;
        bool        bound = false;

        io_enum(ar, type, source_type::random);
        ar(id, index, bound);

        src.buffer = std::span<double>{};
        src.type   = type;
        src.index  = index;

        switch (type) {
        case source_type::constant:
            src.id = static_cast<constant_source_id>(id);
            break;
        case source_type::binary_file:
            src.id = static_cast<binary_file_source_id>(id);
            break;
        case source_type::text_file:
            src.id = static_cast<text_file_source_id>(id);
            break;
        case source_type::random:
            src.id = static_cast<random_source_id>(id);
            break;
        }

        if (not ar.ok())
            return;

        if (bound) {
            if (not bind_source(ar.template context<external_source>(), src,
                                data) or
                src.index > src.buffer.size())
                ar.fail();
        }
    }
}

template<typename Ar>
void binary_serialize(Ar& ar, source_data& d) noexcept
{
    ar(d.chunk_id, d.chunk_real);
}

/// The sources of a dynamics, with their @c source_data.
template<typename Dynamics, typename Fn>
void for_each_source(Dynamics& dyn, Fn&& fn) noexcept
{
    if constexpr (requires { dyn.source_ta; })
        fn(dyn.source_ta, dyn.src_data);

    if constexpr (requires { dyn.source_value; })
        fn(dyn.source_value, dyn.src_data);

    if constexpr (std::is_same_v<std::remove_const_t<Dynamics>, hsm_wrapper>)
        fn(dyn.exec.source_value, dyn.exec.src_data);
}

/// The path of a file: a string.
template<typename Ar>
void binary_serialize(Ar& ar, path& p) noexcept
{
    binary_serialize(ar, static_cast<small_string<1024>&>(p));
}

/* * * * * * * * * *
 *
 * Connections
 *
 * * * * * * * * * */

template<typename Ar>
void binary_serialize(Ar& ar, node& n) noexcept
{
    ar(n.model, n.port_index);

    if constexpr (Ar::is_reader)
        if (n.port_index < 0)
            ar.fail();
}

template<typename Ar>
void binary_serialize(Ar& ar, block_node& b) noexcept
{
    ar(b.nodes, b.next);
}

template<typename Ar>
void binary_serialize(Ar& ar, output_port& p) noexcept
{
    ar(p.msg, p.connections, p.next);
}

/// A ring buffer: the capacity, the number of elements and the elements, from
/// the oldest to the newest. The position of the head in the buffer is not
/// kept, it has no meaning.
template<typename Ar, typename T, typename A>
void binary_serialize(Ar& ar, ring_buffer<T, A>& r) noexcept
{
    if constexpr (Ar::is_writer) {
        ar.write(static_cast<u32>(r.capacity()));
        ar.write(static_cast<u32>(r.size()));

        for (auto it = r.begin(), e = r.end(); it != e; ++it)
            ar(*it);
    } else {
        u32 capacity = 0;
        u32 size     = 0;

        ar(capacity, size);

        r.clear();

        // The ring always keeps one slot free, and an element needs at least
        // one byte.
        const bool valid =
          ar.ok() and capacity <= static_cast<u32>(std::numeric_limits<int>::max()) and
          size <= capacity and (size == 0 or size < capacity) and
          size <= ar.remaining();

        if (not valid) {
            ar.fail();
            return;
        }

        if (std::cmp_greater(capacity, r.capacity())) {
            if (not ar.can_allocate(static_cast<std::size_t>(capacity) *
                                    sizeof(T)) or
                not r.reserve(static_cast<int>(capacity))) {
                ar.fail();
                return;
            }
        }

        for (u32 i = 0; i != size and ar.ok(); ++i) {
            T value{};
            ar(value);

            if (ar.ok() and not r.push_tail(value))
                ar.fail();
        }

        if (not ar.ok())
            r.clear();
    }
}

/* * * * * * * * * *
 *
 * Hierarchical state machine
 *
 * * * * * * * * * */

namespace details {

using hsm_type = hierarchical_state_machine;

inline bool is_valid_state_id(const hsm_type::state_id id) noexcept
{
    return id == hsm_type::invalid_state_id or
           id < hsm_type::max_number_of_state;
}

inline bool is_default_action(const hsm_type::state_action& a) noexcept
{
    u32 bits = 0;
    std::memcpy(&bits, &a.constant, sizeof(bits));

    return a.var1 == hsm_type::variable::none and
           a.var2 == hsm_type::variable::none and
           a.type == hsm_type::action_type::none and bits == 0;
}

inline bool is_default_condition(const hsm_type::condition_action& c) noexcept
{
    u32 bits = 0;
    std::memcpy(&bits, &c.constant, sizeof(bits));

    return c.var1 == hsm_type::variable::none and
           c.var2 == hsm_type::variable::none and
           c.type == hsm_type::condition_type::none and bits == 0;
}

/// A state that the reader rebuilds with its default constructor.
inline bool is_default_state(const hsm_type::state& s) noexcept
{
    return is_default_action(s.enter_action) and
           is_default_action(s.exit_action) and
           is_default_action(s.if_action) and
           is_default_action(s.else_action) and
           is_default_condition(s.condition) and
           s.if_transition == hsm_type::invalid_state_id and
           s.else_transition == hsm_type::invalid_state_id and
           s.super_id == hsm_type::invalid_state_id and
           s.sub_id == hsm_type::invalid_state_id;
}

} // namespace details

template<typename Ar>
void binary_serialize(Ar& ar, hierarchical_state_machine::state_action& a) noexcept
{
    using hsm = hierarchical_state_machine;

    io_enum(ar, a.var1, hsm::variable::source);
    io_enum(ar, a.var2, hsm::variable::source);
    io_enum(ar, a.type, hsm::action_type::bit_xor);
    io_union32(ar, a.constant);
}

template<typename Ar>
void binary_serialize(Ar& ar,
                      hierarchical_state_machine::condition_action& c) noexcept
{
    using hsm = hierarchical_state_machine;

    io_enum(ar, c.var1, hsm::variable::source);
    io_enum(ar, c.var2, hsm::variable::source);
    io_enum(ar, c.type, hsm::condition_type::less_equal);
    io_union32(ar, c.constant);
}

template<typename Ar>
void binary_serialize(Ar& ar, hierarchical_state_machine::state& s) noexcept
{
    ar(s.enter_action, s.exit_action, s.if_action, s.else_action, s.condition,
       s.if_transition, s.else_transition, s.super_id, s.sub_id);

    if constexpr (Ar::is_reader) {
        if (not details::is_valid_state_id(s.if_transition) or
            not details::is_valid_state_id(s.else_transition) or
            not details::is_valid_state_id(s.super_id) or
            not details::is_valid_state_id(s.sub_id))
            ar.fail();
    }
}

/// The state machine: the constants, the top state, the flags, then the
/// number of states that are not the default state and, for each, its index
/// (increasing) and the state. The 254 states are not written.
template<typename Ar>
void binary_serialize(Ar& ar, hierarchical_state_machine& h) noexcept
{
    using hsm = hierarchical_state_machine;

    ar(h.constants, h.parent_id, h.top_state, h.flags);

    if constexpr (Ar::is_writer) {
        u32 count = 0;
        for (const auto& s : h.states)
            if (not details::is_default_state(s))
                ++count;

        ar.write(count);

        for (u32 i = 0; i != h.states.size(); ++i) {
            if (not details::is_default_state(h.states[i])) {
                ar.write(static_cast<u8>(i));
                ar(h.states[i]);
            }
        }
    } else {
        u32 count = 0;
        ar(count);

        if (not ar.ok() or count > h.states.size() or
            not details::is_valid_state_id(h.top_state)) {
            ar.fail();
            return;
        }

        int previous = -1;
        for (u32 i = 0; i != count and ar.ok(); ++i) {
            u8 index = 0;
            ar(index);

            if (index >= hsm::max_number_of_state or
                static_cast<int>(index) <= previous) {
                ar.fail();
                return;
            }

            previous = index;
            ar(h.states[index]);
        }
    }
}

template<typename Ar>
void binary_serialize(Ar& ar, hierarchical_state_machine::execution& e) noexcept
{
    using hsm = hierarchical_state_machine;

    ar(e.i1, e.i2, e.r1, e.r2, e.timer, e.ports, e.message_values,
       e.message_ports);

    i32 messages = e.messages;
    ar(messages);

    ar(e.src_data);
    io_source(ar, e.source_value, e.src_data);

    u8 values = 0;
    if constexpr (Ar::is_writer)
        values = static_cast<u8>(e.values.to_ulong());

    ar(values);
    ar(e.current_state, e.next_state, e.source_state, e.current_source_state,
       e.previous_state, e.disallow_transition);

    if constexpr (Ar::is_reader) {
        bool valid = ar.ok() and messages >= 0 and messages <= 4 and
                     values <= 0x0f;

        for (int i = 0; valid and i != messages; ++i)
            valid = e.message_ports[static_cast<std::size_t>(i)] < 4;

        valid = valid and details::is_valid_state_id(e.current_state) and
                details::is_valid_state_id(e.next_state) and
                details::is_valid_state_id(e.source_state) and
                details::is_valid_state_id(e.current_source_state) and
                details::is_valid_state_id(e.previous_state);

        if (not valid) {
            ar.fail();
            return;
        }

        e.messages = messages;
        e.values   = std::bitset<4>(values);
    }

    (void)sizeof(hsm);
}

template<typename Ar>
void binary_serialize(Ar& ar, hsm_wrapper& d) noexcept
{
    ar(d.x, d.y, d.exec, d.sigma, d.id);
}

/* * * * * * * * * *
 *
 * Dynamics, generated by gen_dyn.py from the members of the structures of
 * core.hpp.
 *
 * * * * * * * * * */

template<typename Ar>
void binary_serialize(Ar& ar, abstract_integrator<1>& d) noexcept
{
    auto& [x, y, dQ, X, q, u, sigma] = d;
    ar(x, y, dQ, X, q, u, sigma);
}

template<typename Ar>
void binary_serialize(Ar& ar, abstract_integrator<2>& d) noexcept
{
    auto& [x, y, dQ, X, u, mu, q, mq, sigma] = d;
    ar(x, y, dQ, X, u, mu, q, mq, sigma);
}

template<typename Ar>
void binary_serialize(Ar& ar, abstract_integrator<3>& d) noexcept
{
    auto& [x, y, dQ, X, u, mu, pu, q, mq, pq, sigma] = d;
    ar(x, y, dQ, X, u, mu, pu, q, mq, pq, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_power<QssLevel>& d) noexcept
{
    auto& [x, y, value, n, sigma] = d;
    ar(x, y, value, n, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_square<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel, std::size_t PortNumber>
void binary_serialize(Ar& ar, abstract_sum<QssLevel, PortNumber>& d) noexcept
{
    auto& [x, y, values, sigma] = d;
    ar(x, y, values, sigma);
}

template<typename Ar, std::size_t QssLevel, std::size_t PortNumber>
void binary_serialize(Ar& ar, abstract_wsum<QssLevel, PortNumber>& d) noexcept
{
    auto& [x, y, input_coeffs, values, sigma] = d;
    ar(x, y, input_coeffs, values, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_inverse<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_multiplier<QssLevel>& d) noexcept
{
    auto& [x, y, values, sigma] = d;
    ar(x, y, values, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_integer<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma, upper, lower, to_send, last_send_value, reach_upper, reach_lower] = d;
    ar(x, y, value, sigma, upper, lower, to_send, last_send_value, reach_upper, reach_lower);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_compare<QssLevel>& d) noexcept
{
    auto& [x, y, a, b, output, sigma, is_a_less_b] = d;
    ar(x, y, a, b, output, sigma, is_a_less_b);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_gain<QssLevel>& d) noexcept
{
    auto& [x, y, value, k, sigma] = d;
    ar(x, y, value, k, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_log<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_exp<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_sin<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_cos<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar>
void binary_serialize(Ar& ar, counter& d) noexcept
{
    auto& [x, event_number, last_value, sum_values, sigma, type] = d;
    ar(x, event_number, last_value, sum_values, sigma);
    io_enum(ar, type, std::remove_cvref_t<decltype(type)>::sum_value);
}

template<typename Ar>
void binary_serialize(Ar& ar, generator& d) noexcept
{
    auto& [x, y, sigma, ta, value, src_data, source_ta, source_value, flags] = d;
    ar(x, y, sigma, ta, value, src_data);
    io_source(ar, source_ta, src_data);
    io_source(ar, source_value, src_data);
    ar(flags);
}

template<typename Ar>
void binary_serialize(Ar& ar, constant& d) noexcept
{
    auto& [y, sigma, offset, value, type, port] = d;
    ar(y, sigma, offset, value);
    io_enum(ar, type, std::remove_cvref_t<decltype(type)>::outcoming_component_n);
    ar(port);
}

template<typename Ar, std::size_t QssLevel, bool IsMax>
void binary_serialize(Ar& ar, abstract_min_max_hold<QssLevel, IsMax>& d) noexcept
{
    auto& [x, y, value, extremum, sigma, mode] = d;
    ar(x, y, value, extremum, sigma);
    io_enum(ar, mode, std::remove_cvref_t<decltype(mode)>::hold);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_filter<QssLevel>& d) noexcept
{
    auto& [x, y, sigma, lower_threshold, upper_threshold, value, reach_lower_threshold, reach_upper_threshold] = d;
    ar(x, y, sigma, lower_threshold, upper_threshold, value, reach_lower_threshold, reach_upper_threshold);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_sample_hold<QssLevel>& d) noexcept
{
    auto& [x, y, value, sample, ts, sigma, emit] = d;
    ar(x, y, value, sample, ts, sigma, emit);
}

template<typename Ar>
void binary_serialize(Ar& ar, zero_order_hold& d) noexcept
{
    auto& [x, y, held, sigma] = d;
    ar(x, y, held, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_quantizer<QssLevel>& d) noexcept
{
    auto& [x, y, value, q, level, sigma, emit] = d;
    ar(x, y, value, q, level, sigma, emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_integrate_and_fire<QssLevel>& d) noexcept
{
    auto& [x, y, value, acc, theta, sigma, fire] = d;
    ar(x, y, value, acc, theta, sigma, fire);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_threshold_crossing<QssLevel>& d) noexcept
{
    auto& [x, y, value, threshold, edge, sigma, side] = d;
    ar(x, y, value, threshold, edge, sigma);
    io_enum(ar, side, std::remove_cvref_t<decltype(side)>::below);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_pwm<QssLevel>& d) noexcept
{
    auto& [x, y, value, period, amplitude, duty, out_level, phase_dur, sigma, expect_fall, do_emit] = d;
    ar(x, y, value, period, amplitude, duty, out_level, phase_dur, sigma, expect_fall, do_emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_sqrt<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_atan<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_tan<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_tanh<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_sigmoid<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_division<QssLevel>& d) noexcept
{
    auto& [x, y, values, sigma] = d;
    ar(x, y, values, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_atan2<QssLevel>& d) noexcept
{
    auto& [x, y, values, sigma] = d;
    ar(x, y, values, sigma);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_saturation<QssLevel>& d) noexcept
{
    auto& [x, y, value, lower, upper, sigma, z, emit] = d;
    ar(x, y, value, lower, upper, sigma);
    io_enum(ar, z, std::remove_cvref_t<decltype(z)>::above);
    ar(emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_dead_zone<QssLevel>& d) noexcept
{
    auto& [x, y, value, lower, upper, sigma, z, emit] = d;
    ar(x, y, value, lower, upper, sigma);
    io_enum(ar, z, std::remove_cvref_t<decltype(z)>::above);
    ar(emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_abs<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma, positive, emit] = d;
    ar(x, y, value, sigma, positive, emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_sign<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma, out, emit] = d;
    ar(x, y, value, sigma, out, emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_hysteresis<QssLevel>& d) noexcept
{
    auto& [x, y, value, lower, upper, out_low, out_high, sigma, high, emit] = d;
    ar(x, y, value, lower, upper, out_low, out_high, sigma, high, emit);
}

template<typename Ar, std::size_t QssLevel, bool IsMax>
void binary_serialize(Ar& ar, abstract_min_max<QssLevel, IsMax>& d) noexcept
{
    auto& [x, y, values, sigma, sel_a, emit] = d;
    ar(x, y, values, sigma, sel_a, emit);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_wrap<QssLevel>& d) noexcept
{
    auto& [x, y, value, origin, modulo, sigma, laps, emit] = d;
    ar(x, y, value, origin, modulo, sigma, laps, emit);
}

template<typename Ar, typename AbstractLogicalTester, std::size_t PortNumber>
void binary_serialize(Ar& ar, abstract_logical<AbstractLogicalTester, PortNumber>& d) noexcept
{
    auto& [x, y, values, sigma, is_valid, value_changed] = d;
    ar(x, y, values, sigma, is_valid, value_changed);
}

template<typename Ar>
void binary_serialize(Ar& ar, logical_invert& d) noexcept
{
    auto& [x, y, sigma, value, value_changed] = d;
    ar(x, y, sigma, value, value_changed);
}

template<typename Ar>
void binary_serialize(Ar& ar, simulation_wrapper& d) noexcept
{
    auto& [x, y, sim_id, sigma, observation_time_step, run, state] = d;
    ar(x, y, sim_id, sigma, observation_time_step);
    io_enum(ar, run, std::remove_cvref_t<decltype(run)>::until);
    io_enum(ar, state, std::remove_cvref_t<decltype(state)>::input_changed);
}

template<typename Ar, std::size_t PortNumber>
void binary_serialize(Ar& ar, accumulator<PortNumber>& d) noexcept
{
    auto& [x, sigma, number, numbers] = d;
    ar(x, sigma, number, numbers);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_cross<QssLevel>& d) noexcept
{
    auto& [x, y, output_values, value, threshold, sigma, zone] = d;
    ar(x, y, output_values, value, threshold, sigma);
    io_enum(ar, zone, std::remove_cvref_t<decltype(zone)>::down);
}

template<typename Ar, std::size_t QssLevel>
void binary_serialize(Ar& ar, abstract_flipflop<QssLevel>& d) noexcept
{
    auto& [x, y, value, sigma] = d;
    ar(x, y, value, sigma);
}

template<typename Ar>
void binary_serialize(Ar& ar, time_func& d) noexcept
{
    auto& [y, offset, timestep, value, sigma, function] = d;
    ar(y, offset, timestep, value, sigma);
    io_enum(ar, function, std::remove_cvref_t<decltype(function)>::linear);
}

template<typename Ar>
void binary_serialize(Ar& ar, queue& d) noexcept
{
    auto& [x, y, sigma, fifo, ta] = d;
    ar(x, y, sigma, fifo, ta);
}

template<typename Ar>
void binary_serialize(Ar& ar, dynamic_queue& d) noexcept
{
    auto& [x, y, sigma, fifo, src_data, source_ta] = d;
    ar(x, y, sigma, fifo, src_data);
    io_source(ar, source_ta, src_data);
}

template<typename Ar>
void binary_serialize(Ar& ar, priority_queue& d) noexcept
{
    auto& [x, y, sigma, fifo, ta, src_data, source_ta] = d;
    ar(x, y, sigma, fifo, ta, src_data);
    io_source(ar, source_ta, src_data);
}

/* * * * * * * * * *
 *
 * Model
 *
 * * * * * * * * * */

/// A model: the time of the last and of the next event, the observer, the
/// handle in the scheduler, the type, then the dynamics. Only the bytes of the
/// dynamics are written, not the unused bytes of @c model::dyn.
template<typename Ar>
void binary_serialize(Ar& ar, model& m) noexcept
{
    ar(m.tl, m.tn, m.obs_id, m.handle);
    io_enum(ar, m.type, static_cast<dynamics_type>(dynamics_type_last()));

    if (not ar.ok())
        return;

    // The embedded simulations are not supported.
    if (m.type == dynamics_type::simulation_wrapper) {
        ar.fail();
        return;
    }

    dispatch(m, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        if constexpr (Ar::is_reader)
            std::construct_at(&dyn);

        ar(dyn);
    });
}

/* * * * * * * * * *
 *
 * External sources: the definitions and the positions. The files are not
 * dumped, the reader opens them again.
 *
 * * * * * * * * * */

template<typename Ar>
void binary_serialize(Ar& ar, constant_source& s) noexcept
{
    u32 length = s.length;

    if constexpr (Ar::is_writer) {
        if (length > s.buffer.size()) {
            ar.fail();
            return;
        }
    }

    ar(s.name, length);

    if constexpr (Ar::is_reader) {
        if (not ar.ok() or length > s.buffer.size()) {
            ar.fail();
            return;
        }

        s.length = length;
        ar.read_span(std::span<real>(s.buffer.data(), length));
    } else {
        ar.write_span(std::span<const real>(s.buffer.data(), length));
    }
}

template<typename Ar>
void binary_serialize(Ar& ar, random_source& s) noexcept
{
    ar(s.name, s.reals, s.ints);
    io_enum(ar, s.distribution, distribution_type::weibull);
}

template<typename Ar>
void binary_serialize(Ar& ar, binary_file_source& s) noexcept
{
    ar(s.name, s.file_path, s.max_clients, s.next_client, s.next_offset);

    if constexpr (Ar::is_reader) {
        // init() allocates a buffer by client.
        if (s.max_clients > (1u << 16) or s.next_client > s.max_clients or
            not ar.can_allocate(static_cast<std::size_t>(s.max_clients) *
                                sizeof(chunk_type)))
            ar.fail();
    }
}

template<typename Ar>
void binary_serialize(Ar& ar, text_file_source& s) noexcept
{
    ar(s.name, s.file_path, s.offset);
}

/* * * * * * * * * *
 *
 * Observers
 *
 * * * * * * * * * */

struct binary_access;

template<typename Ar>
void binary_serialize(Ar& ar, observer& o) noexcept;

template<typename Ar>
void binary_serialize(Ar& ar, resampler& r) noexcept;

/// The friend of the classes that hide their state (the observer, the
/// resampler, the heap, the scheduler and the simulation).
struct binary_access {
    using heap_type = heap<allocator<new_delete_memory_resource>>;
    using heap_node = heap_type::node;

    /* observer */

    /// The model, the last time, the raw samples that were not read yet by
    /// the resampler and the history. The version of the history is not
    /// kept: the reader of the history is a new observer.
    template<typename Ar>
    static void io(Ar& ar, observer& o) noexcept
    {
        ar(o.m_model, o.m_last_t);

        if constexpr (Ar::is_writer) {
            ar.write(static_cast<u32>(o.m_raw.size()));
            o.m_raw.for_each([&ar](const raw_sample& s) noexcept { ar(s); });

            o.m_history.read(
              [&ar](const vector<resampled_sample>& history,
                    const u64 /*version*/) noexcept {
                  ar(history);
              });
        } else {
            u32 count = 0;
            ar.read(count);

            o.m_raw.clear();

            if (not ar.ok() or count >= o.m_raw.capacity() or
                count > ar.remaining() / sizeof(raw_sample)) {
                ar.fail();
                return;
            }

            for (u32 i = 0; i != count and ar.ok(); ++i) {
                raw_sample s;
                ar(s);

                if (ar.ok() and not o.m_raw.push(s))
                    ar.fail();
            }

            o.m_history.write_only(
              [&ar](vector<resampled_sample>& history) noexcept {
                  ar(history);
              });
        }
    }

    /* resampler */

    template<typename Ar>
    static void io(Ar& ar, resampler& r) noexcept
    {
        ar(r.m_batch, r.m_dt, r.m_next_sample_time, r.m_last_pushed_value,
           r.m_pending, r.m_prev);

        auto order = r.m_interp.order();
        io_enum(ar, order, interpolate_type::qss3);

        if constexpr (Ar::is_reader)
            r.m_interp = qss_interpolator(order);

        ar(r.m_has_pending, r.m_has_prev, r.m_has_last_pushed);
    }

    /* scheduler */

    /// Checks the structure of the pairing heap: the free list, the tree and
    /// the detached nodes are disjoint and acyclic, every node is in exactly
    /// one of them, the links are consistent (the previous node of the first
    /// child is its parent, the previous node of the other children is the
    /// previous sibling) and the heap order is respected.
    static bool heap_structure_ok(const heap_node* nodes,
                                  const u32        max_size,
                                  const u32        size,
                                  const u32        free_list,
                                  const u32        root) noexcept
    {
        constexpr u32 none = invalid_heap_handle;

        if (max_size == 0)
            return size == 0 and free_list == none and root == none;

        if (nodes == nullptr)
            return false;

        vector<u8> state; // 0 detached, 1 free, 2 in the tree
        vector<u32> stack;
        if (not state.resize(max_size) or not stack.resize(max_size))
            return false;

        std::fill_n(state.data(), max_size, u8{ 0 });

        for (u32 f = free_list; f != none; f = nodes[f].next) {
            if (f >= max_size or state[f] != 0)
                return false;

            state[f] = 1;

            if (ordinal(nodes[f].id) != 0 or nodes[f].prev != none or
                nodes[f].child != none)
                return false;
        }

        u32 tree_count = 0;

        if (root == none) {
            if (size != 0)
                return false;
        } else {
            if (root >= max_size or nodes[root].prev != none or
                nodes[root].next != none)
                return false;

            u32 top    = 0;
            state[root] = 2;
            tree_count = 1;
            stack[top++] = root;

            while (top != 0) {
                const u32 parent = stack[--top];
                u32       prev   = parent;

                for (u32 c = nodes[parent].child; c != none; c = nodes[c].next) {
                    if (c >= max_size or state[c] != 0 or
                        nodes[c].prev != prev or
                        not(nodes[c].tn >= nodes[parent].tn))
                        return false;

                    state[c]     = 2;
                    stack[top++] = c;
                    ++tree_count;
                    prev = c;
                }
            }
        }

        if (tree_count != size)
            return false;

        for (u32 i = 0; i != max_size; ++i) {
            if (state[i] == 1)
                continue;

            if (ordinal(nodes[i].id) == 0)
                return false;

            if (state[i] == 0 and (nodes[i].prev != none or
                                   nodes[i].next != none or
                                   nodes[i].child != none))
                return false;
        }

        return true;
    }

    static bool heap_ok(const heap_type& h) noexcept
    {
        return heap_structure_ok(h.nodes, h.max_size, h.m_size, h.free_list,
                                 h.root);
    }

    /// The capacity, the sizes, the head of the free list, the root and the
    /// nodes in the order of the array: the handles of the models stay valid.
    template<typename Ar>
    static void io(Ar& ar, heap_type& h) noexcept
    {
        if constexpr (Ar::is_writer) {
            ar(h.capacity, h.max_size, h.m_size, h.free_list, h.root);

            for (u32 i = 0; i != h.max_size; ++i) {
                const auto& n = h.nodes[i];
                ar(n.tn, n.id, n.prev, n.next, n.child);
            }
        } else {
            u32 capacity = 0, max_size = 0, size = 0, free_list = 0, root = 0;

            ar(capacity, max_size, size, free_list, root);
            h.clear();

            constexpr std::size_t node_bytes = sizeof(time) + sizeof(model_id) +
                                               3 * sizeof(u32);

            if (not ar.ok() or capacity >= invalid_heap_handle or
                max_size > capacity or size > max_size or
                max_size > ar.remaining() / node_bytes) {
                ar.fail();
                return;
            }

            if (capacity > h.capacity) {
                if (not ar.can_allocate(static_cast<std::size_t>(capacity) *
                                        sizeof(heap_node)) or
                    not h.reserve(capacity)) {
                    ar.fail();
                    return;
                }
            }

            for (u32 i = 0; i != max_size and ar.ok(); ++i) {
                auto& n = h.nodes[i];
                ar(n.tn, n.id, n.prev, n.next, n.child);
            }

            if (not ar.ok() or
                not heap_structure_ok(h.nodes, max_size, size, free_list,
                                      root)) {
                ar.fail();
                h.clear();
                return;
            }

            h.max_size  = max_size;
            h.m_size    = size;
            h.free_list = free_list;
            h.root      = root;
        }
    }

    /* references between the parts of the simulation */

    template<typename Dynamics>
    static bool references_ok(const simulation& sim,
                              const Dynamics&   dyn) noexcept
    {
        if constexpr (has_output_port<Dynamics>) {
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                if (dyn.y[i] != undefined<output_port_id>() and
                    not sim.output_ports.exists(dyn.y[i]))
                    return false;
        }

        if constexpr (requires { dyn.fifo; }) {
            if (dyn.fifo != undefined<dated_message_id>() and
                not sim.dated_messages.exists(dyn.fifo))
                return false;
        }

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            if (dyn.id != undefined<hsm_id>() and not sim.hsms.exists(dyn.id))
                return false;
        }

        // The messages that are waiting for the next step: they live in the
        // message buffer of the simulation.
        if constexpr (has_input_port<Dynamics>) {
            for (int i = 0, e = length(dyn.x); i != e; ++i) {
                const auto& port = dyn.x[i];

                if (port.size > port.capacity or
                    static_cast<std::size_t>(port.position) + port.size >
                      static_cast<std::size_t>(sim.message_buffer.size()))
                    return false;
            }
        }

        return true;
    }

    static bool node_ok(const node& n) noexcept { return n.port_index >= 0; }

    /// The consistency of the simulation: the writer refuses to write
    /// anything else and the reader refuses to build anything else.
    ///
    /// - the models: valid type, observer and scheduler node that points
    ///   back to the model, identifiers of ports, dated messages and state
    ///   machines that exist;
    /// - the observers: the model exists and points back to the observer;
    /// - the scheduler: the structure of the heap and one node by model with
    ///   a handle;
    /// - the connections: the lists of blocks of an output port end and do
    ///   not share a block.
    static bool check(const simulation& sim) noexcept
    {
        if (sim.sims.size() != 0)
            return false;

        const auto& h = sim.sched.m_heap;

        for (const auto& mdl : sim.models) {
            const auto id = sim.models.get_id(mdl);

            if (std::cmp_greater(static_cast<int>(mdl.type),
                                 dynamics_type_last()) or
                mdl.type == dynamics_type::simulation_wrapper)
                return false;

            if (mdl.obs_id != observer_id{ 0 }) {
                const auto* obs = sim.observers.try_to_get(mdl.obs_id);

                if (obs == nullptr or obs->model() != id)
                    return false;
            }

            if (mdl.handle != invalid_heap_handle and
                (mdl.handle >= h.max_size or h.nodes[mdl.handle].id != id))
                return false;

            const bool refs = dispatch(
              mdl, [&sim]<typename Dynamics>(const Dynamics& dyn) noexcept {
                  return references_ok(sim, dyn);
              });

            if (not refs)
                return false;
        }

        for (const auto& obs : sim.observers) {
            const auto* mdl = sim.models.try_to_get(obs.model());

            if (mdl == nullptr or mdl->obs_id != sim.observers.get_id(obs))
                return false;
        }

        if (not heap_ok(h))
            return false;

        for (u32 i = 0; i != h.max_size; ++i) {
            if (ordinal(h.nodes[i].id) == 0)
                continue;

            const auto* mdl = sim.models.try_to_get(h.nodes[i].id);
            if (mdl == nullptr or mdl->handle != i)
                return false;
        }

        vector<u8> seen;
        if (not seen.resize(static_cast<std::size_t>(sim.nodes.capacity())))
            return false;

        std::fill_n(seen.data(), seen.size(), u8{ 0 });

        for (const auto& port : sim.output_ports) {
            for (const auto& n : port.connections)
                if (not node_ok(n))
                    return false;

            for (auto id = port.next; id != undefined<block_node_id>();) {
                const auto* block = sim.nodes.try_to_get(id);
                if (block == nullptr)
                    return false;

                const auto index = static_cast<std::size_t>(get_index(id));
                if (index >= seen.size() or seen[index] != 0)
                    return false;

                seen[index] = 1;

                for (const auto& n : block->nodes)
                    if (not node_ok(n))
                        return false;

                id = block->next;
            }
        }

        return true;
    }

    /* the parts of the simulation */

    /// The state that changes while the simulation runs. The reader makes
    /// room for the models first (the transient vectors and the scheduler
    /// are sized with the models).
    template<typename Ar>
    static void io_snapshot(Ar& ar, simulation& sim) noexcept
    {
        real t    = 0.0;
        real last = 0.0;

        if constexpr (Ar::is_writer) {
            t    = sim.t.load(std::memory_order_acquire);
            last = sim.last_valid_t;
        }

        ar(t, last);
        ar(sim.models);

        if constexpr (Ar::is_reader) {
            if (not ar.ok())
                return;

            if (not sim.grow_models_to(sim.models.capacity())) {
                ar.fail();
                return;
            }
        }

        ar(sim.observers, sim.nodes, sim.output_ports, sim.dated_messages);
        io(ar, sim.sched.m_heap);
        ar(sim.message_buffer);

        if constexpr (Ar::is_reader) {
            if (not ar.ok())
                return;

            const auto connections = std::max<std::size_t>(
              static_cast<std::size_t>(sim.nodes.capacity()),
              static_cast<std::size_t>(sim.output_ports.capacity()));

            if (not sim.grow_connections_to(connections)) {
                ar.fail();
                return;
            }

            sim.t.store(t, std::memory_order_release);
            sim.last_valid_t = last;
        }
    }

    static void clear_snapshot(simulation& sim) noexcept
    {
        sim.models.clear();
        sim.observers.clear();
        sim.nodes.clear();
        sim.output_ports.clear();
        sim.dated_messages.clear();
        sim.sched.clear();
        sim.message_buffer.clear();
    }

    template<typename Ar>
    static void io_limits(Ar& ar, simulation& sim) noexcept
    {
        time begin = sim.limits.begin();
        time end   = sim.limits.end();

        ar(begin, end);

        if constexpr (Ar::is_reader) {
            sim.limits.set_bound(begin, end);

            // NaN and an interval that set_bound changed are errors.
            if (not(sim.limits.begin() == begin and sim.limits.end() == end))
                ar.fail();
        }
    }

    template<typename Ar>
    static void io_sources(Ar& ar, external_source& srcs) noexcept
    {
        i32 binary_clients = srcs.binary_file_max_client;
        i32 random_clients = srcs.random_max_client;

        ar(srcs.seed, binary_clients, random_clients);

        if constexpr (Ar::is_reader) {
            if (binary_clients < 1 or binary_clients > (1 << 16) or
                random_clients < 1 or random_clients > (1 << 16)) {
                ar.fail();
                return;
            }

            srcs.binary_file_max_client = binary_clients;
            srcs.random_max_client      = random_clients;
        }

        ar(srcs.constant_sources, srcs.binary_file_sources,
           srcs.text_file_sources, srcs.random_sources);
    }

    /// The parameters of the alive models, in the order of the slots.
    template<typename Ar>
    static void io_parameters(Ar& ar, simulation& sim) noexcept
    {
        if constexpr (Ar::is_reader) {
            std::fill_n(sim.parameters.data(), sim.parameters.size(),
                        parameter{});
        }

        for (auto& mdl : sim.models) {
            const auto index = static_cast<std::size_t>(
              get_index(sim.models.get_id(mdl)));

            if (index >= sim.parameters.size()) {
                ar.fail();
                return;
            }

            ar(sim.parameters[index]);
        }
    }

    /* after the reading */

    /// The binary files are opened again (the positions of the dump are kept),
    /// the text files too.
    static status prepare_files(external_source& srcs) noexcept
    {
        for (auto& bin : srcs.binary_file_sources) {
            const auto next_client = bin.next_client;
            const auto next_offset = bin.next_offset;

            irt_check(bin.init());

            bin.next_client = next_client;
            bin.next_offset = next_offset;

            // binary_file_source::restore() fills the buffer of a client if the
            // offset of the client is not the offset of the source: no offset
            // can match.
            std::fill_n(bin.offsets.data(), bin.offsets.size(),
                        ~static_cast<u64>(0));
        }

        for (auto& txt : srcs.text_file_sources)
            irt_check(txt.init());

        return success();
    }

    /// Fills the buffers of the sources that read a file: the buffers are not
    /// dumped. The buffers of the constant and of the random sources are
    /// complete after the reading.
    static status restore_sources(simulation& sim) noexcept
    {
        status result = success();

        for (auto& mdl : sim.models) {
            dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
                for_each_source(
                  dyn, [&](source& src, source_data& data) noexcept {
                      if (not result.has_value() or src.buffer.empty())
                          return;

                      if (src.type == source_type::binary_file or
                          src.type == source_type::text_file)
                          result = sim.srcs.restore_source(src, data);
                  });
            });
        }

        return result;
    }
};

template<typename Ar>
void binary_serialize(Ar& ar, observer& o) noexcept
{
    binary_access::io(ar, o);
}

template<typename Ar>
void binary_serialize(Ar& ar, resampler& r) noexcept
{
    binary_access::io(ar, r);
}

/* * * * * * * * * *
 *
 * API
 *
 * * * * * * * * * */

namespace {

error_code reader_error(const binary_reader& r) noexcept
{
    return make_error(r.remaining() == 0 ? simulation_errc::file_eof_error
                                         : simulation_errc::range_error);
}

} // namespace

status write_snapshot(binary_writer& w, const simulation& sim) noexcept
{
    if (not binary_access::check(sim))
        return make_error(simulation_errc::range_error);

    write_header(w, binary_snapshot_magic, binary_simulation_version);
    binary_access::io_snapshot(w, const_cast<simulation&>(sim));

    if (not w.ok())
        return make_error(simulation_errc::memory_error);

    return success();
}

status read_snapshot(binary_reader& r, simulation& sim) noexcept
{
    r.set_context(&sim.srcs);

    const auto version = read_header(r, binary_snapshot_magic);
    if (version != binary_simulation_version) {
        binary_access::clear_snapshot(sim);
        return make_error(simulation_errc::range_error);
    }

    binary_access::io_snapshot(r, sim);

    if (not r.ok()) {
        binary_access::clear_snapshot(sim);
        return reader_error(r);
    }

    if (not binary_access::check(sim)) {
        binary_access::clear_snapshot(sim);
        return make_error(simulation_errc::range_error);
    }

    return binary_access::restore_sources(sim);
}

status write_simulation(binary_writer& w, const simulation& sim) noexcept
{
    if (not binary_access::check(sim))
        return make_error(simulation_errc::range_error);

    auto& s = const_cast<simulation&>(sim);

    write_header(w, binary_simulation_magic, binary_simulation_version);
    binary_access::io_limits(w, s);
    binary_access::io_sources(w, s.srcs);
    w(s.hsms);
    binary_access::io_snapshot(w, s);
    binary_access::io_parameters(w, s);

    if (not w.ok())
        return make_error(simulation_errc::memory_error);

    return success();
}

status read_simulation(binary_reader& r, simulation& sim) noexcept
{
    const auto fail = [&sim](const error_code ec) noexcept -> status {
        sim.clear();
        sim.srcs.clear();

        return ec;
    };

    sim.clear();
    sim.srcs.clear();
    r.set_context(&sim.srcs);

    if (read_header(r, binary_simulation_magic) != binary_simulation_version)
        return fail(make_error(simulation_errc::range_error));

    binary_access::io_limits(r, sim);
    binary_access::io_sources(r, sim.srcs);

    if (not r.ok())
        return fail(reader_error(r));

    // The binary files must be opened before the models are read: the sources
    // of the models view the buffers of the files.
    if (auto ret = binary_access::prepare_files(sim.srcs); not ret)
        return fail(ret.error());

    r(sim.hsms);
    binary_access::io_snapshot(r, sim);

    if (r.ok())
        binary_access::io_parameters(r, sim);

    if (not r.ok())
        return fail(reader_error(r));

    if (not binary_access::check(sim))
        return fail(make_error(simulation_errc::range_error));

    if (auto ret = binary_access::restore_sources(sim); not ret)
        return fail(ret.error());

    return success();
}

status save_simulation(const path& p, const simulation& sim) noexcept
{
    binary_writer w;
    irt_check(write_simulation(w, sim));

    auto f = file::open(p, file_mode(file_open_options::write));
    if (not f)
        return f.error();

    if (not write_all(*f, w.bytes()))
        return make_error(simulation_errc::file_access_error);

    f->close();

    return success();
}

status load_simulation(const path&       p,
                       simulation&       sim,
                       const std::size_t max_allocation) noexcept
{
    auto f = file::open(p, file_mode(file_open_options::read));
    if (not f)
        return f.error();

    const auto buffer = f->read_entire_file();
    if (buffer.empty())
        return make_error(simulation_errc::file_empty);

    binary_reader r(bytes_of(buffer));
    if (max_allocation != 0)
        r.set_allocation_limit(max_allocation);

    return read_simulation(r, sim);
}

} // namespace irt
