// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020
#define ORG_VLEPROJECT_IRRITATOR_EXAMPLES_2020

#include <irritator/core.hpp>
#include <irritator/error.hpp>

namespace irt {

template<typename Dynamics>
inline auto get_p(simulation& sim, const Dynamics& d) noexcept -> parameter&
{
    return sim.parameters[sim.get_id(d)];
}

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
status example_qss_lotka_volterra(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(8)))
        return new_error(simulation_errc::models_container_full);

    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    get_p(sim, integrator_a).reals[0] = 18.0_r;
    get_p(sim, integrator_a).reals[1] = 0.1_r;

    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();
    get_p(sim, integrator_b).reals[0] = 7.0_r;
    get_p(sim, integrator_b).reals[1] = 0.1_r;

    auto& product = sim.alloc<abstract_multiplier<QssLevel>>();

    auto& sum_a                = sim.alloc<abstract_wsum<QssLevel, 2>>();
    get_p(sim, sum_a).reals[2] = 2.0_r;
    get_p(sim, sum_a).reals[3] = -0.4_r;

    auto& sum_b                = sim.alloc<abstract_wsum<QssLevel, 2>>();
    get_p(sim, sum_b).reals[2] = -1.0_r;
    get_p(sim, sum_b).reals[3] = 0.1_r;

    irt_check(sim.connect_dynamics(sum_a, 0, integrator_a, 0));
    irt_check(sim.connect_dynamics(sum_b, 0, integrator_b, 0));
    irt_check(sim.connect_dynamics(integrator_a, 0, sum_a, 0));
    irt_check(sim.connect_dynamics(integrator_b, 0, sum_b, 0));
    irt_check(sim.connect_dynamics(integrator_a, 0, product, 0));
    irt_check(sim.connect_dynamics(integrator_b, 0, product, 1));
    irt_check(sim.connect_dynamics(product, 0, sum_a, 1));
    irt_check(sim.connect_dynamics(product, 0, sum_b, 1));

    f(sim.get_id(sum_a));
    f(sim.get_id(sum_b));
    f(sim.get_id(product));
    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));

    return success();
}

template<int QssLevel, typename F>
status example_qss_lif(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(7)))
        return new_error(simulation_errc::models_container_full);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto& cst                = sim.alloc<constant>();
    get_p(sim, cst).reals[0] = 1.0;

    auto& cst_cross                = sim.alloc<constant>();
    get_p(sim, cst_cross).reals[0] = Vr;

    auto& sum                = sim.alloc<abstract_wsum<QssLevel, 2>>();
    get_p(sim, sum).reals[2] = -irt::one / tau;
    get_p(sim, sum).reals[3] = V0 / tau;

    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>();
    get_p(sim, integrator).reals[1] = 0.001_r;

    auto& cross                = sim.alloc<abstract_cross<QssLevel>>();
    get_p(sim, cross).reals[0] = Vt;

    irt_check(sim.connect_dynamics(cross, 0, integrator, 1));
    irt_check(sim.connect_dynamics(cross, 1, sum, 0));
    irt_check(sim.connect_dynamics(integrator, 0, cross, 0));
    irt_check(sim.connect_dynamics(integrator, 0, cross, 2));
    irt_check(sim.connect_dynamics(cst_cross, 0, cross, 1));
    irt_check(sim.connect_dynamics(cst, 0, sum, 1));
    irt_check(sim.connect_dynamics(sum, 0, integrator, 0));

    f(sim.get_id(sum));
    f(sim.get_id(cst));
    f(sim.get_id(cst_cross));
    f(sim.get_id(integrator));
    f(sim.get_id(cross));

    return success();
}

