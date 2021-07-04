// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <utility>

namespace irt {

namespace detail {

// inline status
// flat_merge_create_models(none& none_mdl,
//                          const data_array<component, component_id>&
//                          components, const data_array<model, model_id>&
//                          component_models, simulation& sim)
// {
//     auto* compo = components.try_to_get(none_mdl.id);
//     if (!compo)
//         return status::success;

//     auto& c = *compo;

//     // For each none models in the component, we call recursively the
//     // flat_merge_create_model function to flattend the hierarchy of models
//     and
//     // remove the none models. The new simulation models are built from leaf.

//     model* from_c = nullptr;
//     while (c.models.next(from_c)) {
//         if (from_c->type != dynamics_type::none)
//             continue;

//         irt_return_if_bad(flat_merge_create_models(
//           get_dyn<none>(*from_c), components, component_models, sim));
//     }

//     // For each models (none exclude) in the component, we duplicate the
//     model
//     // into the simulation and we fill the dictionary component model ->
//     // simulation model to prepare connections between models.

//     from_c = nullptr;
//     while (c.models.next(from_c)) {
//         if (from_c->type == dynamics_type::none)
//             continue;

//         irt_return_if_fail(sim.can_alloc(),
//                            status::simulation_not_enough_model);

//         auto& mdl = sim.models.alloc();
//         auto id = sim.models.get_id(mdl);
//         mdl.type = from_c->type;
//         mdl.handle = nullptr;

//         // @TODO Need a new status::model_none_not_enough_memory
//         irt_return_if_fail(none_mdl.dict.try_emplace_back(
//                              c.models.get_id(from_c), id) != nullptr,
//                            status::simulation_not_enough_model);

//         dispatch(mdl,
//                  [&sim, &from_c]<typename Dynamics>(Dynamics& dyn) -> void {
//                      new (&dyn) Dynamics(get_dyn<Dynamics>(*from_c));
//                  });
//     }

//     return status::success;
// }

// inline status
// copy_input_connection(const shared_flat_list<node>& connections,
//                       const map<model_id, model_id>& dict,
//                       const data_array<model, model_id>& component_models,
//                       const data_array<component, component_id>& components,
//                       simulation& sim,
//                       model& out_mdl,
//                       int out_port)
// {
//     std::vector<std::tuple<const model*, int>> stack;
//     std::vector<std::tuple<model*, int>> to_connect;

//     for (const auto& elem : connections) {
//         const auto* mdl = component_models.try_to_get(elem.model);
//         if (!mdl)
//             continue;

//         if (mdl->type == dynamics_type::none) {
//             stack.emplace_back(std::make_tuple(mdl, elem.port_index));
//         } else {
//             auto* v = dict.find(elem.model);
//             if (!v)
//                 continue;

//             auto* sim_mdl = sim.models.try_to_get(*v);
//             irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//             to_connect.emplace_back(std::make_tuple(sim_mdl,
//             elem.port_index));
//         }
//     }

//     while (!stack.empty()) {
//         const auto src_mdl = stack.back();
//         stack.pop_back();

//         const auto& src_dyn = get_dyn<none>(*std::get<0>(src_mdl));
//         const auto* compo = components.try_to_get(src_dyn.id);
//         if (!compo)
//             continue;

//         auto it = compo->internal_y.begin();
//         std::advance(it, std::get<1>(src_mdl));

//         for (auto& elem : it->connections) {
//             const auto* mdl = component_models.try_to_get(elem.model);
//             if (!mdl)
//                 continue;

//             if (mdl->type == dynamics_type::none) {
//                 stack.emplace_back(mdl, elem.port_index);
//             } else {
//                 auto* v = src_dyn.dict.find(elem.model);
//                 if (!v)
//                     continue;

//                 auto* sim_mdl = sim.models.try_to_get(*v);
//                 irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//                 to_connect.emplace_back(sim_mdl, elem.port_index);
//             }
//         }
//     }

//     for (auto& elem : to_connect)
//         sim.connect(*std::get<0>(elem), std::get<1>(elem), out_mdl,
//         out_port);
// }

// inline status
// copy_output_connection(const shared_flat_list<node>& connections,
//                        const map<model_id, model_id>& dict,
//                        const data_array<model, model_id>& component_models,
//                        const data_array<component, component_id>& components,
//                        simulation& sim,
//                        model& out_mdl,
//                        int out_port)
// {
//     std::vector<std::tuple<const model*, int>> stack;
//     std::vector<std::tuple<model*, int>> to_connect;

//     for (const auto& elem : connections) {
//         const auto* mdl = component_models.try_to_get(elem.model);
//         if (!mdl)
//             continue;

//         if (mdl->type == dynamics_type::none) {
//             stack.emplace_back(std::make_tuple(mdl, elem.port_index));
//         } else {
//             auto* v = dict.find(elem.model);
//             if (!v)
//                 continue;

//             auto* sim_mdl = sim.models.try_to_get(*v);
//             irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//             to_connect.emplace_back(std::make_tuple(sim_mdl,
//             elem.port_index));
//         }
//     }

//     while (!stack.empty()) {
//         const auto src_mdl = stack.back();
//         stack.pop_back();

//         const auto& src_dyn = get_dyn<none>(*std::get<0>(src_mdl));
//         const auto* compo = components.try_to_get(src_dyn.id);
//         if (!compo)
//             continue;

//         auto it = compo->internal_y.begin();
//         std::advance(it, std::get<1>(src_mdl));

//         for (auto& elem : it->connections) {
//             const auto* mdl = component_models.try_to_get(elem.model);
//             if (!mdl)
//                 continue;

//             if (mdl->type == dynamics_type::none) {
//                 stack.emplace_back(mdl, elem.port_index);
//             } else {
//                 auto* v = src_dyn.dict.find(elem.model);
//                 if (!v)
//                     continue;

//                 auto* sim_mdl = sim.models.try_to_get(*v);
//                 irt_return_if_fail(sim_mdl, status::unknown_dynamics);

//                 to_connect.emplace_back(sim_mdl, elem.port_index);
//             }
//         }
//     }

//     for (auto& elem : to_connect)
//         sim.connect(*std::get<0>(elem), std::get<1>(elem), out_mdl,
//         out_port);
// }

// inline status
// flat_merge_create_connections(
//   none& none_mdl,
//   const data_array<component, component_id>& components,
//   const data_array<model, model_id>& component_models,
//   simulation& sim)
// {
//     none_mdl.dict.sort();

//     // For each models (none exclude) in the component, we duplicate the
//     model
//     // connection using the dictionary.

//     for (const auto& elem : none_mdl.dict.elements) {
//         const auto& from_c_mdl = component_models.get(elem.u);
//         if (from_c_mdl.type == dynamics_type::none)
//             continue;

//         auto& sim_mdl = sim.models.get(elem.v);

//         auto ret = dispatch(
//           sim_mdl,
//           [&components,
//            &component_models,
//            &none_mdl,
//            &sim,
//            &from_c_mdl]<typename Dynamics>(Dynamics& dyn) -> status {
//               auto& from_sim = get_model(dyn);
//               auto& from_c_dyn = get_dyn<Dynamics>(from_c_mdl);

//               if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
//                   int port_index = 0;
//                   for (auto& y : from_c_dyn.y) {
//                       auto ret = copy_output_connection(y.connections,
//                                                         none_mdl.dict,
//                                                         component_models,
//                                                         components,
//                                                         sim,
//                                                         from_sim,
//                                                         port_index);
//                       irt_return_if_bad(ret);
//                       ++port_index;
//                   }
//               }

//               return status::success;
//           });

//         irt_return_if_bad(ret);
//     }

//     return status::success;
// }

} // namespace detail

inline status
flat_merge(
  [[maybe_unused]] none& none_mdl,
  [[maybe_unused]] const data_array<component, component_id>& components,
  [[maybe_unused]] const data_array<model, model_id>& component_models,
  [[maybe_unused]] simulation& sim)
{
    // irt_return_if_bad(detail::flat_merge_create_models(
    //   none_mdl, components, component_models, sim));

    // irt_return_if_bad(detail::flat_merge_create_connections(
    //   none_mdl, components, component_models, sim));

    return status::success;
}

} // namespace irt

#endif
