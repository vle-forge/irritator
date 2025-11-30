// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

using hsm_t = hierarchical_state_machine;

static bool have_if_transition(const hsm_t::state& s) noexcept
{
    return s.if_transition != hsm_t::invalid_state_id;
}

static bool have_else_transition(const hsm_t::state& s) noexcept
{
    return s.else_transition != hsm_t::invalid_state_id;
}

static const char* variable_names[] = {
    "none",           "port_0",         "port_1",         "port_2",
    "port_3",         "variable i1",    "variable i2",    "variable r1",
    "variable r2",    "variable timer", "constant i",     "constant r",
    "hsm constant 0", "hsm constant 1", "hsm constant 2", "hsm constant 3",
    "hsm constant 4", "hsm constant 5", "hsm constant 6", "hsm constant 7",
    "external source"
};

static const char* action_names[] = {
    "none",           "set port",   "unset port", "reset ports",
    "output message", "x = y",      "x = x + y",  "x = x - y",
    "x = -x",         "x = x * y",  "x = x / y",  "x = x % y",
    "x = x and y",    "x = x or y", "x = not x",  "x = x xor y"
};

static const char* condition_names[] = { "none",   "value on port", "timeout",
                                         "x = y",  "x != y",        "x > y",
                                         "x >= y", "x < y",         "x <= y" };

static auto select(hsm_t::variable act, hsm_t::variable cur) noexcept
  -> hsm_t::variable
{
    return ImGui::Selectable(variable_names[ordinal(act)], cur == act) ? act
                                                                       : cur;
}

static auto select_port(hsm_t::variable cur) noexcept -> hsm_t::variable
{
    cur = select(hsm_t::variable::port_0, cur);
    cur = select(hsm_t::variable::port_1, cur);
    cur = select(hsm_t::variable::port_2, cur);
    cur = select(hsm_t::variable::port_3, cur);
    return cur;
}

static auto select_variable(hsm_t::variable cur) noexcept -> hsm_t::variable
{
    cur = select(hsm_t::variable::var_i1, cur);
    cur = select(hsm_t::variable::var_i2, cur);
    cur = select(hsm_t::variable::var_r1, cur);
    cur = select(hsm_t::variable::var_r2, cur);
    cur = select(hsm_t::variable::var_timer, cur);

    return cur;
}

static auto select_source_var(hsm_t::variable cur) noexcept -> hsm_t::variable
{
    return select(hsm_t::variable::source, cur);
}

static auto select_local_constant(hsm_t::variable cur) noexcept
  -> hsm_t::variable
{
    cur = select(hsm_t::variable::constant_i, cur);
    cur = select(hsm_t::variable::constant_r, cur);

    return cur;
}

static auto select_hsm_constant(hsm_t::variable cur) noexcept -> hsm_t::variable
{
    cur = select(hsm_t::variable::hsm_constant_0, cur);
    cur = select(hsm_t::variable::hsm_constant_1, cur);
    cur = select(hsm_t::variable::hsm_constant_2, cur);
    cur = select(hsm_t::variable::hsm_constant_3, cur);
    cur = select(hsm_t::variable::hsm_constant_4, cur);
    cur = select(hsm_t::variable::hsm_constant_5, cur);
    cur = select(hsm_t::variable::hsm_constant_6, cur);
    cur = select(hsm_t::variable::hsm_constant_7, cur);

    return cur;
}

static void show_readable_vars(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);
    ImGui::PushItemWidth(-1);

    const char* preview = variable_names[ordinal(act)];
    if (ImGui::BeginCombo("##var", preview)) {
        act = select_variable(act);
        act = select_port(act);
        act = select_source_var(act);
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

template<typename ConditionOrAction>
static void show_all_vars(hsm_t::variable& act, ConditionOrAction& a) noexcept
{
    ImGui::PushID(&act);
    ImGui::PushItemWidth(-1);

    const char* preview = variable_names[ordinal(act)];
    if (ImGui::BeginCombo("##var", preview)) {
        act = select_port(act);
        act = select_variable(act);
        act = select_source_var(act);
        act = select_local_constant(act);
        act = select_hsm_constant(act);
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    if (act == hsm_t::variable::constant_i) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &a.constant.i);
        ImGui::PopItemWidth();
    }

    if (act == hsm_t::variable::constant_r) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_Float, &a.constant.f);
        ImGui::PopItemWidth();
    }

    ImGui::PopID();
}

