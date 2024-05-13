// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

template<int QssLevel>
static void model_init(const parameter&               param,
                       abstract_integrator<QssLevel>& dyn) noexcept
{
    dyn.default_X  = param.reals[0];
    dyn.default_dQ = param.reals[1];
}

static void model_init(const parameter& /*param*/, counter& /*dyn*/) noexcept {}

static void parameter_init(parameter& /*param*/,
                           const counter& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, constant& dyn) noexcept
{
    dyn.default_value  = param.reals[0];
    dyn.default_offset = param.reals[1];

    dyn.type = (0 <= param.integers[0] && param.integers[0] < 5)
                 ? enum_cast<constant::init_type>(param.integers[0])
                 : constant::init_type::constant;

    dyn.port = param.integers[1];
}

static void parameter_init(parameter& param, const constant& dyn) noexcept
{
    param.reals[0]    = dyn.default_value;
    param.reals[1]    = dyn.default_offset;
    param.integers[0] = ordinal(dyn.type);
    param.integers[1] = dyn.port;
}

template<int PortNumber>
static void model_init(const parameter& /*param*/,
                       accumulator<PortNumber>& /*dyn*/) noexcept
{}

template<int PortNumber>
static void parameter_init(parameter& /*param*/,
                           const accumulator<PortNumber>& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, queue& dyn) noexcept
{
    dyn.default_ta = param.reals[0];
}

static void parameter_init(parameter& param, const queue& dyn) noexcept
{
    param.reals[0] = dyn.default_ta;
}

static void model_init(const parameter& param, dynamic_queue& dyn) noexcept
{
    dyn.stop_on_error        = param.integers[0] != 0;
    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);

    dyn.default_source_ta.type =
      (0 <= param.integers[2] && param.integers[2] < 4)
        ? enum_cast<source::source_type>(param.integers[2])
        : source::source_type::none;
}

static void parameter_init(parameter& param, const dynamic_queue& dyn) noexcept
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
}

static void model_init(const parameter& param, priority_queue& dyn) noexcept
{
    dyn.stop_on_error        = param.integers[0] != 0;
    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);

    dyn.default_source_ta.type =
      (0 <= param.integers[2] && param.integers[2] < 4)
        ? enum_cast<source::source_type>(param.integers[2])
        : source::source_type::none;
}

static void parameter_init(parameter& param, const priority_queue& dyn) noexcept
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
}

static void model_init(const parameter& param, generator& dyn) noexcept
{
    dyn.default_offset = param.reals[0];
    dyn.stop_on_error  = param.integers[0] != 0;

    dyn.default_source_ta.id = static_cast<u64>(param.integers[1]);
    dyn.default_source_ta.type =
      (0 <= param.integers[2] && param.integers[2] < 4)
        ? enum_cast<source::source_type>(param.integers[2])
        : source::source_type::none;

    dyn.default_source_value.id = static_cast<u64>(param.integers[3]);

    dyn.default_source_value.type =
      (0 <= param.integers[4] && param.integers[4] < 4)
        ? enum_cast<source::source_type>(param.integers[4])
        : source::source_type::none;
}

static void parameter_init(parameter& param, const generator& dyn) noexcept
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.default_source_ta.id);
    param.integers[2] = ordinal(dyn.default_source_ta.type);
    param.integers[3] = static_cast<i64>(dyn.default_source_value.id);
    param.integers[4] = ordinal(dyn.default_source_value.type);
}

template<int QssLevel>
static void parameter_init(parameter&                           param,
                           const abstract_integrator<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.default_X;
    param.reals[1] = dyn.default_dQ;
}

template<int QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{}

template<int QssLevel>
static void parameter_init(
  parameter& /*param*/,
  const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{}

template<int QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 2>& dyn) noexcept
{
    dyn.default_values[0] = param.reals[0];
    dyn.default_values[1] = param.reals[1];
}

template<int QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 2>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
}

template<int QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 3>& dyn) noexcept
{
    dyn.default_values[0] = param.reals[0];
    dyn.default_values[1] = param.reals[1];
    dyn.default_values[2] = param.reals[2];
}

template<int QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 3>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
    param.reals[2] = dyn.default_values[2];
}

template<int QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 4>& dyn) noexcept
{
    dyn.default_values[0] = param.reals[0];
    dyn.default_values[1] = param.reals[1];
    dyn.default_values[2] = param.reals[2];
    dyn.default_values[3] = param.reals[3];
}

template<int QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 4>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
    param.reals[2] = dyn.default_values[2];
    param.reals[3] = dyn.default_values[3];
}

template<int QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    dyn.default_values[0]       = param.reals[0];
    dyn.default_values[1]       = param.reals[1];
    dyn.default_input_coeffs[0] = param.reals[2];
    dyn.default_input_coeffs[1] = param.reals[3];
}

template<int QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
    param.reals[2] = dyn.default_input_coeffs[0];
    param.reals[3] = dyn.default_input_coeffs[1];
}

