// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static status make_tree_recursive(data_array<tree_node, tree_node_id>& data,
                                  modeling&                            mod,
                                  tree_node&                           parent,
                                  component&                           compo,
                                  child_id id_in_parent) noexcept;

static status make_tree_recursive(data_array<tree_node, tree_node_id>& data,
                                  modeling&                            mod,
                                  tree_node&                           new_tree,
                                  generic_component& src) noexcept
{
    for (auto child_id : src.children) {
        auto* child        = mod.children.try_to_get(child_id);
        auto  is_component = child && child->type == child_type::component;

        if (is_component) {
            auto compo_id = child->id.compo_id;
            if (auto* compo = mod.components.try_to_get(compo_id); compo)
                irt_return_if_bad(
                  make_tree_recursive(data, mod, new_tree, *compo, child_id));
        }
    }

    new_tree.child_to_node.sort();

    return status::success;
}

static status make_tree_recursive(data_array<tree_node, tree_node_id>& data,
                                  modeling&                            mod,
                                  tree_node&                           new_tree,
                                  grid_component& src) noexcept
{
    for (auto id : src.cache) {
        if (auto* c = mod.children.try_to_get(id); c)
            if (c->type == child_type::component) {
                if (auto* cp = mod.components.try_to_get(c->id.compo_id); cp)
                    irt_return_if_bad(
                      make_tree_recursive(data, mod, new_tree, *cp, id));
            }
    }

    new_tree.child_to_node.sort();

    return status::success;
}

