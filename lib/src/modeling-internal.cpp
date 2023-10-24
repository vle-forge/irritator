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
                      child_flags            param = child_flags_none) noexcept
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

static void connect(modeling&          mod,
                    generic_component& c,
                    child_id           src,
                    int                port_src,
                    child_id           dst,
                    int                port_dst) noexcept
{
    [[maybe_unused]] const auto ret = mod.connect(
      c, mod.children.get(src), port_src, mod.children.get(dst), port_dst);

    irt_assert(is_success(ret));
}

static void add_integrator_component_port(modeling&          mod,
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

    irt_assert(is_success(mod.connect_input(com, *x_port, *c, 1)));
    irt_assert(is_success(mod.connect_output(com, *c, 0, *y_port)));

    c->unique_id = com.make_next_unique_id();
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

    mod.children_parameters[idx].reals[0] = coeff_0;
    mod.children_parameters[idx].reals[1] = coeff_1;
}

static void affect_abstract_cross(modeling&      mod,
                                  const child_id id,
                                  const real     threshold) noexcept
{
    const auto idx = get_index(id);

    mod.children_parameters[idx].reals[0] = threshold;
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

    bool success = mod.children.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "X", child_flags_both);
    affect_abstract_integrator(mod, integrator_a, 18.0_r, 0.1_r);

    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "Y", child_flags_both);
    affect_abstract_integrator(mod, integrator_b, 7.0_r, 0.1_r);

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "X+XY", child_flags_configurable);
    affect_abstract_multiplier(mod, sum_a, 2.0_r, -0.4_r);

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "Y+XY", child_flags_configurable);
    affect_abstract_multiplier(mod, sum_b, -1.0_r, 0.1_r);

    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);
    connect(mod, com, integrator_a, 0, sum_a, 0);
    connect(mod, com, integrator_b, 0, sum_b, 0);
    connect(mod, com, integrator_a, 0, product, 0);
    connect(mod, com, integrator_b, 0, product, 1);
    connect(mod, com, product, 0, sum_a, 1);
    connect(mod, com, product, 0, sum_b, 1);

    add_integrator_component_port(mod, dst, com, integrator_a.second, "X");
    add_integrator_component_port(mod, dst, com, integrator_b.second, "Y");

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.children.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

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

    auto integrator =
      alloc<abstract_integrator<QssLevel>>(mod, com, "lif", child_flags_both);
    affect_abstract_integrator(mod, integrator, 0._r, 0.001_r);

    auto cross = alloc<abstract_cross<QssLevel>>(mod, com);
    affect_abstract_cross(mod, cross, Vt);

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(mod, dst, com, integrator.second, "V");

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling&          mod,
                      component&         dst,
                      generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.children.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto cst     = alloc<constant>(mod, com);
    auto cst2    = alloc<constant>(mod, com);
    auto cst3    = alloc<constant>(mod, com);
    auto sum_a   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_b   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_c   = alloc<abstract_wsum<QssLevel, 4>>(mod, com);
    auto sum_d   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "V", child_flags_both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "U", child_flags_both);
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

    affect_abstract_cross(mod, cross, vt);
    affect_abstract_cross(mod, cross2, vt);

    affect_abstract_integrator(mod, integrator_a, 0.0_r, 0.01_r);
    affect_abstract_integrator(mod, integrator_b, 0.0_r, 0.01_r);

    sum_a.first->default_input_coeffs[0] = 1.0_r;
    sum_a.first->default_input_coeffs[1] = -1.0_r;

    sum_b.first->default_input_coeffs[0] = -a;
    sum_b.first->default_input_coeffs[1] = a * b;

    sum_c.first->default_input_coeffs[0] = 0.04_r;
    sum_c.first->default_input_coeffs[1] = 5.0_r;
    sum_c.first->default_input_coeffs[2] = 140.0_r;
    sum_c.first->default_input_coeffs[3] = 1.0_r;

    sum_d.first->default_input_coeffs[0] = 1.0_r;
    sum_d.first->default_input_coeffs[1] = d;

    connect(mod, com, integrator_a, 0, cross, 0);
    connect(mod, com, cst2, 0, cross, 1);
    connect(mod, com, integrator_a, 0, cross, 2);

    connect(mod, com, cross, 1, product, 0);
    connect(mod, com, cross, 1, product, 1);
    connect(mod, com, product, 0, sum_c, 0);
    connect(mod, com, cross, 1, sum_c, 1);
    connect(mod, com, cross, 1, sum_b, 1);

    connect(mod, com, cst, 0, sum_c, 2);
    connect(mod, com, cst3, 0, sum_c, 3);

    connect(mod, com, sum_c, 0, sum_a, 0);
    connect(mod, com, cross2, 1, sum_a, 1);
    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, cross, 0, integrator_a, 1);

    connect(mod, com, cross2, 1, sum_b, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);

    connect(mod, com, cross2, 0, integrator_b, 1);
    connect(mod, com, integrator_a, 0, cross2, 0);
    connect(mod, com, integrator_b, 0, cross2, 2);
    connect(mod, com, sum_d, 0, cross2, 1);
    connect(mod, com, integrator_b, 0, sum_d, 0);
    connect(mod, com, cst, 0, sum_d, 1);

    add_integrator_component_port(mod, dst, com, integrator_a.second, "V");
    add_integrator_component_port(mod, dst, com, integrator_b.second, "U");

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling&          mod,
                       component&         dst,
                       generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum      = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    auto product1 = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product2 = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "X", child_flags_both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "Y", child_flags_both);

    integrator_a.first->default_X  = 0.0_r;
    integrator_a.first->default_dQ = 0.001_r;

    integrator_b.first->default_X  = 10.0_r;
    integrator_b.first->default_dQ = 0.001_r;

    constexpr double mu                = 4.0_r;
    sum.first->default_input_coeffs[0] = mu;
    sum.first->default_input_coeffs[1] = -mu;
    sum.first->default_input_coeffs[2] = -1.0_r;

    connect(mod, com, integrator_b, 0, integrator_a, 0);
    connect(mod, com, sum, 0, integrator_b, 0);
    connect(mod, com, integrator_b, 0, sum, 0);
    connect(mod, com, product2, 0, sum, 1);
    connect(mod, com, integrator_a, 0, sum, 2);
    connect(mod, com, integrator_b, 0, product1, 0);
    connect(mod, com, integrator_a, 0, product1, 1);
    connect(mod, com, product1, 0, product2, 0);
    connect(mod, com, integrator_a, 0, product2, 1);

    add_integrator_component_port(mod, dst, com, integrator_a.second, "X");
    add_integrator_component_port(mod, dst, com, integrator_b.second, "Y");

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling&          mod,
                        component&         dst,
                        generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto integrator =
      alloc<abstract_integrator<QssLevel>>(mod, com, "V", child_flags_both);
    auto cross     = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cst       = alloc<constant>(mod, com);
    auto cst_cross = alloc<constant>(mod, com);

    constexpr real tau = 10.0_r;
    constexpr real Vt  = -1.0_r;
    constexpr real V0  = -10.0_r;
    constexpr real Vr  = 0.0_r;

    sum.first->default_input_coeffs[0] = -1.0_r / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    cst.first->default_value       = 1.0_r;
    cst_cross.first->default_value = Vr;

    integrator.first->default_X  = 0.0_r;
    integrator.first->default_dQ = 0.001_r;

    cross.first->default_threshold = Vt;
    cross.first->default_detect_up = false;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(mod, dst, com, integrator.second, "V");

    return status::success;
}

