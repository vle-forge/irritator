// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

namespace irt {

template<int QssLevel>
status add_lotka_volterra(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5) && com.connections.can_alloc(8);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& integrator_a      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_a.default_X  = 18.0_r;
    integrator_a.default_dQ = 0.1_r;

    auto& integrator_b      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_b.default_X  = 7.0_r;
    integrator_b.default_dQ = 0.1_r;

    auto& product = mod.alloc<abstract_multiplier<QssLevel>>(com);

    auto& sum_a                   = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_a.default_input_coeffs[0] = 2.0_r;
    sum_a.default_input_coeffs[1] = -0.4_r;

    auto& sum_b                   = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_b.default_input_coeffs[0] = -1.0_r;
    sum_b.default_input_coeffs[1] = 0.1_r;

    mod.connect(com, sum_a, 0, integrator_a, 0);
    mod.connect(com, sum_b, 0, integrator_b, 0);
    mod.connect(com, integrator_a, 0, sum_a, 0);
    mod.connect(com, integrator_b, 0, sum_b, 0);
    mod.connect(com, integrator_a, 0, product, 0);
    mod.connect(com, integrator_b, 0, product, 1);
    mod.connect(com, product, 0, sum_a, 1);
    mod.connect(com, product, 0, sum_b, 1);

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5) && com.connections.can_alloc(7);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto& cst         = mod.alloc<constant>(com);
    cst.default_value = 1.0;

    auto& cst_cross         = mod.alloc<constant>(com);
    cst_cross.default_value = Vr;

    auto& sum                   = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    sum.default_input_coeffs[0] = -irt::one / tau;
    sum.default_input_coeffs[1] = V0 / tau;

    auto& integrator      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator.default_X  = 0._r;
    integrator.default_dQ = 0.001_r;

    auto& cross             = mod.alloc<abstract_cross<QssLevel>>(com);
    cross.default_threshold = Vt;

    mod.connect(com, cross, 0, integrator, 1);
    mod.connect(com, cross, 1, sum, 0);
    mod.connect(com, integrator, 0, cross, 0);
    mod.connect(com, integrator, 0, cross, 2);
    mod.connect(com, cst_cross, 0, cross, 1);
    mod.connect(com, cst, 0, sum, 1);
    mod.connect(com, sum, 0, integrator, 0);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(12) && com.connections.can_alloc(22);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& cst          = mod.alloc<constant>(com);
    auto& cst2         = mod.alloc<constant>(com);
    auto& cst3         = mod.alloc<constant>(com);
    auto& sum_a        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& sum_b        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& sum_c        = mod.alloc<abstract_wsum<QssLevel, 4>>(com);
    auto& sum_d        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& product      = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& integrator_a = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& integrator_b = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& cross        = mod.alloc<abstract_cross<QssLevel>>(com);
    auto& cross2       = mod.alloc<abstract_cross<QssLevel>>(com);

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

    mod.connect(com, integrator_a, 0, cross, 0);
    mod.connect(com, cst2, 0, cross, 1);
    mod.connect(com, integrator_a, 0, cross, 2);

    mod.connect(com, cross, 1, product, 0);
    mod.connect(com, cross, 1, product, 1);
    mod.connect(com, product, 0, sum_c, 0);
    mod.connect(com, cross, 1, sum_c, 1);
    mod.connect(com, cross, 1, sum_b, 1);

    mod.connect(com, cst, 0, sum_c, 2);
    mod.connect(com, cst3, 0, sum_c, 3);

    mod.connect(com, sum_c, 0, sum_a, 0);
    mod.connect(com, cross2, 1, sum_a, 1);
    mod.connect(com, sum_a, 0, integrator_a, 0);
    mod.connect(com, cross, 0, integrator_a, 1);

    mod.connect(com, cross2, 1, sum_b, 0);
    mod.connect(com, sum_b, 0, integrator_b, 0);

    mod.connect(com, cross2, 0, integrator_b, 1);
    mod.connect(com, integrator_a, 0, cross2, 0);
    mod.connect(com, integrator_b, 0, cross2, 2);
    mod.connect(com, sum_d, 0, cross2, 1);
    mod.connect(com, integrator_b, 0, sum_d, 0);
    mod.connect(com, cst, 0, sum_d, 1);

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5) && com.children.can_alloc(5) &&
                   com.connections.can_alloc(9);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum          = mod.alloc<abstract_wsum<QssLevel, 3>>(com);
    auto& product1     = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product2     = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& integrator_a = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& integrator_b = mod.alloc<abstract_integrator<QssLevel>>(com);

    integrator_a.default_X  = 0.0_r;
    integrator_a.default_dQ = 0.001_r;

    integrator_b.default_X  = 10.0_r;
    integrator_b.default_dQ = 0.001_r;

    constexpr double mu         = 4.0_r;
    sum.default_input_coeffs[0] = mu;
    sum.default_input_coeffs[1] = -mu;
    sum.default_input_coeffs[2] = -1.0_r;

    mod.connect(com, integrator_b, 0, integrator_a, 0);
    mod.connect(com, sum, 0, integrator_b, 0);
    mod.connect(com, integrator_b, 0, sum, 0);
    mod.connect(com, product2, 0, sum, 1);
    mod.connect(com, integrator_a, 0, sum, 2);
    mod.connect(com, integrator_b, 0, product1, 0);
    mod.connect(com, integrator_a, 0, product1, 1);
    mod.connect(com, product1, 0, product2, 0);
    mod.connect(com, integrator_a, 0, product2, 1);

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5) && com.children.can_alloc(5) &&
                   com.connections.can_alloc(7);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& integrator = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& cross      = mod.alloc<abstract_cross<QssLevel>>(com);
    auto& cst        = mod.alloc<constant>(com);
    auto& cst_cross  = mod.alloc<constant>(com);

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

    mod.connect(com, cross, 0, integrator, 1);
    mod.connect(com, cross, 1, sum, 0);
    mod.connect(com, integrator, 0, cross, 0);
    mod.connect(com, integrator, 0, cross, 2);
    mod.connect(com, cst_cross, 0, cross, 1);
    mod.connect(com, cst, 0, sum, 1);
    mod.connect(com, sum, 0, integrator, 0);

    return status::success;
}

