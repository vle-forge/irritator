// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/core.hpp"
#include <irritator/modeling.hpp>

namespace irt {

template<typename Dynamics>
static child_id alloc(modeling&              mod,
                      generic_component&     parent,
                      const std::string_view name  = {},
                      bitflags<child_flags>  param = child_flags::none) noexcept
{
    irt_assert(!mod.children.full());
    irt_assert(!mod.hsms.full());

    auto&      child = mod.alloc(parent, dynamics_typeof<Dynamics>());
    const auto id    = mod.children.get_id(child);
    const auto index = get_index(id);

    child.flags = param;

    mod.children_names[index] = name;
    mod.children_parameters[index].clear();

    return id;
}

static status connect(modeling&          mod,
                      generic_component& c,
                      child_id           src,
                      int                port_src,
                      child_id           dst,
                      int                port_dst) noexcept
{
    return mod.connect(
      c, mod.children.get(src), port_src, mod.children.get(dst), port_dst);
}

static status add_integrator_component_port(modeling&          mod,
                                            component&         dst,
                                            generic_component& com,
                                            child_id           id,
                                            std::string_view   port) noexcept
{
    auto  x_port_id = mod.get_or_add_x_index(dst, port);
    auto  y_port_id = mod.get_or_add_y_index(dst, port);
    auto* x_port    = mod.ports.try_to_get(x_port_id);
    auto* y_port    = mod.ports.try_to_get(y_port_id);
    auto* c         = mod.children.try_to_get(id);

    irt_assert(x_port);
    irt_assert(y_port);
    irt_assert(c);

    irt_check(mod.connect_input(com, *x_port, *c, 1));
    irt_check(mod.connect_output(com, *c, 0, *y_port));
    c->unique_id = com.make_next_unique_id();

    return success();
}

static void affect_abstract_integrator(modeling&      mod,
                                       const child_id id,
                                       const real     X,
                                       const real     dQ) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = X;
    mod.children_parameters[idx].reals[1] = dQ;
}

static void affect_abstract_multiplier(modeling&      mod,
                                       const child_id id,
                                       const real     coeff_0,
                                       const real     coeff_1) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = coeff_0;
    mod.children_parameters[idx].reals[1] = coeff_1;
}

static void affect_abstract_wsum(modeling&      mod,
                                 const child_id id,
                                 const real     coeff_0,
                                 const real     coeff_1) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = zero;
    mod.children_parameters[idx].reals[1] = zero;
    mod.children_parameters[idx].reals[2] = coeff_0;
    mod.children_parameters[idx].reals[3] = coeff_1;
}

static void affect_abstract_wsum(modeling&      mod,
                                 const child_id id,
                                 const real     coeff_0,
                                 const real     coeff_1,
                                 const real     coeff_2) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = zero;
    mod.children_parameters[idx].reals[1] = zero;
    mod.children_parameters[idx].reals[2] = zero;
    mod.children_parameters[idx].reals[3] = coeff_0;
    mod.children_parameters[idx].reals[4] = coeff_1;
    mod.children_parameters[idx].reals[5] = coeff_2;
}

static void affect_abstract_wsum(modeling&      mod,
                                 const child_id id,
                                 const real     coeff_0,
                                 const real     coeff_1,
                                 const real     coeff_2,
                                 const real     coeff_3) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = zero;
    mod.children_parameters[idx].reals[1] = zero;
    mod.children_parameters[idx].reals[2] = zero;
    mod.children_parameters[idx].reals[3] = zero;
    mod.children_parameters[idx].reals[4] = coeff_0;
    mod.children_parameters[idx].reals[5] = coeff_1;
    mod.children_parameters[idx].reals[6] = coeff_2;
    mod.children_parameters[idx].reals[7] = coeff_3;
}

static void affect_abstract_cross(modeling&      mod,
                                  const child_id id,
                                  const real     threshold,
                                  const bool     detect_up) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0]    = threshold;
    mod.children_parameters[idx].integers[0] = detect_up ? 1 : 0;
}

