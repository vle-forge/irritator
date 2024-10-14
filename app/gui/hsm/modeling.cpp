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

static const char* variable_names[] = {
    "none",        "port_0",         "port_1",      "port_2",
    "port_3",      "variable i1",    "variable i2", "variable r1",
    "variable r2", "variable timer", "constant i",  "constant r"
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

static const std::string_view test_status_string[] = { "none",
                                                       "being_processed",
                                                       "done",
                                                       "failed" };

/* Display a combobox for input port 1, 2, 3 and 4, variables i1, i2, r1, r1,
 * timer but not integer and real constants */
static void show_ports_variables_combobox(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);

    constexpr auto p0    = (int)ordinal(hsm_t::variable::port_0);
    constexpr auto pn    = (int)ordinal(hsm_t::variable::var_timer);
    constexpr auto nb    = pn - p0 + 1;
    auto           p_act = (int)ordinal(act);

    if (not(p0 <= p_act and p_act <= pn)) {
        act   = enum_cast<hsm_t::variable>(hsm_t::variable::var_i1);
        p_act = p0;
    }

    p_act -= p0;
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &p_act, variable_names + p0, nb)) {
        debug::ensure(0 <= p_act && p_act < nb);
        act = enum_cast<hsm_t::variable>(p_act + p0);
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

/* Display a combobox for input port 1, 2, 3 and 4, variables i1, i2, r1, r1,
 * timer and integer and real constants. */
template<typename ConditionOrAction>
static void show_ports_variables_constants_combobox(
  hsm_t::variable&   act,
  ConditionOrAction& a) noexcept
{
    ImGui::PushID(&act);

    constexpr auto p0    = (int)ordinal(hsm_t::variable::port_0);
    constexpr auto pn    = (int)ordinal(hsm_t::variable::constant_r);
    constexpr auto nb    = pn - p0 + 1;
    auto           p_act = (int)ordinal(act);

    if (not(p0 <= p_act and p_act <= pn)) {
        act   = enum_cast<hsm_t::variable>(hsm_t::variable::var_i1);
        p_act = p0;
    }

    p_act -= p0;
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &p_act, variable_names + p0, nb)) {
        debug::ensure(0 <= p_act && p_act < nb);
        act = enum_cast<hsm_t::variable>(p_act + p0);
    }

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

    ImGui::PopItemWidth();
    ImGui::PopID();
}

/* Display a combobox only for variables i1, i2, r1, r1, timer. */
static void show_variables_combobox(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);

    constexpr auto p0    = (int)ordinal(hsm_t::variable::var_i1);
    constexpr auto pn    = (int)ordinal(hsm_t::variable::var_timer);
    constexpr auto nb    = pn - p0 + 1;
    auto           p_act = (int)ordinal(act);

    if (not(p0 <= p_act and p_act <= pn)) {
        act   = enum_cast<hsm_t::variable>(p0);
        p_act = p0;
    }

    p_act -= p0;
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &p_act, variable_names + p0, nb)) {
        debug::ensure(0 <= p_act && p_act < nb);
        act = enum_cast<hsm_t::variable>(p_act + p0);
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

/* Display a combobox for variables i1, i2, r1, r1, timer but for ports. */
static void show_variables_and_constants_combobox(
  hsm_t::variable&     act,
  hsm_t::state_action& p) noexcept
{
    ImGui::PushID(&act);

    constexpr auto p0    = (int)ordinal(hsm_t::variable::var_i1);
    constexpr auto pn    = (int)ordinal(hsm_t::variable::constant_r);
    constexpr auto nb    = pn - p0 + 1;
    auto           p_act = (int)ordinal(act);

    if (not(p0 <= p_act and p_act <= pn)) {
        act   = enum_cast<hsm_t::variable>(p0);
        p_act = p0;
    }

    p_act -= p0;
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &p_act, variable_names + p0, nb)) {
        debug::ensure(0 <= p_act && p_act < nb);
        act = enum_cast<hsm_t::variable>(p_act + p0);
    }

    if (act == hsm_t::variable::constant_i) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &p.constant.i);
        ImGui::PopItemWidth();
    }

    if (act == hsm_t::variable::constant_r) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_Float, &p.constant.f);
        ImGui::PopItemWidth();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_port_widget(hsm_t::variable& var) noexcept
{
    ImGui::PushID(&var);

    constexpr auto p0    = (int)ordinal(hsm_t::variable::port_0);
    constexpr auto pn    = (int)ordinal(hsm_t::variable::port_3);
    constexpr auto nb    = pn - p0 + 1;
    auto           p_var = (int)ordinal(var);

    if (not(p0 <= p_var and p_var <= pn)) {
        var   = enum_cast<hsm_t::variable>(p0);
        p_var = p0;
    }

    p_var -= p0;
    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##port", &p_var, variable_names + p0, nb)) {
        debug::ensure(0 <= p_var && p_var < nb);
        var = enum_cast<hsm_t::variable>(p_var + p0);
    }

    ImGui::PopItemWidth();
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
    // super_transition = 1,
    if_transition = 1,
    else_transition,
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