static void show_affactable_vars(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);
    ImGui::PushItemWidth(-1);

    const char* preview = variable_names[ordinal(act)];

    if (ImGui::BeginCombo("##var", preview)) {
        act = select_variable(act);
        act = select_port(act);
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_port_vars(hsm_t::variable& var) noexcept
{
    ImGui::PushID(&var);

    const char* preview = variable_names[ordinal(var)];

    if (ImGui::BeginCombo("##var", preview)) {
        ImGui::PushItemWidth(-1);
        var = select(hsm_t::variable::port_0, var);
        var = select(hsm_t::variable::port_1, var);
        var = select(hsm_t::variable::port_2, var);
        var = select(hsm_t::variable::port_3, var);
        ImGui::PopItemWidth();
        ImGui::EndCombo();
    }

    ImGui::PopID();
}

static void show_ports(hsm_t::condition_action& p) noexcept
{
    u8 port, mask;
    p.get(port, mask);

    const int sub_value_3 = port & 0b0001 ? 1 : 0;
    const int sub_value_2 = port & 0b0010 ? 1 : 0;
    const int sub_value_1 = port & 0b0100 ? 1 : 0;
    const int sub_value_0 = port & 0b1000 ? 1 : 0;

    int value_3 = mask & 0b0001 ? sub_value_3 : -1;
    int value_2 = mask & 0b0010 ? sub_value_2 : -1;
    int value_1 = mask & 0b0100 ? sub_value_1 : -1;
    int value_0 = mask & 0b1000 ? sub_value_0 : -1;

    bool have_changed = false;

    have_changed = ImGui::CheckBoxTristate("0", &value_0);
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("1", &value_1) || have_changed;
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("2", &value_2) || have_changed;
    ImGui::SameLine();
    have_changed = ImGui::CheckBoxTristate("3", &value_3) || have_changed;

    if (have_changed) {
        port = value_0 == 1 ? 1u : 0u;
        port <<= 1;
        port |= value_1 == 1 ? 1u : 0u;
        port <<= 1;
        port |= value_2 == 1 ? 1u : 0u;
        port <<= 1;
        port |= value_3 == 1 ? 1u : 0u;

        mask = value_0 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_1 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_2 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_3 != -1 ? 1u : 0u;

        p.set(port, mask);
    }
}

static constexpr int make_state(hsm_t::state_id id) noexcept
{
    return static_cast<int>(id);
}

static constexpr hsm_t::state_id get_state(int idx) noexcept
{
    return static_cast<hsm_t::state_id>(idx);
}

enum class transition_type : u8 {
    super_transition = 0b001,
    if_transition    = 0b010,
    else_transition  = 0b100,
};

struct output {
    hsm_t::state_id output;
    transition_type type;
};

struct transition {
    hsm_t::state_id input;
    hsm_t::state_id output;
    transition_type type;
};

static constexpr int make_input(hsm_t::state_id id) noexcept
{
    return static_cast<int>(id << 16);
}

static constexpr hsm_t::state_id get_input(int idx) noexcept
{
    return static_cast<hsm_t::state_id>((idx >> 16) & 0xff);
}

static constexpr int make_output(hsm_t::state_id id,
                                 transition_type type) noexcept
{
    return static_cast<int>((static_cast<u8>(type) << 8) | id);
}

static constexpr output get_output(int idx) noexcept
{
    return output{ .output = static_cast<u8>(idx & 0xff),
                   .type   = static_cast<transition_type>((idx >> 8) & 0xff) };
}

static constexpr int make_transition(hsm_t::state_id from,
                                     hsm_t::state_id to,
                                     transition_type type) noexcept
{
    return make_input(to) | make_output(from, type);
}

static constexpr transition get_transition(int idx) noexcept
{
    const auto input  = get_input(idx);
    const auto output = get_output(idx);

    return transition{ .input  = input,
                       .output = output.output,
                       .type   = output.type };
}

/** Get the first unused state in the HSM. */
static constexpr auto get_first_available(
  std::array<bool, 254>& enabled) noexcept -> std::optional<u8>
{
    for (u8 i = 1, e = static_cast<u8>(enabled.size()); i != e; ++i)
        if (enabled[i] == false)
            return std::make_optional(i);

    return std::nullopt;
}

static void remove_state(hsm_component&  hsm,
                         hsm_t::state_id id,
                         std::span<bool> enabled) noexcept
{
    debug::ensure(id != 0);

    // If id is the first sub-child, we need a new one.
    if (hsm.machine.states[0].sub_id == id) {
        for (u8 i = 1; i != hsm_t::max_number_of_state; ++i) {
            if (i != id and enabled[i]) {
                hsm.machine.states[0].sub_id   = i;
                hsm.machine.states[i].super_id = 0;
            }
        }
    }

    // Remove any reference to the state @c id.
    for (u8 i = 1; i != hsm_t::max_number_of_state; ++i) {
        if (enabled[i]) {
            if (hsm.machine.states[i].super_id == id)
                hsm.machine.states[i].super_id = hsm_t::invalid_state_id;
            if (hsm.machine.states[i].sub_id == id)
                hsm.machine.states[i].sub_id = hsm_t::invalid_state_id;
            if (hsm.machine.states[i].if_transition == id)
                hsm.machine.states[i].if_transition = hsm_t::invalid_state_id;
            if (hsm.machine.states[i].else_transition == id)
                hsm.machine.states[i].else_transition = hsm_t::invalid_state_id;
        }
    }

    hsm.machine.clear_state(id);
    hsm.names[id].clear();
    hsm.positions[id].reset();
    enabled[id] = false;
}

static void remove_link(hsm_component& hsm, transition t) noexcept
{
    switch (t.type) {
    case transition_type::super_transition:
        debug::ensure(t.output == hsm.machine.top_state);
        debug::ensure(hsm.machine.states[t.output].super_id ==
                      hsm_t::invalid_state_id);

        hsm.machine.states[t.output].sub_id  = hsm_t::invalid_state_id;
        hsm.machine.states[t.input].super_id = hsm_t::invalid_state_id;
        break;
    case transition_type::if_transition:
        hsm.machine.states[t.output].if_transition = hsm_t::invalid_state_id;
        break;
    case transition_type::else_transition:
        hsm.machine.states[t.output].else_transition = hsm_t::invalid_state_id;
        break;
    }
}

static void show_condition(hsm_t::condition_action& act) noexcept
{
    ImGui::TextUnformatted(condition_names[ordinal(act.type)]);
}

template<typename ConditionOrAction>
static void display_condition_2_var(ConditionOrAction& act,
                                    std::string_view   op) noexcept
{
    if (act.var2 == hsm_t::variable::constant_i)
        ImGui::TextFormatDisabled(
          "{} {} {}", variable_names[ordinal(act.var1)], op, act.constant.i);
    else if (act.var2 == hsm_t::variable::constant_r)
        ImGui::TextFormatDisabled(
          "{} {} {}", variable_names[ordinal(act.var1)], op, act.constant.f);
    else
        ImGui::TextFormatDisabled("{} {} {}",
                                  variable_names[ordinal(act.var1)],
                                  op,
                                  variable_names[ordinal(act.var2)]);
}

template<typename ConditionOrAction>
static void display_condition_2_var(ConditionOrAction& act,
                                    std::string_view   op,
                                    std::string_view   section) noexcept
{
    if (act.var2 == hsm_t::variable::constant_i)
        ImGui::TextFormatDisabled("{} {} {} {}",
                                  section,
                                  variable_names[ordinal(act.var1)],
                                  op,
                                  act.constant.i);
    else if (act.var2 == hsm_t::variable::constant_r)
        ImGui::TextFormatDisabled("{} {} {} {}",
                                  section,
                                  variable_names[ordinal(act.var1)],
                                  op,
                                  act.constant.f);
    else
        ImGui::TextFormatDisabled("{} {} {} {}",
                                  section,
                                  variable_names[ordinal(act.var1)],
                                  op,
                                  variable_names[ordinal(act.var2)]);
}

static void display_condition_port(hsm_t::condition_action& act) noexcept
{
    u8 port, mask;
    act.get(port, mask);

    ImGui::TextFormatDisabled("{:b} - {:b}", port, mask);
}

static void display_condition_timer(hsm_t::condition_action& /*act*/) noexcept
{
    ImGui::TextFormatDisabled("waiting R-timer");
}

static void show_complete_condition(hsm_t::condition_action& act) noexcept
{
    switch (act.type) {
    case hsm_t::condition_type::none:
        break;

    case hsm_t::condition_type::port:
        display_condition_port(act);
        break;

    case hsm_t::condition_type::sigma:
        display_condition_timer(act);
        break;

    case hsm_t::condition_type::equal_to:
        display_condition_2_var(act, "=");
        break;

    case hsm_t::condition_type::not_equal_to:
        display_condition_2_var(act, "!=");
        break;

    case hsm_t::condition_type::greater:
        display_condition_2_var(act, ">");
        break;

    case hsm_t::condition_type::greater_equal:
        display_condition_2_var(act, ">=");
        break;

    case hsm_t::condition_type::less:
        display_condition_2_var(act, "<");
        break;

    case hsm_t::condition_type::less_equal:
        display_condition_2_var(act, "<=");
        break;
    }
}

static void display_action(hsm_t::state_action& act,
                           std::string_view     name) noexcept
{
    switch (act.type) {
    case hsm_t::action_type::none:
        break;

    case hsm_t::action_type::set:
        break;

    case hsm_t::action_type::unset:
        break;

    case hsm_t::action_type::reset:
        break;

    case hsm_t::action_type::output:
        display_condition_2_var(act, "output", name);
        break;

    case hsm_t::action_type::affect:
        display_condition_2_var(act, "=", name);
        break;

    case hsm_t::action_type::plus:
        display_condition_2_var(act, "+", name);
        break;

    case hsm_t::action_type::minus:
        display_condition_2_var(act, "-", name);
        break;

    case hsm_t::action_type::negate:
        display_condition_2_var(act, "-", name);
        break;

    case hsm_t::action_type::multiplies:
        display_condition_2_var(act, "*", name);
        break;

    case hsm_t::action_type::divides:
        display_condition_2_var(act, "/", name);
        break;

    case hsm_t::action_type::modulus:
        display_condition_2_var(act, "%", name);
        break;

    case hsm_t::action_type::bit_and:
        display_condition_2_var(act, "bit-and", name);
        break;

    case hsm_t::action_type::bit_or:
        display_condition_2_var(act, "bit-or", name);
        break;

    case hsm_t::action_type::bit_not:
        display_condition_2_var(act, "bit-not", name);
        break;

    case hsm_t::action_type::bit_xor:
        display_condition_2_var(act, "bit-xor", name);
        break;

    default:
        break;
    }
}

static void show_state_action(hsm_t::state_action& action) noexcept
{
    ImGui::PushID(static_cast<void*>(&action));

    int action_type = static_cast<int>(action.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &action_type, action_names, length(action_names))) {
        action.set_default(enum_cast<hsm_t::action_type>(action_type));
    }
    ImGui::PopItemWidth();

    switch (action.type) {
    case hsm_t::action_type::none:
        break;
    case hsm_t::action_type::set:
        show_port_vars(action.var1);
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.constant.i);
        ImGui::PopItemWidth();
        break;
    case hsm_t::action_type::unset:
        show_port_vars(action.var1);
        break;
    case hsm_t::action_type::reset:
        break;
    case hsm_t::action_type::output:
        show_port_vars(action.var1);
        ImGui::PushItemWidth(-1);
        show_all_vars(action.var2, action);
        ImGui::PopItemWidth();
        break;
    case hsm_t::action_type::affect:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::plus:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::minus:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::negate:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::multiplies:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::divides:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::modulus:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::bit_and:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::bit_or:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::bit_not:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    case hsm_t::action_type::bit_xor:
        show_affactable_vars(action.var1);
        show_all_vars(action.var2, action);
        break;
    default:
        break;
    }

    ImGui::PopID();
}