static void affect_abstract_constant(modeling&      mod,
                                     const child_id id,
                                     const real     value,
                                     const real     offset) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = value;
    mod.children_parameters[idx].reals[1] = offset;
}

template<int QssLevel>
status add_lotka_volterra(modeling&          mod,
                          component&         dst,
                          generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!mod.children.can_alloc(5))
        return new_error(modeling::children_error{}, container_full_error{});

    auto integrator_a = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "X",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, integrator_a, 18.0_r, 0.1_r);

    auto integrator_b = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "Y",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, integrator_b, 7.0_r, 0.1_r);

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "X+XY", bitflags<child_flags>(child_flags::configurable));
    affect_abstract_wsum(mod, sum_a, 2.0_r, -0.4_r);

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "Y+XY", bitflags<child_flags>(child_flags::configurable));
    affect_abstract_wsum(mod, sum_b, -1.0_r, 0.1_r);

    irt_check(connect(mod, com, sum_a, 0, integrator_a, 0));
    irt_check(connect(mod, com, sum_b, 0, integrator_b, 0));
    irt_check(connect(mod, com, integrator_a, 0, sum_a, 0));
    irt_check(connect(mod, com, integrator_b, 0, sum_b, 0));
    irt_check(connect(mod, com, integrator_a, 0, product, 0));
    irt_check(connect(mod, com, integrator_b, 0, product, 1));
    irt_check(connect(mod, com, product, 0, sum_a, 1));
    irt_check(connect(mod, com, product, 0, sum_b, 1));

    irt_check(add_integrator_component_port(mod, dst, com, integrator_a, "X"));
    irt_check(add_integrator_component_port(mod, dst, com, integrator_b, "Y"));

    return success();
}

template<int QssLevel>
status add_lif(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!mod.children.can_alloc(5))
        return new_error(modeling::children_error{}, container_full_error{});

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst = alloc<constant>(mod, com);
    affect_abstract_constant(mod, cst, 1.0, 0.0);

    auto cst_cross = alloc<constant>(mod, com);
    affect_abstract_constant(mod, cst, Vr, 0.0);

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    affect_abstract_wsum(mod, sum, -irt::one / tau, V0 / tau);

    auto integrator = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "lif",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, integrator, 0._r, 0.001_r);

    auto cross = alloc<abstract_cross<QssLevel>>(mod, com);
    affect_abstract_cross(mod, cross, Vt, false);

    irt_check(connect(mod, com, cross, 0, integrator, 1));
    irt_check(connect(mod, com, cross, 1, sum, 0));
    irt_check(connect(mod, com, integrator, 0, cross, 0));
    irt_check(connect(mod, com, integrator, 0, cross, 2));
    irt_check(connect(mod, com, cst_cross, 0, cross, 1));
    irt_check(connect(mod, com, cst, 0, sum, 1));
    irt_check(connect(mod, com, sum, 0, integrator, 0));

    irt_check(add_integrator_component_port(mod, dst, com, integrator, "V"));

    return success();
}