/** Get the first unused state from the HSM. */
static constexpr auto get_first_available(
  std::array<bool, 254>& enabled) noexcept -> std::optional<u8>
{
    for (u8 i = 0, e = static_cast<u8>(enabled.size()); i != e; ++i)
        if (enabled[i] == false)
            return std::make_optional(i);

    return std::nullopt;
}

/** After removing a state or a link between a child and his parent, we need to
 * search a new children to assign otherwise, parent child is set to invalid. */
static void update_super_sub_id(hsm_component&  hsm,
                                hsm_t::state_id super,
                                hsm_t::state_id old_sub) noexcept
{
    hsm.machine.states[super].sub_id = hsm_t::invalid_state_id;

    for (u8 i = 0; i != hsm_t::max_number_of_state; ++i) {
        if (i != super && i != old_sub) {
            if (hsm.machine.states[i].super_id == super) {
                hsm.machine.states[super].sub_id = i;
                break;
            }
        }
    }
}

static void remove_state(hsm_component&  hsm,
                         hsm_t::state_id id,
                         std::span<bool> enabled) noexcept
{
    using hsm_t = hsm_t;

    if (hsm.machine.states[id].super_id != hsm_t::invalid_state_id)
        update_super_sub_id(hsm, hsm.machine.states[id].super_id, id);

    for (auto& elem : hsm.machine.states) {
        if (elem.super_id == id)
            elem.super_id = hsm_t::invalid_state_id;
        if (elem.sub_id == id)
            elem.sub_id = hsm_t::invalid_state_id;
        if (elem.if_transition == id)
            elem.if_transition = hsm_t::invalid_state_id;
        if (elem.else_transition == id)
            elem.else_transition = hsm_t::invalid_state_id;
    }

    hsm.machine.clear_state(id);
    hsm.names[id].clear();
    hsm.positions[id].reset();
    enabled[id] = false;
}

