// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

constexpr static inline int pos(int row, int col, int rows) noexcept
{
    return col * rows + row;
}

static status make_tree_recursive(project&   pj,
                                  modeling&  mod,
                                  tree_node& parent,
                                  component& compo,
                                  child_id   id_in_parent) noexcept;

static status make_tree_recursive(project&          pj,
                                  modeling&         mod,
                                  tree_node&        new_tree,
                                  simple_component& src) noexcept
{
    for (auto child_id : src.children) {
        auto* child        = mod.children.try_to_get(child_id);
        auto  is_component = child && child->type == child_type::component;

        if (is_component) {
            auto compo_id = child->id.compo_id;
            if (auto* compo = mod.components.try_to_get(compo_id); compo)
                irt_return_if_bad(
                  make_tree_recursive(pj, mod, new_tree, *compo, child_id));
        }
    }

    return status::success;
}

static status make_tree_recursive(project&        pj,
                                  modeling&       mod,
                                  tree_node&      new_tree,
                                  grid_component& src) noexcept
{
    for (auto id : src.cache) {
        if (auto* c = mod.children.try_to_get(id); c)
            if (c->type == child_type::component) {
                if (auto* cp = mod.components.try_to_get(c->id.compo_id); cp)
                    irt_return_if_bad(
                      make_tree_recursive(pj, mod, new_tree, *cp, id));
            }
    }

    return status::success;
}

