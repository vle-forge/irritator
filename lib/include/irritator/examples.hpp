// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020
#define ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020

#include <irritator/core.hpp>

namespace irt {

/** @brief Implements Lotka-Volterra model for QSS1, QSS2 and QSS3.
 *
 * @details
 *
 * @Tparam QssLevel
 * @Tparam F
 * @param sim
 * @param f
 * @return
 */
template<int QssLevel, typename F>
status
example_qss_lotka_volterra(simulation& sim, F f) noexcept
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = sim.can_alloc(5) && sim.can_connect(8);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_a.default_X = 18.0;
    integrator_a.default_dQ = 0.1;

    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_b.default_X = 7.0;
    integrator_b.default_dQ = 0.1;

    auto& product = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_a.default_input_coeffs[0] = 2.0;
    sum_a.default_input_coeffs[1] = -0.4;

    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_b.default_input_coeffs[0] = -1.0;
    sum_b.default_input_coeffs[1] = 0.1;

    sim.connect(sum_a, 0, integrator_a, 0);
    sim.connect(sum_b, 0, integrator_b, 0);
    sim.connect(integrator_a, 0, sum_a, 0);
    sim.connect(integrator_b, 0, sum_b, 0);
    sim.connect(integrator_a, 0, product, 0);
    sim.connect(integrator_b, 0, product, 1);
    sim.connect(product, 0, sum_a, 1);
    sim.connect(product, 0, sum_b, 1);

    f(sim.get_id(sum_a));
    f(sim.get_id(sum_b));
    f(sim.get_id(product));
    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_lif(simulation& sim, F f) noexcept
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = sim.can_alloc(5) && sim.can_connect(7);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr double tau = 10.0;
    constexpr double Vt = 1.0;
    constexpr double V0 = 10.0;
    constexpr double Vr = -V0;

    auto& cst = sim.alloc<constant>();
    cst.default_value = 1.0;

    auto& cst_cross = sim.alloc<constant>();
    cst_cross.default_value = Vr;

    auto& sum = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum.default_input_coeffs[0] = -1.0 / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>();
    integrator.default_X = 0.0;
    integrator.default_dQ = 0.001;

    auto& cross = sim.alloc<abstract_cross<QssLevel>>();
    cross.default_threshold = Vt;

    sim.connect(cross, 0, integrator, 1);
    sim.connect(cross, 1, sum, 0);
    sim.connect(integrator, 0, cross, 0);
    sim.connect(integrator, 0, cross, 2);
    sim.connect(cst_cross, 0, cross, 1);
    sim.connect(cst, 0, sum, 1);
    sim.connect(sum, 0, integrator, 0);

