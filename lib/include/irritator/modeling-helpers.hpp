// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include <fmt/format.h>

namespace irt {

inline result<registred_path&> get_reg(modeling&         mod,
                                       registred_path_id id) noexcept
{
    if (auto* reg = mod.registred_paths.try_to_get(id); reg)
        return *reg;

    return new_error(project::error::registred_path_access_error);
}

inline result<dir_path&> get_dir(modeling& mod, dir_path_id id) noexcept
{
    if (auto* dir = mod.dir_paths.try_to_get(id); dir)
        return *dir;

    return new_error(project::error::directory_access_error);
}

inline result<file_path&> get_file(modeling& mod, file_path_id id) noexcept
{
    if (auto* f = mod.file_paths.try_to_get(id); f)
        return *f;

    return new_error(project::error::file_access_error);
}

template<typename Fn>
inline std::optional<file> open_file(
  dir_path&                                   dir_p,
  file_path&                                  file_p,
  std::invocable<Fn, file::error_code> auto&& fn) noexcept
{
    try {
        std::filesystem::path p = dir_p.path.u8sv();
        p /= file_p.path.u8sv();

        std::u8string u8str = p.u8string();
        const char*   cstr  = reinterpret_cast<const char*>(u8str.c_str());

        return file::open(cstr, open_mode::read, fn);
    } catch (...) {
        fn(file::error_code::memory_error);
    }

    return std::nullopt;
}

/// Checks the type of \c component pointed by the \c tree_node \c.
///
/// \return true in the underlying \c component in the \c tree_node \c is a
/// graph or a grid otherwise return false. If the component does not exists
/// this function returns false.
inline bool component_is_grid_or_graph(const modeling&  mod,
                                       const tree_node& tn) noexcept
{
    if (const auto* compo = mod.components.try_to_get(tn.id); compo)
        return any_equal(
          compo->type, component_type::graph, component_type::grid);

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

/// \brief Call `f` function if `id` reference a generic component.
///
/// \details `f` is called if `id` reference an existing `generic_component`
///   and do nothing otherwise. Function `f` receives constant `component` and
///   constant `generic_component`.
template<typename Function>
void if_component_is_generic(const modeling&    mod,
                             const component_id id,
                             Function&&         f) noexcept
{
    if (const auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::simple) {
            if (const auto* gen =
                  mod.generic_components.try_to_get(compo->id.generic_id);
                gen)
                f(*compo, *gen);
        }
    }
}

/// \brief Call `f` function if `id` reference a generic component.
///
/// \details `f` is called if `id` reference an existing `generic_component` and
/// do nothing otherwise. Function `f` receives  `component` and
/// `generic_component`.
template<typename Function>
void if_component_is_generic(modeling&          mod,
                             const component_id id,
                             Function&&         f) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::simple) {
            if (auto* gen =
                  mod.generic_components.try_to_get(compo->id.generic_id);
                gen)
                f(*compo, *gen);
        }
    }
}

/// \brief Call `f` function if `id` reference a grid component.
///
/// \details `f` is called if `id` reference an existing `grid_component` and
/// do nothing otherwise. Function `f` receives constant `component` and
/// constant `grid_component`.
template<typename Function>
void if_component_is_grid(const modeling&    mod,
                          const component_id id,
                          Function&&         f) noexcept
{
    if (const auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::grid) {
            if (const auto* gen =
                  mod.grid_components.try_to_get(compo->id.grid_id);
                gen)
                f(*compo, *gen);
        }
    }
}

/// \brief Call `f` function if `id` reference a grid component.
///
/// \details `f` is called if `id` reference an existing `grid_component` and
/// do nothing otherwise. Function `f` receives  `component` and
/// `grid_component`.
template<typename Function>
void if_component_is_grid(modeling&          mod,
                          const component_id id,
                          Function&&         f) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::grid) {
            if (auto* gen = mod.grid_components.try_to_get(compo->id.grid_id);
                gen)
                f(*compo, *gen);
        }
    }
}

/// \brief Call `f` function if `id` reference a graph component.
///
/// \details `f` is called if `id` reference an existing `graph_component` and
/// do nothing otherwise. Function `f` receives constant `component` and
/// constant `graph_component`.
template<typename Function>
void if_component_is_graph(const modeling&    mod,
                           const component_id id,
                           Function&&         f) noexcept
{
    if (const auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::graph) {
            if (const auto* gen =
                  mod.graph_components.try_to_get(compo->id.graph_id);
                gen)
                f(*compo, *gen);
        }
    }
}

/// \brief Call `f` function if `id` reference a graph component.
///
/// \details `f` is called if `id` reference an existing `graph_component` and
/// do nothing otherwise. Function `f` receives  `component` and
/// `graph_component`.
template<typename Function>
void if_component_is_graph(modeling&          mod,
                           const component_id id,
                           Function&&         f) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        if (compo->type == component_type::graph) {
            if (auto* gen = mod.graph_components.try_to_get(compo->id.graph_id);
                gen)
                f(*compo, *gen);
        }
    }
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
        unreachable();
    }
}

template<typename Function>
void for_each_child(modeling& mod, tree_node& tn, Function&& f) noexcept
{
    if_data_exists_do(mod.components, tn.id, [&](auto& compo) noexcept {
        for_each_child(mod, compo, f);
    });
}

//! \brief if child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f a invokable with no const \c child and \c
//! component references.
template<typename function>
void if_child_is_component_do(modeling&                    mod,
                              data_array<child, child_id>& data,
                              child_id                     id,
                              function&&                   f) noexcept
{
    if_data_exists_do(data, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(mod.components,
                              child.id.compo_id,
                              [&](component& compo) { f(child, compo); });
        }
    });
}

//! \brief if child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f a invokable with no const \c child and \c
//! component references.
template<typename function>
void if_child_is_model_do(modeling&                    mod,
                          data_array<child, child_id>& data,
                          child_id                     id,
                          function&&                   f) noexcept
{
    if_data_exists_do(data, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(mod.components,
                              child.id.compo_id,
                              [&](component& compo) { f(child, compo); });
        }
    });
}

//! Call \c f for each model found into \c tree_node::nodes_v table.
template<typename Function>
void for_each_model(simulation& sim, tree_node& tn, Function&& f) noexcept
{
    for (int i = 0, e = tn.unique_id_to_model_id.data.ssize(); i < e; ++i) {
        if_data_exists_do(
          sim.models, tn.unique_id_to_model_id.data[i].value, [&](auto& mdl) {
              f(tn.unique_id_to_model_id.data[i].id, mdl);
          });
    }
}

template<typename Function>
void if_tree_node_is_grid_do(project&     pj,
                             modeling&    mod,
                             tree_node_id tn_id,
                             Function&&   f) noexcept
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
                              Function&&   f) noexcept
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
