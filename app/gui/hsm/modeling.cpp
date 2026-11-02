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

static bool show_readable_vars(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);
    ImGui::PushItemWidth(-1);

    const auto old = act;

    const char* preview = variable_names[ordinal(act)];
    if (ImGui::BeginCombo("##var", preview)) {
        act = select_variable(act);
        act = select_port(act);
        act = select_source_var(act);
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();

    return old != act;
}

template<typename ConditionOrAction>
static bool show_all_vars(hsm_t::variable& act, ConditionOrAction& a) noexcept
{
    int u = 0;

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
        u += ImGui::InputScalar("value", ImGuiDataType_S32, &a.constant.i);
        ImGui::PopItemWidth();
    }

    if (act == hsm_t::variable::constant_r) {
        ImGui::PushItemWidth(-1);
        u += ImGui::InputScalar("value", ImGuiDataType_Float, &a.constant.f);
        ImGui::PopItemWidth();
    }

    ImGui::PopID();

    return u > 0;
}

static bool show_affactable_vars(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);
    ImGui::PushItemWidth(-1);

    const auto old = act;

    const char* preview = variable_names[ordinal(act)];

    if (ImGui::BeginCombo("##var", preview)) {
        act = select_variable(act);
        act = select_port(act);
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();

    return old != act;
}

static bool show_port_vars(hsm_t::variable& var) noexcept
{
    ImGui::PushID(&var);

    const auto old = var;

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

    return old != var;
}

static bool show_ports(hsm_t::condition_action& p) noexcept
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

    int u = 0;

    u += ImGui::CheckBoxTristate("0", &value_0);
    ImGui::SameLine();
    u += ImGui::CheckBoxTristate("1", &value_1);
    ImGui::SameLine();
    u += ImGui::CheckBoxTristate("2", &value_2);
    ImGui::SameLine();
    u += ImGui::CheckBoxTristate("3", &value_3);

    if (u > 0) {
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

        return true;
    }

    return false;
}

enum class transition_type : u8 {
    super_transition = 0b001,
    if_transition    = 0b010,
    else_transition  = 0b100,
};

struct transition {
    hsm_t::state_id input;
    hsm_t::state_id output;
    transition_type type;
};

/** Get the first unused state in the HSM. */
static constexpr auto get_first_available(
  std::array<bool, 254>& enabled) noexcept -> std::optional<u8>
{
    for (u8 i = 1, e = static_cast<u8>(enabled.size()); i != e; ++i)
        if (enabled[i] == false)
            return std::make_optional(i);

    return std::nullopt;
}

static ImVec2 unknown_position() noexcept
{
    return ImVec2(std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN());
}

static bool is_unknown(const ImVec2 p) noexcept { return std::isnan(p.x); }

/// @return true if @c id is @c ancestor or one of its sub states.
static bool is_descendant(const hsm_t&          m,
                          const hsm_t::state_id id,
                          const hsm_t::state_id ancestor) noexcept
{
    auto s = id;

    // The bound protects from a cycle in a component not yet checked.
    for (int i = 0;
         i != hsm_t::max_number_of_state and s != hsm_t::invalid_state_id;
         ++i, s = m.states[s].super_id)
        if (s == ancestor)
            return true;

    return false;
}

static int count_sub_states(const hsm_component&  hsm,
                            std::span<const bool> enabled,
                            const hsm_t::state_id parent) noexcept
{
    auto n = 0;

    for (u8 i = 1; i != hsm_t::max_number_of_state; ++i)
        if (enabled[i] and hsm.machine.states[i].super_id == parent)
            ++n;

    return n;
}

static auto first_sub_state(const hsm_component&  hsm,
                            std::span<const bool> enabled,
                            const hsm_t::state_id parent,
                            const hsm_t::state_id except) noexcept
  -> hsm_t::state_id
{
    for (u8 i = 1; i != hsm_t::max_number_of_state; ++i)
        if (i != except and enabled[i] and
            hsm.machine.states[i].super_id == parent)
            return i;

    return hsm_t::invalid_state_id;
}

