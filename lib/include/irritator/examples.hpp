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

    bool success = sim.can_alloc<abstract_wsum<QssLevel, 2>>(2) &&
            sim.can_alloc<abstract_multiplier<QssLevel>>(1) &&
            sim.can_alloc<abstract_integrator<QssLevel>>(2);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>("int_a");
    integrator_a.default_X = 18.0;
    integrator_a.default_dQ = 0.1;

    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>("int_b");
    integrator_b.default_X = 7.0;
    integrator_b.default_dQ = 0.1;

    auto& product = sim.alloc<abstract_multiplier<QssLevel>>("prod");

    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 2>>("sum_a");
    sum_a.default_input_coeffs[0] = 2.0;
    sum_a.default_input_coeffs[1] = -0.4;

    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>("sum_b");
    sum_b.default_input_coeffs[0] = -1.0;
    sum_b.default_input_coeffs[1] = 0.1;

    sim.connect(sum_a.y[0], integrator_a.x[0]);
    sim.connect(sum_b.y[0], integrator_b.x[0]);
    sim.connect(integrator_a.y[0], sum_a.x[0]);
    sim.connect(integrator_b.y[0], sum_b.x[0]);
    sim.connect(integrator_a.y[0], product.x[0]);
    sim.connect(integrator_b.y[0], product.x[1]);
    sim.connect(product.y[0], sum_a.x[1]);
    sim.connect(product.y[0], sum_b.x[1]);

    f(sum_a.id);
    f(sum_b.id);
    f(product.id);
    f(integrator_a.id);
    f(integrator_b.id);

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_lif(simulation& sim, F f) noexcept
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = sim.can_alloc<abstract_wsum<QssLevel, 2>>(1) &&
        sim.can_alloc<abstract_integrator<QssLevel>>(1) &&
        sim.can_alloc<abstract_cross<QssLevel>>(1) &&
        sim.can_alloc<constant>(2);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr double tau = 10.0;
    constexpr double Vt = 1.0;
    constexpr double V0 = 10.0;
    constexpr double Vr = -V0;

    auto& cst = sim.alloc<constant>("cte");
    cst.default_value = 1.0;

    auto& cst_cross = sim.alloc<constant>("ctecro");
    cst_cross.default_value = Vr;
    
    auto& sum = sim.alloc<abstract_wsum<QssLevel, 2>>("sum");
    sum.default_input_coeffs[0] = -1.0 / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>("int");
    integrator.default_X = 0.0;
    integrator.default_dQ = 0.001;

    auto& cross = sim.alloc<abstract_cross<QssLevel>>("cro");
    cross.default_threshold = Vt;

    sim.connect(cross.y[0], integrator.x[1]);
    sim.connect(cross.y[1], sum.x[0]);
    sim.connect(integrator.y[0], cross.x[0]);
    sim.connect(integrator.y[0], cross.x[2]);
    sim.connect(cst_cross.y[0], cross.x[1]);
    sim.connect(cst.y[0], sum.x[1]);
    sim.connect(sum.y[0], integrator.x[0]);

    f(sum.id);
    f(cst.id);
    f(cst_cross.id);
    f(integrator.id);
    f(cross.id);

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_izhikevich(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc<constant>(3) &&
        sim.can_alloc<abstract_wsum<QssLevel, 2>>(3) &&
        sim.can_alloc<abstract_wsum<QssLevel, 4>>(1) &&
        sim.can_alloc<abstract_multiplier<QssLevel>>(1) &&
        sim.can_alloc<abstract_integrator<QssLevel>>(2) &&
        sim.can_alloc<abstract_cross<QssLevel>>(2) &&
        sim.models.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& cst = sim.alloc<constant>("1");
    auto& cst2 = sim.alloc<constant>("-56");
    auto& cst3 = sim.alloc<constant>("tfun");
    auto& sum_a = sim.alloc<abstract_wsum<QssLevel, 2>>("sum-a");
    auto& sum_b = sim.alloc<abstract_wsum<QssLevel, 2>>("sum-b");
    auto& sum_c = sim.alloc<abstract_wsum<QssLevel, 4>>("sum-c");
    auto& sum_d = sim.alloc<abstract_wsum<QssLevel, 2>>("sum-d");
    auto& product = sim.alloc<abstract_multiplier<QssLevel>>("prod");
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>("int-a");
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>("int-b");
    auto& cross = sim.alloc<abstract_cross<QssLevel>>("cross");
    auto& cross2 = sim.alloc<abstract_cross<QssLevel>>("cross2");

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

    sim.connect(integrator_a.y[0], cross.x[0]);
    sim.connect(cst2.y[0], cross.x[1]);
    sim.connect(integrator_a.y[0], cross.x[2]);

    sim.connect(cross.y[1], product.x[0]);
    sim.connect(cross.y[1], product.x[1]);
    sim.connect(product.y[0], sum_c.x[0]);
    sim.connect(cross.y[1], sum_c.x[1]);
    sim.connect(cross.y[1], sum_b.x[1]);

    sim.connect(cst.y[0], sum_c.x[2]);
    sim.connect(cst3.y[0], sum_c.x[3]);

    sim.connect(sum_c.y[0], sum_a.x[0]);
    sim.connect(cross2.y[1], sum_a.x[1]);
    sim.connect(sum_a.y[0], integrator_a.x[0]);
    sim.connect(cross.y[0], integrator_a.x[1]);

    sim.connect(cross2.y[1], sum_b.x[0]);
    sim.connect(sum_b.y[0], integrator_b.x[0]);

    sim.connect(cross2.y[0], integrator_b.x[1]);
    sim.connect(integrator_a.y[0], cross2.x[0]);
    sim.connect(integrator_b.y[0], cross2.x[2]);
    sim.connect(sum_d.y[0], cross2.x[1]);
    sim.connect(integrator_b.y[0], sum_d.x[0]);
    sim.connect(cst.y[0], sum_d.x[1]);

    f(cst.id);
    f(cst2.id);
    f(cst3.id);
    f(sum_a.id);
    f(sum_b.id);
    f(sum_c.id);
    f(sum_d.id);
    f(product.id);
    f(integrator_a.id);
    f(integrator_b.id);
    f(cross.id);
    f(cross2.id);

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_van_der_pol(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc<abstract_wsum<QssLevel, 3>>(1) &&
        sim.can_alloc<abstract_multiplier<QssLevel>>(2) &&
        sim.can_alloc<abstract_integrator<QssLevel>>(2) &&
        sim.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum = sim.alloc<abstract_wsum<QssLevel, 3>>("sum");
    auto& product1 = sim.alloc<abstract_multiplier<QssLevel>>("prod1");
    auto& product2 = sim.alloc<abstract_multiplier<QssLevel>>("prod2");
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>("int-a");
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>("int-b");

    integrator_a.default_X = 0.0;
    integrator_a.default_dQ = 0.001;        

    integrator_b.default_X = 10.0;
    integrator_b.default_dQ = 0.001;             

    constexpr double mu = 4.0;
    sum.default_input_coeffs[0] = mu;
    sum.default_input_coeffs[1] = -mu;
    sum.default_input_coeffs[2] = -1.0;

    sim.connect(integrator_b.y[0], integrator_a.x[0]);
    sim.connect(sum.y[0], integrator_b.x[0]);
    sim.connect(integrator_b.y[0], sum.x[0]);
    sim.connect(product2.y[0], sum.x[1]);
    sim.connect(integrator_a.y[0], sum.x[2]);
    sim.connect(integrator_b.y[0], product1.x[0]);
    sim.connect(integrator_a.y[0], product1.x[1]);
    sim.connect(product1.y[0], product2.x[0]);
    sim.connect(integrator_a.y[0], product2.x[1]);

    f(sum.id);
    f(product1.id);
    f(product2.id);
    f(integrator_a.id);
    f(integrator_b.id);

    return status::success;
}

template<int QssLevel, typename F>
status
example_qss_negative_lif(simulation& sim, F f) noexcept
{
    bool success = sim.can_alloc<abstract_wsum<QssLevel, 2>>(1) &&
        sim.can_alloc<abstract_integrator<QssLevel>>(1) &&
        sim.can_alloc<abstract_cross<QssLevel>>(1) &&
        sim.can_alloc<constant>(2);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum = sim.alloc<abstract_wsum<QssLevel, 2>>("sum");
    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>("int");
    auto& cross = sim.alloc<abstract_cross<QssLevel>>("cro");
    auto& cst = sim.alloc<constant>("cte");
    auto& cst_cross = sim.alloc<constant>("ctecro");

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

    sim.connect(cross.y[0], integrator.x[1]);
    sim.connect(cross.y[1], sum.x[0]);
    sim.connect(integrator.y[0], cross.x[0]);
    sim.connect(integrator.y[0], cross.x[2]);
    sim.connect(cst_cross.y[0], cross.x[1]);
    sim.connect(cst.y[0], sum.x[1]);
    sim.connect(sum.y[0], integrator.x[0]);

    f(sum.id);
    f(integrator.id);
    f(cross.id);
    f(cst.id);
    f(cst_cross.id);

    return status::success;
}

} // namespace irritator

#endif
