// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

namespace irt {

inline status
flat_merge_create_models(none& none_mdl,
                         const data_array<component, component_id>& components,
                         const data_array<model, model_id>& component_models,
                         simulation& sim)
{
    auto* compo = components.try_to_get(none_mdl.id);
    if (!compo)
        return status::success;

    auto& c = *compo;

    // For each none models in the component, we call recursively the
    // flat_merge_create_model function to flattend the hierarchy of models and
    // remove the none models. The new simulation models are built from leaf.

    for (const auto elem : c.children) {
        auto* from_c = component_models.try_to_get(elem);
        if (!from_c)
            continue;

        if (from_c->type != dynamics_type::none)
            continue;

        irt_return_if_bad(flat_merge_create_models(
          get_dyn<none>(*from_c), components, component_models, sim));
    }

    // For each models (none exclude) in the component, we duplicate the model
    // into the simulation and we fill the dictionary component model ->
    // simulation model to prepare connections between models.

    for (const auto elem : c.children) {
        auto* from_c = component_models.try_to_get(elem);
        if (!from_c)
            continue;

        if (from_c->type == dynamics_type::none)
            continue;

        irt_return_if_fail(sim.can_alloc(),
                           status::simulation_not_enough_model);

        auto& from_c_dyn = get_dyn<none>(*from_c);
        auto& mdl = sim.models.alloc();
        auto id = sim.models.get_id(mdl);
        mdl.type = from_c->type;
        mdl.handle = nullptr;

        // @TODO Need a new status::model_none_not_enough_memory
        irt_return_if_fail(none_mdl.dict.try_emplace_back(elem, id) != nullptr,
                           status::simulation_not_enough_model);

        sim.dispatch(
          mdl, [&sim, &from_c_dyn]<typename Dynamics>(Dynamics& dyn) -> void {
              new (&dyn) Dynamics{ dyn };

              if constexpr (std::is_same_v<Dynamics, integrator>)
                  dyn.archive.set_allocator(
                    &sim.flat_double_list_shared_allocator);
              if constexpr (std::is_same_v<Dynamics, quantifier>)
                  dyn.archive.set_allocator(
                    &sim.flat_double_list_shared_allocator);

              if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                  for (auto& x : dyn.x)
                      x.messages.set_allocator(&sim.message_list_allocator);
              }

              if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                  for (auto& y : dyn.y)
                      y.messages.set_allocator(&sim.message_list_allocator);
              }
          });
    }

    return status::success;
}

inline status
flat_merge_create_connections(
  none& none_mdl,
  const data_array<model, model_id>& component_models,
  simulation& sim)
{
    none_mdl.dict.sort();

    // For each models (none exclude) in the component, we duplicate the model
    // connection using the dictionary.

    for (const auto& elem : none_mdl.dict.elements) {
        const auto& from_c_mdl = component_models.get(elem.u);
        if (from_c_mdl.type == dynamics_type::none)
            continue;

        auto& sim_mdl = sim.models.get(elem.v);

        auto ret = sim.dispatch(
          sim_mdl,
          [&component_models, &none_mdl, &sim, &from_c_mdl]<typename Dynamics>(
            Dynamics& dyn) -> status {
              auto& from_c_dyn = get_dyn<Dynamics>(from_c_mdl);

              if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                  int port_index = 0;
                  for (auto& x : from_c_dyn.x) {
                      auto it = x.connections.begin();
                      auto et = x.connections.end();

                      while (it != et) {
                          auto* mdl = component_models.try_to_get(it->model);
                          if (!mdl)
                              continue;

                          if (mdl->type == dynamics_type::none) {
                              auto& none_dyn = get_dyn<none>(*mdl);
                              auto port = none_dyn.y.begin();
                              std::advance(port, it->port_index);

                              for (auto& cnt : port->connections) {
                                  auto* v = none_dyn.dict.find(cnt.model);
                                  irt_assert(v);

                                  auto* src = sim.models.try_to_get(*v);
                                  irt_assert(src);

                                  irt_return_if_fail(
                                    sim.can_connect(1u),
                                    status::
                                      simulation_not_enough_memory_message_list_allocator);

                                  irt_return_if_bad(sim.connect(*src,
                                                                it->port_index,
                                                                get_model(dyn),
                                                                port_index));
                              }

                          } else {
                              auto* v = none_mdl.dict.find(it->model);
                              irt_assert(v);

                              auto* src = sim.models.try_to_get(*v);
                              irt_assert(src);

                              irt_return_if_fail(
                                sim.can_connect(1u),
                                status::
                                  simulation_not_enough_memory_message_list_allocator);

                              irt_return_if_bad(sim.connect(*src,
                                                            it->port_index,
                                                            get_model(dyn),
                                                            port_index));
                          }
                      }

                      ++port_index;
                  }
              }

              if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                  int port_index = 0;
                  for (auto& y : from_c_dyn.y) {
                      auto it = y.connections.begin();
                      auto et = y.connections.end();

                      while (it != et) {
                          auto* mdl = component_models.try_to_get(it->model);
                          if (!mdl)
                              continue;

                          if (mdl->type == dynamics_type::none) {
                              auto& none_dyn = get_dyn<none>(*mdl);
                              auto port = none_dyn.x.begin();
                              std::advance(port, it->port_index);

                              for (auto& cnt : port->connections) {
                                  auto* v = none_dyn.dict.find(cnt.model);
                                  irt_assert(v);

                                  auto* dst = sim.models.try_to_get(*v);
                                  irt_assert(dst);

                                  irt_return_if_fail(
                                    sim.can_connect(1u),
                                    status::
                                      simulation_not_enough_memory_message_list_allocator);

                                  irt_return_if_bad(
                                    sim.connect(get_model(dyn),
                                                port_index,
                                                *dst,
                                                it->port_index));
                              }

                          } else {
                              auto* v = none_mdl.dict.find(it->model);
                              irt_assert(v);

                              auto* dst = sim.models.try_to_get(*v);
                              irt_assert(dst);

                              irt_return_if_fail(
                                sim.can_connect(1u),
                                status::
                                  simulation_not_enough_memory_message_list_allocator);

                              irt_return_if_bad(sim.connect(get_model(dyn),
                                                            port_index,
                                                            *dst,
                                                            it->port_index));
                          }
                      }

                      ++port_index;
                  }
              }

              return status::success;
          });

        irt_return_if_bad(ret);
    }

    return status::success;
}

inline status
flat_merge(none& none_mdl,
           const data_array<component, component_id>& components,
           const data_array<model, model_id>& component_models,
           simulation& sim)
{
    irt_return_if_bad(
      flat_merge_create_models(none_mdl, components, component_models, sim));

    irt_return_if_bad(
      flat_merge_create_connections(none_mdl, component_models, sim));

    return status::success;
}

} // namespace irt

#endif