/// @return The number of levels below @c id, 0 for a leaf.
static int height_of(const hsm_component&  hsm,
                     std::span<const bool> enabled,
                     const hsm_t::state_id id) noexcept
{
    const auto depth  = hsm.machine.depth_of(id);
    auto       height = 0;

    for (u8 i = 1; i != hsm_t::max_number_of_state; ++i)
        if (enabled[i] and is_descendant(hsm.machine, i, id))
            height = std::max(height, hsm.machine.depth_of(i) - depth);

    return height;
}

template<std::size_t N>
static void append_label(const hsm_component&  hsm,
                         const hsm_t::state_id id,
                         small_string<N>&      out) noexcept
{
    if (id == hsm.machine.top_state)
        out.append("Root");
    else if (hsm.names[id].empty())
        out.append(format_n<16>("state {}", id).sv());
    else
        out.append(hsm.names[id].sv());
}

/// Appends the path of @c id from the top state, e.g. "Root/Marche/Tic".
template<std::size_t N>
static void append_path(const hsm_component&  hsm,
                        const hsm_t::state_id id,
                        small_string<N>&      out) noexcept
{
    std::array<hsm_t::state_id, hsm_t::max_depth> path;
    auto                                          n = 0;

    for (auto s = id; s != hsm_t::invalid_state_id and n < hsm_t::max_depth;
         s      = hsm.machine.states[s].super_id)
        path[n++] = s;

    for (auto i = n - 1; i >= 0; --i) {
        append_label(hsm, path[i], out);
        if (i > 0)
            out.push_back('/');
    }
}

/// Removes the state @c id and its sub states.
static void remove_state(hsm_component&  hsm,
                         hsm_t::state_id id,
                         std::span<bool> enabled) noexcept
{
    debug::ensure(id != hsm.machine.top_state);

    auto& states = hsm.machine.states;

    for (u8 i = 1; i != hsm_t::max_number_of_state; ++i)
        if (enabled[i] and states[i].super_id == id)
            remove_state(hsm, i, enabled);

    if (const auto parent = states[id].super_id;
        parent != hsm_t::invalid_state_id and states[parent].sub_id == id)
        states[parent].sub_id = first_sub_state(hsm, enabled, parent, id);

    for (u8 i = 0; i != hsm_t::max_number_of_state; ++i) {
        if (states[i].if_transition == id)
            states[i].if_transition = hsm_t::invalid_state_id;
        if (states[i].else_transition == id)
            states[i].else_transition = hsm_t::invalid_state_id;
    }

    hsm.machine.clear_state(id);
    hsm.names[id].clear();
    hsm.positions[id].reset();
    enabled[id] = false;
}

/// Moves the state @c id and its sub states under @c parent.
static void move_state(hsm_component&        hsm,
                       std::span<const bool> enabled,
                       const hsm_t::state_id id,
                       const hsm_t::state_id parent) noexcept
{
    auto&      states = hsm.machine.states;
    const auto old    = states[id].super_id;

    if (states[old].sub_id == id)
        states[old].sub_id = first_sub_state(hsm, enabled, old, id);

    states[id].super_id = parent;
    if (states[parent].sub_id == hsm_t::invalid_state_id)
        states[parent].sub_id = id;
}

static bool remove_link(hsm_component& hsm, transition t) noexcept
{
    switch (t.type) {
    case transition_type::super_transition:
        hsm.machine.states[t.output].sub_id = hsm_t::invalid_state_id;
        return true;

    case transition_type::if_transition:
        hsm.machine.states[t.output].if_transition = hsm_t::invalid_state_id;
        return true;

    case transition_type::else_transition:
        hsm.machine.states[t.output].else_transition = hsm_t::invalid_state_id;
        return true;
    }

    return false;
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

static bool show_state_action(hsm_t::state_action& action) noexcept
{
    ImGui::PushID(static_cast<void*>(&action));
    const auto old_action = action;

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

    return action != old_action;
}

static bool show_state_condition(hsm_t::condition_action& condition) noexcept
{
    ImGui::PushID(&condition);
    const auto old_condition = condition;

    auto type = static_cast<int>(condition.type);

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo(
          "##event", &type, condition_names, length(condition_names))) {
        debug::ensure(0 <= type && type < hsm_t::condition_type_count);
        condition.type = enum_cast<hsm_t::condition_type>(type);
    }
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

    return old_condition != condition;
}

