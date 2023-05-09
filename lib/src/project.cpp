// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

struct simulation_copy
{
    simulation_copy(project::cache&                      cache_,
                    modeling&                            mod_,
                    simulation&                          sim_,
                    data_array<tree_node, tree_node_id>& tree_nodes_) noexcept
      : cache(cache_)
      , mod(mod_)
      , sim(sim_)
      , tree_nodes(tree_nodes_)
    {
    }

    project::cache& cache;
    modeling&       mod;
    simulation&     sim;

    data_array<tree_node, tree_node_id>& tree_nodes;
};

static status simulation_copy_source(simulation_copy& sc,
                                     const source&    src,
                                     source&          dst) noexcept;

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       parent,
                                  component&       compo,
                                  child_id         id_in_parent,
                                  u64              unique_id) noexcept;

static status make_tree_leaf(simulation_copy& sc,
                             tree_node&       parent,
                             model&           mod_mdl,
                             child&           ch) noexcept
{
    irt_return_if_fail(sc.sim.models.can_alloc(),
                       status::simulation_not_enough_model);

    if (mod_mdl.type == dynamics_type::hsm_wrapper)
        irt_return_if_fail(sc.sim.hsms.can_alloc(1),
                           status::simulation_not_enough_model);

    auto& new_mdl    = sc.sim.models.alloc();
    auto  new_mdl_id = sc.sim.models.get_id(new_mdl);
    new_mdl.type     = mod_mdl.type;
    new_mdl.handle   = nullptr;

    dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(mod_mdl);
        std::construct_at(&dyn, src_dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            if (auto* hsm_src = sc.mod.hsms.try_to_get(src_dyn.id); hsm_src) {
                auto& hsm = sc.sim.hsms.alloc(*hsm_src);
                auto  id  = sc.sim.hsms.get_id(hsm);
                dyn.id    = id;
            } else {
                auto& hsm = sc.sim.hsms.alloc();
                auto  id  = sc.sim.hsms.get_id(hsm);
                dyn.id    = id;
            }
        }

        if constexpr (std::is_same_v<Dynamics, generator>) {
            simulation_copy_source(
              sc, src_dyn.default_source_ta, dyn.default_source_ta);
            simulation_copy_source(
              sc, src_dyn.default_source_value, dyn.default_source_value);
        }

        if constexpr (std::is_same_v<Dynamics, dynamic_queue>) {
            simulation_copy_source(
              sc, src_dyn.default_source_ta, dyn.default_source_ta);
        }

        if constexpr (std::is_same_v<Dynamics, priority_queue>) {
            simulation_copy_source(
              sc, src_dyn.default_source_ta, dyn.default_source_ta);
        }
    });

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

    if (ch.flags & child_flags_configurable) {
        auto& x     = parent.parameters.emplace_back();
        x.unique_id = ch.unique_id;
        x.mdl_id    = new_mdl_id;
        copy(mod_mdl, x.param);
    }

    if (ch.flags & child_flags_observable) {
        auto& x     = parent.observables.emplace_back();
        x.unique_id = ch.unique_id;
        x.mdl_id    = new_mdl_id;
        x.param     = observable_type::single;
    }

    return status::success;
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
                    irt_return_if_bad(make_tree_recursive(
                      sc, new_tree, *compo, child_id, child->unique_id));
                }
            } else {
                auto mdl_id = child->id.mdl_id;
                if (auto* mdl = sc.mod.models.try_to_get(mdl_id); mdl) {
                    irt_return_if_bad(
                      make_tree_leaf(sc, new_tree, *mdl, *child));
                }
            }
        }
    }

    new_tree.child_to_node.sort();
    new_tree.child_to_sim.sort();

    return status::success;
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  grid_component&  src) noexcept
{
    for (auto child_id : src.cache) {
        if (auto* child = sc.mod.children.try_to_get(child_id); child) {
            if (child->type == child_type::component) {
                auto compo_id = child->id.compo_id;
                if (auto* compo = sc.mod.components.try_to_get(compo_id);
                    compo) {
                    irt_return_if_bad(make_tree_recursive(
                      sc, new_tree, *compo, child_id, child->unique_id));
                }
            } else {
                auto mdl_id = child->id.mdl_id;
                if (auto* mdl = sc.mod.models.try_to_get(mdl_id); mdl) {
                    irt_return_if_bad(
                      make_tree_leaf(sc, new_tree, *mdl, *child));
                }
            }
        }
    }

    new_tree.child_to_node.sort();
    new_tree.child_to_sim.sort();

    return status::success;
}

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       parent,
                                  component&       compo,
                                  child_id         id_in_parent,
                                  u64              unique_id) noexcept
{
    irt_return_if_fail(sc.tree_nodes.can_alloc(),
                       status::data_array_not_enough_memory);

    auto& new_tree =
      sc.tree_nodes.alloc(sc.mod.components.get_id(compo), unique_id);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    auto& x    = parent.child_to_node.data.emplace_back();
    x.id       = id_in_parent;
    x.value.tn = &new_tree;

    switch (compo.type) {
    case component_type::simple: {
        auto s_id = compo.id.simple_id;
        if (auto* s = sc.mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(sc, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(sc, new_tree, *g));
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    return status::success;
}

static status simulation_copy_source(simulation_copy& sc,
                                     const source&    src,
                                     source&          dst) noexcept
{
    switch (src.type) {
    case source::source_type::none:
        break;
    case source::source_type::constant:
        if (auto* ret = sc.cache.constants.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::binary_file:
        if (auto* ret = sc.cache.binary_files.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::text_file:
        if (auto* ret = sc.cache.text_files.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::random:
        if (auto* ret = sc.cache.randoms.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    }

    irt_bad_return(status::source_unknown);
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

static status get_input_models(simulation_copy&               sc,
                               vector<std::pair<model*, i8>>& inputs,
                               tree_node&                     tree,
                               i8                             port_dst);

static status get_output_models(simulation_copy&               sc,
                                vector<std::pair<model*, i8>>& outputs,
                                tree_node&                     tree,
                                i8                             port_dst);

static status get_input_models(simulation_copy&               sc,
                               vector<std::pair<model*, i8>>& inputs,
                               tree_node&                     tree,
                               child_id                       ch,
                               i8                             port)
{
    status ret = status::success;

    if (auto* c = sc.mod.children.try_to_get(ch); c) {
        auto* node = tree.child_to_node.get(ch);
        irt_assert(node);

        if (c->type == child_type::model) {
            inputs.emplace_back(std::make_pair(node->mdl, port));
        } else {
            ret = get_input_models(sc, inputs, *node->tn, port);
        }
    }

    return ret;
}

static status get_output_models(simulation_copy&               sc,
                                vector<std::pair<model*, i8>>& outputs,
                                tree_node&                     tree,
                                child_id                       ch,
                                i8                             port)
{
    status ret = status::success;

    if (auto* c = sc.mod.children.try_to_get(ch); c) {
        auto* node = tree.child_to_node.get(ch);
        irt_assert(node);

        if (c->type == child_type::model) {
            outputs.emplace_back(std::make_pair(node->mdl, port));
        } else {
            ret = get_output_models(sc, outputs, *node->tn, port);
        }
    }

    return ret;
}

static status get_input_models(simulation_copy&               sc,
                               vector<std::pair<model*, i8>>& inputs,
                               tree_node&                     tree,
                               i8                             port_dst)
{
    if (auto* compo = sc.mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g =
                  sc.mod.simple_components.try_to_get(compo->id.simple_id);
                g) {
                for (auto cnx_id : g->connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            irt_return_if_bad(
                              get_input_models(sc,
                                               inputs,
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
                            irt_return_if_bad(
                              get_input_models(sc,
                                               inputs,
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
        }
    }

    return status::success;
}

static status get_output_models(simulation_copy&               sc,
                                vector<std::pair<model*, i8>>& outputs,
                                tree_node&                     tree,
                                i8                             port_dst)
{
    if (auto* compo = sc.mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g =
                  sc.mod.simple_components.try_to_get(compo->id.simple_id);
                g) {
                for (auto cnx_id : g->connections) {
                    if (auto* cnx = sc.mod.connections.try_to_get(cnx_id);
                        cnx) {
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            irt_return_if_bad(
                              get_output_models(sc,
                                                outputs,
                                                tree,
                                                cnx->input.dst,
                                                cnx->input.index_dst));
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
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            irt_return_if_bad(
                              get_output_models(sc,
                                                outputs,
                                                tree,
                                                cnx->input.dst,
                                                cnx->input.index_dst));
                        }
                    }
                }
            }
            break;

        case component_type::internal:
            break;

        case component_type::none:
            break;
        }
    }

    return status::success;
}

static status simulation_copy_connections(
  const vector<std::pair<model*, i8>>& inputs,
  const vector<std::pair<model*, i8>>& outputs,
  simulation&                          sim) noexcept
{
    for (auto src : outputs)
        for (auto dst : inputs)
            irt_return_if_bad(
              sim.connect(*src.first, src.second, *dst.first, dst.second));

    return status::success;
}

static status simulation_copy_connections(simulation_copy&       sc,
                                          tree_node&             tree,
                                          vector<connection_id>& connections)
{
    for (auto cnx_id : connections) {
        sc.cache.inputs.clear();
        sc.cache.outputs.clear();

        auto* cnx = sc.mod.connections.try_to_get(cnx_id);

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
                  std::make_pair(node_src->mdl, cnx->internal.index_src));

                if (dst->type == child_type::model) {
                    sc.cache.inputs.emplace_back(
                      std::make_pair(node_dst->mdl, cnx->internal.index_dst));
                } else {
                    get_input_models(sc,
                                     sc.cache.inputs,
                                     *node_dst->tn,
                                     cnx->internal.index_dst);
                }
            } else {
                get_output_models(
                  sc, sc.cache.outputs, *node_src->tn, cnx->internal.index_src);

                if (dst->type == child_type::model) {
                    sc.cache.inputs.emplace_back(
                      std::make_pair(node_dst->mdl, cnx->internal.index_dst));
                } else {
                    get_input_models(sc,
                                     sc.cache.inputs,
                                     *node_dst->tn,
                                     cnx->internal.index_dst);
                }
            }

            if (auto ret = simulation_copy_connections(
                  sc.cache.inputs, sc.cache.outputs, sc.sim);
                is_bad(ret))
                return ret;
        }
    }

    return status::success;
}

static status simulation_copy_connections(simulation_copy& sc,
                                          tree_node&       tree,
                                          component&       compo)
{
    switch (compo.type) {
    case component_type::simple: {
        if (auto* g = sc.mod.simple_components.try_to_get(compo.id.simple_id);
            g)
            return simulation_copy_connections(sc, tree, g->connections);
    } break;

    case component_type::grid: {
        if (auto* g = sc.mod.grid_components.try_to_get(compo.id.grid_id); g)
            return simulation_copy_connections(sc, tree, g->cache_connections);
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    return status::success;
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
            irt_return_if_bad(simulation_copy_connections(sc, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            sc.cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            sc.cache.stack.emplace_back(child);
    }

    return status::success;
}

static status simulation_copy_sources(project::cache& cache,
                                      modeling&       mod,
                                      simulation&     sim) noexcept
{
    sim.srcs.clear();

    sim.srcs.constant_sources.init(mod.srcs.constant_sources.capacity());
    sim.srcs.binary_file_sources.init(mod.srcs.binary_file_sources.capacity());
    sim.srcs.text_file_sources.init(mod.srcs.text_file_sources.capacity());
    sim.srcs.random_sources.init(mod.srcs.random_sources.capacity());

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

    return status::success;
}

static status make_component_cache(project& /*pj*/, modeling& mod) noexcept
{
    grid_component* grid = nullptr;
    while (mod.grid_components.next(grid))
        irt_return_if_bad(mod.build_grid_component_cache(*grid));

    return status::success;
}

static status make_tree_from(simulation_copy&                     sc,
                             data_array<tree_node, tree_node_id>& data,
                             component&                           parent,
                             tree_node_id*                        out) noexcept
{
    irt_return_if_fail(data.can_alloc(), status::data_array_not_enough_memory);

    auto& new_tree    = data.alloc(sc.mod.components.get_id(parent), 0);
    auto  new_tree_id = data.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);

    switch (parent.type) {
    case component_type::simple: {
        auto s_id = parent.id.simple_id;
        if (auto* s = sc.mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(sc, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = parent.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(sc, new_tree, *g));
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    *out = new_tree_id;

    return status::success;
}

status project::init(int size) noexcept
{
    irt_return_if_bad(m_tree_nodes.init(size));

    return status::success;
}

status project::set(modeling& mod, simulation& sim, component& compo) noexcept
{
    clear();
    clear_cache();
    clean_simulation();
    mod.clean_simulation();

    irt_return_if_bad(make_component_cache(*this, mod));

    simulation_copy sc(m_cache, mod, sim, m_tree_nodes);
    tree_node_id    id  = undefined<tree_node_id>();
    auto            ret = make_tree_from(sc, m_tree_nodes, compo, &id);

    if (is_success(ret)) {
        m_head    = mod.components.get_id(compo);
        m_tn_head = id;

        irt_return_if_bad(simulation_copy_sources(m_cache, mod, sim));
        irt_return_if_bad(simulation_copy_connections(sc, *tn_head()));
    }

    return ret;
}

status project::rebuild(modeling& mod, simulation& sim) noexcept
{
    clear();
    clear_cache();
    clean_simulation();
    mod.clean_simulation();

    irt_return_if_bad(make_component_cache(*this, mod));

    if (auto* compo = mod.components.try_to_get(head()); compo) {
        simulation_copy sc(m_cache, mod, sim, m_tree_nodes);
        tree_node_id    id  = undefined<tree_node_id>();
        auto            ret = make_tree_from(sc, m_tree_nodes, *compo, &id);

        if (is_success(ret)) {
            m_head    = mod.components.get_id(*compo);
            m_tn_head = id;

            irt_return_if_bad(simulation_copy_sources(m_cache, mod, sim));
            irt_return_if_bad(simulation_copy_connections(sc, *tn_head()));
        }

        return ret;
    }

    return status::success;
}

void project::clear() noexcept
{
    m_tree_nodes.clear();

    m_head    = undefined<component_id>();
    m_tn_head = undefined<tree_node_id>();
}

void project::clean_simulation() noexcept
{
    for_all_tree_nodes([](auto& tn) { tn.child_to_node.data.clear(); });
}

status project::load(modeling&   mod,
                     simulation& sim,
                     io_cache&   cache,
                     const char* filename) noexcept
{
    return project_load(*this, mod, sim, cache, filename);
}

status project::save(modeling&   mod,
                     simulation& sim,
                     io_cache&   cache,
                     const char* filename) noexcept
{
    return project_save(*this, mod, sim, cache, filename);
}

} // namespace irt
