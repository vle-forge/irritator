// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <optional>
#include <utility>

namespace irt {

template<typename Dynamics>
std::pair<Dynamics*, child_id> alloc(
  modeling&              mod,
  generic_component&     parent,
  const std::string_view name  = {},
  child_flags            param = child_flags_none) noexcept
{
    irt_assert(!mod.models.full());
    irt_assert(!mod.children.full());
    irt_assert(!mod.hsms.full());

    auto& child    = mod.alloc(parent, dynamics_typeof<Dynamics>());
    auto  child_id = mod.children.get_id(child);
    child.flags    = param;

    mod.children_names[get_index(child_id)] = name;

    return std::make_pair(&get_dyn<Dynamics>(mod.models.get(child.id.mdl_id)),
                          child_id);
}

template<typename DynamicsSrc, typename DynamicsDst>
status connect(modeling&          mod,
               generic_component& c,
               DynamicsSrc&       src,
               i8                 port_src,
               DynamicsDst&       dst,
               i8                 port_dst) noexcept
{
    model& src_model = get_model(*src.first);
    model& dst_model = get_model(*dst.first);

    irt_return_if_fail(
      is_ports_compatible(src_model, port_src, dst_model, port_dst),
      status::model_connect_bad_dynamics);

    mod.connect(c, src.second, port_src, dst.second, port_dst);

    return status::success;
}

status add_integrator_component_port(modeling&          mod,
                                     generic_component& com,
                                     child_id           id,
                                     i8                 port) noexcept
{
    irt_return_if_bad(mod.connect_input(com, port, id, 1));
    irt_return_if_bad(mod.connect_output(com, id, 0, port));

    return status::success;
}

template<int QssLevel>
status add_lotka_volterra(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "X", child_flags_both);
    integrator_a.first->default_X  = 18.0_r;
    integrator_a.first->default_dQ = 0.1_r;

    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "Y", child_flags_both);
    integrator_b.first->default_X  = 7.0_r;
    integrator_b.first->default_dQ = 0.1_r;

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "X+XY", child_flags_configurable);
    sum_a.first->default_input_coeffs[0] = 2.0_r;
    sum_a.first->default_input_coeffs[1] = -0.4_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "Y+XY", child_flags_configurable);
    sum_b.first->default_input_coeffs[0] = -1.0_r;
    sum_b.first->default_input_coeffs[1] = 0.1_r;

    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);
    connect(mod, com, integrator_a, 0, sum_a, 0);
    connect(mod, com, integrator_b, 0, sum_b, 0);
    connect(mod, com, integrator_a, 0, product, 0);
    connect(mod, com, integrator_b, 0, product, 1);
    connect(mod, com, product, 0, sum_a, 1);
    connect(mod, com, product, 0, sum_b, 1);

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst                 = alloc<constant>(mod, com);
    cst.first->default_value = 1.0;

    auto cst_cross                 = alloc<constant>(mod, com);
    cst_cross.first->default_value = Vr;

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    sum.first->default_input_coeffs[0] = -irt::one / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    auto integrator =
      alloc<abstract_integrator<QssLevel>>(mod, com, "lif", child_flags_both);
    integrator.first->default_X  = 0._r;
    integrator.first->default_dQ = 0.001_r;

    auto cross                     = alloc<abstract_cross<QssLevel>>(mod, com);
    cross.first->default_threshold = Vt;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(mod, com, integrator.second, 0);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(12);

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

    cst.first->default_value  = 1.0_r;
    cst2.first->default_value = c;
    cst3.first->default_value = I;

    cross.first->default_threshold  = vt;
    cross2.first->default_threshold = vt;

    integrator_a.first->default_X  = 0.0_r;
    integrator_a.first->default_dQ = 0.01_r;

    integrator_b.first->default_X  = 0.0_r;
    integrator_b.first->default_dQ = 0.01_r;

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

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling& mod, generic_component& com) noexcept
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

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling& mod, generic_component& com) noexcept
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

    add_integrator_component_port(mod, com, integrator.second, 0);

    return status::success;
}

template<int QssLevel>
status add_seirs(modeling& mod, generic_component& com) noexcept
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

    add_integrator_component_port(mod, com, dS.second, 0);
    add_integrator_component_port(mod, com, dE.second, 1);
    add_integrator_component_port(mod, com, dI.second, 2);
    add_integrator_component_port(mod, com, dR.second, 3);

    return status::success;
}