template<int QssLevel>
status add_seir_lineaire(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(10) && com.children.can_alloc(10) &&
                   com.connections.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum_a        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& sum_b        = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    auto& product_a    = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_b    = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& integrator_a = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& integrator_b = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& integrator_c = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& integrator_d = mod.alloc<abstract_integrator<QssLevel>>(com);
    auto& constant_a   = mod.alloc<constant>(com);
    auto& constant_b   = mod.alloc<constant>(com);

    sum_a.default_input_coeffs[0] = -0.005_r;
    sum_a.default_input_coeffs[1] = -0.4_r;

    sum_b.default_input_coeffs[0] = -0.135_r;
    sum_b.default_input_coeffs[1] = 0.1_r;

    integrator_a.default_X  = 10.0_r;
    integrator_a.default_dQ = 0.01_r;

    integrator_b.default_X  = 15.0_r;
    integrator_b.default_dQ = 0.01_r;

    integrator_c.default_X  = 10.0_r;
    integrator_c.default_dQ = 0.01_r;

    integrator_d.default_X  = 18.0_r;
    integrator_d.default_dQ = 0.01_r;

    constant_a.default_value = -0.005_r;

    constant_b.default_value = -0.135_r;

    mod.connect(com, constant_a, 0, product_a, 0);
    mod.connect(com, constant_b, 0, product_b, 0);
    mod.connect(com, sum_a, 0, integrator_c, 0);
    mod.connect(com, sum_b, 0, integrator_d, 0);
    mod.connect(com, integrator_b, 0, sum_a, 0);
    mod.connect(com, integrator_c, 0, sum_a, 1);
    mod.connect(com, integrator_c, 0, sum_b, 0);
    mod.connect(com, integrator_d, 0, sum_b, 1);
    mod.connect(com, integrator_a, 0, product_a, 1);
    mod.connect(com, integrator_b, 0, product_b, 1);
    mod.connect(com, product_a, 0, sum_a, 1);
    mod.connect(com, product_b, 0, sum_b, 1);

    return status::success;
}

