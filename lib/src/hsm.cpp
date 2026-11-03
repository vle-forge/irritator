// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <fmt/format.h>

namespace irt {

using hsm_t = hierarchical_state_machine;

template<typename T, typename... Args>
constexpr bool all_equal(const T& s, Args&&... args) noexcept
{
    return ((s == std::forward<Args>(args)) && ... && true);
}

static constexpr i32 copy_to_i32(
  const std::span<const real, hsm_t::max_constants> c,
  const hsm_t::variable                             v,
  const hsm_t::state_action&                        act,
  hsm_t::execution&                                 e) noexcept
{
    switch (v) {
    case hsm_t::variable::none:
        return 0;

    case hsm_t::variable::port_0:
        return static_cast<i32>(e.ports[3]);

    case hsm_t::variable::port_1:
        return static_cast<i32>(e.ports[2]);

    case hsm_t::variable::port_2:
        return static_cast<i32>(e.ports[1]);

    case hsm_t::variable::port_3:
        return static_cast<i32>(e.ports[0]);

    case hsm_t::variable::var_i1:
        return e.i1;

    case hsm_t::variable::var_i2:
        return e.i2;

    case hsm_t::variable::var_r1:
        return static_cast<i32>(e.r1);

    case hsm_t::variable::var_r2:
        return static_cast<i32>(e.r2);

    case hsm_t::variable::var_timer:
        return static_cast<i32>(e.timer);

    case hsm_t::variable::constant_i:
        return act.constant.i;

    case hsm_t::variable::constant_r:
        return static_cast<i32>(act.constant.f);

    case hsm_t::variable::hsm_constant_0:
    case hsm_t::variable::hsm_constant_1:
    case hsm_t::variable::hsm_constant_2:
    case hsm_t::variable::hsm_constant_3:
    case hsm_t::variable::hsm_constant_4:
    case hsm_t::variable::hsm_constant_5:
    case hsm_t::variable::hsm_constant_6:
    case hsm_t::variable::hsm_constant_7:
        return static_cast<i32>(
          c[ordinal(v) - ordinal(hsm_t::variable::hsm_constant_0)]);

    case hsm_t::variable::source:
        return static_cast<i32>(e.source_value.next());
    }

    irt::unreachable();
}

static constexpr real copy_to_real(
  const std::span<const real, hsm_t::max_constants> c,
  const hsm_t::variable                             v,
  const hsm_t::state_action&                        act,
  hsm_t::execution&                                 e) noexcept
{
    switch (v) {
    case hsm_t::variable::none:
        return 0.0;
    case hsm_t::variable::port_0:
        return e.ports[3];
    case hsm_t::variable::port_1:
        return e.ports[2];
    case hsm_t::variable::port_2:
        return e.ports[1];
    case hsm_t::variable::port_3:
        return e.ports[0];
    case hsm_t::variable::var_i1:
        return static_cast<real>(e.i1);
    case hsm_t::variable::var_i2:
        return static_cast<real>(e.i2);
    case hsm_t::variable::var_r1:
        return e.r1;
    case hsm_t::variable::var_r2:
        return e.r2;
    case hsm_t::variable::var_timer:
        return e.timer;
    case hsm_t::variable::constant_i:
        return static_cast<real>(act.constant.i);
    case hsm_t::variable::constant_r:
        return act.constant.f;
    case hsm_t::variable::hsm_constant_0:
    case hsm_t::variable::hsm_constant_1:
    case hsm_t::variable::hsm_constant_2:
    case hsm_t::variable::hsm_constant_3:
    case hsm_t::variable::hsm_constant_4:
    case hsm_t::variable::hsm_constant_5:
    case hsm_t::variable::hsm_constant_6:
    case hsm_t::variable::hsm_constant_7:
        return c[ordinal(v) - ordinal(hsm_t::variable::hsm_constant_0)];
    case hsm_t::variable::source:
        return e.source_value.next();
    }

    irt::unreachable();
}

struct wrap_var {
    union {
        real r;
        i32  i;
    };

    enum type : u8 { real_t, integer_t } type;

    constexpr bool operator==(const wrap_var& o) const noexcept
    {
        if (type == type::real_t)
            return r ==
                   ((o.type == type::real_t) ? o.r : static_cast<real>(o.i));

        return i == ((o.type == type::real_t) ? static_cast<i32>(o.r) : o.i);
    }

    constexpr std::partial_ordering operator<=>(
      const wrap_var& o) const noexcept
    {
        return (type == type::real_t)
                 ? r <=>
                     ((o.type == type::real_t) ? o.r : static_cast<real>(o.i))
                 : i <=>
                     ((o.type == type::real_t) ? static_cast<i32>(o.r) : o.i);
    }

