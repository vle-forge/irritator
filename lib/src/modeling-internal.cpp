// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling.hpp>

namespace irt {

template<typename Dynamics>
static child_id alloc(modeling&              mod,
                      generic_component&     parent,
                      const std::string_view name = {},
                      bitflags<child_flags>  param =
                        bitflags<child_flags>(child_flags::none)) noexcept
{
    auto&      child = mod.alloc(parent, dynamics_typeof<Dynamics>());
    const auto id    = parent.children.get_id(child);
    const auto index = get_index(id);

    child.flags = param;

    parent.children_names[index] = name;
    parent.children_parameters[index].clear();

    return id;
}

static auto is_connection_exits(const generic_component& c,
                                const child_id           src,
                                const int                port_src,
                                const child_id           dst,
                                const int port_dst) noexcept -> bool
{
    const connection cpy(src, port_src, dst, port_dst);

    for (const auto& con : c.connections)
        if (cpy == con)
            return true;

    return false;
}

static status connect(generic_component& c,
                      child_id           src,
                      int                port_src,
                      child_id           dst,
                      int                port_dst) noexcept
{
    if (not c.connections.can_alloc())
        return new_error(modeling_errc::generic_connection_container_full);

    if (is_connection_exits(c, src, port_src, dst, port_dst))
        return new_error(modeling_errc::generic_connection_already_exist);

    c.connections.alloc(src, port_src, dst, port_dst);

    return success();
}

static status add_integrator_component_port(component&         dst,
                                            generic_component& com,
                                            child_id           id,
                                            std::string_view   port) noexcept
{
    const auto x_port_id = dst.get_or_add_x(port);
    const auto y_port_id = dst.get_or_add_y(port);
    auto*      c         = com.children.try_to_get(id);

    debug::ensure(is_defined(x_port_id));
    debug::ensure(is_defined(y_port_id));
    debug::ensure(c);

    irt_check(com.connect_input(
      x_port_id, com.children.get(id), connection::port{ .model = 1 }));
    irt_check(com.connect_output(
      y_port_id, com.children.get(id), connection::port{ .model = 0 }));

    com.children_names[id] = com.make_unique_name_id(id);

    return success();
}

static void affect_abstract_integrator(generic_component& gen,
                                       const child_id     id,
                                       const real         X,
                                       const real         dQ) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0] = X;
    gen.children_parameters[idx].reals[1] = dQ;
}

static void affect_abstract_wsum(generic_component& gen,
                                 const child_id     id,
                                 const real         coeff_0,
                                 const real         coeff_1) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0] = zero;
    gen.children_parameters[idx].reals[1] = zero;
    gen.children_parameters[idx].reals[2] = coeff_0;
    gen.children_parameters[idx].reals[3] = coeff_1;
}

static void affect_abstract_wsum(generic_component& gen,
                                 const child_id     id,
                                 const real         coeff_0,
                                 const real         coeff_1,
                                 const real         coeff_2) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0] = zero;
    gen.children_parameters[idx].reals[1] = zero;
    gen.children_parameters[idx].reals[2] = zero;
    gen.children_parameters[idx].reals[3] = coeff_0;
    gen.children_parameters[idx].reals[4] = coeff_1;
    gen.children_parameters[idx].reals[5] = coeff_2;
}

static void affect_abstract_wsum(generic_component& gen,
                                 const child_id     id,
                                 const real         coeff_0,
                                 const real         coeff_1,
                                 const real         coeff_2,
                                 const real         coeff_3) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0] = zero;
    gen.children_parameters[idx].reals[1] = zero;
    gen.children_parameters[idx].reals[2] = zero;
    gen.children_parameters[idx].reals[3] = zero;
    gen.children_parameters[idx].reals[4] = coeff_0;
    gen.children_parameters[idx].reals[5] = coeff_1;
    gen.children_parameters[idx].reals[6] = coeff_2;
    gen.children_parameters[idx].reals[7] = coeff_3;
}

static void affect_abstract_cross(generic_component& gen,
                                  const child_id     id,
                                  const real         threshold,
                                  const bool         detect_up) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0]    = threshold;
    gen.children_parameters[idx].integers[0] = detect_up ? 1 : 0;
}

static void affect_abstract_constant(generic_component& gen,
                                     const child_id     id,
                                     const real         value,
                                     const real         offset) noexcept
{
    const auto idx = get_index(id);

    gen.children_parameters[idx].reals[0] = value;
    gen.children_parameters[idx].reals[1] = offset;
}

