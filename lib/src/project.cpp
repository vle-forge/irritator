// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <optional>
#include <utility>

#include <cstdint>

#include "parameter.hpp"

namespace irt {

struct simulation_copy {
    simulation_copy(project::cache&                      cache_,
                    modeling&                            mod_,
                    simulation&                          sim_,
                    data_array<tree_node, tree_node_id>& tree_nodes_) noexcept
      : cache(cache_)
      , mod(mod_)
      , sim(sim_)
      , tree_nodes(tree_nodes_)
    {}

    project::cache& cache;
    modeling&       mod;
    simulation&     sim;

    data_array<tree_node, tree_node_id>& tree_nodes;
    table<hsm_component_id, hsm_id>      hsm_mod_to_sim;
};

static status simulation_copy_source(simulation_copy& sc,
                                     const u64        id,
                                     const u64        type,
                                     source&          dst) noexcept;

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       parent,
                                  component&       compo,
                                  child_id         id_in_parent,
                                  u64              unique_id) noexcept;

static bool get_parent(modeling&   mod,
                       tree_node&  node,
                       tree_node*& parent,
                       component*& parent_compo) noexcept
{
    if (auto* p = node.tree.get_parent(); p) {
        if (auto* c = mod.components.try_to_get(p->id); c) {
            parent       = p;
            parent_compo = c;

            return true;
        }

        return true;
    }

    return false;
}

bool get_graph_connections(const modeling&                 mod,
                           const component&                compo,
                           std::span<const connection_id>& out) noexcept
{
    irt_assert(compo.type == component_type::graph);

    return if_data_exists_do(
      mod.graph_components,
      compo.id.graph_id,
      [&](auto& graph) noexcept -> bool {
          out = std::span<const connection_id>(graph.cache_connections.data(),
                                               graph.cache_connections.ssize());
          return true;
      },
      +[]() noexcept -> bool { return false; });
}

bool get_grid_connections(const modeling&                 mod,
                          const component&                compo,
                          std::span<const connection_id>& out) noexcept
{
    irt_assert(compo.type == component_type::grid);

    if (auto* ptr = mod.grid_components.try_to_get(compo.id.grid_id); ptr) {
        out = std::span<const connection_id>(ptr->cache_connections.data(),
                                             ptr->cache_connections.size());
        return true;
    }

    return false;
}

bool get_generic_connections(const modeling&                 mod,
                             const component&                compo,
                             std::span<const connection_id>& out) noexcept
{
    irt_assert(compo.type == component_type::simple);

    if (auto* ptr = mod.generic_components.try_to_get(compo.id.generic_id);
        ptr) {
        out = std::span<const connection_id>(ptr->connections.data(),
                                             ptr->connections.size());
        return true;
    }

    return false;
}

bool get_connections(const modeling&                 mod,
                     const component&                compo,
                     std::span<const connection_id>& out) noexcept
{
    switch (compo.type) {
    case component_type::grid:
        return get_grid_connections(mod, compo, out);

    case component_type::graph:
        return get_graph_connections(mod, compo, out);

    case component_type::internal:
        return false;

    case component_type::none:
        return false;

    case component_type::simple:
        return get_generic_connections(mod, compo, out);

    case component_type::hsm:
        return false;
    }

    unreachable();
}

static bool count_inputs_connections(const modeling&                       mod,
                                     const std::span<const connection_id>& cnts,
                                     const child_id child,
                                     int&           nb) noexcept
{
    for (const auto c_id : cnts) {
        if (auto* c = mod.connections.try_to_get(c_id); c) {
            if (c->type == connection::connection_type::internal) {
                if (c->internal.dst == child)
                    ++nb;
            } else if (c->type == connection::connection_type::input) {
                if (c->input.dst == child)
                    ++nb;
            }
        }
    }

    return true;
}

static bool count_inputs_connections(const modeling&                       mod,
                                     const std::span<const connection_id>& cnts,
                                     const child_id child,
                                     const port_id  port,
                                     int&           nb) noexcept
{
    for (const auto c_id : cnts) {
        if (auto* c = mod.connections.try_to_get(c_id); c) {
            if (c->type == connection::connection_type::internal) {
                if (c->internal.dst == child &&
                    c->internal.index_dst.compo == port)
                    ++nb;
            } else if (c->type == connection::connection_type::input) {
                if (c->input.dst == child && c->input.index_dst.compo == port)
                    ++nb;
            }
        }
    }

    return true;
}

static bool count_outputs_connections(
  const modeling&                       mod,
  const std::span<const connection_id>& cnts,
  const child_id                        child,
  int&                                  nb) noexcept
{
    for (const auto c_id : cnts) {
        if (auto* c = mod.connections.try_to_get(c_id); c) {
            if (c->type == connection::connection_type::internal) {
                if (c->internal.src == child)
                    ++nb;
            } else if (c->type == connection::connection_type::output) {
                if (c->output.src == child)
                    ++nb;
            }
        }
    }

    return true;
}

static bool count_outputs_connections(
  const modeling&                       mod,
  const std::span<const connection_id>& cnts,
  const child_id                        child,
  const port_id                         port,
  int&                                  nb) noexcept
{
    for (const auto c_id : cnts) {
        if (auto* c = mod.connections.try_to_get(c_id); c) {
            if (c->type == connection::connection_type::internal) {
                if (c->internal.src == child &&
                    c->internal.index_src.compo == port)
                    ++nb;
            } else if (c->type == connection::connection_type::output) {
                if (c->output.src == child && c->output.index_src.compo == port)
                    ++nb;
            }
        }
    }

    return true;
}

