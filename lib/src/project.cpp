// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <optional>
#include <utility>

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
    {
        /* For all hsm component, make a copy from modeling::hsm into
         * simulation::hsm to ensure link between simulation and modeling. */
        for (auto& modhsm : mod.hsm_components) {
            if (not sim.hsms.can_alloc())
                break;

            auto& sim_hsm = sim.hsms.alloc(modhsm.machine);

            const auto hsm_id = mod.hsm_components.get_id(modhsm);
            const auto sim_id = sim.hsms.get_id(sim_hsm);

            hsm_mod_to_sim.data.emplace_back(hsm_id, sim_id);
        }

        hsm_mod_to_sim.sort();
    }

    project::cache& cache;
    modeling&       mod;
    simulation&     sim;

    data_array<tree_node, tree_node_id>& tree_nodes;
    table<hsm_component_id, hsm_id>      hsm_mod_to_sim;
};

static auto make_tree_recursive(simulation_copy& sc,
                                tree_node&       parent,
                                component&       compo,
                                u64 unique_id) noexcept -> result<tree_node_id>;

struct parent_t {
    tree_node& parent;
    component& compo;
};

static auto get_incoming_connection(const generic_component& gen,
                                    const port_id            id) noexcept -> int
{
    int i = 0;

    for (const auto& input : gen.input_connections)
        if (input.x == id)
            ++i;

    for (const auto& internal : gen.connections)
        if (internal.index_dst.compo == id)
            ++i;

    return i;
}

static auto get_incoming_connection(const modeling&  mod,
                                    const tree_node& tn,
                                    const port_id    id) noexcept -> result<int>
{
    const auto* compo = mod.components.try_to_get(tn.id);
    if (not compo)
        return new_error(project::part::tree_nodes);

    if (not compo->x.exists(id))
        return new_error(project::part::tree_nodes);

    if (compo->type == component_type::simple) {
        auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
        return gen ? get_incoming_connection(*gen, id) : 0;
    }

    return new_error(project::part::tree_nodes);
}

static auto get_incoming_connection(const modeling&  mod,
                                    const tree_node& tn) noexcept -> result<int>
{
    const auto* compo = mod.components.try_to_get(tn.id);
    if (not compo)
        return new_error(project::part::tree_nodes);

    int nb = 0;
    compo->x.for_each_id([&](auto id) noexcept {
        if (compo->type == component_type::simple) {
            auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
            if (gen)
                nb += get_incoming_connection(*gen, id);
        }
    });

    return nb;
}

static auto get_outcoming_connection(const generic_component& gen,
                                     const port_id id) noexcept -> int
{
    int i = 0;

    for (const auto& input : gen.output_connections)
        if (input.y == id)
            ++i;

    for (const auto& internal : gen.connections)
        if (internal.index_src.compo == id)
            ++i;

    return i;
}

static auto get_outcoming_connection(const modeling&  mod,
                                     const tree_node& tn,
                                     const port_id id) noexcept -> result<int>
{
    const auto* compo = mod.components.try_to_get(tn.id);
    if (not compo)
        return new_error(project::part::tree_nodes);

    if (not compo->y.exists(id))
        return new_error(project::part::tree_nodes);

    if (compo->type == component_type::simple) {
        auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
        return gen ? get_outcoming_connection(*gen, id) : 0;
    }

    return new_error(project::part::tree_nodes);
}

static auto get_outcoming_connection(const modeling&  mod,
                                     const tree_node& tn) noexcept
  -> result<int>
{
    const auto* compo = mod.components.try_to_get(tn.id);
    if (not compo)
        return new_error(project::part::tree_nodes);

    int nb = 0;
    compo->y.for_each_id([&](auto id) noexcept {
        if (compo->type == component_type::simple) {
            auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
            if (gen)
                nb += get_outcoming_connection(*gen, id);
        }
    });

    return nb;
}

