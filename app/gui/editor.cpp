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
                                 source::source_type& src_type,
                                 source::id_type&     src_id) noexcept;

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 source&          src) noexcept
{
    source::id_type     id   = src.id;
    source::source_type type = src.type;

    if (show_external_sources_combo(srcs, title, type, id)) {
        src.id   = id;
        src.type = type;
        return true;
    }

    return false;
}

bool show_external_sources_combo(external_source& srcs,
                                 const char*      title,
                                 i64&             integer_type,
                                 i64&             integer_id) noexcept
{
    debug::ensure(is_numeric_castable<u64>(integer_id));
    debug::ensure(std::cmp_less_equal(0, integer_type) &&
                  std::cmp_less_equal(integer_type, source::source_type_count));

    auto  src  = get_source(integer_type, integer_id);
    auto& id   = src.id;
    auto& type = src.type;

    if (show_external_sources_combo(srcs, title, type, id)) {
        std::tie(integer_type, integer_id) = source_to_parameters(src);
        return true;
    }

    return false;
}

static void build_selected_source_label(const source::source_type src_type,
                                        const source::id_type     src_id,
                                        const external_source&    srcs,
                                        small_string<63>& label) noexcept
{
    switch (src_type) {
    case source::source_type::binary_file: {
        const auto id    = src_id.binary_file_id;
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
        const auto id    = src_id.constant_id;
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
        const auto id    = src_id.random_id;
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
        const auto id    = src_id.text_file_id;
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
}

bool show_external_sources_combo(external_source&     srcs,
                                 const char*          title,
                                 source::source_type& src_type,
                                 source::id_type&     src_id) noexcept
{
    bool             is_changed = false;
    small_string<63> label("-");
    build_selected_source_label(src_type, src_id, srcs, label);

    if (ImGui::BeginCombo(title, label.c_str())) {
        {
            bool is_selected = src_type == source::source_type::constant;
            ImGui::Selectable("-", is_selected);
        }

        for (const auto& s : srcs.constant_sources) {
            const auto id    = srcs.constant_sources.get_id(s);
            const auto index = get_index(id);

            format(label, "{} (constant)##{}", s.name.sv(), index);

            bool is_selected = src_type == source::source_type::constant &&
                               src_id.constant_id == id;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                src_type           = source::source_type::constant;
                src_id.constant_id = id;
                is_changed         = true;
            }
        }

        for (const auto& s : srcs.binary_file_sources) {
            const auto id    = srcs.binary_file_sources.get_id(s);
            const auto index = get_index(id);

            format(label, "{} (bin)##{}", s.name.sv(), index);

            bool is_selected = src_type == source::source_type::binary_file &&
                               src_id.binary_file_id == id;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                src_type              = source::source_type::binary_file;
                src_id.binary_file_id = id;
                is_changed            = true;
            }
        }

        for (const auto& s : srcs.text_file_sources) {
            const auto id    = srcs.text_file_sources.get_id(s);
            const auto index = get_index(id);

            format(label, "{} (text)##{}", s.name.sv(), index);

            bool is_selected = src_type == source::source_type::text_file &&
                               src_id.text_file_id == id;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                src_type            = source::source_type::text_file;
                src_id.text_file_id = id;
                is_changed          = true;
            }
        }

        for (const auto& s : srcs.random_sources) {
            const auto id    = srcs.random_sources.get_id(s);
            const auto index = get_index(id);

            format(label, "{} (random)##{}", s.name.sv(), index);

            bool is_selected =
              src_type == source::source_type::random && src_id.random_id == id;
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                src_type         = source::source_type::random;
                src_id.random_id = id;
                is_changed       = true;
            }
        }

        ImGui::EndCombo();
    }

    return is_changed;
}

/////////////////////////////////////////////////////////////////////

static bool show_parameter(qss_log_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_exp_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_sin_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_cos_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_integer_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_compare_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("a < b", &p.reals[0]);
    const auto b2 = ImGui::InputReal("not a < b", &p.reals[1]);

    return b1 or b2;
}

static bool show_parameter(counter_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_integrator_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("value", &p.reals[0]);
    const auto b2 = ImGui::InputReal("dQ", &p.reals[1]);

    return b1 or b2;
}