static void remove_link(hsm_t& hsm, transition t) noexcept
{
    switch (t.type) {
    // case transition_type::super_transition:
    //     hsm.states[t.output].super_id = hsm_t::invalid_state_id;
    //     update_super_sub_id(hsm, t.input, t.output);
    //     break;
    case transition_type::if_transition:
        hsm.states[t.output].if_transition = hsm_t::invalid_state_id;
        break;
    case transition_type::else_transition:
        hsm.states[t.output].else_transition = hsm_t::invalid_state_id;
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
          "##event", &action_type, action_names, length(action_names)))
        action.type = enum_cast<hsm_t::action_type>(action_type);
    ImGui::PopItemWidth();

    switch (action.type) {
    case hsm_t::action_type::none:
        break;
    case hsm_t::action_type::set:
        show_port_widget(action.var1);
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.constant.i);
        ImGui::PopItemWidth();
        break;
    case hsm_t::action_type::unset:
        show_port_widget(action.var1);
        break;
    case hsm_t::action_type::reset:
        break;
    case hsm_t::action_type::output:
        show_port_widget(action.var1);
        ImGui::PushItemWidth(-1);
        show_variables_and_constants_combobox(action.var2, action);
        ImGui::PopItemWidth();
        break;
    case hsm_t::action_type::affect:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::plus:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::minus:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::negate:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::multiplies:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::divides:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::modulus:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::bit_and:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::bit_or:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::bit_not:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
        break;
    case hsm_t::action_type::bit_xor:
        show_variables_combobox(action.var1);
        show_ports_variables_constants_combobox(action.var2, action);
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
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::not_equal_to:
        ImGui::PushItemWidth(-1);
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::greater:
        ImGui::PushItemWidth(-1);
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::greater_equal:
        ImGui::PushItemWidth(-1);
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::less:
        ImGui::PushItemWidth(-1);
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::less_equal:
        ImGui::PushItemWidth(-1);
        show_ports_variables_combobox(condition.var1);
        show_ports_variables_constants_combobox(condition.var2, condition);
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
    const auto with_details =
      m_options.test(hsm_component_editor_data::display_action_label);

    for (u8 i = 0, e = static_cast<u8>(hsm_t::max_number_of_state); i != e;
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

        // ImNodes::BeginOutputAttribute(
        //   make_output(i, transition_type::super_transition),
        //   ImNodesPinShape_CircleFilled);
        // ImGui::TextUnformatted("super");
        // ImNodes::EndOutputAttribute();

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

    for (u8 i = 0, e = static_cast<u8>(hsm_t::max_number_of_state); i != e;
         ++i) {
        if (m_enabled[i] == false)
            continue;

        // if (hsm.states[i].super_id != hsm_t::invalid_state_id)
        //     ImNodes::Link(make_transition(i,
        //                                   hsm.states[i].super_id,
        //                                   transition_type::super_transition),
        //                   make_output(i, transition_type::super_transition),
        //                   make_input(hsm.states[i].super_id));

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
        // case transition_type::super_transition:
        //     hsm.states[out.output].super_id = in;
        //     if (hsm.states[in].sub_id == hsm_t::invalid_state_id)
        //         hsm.states[in].sub_id = out.output;
        //     break;
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

                if (hsm.machine.top_state == hsm_t::invalid_state_id)
                    (void)hsm.machine.set_state(id.value());
                else {
                    if (hsm.machine.states[hsm.machine.top_state].sub_id ==
                        hsm_t::invalid_state_id)
                        hsm.machine.states[hsm.machine.top_state].sub_id =
                          id.value();
                    (void)hsm.machine.set_state(id.value(),
                                                hsm.machine.top_state);
                }

                // if (hsm.states[0].sub_id == hsm_t::invalid_state_id) {
                //     hsm.states[0].sub_id = id.value();
                // }

                // hsm.states[id.value()].super_id = 0u;
                // hsm.states[id.value()].sub_id   = hsm_t::invalid_state_id;
                hsm.positions[id.value()].x = click_pos.x;
                hsm.positions[id.value()].y = click_pos.y;
                hsm.names[id.value()].clear();
                ImNodes::SetNodeScreenSpacePos(id.value(), click_pos);
            }

            auto action_lbl = m_options.test(display_action_label);
            if (ImGui::MenuItem("Enable action labels", nullptr, &action_lbl))
                m_options.set(display_action_label, action_lbl);

            auto condition_lbl = m_options.test(display_condition_label);
            if (ImGui::MenuItem(
                  "Enable action labels", nullptr, &condition_lbl))
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
                    remove_link(hsm.machine, get_transition(idx));
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

void hsm_component_editor_data::show_panel(hsm_component& hsm) noexcept
{
    if (ImGui::CollapsingHeader("selected states",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0, e = m_selected_nodes.Size; i != e; ++i) {
            const auto id    = get_state(m_selected_nodes[i]);
            auto&      state = hsm.machine.states[id];

            ImGui::PushID(i);

            ImGui::InputSmallString("Name", hsm.names[id]);
            ImGui::LabelFormat("Id", "{}", static_cast<unsigned>(id));

            const auto old_state_0 = hsm.machine.top_state == id;
            auto       state_0     = hsm.machine.top_state == id;

            if (ImGui::Checkbox("initial state", &state_0)) {
                if (old_state_0)
                    hsm.machine.top_state = hsm_t::invalid_state_id;
                if (state_0)
                    hsm.machine.top_state = id;
            }

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

    ImGui::LabelFormat("status", "{}", test_status_string[ordinal(m_test)]);

    ImGui::BeginDisabled(any_equal(m_test, test_status::being_processed));
    if (ImGui::Button("test")) {
        // app.start_hsm_test_start();
    }
    ImGui::EndDisabled();
}

bool hsm_component_editor_data::valid(hsm_component& hsm) noexcept
{
    if (any_equal(
          m_test, test_status::none, test_status::done, test_status::failed)) {
        m_test = test_status::being_processed;

        small_vector<hsm_t::state_id, max_number_of_state> stack;
        std::array<bool, max_number_of_state>              read;
        read.fill(false);
        read[0] = true;

        if (hsm.machine.states[0].if_transition != hsm_t::invalid_state_id)
            stack.emplace_back(hsm.machine.states[0].if_transition);
        if (hsm.machine.states[0].else_transition != hsm_t::invalid_state_id)
            stack.emplace_back(hsm.machine.states[0].else_transition);

        while (!stack.empty()) {
            auto top = stack.back();
            stack.pop_back();

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

        if (read != m_enabled) {
            // format(msg, "All state are not connected to the network");
            // m_messages.push(msg);
            // m_test = test_status::failed;
        }

        m_test = test_status::done;
    }

    return true;
}

void hsm_component_editor_data::show(component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto* hsm = app.mod.hsm_components.try_to_get(m_hsm_id);
    if (not hsm)
        return;

    const auto region_height = ImGui::GetContentRegionAvail().y;
    const auto table_heigth  = region_height -
                              ImGui::GetFrameHeightWithSpacing() -
                              ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("##table-editor", ImVec2(0, table_heigth), false);

    if (ImGui::BeginTabBar("##hsm-editor")) {
        if (ImGui::BeginTabItem("editor")) {
            show_graph(*hsm);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("log")) {
            if (hsm->machine.top_state == hsm_t::invalid_state_id) {
                ImGui::TextUnformatted("Top state is undefined");
            } else {
                ImGui::TextFormat("Top state: {}", hsm->machine.top_state);
            }

            ImGui::TextFormatDisabled("Not yet implemented.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void hsm_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    auto* hsm = app.mod.hsm_components.try_to_get(m_hsm_id);
    if (not hsm)
        return;

    show_panel(*hsm);
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
        for (auto i = 0, e = length(hsm->machine.states); i != e; ++i) {
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

    if (hsm.machine.top_state != hierarchical_state_machine::invalid_state_id)
        m_enabled[hsm.machine.top_state] = true;

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