    template<typename Action>
    constexpr wrap_var(const std::span<const real, hsm_t::max_constants> c,
                       const hsm_t::variable                             v,
                       Action&                                           act,
                       hsm_t::execution& e) noexcept
    {
        switch (v) {
        case hsm_t::variable::none:
            r    = 0;
            i    = 0;
            type = type::integer_t;
            break;

        case hsm_t::variable::port_0:
            r    = e.ports[3];
            i    = static_cast<i32>(e.ports[3]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_1:
            r    = e.ports[2];
            i    = static_cast<i32>(e.ports[2]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_2:
            r    = e.ports[1];
            i    = static_cast<i32>(e.ports[1]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_3:
            r    = e.ports[0];
            i    = static_cast<i32>(e.ports[0]);
            type = type::real_t;
            break;

        case hsm_t::variable::var_i1:
            r    = static_cast<double>(e.i1);
            i    = e.i1;
            type = type::integer_t;
            break;

        case hsm_t::variable::var_i2:
            r    = static_cast<double>(e.i2);
            i    = e.i2;
            type = type::integer_t;
            break;

        case hsm_t::variable::var_r1:
            r    = e.r1;
            i    = static_cast<i32>(e.r1);
            type = type::real_t;
            break;

        case hsm_t::variable::var_r2:
            r    = e.r2;
            i    = static_cast<i32>(e.r2);
            type = type::real_t;
            break;

        case hsm_t::variable::var_timer:
            r    = e.timer;
            i    = static_cast<i32>(e.timer);
            type = type::real_t;
            break;

        case hsm_t::variable::constant_i:
            r    = static_cast<double>(act.constant.i);
            i    = act.constant.i;
            type = type::integer_t;
            break;

        case hsm_t::variable::constant_r:
            r    = act.constant.f;
            i    = static_cast<i32>(act.constant.f);
            type = type::real_t;
            break;

        case hsm_t::variable::hsm_constant_0:
        case hsm_t::variable::hsm_constant_1:
        case hsm_t::variable::hsm_constant_2:
        case hsm_t::variable::hsm_constant_3:
        case hsm_t::variable::hsm_constant_4:
        case hsm_t::variable::hsm_constant_5:
        case hsm_t::variable::hsm_constant_6:
        case hsm_t::variable::hsm_constant_7:
            r    = c[ordinal(v) - ordinal(hsm_t::variable::hsm_constant_0)];
            i    = static_cast<i32>(act.constant.f);
            type = type::real_t;
            break;

        case hsm_t::variable::source:
            r    = e.source_value.next();
            i    = static_cast<i32>(r);
            type = type::real_t;
            break;
        }
    }
};

static auto is_action_using_source(const hsm_t::state_action& act) noexcept
  -> bool
{
    return any_equal(act.type,
                     hsm_t::action_type::affect,
                     hsm_t::action_type::plus,
                     hsm_t::action_type::minus,
                     hsm_t::action_type::negate,
                     hsm_t::action_type::multiplies,
                     hsm_t::action_type::divides,
                     hsm_t::action_type::modulus,
                     hsm_t::action_type::output) and
           act.var2 == hsm_t::variable::source;
}

auto hierarchical_state_machine::compute_is_using_source() const noexcept
  -> bool
{
    for (auto i = 0, e = length(states); i != e; ++i) {
        if (not all_equal(hierarchical_state_machine::invalid_state_id,
                          states[i].if_transition,
                          states[i].else_transition,
                          states[i].super_id,
                          states[i].sub_id) and
            (is_action_using_source(states[i].if_action) or
             is_action_using_source(states[i].else_action) or
             is_action_using_source(states[i].enter_action) or
             is_action_using_source(states[i].exit_action)))
            return true;
    }

    return false;
}

int hierarchical_state_machine::compute_max_state_used() const noexcept
{
    int biggest = 0;

    for (auto i = 0, e = length(states); i != e; ++i)
        if (not all_equal(hierarchical_state_machine::invalid_state_id,
                          states[i].if_transition,
                          states[i].else_transition,
                          states[i].super_id,
                          states[i].sub_id))
            biggest = i;

    return biggest;
}

bool hierarchical_state_machine::is_dispatching(execution& exec) const noexcept
{
    return exec.current_source_state != invalid_state_id;
}

static constexpr bool is_port(const hsm_t::variable v) noexcept
{
    return irt::any_equal(v,
                          hsm_t::variable::port_0,
                          hsm_t::variable::port_1,
                          hsm_t::variable::port_2,
                          hsm_t::variable::port_3);
}

static constexpr bool is_affectable(const hsm_t::variable v) noexcept
{
    return irt::any_equal(v,
                          hsm_t::variable::port_0,
                          hsm_t::variable::port_1,
                          hsm_t::variable::port_2,
                          hsm_t::variable::port_3,
                          hsm_t::variable::var_i1,
                          hsm_t::variable::var_i2,
                          hsm_t::variable::var_r1,
                          hsm_t::variable::var_r2,
                          hsm_t::variable::var_timer);
}

static constexpr bool is_bit_operable(const hsm_t::variable v) noexcept
{
    return irt::any_equal(v,
                          hsm_t::variable::port_0,
                          hsm_t::variable::port_1,
                          hsm_t::variable::port_2,
                          hsm_t::variable::port_3,
                          hsm_t::variable::var_i1,
                          hsm_t::variable::var_i2,
                          hsm_t::variable::constant_i);
}

static constexpr hsm_t::state_action do_affect(hsm_t::action_type type,
                                               hsm_t::variable    v1,
                                               hsm_t::variable    v2) noexcept
{
    return hsm_t::state_action{
        .var1 = v1, .var2 = v2, .type = type, .constant = { .i = 0 }
    };
}

static constexpr hsm_t::state_action do_affect(hsm_t::action_type type,
                                               hsm_t::variable    v1,
                                               i32                i) noexcept
{
    return hsm_t::state_action{ .var1     = v1,
                                .var2     = hsm_t::variable::constant_i,
                                .type     = type,
                                .constant = { .i = i } };
}

static constexpr hsm_t::state_action do_affect(hsm_t::action_type type,
                                               hsm_t::variable    v1,
                                               float              f) noexcept
{
    return hsm_t::state_action{ .var1     = v1,
                                .var2     = hsm_t::variable::constant_r,
                                .type     = type,
                                .constant = { .f = f } };
}

static constexpr hsm_t::state_action do_affect(hsm_t::action_type type,
                                               hsm_t::variable    v1) noexcept
{
    return hsm_t::state_action{ .var1     = v1,
                                .var2     = hsm_t::variable::none,
                                .type     = type,
                                .constant = { .i = 0 } };
}

void hierarchical_state_machine::state_action::set_default(
  const action_type t) noexcept
{
    type       = t;
    var1       = variable::none;
    var2       = variable::none;
    constant.i = 0;

    switch (t) {
    case action_type::none:
        break;
    case action_type::set:
        var1 = variable::port_0;
        break;
    case action_type::unset:
        var1 = variable::port_0;
        break;
    case action_type::reset:
        break;
    case action_type::output:
        var1       = variable::port_0;
        var2       = variable::constant_i;
        constant.i = 1;
        break;
    case action_type::affect:
    case action_type::plus:
    case action_type::minus:
    case action_type::negate:
    case action_type::multiplies:
    case action_type::divides:
    case action_type::modulus:
        var1       = variable::var_i1;
        var2       = variable::constant_i;
        constant.i = 1;
        break;
    case action_type::bit_and:
    case action_type::bit_or:
    case action_type::bit_not:
    case action_type::bit_xor:
        var1 = variable::var_i1;
        var2 = variable::var_i2;
        break;
    }
}

void hierarchical_state_machine::state_action::set_setport(variable v1) noexcept
{
    debug::ensure(is_port(v1));

    if (is_port(v1))
        *this = do_affect(hsm_t::action_type::set, v1);
}

void hierarchical_state_machine::state_action::set_unsetport(
  variable v1) noexcept
{
    debug::ensure(is_port(v1));

    if (is_port(v1))
        *this = do_affect(hsm_t::action_type::unset, v1);
}

void hierarchical_state_machine::state_action::set_affect(variable v1,
                                                          variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::affect, v1, v2);
}

void hierarchical_state_machine::state_action::set_reset() noexcept
{
    *this = do_affect(hsm_t::action_type::reset, hsm_t::variable::none);
}

void hierarchical_state_machine::state_action::set_output(variable v1,
                                                          variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    *this = do_affect(hsm_t::action_type::output, v1, v2);
}

void hierarchical_state_machine::state_action::set_output(variable v1,
                                                          i32      i) noexcept
{
    debug::ensure(is_affectable(v1));

    *this = do_affect(hsm_t::action_type::output, v1, i);
}

void hierarchical_state_machine::state_action::set_output(variable v1,
                                                          float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    *this = do_affect(hsm_t::action_type::output, v1, f);
}

void hierarchical_state_machine::state_action::set_affect(variable v1,
                                                          i32      i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::affect, v1, i);
}

void hierarchical_state_machine::state_action::set_affect(variable v1,
                                                          float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::affect, v1, f);
}

void hierarchical_state_machine::state_action::set_plus(variable v1,
                                                        variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::plus, v1, v2);
}