static bool search_child(const tree_node& child,
                         const tree_node& parent,
                         child_id&        id) noexcept
{
    for (int i = 0, e = parent.child_to_node.ssize(); i != e; ++i) {
        if (parent.child_to_node.data[i].value.tn == &child) {
            id = parent.child_to_node.data[i].id;
            return true;
        }
    }

    return false;
}

static int compute_incoming_component(modeling& mod, tree_node& node) noexcept

{
    tree_node*                     parent = nullptr;
    component*                     compo  = nullptr;
    std::span<const connection_id> vector;
    child_id                       id = undefined<child_id>();
    int                            nb = 0;

    return get_parent(mod, node, parent, compo) &&
               search_child(node, *parent, id) &&
               get_connections(mod, *compo, vector) &&
               count_inputs_connections(mod, vector, id, nb)
             ? nb
             : 0;
}

static int compute_outcoming_component(modeling& mod, tree_node& node) noexcept

{
    tree_node*                     parent = nullptr;
    component*                     compo  = nullptr;
    std::span<const connection_id> vector;
    child_id                       id = undefined<child_id>();
    int                            nb = 0;

    return get_parent(mod, node, parent, compo) &&
               search_child(node, *parent, id) &&
               get_connections(mod, *compo, vector) &&
               count_outputs_connections(mod, vector, id, nb)
             ? nb
             : 0;
}

static int compute_incoming_component(modeling&  mod,
                                      tree_node& node,
                                      port_id    port) noexcept
{
    tree_node*                     parent = nullptr;
    component*                     compo  = nullptr;
    std::span<const connection_id> vector;
    child_id                       id = undefined<child_id>();
    int                            nb = 0;

    return get_parent(mod, node, parent, compo) &&
               search_child(node, *parent, id) &&
               get_connections(mod, *compo, vector) &&
               count_inputs_connections(mod, vector, id, port, nb)
             ? nb
             : 0;
}

static int compute_outcoming_component(modeling&  mod,
                                       tree_node& node,
                                       port_id    port) noexcept
{
    tree_node*                     parent = nullptr;
    component*                     compo  = nullptr;
    std::span<const connection_id> vector;
    child_id                       id = undefined<child_id>();
    int                            nb = 0;

    return get_parent(mod, node, parent, compo) &&
               search_child(node, *parent, id) &&
               get_connections(mod, *compo, vector) &&
               count_outputs_connections(mod, vector, id, port, nb)
             ? nb
             : 0;
}

static status make_tree_leaf(simulation_copy& sc,
                             tree_node&       parent,
                             u64              unique_id,
                             dynamics_type    mdl_type,
                             child_id         ch_id,
                             child&           ch) noexcept
{
    if (!sc.sim.models.can_alloc())
        return new_error(project::error::not_enough_memory);

    if (mdl_type == dynamics_type::hsm_wrapper)
        if (!sc.sim.hsms.can_alloc())
            return new_error(project::error::not_enough_memory);

    const auto ch_idx     = get_index(ch_id);
    auto&      new_mdl    = sc.sim.models.alloc();
    auto       new_mdl_id = sc.sim.models.get_id(new_mdl);

    new_mdl.type   = mdl_type;
    new_mdl.handle = invalid_heap_handle;

    auto ret =
      dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> status {
          std::construct_at(&dyn);

          if constexpr (has_input_port<Dynamics>)
              for (int i = 0, e = length(dyn.x); i != e; ++i)
                  dyn.x[i] = undefined<message_id>();

          if constexpr (has_output_port<Dynamics>)
              for (int i = 0, e = length(dyn.y); i != e; ++i)
                  dyn.y[i] = undefined<node_id>();

          sc.mod.children_parameters[ch_idx].copy_to(new_mdl);

          if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
              const auto child_index = get_index(ch_id);
              const auto mod_hsm_id  = enum_cast<hsm_component_id>(
                sc.mod.children_parameters[child_index].integers[0]);

              const auto* sim_hsm_id = sc.hsm_mod_to_sim.get(mod_hsm_id);
              if (sim_hsm_id) {
                  const auto* hsm = sc.sim.hsms.try_to_get(*sim_hsm_id);
                  irt_assert(hsm);
                  dyn.id = *sim_hsm_id;
              }
          }

          if constexpr (std::is_same_v<Dynamics, generator>) {
              using enum generator_parameter_indices;

              irt_check(simulation_copy_source(
                sc,
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_id)],
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_type)],
                dyn.default_source_ta));

              irt_check(simulation_copy_source(
                sc,
                sc.mod.children_parameters[ch_idx].integers[ordinal(value_id)],
                sc.mod.children_parameters[ch_idx]
                  .integers[ordinal(value_type)],
                dyn.default_source_value));
          }

          if constexpr (std::is_same_v<Dynamics, dynamic_queue>) {
              using enum dynamic_queue_parameter_indices;

              irt_check(simulation_copy_source(
                sc,
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_id)],
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_type)],
                dyn.default_source_ta));
          }

          if constexpr (std::is_same_v<Dynamics, priority_queue>) {
              using enum priority_queue_parameter_indices;

              irt_check(simulation_copy_source(
                sc,
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_id)],
                sc.mod.children_parameters[ch_idx].integers[ordinal(ta_type)],
                dyn.default_source_ta));
          }

          if constexpr (std::is_same_v<Dynamics, constant>) {
              switch (dyn.type) {
              case constant::init_type::constant:
                  break;
              case constant::init_type::incoming_component_all:
                  dyn.default_value =
                    compute_incoming_component(sc.mod, parent);
                  break;
              case constant::init_type::outcoming_component_all:
                  dyn.default_value =
                    compute_outcoming_component(sc.mod, parent);
                  break;
              case constant::init_type::incoming_component_n: {
                  const auto id          = enum_cast<port_id>(dyn.port);
                  auto*      port_id_ptr = sc.mod.ports.try_to_get(id);

                  if (port_id_ptr)
                      dyn.default_value =
                        compute_incoming_component(sc.mod, parent, id);
              } break;
              case constant::init_type::outcoming_component_n: {
                  const auto id          = enum_cast<port_id>(dyn.port);
                  auto*      port_id_ptr = sc.mod.ports.try_to_get(id);

                  if (port_id_ptr)
                      dyn.default_value =
                        compute_outcoming_component(sc.mod, parent, id);
                  break;
              }
              }
          }

          return success();
      });

    if (!ret)
        return ret.error();

    {
        auto& x     = parent.child_to_node.data.emplace_back();
        x.id        = sc.mod.children.get_id(ch);
        x.value.mdl = &new_mdl;
    }

    {
        auto& x = parent.child_to_sim.data.emplace_back();
        x.id    = sc.mod.children.get_id(ch);
        x.value = new_mdl_id;
    }

    {
        irt_assert(unique_id != 0);

        if (ch.flags[child_flags::configurable] and
            ch.flags[child_flags::observable])
            parent.nodes_v.data.emplace_back(unique_id, new_mdl_id);
    }

    return success();
}