static void show_state_condition(hsm_t::condition_action& condition) noexcept
{
    ImGui::PushID(&condition);

    int type = static_cast<int>(condition.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &type, condition_names, length(condition_names)))
        debug::ensure(0 <= type && type < hsm_t::condition_type_count);
    condition.type = enum_cast<hsm_t::condition_type>(type);
    ImGui::PopItemWidth();

    switch (condition.type) {
    case hsm_t::condition_type::none:
        break;
    case hsm_t::condition_type::port:
        show_ports(condition);
        break;
    case hsm_t::condition_type::sigma:
        break;
    case hsm_t::condition_type::equal_to:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::not_equal_to:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::greater:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::greater_equal:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::less:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::less_equal:
        ImGui::PushItemWidth(-1);
        show_readable_vars(condition.var1);
        show_all_vars(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    }
    ImGui::PopID();
}

void hsm_component_editor_data::clear(hsm_component& hsm) noexcept
{
    m_selected_links.clear();
    m_selected_nodes.clear();
    m_enabled.fill(false);
    m_enabled[0] = true;

    hsm.clear();
}

void hsm_component_editor_data::show_hsm(hsm_component& hsm) noexcept
{
    ImNodes::BeginNode(make_state(0));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("Initial state");
    ImNodes::EndNodeTitleBar();

    ImNodes::BeginOutputAttribute(
      make_output(0, transition_type::super_transition),
      ImNodesPinShape_CircleFilled);
    ImGui::TextUnformatted("start");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    const auto with_details =
      m_options.test(hsm_component_editor_data::display_action_label);

    for (u8 i = 1, e = static_cast<u8>(hsm_t::max_number_of_state); i != e;
         ++i) {
        if (m_enabled[i] == false)
            continue;

        ImNodes::BeginNode(make_state(i));
        ImNodes::BeginNodeTitleBar();
        ImGui::TextFormat("{} (id: {})", hsm.names[i].sv(), i);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(make_input(i),
                                     ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("in");
        ImNodes::EndInputAttribute();

        if (with_details and
            hsm.machine.states[i].enter_action.type != hsm_t::action_type::none)
            display_action(hsm.machine.states[i].enter_action, "on enter");

        show_condition(hsm.machine.states[i].condition);
        if (m_options.test(hsm_component_editor_data::display_condition_label))
            show_complete_condition(hsm.machine.states[i].condition);

        ImNodes::BeginOutputAttribute(
          make_output(i, transition_type::if_transition),
          ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("if condition is valid do");
        ImNodes::EndOutputAttribute();

        if (with_details and
            hsm.machine.states[i].if_action.type != hsm_t::action_type::none)
            display_action(hsm.machine.states[i].if_action, "");

        ImNodes::BeginOutputAttribute(
          make_output(i, transition_type::else_transition),
          ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("Otherwise do");
        ImNodes::EndOutputAttribute();

        if (with_details) {
            if (hsm.machine.states[i].else_action.type !=
                hsm_t::action_type::none)
                display_action(hsm.machine.states[i].else_action, "");
            if (hsm.machine.states[i].exit_action.type !=
                hsm_t::action_type::none)
                display_action(hsm.machine.states[i].exit_action, "on exit");
        }

        ImNodes::EndNode();
    }

    if (hsm.machine.states[0].sub_id != hsm_t::invalid_state_id)
        ImNodes::Link(make_transition(0,
                                      hsm.machine.states[0].sub_id,
                                      transition_type::super_transition),
                      make_output(0, transition_type::super_transition),
                      make_input(hsm.machine.states[0].sub_id));

    for (u8 i = 1, e = static_cast<u8>(hsm_t::max_number_of_state); i != e;
         ++i) {
        if (m_enabled[i] == false)
            continue;

        if (hsm.machine.states[i].if_transition != hsm_t::invalid_state_id)
            ImNodes::Link(make_transition(i,
                                          hsm.machine.states[i].if_transition,
                                          transition_type::if_transition),
                          make_output(i, transition_type::if_transition),
                          make_input(hsm.machine.states[i].if_transition));

        if (hsm.machine.states[i].else_transition != hsm_t::invalid_state_id)
            ImNodes::Link(make_transition(i,
                                          hsm.machine.states[i].else_transition,
                                          transition_type::else_transition),
                          make_output(i, transition_type::else_transition),
                          make_input(hsm.machine.states[i].else_transition));
    }
}

static void is_link_created(hsm_component& hsm) noexcept
{
    int output_idx, input_idx;
    if (ImNodes::IsLinkCreated(&output_idx, &input_idx)) {
        auto out = get_output(output_idx);
        auto in  = get_input(input_idx);

        switch (out.type) {
        case transition_type::super_transition:
            debug::ensure(out.output == 0);

            if (hsm.machine.states[0].sub_id != hsm_t::invalid_state_id)
                hsm.machine.states[hsm.machine.states[0].sub_id].super_id =
                  hsm_t::invalid_state_id;

            hsm.machine.states[in].super_id       = 0;
            hsm.machine.states[out.output].sub_id = in;
            break;
        case transition_type::if_transition:
            hsm.machine.states[out.output].if_transition = in;
            break;
        case transition_type::else_transition:
            hsm.machine.states[out.output].else_transition = in;
            break;
        }
    }
}

void hsm_component_editor_data::show_menu(hsm_component& hsm) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        if (auto id = get_first_available(m_enabled); id.has_value()) {
            if (ImGui::MenuItem("new state")) {
                m_enabled[id.value()] = true;

                debug::ensure(hsm.machine.top_state == 0);

                if (hsm.machine.states[0].sub_id == hsm_t::invalid_state_id)
                    hsm.machine.states[0].sub_id = id.value();
                (void)hsm.machine.set_state(id.value(), 0);

                hsm.positions[id.value()].x = click_pos.x;
                hsm.positions[id.value()].y = click_pos.y;
                hsm.names[id.value()].clear();
                ImNodes::SetNodeScreenSpacePos(id.value(), click_pos);
            }

            auto action_lbl = m_options.test(display_action_label);
            if (ImGui::MenuItem("Display action labels", nullptr, &action_lbl))
                m_options.set(display_action_label, action_lbl);

            auto condition_lbl = m_options.test(display_condition_label);
            if (ImGui::MenuItem(
                  "Display condition labels", nullptr, &condition_lbl))
                m_options.set(display_condition_label, condition_lbl);
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void hsm_component_editor_data::show_graph(hsm_component& hsm) noexcept
{
    ImNodes::EditorContextSet(m_context);
    ImNodes::BeginNodeEditor();

    const auto is_editor_hovered =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) and
      ImNodes::IsEditorHovered();

    show_menu(hsm);
    show_hsm(hsm);
    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);
    ImNodes::EndNodeEditor();
    is_link_created(hsm);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();

    if (num_selected_nodes == 0) {
        m_selected_nodes.clear();
        ImNodes::ClearNodeSelection();
    }

    if (num_selected_links == 0) {
        m_selected_links.clear();
        ImNodes::ClearLinkSelection();
    }

    if (is_editor_hovered and not ImGui::IsAnyItemHovered()) {
        if (num_selected_nodes > 0) {
            m_selected_nodes.resize(num_selected_nodes, 0);
            ImNodes::GetSelectedNodes(m_selected_nodes.begin());

            if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
                for (auto idx : m_selected_nodes) {
                    if (idx != 0)
                        remove_state(hsm, get_state(idx), m_enabled);
                }
            }
        }

        if (num_selected_links > 0) {
            m_selected_links.resize(num_selected_links, 0);
            ImNodes::GetSelectedLinks(m_selected_links.begin());

            if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
                auto need_clear = false;

                for (auto idx : m_selected_links) {
                    if (idx != 0) {
                        remove_link(hsm, get_transition(idx));
                        need_clear = true;
                    }
                }

                if (need_clear) {
                    m_selected_links.clear();
                    ImNodes::ClearLinkSelection();
                }
            }
        }
    }
}