/// The view of a level of the state machine for the @c node_editor.
struct hsm_component_editor_data::graph {
    hsm_component_editor_data& ed;
    small_string<127>          buffer;
    std::array<ImU32, 2>       composite_colors = {};

    const view_node& node(const u32 i) const noexcept { return ed.m_nodes[i]; }

    bool is_composite(const state_id id) const noexcept
    {
        return count_sub_states(ed.m_hsm, ed.m_enabled, id) > 0;
    }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(ed.m_nodes.size());
    }

    ImVec2& position(const u32 i) noexcept
    {
        const auto& n = node(i);

        if (n.kind == node_kind::target)
            return ed.m_target_positions[n.id];

        // The position of a composite state is the one of its level parent:
        // its initial pseudo state has its own position.
        if (n.kind == node_kind::initial and n.id != ed.m_hsm.machine.top_state)
            return ed.m_initial_position;

        auto& p = ed.m_positions[n.id];
        if (is_unknown(p))
            p = ImVec2(ed.m_hsm.positions[n.id].x, ed.m_hsm.positions[n.id].y);

        return p;
    }

    ImU32 title_color(const u32 i, const bool selected) const noexcept
    {
        const auto& n = node(i);

        if (n.kind == node_kind::target)
            return ImGui::GetColorU32(ImGuiCol_FrameBg);

        if (n.kind == node_kind::state and is_composite(n.id))
            return composite_colors[selected];

        return ImGui::GetColorU32(selected ? ImGuiCol_TitleBgActive
                                           : ImGuiCol_TitleBg);
    }

    std::string_view title(const u32 i) noexcept
    {
        const auto& n = node(i);

        switch (n.kind) {
        case node_kind::initial:
            return "Initial state";

        case node_kind::target:
            buffer = "goto ";
            append_path(ed.m_hsm, n.id, buffer);
            return buffer.sv();

        case node_kind::state:
            format(buffer, "{} (id: {})", ed.m_hsm.names[n.id].sv(), n.id);
            return buffer.sv();
        }

        unreachable();
    }

    u32 input_count(const u32 i) const noexcept
    {
        return node(i).kind == node_kind::initial ? 0u : 1u;
    }

    u32 output_count(const u32 i) const noexcept
    {
        switch (node(i).kind) {
        case node_kind::initial:
            return 1u;
        case node_kind::state:
            return 2u;
        case node_kind::target:
            return 0u;
        }

        unreachable();
    }

    std::string_view input_name(const u32 /*node*/,
                                const u32 /*port*/) const noexcept
    {
        return "in";
    }

    std::string_view output_name(const u32 i, const u32 port) const noexcept
    {
        if (node(i).kind == node_kind::initial)
            return "start";

        return port == 0 ? "if condition is valid do" : "Otherwise do";
    }

    void content(const u32 i) noexcept
    {
        const auto& n = node(i);
        if (n.kind != node_kind::state)
            return;

        auto&      s       = ed.m_hsm.machine.states[n.id];
        const auto details = ed.m_options.test(display_action_label);

        if (const auto subs = count_sub_states(ed.m_hsm, ed.m_enabled, n.id);
            subs > 0)
            ImGui::TextDisabled("%d sub states (double-click to open)", subs);

        if (details and s.enter_action.type != hsm_t::action_type::none)
            display_action(s.enter_action, "on enter");

        show_condition(s.condition);
        if (ed.m_options.test(display_condition_label))
            show_complete_condition(s.condition);

        if (details) {
            if (s.if_action.type != hsm_t::action_type::none)
                display_action(s.if_action, "if");
            if (s.else_action.type != hsm_t::action_type::none)
                display_action(s.else_action, "else");
            if (s.exit_action.type != hsm_t::action_type::none)
                display_action(s.exit_action, "on exit");
        }
    }

    template<typename Fn>
    void for_each_link(Fn&& fn) const noexcept
    {
        const auto& states = ed.m_hsm.machine.states;

        for (const auto& l : ed.m_links) {
            const auto port      = l.type ==
                                  ordinal(transition_type::else_transition)
                                     ? 1u
                                     : 0u;
            const auto from_kind = l.type ==
                                       ordinal(
                                         transition_type::super_transition)
                                     ? node_kind::initial
                                     : node_kind::state;
            const auto to_kind   = states[l.to].super_id == ed.m_level
                                     ? node_kind::state
                                     : node_kind::target;

            fn(ed.node_of(l.from, from_kind), port, ed.node_of(l.to, to_kind),
               0u);
        }
    }
};

