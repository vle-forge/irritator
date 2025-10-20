// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

template<size_t QssLevel>
static void model_init(const parameter&               param,
                       abstract_integrator<QssLevel>& dyn) noexcept
{
    dyn.X  = param.reals[qss_integrator_tag::X];
    dyn.dQ = param.reals[qss_integrator_tag::dQ];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_compare<QssLevel>& dyn) noexcept
{
    dyn.output[0] = param.reals[qss_compare_tag::equal];
    dyn.output[1] = param.reals[qss_compare_tag::not_equal];
}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_integer<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_compare<QssLevel>& dyn) noexcept
{
    param.reals[qss_compare_tag::equal]     = dyn.output[0];
    param.reals[qss_compare_tag::not_equal] = dyn.output[1];
}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_integer<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_sin<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_log<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_sin<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_log<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_cos<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_exp<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_cos<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_exp<QssLevel>& /*dyn*/) noexcept
{}

static void model_init(const parameter& /*param*/, counter& /*dyn*/) noexcept {}

static void parameter_init(parameter& /*param*/,
                           const counter& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, constant& dyn) noexcept
{
    dyn.value  = param.reals[constant_tag::value];
    dyn.offset = param.reals[constant_tag::offset];

    dyn.type =
      (0 <= param.integers[constant_tag::i_type] &&
       param.integers[constant_tag::i_type] < 5)
        ? enum_cast<constant::init_type>(param.integers[constant_tag::i_type])
        : constant::init_type::constant;

    dyn.port = param.integers[constant_tag::i_port];
}

static void parameter_init(parameter& param, const constant& dyn) noexcept
{
    param.reals[constant_tag::value]     = dyn.value;
    param.reals[constant_tag::offset]    = dyn.offset;
    param.integers[constant_tag::i_type] = static_cast<i64>(ordinal(dyn.type));
    param.integers[constant_tag::i_port] = static_cast<i64>(dyn.port);
}

template<size_t PortNumber>
static void model_init(const parameter& /*param*/,
                       abstract_inverse<PortNumber>& /*dyn*/) noexcept
{}

template<size_t PortNumber>
static void parameter_init(parameter& /*param*/,
                           const abstract_inverse<PortNumber>& /*dyn*/) noexcept
{}

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
    dyn.ta = param.reals[queue_tag::sigma];
}

static void parameter_init(parameter& param, const queue& dyn) noexcept
{
    param.reals[queue_tag::sigma] = dyn.ta;
}

static void model_init(const parameter& param, dynamic_queue& dyn) noexcept
{
    dyn.source_ta = get_source(param.integers[dynamic_queue_tag::source_ta]);
}

static void parameter_init(parameter& param, const dynamic_queue& dyn) noexcept
{
    param.integers[dynamic_queue_tag::source_ta] = from_source(dyn.source_ta);
}

static void model_init(const parameter& param, priority_queue& dyn) noexcept
{
    dyn.ta        = param.reals[priority_queue_tag::sigma];
    dyn.source_ta = get_source(param.integers[priority_queue_tag::source_ta]);
}

static void parameter_init(parameter& param, const priority_queue& dyn) noexcept
{
    param.reals[priority_queue_tag::sigma]        = dyn.ta;
    param.integers[priority_queue_tag::source_ta] = from_source(dyn.source_ta);
}

static void model_init(const parameter& param, generator& dyn) noexcept
{
    dyn.flags =
      bitflags<generator::option>(param.integers[generator_tag::i_options]);

    if (dyn.flags[generator::option::ta_use_source]) {
        dyn.source_ta = get_source(param.integers[generator_tag::source_ta]);
    }

    if (dyn.flags[generator::option::value_use_source]) {
        dyn.source_value =
          get_source(param.integers[generator_tag::source_value]);
    }
}