static status make_tree_recursive(simulation_copy&   sc,
                                  tree_node&         new_tree,
                                  generic_component& src) noexcept
{

    for (auto child_id : src.children) {
        if (auto* child = sc.mod.children.try_to_get(child_id); child) {
            if (child->type == child_type::component) {
                auto compo_id = child->id.compo_id;
                if (auto* compo = sc.mod.components.try_to_get(compo_id);
                    compo) {
                    if (auto ret = make_tree_recursive(
                          sc, new_tree, *compo, child_id, child->unique_id);
                        !ret)
                        return ret.error();
                }
            } else {
                const auto mdl_type = child->id.mdl_type;

                if (auto ret = make_tree_leaf(sc,
                                              new_tree,
                                              child->unique_id,
                                              mdl_type,
                                              child_id,
                                              *child);
                    !ret)
                    return ret.error();
            }
        }
    }

    new_tree.child_to_node.sort();
    new_tree.child_to_sim.sort();
    new_tree.nodes_v.sort();

    return success();
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  grid_component&  src) noexcept
{
    for (int i = 0, e = src.cache.ssize(); i != e; ++i) {
        if (auto* child = sc.mod.children.try_to_get(src.cache[i]); child) {
            if (child->type == child_type::component) {
                auto compo_id = child->id.compo_id;
                if (auto* compo = sc.mod.components.try_to_get(compo_id);
                    compo) {
                    if (auto ret = make_tree_recursive(
                          sc, new_tree, *compo, src.cache[i], child->unique_id);
                        !ret)
                        return ret.error();
                }
            } else {
                if (auto ret = make_tree_leaf(sc,
                                              new_tree,
                                              src.unique_id(i),
                                              child->id.mdl_type,
                                              src.cache[i],
                                              *child);
                    !ret)
                    return ret.error();
            }
        }
    }

    new_tree.child_to_node.sort();
    new_tree.child_to_sim.sort();
    new_tree.nodes_v.sort();

    return success();
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  graph_component& src) noexcept
{
    for (int i = 0, e = src.cache.ssize(); i != e; ++i) {
        if (auto* child = sc.mod.children.try_to_get(src.cache[i]); child) {
            if (child->type == child_type::component) {
                auto compo_id = child->id.compo_id;
                if (auto* compo = sc.mod.components.try_to_get(compo_id);
                    compo) {
                    if (auto ret = make_tree_recursive(
                          sc, new_tree, *compo, src.cache[i], child->unique_id);
                        !ret)
                        return ret.error();
                }
            } else {
                if (auto ret = make_tree_leaf(sc,
                                              new_tree,
                                              src.unique_id(i),
                                              child->id.mdl_type,
                                              src.cache[i],
                                              *child);
                    !ret)
                    return ret.error();
            }
        }
    }

    new_tree.child_to_node.sort();
    new_tree.child_to_sim.sort();
    new_tree.nodes_v.sort();

    return success();
}

