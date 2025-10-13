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

    if (!(sim.can_alloc(19) && sim.can_connect(22)))
        return new_error(simulation_errc::models_container_full);

    auto& mdl_0 = sim.alloc<irt::abstract_integrator<QssLevel>>();
    sim.parameters[sim.get_id(mdl_0)].reals = {
        0.00000000000000000, 0.01000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_0)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_1 = sim.alloc<irt::abstract_integrator<QssLevel>>();
    sim.parameters[sim.get_id(mdl_1)].reals = {
        0.00000000000000000, 0.01000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_1)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_2 = sim.alloc<irt::abstract_square<QssLevel>>();
    sim.parameters[sim.get_id(mdl_2)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_2)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_3 = sim.alloc<irt::abstract_multiplier<QssLevel>>();
    sim.parameters[sim.get_id(mdl_3)].reals = {
        0.04000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_3)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_4 = sim.alloc<irt::abstract_multiplier<QssLevel>>();
    sim.parameters[sim.get_id(mdl_4)].reals = {
        5.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_4)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_5 = sim.alloc<irt::abstract_wsum<QssLevel, 2>>();
    sim.parameters[sim.get_id(mdl_5)].reals = {
        140.00000000000000000, 0.00000000000000000, 1.00000000000000000,
        -1.00000000000000000,  0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_5)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_6                             = sim.alloc<irt::constant>();
    sim.parameters[sim.get_id(mdl_6)].reals = {
        -99.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_6)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_7 = sim.alloc<irt::abstract_sum<QssLevel, 4>>();
    sim.parameters[sim.get_id(mdl_7)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_7)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_8                             = sim.alloc<irt::constant>();
    sim.parameters[sim.get_id(mdl_8)].reals = {
        0.20000000000000001, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_8)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_9                             = sim.alloc<irt::constant>();
    sim.parameters[sim.get_id(mdl_9)].reals = {
        2.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_9)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_10 = sim.alloc<irt::abstract_multiplier<QssLevel>>();
    sim.parameters[sim.get_id(mdl_10)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_10)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_11 = sim.alloc<irt::abstract_wsum<QssLevel, 2>>();
    sim.parameters[sim.get_id(mdl_11)].reals = {
        0.00000000000000000,  0.00000000000000000, 1.00000000000000000,
        -1.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,  0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_11)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_12 = sim.alloc<irt::abstract_multiplier<QssLevel>>();
    sim.parameters[sim.get_id(mdl_12)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_12)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_13 = sim.alloc<irt::abstract_cross<QssLevel>>();
    sim.parameters[sim.get_id(mdl_13)].reals = {
        30.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,  0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,  0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_13)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_14 = sim.alloc<irt::abstract_flipflop<QssLevel>>();
    sim.parameters[sim.get_id(mdl_14)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_14)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_15                             = sim.alloc<irt::constant>();
    sim.parameters[sim.get_id(mdl_15)].reals = {
        -65.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_15)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_16 = sim.alloc<irt::abstract_sum<QssLevel, 2>>();
    sim.parameters[sim.get_id(mdl_16)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_16)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_17                             = sim.alloc<irt::constant>();
    sim.parameters[sim.get_id(mdl_17)].reals = {
        -16.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000, 0.00000000000000000,
        0.00000000000000000,   0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_17)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    auto& mdl_18 = sim.alloc<irt::abstract_flipflop<QssLevel>>();
    sim.parameters[sim.get_id(mdl_18)].reals = {
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000, 0.00000000000000000,
        0.00000000000000000, 0.00000000000000000
    };
    sim.parameters[sim.get_id(mdl_18)].integers = { 0, 0, 0, 0, 0, 0, 0, 0 };

    irt_check(sim.connect_dynamics(mdl_0, 0, mdl_2, 0));
    irt_check(sim.connect_dynamics(mdl_0, 0, mdl_4, 1));
    irt_check(sim.connect_dynamics(mdl_0, 0, mdl_10, 1));
    irt_check(sim.connect_dynamics(mdl_0, 0, mdl_13, 0));
    irt_check(sim.connect_dynamics(mdl_1, 0, mdl_5, 1));
    irt_check(sim.connect_dynamics(mdl_1, 0, mdl_11, 1));
    irt_check(sim.connect_dynamics(mdl_1, 0, mdl_16, 0));
    irt_check(sim.connect_dynamics(mdl_2, 0, mdl_3, 1));
    irt_check(sim.connect_dynamics(mdl_3, 0, mdl_7, 0));
    irt_check(sim.connect_dynamics(mdl_4, 0, mdl_7, 1));
    irt_check(sim.connect_dynamics(mdl_5, 0, mdl_7, 2));
    irt_check(sim.connect_dynamics(mdl_6, 0, mdl_7, 3));
    irt_check(sim.connect_dynamics(mdl_7, 0, mdl_0, 0));
    irt_check(sim.connect_dynamics(mdl_8, 0, mdl_12, 0));
    irt_check(sim.connect_dynamics(mdl_9, 0, mdl_10, 0));
    irt_check(sim.connect_dynamics(mdl_10, 0, mdl_11, 0));
    irt_check(sim.connect_dynamics(mdl_11, 0, mdl_12, 1));
    irt_check(sim.connect_dynamics(mdl_12, 0, mdl_1, 0));
    irt_check(sim.connect_dynamics(mdl_13, 0, mdl_14, 1));
    irt_check(sim.connect_dynamics(mdl_13, 0, mdl_18, 1));
    irt_check(sim.connect_dynamics(mdl_14, 0, mdl_0, 1));
    irt_check(sim.connect_dynamics(mdl_15, 0, mdl_14, 0));
    irt_check(sim.connect_dynamics(mdl_16, 0, mdl_18, 0));
    irt_check(sim.connect_dynamics(mdl_17, 0, mdl_16, 1));
    irt_check(sim.connect_dynamics(mdl_18, 0, mdl_1, 1));

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