void hierarchical_state_machine::state_action::set_plus(variable v1,

                                                        i32 i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::plus, v1, i);
}

void hierarchical_state_machine::state_action::set_plus(variable v1,
                                                        float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::plus, v1, f);
}

void hierarchical_state_machine::state_action::set_minus(variable v1,
                                                         variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::minus, v1, v2);
}

void hierarchical_state_machine::state_action::set_minus(variable v1,

                                                         i32 i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::minus, v1, i);
}

void hierarchical_state_machine::state_action::set_minus(variable v1,
                                                         float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::minus, v1, f);
}

void hierarchical_state_machine::state_action::set_negate(variable v1,
                                                          variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::negate, v1, v2);
}

void hierarchical_state_machine::state_action::set_multiplies(
  variable v1,
  variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::multiplies, v1, v2);
}

void hierarchical_state_machine::state_action::set_multiplies(variable v1,

                                                              i32 i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::multiplies, v1, i);
}

void hierarchical_state_machine::state_action::set_multiplies(variable v1,
                                                              float f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::multiplies, v1, f);
}

void hierarchical_state_machine::state_action::set_divides(variable v1,
                                                           variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::divides, v1, v2);
}

void hierarchical_state_machine::state_action::set_divides(variable v1,

                                                           i32 i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::divides, v1, i);
}

