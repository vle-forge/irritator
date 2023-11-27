// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include "parameter.hpp"

#include <optional>
#include <utility>

namespace irt {

static auto model_init(const parameter& param, integrator& dyn) noexcept
  -> status
{
    dyn.default_current_value = param.reals[0];
    dyn.default_reset_value   = param.reals[1];

    return success();
}

static auto parameter_init(parameter& param, const integrator& dyn) noexcept
  -> void
{
    param.reals[0] = dyn.default_current_value;
    param.reals[1] = dyn.default_reset_value;
}

static auto model_init(const parameter& param, quantifier& dyn) noexcept
  -> status
{
    dyn.default_step_size   = param.reals[0];
    dyn.default_past_length = static_cast<int>(param.integers[0]);

    return success();
}

static auto parameter_init(parameter& param, const quantifier& dyn) noexcept
  -> void
{
    param.reals[0]    = dyn.default_step_size;
    param.integers[1] = dyn.default_past_length;
}

template<int QssLevel>
static auto model_init(const parameter&               param,
                       abstract_integrator<QssLevel>& dyn) noexcept -> status
{
    dyn.default_X  = param.reals[0];
    dyn.default_dQ = param.reals[1];

    return success();
}

template<int PortNumber>
static auto model_init(const parameter& param, adder<PortNumber>& dyn) noexcept
  -> status
{
    dyn.default_input_coeffs[0] = param.reals[0];
    dyn.default_input_coeffs[1] = param.reals[1];

    if constexpr (PortNumber > 2)
        dyn.default_input_coeffs[2] = param.reals[2];

    if constexpr (PortNumber > 3)
        dyn.default_input_coeffs[3] = param.reals[3];

    return success();
}

template<int PortNumber>
static auto parameter_init(parameter&               param,
                           const adder<PortNumber>& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_input_coeffs[0];
    param.reals[1] = dyn.default_input_coeffs[1];

    if constexpr (PortNumber > 2)
        param.reals[2] = dyn.default_input_coeffs[2];

    if constexpr (PortNumber > 3)
        param.reals[3] = dyn.default_input_coeffs[3];
}

template<int PortNumber>
static auto model_init(const parameter& param, mult<PortNumber>& dyn) noexcept
  -> status
{
    dyn.default_input_coeffs[0] = param.reals[0];
    dyn.default_input_coeffs[1] = param.reals[1];

    if constexpr (PortNumber > 2)
        dyn.default_input_coeffs[2] = param.reals[2];

    if constexpr (PortNumber > 3)
        dyn.default_input_coeffs[3] = param.reals[3];

    return success();
}

template<int PortNumber>
static auto parameter_init(parameter&              param,
                           const mult<PortNumber>& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_input_coeffs[0];
    param.reals[1] = dyn.default_input_coeffs[1];

    if constexpr (PortNumber > 2)
        param.reals[2] = dyn.default_input_coeffs[2];

    if constexpr (PortNumber > 3)
        param.reals[3] = dyn.default_input_coeffs[3];
}

static auto model_init(const parameter& param, cross& dyn) noexcept -> status
{
    dyn.default_threshold = param.reals[0];

    return success();
}

static auto parameter_init(parameter& param, const cross& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_threshold;
}

static auto model_init(const parameter& param, filter& dyn) noexcept -> status
{
    dyn.default_lower_threshold = param.reals[0];
    dyn.default_upper_threshold = param.reals[1];

    return success();
}

static auto parameter_init(parameter& param, const filter& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_lower_threshold;
    param.reals[1] = dyn.default_upper_threshold;
}

static auto model_init(const parameter& /*param*/, counter& /*dyn*/) noexcept
  -> status
{
    return success();
}

static auto parameter_init(parameter& /*param*/,
                           const counter& /*dyn*/) noexcept -> void
{
}

static auto model_init(const parameter& param, constant& dyn) noexcept -> status
{
    dyn.default_value  = param.reals[0];
    dyn.default_offset = param.reals[1];

    if (!(0 <= param.integers[0] && param.integers[0] < 5))
        return new_error(modeling::error::unknown_type_constant);

    dyn.type = enum_cast<constant::init_type>(param.integers[0]);
    dyn.port = param.integers[1];

    return success();
}

static auto parameter_init(parameter& param, const constant& dyn) noexcept
  -> void
{
    param.reals[0]    = dyn.default_value;
    param.reals[1]    = dyn.default_offset;
    param.integers[0] = ordinal(dyn.type);
    param.integers[1] = dyn.port;
}

template<int PortNumber>
static auto model_init(const parameter& /*param*/,
                       accumulator<PortNumber>& /*dyn*/) noexcept -> status
{
    return success();
}

template<int PortNumber>
static auto parameter_init(parameter& /*param*/,
                           const accumulator<PortNumber>& /*dyn*/) noexcept
  -> void
{
}

static auto model_init(const parameter& param, queue& dyn) noexcept -> status
{
    dyn.default_ta = param.reals[0];

    return success();
}

static auto parameter_init(parameter& param, const queue& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_ta;
}

static auto model_init(const parameter& param, dynamic_queue& dyn) noexcept
  -> status
{
    dyn.stop_on_error        = param.integers[0] != 0;
    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);