    f(sim.get_id(sum));
    f(sim.get_id(cst));
    f(sim.get_id(cst_cross));
    f(sim.get_id(integrator));
    f(sim.get_id(cross));

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_izhikevich(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc(12) && sim.can_connect(22);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& cst = sim.alloc<constant>();
    auto& cst2 = sim.alloc<constant>();
    auto& cst3 = sim.alloc<constant>();
    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& sum_c = sim.alloc<abstract_wsum<QssLevel, 4>>();
    auto& sum_d = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& product = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    auto& cross = sim.alloc<abstract_cross<QssLevel>>();
    auto& cross2 = sim.alloc<abstract_cross<QssLevel>>();

    constexpr double a = 0.2;
    constexpr double b = 2.0;
    constexpr double c = -56.0;
    constexpr double d = -16.0;
    constexpr double I = -99.0;
    constexpr double vt = 30.0;

    cst.default_value = 1.0;
    cst2.default_value = c;
    cst3.default_value = I;

    cross.default_threshold = vt;
    cross2.default_threshold = vt;

    integrator_a.default_X = 0.0;
    integrator_a.default_dQ = 0.01;

    integrator_b.default_X = 0.0;
    integrator_b.default_dQ = 0.01;

    sum_a.default_input_coeffs[0] = 1.0;
    sum_a.default_input_coeffs[1] = -1.0;
    sum_b.default_input_coeffs[0] = -a;
    sum_b.default_input_coeffs[1] = a * b;
    sum_c.default_input_coeffs[0] = 0.04;
    sum_c.default_input_coeffs[1] = 5.0;
    sum_c.default_input_coeffs[2] = 140.0;
    sum_c.default_input_coeffs[3] = 1.0;
    sum_d.default_input_coeffs[0] = 1.0;
    sum_d.default_input_coeffs[1] = d;

    sim.connect(integrator_a, 0, cross, 0);
    sim.connect(cst2, 0, cross, 1);
    sim.connect(integrator_a, 0, cross, 2);

    sim.connect(cross, 1, product, 0);
    sim.connect(cross, 1, product, 1);
    sim.connect(product, 0, sum_c, 0);
    sim.connect(cross, 1, sum_c, 1);
    sim.connect(cross, 1, sum_b, 1);

    sim.connect(cst, 0, sum_c, 2);
    sim.connect(cst3, 0, sum_c, 3);

    sim.connect(sum_c, 0, sum_a, 0);
    sim.connect(cross2, 1, sum_a, 1);
    sim.connect(sum_a, 0, integrator_a, 0);
    sim.connect(cross, 0, integrator_a, 1);

    sim.connect(cross2, 1, sum_b, 0);
    sim.connect(sum_b, 0, integrator_b, 0);

    sim.connect(cross2, 0, integrator_b, 1);
    sim.connect(integrator_a, 0, cross2, 0);
    sim.connect(integrator_b, 0, cross2, 2);
    sim.connect(sum_d, 0, cross2, 1);
    sim.connect(integrator_b, 0, sum_d, 0);
    sim.connect(cst, 0, sum_d, 1);

    f(sim.get_id(cst));
    f(sim.get_id(cst2));
    f(sim.get_id(cst3));
    f(sim.get_id(sum_a));
    f(sim.get_id(sum_b));
    f(sim.get_id(sum_c));
    f(sim.get_id(sum_d));
    f(sim.get_id(product));
    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));
    f(sim.get_id(cross));
    f(sim.get_id(cross2));

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_van_der_pol(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc(5) && sim.can_connect(9);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum = sim.alloc<abstract_wsum<QssLevel, 3>>();
    auto& product1 = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& product2 = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();

    integrator_a.default_X = 0.0;
    integrator_a.default_dQ = 0.001;

    integrator_b.default_X = 10.0;
    integrator_b.default_dQ = 0.001;

    constexpr double mu = 4.0;
    sum.default_input_coeffs[0] = mu;
    sum.default_input_coeffs[1] = -mu;
    sum.default_input_coeffs[2] = -1.0;

    sim.connect(integrator_b, 0, integrator_a, 0);
    sim.connect(sum, 0, integrator_b, 0);
    sim.connect(integrator_b, 0, sum, 0);
    sim.connect(product2, 0, sum, 1);
    sim.connect(integrator_a, 0, sum, 2);
    sim.connect(integrator_b, 0, product1, 0);
    sim.connect(integrator_a, 0, product1, 1);
    sim.connect(product1, 0, product2, 0);
    sim.connect(integrator_a, 0, product2, 1);

    f(sim.get_id(sum));
    f(sim.get_id(product1));
    f(sim.get_id(product2));
    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_negative_lif(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc(5) && sim.can_connect(7);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>();
    auto& cross = sim.alloc<abstract_cross<QssLevel>>();
    auto& cst = sim.alloc<constant>();
    auto& cst_cross = sim.alloc<constant>();

    constexpr double tau = 10.0;
    constexpr double Vt = -1.0;
    constexpr double V0 = -10.0;
    constexpr double Vr = 0.0;

    sum.default_input_coeffs[0] = -1.0 / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    cst.default_value = 1.0;
    cst_cross.default_value = Vr;

    integrator.default_X = 0.0;
    integrator.default_dQ = 0.001;

    cross.default_threshold = Vt;
    cross.default_detect_up = false;

    sim.connect(cross, 0, integrator, 1);
    sim.connect(cross, 1, sum, 0);
    sim.connect(integrator, 0, cross, 0);
    sim.connect(integrator, 0, cross, 2);
    sim.connect(cst_cross, 0, cross, 1);
    sim.connect(cst, 0, sum, 1);
    sim.connect(sum, 0, integrator, 0);

    f(sim.get_id(sum));
    f(sim.get_id(integrator));
    f(sim.get_id(cross));
    f(sim.get_id(cst));
    f(sim.get_id(cst_cross));

    return status::success;
}

/** @brief Implements the Linear SEIR model for QSS1, QSS2 and QSS3.
 *
 * @details
 *
 * @Tparam QssLevel
 * @Tparam F
 * @param sim
 * @param f
 * @return
 */
template<int QssLevel, typename F>
status
example_qss_seir_lineaire(simulation& sim, F f) noexcept
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = sim.can_alloc(10) && sim.can_connect(12)

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_a.default_input_coeffs[0] = -0.005;
    sum_a.default_input_coeffs[1] = -0.4;

    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_b.default_input_coeffs[0] = -0.135;
    sum_b.default_input_coeffs[1] = 0.1;

    auto& product_a = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_b = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_a.default_X = 10.0;
    integrator_a.default_dQ = 0.01;

    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_b.default_X = 15.0;
    integrator_b.default_dQ = 0.01;

    auto& integrator_c = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_c.default_X = 10.0;
    integrator_c.default_dQ = 0.01;

    auto& integrator_d = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_d.default_X = 18.0;
    integrator_d.default_dQ = 0.01;

    auto& constant_a = sim.alloc<constant>();
    constant_a.default_value = -0.005;

    auto& constant_b = sim.alloc<constant>();
    constant_b.default_value = -0.135;

    sim.connect(constant_a, 0, product_a, 0);
    sim.connect(constant_b, 0, product_b, 0);
    sim.connect(sum_a, 0, integrator_c, 0);
    sim.connect(sum_b, 0, integrator_d, 0);
    sim.connect(integrator_b, 0, sum_a, 0);
    sim.connect(integrator_c, 0, sum_a, 1);
    sim.connect(integrator_c, 0, sum_b, 0);
    sim.connect(integrator_d, 0, sum_b, 1);
    sim.connect(integrator_a, 0, product_a, 1);
    sim.connect(integrator_b, 0, product_b, 1);
    sim.connect(product_a, 0, sum_a, 1);
    sim.connect(product_b, 0, sum_b, 1);


    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));
    f(sim.get_id(integrator_c));
    f(sim.get_id(integrator_d));
    f(sim.get_id(product_a));
    f(sim.get_id(product_b));
    f(sim.get_id(sum_a));
    f(sim.get_id(sum_b));
    f(sim.get_id(constant_a));
    f(sim.get_id(constant_b));


    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_seir_nonlineaire(simulation& sim, F f) noexcept
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success=sim.can_alloc(27) && sim.can_connect(32)

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 3>>();
    sum_a.default_input_coeffs[0] = 0.5;
    sum_a.default_input_coeffs[1] = 1.0;
    sum_a.default_input_coeffs[2] = 1.0;

    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_b.default_input_coeffs[0] = 1.0;
    sum_b.default_input_coeffs[1] = 1.0;

    auto& sum_c = sim.alloc<abstract_wsum<QssLevel, 3>>();
    sum_c.default_input_coeffs[0] = 1.5;
    sum_c.default_input_coeffs[1] = 0.698;
    sum_c.default_input_coeffs[2] = 0.387;

    auto& sum_d = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_d.default_input_coeffs[0] = 1.0;
    sum_d.default_input_coeffs[1] = 1.5;

    auto& product_a = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_b = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_c = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_d = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_e = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_f = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_g = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_h = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& product_i = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_a.default_X = 10.0;
    integrator_a.default_dQ = 0.01;

    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_b.default_X = 12.0;
    integrator_b.default_dQ = 0.01;

    auto& integrator_c = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_c.default_X = 13.50;
    integrator_c.default_dQ = 0.01;

    auto& integrator_d = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_d.default_X = 15.0;
    integrator_d.default_dQ = 0.01;

    // The values used here are from Singh et al., 2017

    auto& constant_a = sim.alloc<constant>();
    constant_a.default_value = 0.005;

    auto& constant_b = sim.alloc<constant>();
    constant_b.default_value = -0.0057;

    auto& constant_c = sim.alloc<constant>();
    constant_c.default_value = -0.005;

    auto& constant_d = sim.alloc<constant>();
    constant_d.default_value = 0.0057;

    auto& constant_e = sim.alloc<constant>();
    constant_e.default_value = -0.135;

    auto& constant_f = sim.alloc<constant>();
    constant_f.default_value = 0.135;

    auto& constant_g = sim.alloc<constant>();
    constant_g.default_value = -0.072;

    auto& constant_h = sim.alloc<constant>();
    constant_h.default_value = 0.005;

    auto& constant_i = sim.alloc<constant>();
    constant_i.default_value = 0.067;

    auto& constant_j = sim.alloc<constant>();
    constant_j.default_value = -0.005;

    sim.connect(constant_a, 0, sum_a, 0);
    sim.connect(constant_h, 0, sum_c, 2);
    sim.connect(constant_b, 0, product_a, 0);
    sim.connect(constant_c, 0, product_b, 0);
    sim.connect(constant_d, 0, product_c, 0);
    sim.connect(constant_e, 0, product_d, 0);
    sim.connect(constant_f, 0, product_e, 0);
    sim.connect(constant_g, 0, product_f, 0);
    sim.connect(constant_h, 0, product_g, 0);
    sim.connect(constant_i, 0, product_h, 0);
    sim.connect(product_i, 0, product_a, 1);
    sim.connect(product_i, 0, product_c, 1);
    sim.connect(sum_a, 0, integrator_a, 0);
    sim.connect(sum_b, 0, integrator_b, 0);
    sim.connect(sum_c, 0, integrator_c, 0);
    sim.connect(sum_d, 0, integrator_d, 0);
    sim.connect(product_a, 0, sum_a, 1);
    sim.connect(product_b, 0, sum_a, 2);
    sim.connect(product_c, 0, sum_b, 0);
    sim.connect(product_d, 0, sum_b, 1);
    sim.connect(product_e, 0, sum_c, 0);
    sim.connect(product_f, 0, sum_c, 1);
    sim.connect(product_g, 0, sum_d, 0);
    sim.connect(product_h, 0, sum_d, 1);
    sim.connect(integrator_a, 0, product_b, 1);
    sim.connect(integrator_b, 0, product_d, 1);
    sim.connect(integrator_b, 0, product_e, 1);
    sim.connect(integrator_c, 0, product_f, 1);
    sim.connect(integrator_c, 0, product_g, 1);
    sim.connect(integrator_d, 0, product_h, 1);
    sim.connect(integrator_a, 0, product_i, 0);
    sim.connect(integrator_c, 0, product_i, 1);

    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));
    f(sim.get_id(integrator_c));
    f(sim.get_id(integrator_d));
    f(sim.get_id(product_a));
    f(sim.get_id(product_b));
    f(sim.get_id(product_c));
    f(sim.get_id(product_d));
    f(sim.get_id(product_e));
    f(sim.get_id(product_f));
    f(sim.get_id(product_g));
    f(sim.get_id(product_h));
    f(sim.get_id(product_i));
    f(sim.get_id(sum_a));
    f(sim.get_id(sum_b));
    f(sim.get_id(sum_c));
    f(sim.get_id(sum_d));
    f(sim.get_id(constant_a));
    f(sim.get_id(constant_b));
    f(sim.get_id(constant_c));
    f(sim.get_id(constant_d));
    f(sim.get_id(constant_e));
    f(sim.get_id(constant_f));
    f(sim.get_id(constant_g));
    f(sim.get_id(constant_h));
    f(sim.get_id(constant_i));
    f(sim.get_id(constant_j));

    return status::success;
}

} // namespace irritator

#endif