template<int QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    dyn.default_values[0]       = param.reals[0];
    dyn.default_values[1]       = param.reals[1];
    dyn.default_values[2]       = param.reals[2];
    dyn.default_input_coeffs[0] = param.reals[3];
    dyn.default_input_coeffs[1] = param.reals[4];
    dyn.default_input_coeffs[2] = param.reals[5];
}

template<int QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
    param.reals[2] = dyn.default_values[2];
    param.reals[3] = dyn.default_input_coeffs[0];
    param.reals[4] = dyn.default_input_coeffs[1];
    param.reals[5] = dyn.default_input_coeffs[2];
}

template<int QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 4>& dyn) noexcept
{
    dyn.default_values[0]       = param.reals[0];
    dyn.default_values[1]       = param.reals[1];
    dyn.default_values[2]       = param.reals[2];
    dyn.default_values[3]       = param.reals[3];
    dyn.default_input_coeffs[0] = param.reals[4];
    dyn.default_input_coeffs[1] = param.reals[5];
    dyn.default_input_coeffs[2] = param.reals[6];
    dyn.default_input_coeffs[3] = param.reals[7];
}

template<int QssLevel, int PortNumber>
static void parameter_init(
  parameter&                                 param,
  const abstract_wsum<QssLevel, PortNumber>& dyn) noexcept
{
    param.reals[0] = dyn.default_values[0];
    param.reals[1] = dyn.default_values[1];
    param.reals[2] = dyn.default_values[2];
    param.reals[3] = dyn.default_values[3];
    param.reals[4] = dyn.default_input_coeffs[0];
    param.reals[5] = dyn.default_input_coeffs[1];
    param.reals[6] = dyn.default_input_coeffs[2];
    param.reals[7] = dyn.default_input_coeffs[3];
}

template<int QssLevel>
static void model_init(const parameter&          param,
                       abstract_cross<QssLevel>& dyn) noexcept
{
    dyn.default_threshold = param.reals[0];
    dyn.default_detect_up = param.integers[0] ? true : false;
}

template<int QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_cross<QssLevel>& dyn) noexcept
{
    param.reals[0]    = dyn.default_threshold;
    param.integers[0] = dyn.default_detect_up ? 1 : 0;
}

template<int QssLevel>
static void model_init(const parameter&           param,
                       abstract_filter<QssLevel>& dyn) noexcept
{
    dyn.default_lower_threshold = param.reals[0];
    dyn.default_upper_threshold = param.reals[1];
}

template<int QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_filter<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.default_lower_threshold;
    param.reals[1] = dyn.default_upper_threshold;
}

template<int QssLevel>
static void model_init(const parameter&          param,
                       abstract_power<QssLevel>& dyn) noexcept
{
    dyn.default_n = param.reals[0];
}

template<int QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_power<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.default_n;
}

template<int QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_square<QssLevel>& /*dyn*/) noexcept
{}

template<int QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_square<QssLevel>& /*dyn*/) noexcept
{}

template<typename AbstractLogicalTester, int PortNumber>
static void model_init(
  const parameter&                                     param,
  abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept
{
    dyn.default_values[0] = param.integers[0];
    dyn.default_values[1] = param.integers[1];

    if constexpr (PortNumber == 3)
        dyn.default_values[2] = param.integers[2];
}

template<typename AbstractLogicalTester, int PortNumber>
static void parameter_init(
  parameter&                                                 param,
  const abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept
{
    param.integers[0] = dyn.default_values[0];
    param.integers[1] = dyn.default_values[1];

    if constexpr (PortNumber == 3)
        param.integers[2] = dyn.default_values[2];
}

static void model_init(const parameter& /*param*/,
                       logical_invert& /*dyn*/) noexcept
{}

static void parameter_init(parameter& /*param*/,
                           const logical_invert& /*dyn*/) noexcept
{}

static void model_init(const parameter& /*param*/,
                       hsm_wrapper& /*dyn*/) noexcept
{}

static void parameter_init(parameter& /*param*/,
                           const hsm_wrapper& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, time_func& dyn) noexcept
{
    dyn.default_f = param.integers[0] == 0   ? &time_function
                    : param.integers[0] == 1 ? &square_time_function
                                             : sin_time_function;
}

static void parameter_init(parameter& param, const time_func& dyn) noexcept
{
    param.integers[0] = dyn.default_f == &time_function          ? 0
                        : dyn.default_f == &square_time_function ? 1
                                                                 : 2;
}

void parameter::copy_to(model& mdl) const noexcept
{
    dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        model_init(*this, dyn);
    });
}

void parameter::copy_from(const model& mdl) noexcept
{
    clear();

    dispatch(mdl, [&]<typename Dynamics>(const Dynamics& dyn) noexcept {
        parameter_init(*this, dyn);
    });
};

void parameter::clear() noexcept
{
    reals.fill(0);
    integers.fill(0);
}

} // namespace irt
