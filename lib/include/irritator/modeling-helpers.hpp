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

constexpr inline auto split(const std::string_view str,
                            const char             delim = '_') noexcept
  -> std::pair<std::string_view, std::string_view>
{
    if (const auto pos = str.find_last_of(delim);
        pos != std::string_view::npos) {
        if (std::cmp_less(pos + 1, str.size())) {
            return std::make_pair(str.substr(0, pos),
                                  str.substr(pos + 1, std::string_view::npos));
        } else {
            return std::make_pair(str.substr(0, pos), std::string_view{});
        }
    } else {
        return std::make_pair(str, std::string_view{});
    }
}

static_assert(split("m_0", '_').first == std::string_view("m"));
static_assert(split("m_0", '_').second == std::string_view("0"));
static_assert(split("m_123", '_').first == std::string_view("m"));
static_assert(split("m_123", '_').second == std::string_view("123"));
static_assert(split("m", '_').first == std::string_view("m"));
static_assert(split("m", '_').second == std::string_view{});
static_assert(split("m_", '_').first == std::string_view("m"));
static_assert(split("m_", '_').second == std::string_view{});

inline file_path_id get_file_from_component(const modeling&        mod,
                                            const component&       compo,
                                            const std::string_view str) noexcept
{
    auto ret = undefined<file_path_id>();

    if (const auto* dir = mod.dir_paths.try_to_get(compo.dir); dir) {
        ret = dir->children.read(
          [&](const auto& vec,
              const auto /*version*/) noexcept -> file_path_id {
              for (const auto f_id : vec)
                  if (const auto* f = mod.file_paths.try_to_get(f_id); f)
                      if (f->path.sv() == str)
                          return f_id;

              return undefined<file_path_id>();
          });
    }

    return ret;
}

inline std::optional<std::filesystem::path> make_file(
  const registred_path& r,
  const dir_path&       d,
  const file_path&      f) noexcept
{
    try {
        std::filesystem::path ret(r.path.sv());
        ret /= d.path.sv();
        ret /= f.path.sv();
        return ret;
    } catch (...) {
        return std::nullopt;
    }
}

inline std::optional<std::filesystem::path> make_file(
  const modeling&  mod,
  const file_path& f) noexcept
{
    if (auto* dir = mod.dir_paths.try_to_get(f.parent))
        if (auto* reg = mod.registred_paths.try_to_get(dir->parent))
            return make_file(*reg, *dir, f);

    return std::nullopt;
}

inline std::optional<std::filesystem::path> make_file(
  const modeling&    mod,
  const file_path_id f) noexcept
{
    if (auto* file = mod.file_paths.try_to_get(f))
        return make_file(mod, *file);

    return std::nullopt;
}