static status make_tree_recursive(data_array<tree_node, tree_node_id>& data,
                                  modeling&                            mod,
                                  tree_node&                           parent,
                                  component&                           compo,
                                  child_id id_in_parent) noexcept
{
    irt_return_if_fail(data.can_alloc(), status::data_array_not_enough_memory);

    auto& new_tree = data.alloc(mod.components.get_id(compo), id_in_parent);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    parent.child_to_node.data.emplace_back(id_in_parent, &new_tree);

    switch (compo.type) {
    case component_type::simple: {
        auto s_id = compo.id.simple_id;
        if (auto* s = mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(data, mod, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(data, mod, new_tree, *g));
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
                                           generic_component&      src) noexcept
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

        cur->sim.sort();
    }

    return status::success;
}

static status get_input_models(vector<std::pair<model_id, i8>>& inputs,
                               modeling&                        mod,
                               tree_node&                       tree,
                               i8                               port_dst);

static status get_output_models(vector<std::pair<model_id, i8>>& outputs,
                                modeling&                        mod,
                                tree_node&                       tree,
                                i8                               port_dst);

static status get_input_models(vector<std::pair<model_id, i8>>& inputs,
                               modeling&                        mod,
                               tree_node&                       tree,
                               child_id                         ch,
                               i8                               port)
{
    status ret = status::success;

    if (auto* c = mod.children.try_to_get(ch); c) {
        if (c->type == child_type::model) {
            auto* sim_model = tree.sim.get(c->id.mdl_id);
            irt_assert(sim_model);
            inputs.emplace_back(*sim_model, port);
        } else {
            auto* tn = tree.child_to_node.get(ch);
            irt_assert(tn && *tn);

            ret = get_input_models(inputs, mod, **tn, port);
        }
    }

    return ret;
}

static status get_output_models(vector<std::pair<model_id, i8>>& outputs,
                                modeling&                        mod,
                                tree_node&                       tree,
                                child_id                         ch,
                                i8                               port)
{
    status ret = status::success;

    if (auto* c = mod.children.try_to_get(ch); c) {
        if (c->type == child_type::model) {
            auto* sim_model = tree.sim.get(c->id.mdl_id);
            irt_assert(sim_model);
            outputs.emplace_back(*sim_model, port);
        } else {
            auto* tn = tree.child_to_node.get(ch);
            irt_assert(tn && *tn);

            ret = get_output_models(outputs, mod, **tn, port);
        }
    }

    return ret;
}

static status get_input_models(vector<std::pair<model_id, i8>>& inputs,
                               modeling&                        mod,
                               tree_node&                       tree,
                               i8                               port_dst)
{
    if (auto* compo = mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g = mod.simple_components.try_to_get(compo->id.simple_id);
                g) {
                for (auto cnx_id : g->connections) {
                    if (auto* cnx = mod.connections.try_to_get(cnx_id); cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            if (auto ret =
                                  get_input_models(inputs,
                                                   mod,
                                                   tree,
                                                   cnx->output.src,
                                                   cnx->output.index_src);
                                is_bad(ret))
                                return ret;
                        }
                    }
                }
            }
            break;

        case component_type::grid:
            if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = mod.connections.try_to_get(cnx_id); cnx) {
                        if (cnx->type == connection::connection_type::input &&
                            cnx->input.index == port_dst) {
                            if (auto ret =
                                  get_input_models(inputs,
                                                   mod,
                                                   tree,
                                                   cnx->output.src,
                                                   cnx->output.index_src);
                                is_bad(ret))
                                return ret;
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

static status get_output_models(vector<std::pair<model_id, i8>>& outputs,
                                modeling&                        mod,
                                tree_node&                       tree,
                                i8                               port_dst)
{
    if (auto* compo = mod.components.try_to_get(tree.id); compo) {
        switch (compo->type) {
        case component_type::simple:
            if (auto* g = mod.simple_components.try_to_get(compo->id.simple_id);
                g) {
                for (auto cnx_id : g->connections) {
                    if (auto* cnx = mod.connections.try_to_get(cnx_id); cnx) {
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            if (auto ret =
                                  get_output_models(outputs,
                                                    mod,
                                                    tree,
                                                    cnx->input.dst,
                                                    cnx->input.index_dst);
                                is_bad(ret))
                                return ret;
                        }
                    }
                }
            }
            break;

        case component_type::grid:
            if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id);
                g) {
                for (auto cnx_id : g->cache_connections) {
                    if (auto* cnx = mod.connections.try_to_get(cnx_id); cnx) {
                        if (cnx->type == connection::connection_type::output &&
                            cnx->output.index == port_dst) {
                            if (auto ret =
                                  get_output_models(outputs,
                                                    mod,
                                                    tree,
                                                    cnx->input.dst,
                                                    cnx->input.index_dst);
                                is_bad(ret))
                                return ret;
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
  const vector<std::pair<model_id, i8>>& inputs,
  const vector<std::pair<model_id, i8>>& outputs,
  simulation&                            sim) noexcept
{
    for (auto src : outputs) {
        auto* src_model = sim.models.try_to_get(src.first);
        irt_assert(src_model);

        for (auto dst : inputs) {
            auto* dst_model = sim.models.try_to_get(dst.first);
            irt_assert(dst_model);

            auto ret =
              sim.connect(*src_model, src.second, *dst_model, dst.second);

            if (is_bad(ret))
                return ret;
        }
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              tree,
                                          vector<connection_id>&  connections)
{
    for (auto cnx_id : connections) {
        cache.inputs.clear();
        cache.outputs.clear();

        auto* cnx = mod.connections.try_to_get(cnx_id);

        const bool is_internal_connection =
          cnx && cnx->type == connection::connection_type::internal;

        if (is_internal_connection) {
            auto* src = mod.children.try_to_get(cnx->internal.src);
            auto* dst = mod.children.try_to_get(cnx->internal.dst);

            if (src->type == child_type::model) {
                auto* sim_src = tree.sim.get(src->id.mdl_id);
                irt_assert(sim_src);
                cache.outputs.emplace_back(*sim_src, cnx->internal.index_src);

                if (dst->type == child_type::model) {
                    auto* sim_dst = tree.sim.get(dst->id.mdl_id);
                    irt_assert(sim_dst);

                    cache.inputs.emplace_back(*sim_dst,
                                              cnx->internal.index_dst);
                } else {
                    auto* tn_dst = tree.child_to_node.get(cnx->internal.dst);
                    irt_assert(tn_dst && *tn_dst);

                    get_input_models(
                      cache.inputs, mod, **tn_dst, cnx->internal.index_dst);
                }
            } else {
                auto* tn_src = tree.child_to_node.get(cnx->internal.src);
                irt_assert(tn_src && *tn_src);

                get_output_models(
                  cache.outputs, mod, **tn_src, cnx->internal.index_src);

                if (dst->type == child_type::model) {
                    auto* sim_dst = tree.sim.get(dst->id.mdl_id);
                    irt_assert(sim_dst);

                    cache.inputs.emplace_back(*sim_dst,
                                              cnx->internal.index_dst);
                } else {
                    auto* tn_dst = tree.child_to_node.get(cnx->internal.dst);
                    irt_assert(tn_dst && *tn_dst);

                    get_input_models(
                      cache.inputs, mod, **tn_dst, cnx->internal.index_dst);
                }
            }

            if (auto ret =
                  simulation_copy_connections(cache.inputs, cache.outputs, sim);
                is_bad(ret))
                return ret;
        }
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              tree,
                                          component&              compo)
{
    switch (compo.type) {
    case component_type::simple: {
        if (auto* g = mod.simple_components.try_to_get(compo.id.simple_id); g)
            return simulation_copy_connections(
              cache, mod, sim, tree, g->connections);
    } break;

    case component_type::grid: {
        if (auto* g = mod.grid_components.try_to_get(compo.id.grid_id); g)
            return simulation_copy_connections(
              cache, mod, sim, tree, g->cache_connections);
    } break;

    case component_type::internal:
        break;

    case component_type::none:
        break;
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              head) noexcept
{
    cache.inputs.clear();
    cache.outputs.clear();

    cache.stack.clear();
    cache.stack.emplace_back(&head);

    while (!cache.stack.empty()) {
        auto cur = cache.stack.back();
        cache.stack.pop_back();

        if (auto* compo = mod.components.try_to_get(cur->id); compo)
            irt_return_if_bad(
              simulation_copy_connections(cache, mod, sim, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    return status::success;
}

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

static status make_component_cache(project& /*pj*/, modeling& mod) noexcept
{
    grid_component* grid = nullptr;
    while (mod.grid_components.next(grid))
        irt_return_if_bad(mod.build_grid_component_cache(*grid));

    return status::success;
}

static status make_tree_from(data_array<tree_node, tree_node_id>& data,
                             modeling&                            mod,
                             component&                           parent,
                             tree_node_id*                        out) noexcept
{
    irt_return_if_fail(data.can_alloc(), status::data_array_not_enough_memory);

    auto& new_tree =
      data.alloc(mod.components.get_id(parent), undefined<child_id>());
    auto new_tree_id = data.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);

    switch (parent.type) {
    case component_type::simple: {
        auto s_id = parent.id.simple_id;
        if (auto* s = mod.simple_components.try_to_get(s_id); s)
            irt_return_if_bad(make_tree_recursive(data, mod, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = parent.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(g_id); g)
            irt_return_if_bad(make_tree_recursive(data, mod, new_tree, *g));
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

status project::set(modeling& mod, component& compo) noexcept
{
    clear();

    irt_return_if_bad(make_component_cache(*this, mod));

    tree_node_id id  = undefined<tree_node_id>();
    auto         ret = make_tree_from(m_tree_nodes, mod, compo, &id);

    if (is_success(ret)) {
        m_head    = mod.components.get_id(compo);
        m_tn_head = id;
    }

    return ret;
}

status project::rebuild(modeling& mod) noexcept
{
    m_tree_nodes.clear();

    m_tn_head = undefined<tree_node_id>();

    irt_return_if_bad(make_component_cache(*this, mod));

    if (auto* compo = mod.components.try_to_get(m_head); compo) {
        tree_node_id id  = undefined<tree_node_id>();
        auto         ret = make_tree_from(m_tree_nodes, mod, *compo, &id);

        if (is_success(ret)) {
            m_tn_head = id;
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
    tree_node* tn = nullptr;
    while (m_tree_nodes.next(tn))
        tn->sim.data.clear();
}

status project::load(modeling&   mod,
                     io_cache&   cache,
                     const char* filename) noexcept
{
    return project_load(*this, mod, cache, filename);
}

status project::save(modeling&   mod,
                     io_cache&   cache,
                     const char* filename) noexcept
{
    return project_save(*this, mod, cache, filename);
}

status simulation_init(project&                pj,
                       modeling&               mod,
                       simulation&             sim,
                       modeling_to_simulation& cache) noexcept
{
    cache.clear();
    sim.clear();

    if (auto* compo = mod.components.try_to_get(pj.head()); compo) {
        mod.clean_simulation();
        pj.clean_simulation();

        if (auto* head = pj.tn_head(); head) {
            irt_return_if_bad(pj.rebuild(mod));
            irt_return_if_bad(simulation_copy_sources(cache, mod, sim));
            irt_return_if_bad(simulation_copy_models(cache, mod, sim, *head));
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
