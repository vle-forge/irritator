// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP

#include <irritator/core.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

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
                          compo.id.simple_id,
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

template<typename Function>
void for_each_model(simulation& sim, tree_node& tn, Function&& f) noexcept
{
    for (int i = 0, e = tn.nodes_v.data.ssize(); i != e; ++i) {
        switch (tn.nodes_v.data[i].value.index()) {
        case 0:
            break;

        case 1: {
            auto& mdl_id = *std::get_if<model_id>(&tn.nodes_v.data[i].value);
            if_data_exists_do(
              sim.models, mdl_id, [&](auto& mdl) { f(sim, tn, mdl); });
        } break;

        default:
            irt_unreachable();
        }
    }
}

} // namespace irt

#endif