    if (0 <= param.integers[2] && param.integers[2] < 4)
        return new_error(modeling::error::unknown_source_type);

    dyn.default_source_ta.type =
      enum_cast<source::source_type>(param.integers[2]);

    return success();
}

static auto parameter_init(parameter& param, const dynamic_queue& dyn) noexcept
  -> void
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
}

static auto model_init(const parameter& param, priority_queue& dyn) noexcept
  -> status
{
    dyn.stop_on_error        = param.integers[0] != 0;
    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);

    if (0 <= param.integers[2] && param.integers[2] < 4)
        return new_error(modeling::error::unknown_source_type);

    dyn.default_source_ta.type =
      enum_cast<source::source_type>(param.integers[2]);

    return success();
}

static auto parameter_init(parameter& param, const priority_queue& dyn) noexcept
  -> void
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
}

static auto model_init(const parameter& param, generator& dyn) noexcept
  -> status
{
    dyn.default_offset = param.reals[0];
    dyn.stop_on_error  = param.integers[0] != 0;

    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);
    if (0 <= param.integers[2] && param.integers[2] < 4)
        return new_error(modeling::error::unknown_source_type);
    dyn.default_source_ta.type =
      enum_cast<source::source_type>(param.integers[2]);

    dyn.default_source_value.id = static_cast<u64>(param.integers[3]);

    if (0 <= param.integers[4] && param.integers[4] < 4)
        dyn.default_source_value.type =
          enum_cast<source::source_type>(param.integers[4]);

    return success();
}

static auto parameter_init(parameter& param, const generator& dyn) noexcept
  -> void
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
    param.integers[3] = static_cast<i64>(dyn.default_source_value.id);
    param.integers[4] = ordinal(dyn.default_source_value.type);
}

template<int QssLevel>
static auto parameter_init(parameter&                           param,
                           const abstract_integrator<QssLevel>& dyn) noexcept
  -> void
{
    param.reals[0] = dyn.default_X;
    param.reals[1] = dyn.default_dQ;
}

template<int QssLevel>
static auto model_init(const parameter& /*param*/,
                       abstract_multiplier<QssLevel>& /*dyn*/) noexcept
  -> status
{
    return success();
}

template<int QssLevel>
static auto parameter_init(
  parameter& /*param*/,
  const abstract_multiplier<QssLevel>& /*dyn*/) noexcept -> void
{
}

