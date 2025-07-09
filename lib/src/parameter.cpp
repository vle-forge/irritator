// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static inline source::source_type get_source_type(i64 type) noexcept
{
    return (0 <= type && type < 4) ? enum_cast<source::source_type>(type)
                                   : source::source_type::constant;
}

static inline u64 get_source_id(i64 id) noexcept
{
    return static_cast<u64>(id);
}

template<size_t QssLevel>
static void model_init(const parameter&               param,
                       abstract_integrator<QssLevel>& dyn) noexcept
{
    dyn.X  = param.reals[0];
    dyn.dQ = param.reals[1];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_compare<QssLevel>& dyn) noexcept
{
    dyn.a[0]      = param.reals[0];
    dyn.b[0]      = param.reals[1];
    dyn.output[0] = param.reals[2];
    dyn.output[1] = param.reals[3];
}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_integer<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_compare<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.a[0];
    param.reals[1] = dyn.b[0];
    param.reals[2] = dyn.output[0];
    param.reals[3] = dyn.output[1];
}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_integer<QssLevel>& /*dyn*/) noexcept
{}

static void model_init(const parameter& /*param*/, counter& /*dyn*/) noexcept {}

static void parameter_init(parameter& /*param*/,
                           const counter& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, constant& dyn) noexcept
{
    dyn.value  = param.reals[0];
    dyn.offset = param.reals[1];

    dyn.type = (0 <= param.integers[0] && param.integers[0] < 5)
                 ? enum_cast<constant::init_type>(param.integers[0])
                 : constant::init_type::constant;

    dyn.port = param.integers[1];
}

static void parameter_init(parameter& param, const constant& dyn) noexcept
{
    param.reals[0]    = dyn.value;
    param.reals[1]    = dyn.offset;
    param.integers[0] = ordinal(dyn.type);
    param.integers[1] = dyn.port;
}

template<size_t PortNumber>
static void model_init(const parameter& /*param*/,
                       accumulator<PortNumber>& /*dyn*/) noexcept
{}

template<size_t PortNumber>
static void parameter_init(parameter& /*param*/,
                           const accumulator<PortNumber>& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, queue& dyn) noexcept
{
    dyn.ta = param.reals[0];
}

static void parameter_init(parameter& param, const queue& dyn) noexcept
{
    param.reals[0] = dyn.ta;
}

static void model_init(const parameter& param, dynamic_queue& dyn) noexcept
{
    dyn.stop_on_error  = param.integers[0] != 0;
    dyn.source_ta.id   = get_source_id(param.integers[1]);
    dyn.source_ta.type = get_source_type(param.integers[2]);
}

static void parameter_init(parameter& param, const dynamic_queue& dyn) noexcept
{
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.source_ta.id);
    param.integers[2] = ordinal(dyn.source_ta.type);
}

static void model_init(const parameter& param, priority_queue& dyn) noexcept
{
    dyn.ta             = param.reals[0];
    dyn.stop_on_error  = param.integers[0] != 0;
    dyn.source_ta.id   = get_source_id(param.integers[1]);
    dyn.source_ta.type = get_source_type(param.integers[2]);
}

static void parameter_init(parameter& param, const priority_queue& dyn) noexcept
{
    param.reals[0]    = dyn.ta;
    param.integers[0] = dyn.stop_on_error ? 1 : 0;
    param.integers[1] = static_cast<i64>(dyn.source_ta.id);
    param.integers[2] = ordinal(dyn.source_ta.type);
}

static void model_init(const parameter& param, generator& dyn) noexcept
{
    dyn.flags = bitflags<generator::option>(param.integers[0]);

    if (dyn.flags[generator::option::ta_use_source]) {
        dyn.offset         = param.reals[0];
        dyn.source_ta.id   = get_source_id(param.integers[1]);
        dyn.source_ta.type = get_source_type(param.integers[2]);
    }

    if (dyn.flags[generator::option::value_use_source]) {
        dyn.source_value.id   = get_source_id(param.integers[3]);
        dyn.source_value.type = get_source_type(param.integers[4]);
    }
}