template<int QssLevel>
status add_izhikevich(modeling&          mod,
                      component&         dst,
                      generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!mod.children.can_alloc(12))
        return new_error(modeling::children_error{}, container_full_error{});

    auto cst          = alloc<constant>(mod, com);
    auto cst2         = alloc<constant>(mod, com);
    auto cst3         = alloc<constant>(mod, com);
    auto sum_a        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_b        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_c        = alloc<abstract_wsum<QssLevel, 4>>(mod, com);
    auto sum_d        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto product      = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "V",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "U",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    auto cross  = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cross2 = alloc<abstract_cross<QssLevel>>(mod, com);

    constexpr irt::real a  = 0.2_r;
    constexpr irt::real b  = 2.0_r;
    constexpr irt::real c  = -56.0_r;
    constexpr irt::real d  = -16.0_r;
    constexpr irt::real I  = -99.0_r;
    constexpr irt::real vt = 30.0_r;

    affect_abstract_constant(mod, cst, 1.0_r, 0.0);
    affect_abstract_constant(mod, cst2, c, 0.0);
    affect_abstract_constant(mod, cst3, I, 0.0);

    affect_abstract_cross(mod, cross, vt, true);
    affect_abstract_cross(mod, cross2, vt, true);

    affect_abstract_integrator(mod, integrator_a, 0.0_r, 0.01_r);
    affect_abstract_integrator(mod, integrator_b, 0.0_r, 0.01_r);

    affect_abstract_wsum(mod, sum_a, 1.0_r, -1.0_r);
    affect_abstract_wsum(mod, sum_b, -a, a * b);
    affect_abstract_wsum(mod, sum_c, 0.04_r, 5.0_r, 140.0_r, 1.0_r);
    affect_abstract_wsum(mod, sum_d, 1.0_r, d);

    irt_check(connect(mod, com, integrator_a, 0, cross, 0));
    irt_check(connect(mod, com, cst2, 0, cross, 1));
    irt_check(connect(mod, com, integrator_a, 0, cross, 2));

    irt_check(connect(mod, com, cross, 1, product, 0));
    irt_check(connect(mod, com, cross, 1, product, 1));
    irt_check(connect(mod, com, product, 0, sum_c, 0));
    irt_check(connect(mod, com, cross, 1, sum_c, 1));
    irt_check(connect(mod, com, cross, 1, sum_b, 1));

    irt_check(connect(mod, com, cst, 0, sum_c, 2));
    irt_check(connect(mod, com, cst3, 0, sum_c, 3));

    irt_check(connect(mod, com, sum_c, 0, sum_a, 0));
    irt_check(connect(mod, com, cross2, 1, sum_a, 1));
    irt_check(connect(mod, com, sum_a, 0, integrator_a, 0));
    irt_check(connect(mod, com, cross, 0, integrator_a, 1));

    irt_check(connect(mod, com, cross2, 1, sum_b, 0));
    irt_check(connect(mod, com, sum_b, 0, integrator_b, 0));

    irt_check(connect(mod, com, cross2, 0, integrator_b, 1));
    irt_check(connect(mod, com, integrator_a, 0, cross2, 0));
    irt_check(connect(mod, com, integrator_b, 0, cross2, 2));
    irt_check(connect(mod, com, sum_d, 0, cross2, 1));
    irt_check(connect(mod, com, integrator_b, 0, sum_d, 0));
    irt_check(connect(mod, com, cst, 0, sum_d, 1));

    irt_check(add_integrator_component_port(mod, dst, com, integrator_a, "V"));
    irt_check(add_integrator_component_port(mod, dst, com, integrator_b, "U"));

    return success();
}

template<int QssLevel>
status add_van_der_pol(modeling&          mod,
                       component&         dst,
                       generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!mod.children.can_alloc(5))
        return new_error(modeling::children_error{}, container_full_error{});

    auto sum          = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    auto product1     = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product2     = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "X",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "Y",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));

    affect_abstract_integrator(mod, integrator_a, 0.0_r, 0.001_r);
    affect_abstract_integrator(mod, integrator_b, 10.0_r, 0.001_r);

    constexpr double mu = 4.0_r;
    affect_abstract_wsum(mod, sum, mu, -mu, -1.0_r);

    irt_check(connect(mod, com, integrator_b, 0, integrator_a, 0));
    irt_check(connect(mod, com, sum, 0, integrator_b, 0));
    irt_check(connect(mod, com, integrator_b, 0, sum, 0));
    irt_check(connect(mod, com, product2, 0, sum, 1));
    irt_check(connect(mod, com, integrator_a, 0, sum, 2));
    irt_check(connect(mod, com, integrator_b, 0, product1, 0));
    irt_check(connect(mod, com, integrator_a, 0, product1, 1));
    irt_check(connect(mod, com, product1, 0, product2, 0));
    irt_check(connect(mod, com, integrator_a, 0, product2, 1));

    irt_check(add_integrator_component_port(mod, dst, com, integrator_a, "X"));
    irt_check(add_integrator_component_port(mod, dst, com, integrator_b, "Y"));

    return success();
}