static void parameter_init(parameter& param, const generator& dyn) noexcept
{
    param.integers[generator_tag::i_options] = dyn.flags.to_unsigned();

    if (dyn.flags[generator::option::ta_use_source])
        param.integers[generator_tag::source_ta] = from_source(dyn.source_ta);

    if (dyn.flags[generator::option::value_use_source])
        param.integers[generator_tag::source_value] =
          from_source(dyn.source_value);
}

template<size_t QssLevel>
static void parameter_init(parameter&                           param,
                           const abstract_integrator<QssLevel>& dyn) noexcept
{
    param.reals[qss_integrator_tag::X]  = dyn.X;
    param.reals[qss_integrator_tag::dQ] = dyn.dQ;
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
static void model_init(const parameter& /*param*/,
                       abstract_sum<QssLevel, 2>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_sum<QssLevel, 2>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_sum<QssLevel, 3>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_sum<QssLevel, 3>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_sum<QssLevel, 4>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_sum<QssLevel, 4>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    dyn.input_coeffs[0] = param.reals[qss_wsum_2_tag::coeff1];
    dyn.input_coeffs[1] = param.reals[qss_wsum_2_tag::coeff2];
}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 2>& dyn) noexcept
{
    param.reals[qss_wsum_2_tag::coeff1] = dyn.input_coeffs[0];
    param.reals[qss_wsum_2_tag::coeff2] = dyn.input_coeffs[1];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    dyn.input_coeffs[0] = param.reals[qss_wsum_3_tag::coeff1];
    dyn.input_coeffs[1] = param.reals[qss_wsum_3_tag::coeff2];
    dyn.input_coeffs[2] = param.reals[qss_wsum_3_tag::coeff3];
}

template<size_t QssLevel>
static void parameter_init(parameter&                        param,
                           const abstract_wsum<QssLevel, 3>& dyn) noexcept
{
    param.reals[qss_wsum_3_tag::coeff1] = dyn.input_coeffs[0];
    param.reals[qss_wsum_3_tag::coeff2] = dyn.input_coeffs[1];
    param.reals[qss_wsum_3_tag::coeff3] = dyn.input_coeffs[2];
}

template<size_t QssLevel>
static void model_init(const parameter&            param,
                       abstract_wsum<QssLevel, 4>& dyn) noexcept
{
    dyn.input_coeffs[0] = param.reals[qss_wsum_4_tag::coeff1];
    dyn.input_coeffs[1] = param.reals[qss_wsum_4_tag::coeff2];
    dyn.input_coeffs[2] = param.reals[qss_wsum_4_tag::coeff3];
    dyn.input_coeffs[3] = param.reals[qss_wsum_4_tag::coeff4];
}

template<size_t QssLevel, std::size_t PortNumber>
static void parameter_init(
  parameter&                                 param,
  const abstract_wsum<QssLevel, PortNumber>& dyn) noexcept
{
    param.reals[qss_wsum_4_tag::coeff1] = dyn.input_coeffs[0];
    param.reals[qss_wsum_4_tag::coeff2] = dyn.input_coeffs[1];
    param.reals[qss_wsum_4_tag::coeff3] = dyn.input_coeffs[2];
    param.reals[qss_wsum_4_tag::coeff4] = dyn.input_coeffs[3];
}

template<size_t QssLevel>
static void model_init(const parameter&          param,
                       abstract_cross<QssLevel>& dyn) noexcept
{
    dyn.threshold        = param.reals[qss_cross_tag::threshold];
    dyn.output_values[0] = param.reals[qss_cross_tag::up_value];
    dyn.output_values[1] = param.reals[qss_cross_tag::bottom_value];
}

template<size_t QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_cross<QssLevel>& dyn) noexcept
{
    param.reals[qss_cross_tag::threshold]    = dyn.threshold;
    param.reals[qss_cross_tag::up_value]     = dyn.output_values[0];
    param.reals[qss_cross_tag::bottom_value] = dyn.output_values[1];
}