template<int QssLevel>
status add_lotka_volterra(modeling&          mod,
                          component&         dst,
                          generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!com.children.can_alloc(5))
        return new_error(modeling_errc::generic_children_container_full);

    auto integrator_a = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "X",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, integrator_a, 18.0_r, 0.1_r);

    auto integrator_b = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "Y",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, integrator_b, 7.0_r, 0.1_r);

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "X+XY", bitflags<child_flags>(child_flags::configurable));
    affect_abstract_wsum(com, sum_a, 2.0_r, -0.4_r);

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "Y+XY", bitflags<child_flags>(child_flags::configurable));
    affect_abstract_wsum(com, sum_b, -1.0_r, 0.1_r);

    irt_check(connect(com, sum_a, 0, integrator_a, 0));
    irt_check(connect(com, sum_b, 0, integrator_b, 0));
    irt_check(connect(com, integrator_a, 0, sum_a, 0));
    irt_check(connect(com, integrator_b, 0, sum_b, 0));
    irt_check(connect(com, integrator_a, 0, product, 0));
    irt_check(connect(com, integrator_b, 0, product, 1));
    irt_check(connect(com, product, 0, sum_a, 1));
    irt_check(connect(com, product, 0, sum_b, 1));

    irt_check(add_integrator_component_port(dst, com, integrator_a, "X"));
    irt_check(add_integrator_component_port(dst, com, integrator_b, "Y"));

    return success();
}

template<int QssLevel>
status add_lif(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!com.children.can_alloc(5))
        return new_error(modeling_errc::generic_children_container_full);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst = alloc<constant>(mod, com);
    affect_abstract_constant(com, cst, 1.0, 0.0);

    auto cst_cross = alloc<constant>(mod, com);
    affect_abstract_constant(com, cst, Vr, 0.0);

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    affect_abstract_wsum(com, sum, -irt::one / tau, V0 / tau);

    auto integrator = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "lif",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, integrator, 0._r, 0.001_r);

    auto cross = alloc<abstract_cross<QssLevel>>(mod, com);
    affect_abstract_cross(com, cross, Vt, false);

    irt_check(connect(com, cross, 0, integrator, 1));
    irt_check(connect(com, cross, 1, sum, 0));
    irt_check(connect(com, integrator, 0, cross, 0));
    irt_check(connect(com, integrator, 0, cross, 2));
    irt_check(connect(com, cst_cross, 0, cross, 1));
    irt_check(connect(com, cst, 0, sum, 1));
    irt_check(connect(com, sum, 0, integrator, 0));

    irt_check(add_integrator_component_port(dst, com, integrator, "V"));

    return success();
}

template<int QssLevel>
status add_izhikevich(modeling&          mod,
                      component&         dst,
                      generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!com.children.can_alloc(12))
        return new_error(modeling_errc::generic_children_container_full);

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

    affect_abstract_constant(com, cst, 1.0_r, 0.0);
    affect_abstract_constant(com, cst2, c, 0.0);
    affect_abstract_constant(com, cst3, I, 0.0);

    affect_abstract_cross(com, cross, vt, true);
    affect_abstract_cross(com, cross2, vt, true);

    affect_abstract_integrator(com, integrator_a, 0.0_r, 0.01_r);
    affect_abstract_integrator(com, integrator_b, 0.0_r, 0.01_r);

    affect_abstract_wsum(com, sum_a, 1.0_r, -1.0_r);
    affect_abstract_wsum(com, sum_b, -a, a * b);
    affect_abstract_wsum(com, sum_c, 0.04_r, 5.0_r, 140.0_r, 1.0_r);
    affect_abstract_wsum(com, sum_d, 1.0_r, d);

    irt_check(connect(com, integrator_a, 0, cross, 0));
    irt_check(connect(com, cst2, 0, cross, 1));
    irt_check(connect(com, integrator_a, 0, cross, 2));

    irt_check(connect(com, cross, 1, product, 0));
    irt_check(connect(com, cross, 1, product, 1));
    irt_check(connect(com, product, 0, sum_c, 0));
    irt_check(connect(com, cross, 1, sum_c, 1));
    irt_check(connect(com, cross, 1, sum_b, 1));

    irt_check(connect(com, cst, 0, sum_c, 2));
    irt_check(connect(com, cst3, 0, sum_c, 3));

    irt_check(connect(com, sum_c, 0, sum_a, 0));
    irt_check(connect(com, cross2, 1, sum_a, 1));
    irt_check(connect(com, sum_a, 0, integrator_a, 0));
    irt_check(connect(com, cross, 0, integrator_a, 1));

    irt_check(connect(com, cross2, 1, sum_b, 0));
    irt_check(connect(com, sum_b, 0, integrator_b, 0));

    irt_check(connect(com, cross2, 0, integrator_b, 1));
    irt_check(connect(com, integrator_a, 0, cross2, 0));
    irt_check(connect(com, integrator_b, 0, cross2, 2));
    irt_check(connect(com, sum_d, 0, cross2, 1));
    irt_check(connect(com, integrator_b, 0, sum_d, 0));
    irt_check(connect(com, cst, 0, sum_d, 1));

    irt_check(add_integrator_component_port(dst, com, integrator_a, "V"));
    irt_check(add_integrator_component_port(dst, com, integrator_b, "U"));

    return success();
}

