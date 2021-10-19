// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

#include <fmt/format.h>

namespace irt {

template<typename Dynamics>
std::pair<Dynamics*, child_id> alloc(modeling& mod, component& parent) noexcept
{
    irt_assert(!mod.models.full());

    auto& mdl  = mod.models.alloc();
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

    auto& child = mod.children.alloc(mod.models.get_id(mdl));
    parent.children.emplace_back(mod.children.get_id(child));

    return std::make_pair(&dyn, mod.children.get_id(child));
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

    irt_return_if_fail(mod.connections.can_alloc(1),
                       status::simulation_not_enough_connection);

    mod.connect(c, src.second, port_src, dst.second, port_dst);

    return status::success;
}

status add_integrator_component_port(component& com, child_id id) noexcept
{
    com.x.emplace_back(id, i8(0));
    com.y.emplace_back(id, i8(0));

    return status::success;
}

template<int QssLevel>
status add_lotka_volterra(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_a.first->default_X  = 18.0_r;
    integrator_a.first->default_dQ = 0.1_r;

    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_b.first->default_X  = 7.0_r;
    integrator_b.first->default_dQ = 0.1_r;

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    sum_a.first->default_input_coeffs[0] = 2.0_r;
    sum_a.first->default_input_coeffs[1] = -0.4_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
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

    auto integrator = alloc<abstract_integrator<QssLevel>>(mod, com);
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

    add_integrator_component_port(com, integrator.second);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto cst          = alloc<constant>(mod, com);
    auto cst2         = alloc<constant>(mod, com);
    auto cst3         = alloc<constant>(mod, com);
    auto sum_a        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_b        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_c        = alloc<abstract_wsum<QssLevel, 4>>(mod, com);
    auto sum_d        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto product      = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto cross        = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cross2       = alloc<abstract_cross<QssLevel>>(mod, com);

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
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum          = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    auto product1     = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product2     = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com);

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
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto integrator = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto cross      = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cst        = alloc<constant>(mod, com);
    auto cst_cross  = alloc<constant>(mod, com);

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
    bool success = mod.models.can_alloc(10) && com.children.can_alloc(10) &&
                   com.connections.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum_a        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_b        = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto product_a    = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_b    = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto integrator_c = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto integrator_d = alloc<abstract_integrator<QssLevel>>(mod, com);
    auto constant_a   = alloc<constant>(mod, com);
    auto constant_b   = alloc<constant>(mod, com);

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
    bool success = mod.models.can_alloc(27) && com.children.can_alloc(27) &&
                   com.connections.can_alloc(32);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum_a = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    sum_a.first->default_input_coeffs[0] = 0.5_r;
    sum_a.first->default_input_coeffs[1] = 1.0_r;
    sum_a.first->default_input_coeffs[2] = 1.0_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    sum_b.first->default_input_coeffs[0] = 1.0_r;
    sum_b.first->default_input_coeffs[1] = 1.0_r;

    auto sum_c = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    sum_c.first->default_input_coeffs[0] = 1.5_r;
    sum_c.first->default_input_coeffs[1] = 0.698_r;
    sum_c.first->default_input_coeffs[2] = 0.387_r;

    auto sum_d = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    sum_d.first->default_input_coeffs[0] = 1.0_r;
    sum_d.first->default_input_coeffs[1] = 1.5_r;

    auto product_a = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_b = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_c = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_d = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_e = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_f = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_g = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_h = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product_i = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto integrator_a = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_a.first->default_X  = 10.0_r;
    integrator_a.first->default_dQ = 0.01_r;

    auto integrator_b = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_b.first->default_X  = 12.0_r;
    integrator_b.first->default_dQ = 0.01_r;

    auto integrator_c = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_c.first->default_X  = 13.50_r;
    integrator_c.first->default_dQ = 0.01_r;

    auto integrator_d = alloc<abstract_integrator<QssLevel>>(mod, com);
    integrator_d.first->default_X  = 15.0_r;
    integrator_d.first->default_dQ = 0.01_r;

