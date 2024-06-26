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

static const char* variable_names[] = { "none",       "port_0",  "port_1",
                                        "port_2",     "port_3",  "variable a",
                                        "variable b", "constant" };

static const char* action_names[] = {
    "none",        "set port",   "unset port", "reset ports",
    "output port", "x = y",      "x = x + y",  "x = x - y",
    "x = -x",      "x = x * y",  "x = x / y",  "x = x % y",
    "x = x and y", "x = x or y", "x = not x",  "x = x xor y"
};

static const char* condition_names[] = {
    "none",          "value on port", "a = constant",  "a != constant",
    "a > constant",  "a < constant",  "a >= constant", "a <= constant",
    "b = constant",  "b != constant", "b > constant",  "b < constant",
    "b >= constant", "b <= constant", "a = b",         "a != b",
    "a > b",         "a < b",         "a >= b",        "a <= b",
};

static const std::string_view test_status_string[] = { "none",
                                                       "being_processed",
                                                       "done",
                                                       "failed" };

static void show_only_variable_widget(hsm_t::variable& act) noexcept
{
    ImGui::PushID(&act);
    int var = static_cast<int>(act) - 5;
    if (!(0 <= var && var < 2))
        var = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &var, variable_names + 5, 2)) {
        debug::ensure(0 <= var && var < 2);
        act = enum_cast<hsm_t::variable>(var + 5);
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_variable_widget(hsm_t::variable& act, i32& parameter) noexcept
{
    ImGui::PushID(&act);
    int var = static_cast<int>(act) - 5;
    if (!(0 <= var && var < 3))
        var = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##var", &var, variable_names + 5, 3)) {
        debug::ensure(0 <= var && var < 3);
        act = enum_cast<hsm_t::variable>(var + 5);
    }

    if (act == hsm_t::variable::constant) {
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &parameter);
        ImGui::PopItemWidth();
    }

    ImGui::PopItemWidth();
    ImGui::PopID();
}

constexpr int make_state(hsm_t::state_id id) noexcept
{
    return static_cast<int>(id);
}

constexpr hsm_t::state_id get_state(int idx) noexcept
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

constexpr int make_input(hsm_t::state_id id) noexcept
{
    return static_cast<int>(id << 16);
}

constexpr hsm_t::state_id get_input(int idx) noexcept
{
    return static_cast<hsm_t::state_id>((idx >> 16) & 0xff);
}

constexpr int make_output(hsm_t::state_id id, transition_type type) noexcept
{
    return static_cast<int>((static_cast<u8>(type) << 8) | id);
}

constexpr output get_output(int idx) noexcept
{
    return output{ .output = static_cast<u8>(idx & 0xff),
                   .type   = static_cast<transition_type>((idx >> 8) & 0xff) };
}

constexpr int make_transition(hsm_t::state_id from,
                              hsm_t::state_id to,
                              transition_type type) noexcept
{
    return make_input(to) | make_output(from, type);
}

constexpr transition get_transition(int idx) noexcept
{
    const auto input  = get_input(idx);
    const auto output = get_output(idx);

    return transition{ .input  = input,
                       .output = output.output,
                       .type   = output.type };
}

//! Get the first unused state from the HSM.
static constexpr auto get_first_available(
  std::array<bool, 254>& enabled) noexcept -> std::optional<u8>
{
    for (u8 i = 0, e = static_cast<u8>(enabled.size()); i != e; ++i)
        if (enabled[i] == false)
            return std::make_optional(i);

    return std::nullopt;
}

//! After removing a state or a link between a child and his parent, we need to
//! search a new children to assign otherwise, parent child is set to invalid.
static void update_super_sub_id(hsm_t&          hsm,
                                hsm_t::state_id super,
                                hsm_t::state_id old_sub) noexcept
{
    using hsm_t = hsm_t;

    hsm.states[super].sub_id = hsm_t::invalid_state_id;

    for (u8 i = 0; i != hsm_t::max_number_of_state; ++i) {
        if (i != super && i != old_sub) {
            if (hsm.states[i].super_id == super) {
                hsm.states[super].sub_id = i;
                break;
            }
        }
    }
}

