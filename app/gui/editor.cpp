// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "editor.hpp"
#include "application.hpp"
#include "internal.hpp"

#include <utility>

namespace irt {

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 id,
                                 source::source_type& type) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept
{
    u64                 id   = src.id;
    source::source_type type = src.type;

    if (show_external_sources_combo(srcs, title, id, type)) {
        src.id   = id;
        src.type = type;
        return true;
    }

    return false;
}

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 i64&             integer_0,
                                 i64&             integer_1) noexcept
{
    debug::ensure(is_numeric_castable<u64>(integer_0));
    debug::ensure(std::cmp_less_equal(0, integer_1) &&
                  std::cmp_less_equal(integer_1, source::source_type_count));

    u64                 id   = static_cast<u64>(integer_0);
    source::source_type type = enum_cast<source::source_type>(integer_0);

    if (show_external_sources_combo(srcs, title, id, type)) {
        debug::ensure(is_numeric_castable<i64>(id));
        integer_0 = static_cast<i64>(id);
        integer_1 = ordinal(type);
        return true;
    }

    return false;
}

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 u64&                 src_id,
                                 source::source_type& src_type) noexcept
{
    bool             is_changed = false;
    small_string<63> label("None");

    switch (src_type) {
    case source::source_type::binary_file: {
        const auto id    = enum_cast<binary_file_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.binary_file_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::binary_file),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::constant: {
        const auto id    = enum_cast<constant_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.constant_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::constant),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::random: {
        const auto id    = enum_cast<random_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.random_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::random),
                   index,
                   es->name.c_str());
        }
    } break;

    case source::source_type::text_file: {
        const auto id    = enum_cast<text_file_source_id>(src_id);
        const auto index = get_index(id);
        if (auto* es = srcs.text_file_sources.try_to_get(id)) {
            format(label,
                   "{}-{} {}",
                   ordinal(source::source_type::text_file),
                   index,
                   es->name.c_str());
        }
    } break;
    default:
        unreachable();
    }

    if (ImGui::BeginCombo(title, label.c_str())) {
        {
            bool is_selected = src_type == source::source_type::constant;
            ImGui::Selectable("None", is_selected);
        }

        {
            constant_source* s = nullptr;
            while (srcs.constant_sources.next(s)) {
                const auto id    = srcs.constant_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::constant),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::constant &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::constant;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            binary_file_source* s = nullptr;
            while (srcs.binary_file_sources.next(s)) {
                const auto id    = srcs.binary_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::binary_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src_type == source::source_type::binary_file &&
                  src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::binary_file;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            random_source* s = nullptr;
            while (srcs.random_sources.next(s)) {
                const auto id    = srcs.random_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::random),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::random &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::random;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        {
            text_file_source* s = nullptr;
            while (srcs.text_file_sources.next(s)) {
                const auto id    = srcs.text_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(source::source_type::text_file),
                       index,
                       s->name.c_str());

                bool is_selected = src_type == source::source_type::text_file &&
                                   src_id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src_type   = source::source_type::text_file;
                    src_id     = ordinal(id);
                    is_changed = true;
                    ImGui::EndCombo();
                    break;
                }
            }
        }

        ImGui::EndCombo();
    }

    return is_changed;
}

/////////////////////////////////////////////////////////////////////

bool show_parameter(dynamics_counter_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_integrator_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss2_integrator_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss3_integrator_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_multiplier_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /* p */) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss1_sum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_sum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);

    return b1 or b2 or b3;
}

bool show_parameter(dynamics_qss1_sum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss1_wsum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("coeff-0", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-1", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss1_wsum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-0", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-1", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-2", &p.reals[5]);

    return b1 or b2 or b3 or b4 or b5 or b6;
}

bool show_parameter(dynamics_qss1_wsum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-0", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-1", &p.reals[5]);
    const auto b7 = ImGui::InputReal("coeff-2", &p.reals[6]);
    const auto b8 = ImGui::InputReal("coeff-3", &p.reals[7]);

    return b1 or b2 or b3 or b4 or b5 or b6 or b7 or b8;
}