static void parameter_init(parameter& param, const generator& dyn) noexcept
{
    param.integers[0] = dyn.flags.to_unsigned();

    if (dyn.flags[generator::option::ta_use_source]) {
        param.reals[0]    = dyn.offset;
        param.integers[1] = static_cast<i64>(dyn.source_ta.id);
        param.integers[2] = ordinal(dyn.source_ta.type);
    }

    if (dyn.flags[generator::option::value_use_source]) {
        param.integers[3] = static_cast<i64>(dyn.source_value.id);
        param.integers[4] = ordinal(dyn.source_value.type);
    }
}

template<size_t QssLevel>
static void parameter_init(parameter&                           param,
                           const abstract_integrator<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.X;
    param.reals[1] = dyn.dQ;
}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(
  parameter& /*param*/,
  const abstract_multiplier<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 2>& dyn) noexcept
{
    dyn.values[0] = param.reals[0];
    dyn.values[1] = param.reals[1];
}

template<size_t QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 2>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
}

template<size_t QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 3>& dyn) noexcept
{
    dyn.values[0] = param.reals[0];
    dyn.values[1] = param.reals[1];
    dyn.values[2] = param.reals[2];
}

template<size_t QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 3>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
    param.reals[2] = dyn.values[2];
}

template<size_t QssLevel>
static void model_init(const parameter&           param,
                       abstract_sum<QssLevel, 4>& dyn) noexcept
{
    dyn.values[0] = param.reals[0];
    dyn.values[1] = param.reals[1];
    dyn.values[2] = param.reals[2];
    dyn.values[3] = param.reals[3];
}

template<size_t QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_sum<QssLevel, 4>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
    param.reals[2] = dyn.values[2];
    param.reals[3] = dyn.values[3];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    dyn.values[0]       = param.reals[0];
    dyn.values[1]       = param.reals[1];
    dyn.input_coeffs[0] = param.reals[2];
    dyn.input_coeffs[1] = param.reals[3];
}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
    param.reals[2] = dyn.input_coeffs[0];
    param.reals[3] = dyn.input_coeffs[1];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    dyn.values[0]       = param.reals[0];
    dyn.values[1]       = param.reals[1];
    dyn.values[2]       = param.reals[2];
    dyn.input_coeffs[0] = param.reals[3];
    dyn.input_coeffs[1] = param.reals[4];
    dyn.input_coeffs[2] = param.reals[5];
}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
    param.reals[2] = dyn.values[2];
    param.reals[3] = dyn.input_coeffs[0];
    param.reals[4] = dyn.input_coeffs[1];
    param.reals[5] = dyn.input_coeffs[2];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 4>& dyn) noexcept
{
    dyn.values[0]       = param.reals[0];
    dyn.values[1]       = param.reals[1];
    dyn.values[2]       = param.reals[2];
    dyn.values[3]       = param.reals[3];
    dyn.input_coeffs[0] = param.reals[4];
    dyn.input_coeffs[1] = param.reals[5];
    dyn.input_coeffs[2] = param.reals[6];
    dyn.input_coeffs[3] = param.reals[7];
}

template<size_t QssLevel, std::size_t PortNumber>
static void parameter_init(
  parameter&                                 param,
  const abstract_wsum<QssLevel, PortNumber>& dyn) noexcept
{
    param.reals[0] = dyn.values[0];
    param.reals[1] = dyn.values[1];
    param.reals[2] = dyn.values[2];
    param.reals[3] = dyn.values[3];
    param.reals[4] = dyn.input_coeffs[0];
    param.reals[5] = dyn.input_coeffs[1];
    param.reals[6] = dyn.input_coeffs[2];
    param.reals[7] = dyn.input_coeffs[3];
}

template<size_t QssLevel>
static void model_init(const parameter&          param,
                       abstract_cross<QssLevel>& dyn) noexcept
{
    dyn.threshold = param.reals[0];
    dyn.detect_up = param.integers[0] ? true : false;
}

template<size_t QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_cross<QssLevel>& dyn) noexcept
{
    param.reals[0]    = dyn.threshold;
    param.integers[0] = dyn.detect_up;
}

template<size_t QssLevel>
static void model_init(const parameter&           param,
                       abstract_filter<QssLevel>& dyn) noexcept
{
    dyn.lower_threshold = param.reals[0];
    dyn.upper_threshold = param.reals[1];
}

template<size_t QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_filter<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.lower_threshold;
    param.reals[1] = dyn.upper_threshold;
}

template<size_t QssLevel>
static void model_init(const parameter&          param,
                       abstract_power<QssLevel>& dyn) noexcept
{
    dyn.n = param.reals[0];
}