static void remove_state(hsm_t&          hsm,
                         hsm_t::state_id id,
                         std::span<bool> enabled) noexcept
{
    using hsm_t = hsm_t;

    if (hsm.states[id].super_id != hsm_t::invalid_state_id)
        update_super_sub_id(hsm, hsm.states[id].super_id, id);

    for (auto& elem : hsm.states) {
        if (elem.super_id == id)
            elem.super_id = hsm_t::invalid_state_id;
        if (elem.sub_id == id)
            elem.sub_id = hsm_t::invalid_state_id;
        if (elem.if_transition == id)
            elem.if_transition = hsm_t::invalid_state_id;
        if (elem.else_transition == id)
            elem.else_transition = hsm_t::invalid_state_id;
    }

    hsm.clear_state(id);
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

// static void show_state_id_editor(const std::array<bool, 254>& enabled,
//                                  hsm_t::state_id&             current)
//                                  noexcept
// {
//     ImGui::PushID(&current);

//     small_string<7> preview_value("-");
//     if (current != hsm_t::invalid_state_id)
//         format(preview_value, "{}", current);

//     ImGui::PushItemWidth(-1);
//     if (ImGui::BeginCombo("##transition", preview_value.c_str())) {
//         if (ImGui::Selectable("-", current == hsm_t::invalid_state_id))
//             current = hsm_t::invalid_state_id;

//         for (u8 i = 0, e = hsm_t::max_number_of_state; i < e; ++i) {
//             if (enabled[i]) {
//                 format(preview_value, "{}", i);
//                 if (ImGui::Selectable(preview_value.c_str(), i == current))
//                     current = i;
//             }
//         }

//         ImGui::EndCombo();
//     }
//     ImGui::PopItemWidth();
//     ImGui::PopID();
// }

static void show_port_widget(hsm_t::variable& var) noexcept
{
    ImGui::PushID(&var);
    int port = static_cast<int>(var) - 1;
    if (!(0 <= port && port < 4))
        port = 0;

    ImGui::PushItemWidth(-1);
    if (ImGui::Combo("##port", &port, variable_names + 1, 4)) {
        debug::ensure(0 <= port && port <= 3);
        var = enum_cast<hsm_t::variable>(port + 1);
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
}

static void show_ports(u8& value, u8& mask) noexcept
{
    ImGui::PushID(static_cast<void*>(&value));

    const int sub_value_3 = value & 0b0001 ? 1 : 0;
    const int sub_value_2 = value & 0b0010 ? 1 : 0;
    const int sub_value_1 = value & 0b0100 ? 1 : 0;
    const int sub_value_0 = value & 0b1000 ? 1 : 0;

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
        value = value_0 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_1 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_2 == 1 ? 1u : 0u;
        value <<= 1;
        value |= value_3 == 1 ? 1u : 0u;

        mask = value_0 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_1 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_2 != -1 ? 1u : 0u;
        mask <<= 1;
        mask |= value_3 != -1 ? 1u : 0u;
    }

    ImGui::PopID();
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
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.parameter);
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
        ImGui::InputScalar("value", ImGuiDataType_S32, &action.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::action_type::affect:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::plus:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::minus:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::negate:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::multiplies:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::divides:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::modulus:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::bit_and:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::bit_or:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::bit_not:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
        break;
    case hsm_t::action_type::bit_xor:
        show_only_variable_widget(action.var1);
        show_variable_widget(action.var2, action.parameter);
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
        show_ports(condition.port, condition.mask);
        break;
    case hsm_t::condition_type::a_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_not_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_greater_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_less_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_greater_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_less_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_not_equal_to_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_greater_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_less_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_greater_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::b_less_equal_cst:
        ImGui::PushItemWidth(-1);
        ImGui::InputScalar("value", ImGuiDataType_S32, &condition.parameter);
        ImGui::PopItemWidth();
        break;
    case hsm_t::condition_type::a_equal_to_b:
        break;
    case hsm_t::condition_type::a_not_equal_to_b:
        break;
    case hsm_t::condition_type::a_greater_b:
        break;
    case hsm_t::condition_type::a_less_b:
        break;
    case hsm_t::condition_type::a_greater_equal_b:
        break;
    case hsm_t::condition_type::a_less_equal_b:
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
    (void)hsm.set_state(0);
}

void hsm_component_editor_data::show_hsm(hsm_component& hsm) noexcept
{
    for (u8 i = 0, e = static_cast<u8>(hsm_t::max_number_of_state); i != e;
         ++i) {
        if (m_enabled[i] == false)
            continue;

        ImNodes::BeginNode(make_state(i));
        ImNodes::BeginNodeTitleBar();
        ImGui::TextFormat("{}", i);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(make_input(i),
                                     ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("in");
        ImNodes::EndInputAttribute();

        show_condition(hsm.states[i].condition);

        // ImNodes::BeginOutputAttribute(
        //   make_output(i, transition_type::super_transition),
        //   ImNodesPinShape_CircleFilled);
        // ImGui::TextUnformatted("super");
        // ImNodes::EndOutputAttribute();
        ImNodes::BeginOutputAttribute(
          make_output(i, transition_type::if_transition),
          ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("if");
        ImNodes::EndOutputAttribute();
        ImNodes::BeginOutputAttribute(
          make_output(i, transition_type::else_transition),
          ImNodesPinShape_CircleFilled);
        ImGui::TextUnformatted("else");
        ImNodes::EndOutputAttribute();

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

        if (hsm.states[i].if_transition != hsm_t::invalid_state_id)
            ImNodes::Link(make_transition(i,
                                          hsm.states[i].if_transition,
                                          transition_type::if_transition),
                          make_output(i, transition_type::if_transition),
                          make_input(hsm.states[i].if_transition));

        if (hsm.states[i].else_transition != hsm_t::invalid_state_id)
            ImNodes::Link(make_transition(i,
                                          hsm.states[i].else_transition,
                                          transition_type::else_transition),
                          make_output(i, transition_type::else_transition),
                          make_input(hsm.states[i].else_transition));
    }
}

static void is_link_created(hsm_t& hsm) noexcept
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
            hsm.states[out.output].if_transition = in;
            break;
        case transition_type::else_transition:
            hsm.states[out.output].else_transition = in;
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

                if (hsm.top_state == hsm_t::invalid_state_id)
                    (void)hsm.set_state(id.value());
                else {
                    if (hsm.states[hsm.top_state].sub_id ==
                        hsm_t::invalid_state_id)
                        hsm.states[hsm.top_state].sub_id = id.value();
                    (void)hsm.set_state(id.value(), hsm.top_state);
                }

                // if (hsm.states[0].sub_id == hsm_t::invalid_state_id) {
                //     hsm.states[0].sub_id = id.value();
                // }

                // hsm.states[id.value()].super_id = 0u;
                // hsm.states[id.value()].sub_id   = hsm_t::invalid_state_id;
                m_position[id.value()] = click_pos;
                ImNodes::SetNodeScreenSpacePos(id.value(), click_pos);
            }
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

void hsm_component_editor_data::show_panel(hsm_component& hsm) noexcept
{
    if (ImGui::CollapsingHeader("parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Re-add a and b?");
        // ImGui::InputScalar("a", ImGuiDataType_S32, &m_hsm.a);
        // ImGui::InputScalar("b", ImGuiDataType_S32, &m_hsm.a);
    }

    if (ImGui::CollapsingHeader("selected states",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0, e = m_selected_nodes.Size; i != e; ++i) {
            const auto id    = get_state(m_selected_nodes[i]);
            auto&      state = hsm.states[id];

            ImGui::PushID(i);
            // show_state_id_editor(m_enabled, state.super_id);
            ImGui::TextFormat("State {}", m_selected_nodes[i]);

            ImGui::SeparatorText("Condition");
            show_state_condition(state.condition);

            ImGui::SeparatorText("Actions");
            ImGui::TextUnformatted("if condition is true");
            show_state_action(state.if_action);
            // show_state_id_editor(m_enabled, state.if_transition);
            ImGui::Separator();

            ImGui::TextUnformatted("else");
            show_state_action(state.else_action);
            // show_state_id_editor(m_enabled, state.else_transition);
            ImGui::Separator();

            ImGui::SeparatorText("Enter/Exit actions");
            ImGui::TextUnformatted("enter state run:");
            show_state_action(state.enter_action);

            ImGui::TextUnformatted("exit state run:");
            show_state_action(state.exit_action);

            ImGui::PopID();
            ImGui::Separator();
        }
    }

    ImGui::TextFormat("status: {}", test_status_string[ordinal(m_test)]);
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

        if (hsm.states[0].if_transition != hsm_t::invalid_state_id)
            stack.emplace_back(hsm.states[0].if_transition);
        if (hsm.states[0].else_transition != hsm_t::invalid_state_id)
            stack.emplace_back(hsm.states[0].else_transition);

        while (!stack.empty()) {
            auto top = stack.back();
            stack.pop_back();

            read[top] = true;

            if (hsm.states[top].if_transition != hsm_t::invalid_state_id &&
                read[hsm.states[top].if_transition] == false)
                stack.emplace_back(hsm.states[top].if_transition);
            if (hsm.states[top].else_transition != hsm_t::invalid_state_id &&
                read[hsm.states[top].else_transition] == false)
                stack.emplace_back(hsm.states[top].else_transition);
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

// static auto get(application& app,
//                 component_id cid) noexcept -> std::optional<hsm_component_id>
// {
//     if (auto* compo = app.mod.components.try_to_get(cid);
//         compo && compo->type == component_type::hsm) {

//         if (auto* hsm = app.mod.hsm_components.try_to_get(compo->id.hsm_id);
//             hsm) {
//             return compo->id.hsm_id;
//         }
//     }

//     return std::nullopt;
// }

// void hsm_component_editor_data::load(component_id c_id, model_id m_id)
// noexcept
//{
//     clear();
//
//     m_compo_id = c_id;
//     m_model_id = m_id;
//
//     if (auto* machine = get(*this, m_compo_id); machine) {
//         m_stack.push_back(0);
//
//         while (!m_stack.empty()) {
//             const auto idx = m_stack.back();
//             m_enabled[idx] = true;
//             m_stack.pop_back();
//
//             if (machine->states[idx].super_id != hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].super_id] == false)
//                     m_stack.push_back(machine->states[idx].super_id);
//
//             if (machine->states[idx].sub_id != hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].sub_id] == false)
//                     m_stack.push_back(machine->states[idx].sub_id);
//
//             if (machine->states[idx].if_transition !=
//             hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].if_transition] == false)
//                     m_stack.push_back(machine->states[idx].if_transition);
//
//             if (machine->states[idx].else_transition !=
//             hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].else_transition] == false)
//                     m_stack.push_back(machine->states[idx].else_transition);
//
//             m_hsm.states[idx] = machine->states[idx];
//         }
//     }
// }
//
// void hsm_component_editor_data::load(model_id m_id) noexcept
//{
//     m_compo_id = undefined<component_id>();
//     m_model_id = m_id;
//
//     if (auto* machine = get(*this, m_compo_id); machine) {
//         m_stack.push_back(0);
//
//         while (!m_stack.empty()) {
//             const auto idx = m_stack.back();
//             m_enabled[idx] = true;
//             m_stack.pop_back();
//
//             if (machine->states[idx].super_id != hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].super_id] == false)
//                     m_stack.push_back(machine->states[idx].super_id);
//
//             if (machine->states[idx].sub_id != hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].sub_id] == false)
//                     m_stack.push_back(machine->states[idx].sub_id);
//
//             if (machine->states[idx].if_transition !=
//             hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].if_transition] == false)
//                     m_stack.push_back(machine->states[idx].if_transition);
//
//             if (machine->states[idx].else_transition !=
//             hsm_t::invalid_state_id)
//                 if (m_enabled[machine->states[idx].else_transition] == false)
//                     m_stack.push_back(machine->states[idx].else_transition);
//
//             m_hsm.states[idx] = machine->states[idx];
//         }
//     }
// }
//
// void hsm_component_editor_data::save() noexcept
//{
//     if (auto* machine = get(*this, m_compo_id); machine)
//         *machine = m_hsm;
// }

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
            if (ImGui::BeginTable("editor",
                                  2,
                                  ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_NoSavedSettings |
                                    ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Editor",
                                        ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn(
                  "Parameter", ImGuiTableColumnFlags_WidthFixed, 128.f);

                ImGui::TableNextColumn();
                show_graph(*hsm);
                ImGui::TableNextColumn();
                show_panel(*hsm);
                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("log")) {
            ImGui::TextFormatDisabled("Not yet implemented.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void hsm_component_editor_data::show_selected_nodes(
  component_editor& /*ed*/) noexcept
{}

hsm_component_editor_data::hsm_component_editor_data(
  const component_id     id,
  const hsm_component_id hid,
  hsm_component&         hsm) noexcept
  : m_id(id)
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

    if (hsm.top_state != hierarchical_state_machine::invalid_state_id)
        m_enabled[hsm.top_state] = true;

    for (auto i = 0, e = length(hsm.states); i != e; ++i) {
        if (hsm.states[i].if_transition !=
            hierarchical_state_machine::invalid_state_id)
            m_enabled[hsm.states[i].if_transition] = true;
        if (hsm.states[i].else_transition !=
            hierarchical_state_machine::invalid_state_id)
            m_enabled[hsm.states[i].else_transition] = true;
        if (hsm.states[i].super_id !=
            hierarchical_state_machine::invalid_state_id)
            m_enabled[hsm.states[i].super_id] = true;
        if (hsm.states[i].sub_id !=
            hierarchical_state_machine::invalid_state_id)
            m_enabled[hsm.states[i].sub_id] = true;
    }
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