static auto make_tree_leaf(simulation_copy&   sc,
                           tree_node&         parent,
                           generic_component& gen,
                           u64                unique_id,
                           dynamics_type      mdl_type,
                           child_id           ch_id,
                           child&             ch) noexcept -> result<model_id>
{
    if (!sc.sim.models.can_alloc())
        return new_error(project::error::not_enough_memory);

    const auto ch_idx     = get_index(ch_id);
    auto&      new_mdl    = sc.sim.models.alloc();
    auto       new_mdl_id = sc.sim.models.get_id(new_mdl);

    new_mdl.type   = mdl_type;
    new_mdl.handle = invalid_heap_handle;

    irt_check(
      dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> status {
          std::construct_at(&dyn);

          if constexpr (has_input_port<Dynamics>)
              for (int i = 0, e = length(dyn.x); i != e; ++i)
                  dyn.x[i] = undefined<message_id>();

          if constexpr (has_output_port<Dynamics>)
              for (int i = 0, e = length(dyn.y); i != e; ++i)
                  dyn.y[i] = undefined<node_id>();

          gen.children_parameters[ch_idx].copy_to(new_mdl);

          if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
              const auto child_index = get_index(ch_id);
              const auto compo_id    = enum_cast<component_id>(
                gen.children_parameters[child_index].integers[0]);

              if (const auto* compo = sc.mod.components.try_to_get(compo_id)) {
                  debug::ensure(compo->type == component_type::hsm);

                  const auto hsm_id = compo->id.hsm_id;
                  if (const auto* hsm =
                        sc.mod.hsm_components.try_to_get(hsm_id)) {
                      const auto* shsm = sc.hsm_mod_to_sim.get(hsm_id);
                      if (shsm) {
                          dyn.id      = *shsm;
                          dyn.exec.i1 = static_cast<i32>(
                            gen.children_parameters[child_index].integers[1]);
                          dyn.exec.i2 = static_cast<i32>(
                            gen.children_parameters[child_index].integers[2]);
                          dyn.exec.r1 =
                            gen.children_parameters[child_index].reals[0];
                          dyn.exec.r2 =
                            gen.children_parameters[child_index].reals[1];
                          dyn.exec.timer =
                            gen.children_parameters[child_index].reals[2];
                      } else {
                          return new_error(project::hsm_error{},
                                           undefined_error{});
                      }
                  } else {
                      return new_error(project::hsm_error{}, unknown_error{});
                  }
              } else {
                  return new_error(project::hsm_error{},
                                   incompatibility_error{});
              }
          }

          if constexpr (std::is_same_v<Dynamics, constant>) {
              switch (dyn.type) {
              case constant::init_type::constant:
                  break;

              case constant::init_type::incoming_component_all: {
                  irt_auto(nb, get_incoming_connection(sc.mod, parent));
                  gen.children_parameters[ch_idx].reals[0] = nb;
                  dyn.default_value                        = nb;
              } break;

              case constant::init_type::outcoming_component_all: {
                  irt_auto(nb, get_outcoming_connection(sc.mod, parent));
                  gen.children_parameters[ch_idx].reals[0] = nb;
                  dyn.default_value                        = nb;
              } break;

              case constant::init_type::incoming_component_n: {
                  const auto id = enum_cast<port_id>(dyn.port);
                  if (not sc.mod.components.get(parent.id).x.exists(id))
                      return new_error(project::part::tree_nodes);

                  irt_auto(nb, get_incoming_connection(sc.mod, parent, id));
                  gen.children_parameters[ch_idx].reals[0] = nb;
                  dyn.default_value                        = nb;
              } break;

              case constant::init_type::outcoming_component_n: {
                  const auto id = enum_cast<port_id>(dyn.port);
                  if (not sc.mod.components.get(parent.id).y.exists(id))
                      return new_error(project::part::tree_nodes);

                  irt_auto(nb, get_outcoming_connection(sc.mod, parent, id));
                  gen.children_parameters[ch_idx].reals[0] = nb;
                  dyn.default_value                        = nb;
              } break;
              }
          }

          return success();
      }));

    if (unique_id != 0 and (ch.flags[child_flags::configurable] or
                            ch.flags[child_flags::observable]))
        parent.unique_id_to_model_id.data.emplace_back(unique_id, new_mdl_id);

    return new_mdl_id;
}