void hierarchical_state_machine::state_action::set_divides(variable v1,
                                                           float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::divides, v1, f);
}

void hierarchical_state_machine::state_action::set_modulus(variable v1,
                                                           variable v2) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::modulus, v1, v2);
}

void hierarchical_state_machine::state_action::set_modulus(variable v1,
                                                           i32      i) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::modulus, v1, i);
}

void hierarchical_state_machine::state_action::set_modulus(variable v1,
                                                           float    f) noexcept
{
    debug::ensure(is_affectable(v1));

    if (is_affectable(v1))
        *this = do_affect(hsm_t::action_type::modulus, v1, f);
}

void hierarchical_state_machine::state_action::set_bit_and(variable v1,
                                                           variable v2) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_and, v1, v2);
}

void hierarchical_state_machine::state_action::set_bit_and(variable v1,

                                                           i32 i) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_and, v1, i);
}

void hierarchical_state_machine::state_action::set_bit_or(variable v1,
                                                          variable v2) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_or, v1, v2);
}

void hierarchical_state_machine::state_action::set_bit_or(variable v1,

                                                          i32 i) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_or, v1, i);
}

void hierarchical_state_machine::state_action::set_bit_not(variable v1,
                                                           variable v2) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_not, v1, v2);
}

void hierarchical_state_machine::state_action::set_bit_not(variable v1,

                                                           i32 i) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_not, v1, i);
}

void hierarchical_state_machine::state_action::set_bit_xor(variable v1,
                                                           variable v2) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_xor, v1, v2);
}

void hierarchical_state_machine::state_action::set_bit_xor(variable v1,

                                                           i32 i) noexcept
{
    debug::ensure(is_bit_operable(v1));

    if (is_bit_operable(v1))
        *this = do_affect(hsm_t::action_type::bit_xor, v1, i);
}

void hierarchical_state_machine::state_action::clear() noexcept
{
    *this = do_affect(hsm_t::action_type::none, variable::none, variable::none);
}

void hierarchical_state_machine::condition_action::set(u8 port,
                                                       u8 mask) noexcept
{
    type       = condition_type::port;
    constant.u = (port << 4) | mask;
}

void hierarchical_state_machine::condition_action::get(u8& port,
                                                       u8& mask) const noexcept
{
    port = (constant.u >> 4) & 0b1111;
    mask = constant.u & 0b1111;
}

void hierarchical_state_machine::condition_action::set(
  const std::bitset<4> port,
  const std::bitset<4> mask) noexcept
{
    type       = condition_type::port;
    constant.u = static_cast<u32>(port.to_ulong() << 4 | mask.to_ulong());
}

std::pair<std::bitset<4>, std::bitset<4>>
hierarchical_state_machine::condition_action::get_bitset() const noexcept
{
    return std::make_pair<std::bitset<4>, std::bitset<4>>(
      std::bitset<4>(constant.u >> 4 & 0b1111),
      std::bitset<4>(constant.u & 0b1111));
}

void hierarchical_state_machine::condition_action::set_timer() noexcept
{
    type = condition_type::sigma;
}

static hsm_t::condition_action do_affect(hsm_t::condition_type type,
                                         hsm_t::variable       v1,
                                         hsm_t::variable       v2) noexcept
{
    return hsm_t::condition_action{
        .var1 = v1, .var2 = v2, .type = type, .constant{ .i = 0 }
    };
}

static hsm_t::condition_action do_affect(hsm_t::condition_type type,
                                         hsm_t::variable       v1,
                                         i32                   i) noexcept
{
    return hsm_t::condition_action{ .var1 = v1,
                                    .var2 = hsm_t::variable::constant_i,
                                    .type = type,
                                    .constant{ .i = i } };
}

static hsm_t::condition_action do_affect(hsm_t::condition_type type,
                                         hsm_t::variable       v1,
                                         float                 f) noexcept
{
    return hsm_t::condition_action{ .var1 = v1,
                                    .var2 = hsm_t::variable::constant_r,
                                    .type = type,
                                    .constant{ .f = f } };
}

void hierarchical_state_machine::condition_action::set_equal_to(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_equal_to(variable v1,
                                                                i32 i) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_equal_to(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::set_not_equal_to(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_not_equal_to(
  variable v1,
  i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_not_equal_to(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::set_greater(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater, v1, v2);
}

void hierarchical_state_machine::condition_action::set_greater(variable v1,
                                                               i32 i) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater, v1, i);
}

void hierarchical_state_machine::condition_action::set_greater(variable v1,
                                                               float f) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater, v1, f);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater_equal, v1, v2);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater_equal, v1, i);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::greater_equal, v1, f);
}

void hierarchical_state_machine::condition_action::set_less(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::less, v1, v2);
}

void hierarchical_state_machine::condition_action::set_less(variable v1,
                                                            i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::less, v1, i);
}

