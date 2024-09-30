// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

using hsm_t = hierarchical_state_machine;

static constexpr i32 copy_to_i32(const hsm_t::variable      v,
                                 const hsm_t::state_action& act,
                                 const hsm_t::execution&    e) noexcept
{
    switch (v) {
    case hsm_t::variable::none:
        return 0;
    case hsm_t::variable::port_0:
        return static_cast<i32>(e.ports[0]);
    case hsm_t::variable::port_1:
        return static_cast<i32>(e.ports[1]);
    case hsm_t::variable::port_2:
        return static_cast<i32>(e.ports[2]);
    case hsm_t::variable::port_3:
        return static_cast<i32>(e.ports[3]);
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
    }

    irt::unreachable();
}

static constexpr real copy_to_real(const hsm_t::variable      v,
                                   const hsm_t::state_action& act,
                                   const hsm_t::execution&    e) noexcept
{
    switch (v) {
    case hsm_t::variable::none:
        return 0.0;
    case hsm_t::variable::port_0:
        return e.ports[0];
    case hsm_t::variable::port_1:
        return e.ports[1];
    case hsm_t::variable::port_2:
        return e.ports[2];
    case hsm_t::variable::port_3:
        return e.ports[3];
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
    }

    irt::unreachable();
}

struct wrap_var {
    union {
        real r;
        i32  i;
    };