static status make_tree_recursive(simulation_copy&   sc,
                                  tree_node&         new_tree,
                                  generic_component& src) noexcept
{
    new_tree.children.resize(src.children.max_used());

    for (auto& child : src.children) {
        const auto child_id = src.children.get_id(child);

        if (child.type == child_type::component) {
            const auto compo_id = child.id.compo_id;

            if (auto* compo = sc.mod.components.try_to_get(compo_id); compo) {
                auto tn_id =
                  make_tree_recursive(sc, new_tree, *compo, child.unique_id);

                if (tn_id.has_error())
                    return tn_id.error();

                new_tree.children[get_index(child_id)].set(
                  sc.tree_nodes.try_to_get(*tn_id));
            }
        } else {
            const auto mdl_type = child.id.mdl_type;

            irt_auto(
              mdl_id,
              make_tree_leaf(
                sc, new_tree, src, child.unique_id, mdl_type, child_id, child));

            new_tree.children[get_index(child_id)].set(
              sc.sim.models.try_to_get(mdl_id));
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  grid_component&  src) noexcept
{
    new_tree.children.resize(src.cache.max_used());

    for (auto& child : src.cache) {
        const auto child_id = src.cache.get_id(child);

        debug::ensure(child.type == child_type::component);

        if (child.type == child_type::component) {
            const auto compo_id = child.id.compo_id;

            if (auto* compo = sc.mod.components.try_to_get(compo_id); compo) {
                irt_auto(
                  tn_id,
                  make_tree_recursive(sc, new_tree, *compo, child.unique_id));

                new_tree.children[get_index(child_id)].set(
                  sc.tree_nodes.try_to_get(tn_id));
            }
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  graph_component& src) noexcept
{
    new_tree.children.resize(src.cache.max_used());

    for (auto& child : src.cache) {
        const auto child_id = src.cache.get_id(child);

        debug::ensure(child.type == child_type::component);

        if (child.type == child_type::component) {
            const auto compo_id = child.id.compo_id;

            if (auto* compo = sc.mod.components.try_to_get(compo_id); compo) {
                auto tn_id =
                  make_tree_recursive(sc, new_tree, *compo, child.unique_id);
                if (tn_id.has_error())
                    return tn_id.error();

                new_tree.children[get_index(child_id)].set(
                  sc.tree_nodes.try_to_get(*tn_id));
            }
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static status make_tree_recursive([[maybe_unused]] simulation_copy& sc,
                                  [[maybe_unused]] tree_node&       new_tree,
                                  [[maybe_unused]] hsm_component& src) noexcept
{
    debug::ensure(sc.sim.hsms.can_alloc());

    return success();
}

static auto make_tree_recursive(simulation_copy& sc,
                                tree_node&       parent,
                                component&       compo,
                                u64 unique_id) noexcept -> result<tree_node_id>
{
    if (not sc.tree_nodes.can_alloc())
        return new_error(project::error::not_enough_memory);

    auto& new_tree =
      sc.tree_nodes.alloc(sc.mod.components.get_id(compo), unique_id);
    const auto tn_id = sc.tree_nodes.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    switch (compo.type) {
    case component_type::simple: {
        auto s_id = compo.id.generic_id;
        if (auto* s = sc.mod.generic_components.try_to_get(s_id); s)
            irt_check(make_tree_recursive(sc, new_tree, *s));
        parent.unique_id_to_tree_node_id.data.emplace_back(unique_id, tn_id);
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(unique_id, tn_id);
    } break;

    case component_type::graph: {
        auto g_id = compo.id.graph_id;
        if (auto* g = sc.mod.graph_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(unique_id, tn_id);
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;

    case component_type::hsm:
        break;
    }

    return sc.tree_nodes.get_id(new_tree);
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

static status simulation_copy_connections(
  const vector<project::cache::model_port>& inputs,
  const vector<project::cache::model_port>& outputs,
  simulation&                               sim) noexcept
{
    for (auto src : outputs) {
        for (auto dst : inputs) {
            if (auto ret = sim.connect(*src.mdl, src.port, *dst.mdl, dst.port);
                !ret)
                return new_error(project::error::impossible_connection);
        }
    }

    return success();
}

static void get_input_models(vector<project::cache::model_port>& inputs,
                             const modeling&                     mod,
                             const tree_node&                    tn,
                             const port_id                       p) noexcept;

static void get_input_models(vector<project::cache::model_port>& inputs,
                             const modeling&                     mod,
                             const tree_node&                    tn,
                             const generic_component&            gen,
                             const port_id                       p) noexcept
{
    for (const auto& con : gen.input_connections) {
        if (con.x != p)
            continue;

        auto* child = gen.children.try_to_get(con.dst);
        if (not child)
            continue;

        if (child->type == child_type::model) {
            debug::ensure(tn.children[get_index(con.dst)].mdl);

            inputs.emplace_back(tn.children[get_index(con.dst)].mdl,
                                con.port.model);
        } else {
            debug::ensure(tn.children[get_index(con.dst)].tn);
            get_input_models(
              inputs, mod, *tn.children[get_index(con.dst)].tn, con.port.compo);
        }
    }
}

static void get_input_models(vector<project::cache::model_port>& inputs,
                             const modeling&                     mod,
                             const tree_node&                    tn,
                             const graph_component&              graph,
                             const port_id                       p) noexcept
{
    for (const auto& con : graph.input_connections) {
        if (con.x != p)
            continue;

        auto* vertex = graph.children.try_to_get(con.v);
        if (not vertex)
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_input_models(vector<project::cache::model_port>& inputs,
                             const modeling&                     mod,
                             const tree_node&                    tn,
                             const grid_component&               grid,
                             const port_id                       p) noexcept
{
    for (const auto& con : grid.input_connections) {
        if (con.x != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_input_models(vector<project::cache::model_port>& inputs,
                             const modeling&                     mod,
                             const tree_node&                    tn,
                             const port_id                       p) noexcept
{
    auto* compo = mod.components.try_to_get(tn.id);
    debug::ensure(compo != nullptr);
    if (not compo)
        return;

    switch (compo->type) {
    case component_type::simple: {
        if (auto* g = mod.generic_components.try_to_get(compo->id.generic_id))
            get_input_models(inputs, mod, tn, *g, p);
    } break;

    case component_type::graph: {
        if (auto* g = mod.graph_components.try_to_get(compo->id.graph_id))
            get_input_models(inputs, mod, tn, *g, p);
    } break;

    case component_type::grid: {
        if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id))
            get_input_models(inputs, mod, tn, *g, p);
    } break;

    case component_type::hsm:
    case component_type::none:
    case component_type::internal:
        break;
    }
}

static void get_output_models(vector<project::cache::model_port>& outputs,
                              const modeling&                     mod,
                              const tree_node&                    tn,
                              const port_id                       p) noexcept;

static void get_output_models(vector<project::cache::model_port>& outputs,
                              const modeling&                     mod,
                              const tree_node&                    tn,
                              const generic_component&            gen,
                              const port_id                       p) noexcept
{
    for (const auto& con : gen.output_connections) {
        if (con.y != p)
            continue;

        auto* child = gen.children.try_to_get(con.src);
        if (not child)
            continue;

        if (child->type == child_type::model) {
            debug::ensure(tn.children[get_index(con.src)].mdl);

            outputs.emplace_back(tn.children[get_index(con.src)].mdl,
                                 con.port.model);
        } else {
            debug::ensure(tn.children[get_index(con.src)].tn);
            get_output_models(outputs,
                              mod,
                              *tn.children[get_index(con.src)].tn,
                              con.port.compo);
        }
    }
}

static void get_output_models(vector<project::cache::model_port>& outputs,
                              const modeling&                     mod,
                              const tree_node&                    tn,
                              const graph_component&              graph,
                              const port_id                       p) noexcept
{
    for (const auto& con : graph.output_connections) {
        if (con.y != p)
            continue;

        auto* vertex = graph.children.try_to_get(con.v);
        if (not vertex)
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_output_models(vector<project::cache::model_port>& outputs,
                              const modeling&                     mod,
                              const tree_node&                    tn,
                              const grid_component&               grid,
                              const port_id                       p) noexcept
{
    for (const auto& con : grid.output_connections) {
        if (con.y != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_output_models(vector<project::cache::model_port>& outputs,
                              const modeling&                     mod,
                              const tree_node&                    tn,
                              const port_id                       p) noexcept
{
    auto* compo = mod.components.try_to_get(tn.id);
    debug::ensure(compo != nullptr);
    if (not compo)
        return;

    switch (compo->type) {
    case component_type::simple: {
        if (auto* g = mod.generic_components.try_to_get(compo->id.generic_id))
            get_output_models(outputs, mod, tn, *g, p);
    } break;

    case component_type::graph: {
        if (auto* g = mod.graph_components.try_to_get(compo->id.graph_id))
            get_output_models(outputs, mod, tn, *g, p);
    } break;

    case component_type::grid: {
        if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id))
            get_output_models(outputs, mod, tn, *g, p);
    } break;

    case component_type::hsm:
    case component_type::none:
    case component_type::internal:
        break;
    }
}

static status simulation_copy_connections(
  simulation_copy&                             sc,
  tree_node&                                   tree,
  const data_array<child, child_id>&           children,
  const data_array<connection, connection_id>& connections)
{
    for (const auto& cnx : connections) {
        sc.cache.inputs.clear();
        sc.cache.outputs.clear();

        auto* src = children.try_to_get(cnx.src);
        auto* dst = children.try_to_get(cnx.dst);

        debug::ensure(src and dst);

        if (not src or not dst)
            continue;

        const auto src_idx = get_index(cnx.src);
        const auto dst_idx = get_index(cnx.dst);

        if (tree.is_model(cnx.src)) {
            sc.cache.outputs.emplace_back(tree.children[src_idx].mdl,
                                          cnx.index_src.model);

            if (tree.is_model(cnx.dst)) {
                sc.cache.inputs.emplace_back(tree.children[dst_idx].mdl,
                                             cnx.index_dst.model);
            } else {
                get_input_models(sc.cache.inputs,
                                 sc.mod,
                                 *tree.children[dst_idx].tn,
                                 cnx.index_dst.compo);
            }
        } else {
            get_output_models(sc.cache.outputs,
                              sc.mod,
                              *tree.children[src_idx].tn,
                              cnx.index_src.compo);

            if (tree.is_model(cnx.dst)) {
                sc.cache.inputs.emplace_back(tree.children[dst_idx].mdl,
                                             cnx.index_dst.model);
            } else {
                get_input_models(sc.cache.inputs,
                                 sc.mod,
                                 *tree.children[dst_idx].tn,
                                 cnx.index_dst.compo);
            }
        }

        irt_check(simulation_copy_connections(
          sc.cache.inputs, sc.cache.outputs, sc.sim));
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
            return simulation_copy_connections(
              sc, tree, g->children, g->connections);
    } break;

    case component_type::grid:
        if (auto* g = sc.mod.grid_components.try_to_get(compo.id.grid_id); g)
            return simulation_copy_connections(
              sc, tree, g->cache, g->cache_connections);
        break;

    case component_type::graph:
        if (auto* g = sc.mod.graph_components.try_to_get(compo.id.graph_id); g)
            return simulation_copy_connections(
              sc, tree, g->cache, g->cache_connections);
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

    while (not sc.cache.stack.empty()) {
        auto cur = sc.cache.stack.back();
        sc.cache.stack.pop_back();

        if (auto* compo = sc.mod.components.try_to_get(cur->id); compo)
            irt_check(simulation_copy_connections(sc, *cur, *compo));

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
    (void)cache;
    (void)mod;
    (void)sim;

    // sim.srcs.clear();

    // if (mod.srcs.constant_sources.can_alloc(
    //       mod.srcs.constant_sources.capacity())) {
    //     sim.srcs.constant_sources.reserve(mod.srcs.constant_sources.capacity());
    //     if (not sim.srcs.constant_sources.can_alloc(
    //           mod.srcs.constant_sources.capacity()))
    //         return new_error(external_source::part::constant_source);
    // }

    // if (mod.srcs.binary_file_sources.can_alloc(
    //       mod.srcs.binary_file_sources.capacity())) {
    //     sim.srcs.binary_file_sources.reserve(mod.srcs.binary_file_sources.capacity());
    //     if (not sim.srcs.binary_file_sources.can_alloc(
    //           mod.srcs.binary_file_sources.capacity()))
    //         return new_error(external_source::part::binary_file_source);
    // }

    // if (mod.srcs.text_file_sources.can_alloc(
    //       mod.srcs.text_file_sources.capacity())) {
    //     sim.srcs.text_file_sources.reserve(mod.srcs.text_file_sources.capacity());
    //     if (not sim.srcs.text_file_sources.can_alloc(
    //           mod.srcs.text_file_sources.capacity()))
    //         return new_error(external_source::part::text_file_source);
    // }

    // if (mod.srcs.random_sources.can_alloc(
    //       mod.srcs.random_sources.capacity())) {
    //     sim.srcs.random_sources.reserve(mod.srcs.random_sources.capacity());
    //     if (not sim.srcs.random_sources.can_alloc(
    //           mod.srcs.random_sources.capacity()))
    //         return new_error(external_source::part::random_source);
    // }

    //{
    //    constant_source* src = nullptr;
    //    while (mod.srcs.constant_sources.next(src)) {
    //        auto& n_src    = mod.srcs.constant_sources.alloc(*src);
    //        auto  src_id   = mod.srcs.constant_sources.get_id(*src);
    //        auto  n_src_id = mod.srcs.constant_sources.get_id(n_src);
    //        cache.constants.data.emplace_back(ordinal(src_id), n_src_id);
    //    }

    //    cache.constants.sort();
    //}

    //{
    //    binary_file_source* src = nullptr;
    //    while (mod.srcs.binary_file_sources.next(src)) {
    //        auto& n_src    = mod.srcs.binary_file_sources.alloc(*src);
    //        auto  src_id   = mod.srcs.binary_file_sources.get_id(*src);
    //        auto  n_src_id = mod.srcs.binary_file_sources.get_id(n_src);
    //        cache.binary_files.data.emplace_back(ordinal(src_id), n_src_id);
    //    }

    //    cache.binary_files.sort();
    //}

    //{
    //    text_file_source* src = nullptr;
    //    while (mod.srcs.text_file_sources.next(src)) {
    //        auto& n_src    = mod.srcs.text_file_sources.alloc(*src);
    //        auto  src_id   = mod.srcs.text_file_sources.get_id(*src);
    //        auto  n_src_id = mod.srcs.text_file_sources.get_id(n_src);
    //        cache.text_files.data.emplace_back(ordinal(src_id), n_src_id);
    //    }

    //    cache.text_files.sort();
    //}

    //{
    //    random_source* src = nullptr;
    //    while (mod.srcs.random_sources.next(src)) {
    //        auto& n_src    = mod.srcs.random_sources.alloc(*src);
    //        auto  src_id   = mod.srcs.random_sources.get_id(*src);
    //        auto  n_src_id = mod.srcs.random_sources.get_id(n_src);
    //        cache.randoms.data.emplace_back(ordinal(src_id), n_src_id);
    //    }

    //    cache.randoms.sort();
    //}

    return success();
}

static status make_component_cache(project& /*pj*/, modeling& mod) noexcept
{
    for (auto& grid : mod.grid_components)
        irt_check(grid.build_cache(mod));

    for (auto& graph : mod.graph_components)
        irt_check(graph.build_cache(mod));

    return success();
}

static auto make_tree_from(simulation_copy&                     sc,
                           data_array<tree_node, tree_node_id>& data,
                           component& parent) noexcept -> result<tree_node_id>
{
    if (not data.can_alloc())
        return new_error(project::error::not_enough_memory);

    auto& new_tree = data.alloc(sc.mod.components.get_id(parent), 0);
    new_tree.tree.set_id(&new_tree);

    switch (parent.type) {
    case component_type::simple: {
        auto s_id = parent.id.generic_id;
        if (auto* s = sc.mod.generic_components.try_to_get(s_id); s)
            irt_check(make_tree_recursive(sc, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = parent.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
    } break;

    case component_type::graph: {
        auto g_id = parent.id.graph_id;
        if (auto* g = sc.mod.graph_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
    } break;

    case component_type::hsm: {
        auto h_id = parent.id.hsm_id;
        if (auto* h = sc.mod.hsm_components.try_to_get(h_id); h)
            irt_check(make_tree_recursive(sc, new_tree, *h));
        break;
    }

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    return data.get_id(new_tree);
}

status project::init(const modeling_initializer& init) noexcept
{
    parameters.reserve(256);

    tree_nodes.reserve(init.tree_capacity);
    if (not tree_nodes.can_alloc())
        return new_error(project::part::tree_nodes);
    variable_observers.reserve(init.tree_capacity);
    if (not variable_observers.can_alloc())
        return new_error(project::part::variable_observers);
    grid_observers.reserve(init.tree_capacity);
    if (not grid_observers.can_alloc())
        return new_error(project::part::grid_observers);

    return success();
}

class treenode_require_computer
{
public:
    table<component_id, project::required_data> map;

    project::required_data compute(const modeling&          mod,
                                   const generic_component& g) noexcept
    {
        project::required_data ret;

        for (const auto& ch : g.children) {
            if (ch.type == child_type::component) {
                const auto* sub_c = mod.components.try_to_get(ch.id.compo_id);
                if (sub_c)
                    ret += compute(mod, *sub_c);
            } else {
                ++ret.model_nb;
            }
        }

        return ret;
    }

    project::required_data compute(const modeling&       mod,
                                   const grid_component& g) noexcept
    {
        project::required_data ret;

        for (auto r = 0; r < g.row; ++r) {
            for (auto c = 0; c < g.column; ++c) {
                const auto* sub_c =
                  mod.components.try_to_get(g.children[g.pos(r, c)]);
                if (sub_c)
                    ret += compute(mod, *sub_c);
            }
        }

        return ret;
    }

    project::required_data compute(const modeling&        mod,
                                   const graph_component& g) noexcept
    {
        project::required_data ret;

        for (const auto& v : g.children) {
            const auto* sub_c = mod.components.try_to_get(v.id);
            if (sub_c)
                ret += compute(mod, *sub_c);
        }

        return ret;
    }

public:
    project::required_data compute(const modeling&  mod,
                                   const component& c) noexcept
    {
        project::required_data ret{ .tree_node_nb = 1 };

        if (auto* ptr = map.get(mod.components.get_id(c)); ptr) {
            ret = *ptr;
        } else {
            switch (c.type) {
            case component_type::simple: {
                auto s_id = c.id.generic_id;
                if (auto* s = mod.generic_components.try_to_get(s_id); s)
                    ret += compute(mod, *s);
            } break;

            case component_type::grid: {
                auto g_id = c.id.grid_id;
                if (auto* g = mod.grid_components.try_to_get(g_id); g)
                    ret += compute(mod, *g);
            } break;

            case component_type::graph: {
                auto g_id = c.id.graph_id;
                if (auto* g = mod.graph_components.try_to_get(g_id); g)
                    ret += compute(mod, *g);
            } break;

            case component_type::hsm:
                ret.hsm_nb++;
                break;

            case component_type::internal:
                break;

            case component_type::none:
                break;
            }

            map.data.emplace_back(mod.components.get_id(c), ret);
        }

        return ret;
    }
};

project::required_data project::compute_memory_required(
  const modeling&  mod,
  const component& c) const noexcept
{
    treenode_require_computer tn;

    return tn.compute(mod, c);
}

static result<std::pair<tree_node_id, component_id>> set_project_from_hsm(
  simulation_copy& sc,
  const component& compo) noexcept
{
    const auto compo_id = sc.mod.components.get_id(compo);

    if (not sc.tree_nodes.can_alloc())
        return new_error(project::part::tree_nodes, container_full_error{});

    auto& tn = sc.tree_nodes.alloc(compo_id, 0);
    tn.tree.set_id(&tn);

    auto* com_hsm = sc.mod.hsm_components.try_to_get(compo.id.hsm_id);
    if (not com_hsm)
        return new_error(project::part::tree_nodes);

    auto* sim_hsm_id = sc.hsm_mod_to_sim.get(compo.id.hsm_id);
    if (not sim_hsm_id)
        return new_error(project::part::tree_nodes);

    auto* sim_hsm = sc.sim.hsms.try_to_get(*sim_hsm_id);
    if (not sim_hsm)
        return new_error(project::part::tree_nodes);

    auto& dyn    = sc.sim.alloc<hsm_wrapper>();
    dyn.compo_id = ordinal(sc.mod.components.get_id(compo));
    dyn.id       = (*sim_hsm_id);

    dyn.exec.i1    = com_hsm->i1;
    dyn.exec.i2    = com_hsm->i2;
    dyn.exec.r1    = com_hsm->r1;
    dyn.exec.r2    = com_hsm->r2;
    dyn.exec.timer = com_hsm->timeout;

    return std::make_pair(sc.tree_nodes.get_id(tn), compo_id);
}

status project::set(modeling& mod, simulation& sim, component& compo) noexcept
{
    clear();
    clear_cache();

    auto numbers = compute_memory_required(mod, compo);
    numbers.fix();

    if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity())) {
        tree_nodes.reserve(numbers.tree_node_nb);

        if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity()))
            return new_error(
              tree_node_error{},
              e_memory{ numbers.model_nb, tree_nodes.capacity() });
    }

    irt_check(make_component_cache(*this, mod));

    simulation_memory_requirement smr(numbers.model_nb, numbers.hsm_nb);
    sim.destroy();

    if (smr.global_b < 1024u * 1024u * 8u) {
        if (not sim.m_alloc.can_alloc_bytes(1024u * 1024u * 8u))
            return new_error(simulation::model_error{},
                             e_memory(smr.global_b, 0));

        sim.realloc(simulation_memory_requirement(1024u * 1024u * 8u));
    } else {
        if (not sim.m_alloc.can_alloc_bytes(smr.global_b))
            return new_error(simulation::model_error{},
                             e_memory(smr.global_b, 0));

        sim.realloc(smr);
    }

    simulation_copy sc(m_cache, mod, sim, tree_nodes);

    if (compo.type == component_type::hsm) {
        irt_auto(tn_compo, set_project_from_hsm(sc, compo));
        m_tn_head = tn_compo.first;
        m_head    = tn_compo.second;
    } else {
        irt_auto(id, make_tree_from(sc, tree_nodes, compo));
        const auto compo_id = sc.mod.components.get_id(compo);
        m_tn_head           = id;
        m_head              = compo_id;
    }

    irt_check(simulation_copy_sources(m_cache, mod, sim));
    irt_check(simulation_copy_connections(sc, *tn_head()));

    return success();
}

status project::rebuild(modeling& mod, simulation& sim) noexcept
{
    auto* compo = mod.components.try_to_get(head());

    return compo ? set(mod, sim, *compo) : success();
}

void project::cache::clear() noexcept
{
    stack.clear();
    inputs.clear();
    outputs.clear();

    constants.data.clear();
    binary_files.data.clear();
    text_files.data.clear();
    randoms.data.clear();
}

void project::clear() noexcept
{
    tree_nodes.clear();

    m_head    = undefined<component_id>();
    m_tn_head = undefined<tree_node_id>();

    tree_nodes.clear();
    variable_observers.clear();
    grid_observers.clear();
    graph_observers.clear();
    parameters.clear();

    m_cache.clear();
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
    debug::ensure(tree_nodes.get_id(from) != tree_nodes.get_id(to));
    debug::ensure(to.tree.get_parent() != nullptr);

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
    debug::ensure(path.ids.ssize() >= 2);

    tree_node_id ret_node_id = tree_nodes.get_id(tn);
    model_id     ret_mdl_id  = undefined<model_id>();

    const auto* from = &tn;
    const int   first =
      path.ids.ssize() - 2; // Do not read the first child of the grid
                            // component tree node. Use tn instead.

    int i = first;
    while (i >= 1) {
        const auto* ptr = from->unique_id_to_tree_node_id.get(path.ids[i]);

        if (ptr) {
            ret_node_id = *ptr;
            from        = tree_nodes.try_to_get(*ptr);
            --i;
        } else {
            break;
        }
    }

    if (i == 0 and from != nullptr) {
        const auto* mdl_id_ptr = from->unique_id_to_model_id.get(path.ids[0]);
        if (mdl_id_ptr)
            ret_mdl_id = *mdl_id_ptr;
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

    debug::ensure(tn);

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

template<typename T>
static auto already_name_exists(const T& obs, std::string_view str) noexcept
  -> bool
{
    for (const auto& o : obs)
        if (o.name == str)
            return true;

    return false;
};

template<typename T>
static void assign_name(const T& obs, name_str& str) noexcept
{
    name_str temp;

    for (auto i = 0; i < INT32_MAX; ++i) {
        format(temp, "New {}", i);

        if (not already_name_exists(obs, temp.sv())) {
            str = temp;
            return;
        }
    }

    str = "New";
};

variable_observer& project::alloc_variable_observer() noexcept
{
    debug::ensure(variable_observers.can_alloc());

    auto& obs = variable_observers.alloc();
    assign_name(variable_observers, obs.name);
    return obs;
}

grid_observer& project::alloc_grid_observer() noexcept
{
    debug::ensure(grid_observers.can_alloc());

    auto& obs = grid_observers.alloc();
    assign_name(grid_observers, obs.name);
    return obs;
}

graph_observer& project::alloc_graph_observer() noexcept
{
    debug::ensure(graph_observers.can_alloc());

    auto& obs = graph_observers.alloc();
    assign_name(graph_observers, obs.name);
    return obs;
}

std::string_view to_string(const project::part p) noexcept
{
    using integer = std::underlying_type_t<project::part>;

    static const std::string_view str[] = { "tree nodes",
                                            "variable observers",
                                            "grid observers",
                                            "graph observers",
                                            "global parameters" };

    debug::ensure(std::cmp_less(static_cast<integer>(p), 5));

    return str[static_cast<integer>(p)];
};

std::string_view to_string(const project::error e) noexcept
{
    using integer = std::underlying_type_t<project::error>;

    static const std::string_view str[] = {
        "not enough memory",
        "unknown source",
        "impossible connection",
        "empty project",
        "component empty",
        "component type error",
        "file error",
        "file component type error",
        "registred path access error",
        "directory access error",
        "file access error",
        "file open error",
        "file parameters error",
        "file parameters access error",
        "file parameters type error",
        "file parameters init error",
    };

    debug::ensure(std::cmp_less(static_cast<integer>(e), 16));

    return str[static_cast<integer>(e)];
}

} // namespace irt