static bool is_ports_compatible(modeling&          mod,
                                model&             mdl_src,
                                i8                 port_src,
                                generic_component& compo_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_dst.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_dst) {
            auto* sub_child_dst = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_dst);
            irt_assert(sub_child_dst->type == child_type::model);

            auto  sub_model_dst_id = sub_child_dst->id.mdl_id;
            auto* sub_model_dst    = mod.models.try_to_get(sub_model_dst_id);

            if (!is_ports_compatible(
                  mdl_src, port_src, *sub_model_dst, con->output.index_src)) {
                is_compatible = false;
                break;
            }
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                i8                 port_src,
                                model&             mdl_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::input &&
            con->input.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->input.dst);
            irt_assert(sub_child_src);
            if (sub_child_src->type == child_type::model) {

                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(*sub_model_src,
                                         con->input.index_dst,
                                         mdl_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                i8                 port_src,
                                generic_component& compo_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_src);

            if (sub_child_src->type == child_type::model) {
                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(mod,
                                         *sub_model_src,
                                         con->output.index_src,
                                         compo_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling& mod,
                                generic_component& /*parent*/,
                                child_id src,
                                i8       port_src,
                                child_id dst,
                                i8       port_dst) noexcept
{
    auto* child_src = mod.children.try_to_get(src);
    auto* child_dst = mod.children.try_to_get(dst);
    irt_assert(child_src);
    irt_assert(child_dst);

    if (child_src->type == child_type::model) {
        auto  mdl_src_id = child_src->id.mdl_id;
        auto* mdl_src    = mod.models.try_to_get(mdl_src_id);

        if (child_dst->type == child_type::model) {
            auto  mdl_dst_id = child_dst->id.mdl_id;
            auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
            return is_ports_compatible(*mdl_src, port_src, *mdl_dst, port_dst);

        } else {
            auto  compo_dst_id = child_dst->id.compo_id;
            auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
            irt_assert(compo_dst);
            auto* s_compo_dst =
              mod.simple_components.try_to_get(compo_dst->id.simple_id);
            irt_assert(s_compo_dst);

            return is_ports_compatible(
              mod, *mdl_src, port_src, *s_compo_dst, port_dst);
        }
    } else {
        auto  compo_src_id = child_src->id.compo_id;
        auto* compo_src    = mod.components.try_to_get(compo_src_id);
        irt_assert(compo_src);
        auto* s_compo_src =
          mod.simple_components.try_to_get(compo_src->id.simple_id);
        irt_assert(s_compo_src);

        if (child_dst->type == child_type::model) {
            auto  mdl_dst_id = child_dst->id.mdl_id;
            auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
            irt_assert(mdl_dst);

            return is_ports_compatible(
              mod, *s_compo_src, port_src, *mdl_dst, port_dst);
        } else {
            auto  compo_dst_id = child_dst->id.compo_id;
            auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
            irt_assert(compo_dst);
            auto* s_compo_dst =
              mod.simple_components.try_to_get(compo_dst->id.simple_id);
            irt_assert(s_compo_dst);

            return is_ports_compatible(
              mod, *s_compo_src, port_src, *s_compo_dst, port_dst);
        }
    }
}

status modeling::connect_input(generic_component& parent,
                               i8                 port_src,
                               child_id           dst,
                               i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto* child = children.try_to_get(dst);
    irt_assert(child);

    if (child->type == child_type::model) {
        auto  mdl_id  = child->id.mdl_id;
        auto* mdl_dst = models.try_to_get(mdl_id);
        irt_assert(mdl_dst);

        irt_return_if_fail(
          is_ports_compatible(*this, parent, port_src, *mdl_dst, port_dst),
          status::model_connect_bad_dynamics);
    }

    auto& con           = connections.alloc();
    auto  con_id        = connections.get_id(con);
    con.input.dst       = dst;
    con.input.index     = port_src;
    con.input.index_dst = port_dst;
    con.type            = connection::connection_type::input;

    parent.connections.emplace_back(con_id);

    return status::success;
}

status modeling::connect_output(generic_component& parent,
                                child_id           src,
                                i8                 port_src,
                                i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto* child = children.try_to_get(src);
    irt_assert(child);

    if (child->type == child_type::model) {
        auto  mdl_id  = child->id.mdl_id;
        auto* mdl_src = models.try_to_get(mdl_id);
        irt_assert(mdl_src);

        irt_return_if_fail(
          is_ports_compatible(*this, *mdl_src, port_src, parent, port_dst),
          status::model_connect_bad_dynamics);
    }

    auto& con            = connections.alloc();
    auto  con_id         = connections.get_id(con);
    con.output.src       = src;
    con.output.index_src = port_src;
    con.output.index     = port_dst;
    con.type             = connection::connection_type::output;

    parent.connections.emplace_back(con_id);

    return status::success;
}

status modeling::connect(generic_component& parent,
                         child_id           src,
                         i8                 port_src,
                         child_id           dst,
                         i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(1),
                       status::data_array_not_enough_memory);

    irt_return_if_fail(
      is_ports_compatible(*this, parent, src, port_src, dst, port_dst),
      status::model_connect_bad_dynamics);

    auto& con              = connections.alloc();
    auto  con_id           = connections.get_id(con);
    con.internal.src       = src;
    con.internal.dst       = dst;
    con.internal.index_src = port_src;
    con.internal.index_dst = port_dst;
    con.type               = connection::connection_type::internal;

    parent.connections.emplace_back(con_id);

    return status::success;
}

status modeling::copy(const generic_component& src,
                      generic_component&       dst) noexcept
{
    table<child_id, child_id> mapping;

    for (auto child_id : src.children) {
        auto* c = children.try_to_get(child_id);
        if (!c)
            continue;

        if (c->type == child_type::model) {
            auto id      = c->id.mdl_id;
            auto src_idx = get_index(id);

            if (auto* mdl = models.try_to_get(id); mdl) {
                auto& new_child                   = alloc(dst, mdl->type);
                auto  new_child_id                = children.get_id(new_child);
                auto  new_child_idx               = get_index(new_child_id);
                children_names[new_child_idx]     = children_names[src_idx];
                children_positions[new_child_idx] = {
                    .x = children_positions[src_idx].x,
                    .y = children_positions[src_idx].y
                };

                mapping.data.emplace_back(child_id, new_child_id);
            }
        } else {
            auto compo_id = c->id.compo_id;
            auto src_idx  = get_index(compo_id);

            if (auto* compo = components.try_to_get(compo_id); compo) {
                auto& new_child                   = alloc(dst, compo_id);
                auto  new_child_id                = children.get_id(new_child);
                auto  new_child_idx               = get_index(new_child_id);
                children_names[new_child_idx]     = children_names[src_idx];
                children_positions[new_child_idx] = {
                    .x = children_positions[src_idx].x,
                    .y = children_positions[src_idx].y
                };

                mapping.data.emplace_back(child_id, new_child_id);
            }
        }
    }

    mapping.sort();

    for (auto connection_id : src.connections) {
        auto* con = connections.try_to_get(connection_id);

        if (con->type == connection::connection_type::internal) {
            if (auto* child_src = mapping.get(con->internal.src); child_src) {
                if (auto* child_dst = mapping.get(con->internal.dst);
                    child_dst) {
                    irt_return_if_bad(connect(dst,
                                              *child_src,
                                              con->internal.index_src,
                                              *child_dst,
                                              con->internal.index_dst));
                }
            }
        }
    }

    return status::success;
}

status modeling::copy(internal_component src, component& dst) noexcept
{
    irt_return_if_fail(simple_components.can_alloc(),
                       status::data_array_not_enough_memory);

    auto& s_compo    = simple_components.alloc();
    auto  s_compo_id = simple_components.get_id(s_compo);
    dst.type         = component_type::simple;
    dst.id.simple_id = s_compo_id;

    switch (src) {
    case internal_component::qss1_izhikevich:
        return add_izhikevich<1>(*this, s_compo);
    case internal_component::qss1_lif:
        return add_lif<1>(*this, s_compo);
    case internal_component::qss1_lotka_volterra:
        return add_lotka_volterra<1>(*this, s_compo);
    case internal_component::qss1_negative_lif:
        return add_negative_lif<1>(*this, s_compo);
    case internal_component::qss1_seirs:
        return add_seirs<1>(*this, s_compo);
    case internal_component::qss1_van_der_pol:
        return add_van_der_pol<1>(*this, s_compo);
    case internal_component::qss2_izhikevich:
        return add_izhikevich<2>(*this, s_compo);
    case internal_component::qss2_lif:
        return add_lif<2>(*this, s_compo);
    case internal_component::qss2_lotka_volterra:
        return add_lotka_volterra<2>(*this, s_compo);
    case internal_component::qss2_negative_lif:
        return add_negative_lif<2>(*this, s_compo);
    case internal_component::qss2_seirs:
        return add_seirs<2>(*this, s_compo);
    case internal_component::qss2_van_der_pol:
        return add_van_der_pol<2>(*this, s_compo);
    case internal_component::qss3_izhikevich:
        return add_izhikevich<3>(*this, s_compo);
    case internal_component::qss3_lif:
        return add_lif<3>(*this, s_compo);
    case internal_component::qss3_lotka_volterra:
        return add_lotka_volterra<3>(*this, s_compo);
    case internal_component::qss3_negative_lif:
        return add_negative_lif<3>(*this, s_compo);
    case internal_component::qss3_seirs:
        return add_seirs<3>(*this, s_compo);
    case internal_component::qss3_van_der_pol:
        return add_van_der_pol<3>(*this, s_compo);
    }

    irt_unreachable();
}

} // namespace irt