static status make_tree_recursive([[maybe_unused]] simulation_copy& sc,
                                  [[maybe_unused]] tree_node&       new_tree,
                                  [[maybe_unused]] hsm_component& src) noexcept
{
    irt_assert(false && "missing hsm-component implementation");

    return success();
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       parent,
                                  component&       compo,
                                  child_id         id_in_parent,
                                  u64              unique_id) noexcept
{
    if (!sc.tree_nodes.can_alloc())
        return new_error(project::error::not_enough_memory);

    auto& new_tree =
      sc.tree_nodes.alloc(sc.mod.components.get_id(compo), unique_id);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    auto& x    = parent.child_to_node.data.emplace_back();
    x.id       = id_in_parent;
    x.value.tn = &new_tree;

    switch (compo.type) {
    case component_type::simple: {
        auto s_id = compo.id.generic_id;
        if (auto* s = sc.mod.generic_components.try_to_get(s_id); s)
            if (auto ret = make_tree_recursive(sc, new_tree, *s); !ret)
                return ret.error();
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            if (auto ret = make_tree_recursive(sc, new_tree, *g); !ret)
                return ret.error();
    } break;

    case component_type::graph: {
        auto g_id = compo.id.graph_id;
        if (auto* g = sc.mod.graph_components.try_to_get(g_id); g)
            if (auto ret = make_tree_recursive(sc, new_tree, *g); ret)
                return ret.error();
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;

    case component_type::hsm:
        break;
    }

    return success();
}

static status simulation_copy_source(simulation_copy& sc,
                                     const u64        id,
                                     const u64        type,
                                     source&          dst) noexcept
{
    switch (enum_cast<source::source_type>(type)) {
    case source::source_type::none:
        break;
    case source::source_type::constant:
        if (auto* ret = sc.cache.constants.get(id); ret) {
            dst.id = ordinal(*ret);
            return success();
        }
        break;
    case source::source_type::binary_file:
        if (auto* ret = sc.cache.binary_files.get(id); ret) {
            dst.id = ordinal(*ret);
            return success();
        }
        break;
    case source::source_type::text_file:
        if (auto* ret = sc.cache.text_files.get(id); ret) {
            dst.id = ordinal(*ret);
            return success();
        }
        break;
    case source::source_type::random:
        if (auto* ret = sc.cache.randoms.get(id); ret) {
            dst.id = ordinal(*ret);
            return success();
        }
        break;
    }

    return new_error(project::error::unknown_source, id, type);
}

void project::clear_cache() noexcept
{
    m_cache.stack.clear();
    m_cache.inputs.clear();
    m_cache.outputs.clear();

    m_cache.constants.data.clear();
    m_cache.binary_files.data.clear();
    m_cache.text_files.data.clear();
    m_cache.randoms.data.clear();
}

void project::destroy_cache() noexcept
{
    // @TODO homogeneize destroy functions in all irritator container.

    // stack.destroy();
    // inputs.destroy();
    // outputs.destroy();
    // sim_tree_nodes.destroy();

    clear();
}

static status get_input_models(simulation_copy& sc,
                               tree_node&       tree,
                               port_id          port_dst);

static status get_output_models(simulation_copy& sc,
                                tree_node&       tree,
                                port_id          port_dst);

static status get_input_models(simulation_copy& sc,
                               tree_node&       tree,
                               child_id         ch,
                               connection::port port)
{
    if (auto* child = sc.mod.children.try_to_get(ch); child) {
        auto* node = tree.child_to_node.get(ch);
        irt_assert(node);

        if (child->type == child_type::model) {
            sc.cache.inputs.emplace_back(std::make_pair(node->mdl, port.model));
        } else {
            return get_input_models(sc, *node->tn, port.compo);
        }
    }

    return success();
}

static status get_output_models(simulation_copy& sc,
                                tree_node&       tree,
                                child_id         ch,
                                connection::port port)
{
    if (auto* child = sc.mod.children.try_to_get(ch); child) {
        auto* node = tree.child_to_node.get(ch);
        irt_assert(node);

        if (child->type == child_type::model) {
            sc.cache.outputs.emplace_back(
              std::make_pair(node->mdl, port.model));
        } else {
            return get_output_models(sc, *node->tn, port.compo);
        }
    }

    return success();
}

static status get_input_models(simulation_copy& sc,
                               tree_node&       tree,
                               port_id          port_dst)
{

    if (auto* compo = sc.mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g =
                  sc.mod.generic_components.try_to_get(compo->id.generic_id);
                g) {
                for (auto cnx_id : g->connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            irt_check(get_input_models(sc,
                                                       tree,
                                                       cnx->output.src,
                                                       cnx->output.index_src));
                        }
                    }
                }
            }
            break;

        case component_type::grid:
            if (auto* g = sc.mod.grid_components.try_to_get(compo->id.grid_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            irt_check(get_input_models(sc,
                                                       tree,
                                                       cnx->output.src,
                                                       cnx->output.index_src));
                        }
                    }
                }
            }
            break;

        case component_type::graph:
            if (auto* g =
                  sc.mod.graph_components.try_to_get(compo->id.graph_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            irt_check(get_input_models(sc,
                                                       tree,
                                                       cnx->output.src,
                                                       cnx->output.index_src));
                        }
                    }
                }
            }
            break;

        case component_type::internal:
            break;

        case component_type::none:
            break;

        case component_type::hsm:
            break;
        }
    }

    return success();
}

