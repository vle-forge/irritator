// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020
#define ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020

#include <irritator/core.hpp>
#include <irritator/error.hpp>

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
status2 example_qss_lotka_volterra(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(8)))
        return new_error(status::simulation_not_enough_model);

    auto& integrator_a      = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_a.default_X  = 18.0_r;
    integrator_a.default_dQ = 0.1_r;

    auto& integrator_b      = sim.alloc<abstract_integrator<QssLevel>>();
    integrator_b.default_X  = 7.0_r;
    integrator_b.default_dQ = 0.1_r;

    auto& product = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& sum_a                   = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_a.default_input_coeffs[0] = 2.0_r;
    sum_a.default_input_coeffs[1] = -0.4_r;

    auto& sum_b                   = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum_b.default_input_coeffs[0] = -1.0_r;
    sum_b.default_input_coeffs[1] = 0.1_r;

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

    return success();
}

template<int QssLevel, typename F>
status2 example_qss_lif(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(7)))
        return new_error(status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto& cst         = sim.alloc<constant>();
    cst.default_value = 1.0;

    auto& cst_cross         = sim.alloc<constant>();
    cst_cross.default_value = Vr;

    auto& sum                   = sim.alloc<abstract_wsum<QssLevel, 2>>();
    sum.default_input_coeffs[0] = -irt::one / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    auto& integrator      = sim.alloc<abstract_integrator<QssLevel>>();
    integrator.default_X  = 0._r;
    integrator.default_dQ = 0.001_r;

    auto& cross             = sim.alloc<abstract_cross<QssLevel>>();
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

    return success();
}

template<int QssLevel, typename F>
status2 example_qss_izhikevich(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(12) && sim.can_connect(22)))
        return new_error(status::simulation_not_enough_model);

    auto& cst          = sim.alloc<constant>();
    auto& cst2         = sim.alloc<constant>();
    auto& cst3         = sim.alloc<constant>();
    auto& sum_a        = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& sum_b        = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& sum_c        = sim.alloc<abstract_wsum<QssLevel, 4>>();
    auto& sum_d        = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& product      = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    auto& cross        = sim.alloc<abstract_cross<QssLevel>>();
    auto& cross2       = sim.alloc<abstract_cross<QssLevel>>();

    constexpr irt::real a  = 0.2_r;
    constexpr irt::real b  = 2.0_r;
    constexpr irt::real c  = -56.0_r;
    constexpr irt::real d  = -16.0_r;
    constexpr irt::real I  = -99.0_r;
    constexpr irt::real vt = 30.0_r;

    cst.default_value  = 1.0_r;
    cst2.default_value = c;
    cst3.default_value = I;

    cross.default_threshold  = vt;
    cross2.default_threshold = vt;

    integrator_a.default_X  = 0.0_r;
    integrator_a.default_dQ = 0.01_r;

    integrator_b.default_X  = 0.0_r;
    integrator_b.default_dQ = 0.01_r;

    sum_a.default_input_coeffs[0] = 1.0_r;
    sum_a.default_input_coeffs[1] = -1.0_r;
    sum_b.default_input_coeffs[0] = -a;
    sum_b.default_input_coeffs[1] = a * b;
    sum_c.default_input_coeffs[0] = 0.04_r;
    sum_c.default_input_coeffs[1] = 5.0_r;
    sum_c.default_input_coeffs[2] = 140.0_r;
    sum_c.default_input_coeffs[3] = 1.0_r;
    sum_d.default_input_coeffs[0] = 1.0_r;
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

    return success();
}

template<int QssLevel, typename F>
status2 example_qss_van_der_pol(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(9)))
        return new_error(status::simulation_not_enough_model);

    auto& sum          = sim.alloc<abstract_wsum<QssLevel, 3>>();
    auto& product1     = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& product2     = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();

    integrator_a.default_X  = 0.0_r;
    integrator_a.default_dQ = 0.001_r;

    integrator_b.default_X  = 10.0_r;
    integrator_b.default_dQ = 0.001_r;

    constexpr double mu         = 4.0_r;
    sum.default_input_coeffs[0] = mu;
    sum.default_input_coeffs[1] = -mu;
    sum.default_input_coeffs[2] = -1.0_r;

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

    return success();
}

template<int QssLevel, typename F>
status2 example_qss_negative_lif(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(7)))
        return new_error(status::simulation_not_enough_model);

    auto& sum        = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>();
    auto& cross      = sim.alloc<abstract_cross<QssLevel>>();
    auto& cst        = sim.alloc<constant>();
    auto& cst_cross  = sim.alloc<constant>();

    constexpr real tau = 10.0_r;
    constexpr real Vt  = -1.0_r;
    constexpr real V0  = -10.0_r;
    constexpr real Vr  = 0.0_r;

    sum.default_input_coeffs[0] = -1.0_r / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    cst.default_value       = 1.0_r;
    cst_cross.default_value = Vr;

    integrator.default_X  = 0.0_r;
    integrator.default_dQ = 0.001_r;

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

    return success();
}

} // namespace irritator

#endif