template<int QssLevel>
status add_negative_lif(modeling&          mod,
                        component&         dst,
                        generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!mod.children.can_alloc(5))
        return new_error(modeling::children_error{}, container_full_error{});

    auto sum        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto integrator = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "V",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    auto cross     = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cst       = alloc<constant>(mod, com);
    auto cst_cross = alloc<constant>(mod, com);

    constexpr real tau = 10.0_r;
    constexpr real Vt  = -1.0_r;
    constexpr real V0  = -10.0_r;
    constexpr real Vr  = 0.0_r;

    affect_abstract_wsum(mod, sum, -1.0_r / tau, V0 / tau);
    affect_abstract_constant(mod, cst, 1.0_r, 0.0_r);
    affect_abstract_constant(mod, cst_cross, Vr, 0.0_r);
    affect_abstract_integrator(mod, integrator, 0.0_r, 0.001_r);
    affect_abstract_cross(mod, cross, Vt, true);

    irt_check(connect(mod, com, cross, 0, integrator, 1));
    irt_check(connect(mod, com, cross, 1, sum, 0));
    irt_check(connect(mod, com, integrator, 0, cross, 0));
    irt_check(connect(mod, com, integrator, 0, cross, 2));
    irt_check(connect(mod, com, cst_cross, 0, cross, 1));
    irt_check(connect(mod, com, cst, 0, sum, 1));
    irt_check(connect(mod, com, sum, 0, integrator, 0));

    irt_check(add_integrator_component_port(mod, dst, com, integrator, "V"));

    return success();
}

template<int QssLevel>
status add_seirs(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!mod.children.can_alloc(17))
        return new_error(modeling::children_error{}, container_full_error{});

    auto dS = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dS",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, dS, 0.999_r, 0.0001_r);

    auto dE = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dE",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, dE, 0.0_r, 0.0001_r);

    auto dI = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dI",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, dI, 0.001_r, 0.0001_r);

    auto dR = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dR",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(mod, dR, 0.0_r, 0.0001_r);

    auto beta = alloc<constant>(mod, com, "beta");
    affect_abstract_constant(mod, beta, 0.5_r, 0.0_r);
    auto rho = alloc<constant>(mod, com, "rho");
    affect_abstract_constant(mod, rho, 0.00274397_r, 0.0_r);
    auto sigma = alloc<constant>(mod, com, "sigma");
    affect_abstract_constant(mod, sigma, 0.33333_r, 0.0_r);
    auto gamma = alloc<constant>(mod, com, "gamma");
    affect_abstract_constant(mod, gamma, 0.142857_r, 0.0_r);

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(mod, com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "rho R - beta S I");
    affect_abstract_wsum(mod, rho_R_beta_S_I, 1.0_r, -1.0_r);
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "beta S I - sigma E");
    affect_abstract_wsum(mod, beta_S_I_sigma_E, 1.0_r, -1.0_r);

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(mod, com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "sigma E - gamma I");
    affect_abstract_wsum(mod, sigma_E_gamma_I, 1.0_r, -1.0_r);
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "gamma I - rho R");
    affect_abstract_wsum(mod, gamma_I_rho_R, -1.0_r, 1.0_r);

    irt_check(connect(mod, com, rho, 0, rho_R, 0));
    irt_check(connect(mod, com, beta, 0, rho_R, 1));
    irt_check(connect(mod, com, beta, 0, beta_S, 1));
    irt_check(connect(mod, com, dS, 0, beta_S, 0));
    irt_check(connect(mod, com, dI, 0, beta_S_I, 0));
    irt_check(connect(mod, com, beta_S, 0, beta_S_I, 1));
    irt_check(connect(mod, com, rho_R, 0, rho_R_beta_S_I, 0));
    irt_check(connect(mod, com, beta_S_I, 0, rho_R_beta_S_I, 1));
    irt_check(connect(mod, com, rho_R_beta_S_I, 0, dS, 0));
    irt_check(connect(mod, com, dE, 0, sigma_E, 0));
    irt_check(connect(mod, com, sigma, 0, sigma_E, 1));
    irt_check(connect(mod, com, beta_S_I, 0, beta_S_I_sigma_E, 0));
    irt_check(connect(mod, com, sigma_E, 0, beta_S_I_sigma_E, 1));
    irt_check(connect(mod, com, beta_S_I_sigma_E, 0, dE, 0));
    irt_check(connect(mod, com, dI, 0, gamma_I, 0));
    irt_check(connect(mod, com, gamma, 0, gamma_I, 1));
    irt_check(connect(mod, com, sigma_E, 0, sigma_E_gamma_I, 0));
    irt_check(connect(mod, com, gamma_I, 0, sigma_E_gamma_I, 1));
    irt_check(connect(mod, com, sigma_E_gamma_I, 0, dI, 0));
    irt_check(connect(mod, com, rho_R, 0, gamma_I_rho_R, 0));
    irt_check(connect(mod, com, gamma_I, 0, gamma_I_rho_R, 1));
    irt_check(connect(mod, com, gamma_I_rho_R, 0, dR, 0));

    irt_check(add_integrator_component_port(mod, dst, com, dS, "S"));
    irt_check(add_integrator_component_port(mod, dst, com, dE, "E"));
    irt_check(add_integrator_component_port(mod, dst, com, dI, "I"));
    irt_check(add_integrator_component_port(mod, dst, com, dR, "R"));

    return success();
}