static status get_output_models(simulation_copy& sc,
                                tree_node&       tree,
                                port_id          port_dst)
{
    if (auto* compo = sc.mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g =
                  sc.mod.generic_components.try_to_get(compo->id.generic_id);
                g) {
                for (auto cnx_id : g->connections) {
                    const auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                    const bool  is_output =
                      cnx && cnx->type == connection::connection_type::output &&
                      cnx->output.index == port_dst;

                    if (is_output)
                        irt_check(get_output_models(
                          sc, tree, cnx->input.dst, cnx->input.index_dst));
                }
            }
            break;

        case component_type::grid:
            if (auto* g = sc.mod.grid_components.try_to_get(compo->id.grid_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            irt_check(get_output_models(
                              sc, tree, cnx->input.dst, cnx->input.index_dst));
                        }
                    }
                }
            }
            break;

        case component_type::graph:
            if (auto* g =
                  sc.mod.graph_components.try_to_get(compo->id.graph_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            irt_check(get_output_models(
                              sc, tree, cnx->input.dst, cnx->input.index_dst));
                        }
                    }
                }
            }
            break;

        case component_type::internal:
            break;

        case component_type::none:
            break;

        case component_type::hsm:
            break;
        }
    }

    return success();
}

static status simulation_copy_connections(
  const vector<std::pair<model*, int>>& inputs,
  const vector<std::pair<model*, int>>& outputs,
  simulation&                           sim) noexcept
{
    for (auto src : outputs)
        for (auto dst : inputs)
            if (auto ret =
                  sim.connect(*src.first, src.second, *dst.first, dst.second);
                !ret)
                return new_error(project::error::impossible_connection);

    return success();
}

static status simulation_copy_connections(simulation_copy&       sc,
                                          tree_node&             tree,
                                          vector<connection_id>& connections)
{
    for (auto cnx_id : connections) {
        sc.cache.inputs.clear();
        sc.cache.outputs.clear();

        auto* cnx = sc.mod.connections.try_to_get(cnx_id);
        irt_assert(cnx);

        const bool is_internal_connection =
          cnx && cnx->type == connection::connection_type::internal;

        if (is_internal_connection) {
            auto* src = sc.mod.children.try_to_get(cnx->internal.src);
            auto* dst = sc.mod.children.try_to_get(cnx->internal.dst);
            if (!(src && dst))
                continue;

            auto* node_src = tree.child_to_node.get(cnx->internal.src);
            auto* node_dst = tree.child_to_node.get(cnx->internal.dst);

            if (src->type == child_type::model) {
                sc.cache.outputs.emplace_back(
                  std::make_pair(node_src->mdl, cnx->internal.index_src.model));

                if (dst->type == child_type::model) {
                    sc.cache.inputs.emplace_back(std::make_pair(
                      node_dst->mdl, cnx->internal.index_dst.model));
                } else {
                    irt_check(get_input_models(
                      sc, *node_dst->tn, cnx->internal.index_dst.compo));
                }
            } else {
                irt_check(get_output_models(
                  sc, *node_src->tn, cnx->internal.index_src.compo));

                if (dst->type == child_type::model) {
                    sc.cache.inputs.emplace_back(std::make_pair(
                      node_dst->mdl, cnx->internal.index_dst.model));
                } else {
                    irt_check(get_input_models(
                      sc, *node_dst->tn, cnx->internal.index_dst.compo));
                }
            }

            if (auto ret = simulation_copy_connections(
                  sc.cache.inputs, sc.cache.outputs, sc.sim);
                !ret)
                return ret.error();
        }
    }

    return success();
}

static status simulation_copy_connections(simulation_copy& sc,
                                          tree_node&       tree,
                                          component&       compo)
{
    switch (compo.type) {
    case component_type::simple: {
        if (auto* g = sc.mod.generic_components.try_to_get(compo.id.generic_id);
            g)
            return simulation_copy_connections(sc, tree, g->connections);
    } break;

    case component_type::grid:
        if (auto* g = sc.mod.grid_components.try_to_get(compo.id.grid_id); g)
            return simulation_copy_connections(sc, tree, g->cache_connections);
        break;

    case component_type::graph:
        if (auto* g = sc.mod.graph_components.try_to_get(compo.id.graph_id); g)
            return simulation_copy_connections(sc, tree, g->cache_connections);
        break;

    case component_type::internal:
        break;

    case component_type::none:
        break;

    case component_type::hsm:
        break;
    }

    return success();
}

static status simulation_copy_connections(simulation_copy& sc,
                                          tree_node&       head) noexcept
{
    sc.cache.stack.clear();
    sc.cache.stack.emplace_back(&head);

    while (!sc.cache.stack.empty()) {
        auto cur = sc.cache.stack.back();
        sc.cache.stack.pop_back();

        if (auto* compo = sc.mod.components.try_to_get(cur->id); compo)
            if (auto ret = simulation_copy_connections(sc, *cur, *compo); !ret)
                return ret;

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            sc.cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            sc.cache.stack.emplace_back(child);
    }

    return success();
}