template<int QssLevel>
status add_van_der_pol(modeling&          mod,
                       component&         dst,
                       generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!com.children.can_alloc(5))
        return new_error(modeling_errc::generic_children_container_full);

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

    affect_abstract_integrator(com, integrator_a, 0.0_r, 0.001_r);
    affect_abstract_integrator(com, integrator_b, 10.0_r, 0.001_r);

    constexpr double mu = 4.0_r;
    affect_abstract_wsum(com, sum, mu, -mu, -1.0_r);

    irt_check(connect(com, integrator_b, 0, integrator_a, 0));
    irt_check(connect(com, sum, 0, integrator_b, 0));
    irt_check(connect(com, integrator_b, 0, sum, 0));
    irt_check(connect(com, product2, 0, sum, 1));
    irt_check(connect(com, integrator_a, 0, sum, 2));
    irt_check(connect(com, integrator_b, 0, product1, 0));
    irt_check(connect(com, integrator_a, 0, product1, 1));
    irt_check(connect(com, product1, 0, product2, 0));
    irt_check(connect(com, integrator_a, 0, product2, 1));

    irt_check(add_integrator_component_port(dst, com, integrator_a, "X"));
    irt_check(add_integrator_component_port(dst, com, integrator_b, "Y"));

    return success();
}

template<int QssLevel>
status add_negative_lif(modeling&          mod,
                        component&         dst,
                        generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!com.children.can_alloc(5))
        return new_error(modeling_errc::generic_children_container_full);

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

    affect_abstract_wsum(com, sum, -1.0_r / tau, V0 / tau);
    affect_abstract_constant(com, cst, 1.0_r, 0.0_r);
    affect_abstract_constant(com, cst_cross, Vr, 0.0_r);
    affect_abstract_integrator(com, integrator, 0.0_r, 0.001_r);
    affect_abstract_cross(com, cross, Vt, true);

    irt_check(connect(com, cross, 0, integrator, 1));
    irt_check(connect(com, cross, 1, sum, 0));
    irt_check(connect(com, integrator, 0, cross, 0));
    irt_check(connect(com, integrator, 0, cross, 2));
    irt_check(connect(com, cst_cross, 0, cross, 1));
    irt_check(connect(com, cst, 0, sum, 1));
    irt_check(connect(com, sum, 0, integrator, 0));

    irt_check(add_integrator_component_port(dst, com, integrator, "V"));

    return success();
}

template<int QssLevel>
status add_seirs(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!com.children.can_alloc(17))
        return new_error(modeling_errc::generic_children_container_full);

    auto dS = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dS",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, dS, 0.999_r, 0.0001_r);

    auto dE = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dE",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, dE, 0.0_r, 0.0001_r);

    auto dI = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dI",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, dI, 0.001_r, 0.0001_r);

    auto dR = alloc<abstract_integrator<QssLevel>>(
      mod,
      com,
      "dR",
      bitflags<child_flags>(child_flags::configurable,
                            child_flags::observable));
    affect_abstract_integrator(com, dR, 0.0_r, 0.0001_r);

    auto beta = alloc<constant>(mod, com, "beta");
    affect_abstract_constant(com, beta, 0.5_r, 0.0_r);
    auto rho = alloc<constant>(mod, com, "rho");
    affect_abstract_constant(com, rho, 0.00274397_r, 0.0_r);
    auto sigma = alloc<constant>(mod, com, "sigma");
    affect_abstract_constant(com, sigma, 0.33333_r, 0.0_r);
    auto gamma = alloc<constant>(mod, com, "gamma");
    affect_abstract_constant(com, gamma, 0.142857_r, 0.0_r);

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(mod, com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "rho R - beta S I");
    affect_abstract_wsum(com, rho_R_beta_S_I, 1.0_r, -1.0_r);
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "beta S I - sigma E");
    affect_abstract_wsum(com, beta_S_I_sigma_E, 1.0_r, -1.0_r);

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(mod, com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "sigma E - gamma I");
    affect_abstract_wsum(com, sigma_E_gamma_I, 1.0_r, -1.0_r);
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "gamma I - rho R");
    affect_abstract_wsum(com, gamma_I_rho_R, -1.0_r, 1.0_r);

    irt_check(connect(com, rho, 0, rho_R, 0));
    irt_check(connect(com, beta, 0, rho_R, 1));
    irt_check(connect(com, beta, 0, beta_S, 1));
    irt_check(connect(com, dS, 0, beta_S, 0));
    irt_check(connect(com, dI, 0, beta_S_I, 0));
    irt_check(connect(com, beta_S, 0, beta_S_I, 1));
    irt_check(connect(com, rho_R, 0, rho_R_beta_S_I, 0));
    irt_check(connect(com, beta_S_I, 0, rho_R_beta_S_I, 1));
    irt_check(connect(com, rho_R_beta_S_I, 0, dS, 0));
    irt_check(connect(com, dE, 0, sigma_E, 0));
    irt_check(connect(com, sigma, 0, sigma_E, 1));
    irt_check(connect(com, beta_S_I, 0, beta_S_I_sigma_E, 0));
    irt_check(connect(com, sigma_E, 0, beta_S_I_sigma_E, 1));
    irt_check(connect(com, beta_S_I_sigma_E, 0, dE, 0));
    irt_check(connect(com, dI, 0, gamma_I, 0));
    irt_check(connect(com, gamma, 0, gamma_I, 1));
    irt_check(connect(com, sigma_E, 0, sigma_E_gamma_I, 0));
    irt_check(connect(com, gamma_I, 0, sigma_E_gamma_I, 1));
    irt_check(connect(com, sigma_E_gamma_I, 0, dI, 0));
    irt_check(connect(com, rho_R, 0, gamma_I_rho_R, 0));
    irt_check(connect(com, gamma_I, 0, gamma_I_rho_R, 1));
    irt_check(connect(com, gamma_I_rho_R, 0, dR, 0));

    irt_check(add_integrator_component_port(dst, com, dS, "S"));
    irt_check(add_integrator_component_port(dst, com, dE, "E"));
    irt_check(add_integrator_component_port(dst, com, dI, "I"));
    irt_check(add_integrator_component_port(dst, com, dR, "R"));

    return success();
}