bool show_parameter(dynamics_qss2_multiplier_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_sum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss2_sum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);

    return b1 or b2 or b3;
}

bool show_parameter(dynamics_qss2_sum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss2_wsum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("coeff-0", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-1", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss2_wsum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-0", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-1", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-2", &p.reals[5]);

    return b1 or b2 or b3 or b4 or b5 or b6;
}

bool show_parameter(dynamics_qss2_wsum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-0", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-1", &p.reals[5]);
    const auto b7 = ImGui::InputReal("coeff-2", &p.reals[6]);
    const auto b8 = ImGui::InputReal("coeff-3", &p.reals[7]);

    return b1 or b2 or b3 or b4 or b5 or b6 or b7 or b8;
}

bool show_parameter(dynamics_qss3_multiplier_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_sum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss3_sum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);

    return b1 or b2 or b3;
}

bool show_parameter(dynamics_qss3_sum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss3_wsum_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("coeff-0", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-1", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

bool show_parameter(dynamics_qss3_wsum_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-0", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-1", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-2", &p.reals[5]);

    return b1 or b2 or b3 or b4 or b5 or b6;
}

bool show_parameter(dynamics_qss3_wsum_4_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("value-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("value-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("value-3", &p.reals[3]);
    const auto b5 = ImGui::InputReal("coeff-0", &p.reals[4]);
    const auto b6 = ImGui::InputReal("coeff-1", &p.reals[5]);
    const auto b7 = ImGui::InputReal("coeff-2", &p.reals[6]);
    const auto b8 = ImGui::InputReal("coeff-3", &p.reals[7]);

    return b1 or b2 or b3 or b4 or b5 or b6 or b7 or b8;
}

bool show_parameter(dynamics_queue_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("delay", &p.reals[0]);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");

    return b1;
}

bool show_parameter(dynamics_dynamic_queue_tag,
                    application& /*app*/,
                    external_source& srcs,
                    parameter&       p) noexcept
{
    auto       stop_on_error = p.integers[0] != 0;
    const auto b1            = ImGui::Checkbox("Stop on error", &stop_on_error);
    if (b1)
        p.integers[0] = stop_on_error;

    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the dynamic queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    const auto b2 =
      show_external_sources_combo(srcs, "time", p.integers[1], p.integers[2]);

    return b1 or b2;
}

bool show_parameter(dynamics_priority_queue_tag,
                    application& /*app*/,
                    external_source& srcs,
                    parameter&       p) noexcept
{
    bool is_changed = false;
    bool value      = p.integers[0] != 0;

    if (ImGui::Checkbox("Stop on error", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    if (show_external_sources_combo(
          srcs, "time", p.integers[1], p.integers[2])) {
        is_changed = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_generator_tag,
                    application& /*app*/,
                    external_source& srcs,
                    parameter&       p) noexcept
{
    static const char* items[] = { "source", "external events" };

    auto flags      = bitflags<generator::option>(p.integers[0]);
    auto is_changed = false;

    auto combo_ta    = flags[generator::option::ta_use_source] ? 0 : 1;
    auto combo_value = flags[generator::option::value_use_source] ? 0 : 1;

    {
        auto ret = ImGui::Combo("ta", &combo_ta, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input ports.");

        if (ret) {
            flags.set(generator::option::ta_use_source, combo_ta == 0);
            is_changed = true;
        }
    }

    {
        auto ret = ImGui::Combo("value", &combo_value, items, length(items));
        ImGui::SameLine();
        HelpMarker(
          "`Source` means you need to setup external source like random "
          "number, input file etc. In `external events`, the value comes "
          "from the input port.");

        if (ret) {
            flags.set(generator::option::value_use_source, combo_value == 0);
            is_changed = true;
        }
    }

    if (is_changed)
        p.integers[0] = flags.to_unsigned();

    if (flags[generator::option::ta_use_source]) {
        auto stop_on_error = flags[generator::option::stop_on_error];

        if (show_external_sources_combo(
              srcs, "time", p.integers[1], p.integers[2]))
            is_changed = true;

        if (ImGui::InputReal("offset", &p.reals[0])) {
            p.reals[0] = p.reals[0] < 0.0 ? 0.0 : p.reals[0];
            is_changed = true;
        }

        if (ImGui::Checkbox("Stop on error", &stop_on_error)) {
            flags.set(generator::option::stop_on_error, stop_on_error);
            is_changed = true;
        }

        ImGui::SameLine();
        HelpMarker(
          "Unchecked, the generator stops to send data if the source are "
          "empty or undefined. Checked, the simulation will stop.");
    }

    if (flags[generator::option::value_use_source])
        if (show_external_sources_combo(
              srcs, "source", p.integers[3], p.integers[4]))
            is_changed = true;

    return is_changed;
}

bool show_parameter(dynamics_constant_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    static const char* type_names[] = { "constant",
                                        "incoming component all",
                                        "outcoming component all",
                                        "incoming component n",
                                        "outcoming component n" };

    bool is_changed = false;
    debug::ensure(
      std::cmp_equal(std::size(type_names), constant::init_type_count));

    if (ImGui::InputReal("value", &p.reals[0]))
        is_changed = true;

    if (ImGui::InputReal("offset", &p.reals[1]))
        is_changed = true;

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], constant::init_type_count));

    int i = static_cast<int>(p.integers[0]);
    if (ImGui::Combo("type", &i, type_names, length(type_names))) {
        p.integers[0] = i;
        is_changed    = true;
    }

    if (i == ordinal(constant::init_type::incoming_component_n) ||
        i == ordinal(constant::init_type::outcoming_component_n)) {
        int port = static_cast<int>(p.integers[1]);
        if (ImGui::SliderInt("port", &port, 0, 32)) {
            p.integers[1] = static_cast<i64>(port);
            is_changed    = true;
        }
    }

    return is_changed;
}

bool show_parameter(dynamics_qss1_cross_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss2_cross_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept

{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss3_cross_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = ImGui::InputReal("threshold", &p.reals[0]);

    bool value = p.integers[0] != 0;
    if (ImGui::Checkbox("up detection", &value)) {
        p.integers[0] = value ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_qss1_filter_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss2_filter_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss3_filter_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lowe threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

bool show_parameter(dynamics_qss1_power_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss2_power_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss3_power_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

bool show_parameter(dynamics_qss1_square_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss2_square_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_qss3_square_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_accumulator_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

bool show_parameter(dynamics_time_func_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    static const char* items[] = { "time", "square", "sin" };

    debug::ensure(std::cmp_less_equal(0, p.integers[0]) &&
                  std::cmp_less(p.integers[0], 3));

    bool is_changed = false;
    auto value      = static_cast<int>(p.integers[0]);

    ImGui::PushItemWidth(120.0f);
    if (ImGui::Combo("function", &value, items, IM_ARRAYSIZE(items))) {
        p.integers[0] = value;
        is_changed    = true;
    }
    ImGui::PopItemWidth();

    return is_changed;
}

bool show_parameter(dynamics_logical_and_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_or_2_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_and_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_or_3_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    bool is_changed = false;
    bool value_0    = p.integers[0] != 0;
    bool value_1    = p.integers[1] != 0;
    bool value_2    = p.integers[2] != 0;

    if (ImGui::Checkbox("value 1", &value_0)) {
        p.integers[0] = value_0 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 2", &value_1)) {
        p.integers[1] = value_1 ? 1 : 0;
        is_changed    = true;
    }

    if (ImGui::Checkbox("value 3", &value_2)) {
        p.integers[2] = value_2 ? 1 : 0;
        is_changed    = true;
    }

    return is_changed;
}

bool show_parameter(dynamics_logical_invert_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& /*p*/) noexcept
{
    return false;
}

static auto build_default_hsm_name(const modeling& mod, parameter& p) noexcept
  -> const char*
{
    static constexpr auto undefined_name = "-";

    auto compo =
      mod.components.try_to_get(enum_cast<component_id>(p.integers[0]));

    if (compo->type != component_type::hsm) {
        p.integers[0] = 0;
        compo         = nullptr;
    }

    return compo ? compo->name.c_str() : undefined_name;
}

bool show_hsm_parameter(const modeling& mod, parameter& p) noexcept
{
    int changed = false;

    if (ImGui::BeginCombo("hsm component", build_default_hsm_name(mod, p))) {
        auto imgui_id = 0;

        ImGui::PushID(imgui_id++);
        if (ImGui::Selectable("-", p.integers[0] == 0)) {
            p.integers[0] = 0;
            ++changed;
        }
        ImGui::PopID();

        for (const auto& c : mod.components) {
            if (c.type == component_type::hsm) {
                ImGui::PushID(imgui_id++);
                const auto c_id = static_cast<i64>(mod.components.get_id(c));
                if (ImGui::Selectable(c.name.c_str(), p.integers[0] == c_id)) {
                    p.integers[0] = c_id;
                    ++changed;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool show_parameter(dynamics_hsm_wrapper_tag,
                    application& /*app*/,
                    external_source& /*srcs*/,
                    parameter& p) noexcept
{
    int changed = false;

    changed += ImGui::InputScalar("i1", ImGuiDataType_S64, &p.integers[1]);
    changed += ImGui::InputScalar("i2", ImGuiDataType_S64, &p.integers[2]);
    changed += ImGui::InputDouble("r1", &p.reals[0]);
    changed += ImGui::InputDouble("r2", &p.reals[1]);
    changed += ImGui::InputDouble("timer", &p.reals[2]);

    return changed;
}

bool show_parameter_editor(application&    app,
                           project_editor& ed,
                           dynamics_type   type,
                           parameter&      p) noexcept
{
    return dispatcher(
      type,
      [](auto tag, application& app, project_editor& ed, parameter& p) noexcept
        -> bool { return show_parameter(tag, app, ed.pj.sim.srcs, p); },
      app,
      ed,
      p);
}

///////////////////////////////////////////////////////////

auto get_dynamics_input_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "x-dot", "reset" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "if", "else", "threshold"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "value", "set-t", "add-t", "mult_t"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_input_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "nb-1", "nb-2"
    };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_input_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 2> names = { "in-1", "in-2" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "in-1",
                                                           "in-2",
                                                           "in-3" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "in" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_input_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "in-1", "in-2", "in-3", "in-4"
    };

    return std::span{ names.data(), names.size() };
}

// Dynamics output names

auto get_dynamics_output_names(dynamics_qss1_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss1_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss2_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_integrator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_multiplier_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_cross_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "if-value",
                                                           "else-value",
                                                           "event" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_filter_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 3> names = { "value",
                                                           "up",
                                                           "down" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_power_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_square_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_sum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_qss3_wsum_4_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_counter_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_output_names(dynamics_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_dynamic_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_priority_queue_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_generator_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_constant_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_accumulator_2_tag) noexcept
  -> std::span<const std::string_view>
{
    return std::span<const std::string_view>{};
}

auto get_dynamics_output_names(dynamics_time_func_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_and_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_and_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_or_2_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_or_3_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_logical_invert_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 1> names = { "out" };

    return std::span{ names.data(), names.size() };
}

auto get_dynamics_output_names(dynamics_hsm_wrapper_tag) noexcept
  -> std::span<const std::string_view>
{
    static const std::array<std::string_view, 4> names = {
        "out-1", "out-2", "out-3", "out-4"
    };

    return std::span{ names.data(), names.size() };
}

} // irt
