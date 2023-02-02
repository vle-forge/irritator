// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

hierarchical_state_machine::hierarchical_state_machine(
  const hierarchical_state_machine& other) noexcept
  : states(other.states)
  , outputs(other.outputs)
  , a(other.a)
  , b(other.b)
  , values(other.values)
  , m_current_state(other.m_current_state)
  , m_next_state(other.m_next_state)
  , m_source_state(other.m_source_state)
  , m_current_source_state(other.m_current_source_state)
  , m_top_state(other.m_top_state)
  , m_disallow_transition(other.m_disallow_transition)
{
}

hierarchical_state_machine& hierarchical_state_machine::operator=(
  const hierarchical_state_machine& other) noexcept
{
    if (this != &other) {
        states                 = other.states;
        outputs                = other.outputs;
        a                      = other.a;
        b                      = other.b;
        values                 = other.values;
        m_current_state        = other.m_current_state;
        m_next_state           = other.m_next_state;
        m_source_state         = other.m_source_state;
        m_current_source_state = other.m_current_source_state;
        m_top_state            = other.m_top_state;
        m_disallow_transition  = other.m_disallow_transition;
    }

    return *this;
}

bool hierarchical_state_machine::is_dispatching() const noexcept
{
    return m_current_source_state != invalid_state_id;
}

typename hierarchical_state_machine::state_id
hierarchical_state_machine::get_current_state() const noexcept
{
    return m_current_state;
}

typename hierarchical_state_machine::state_id
hierarchical_state_machine::get_source_state() const noexcept
{
    return m_source_state;
}

void hierarchical_state_machine::state_action::clear() noexcept
{
    parameter = 0;
    var1      = variable::none;
    var2      = variable::none;
    type      = action_type::none;
}

void hierarchical_state_machine::condition_action::clear() noexcept
{
    parameter = 0;
    type      = condition_type::none;
    port      = 0;
    mask      = 0;
}

status hierarchical_state_machine::start() noexcept
{
    if (states.empty())
        return status::success;

    irt_return_if_fail(m_top_state != invalid_state_id,
                       status::model_hsm_bad_top_state);

    m_current_state = m_top_state;
    m_next_state    = invalid_state_id;

    handle(m_current_state, event_type::enter);

    while ((m_next_state = states[m_current_state].sub_id) !=
           invalid_state_id) {
        irt_return_if_bad(on_enter_sub_state());
    }

    return status::success;
}

void hierarchical_state_machine::clear() noexcept
{
    values = 0u;
    outputs.clear();

    m_top_state            = invalid_state_id;
    m_current_state        = invalid_state_id;
    m_next_state           = invalid_state_id;
    m_source_state         = invalid_state_id;
    m_current_source_state = invalid_state_id;
    m_disallow_transition  = false;
}

std::pair<status, bool> hierarchical_state_machine::dispatch(
  const event_type event) noexcept
{
    irt_assert(!is_dispatching());

    bool is_processed = false;

    for (state_id sid = m_current_state; sid != invalid_state_id;) {
        auto& state            = states[sid];
        m_current_source_state = sid;

        if (handle(sid, event)) {
            if (m_next_state != invalid_state_id) {
                do {
                    if (auto ret = on_enter_sub_state(); is_bad(ret))
                        return { ret, false };
                } while ((m_next_state = states[m_current_state].sub_id) !=
                         invalid_state_id);
            }
            is_processed = true;
            break;
        }

        sid = state.super_id;
    }
    m_current_source_state = invalid_state_id;

    return { status::success, is_processed };
}

status hierarchical_state_machine::on_enter_sub_state() noexcept
{
    irt_return_if_fail(m_next_state != invalid_state_id,
                       status::model_hsm_bad_next_state);

    small_vector<state_id, max_number_of_state> entry_path;
    for (state_id sid = m_next_state; sid != m_current_state;) {
        auto& state = states[sid];

        entry_path.emplace_back(sid);
        sid = state.super_id;

        irt_return_if_fail(sid != invalid_state_id,
                           status::model_hsm_bad_next_state);
    }

    while (!entry_path.empty()) {
        m_disallow_transition = true;

        handle(entry_path.back(), event_type::enter);

        m_disallow_transition = false;
        entry_path.pop_back();
    }

    m_current_state = m_next_state;
    m_next_state    = invalid_state_id;

    return status::success;
}

void hierarchical_state_machine::transition(state_id target) noexcept
{
    irt_assert(target < max_number_of_state);
    irt_assert(m_disallow_transition == false);
    irt_assert(is_dispatching());

    if (m_disallow_transition)
        return;

    if (m_current_source_state != invalid_state_id)
        m_source_state = m_current_source_state;

    m_disallow_transition = true;
    state_id sid;
    for (sid = m_current_state; sid != m_source_state;) {
        handle(sid, event_type::exit);
        sid = states[sid].super_id;
    }

    int stepsToCommonRoot = steps_to_common_root(m_source_state, target);
    irt_assert(stepsToCommonRoot >= 0);

    while (stepsToCommonRoot--) {
        handle(sid, event_type::exit);
        sid = states[sid].super_id;
    }

    m_disallow_transition = false;

    m_current_state = sid;
    m_next_state    = target;
}