static status simulation_copy_sources(project::cache& cache,
                                      modeling&       mod,
                                      simulation&     sim) noexcept
{
    sim.srcs.clear();

    sim.srcs.constant_sources.reserve(mod.srcs.constant_sources.capacity());
    if (not sim.srcs.constant_sources.can_alloc())
        return new_error(external_source::part::constant_source);

    sim.srcs.binary_file_sources.reserve(
      mod.srcs.binary_file_sources.capacity());
    if (not sim.srcs.binary_file_sources.can_alloc())
        return new_error(external_source::part::binary_file_source);

    sim.srcs.text_file_sources.reserve(mod.srcs.text_file_sources.capacity());
    if (not sim.srcs.text_file_sources.can_alloc())
        return new_error(external_source::part::text_file_source);

    sim.srcs.random_sources.reserve(mod.srcs.random_sources.capacity());
    if (not sim.srcs.random_sources.can_alloc())
        return new_error(external_source::part::random_source);

    {
        constant_source* src = nullptr;
        while (mod.srcs.constant_sources.next(src)) {
            auto& n_src    = mod.srcs.constant_sources.alloc(*src);
            auto  src_id   = mod.srcs.constant_sources.get_id(*src);
            auto  n_src_id = mod.srcs.constant_sources.get_id(n_src);
            cache.constants.data.emplace_back(ordinal(src_id), n_src_id);
        }

        cache.constants.sort();
    }

    {
        binary_file_source* src = nullptr;
        while (mod.srcs.binary_file_sources.next(src)) {
            auto& n_src    = mod.srcs.binary_file_sources.alloc(*src);
            auto  src_id   = mod.srcs.binary_file_sources.get_id(*src);
            auto  n_src_id = mod.srcs.binary_file_sources.get_id(n_src);
            cache.binary_files.data.emplace_back(ordinal(src_id), n_src_id);
        }

        cache.binary_files.sort();
    }

    {
        text_file_source* src = nullptr;
        while (mod.srcs.text_file_sources.next(src)) {
            auto& n_src    = mod.srcs.text_file_sources.alloc(*src);
            auto  src_id   = mod.srcs.text_file_sources.get_id(*src);
            auto  n_src_id = mod.srcs.text_file_sources.get_id(n_src);
            cache.text_files.data.emplace_back(ordinal(src_id), n_src_id);
        }

        cache.text_files.sort();
    }

    {
        random_source* src = nullptr;
        while (mod.srcs.random_sources.next(src)) {
            auto& n_src    = mod.srcs.random_sources.alloc(*src);
            auto  src_id   = mod.srcs.random_sources.get_id(*src);
            auto  n_src_id = mod.srcs.random_sources.get_id(n_src);
            cache.randoms.data.emplace_back(ordinal(src_id), n_src_id);
        }

        cache.randoms.sort();
    }

    return success();
}

static status make_component_cache(project& /*pj*/, modeling& mod) noexcept
{
    for (auto& grid : mod.grid_components)
        if (auto ret = mod.build_grid_component_cache(grid); !ret)
            return ret.error();

    for (auto& graph : mod.graph_components)
        if (auto ret = mod.build_graph_component_cache(graph); !ret)
            return ret.error();

    return success();
}

static status make_tree_from(simulation_copy&                     sc,
                             data_array<tree_node, tree_node_id>& data,
                             component&                           parent,
                             tree_node_id*                        out) noexcept
{
    if (!data.can_alloc())
        return new_error(project::error::not_enough_memory);

    auto& new_tree    = data.alloc(sc.mod.components.get_id(parent), 0);
    auto  new_tree_id = data.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);

    switch (parent.type) {
    case component_type::simple: {
        auto s_id = parent.id.generic_id;
        if (auto* s = sc.mod.generic_components.try_to_get(s_id); s)
            if (auto ret = make_tree_recursive(sc, new_tree, *s); !ret)
                return ret.error();
    } break;

    case component_type::grid: {
        auto g_id = parent.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            if (auto ret = make_tree_recursive(sc, new_tree, *g); !ret)
                return ret.error();
    } break;

    case component_type::graph: {
        auto g_id = parent.id.graph_id;
        if (auto* g = sc.mod.graph_components.try_to_get(g_id); g)
            if (auto ret = make_tree_recursive(sc, new_tree, *g); !ret)
                return ret.error();
    } break;

    case component_type::hsm: {
        auto h_id = parent.id.hsm_id;
        if (auto* h = sc.mod.hsm_components.try_to_get(h_id); h)
            if (auto ret = make_tree_recursive(sc, new_tree, *h); !ret)
                return ret.error();
        break;
    }

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    *out = new_tree_id;

    return success();
}

status project::init(const modeling_initializer& init) noexcept
{
    tree_nodes.reserve(init.tree_capacity);
    if (not tree_nodes.can_alloc())
        return new_error(project::part::tree_nodes);
    variable_observers.reserve(init.tree_capacity);
    if (not variable_observers.can_alloc())
        return new_error(project::part::variable_observers);
    grid_observers.reserve(init.tree_capacity);
    if (not grid_observers.can_alloc())
        return new_error(project::part::grid_observers);
    global_parameters.reserve(init.tree_capacity);
    if (not global_parameters.can_alloc())
        return new_error(project::part::global_parameters);

    grid_observation_systems.resize(init.tree_capacity);

    return success();
}

status project::set(modeling& mod, simulation& sim, component& compo) noexcept
{
    clear();
    clear_cache();
    clean_simulation();
    mod.clean_simulation();

    if (auto ret = make_component_cache(*this, mod); !ret)
        return ret.error();

    simulation_copy sc(m_cache, mod, sim, tree_nodes);
    tree_node_id    id = undefined<tree_node_id>();

    if (auto ret = make_tree_from(sc, tree_nodes, compo, &id); !ret)
        return ret.error();

    m_head    = mod.components.get_id(compo);
    m_tn_head = id;

    if (auto ret = simulation_copy_sources(m_cache, mod, sim); !ret)
        return ret.error();
    if (auto ret = simulation_copy_connections(sc, *tn_head()); !ret)
        return ret.error();

    return success();
}