u32 hsm_component_editor_data::node_of(const state_id  id,
                                       const node_kind kind) const noexcept
{
    for (u32 i = 0, e = static_cast<u32>(m_nodes.size()); i != e; ++i)
        if (m_nodes[i].id == id and m_nodes[i].kind == kind)
            return i;

    return 0;
}

void hsm_component_editor_data::open_level(const state_id id) noexcept
{
    m_level            = id;
    m_initial_position = unknown_position();
    m_target_positions.fill(unknown_position());
    m_editor.clear_selection();
    m_editor.request_fit();
}

void hsm_component_editor_data::build_graph() noexcept
{
    const auto& states = m_hsm.machine.states;

    // An undo or a removal may delete the opened level.
    if (m_level != m_hsm.machine.top_state and not m_enabled[m_level])
        open_level(m_hsm.machine.top_state);

    vector<view_node> nodes;
    nodes.push_back(view_node{ .id = m_level, .kind = node_kind::initial });
    for (u8 i = 1, e = static_cast<u8>(hsm_t::max_number_of_state); i != e; ++i)
        if (m_enabled[i] and states[i].super_id == m_level)
            nodes.push_back(view_node{ .id = i, .kind = node_kind::state });

    const auto state_end = static_cast<int>(nodes.size());
    for (auto i = 1; i < state_end; ++i) {
        const auto& s = states[nodes[i].id];

        for (const auto t : { s.if_transition, s.else_transition }) {
            const auto target = view_node{ .id = t, .kind = node_kind::target };

            if (t != hsm_t::invalid_state_id and
                states[t].super_id != m_level and
                std::ranges::find(nodes, target) == nodes.end())
                nodes.push_back(target);
        }
    }

    if (not std::ranges::equal(nodes, m_nodes)) {
        m_nodes = std::move(nodes);
        m_editor.reset();

        // The initial pseudo state goes on the left of the sub states and the
        // targets on their right.
        auto min = ImVec2(0.f, 0.f), max = ImVec2(0.f, 0.f);
        for (auto i = 1; i < state_end; ++i) {
            const auto& p = m_hsm.positions[m_nodes[i].id];
            min           = i == 1 ? ImVec2(p.x, p.y)
                                   : ImVec2(std::min(min.x, p.x), std::min(min.y, p.y));
            max           = i == 1 ? ImVec2(p.x, p.y)
                                   : ImVec2(std::max(max.x, p.x), std::max(max.y, p.y));
        }

        if (is_unknown(m_initial_position))
            m_initial_position = ImVec2(min.x - 300.f, min.y);

        auto row = 0.f;
        for (auto i = state_end, e = static_cast<int>(m_nodes.size()); i < e;
             ++i)
            if (auto& p = m_target_positions[m_nodes[i].id]; is_unknown(p))
                p = ImVec2(max.x + 400.f, min.y + 120.f * row++);
    }

    const auto old_size = m_links.size();
    m_links.clear();

    if (const auto first = states[m_level].sub_id;
        first != hsm_t::invalid_state_id and states[first].super_id == m_level)
        m_links.push_back(
          { .from = m_level,
            .to   = first,
            .type = ordinal(transition_type::super_transition) });

    for (auto i = 1; i < state_end; ++i) {
        const auto  id = m_nodes[i].id;
        const auto& s  = states[id];

        if (s.if_transition != hsm_t::invalid_state_id)
            m_links.push_back(
              { .from = id,
                .to   = s.if_transition,
                .type = ordinal(transition_type::if_transition) });

        if (s.else_transition != hsm_t::invalid_state_id)
            m_links.push_back(
              { .from = id,
                .to   = s.else_transition,
                .type = ordinal(transition_type::else_transition) });
    }

    if (m_links.size() != old_size)
        m_editor.clear_selection();
}

