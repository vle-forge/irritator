// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling.hpp>

namespace irt {

static status
build_models_recursively(const modeling&      mod,
                         const component_ref& comp_ref,
                         simulation&          sim)
{
    comp_ref.mappers.clear();

    auto* comp = mod.components.try_to_get(comp_ref.id);
    if (!comp)
        return status::success; /* @TODO certainly an error in API */

    for (i64 i = 0, e = comp->children.size(); i != e; ++i) {
        u64 id = comp->children[i].id;

        if (comp->children[i].type == child_type::model) {
            auto  src_id = enum_cast<model_id>(id);
            auto* src    = mod.models.try_to_get(src_id);
            if (!src)
                continue;

            irt_return_if_fail(sim.models.can_alloc(1),
                               status::simulation_not_enough_model);

            auto& dst    = sim.models.clone(*src);
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

static status
build_simulation_connection(const component& src,
                            model_id         mdl_src,
                            i8               port_src,
                            const component& dst,
                            model_id         mdl_dst,
                            i8               port_dst,
                            simulation&      sim)
{
    auto* src_map_id = src.mappers.get(mdl_src);
    auto* dst_map_id = dst.mappers.get(mdl_dst);

    irt_return_if_fail(src_map_id && dst_map_id,
                       status::model_connect_unknown_dynamics);

    auto* src_sim = sim.models.try_to_get(*src_map_id);
    auto* dst_sim = sim.models.try_to_get(*dst_map_id);

    irt_return_if_fail(src_sim && dst_sim,
                       status::model_connect_unknown_dynamics);

    return sim.connect(*src_sim, port_src, *dst_sim, port_dst);
}

static status
build_model_to_model_connection(const modeling&      mod,
                                const component_ref& comp_ref,
                                simulation&          sim,
                                int                  i)
{
    irt_assert(comp_ref.connections[i].type_src == child_type::model);
    irt_assert(comp_ref.connections[i].type_dst == child_type::model);
    irt_assert(i >= 0 && i < comp_ref.connections.size());

    auto src_id = enum_cast<model_id>(comp_ref.connections[i].src);
    auto dst_id = enum_cast<model_id>(comp_ref.connections[i].dst);

    return build_simulation_connection(comp_ref,
                                       src_id,
                                       comp.connections[i].port_src,
                                       comp_ref,
                                       dst_id,
                                       comp.connections[i].port_dst,
                                       sim);
}

static status
build_model_to_component_connection(const modeling&      mod,
                                    const component_ref& compo_ref,
                                    simulation&          sim,
                                    int                  index)
{
    irt_assert(comp_ref.connections[i].type_src == child_type::model);
    irt_assert(comp_ref.connections[i].type_dst == child_type::component);
    irt_assert(i >= 0 && i < comp_ref.connections.size());

    auto* compo = m.components.try_to_get(compo_ref.id);
    if (!compo)
        return status::success; // @todo certainly an error

    if (comp.connections[index].port_dst >= length(compo->x))
        return status::success; // @todo certainly an error

    if (comp.connections[index].type == child_type::model) {
        // classic connection between compo_ref
    } else {
    }
}

static status
build_connections_recursively(const modeling&      mod,
                              const component_ref& compo_ref,
                              simulation&          sim)
{
    auto* compo = m.components.try_to_get(compo_ref.id);
    if (!compo)
        return status::success; // @todo certainly an error

    for (i64 i = 0, e = compo->children.size(); i != e; ++i) {
        if (compo->children[i].type == child_type::component) {
            u64   id             = compo->children[i].id;
            auto  child_c_ref_id = enum_cast<component_ref_id>(id);
            auto* child_c_ref = mod.component_refs.try_to_get(child_c_ref_id);
            if (!child_c_ref)
                continue;

            build_connections_recursively(mod, *child_c_ref, sim);
        }
    }

    for (i64 i = 0, e = compo->connections.size(); i != e; ++i) {
        auto src = compo->connections[i].src;
        auto dst = compo->connections[i].dst;

        if (compo->connections[i].type_src == child_type::model) {
            if (compo->connections[i].type_dst == child_type::model) {
                irt_return_if_bad(
                  build_model_to_model_connection(mod, comp, sim, i));
            } else {
                irt_return_if_bad(
                  build_model_to_component_connection(mod, comp, sim, i));
            }
        } else {
            if (compo->connections[i].type_dst == child_type::model) {
                irt_return_if_bad(
                  buid_component_to_model_connection(mod, comp, sim, i));
            } else {
                irt_return_if_bad(
                  buid_component_to_component_connection(mod, comp, sim, i));
            }
        }
    }

    return status::success;
}

static status
build_models(const modeling& mod, const component_ref& comp, simulation& sim)
{
    irt_return_if_bad(build_models_recursively(mod, component_ref, sim));
    irt_return_if_bad(build_connections_recursively(mod, component_ref, sim));
}

status
build_simulation(const modeling& mod, simulation& sim)
{
    auto* component_ref = mod.component_refs.try_to_get(mod.head);
    if (!component_refs)
        return status::success;

    return build_models_from_component(mod, *component_ref, sim);
}

} // namespace irt