    // The values used here are from Singh et al., 2017

    auto constant_a                 = alloc<constant>(mod, com);
    constant_a.first->default_value = 0.005_r;

    auto constant_b                 = alloc<constant>(mod, com);
    constant_b.first->default_value = -0.0057_r;

    auto constant_c                 = alloc<constant>(mod, com);
    constant_c.first->default_value = -0.005_r;

    auto constant_d                 = alloc<constant>(mod, com);
    constant_d.first->default_value = 0.0057_r;

    auto constant_e                 = alloc<constant>(mod, com);
    constant_e.first->default_value = -0.135_r;

    auto constant_f                 = alloc<constant>(mod, com);
    constant_f.first->default_value = 0.135_r;

    auto constant_g                 = alloc<constant>(mod, com);
    constant_g.first->default_value = -0.072_r;

    auto constant_h                 = alloc<constant>(mod, com);
    constant_h.first->default_value = 0.005_r;

    auto constant_i                 = alloc<constant>(mod, com);
    constant_i.first->default_value = 0.067_r;

    auto constant_j                 = alloc<constant>(mod, com);
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

static status build_models_recursively(const modeling& mod,
                                       component_ref&  comp_ref,
                                       simulation&     sim)
{
    comp_ref.mappers.data.clear();

    auto* comp = mod.components.try_to_get(comp_ref.id);
    if (!comp)
        return status::success; /* @TODO certainly an error in API */

    for (i32 i = 0, e = comp->children.ssize(); i != e; ++i) {
        auto* child = mod.children.try_to_get(comp->children[i]);
        if (!child)
            continue;

        u64 id = child->id;

        if (child->type == child_type::model) {
            auto  src_id = enum_cast<model_id>(id);
            auto* src    = mod.models.try_to_get(src_id);
            if (!src)
                continue;

            irt_return_if_fail(sim.models.can_alloc(1),
                               status::simulation_not_enough_model);

            auto& dst    = sim.clone(*src);
            auto  dst_id = sim.models.get_id(dst);
            comp_ref.mappers.data.emplace_back(src_id, dst_id);
        } else {
            auto  src_id = enum_cast<component_ref_id>(id);
            auto* src    = mod.component_refs.try_to_get(src_id);
            if (!src)
                continue;

            irt_return_if_bad(build_models_recursively(mod, *src, sim));
        }
    }

    return status::success;
}

static status build_model_to_model_connection(component_ref& comp_ref,
                                              simulation&    sim,
                                              model_id       src,
                                              i8             port_src,
                                              model_id       dst,
                                              i8             port_dst) noexcept
{
    auto* src_mdl_id = comp_ref.mappers.get(enum_cast<model_id>(src));
    auto* dst_mdl_id = comp_ref.mappers.get(enum_cast<model_id>(dst));
    irt_assert(src_mdl_id && !dst_mdl_id);

    auto* mdl_src = sim.models.try_to_get(*src_mdl_id);
    auto* mdl_dst = sim.models.try_to_get(*dst_mdl_id);
    irt_assert(mdl_src && mdl_dst);

    return global_connect(sim, *mdl_src, port_src, *dst_mdl_id, port_dst);
}

static bool found_input_port(const modeling&  mod,
                             component_ref_id compo_ref,
                             i8               port,
                             model_id&        model_found,
                             i8&              port_found)
{
    for (;;) {
        auto* c_ref = mod.component_refs.try_to_get(compo_ref);
        if (!c_ref)
            return false;

        auto* c = mod.components.try_to_get(c_ref->id);
        if (!c)
            return false;

        if (!(0 <= port && port < c->x.ssize()))
            return false;

        auto* child = mod.children.try_to_get(c->x[port].id);
        if (!child)
            return false;

        if (child->type == child_type::model) {
            auto  id        = enum_cast<model_id>(child->id);
            auto* mapped_id = c_ref->mappers.get(id);

            if (mapped_id) {
                model_found = *mapped_id;
                port_found  = c->x[port].index;
                return true;
            } else {
                return false;
            }
        }

        compo_ref = enum_cast<component_ref_id>(child->id);
        port      = c->x[port].index;
    }
}

static bool found_output_port(const modeling&  mod,
                              component_ref_id compo_ref,
                              i8               port,
                              model_id&        model_found,
                              i8&              port_found)
{
    for (;;) {
        auto* c_ref = mod.component_refs.try_to_get(compo_ref);
        if (!c_ref)
            return false;

        auto* c = mod.components.try_to_get(c_ref->id);
        if (!c)
            return false;

        if (!(0 <= port && port < c->y.ssize()))
            return false;

        auto* child = mod.children.try_to_get(c->y[port].id);
        if (!child)
            return false;

        if (child->type == child_type::model) {
            auto  id        = enum_cast<model_id>(child->id);
            auto* mapped_id = c_ref->mappers.get(id);

            if (mapped_id) {
                model_found = *mapped_id;
                port_found  = c->y[port].index;
                return true;
            } else {
                return false;
            }
        }

        compo_ref = enum_cast<component_ref_id>(child->id);
        port      = c->y[port].index;
    }
}

static status build_model_to_component_connection(
  const modeling&      mod,
  const component_ref& compo_ref,
  simulation&          sim,
  model_id             src,
  i8                   port_src,
  component_ref_id     dst,
  i8                   port_dst)
{
    model_id model_found;
    i8       port_found;

    if (found_input_port(mod, dst, port_dst, model_found, port_found))
        if (auto* src_mdl_id = compo_ref.mappers.get(src); src_mdl_id)
            if (auto* src_mdl = sim.models.try_to_get(*src_mdl_id); src_mdl)
                return global_connect(
                  sim, *src_mdl, port_src, model_found, port_found);

    return status::success; // @todo fail
}

static status build_component_to_model_connection(
  const modeling&      mod,
  const component_ref& compo_ref,
  simulation&          sim,
  component_ref_id     src,
  i8                   port_src,
  model_id             dst,
  i8                   port_dst)
{
    model_id model_found;
    i8       port_found;

    if (found_output_port(mod, src, port_src, model_found, port_found)) {
        auto* dst_mdl_id = compo_ref.mappers.get(dst);
        irt_return_if_fail(
          dst_mdl_id,
          status::model_connect_bad_dynamics); // @todo Fix connection

        if (auto* src_mdl_id = compo_ref.mappers.get(model_found); src_mdl_id)
            if (auto* src_mdl = sim.models.try_to_get(*src_mdl_id); src_mdl)

                return global_connect(
                  sim, *src_mdl, port_found, *dst_mdl_id, port_dst);
    }

    return status::success;
}

static status build_component_to_component_connection(const modeling&  mod,
                                                      simulation&      sim,
                                                      component_ref_id src,
                                                      i8               port_src,
                                                      component_ref_id dst,
                                                      i8               port_dst)
{
    model_id src_model_found, dst_model_found;
    i8       src_port_found, dst_port_found;

    model* src_mdl = nullptr;

    if (found_input_port(mod, dst, port_dst, dst_model_found, dst_port_found))
        if (found_output_port(
              mod, src, port_src, src_model_found, src_port_found))
            if (src_mdl = sim.models.try_to_get(src_model_found); src_mdl)
                return global_connect(sim,
                                      *src_mdl,
                                      src_port_found,
                                      dst_model_found,
                                      dst_port_found);

    return status::model_connect_bad_dynamics; // @todo Fix connection
}

static status build_connections_recursively(modeling&      mod,
                                            component_ref& c_ref,
                                            simulation&    sim)
{
    auto* compo = mod.components.try_to_get(c_ref.id);
    if (!compo)
        return status::success; // @todo certainly an error

    // Build connections from the leaf to the head of the component
    // hierarchy.
    for (i32 i = 0, e = compo->children.ssize(); i != e; ++i) {
        if (auto* child = mod.children.try_to_get(compo->children[i]); child) {
            if (child->type == child_type::component) {
                u64   id       = child->id;
                auto  child_id = enum_cast<component_ref_id>(id);
                auto* child    = mod.component_refs.try_to_get(child_id);
                if (!child)
                    continue;

                build_connections_recursively(mod, *child, sim);
            }
        }
    }

    for (i32 i = 0, e = compo->connections.ssize(); i != e; ++i) {
        auto* con = mod.connections.try_to_get(compo->connections[i]);
        if (!con)
            continue;

        auto* src = mod.children.try_to_get(con->src);
        auto* dst = mod.children.try_to_get(con->dst);

        if (src->type == child_type::model) {
            if (dst->type == child_type::model) {
                irt_return_if_bad(
                  build_model_to_model_connection(c_ref,
                                                  sim,
                                                  enum_cast<model_id>(src->id),
                                                  con->index_src,
                                                  enum_cast<model_id>(dst->id),
                                                  con->index_dst));
            } else {
                irt_return_if_bad(build_model_to_component_connection(
                  mod,
                  c_ref,
                  sim,
                  enum_cast<model_id>(src->id),
                  con->index_src,
                  enum_cast<component_ref_id>(dst->id),
                  con->index_dst));
            }
        } else {
            if (dst->type == child_type::model) {
                irt_return_if_bad(build_component_to_model_connection(
                  mod,
                  c_ref,
                  sim,
                  enum_cast<component_ref_id>(src->id),
                  con->index_src,
                  enum_cast<model_id>(dst->id),
                  con->index_dst));
            } else {
                irt_return_if_bad(build_component_to_component_connection(
                  mod,
                  sim,
                  enum_cast<component_ref_id>(src->id),
                  con->index_src,
                  enum_cast<component_ref_id>(dst->id),
                  con->index_dst));
            }
        }
    }

    return status::success;
}

static status build_models(modeling& /*mod*/,
                           component& /*compo*/,
                           simulation& /*sim*/)
{
    // irt_return_if_bad(build_models_recursively(mod, c_ref, sim));
    // irt_return_if_bad(build_connections_recursively(mod, c_ref, sim));

    irt_bad_return(status::success);
}

static status modeling_fill_file_component(
  modeling&                    mod,
  component&                   compo,
  const std::filesystem::path& path) noexcept
{
    try {
        std::ifstream ifs{ path };
        irt_return_if_fail(ifs.is_open(), status::io_file_source_full);

        reader r{ ifs };
        if (auto ret = r(mod, compo, mod.srcs); is_bad(ret)) {
            // log line_error, column error mode
            irt_bad_return(status::io_file_source_full);
        }
    } catch (const std::exception& /*e*/) {
        irt_bad_return(status::io_file_source_full);
    }

    try {
        auto desc_file = path;
        desc_file.replace_extension(".desc");

        if (std::ifstream ifs{ desc_file }; ifs) {
            auto& desc = mod.descriptions.alloc();
            if (ifs.read(desc.data.begin(), desc.data.capacity())) {
                compo.desc = mod.descriptions.get_id(desc);
            } else {
                mod.descriptions.free(desc);
            }
        }
    } catch (const std::exception& /*e*/) {
        irt_bad_return(status::io_file_source_full);
    }

    return status::success;
}

static status modeling_fill_file_component(
  modeling&                    mod,
  dir_path&                    d_path,
  const std::filesystem::path& path) noexcept
{
    namespace fs = std::filesystem;

    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator{ path, ec }) {
            if (entry.is_regular_file() && entry.path().extension() == ".irt") {
                if (mod.components.can_alloc()) {
                    auto& compo = mod.components.alloc();
                    compo.name.assign(entry.path().filename().string().c_str());
                    compo.dir  = mod.dir_paths.get_id(d_path);
                    compo.type = component_type::file;

                    auto ret =
                      modeling_fill_file_component(mod, compo, entry.path());

                    if (is_bad(ret)) {
                        mod.free(compo);
                    } else {
                        auto file =
                          std::filesystem::relative(entry.path(), path, ec);

                        if (ec) {
                            mod.free(compo);
                        } else {
                            auto& file_path = mod.file_paths.alloc();
                            file_path.path  = file.string().c_str();
                            compo.file      = mod.file_paths.get_id(file_path);
                        }
                    }
                }
            }
        }
    }

    return status::success;
}