void hierarchical_state_machine::condition_action::set_less(variable v1,
                                                            float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::less, v1, f);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::less_equal, v1, v2);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::less_equal, v1, i);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::less_equal, v1, f);
}

void hierarchical_state_machine::condition_action::clear() noexcept
{
    *this = do_affect(condition_type::none, variable::none, variable::none);
}

status hierarchical_state_machine::start(execution&       exec,
                                         external_source& srcs) noexcept
{
    if (states.empty() or top_state == invalid_state_id)
        return make_error(simulation_errc::hsm_top_state_error);

    exec.current_state = top_state;
    exec.next_state    = invalid_state_id;
    exec.messages      = 0;

    irt_check(handle(exec.current_state, event_type::enter, exec, srcs));

    while ((exec.next_state = states[exec.current_state].sub_id) !=
           invalid_state_id) {
        irt_check(on_enter_sub_state(exec, srcs));
    }

    if (exec.message_overflow)
        return make_error(simulation_errc::hsm_output_overflow_error);

    return success();
}

expected<bool> hierarchical_state_machine::dispatch(
  const event_type event,
  execution&       exec,
  external_source& srcs) noexcept
{
    debug::ensure(not is_dispatching(exec));

    auto sid = exec.current_state;

    // A time event belongs to the state which armed the timer: it does not
    // bubble to the super states.
    if (event == event_type::wake_up) {
        for (auto d = depth_of(sid); sid != invalid_state_id;
             sid    = states[sid].super_id, --d)
            if (d < max_depth and exec.timers[d] <= 0.0)
                break;
    }

    auto is_consumed = false;
    while (sid != invalid_state_id) {
        exec.current_source_state = sid;

        auto ret = handle(sid, event, exec, srcs);
        if (not ret) {
            exec.current_source_state = invalid_state_id;
            return ret.error();
        }

        if (*ret) {
            is_consumed = true;
            break;
        }

        sid = event == event_type::wake_up ? invalid_state_id
                                           : states[sid].super_id;
    }

    exec.current_source_state = invalid_state_id;

    if (is_consumed and exec.next_state != invalid_state_id) {
        do {
            irt_check(on_enter_sub_state(exec, srcs));
        } while ((exec.next_state = states[exec.current_state].sub_id) !=
                 invalid_state_id);
    }

    if (exec.message_overflow)
        return make_error(simulation_errc::hsm_output_overflow_error);

    return is_consumed;
}

static bool have_completion(const hsm_t::state& s) noexcept
{
    return s.condition.type != hsm_t::condition_type::port and
           s.condition.type != hsm_t::condition_type::sigma;
}

status hierarchical_state_machine::run_to_completion(
  execution&       exec,
  bool             input_received,
  external_source& srcs) noexcept
{
    const auto can_recheck = [this](const state_id id) noexcept {
        return have_completion(states[id]) and not states[id].is_terminal();
    };

    // The completion event of a state is generated once, when the state is
    // entered: a terminal state at the beginning already sent its own. The
    // condition of a state already entered is checked again last, because the
    // timers and the message can change the variables it reads.
    auto entered = false;
    auto recheck = can_recheck(exec.current_state);

    // The cycles with a guard or with null timers are not found by
    // find_error: the number of events stops them.
    auto steps = 0;

    for (;;) {
        const auto before   = exec.current_state;
        exec.previous_state = before;

        if (entered) {
            entered = false;
            irt_check(dispatch(event_type::internal, exec, srcs));
        } else if (exec.next_timer() <= 0.0) {
            irt_check(dispatch(event_type::wake_up, exec, srcs));
            recheck = can_recheck(exec.current_state);
        } else if (input_received) {
            input_received = false;
            if (exec.values.any())
                irt_check(dispatch(event_type::input_changed, exec, srcs));
            recheck = can_recheck(exec.current_state);
        } else if (recheck) {
            recheck = false;
            irt_check(dispatch(event_type::internal, exec, srcs));
        } else {
            break;
        }

        if (++steps > max_run_to_completion_steps)
            return make_error(simulation_errc::hsm_run_to_completion_error);

        if (exec.current_state != before) {
            entered = have_completion(states[exec.current_state]);
            recheck = false;
        }
    }

    return success();
}

status hierarchical_state_machine::on_enter_sub_state(
  execution&       exec,
  external_source& srcs) noexcept
{
    if (exec.next_state == invalid_state_id)
        return make_error(simulation_errc::hsm_next_state_error);

    small_vector<state_id, max_number_of_state> entry_path;
    for (state_id sid = exec.next_state; sid != exec.current_state;) {
        auto& state = states[sid];

        entry_path.emplace_back(sid);
        sid = state.super_id;

        if (sid == invalid_state_id)
            return make_error(simulation_errc::hsm_next_state_error);
    }

    while (!entry_path.empty()) {
        exec.disallow_transition = true;

        irt_check(handle(entry_path.back(), event_type::enter, exec, srcs));

        exec.disallow_transition = false;
        entry_path.pop_back();
    }

    exec.current_state = exec.next_state;
    exec.next_state    = invalid_state_id;

    return success();
}