template<int QssLevel>
status add_seirs(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(17);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto dS =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dS", child_flags_both);
    dS.first->default_X  = 0.999_r;
    dS.first->default_dQ = 0.0001_r;

    auto dE =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dE", child_flags_both);
    dE.first->default_X  = 0.0_r;
    dE.first->default_dQ = 0.0001_r;

    auto dI =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dI", child_flags_both);
    dI.first->default_X  = 0.001_r;
    dI.first->default_dQ = 0.0001_r;

    auto dR =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dR", child_flags_both);
    dR.first->default_X  = 0.0_r;
    dR.first->default_dQ = 0.0001_r;

    auto beta                  = alloc<constant>(mod, com, "beta");
    beta.first->default_value  = 0.5_r;
    auto rho                   = alloc<constant>(mod, com, "rho");
    rho.first->default_value   = 0.00274397_r;
    auto sigma                 = alloc<constant>(mod, com, "sigma");
    sigma.first->default_value = 0.33333_r;
    auto gamma                 = alloc<constant>(mod, com, "gamma");
    gamma.first->default_value = 0.142857_r;

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(mod, com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "rho R - beta S I");
    rho_R_beta_S_I.first->default_input_coeffs[0] = 1.0_r;
    rho_R_beta_S_I.first->default_input_coeffs[1] = -1.0_r;
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "beta S I - sigma E");
    beta_S_I_sigma_E.first->default_input_coeffs[0] = 1.0_r;
    beta_S_I_sigma_E.first->default_input_coeffs[1] = -1.0_r;

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(mod, com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "sigma E - gamma I");
    sigma_E_gamma_I.first->default_input_coeffs[0] = 1.0_r;
    sigma_E_gamma_I.first->default_input_coeffs[1] = -1.0_r;
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "gamma I - rho R");
    gamma_I_rho_R.first->default_input_coeffs[0] = -1.0_r;
    gamma_I_rho_R.first->default_input_coeffs[1] = 1.0_r;

    connect(mod, com, rho, 0, rho_R, 0);
    connect(mod, com, beta, 0, rho_R, 1);
    connect(mod, com, beta, 0, beta_S, 1);
    connect(mod, com, dS, 0, beta_S, 0);
    connect(mod, com, dI, 0, beta_S_I, 0);
    connect(mod, com, beta_S, 0, beta_S_I, 1);
    connect(mod, com, rho_R, 0, rho_R_beta_S_I, 0);
    connect(mod, com, beta_S_I, 0, rho_R_beta_S_I, 1);
    connect(mod, com, rho_R_beta_S_I, 0, dS, 0);
    connect(mod, com, dE, 0, sigma_E, 0);
    connect(mod, com, sigma, 0, sigma_E, 1);
    connect(mod, com, beta_S_I, 0, beta_S_I_sigma_E, 0);
    connect(mod, com, sigma_E, 0, beta_S_I_sigma_E, 1);
    connect(mod, com, beta_S_I_sigma_E, 0, dE, 0);
    connect(mod, com, dI, 0, gamma_I, 0);
    connect(mod, com, gamma, 0, gamma_I, 1);
    connect(mod, com, sigma_E, 0, sigma_E_gamma_I, 0);
    connect(mod, com, gamma_I, 0, sigma_E_gamma_I, 1);
    connect(mod, com, sigma_E_gamma_I, 0, dI, 0);
    connect(mod, com, rho_R, 0, gamma_I_rho_R, 0);
    connect(mod, com, gamma_I, 0, gamma_I_rho_R, 1);
    connect(mod, com, gamma_I_rho_R, 0, dR, 0);

    add_integrator_component_port(mod, dst, com, dS.second, "S");
    add_integrator_component_port(mod, dst, com, dE.second, "E");
    add_integrator_component_port(mod, dst, com, dI.second, "I");
    add_integrator_component_port(mod, dst, com, dR.second, "R");

    return status::success;
}

status modeling::copy(internal_component src, component& dst) noexcept
{
    irt_return_if_fail(generic_components.can_alloc(),
                       status::data_array_not_enough_memory);

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

    irt_unreachable();
}

} // namespace irt