static status make_tree_recursive(project&   pj,
                                  modeling&  mod,
                                  tree_node& parent,
                                  component& compo,
                                  child_id   id_in_parent) noexcept
{
    irt_return_if_fail(pj.tree_nodes.can_alloc(),
                       status::data_array_not_enough_memory);

    auto& new_tree =
      pj.tree_nodes.alloc(mod.components.get_id(compo), id_in_parent);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    switch (compo.type) {
    case component_type::simple: {
        auto s_id = compo.id.simple_id;
        if (auto* s = mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(pj, mod, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(pj, mod, new_tree, *g));
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    return status::success;
}

static status simulation_copy_source(modeling_to_simulation& cache,
                                     const source&           src,
                                     source&                 dst) noexcept
{
    switch (src.type) {
    case source::source_type::none:
        break;
    case source::source_type::constant:
        if (auto* ret = cache.constants.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::binary_file:
        if (auto* ret = cache.binary_files.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::text_file:
        if (auto* ret = cache.text_files.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    case source::source_type::random:
        if (auto* ret = cache.randoms.get(src.id); ret) {
            dst.id = ordinal(*ret);
            return status::success;
        }
        break;
    }

    irt_bad_return(status::source_unknown);
}

static status simulation_copy_grid_model(modeling&               mod,
                                         modeling_to_simulation& cache,
                                         simulation&             sim,
                                         tree_node&              tree,
                                         grid_component&         src) noexcept
{
    irt_assert(tree.children.empty());

    for (int row = 0; row < src.row; ++row) {
        for (int col = 0; col < src.column; ++col) {
            auto  id = src.cache[row * src.column + col];
            auto* c  = mod.children.try_to_get(id);

            if (!c || c->type == child_type::component)
                continue;

            auto  mdl_id = c->id.mdl_id;
            auto* mdl    = mod.models.try_to_get(mdl_id);

            if (!mdl)
                continue;

            irt_return_if_fail(sim.models.can_alloc(),
                               status::simulation_not_enough_model);

            if (mdl->type == dynamics_type::hsm_wrapper)
                irt_return_if_fail(sim.hsms.can_alloc(1),
                                   status::simulation_not_enough_model);

            auto& new_mdl    = sim.models.alloc();
            auto  new_mdl_id = sim.models.get_id(new_mdl);
            new_mdl.type     = mdl->type;
            new_mdl.handle   = nullptr;

            dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
                const auto& src_dyn = get_dyn<Dynamics>(*mdl);
                std::construct_at(&dyn, src_dyn);

                if constexpr (has_input_port<Dynamics>)
                    for (int i = 0, e = length(dyn.x); i != e; ++i)
                        dyn.x[i] = static_cast<u64>(-1);

                if constexpr (has_output_port<Dynamics>)
                    for (int i = 0, e = length(dyn.y); i != e; ++i)
                        dyn.y[i] = static_cast<u64>(-1);

                if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                    if (auto* hsm_src = mod.hsms.try_to_get(src_dyn.id);
                        hsm_src) {
                        auto& hsm = sim.hsms.alloc(*hsm_src);
                        auto  id  = sim.hsms.get_id(hsm);
                        dyn.id    = id;
                    } else {
                        auto& hsm = sim.hsms.alloc();
                        auto  id  = sim.hsms.get_id(hsm);
                        dyn.id    = id;
                    }
                }

                if constexpr (std::is_same_v<Dynamics, generator>) {
                    simulation_copy_source(
                      cache, src_dyn.default_source_ta, dyn.default_source_ta);
                    simulation_copy_source(cache,
                                           src_dyn.default_source_value,
                                           dyn.default_source_value);
                }

                if constexpr (std::is_same_v<Dynamics, dynamic_queue>) {
                    simulation_copy_source(
                      cache, src_dyn.default_source_ta, dyn.default_source_ta);
                }

                if constexpr (std::is_same_v<Dynamics, priority_queue>) {
                    simulation_copy_source(
                      cache, src_dyn.default_source_ta, dyn.default_source_ta);
                }
            });

            tree.children.emplace_back(new_mdl_id);
            tree.sim.data.emplace_back(mdl_id, new_mdl_id);
        }
    }

    return status::success;
}

static status simulation_copy_simple_model(modeling&               mod,
                                           modeling_to_simulation& cache,
                                           simulation&             sim,
                                           tree_node&              tree,
                                           simple_component&       src) noexcept
{
    irt_assert(tree.children.empty());

    for (auto child_id : src.children) {
        auto* c = mod.children.try_to_get(child_id);
        if (!c)
            continue;

        if (c->type == child_type::component)
            continue;

        auto  mdl_id = c->id.mdl_id;
        auto* mdl    = mod.models.try_to_get(mdl_id);

        if (!mdl)
            continue;

        irt_return_if_fail(sim.models.can_alloc(),
                           status::simulation_not_enough_model);

        if (mdl->type == dynamics_type::hsm_wrapper)
            irt_return_if_fail(sim.hsms.can_alloc(1),
                               status::simulation_not_enough_model);

        auto& new_mdl    = sim.models.alloc();
        auto  new_mdl_id = sim.models.get_id(new_mdl);
        new_mdl.type     = mdl->type;
        new_mdl.handle   = nullptr;

        dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
            const auto& src_dyn = get_dyn<Dynamics>(*mdl);
            std::construct_at(&dyn, src_dyn);

            if constexpr (has_input_port<Dynamics>)
                for (int i = 0, e = length(dyn.x); i != e; ++i)
                    dyn.x[i] = static_cast<u64>(-1);

            if constexpr (has_output_port<Dynamics>)
                for (int i = 0, e = length(dyn.y); i != e; ++i)
                    dyn.y[i] = static_cast<u64>(-1);

            if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                if (auto* hsm_src = mod.hsms.try_to_get(src_dyn.id); hsm_src) {
                    auto& hsm = sim.hsms.alloc(*hsm_src);
                    auto  id  = sim.hsms.get_id(hsm);
                    dyn.id    = id;
                } else {
                    auto& hsm = sim.hsms.alloc();
                    auto  id  = sim.hsms.get_id(hsm);
                    dyn.id    = id;
                }
            }

            if constexpr (std::is_same_v<Dynamics, generator>) {
                simulation_copy_source(
                  cache, src_dyn.default_source_ta, dyn.default_source_ta);
                simulation_copy_source(cache,
                                       src_dyn.default_source_value,
                                       dyn.default_source_value);
            }

            if constexpr (std::is_same_v<Dynamics, dynamic_queue>) {
                simulation_copy_source(
                  cache, src_dyn.default_source_ta, dyn.default_source_ta);
            }

            if constexpr (std::is_same_v<Dynamics, priority_queue>) {
                simulation_copy_source(
                  cache, src_dyn.default_source_ta, dyn.default_source_ta);
            }
        });

        tree.children.emplace_back(new_mdl_id);
        tree.sim.data.emplace_back(mdl_id, new_mdl_id);
    }

    return status::success;
}