static bool show_parameter(qss_multiplier_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_sum_2_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_sum_3_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_sum_4_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_wsum_2_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("coeff-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("coeff-1", &p.reals[1]);

    return b1 or b2;
}

static bool show_parameter(qss_wsum_3_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("coeff-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("coeff-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("coeff-2", &p.reals[2]);

    return b1 or b2 or b3;
}

static bool show_parameter(qss_wsum_4_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("coeff-0", &p.reals[0]);
    const auto b2 = ImGui::InputReal("coeff-1", &p.reals[1]);
    const auto b3 = ImGui::InputReal("coeff-2", &p.reals[2]);
    const auto b4 = ImGui::InputReal("coeff-3", &p.reals[3]);

    return b1 or b2 or b3 or b4;
}

static bool show_parameter(queue_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    auto value = p.reals[0];

    if (ImGui::InputReal("delay", &value)) {
        if (not std::isnormal(value) or not std::signbit(value)) {
            p.reals[0] = value;
            return true;
        }
    }

    return false;
}

static bool show_parameter(dynamic_queue_tag,
                           application& /*app*/,
                           external_source& srcs,
                           parameter&       p) noexcept
{
    return show_external_sources_combo(
      srcs, "time", p.integers[0], p.integers[1]);
}

static bool show_parameter(priority_queue_tag,
                           application& /*app*/,
                           external_source& srcs,
                           parameter&       p) noexcept
{
    return show_external_sources_combo(
      srcs, "time", p.integers[0], p.integers[1]);
}

static bool show_parameter(generator_tag,
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
        if (show_external_sources_combo(
              srcs, "time", p.integers[1], p.integers[2]))
            is_changed = true;

        if (ImGui::InputReal("offset", &p.reals[0])) {
            p.reals[0] = p.reals[0] < 0.0 ? 0.0 : p.reals[0];
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

static bool show_parameter(constant_tag,
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

    return is_changed;
}

static const char* get_selected_input_name(const component& c, const port_id p)
{
    return c.x.exists(p) ? c.x.get<port_str>(p).c_str() : "-";
}

static const char* get_selected_output_name(const component& c, const port_id p)
{
    return c.y.exists(p) ? c.y.get<port_str>(p).c_str() : "-";
}

bool show_extented_constant_parameter(const modeling&    mod,
                                      const component_id id,
                                      parameter&         p) noexcept
{
    int ret = false;

    if (const auto* c = mod.components.try_to_get<component>(id)) {
        const auto type = enum_cast<constant::init_type>(p.integers[0]);
        const auto port = enum_cast<port_id>(p.integers[1]);

        if (type == constant::init_type::incoming_component_n) {
            const auto selected      = c->x.exists(port);
            const auto selected_name = get_selected_input_name(*c, port);

            if (ImGui::BeginCombo("input port", selected_name)) {
                if (ImGui::Selectable("-", not selected)) {
                    p.integers[1] = 0;
                    ++ret;
                }

                c->x.for_each<port_str>([&](const auto id, const auto& name) {
                    if (ImGui::Selectable(name.c_str(),
                                          p.integers[1] == ordinal(id))) {
                        p.integers[1] = ordinal(id);
                        ++ret;
                    }
                });

                ImGui::EndCombo();
            }
        } else if (type == constant::init_type::outcoming_component_n) {
            const auto selected      = c->y.exists(port);
            const auto selected_name = get_selected_output_name(*c, port);

            if (ImGui::BeginCombo("output port", selected_name)) {
                if (ImGui::Selectable("-", not selected)) {
                    p.integers[1] = 0;
                    ++ret;
                }

                c->y.for_each<port_str>([&](const auto id, const auto& name) {
                    if (ImGui::Selectable(name.c_str(),
                                          p.integers[1] == ordinal(id))) {
                        p.integers[1] = ordinal(id);
                        ++ret;
                    }
                });

                ImGui::EndCombo();
            }
        }
    }

    return ret;
}

static bool show_parameter(qss_invert_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_cross_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    return ImGui::InputReal("threshold", &p.reals[0]);
}

static bool show_parameter(qss_flipflop_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(qss_filter_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    const auto b1 = ImGui::InputReal("lower threshold", &p.reals[0]);
    const auto b2 = ImGui::InputReal("upper threshold", &p.reals[1]);

    return b1 or b2;
}

static bool show_parameter(qss_power_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& p) noexcept
{
    return ImGui::InputReal("n", &p.reals[0]);
}

static bool show_parameter(qss_square_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(accumulator_2_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

static bool show_parameter(time_func_tag,
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

static bool show_parameter(logical_and_2_tag,
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

static bool show_parameter(logical_or_2_tag,
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

static bool show_parameter(logical_and_3_tag,
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

static bool show_parameter(logical_or_3_tag,
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

static bool show_parameter(logical_invert_tag,
                           application& /*app*/,
                           external_source& /*srcs*/,
                           parameter& /*p*/) noexcept
{
    return false;
}

bool show_extented_hsm_parameter(const application& app, parameter& p) noexcept
{
    const auto param_compo_id = enum_cast<component_id>(p.integers[0]);
    const auto compo_id =
      is_defined(param_compo_id) and app.mod.components.exists(param_compo_id)
        ? param_compo_id
        : undefined<component_id>();

    const auto ret = app.component_sel.combobox(
      "hsm component", component_type::hsm, compo_id);
    if (ret.is_done) {
        p.integers[0] = ordinal(ret.id);
        return true;
    }

    return false;
}

static bool show_parameter(hsm_wrapper_tag,
                           application& /*app*/,
                           external_source& srcs,
                           parameter&       p) noexcept
{
    int changed = false;

    changed += ImGui::InputScalar("i1", ImGuiDataType_S64, &p.integers[1]);
    changed += ImGui::InputScalar("i2", ImGuiDataType_S64, &p.integers[2]);
    changed += ImGui::InputDouble("r1", &p.reals[0]);
    changed += ImGui::InputDouble("r2", &p.reals[1]);
    changed += ImGui::InputDouble("timer", &p.reals[2]);

    changed +=
      show_external_sources_combo(srcs, "value", p.integers[3], p.integers[4]);

    return changed;
}

bool show_parameter_editor(application&     app,
                           external_source& srcs,
                           dynamics_type    type,
                           parameter&       p) noexcept
{
    return dispatch(
      type,
      [](const auto       tag,
         application&     app,
         external_source& srcs,
         parameter&       p) noexcept -> bool {
          return show_parameter(tag, app, srcs, p);
      },
      app,
      srcs,
      p);
}

} // irt