template<int QssLevel>
status add_seir_nonlineaire(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(27) && com.children.can_alloc(27) &&
                   com.connections.can_alloc(32);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto& sum_a                   = mod.alloc<abstract_wsum<QssLevel, 3>>(com);
    sum_a.default_input_coeffs[0] = 0.5_r;
    sum_a.default_input_coeffs[1] = 1.0_r;
    sum_a.default_input_coeffs[2] = 1.0_r;

    auto& sum_b                   = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_b.default_input_coeffs[0] = 1.0_r;
    sum_b.default_input_coeffs[1] = 1.0_r;

    auto& sum_c                   = mod.alloc<abstract_wsum<QssLevel, 3>>(com);
    sum_c.default_input_coeffs[0] = 1.5_r;
    sum_c.default_input_coeffs[1] = 0.698_r;
    sum_c.default_input_coeffs[2] = 0.387_r;

    auto& sum_d                   = mod.alloc<abstract_wsum<QssLevel, 2>>(com);
    sum_d.default_input_coeffs[0] = 1.0_r;
    sum_d.default_input_coeffs[1] = 1.5_r;

    auto& product_a = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_b = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_c = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_d = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_e = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_f = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_g = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_h = mod.alloc<abstract_multiplier<QssLevel>>(com);
    auto& product_i = mod.alloc<abstract_multiplier<QssLevel>>(com);

    auto& integrator_a      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_a.default_X  = 10.0_r;
    integrator_a.default_dQ = 0.01_r;

    auto& integrator_b      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_b.default_X  = 12.0_r;
    integrator_b.default_dQ = 0.01_r;

    auto& integrator_c      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_c.default_X  = 13.50_r;
    integrator_c.default_dQ = 0.01_r;

    auto& integrator_d      = mod.alloc<abstract_integrator<QssLevel>>(com);
    integrator_d.default_X  = 15.0_r;
    integrator_d.default_dQ = 0.01_r;

    // The values used here are from Singh et al., 2017

    auto& constant_a         = mod.alloc<constant>(com);
    constant_a.default_value = 0.005_r;

    auto& constant_b         = mod.alloc<constant>(com);
    constant_b.default_value = -0.0057_r;

    auto& constant_c         = mod.alloc<constant>(com);
    constant_c.default_value = -0.005_r;

    auto& constant_d         = mod.alloc<constant>(com);
    constant_d.default_value = 0.0057_r;

    auto& constant_e         = mod.alloc<constant>(com);
    constant_e.default_value = -0.135_r;

    auto& constant_f         = mod.alloc<constant>(com);
    constant_f.default_value = 0.135_r;

    auto& constant_g         = mod.alloc<constant>(com);
    constant_g.default_value = -0.072_r;

    auto& constant_h         = mod.alloc<constant>(com);
    constant_h.default_value = 0.005_r;

    auto& constant_i         = mod.alloc<constant>(com);
    constant_i.default_value = 0.067_r;

    auto& constant_j         = mod.alloc<constant>(com);
    constant_j.default_value = -0.005_r;

    mod.connect(com, constant_a, 0, sum_a, 0);
    mod.connect(com, constant_h, 0, sum_c, 2);
    mod.connect(com, constant_b, 0, product_a, 0);
    mod.connect(com, constant_c, 0, product_b, 0);
    mod.connect(com, constant_d, 0, product_c, 0);
    mod.connect(com, constant_e, 0, product_d, 0);
    mod.connect(com, constant_f, 0, product_e, 0);
    mod.connect(com, constant_g, 0, product_f, 0);
    mod.connect(com, constant_h, 0, product_g, 0);
    mod.connect(com, constant_i, 0, product_h, 0);
    mod.connect(com, product_i, 0, product_a, 1);
    mod.connect(com, product_i, 0, product_c, 1);
    mod.connect(com, sum_a, 0, integrator_a, 0);
    mod.connect(com, sum_b, 0, integrator_b, 0);
    mod.connect(com, sum_c, 0, integrator_c, 0);
    mod.connect(com, sum_d, 0, integrator_d, 0);
    mod.connect(com, product_a, 0, sum_a, 1);
    mod.connect(com, product_b, 0, sum_a, 2);
    mod.connect(com, product_c, 0, sum_b, 0);
    mod.connect(com, product_d, 0, sum_b, 1);
    mod.connect(com, product_e, 0, sum_c, 0);
    mod.connect(com, product_f, 0, sum_c, 1);
    mod.connect(com, product_g, 0, sum_d, 0);
    mod.connect(com, product_h, 0, sum_d, 1);
    mod.connect(com, integrator_a, 0, product_b, 1);
    mod.connect(com, integrator_b, 0, product_d, 1);
    mod.connect(com, integrator_b, 0, product_e, 1);
    mod.connect(com, integrator_c, 0, product_f, 1);
    mod.connect(com, integrator_c, 0, product_g, 1);
    mod.connect(com, integrator_d, 0, product_h, 1);
    mod.connect(com, integrator_a, 0, product_i, 0);
    mod.connect(com, integrator_c, 0, product_i, 1);

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
        u64 id = comp->children[i].id;

        if (comp->children[i].type == child_type::model) {
            auto  src_id = enum_cast<model_id>(id);
            auto* src    = mod.models.try_to_get(src_id);
            if (!src)
                continue;

            irt_return_if_fail(sim.models.can_alloc(1),
                               status::simulation_not_enough_model);

            auto& dst    = sim.clone(*src);
            auto  dst_id = sim.models.get_id(dst);

            irt_return_if_bad(
              comp_ref.mappers.data.try_emplace_back(src_id, dst_id));
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
                                              component&     comp,
                                              simulation&    sim,
                                              int            i)
{
    irt_assert(comp.connections[i].type_src == child_type::model);
    irt_assert(comp.connections[i].type_dst == child_type::model);
    irt_assert(i >= 0 && i < comp.connections.ssize());

    auto  src_id     = enum_cast<model_id>(comp.connections[i].src);
    auto  dst_id     = enum_cast<model_id>(comp.connections[i].dst);
    auto* src_mdl_id = comp_ref.mappers.get(src_id);
    auto* dst_mdl_id = comp_ref.mappers.get(dst_id);

    if (!src_mdl_id || !dst_mdl_id)
        return status::success;

    auto* src = sim.models.try_to_get(*src_mdl_id);
    auto* dst = sim.models.try_to_get(*dst_mdl_id);

    if (!src || !dst)
        return status::success;

    return global_connect(sim,
                          *src,
                          comp.connections[i].port_src,
                          *dst_mdl_id,
                          comp.connections[i].port_dst);
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

        if (c->x[port].type == child_type::model) {
            auto  id        = enum_cast<model_id>(c->x[port].id);
            auto* mapped_id = c_ref->mappers.get(id);

            if (mapped_id) {
                model_found = *mapped_id;
                port_found  = c->x[port].index;
                return true;
            } else {
                return false;
            }
        }

        compo_ref = enum_cast<component_ref_id>(c->x[port].id);
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

        if (c->y[port].type == child_type::model) {
            auto  id        = enum_cast<model_id>(c->y[port].id);
            auto* mapped_id = c_ref->mappers.get(id);

            if (mapped_id) {
                model_found = *mapped_id;
                port_found  = c->y[port].index;
                return true;
            } else {
                return false;
            }
        }

        compo_ref = enum_cast<component_ref_id>(c->y[port].id);
        port      = c->y[port].index;
    }
}

