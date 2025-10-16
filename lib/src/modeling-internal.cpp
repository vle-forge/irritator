// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

namespace irt {

template<typename Dynamics>
static child_id alloc(modeling&              mod,
                      generic_component&     parent,
                      const std::string_view name = {}) noexcept
{
    auto&      child = mod.alloc(parent, dynamics_typeof<Dynamics>());
    const auto id    = parent.children.get_id(child);
    const auto index = get_index(id);

    parent.children_names[index] = name;
    parent.children_parameters[index].clear();

    if constexpr (std::is_same_v<Dynamics, constant>) {
        child.flags.set(child_flags::configurable);

        if (name.empty())
            format(parent.children_names[id], "cst {}", index);
    }

    if constexpr (std::is_same_v<Dynamics, abstract_integrator<1>> or
                  std::is_same_v<Dynamics, abstract_integrator<2>> or
                  std::is_same_v<Dynamics, abstract_integrator<3>>) {
        child.flags.set(child_flags::configurable);
        child.flags.set(child_flags::observable);

        if (name.empty())
            format(parent.children_names[id], "int {}", index);
    }

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

template<int QssLevel>
status add_lotka_volterra(modeling&          mod,
                          component&         dst,
                          generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!com.children.can_alloc(5))
        return new_error(modeling_errc::generic_children_container_full);

    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com, "X");
    com.children_parameters[integrator_a].set_integrator(18.0_r, 0.1_r);

    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com, "Y");
    com.children_parameters[integrator_b].set_integrator(7.0_r, 0.1_r);

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(mod, com, "X+XY");
    com.children_parameters[sum_a].set_wsum2(2.0_r, -0.4_r);

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(mod, com, "Y+XY");
    com.children_parameters[sum_b].set_wsum2(-1.0_r, 0.1_r);

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

    if (not com.children.can_alloc(6) and not com.children.grow<2, 1>())
        return new_error(modeling_errc::generic_children_container_full);

    if (not com.connections.can_alloc(7) and not com.connections.grow<2, 1>())
        return new_error(modeling_errc::generic_children_container_full);

    auto mdl_0 = alloc<irt::constant>(mod, com, "1");
    com.children_parameters[mdl_0].set_constant(1, 0);

    auto mdl_1 = alloc<irt::constant>(mod, com, "-10");
    com.children_parameters[mdl_1].set_constant(-10, 0);

    auto mdl_2 = alloc<irt::qss3_wsum_2>(mod, com);
    com.children_parameters[mdl_2].set_wsum2(-0.1, 1);

    auto mdl_3 = alloc<irt::qss3_integrator>(mod, com, "u");
    com.children_parameters[mdl_3].set_integrator(0, 0.001);

    auto mdl_4 = alloc<irt::qss3_cross>(mod, com);
    com.children_parameters[mdl_4].set_cross(1);

    auto mdl_5 = alloc<irt::qss3_flipflop>(mod, com);

    irt_check(connect(com, mdl_0, 0, mdl_2, 1));
    irt_check(connect(com, mdl_1, 0, mdl_5, 0));
    irt_check(connect(com, mdl_2, 0, mdl_3, 0));
    irt_check(connect(com, mdl_3, 0, mdl_2, 0));
    irt_check(connect(com, mdl_3, 0, mdl_4, 0));
    irt_check(connect(com, mdl_4, 0, mdl_5, 1));
    irt_check(connect(com, mdl_5, 0, mdl_3, 1));

    irt_check(add_integrator_component_port(dst, com, mdl_3, "u"));

    return success();
}