template<int QssLevel, typename F>
status example_qss_izhikevich(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(12) && sim.can_connect(22)))
        return new_error(simulation_errc::models_container_full);

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

    get_p(sim, cst).reals[0]  = 1.0_r;
    get_p(sim, cst2).reals[0] = c;
    get_p(sim, cst3).reals[0] = I;

    get_p(sim, cross).reals[0]  = vt;
    get_p(sim, cross2).reals[0] = vt;

    get_p(sim, integrator_a).reals[1] = 0.01_r;
    get_p(sim, integrator_b).reals[1] = 0.01_r;

    get_p(sim, sum_a).reals[2] = 1.0_r;
    get_p(sim, sum_a).reals[3] = -1.0_r;
    get_p(sim, sum_b).reals[2] = -a;
    get_p(sim, sum_b).reals[3] = a * b;
    get_p(sim, sum_c).reals[2] = 0.04_r;
    get_p(sim, sum_c).reals[3] = 5.0_r;
    get_p(sim, sum_c).reals[2] = 140.0_r;
    get_p(sim, sum_c).reals[3] = 1.0_r;
    get_p(sim, sum_d).reals[2] = 1.0_r;
    get_p(sim, sum_d).reals[3] = d;

    irt_check(sim.connect_dynamics(integrator_a, 0, cross, 0));
    irt_check(sim.connect_dynamics(cst2, 0, cross, 1));
    irt_check(sim.connect_dynamics(integrator_a, 0, cross, 2));

    irt_check(sim.connect_dynamics(cross, 1, product, 0));
    irt_check(sim.connect_dynamics(cross, 1, product, 1));
    irt_check(sim.connect_dynamics(product, 0, sum_c, 0));
    irt_check(sim.connect_dynamics(cross, 1, sum_c, 1));
    irt_check(sim.connect_dynamics(cross, 1, sum_b, 1));

    irt_check(sim.connect_dynamics(cst, 0, sum_c, 2));
    irt_check(sim.connect_dynamics(cst3, 0, sum_c, 3));

    irt_check(sim.connect_dynamics(sum_c, 0, sum_a, 0));
    irt_check(sim.connect_dynamics(cross2, 1, sum_a, 1));
    irt_check(sim.connect_dynamics(sum_a, 0, integrator_a, 0));
    irt_check(sim.connect_dynamics(cross, 0, integrator_a, 1));

    irt_check(sim.connect_dynamics(cross2, 1, sum_b, 0));
    irt_check(sim.connect_dynamics(sum_b, 0, integrator_b, 0));

    irt_check(sim.connect_dynamics(cross2, 0, integrator_b, 1));
    irt_check(sim.connect_dynamics(integrator_a, 0, cross2, 0));
    irt_check(sim.connect_dynamics(integrator_b, 0, cross2, 2));
    irt_check(sim.connect_dynamics(sum_d, 0, cross2, 1));
    irt_check(sim.connect_dynamics(integrator_b, 0, sum_d, 0));
    irt_check(sim.connect_dynamics(cst, 0, sum_d, 1));

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
status example_qss_van_der_pol(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(9)))
        return new_error(simulation_errc::models_container_full);

    auto& sum          = sim.alloc<abstract_wsum<QssLevel, 3>>();
    auto& product1     = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& product2     = sim.alloc<abstract_multiplier<QssLevel>>();
    auto& integrator_a = sim.alloc<abstract_integrator<QssLevel>>();
    auto& integrator_b = sim.alloc<abstract_integrator<QssLevel>>();

    get_p(sim, integrator_a).reals[0] = 0.0_r;
    get_p(sim, integrator_a).reals[1] = 0.001_r;

    get_p(sim, integrator_b).reals[0] = 10.0_r;
    get_p(sim, integrator_b).reals[1] = 0.001_r;

    constexpr double mu      = 4.0_r;
    get_p(sim, sum).reals[3] = mu;
    get_p(sim, sum).reals[4] = -mu;
    get_p(sim, sum).reals[5] = -1.0_r;

    irt_check(sim.connect_dynamics(integrator_b, 0, integrator_a, 0));
    irt_check(sim.connect_dynamics(sum, 0, integrator_b, 0));
    irt_check(sim.connect_dynamics(integrator_b, 0, sum, 0));
    irt_check(sim.connect_dynamics(product2, 0, sum, 1));
    irt_check(sim.connect_dynamics(integrator_a, 0, sum, 2));
    irt_check(sim.connect_dynamics(integrator_b, 0, product1, 0));
    irt_check(sim.connect_dynamics(integrator_a, 0, product1, 1));
    irt_check(sim.connect_dynamics(product1, 0, product2, 0));
    irt_check(sim.connect_dynamics(integrator_a, 0, product2, 1));

    f(sim.get_id(sum));
    f(sim.get_id(product1));
    f(sim.get_id(product2));
    f(sim.get_id(integrator_a));
    f(sim.get_id(integrator_b));

    return success();
}

template<int QssLevel, typename F>
status example_qss_negative_lif(simulation& sim, F f) noexcept
{
    using namespace irt::literals;

    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    if (!(sim.can_alloc(5) && sim.can_connect(7)))
        return new_error(simulation_errc::models_container_full);

    auto& sum        = sim.alloc<abstract_wsum<QssLevel, 2>>();
    auto& integrator = sim.alloc<abstract_integrator<QssLevel>>();
    auto& cross      = sim.alloc<abstract_cross<QssLevel>>();
    auto& cst        = sim.alloc<constant>();
    auto& cst_cross  = sim.alloc<constant>();

    constexpr real tau = 10.0_r;
    constexpr real Vt  = -1.0_r;
    constexpr real V0  = -10.0_r;
    constexpr real Vr  = 0.0_r;

    get_p(sim, sum).reals[2] = -1.0_r / tau;
    get_p(sim, sum).reals[3] = V0 / tau;

    get_p(sim, cst).reals[0]       = 1.0_r;
    get_p(sim, cst_cross).reals[0] = Vr;

    get_p(sim, integrator).reals[0] = 0.0_r;
    get_p(sim, integrator).reals[1] = 0.001_r;

    get_p(sim, cross).reals[0]    = Vt;
    get_p(sim, cross).integers[0] = false;

    irt_check(sim.connect_dynamics(cross, 0, integrator, 1));
    irt_check(sim.connect_dynamics(cross, 1, sum, 0));
    irt_check(sim.connect_dynamics(integrator, 0, cross, 0));
    irt_check(sim.connect_dynamics(integrator, 0, cross, 2));
    irt_check(sim.connect_dynamics(cst_cross, 0, cross, 1));
    irt_check(sim.connect_dynamics(cst, 0, sum, 1));
    irt_check(sim.connect_dynamics(sum, 0, integrator, 0));

    f(sim.get_id(sum));
    f(sim.get_id(integrator));
    f(sim.get_id(cross));
    f(sim.get_id(cst));
    f(sim.get_id(cst_cross));

    return success();
}

} // namespace irritator

#endif