status project::rebuild(modeling& mod, simulation& sim) noexcept
{
    clear();
    clear_cache();
    clean_simulation();
    mod.clean_simulation();

    if (auto ret = make_component_cache(*this, mod); !ret)
        return ret.error();

    if (auto* compo = mod.components.try_to_get(head()); compo) {
        simulation_copy sc(m_cache, mod, sim, tree_nodes);
        tree_node_id    id = undefined<tree_node_id>();

        if (auto ret = make_tree_from(sc, tree_nodes, *compo, &id); !ret)
            return ret.error();

        m_head    = mod.components.get_id(*compo);
        m_tn_head = id;

        if (auto ret = simulation_copy_sources(m_cache, mod, sim); !ret)
            return ret.error();
        if (auto ret = simulation_copy_connections(sc, *tn_head()); !ret)
            return ret.error();
    }

    return success();
}

void project::clear() noexcept
{
    tree_nodes.clear();

    m_head    = undefined<component_id>();
    m_tn_head = undefined<tree_node_id>();

    variable_observers.clear();
    grid_observers.clear();
    graph_observers.clear();
    global_parameters.clear();
}

void project::clean_simulation() noexcept
{
    for_all_tree_nodes([](auto& tn) { tn.child_to_node.data.clear(); });

    for_each_data(grid_observers, [&](auto& grid_obs) noexcept {
        auto id  = grid_observers.get_id(grid_obs);
        auto idx = get_index(id);

        grid_observation_systems[idx].clear();
    });
}

status project::load(modeling&   mod,
                     simulation& sim,
                     cache_rw&   cache,
                     const char* filename) noexcept
{
    if (auto file = file::make_file(filename, open_mode::read); !file) {
        return file.error();
    } else {
        json_archiver j;
        return j.project_load(*this, mod, sim, cache, *file);
    }
}

status project::save(modeling&   mod,
                     simulation& sim,
                     cache_rw&   cache,
                     const char* filename) noexcept
{
    if (auto file = file::make_file(filename, open_mode::write); !file) {
        return file.error();
    } else {
        json_archiver j;
        return j.project_save(*this, mod, sim, cache, *file);
    }
}

static void project_build_unique_id_path(const u64       model_unique_id,
                                         unique_id_path& out) noexcept
{
    out.clear();
    out.emplace_back(model_unique_id);
}

static void project_build_unique_id_path(const tree_node& tn,
                                         unique_id_path&  out) noexcept
{
    out.clear();

    auto* parent = &tn;

    do {
        out.emplace_back(parent->unique_id);
        parent = parent->tree.get_parent();
    } while (parent);
}

static void project_build_unique_id_path(
  const tree_node& model_unique_id_parent,
  const u64        model_unique_id,
  unique_id_path&  out) noexcept
{
    project_build_unique_id_path(model_unique_id_parent, out);
    out.emplace_back(model_unique_id);
}

auto project::build_relative_path(const tree_node& from,
                                  const tree_node& to,
                                  const model_id   mdl_id) noexcept
  -> relative_id_path
{
    irt_assert(tree_nodes.get_id(from) != tree_nodes.get_id(to));
    irt_assert(to.tree.get_parent() != nullptr);

    relative_id_path ret;

    const auto mdl_unique_id = to.get_unique_id(mdl_id);

    if (mdl_unique_id) {
        const auto from_id = tree_nodes.get_id(from);

        ret.tn = tree_nodes.get_id(from);
        ret.ids.emplace_back(mdl_unique_id);
        ret.ids.emplace_back(to.unique_id);

        auto* parent    = to.tree.get_parent();
        auto  parent_id = tree_nodes.get_id(*parent);
        while (parent && parent_id != from_id) {
            ret.ids.emplace_back(parent->unique_id);
            parent = parent->tree.get_parent();
        }
    }

    return ret;
}

auto project::get_model(const relative_id_path& path) noexcept
  -> std::pair<tree_node_id, model_id>
{
    const auto* tn = tree_nodes.try_to_get(path.tn);

    return tn
             ? get_model(*tn, path)
             : std::make_pair(undefined<tree_node_id>(), undefined<model_id>());
}

auto project::get_model(const tree_node&        tn,
                        const relative_id_path& path) noexcept
  -> std::pair<tree_node_id, model_id>
{
    irt_assert(path.ids.ssize() >= 2);

    tree_node_id ret_node_id = tree_nodes.get_id(tn);
    model_id     ret_mdl_id  = undefined<model_id>();

    const auto* from = &tn;
    const int   first =
      path.ids.ssize() - 2; // Do not read the first child of the grid component
                            // tree node. Use tn instead.

    for (int i = first; i >= 0; --i) {
        const tree_node::node_v* ptr = from->nodes_v.get(path.ids[i]);

        if (!ptr)
            break;

        if (i != 0) {
            const auto* tn_id = std::get_if<tree_node_id>(ptr);
            if (!tn_id)
                break;

            ret_node_id = *tn_id;
            from        = tree_nodes.try_to_get(*tn_id);
        } else {
            const auto* mdl_id = std::get_if<model_id>(ptr);
            if (!mdl_id)
                break;

            ret_mdl_id = *mdl_id;
        }
    }

    return std::make_pair(ret_node_id, ret_mdl_id);
}