static bool check(const modeling_initializer& params) noexcept
{
    return params.model_capacity > 0 && params.component_ref_capacity > 0 &&
           params.description_capacity > 0 && params.component_capacity > 0 &&
           params.observer_capacity > 0 && params.dir_path_capacity > 0 &&
           params.file_path_capacity > 0 && params.children_capacity > 0 &&
           params.connection_capacity > 0 &&
           params.constant_source_capacity > 0 &&
           params.binary_file_source_capacity > 0 &&
           params.text_file_source_capacity > 0 &&
           params.random_source_capacity > 0;
}

status modeling::init(const modeling_initializer& params) noexcept
{
    // In the future, these allocations will have to be replaced by an
    // allocator who can exit if the allocation fails.

    irt_return_if_fail(check(params), status::gui_not_enough_memory);

    irt_return_if_bad(models.init(params.model_capacity));
    irt_return_if_bad(component_refs.init(params.component_ref_capacity));
    irt_return_if_bad(descriptions.init(params.description_capacity));
    irt_return_if_bad(components.init(params.component_capacity));
    irt_return_if_bad(observers.init(params.observer_capacity));
    irt_return_if_bad(dir_paths.init(params.dir_path_capacity));
    irt_return_if_bad(file_paths.init(params.file_path_capacity));
    irt_return_if_bad(children.init(params.children_capacity));
    irt_return_if_bad(connections.init(params.connection_capacity));

    irt_return_if_bad(
      srcs.constant_sources.init(params.constant_source_capacity));
    irt_return_if_bad(
      srcs.binary_file_sources.init(params.binary_file_source_capacity));
    irt_return_if_bad(
      srcs.text_file_sources.init(params.text_file_source_capacity));
    irt_return_if_bad(srcs.random_sources.init(params.random_source_capacity));

    return status::success;
}

