// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP
#define ORG_VLEPROJECT_IRRITATOR_EXTENSION_HPP

#include <irritator/core.hpp>

#include <utility>

namespace irt {

template<typename T>
class hierarchy
{
private:
    hierarchy* m_parent  = nullptr;
    hierarchy* m_sibling = nullptr;
    hierarchy* m_child   = nullptr;
    T          m_owner   = undefined<T>();

public:
    hierarchy() noexcept = default;
    ~hierarchy() noexcept;

    void set_owner(T object) noexcept;
    T    owner() const noexcept;

    void parent_to(hierarchy& node) noexcept;
    void make_sibling_after(hierarchy& node) noexcept;
    bool parented_by(const hierarchy& node) const noexcept;
    void remove_from_parent() noexcept;
    void remove_from_hierarchy() noexcept;

    T get_parent() const noexcept;
    T get_child() const noexcept;
    T get_sibling() const noexcept;
    T get_prior_sibling() const noexcept;
    T get_next() const noexcept;
    T get_next_leaf() const noexcept;

private:
    hierarchy<T>* get_prior_sibling_node() const noexcept;
};

template<typename T>
inline hierarchy<T>::~hierarchy() noexcept
{
    remove_from_hierarchy();
}

template<typename T>
T
hierarchy<T>::owner() const noexcept
{
    return m_owner;
}

template<typename T>
void
hierarchy<T>::set_owner(T id) noexcept
{
    m_owner = id;
}

template<typename T>
bool
hierarchy<T>::parented_by(const hierarchy& node) const noexcept
{
    if (m_parent == &node)
        return true;

    if (m_parent)
        return m_parent->parented_by(node);

    return false;
}

template<typename T>
void
hierarchy<T>::parent_to(hierarchy& node) noexcept
{
    remove_from_parent();

    m_parent   = &node;
    m_sibling  = node.m_child;
    node.m_child = this;
}

template<typename T>
void
hierarchy<T>::make_sibling_after(hierarchy& node) noexcept
{
    remove_from_parent();

    m_parent       = node.m_parent;
    m_sibling      = node.m_sibling;
    node.m_sibling = this;
}

template<typename T>
void
hierarchy<T>::remove_from_parent() noexcept
{

    if (m_parent) {
        hierarchy<T>* prev = get_prior_sibling_node();

        if (prev) {
            prev->m_sibling = m_sibling;
        } else {
            m_parent->m_child = m_sibling;
        }
    }

    m_parent  = nullptr;
    m_sibling = nullptr;
}

template<typename T>
void
hierarchy<T>::remove_from_hierarchy() noexcept
{
    hierarchy<T>* parent_node = m_parent;

    parent_node = m_parent;
    remove_from_parent();

    if (parent_node) {
        while (m_child) {
            hierarchy<T>* node = m_child;
            node->remove_from_parent();
            node->parent_to(*parent_node);
        }
    } else {
        while (m_child)
            m_child->remove_from_parent();
    }
}

template<typename T>
T
hierarchy<T>::get_parent() const noexcept
{
    return m_parent ? m_parent->m_owner : undefined<T>();
}

template<typename T>
T
hierarchy<T>::get_child() const noexcept
{
    return m_child ? m_child->m_owner : undefined<T>();
}

template<typename T>
T
hierarchy<T>::get_sibling() const noexcept
{
    return m_sibling ? m_sibling->m_owner : undefined<T>();
}

template<typename T>
hierarchy<T>*
hierarchy<T>::get_prior_sibling_node() const noexcept
{
    if (!m_parent || (m_parent->m_child == this))
        return nullptr;

    hierarchy<T>* prev;
    hierarchy<T>* node;

    node = m_parent->m_child;
    prev = nullptr;
    while ((node != this) && (node != nullptr)) {
        prev = node;
        node = node->m_sibling;
    }

    irt_assert(node == this &&
               "could not find node in parent's list of children");

    return prev;
}

template<typename T>
T
hierarchy<T>::get_prior_sibling() const noexcept
{
    hierarchy<T>* prior = get_prior_sibling_node();

    return prior ? prior->m_owner : undefined<T>();
}

template<typename T>
T
hierarchy<T>::get_next() const noexcept
{
    if (m_child)
        return m_child->m_owner;

    const hierarchy<T>* node = this;
    while (node && node->m_sibling == nullptr)
        node = node->m_parent;

    return node ? node->m_sibling->m_owner : undefined<T>();
}

template<typename T>
T
hierarchy<T>::get_next_leaf() const noexcept
{
    if (m_child) {
        const hierarchy<T>* node = m_child;
        while (node->m_child)
            node = node->m_child;

        return node->m_owner;
    }

    const hierarchy<T>* node = this;
    while (node && node->m_sibling == nullptr)
        node = node->m_parent;

    if (node) {
        node = node->m_sibling;
        while (node->m_child) {
            node = node->m_child;
        }
        return node->m_owner;
    }

    return nullptr;
}

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
  [[maybe_unused]] none&                                      none_mdl,
  [[maybe_unused]] const data_array<component, component_id>& components,
  [[maybe_unused]] const data_array<model, model_id>&         component_models,
  [[maybe_unused]] simulation&                                sim)
{
    // irt_return_if_bad(detail::flat_merge_create_models(
    //   none_mdl, components, component_models, sim));

    // irt_return_if_bad(detail::flat_merge_create_connections(
    //   none_mdl, components, component_models, sim));

    return status::success;
}

} // namespace irt

#endif