status modeling::copy(internal_component src, component& dst) noexcept
{
    if (!generic_components.can_alloc())
        return new_error(part::generic_components, container_full_error{});

    auto& s_compo     = generic_components.alloc();
    auto  s_compo_id  = generic_components.get_id(s_compo);
    dst.type          = component_type::simple;
    dst.id.generic_id = s_compo_id;

    switch (src) {
    case internal_component::qss1_izhikevich:
        return add_izhikevich<1>(*this, dst, s_compo);
    case internal_component::qss1_lif:
        return add_lif<1>(*this, dst, s_compo);
    case internal_component::qss1_lotka_volterra:
        return add_lotka_volterra<1>(*this, dst, s_compo);
    case internal_component::qss1_negative_lif:
        return add_negative_lif<1>(*this, dst, s_compo);
    case internal_component::qss1_seirs:
        return add_seirs<1>(*this, dst, s_compo);
    case internal_component::qss1_van_der_pol:
        return add_van_der_pol<1>(*this, dst, s_compo);
    case internal_component::qss2_izhikevich:
        return add_izhikevich<2>(*this, dst, s_compo);
    case internal_component::qss2_lif:
        return add_lif<2>(*this, dst, s_compo);
    case internal_component::qss2_lotka_volterra:
        return add_lotka_volterra<2>(*this, dst, s_compo);
    case internal_component::qss2_negative_lif:
        return add_negative_lif<2>(*this, dst, s_compo);
    case internal_component::qss2_seirs:
        return add_seirs<2>(*this, dst, s_compo);
    case internal_component::qss2_van_der_pol:
        return add_van_der_pol<2>(*this, dst, s_compo);
    case internal_component::qss3_izhikevich:
        return add_izhikevich<3>(*this, dst, s_compo);
    case internal_component::qss3_lif:
        return add_lif<3>(*this, dst, s_compo);
    case internal_component::qss3_lotka_volterra:
        return add_lotka_volterra<3>(*this, dst, s_compo);
    case internal_component::qss3_negative_lif:
        return add_negative_lif<3>(*this, dst, s_compo);
    case internal_component::qss3_seirs:
        return add_seirs<3>(*this, dst, s_compo);
    case internal_component::qss3_van_der_pol:
        return add_van_der_pol<3>(*this, dst, s_compo);
    }

    unreachable();

    return success();
}

} // namespace irt
