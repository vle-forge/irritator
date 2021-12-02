// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

#include <fmt/format.h>

namespace irt {

template<typename Dynamics>
std::pair<Dynamics*, child_id> alloc(component& parent) noexcept
{
    irt_assert(!parent.models.full());

    auto& mdl  = parent.models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = nullptr;

    new (&mdl.dyn) Dynamics{};
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (is_detected_v<has_input_port_t, Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = static_cast<u64>(-1);

    if constexpr (is_detected_v<has_output_port_t, Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = static_cast<u64>(-1);

    auto& child = parent.children.alloc(parent.models.get_id(mdl));

    return std::make_pair(&dyn, parent.children.get_id(child));
}

template<typename DynamicsSrc, typename DynamicsDst>
status connect(modeling&    mod,
               component&   c,
               DynamicsSrc& src,
               i8           port_src,
               DynamicsDst& dst,
               i8           port_dst) noexcept
{
    model& src_model = get_model(*src.first);
    model& dst_model = get_model(*dst.first);

    irt_return_if_fail(
      is_ports_compatible(src_model, port_src, dst_model, port_dst),
      status::model_connect_bad_dynamics);

    irt_return_if_fail(c.connections.can_alloc(1),
                       status::simulation_not_enough_connection);

    mod.connect(c, src.second, port_src, dst.second, port_dst);

    return status::success;
}

status add_integrator_component_port(component& com, child_id id) noexcept
{
    auto* child = com.children.try_to_get(id);
    irt_assert(child);
    irt_assert(child->type == child_type::model);

    com.x.emplace_back(enum_cast<model_id>(child->id), i8(0));
    com.y.emplace_back(enum_cast<model_id>(child->id), i8(0));

    return status::success;
}

template<int QssLevel>
status add_lotka_volterra(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = com.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_a.first->default_X  = 18.0_r;
    integrator_a.first->default_dQ = 0.1_r;

    auto integrator_b              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_b.first->default_X  = 7.0_r;
    integrator_b.first->default_dQ = 0.1_r;

    auto product = alloc<abstract_multiplier<QssLevel>>(com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_a.first->default_input_coeffs[0] = 2.0_r;
    sum_a.first->default_input_coeffs[1] = -0.4_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(com);
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

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = com.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst                 = alloc<constant>(com);
    cst.first->default_value = 1.0;

    auto cst_cross                 = alloc<constant>(com);
    cst_cross.first->default_value = Vr;

    auto sum                           = alloc<abstract_wsum<QssLevel, 2>>(com);
    sum.first->default_input_coeffs[0] = -irt::one / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    auto integrator              = alloc<abstract_integrator<QssLevel>>(com);
    integrator.first->default_X  = 0._r;
    integrator.first->default_dQ = 0.001_r;

    auto cross                     = alloc<abstract_cross<QssLevel>>(com);
    cross.first->default_threshold = Vt;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(com, integrator.second);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto cst          = alloc<constant>(com);
    auto cst2         = alloc<constant>(com);
    auto cst3         = alloc<constant>(com);
    auto sum_a        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto sum_b        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto sum_c        = alloc<abstract_wsum<QssLevel, 4>>(com);
    auto sum_d        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto product      = alloc<abstract_multiplier<QssLevel>>(com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(com);
    auto cross        = alloc<abstract_cross<QssLevel>>(com);
    auto cross2       = alloc<abstract_cross<QssLevel>>(com);

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

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum          = alloc<abstract_wsum<QssLevel, 3>>(com);
    auto product1     = alloc<abstract_multiplier<QssLevel>>(com);
    auto product2     = alloc<abstract_multiplier<QssLevel>>(com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(com);

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

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto integrator = alloc<abstract_integrator<QssLevel>>(com);
    auto cross      = alloc<abstract_cross<QssLevel>>(com);
    auto cst        = alloc<constant>(com);
    auto cst_cross  = alloc<constant>(com);

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

    add_integrator_component_port(com, integrator.second);

    return status::success;
}

template<int QssLevel>
status add_seir_lineaire(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(10) && com.children.can_alloc(10) &&
                   com.connections.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum_a        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto sum_b        = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto product_a    = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_b    = alloc<abstract_multiplier<QssLevel>>(com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(com);
    auto integrator_c = alloc<abstract_integrator<QssLevel>>(com);
    auto integrator_d = alloc<abstract_integrator<QssLevel>>(com);
    auto constant_a   = alloc<constant>(com);
    auto constant_b   = alloc<constant>(com);

    sum_a.first->input_coeffs[0] = -0.005_r;
    sum_a.first->input_coeffs[1] = -0.4_r;

    sum_b.first->input_coeffs[0] = -0.135_r;
    sum_b.first->input_coeffs[1] = 0.1_r;

    integrator_a.first->X  = 10.0_r;
    integrator_a.first->dQ = 0.01_r;

    integrator_b.first->X  = 15.0_r;
    integrator_b.first->dQ = 0.01_r;

    integrator_c.first->X  = 10.0_r;
    integrator_c.first->dQ = 0.01_r;

    integrator_d.first->X  = 18.0_r;
    integrator_d.first->dQ = 0.01_r;

    constant_a.first->value = -0.005_r;

    constant_b.first->value = -0.135_r;

    connect(mod, com, constant_a, 0, product_a, 0);
    connect(mod, com, constant_b, 0, product_b, 0);
    connect(mod, com, sum_a, 0, integrator_c, 0);
    connect(mod, com, sum_b, 0, integrator_d, 0);
    connect(mod, com, integrator_b, 0, sum_a, 0);
    connect(mod, com, integrator_c, 0, sum_a, 1);
    connect(mod, com, integrator_c, 0, sum_b, 0);
    connect(mod, com, integrator_d, 0, sum_b, 1);
    connect(mod, com, integrator_a, 0, product_a, 1);
    connect(mod, com, integrator_b, 0, product_b, 1);
    connect(mod, com, product_a, 0, sum_a, 1);
    connect(mod, com, product_b, 0, sum_b, 1);

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);
    add_integrator_component_port(com, integrator_c.second);
    add_integrator_component_port(com, integrator_d.second);

    return status::success;
}

template<int QssLevel>
status add_seir_nonlineaire(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(27) && com.children.can_alloc(27) &&
                   com.connections.can_alloc(32);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum_a = alloc<abstract_wsum<QssLevel, 3>>(com);
    sum_a.first->default_input_coeffs[0] = 0.5_r;
    sum_a.first->default_input_coeffs[1] = 1.0_r;
    sum_a.first->default_input_coeffs[2] = 1.0_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_b.first->default_input_coeffs[0] = 1.0_r;
    sum_b.first->default_input_coeffs[1] = 1.0_r;

    auto sum_c = alloc<abstract_wsum<QssLevel, 3>>(com);
    sum_c.first->default_input_coeffs[0] = 1.5_r;
    sum_c.first->default_input_coeffs[1] = 0.698_r;
    sum_c.first->default_input_coeffs[2] = 0.387_r;

    auto sum_d = alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_d.first->default_input_coeffs[0] = 1.0_r;
    sum_d.first->default_input_coeffs[1] = 1.5_r;

    auto product_a = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_b = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_c = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_d = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_e = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_f = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_g = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_h = alloc<abstract_multiplier<QssLevel>>(com);
    auto product_i = alloc<abstract_multiplier<QssLevel>>(com);

    auto integrator_a              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_a.first->default_X  = 10.0_r;
    integrator_a.first->default_dQ = 0.01_r;

    auto integrator_b              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_b.first->default_X  = 12.0_r;
    integrator_b.first->default_dQ = 0.01_r;

    auto integrator_c              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_c.first->default_X  = 13.50_r;
    integrator_c.first->default_dQ = 0.01_r;

    auto integrator_d              = alloc<abstract_integrator<QssLevel>>(com);
    integrator_d.first->default_X  = 15.0_r;
    integrator_d.first->default_dQ = 0.01_r;

    // The values used here are from Singh et al., 2017

    auto constant_a                 = alloc<constant>(com);
    constant_a.first->default_value = 0.005_r;

    auto constant_b                 = alloc<constant>(com);
    constant_b.first->default_value = -0.0057_r;

    auto constant_c                 = alloc<constant>(com);
    constant_c.first->default_value = -0.005_r;

    auto constant_d                 = alloc<constant>(com);
    constant_d.first->default_value = 0.0057_r;

    auto constant_e                 = alloc<constant>(com);
    constant_e.first->default_value = -0.135_r;

    auto constant_f                 = alloc<constant>(com);
    constant_f.first->default_value = 0.135_r;

    auto constant_g                 = alloc<constant>(com);
    constant_g.first->default_value = -0.072_r;

    auto constant_h                 = alloc<constant>(com);
    constant_h.first->default_value = 0.005_r;

    auto constant_i                 = alloc<constant>(com);
    constant_i.first->default_value = 0.067_r;

    auto constant_j                 = alloc<constant>(com);
    constant_j.first->default_value = -0.005_r;

    connect(mod, com, constant_a, 0, sum_a, 0);
    connect(mod, com, constant_h, 0, sum_c, 2);
    connect(mod, com, constant_b, 0, product_a, 0);
    connect(mod, com, constant_c, 0, product_b, 0);
    connect(mod, com, constant_d, 0, product_c, 0);
    connect(mod, com, constant_e, 0, product_d, 0);
    connect(mod, com, constant_f, 0, product_e, 0);
    connect(mod, com, constant_g, 0, product_f, 0);
    connect(mod, com, constant_h, 0, product_g, 0);
    connect(mod, com, constant_i, 0, product_h, 0);
    connect(mod, com, product_i, 0, product_a, 1);
    connect(mod, com, product_i, 0, product_c, 1);
    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);
    connect(mod, com, sum_c, 0, integrator_c, 0);
    connect(mod, com, sum_d, 0, integrator_d, 0);
    connect(mod, com, product_a, 0, sum_a, 1);
    connect(mod, com, product_b, 0, sum_a, 2);
    connect(mod, com, product_c, 0, sum_b, 0);
    connect(mod, com, product_d, 0, sum_b, 1);
    connect(mod, com, product_e, 0, sum_c, 0);
    connect(mod, com, product_f, 0, sum_c, 1);
    connect(mod, com, product_g, 0, sum_d, 0);
    connect(mod, com, product_h, 0, sum_d, 1);
    connect(mod, com, integrator_a, 0, product_b, 1);
    connect(mod, com, integrator_b, 0, product_d, 1);
    connect(mod, com, integrator_b, 0, product_e, 1);
    connect(mod, com, integrator_c, 0, product_f, 1);
    connect(mod, com, integrator_c, 0, product_g, 1);
    connect(mod, com, integrator_d, 0, product_h, 1);
    connect(mod, com, integrator_a, 0, product_i, 0);
    connect(mod, com, integrator_c, 0, product_i, 1);

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);
    add_integrator_component_port(com, integrator_c.second);
    add_integrator_component_port(com, integrator_d.second);

    return status::success;
}

static bool get_component_type(const char*     type_string,
                               component_type* type_found) noexcept
{
    struct cpp_component_entry
    {
        const char*    name;
        component_type type;
    };

    static const cpp_component_entry tab[] = {
        { "qss1_izhikevich", component_type::qss1_izhikevich },
        { "qss1_lif", component_type::qss1_lif },
        { "qss1_lotka_volterra", component_type::qss1_lotka_volterra },
        { "qss1_negative_lif", component_type::qss1_negative_lif },
        { "qss1_seir_lineaire", component_type::qss1_seir_lineaire },
        { "qss1_seir_nonlineaire", component_type::qss1_seir_nonlineaire },
        { "qss1_van_der_pol", component_type::qss1_van_der_pol },
        { "qss2_izhikevich", component_type::qss2_izhikevich },
        { "qss2_lif", component_type::qss2_lif },
        { "qss2_lotka_volterra", component_type::qss2_lotka_volterra },
        { "qss2_negative_lif", component_type::qss2_negative_lif },
        { "qss2_seir_lineaire", component_type::qss2_seir_lineaire },
        { "qss2_seir_nonlineaire", component_type::qss2_seir_nonlineaire },
        { "qss2_van_der_pol", component_type::qss2_van_der_pol },
        { "qss3_izhikevich", component_type::qss3_izhikevich },
        { "qss3_lif", component_type::qss3_lif },
        { "qss3_lotka_volterra", component_type::qss3_lotka_volterra },
        { "qss3_negative_lif", component_type::qss3_negative_lif },
        { "qss3_seir_lineaire", component_type::qss3_seir_lineaire },
        { "qss3_seir_nonlineaire", component_type::qss3_seir_nonlineaire },
        { "qss3_van_der_pol", component_type::qss3_van_der_pol },
        { "file", component_type::file },
        { "memory", component_type::memory }
    };

    auto it = std::lower_bound(std::begin(tab),
                               std::end(tab),
                               type_string,
                               [](const auto& entry, const char* buffer) {
                                   return 0 == std::strcmp(entry.name, buffer);
                               });

    if (it == std::end(tab) || std::strcmp(it->name, type_string))
        return false;

    *type_found = it->type;

    return true;
}

static component* find_file_component(modeling&  mod,
                                      dir_path&  dir,
                                      file_path& file) noexcept
{
    auto dir_id  = mod.dir_paths.get_id(dir);
    auto file_id = mod.file_paths.get_id(file);

    component* compo = nullptr;
    while (mod.components.next(compo)) {
        if (compo->dir == dir_id && compo->file == file_id)
            return compo;
    }

    return nullptr;
}

static component* load_component(modeling&   mod,
                                 dir_path&   dir,
                                 const char* file) noexcept
{
    component* ret = nullptr;

    try {
        std::filesystem::path irt_file(dir.path.c_str());
        irt_file /= file;

        std::error_code ec;
        if (std::filesystem::is_regular_file(irt_file, ec)) {
            if (std::ifstream ifs(irt_file); ifs) {
                auto&  compo = mod.components.alloc();
                reader r(ifs);
                if (is_success(r(mod, compo, mod.srcs)))
                    ret = &compo;
            }
        }
    } catch (const std::exception& /*e*/) {
    }

    return ret;
}

static component* find_cpp_component(modeling&   mod,
                                     const char* type_str) noexcept
{
    component_type type;
    if (get_component_type(type_str, &type)) {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            if (compo->type == type)
                return compo;
        }
    }

    irt_unreachable();
}

static bool is_valid(const modeling_initializer& params) noexcept
{
    return params.model_capacity > 0 && params.tree_capacity > 0 &&
           params.description_capacity > 0 && params.component_capacity > 0 &&
           params.observer_capacity > 0 && params.file_path_capacity > 0 &&
           params.children_capacity > 0 && params.connection_capacity > 0 &&
           params.constant_source_capacity > 0 &&
           params.binary_file_source_capacity > 0 &&
           params.text_file_source_capacity > 0 &&
           params.random_source_capacity > 0;
}

static status try_init(modeling& mod, modeling_initializer& p) noexcept
{
    irt_return_if_bad(mod.tree_nodes.init(p.tree_capacity));
    irt_return_if_bad(mod.descriptions.init(p.description_capacity));
    irt_return_if_bad(mod.components.init(p.component_capacity));
    irt_return_if_bad(mod.observers.init(p.observer_capacity));
    irt_return_if_bad(mod.file_paths.init(p.file_path_capacity));
    irt_return_if_bad(
      mod.srcs.constant_sources.init(p.constant_source_capacity));
    irt_return_if_bad(
      mod.srcs.text_file_sources.init(p.text_file_source_capacity));
    irt_return_if_bad(mod.srcs.random_sources.init(p.random_source_capacity));
    irt_return_if_bad(
      mod.srcs.binary_file_sources.init(p.binary_file_source_capacity));

    return status::success;
}

status modeling::init(modeling_initializer& params) noexcept
{
    // In the future, these allocations will have to be replaced by an
    // allocator who can exit if the allocation fails.

    modeling_initializer default_params = {
        .model_capacity              = 256 * 64 * 16,
        .tree_capacity               = 256 * 16,
        .description_capacity        = 256 * 16,
        .component_capacity          = 256 * 12 * 8,
        .observer_capacity           = 256 * 16,
        .file_path_capacity          = 256 * 25 * 6,
        .children_capacity           = 256 * 64 * 16,
        .connection_capacity         = 256 * 64,
        .port_capacity               = 256 * 64,
        .constant_source_capacity    = 16,
        .binary_file_source_capacity = 16,
        .text_file_source_capacity   = 16,
        .random_source_capacity      = 16,
        .random_generator_seed       = 123456789u,
    };

    if (!is_valid(params))
        params = default_params;

    if (is_bad(try_init(*this, params))) {
        params = default_params;

        irt_return_if_bad(try_init(*this, params));
    }

    return status::success;
}

component_id modeling::search_component(const char* name,
                                        const char* hint) noexcept
{
    component* ret = nullptr;

    while (components.next(ret))
        if (ret->name == name)
            return components.get_id(ret);

    if (hint && std::strcmp(hint, "cpp") == 0) {
        ret = find_cpp_component(*this, name);
    } else {
        file_path* file = nullptr;
        while (file_paths.next(file))
            if (file->path == name)
                break;

        if (!file) {
            for (auto id : component_repertories) {
                if (auto* dir = dir_paths.try_to_get(id); dir) {
                    if (ret = load_component(*this, *dir, name); ret)
                        break;
                }
            }
        } else {
            for (auto id : component_repertories) {
                if (auto* dir = dir_paths.try_to_get(id); dir) {
                    if (ret = find_file_component(*this, *dir, *file); ret)
                        break;
                }
            }
        }
    }

    return ret ? components.get_id(*ret) : undefined<component_id>();
}

status modeling::fill_internal_components() noexcept
{
    irt_return_if_fail(components.can_alloc(15), status::success);

    {
        auto& c = components.alloc();
        c.name  = "QSS1 lotka volterra";
        c.type  = component_type::qss1_lotka_volterra;
        c.state = component_status::read_only;
        if (auto ret = add_lotka_volterra<1>(*this, c); is_bad(ret))
            return ret;
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 lotka volterra";
        c.type  = component_type::qss2_lotka_volterra;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 lotka volterra";
        c.type  = component_type::qss2_lotka_volterra;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 lif";
        c.type  = component_type::qss1_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 lif";
        c.type  = component_type::qss2_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 lif";
        c.type  = component_type::qss3_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 izhikevich";
        c.type  = component_type::qss1_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 izhikevich";
        c.type  = component_type::qss2_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 izhikevich";
        c.type  = component_type::qss3_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 van der pol";
        c.type  = component_type::qss1_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 van der pol";
        c.type  = component_type::qss2_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 van der pol";
        c.type  = component_type::qss3_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 negative lif";
        c.type  = component_type::qss1_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 negative lif";
        c.type  = component_type::qss2_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 negative lif";
        c.type  = component_type::qss3_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<3>(*this, c));
    }

    return status::success;
}

static void prepare_component_loading(modeling&              mod,
                                      dir_path_id            path_id,
                                      std::filesystem::path& file) noexcept
{
    auto& file_path  = mod.file_paths.alloc();
    file_path.parent = path_id;
    file_path.path   = std::string_view(file.string());

    auto& compo = mod.components.alloc();
    compo.name.assign(file.filename().string().c_str());
    compo.dir   = path_id;
    compo.file  = mod.file_paths.get_id(file_path);
    compo.type  = component_type::file;
    compo.state = component_status::unread;

    try {
        auto desc_file = file;
        desc_file.replace_extension(".desc");
        std::error_code ec;
        if (std::filesystem::exists(desc_file, ec)) {
            auto& desc  = mod.descriptions.alloc();
            desc.status = description_status::unread;
            compo.desc  = mod.descriptions.get_id(desc);
        }
    } catch (const std::exception& /*e*/) {
    }
}

static void prepare_component_loading(modeling&              mod,
                                      dir_path&              d_path,
                                      std::filesystem::path& path) noexcept
{
    namespace fs   = std::filesystem;
    auto d_path_id = mod.dir_paths.get_id(d_path);

    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        auto it = fs::recursive_directory_iterator{ path, ec };
        auto et = fs::recursive_directory_iterator{};

        while (it != et) {
            if (it->is_regular_file() && it->path().extension() == ".irt") {
                if (mod.components.can_alloc()) {
                    auto file = it->path();
                    file      = std::filesystem::relative(file, path, ec);
                    prepare_component_loading(mod, d_path_id, file);
                }
            }

            it = it.increment(ec);
        }
    }
}

static void prepare_component_loading(modeling& mod, dir_path& dir) noexcept
{
    try {
        std::filesystem::path p(dir.path.c_str());
        std::error_code       ec;

        if (std::filesystem::exists(p, ec)) {
            prepare_component_loading(mod, dir, p);
            dir.status = dir_path::status_option::read;
        }
    } catch (...) {
    }
}

static void prepare_component_loading(modeling& mod) noexcept
{
    for (auto id : mod.component_repertories)
        if (auto* dir = mod.dir_paths.try_to_get(id); dir)
            prepare_component_loading(mod, *dir);
}

static status load_component(modeling& mod, component& compo) noexcept
{
    try {
        auto& dir  = mod.dir_paths.get(compo.dir);
        auto& file = mod.file_paths.get(compo.file);

        std::filesystem::path file_path(dir.path.c_str());
        file_path /= file.path.c_str();
        bool read_description = false;

        {
            std::ifstream ifs(file_path);
            if (ifs.is_open()) {
                reader r(ifs);
                if (auto ret = r(mod, compo, mod.srcs); is_success(ret)) {
                    read_description = true;
                    compo.state      = component_status::unmodified;
                } else {
                    return ret;
                }
            }
        }

        if (read_description) {
            std::filesystem::path desc_file(file_path);
            desc_file.replace_extension(".desc");

            if (std::ifstream ifs{ desc_file }; ifs) {
                auto* desc = mod.descriptions.try_to_get(compo.desc);
                if (!desc) {
                    auto& d    = mod.descriptions.alloc();
                    desc       = &d;
                    compo.desc = mod.descriptions.get_id(d);
                }

                if (!ifs.read(desc->data.begin(), desc->data.capacity())) {
                    mod.descriptions.free(*desc);
                    compo.desc = undefined<description_id>();
                }
            }
        }
    } catch (...) {
        return status::io_file_format_error;
    }

    return status::success;
}

status modeling::fill_components() noexcept
{
    prepare_component_loading(*this);

    component* compo  = nullptr;
    component* to_del = nullptr;

    while (components.next(compo)) {
        if (to_del) {
            free(*to_del);
            to_del = nullptr;
        }

        if (compo->state == component_status::unread) {
            if (is_bad(load_component(*this, *compo)))
                to_del = compo;
        }
    }

    if (to_del)
        free(*to_del);

    return status::success;
}

static void remove_components_from_path(modeling& mod, dir_path& path) noexcept
{
    const auto id     = mod.dir_paths.get_id(path);
    component* compo  = nullptr;
    component* to_del = nullptr;

    while (mod.components.next(compo)) {
        if (to_del) {
            mod.free(*to_del);
            to_del = nullptr;
        }

        if (compo->dir == id)
            to_del = compo;
    }

    if (to_del)
        mod.free(*to_del);
}

status modeling::fill_components(dir_path& path) noexcept
{
    remove_components_from_path(*this, path);

    try {
        std::filesystem::path p(path.path.c_str());
        std::error_code       ec;

        if (std::filesystem::exists(p, ec) &&
            std::filesystem::is_directory(p, ec)) {
            prepare_component_loading(*this, path, p);
        }
    } catch (...) {
    }

    return status::success;
}

status modeling::connect(component& parent,
                         child_id   src,
                         i8         port_src,
                         child_id   dst,
                         i8         port_dst) noexcept
{
    irt_return_if_fail(parent.connections.can_alloc(1),
                       status::data_array_not_enough_memory);

    parent.connections.alloc(src, port_src, dst, port_dst);

    return status::success;
}

status build_simulation(modeling& /*mod*/, simulation& /*sim*/) noexcept
{
    // if (auto* c_ref = mod.component_refs.try_to_get(mod.head); c_ref)
    //     if (auto* compo = mod.components.try_to_get(c_ref->id); compo)
    //         return build_models(mod, *compo, sim);

    irt_bad_return(status::success);
}

static void free_child(data_array<child, child_id>& children,
                       data_array<model, model_id>& models,
                       child&                       c) noexcept
{
    if (c.type == child_type::model) {
        auto id = enum_cast<model_id>(c.id);
        if (auto* mdl = models.try_to_get(id); mdl)
            models.free(*mdl);
    }

    children.free(c);
}

void modeling::free(component& c) noexcept
{
    c.children.clear();
    c.models.clear();
    c.connections.clear();

    if (auto* desc = descriptions.try_to_get(c.desc); desc)
        descriptions.free(*desc);

    if (auto* path = file_paths.try_to_get(c.file); path)
        file_paths.free(*path);

    components.free(c);
}

void modeling::free(tree_node& node) noexcept
{
    // for (i32 i = 0, e = node.parameters.data.ssize(); i != e; ++i) {
    //     auto mdl_id = node.parameters.data[i].value;
    //     if (auto* mdl = models.try_to_get(mdl_id); mdl)
    //         models.free(*mdl);
    // }

    tree_nodes.free(node);
}

void modeling::free(component& parent, child& c) noexcept
{
    free_child(parent.children, parent.models, c);
}

void modeling::free(component& parent, connection& c) noexcept
{
    parent.connections.free(c);
}

status modeling::copy(component& src, component& dst) noexcept
{
    table<child_id, child_id> mapping;

    child* c = nullptr;
    while (src.children.next(c)) {
        const auto c_id = src.children.get_id(*c);

        if (c->type == child_type::model) {
            auto id = enum_cast<model_id>(c->id);
            if (auto* mdl = src.models.try_to_get(id); mdl) {
                irt_return_if_fail(dst.models.can_alloc(1),
                                   status::data_array_not_enough_memory);

                auto& new_child    = alloc(dst, mdl->type);
                auto  new_child_id = dst.children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(c_id, new_child_id);
            }
        } else {
            auto compo_id = enum_cast<component_id>(c->id);
            if (auto* compo = components.try_to_get(compo_id); compo) {
                auto& new_child    = dst.children.alloc(compo_id);
                auto  new_child_id = dst.children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(c_id, new_child_id);
            }
        }
    }

    mapping.sort();

    connection* con = nullptr;
    while (src.connections.next(con)) {
        if (auto* child_src = mapping.get(con->src); child_src) {
            if (auto* child_dst = mapping.get(con->dst); child_dst) {
                irt_return_if_fail(dst.connections.can_alloc(1),
                                   status::data_array_not_enough_memory);

                dst.connections.alloc(
                  *child_src, con->index_src, *child_dst, con->index_dst);
            }
        }
    }

    return status::success;
}

static status make_tree_recursive(
  data_array<component, component_id>& components,
  data_array<tree_node, tree_node_id>& trees,
  tree_node&                           parent,
  child_id                             compo_child_id,
  child&                               compo_child) noexcept
{
    auto compo_id = enum_cast<component_id>(compo_child.id);

    if (auto* compo = components.try_to_get(compo_id); compo) {
        irt_return_if_fail(trees.can_alloc(),
                           status::data_array_not_enough_memory);

        auto& new_tree = trees.alloc(compo_id, compo_child_id);
        new_tree.tree.set_id(&new_tree);
        new_tree.tree.parent_to(parent.tree);

        child* c = nullptr;
        while (compo->children.next(c)) {
            if (c->type == child_type::component) {
                irt_return_if_bad(make_tree_recursive(
                  components, trees, new_tree, compo->children.get_id(*c), *c));
            }
        }
    }

    return status::success;
}

void modeling::free(dir_path& dir) noexcept
{
    remove_components_from_path(*this, dir);
    dir_paths.free(dir);

    component_repertories.clear();

    dir_path* ptr = nullptr;
    while (dir_paths.next(ptr))
        component_repertories.emplace_back(dir_paths.get_id(ptr));
}

status modeling::make_tree_from(component& parent, tree_node_id* out) noexcept
{
    irt_return_if_fail(tree_nodes.can_alloc(),
                       status::data_array_not_enough_memory);

    auto  compo_id    = components.get_id(parent);
    auto& tree_parent = tree_nodes.alloc(compo_id, undefined<child_id>());

    tree_parent.tree.set_id(&tree_parent);

    child* c = nullptr;
    while (parent.children.next(c)) {
        if (c->type == child_type::component) {
            irt_return_if_bad(make_tree_recursive(components,
                                                  tree_nodes,
                                                  tree_parent,
                                                  parent.children.get_id(c),
                                                  *c));
        }
    }

    *out = tree_nodes.get_id(tree_parent);

    return status::success;
}

status modeling::clean(component& compo) noexcept
{
    child* c              = nullptr;
    child* to_del_c       = nullptr;
    bool   need_more_loop = false;

    do {
        while (compo.children.next(c)) {
            if (to_del_c) {
                compo.children.free(*to_del_c);
                to_del_c = nullptr;
            }

            if (c->type == child_type::model) {
                const auto id = enum_cast<model_id>(c->id);

                if (auto* model = compo.models.try_to_get(id); !model) {
                    to_del_c       = c;
                    need_more_loop = true;
                }
            } else {
                const auto id = enum_cast<component_id>(c->id);
                if (auto* compo = components.try_to_get(id); !compo) {
                    to_del_c       = c;
                    need_more_loop = true;
                }
            }
        }

        if (to_del_c)
            compo.children.free(*to_del_c);

        connection* con        = nullptr;
        connection* to_del_con = nullptr;

        while (compo.connections.next(con)) {
            if (to_del_con) {
                compo.connections.free(*to_del_con);
                to_del_con = nullptr;
            }

            auto* src = compo.children.try_to_get(con->src);
            auto* dst = compo.children.try_to_get(con->dst);

            if (!src || !dst) {
                to_del_con = con;
                continue;
            }

            if (src->type == child_type::model) {
                auto id = enum_cast<model_id>(src->id);
                if (auto* model = compo.models.try_to_get(id); !model) {
                    compo.children.free(*src);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_id>(src->id);
                if (auto* c = components.try_to_get(id); !c) {
                    compo.children.free(*src);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            }

            if (dst->type == child_type::model) {
                auto id = enum_cast<model_id>(dst->id);
                if (auto* model = compo.models.try_to_get(id); !model) {
                    compo.children.free(*dst);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_id>(dst->id);
                if (auto* c = components.try_to_get(id); !c) {
                    compo.children.free(*dst);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            }
        }

        if (to_del_con)
            compo.connections.free(*to_del_con);

    } while (need_more_loop);

    return status::success;
}

status modeling::save(component& c) noexcept
{
    auto* dir  = dir_paths.try_to_get(c.dir);
    auto* file = file_paths.try_to_get(c.file);

    // @todo Use a better status
    irt_return_if_fail(dir && file, status::gui_not_enough_memory);

    {
        std::filesystem::path p{ dir->path.c_str() };
        p /= file->path.c_str();
        p.replace_extension(".irt");

        std::ofstream ofs{ p };
        // @todo Use a better status
        irt_return_if_fail(ofs.is_open(), status::gui_not_enough_memory);

        writer w{ ofs };
        irt_return_if_bad(w(*this, c, srcs));
    }

    if (auto* desc = descriptions.try_to_get(c.desc); desc) {
        std::filesystem::path p{ dir->path.c_str() };
        p /= file->path.c_str();
        p.replace_extension(".desc");
        std::ofstream ofs{ p };
        ofs.write(desc->data.c_str(), desc->data.size());
    }

    c.state = component_status::unmodified;
    c.type  = component_type::file;

    return status::success;
}

} // namespace irt