template<int QssLevel, int PortNumber>
static auto model_init(const parameter& /*param*/,
                       abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept
  -> status
{
    return success();
}

template<int QssLevel, int PortNumber>
static auto parameter_init(
  parameter& /*param*/,
  const abstract_sum<QssLevel, PortNumber>& /*dyn*/) noexcept -> void
{
}

template<int QssLevel, int PortNumber>
static auto model_init(const parameter&                     param,
                       abstract_wsum<QssLevel, PortNumber>& dyn) noexcept
  -> status
{
    dyn.default_input_coeffs[0] = param.reals[0];
    dyn.default_input_coeffs[1] = param.reals[1];

    if constexpr (PortNumber > 2)
        dyn.default_input_coeffs[2] = param.reals[2];

    if constexpr (PortNumber > 3)
        dyn.default_input_coeffs[3] = param.reals[3];

    return success();
}

template<int QssLevel, int PortNumber>
static auto parameter_init(
  parameter&                                 param,
  const abstract_wsum<QssLevel, PortNumber>& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_input_coeffs[0];
    param.reals[1] = dyn.default_input_coeffs[1];

    if constexpr (PortNumber > 2)
        param.reals[2] = dyn.default_input_coeffs[2];

    if constexpr (PortNumber > 3)
        param.reals[3] = dyn.default_input_coeffs[3];
}

template<int QssLevel>
static auto model_init(const parameter&          param,
                       abstract_cross<QssLevel>& dyn) noexcept -> status
{
    dyn.default_threshold = param.reals[0];
    dyn.default_detect_up = param.integers[0] ? true : false;

    return success();
}

template<int QssLevel>
static auto parameter_init(parameter&                      param,
                           const abstract_cross<QssLevel>& dyn) noexcept -> void
{
    param.reals[0]    = dyn.default_threshold;
    param.integers[0] = dyn.default_detect_up ? 1 : 0;
}

template<int QssLevel>
static auto model_init(const parameter&           param,
                       abstract_filter<QssLevel>& dyn) noexcept -> status
{
    dyn.default_lower_threshold = param.reals[0];
    dyn.default_upper_threshold = param.reals[1];

    return success();
}

template<int QssLevel>
static auto parameter_init(parameter&                       param,
                           const abstract_filter<QssLevel>& dyn) noexcept
  -> void
{
    param.reals[0] = dyn.default_lower_threshold;
    param.reals[1] = dyn.default_upper_threshold;
}

template<int QssLevel>
static auto model_init(const parameter&          param,
                       abstract_power<QssLevel>& dyn) noexcept -> status
{
    dyn.default_n = param.reals[0];

    return success();
}

template<int QssLevel>
static auto parameter_init(parameter&                      param,
                           const abstract_power<QssLevel>& dyn) noexcept -> void
{
    param.reals[0] = dyn.default_n;
}

template<int QssLevel>
static auto model_init(const parameter& /*param*/,
                       abstract_square<QssLevel>& /*dyn*/) noexcept -> status
{
    return success();
}

template<int QssLevel>
static auto parameter_init(parameter& /*param*/,
                           const abstract_square<QssLevel>& /*dyn*/) noexcept
  -> void
{
}

template<typename AbstractLogicalTester, int PortNumber>
static auto model_init(
  const parameter&                                     param,
  abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept -> status
{
    dyn.default_values[0] = param.integers[0];
    dyn.default_values[1] = param.integers[1];

    if constexpr (PortNumber == 3)
        dyn.default_values[2] = param.integers[2];

    return success();
}

template<typename AbstractLogicalTester, int PortNumber>
static auto parameter_init(
  parameter&                                                 param,
  const abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept
  -> void
{
    param.integers[0] = dyn.default_values[0];
    param.integers[1] = dyn.default_values[1];

    if constexpr (PortNumber == 3)
        param.integers[2] = dyn.default_values[2];
}

static auto model_init(const parameter& /*param*/,
                       logical_invert& /*dyn*/) noexcept -> status
{
    return success();
}

static auto parameter_init(parameter& /*param*/,
                           const logical_invert& /*dyn*/) noexcept -> void
{
}

static auto model_init(const parameter& /*param*/,
                       hsm_wrapper& /*dyn*/) noexcept -> status
{
    return success();
}

static auto parameter_init(parameter& /*param*/,
                           const hsm_wrapper& /*dyn*/) noexcept -> void
{
}

static auto model_init(const parameter& param, time_func& dyn) noexcept
  -> status
{
    dyn.default_f = param.integers[0] == 0   ? &time_function
                    : param.integers[0] == 1 ? &square_time_function
                                             : sin_time_function;

    return success();
}

static auto parameter_init(parameter& param, const time_func& dyn) noexcept
  -> void
{
    param.integers[0] = dyn.default_f == &time_function          ? 0
                        : dyn.default_f == &square_time_function ? 1
                                                                 : 2;
}

status parameter::copy_to(model& mdl) const noexcept
{
    return dispatch(mdl,
                    [&]<typename Dynamics>(Dynamics& dyn) noexcept -> status {
                        return model_init(*this, dyn);
                    });
}

void parameter::copy_from(const model& mdl) noexcept
{
    clear();

    dispatch(mdl, [&]<typename Dynamics>(const Dynamics& dyn) noexcept -> void {
        parameter_init(*this, dyn);
    });
};

void parameter::clear() noexcept
{
    reals.fill(0);
    integers.fill(0);
}

} // namespace irt