void modeling_to_simulation::clear() noexcept
{
    stack.clear();
    inputs.clear();
    outputs.clear();

    constants.data.clear();
    binary_files.data.clear();
    text_files.data.clear();
    randoms.data.clear();
}

void modeling_to_simulation::destroy() noexcept
{
    // @TODO homogeneize destroy functions in all irritator container.

    // stack.destroy();
    // inputs.destroy();
    // outputs.destroy();
    // sim_tree_nodes.destroy();

    clear();
}

static status simulation_copy_models(modeling_to_simulation& cache,
                                     project&                pj,
                                     modeling&               mod,
                                     simulation&             sim,
                                     tree_node&              head) noexcept
{
    cache.stack.clear();
    cache.stack.emplace_back(&head);

    while (!cache.stack.empty()) {
        auto cur = cache.stack.back();
        cache.stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = mod.components.try_to_get(compo_id);

        if (compo) {
            switch (compo->type) {
            case component_type::none:
                break;

            case component_type::internal:
                break;

            case component_type::grid: {
                auto g_id = compo->id.grid_id;
                if (auto* g = mod.grid_components.try_to_get(g_id); g) {
                    irt_return_if_bad(
                      simulation_copy_grid_model(mod, cache, sim, *cur, *g));
                }
            } break;

            case component_type::simple: {
                auto  s_compo_id = compo->id.simple_id;
                auto* s_compo    = mod.simple_components.try_to_get(s_compo_id);

                if (s_compo) {
                    irt_return_if_bad(simulation_copy_simple_model(
                      mod, cache, sim, *cur, *s_compo));
                }
            } break;
            }
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    tree_node* tree = nullptr;
    while (pj.tree_nodes.next(tree))
        tree->sim.sort();

    return status::success;
}

static auto get_treenode(tree_node& parent, child_id to_search) noexcept
  -> tree_node*
{
    for (auto* child = parent.tree.get_child(); child;
         child       = child->tree.get_sibling()) {
        if (child->id_in_parent == to_search)
            return child;
    }

    return nullptr;
}

static auto get_component(modeling& mod, child& c) noexcept -> component*
{
    auto compo_id = c.id.compo_id;

    return mod.components.try_to_get(compo_id);
}

static auto get_simulation_model(tree_node&  parent,
                                 simulation& sim,
                                 child&      ch) noexcept -> model*
{
    auto mod_model_id = ch.id.mdl_id;
    if (auto* sim_model_id = parent.sim.get(mod_model_id); sim_model_id)
        return sim.models.try_to_get(*sim_model_id);

    return nullptr;
}

struct model_to_component_connect
{
    modeling&   mod;
    simulation& sim;
    model_id    mdl_id;
    i8          port;
};

static auto input_connect(modeling&                   mod,
                          model_to_component_connect& ic,
                          simple_component&           compo,
                          tree_node&                  tree,
                          i8 port_dst) noexcept -> status
{
    for (auto connection_id : compo.connections) {
        auto* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (!(con->type == connection::connection_type::input &&
              port_dst == con->input.index))
            continue;

        auto* child = mod.children.try_to_get(con->input.dst);
        if (!child)
            continue;

        irt_return_if_fail(child->type == child_type::model,
                           status::model_connect_bad_dynamics);

        auto* sim_mod = get_simulation_model(tree, ic.sim, *child);
        if (!sim_mod)
            continue;

        auto sim_mod_id = ic.sim.models.get_id(*sim_mod);

        irt_return_if_bad(
          ic.sim.connect(ic.mdl_id, ic.port, sim_mod_id, con->input.index_dst));
    }

    return status::success;
}

static auto output_connect(modeling&                   mod,
                           model_to_component_connect& ic,
                           simple_component&           compo,
                           tree_node&                  tree,
                           i8 port_dst) noexcept -> status
{
    for (auto connection_id : compo.connections) {
        auto* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (!(con->type == connection::connection_type::output &&
              port_dst == con->output.index))
            continue;

        auto* child = mod.children.try_to_get(con->output.src);
        if (!child)
            continue;

        irt_return_if_fail(child->type == child_type::model,
                           status::model_connect_bad_dynamics);

        auto* sim_mod = get_simulation_model(tree, ic.sim, *child);
        if (!sim_mod)
            continue;

        auto sim_mod_id = ic.sim.models.get_id(*sim_mod);

        irt_return_if_bad(ic.sim.connect(
          sim_mod_id, con->output.index_src, ic.mdl_id, ic.port));
    }

    return status::success;
}

static auto get_input_model_from_component(modeling&               mod,
                                           modeling_to_simulation& cache,
                                           simulation&             sim,
                                           simple_component&       compo,
                                           tree_node&              tree,
                                           i8 port) noexcept -> status
{
    cache.inputs.clear();

    for (auto connection_id : compo.connections) {
        auto* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::input &&
            con->input.index == port) {
            auto  child_id = con->input.dst;
            auto* child    = mod.children.try_to_get(child_id);
            if (!child)
                continue;

            irt_assert(child->type == child_type::model);

            auto  mod_model_id = child->id.mdl_id;
            auto* sim_model_id = tree.sim.get(mod_model_id);
            irt_assert(sim_model_id);

            auto* sim_model = sim.models.try_to_get(*sim_model_id);
            irt_assert(sim_model);

            cache.inputs.emplace_back(
              std::make_pair(*sim_model_id, con->input.index_dst));
        }
    }

    return status::success;
}

static auto get_output_model_from_component(modeling&               mod,
                                            modeling_to_simulation& cache,
                                            simulation&             sim,
                                            simple_component&       compo,
                                            tree_node&              tree,
                                            i8 port) noexcept -> status
{
    cache.outputs.clear();

    for (auto connection_id : compo.connections) {
        auto* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port) {
            auto  child_id = con->output.src;
            auto* child    = mod.children.try_to_get(child_id);
            if (!child)
                continue;

            irt_assert(child->type == child_type::model);

            auto  mod_model_id = child->id.mdl_id;
            auto* sim_model_id = tree.sim.get(mod_model_id);
            irt_assert(sim_model_id);

            auto* sim_model = sim.models.try_to_get(*sim_model_id);
            irt_assert(sim_model);

            cache.outputs.emplace_back(
              std::make_pair(*sim_model_id, con->output.index_src));
        }
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              tree,
                                          simple_component&       compo)
{
    for (auto connection_id : compo.connections) {
        auto* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::internal) {
            child* src = mod.children.try_to_get(con->internal.src);
            child* dst = mod.children.try_to_get(con->internal.dst);

            if (!(src && dst))
                continue;

            if (src->type == child_type::model) {
                model* m_src = get_simulation_model(tree, sim, *src);
                if (!m_src)
                    continue;

                if (dst->type == child_type::model) {
                    model* m_dst = get_simulation_model(tree, sim, *dst);
                    if (!m_dst)
                        continue;

                    irt_return_if_bad(sim.connect(*m_src,
                                                  con->internal.index_src,
                                                  *m_dst,
                                                  con->internal.index_dst));
                } else {
                    auto* c_dst = get_component(mod, *dst);
                    auto* t_dst = get_treenode(tree, con->internal.dst);

                    if (!(c_dst && t_dst))
                        continue;

                    auto  sc_dst_id = c_dst->id.simple_id;
                    auto* sc_dst = mod.simple_components.try_to_get(sc_dst_id);
                    if (!sc_dst)
                        continue;

                    model_to_component_connect ic{ .mod    = mod,
                                                   .sim    = sim,
                                                   .mdl_id = sim.get_id(*m_src),
                                                   .port =
                                                     con->internal.index_src };

                    irt_return_if_bad(input_connect(
                      mod, ic, *sc_dst, *t_dst, con->internal.index_dst));
                }
            } else {
                auto* c_src = get_component(mod, *src);
                auto* t_src = get_treenode(tree, con->internal.src);

                if (!(c_src && t_src))
                    continue;

                auto  sc_src_id = c_src->id.simple_id;
                auto* sc_src    = mod.simple_components.try_to_get(sc_src_id);

                if (sc_src)
                    continue;

                if (dst->type == child_type::model) {
                    model* m_dst = get_simulation_model(tree, sim, *dst);
                    if (!m_dst)
                        continue;

                    model_to_component_connect oc{ .mod    = mod,
                                                   .sim    = sim,
                                                   .mdl_id = sim.get_id(*m_dst),
                                                   .port =
                                                     con->internal.index_dst };

                    irt_return_if_bad(output_connect(
                      mod, oc, *sc_src, *t_src, con->internal.index_src));
                } else {
                    auto* c_dst = get_component(mod, *dst);
                    auto* t_dst = get_treenode(tree, con->internal.dst);
                    auto* c_src = get_component(mod, *src);
                    auto* t_src = get_treenode(tree, con->internal.src);

                    if (!(c_dst && t_dst && c_src && t_src))
                        continue;

                    auto  sc_dst_id = c_dst->id.simple_id;
                    auto* sc_dst = mod.simple_components.try_to_get(sc_dst_id);
                    auto  sc_src_id = c_src->id.simple_id;
                    auto* sc_src = mod.simple_components.try_to_get(sc_src_id);

                    if (!(sc_src && sc_dst))
                        continue;

                    irt_return_if_bad(
                      get_input_model_from_component(mod,
                                                     cache,
                                                     sim,
                                                     *sc_dst,
                                                     *t_dst,
                                                     con->internal.index_dst));
                    irt_return_if_bad(
                      get_output_model_from_component(mod,
                                                      cache,
                                                      sim,
                                                      *sc_src,
                                                      *t_src,
                                                      con->internal.index_src));

                    for (auto& out : cache.outputs) {
                        for (auto& in : cache.inputs) {
                            irt_return_if_bad(sim.connect(
                              out.first, out.second, in.first, in.second));
                        }
                    }
                }
            }
        }
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              head) noexcept
{
    cache.stack.clear();
    cache.stack.emplace_back(&head);

    while (!cache.stack.empty()) {
        auto cur = cache.stack.back();
        cache.stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = mod.components.try_to_get(compo_id);

        if (compo && compo->type == component_type::simple) {
            auto  s_compo_id = compo->id.simple_id;
            auto* s_compo    = mod.simple_components.try_to_get(s_compo_id);

            irt_return_if_bad(
              simulation_copy_connections(cache, mod, sim, *cur, *s_compo));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    return status::success;
}

// static auto compute_simulation_copy_models_number(modeling_to_simulation&
// cache,
//                                                   const modeling& mod,
//                                                   tree_node& head) noexcept
//   -> std::pair<int, int>
// {
//     int models      = 0;
//     int connections = 0;

//     cache.stack.clear();
//     cache.stack.emplace_back(&head);

//     while (!cache.stack.empty()) {
//         auto cur = cache.stack.back();
//         cache.stack.pop_back();

//         auto  compo_id = cur->id;
//         auto* compo    = mod.components.try_to_get(compo_id);

//         if (compo) {
//             auto  s_compo_id = compo->id.simple_id;
//             auto* s_compo    = mod.simple_components.try_to_get(s_compo_id);

//             if (s_compo) {
//                 models += s_compo->children.ssize();
//                 connections += s_compo->connections.ssize();
//             }
//         }

//         if (auto* sibling = cur->tree.get_sibling(); sibling)
//             cache.stack.emplace_back(sibling);

//         if (auto* child = cur->tree.get_child(); child)
//             cache.stack.emplace_back(child);
//     }

//     return std::make_pair(models, connections);
// }

static status simulation_copy_sources(modeling_to_simulation& cache,
                                      modeling&               mod,
                                      simulation&             sim) noexcept
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

//! Clear children list and map between component model -> simulation model.
static void simulation_clear_tree(project& pj, modeling& mod) noexcept
{
    grid_component* grid = nullptr;
    while (mod.grid_components.next(grid)) {
        for (auto id : grid->cache)
            mod.children.free(id);

        for (auto id : grid->cache_connections)
            mod.connections.free(id);

        grid->cache.clear();
        grid->cache_connections.clear();
    }

    pj.tree_nodes.clear();
}

//! Build the project hierarchy from @c top as head of the hierarchy.
//!
//! For grid_component, build the real children and connections grid
//! based on default_chidren and specific_children vectors and grid_component
//! options (torus, cylinder etc.).
static status simulation_initialize_tree(project&   pj,
                                         modeling&  mod,
                                         component& top) noexcept
{
    grid_component* grid = nullptr;
    while (mod.grid_components.next(grid)) {
        // @TODO add a grid_component_access_error
        irt_return_if_fail(grid->row && grid->column,
                           status::io_project_file_error);
        irt_return_if_fail(mod.children.can_alloc(grid->row * grid->column),
                           status::data_array_not_enough_memory);
        irt_return_if_fail(
          mod.connections.can_alloc(grid->row * grid->column * 4),
          status::data_array_not_enough_memory);

        for (auto& elem : grid->cache)
            if (auto* c = mod.children.try_to_get(elem); c)
                mod.free(*c);

        for (auto& con : grid->cache_connections)
            if (auto* c = mod.connections.try_to_get(con); c)
                mod.free(*c);

        grid->cache.resize(grid->row * grid->column);
        grid->cache_connections.clear();

        int x = 0, y = 0;

        for (int row = 0; row < grid->row; ++row) {
            if (row == 0)
                y = 0;
            else if (1 <= row && row + 1 < grid->row)
                y = 1;
            else
                y = 2;

            for (int col = 0; col < grid->column; ++col) {
                if (col == 0)
                    x = 0;
                else if (1 <= col && col + 1 < grid->column)
                    x = 1;
                else
                    x = 2;

                auto& ch    = mod.children.alloc();
                auto  ch_id = mod.children.get_id(ch);
                mod.copy(grid->default_children[x][y], ch);
                grid->cache[pos(row, col, grid->row)] = ch_id;
            }
        }

        for (const auto& elem : grid->specific_children) {
            irt_assert(0 <= elem.row && elem.row < grid->row);
            irt_assert(0 <= elem.column && elem.column < grid->column);

            auto& ch    = mod.children.alloc();
            auto  ch_id = mod.children.get_id(ch);
            mod.copy(elem.ch, ch);
            grid->cache[pos(elem.row, elem.column, grid->row)] = ch_id;
        }

        if (grid->connection_type == grid_component::type::number) {
            for (int row = 0; row < grid->row; ++row) {
                for (int col = 0; col < grid->column; ++col) {
                    const auto src_id  = grid->cache[row * grid->column + col];
                    const auto row_min = std::clamp(row, 0, row - 1);
                    const auto row_max =
                      std::clamp(row, row + 1, grid->row - 1);
                    const auto col_min = std::clamp(col, 0, col - 1);
                    const auto col_max =
                      std::clamp(col, col + 1, grid->column - 1);

                    for (int i = row_min; i <= row_max; ++i) {
                        for (int j = col_min; j <= col_max; ++j) {
                            if (i != row && j != col) {
                                const auto dst_id =
                                  grid->cache[pos(i, j, grid->row)];

                                auto& c    = mod.connections.alloc();
                                auto  c_id = mod.connections.get_id(c);
                                c.type = connection::connection_type::internal;
                                c.internal.src       = src_id;
                                c.internal.index_src = 0;
                                c.internal.dst       = dst_id;
                                c.internal.index_dst = 0;
                                grid->cache_connections.emplace_back(c_id);
                            }
                        }
                    }
                }
            }
        }

        const auto use_row_cylinder =
          match(grid->opts,
                grid_component::options::row_cylinder,
                grid_component::options::torus);

        if (use_row_cylinder) {
            for (int row = 0; row < grid->row; ++row) {
                const auto src_id = grid->cache[pos(row, 0, grid->row)];
                const auto dst_id =
                  grid->cache[pos(row, grid->column - 1, grid->row)];

                auto& c1 = mod.connections.alloc();
                grid->cache_connections.emplace_back(
                  mod.connections.get_id(c1));
                c1.type               = connection::connection_type::internal;
                c1.internal.src       = src_id;
                c1.internal.index_src = 0;
                c1.internal.dst       = dst_id;
                c1.internal.index_dst = 0;

                auto& c2 = mod.connections.alloc();
                grid->cache_connections.emplace_back(
                  mod.connections.get_id(c2));
                c2.type               = connection::connection_type::internal;
                c2.internal.src       = src_id;
                c2.internal.index_src = 0;
                c2.internal.dst       = dst_id;
                c2.internal.index_dst = 0;
            }
        }

        const auto use_column_cylinder =
          match(grid->opts,
                grid_component::options::column_cylinder,
                grid_component::options::torus);

        if (use_column_cylinder) {
            for (int col = 0; col < grid->column; ++col) {
                const auto src_id = grid->cache[pos(0, col, grid->row)];
                const auto dst_id =
                  grid->cache[pos(grid->row - 1, col, grid->row)];

                auto& c1 = mod.connections.alloc();
                grid->cache_connections.emplace_back(
                  mod.connections.get_id(c1));
                c1.internal.src       = src_id;
                c1.internal.index_src = 0;
                c1.internal.dst       = dst_id;
                c1.internal.index_dst = 0;

                auto& c2 = mod.connections.alloc();
                grid->cache_connections.emplace_back(
                  mod.connections.get_id(c2));
                c2.internal.src       = src_id;
                c2.internal.index_src = 0;
                c2.internal.dst       = dst_id;
                c2.internal.index_dst = 0;
            }
        }
    }

    irt_return_if_bad(project_init(pj, mod, top));

    return status::success;
}

static status make_tree_from(project&      pj,
                             modeling&     mod,
                             component&    parent,
                             tree_node_id* out) noexcept
{
    irt_return_if_fail(pj.tree_nodes.can_alloc(),
                       status::data_array_not_enough_memory);

    auto& new_tree =
      pj.tree_nodes.alloc(mod.components.get_id(parent), undefined<child_id>());
    new_tree.tree.set_id(&new_tree);

    switch (parent.type) {
    case component_type::simple: {
        auto s_id = parent.id.simple_id;
        if (auto* s = mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(pj, mod, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = parent.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(pj, mod, new_tree, *g));
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    *out = pj.tree_nodes.get_id(new_tree);

    return status::success;
}

status project::init(int size) noexcept
{
    irt_return_if_bad(tree_nodes.init(size));

    return status::success;
}

void project_clear(project& pj) noexcept
{
    pj.tree_nodes.clear();

    pj.head    = undefined<component_id>();
    pj.tn_head = undefined<tree_node_id>();
}

status project_init(project& pj, modeling& mod, component& compo) noexcept
{
    project_clear(pj);

    irt_return_if_bad(make_tree_from(pj, mod, compo, &pj.tn_head));
    pj.head = mod.components.get_id(compo);

    return status::success;
}

tree_node_id build_tree(project& pj, modeling& mod, component& compo) noexcept
{
    tree_node_id id = undefined<tree_node_id>();

    if (is_success(make_tree_from(pj, mod, compo, &id)))
        return id;

    return undefined<tree_node_id>();
}

status simulation_init(project&                pj,
                       modeling&               mod,
                       simulation&             sim,
                       modeling_to_simulation& cache) noexcept
{
    cache.clear();
    sim.clear();

    if (auto* compo = mod.components.try_to_get(pj.head); compo) {
        simulation_clear_tree(pj, mod);
        irt_return_if_bad(simulation_initialize_tree(pj, mod, *compo));

        if (auto* head = pj.tree_nodes.try_to_get(pj.tn_head); head) {
            irt_return_if_bad(simulation_copy_sources(cache, mod, sim));
            irt_return_if_bad(
              simulation_copy_models(cache, pj, mod, sim, *head));
            irt_return_if_bad(
              simulation_copy_connections(cache, mod, sim, *head));
        } else {
            irt_bad_return(status::modeling_component_save_error);
        }
    } else {
        irt_bad_return(status::modeling_component_save_error);
    }

    return status::success;
}

}
// namespace irt