void hsm_component_editor_data::show_breadcrumb() noexcept
{
    std::array<state_id, hsm_t::max_depth> path;
    auto                                   n = 0;

    for (auto s = m_level;
         s != hsm_t::invalid_state_id and n < hsm_t::max_depth;
         s = m_hsm.machine.states[s].super_id)
        path[n++] = s;

    for (auto i = n - 1; i >= 0; --i) {
        small_string<64> label;
        append_label(m_hsm, path[i], label);

        ImGui::PushID(path[i]);
        if (i == 0)
            ImGui::TextUnformatted(label.c_str());
        else if (ImGui::SmallButton(label.c_str()))
            open_level(path[i]);
        ImGui::PopID();

        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextUnformatted(">");
            ImGui::SameLine();
        }
    }
}

bool hsm_component_editor_data::show_menu() noexcept
{
    auto u = 0;

    const bool open_popup = ImGui::IsWindowFocused(
                              ImGuiFocusedFlags_RootAndChildWindows) and
                            m_editor.is_background_hovered() and
                            ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        const auto click_pos = m_editor.to_world(
          ImGui::GetMousePosOnOpeningCurrentPopup());

        const auto id        = get_first_available(m_enabled);
        const auto can_level = m_hsm.machine.depth_of(m_level) + 1 <
                               hsm_t::max_depth;

        if (ImGui::MenuItem("new state", nullptr, false,
                            id.has_value() and can_level)) {
            auto& states   = m_hsm.machine.states;
            m_enabled[*id] = true;
            ++u;

            if (states[m_level].sub_id == hsm_t::invalid_state_id)
                states[m_level].sub_id = *id;
            (void)m_hsm.machine.set_state(*id, m_level);

            m_hsm.positions[*id].x = click_pos.x;
            m_hsm.positions[*id].y = click_pos.y;
            m_hsm.names[*id].clear();
            m_positions[*id] = click_pos;
        }

        if (m_level != m_hsm.machine.top_state and
            ImGui::MenuItem("Open the parent level"))
            open_level(m_hsm.machine.states[m_level].super_id);

        ImGui::Separator();

        auto action_lbl = m_options.test(display_action_label);
        if (ImGui::MenuItem("Display action labels", nullptr, &action_lbl))
            m_options.set(display_action_label, action_lbl);

        auto condition_lbl = m_options.test(display_condition_label);
        if (ImGui::MenuItem("Display condition labels", nullptr,
                            &condition_lbl))
            m_options.set(display_condition_label, condition_lbl);

        if (ImGui::MenuItem("Grid layout"))
            m_editor.request_grid_layout();

        if (ImGui::MenuItem("Automatic layout"))
            m_editor.request_force_layout();

        ImGui::MenuItem("Show minimap", "", &m_editor.show_minimap);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();

    return u > 0;
}

bool hsm_component_editor_data::show_graph(application& app) noexcept
{
    auto  u      = 0;
    auto& states = m_hsm.machine.states;

    // The breadcrumb may change the level: the graph is built after.
    show_breadcrumb();
    build_graph();

    auto g = graph{ .ed = *this, .buffer = {}, .composite_colors = {} };
    app.config.vars.colors.read(
      [&](const auto& colors, const auto /*v*/) noexcept {
          g.composite_colors[0] = to_ImU32(colors[style_color::node_2]);
          g.composite_colors[1] = to_ImU32(colors[style_color::node_2_active]);
      });

    const auto ev = m_editor.show("hsm-graph", g);

    u += show_menu();

    if (ev.link_created) {
        const auto from = m_nodes[ev.link_created->out.node];
        const auto to   = m_nodes[ev.link_created->in.node];

        if (from.kind == node_kind::initial) {
            if (to.kind == node_kind::state) {
                states[m_level].sub_id = to.id;
                ++u;
            }
        } else if (from.kind == node_kind::state and
                   to.id != m_hsm.machine.top_state) {
            if (ev.link_created->out.port == 0)
                states[from.id].if_transition = to.id;
            else
                states[from.id].else_transition = to.id;
            ++u;
        }
    }

    if (ev.delete_selection) {
        for (const auto i : m_editor.selected_links()) {
            const auto& l = m_links[i];
            u += remove_link(
              m_hsm, transition{ .input  = l.to,
                                 .output = l.from,
                                 .type = enum_cast<transition_type>(l.type) });
        }

        for (const auto i : m_editor.selected_nodes()) {
            if (const auto n = m_nodes[i];
                n.kind == node_kind::state and m_enabled[n.id]) {
                remove_state(m_hsm, n.id, m_enabled);
                m_positions[n.id] = unknown_position();
                ++u;
            }
        }

        m_editor.clear_selection();
    }

    if (ev.node_activated) {
        const auto n = m_nodes[*ev.node_activated];

        if (n.kind == node_kind::state)
            open_level(n.id);
        else if (n.kind == node_kind::target and
                 states[n.id].super_id != hsm_t::invalid_state_id)
            open_level(states[n.id].super_id);
    }

    if (ev.nodes_moved)
        ++u;

    return u > 0;
}