status hierarchical_state_machine::transition(state_id            target,
                                              const state_action& effect,
                                              execution&          exec,
                                              external_source&    srcs) noexcept
{
    debug::ensure(target < max_number_of_state);
    debug::ensure(exec.disallow_transition == false);
    debug::ensure(is_dispatching(exec));

    if (exec.disallow_transition)
        return success();

    const auto lca = find_lca(exec.current_source_state, target);
    if (lca == invalid_state_id)
        return make_error(simulation_errc::hsm_transition_error);

    exec.source_state        = exec.current_source_state;
    exec.disallow_transition = true;

    for (auto sid = exec.current_state; sid != lca; sid = states[sid].super_id)
        irt_check(handle(sid, event_type::exit, exec, srcs));

    exec.disallow_transition = false;

    affect_action(effect, exec);

    exec.current_state = lca;
    exec.next_state    = target;

    return success();
}

status hierarchical_state_machine::set_state(state_id id,
                                             state_id super_id,
                                             state_id sub_id) noexcept
{
    if (super_id == invalid_state_id) {
        if (top_state != invalid_state_id and top_state != id)
            return make_error(simulation_errc::hsm_top_state_error);
        top_state = id;
    }

    states[id].super_id = super_id;
    states[id].sub_id   = sub_id;

    if (!((super_id == invalid_state_id ||
           states[super_id].sub_id != invalid_state_id)))
        return make_error(simulation_errc::hsm_top_state_error);

    return success();
}

void hierarchical_state_machine::clear_state(state_id id) noexcept
{
    states[id].enter_action.clear();
    states[id].exit_action.clear();
    states[id].if_action.clear();
    states[id].else_action.clear();
    states[id].condition.clear();

    states[id].if_transition   = invalid_state_id;
    states[id].else_transition = invalid_state_id;
    states[id].super_id        = invalid_state_id;
    states[id].sub_id          = invalid_state_id;
}

bool hierarchical_state_machine::is_in_state(execution& exec,
                                             state_id   id) const noexcept
{
    state_id sid = exec.current_state;

    while (sid != invalid_state_id) {
        if (sid == id)
            return true;

        sid = states[sid].super_id;
    }

    return false;
}

expected<bool> hierarchical_state_machine::handle(
  const state_id   state,
  const event_type event,
  execution&       exec,
  external_source& srcs) noexcept
{
    if (is_using_source()) { /* If this HSM use an external source and if the
                                underlying buffer is empty, we update the
                                buffer. This function can only eat one value
                                from the buffer of the source. */
        if (exec.source_value.is_empty())
            irt_check(srcs.update_source(exec.source_value, exec.src_data));
    }

    const auto& s     = states[state];
    const auto  depth = depth_of(state);

    // A branch without target is an UML internal transition: its action runs
    // without exit nor enter. A completion event needs a target, otherwise a
    // state without transition would consume its completion forever.
    const auto fire = [&](const state_id target, const state_action& action,
                          const bool is_completion) -> expected<bool> {
        if (target != invalid_state_id) {
            irt_check(transition(target, action, exec, srcs));
            return true;
        }

        if (is_completion or action.type == action_type::none)
            return false;

        affect_action(action, exec);
        return true;
    };

    switch (event) {
    case event_type::enter:
        affect_action(s.enter_action, exec);
        if (s.condition.type == condition_type::sigma and depth < max_depth)
            exec.timers[depth] = exec.timer;
        return true;

    case event_type::exit:
        affect_action(s.exit_action, exec);
        if (depth < max_depth)
            exec.timers[depth] = time_domain<time>::infinity;
        return true;

    case event_type::internal:
        if (not have_completion(s))
            return false;

        return s.condition.check(constants, exec)
                 ? fire(s.if_transition, s.if_action, true)
                 : fire(s.else_transition, s.else_action, true);

    case event_type::input_changed:
        if (s.condition.type == condition_type::sigma) {
            if (depth < max_depth)
                exec.timer = exec.timers[depth];
            return fire(s.else_transition, s.else_action, false);
        }

        if (s.condition.type != condition_type::port)
            return false;

        if (s.condition.check(constants, exec)) {
            if (s.if_transition == invalid_state_id and
                s.if_action.type == action_type::none)
                return false;

            exec.values.reset();
            return fire(s.if_transition, s.if_action, false);
        }

        return fire(s.else_transition, s.else_action, false);

    case event_type::wake_up:
        if (s.condition.type != condition_type::sigma)
            return false;

        exec.timer = time_domain<time>::zero;
        if (depth < max_depth)
            exec.timers[depth] = time_domain<time>::infinity;
        return fire(s.if_transition, s.if_action, false);
    }

    unreachable();
}