void hsm_component_editor_data::show_panel(application&   app,
                                           component&     compo,
                                           hsm_component& hsm) noexcept
{
    if (ImGui::CollapsingHeader("constants settings")) {
        ImGui::InputReal("constant 0", &hsm.machine.constants[0]);
        ImGui::InputReal("constant 1", &hsm.machine.constants[1]);
        ImGui::InputReal("constant 2", &hsm.machine.constants[2]);
        ImGui::InputReal("constant 3", &hsm.machine.constants[3]);
        ImGui::InputReal("constant 4", &hsm.machine.constants[4]);
        ImGui::InputReal("constant 5", &hsm.machine.constants[5]);
        ImGui::InputReal("constant 6", &hsm.machine.constants[6]);
        ImGui::InputReal("constant 7", &hsm.machine.constants[7]);
    }

    if (ImGui::CollapsingHeader("External sources")) {
        if (ImGui::Button("Refresh source"))
            hsm.machine.flags.set(hsm_t::option::use_source,
                                  hsm.machine.compute_is_using_source());

        if (hsm.machine.flags[hsm_t::option::use_source]) {
            show_combobox_external_sources(compo.srcs, hsm.src);
        } else {
            ImGui::TextDisabled("HSM does not use external source");
        }
    }

    if (ImGui::CollapsingHeader("selected states",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0, e = m_selected_nodes.Size; i != e; ++i) {
            const auto id    = get_state(m_selected_nodes[i]);
            auto&      state = hsm.machine.states[id];

            if (id == 0)
                continue;

            ImGui::PushID(i);

            ImGui::InputSmallString("Name", hsm.names[id]);
            ImGui::LabelFormat("Id", "{}", static_cast<unsigned>(id));

            ImGui::LabelFormat(
              "super-id",
              "{}",
              static_cast<u32>(hsm.machine.states[id].super_id));
            ImGui::LabelFormat(
              "sub-id", "{}", static_cast<u32>(hsm.machine.states[id].sub_id));

            ImGui::SeparatorText("Condition");
            show_state_condition(state.condition);

            ImGui::SeparatorText("Actions");
            if (ImGui::CollapsingHeader("Enter action"))
                show_state_action(state.enter_action);

            if (ImGui::CollapsingHeader("If condition is true"))
                show_state_action(state.if_action);

            if (ImGui::CollapsingHeader("Else"))
                show_state_action(state.else_action);

            if (ImGui::CollapsingHeader("Exit action"))
                show_state_action(state.exit_action);

            ImGui::PopID();
            ImGui::Separator();
        }
    }
}