status modeling::fill_internal_components() noexcept
{
    irt_return_if_fail(components.can_alloc(15), status::success);

    {
        auto& c  = components.alloc();
        c.name   = "QSS1 lotka volterra";
        c.type   = component_type::qss1_lotka_volterra;
        c.status = component_status::read_only;
        if (auto ret = add_lotka_volterra<1>(*this, c); is_bad(ret))
            return ret;
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS2 lotka volterra";
        c.type   = component_type::qss2_lotka_volterra;
        c.status = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<2>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS3 lotka volterra";
        c.type   = component_type::qss2_lotka_volterra;
        c.status = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<3>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS1 lif";
        c.type   = component_type::qss1_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_lif<1>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS2 lif";
        c.type   = component_type::qss2_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_lif<2>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS3 lif";
        c.type   = component_type::qss3_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_lif<3>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS1 izhikevich";
        c.type   = component_type::qss1_izhikevich;
        c.status = component_status::read_only;
        irt_return_if_bad(add_izhikevich<1>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS2 izhikevich";
        c.type   = component_type::qss2_izhikevich;
        c.status = component_status::read_only;
        irt_return_if_bad(add_izhikevich<2>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS3 izhikevich";
        c.type   = component_type::qss3_izhikevich;
        c.status = component_status::read_only;
        irt_return_if_bad(add_izhikevich<3>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS1 van der pol";
        c.type   = component_type::qss1_van_der_pol;
        c.status = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<1>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS2 van der pol";
        c.type   = component_type::qss2_van_der_pol;
        c.status = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<2>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS3 van der pol";
        c.type   = component_type::qss3_van_der_pol;
        c.status = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<3>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS1 negative lif";
        c.type   = component_type::qss1_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_negative_lif<1>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS2 negative lif";
        c.type   = component_type::qss2_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_negative_lif<2>(*this, c));
    }

    {
        auto& c  = components.alloc();
        c.name   = "QSS3 negative lif";
        c.type   = component_type::qss3_lif;
        c.status = component_status::read_only;
        irt_return_if_bad(add_negative_lif<3>(*this, c));
    }

    return status::success;
}

status modeling::fill_components() noexcept
{
    dir_path* path = nullptr;
    while (dir_paths.next(path))
        fill_components(*path);

    return status::success;
}

status modeling::fill_components(dir_path& path) noexcept
{
    try {
        std::filesystem::path p(path.path.c_str());
        std::error_code       ec;

        if (std::filesystem::exists(p, ec)) {
            irt_return_if_bad(modeling_fill_file_component(*this, path, p));
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
    irt_return_if_fail(connections.can_alloc(1),
                       status::data_array_not_enough_memory);

    auto& c = connections.alloc(src, port_src, dst, port_dst);
    parent.connections.emplace_back(connections.get_id(c));

    return status::success;
}

static component* find_file_component(modeling&   mod,
                                      const char* file_path) noexcept
{
    component* compo = nullptr;
    while (mod.components.next(compo)) {
        if ((compo->type == component_type::file) && compo->name == file_path)
            return compo;
    }

    return nullptr;
}

static component* find_cpp_component(modeling&      mod,
                                     component_type type) noexcept
{
    component* compo = nullptr;
    while (mod.components.next(compo)) {
        if (compo->type == type)
            return compo;
    }

    irt_unreachable();
}

status add_file_component_ref(const char*    file_path,
                              modeling&      mod,
                              component&     parent,
                              component_ref& compo_ref) noexcept
{
    irt_return_if_fail(parent.children.can_alloc(),
                       status::io_file_format_error);

    auto* file_compo = find_file_component(mod, file_path);
    compo_ref.id     = mod.components.get_id(*file_compo);

    auto& child = mod.children.alloc(mod.component_refs.get_id(compo_ref));
    parent.children.emplace_back(mod.children.get_id(child));

    return status::success;
}

status add_cpp_component_ref(const char*    buffer,
                             modeling&      mod,
                             component&     parent,
                             component_ref& compo_ref) noexcept
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
        { "qss3_van_der_pol", component_type::qss3_van_der_pol }
    };

    auto it = std::lower_bound(std::begin(tab),
                               std::end(tab),
                               buffer,
                               [](const auto& entry, const char* buffer) {
                                   return 0 == std::strcmp(entry.name, buffer);
                               });
    if (it == std::end(tab) || std::strcmp(it->name, buffer))
        return status::io_file_format_error;

    irt_return_if_fail(parent.children.can_alloc(),
                       status::io_file_format_error);

    auto* cpp_compo = find_cpp_component(mod, it->type);
    compo_ref.id    = mod.components.get_id(*cpp_compo);

    auto& child = mod.children.alloc(mod.component_refs.get_id(compo_ref));
    parent.children.emplace_back(mod.children.get_id(child));

    return status::success;
}

status build_simulation(modeling& mod, simulation& sim) noexcept
{
    if (auto* c_ref = mod.component_refs.try_to_get(mod.head); c_ref)
        if (auto* compo = mod.components.try_to_get(c_ref->id); compo)
            return build_models(mod, *compo, sim);

    irt_bad_return(status::success);
}

static void free_child(
  data_array<child, child_id>&                 children,
  data_array<model, model_id>&                 models,
  data_array<component_ref, component_ref_id>& component_refs,
  child&                                       c) noexcept
{
    if (c.type == child_type::component) {
        auto id = enum_cast<component_ref_id>(c.id);
        if (auto* c_ref = component_refs.try_to_get(id); c_ref)
            component_refs.free(*c_ref);
    } else {
        auto id = enum_cast<model_id>(c.id);
        if (auto* mdl = models.try_to_get(id); mdl)
            models.free(*mdl);
    }

    children.free(c);
}

static void free_connection(data_array<connection, connection_id>& connections,
                            connection&                            con) noexcept
{
    connections.free(con);
}

void modeling::free(component& c) noexcept
{
    for (int i = 0, e = c.children.ssize(); i != e; ++i)
        if (auto* child = children.try_to_get(c.children[i]); child)
            free_child(children, models, component_refs, *child);
    c.children.clear();

    for (int i = 0, e = c.connections.ssize(); i != e; ++i)
        if (auto* cnt = connections.try_to_get(c.connections[i]); cnt)
            free_connection(connections, *cnt);
    c.connections.clear();

    if (auto* desc = descriptions.try_to_get(c.desc); desc)
        descriptions.free(*desc);

    if (auto* path = dir_paths.try_to_get(c.dir); path)
        dir_paths.free(*path);

    if (auto* path = file_paths.try_to_get(c.file); path)
        file_paths.free(*path);

    components.free(c);
}

void modeling::free(component& parent, child& c) noexcept
{
    auto child_id = children.get_id(c);
    if (auto index = parent.children.find(child_id);
        index < parent.children.ssize()) {
        parent.children.swap_pop_back(index);
    }

    free_child(children, models, component_refs, c);
}

void modeling::free(component& parent, connection& c) noexcept
{
    auto con_id = connections.get_id(c);
    if (auto index = parent.connections.find(con_id);
        index < parent.connections.ssize()) {
        parent.connections.swap_pop_back(index);
    }

    free_connection(connections, c);
}

status modeling::copy(component& src, component& dst) noexcept
{
    table<child_id, child_id> mapping;

    for (i32 i = 0, e = src.children.ssize(); i != e; ++i) {
        if (auto* child = children.try_to_get(src.children[i])) {
            if (child->type == child_type::model) {
                auto id = enum_cast<model_id>(child->id);
                if (auto* mdl = models.try_to_get(id); mdl) {
                    irt_return_if_fail(models.can_alloc(1),
                                       status::data_array_not_enough_memory);

                    auto& new_child    = alloc(dst, mdl->type);
                    auto  new_child_id = children.get_id(new_child);
                    new_child.x        = child->x;
                    new_child.y        = child->y;

                    mapping.data.emplace_back(src.children[i], new_child_id);
                }
            } else {
                auto id = enum_cast<component_ref_id>(child->id);
                if (auto* c_ref = component_refs.try_to_get(id); c_ref) {
                    auto compo_id = enum_cast<component_id>(c_ref->id);
                    if (auto* compo = components.try_to_get(compo_id); compo) {
                        irt_return_if_fail(
                          component_refs.can_alloc(1),
                          status::data_array_not_enough_memory);

                        auto& new_c_ref = component_refs.alloc();
                        new_c_ref.id    = c_ref->id;

                        auto& new_child =
                          children.alloc(component_refs.get_id(new_c_ref));
                        auto new_child_id = children.get_id(new_child);
                        new_child.name    = child->name;
                        dst.children.emplace_back(children.get_id(new_child));

                        mapping.data.emplace_back(src.children[i],
                                                  new_child_id);
                    }
                }
            }
        }
    }

    mapping.sort();

    for (i32 i = 0, e = src.connections.ssize(); i != e; ++i) {
        if (auto* con = connections.try_to_get(src.connections[i]); con) {
            if (auto* child_src = mapping.get(con->src); child_src) {
                if (auto* child_dst = mapping.get(con->dst); child_dst) {
                    irt_return_if_fail(connections.can_alloc(1),
                                       status::data_array_not_enough_memory);

                    auto& new_con = connections.alloc(
                      *child_src, con->index_src, *child_dst, con->index_dst);
                    auto new_con_id = connections.get_id(new_con);

                    dst.connections.emplace_back(new_con_id);
                }
            }
        }
    }

    return status::success;
}

static void make_tree_recursive(
  data_array<child, child_id>&                 children,
  data_array<component, component_id>&         components,
  data_array<component_ref, component_ref_id>& component_refs,
  component_ref&                               parent,
  component_ref_id                             child) noexcept
{
    if (auto* c_ref = component_refs.try_to_get(child); c_ref) {
        c_ref->tree.set_id(c_ref);
        c_ref->tree.parent_to(parent.tree);

        if (auto* compo = components.try_to_get(parent.id); compo) {
            for (i32 i = 0, e = compo->children.ssize(); i != e; ++i) {
                auto child_id = compo->children[i];
                if (auto* child = children.try_to_get(child_id); child) {
                    if (child->type == child_type::component) {
                        make_tree_recursive(
                          children,
                          components,
                          component_refs,
                          *c_ref,
                          enum_cast<component_ref_id>(child->id));
                    }
                }
            }
        }
    }
}

void modeling::make_tree_from(component_ref& parent) noexcept
{
    parent.tree.set_id(&parent);

    if (auto* compo = components.try_to_get(parent.id); compo) {
        for (i32 i = 0, e = compo->children.ssize(); i != e; ++i) {
            auto child_id = compo->children[i];
            if (auto* child = children.try_to_get(child_id); child) {
                if (child->type == child_type::component) {
                    make_tree_recursive(children,
                                        components,
                                        component_refs,
                                        parent,
                                        enum_cast<component_ref_id>(child->id));
                }
            }
        }
    }
}

status modeling::clean(component& c) noexcept
{
    int  i              = 0;
    bool need_more_loop = false;

    do {
        need_more_loop = false;

        while (i < c.children.ssize()) {
            auto  child_id = c.children[i];
            auto* child    = children.try_to_get(child_id);

            if (!child) {
                c.children.swap_pop_back(i);
                continue;
            }

            if (child->type == child_type::model) {
                auto id = enum_cast<model_id>(child->id);
                if (auto* model = models.try_to_get(id); !model) {
                    children.free(*child);
                    c.children.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_ref_id>(child->id);
                if (auto* compo = component_refs.try_to_get(id); !compo) {
                    children.free(*child);
                    c.children.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            }

            ++i;
        }

        while (i < c.connections.ssize()) {
            auto  con_id = c.connections[i];
            auto* con    = connections.try_to_get(con_id);

            if (!con) {
                c.connections.swap_pop_back(i);
                continue;
            }

            auto* src = children.try_to_get(con->src);
            auto* dst = children.try_to_get(con->dst);
            if (!src || !dst) {
                connections.free(*con);
                c.connections.swap_pop_back(i);
                need_more_loop = true;
                continue;
            }

            if (src->type == child_type::model) {
                auto id = enum_cast<model_id>(src->id);
                if (auto* model = models.try_to_get(id); !model) {
                    children.free(*src);
                    connections.free(*con);
                    c.connections.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_ref_id>(src->id);
                if (auto* compo = component_refs.try_to_get(id); !compo) {
                    children.free(*src);
                    connections.free(*con);
                    c.connections.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            }

            if (dst->type == child_type::model) {
                auto id = enum_cast<model_id>(dst->id);
                if (auto* model = models.try_to_get(id); !model) {
                    children.free(*dst);
                    connections.free(*con);
                    c.connections.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_ref_id>(dst->id);
                if (auto* compo = component_refs.try_to_get(id); !compo) {
                    children.free(*dst);
                    connections.free(*con);
                    c.connections.swap_pop_back(i);
                    need_more_loop = true;
                    continue;
                }
            }
        }
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

    return status::success;
}

} // namespace irt
