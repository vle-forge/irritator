// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

hierarchical_state_machine::hierarchical_state_machine(
  const hierarchical_state_machine& other) noexcept
  : states(other.states)
  , top_state(other.top_state)
{}

hierarchical_state_machine& hierarchical_state_machine::operator=(
  const hierarchical_state_machine& other) noexcept
{
    if (this != &other) {
        states    = other.states;
        top_state = other.top_state;
    }

    return *this;
}

bool hierarchical_state_machine::is_dispatching(execution& exec) const noexcept
{
    return exec.current_source_state != invalid_state_id;
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

status hierarchical_state_machine::start(execution& exec) noexcept
{
    if (states.empty())
        return success();

    if (top_state == invalid_state_id)
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
    irt_assert(!is_dispatching(exec));

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
    irt_assert(target < max_number_of_state);
    irt_assert(exec.disallow_transition == false);
    irt_assert(is_dispatching(exec));

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
    irt_assert(stepsToCommonRoot >= 0);

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

    case event_type::input_changed:
        if (states[state].condition.check(exec.values, exec.a, exec.b)) {
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
        return false;

    default:
        break;
    }

    unreachable();
}

void hierarchical_state_machine::affect_action(const state_action& action,
                                               execution& exec) noexcept
{
    i32 param = action.parameter;
    u8  port  = 255; // @TODO better code need

    if (action.var1 >= variable::port_0 && action.var1 <= variable::port_3)
        port = ordinal(action.var1) - 1; // @TODO better code need

    i32* var_1 = action.var1 == variable::var_a   ? &exec.a
                 : action.var1 == variable::var_b ? &exec.b
                                                  : nullptr;

    i32* var_2 = action.var2 == variable::var_a      ? &exec.a
                 : action.var2 == variable::var_b    ? &exec.b
                 : action.var2 == variable::constant ? &param
                                                     : nullptr;

    switch (action.type) {
    case action_type::none:
        break;
    case action_type::set:
        irt_assert(port >= 0 && port <= 3);
        exec.values |= static_cast<u8>(1u << port);
        break;
    case action_type::unset:
        irt_assert(port >= 0 && port <= 3);
        exec.values &= static_cast<u8>(~(1u << port));
        break;
    case action_type::reset:
        exec.values = static_cast<u8>(0u);
        break;
    case action_type::output:
        irt_assert(port >= 0 && port <= 3);
        exec.outputs.emplace_back(hierarchical_state_machine::output_message{
          .value = action.parameter, .port = port });
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
        unreachable();
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
