// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include <fmt/format.h>

namespace irt {

/// Checks the type of @c component pointed by the @c tree_node @c.
///
/// @return true in the underlying @c component in the @c tree_node @c is a
/// graph or a grid otherwise return false. If the component does not exists
/// this function returns false.
inline bool component_is_grid_or_graph(const modeling&  mod,
                                       const tree_node& tn) noexcept
{
    if (const auto* compo = mod.components.try_to_get(tn.id); compo)
        return match(compo->type, component_type::graph, component_type::grid);

    return false;
}

template<typename Function>
void for_each_component(modeling&       mod,
                        registred_path& reg_path,
                        dir_path&       dir_path,
                        Function&&      f) noexcept
{
    for_specified_data(
      mod.file_paths, dir_path.children, [&](auto& file_path) noexcept {
          if_data_exists_do(
            mod.components, file_path.component, [&](auto& compo) noexcept {
                f(reg_path, dir_path, file_path, compo);
            });
      });
}

template<typename Function>
void for_each_component(modeling&  mod,
                        dir_path&  dir_path,
                        Function&& f) noexcept
{
    for_specified_data(
      mod.file_paths, dir_path.children, [&](auto& file_path) noexcept {
          if_data_exists_do(
            mod.components, file_path.component, [&](auto& compo) noexcept {
                f(dir_path, file_path, compo);
            });
      });
}

template<typename Function>
void for_each_component(modeling&       mod,
                        registred_path& reg_path,
                        Function&&      f) noexcept
{
    for_specified_data(
      mod.dir_paths, reg_path.children, [&](auto& dir_path) noexcept {
          return for_each_component(mod, reg_path, dir_path, f);
      });
}

template<typename Function>
void for_each_component(modeling&                  mod,
                        vector<registred_path_id>& dirs,
                        Function&&                 f) noexcept
{
    for_specified_data(mod.registred_paths, dirs, [&](auto& reg_path) noexcept {
        return for_each_component(mod, reg_path, f);
    });
}

template<typename Function>
void for_each_child(modeling&          mod,
                    component&         compo,
                    generic_component& generic,
                    Function&&         f) noexcept
{
    for_each_data(mod.children, generic.children, [&](auto& child) noexcept {
        f(compo, generic, child);
    });
}

template<typename Function>
void for_each_child(modeling&       mod,
                    component&      compo,
                    grid_component& grid,
                    Function&&      f) noexcept
{
    for_each_data(mod.children, grid.cache, [&](auto& child) noexcept {
        f(compo, grid, child);
    });
}

template<typename Function>
void for_each_child(modeling&        mod,
                    component&       compo,
                    graph_component& graph,
                    Function&&       f) noexcept
{
    for_each_data(mod.children, graph.children, [&](auto& child) noexcept {
        f(compo, graph, child);
    });
}

template<typename Function>
void for_each_child(modeling& mod, component& compo, Function&& f) noexcept
{
    switch (compo.type) {
    case component_type::simple:
        if_data_exists_do(mod.generic_components,
                          compo.id.generic_id,
                          [&](auto& generic) noexcept {
                              for_each_child(mod, compo, generic, f);
                          });
        break;

    case component_type::internal:
        break;

    case component_type::grid:
        if_data_exists_do(
          mod.grid_components, compo.id.grid_id, [&](auto& grid) noexcept {
              for_each_child(mod, compo, grid, f);
          });
        break;

    case component_type::graph:
        if_data_exists_do(
          mod.graph_components, compo.id.graph_id, [&](auto& graph) noexcept {
              for_each_child(mod, compo, graph, f);
          });
        break;

    default:
        irt_unreachable();
    }
}

template<typename Function>
void for_each_child(modeling& mod, tree_node& tn, Function&& f) noexcept
{
    if_data_exists_do(mod.components, tn.id, [&](auto& compo) noexcept {
        for_each_child(mod, compo, f);
    });
}

//! \brief If child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f A invokable with no const \c child and \c
//! component references.
template<typename Function>
void if_child_is_component_do(modeling& mod, child_id id, Function&& f) noexcept
{
    if_data_exists_do(mod.children, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(mod.components,
                              child.id.compo_id,
                              [&](component& compo) { f(child, compo); });
        }
    });
}

//! \brief If child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f A invokable with no const \c child and \c
//! component references.
template<typename Function>
void if_child_is_model_do(modeling& mod, child_id id, Function&& f) noexcept
{
    if_data_exists_do(mod.children, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(mod.components,
                              child.id.compo_id,
                              [&](component& compo) { f(child, compo); });
        }
    });
}

//! Call @c f for each model found into @c tree_node::nodes_v table.
template<typename Function>
void for_each_model(simulation& sim, tree_node& tn, Function&& f) noexcept
{
    for (int i = 0, e = tn.nodes_v.data.ssize(); i < e; ++i) {
        switch (tn.nodes_v.data[i].value.index()) {
        case 0:
            break;

        case 1: {
            auto& mdl_id = *std::get_if<model_id>(&tn.nodes_v.data[i].value);
            if_data_exists_do(sim.models, mdl_id, [&](auto& mdl) {
                f(sim, tn, tn.nodes_v.data[i].id, mdl);
            });
        } break;

        default:
            irt_unreachable();
        }
    }
}

template<typename Function>
void if_tree_node_is_grid_do(project&     pj,
                             modeling&    mod,
                             tree_node_id tn_id,
                             Function&&   f)
{
    tree_node*      grid_tn{};
    component*      compo{};
    grid_component* g_compo{};

    if (grid_tn = pj.tree_nodes.try_to_get(tn_id); grid_tn) {
        if (compo = mod.components.try_to_get(grid_tn->id); compo) {
            if (compo->type == component_type::grid) {
                if (g_compo = mod.grid_components.try_to_get(compo->id.grid_id);
                    g_compo) {
                    f(*grid_tn, *compo, *g_compo);
                }
            }
        }
    }
}

template<typename Function>
void if_tree_node_is_graph_do(project&     pj,
                              modeling&    mod,
                              tree_node_id tn_id,
                              Function&&   f)
{
    tree_node*       graph_tn{};
    component*       compo{};
    graph_component* g_compo{};

    if (graph_tn = pj.tree_nodes.try_to_get(tn_id); graph_tn) {
        if (compo = mod.components.try_to_get(graph_tn->id); compo) {
            if (compo->type == component_type::graph) {
                if (g_compo =
                      mod.graph_components.try_to_get(compo->id.graph_id);
                    g_compo) {
                    f(*graph_tn, *compo, *g_compo);
                }
            }
        }
    }
}

} // namespace irt

#endif
