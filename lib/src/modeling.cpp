// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling.hpp>

namespace irt {

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

status build_simulation(modeling& mod, simulation& sim)
{
    if (auto* c_ref = mod.component_refs.try_to_get(mod.head); c_ref)
        return build_models(mod, *c_ref, sim);

    return status::success;
}

} // namespace irt