inline expected<file> open_file(dir_path& dir_p, file_path& file_p) noexcept
{
    try {
        std::filesystem::path p = dir_p.path.u8sv();
        p /= file_p.path.u8sv();

        std::u8string u8str = p.u8string();
        const char*   cstr  = reinterpret_cast<const char*>(u8str.c_str());

        return file::open(cstr, open_mode::read);
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

/// Checks the type of \c component pointed by the \c tree_node \c.
///
/// \return true in the underlying \c component in the \c tree_node \c is a
/// graph or a grid otherwise return false. If the component does not exists
/// this function returns false.
inline bool component_is_grid_or_graph(const modeling&  mod,
                                       const tree_node& tn) noexcept
{
    if (const auto* compo = mod.components.try_to_get<component>(tn.id))
        return any_equal(
          compo->type, component_type::graph, component_type::grid);

    return false;
}

template<typename Function>
void for_each_component(const modeling&       mod,
                        const registred_path& reg_path,
                        const dir_path&       dir_path,
                        Function&&            f) noexcept
{
    dir_path.children.read([&](const auto& vec, const auto /*vers*/) noexcept {
        for_specified_data(mod.file_paths, vec, [&](auto& file_path) noexcept {
            if (auto* c =
                  mod.components.try_to_get<component>(file_path.component))
                std::invoke(
                  std::forward<Function>(f), reg_path, dir_path, file_path, *c);
        });
    });
}

template<typename Function>
void for_each_component(modeling&       mod,
                        registred_path& reg_path,
                        dir_path&       dir_path,
                        Function&&      f) noexcept
{
    dir_path.children.write([&](auto& vec) noexcept {
        for_specified_data(
          mod.file_paths, vec, [&](const auto& file_path) noexcept {
              if (auto* c =
                    mod.components.try_to_get<component>(file_path.component))
                  std::invoke(std::forward<Function>(f),
                              reg_path,
                              dir_path,
                              file_path,
                              *c);
          });
    });
}

template<typename Function>
void for_each_component(modeling&  mod,
                        dir_path&  dir_path,
                        Function&& f) noexcept
{
    dir_path.children.write([&](auto& vec) noexcept {
        for_specified_data(mod.file_paths, vec, [&](auto& file_path) noexcept {
            if (auto* c =
                  mod.components.try_to_get<component>(file_path.component)) {
                std::invoke(std::forward<Function>(f), dir_path, file_path, *c);
            }
        });
    });
}

template<typename Function>
void for_each_component(const modeling& mod,
                        const dir_path& dir_path,
                        Function&&      f) noexcept
{
    dir_path.children.read([&](const auto& vec, const auto /*vers*/) noexcept {
        for_specified_data(
          mod.file_paths, vec, [&](const auto& file_path) noexcept {
              if (auto* c =
                    mod.components.try_to_get<component>(file_path.component)) {
                  std::invoke(
                    std::forward<Function>(f), dir_path, file_path, *c);
              }
          });
    });
}

template<typename Function>
void for_each_component(modeling&       mod,
                        registred_path& reg_path,
                        Function&&      f) noexcept
{
    reg_path.children.read([&](const auto& vec, const auto /*vers*/) noexcept {
        for_specified_data(mod.dir_paths, vec, [&](auto& dir_path) noexcept {
            return for_each_component(
              mod, reg_path, dir_path, std::forward<Function>(f));
        });
    });
}

template<typename Function>
void for_each_component(const modeling&       mod,
                        const registred_path& reg_path,
                        Function&&            f) noexcept
{
    reg_path.children.read([&](const auto& vec, const auto /*vers*/) noexcept {
        for_specified_data(
          mod.dir_paths, vec, [&](const auto& dir_path) noexcept {
              return for_each_component(
                mod, reg_path, dir_path, std::forward<Function>(f));
          });
    });
}

template<typename Function>
void for_each_component(modeling&                  mod,
                        vector<registred_path_id>& dirs,
                        Function&&                 f) noexcept
{
    for_specified_data(mod.registred_paths, dirs, [&](auto& reg_path) noexcept {
        return for_each_component(mod, reg_path, std::forward<Function>(f));
    });
}

template<typename Function>
void for_each_component(const modeling&                  mod,
                        const vector<registred_path_id>& dirs,
                        Function&&                       f) noexcept
{
    for_specified_data(
      mod.registred_paths, dirs, [&](const auto& reg_path) noexcept {
          return for_each_component(mod, reg_path, std::forward<Function>(f));
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
    if (const auto* compo = mod.components.try_to_get<component>(id)) {
        if (compo->type == component_type::generic) {
            if (const auto* gen =
                  mod.generic_components.try_to_get(compo->id.generic_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
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
    if (auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::generic) {
            if (auto* gen =
                  mod.generic_components.try_to_get(compo->id.generic_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
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
    if (const auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::grid) {
            if (const auto* gen =
                  mod.grid_components.try_to_get(compo->id.grid_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
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
    if (auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::grid) {
            if (auto* gen = mod.grid_components.try_to_get(compo->id.grid_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
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
    if (const auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::graph) {
            if (const auto* gen =
                  mod.graph_components.try_to_get(compo->id.graph_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
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
    if (auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::graph) {
            if (auto* gen = mod.graph_components.try_to_get(compo->id.graph_id);
                gen)
                std::invoke(std::forward<Function>(f), *compo, *gen);
        }
    }
}

template<typename Function>
void for_each_child(modeling& mod, component& compo, Function&& f) noexcept
{
    switch (compo.type) {
    case component_type::generic:
        if_data_exists_do(mod.generic_components,
                          compo.id.generic_id,
                          [&](auto& generic) noexcept {
                              for_each_child(
                                mod, compo, generic, std::forward<Function>(f));
                          });
        break;

    case component_type::grid:
        if_data_exists_do(
          mod.grid_components, compo.id.grid_id, [&](auto& grid) noexcept {
              for_each_child(mod, compo, grid, std::forward<Function>(f));
          });
        break;

    case component_type::graph:
        if_data_exists_do(
          mod.graph_components, compo.id.graph_id, [&](auto& graph) noexcept {
              for_each_child(mod, compo, graph, std::forward<Function>(f));
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
        for_each_child(mod, compo, std::forward<Function>(f));
    });
}

//! \brief if child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f a invokable with no const \c child and \c
//! component references.
template<typename Function>
void if_child_is_component_do(
  modeling&                                       mod,
  data_array<generic_component::child, child_id>& data,
  child_id                                        id,
  Function&&                                      f) noexcept
{
    if_data_exists_do(data, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(
              mod.components, child.id.compo_id, [&](component& compo) {
                  std::invoke(std::forward<Function>(f), child, compo);
              });
        }
    });
}

//! \brief if child exists and is a component, invoke the function \c f
//! otherwise do nothing. \param f a invokable with no const \c child and \c
//! component references.
template<typename Function>
void if_child_is_model_do(modeling&                                       mod,
                          data_array<generic_component::child, child_id>& data,
                          child_id                                        id,
                          Function&& f) noexcept
{
    if_data_exists_do(data, id, [&](auto& child) noexcept {
        if (child.type == child_type::component) {
            if_data_exists_do(
              mod.components, child.id.compo_id, [&](component& compo) {
                  std::invoke(std::forward<Function>(f), child, compo);
              });
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
              std::invoke(std::forward<Function>(f),
                          tn.unique_id_to_model_id.data[i].id.sv(),
                          mdl);
          });
    }
}

template<typename Function>
void for_each_model(const simulation& sim,
                    const tree_node&  tn,
                    Function&&        f) noexcept
{
    for (int i = 0, e = tn.unique_id_to_model_id.data.ssize(); i < e; ++i) {
        const auto mdl_id = tn.unique_id_to_model_id.data[i].value;
        if (const auto* mdl = sim.models.try_to_get(mdl_id); mdl)
            std::invoke(std::forward<Function>(f),
                        tn.unique_id_to_model_id.data[i].id.sv(),
                        *mdl);
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
        if (compo = mod.components.try_to_get<component>(grid_tn->id); compo) {
            if (compo->type == component_type::grid) {
                if (g_compo = mod.grid_components.try_to_get(compo->id.grid_id);
                    g_compo) {
                    std::invoke(
                      std::forward<Function>(f), *grid_tn, *compo, *g_compo);
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
        if (compo = mod.components.try_to_get<component>(graph_tn->id); compo) {
            if (compo->type == component_type::graph) {
                if (g_compo =
                      mod.graph_components.try_to_get(compo->id.graph_id);
                    g_compo) {
                    std::invoke(
                      std::forward<Function>(f), *graph_tn, *compo, *g_compo);
                }
            }
        }
    }
}

template<typename Function>
void dispatch_component(modeling& mod, component& compo, Function&& f) noexcept
{
    switch (compo.type) {
    case component_type::none:
        break;
    case component_type::generic:
        if_data_exists_do(mod.generic_components,
                          compo.id.generic_id,
                          std::forward<Function>(f));
        break;
    case component_type::grid:
        if_data_exists_do(
          mod.grid_components, compo.id.grid_id, std::forward<Function>(f));
        break;
    case component_type::graph:
        if_data_exists_do(
          mod.graph_components, compo.id.graph_id, std::forward<Function>(f));
        break;
    case component_type::hsm:
        if_data_exists_do(
          mod.hsm_components, compo.id.hsm_id, std::forward<Function>(f));
        break;
    }
}

} // namespace irt

#endif