bool hsm_component_editor_data::valid(hsm_component& hsm) noexcept
{
    debug::ensure(hsm.machine.states[0].super_id == hsm_t::invalid_state_id);

    small_vector<hsm_t::state_id, max_number_of_state> stack;
    std::array<bool, max_number_of_state>              read;
    read.fill(false);
    read[0] = true;

    debug::ensure(hsm.machine.states[0].if_transition ==
                  hsm_t::invalid_state_id);
    debug::ensure(hsm.machine.states[0].else_transition ==
                  hsm_t::invalid_state_id);
    debug::ensure(hsm.machine.states[0].super_id == hsm_t::invalid_state_id);

    const auto init_s     = hsm.machine.states[0].sub_id;
    auto       have_error = false;

    if (init_s == hsm_t::invalid_state_id) {
        ImGui::LabelFormat("Initiale state", "State machine is empty");
        have_error = true;
    } else {
        stack.emplace_back(hsm.machine.states[0].sub_id);

        while (not stack.empty()) {
            auto top = stack.back();
            stack.pop_back();

            switch (hsm.machine.states[top].condition.type) {
            case hierarchical_state_machine::condition_type::none:
                break;

            case hierarchical_state_machine::condition_type::port:
                if (not have_if_transition(hsm.machine.states[top])) {
                    ImGui::TextFormat("state {}: connect if-condition",
                                      (u32)top);
                    have_error = true;
                }
                break;

            case hierarchical_state_machine::condition_type::sigma:
                if (not have_if_transition(hsm.machine.states[top])) {
                    ImGui::TextFormat("state {}: connect if-condition",
                                      (u32)top);
                    have_error = true;
                }
                break;

            case hierarchical_state_machine::condition_type::equal_to:
            case hierarchical_state_machine::condition_type::not_equal_to:
            case hierarchical_state_machine::condition_type::greater:
            case hierarchical_state_machine::condition_type::greater_equal:
            case hierarchical_state_machine::condition_type::less:
            case hierarchical_state_machine::condition_type::less_equal:
                if (not have_if_transition(hsm.machine.states[top])) {
                    ImGui::TextFormat("state {}: connect if-condition",
                                      (u32)top);
                    have_error = true;
                }
                if (not have_else_transition(hsm.machine.states[top])) {
                    ImGui::TextFormat("state {}: connect else-condition",
                                      (u32)top);
                    have_error = true;
                }
                break;
            }

            read[top] = true;

            if (hsm.machine.states[top].if_transition !=
                  hsm_t::invalid_state_id &&
                read[hsm.machine.states[top].if_transition] == false)
                stack.emplace_back(hsm.machine.states[top].if_transition);
            if (hsm.machine.states[top].else_transition !=
                  hsm_t::invalid_state_id &&
                read[hsm.machine.states[top].else_transition] == false)
                stack.emplace_back(hsm.machine.states[top].else_transition);
        }
    }

    return not have_error and read == m_enabled;
}