template<size_t QssLevel>
static void model_init(const parameter& /*param*/,
                       abstract_flipflop<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void parameter_init(parameter& /*param*/,
                           const abstract_flipflop<QssLevel>& /*dyn*/) noexcept
{}

template<size_t QssLevel>
static void model_init(const parameter&           param,
                       abstract_filter<QssLevel>& dyn) noexcept
{
    dyn.lower_threshold = param.reals[qss_filter_tag::lower_bound];
    dyn.upper_threshold = param.reals[qss_filter_tag::upper_bound];
}

template<size_t QssLevel>
static void parameter_init(parameter&                       param,
                           const abstract_filter<QssLevel>& dyn) noexcept
{
    param.reals[qss_filter_tag::lower_bound] = dyn.lower_threshold;
    param.reals[qss_filter_tag::upper_bound] = dyn.upper_threshold;
}

template<size_t QssLevel>
static void model_init(const parameter&          param,
                       abstract_power<QssLevel>& dyn) noexcept
{
    dyn.n = param.reals[qss_power_tag::exponent];
}

template<size_t QssLevel>
static void parameter_init(parameter&                      param,
                           const abstract_power<QssLevel>& dyn) noexcept
{
    param.reals[qss_power_tag::exponent] = dyn.n;
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
  const parameter& /*param*/,
  abstract_logical<AbstractLogicalTester, PortNumber>& /*dyn*/) noexcept
{}

template<typename AbstractLogicalTester, std::size_t PortNumber>
static void parameter_init(
  parameter& /*param*/,
  const abstract_logical<AbstractLogicalTester, PortNumber>& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, logical_invert& /*dyn*/) noexcept
{}

static void parameter_init(parameter& param,
                           const logical_invert& /*dyn*/) noexcept
{}

static void model_init(const parameter& param, hsm_wrapper& dyn) noexcept
{
    dyn.id      = enum_cast<hsm_id>(param.integers[hsm_wrapper_tag::id]);
    dyn.exec.i1 = static_cast<i32>(param.integers[hsm_wrapper_tag::i1]);
    dyn.exec.i2 = static_cast<i32>(param.integers[hsm_wrapper_tag::i2]);

    dyn.exec.source_value =
      get_source(param.integers[hsm_wrapper_tag::source_value]);

    dyn.exec.r1    = param.reals[hsm_wrapper_tag::r1];
    dyn.exec.r2    = param.reals[hsm_wrapper_tag::r2];
    dyn.exec.timer = param.reals[hsm_wrapper_tag::timer];
}

static void parameter_init(parameter& param, const hsm_wrapper& dyn) noexcept
{
    param.integers[hsm_wrapper_tag::id] = ordinal(dyn.id);
    param.integers[hsm_wrapper_tag::i1] = static_cast<i64>(dyn.exec.i1);
    param.integers[hsm_wrapper_tag::i2] = static_cast<i64>(dyn.exec.i2);

    param.integers[hsm_wrapper_tag::source_value] =
      from_source(dyn.exec.source_value);

    param.reals[hsm_wrapper_tag::r1]    = dyn.exec.r1;
    param.reals[hsm_wrapper_tag::r2]    = dyn.exec.r2;
    param.reals[hsm_wrapper_tag::timer] = dyn.exec.timer;
}

static void model_init(const parameter& param, time_func& dyn) noexcept
{
    dyn.offset   = param.reals[time_func_tag::offset];
    dyn.timestep = param.reals[time_func_tag::timestep];
    dyn.f        = param.integers[time_func_tag::i_type] == 0 ? &time_function
                   : param.integers[time_func_tag::i_type] == 1 ? &square_time_function
                                                                : &sin_time_function;
}

static void parameter_init(parameter& param, const time_func& dyn) noexcept
{
    param.reals[time_func_tag::offset]   = dyn.offset;
    param.reals[time_func_tag::timestep] = dyn.timestep;

    param.integers[time_func_tag::i_type] = dyn.f == &time_function ? 0
                                            : dyn.f == &square_time_function
                                              ? 1
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
        reals[qss_integrator_tag::X]  = zero;
        reals[qss_integrator_tag::dQ] = 0.01;
    } else if (any_equal(type,
                         dynamics_type::qss1_power,
                         dynamics_type::qss2_power,
                         dynamics_type::qss3_power)) {
        reals[qss_power_tag::exponent] = 1.0;
    } else if (any_equal(type,
                         dynamics_type::qss1_filter,
                         dynamics_type::qss2_filter,
                         dynamics_type::qss3_filter)) {
        reals[qss_filter_tag::lower_bound] =
          -std::numeric_limits<real>::infinity();
        reals[qss_filter_tag::upper_bound] =
          +std::numeric_limits<real>::infinity();
    } else if (any_equal(type,
                         dynamics_type::qss1_cross,
                         dynamics_type::qss2_cross,
                         dynamics_type::qss3_cross)) {
        reals[qss_cross_tag::threshold]    = zero;
        reals[qss_cross_tag::up_value]     = one;
        reals[qss_cross_tag::bottom_value] = one;
    } else if (any_equal(type,
                         dynamics_type::qss1_wsum_2,
                         dynamics_type::qss1_wsum_3,
                         dynamics_type::qss1_wsum_4,
                         dynamics_type::qss2_wsum_2,
                         dynamics_type::qss2_wsum_3,
                         dynamics_type::qss2_wsum_4,
                         dynamics_type::qss3_wsum_2,
                         dynamics_type::qss3_wsum_3,
                         dynamics_type::qss3_wsum_4)) {
        reals.fill(one);
    } else if (any_equal(type,
                         dynamics_type::qss1_compare,
                         dynamics_type::qss2_compare,
                         dynamics_type::qss3_compare)) {
        reals[qss_compare_tag::equal]     = one;
        reals[qss_compare_tag::not_equal] = one;
    } else if (any_equal(type, dynamics_type::time_func)) {
        reals[time_func_tag::timestep] = 0.01;
    } else if (any_equal(type, dynamics_type::priority_queue)) {
        reals[priority_queue_tag::sigma] = 1.0;
    } else if (any_equal(type, dynamics_type::queue)) {
        reals[queue_tag::sigma] = 1.0;
    }
};