static status build_model_to_component_connection(
  const modeling&      mod,
  const component_ref& compo_ref,
  component&           compo,
  simulation&          sim,
  int                  index)
{
    irt_assert(index >= 0 && index < compo.connections.ssize());
    irt_assert(compo.connections[index].type_src == child_type::model);
    irt_assert(compo.connections[index].type_dst == child_type::component);

    model_id model_found;
    i8       port_found;

    bool found = found_input_port(
      mod,
      enum_cast<component_ref_id>(compo.connections[index].dst),
      compo.connections[index].port_dst,
      model_found,
      port_found);

    if (found) {
        auto  id         = enum_cast<model_id>(compo.connections[index].src);
        auto* src_mdl_id = compo_ref.mappers.get(id);

        if (src_mdl_id) {
            auto* src_mdl = sim.models.try_to_get(*src_mdl_id);

            if (src_mdl) {
                auto src_port = compo.connections[index].port_src;
                return global_connect(
                  sim, *src_mdl, src_port, model_found, port_found);
            }
        }
    }

    return status::success; // @todo fail
}

static status build_component_to_model_connection(
  const modeling&      mod,
  const component_ref& compo_ref,
  component&           compo,
  simulation&          sim,
  int                  index)
{
    irt_assert(index >= 0 && index < compo.connections.ssize());
    irt_assert(compo.connections[index].type_src == child_type::component);
    irt_assert(compo.connections[index].type_dst == child_type::model);

    model_id model_found;
    i8       port_found;

    bool found = found_output_port(
      mod,
      enum_cast<component_ref_id>(compo.connections[index].src),
      compo.connections[index].port_src,
      model_found,
      port_found);

    if (found) {
        auto  id         = enum_cast<model_id>(compo.connections[index].dst);
        auto* dst_mdl_id = compo_ref.mappers.get(id);

        if (dst_mdl_id) {
            auto* src_mdl  = sim.models.try_to_get(model_found);
            auto  dst_port = compo.connections[index].port_dst;

            return global_connect(
              sim, *src_mdl, port_found, *dst_mdl_id, dst_port);
        }
    }

    return status::success; // @todo fail
}

