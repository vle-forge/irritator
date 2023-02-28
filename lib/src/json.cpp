// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/modeling.hpp"
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

#include <limits>
#include <string_view>

namespace irt {

/*****************************************************************************
 *
 * Model file read part
 *
 ****************************************************************************/

static status get_double(const rapidjson::Value& value,
                         std::string_view        name,
                         double&                 data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsDouble()) {
        data = val->value.GetDouble();
        return status::success;
    }

    return status::io_file_format_error;
}

static status get_float(const rapidjson::Value& value,
                        std::string_view        name,
                        float&                  data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsDouble()) {
        constexpr double l =
          static_cast<double>(std::numeric_limits<float>::lowest());
        constexpr double m =
          static_cast<double>(std::numeric_limits<float>::max());

        auto temp = val->value.GetDouble();
        if (l <= temp && temp <= m) {
            data = static_cast<float>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_bool(const rapidjson::Value& value,
                       std::string_view        name,
                       bool&                   data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsBool()) {
        data = val->value.GetBool();
        return status::success;
    }

    return status::io_file_format_error;
}

static status get_u8(const rapidjson::Value& value,
                     std::string_view        name,
                     u8&                     data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsUint()) {
        auto temp = val->value.GetUint64();

        if (temp <= UINT8_MAX) {
            data = static_cast<u8>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_i16(const rapidjson::Value& value,
                      std::string_view        name,
                      i16&                    data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsInt()) {
        auto temp = val->value.GetInt64();

        if (INT16_MIN <= temp && temp <= UINT16_MAX) {
            data = static_cast<i16>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_u64(const rapidjson::Value& value,
                      std::string_view        name,
                      u64&                    data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsUint64()) {
        auto temp = val->value.GetUint64();

        if (temp <= UINT64_MAX) {
            data = static_cast<u64>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_i32(const rapidjson::Value& value,
                      std::string_view        name,
                      i32&                    data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsInt()) {
        auto temp = val->value.GetInt64();

        if (INT32_MIN <= temp && temp <= INT32_MAX) {
            data = static_cast<i32>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_u32(const rapidjson::Value& value,
                      std::string_view        name,
                      u32&                    data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsInt()) {
        auto temp = val->value.GetUint64();

        if (temp <= UINT32_MAX) {
            data = static_cast<u32>(temp);
            return status::success;
        }
    }

    return status::io_file_format_error;
}

static status get_hsm_action(
  const rapidjson::Value&                  value,
  std::string_view                         name,
  hierarchical_state_machine::action_type& data) noexcept
{
    u8 temp;

    irt_return_if_bad(get_u8(value, name, temp));
    irt_return_if_fail(temp < hierarchical_state_machine::action_type_count,
                       status::io_file_format_error);

    data = enum_cast<hierarchical_state_machine::action_type>(temp);
    return status::success;
}

static status get_hsm_variable(
  const rapidjson::Value&               value,
  std::string_view                      name,
  hierarchical_state_machine::variable& data) noexcept
{
    u8 temp;

    irt_return_if_bad(get_u8(value, name, temp));
    irt_return_if_fail(temp < hierarchical_state_machine::variable_count,
                       status::io_file_format_error);

    data = enum_cast<hierarchical_state_machine::variable>(data);
    return status::success;
}

static status get_hsm_condition(
  const rapidjson::Value&                     value,
  std::string_view                            name,
  hierarchical_state_machine::condition_type& data) noexcept
{
    u8 temp;

    irt_return_if_bad(get_u8(value, name, temp));
    irt_return_if_fail(temp < hierarchical_state_machine::condition_type_count,
                       status::io_file_format_error);

    data = enum_cast<hierarchical_state_machine::condition_type>(temp);
    return status::success;
}

static status get_string(const rapidjson::Value& value,
                         std::string_view        name,
                         std::string&            data) noexcept
{
    const auto str = rapidjson::GenericStringRef<char>(
      name.data(), static_cast<rapidjson::SizeType>(name.size()));
    const auto val = value.FindMember(str);

    if (val != value.MemberEnd() && val->value.IsString()) {
        data = val->value.GetString();
        return status::success;
    }

    return status::io_file_format_error;
}

template<int QssLevel>
status load(const rapidjson::Value&        val,
            abstract_integrator<QssLevel>& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "X", dyn.default_X));
    irt_return_if_bad(get_double(val, "dQ", dyn.default_dQ));

    return status::success;
}

template<typename Writer, int QssLevel>
void write(Writer& writer, const abstract_integrator<QssLevel>& dyn) noexcept
{
    writer.StartObject();
    writer.Key("X");
    writer.Double(dyn.default_X);
    writer.Key("dQ");
    writer.Double(dyn.default_dQ);
    writer.EndObject();
}

template<int QssLevel>
status load(const rapidjson::Value& /*val*/,
            abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{
    return status::success;
}

template<typename Writer, int QssLevel>
void write(Writer& writer,
           const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer, int QssLevel, int PortNumber>
void write(Writer& writer,
           const abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<int QssLevel, int PortNumber>
status load(const rapidjson::Value& /*val*/,
            const abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept
{
    return status::success;
}

template<int QssLevel>
status load(const rapidjson::Value&    val,
            abstract_wsum<QssLevel, 2> dyn) noexcept
{
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));

    return status::success;
}

template<int QssLevel>
status load(const rapidjson::Value&    val,
            abstract_wsum<QssLevel, 3> dyn) noexcept
{
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));

    return status::success;
}

template<int QssLevel>
status load(const rapidjson::Value&    val,
            abstract_wsum<QssLevel, 4> dyn) noexcept
{
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));
    irt_return_if_bad(get_double(val, "coeff-3", dyn.default_input_coeffs[3]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const qss1_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss1_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss1_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_sum_2& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_sum_3& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_sum_4& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_sum_2& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_sum_3& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_sum_4& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_wsum_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_wsum_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_wsum_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, integrator& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value", dyn.default_current_value));
    irt_return_if_bad(get_double(val, "reset", dyn.default_reset_value));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const integrator& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_current_value);
    writer.Key("reset");
    writer.Double(dyn.default_reset_value);
    writer.EndObject();
}

status load(const rapidjson::Value& val, quantifier& dyn) noexcept
{
    auto it = val.FindMember("adapt-state");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsString(), status::io_file_format_error);

    auto str = it->value.GetString();
    if (std::strcmp(str, "possible") == 0)
        dyn.default_adapt_state = quantifier::adapt_state::possible;
    else if (std ::strcmp(str, "impossible") == 0)
        dyn.default_adapt_state = quantifier::adapt_state::impossible;
    else
        dyn.default_adapt_state = quantifier::adapt_state::done;

    irt_return_if_bad(get_double(val, "step-size", dyn.default_step_size));
    irt_return_if_bad(get_i32(val, "past-length", dyn.default_past_length));
    irt_return_if_bad(
      get_bool(val, "zero-init-offset", dyn.default_zero_init_offset));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const quantifier& dyn) noexcept
{
    writer.StartObject();
    writer.Key("step-size");
    writer.Double(dyn.default_step_size);
    writer.Key("past-length");
    writer.Int(dyn.default_past_length);
    writer.Key("adapt-state");
    writer.String((dyn.default_adapt_state == quantifier::adapt_state::possible)
                    ? "possible "
                  : dyn.default_adapt_state ==
                      quantifier::adapt_state::impossible
                    ? "impossibe "
                    : "done ");
    writer.Key("zero-init-offset");
    writer.Bool(dyn.default_zero_init_offset);
    writer.EndObject();
}

status load(const rapidjson::Value& val, adder_2& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const adder_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, adder_3& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "value-2", dyn.default_values[2]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const adder_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, adder_4& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "value-2", dyn.default_values[2]));
    irt_return_if_bad(get_double(val, "value-3", dyn.default_values[3]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));
    irt_return_if_bad(get_double(val, "coeff-3", dyn.default_input_coeffs[3]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const adder_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_input_coeffs[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_input_coeffs[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_input_coeffs[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_input_coeffs[3]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, mult_2& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const mult_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, mult_3& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "value-2", dyn.default_values[2]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const mult_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.EndObject();
}

status load(const rapidjson::Value& val, mult_4& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_double(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_double(val, "value-2", dyn.default_values[2]));
    irt_return_if_bad(get_double(val, "value-3", dyn.default_values[3]));
    irt_return_if_bad(get_double(val, "coeff-0", dyn.default_input_coeffs[0]));
    irt_return_if_bad(get_double(val, "coeff-1", dyn.default_input_coeffs[1]));
    irt_return_if_bad(get_double(val, "coeff-2", dyn.default_input_coeffs[2]));
    irt_return_if_bad(get_double(val, "coeff-3", dyn.default_input_coeffs[3]));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const mult_4& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("value-3");
    writer.Double(dyn.default_values[3]);
    writer.Key("coeff-0");
    writer.Double(dyn.default_values[0]);
    writer.Key("coeff-1");
    writer.Double(dyn.default_values[1]);
    writer.Key("coeff-2");
    writer.Double(dyn.default_values[2]);
    writer.Key("coeff-3");
    writer.Double(dyn.default_values[3]);
    writer.EndObject();
}

status load(const rapidjson::Value& /*val*/, counter& /*dyn*/) noexcept
{
    return status::success;
}

template<typename Writer>
void write(Writer& writer, const counter& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

status load(const rapidjson::Value& val, queue& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "ta", dyn.default_ta));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.EndObject();
}

status load(const rapidjson::Value& val, dynamic_queue& dyn) noexcept
{
    i16 type{};

    irt_return_if_bad(get_i16(val, "source-ta-type", type));
    irt_return_if_bad(get_u64(val, "source-ta-id", dyn.default_source_ta.id));
    irt_return_if_bad(get_bool(val, "stop-on-error", dyn.stop_on_error));

    irt_return_if_fail(0 <= type && type < source::source_type_count,
                       status::io_file_format_error);

    dyn.default_source_ta.type = enum_cast<source::source_type>(type);

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const dynamic_queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

status load(const rapidjson::Value& val, priority_queue& dyn) noexcept
{
    i16 type{};

    irt_return_if_bad(get_double(val, "ta", dyn.default_ta));
    irt_return_if_bad(get_i16(val, "source-ta-type", type));
    irt_return_if_bad(get_u64(val, "source-ta-id", dyn.default_source_ta.id));
    irt_return_if_bad(get_bool(val, "stop-on-error", dyn.stop_on_error));

    irt_return_if_fail(0 <= type && type < source::source_type_count,
                       status::io_file_format_error);

    dyn.default_source_ta.type = enum_cast<source::source_type>(type);

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const priority_queue& dyn) noexcept
{
    writer.StartObject();
    writer.Key("ta");
    writer.Double(dyn.default_ta);
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

status load(const rapidjson::Value& val, generator& dyn) noexcept
{
    i16 type_ta{}, type_value{};

    irt_return_if_bad(get_double(val, "offset", dyn.default_offset));
    irt_return_if_bad(get_i16(val, "source-ta-type", type_ta));
    irt_return_if_bad(get_u64(val, "source-ta-id", dyn.default_source_ta.id));
    irt_return_if_bad(get_i16(val, "source-value-type", type_value));
    irt_return_if_bad(
      get_u64(val, "source-value-id", dyn.default_source_value.id));
    irt_return_if_bad(get_bool(val, "stop-on-error", dyn.stop_on_error));

    irt_return_if_fail(0 <= type_ta && type_ta < source::source_type_count,
                       status::io_file_format_error);

    irt_return_if_fail(0 <= type_value &&
                         type_value < source::source_type_count,
                       status::io_file_format_error);

    dyn.default_source_ta.type    = enum_cast<source::source_type>(type_ta);
    dyn.default_source_value.type = enum_cast<source::source_type>(type_value);

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const generator& dyn) noexcept
{
    writer.StartObject();
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    writer.Key("source-ta-type");
    writer.Int(ordinal(dyn.default_source_ta.type));
    writer.Key("source-ta-id");
    writer.Uint64(dyn.default_source_ta.id);
    writer.Key("source-value-type");
    writer.Int(ordinal(dyn.default_source_value.type));
    writer.Key("source-value-id");
    writer.Uint64(dyn.default_source_value.id);
    writer.Key("stop-on-error");
    writer.Bool(dyn.stop_on_error);
    writer.EndObject();
}

status load(const rapidjson::Value& val, constant& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "value", dyn.default_value));
    irt_return_if_bad(get_double(val, "offset", dyn.default_offset));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const constant& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value");
    writer.Double(dyn.default_value);
    writer.Key("offset");
    writer.Double(dyn.default_offset);
    writer.EndObject();
}

template<int QssLevel>
status load(const rapidjson::Value& val, abstract_cross<QssLevel>& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "threshold", dyn.default_threshold));
    irt_return_if_bad(get_bool(val, "detect-up", dyn.default_detect_up));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const qss1_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.Key("detect-up");
    writer.Bool(dyn.default_detect_up);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss1_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(std::isinf(dyn.default_lower_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(std::isinf(dyn.default_upper_threshold)
                    ? std::numeric_limits<double>::max()
                    : dyn.default_upper_threshold);
    writer.EndObject();
}

template<int QssLevel>
status load(const rapidjson::Value&    val,
            abstract_filter<QssLevel>& dyn) noexcept
{
    irt_return_if_bad(
      get_double(val, "lower-threshold", dyn.default_lower_threshold));
    irt_return_if_bad(
      get_double(val, "upper-threshold", dyn.default_upper_threshold));

    return status::success;
}

template<int QssLevel>
status load(const rapidjson::Value& val, abstract_power<QssLevel>& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "n", dyn.default_n));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const qss1_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss2_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const qss3_power& dyn) noexcept
{
    writer.StartObject();
    writer.Key("n");
    writer.Double(dyn.default_n);
    writer.EndObject();
}

template<int QssLevel>
status load(const rapidjson::Value& /*val*/,
            abstract_square<QssLevel>& /*dyn*/) noexcept
{
    return status::success;
}

template<typename Writer, int QssLevel>
void write(Writer& writer, const abstract_square<QssLevel>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

status load(const rapidjson::Value& val, cross& dyn) noexcept
{
    irt_return_if_bad(get_double(val, "threshold", dyn.default_threshold));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const cross& dyn) noexcept
{
    writer.StartObject();
    writer.Key("threshold");
    writer.Double(dyn.default_threshold);
    writer.EndObject();
}

template<size_t PortNumber>
status load(const rapidjson::Value& /*val*/,
            accumulator<PortNumber>& /*dyn*/) noexcept
{
    return status::success;
}

template<typename Writer, size_t PortNumber>
void write(Writer& writer, const accumulator<PortNumber>& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

status load(const rapidjson::Value& val, time_func& dyn) noexcept
{
    auto it = val.FindMember("function");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsString(), status::io_file_format_error);

    auto str = it->value.GetString();
    if (std::strcmp(str, "time") == 0)
        dyn.default_f = &time_function;
    else
        dyn.default_f = &square_time_function;

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const time_func& dyn) noexcept
{
    writer.StartObject();
    writer.Key("function");
    writer.String(dyn.default_f == &time_function ? "time" : "square");
    writer.EndObject();
}

status load(const rapidjson::Value& val, filter& dyn) noexcept
{
    irt_return_if_bad(
      get_double(val, "lower-threshold", dyn.default_lower_threshold));
    irt_return_if_bad(
      get_double(val, "upper-threshold", dyn.default_upper_threshold));

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const filter& dyn) noexcept
{
    writer.StartObject();
    writer.Key("lower-threshold");
    writer.Double(dyn.default_lower_threshold);
    writer.Key("upper-threshold");
    writer.Double(dyn.default_upper_threshold);
    writer.EndObject();
}

status load(const rapidjson::Value& val, logical_and_2& dyn) noexcept
{
    irt_return_if_bad(get_bool(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_bool(val, "value-1", dyn.default_values[1]));

    return status::success;
}

status load(const rapidjson::Value& val, logical_or_2& dyn) noexcept
{
    irt_return_if_bad(get_bool(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_bool(val, "value-1", dyn.default_values[1]));

    return status::success;
}

status load(const rapidjson::Value& val, logical_and_3& dyn) noexcept
{
    irt_return_if_bad(get_bool(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_bool(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_bool(val, "value-2", dyn.default_values[2]));

    return status::success;
}

status load(const rapidjson::Value& val, logical_or_3& dyn) noexcept
{
    irt_return_if_bad(get_bool(val, "value-0", dyn.default_values[0]));
    irt_return_if_bad(get_bool(val, "value-1", dyn.default_values[1]));
    irt_return_if_bad(get_bool(val, "value-2", dyn.default_values[2]));

    return status::success;
}

status load(const rapidjson::Value& /*val*/, logical_invert& /*dyn*/) noexcept
{
    return status::success;
}

status load(const rapidjson::Value&                   val,
            hierarchical_state_machine::state_action& state) noexcept
{
    irt_return_if_bad(get_i32(val, "parameter", state.parameter));
    irt_return_if_bad(get_hsm_variable(val, "var-1", state.var1));
    irt_return_if_bad(get_hsm_variable(val, "var-2", state.var2));
    irt_return_if_bad(get_hsm_action(val, "type", state.type));

    return status::success;
}

status load(const rapidjson::Value&                       val,
            hierarchical_state_machine::condition_action& cond) noexcept
{
    irt_return_if_bad(get_i32(val, "parameter", cond.parameter));
    irt_return_if_bad(get_hsm_condition(val, "type", cond.type));
    irt_return_if_bad(get_u8(val, "port", cond.port));
    irt_return_if_bad(get_u8(val, "mask", cond.mask));

    return status::success;
}

status load(const rapidjson::Value& val,
            hsm_wrapper& /*dyn*/,
            hierarchical_state_machine& hsm) noexcept
{
    auto states_it = val.FindMember("states");
    irt_return_if_fail(states_it != val.MemberEnd(),
                       status::io_file_format_error);
    irt_return_if_fail(states_it->value.IsArray(), status::io_filesystem_error);

    hsm.clear();

    constexpr auto length = hierarchical_state_machine::max_number_of_state;

    auto& states = states_it->value;
    for (rapidjson::SizeType i = 0, e = states.Size(); i != e; ++i) {
        u32 idx;
        irt_return_if_fail(states[i].IsObject(), status::io_file_format_error);

        irt_return_if_bad(get_u32(states[i], "id", idx));
        irt_return_if_fail(idx < length, status::io_file_format_error);

        auto enter = states[i].FindMember("enter");
        irt_return_if_fail(enter != states[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(enter->value.IsObject(),
                           status::io_file_format_error);
        irt_return_if_bad(
          load(enter->value.GetObject(), hsm.states[idx].enter_action));

        auto exit = states[i].FindMember("exit");
        irt_return_if_fail(exit != states[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(exit->value.IsObject(),
                           status::io_file_format_error);
        irt_return_if_bad(
          load(exit->value.GetObject(), hsm.states[idx].exit_action));

        auto if_action = states[i].FindMember("if");
        irt_return_if_fail(if_action != states[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(if_action->value.IsObject(),
                           status::io_file_format_error);
        irt_return_if_bad(
          load(if_action->value.GetObject(), hsm.states[idx].if_action));

        auto else_action = states[i].FindMember("else");
        irt_return_if_fail(else_action != states[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(else_action->value.IsObject(),
                           status::io_file_format_error);
        irt_return_if_bad(
          load(else_action->value.GetObject(), hsm.states[idx].else_action));

        auto condition = states[i].FindMember("condition");
        irt_return_if_fail(condition != states[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(condition->value.IsObject(),
                           status::io_file_format_error);
        irt_return_if_bad(
          load(condition->value.GetObject(), hsm.states[idx].condition));

        irt_return_if_bad(
          get_u8(val, "if-transition", hsm.states[idx].if_transition));
        irt_return_if_bad(
          get_u8(val, "else-transition", hsm.states[idx].else_transition));
        irt_return_if_bad(get_u8(val, "super-id", hsm.states[idx].super_id));
        irt_return_if_bad(get_u8(val, "sub-id", hsm.states[idx].sub_id));
    }

    auto outputs_it = val.FindMember("outputs");
    irt_return_if_fail(outputs_it != val.MemberEnd(),
                       status::io_file_format_error);
    irt_return_if_fail(outputs_it->value.IsArray(),
                       status::io_file_format_error);

    int i = 0;
    for (auto& elem : outputs_it->value.GetArray()) {
        irt_return_if_fail(elem.IsObject(), status::io_file_format_error);
        irt_return_if_bad(get_u8(elem, "port", hsm.outputs[i].port));
        irt_return_if_bad(get_i32(elem, "value", hsm.outputs[i].value));
        ++i;
    }

    return status::success;
}

template<typename Writer>
void write(Writer& writer, const logical_and_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const logical_and_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const logical_or_2& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const logical_or_3& dyn) noexcept
{
    writer.StartObject();
    writer.Key("value-0");
    writer.Bool(dyn.default_values[0]);
    writer.Key("value-1");
    writer.Bool(dyn.default_values[1]);
    writer.Key("value-2");
    writer.Bool(dyn.default_values[2]);
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer, const logical_invert& /*dyn*/) noexcept
{
    writer.StartObject();
    writer.EndObject();
}

template<typename Writer>
void write(Writer&                                         writer,
           std::string_view                                name,
           const hierarchical_state_machine::state_action& state) noexcept
{
    writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
    writer.StartObject();
    writer.Key("parameter");
    writer.Int(state.parameter);
    writer.Key("var-1");
    writer.Int(static_cast<int>(state.var1));
    writer.Key("var-2");
    writer.Int(static_cast<int>(state.var2));
    writer.Key("type");
    writer.Int(static_cast<int>(state.type));
    writer.EndObject();
}

template<typename Writer>
void write(Writer&                                             writer,
           std::string_view                                    name,
           const hierarchical_state_machine::condition_action& state) noexcept
{
    writer.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
    writer.StartObject();
    writer.Key("parameter");
    writer.Int(state.parameter);
    writer.Key("type");
    writer.Int(static_cast<int>(state.type));
    writer.Key("port");
    writer.Int(static_cast<int>(state.port));
    writer.Key("mask");
    writer.Int(static_cast<int>(state.mask));
    writer.EndObject();
}

template<typename Writer>
void write(Writer& writer,
           const hsm_wrapper& /*dyn*/,
           const hierarchical_state_machine& machine) noexcept
{
    writer.StartObject();
    writer.Key("states");
    writer.StartArray();

    constexpr auto length =
      to_unsigned(hierarchical_state_machine::max_number_of_state);
    constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

    std::array<bool, length> states_to_write;
    states_to_write.fill(false);

    for (unsigned i = 0; i != length; ++i) {
        if (machine.states[i].if_transition != invalid)
            states_to_write[machine.states[i].if_transition] = true;
        if (machine.states[i].else_transition != invalid)
            states_to_write[machine.states[i].else_transition] = true;
        if (machine.states[i].super_id != invalid)
            states_to_write[machine.states[i].super_id] = true;
        if (machine.states[i].sub_id != invalid)
            states_to_write[machine.states[i].sub_id] = true;
    }

    for (unsigned i = 0; i != length; ++i) {
        if (states_to_write[i]) {
            writer.Key("id");
            writer.Uint(i);
            write(writer, "enter", machine.states[i].enter_action);
            write(writer, "exit", machine.states[i].exit_action);
            write(writer, "if", machine.states[i].if_action);
            write(writer, "else", machine.states[i].else_action);
            write(writer, "condition", machine.states[i].condition);

            writer.Key("if-transition");
            writer.Int(machine.states[i].if_transition);
            writer.Key("else-transition");
            writer.Int(machine.states[i].else_transition);
            writer.Key("super-id");
            writer.Int(machine.states[i].super_id);
            writer.Key("sub-id");
            writer.Int(machine.states[i].sub_id);
        }
    }
    writer.EndArray();

    writer.Key("outputs");
    writer.StartArray();
    for (int i = 0, e = machine.outputs.ssize(); i != e; ++i) {
        writer.StartObject();
        writer.Key("port");
        writer.Int(machine.outputs[i].port);
        writer.Key("value");
        writer.Int(machine.outputs[i].value);
        writer.EndObject();
    }

    writer.EndArray();
    writer.EndObject();
}

void json_cache::clear() noexcept
{
    buffer.clear();
    string_buffer.clear();

    model_mapping.data.clear();
    constant_mapping.data.clear();
    binary_file_mapping.data.clear();
    random_mapping.data.clear();
    text_file_mapping.data.clear();
}

static status read_constant_sources(json_cache&             cache,
                                    external_source&        srcs,
                                    const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("constant-sources");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);
    irt_return_if_fail(srcs.constant_sources.can_alloc(it->value.Size()),
                       status::io_not_enough_memory);

    for (rapidjson::SizeType i = 0, e = it->value.Size(); i != e; ++i) {
        irt_return_if_fail(it->value[i].IsObject(),
                           status::io_file_format_error);

        u64 id = 0;
        irt_return_if_bad(get_u64(it->value[i], "id", id));

        auto data = it->value[i].FindMember("parameters");
        irt_return_if_fail(data != it->value[i].MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(data->value.IsArray(), status::io_file_format_error);

        auto& cst    = srcs.constant_sources.alloc();
        auto  cst_id = srcs.constant_sources.get_id(cst);

        for (rapidjson::SizeType j = 0, ej = data->value.Size(); j != ej; ++j) {
            irt_return_if_fail(data->value[i].IsObject(),
                               status::io_file_format_error);

            irt_return_if_fail(data->value[j].IsDouble(),
                               status::io_file_format_error);

            cst.buffer[j] = data->value[j].GetDouble();
        }

        cache.constant_mapping.data.emplace_back(id, ordinal(cst_id));
    }

    cache.constant_mapping.sort();

    return status::success;
}

static status read_binary_file_sources(json_cache&             cache,
                                       external_source&        srcs,
                                       const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("binary-file-sources");
    irt_return_if_fail(it != val.MemberEnd() && it->value.IsArray(),
                       status::io_file_format_error);

    irt_return_if_fail(srcs.binary_file_sources.can_alloc(it->value.Size()),
                       status::io_not_enough_memory);

    for (rapidjson::SizeType i = 0, e = it->value.Size(); i != e; ++i) {
        irt_return_if_fail(it->value[i].IsObject(),
                           status::io_file_format_error);
        u64 id          = 0;
        u32 max_clients = 0;

        irt_return_if_bad(get_u64(it->value[i], "id", id));
        irt_return_if_bad(get_u32(it->value[i], "max-clients", max_clients));
        irt_return_if_bad(
          get_string(it->value[i], "path", cache.string_buffer));

        auto& bin    = srcs.binary_file_sources.alloc();
        auto  bin_id = srcs.binary_file_sources.get_id(bin);

        bin.max_clients = max_clients;
        bin.file_path   = cache.string_buffer;
        cache.binary_file_mapping.data.emplace_back(id, ordinal(bin_id));
    }

    cache.binary_file_mapping.sort();

    return status::success;
}

static status read_text_file_sources(json_cache&             cache,
                                     external_source&        srcs,
                                     const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("text-file-sources");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);
    irt_return_if_fail(srcs.text_file_sources.can_alloc(it->value.Size()),
                       status::io_not_enough_memory);

    for (rapidjson::SizeType i = 0, e = it->value.Size(); i != e; ++i) {
        irt_return_if_fail(it->value[i].IsObject(),
                           status::io_file_format_error);

        u64 id = 0;

        irt_return_if_bad(get_u64(it->value[i], "id", id));
        irt_return_if_bad(
          get_string(it->value[i], "path", cache.string_buffer));

        auto& text    = srcs.text_file_sources.alloc();
        auto  text_id = srcs.text_file_sources.get_id(text);

        text.file_path = cache.string_buffer;
        cache.text_file_mapping.data.emplace_back(id, ordinal(text_id));
    }

    cache.text_file_mapping.sort();

    return status::success;
}

static status read_random_sources(json_cache&             cache,
                                  external_source&        srcs,
                                  const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("random-sources");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);
    irt_return_if_fail(srcs.random_sources.can_alloc(it->value.Size()),
                       status::io_not_enough_memory);

    for (rapidjson::SizeType i = 0, e = it->value.Size(); i != e; ++i) {
        irt_return_if_fail(it->value[i].IsObject(),
                           status::io_file_format_error);

        u64 id = 0;

        irt_return_if_bad(get_u64(it->value[i], "id", id));
        irt_return_if_bad(
          get_string(it->value[i], "type", cache.string_buffer));

        auto& rnd        = srcs.random_sources.alloc();
        auto  rnd_id     = srcs.random_sources.get_id(rnd);
        rnd.distribution = enum_cast<distribution_type>(rnd_id);

        switch (rnd.distribution) {
        case distribution_type::uniform_int:
            irt_return_if_bad(get_i32(it->value[i], "a", rnd.a32));
            irt_return_if_bad(get_i32(it->value[i], "b", rnd.b32));
            break;

        case distribution_type::uniform_real:
            irt_return_if_bad(get_double(it->value[i], "a", rnd.a));
            irt_return_if_bad(get_double(it->value[i], "b", rnd.b));
            break;

        case distribution_type::bernouilli:
            irt_return_if_bad(get_double(it->value[i], "p", rnd.p));
            break;

        case distribution_type::binomial:
            irt_return_if_bad(get_i32(it->value[i], "t", rnd.t32));
            irt_return_if_bad(get_double(it->value[i], "p", rnd.p));
            break;

        case distribution_type::negative_binomial:
            irt_return_if_bad(get_i32(it->value[i], "t", rnd.t32));
            irt_return_if_bad(get_double(it->value[i], "p", rnd.p));
            break;

        case distribution_type::geometric:
            irt_return_if_bad(get_double(it->value[i], "p", rnd.p));
            break;

        case distribution_type::poisson:
            irt_return_if_bad(get_double(it->value[i], "mean", rnd.mean));
            break;

        case distribution_type::exponential:
            irt_return_if_bad(get_double(it->value[i], "lambda", rnd.lambda));
            break;

        case distribution_type::gamma:
            irt_return_if_bad(get_double(it->value[i], "alpha", rnd.alpha));
            irt_return_if_bad(get_double(it->value[i], "beta", rnd.beta));
            break;

        case distribution_type::weibull:
            irt_return_if_bad(get_double(it->value[i], "a", rnd.a));
            irt_return_if_bad(get_double(it->value[i], "b", rnd.b));
            break;

        case distribution_type::exterme_value:
            irt_return_if_bad(get_double(it->value[i], "a", rnd.a));
            irt_return_if_bad(get_double(it->value[i], "b", rnd.b));
            break;

        case distribution_type::normal:
            irt_return_if_bad(get_double(it->value[i], "mean", rnd.mean));
            irt_return_if_bad(get_double(it->value[i], "stddev", rnd.stddev));
            break;

        case distribution_type::lognormal:
            irt_return_if_bad(get_double(it->value[i], "m", rnd.m));
            irt_return_if_bad(get_double(it->value[i], "s", rnd.s));
            break;

        case distribution_type::chi_squared:
            irt_return_if_bad(get_double(it->value[i], "n", rnd.n));
            break;

        case distribution_type::cauchy:
            irt_return_if_bad(get_double(it->value[i], "a", rnd.a));
            irt_return_if_bad(get_double(it->value[i], "a", rnd.b));
            break;

        case distribution_type::fisher_f:
            irt_return_if_bad(get_double(it->value[i], "m", rnd.m));
            irt_return_if_bad(get_double(it->value[i], "n", rnd.n));
            break;

        case distribution_type::student_t:
            irt_return_if_bad(get_double(it->value[i], "n", rnd.n));
            break;
        }

        cache.random_mapping.data.emplace_back(id, ordinal(rnd_id));
    }

    cache.random_mapping.sort();

    return status::success;
}

static status read_children(json_cache&             cache,
                            modeling&               mod,
                            simple_component&       compo,
                            const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("children");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);

    child* last = nullptr;

    for (auto& elem : it->value.GetArray()) {
        u64 id;

        irt_return_if_bad(get_string(elem, "type", cache.string_buffer));
        irt_return_if_bad(get_u64(elem, "id", id));

        if (cache.string_buffer == "component") {
            irt_return_if_bad(
              get_string(elem, "component-type", cache.string_buffer));

            const bool is_file = cache.string_buffer == "file";
            const bool is_cpp  = cache.string_buffer == "cpp";

            irt_return_if_fail(is_file || is_cpp, status::io_file_format_error);
            irt_return_if_bad(
              get_string(elem, "component-parameter", cache.string_buffer));

            auto  compo_id  = mod.search_component(cache.string_buffer.c_str(),
                                                 is_file ? "file" : "cpp");
            auto* sub_compo = mod.components.try_to_get(compo_id);
            irt_return_if_fail(!sub_compo, status::io_file_format_error);
            irt_return_if_fail(compo.children.can_alloc(),
                               status::io_not_enough_memory);

            auto& child = compo.children.alloc(compo_id);
            last        = &child;
        } else {
            auto opt_type = convert(cache.string_buffer);
            irt_return_if_fail(opt_type.has_value(),
                               status::io_file_format_error);

            auto dynamics_it = elem.FindMember("dynamics");
            irt_return_if_fail(dynamics_it != elem.MemberEnd(),
                               status::io_file_format_error);
            irt_return_if_fail(dynamics_it->value.IsObject(),
                               status::io_file_format_error);

            auto& child = mod.alloc(compo, opt_type.value());
            last        = &child;

            irt_assert(compo.models.try_to_get(enum_cast<model_id>(child.id)) !=
                       nullptr);

            auto& mdl = compo.models.get(enum_cast<model_id>(child.id));

            auto ret = dispatch(
              mdl,
              [&compo,
               &dynamics_it]<typename Dynamics>(Dynamics& dyn) -> status {
                  if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                      if (auto* hsm = compo.hsms.try_to_get(dyn.id); hsm)
                          return load(dynamics_it->value, dyn, *hsm);
                  } else {
                      return load(dynamics_it->value, dyn);
                  }

                  return status::io_file_format_error;
              });

            irt_return_if_bad(ret);
        }

        bool input  = false;
        bool output = false;

        irt_return_if_bad(get_float(elem, "x", last->x));
        irt_return_if_bad(get_float(elem, "y", last->y));
        irt_return_if_bad(get_bool(elem, "configurable", last->configurable));
        irt_return_if_bad(get_bool(elem, "observable", last->observable));
        irt_return_if_bad(get_bool(elem, "input", input));
        irt_return_if_bad(get_bool(elem, "output", output));

        const auto last_id = compo.children.get_id(last);
        cache.model_mapping.data.emplace_back(id, ordinal(last_id));
    }

    cache.model_mapping.sort();

    return status::success;
}

static status read_ports(simple_component&       compo,
                         const rapidjson::Value& val) noexcept
{
    {
        auto it = val.FindMember("x");
        irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
        irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);
        irt_return_if_fail(it->value.GetArray().Size() ==
                             simple_component::port_number,
                           status::io_file_format_error);

        unsigned i = 0;
        for (auto& elem : it->value.GetArray()) {
            irt_return_if_fail(elem.IsString(), status::io_file_format_error);
            compo.x_names[i] = elem.GetString();
            ++i;
        }
    }

    {
        auto it = val.FindMember("y");
        irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
        irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);
        irt_return_if_fail(it->value.GetArray().Size() ==
                             simple_component::port_number,
                           status::io_file_format_error);

        unsigned i = 0;
        for (auto& elem : it->value.GetArray()) {
            irt_return_if_fail(elem.IsString(), status::io_file_format_error);
            compo.y_names[i] = elem.GetString();
            ++i;
        }
    }

    return status::success;
}

static status read_connections(json_cache&             cache,
                               modeling&               mod,
                               simple_component&       compo,
                               const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("connections");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);

    for (auto& elem : it->value.GetArray()) {
        irt_return_if_bad(get_string(elem, "type", cache.string_buffer));

        if (cache.string_buffer == "internal") {
            u64 source, destination;
            i32 port_source, port_destination;

            irt_return_if_bad(get_u64(elem, "source", source));
            irt_return_if_bad(get_i32(elem, "port-source", port_source));
            irt_return_if_bad(get_u64(elem, "destination", destination));
            irt_return_if_bad(
              get_i32(elem, "port-destination", port_destination));

            auto* src_id = cache.model_mapping.get(source);
            auto* dst_id = cache.model_mapping.get(destination);

            irt_return_if_fail(src_id, status::io_file_format_model_unknown);
            irt_return_if_fail(dst_id, status::io_file_format_model_unknown);

            irt_return_if_fail(is_numeric_castable<i8>(port_source),
                               status::io_file_format_model_unknown);
            irt_return_if_fail(is_numeric_castable<i8>(port_destination),
                               status::io_file_format_model_unknown);

            irt_return_if_bad(mod.connect(compo,
                                          enum_cast<child_id>(*src_id),
                                          numeric_cast<i8>(port_source),
                                          enum_cast<child_id>(*dst_id),
                                          numeric_cast<i8>(port_destination)));
        } else if (cache.string_buffer == "input") {
            u64 destination;
            i32 port, port_destination;

            irt_return_if_bad(get_i32(elem, "port", port));
            irt_return_if_bad(get_u64(elem, "destination", destination));
            irt_return_if_bad(
              get_i32(elem, "port-destination", port_destination));

            auto* dst_id = cache.model_mapping.get(destination);
            irt_return_if_fail(dst_id, status::io_file_format_model_unknown);

            irt_return_if_fail(is_numeric_castable<i8>(port),
                               status::io_file_format_model_unknown);
            irt_return_if_fail(is_numeric_castable<i8>(port_destination),
                               status::io_file_format_model_unknown);

            irt_return_if_bad(
              mod.connect_input(compo,
                                static_cast<i8>(port),
                                enum_cast<child_id>(*dst_id),
                                static_cast<i8>(port_destination)));
        } else if (cache.string_buffer == "output") {
            u64 source;
            i32 port_source, port_destination;

            irt_return_if_bad(get_u64(elem, "source", source));
            irt_return_if_bad(get_i32(elem, "port-source", port_source));
            irt_return_if_bad(get_i32(elem, "port", port_destination));

            auto* src_id = cache.model_mapping.get(source);
            irt_return_if_fail(src_id, status::io_file_format_model_unknown);

            irt_return_if_fail(is_numeric_castable<i8>(port_source),
                               status::io_file_format_model_unknown);
            irt_return_if_fail(is_numeric_castable<i8>(port_destination),
                               status::io_file_format_model_unknown);

            irt_return_if_bad(
              mod.connect_output(compo,
                                 enum_cast<child_id>(*src_id),
                                 numeric_cast<i8>(port_source),
                                 numeric_cast<i8>(port_destination)));
        } else
            irt_bad_return(status::io_file_format_error);
    }

    return status::success;
}

static status do_read(json_cache&             cache,
                      modeling&               mod,
                      simple_component&       compo,
                      const rapidjson::Value& val) noexcept
{
    irt_return_if_bad(read_constant_sources(cache, mod.srcs, val));
    irt_return_if_bad(read_binary_file_sources(cache, mod.srcs, val));
    irt_return_if_bad(read_text_file_sources(cache, mod.srcs, val));
    irt_return_if_bad(read_random_sources(cache, mod.srcs, val));
    irt_return_if_bad(read_children(cache, mod, compo, val));
    irt_return_if_bad(read_ports(compo, val));
    irt_return_if_bad(read_connections(cache, mod, compo, val));

    compo.type  = component_type::file;
    compo.state = component_status::unmodified;

    return status::success;
}

status component_load(modeling&         mod,
                      simple_component& compo,
                      json_cache&       cache,
                      const char*       filename) noexcept
{
    file f{ filename, open_mode::read };

    irt_return_if_fail(f.is_open(), status::io_filesystem_error);
    auto* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();

    std::fseek(fp, 0, SEEK_END);
    auto filesize = std::ftell(fp);
    if (filesize <= 0)
        return status::io_filesystem_error;

    std::fseek(fp, 0, SEEK_SET);

    cache.buffer.resize(static_cast<int>(filesize + 1));
    auto read_length =
      std::fread(cache.buffer.data(), 1, static_cast<sz>(filesize), fp);
    cache.buffer[static_cast<int>(read_length)] = '\0';
    f.close();

    rapidjson::Document    d;
    rapidjson::ParseResult s = d.ParseInsitu(cache.buffer.data());

    irt_return_if_fail(s && d.IsObject(), status::io_file_format_error);
    return do_read(cache, mod, compo, d.GetObject());
}

template<typename Writer>
static status write_constant_sources(json_cache& /*cache*/,
                                     const external_source& srcs,
                                     Writer&                w) noexcept
{
    w.Key("constant-sources");
    w.StartArray();

    constant_source* src = nullptr;
    while (srcs.constant_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.constant_sources.get_id(*src)));
        w.Key("parameters");

        w.StartArray();
        for (const auto elem : src->buffer)
            w.Double(elem);
        w.EndArray();

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_binary_file_sources(json_cache& /*cache*/,
                                        const external_source& srcs,
                                        Writer&                w) noexcept
{
    w.Key("binary-file-sources");
    w.StartArray();

    binary_file_source* src = nullptr;
    while (srcs.binary_file_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.binary_file_sources.get_id(*src)));
        w.Key("max-clients");
        w.Uint(src->max_clients);
        w.Key("path");
        w.String(src->file_path.string().c_str());
        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_text_file_sources(json_cache& /*cache*/,
                                      const external_source& srcs,
                                      Writer&                w) noexcept
{
    w.Key("text-file-sources");
    w.StartArray();

    text_file_source* src = nullptr;
    while (srcs.text_file_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.text_file_sources.get_id(*src)));
        w.Key("path");
        w.String(src->file_path.string().c_str());
        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_random_sources(json_cache& /*cache*/,
                                   const external_source& srcs,
                                   Writer&                w) noexcept
{
    w.Key("random-sources");
    w.StartArray();

    random_source* src = nullptr;
    while (srcs.random_sources.next(src)) {
        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(srcs.random_sources.get_id(*src)));
        w.Key("type");
        w.String(distribution_str(src->distribution));

        switch (src->distribution) {
        case distribution_type::uniform_int:
            w.Key("a");
            w.Int(src->a32);
            w.Key("b");
            w.Int(src->b32);
            break;

        case distribution_type::uniform_real:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::bernouilli:
            w.Key("p");
            w.Double(src->p);
            ;
            break;

        case distribution_type::binomial:
            w.Key("t");
            w.Int(src->t32);
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::negative_binomial:
            w.Key("t");
            w.Int(src->t32);
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::geometric:
            w.Key("p");
            w.Double(src->p);
            break;

        case distribution_type::poisson:
            w.Key("mean");
            w.Double(src->mean);
            break;

        case distribution_type::exponential:
            w.Key("lambda");
            w.Double(src->lambda);
            break;

        case distribution_type::gamma:
            w.Key("alpha");
            w.Double(src->alpha);
            w.Key("beta");
            w.Double(src->beta);
            break;

        case distribution_type::weibull:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::exterme_value:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::normal:
            w.Key("mean");
            w.Double(src->mean);
            w.Key("stddev");
            w.Double(src->stddev);
            break;

        case distribution_type::lognormal:
            w.Key("m");
            w.Double(src->m);
            w.Key("s");
            w.Double(src->s);
            break;

        case distribution_type::chi_squared:
            w.Key("n");
            w.Double(src->n);
            break;

        case distribution_type::cauchy:
            w.Key("a");
            w.Double(src->a);
            w.Key("b");
            w.Double(src->b);
            break;

        case distribution_type::fisher_f:
            w.Key("m");
            w.Double(src->m);
            w.Key("n");
            w.Double(src->n);
            break;

        case distribution_type::student_t:
            w.Key("n");
            w.Double(src->n);
            break;
        }

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_children(json_cache& /*cache*/,
                             const modeling&         mod,
                             const simple_component& compo,
                             Writer&                 w) noexcept
{
    w.Key("children");
    w.StartArray();

    child* c = nullptr;
    while (compo.children.next(c)) {
        w.StartObject();
        const auto id = get_index(compo.children.get_id(*c));
        w.Key("id");
        w.Uint64(id);
        w.Key("x");
        w.Double(static_cast<double>(c->x));
        w.Key("y");
        w.Double(static_cast<double>(c->y));
        w.Key("configurable");
        w.Bool(c->configurable);
        w.Key("observable");
        w.Bool(c->observable);
        w.Key("input");
        w.Bool(false);
        w.Key("output");
        w.Bool(false);

        if (!c->name.empty()) {
            w.Key("name");
            w.String(c->name.c_str());
        }

        if (c->type == child_type::component) {
            w.Key("type");
            w.String("component");

            auto  cpn_id = enum_cast<component_id>(c->id);
            auto* cpn    = mod.components.try_to_get(cpn_id);
            irt_assert(cpn && "cleanup all component vectors before save");

            if (match(
                  compo.type, component_type::file, component_type::memory)) {
                w.Key("component-type");
                w.String("file");
                w.Key("component-parameter");
                w.String(mod.file_paths.get(compo.file).path.c_str());
            } else {
                w.Key("component-type");
                w.String("cpp");
                w.Key("component-parameter");
                w.String(component_type_names[ordinal(compo.type)]);
            }

        } else {
            auto  mdl_id = enum_cast<model_id>(c->id);
            auto* mdl    = compo.models.try_to_get(mdl_id);
            irt_assert(mdl && "cleanup all component vectors before save");

            w.Key("type");
            w.String(dynamics_type_names[ordinal(mdl->type)]);

            w.Key("dynamics");
            dispatch(*mdl,
                     [&compo, &w]<typename Dynamics>(Dynamics& dyn) -> void {
                         if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                             auto* hsm = compo.hsms.try_to_get(dyn.id);
                             irt_assert(hsm);
                             write(w, dyn, *hsm);
                         } else {
                             write(w, dyn);
                         }
                     });
        }

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_ports(json_cache& /*cache*/,
                          const modeling& /*mod*/,
                          const simple_component& compo,
                          Writer&                 w) noexcept
{
    w.Key("x");
    w.StartArray();

    for (int i = 0; i != simple_component::port_number; ++i)
        w.String(compo.x_names[static_cast<unsigned>(i)].c_str());

    w.EndArray();

    w.Key("y");
    w.StartArray();

    for (int i = 0; i != simple_component::port_number; ++i)
        w.String(compo.y_names[static_cast<unsigned>(i)].c_str());

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_connections(json_cache& /*cache*/,
                                const modeling& /*mod*/,
                                const simple_component& compo,
                                Writer&                 w) noexcept
{
    w.Key("connections");
    w.StartArray();

    connection* c = nullptr;
    while (compo.connections.next(c)) {
        w.StartObject();

        w.Key("type");

        switch (c->type) {
        case connection::connection_type::input:
            w.String("input");
            w.Key("port");
            w.Int(c->input.index);
            w.Key("destination");
            w.Uint(get_index(c->input.dst));
            w.Key("port-destination");
            w.Int(c->input.index_dst);
            break;
        case connection::connection_type::internal:
            w.String("internal");
            w.Key("source");
            w.Uint(get_index(c->internal.src));
            w.Key("port-source");
            w.Int(c->internal.index_src);
            w.Key("destination");
            w.Uint(get_index(c->internal.dst));
            w.Key("port-destination");
            w.Int(c->internal.index_dst);
            break;
        case connection::connection_type::output:
            w.String("output");
            w.Key("source");
            w.Uint(get_index(c->output.src));
            w.Key("port-source");
            w.Int(c->output.index_src);
            w.Key("port");
            w.Int(c->output.index);
            break;
        }

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

status component_save(const modeling&         mod,
                      const simple_component& compo,
                      json_cache&             cache,
                      const char*             filename,
                      json_pretty_print /*print_options*/) noexcept
{
    file f{ filename, open_mode::write };

    irt_return_if_fail(f.is_open(), status::io_file_format_error);

    FILE* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    w.StartObject();

    irt_return_if_bad(write_constant_sources(cache, mod.srcs, w));
    irt_return_if_bad(write_binary_file_sources(cache, mod.srcs, w));
    irt_return_if_bad(write_text_file_sources(cache, mod.srcs, w));
    irt_return_if_bad(write_random_sources(cache, mod.srcs, w));
    irt_return_if_bad(write_children(cache, mod, compo, w));
    irt_return_if_bad(write_ports(cache, mod, compo, w));
    irt_return_if_bad(write_connections(cache, mod, compo, w));

    w.EndObject();

    return status::success;
}

/*****************************************************************************
 *
 * Simulation file read part
 *
 ****************************************************************************/

template<typename Writer>
static status write_model(const simulation& sim, Writer& w) noexcept
{
    w.Key("models");
    w.StartArray();

    model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        const auto mdl_id = sim.models.get_id(*mdl);

        w.StartObject();
        w.Key("id");
        w.Uint64(ordinal(mdl_id));
        w.Key("type");
        w.String(dynamics_type_names[ordinal(mdl->type)]);
        w.Key("dynamics");

        dispatch(*mdl, [&w, &sim]<typename Dynamics>(Dynamics& dyn) -> void {
            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                auto* hsm = sim.hsms.try_to_get(dyn.id);
                irt_assert(hsm);
                write(w, dyn, *hsm);
            } else {
                write(w, dyn);
            }
        });

        w.EndObject();
    }

    w.EndArray();

    return status::success;
}

template<typename Writer>
static status write_connections(const simulation& sim, Writer& w) noexcept
{
    w.Key("connections");
    w.StartArray();

    model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        dispatch(*mdl, [&sim, &mdl, &w]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_output_port<Dynamics>) {
                for (auto i = 0, e = length(dyn.y); i != e; ++i) {
                    auto list = get_node(sim, dyn.y[i]);
                    for (const auto& cnt : list) {
                        auto* dst = sim.models.try_to_get(cnt.model);
                        if (dst) {
                            w.StartObject();
                            w.Key("source");
                            w.Uint64(ordinal(sim.models.get_id(*mdl)));
                            w.Key("port-source");
                            w.Uint64(static_cast<u64>(i));
                            w.Key("destination");
                            w.Uint64(ordinal(cnt.model));
                            w.Key("port-destination");
                            w.Uint64(static_cast<u64>(cnt.port_index));
                            w.EndObject();
                        }
                    }
                }
            }
        });
    }

    w.EndArray();
    return status::success;
}

template<typename Writer>
status do_simulation_save(Writer&           w,
                          const simulation& sim,
                          json_cache&       cache) noexcept
{
    w.StartObject();

    irt_return_if_bad(write_constant_sources(cache, sim.srcs, w));
    irt_return_if_bad(write_binary_file_sources(cache, sim.srcs, w));
    irt_return_if_bad(write_text_file_sources(cache, sim.srcs, w));
    irt_return_if_bad(write_random_sources(cache, sim.srcs, w));
    irt_return_if_bad(write_model(sim, w));
    irt_return_if_bad(write_connections(sim, w));

    w.EndObject();

    return status::success;
}

status simulation_save(const simulation& sim,
                       json_cache&       cache,
                       const char*       filename,
                       json_pretty_print /*print_option*/) noexcept
{
    file f{ filename, open_mode::write };
    irt_return_if_fail(f.is_open(), status::io_filesystem_error);

    FILE* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    irt_return_if_bad(do_simulation_save(w, sim, cache));

    return status::success;
}

status simulation_save(const simulation& sim,
                       json_cache&       cache,
                       vector<char>&     out,
                       json_pretty_print print_option) noexcept
{
    rapidjson::StringBuffer buffer;

    switch (print_option) {
    case json_pretty_print::indent_2: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }

    case json_pretty_print::indent_2_one_line_array: {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> w(buffer);
        w.SetIndent(' ', 2);
        w.SetFormatOptions(rapidjson::kFormatSingleLineArray);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }

    default: {
        rapidjson::Writer<rapidjson::StringBuffer> w(buffer);
        irt_return_if_bad(do_simulation_save(w, sim, cache));
        break;
    }
    }

    auto length = buffer.GetSize();
    auto str    = buffer.GetString();
    out.resize(static_cast<int>(length + 1));
    std::copy_n(str, length, out.data());
    out.back() = '\0';

    return status::success;
}

static status read_model(json_cache&             cache,
                         simulation&             sim,
                         const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("models");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);

    for (auto& elem : it->value.GetArray()) {
        u64 id;

        irt_return_if_bad(get_string(elem, "type", cache.string_buffer));
        irt_return_if_bad(get_u64(elem, "id", id));

        auto opt_type = convert(cache.string_buffer);
        irt_return_if_fail(opt_type.has_value(),
                           status::io_file_format_model_unknown);

        irt_return_if_fail(sim.models.can_alloc(),
                           status::io_not_enough_memory);

        auto& mdl    = sim.alloc(opt_type.value());
        auto  mdl_id = sim.models.get_id(mdl);

        auto dynamics_it = elem.FindMember("dynamics");
        irt_return_if_fail(dynamics_it != elem.MemberEnd(),
                           status::io_file_format_error);
        irt_return_if_fail(dynamics_it->value.IsObject(),
                           status::io_file_format_error);

        auto ret = dispatch(
          mdl,
          [&sim, &dynamics_it]<typename Dynamics>(Dynamics& dyn) -> status {
              if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                  if (auto* hsm = sim.hsms.try_to_get(dyn.id); hsm)
                      return load(dynamics_it->value, dyn, *hsm);
              } else {
                  return load(dynamics_it->value, dyn);
              }

              return status::io_file_format_error;
          });

        irt_return_if_bad(ret);

        cache.model_mapping.data.emplace_back(id, ordinal(mdl_id));
    }

    cache.model_mapping.sort();

    return status::success;
}

static status read_connections(json_cache&             cache,
                               simulation&             sim,
                               const rapidjson::Value& val) noexcept
{
    auto it = val.FindMember("connections");
    irt_return_if_fail(it != val.MemberEnd(), status::io_file_format_error);
    irt_return_if_fail(it->value.IsArray(), status::io_file_format_error);

    for (auto& elem : it->value.GetArray()) {
        u64 source, destination;
        i32 port_source, port_destination;

        irt_return_if_bad(get_u64(elem, "source", source));
        irt_return_if_bad(get_i32(elem, "port-source", port_source));
        irt_return_if_bad(get_u64(elem, "destination", destination));
        irt_return_if_bad(get_i32(elem, "port-destination", port_destination));

        auto* mdl_src_id = cache.model_mapping.get(source);
        irt_return_if_fail(mdl_src_id, status::io_file_format_model_unknown);

        auto* mdl_src = sim.models.try_to_get(enum_cast<model_id>(*mdl_src_id));
        irt_return_if_fail(mdl_src, status::io_file_format_model_unknown);

        auto* mdl_dst_id = cache.model_mapping.get(destination);
        irt_return_if_fail(mdl_dst_id, status::io_file_format_model_unknown);

        auto* mdl_dst = sim.models.try_to_get(enum_cast<model_id>(*mdl_dst_id));
        irt_return_if_fail(mdl_dst, status::io_file_format_model_unknown);

        output_port* out = nullptr;
        input_port*  in  = nullptr;

        irt_return_if_bad(
          get_output_port(*mdl_src, static_cast<int>(port_source), out));
        irt_return_if_bad(
          get_input_port(*mdl_dst, static_cast<int>(port_destination), in));
        irt_return_if_bad(
          sim.connect(*mdl_src, port_source, *mdl_dst, port_destination));
    }

    return status::success;
}

static status do_read(json_cache&             cache,
                      simulation&             sim,
                      const rapidjson::Value& val) noexcept
{
    irt_return_if_bad(read_constant_sources(cache, sim.srcs, val));
    irt_return_if_bad(read_binary_file_sources(cache, sim.srcs, val));
    irt_return_if_bad(read_text_file_sources(cache, sim.srcs, val));
    irt_return_if_bad(read_random_sources(cache, sim.srcs, val));
    irt_return_if_bad(read_model(cache, sim, val));
    irt_return_if_bad(read_connections(cache, sim, val));

    return status::success;
}

status simulation_load(simulation& sim,
                       json_cache& cache,
                       const char* filename) noexcept
{
    file f{ filename, open_mode::read };
    irt_return_if_fail(f.is_open(), status::io_file_format_error);

    auto* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();

    std::fseek(fp, 0, SEEK_END);
    auto filesize = std::ftell(fp);
    irt_return_if_fail(filesize >= 0, status::io_file_format_error);
    std::fseek(fp, 0, SEEK_SET);

    cache.buffer.resize(static_cast<int>(filesize + 1));
    auto read_length =
      std::fread(cache.buffer.data(), 1, static_cast<sz>(filesize), fp);
    cache.buffer[static_cast<int>(read_length)] = '\0';
    f.close();

    rapidjson::Document    d;
    rapidjson::ParseResult s = d.ParseInsitu(cache.buffer.data());

    irt_return_if_fail(s, status::io_file_format_error);
    irt_return_if_fail(d.IsObject(), status::io_file_format_error);

    return do_read(cache, sim, d.GetObject());
}

status simulation_load(simulation&     sim,
                       json_cache&     cache,
                       std::span<char> in) noexcept
{
    cache.clear();

    rapidjson::Document    d;
    rapidjson::ParseResult s = d.ParseInsitu(in.data());

    irt_return_if_fail(s, status::io_file_format_error);
    irt_return_if_fail(d.IsObject(), status::io_file_format_error);

    return do_read(cache, sim, d.GetObject());
}

/*****************************************************************************
 *
 * Project file read part
 *
 ****************************************************************************/

static bool load_component_path(modeling&          mod,
                                std::string_view   path,
                                registred_path_id& reg_id) noexcept
{
    bool found = false;

    for (auto id : mod.component_repertories) {
        if (auto* reg = mod.registred_paths.try_to_get(id); reg) {
            if (reg->name == path) {
                reg_id = id;
                found  = true;
                break;
            }
        }
    }

    return found;
}

static bool load_component_directory(modeling&             mod,
                                     std::string_view      path,
                                     const registred_path& reg,
                                     dir_path_id&          dir_id) noexcept
{
    bool found = false;

    for (auto id : reg.children) {
        if (auto* dir = mod.dir_paths.try_to_get(id); dir) {
            if (dir->path == path) {
                dir_id = id;
                found  = true;
                break;
            }
        }
    }

    return found;
}

static bool load_component_file(modeling&        mod,
                                std::string_view path,
                                const dir_path&  dir,
                                file_path_id&    file_id) noexcept
{
    bool found = false;

    for (auto id : dir.children) {
        if (auto* file = mod.file_paths.try_to_get(id); file) {
            if (file->path == path) {
                file_id = id;
                found   = true;
                break;
            }
        }
    }

    return found;
}

static status load_access(modeling&               mod,
                          json_cache&             cache,
                          const rapidjson::Value& val,
                          model*&                 mdl) noexcept
{
    cache.stack.clear();

    irt_return_if_fail(val.IsArray(), status::io_project_file_error);

    for (rapidjson::SizeType i = 0, e = val.Size(); i != e; ++i) {
        irt_return_if_fail(val[i].IsInt(), status::io_file_format_error);
        cache.stack.emplace_back(val[i].GetInt());
    }

    auto& head       = mod.tree_nodes.get(mod.head);
    auto& compo_head = mod.components.get(head.id);
    auto* compo      = &compo_head;

    irt_return_if_fail(!cache.stack.empty(), status::io_file_format_error);

    for (i32 i = 0, e = cache.stack.ssize(); i != e; ++i) {
        auto* child_id = compo->child_mapping_io.get(cache.stack[i]);
        irt_return_if_fail(child_id, status::io_file_format_error);

        auto* child = compo->children.try_to_get(*child_id);
        irt_return_if_fail(child, status::io_file_format_error);

        if (i + 1 < e) {
            irt_return_if_fail(child->type == child_type::model,
                               status::io_file_format_error);

            auto id = enum_cast<component_id>(child->id);
            compo   = mod.components.try_to_get(id);
            irt_return_if_fail(compo, status::io_file_format_error);
        } else {
            irt_return_if_fail(child->type != child_type::component,
                               status::io_file_format_error);

            auto id = enum_cast<model_id>(child->id);
            mdl     = compo->models.try_to_get(id);
            irt_return_if_fail(mdl, status::io_file_format_error);
            break;
        }
    }

    irt_return_if_fail(mdl, status::io_file_format_error);
    return status::success;
}

static status load_parameter(modeling&               mod,
                             dynamics_type           type,
                             const rapidjson::Value& val,
                             model&                  mdl) noexcept
{
    irt_return_if_fail(val.IsObject(), status::io_file_format_error);
    irt_return_if_fail(mdl.type == type, status::io_file_format_error);

    auto& head       = mod.tree_nodes.get(mod.head);
    auto& compo_head = mod.components.get(head.id);
    auto* compo      = &compo_head;

    return dispatch(mdl,
                    [&val, &compo]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                            auto* machine = compo->hsms.try_to_get(dyn.id);
                            if (machine)
                                return load(val, dyn, *machine);
                        } else {
                            return load(val, dyn);
                        }

                        return status::io_file_format_error;
                    });
}

static status load_project_parameters(json_cache              cache,
                                      modeling&               mod,
                                      const rapidjson::Value& top)
{
    auto param_it = top.FindMember("component-parameters");
    irt_return_if_fail(param_it != top.MemberEnd() && param_it->value.IsArray(),
                       status::io_project_file_parameters_error);

    for (auto& elm : param_it->value.GetArray()) {
        if (elm.IsObject()) {
            model* mdl = nullptr;

            auto access_it = elm.FindMember("access");
            irt_return_if_fail(access_it != elm.MemberEnd(),
                               status::io_project_file_parameters_access_error);
            irt_return_if_bad_map(
              load_access(mod, cache, access_it->value, mdl),
              status::io_project_file_parameters_access_error);

            irt_return_if_bad_map(
              get_string(elm, "type", cache.string_buffer),
              status::io_project_file_parameters_type_error);

            auto opt_type = convert(cache.string_buffer);
            irt_return_if_fail(opt_type.has_value(),
                               status::io_project_file_parameters_init_error);

            auto parameter_it = elm.FindMember("parameter");
            irt_return_if_fail(parameter_it != elm.MemberEnd(),
                               status::io_project_file_parameters_init_error);
            irt_return_if_bad_map(
              load_parameter(mod, opt_type.value(), parameter_it->value, *mdl),
              status::io_project_file_parameters_init_error);
        }
    }

    return status::success;
}

static status load_file_project(json_cache              cache,
                                modeling&               mod,
                                const rapidjson::Value& top) noexcept
{
    registred_path_id reg_id  = undefined<registred_path_id>();
    dir_path_id       dir_id  = undefined<dir_path_id>();
    file_path_id      file_id = undefined<file_path_id>();

    irt_return_if_bad_map(
      get_string(top, "component-path", cache.string_buffer),
      status::io_project_file_component_path_error);
    irt_return_if_fail(load_component_path(mod, cache.string_buffer, reg_id),
                       status::io_project_file_component_path_error);

    irt_return_if_bad_map(
      get_string(top, "component-directory", cache.string_buffer),
      status::io_project_file_component_directory_error);
    irt_return_if_fail(
      load_component_directory(
        mod, cache.string_buffer, mod.registred_paths.get(reg_id), dir_id),
      status::io_project_file_component_directory_error);

    irt_return_if_bad_map(
      get_string(top, "component-file", cache.string_buffer),
      status::io_project_file_component_file_error);
    irt_return_if_fail(
      load_component_file(
        mod, cache.string_buffer, mod.dir_paths.get(dir_id), file_id),
      status::io_project_file_component_file_error);

    auto* fp    = mod.file_paths.try_to_get(file_id);
    auto& compo = mod.components.get(fp->component);
    mod.init_project(compo);

    auto param_it = top.FindMember("component-parameters");
    irt_return_if_fail(param_it != top.MemberEnd() && param_it->value.IsArray(),
                       status::io_project_file_parameters_error);

    return load_project_parameters(cache, mod, top);
}

static status load_internal_project(json_cache              cache,
                                    modeling&               mod,
                                    const rapidjson::Value& top,
                                    std::string_view        name) noexcept
{
    auto type_opt = get_component_type(name);
    irt_return_if_fail(type_opt.has_value(),
                       status::io_project_component_type_error);

    auto  compo_id = mod.search_internal_component(type_opt.value());
    auto& compo    = mod.components.get(compo_id);
    mod.init_project(compo);

    auto param_it = top.FindMember("component-parameters");
    irt_return_if_fail(param_it != top.MemberEnd() && param_it->value.IsArray(),
                       status::io_project_file_parameters_error);

    return load_project_parameters(cache, mod, top);
}

static status load_project(json_cache              cache,
                           modeling&               mod,
                           const rapidjson::Value& value) noexcept
{
    irt_return_if_fail(value.IsObject(), status::io_project_file_error);
    const auto& top = value.GetObject();

    irt_return_if_bad_map(
      get_string(top, "component-type", cache.string_buffer),
      status::io_project_file_component_path_error);

    return cache.string_buffer == "file"
             ? load_file_project(cache, mod, top)
             : load_internal_project(cache, mod, top, cache.string_buffer);
}

status project_load(modeling&   mod,
                    json_cache& cache,
                    const char* filename) noexcept
{
    file f{ filename, open_mode::read };
    irt_return_if_fail(f.is_open(), status::io_filesystem_error);

    auto* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();

    std::fseek(fp, 0, SEEK_END);
    auto filesize = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);

    cache.buffer.resize(static_cast<int>(filesize + 1));
    auto read_length =
      std::fread(cache.buffer.data(), 1, static_cast<sz>(filesize), fp);
    cache.buffer[static_cast<int>(read_length)] = '\0';
    f.close();

    rapidjson::Document    d;
    rapidjson::ParseResult s = d.ParseInsitu(cache.buffer.data());
    irt_return_if_fail(s, status::io_file_format_error);

    mod.clear_project();
    return load_project(cache, mod, d.GetObject());
}

status modeling::load_project(const char* filename) noexcept
{
    json_cache cache;
    auto       ret = project_load(*this, cache, filename);

    if (is_success(ret))
        state = modeling_status::unmodified;

    return ret;
}

/*****************************************************************************
 *
 * project file write part
 *
 ****************************************************************************/

void write_node(rapidjson::PrettyWriter<rapidjson::FileWriteStream>& w,
                modeling&                                            mod,
                tree_node& parent) noexcept
{
    tree_node* tree = nullptr;
    while (mod.tree_nodes.next(tree)) {
        auto& compo = mod.components.get(tree->id);

        for (auto pair : tree->parameters.data) {
            auto* src = compo.models.try_to_get(pair.id);
            auto* dst = compo.models.try_to_get(pair.value);

            if (src && dst) {
                irt_assert(src->type == dst->type);

                w.StartObject();

                w.Key("access");
                w.StartArray();

                {
                    auto* p = parent.tree.get_parent();
                    while (p) {
                        w.Int(static_cast<int>(get_index(p->id_in_parent)));
                        p = p->tree.get_parent();
                    }
                }

                w.EndArray();

                w.Key("type");
                w.String(dynamics_type_names[ordinal(dst->type)]);
                w.Key("dynamics");

                dispatch(
                  *dst, [&compo, &w]<typename Dynamics>(Dynamics& dyn) -> void {
                      if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                          auto* machine = compo.hsms.try_to_get(dyn.id);
                          if (machine)
                              write(w, dyn, *machine);
                      } else {
                          write(w, dyn);
                      }
                  });
            }
        }
    }
}

status modeling::save_project(const char* filename) noexcept
{
    json_cache  cache;
    irt::status ret = project_save(*this, cache, filename);

    if (is_success(ret))
        state = modeling_status::unmodified;

    return ret;
}

static status project_save_file_component(modeling&         mod,
                                          json_cache&       cache,
                                          tree_node&        parent,
                                          simple_component& compo,
                                          file&             f) noexcept
{
    auto* reg  = mod.registred_paths.try_to_get(compo.reg_path);
    auto* dir  = mod.dir_paths.try_to_get(compo.dir);
    auto* file = mod.file_paths.try_to_get(compo.file);
    irt_return_if_fail(reg && dir && file, status::io_filesystem_error);

    auto* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    w.StartObject();
    w.Key("component-type");
    w.String("file", 4);

    w.Key("component-path");
    w.String(reg->name.c_str(), static_cast<u32>(reg->name.size()), false);

    w.Key("component-directory");
    w.String(dir->path.begin(), static_cast<u32>(dir->path.size()), false);

    w.Key("component-file");
    w.String(file->path.begin(), static_cast<u32>(file->path.size()), false);

    w.Key("component-parameters");
    w.StartArray();
    write_node(w, mod, parent);
    w.EndArray();
    w.EndObject();

    return status::success;
}

static status project_save_internal_component(modeling&         mod,
                                              json_cache&       cache,
                                              tree_node&        parent,
                                              simple_component& compo,
                                              file&             f) noexcept
{
    auto* fp = reinterpret_cast<FILE*>(f.get_handle());
    cache.clear();
    cache.buffer.resize(4096);

    rapidjson::FileWriteStream os(fp, cache.buffer.data(), cache.buffer.size());
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    w.StartObject();
    w.Key("component-type");
    w.String(component_type_names[ordinal(compo.type)]);

    w.Key("component-parameters");
    w.StartArray();
    write_node(w, mod, parent);
    w.EndArray();
    w.EndObject();

    return status::success;
}

status project_save(modeling&   mod,
                    json_cache& cache,
                    const char* filename,
                    json_pretty_print /*print_options*/) noexcept
{
    auto* parent = mod.tree_nodes.try_to_get(mod.head);
    irt_return_if_fail(parent, status::io_project_component_empty);

    file f{ filename, open_mode::write };
    irt_return_if_fail(f.is_open(), status::io_project_file_error);

    auto& compo = mod.components.get(parent->id);
    irt_return_if_fail(compo.type != component_type::memory,
                       status::io_project_component_type_error);

    return compo.type == component_type::file
             ? project_save_file_component(mod, cache, *parent, compo, f)
             : project_save_internal_component(mod, cache, *parent, compo, f);
}

} //  irt