parameter& parameter::clear() noexcept
{
    reals.fill(0);
    integers.fill(0);

    return *this;
}

parameter& parameter::set_constant(real value, real offset) noexcept
{
    reals[constant_tag::value] = value;
    reals[constant_tag::offset] =
      std::isfinite(offset) ? std::abs(offset) : 0.0;
    integers[constant_tag::i_type] = ordinal(constant::init_type::constant);
    integers[constant_tag::i_port] = 0;

    return *this;
}

parameter& parameter::set_cross(real threshold) noexcept
{
    reals[qss_cross_tag::threshold] =
      std::isfinite(threshold) ? threshold : 0.0;
    reals[qss_cross_tag::up_value]     = one;
    reals[qss_cross_tag::bottom_value] = one;

    return *this;
}

parameter& parameter::set_integrator(real X, real dQ) noexcept
{
    reals[qss_integrator_tag::X]  = std::isfinite(X) ? X : 0.0;
    reals[qss_integrator_tag::dQ] = std::isfinite(dQ) ? dQ : 0.01;
    return *this;
}

parameter& parameter::set_time_func(real offset,
                                    real timestep,
                                    int  type) noexcept
{
    reals[time_func_tag::offset] =
      std::isfinite(offset) ? std::abs(offset) : 0.0;
    reals[time_func_tag::timestep] =
      std::isfinite(timestep) ? timestep <= 0.0 ? 0.1 : timestep : 0.1;
    integers[time_func_tag::i_type] = type < 0 ? 0 : type > 2 ? 2 : type;
    return *this;
}