static status build_component_to_component_connection(
  const modeling&      mod,
  const component_ref& compo_ref,
  component&           compo,
  simulation&          sim,
  int                  index)
{
    irt_assert(index >= 0 && index < compo.connections.ssize());
    irt_assert(compo.connections[index].type_src == child_type::component);
    irt_assert(compo.connections[index].type_dst == child_type::component);

    model_id src_model_found, dst_model_found;
    i8       src_port_found, dst_port_found;

    bool found_src = found_output_port(
      mod,
      enum_cast<component_ref_id>(compo.connections[index].src),
      compo.connections[index].port_src,
      src_model_found,
      src_port_found);

    bool found_dst = found_input_port(
      mod,
      enum_cast<component_ref_id>(compo.connections[index].dst),
      compo.connections[index].port_dst,
      dst_model_found,
      dst_port_found);

    if (found_src && found_dst) {
        auto* src_mdl_id = compo_ref.mappers.get(src_model_found);
        auto* dst_mdl_id = compo_ref.mappers.get(dst_model_found);

        if (src_mdl_id && dst_mdl_id) {
            auto* src_mdl = sim.models.try_to_get(*src_mdl_id);
            auto* dst_mdl = sim.models.try_to_get(*dst_mdl_id);

            if (src_mdl && dst_mdl)
                return global_connect(
                  sim, *src_mdl, src_port_found, *dst_mdl_id, dst_port_found);
        }
    }

    return status::success; // @todo fail
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
        if (compo->children[i].type == child_type::component) {
            u64   id       = compo->children[i].id;
            auto  child_id = enum_cast<component_ref_id>(id);
            auto* child    = mod.component_refs.try_to_get(child_id);
            if (!child)
                continue;

            build_connections_recursively(mod, *child, sim);
        }
    }

    for (i32 i = 0, e = compo->connections.ssize(); i != e; ++i) {
        if (compo->connections[i].type_src == child_type::model) {
            if (compo->connections[i].type_dst == child_type::model) {
                irt_return_if_bad(
                  build_model_to_model_connection(c_ref, *compo, sim, i));
            } else {
                irt_return_if_bad(build_model_to_component_connection(
                  mod, c_ref, *compo, sim, i));
            }
        } else {
            if (compo->connections[i].type_dst == child_type::model) {
                irt_return_if_bad(build_component_to_model_connection(
                  mod, c_ref, *compo, sim, i));
            } else {
                irt_return_if_bad(build_component_to_component_connection(
                  mod, c_ref, *compo, sim, i));
            }
        }
    }

    return status::success;
}

static status build_models(modeling& mod, component_ref& c_ref, simulation& sim)
{
    irt_return_if_bad(build_models_recursively(mod, c_ref, sim));
    irt_return_if_bad(build_connections_recursively(mod, c_ref, sim));

    return status::success;
}