void hsm_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* hsm = app.mod.hsm_components.try_to_get(m_hsm_id); hsm) {
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto table_heigth  = region_height -
                                  ImGui::GetFrameHeightWithSpacing() -
                                  ImGui::GetStyle().ItemSpacing.y;

        if (ImGui::BeginChild(
              "##table-editor", ImVec2(0, table_heigth), false)) {
            if (ImGui::BeginTabBar("##hsm-editor")) {
                if (ImGui::BeginTabItem("Editor")) {
                    show_graph(*hsm);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Test")) {
                    if (not valid(*hsm))
                        ImGui::TextFormat("Error in HSM");

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::EndChild();
    }
}

void hsm_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* hsm = app.mod.hsm_components.try_to_get(m_hsm_id); hsm)
        show_panel(app, app.mod.components.get<component>(m_id), *hsm);
}

bool hsm_component_editor_data::need_show_selected_nodes(
  component_editor& /*ed*/) noexcept
{
    return not m_selected_nodes.empty();
}

void hsm_component_editor_data::clear_selected_nodes() noexcept
{
    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();
    m_selected_nodes.clear();
    m_selected_links.clear();
}

void hsm_component_editor_data::store(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* hsm = app.mod.hsm_components.try_to_get(m_hsm_id); hsm) {
        for (auto i = 1, e = length(hsm->machine.states); i != e; ++i) {
            if (m_enabled[i]) {
                const auto pos = ImNodes::GetNodeEditorSpacePos(get_state(i));
                hsm->positions[i].x = pos.x;
                hsm->positions[i].y = pos.y;
            }
        }
    }
}