status hierarchical_state_machine::set_state(state_id id,
                                             state_id super_id,
                                             state_id sub_id) noexcept
{
    if (super_id == invalid_state_id) {
        irt_return_if_fail(m_top_state == invalid_state_id,
                           status::model_hsm_bad_top_state);
        m_top_state = id;
    }

    states[id].super_id = super_id;
    states[id].sub_id   = sub_id;

    irt_return_if_fail((super_id == invalid_state_id ||
                        states[super_id].sub_id != invalid_state_id),
                       status::model_hsm_bad_top_state);

    return status::success;
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

bool hierarchical_state_machine::is_in_state(state_id id) const noexcept
{
    state_id sid = m_current_state;

    while (sid != invalid_state_id) {
        if (sid == id)
            return true;

        sid = states[sid].super_id;
    }

    return false;
}

bool hierarchical_state_machine::handle(const state_id   state,
                                        const event_type event) noexcept
{
    switch (event) {
    case event_type::enter:
        affect_action(states[state].enter_action);
        return true;

    case event_type::exit:
        affect_action(states[state].exit_action);
        return true;

    case event_type::input_changed:
        if (states[state].condition.check(values, a, b)) {
            affect_action(states[state].if_action);
            if (states[state].if_transition != invalid_state_id)
                transition(states[state].if_transition);
            return true;
        } else {
            affect_action(states[state].else_action);
            if (states[state].else_transition != invalid_state_id)
                transition(states[state].else_transition);
            return true;
        }
        return false;

    default:
        break;
    }

    irt_unreachable();
}

void hierarchical_state_machine::affect_action(
  const state_action& action) noexcept
{
    i32 param = action.parameter;
    u8  port  = 255; // @TODO better code need

    if (action.var1 >= variable::port_0 && action.var1 <= variable::port_3)
        port = ordinal(action.var1) - 1; // @TODO better code need

    i32* var_1 = action.var1 == variable::var_a   ? &a
                 : action.var1 == variable::var_b ? &b
                                                  : nullptr;

    i32* var_2 = action.var2 == variable::var_a      ? &a
                 : action.var2 == variable::var_b    ? &b
                 : action.var2 == variable::constant ? &param
                                                     : nullptr;

    switch (action.type) {
    case action_type::none:
        break;
    case action_type::set:
        irt_assert(port >= 0 && port <= 3);
        values |= static_cast<u8>(1u << port);
        break;
    case action_type::unset:
        irt_assert(port >= 0 && port <= 3);
        values &= static_cast<u8>(~(1u << port));
        break;
    case action_type::reset:
        values = static_cast<u8>(0u);
        break;
    case action_type::output:
        irt_assert(port >= 0 && port <= 3);
        outputs.emplace_back(hierarchical_state_machine::output_message{
          .port = port, .value = action.parameter });
        break;
    case action_type::affect:
        irt_assert(var_1 && var_2);
        *var_1 = *var_2;
        break;
    case action_type::plus:
        irt_assert(var_1 && var_2);
        *var_1 = std::plus<>{}(*var_1, *var_2);
        break;
    case action_type::minus:
        irt_assert(var_1 && var_2);
        *var_1 = std::minus<>{}(*var_1, *var_2);
        break;
    case action_type::negate:
        irt_assert(var_1 && var_2);
        *var_1 = std::negate<>{}(*var_1);
        break;
    case action_type::multiplies:
        irt_assert(var_1 && var_2);
        *var_1 = std::multiplies<>{}(*var_1, *var_2);
        break;
    case action_type::divides:
        irt_assert(var_1 && var_2);
        *var_1 = std::divides<>{}(*var_1, *var_2);
        break;
    case action_type::modulus:
        irt_assert(var_1 && var_2);
        *var_1 = std::modulus<>{}(*var_1, *var_2);
        break;
    case action_type::bit_and:
        irt_assert(var_1 && var_2);
        *var_1 = std::bit_and<>{}(*var_1, *var_2);
        break;
    case action_type::bit_or:
        irt_assert(var_1 && var_2);
        *var_1 = std::bit_or<>{}(*var_1, *var_2);
        break;
    case action_type::bit_not:
        irt_assert(var_1 && var_2);
        *var_1 = std::bit_not<>{}(*var_1);
        break;
    case action_type::bit_xor:
        irt_assert(var_1 && var_2);
        *var_1 = std::bit_xor<>{}(*var_1, *var_2);
        break;

    default:
        irt_unreachable();
    }
}

bool hierarchical_state_machine::condition_action::check(u8  port_values,
                                                         i32 a,
                                                         i32 b) noexcept
{
    switch (type) {
    case condition_type::none:
        return true;
    case condition_type::port:
        return !((port_values ^ port) & mask);
    case condition_type::a_equal_to_cst:
        return a == parameter;
    case condition_type::a_not_equal_to_cst:
        return a != parameter;
    case condition_type::a_greater_cst:
        return a > parameter;
    case condition_type::a_less_cst:
        return a < parameter;
    case condition_type::a_greater_equal_cst:
        return a >= parameter;
    case condition_type::a_less_equal_cst:
        return a <= parameter;
    case condition_type::b_equal_to_cst:
        return b == parameter;
    case condition_type::b_not_equal_to_cst:
        return b != parameter;
    case condition_type::b_greater_cst:
        return b > parameter;
    case condition_type::b_less_cst:
        return b < parameter;
    case condition_type::b_greater_equal_cst:
        return b >= parameter;
    case condition_type::b_less_equal_cst:
        return b <= parameter;
    case condition_type::a_equal_to_b:
        return a == b;
    case condition_type::a_not_equal_to_b:
        return a != b;
    case condition_type::a_greater_b:
        return a > b;
    case condition_type::a_less_b:
        return a < b;
    case condition_type::a_greater_equal_b:
        return a >= b;
    case condition_type::a_less_equal_b:
        return a <= b;
    }

    irt_unreachable();

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