template<int QssLevel>
status add_izhikevich(modeling&          mod,
                      component&         dst,
                      generic_component& com) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(com.children.can_alloc(19) && com.connections.can_alloc(22)))
        return new_error(modeling_errc::generic_children_container_full);

    auto mdl_0 = alloc<irt::abstract_integrator<QssLevel>>(mod, com, "u");
    auto mdl_1 = alloc<irt::abstract_integrator<QssLevel>>(mod, com, "v");

    com.children_parameters[mdl_0].set_integrator(0, 0.01);
    com.children_parameters[mdl_1].set_integrator(0, 0.01);

    auto mdl_2       = alloc<irt::abstract_square<QssLevel>>(mod, com);
    auto mdl_cst_004 = alloc<irt::constant>(mod, com, "0.04");
    com.children_parameters[mdl_cst_004].set_constant(0.04, 0);

    auto mdl_3 = alloc<irt::abstract_multiplier<QssLevel>>(mod, com);

    auto mdl_cst_5 = alloc<irt::constant>(mod, com, "5");
    com.children_parameters[mdl_cst_5].set_constant(5, 0);
    auto mdl_4 = alloc<irt::abstract_multiplier<QssLevel>>(mod, com);

    auto mdl_cst_140 = alloc<irt::constant>(mod, com, "140");
    com.children_parameters[mdl_cst_140].set_constant(140, 0);
    auto mdl_5 = alloc<irt::abstract_wsum<QssLevel, 2>>(mod, com);
    com.children_parameters[mdl_5].set_wsum2(1, -1);

    auto mdl_6 = alloc<irt::constant>(mod, com, "-99");
    com.children_parameters[mdl_6].set_constant(-99, 0);

    auto mdl_7 = alloc<irt::abstract_sum<QssLevel, 4>>(mod, com);

    auto mdl_8 = alloc<irt::constant>(mod, com, "0.2");
    com.children_parameters[mdl_8].set_constant(0.2, 0);

    auto mdl_9 = alloc<irt::constant>(mod, com, "2");
    com.children_parameters[mdl_9].set_constant(2, 0);

    auto mdl_10 = alloc<irt::abstract_multiplier<QssLevel>>(mod, com);

    auto mdl_11 = alloc<irt::abstract_wsum<QssLevel, 2>>(mod, com);
    com.children_parameters[mdl_11].set_wsum2(1, -1);

    auto mdl_12 = alloc<irt::abstract_multiplier<QssLevel>>(mod, com);

    auto mdl_13 = alloc<irt::abstract_cross<QssLevel>>(mod, com);
    com.children_parameters[mdl_13].set_cross(30);

    auto mdl_14 = alloc<irt::abstract_flipflop<QssLevel>>(mod, com);

    auto mdl_15 = alloc<irt::constant>(mod, com);
    com.children_parameters[mdl_15].set_constant(-65, 0);

    auto mdl_16 = alloc<irt::abstract_sum<QssLevel, 2>>(mod, com);

    auto mdl_17 = alloc<irt::constant>(mod, com);
    com.children_parameters[mdl_17].set_constant(-16, 0);

    auto mdl_18 = alloc<irt::abstract_flipflop<QssLevel>>(mod, com);

    irt_check(connect(com, mdl_cst_004, 0, mdl_3, 0));
    irt_check(connect(com, mdl_cst_5, 0, mdl_4, 0));
    irt_check(connect(com, mdl_cst_140, 0, mdl_5, 0));

    irt_check(connect(com, mdl_0, 0, mdl_2, 0));
    irt_check(connect(com, mdl_0, 0, mdl_4, 1));
    irt_check(connect(com, mdl_0, 0, mdl_10, 1));
    irt_check(connect(com, mdl_0, 0, mdl_13, 0));
    irt_check(connect(com, mdl_1, 0, mdl_5, 1));
    irt_check(connect(com, mdl_1, 0, mdl_11, 1));
    irt_check(connect(com, mdl_1, 0, mdl_16, 0));
    irt_check(connect(com, mdl_2, 0, mdl_3, 1));
    irt_check(connect(com, mdl_3, 0, mdl_7, 0));
    irt_check(connect(com, mdl_4, 0, mdl_7, 1));
    irt_check(connect(com, mdl_5, 0, mdl_7, 2));
    irt_check(connect(com, mdl_6, 0, mdl_7, 3));
    irt_check(connect(com, mdl_7, 0, mdl_0, 0));
    irt_check(connect(com, mdl_8, 0, mdl_12, 0));
    irt_check(connect(com, mdl_9, 0, mdl_10, 0));
    irt_check(connect(com, mdl_10, 0, mdl_11, 0));
    irt_check(connect(com, mdl_11, 0, mdl_12, 1));
    irt_check(connect(com, mdl_12, 0, mdl_1, 0));
    irt_check(connect(com, mdl_13, 0, mdl_14, 1));
    irt_check(connect(com, mdl_13, 0, mdl_18, 1));
    irt_check(connect(com, mdl_14, 0, mdl_0, 1));
    irt_check(connect(com, mdl_15, 0, mdl_14, 0));
    irt_check(connect(com, mdl_16, 0, mdl_18, 0));
    irt_check(connect(com, mdl_17, 0, mdl_16, 1));
    irt_check(connect(com, mdl_18, 0, mdl_1, 1));

    irt_check(add_integrator_component_port(dst, com, mdl_0, "u"));
    irt_check(add_integrator_component_port(dst, com, mdl_1, "v"));

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
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com, "X");
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com, "Y");

    com.children_parameters[integrator_a].set_integrator(0.0_r, 0.001_r);
    com.children_parameters[integrator_b].set_integrator(10.0_r, 0.001_r);

    constexpr double mu = 4.0_r;
    com.children_parameters[sum].set_wsum3(mu, -mu, -1.0_r);

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

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (not com.children.can_alloc(6) and not com.children.grow<2, 1>())
        return new_error(modeling_errc::generic_children_container_full);

    if (not com.connections.can_alloc(7) and not com.connections.grow<2, 1>())
        return new_error(modeling_errc::generic_children_container_full);

    auto mdl_0 = alloc<irt::constant>(mod, com, "1");
    com.children_parameters[mdl_0].set_constant(1, 0);

    auto mdl_1 = alloc<irt::constant>(mod, com, "0");
    com.children_parameters[mdl_1].set_constant(0, 0);

    auto mdl_2 = alloc<irt::qss3_wsum_2>(mod, com);
    com.children_parameters[mdl_2].set_wsum2(-0.1, -1);

    auto mdl_3 = alloc<irt::qss3_integrator>(mod, com, "u");
    com.children_parameters[mdl_3].set_integrator(0, 0.001);

    auto mdl_4 = alloc<irt::qss3_cross>(mod, com);
    com.children_parameters[mdl_4].set_cross(-1);

    auto mdl_5 = alloc<irt::qss3_flipflop>(mod, com);

    irt_check(connect(com, mdl_0, 0, mdl_2, 1));
    irt_check(connect(com, mdl_1, 0, mdl_5, 0));
    irt_check(connect(com, mdl_2, 0, mdl_3, 0));
    irt_check(connect(com, mdl_3, 0, mdl_2, 0));
    irt_check(connect(com, mdl_3, 0, mdl_4, 0));
    irt_check(connect(com, mdl_4, 0, mdl_5, 1));
    irt_check(connect(com, mdl_5, 0, mdl_3, 1));

    irt_check(add_integrator_component_port(dst, com, mdl_3, "u"));

    return success();
}