static status modeling_fill_cpp_component(modeling& mod) noexcept
{
    irt_return_if_fail(mod.components.can_alloc(15), status::success);

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS1 lotka volterra");
        if (auto ret = add_lotka_volterra<1>(mod, c); is_bad(ret))
            return ret;
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS2 lotka volterra");
        irt_return_if_bad(add_lotka_volterra<2>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS3 lotka volterra");
        irt_return_if_bad(add_lotka_volterra<3>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS1 lif");
        irt_return_if_bad(add_lif<1>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS2 lif");
        irt_return_if_bad(add_lif<2>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS3 lif");
        irt_return_if_bad(add_lif<3>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS1 izhikevich");
        irt_return_if_bad(add_izhikevich<1>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS2 izhikevich");
        irt_return_if_bad(add_izhikevich<2>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS3 izhikevich");
        irt_return_if_bad(add_izhikevich<3>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS1 van der pol");
        irt_return_if_bad(add_van_der_pol<1>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS2 van der pol");
        irt_return_if_bad(add_van_der_pol<2>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS3 van der pol");
        irt_return_if_bad(add_van_der_pol<3>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS1 negative lif");
        irt_return_if_bad(add_negative_lif<1>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS2 negative lif");
        irt_return_if_bad(add_negative_lif<2>(mod, c));
    }

    {
        auto& c = mod.components.alloc();
        c.name.assign("QSS3 negative lif");
        irt_return_if_bad(add_negative_lif<3>(mod, c));
    }

    return status::success;
}

static status modeling_fill_file_component(
  modeling&                    mod,
  component&                   compo,
  const std::filesystem::path& path) noexcept
{
    (void)mod;
    (void)compo;

    std::ifstream ifs{ path };
    irt_return_if_fail(ifs.is_open(), status::io_file_source_full);

    reader r{ ifs };
    // if (auto ret = r(mod, compo, mod.srcs); is_bad(ret)) {
    //    // log line_error, column error mode
    //    irt_bad_return(status::io_file_source_full);
    //}

    return status::success;
}

static status modeling_fill_file_component(
  modeling&                    mod,
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

                    auto ret =
                      modeling_fill_file_component(mod, compo, entry.path());
                    if (is_bad(ret)) {
                        // @todo log
                        mod.components.free(compo);
                    }
                }
            }
        }
    }

    return status::success;
}

inline status modeling::init() noexcept
{
    irt_return_if_bad(models.init(1024));
    irt_return_if_bad(component_refs.init(256));
    irt_return_if_bad(descriptions.init(256));
    irt_return_if_bad(components.init(256));
    irt_return_if_bad(dir_paths.init(16));
    irt_return_if_bad(file_paths.init(256));

    //    irt::external_source srcs;

    return status::success;
}

status modeling::fill_component() noexcept
{
    irt_return_if_bad(modeling_fill_cpp_component(*this));

    try {
        std::error_code ec;
        auto            p = std::filesystem::current_path(ec);

        if (ec) {
            irt_return_if_bad(modeling_fill_file_component(*this, p));
        }
    } catch (...) {
    }

    return status::success;
}

status modeling::connect(component& parent,
                         i32        src,
                         i8         port_src,
                         i32        dst,
                         i8         port_dst) noexcept
{
    irt_return_if_fail(0 <= src && src < parent.children.ssize(),
                       status::model_connect_bad_dynamics);
    irt_return_if_fail(0 <= dst && dst < parent.children.ssize(),
                       status::model_connect_bad_dynamics);
    irt_assert(!parent.connections.full());

    auto src_child = parent.children[src];
    auto dst_child = parent.children[dst];

    model_id real_src_id, real_dst_id;
    i8       real_src_port, real_dst_port;

    if (src_child.type == child_type::component) {
        irt_return_if_fail(
          found_output_port(*this,
                            enum_cast<component_ref_id>(src_child.id),
                            port_src,
                            real_src_id,
                            real_src_port),
          status::model_connect_bad_dynamics);
    } else {
        real_src_id   = enum_cast<model_id>(src_child.id);
        real_src_port = port_src;
    }

    if (dst_child.type == child_type::component) {
        irt_return_if_fail(
          found_input_port(*this,
                           enum_cast<component_ref_id>(dst_child.id),
                           port_dst,
                           real_dst_id,
                           real_dst_port),
          status::model_connect_bad_dynamics);
    } else {
        real_dst_id   = enum_cast<model_id>(dst_child.id);
        real_dst_port = port_dst;
    }

    if (auto* mdl_src = models.try_to_get(real_src_id); mdl_src) {
        if (auto* mdl_dst = models.try_to_get(real_dst_id); mdl_dst) {
            irt_return_if_fail(
              is_ports_compatible(
                *mdl_src, real_src_port, *mdl_dst, real_dst_port),
              status::model_connect_bad_dynamics);

            parent.connections.emplace_back(
              parent.children[src], port_src, parent.children[dst], port_dst);

            return status::success;
        }
    }

    return status::model_connect_bad_dynamics;
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

    parent.children.emplace_back(mod.component_refs.get_id(compo_ref));

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

    parent.children.emplace_back(mod.component_refs.get_id(compo_ref));

    return status::success;
}

status build_simulation(modeling& mod, simulation& sim) noexcept
{
    if (auto* c_ref = mod.component_refs.try_to_get(mod.head); c_ref)
        return build_models(mod, *c_ref, sim);

    return status::success;
}

} // namespace irt