void project::build_unique_id_path(const tree_node_id tn_id,
                                   const model_id     mdl_id,
                                   unique_id_path&    out) noexcept
{
    out.clear();

    if_data_exists_do(tree_nodes, tn_id, [&](const auto& tn) noexcept {
        auto model_unique_id = tn.get_unique_id(mdl_id);
        if (model_unique_id != 0)
            build_unique_id_path(tn, model_unique_id, out);
    });
}

void project::build_unique_id_path(const tree_node_id tn_id,
                                   unique_id_path&    out) noexcept
{
    out.clear();

    if (tn_id != m_tn_head) {
        if_data_exists_do(tree_nodes, tn_id, [&](const auto& tn) noexcept {
            project_build_unique_id_path(tn, out);
        });
    };
}

void project::build_unique_id_path(const tree_node& model_unique_id_parent,
                                   const u64        model_unique_id,
                                   unique_id_path&  out) noexcept
{
    out.clear();

    return tree_nodes.get_id(model_unique_id_parent) == m_tn_head
             ? project_build_unique_id_path(model_unique_id, out)
             : project_build_unique_id_path(
                 model_unique_id_parent, model_unique_id, out);
}

auto project::get_model_path(u64 id) const noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    auto model_id_opt = tn_head()->get_model_id(id);

    return model_id_opt.has_value()
             ? std::make_optional(std::make_pair(m_tn_head, *model_id_opt))
             : std::nullopt;
}

static auto project_get_model_path(const project&        pj,
                                   const unique_id_path& path) noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    std::optional<tree_node_id> tn_id_opt;
    std::optional<model_id>     mdl_id_opt;
    bool                        error = false;

    auto* tn   = pj.tn_head();
    u64   last = path.back();

    irt_assert(tn);

    for (int i = 0, e = path.size() - 1; i != e; ++i) {
        tn_id_opt = tn->get_tree_node_id(path[i]);

        if (!tn_id_opt.has_value()) {
            error = true;
            break;
        }

        tn = pj.node(*tn_id_opt);

        if (!tn) {
            error = true;
            break;
        }
    }

    if (!error) {
        tn_id_opt  = pj.node(*tn);
        mdl_id_opt = tn->get_model_id(last);

        if (!mdl_id_opt.has_value()) {
            error = true;
        }
    }

    return !error ? std::make_optional(std::make_pair(*tn_id_opt, *mdl_id_opt))
                  : std::nullopt;
}

auto project::get_model_path(const unique_id_path& path) const noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    switch (path.ssize()) {
    case 0:
        return std::nullopt;

    case 1:
        return get_model_path(path.front());

    default:
        return project_get_model_path(*this, path);
    }

    unreachable();
}

static auto project_get_first_tn_id_from(const project&   pj,
                                         const tree_node& from,
                                         u64              id) noexcept
  -> std::optional<tree_node_id>
{
    if (auto* child = from.tree.get_child(); child) {
        do {
            if (child->unique_id == id)
                return std::make_optional(pj.node(*child));

            child = child->tree.get_sibling();
        } while (child);
    }

    return std::nullopt;
}

static auto project_get_first_tn_id(const project& pj, u64 id) noexcept
  -> std::optional<tree_node_id>
{
    if (const auto* head = pj.tn_head(); head)
        return project_get_first_tn_id_from(pj, *head, id);

    return std::nullopt;
}

static auto project_get_tn_id(const project&             pj,
                              const std::span<const u64> ids) noexcept
  -> std::optional<tree_node_id>
{
    if (const auto* tn = pj.tn_head(); tn) {
        for (auto id : ids) {
            auto child_opt = project_get_first_tn_id_from(pj, *tn, id);
            if (!child_opt.has_value())
                return std::nullopt;

            tn = pj.node(*child_opt);
            if (!tn)
                return std::nullopt;

            if (ids.size() == 1)
                return std::make_optional(pj.node(*tn));
        }
    }

    return std::nullopt;
}

auto project::get_tn_id(const unique_id_path& path) const noexcept
  -> std::optional<tree_node_id>
{
    if (const auto* head = tn_head(); head) {
        switch (path.ssize()) {
        case 0:
            return m_tn_head;

        case 1:
            return project_get_first_tn_id(*this, path.front());

        default:
            return project_get_tn_id(
              *this, std::span<const u64>(path.data(), path.size()));
        }
    }

    return std::nullopt;
}

auto project::head() const noexcept -> component_id { return m_head; }

auto project::tn_head() const noexcept -> tree_node*
{
    return tree_nodes.try_to_get(m_tn_head);
}

auto project::node(tree_node_id id) const noexcept -> tree_node*
{
    return tree_nodes.try_to_get(id);
}

auto project::node(tree_node& node) const noexcept -> tree_node_id
{
    return tree_nodes.get_id(node);
}

auto project::node(const tree_node& node) const noexcept -> tree_node_id
{
    return tree_nodes.get_id(node);
}

auto project::tree_nodes_size() const noexcept -> std::pair<int, int>
{
    return std::make_pair(tree_nodes.ssize(), tree_nodes.capacity());
}

} // namespace irt