template<int QssLevel>
status add_seirs(modeling& mod, component& dst, generic_component& com) noexcept
{
    using namespace irt::literals;
    if (!com.children.can_alloc(17))
        return new_error(modeling_errc::generic_children_container_full);

    auto dS = alloc<abstract_integrator<QssLevel>>(mod, com, "dS");
    com.children_parameters[dS].set_integrator(0.999_r, 0.0001_r);

    auto dE = alloc<abstract_integrator<QssLevel>>(mod, com, "dE");
    com.children_parameters[dE].set_integrator(0.0_r, 0.0001_r);

    auto dI = alloc<abstract_integrator<QssLevel>>(mod, com, "dI");
    com.children_parameters[dI].set_integrator(0.001_r, 0.0001_r);

    auto dR = alloc<abstract_integrator<QssLevel>>(mod, com, "dR");
    com.children_parameters[dR].set_integrator(0.0_r, 0.0001_r);

    auto beta = alloc<constant>(mod, com, "beta");
    com.children_parameters[beta].set_constant(0.5_r, 0.0_r);
    auto rho = alloc<constant>(mod, com, "rho");
    com.children_parameters[rho].set_constant(0.00274397_r, 0.0_r);
    auto sigma = alloc<constant>(mod, com, "sigma");
    com.children_parameters[sigma].set_constant(0.33333_r, 0.0_r);
    auto gamma = alloc<constant>(mod, com, "gamma");
    com.children_parameters[gamma].set_constant(0.142857_r, 0.0_r);

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(mod, com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "rho R - beta S I");
    com.children_parameters[rho_R_beta_S_I].set_wsum2(1.0_r, -1.0_r);
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "beta S I - sigma E");
    com.children_parameters[beta_S_I_sigma_E].set_wsum2(1.0_r, -1.0_r);

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(mod, com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "sigma E - gamma I");
    com.children_parameters[sigma_E_gamma_I].set_wsum2(1.0_r, -1.0_r);
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "gamma I - rho R");
    com.children_parameters[gamma_I_rho_R].set_wsum2(-1.0_r, 1.0_r);

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