    enum type { real_t, integer_t } type;

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
    constexpr wrap_var(const hsm_t::variable   v,
                       Action&                 act,
                       const hsm_t::execution& e) noexcept
    {
        switch (v) {
        case hsm_t::variable::none:
            r    = 0;
            i    = 0;
            type = type::integer_t;
            break;

        case hsm_t::variable::port_0:
            r    = e.ports[0];
            i    = static_cast<i32>(e.ports[0]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_1:
            r    = e.ports[1];
            i    = static_cast<i32>(e.ports[1]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_2:
            r    = e.ports[2];
            i    = static_cast<i32>(e.ports[2]);
            type = type::real_t;
            break;

        case hsm_t::variable::port_3:
            r    = e.ports[3];
            i    = static_cast<i32>(e.ports[3]);
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
        }
    }
};

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

void hierarchical_state_machine::state_action::set_setport(variable v1) noexcept
{
    debug::ensure(is_port(v1));

    if (is_port(v1))
        do_affect(hsm_t::action_type::set, v1);
}

void hierarchical_state_machine::state_action::set_unsetport(
  variable v1) noexcept
{
    debug::ensure(is_port(v1));

    if (is_port(v1))
        do_affect(hsm_t::action_type::unset, v1);
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

                                                          i32 i) noexcept
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

                                                           i32 i) noexcept
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
    *this = do_affect(hsm_t::condition_type::equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_greater(variable v1,
                                                               i32 i) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_greater(variable v1,
                                                               float f) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_greater_equal(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::set_less(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_less(variable v1,
                                                            i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_less(variable v1,
                                                            float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  variable v2) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, v2);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  i32      i) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, i);
}

void hierarchical_state_machine::condition_action::set_less_equal(
  variable v1,
  float    f) noexcept
{
    *this = do_affect(hsm_t::condition_type::not_equal_to, v1, f);
}

void hierarchical_state_machine::condition_action::clear() noexcept
{
    *this = do_affect(condition_type::none, variable::none, variable::none);
}

status hierarchical_state_machine::start(execution& exec) noexcept
{
    exec.timer = time_domain<time>::infinity;

    if (states.empty() or top_state == invalid_state_id)
        return new_error(top_state_error{});

    exec.current_state = top_state;
    exec.next_state    = invalid_state_id;

    handle(exec.current_state, event_type::enter, exec);

    while ((exec.next_state = states[exec.current_state].sub_id) !=
           invalid_state_id) {
        irt_check(on_enter_sub_state(exec));
    }

    return success();
}

result<bool> hierarchical_state_machine::dispatch(const event_type event,
                                                  execution& exec) noexcept
{
    debug::ensure(!is_dispatching(exec));

    bool is_processed = false;

    for (state_id sid = exec.current_state; sid != invalid_state_id;) {
        auto& state               = states[sid];
        exec.current_source_state = sid;

        if (handle(sid, event, exec)) {
            if (exec.next_state != invalid_state_id) {
                do {
                    if (auto ret = on_enter_sub_state(exec); !ret)
                        return ret.error();
                } while (
                  (exec.next_state = states[exec.current_state].sub_id) !=
                  invalid_state_id);
            }
            is_processed = true;
            break;
        }

        sid = state.super_id;
    }

    exec.current_source_state = invalid_state_id;

    return is_processed;
}

status hierarchical_state_machine::on_enter_sub_state(execution& exec) noexcept
{
    if (exec.next_state == invalid_state_id)
        return new_error(next_state_error{});

    small_vector<state_id, max_number_of_state> entry_path;
    for (state_id sid = exec.next_state; sid != exec.current_state;) {
        auto& state = states[sid];

        entry_path.emplace_back(sid);
        sid = state.super_id;

        if (sid == invalid_state_id)
            return new_error(next_state_error{});
    }

    while (!entry_path.empty()) {
        exec.disallow_transition = true;

        handle(entry_path.back(), event_type::enter, exec);

        exec.disallow_transition = false;
        entry_path.pop_back();
    }

    exec.current_state = exec.next_state;
    exec.next_state    = invalid_state_id;

    return success();
}

void hierarchical_state_machine::transition(state_id   target,
                                            execution& exec) noexcept
{
    debug::ensure(target < max_number_of_state);
    debug::ensure(exec.disallow_transition == false);
    debug::ensure(is_dispatching(exec));

    if (exec.disallow_transition)
        return;

    if (exec.current_source_state != invalid_state_id)
        exec.source_state = exec.current_source_state;

    exec.disallow_transition = true;
    state_id sid;
    for (sid = exec.source_state; sid != exec.source_state;) {
        handle(sid, event_type::exit, exec);
        sid = states[sid].super_id;
    }

    int stepsToCommonRoot = steps_to_common_root(exec.source_state, target);
    debug::ensure(stepsToCommonRoot >= 0);

    while (stepsToCommonRoot--) {
        handle(sid, event_type::exit, exec);
        sid = states[sid].super_id;
    }

    exec.disallow_transition = false;

    exec.current_state = sid;
    exec.next_state    = target;
}

status hierarchical_state_machine::set_state(state_id id,
                                             state_id super_id,
                                             state_id sub_id) noexcept
{
    if (super_id == invalid_state_id) {
        if (top_state != invalid_state_id)
            return new_error(top_state_error{});
        top_state = id;
    }

    states[id].super_id = super_id;
    states[id].sub_id   = sub_id;

    if (!((super_id == invalid_state_id ||
           states[super_id].sub_id != invalid_state_id)))
        return new_error(top_state_error{});

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

bool hierarchical_state_machine::handle(const state_id   state,
                                        const event_type event,
                                        execution&       exec) noexcept
{
    switch (event) {
    case event_type::enter:
        affect_action(states[state].enter_action, exec);
        return true;

    case event_type::exit:
        affect_action(states[state].exit_action, exec);
        return true;

    case event_type::internal:
        if (any_equal(states[state].condition.type,
                      condition_type::sigma,
                      condition_type::port))
            return false;

        if (states[state].condition.check(exec)) {
            affect_action(states[state].if_action, exec);
            if (states[state].if_transition != invalid_state_id)
                transition(states[state].if_transition, exec);
            return true;
        } else {
            affect_action(states[state].else_action, exec);
            if (states[state].else_transition != invalid_state_id)
                transition(states[state].else_transition, exec);
            return true;
        }
        break;

    case event_type::input_changed:
        if (states[state].condition.type == condition_type::sigma) {
            affect_action(states[state].else_action, exec);
            if (states[state].else_transition != invalid_state_id)
                transition(states[state].else_transition, exec);
            return true;
        } else {
            if (states[state].condition.check(exec)) {
                affect_action(states[state].if_action, exec);
                if (states[state].if_transition != invalid_state_id)
                    transition(states[state].if_transition, exec);
                return true;
            } else {
                affect_action(states[state].else_action, exec);
                if (states[state].else_transition != invalid_state_id)
                    transition(states[state].else_transition, exec);
                return true;
            }
        }
        break;

    case event_type::wake_up:
        if (states[state].condition.check(exec)) {
            affect_action(states[state].if_action, exec);
            if (states[state].if_transition != invalid_state_id)
                transition(states[state].if_transition, exec);
            return true;
        } else {
            affect_action(states[state].else_action, exec);
            if (states[state].else_transition != invalid_state_id)
                transition(states[state].else_transition, exec);
            return true;
        }

    default:
        break;
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
        const u8 port = ordinal(action.var1) - 1u;
        e.values |= static_cast<u8>(1u << port);
    } break;

    case action_type::unset: {
        debug::ensure(1 <= ordinal(action.var1) and ordinal(action.var1) <= 4);
        const u8 port = ordinal(action.var1) - 1u;
        e.values &= static_cast<u8>(~(1u << port));
    } break;

    case action_type::reset:
        e.values = static_cast<u8>(0u);
        break;

    case action_type::output: {
        // @TODO May returns a buffer full.
        debug::ensure(0 <= e.messages and e.messages < 4);
        debug::ensure(1 <= ordinal(action.var1) and ordinal(action.var1) <= 4);

        const u8 port = ordinal(action.var1) - 1u;

        switch (action.var2) {
        case variable::none:
            irt::unreachable();
            break;

        case variable::port_0:
            e.push_message(e.ports[0], port);
            break;
        case variable::port_1:
            e.push_message(e.ports[1], port);
            break;
        case variable::port_2:
            e.push_message(e.ports[2], port);
            break;
        case variable::port_3:
            e.push_message(e.ports[3], port);
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
        }
    } break;

    case action_type::affect:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = copy_to_i32(action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 = copy_to_i32(action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 = copy_to_real(action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 = copy_to_real(action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer = copy_to_real(action.var2, action, e);
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::plus:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 += copy_to_i32(action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 += copy_to_i32(action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 += copy_to_real(action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 += copy_to_real(action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer += copy_to_real(action.var2, action, e);
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::minus:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 -= copy_to_i32(action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 -= copy_to_i32(action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 -= copy_to_real(action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 -= copy_to_real(action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer -= copy_to_real(action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::negate:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = -1 * copy_to_i32(action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 = -1 * copy_to_i32(action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 = -1.0 * copy_to_real(action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 = -1.0 * copy_to_real(action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer = -1.0 * copy_to_real(action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::multiplies:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 *= copy_to_i32(action.var2, action, e);
            break;
        case variable::var_i2:
            e.i2 *= copy_to_i32(action.var2, action, e);
            break;
        case variable::var_r1:
            e.r1 *= copy_to_real(action.var2, action, e);
            break;
        case variable::var_r2:
            e.r2 *= copy_to_real(action.var2, action, e);
            break;
        case variable::var_timer:
            e.timer *= copy_to_real(action.var2, action, e);
            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::divides:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1: {
            if (auto val = copy_to_i32(action.var2, action, e); val)
                e.i1 /= val;
            else
                e.i1 = INT32_MAX;
        } break;
        case variable::var_i2: {
            if (auto val = copy_to_i32(action.var2, action, e); val)
                e.i2 /= val;
            else
                e.i2 = INT32_MAX;
        } break;
        case variable::var_r1: {
            if (auto val = copy_to_real(action.var2, action, e); val)
                e.r1 /= val;
            else
                e.r1 = std::numeric_limits<double>::infinity();
        } break;
        case variable::var_r2: {
            if (auto val = copy_to_real(action.var2, action, e); val)
                e.r2 /= val;
            else
                e.r2 = std::numeric_limits<double>::infinity();
        } break;
        case variable::var_timer: {
            if (auto val = copy_to_real(action.var2, action, e); val)
                e.timer /= val;
            else
                e.timer = std::numeric_limits<double>::infinity();

            e.timer = e.timer < 0.0 ? 0.0 : e.timer;
        } break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::modulus:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = std::modulus<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::modulus<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_r1:
            irt::unreachable();
            break;
        case variable::var_r2:
            irt::unreachable();
            break;
        case variable::var_timer:
            irt::unreachable();
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_and:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = std::bit_and<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_and<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_r1:
            irt::unreachable();
            break;
        case variable::var_r2:
            irt::unreachable();
            break;
        case variable::var_timer:
            irt::unreachable();
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_or:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = std::bit_or<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_or<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_r1:
            irt::unreachable();
            break;
        case variable::var_r2:
            irt::unreachable();
            break;
        case variable::var_timer:
            irt::unreachable();
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_not:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = std::bit_not<>{}(copy_to_i32(action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_not<>{}(copy_to_i32(action.var2, action, e));
            break;
        case variable::var_r1:
            irt::unreachable();
            break;
        case variable::var_r2:
            irt::unreachable();
            break;
        case variable::var_timer:
            irt::unreachable();
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    case action_type::bit_xor:
        switch (action.var1) {
        case variable::none:
        case variable::port_0:
        case variable::port_1:
        case variable::port_2:
        case variable::port_3:
            unreachable();
            break;

        case variable::var_i1:
            e.i1 = std::bit_xor<>{}(e.i1, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_i2:
            e.i2 = std::bit_xor<>{}(e.i2, copy_to_i32(action.var2, action, e));
            break;
        case variable::var_r1:
            irt::unreachable();
            break;
        case variable::var_r2:
            irt::unreachable();
            break;
        case variable::var_timer:
            irt::unreachable();
            break;
        case variable::constant_i:
            irt::unreachable();
            break;
        case variable::constant_r:
            irt::unreachable();
            break;
        }
        break;

    default:
        unreachable();
    }
}

bool hierarchical_state_machine::condition_action::check(
  hierarchical_state_machine::execution& e) noexcept
{
    switch (type) {
    case condition_type::none:
        return true;

    case condition_type::port: {
        u8 port{}, mask{};
        get(port, mask);
        std::bitset<4> p(port);
        std::bitset<4> m(mask);

        return ((e.values ^ p) & m).flip().any();
    }

    case condition_type::sigma:
        return e.timer <= 0.0;

    case condition_type::equal_to:
        return wrap_var(var1, *this, e) == wrap_var(var2, *this, e);

    case condition_type::not_equal_to:
        return wrap_var(var1, *this, e) != wrap_var(var2, *this, e);

    case condition_type::greater:
        return wrap_var(var1, *this, e) > wrap_var(var2, *this, e);

    case condition_type::greater_equal:
        return wrap_var(var1, *this, e) >= wrap_var(var2, *this, e);

    case condition_type::less:
        return wrap_var(var1, *this, e) < wrap_var(var2, *this, e);

    case condition_type::less_equal:
        return wrap_var(var1, *this, e) <= wrap_var(var2, *this, e);
    }

    unreachable();

    return false;
}

int hierarchical_state_machine::steps_to_common_root(state_id source,
                                                     state_id target) noexcept
{
    if (source == target)
        return 1;

    int tolca = 0;
    for (state_id s = source; s != invalid_state_id; ++tolca) {
        for (state_id t = target; t != invalid_state_id;) {
            if (s == t)
                return tolca;

            t = states[t].super_id;
        }

        s = states[s].super_id;
    }

    return -1;
}

} // namespace irt