status modeling::copy(const internal_component src,
                      component&               dst,
                      generic_component&       gen) noexcept
{
    switch (src) {
    case internal_component::qss1_izhikevich:
        irt_check(add_izhikevich<1>(*this, dst, gen));
        break;
    case internal_component::qss1_lif:
        irt_check(add_lif<1>(*this, dst, gen));
        break;
    case internal_component::qss1_lotka_volterra:
        irt_check(add_lotka_volterra<1>(*this, dst, gen));
        break;
    case internal_component::qss1_negative_lif:
        irt_check(add_negative_lif<1>(*this, dst, gen));
        break;
    case internal_component::qss1_seirs:
        irt_check(add_seirs<1>(*this, dst, gen));
        break;
    case internal_component::qss1_van_der_pol:
        irt_check(add_van_der_pol<1>(*this, dst, gen));
        break;
    case internal_component::qss2_izhikevich:
        irt_check(add_izhikevich<2>(*this, dst, gen));
        break;
    case internal_component::qss2_lif:
        irt_check(add_lif<2>(*this, dst, gen));
        break;
    case internal_component::qss2_lotka_volterra:
        irt_check(add_lotka_volterra<2>(*this, dst, gen));
        break;
    case internal_component::qss2_negative_lif:
        irt_check(add_negative_lif<2>(*this, dst, gen));
        break;
    case internal_component::qss2_seirs:
        irt_check(add_seirs<2>(*this, dst, gen));
        break;
    case internal_component::qss2_van_der_pol:
        irt_check(add_van_der_pol<2>(*this, dst, gen));
        break;
    case internal_component::qss3_izhikevich:
        irt_check(add_izhikevich<3>(*this, dst, gen));
        break;
    case internal_component::qss3_lif:
        irt_check(add_lif<3>(*this, dst, gen));
        break;
    case internal_component::qss3_lotka_volterra:
        irt_check(add_lotka_volterra<3>(*this, dst, gen));
        break;
    case internal_component::qss3_negative_lif:
        irt_check(add_negative_lif<3>(*this, dst, gen));
        break;
    case internal_component::qss3_seirs:
        irt_check(add_seirs<3>(*this, dst, gen));
        break;
    case internal_component::qss3_van_der_pol:
        irt_check(add_van_der_pol<3>(*this, dst, gen));
    }

    const auto children = gen.children.size();
    const auto sq       = static_cast<int>(std::floor(std::sqrt(children)));

    auto x = 0;
    auto y = 0;

    debug::ensure(gen.children_positions.size() >= children);
    for (auto& c : gen.children) {
        const auto index = get_index(gen.children.get_id(c));

        const auto px = 100.f + static_cast<float>(x * 240.f);
        const auto py = 100.f + static_cast<float>(y * 200.f);

        gen.children_positions[index].x = px;
        gen.children_positions[index].y = py;

        ++x;
        if (x >= sq) {
            ++y;
            x = 0;
        }
    }

    return success();
}

} // namespace irt