bool hsm_component_editor_data::show_panel(component& compo) noexcept
{
    int u = 0;

    if (ImGui::CollapsingHeader("constants settings")) {
        u += ImGui::InputReal("constant 0", &m_hsm.machine.constants[0]);
        u += ImGui::InputReal("constant 1", &m_hsm.machine.constants[1]);
        u += ImGui::InputReal("constant 2", &m_hsm.machine.constants[2]);
        u += ImGui::InputReal("constant 3", &m_hsm.machine.constants[3]);
        u += ImGui::InputReal("constant 4", &m_hsm.machine.constants[4]);
        u += ImGui::InputReal("constant 5", &m_hsm.machine.constants[5]);
        u += ImGui::InputReal("constant 6", &m_hsm.machine.constants[6]);
        u += ImGui::InputReal("constant 7", &m_hsm.machine.constants[7]);
    }

    if (ImGui::CollapsingHeader("External sources")) {
        if (ImGui::Button("Refresh source"))
            ++u;
        m_hsm.machine.flags.set(hsm_t::option::use_source,
                                m_hsm.machine.compute_is_using_source());

        if (m_hsm.machine.flags[hsm_t::option::use_source]) {
            show_combobox_external_sources(compo.srcs, m_hsm.src);
        } else {
            ImGui::TextDisabled("HSM does not use external source");
        }
    }

    const auto top = m_hsm.machine.top_state;

    // Shows the states accepted by @c accept with their path and returns the
    // state selected by the user.
    const auto combo_state = [&](const char* label, const state_id current,
                                 auto&& accept) -> std::optional<state_id> {
        small_string<127> preview;
        if (current == hsm_t::invalid_state_id)
            preview = "None";
        else
            append_path(m_hsm, current, preview);

        std::optional<state_id> ret;
        if (ImGui::BeginCombo(label, preview.c_str())) {
            if (accept(hsm_t::invalid_state_id) and
                ImGui::Selectable("None", current == hsm_t::invalid_state_id))
                ret = hsm_t::invalid_state_id;

            for (u8 i = 0; i != hsm_t::max_number_of_state; ++i) {
                if (not(i == top or m_enabled[i]) or not accept(i))
                    continue;

                small_string<127> path;
                append_path(m_hsm, i, path);

                ImGui::PushID(i);
                if (ImGui::Selectable(path.c_str(), i == current))
                    ret = i;
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return ret;
    };

    if (ImGui::CollapsingHeader("selected states",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto node : m_editor.selected_nodes()) {
            if (m_nodes[node].kind != node_kind::state)
                continue;

            const auto id    = m_nodes[node].id;
            auto&      state = m_hsm.machine.states[id];

            ImGui::PushID(static_cast<int>(id));

            name_str name = m_hsm.names[id];
            if (ImGui::InputSmallString("Name", name)) {
                m_hsm.names[id] = name;
                ++u;
            }
            ImGui::LabelFormat("Id", "{}", static_cast<unsigned>(id));

            ImGui::SeparatorText("Hierarchy");

            const auto height = height_of(m_hsm, m_enabled, id);
            if (const auto p = combo_state(
                  "Parent", state.super_id,
                  [&](const state_id c) noexcept {
                      return c != hsm_t::invalid_state_id and
                             not is_descendant(m_hsm.machine, c, id) and
                             m_hsm.machine.depth_of(c) + 1 + height <
                               hsm_t::max_depth;
                  });
                p.has_value() and *p != state.super_id) {
                move_state(m_hsm, m_enabled, id, *p);
                ++u;
            }

            if (count_sub_states(m_hsm, m_enabled, id) > 0) {
                if (const auto s = combo_state(
                      "Initial state", state.sub_id, [&](const state_id c) {
                          return c != hsm_t::invalid_state_id and
                                 m_hsm.machine.states[c].super_id == id;
                      })) {
                    state.sub_id = *s;
                    ++u;
                }
            }

            const auto is_target = [&](const state_id c) noexcept {
                return c != top;
            };

            if (const auto t = combo_state("If transition", state.if_transition,
                                           is_target)) {
                state.if_transition = *t;
                ++u;
            }

            if (const auto t = combo_state("Else transition",
                                           state.else_transition, is_target)) {
                state.else_transition = *t;
                ++u;
            }

            if (ImGui::Button("Open"))
                open_level(id);

            ImGui::SeparatorText("Condition");
            u += show_state_condition(state.condition);

            ImGui::SeparatorText("Actions");
            if (ImGui::CollapsingHeader("Enter action"))
                u += show_state_action(state.enter_action);

            if (ImGui::CollapsingHeader("If condition is true"))
                u += show_state_action(state.if_action);

            if (ImGui::CollapsingHeader("Else"))
                u += show_state_action(state.else_action);

            if (ImGui::CollapsingHeader("Exit action"))
                u += show_state_action(state.exit_action);

            ImGui::PopID();
            ImGui::Separator();
        }
    }

    return u > 0;
}

static const char* hierarchy_error_message(const simulation_errc ec) noexcept
{
    switch (ec) {
    case simulation_errc::hsm_top_state_error:
        return "the top state is undefined";
    case simulation_errc::hsm_hierarchy_error:
        return "a cycle or more than 8 levels in the hierarchy";
    case simulation_errc::hsm_initial_state_error:
        return "a composite state without initial state";
    case simulation_errc::hsm_transition_error:
        return "a transition from or to the top state";
    default:
        return "unknown error";
    }
}

bool hsm_component_editor_data::valid() noexcept
{
    const auto& states     = m_hsm.machine.states;
    auto        have_error = false;

    if (const auto ec = m_hsm.machine.find_error();
        ec != simulation_errc::none) {
        ImGui::TextFormat("Hierarchy: {}", hierarchy_error_message(ec));
        have_error = true;
    }

    // A state is reached by a transition or as the initial state of a
    // reached composite state.
    small_vector<hsm_t::state_id, max_number_of_state> stack;
    std::array<bool, max_number_of_state>              read;
    read.fill(false);

    const auto top = m_hsm.machine.top_state;
    if (states[top].sub_id == hsm_t::invalid_state_id) {
        ImGui::LabelFormat("Initiale state", "State machine is empty");
        return false;
    }

    read[top] = true;
    stack.emplace_back(states[top].sub_id);

    while (not stack.empty()) {
        const auto id = stack.back();
        stack.pop_back();

        if (read[id])
            continue;

        read[id]      = true;
        const auto& s = states[id];

        switch (s.condition.type) {
        case hsm_t::condition_type::none:
            break;

        case hsm_t::condition_type::port:
        case hsm_t::condition_type::sigma:
            if (not have_if_transition(s)) {
                ImGui::TextFormat("state {}: connect if-condition", id);
                have_error = true;
            }
            break;

        case hsm_t::condition_type::equal_to:
        case hsm_t::condition_type::not_equal_to:
        case hsm_t::condition_type::greater:
        case hsm_t::condition_type::greater_equal:
        case hsm_t::condition_type::less:
        case hsm_t::condition_type::less_equal:
            if (not have_if_transition(s)) {
                ImGui::TextFormat("state {}: connect if-condition", id);
                have_error = true;
            }
            if (not have_else_transition(s)) {
                ImGui::TextFormat("state {}: connect else-condition", id);
                have_error = true;
            }
            break;
        }

        for (const auto next : { s.if_transition, s.else_transition, s.sub_id })
            if (next != hsm_t::invalid_state_id and not read[next])
                stack.emplace_back(next);
    }

    for (u8 i = 1; i != max_number_of_state; ++i) {
        if (m_enabled[i] and not read[i]) {
            ImGui::TextFormat("state {}: unreachable", i);
            have_error = true;
        }
    }

    return not have_error;
}

bool hsm_component_editor_data::show(component_editor& ed,
                                     const component_access& /*ids*/,
                                     component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto  u   = 0;

    read(app, compo);

    const auto region_height = ImGui::GetContentRegionAvail().y;
    const auto table_heigth  = region_height -
                              ImGui::GetFrameHeightWithSpacing() -
                              ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginChild("##table-editor", ImVec2(0, table_heigth), false)) {
        if (ImGui::BeginTabBar("##hsm-editor")) {
            if (ImGui::BeginTabItem("Editor")) {
                if (show_graph(app)) {
                    write(app, compo);
                    ++u;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Test")) {
                if (not valid())
                    ImGui::TextFormat("Error in HSM");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::EndChild();

    return u > 0;
}

bool hsm_component_editor_data::show_selected_nodes(
  component_editor& ed,
  const component_access& /*ids*/,
  component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    auto  u   = 0;

    read(app, compo);
    if (show_panel(compo)) {
        write(app, compo);
        ++u;
    }

    return u > 0;
}

void hsm_component_editor_data::read(application& app,
                                     component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        debug::ensure(compo.type == component_type::hsm);

        if (auto* hsm = ids.hsm_components.try_to_get(m_hsm_id)) {
            m_hsm = *hsm;
        }
    });
}

static void update_enabled_states(const hierarchical_state_machine& hsm,
                                  std::span<bool> vec) noexcept
{
    if (vec.size() == hierarchical_state_machine::max_number_of_state) {
        std::ranges::fill(vec, false);

        for (auto i = 0, e = length(hsm.states); i != e; ++i) {
            if (hsm.states[i].if_transition !=
                hierarchical_state_machine::invalid_state_id) {
                vec[hsm.states[i].if_transition] = true;
                vec[i]                           = true;
            }
            if (hsm.states[i].else_transition !=
                hierarchical_state_machine::invalid_state_id) {
                vec[hsm.states[i].else_transition] = true;
                vec[i]                             = true;
            }
            if (hsm.states[i].super_id !=
                hierarchical_state_machine::invalid_state_id) {
                vec[hsm.states[i].super_id] = true;
                vec[i]                      = true;
            }
            if (hsm.states[i].sub_id !=
                hierarchical_state_machine::invalid_state_id) {
                vec[hsm.states[i].sub_id] = true;
                vec[i]                    = true;
            }
        }
    }
}

void hsm_component_editor_data::write(application& app,
                                      component&   compo) noexcept
{
    for (auto i = 0, e = length(m_hsm.machine.states); i != e; ++i) {
        if ((i == m_hsm.machine.top_state or m_enabled[i]) and
            not is_unknown(m_positions[i])) {
            m_hsm.positions[i].x = m_positions[i].x;
            m_hsm.positions[i].y = m_positions[i].y;
        }
    }

    auto c_ptr = std::make_unique<component>(compo);
    auto g_ptr = std::make_unique<hsm_component>(m_hsm);

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (c->type == component_type::hsm) {
                  const auto hsm_id = c->id.hsm_id;

                  if (auto* hsm = ids.hsm_components.try_to_get(hsm_id)) {
                      *hsm = *g;
                  }
              }
          });
      });
}

hsm_component_editor_data::hsm_component_editor_data(
  const component_id     id,
  const hsm_component_id hid,
  const hsm_component&   hsm) noexcept
  : m_hsm(hsm)
  , m_options(3)
  , m_id(id)
  , m_hsm_id(hid)
{
    m_positions.fill(unknown_position());
    m_target_positions.fill(unknown_position());
    m_initial_position = unknown_position();

    m_hsm.machine.states[0].super_id = hsm_t::invalid_state_id;
    m_hsm.machine.top_state          = 0;

    m_enabled.fill(false);
    update_enabled_states(m_hsm.machine, m_enabled);
}

} // namespace irt