template<size_t QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_power<QssLevel>& dyn) noexcept
{
    param.reals[0] = dyn.n;
}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_square<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_square<QssLevel>& /*dyn*/) noexcept
{}

template<typename AbstractLogicalTester, std::size_t PortNumber>
static void model_init(
  const parameter&                                     param,
  abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept
{
    dyn.values[0] = param.integers[0];
    dyn.values[1] = param.integers[1];

    if constexpr (PortNumber == 3)
        dyn.values[2] = param.integers[2];
}

template<typename AbstractLogicalTester, std::size_t PortNumber>
static void parameter_init(
  parameter&                                                 param,
  const abstract_logical<AbstractLogicalTester, PortNumber>& dyn) noexcept
{
    param.integers[0] = dyn.values[0];
    param.integers[1] = dyn.values[1];

    if constexpr (PortNumber == 3)
        param.integers[2] = dyn.values[2];
}

static void model_init(const parameter& param, logical_invert& dyn) noexcept
{
    dyn.value = param.integers[0] ? true : false;
}

static void parameter_init(parameter& param, const logical_invert& dyn) noexcept
{
    param.integers[0] = dyn.value;
}

static void model_init(const parameter& param, hsm_wrapper& dyn) noexcept
{
    dyn.id                     = enum_cast<hsm_id>(param.integers[0]);
    dyn.exec.i1                = static_cast<i32>(param.integers[1]);
    dyn.exec.i2                = static_cast<i32>(param.integers[2]);
    dyn.exec.source_value.id   = get_source_id(param.integers[3]);
    dyn.exec.source_value.type = get_source_type(param.integers[4]);
    dyn.exec.r1                = param.reals[0];
    dyn.exec.r2                = param.reals[1];
    dyn.exec.timer             = param.reals[2];
}

static void parameter_init(parameter& param, const hsm_wrapper& dyn) noexcept
{
    param.integers[0] = ordinal(dyn.id);
    param.integers[1] = static_cast<i64>(dyn.exec.i1);
    param.integers[2] = static_cast<i64>(dyn.exec.i2);
    param.integers[3] = static_cast<i64>(dyn.exec.source_value.id);
    param.integers[4] = static_cast<i64>(dyn.exec.source_value.type);
    param.reals[0]    = dyn.exec.r1;
    param.reals[1]    = dyn.exec.r2;
    param.reals[2]    = dyn.exec.timer;
}

static void model_init(const parameter& param, time_func& dyn) noexcept
{
    dyn.offset   = param.reals[0];
    dyn.timestep = param.reals[1];
    dyn.f        = param.integers[0] == 0   ? &time_function
                   : param.integers[0] == 1 ? &square_time_function
                                            : &sin_time_function;
}

static void parameter_init(parameter& param, const time_func& dyn) noexcept
{
    param.reals[0] = dyn.offset;
    param.reals[1] = dyn.timestep;

    param.integers[0] = dyn.f == &time_function          ? 0
                        : dyn.f == &square_time_function ? 1
                                                         : 2;
}

parameter::parameter(const model& mdl) noexcept
{
    dispatch(mdl, [&]<typename Dynamics>(const Dynamics& dyn) noexcept {
        parameter_init(*this, dyn);
    });
}

parameter::parameter(const dynamics_type type) noexcept { init_from(type); }

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

void parameter::init_from(const dynamics_type type) noexcept
{
    clear();
    if (any_equal(type,
                  dynamics_type::qss1_integrator,
                  dynamics_type::qss2_integrator,
                  dynamics_type::qss3_integrator)) {
        reals[1] = 0.01; // dQ parameter
    } else if (any_equal(type,
                         dynamics_type::qss1_power,
                         dynamics_type::qss2_power,
                         dynamics_type::qss3_power)) {
        reals[0] = 1.0;
    } else if (any_equal(type,
                         dynamics_type::qss1_filter,
                         dynamics_type::qss2_filter,
                         dynamics_type::qss3_filter)) {
        reals[0] = -std::numeric_limits<real>::infinity();
        reals[1] = +std::numeric_limits<real>::infinity();
    } else if (any_equal(type,
                         dynamics_type::qss1_cross,
                         dynamics_type::qss2_cross,
                         dynamics_type::qss3_cross)) {
        integers[0] = 1;
    } else if (any_equal(type,
                         dynamics_type::qss1_compare,
                         dynamics_type::qss2_compare,
                         dynamics_type::qss3_compare)) {
        reals[0]    = std::numeric_limits<real>::epsilon();
        integers[0] = 0; // equal_to
        integers[1] = 1;
    } else if (any_equal(type, dynamics_type::time_func)) {
        reals[1] = 0.01;
    } else if (any_equal(
                 type, dynamics_type::priority_queue, dynamics_type::queue)) {
        reals[0] = 1.0;
    }
};

void parameter::clear() noexcept
{
    reals.fill(0);
    integers.fill(0);
}

parameter& parameter::set_constant(real value, real offset) noexcept
{
    reals[0]    = value;
    reals[1]    = std::isfinite(offset) ? std::abs(offset) : 0.0;
    integers[0] = ordinal(constant::init_type::constant);
    integers[1] = 0;
    return *this;
}

parameter& parameter::set_cross(real threshold, bool detect_up) noexcept
{

    reals[0]    = std::isfinite(threshold) ? threshold : 1.0;
    integers[0] = detect_up;

    return *this;
}

parameter& parameter::set_integrator(real X, real dQ) noexcept
{
    reals[0] = std::isfinite(X) ? X : 0.0;
    reals[1] = std::isfinite(dQ) ? dQ : 0.01;
    return *this;
}

parameter& parameter::set_time_func(real offset,
                                    real timestep,
                                    int  type) noexcept
{
    reals[0] = std::isfinite(offset) ? std::abs(offset) : 0.0;
    reals[1] = std::isfinite(timestep) ? timestep <= 0.0 ? 0.1 : timestep : 0.1;
    integers[0] = type < 0 ? 0 : type > 2 ? 2 : type;
    return *this;
}

parameter& parameter::set_wsum2(real v1,
                                real coeff1,
                                real v2,
                                real coeff2) noexcept
{
    reals[0] = std::isfinite(v1) ? v1 : 0.0;
    reals[1] = std::isfinite(v2) ? v2 : 0.0;
    reals[2] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[3] = std::isfinite(coeff2) ? coeff2 : 1.0;
    return *this;
}

parameter& parameter::set_wsum3(real v1,
                                real coeff1,
                                real v2,
                                real coeff2,
                                real v3,
                                real coeff3) noexcept
{
    reals[0] = std::isfinite(v1) ? v1 : 0.0;
    reals[1] = std::isfinite(v2) ? v2 : 0.0;
    reals[2] = std::isfinite(v3) ? v3 : 0.0;
    reals[3] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[4] = std::isfinite(coeff2) ? coeff2 : 1.0;
    reals[5] = std::isfinite(coeff3) ? coeff3 : 1.0;
    return *this;
}

parameter& parameter::set_wsum4(real v1,
                                real coeff1,
                                real v2,
                                real coeff2,
                                real v3,
                                real coeff3,
                                real v4,
                                real coeff4) noexcept
{
    reals[0] = std::isfinite(v1) ? v1 : 0.0;
    reals[1] = std::isfinite(v2) ? v2 : 0.0;
    reals[2] = std::isfinite(v3) ? v3 : 0.0;
    reals[3] = std::isfinite(v4) ? v4 : 0.0;
    reals[4] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[5] = std::isfinite(coeff2) ? coeff2 : 1.0;
    reals[6] = std::isfinite(coeff3) ? coeff3 : 1.0;
    reals[7] = std::isfinite(coeff4) ? coeff4 : 1.0;
    return *this;
}

parameter& parameter::set_hsm_wrapper(const u32 id) noexcept
{
    integers[0] = id;

    return *this;
}

parameter& parameter::set_hsm_wrapper(i64  i1,
                                      i64  i2,
                                      real r1,
                                      real r2,
                                      real timer) noexcept
{
    integers[1] = i1;
    integers[2] = i2;
    reals[0]    = r1;
    reals[1]    = r2;
    reals[2]    = std::isfinite(timer) ? timer : 0.0;

    return *this;
}

parameter& parameter::set_hsm_wrapper(const u64                 id,
                                      const source::source_type type) noexcept
{
    integers[3] = static_cast<i64>(id);
    integers[4] = static_cast<i64>(type);

    return *this;
}

} // namespace irt