parameter& parameter::set_wsum2(real coeff1, real coeff2) noexcept
{
    reals[qss_wsum_4_tag::coeff1] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[qss_wsum_4_tag::coeff2] = std::isfinite(coeff2) ? coeff2 : 1.0;
    return *this;
}

parameter& parameter::set_wsum3(real coeff1, real coeff2, real coeff3) noexcept
{
    reals[qss_wsum_4_tag::coeff1] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[qss_wsum_4_tag::coeff2] = std::isfinite(coeff2) ? coeff2 : 1.0;
    reals[qss_wsum_4_tag::coeff3] = std::isfinite(coeff3) ? coeff3 : 1.0;
    return *this;
}

parameter& parameter::set_wsum4(real coeff1,
                                real coeff2,
                                real coeff3,
                                real coeff4) noexcept
{
    reals[qss_wsum_4_tag::coeff1] = std::isfinite(coeff1) ? coeff1 : 1.0;
    reals[qss_wsum_4_tag::coeff2] = std::isfinite(coeff2) ? coeff2 : 1.0;
    reals[qss_wsum_4_tag::coeff3] = std::isfinite(coeff3) ? coeff3 : 1.0;
    reals[qss_wsum_4_tag::coeff4] = std::isfinite(coeff4) ? coeff4 : 1.0;
    return *this;
}

parameter& parameter::set_hsm_wrapper(const u32 id) noexcept
{
    integers[hsm_wrapper_tag::id] = id;

    return *this;
}

parameter& parameter::set_hsm_wrapper(i64  i1,
                                      i64  i2,
                                      real r1,
                                      real r2,
                                      real timer) noexcept
{
    integers[hsm_wrapper_tag::i1] = i1;
    integers[hsm_wrapper_tag::i2] = i2;
    reals[hsm_wrapper_tag::r1]    = r1;
    reals[hsm_wrapper_tag::r2]    = r2;
    reals[hsm_wrapper_tag::timer] = std::isfinite(timer) ? timer : 0.0;

    return *this;
}

parameter& parameter::set_hsm_wrapper_value(const source& src) noexcept
{
    integers[hsm_wrapper_tag::source_value] = from_source(src);
    return *this;
}

parameter& parameter::set_generator_ta(const source& src) noexcept
{
    bitflags<generator::option> flags(integers[generator_tag::i_options]);
    flags.set(generator::option::ta_use_source, true);

    integers[generator_tag::i_options] = static_cast<i64>(flags.to_unsigned());
    integers[generator_tag::source_ta] = from_source(src);
    return *this;
}

parameter& parameter::set_generator_value(const source& src) noexcept
{
    bitflags<generator::option> flags(integers[generator_tag::i_options]);
    flags.set(generator::option::value_use_source, true);
    integers[generator_tag::i_options] = static_cast<i64>(flags.to_unsigned());

    integers[generator_tag::source_value] = from_source(src);
    return *this;
}

parameter& parameter::set_dynamic_queue_ta(const source& src) noexcept
{
    integers[dynamic_queue_tag::source_ta] = from_source(src);
    return *this;
}

parameter& parameter::set_priority_queue_ta(const source& src) noexcept
{
    integers[priority_queue_tag::source_ta] = from_source(src);
    return *this;
}

source parameter::get_hsm_wrapper_value() noexcept
{
    return get_source(integers[hsm_wrapper_tag::source_value]);
}

source parameter::get_generator_ta() noexcept
{
    return get_source(integers[generator_tag::source_ta]);
}

source parameter::get_generator_value() noexcept
{
    return get_source(integers[generator_tag::source_value]);
}

source parameter::get_dynamic_queue_ta() noexcept
{
    return get_source(integers[dynamic_queue_tag::source_ta]);
}

source parameter::get_priority_queue_ta() noexcept
{
    return get_source(integers[priority_queue_tag::source_ta]);
}

} // namespace irt