hsm_component_editor_data::hsm_component_editor_data(
  const component_id     id,
  const hsm_component_id hid,
  hsm_component&         hsm) noexcept
  : m_options(3)
  , m_id(id)
  , m_hsm_id(hid)
{
    m_context = ImNodes::EditorContextCreate();
    ImNodes::EditorContextSet(m_context);
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;

    m_enabled.fill(false);

    hsm.machine.states[0].super_id = hsm_t::invalid_state_id;
    hsm.machine.top_state          = 0;

    for (auto i = 0, e = length(hsm.machine.states); i != e; ++i) {
        if (hsm.machine.states[i].if_transition !=
            hierarchical_state_machine::invalid_state_id) {
            m_enabled[hsm.machine.states[i].if_transition] = true;
            m_enabled[i]                                   = true;
        }
        if (hsm.machine.states[i].else_transition !=
            hierarchical_state_machine::invalid_state_id) {
            m_enabled[hsm.machine.states[i].else_transition] = true;
            m_enabled[i]                                     = true;
        }
        if (hsm.machine.states[i].super_id !=
            hierarchical_state_machine::invalid_state_id) {
            m_enabled[hsm.machine.states[i].super_id] = true;
            m_enabled[i]                              = true;
        }
        if (hsm.machine.states[i].sub_id !=
            hierarchical_state_machine::invalid_state_id) {
            m_enabled[hsm.machine.states[i].sub_id] = true;
            m_enabled[i]                            = true;
        }
    }

    for (auto i = 0, e = length(hsm.machine.states); i != e; ++i)
        if (m_enabled[i])
            ImNodes::SetNodeEditorSpacePos(
              get_state(i), ImVec2(hsm.positions[i].x, hsm.positions[i].y));
}

hsm_component_editor_data::~hsm_component_editor_data() noexcept
{
    if (m_context) {
        ImNodes::EditorContextSet(m_context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(m_context);
    }
}

} // namespace irt