void hierarchical_state_machine::affect_action(const state_action& action,
                                               execution&          e) noexcept
{
    switch (action.type) {
    case action_type::none:
        break;

    case action_type::set: {
        debug::ensure(1 <= ordinal(action.var1) and ordinal(action.var1) <= 4);
        // @c execution::values stores the port bits in big endian order:
        // bit 3 is @c variable::port_0 and bit 0 is @c variable::port_3.
        const u8 bit = 4u - ordinal(action.var1);
        e.values |= static_cast<u8>(1u << bit);
    } break;

    case action_type::unset: {
        debug::ensure(1 <= ordinal(action.var1) and ordinal(action.var1) <= 4);
        const u8 bit = 4u - ordinal(action.var1);
        e.values &= static_cast<u8>(~(1u << bit));
    } break;

    case action_type::reset:
        e.values = static_cast<u8>(0u);
        break;

    case action_type::output: {
        debug::ensure(0 <= e.messages);
        debug::ensure(1 <= ordinal(action.var1) and ordinal(action.var1) <= 4);

        const u8 port = ordinal(action.var1) - 1u;

        switch (action.var2) {
        case variable::port_0:
            e.push_message(e.ports[3], port);
            break;
        case variable::port_1:
            e.push_message(e.ports[2], port);
            break;
        case variable::port_2:
            e.push_message(e.ports[1], port);
            break;
        case variable::port_3:
            e.push_message(e.ports[0], port);
            break;

        case variable::var_i1:
            e.push_message(static_cast<real>(e.i1), port);
            break;
        case variable::var_i2:
            e.push_message(static_cast<real>(e.i2), port);
            break;
        case variable::var_r1:
            e.push_message(e.r1, port);
            break;
        case variable::var_r2:
            e.push_message(e.r2, port);
            break;
        case variable::var_timer:
            e.push_message(e.timer, port);
            break;
        case variable::constant_i:
            e.push_message(static_cast<real>(action.constant.i), port);
            break;
        case variable::constant_r:
            e.push_message(action.constant.f, port);
            break;
        case variable::hsm_constant_0:
        case variable::hsm_constant_1:
        case variable::hsm_constant_2:
        case variable::hsm_constant_3:
        case variable::hsm_constant_4:
        case variable::hsm_constant_5:
        case variable::hsm_constant_6:
        case variable::hsm_constant_7:
            e.push_message(constants[ordinal(action.var2) -
                                     ordinal(variable::hsm_constant_0)],
                           port);
            break;
        default:
            irt::unreachable();
            break;
        }
    } break;

    case action_type::affect:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 = copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 = copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 = copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer = copy_to_real(constants, action.var2, action, e);
            break;
        default:
            irt::unreachable();
        }
        break;

    case action_type::plus:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 += copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 += copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 += copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 += copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer += copy_to_real(constants, action.var2, action, e);
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::minus:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 -= copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 -= copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 -= copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 -= copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer -= copy_to_real(constants, action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::negate:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = -1 * copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 = -1 * copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 = -1.0 * copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 = -1.0 * copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer = -1.0 * copy_to_real(constants, action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::multiplies:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 *= copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 *= copy_to_i32(constants, action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 *= copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 *= copy_to_real(constants, action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer *= copy_to_real(constants, action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::divides:
        switch (action.var1) {
        case variable::var_i1: {
            if (auto val = copy_to_i32(constants, action.var2, action, e); val)
                e.i1 /= val;
            else
                e.i1 = INT32_MAX;
        } break;
        case variable::var_i2: {
            if (auto val = copy_to_i32(constants, action.var2, action, e); val)
                e.i2 /= val;
            else
                e.i2 = INT32_MAX;
        } break;
        case variable::var_r1: {
            if (auto val = copy_to_real(constants, action.var2, action, e); val)
                e.r1 /= val;
            else
                e.r1 = std::numeric_limits<double>::infinity();
        } break;
        case variable::var_r2: {
            if (auto val = copy_to_real(constants, action.var2, action, e); val)
                e.r2 /= val;
            else
                e.r2 = std::numeric_limits<double>::infinity();
        } break;
        case variable::var_timer: {
            if (auto val = copy_to_real(constants, action.var2, action, e); val)
                e.timer /= val;
            else
                e.timer = std::numeric_limits<double>::infinity();

            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
        } break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::modulus:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = std::modulus<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::modulus<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_and:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = std::bit_and<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_and<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        default:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_or:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = std::bit_or<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_or<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        default:
            irt::unreachable();
        }
        break;

    case action_type::bit_not:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 =
              std::bit_not<>{}(copy_to_i32(constants, action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 =
              std::bit_not<>{}(copy_to_i32(constants, action.var2, action, e));
            break;
        default:
            irt::unreachable();
        }
        break;

    case action_type::bit_xor:
        switch (action.var1) {
        case variable::var_i1:
            e.i1 = std::bit_xor<>{}(
              e.i1, copy_to_i32(constants, action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_xor<>{}(
              e.i2, copy_to_i32(constants, action.var2, action, e));
            break;
        default:
            irt::unreachable();
        }
        break;

    default:
        unreachable();
    }
}

bool hierarchical_state_machine::condition_action::check(
  const std::span<const real, hsm_t::max_constants> constants,
  hierarchical_state_machine::execution&            e) const noexcept
{
    switch (type) {
    case condition_type::none:
        return true;

    case condition_type::port: {
        const auto [p, m] = get_bitset();

        return ((e.values ^ p) & m).flip().all();
    }

    case condition_type::sigma:
        return e.timer <= 0.0;

    case condition_type::equal_to:
        return wrap_var(constants, var1, *this, e) ==
               wrap_var(constants, var2, *this, e);

    case condition_type::not_equal_to:
        return wrap_var(constants, var1, *this, e) !=
               wrap_var(constants, var2, *this, e);

    case condition_type::greater:
        return wrap_var(constants, var1, *this, e) >
               wrap_var(constants, var2, *this, e);

    case condition_type::greater_equal:
        return wrap_var(constants, var1, *this, e) >=
               wrap_var(constants, var2, *this, e);

    case condition_type::less:
        return wrap_var(constants, var1, *this, e) <
               wrap_var(constants, var2, *this, e);

    case condition_type::less_equal:
        return wrap_var(constants, var1, *this, e) <=
               wrap_var(constants, var2, *this, e);
    }

    unreachable();

    return false;
}

auto hierarchical_state_machine::find_lca(state_id source,
                                          state_id target) const noexcept
  -> state_id
{
    for (auto s = states[source].super_id; s != invalid_state_id;
         s      = states[s].super_id)
        for (auto t = states[target].super_id; t != invalid_state_id;
             t      = states[t].super_id)
            if (s == t)
                return s;

    return invalid_state_id;
}

int hierarchical_state_machine::depth_of(state_id id) const noexcept
{
    auto depth = 0;

    // The bound protects from a cycle in a machine not yet checked.
    for (auto s = states[id].super_id;
         s != invalid_state_id and depth < max_number_of_state;
         s = states[s].super_id)
        ++depth;

    return depth;
}

static simulation_errc find_completion_cycle(const hsm_t& hsm) noexcept
{
    using state_id = hsm_t::state_id;

    constexpr auto max_number_of_state = hsm_t::max_number_of_state;
    constexpr auto invalid_state_id    = hsm_t::invalid_state_id;

    const auto& states = hsm.states;

    // A leaf with a completion event has at most one certain completion
    // transition, the first one without guard from the leaf to the top: these
    // transitions form a functional graph, whose cycles are found in linear
    // time (find_error runs at each frame of the editor).
    std::array<state_id, max_number_of_state> next;
    next.fill(invalid_state_id);

    for (int i = 0; i != max_number_of_state; ++i) {
        const auto id = static_cast<state_id>(i);

        if ((id != hsm.top_state and states[i].super_id == invalid_state_id) or
            states[i].sub_id != invalid_state_id or
            not have_completion(states[i]))
            continue;

        for (auto s = id; s != invalid_state_id; s = states[s].super_id) {
            const auto& st = states[s];

            if (not have_completion(st))
                continue;

            if (st.condition.type != hsm_t::condition_type::none)
                break;

            if (st.if_transition == invalid_state_id)
                continue;

            auto target = st.if_transition;
            while (states[target].sub_id != invalid_state_id)
                target = states[target].sub_id;

            if (have_completion(states[target]))
                next[i] = target;

            break;
        }
    }

    std::array<state_id, max_number_of_state> visited_from;
    visited_from.fill(invalid_state_id);

    for (int i = 0; i != max_number_of_state; ++i) {
        const auto start = static_cast<state_id>(i);
        auto       s     = start;

        while (s != invalid_state_id and visited_from[s] == invalid_state_id) {
            visited_from[s] = start;
            s               = next[s];
        }

        if (s != invalid_state_id and visited_from[s] == start)
            return simulation_errc::hsm_completion_cycle_error;
    }

    return simulation_errc::none;
}

simulation_errc hierarchical_state_machine::find_error() const noexcept
{
    const auto is_valid = [](const state_id id) noexcept {
        return id < max_number_of_state;
    };

    if (not is_valid(top_state) or
        states[top_state].super_id != invalid_state_id)
        return simulation_errc::hsm_top_state_error;

    const auto is_used = [&](const state_id id) noexcept {
        return id == top_state or states[id].super_id != invalid_state_id;
    };

    for (int i = 0; i != max_number_of_state; ++i) {
        const auto  id = static_cast<state_id>(i);
        const auto& s  = states[i];

        if (not is_used(id))
            continue;

        auto depth = 0;
        for (auto p = id; p != top_state; p = states[p].super_id)
            if (not is_valid(states[p].super_id) or ++depth >= max_depth)
                return simulation_errc::hsm_hierarchy_error;

        if (s.super_id != invalid_state_id) {
            const auto first = states[s.super_id].sub_id;
            if (not is_valid(first) or states[first].super_id != s.super_id)
                return simulation_errc::hsm_initial_state_error;
        }

        if (s.sub_id != invalid_state_id and
            (not is_valid(s.sub_id) or states[s.sub_id].super_id != id))
            return simulation_errc::hsm_initial_state_error;

        for (const auto t : { s.if_transition, s.else_transition })
            if (t != invalid_state_id and (id == top_state or t == top_state or
                                           not is_valid(t) or not is_used(t)))
                return simulation_errc::hsm_transition_error;
    }

    return find_completion_cycle(*this);
}

} // namespace irt
