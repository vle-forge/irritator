// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <optional>
#include <utility>

namespace irt {

template<typename D>
static bool data_array_reserve_add(D& d, std::integral auto size) noexcept
{
    if (not d.can_alloc(size) and not d.template grow<3, 2>())
        return false;

    return true;
}

template<typename V>
static bool vector_reserve_add(V& v, std::integral auto size) noexcept
{
    if (not v.can_alloc(size) and not v.template grow<2, 1>())
        return false;

    return true;
}

/* Make a link between component modeling external source and simulation
 * external source. */
class mod_to_sim_srcs
{
public:
    constexpr mod_to_sim_srcs(external_source_definition::id mid,
                              constant_source_id             sid) noexcept
      : mod_id{ mid }
      , sim_id{ sid }
      , type{ source_type::constant }
    {}

    constexpr mod_to_sim_srcs(external_source_definition::id mid,
                              binary_file_source_id          sid) noexcept
      : mod_id{ mid }
      , sim_id{ sid }
      , type{ source_type::binary_file }
    {}

    constexpr mod_to_sim_srcs(external_source_definition::id mid,
                              text_file_source_id            sid) noexcept
      : mod_id{ mid }
      , sim_id{ sid }
      , type{ source_type::text_file }
    {}

    constexpr mod_to_sim_srcs(external_source_definition::id mid,
                              random_source_id               sid) noexcept
      : mod_id{ mid }
      , sim_id{ sid }
      , type{ source_type::random }
    {}

    external_source_definition::id mod_id;
    source_any_id                  sim_id;
    source_type                    type;
};

constexpr auto convert_mod_to_sim_source_id(
  const std::span<const mod_to_sim_srcs> mapping,
  const external_source_definition::id   mod_id) noexcept
  -> std::optional<mod_to_sim_srcs>
{
    auto it = std::find_if(
      mapping.begin(), mapping.end(), [&](const auto& map) noexcept -> bool {
          return map.mod_id == mod_id;
      });

    return it == mapping.end() ? std::nullopt : std::optional(*it);
}

constexpr void convert_mod_to_sim_source(
  const std::span<const mod_to_sim_srcs> mapping,
  const dynamics_type                    type,
  parameter&                             p) noexcept
{
    switch (type) {
    case dynamics_type::dynamic_queue: {
        const auto mod_src = enum_cast<external_source_definition::id>(
          p.integers[dynamic_queue_tag::source_ta]);
        if (const auto sim_src = convert_mod_to_sim_source_id(mapping, mod_src);
            sim_src.has_value())
            p.set_dynamic_queue_ta(sim_src->type, sim_src->sim_id);
    } break;

    case dynamics_type::priority_queue: {
        const auto mod_src = enum_cast<external_source_definition::id>(
          p.integers[priority_queue_tag::source_ta]);
        if (const auto sim_src = convert_mod_to_sim_source_id(mapping, mod_src);
            sim_src.has_value())
            p.set_priority_queue_ta(sim_src->type, sim_src->sim_id);
    } break;

    case dynamics_type::generator: {
        const auto flags = bitflags<generator::option>(p.integers[0]);
        if (flags[generator::option::ta_use_source]) {
            const auto mod_src = enum_cast<external_source_definition::id>(
              p.integers[generator_tag::source_ta]);
            if (const auto sim_src =
                  convert_mod_to_sim_source_id(mapping, mod_src);
                sim_src.has_value())
                p.set_generator_ta(sim_src->type, sim_src->sim_id);
        }

        if (flags[generator::option::value_use_source]) {
            const auto mod_src = enum_cast<external_source_definition::id>(
              p.integers[generator_tag::source_value]);
            if (const auto sim_src =
                  convert_mod_to_sim_source_id(mapping, mod_src);
                sim_src.has_value())
                p.set_generator_value(sim_src->type, sim_src->sim_id);
        }
    } break;

    case dynamics_type::hsm_wrapper: {
        const auto mod_src = enum_cast<external_source_definition::id>(
          p.integers[hsm_wrapper_tag::source_value]);
        if (const auto sim_src = convert_mod_to_sim_source_id(mapping, mod_src);
            sim_src.has_value())
            p.set_hsm_wrapper_value(sim_src->type, sim_src->sim_id);
    } break;

    default:
        unreachable();
    }
}

struct model_port {
    model_id mdl  = undefined<model_id>();
    int      port = 0;

    model_port(const model_id mdl_, const int port_) noexcept
      : mdl{ mdl_ }
      , port{ port_ }
    {}
};

class sum_connection
{
private:
    tree_node* tn = nullptr;
    port_id    id = undefined<port_id>();

    model_id output_mdl = undefined<model_id>();
    model_id mdl        = undefined<model_id>();
    int      port       = 0;

    bool output_already_connected = false;

    status add_output_sum_model(simulation& sim) noexcept
    {
        if (not sim.models.can_alloc(1) and not sim.grow_models<2, 1>())
            return new_error(project_errc::component_cache_error);

        output_mdl = sim.models.get_id(sim.alloc(dynamics_type::qss3_sum_4));
        mdl        = output_mdl;
        port       = 0;

        tn->children.emplace_back().set(output_mdl);

        return success();
    }

    status add_input_sum_model(simulation& sim) noexcept
    {
        debug::ensure(is_defined(output_mdl));

        if (not sim.models.can_alloc(1) and not sim.grow_models<2, 1>())
            return new_error(project_errc::component_cache_error);

        auto& new_mdl = sim.alloc(dynamics_type::qss3_sum_4);
        if (not sim.can_connect(1) and not sim.grow_connections<2, 1>())
            return new_error(project_errc::component_cache_error);

        const auto& old_mdl = sim.models.get(mdl);
        mdl                 = sim.models.get_id(new_mdl);
        port                = 0;

        tn->children.emplace_back().set(mdl);

        return sim.connect(new_mdl, 0, old_mdl, 3);
    }

public:
    sum_connection(tree_node* tn_, port_id id_) noexcept
      : tn{ tn_ }
      , id{ id_ }
    {}

    constexpr bool is_equal(const tree_node* tn_,
                            const port_id    p_id_) const noexcept
    {
        return tn == tn_ and id == p_id_;
    }

    status add_output_connection(simulation&    sim,
                                 const model_id dst,
                                 int            port_dst) noexcept
    {
        if (is_undefined(output_mdl))
            if (auto ret = add_output_sum_model(sim); not ret)
                return ret.error();

        if (output_already_connected)
            return success();

        output_already_connected = true;

        return sim.connect(
          sim.models.get(output_mdl), 0, sim.models.get(dst), port_dst);
    }

    status add_source_connection(simulation& sim,
                                 model_id    src,
                                 int         port_src) noexcept
    {
        if (is_undefined(output_mdl))
            if (auto ret = add_output_sum_model(sim); not ret)
                return ret.error();

        if (port > 2) {
            if (auto ret = add_input_sum_model(sim); not ret)
                return ret.error();
        }

        return sim.connect(
          sim.models.get(src), port_src, sim.models.get(mdl), port++);
    }

    constexpr bool operator==(const sum_connection& other) const noexcept
    {
        return tn == other.tn and ordinal(id) == ordinal(other.id);
    }

    constexpr bool operator<(const sum_connection& other) const noexcept
    {
        return tn == other.tn ? ordinal(id) < ordinal(other.id) : tn < other.tn;
    }
};

struct grid_component_cache {
    data_array<grid_component::child, child_id> cache;
    data_array<connection, connection_id>       cache_connections;
    vector<name_str>                            cache_names;
};

struct graph_component_cache {
    data_array<graph_component::child, child_id> cache;
    data_array<connection, connection_id>        cache_connections;
    vector<name_str>                             cache_names;
};

// using grid_cache_type  = table<grid_component_id, grid_component_cache>;
// using graph_cache_type = table<graph_component_id, graph_component_cache>;

struct simulation_copy {
    simulation_copy(project&                             pj_,
                    modeling&                            mod_,
                    data_array<tree_node, tree_node_id>& tree_nodes_) noexcept
      : pj{ pj_ }
      , mod(mod_)
      , tree_nodes(tree_nodes_)
    {}

    /// For all hsm component, make a copy from modeling::hsm into
    /// simulation::hsm to ensure link between simulation and modeling.
    status make_hsm_mod_to_sim(const component_access& ids) noexcept
    {
        hsm_mod_to_sim.data.clear();

        for (const auto& modhsm : ids.hsm_components) {
            if (not pj.sim.hsms.can_alloc())
                return new_error(project_errc::memory_error);

            auto& sim_hsm     = pj.sim.hsms.alloc(modhsm.machine);
            sim_hsm.parent_id = ordinal(ids.hsm_components.get_id(modhsm));

            const auto hsm_id = ids.hsm_components.get_id(modhsm);
            const auto sim_id = pj.sim.hsms.get_id(sim_hsm);

            hsm_mod_to_sim.data.emplace_back(hsm_id, sim_id);
        }

        hsm_mod_to_sim.sort();

        return success();
    }

    /// For grid and graph components, build the names, children and connections
    /// cache.
    status make_component_cache(const component_access& ids) noexcept
    {
        grid_caches.data.clear();
        graph_caches.data.clear();

        for (const auto& grid : ids.grid_components) {
            const auto grid_id = ids.grid_components.get_id(grid);
            grid_caches.data.emplace_back(grid_id, grid_component_cache{});

            const auto ret =
              grid.build_cache(ids,
                               grid_caches.data.back().value.cache,
                               grid_caches.data.back().value.cache_connections,
                               grid_caches.data.back().value.cache_names);

            if (ret.has_error())
                return new_error(project_errc::component_cache_error);
        }

        for (const auto& graph : ids.graph_components) {
            const auto graph_id = ids.graph_components.get_id(graph);
            graph_caches.data.emplace_back(graph_id, graph_component_cache{});

            const auto ret = graph.build_cache(
              ids,
              graph_caches.data.back().value.cache,
              graph_caches.data.back().value.cache_connections,
              graph_caches.data.back().value.cache_names);

            if (ret.has_error())
                return new_error(project_errc::component_cache_error);
        }

        grid_caches.sort();
        graph_caches.sort();

        return success();
    }

    project&  pj;
    modeling& mod;

    vector<tree_node*> stack;
    vector<model_port> inputs;
    vector<model_port> outputs;

    vector<sum_connection> sum_input_connections;
    vector<sum_connection> sum_output_connections;

    table<u64, constant_source_id>    constants;
    table<u64, binary_file_source_id> binary_files;
    table<u64, text_file_source_id>   text_files;
    table<u64, random_source_id>      randoms;

    data_array<tree_node, tree_node_id>&         tree_nodes;
    table<hsm_component_id, hsm_id>              hsm_mod_to_sim;
    table<component_id, vector<mod_to_sim_srcs>> srcs_mod_to_sim;

    table<grid_component_id, grid_component_cache>   grid_caches;
    table<graph_component_id, graph_component_cache> graph_caches;
};

static auto make_tree_recursive(simulation_copy&        sc,
                                const component_access& ids,
                                tree_node&              parent,
                                const component_id      compo_id,
                                const std::string_view  uid) noexcept
  -> expected<tree_node_id>;

struct parent_t {
    tree_node& parent;
    component& compo;
};

static auto get_incoming_connection(const generic_component& gen,
                                    const port_id            id) noexcept -> int
{
    const auto nb_input = std::ranges::count_if(
      gen.input_connections,
      [&](const auto& input) noexcept -> bool { return input.x == id; });

    const auto nb_internal = std::ranges::count_if(
      gen.connections, [&](const auto& internal) noexcept -> bool {
          return internal.index_dst.compo == id;
      });

    return static_cast<int>(nb_input + nb_internal);
}

static auto get_incoming_connection(const component_access& ids,
                                    const tree_node&        tn,
                                    const port_id id) noexcept -> expected<int>
{
    if (not ids.exists(tn.id))
        return new_error(project_errc::import_error);

    const auto& compo = ids.components[tn.id];

    if (not compo.x.exists(id))
        return new_error(project_errc::import_error);

    if (compo.type == component_type::generic) {
        auto* gen = ids.generic_components.try_to_get(compo.id.generic_id);
        return gen ? get_incoming_connection(*gen, id) : 0;
    }

    return new_error(project_errc::import_error);
}

static auto get_incoming_connection(const modeling& /*mod*/,
                                    const component_access& ids,
                                    const tree_node&        tn) noexcept
  -> expected<int>
{
    if (not ids.exists(tn.id))
        return new_error(project_errc::import_error);

    const auto& compo = ids.components[tn.id];
    auto        nb    = 0;

    compo.x.for_each_id([&](auto id) noexcept {
        if (compo.type == component_type::generic) {
            if (auto* gen =
                  ids.generic_components.try_to_get(compo.id.generic_id))
                nb += get_incoming_connection(*gen, id);
        }
    });

    return nb;
}

static auto get_outcoming_connection(const generic_component& gen,
                                     const port_id id) noexcept -> int
{
    const auto nb_input = std::ranges::count_if(
      gen.output_connections,
      [&](const auto& input) noexcept -> bool { return input.y == id; });

    const auto nb_internal = std::ranges::count_if(
      gen.connections, [&](const auto& internal) noexcept -> bool {
          return internal.index_src.compo == id;
      });

    return static_cast<int>(nb_input + nb_internal);
}

static auto get_outcoming_connection(const modeling& /*mod*/,
                                     const component_access& ids,
                                     const tree_node&        tn,
                                     const port_id id) noexcept -> expected<int>
{
    if (not ids.exists(tn.id))
        return new_error(project_errc::import_error);

    const auto& compo = ids.components[tn.id];

    if (not compo.y.exists(id))
        return new_error(project_errc::import_error);

    if (compo.type == component_type::generic) {
        auto* gen = ids.generic_components.try_to_get(compo.id.generic_id);
        return gen ? get_outcoming_connection(*gen, id) : 0;
    }

    return new_error(project_errc::import_error);
}

static auto get_outcoming_connection(const modeling& /*mod*/,
                                     const component_access& ids,
                                     const tree_node&        tn) noexcept
  -> expected<int>
{
    if (not ids.exists(tn.id))
        return new_error(project_errc::import_error);

    const auto& compo = ids.components[tn.id];
    auto        nb    = 0;

    compo.y.for_each_id([&](auto id) noexcept {
        if (compo.type == component_type::generic) {
            auto* gen = ids.generic_components.try_to_get(compo.id.generic_id);
            if (gen)
                nb += get_outcoming_connection(*gen, id);
        }
    });

    return nb;
}

static auto make_tree_hsm_leaf(const simulation_copy&  sc,
                               const component_access& ids,
                               const parameter&        mod_parameter,
                               parameter&              sim_parameter,
                               hsm_wrapper&            dyn) noexcept -> status
{
    const auto id_param_0 = mod_parameter.integers[hsm_wrapper_tag::id];
    const auto compo_id   = enum_cast<component_id>(id_param_0);

    if (not ids.exists(compo_id))
        return new_error(project_errc::component_unknown);

    const auto& compo = ids.components[compo_id];
    if (compo.type != component_type::hsm)
        return new_error(project_errc::component_unknown);

    const auto hsm_id = compo.id.hsm_id;

    debug::ensure(ids.hsm_components.try_to_get(hsm_id));
    const auto* shsm    = sc.hsm_mod_to_sim.get(hsm_id);
    const auto  shsm_id = *shsm;

    debug::ensure(sc.pj.sim.hsms.try_to_get(shsm_id));
    const auto shsm_ord = ordinal(shsm_id);

    sim_parameter.integers[hsm_wrapper_tag::id] = shsm_ord;
    dyn.id                                      = shsm_id;

    return success();
}

static auto make_tree_constant_leaf(simulation_copy&        sc,
                                    const component_access& ids,
                                    tree_node&              parent,
                                    const parameter&        mod_parameter,
                                    parameter&              sim_parameter,
                                    constant& dyn) noexcept -> status
{
    const auto raw_type = mod_parameter.integers[constant_tag::i_type];
    debug::ensure(0 <= raw_type and raw_type < constant::init_type_count);

    const auto type_64 =
      0 <= raw_type and raw_type < constant::init_type_count ? raw_type : 0;

    const auto type = enum_cast<constant::init_type>(type_64);

    switch (type) {
    case constant::init_type::constant:
        break;

    case constant::init_type::incoming_component_all: {
        if (auto nb = get_incoming_connection(sc.mod, ids, parent);
            nb.has_value()) {
            sim_parameter.reals[constant_tag::value] = *nb;
            dyn.value                                = *nb;
        } else
            return nb.error();
    } break;

    case constant::init_type::outcoming_component_all: {
        if (auto nb = get_outcoming_connection(sc.mod, ids, parent);
            nb.has_value()) {
            sim_parameter.reals[constant_tag::value] = *nb;
            dyn.value                                = *nb;
        } else
            return nb.error();
    } break;

    case constant::init_type::incoming_component_n: {
        const auto port = mod_parameter.integers[constant_tag::i_port];
        const auto id   = enum_cast<port_id>(port);

        return sc.mod.ids.read([&](const auto& ids, auto) noexcept -> status {
            if (not ids.exists(parent.id))
                return new_error(project_errc::import_error);

            if (not ids.components[parent.id].x.exists(id))
                return new_error(project_errc::component_port_x_unknown);

            if (auto nb = get_incoming_connection(ids, parent, id);
                nb.has_value()) {
                sim_parameter.reals[constant_tag::value] = *nb;
                dyn.value                                = *nb;
                return success();
            } else
                return nb.error();
        });
    } break;

    case constant::init_type::outcoming_component_n: {
        const auto port = mod_parameter.integers[constant_tag::i_port];
        const auto id   = enum_cast<port_id>(port);

        return sc.mod.ids.read([&](const auto& ids, auto) noexcept -> status {
            if (not ids.exists(parent.id))
                return new_error(project_errc::import_error);

            if (not ids.components[parent.id].y.exists(id))
                return new_error(project_errc::component_port_y_unknown);

            if (auto nb = get_outcoming_connection(sc.mod, ids, parent, id);
                nb.has_value()) {
                sim_parameter.reals[constant_tag::value] = *nb;
                dyn.value                                = *nb;
                return success();
            } else
                return nb.error();
        });
    } break;
    }

    return success();
}

static auto make_tree_leaf(simulation_copy&                sc,
                           const component_access&         ids,
                           tree_node&                      parent,
                           const generic_component&        gen,
                           const std::string_view          uid,
                           dynamics_type                   mdl_type,
                           child_id                        ch_id,
                           const generic_component::child& ch) noexcept
  -> expected<model_id>
{
    if (not sc.pj.sim.models.can_alloc()) {
        const auto increase =
          sc.pj.sim.models.capacity() == 0 ? 1024 : sc.pj.sim.models.capacity();

        if (not data_array_reserve_add(sc.pj.sim.models, increase) or
            not vector_reserve_add(sc.pj.sim.parameters, increase))
            return new_error(project_errc::memory_error);
    }

    const auto ch_idx     = get_index(ch_id);
    auto&      new_mdl    = sc.pj.sim.models.alloc();
    auto       new_mdl_id = sc.pj.sim.models.get_id(new_mdl);

    new_mdl.type   = mdl_type;
    new_mdl.handle = invalid_heap_handle;

    irt_check(
      dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> status {
          std::construct_at(&dyn);

          if constexpr (has_input_port<Dynamics>)
              for (int i = 0, e = length(dyn.x); i != e; ++i)
                  dyn.x[i].reset();

          if constexpr (has_output_port<Dynamics>)
              for (int i = 0, e = length(dyn.y); i != e; ++i)
                  dyn.y[i] = undefined<output_port_id>();

          sc.pj.sim.parameters[new_mdl_id] = gen.children_parameters[ch_idx];

          if constexpr (std::is_same_v<Dynamics, generator> or
                        std::is_same_v<Dynamics, priority_queue> or
                        std::is_same_v<Dynamics, dynamic_queue> or
                        std::is_same_v<Dynamics, hsm_wrapper>) {
              if (auto* compo = sc.srcs_mod_to_sim.get(parent.id)) {
                  convert_mod_to_sim_source(
                    std::span(compo->data(), compo->size()),
                    new_mdl.type,
                    sc.pj.sim.parameters[new_mdl_id]);
              }
          }

          if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
              // For hsm-wrapper, we need to update the hsm_id in simulation
              // parameters which is different between modeling and
              // simulation.

              if (const auto ret =
                    make_tree_hsm_leaf(sc,
                                       ids,
                                       gen.children_parameters[ch_id],
                                       sc.pj.sim.parameters[new_mdl_id],
                                       dyn);
                  ret.has_error())
                  return ret.error();
          }

          if constexpr (std::is_same_v<Dynamics, constant>) {
              // For constant, we need to update the real simulation
              // parameter that are different between modeling and
              // simulation about input connection.

              if (const auto ret =
                    make_tree_constant_leaf(sc,
                                            ids,
                                            parent,
                                            gen.children_parameters[ch_id],
                                            sc.pj.sim.parameters[new_mdl_id],
                                            dyn);
                  ret.has_error())
                  return ret.error();
          }

          sc.pj.sim.parameters[new_mdl_id].copy_to(new_mdl);

          return success();
      }));

    const auto is_public = (ch.flags[child_flags::configurable] or
                            ch.flags[child_flags::observable]);

    // @TODO Add a test function in bitflags to allow:
    // if (ch.flags.test(child_flags::configurable,
    // child_flags::configurable);

    if (is_public) {
        debug::ensure(not uid.empty());
        parent.unique_id_to_model_id.data.emplace_back(name_str(uid),
                                                       new_mdl_id);
        parent.model_id_to_unique_id.data.emplace_back(new_mdl_id,
                                                       name_str(uid));
        if (not sc.pj.parameters.can_alloc(1) and
            not sc.pj.parameters.grow<2, 1>())
            return new_error(project_errc::memory_error);

        if (not parent.parameters_ids.data.can_alloc(1) and
            not parent.parameters_ids.data.grow<2, 1>())
            return new_error(project_errc::memory_error);

        const auto id = sc.pj.parameters.alloc_id();

        sc.pj.parameters.get<name_str>(id) = name_str(uid);
        sc.pj.parameters.get<tree_node_id>(id) =
          sc.pj.tree_nodes.get_id(parent);
        sc.pj.parameters.get<model_id>(id) = new_mdl_id;

        sc.pj.parameters.get<parameter>(id).copy_from(new_mdl);
        parent.parameters_ids.data.emplace_back(name_str(uid), id);
    }

    return new_mdl_id;
}

static status make_tree_recursive(simulation_copy&        sc,
                                  const component_access& ids,
                                  tree_node&              new_tree,
                                  generic_component&      src) noexcept
{
    new_tree.children.resize(src.children.max_used());

    for (auto& child : src.children) {
        const auto child_id  = src.children.get_id(child);
        const auto child_idx = get_index(child_id);

        if (child.type == child_type::component) {
            const auto compo_id = child.id.compo_id;

            if (ids.exists(compo_id)) {
                auto tn_id =
                  make_tree_recursive(sc,
                                      ids,
                                      new_tree,
                                      compo_id,
                                      src.children_names[child_idx].sv());

                if (not tn_id.has_value())
                    return tn_id.error();

                new_tree.children[child_id].set(
                  sc.tree_nodes.try_to_get(*tn_id));
            }
        } else {
            const auto mdl_type = child.id.mdl_type;
            auto       mdl_id   = make_tree_leaf(sc,
                                         ids,
                                         new_tree,
                                         src,
                                         src.children_names[child_idx].sv(),
                                         mdl_type,
                                         child_id,
                                         child);

            if (not mdl_id)
                return mdl_id.error();

            new_tree.children[child_id].set(*mdl_id);
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.model_id_to_unique_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static status make_tree_recursive(simulation_copy&        sc,
                                  const component_access& ids,
                                  tree_node&              new_tree,
                                  grid_component&         src) noexcept
{
    const auto  src_id    = ids.grid_components.get_id(src);
    const auto* src_cache = sc.grid_caches.get(src_id);
    fatal::ensure(src_cache);

    new_tree.children.resize(src_cache->cache.max_used());

    for (auto& child : src_cache->cache) {
        const auto child_id = src_cache->cache.get_id(child);
        const auto compo_id = child.compo_id;

        if (ids.exists(compo_id)) {
            const auto tn_id =
              make_tree_recursive(sc,
                                  ids,
                                  new_tree,
                                  compo_id,
                                  src_cache->cache_names[child_id].sv());

            if (not tn_id)
                return tn_id.error();

            new_tree.children[child_id].set(sc.tree_nodes.try_to_get(*tn_id));
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.model_id_to_unique_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static status make_tree_recursive(simulation_copy&        sc,
                                  const component_access& ids,
                                  tree_node&              new_tree,
                                  graph_component&        src) noexcept
{
    const auto  src_id    = ids.graph_components.get_id(src);
    const auto* src_cache = sc.graph_caches.get(src_id);
    fatal::ensure(src_cache);

    new_tree.children.resize(src_cache->cache.max_used());

    for (auto& child : src_cache->cache) {
        const auto child_id = src_cache->cache.get_id(child);
        const auto compo_id = child.compo_id;

        if (ids.exists(compo_id)) {
            auto tn_id =
              make_tree_recursive(sc,
                                  ids,
                                  new_tree,
                                  compo_id,
                                  src_cache->cache_names[child_id].sv());

            if (not tn_id.has_value())
                return tn_id.error();

            new_tree.children[child_id].set(sc.tree_nodes.try_to_get(*tn_id));
        }
    }

    new_tree.unique_id_to_model_id.sort();
    new_tree.model_id_to_unique_id.sort();
    new_tree.unique_id_to_tree_node_id.sort();

    return success();
}

static bool external_sources_reserve_add(const external_source_definition& src,
                                         external_source& dst) noexcept
{
    std::array<unsigned, 4> more_reserve{};
    const auto& srcs = src.data.get<external_source_definition::source_element>();

    for (const auto id : src.data)
        ++more_reserve[ordinal(srcs[id].type)];

    return data_array_reserve_add(dst.constant_sources, more_reserve[0]) and
           data_array_reserve_add(dst.binary_file_sources, more_reserve[1]) and
           data_array_reserve_add(dst.text_file_sources, more_reserve[2]) and
           data_array_reserve_add(dst.random_sources, more_reserve[3]);
}

static status external_source_copy(const modeling&                   mod,
                                   vector<mod_to_sim_srcs>&          v,
                                   const external_source_definition& src,
                                   external_source& dst) noexcept
{
    if (not external_sources_reserve_add(src, dst) or
        not vector_reserve_add(v, src.data.size()))
        return new_error(external_source_errc::memory_error);

    const auto& src_elems =
      src.data.get<external_source_definition::source_element>();
    const auto& src_names = src.data.get<name_str>();

    for (const auto id : src.data) {
        switch (src_elems[id].type) {
        case source_type::constant: {
            auto& n_src = src_elems[id].cst;
            auto& n_res = dst.constant_sources.alloc(n_src.data);
            n_res.name    = src_names[id];
            auto n_res_id = dst.constant_sources.get_id(n_res);

            for (int i = 0; i < n_src.data.ssize(); ++i)
                n_res.buffer[i] = n_src.data[i];
            n_res.length = n_src.data.ssize();

            v.emplace_back(id, n_res_id);
        } break;

        case source_type::binary_file: {
            auto& n_src = src_elems[id].bin;

            const auto p =
              make_file(mod, n_src.file).value_or(std::filesystem::path{});

            auto& n_res   = dst.binary_file_sources.alloc(p);
            n_res.name    = src_names[id];
            auto n_res_id = dst.binary_file_sources.get_id(n_res);

            v.emplace_back(id, n_res_id);

        } break;

        case source_type::text_file: {
            auto& n_src = src_elems[id].txt;

            const auto p =
              make_file(mod, n_src.file).value_or(std::filesystem::path{});

            auto& n_res   = dst.text_file_sources.alloc(p);
            n_res.name    = src_names[id];
            auto n_res_id = dst.text_file_sources.get_id(n_res);

            v.emplace_back(id, n_res_id);
        } break;

        case source_type::random: {
            auto& n_src = src_elems[id].rnd;
            auto& n_res =
              dst.random_sources.alloc(n_src.type, n_src.reals, n_src.ints);
            n_res.name    = src_names[id];
            auto n_res_id = dst.random_sources.get_id(n_res);

            v.emplace_back(id, n_res_id);
        } break;
        }
    }

    return success();
}

static status make_tree_recursive([[maybe_unused]] simulation_copy& sc,
                                  [[maybe_unused]] const component_access&,
                                  [[maybe_unused]] tree_node&     new_tree,
                                  [[maybe_unused]] hsm_component& src) noexcept
{
    debug::ensure(sc.pj.sim.hsms.can_alloc());

    return success();
}

static status update_external_source(simulation_copy&        sc,
                                     const component_access& ids,
                                     const component_id      compo_id) noexcept
{
    if (const auto* exist = sc.srcs_mod_to_sim.get(compo_id); not exist) {
        debug::ensure(ids.exists(compo_id));
        auto& compo = ids.components[compo_id];

        if (not compo.srcs.data.empty()) {
            external_sources_reserve_add(compo.srcs, sc.pj.sim.srcs);
            sc.srcs_mod_to_sim.data.emplace_back(compo_id,
                                                 vector<mod_to_sim_srcs>());
            vector_reserve_add(sc.srcs_mod_to_sim.data.back().value,
                               compo.srcs.data.size());

            irt_check(external_source_copy(sc.mod,
                                           sc.srcs_mod_to_sim.data.back().value,
                                           compo.srcs,
                                           sc.pj.sim.srcs));

            sc.srcs_mod_to_sim.sort();
        }
    }

    return success();
}

static auto make_tree_recursive(simulation_copy&        sc,
                                const component_access& ids,
                                tree_node&              parent,
                                const component_id      compo_id,
                                const std::string_view  unique_id) noexcept
  -> expected<tree_node_id>
{
    if (not sc.tree_nodes.can_alloc())
        return new_error(project_errc::memory_error);

    debug::ensure(ids.exists(compo_id));

    auto&      new_tree = sc.tree_nodes.alloc(compo_id, unique_id);
    const auto tn_id    = sc.tree_nodes.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    const auto& compo = ids.components[compo_id];

    irt_check(update_external_source(sc, ids, compo_id));

    switch (compo.type) {
    case component_type::generic: {
        auto s_id = compo.id.generic_id;
        if (auto* s = ids.generic_components.try_to_get(s_id); s)
            irt_check(make_tree_recursive(sc, ids, new_tree, *s));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = ids.grid_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, ids, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::graph: {
        auto g_id = compo.id.graph_id;
        if (auto* g = ids.graph_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, ids, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::none:
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
        break;
    }

    return sc.tree_nodes.get_id(new_tree);
}

static status simulation_copy_connections(const vector<model_port>& inputs,
                                          const vector<model_port>& outputs,
                                          simulation& sim) noexcept
{
    for (auto src : outputs) {
        for (auto dst : inputs) {
            if (auto ret = sim.connect(sim.models.get(src.mdl),
                                       src.port,
                                       sim.models.get(dst.mdl),
                                       dst.port);
                !ret)
                return new_error(project_errc::import_error);
        }
    }

    return success();
}

static void get_input_models(vector<model_port>&     inputs,
                             const simulation_copy&  sc,
                             const component_access& ids,
                             const tree_node&        tn,
                             const port_id           p) noexcept;

static void get_input_models(vector<model_port>&      inputs,
                             const simulation_copy&   sc,
                             const component_access&  ids,
                             const tree_node&         tn,
                             const generic_component& gen,
                             const port_id            p) noexcept
{
    for (const auto& con : gen.input_connections) {
        if (con.x != p)
            continue;

        auto* child = gen.children.try_to_get(con.dst);
        if (not child)
            continue;

        if (child->type == child_type::model) {
            debug::ensure(tn.children[con.dst].mdl);

            inputs.emplace_back(tn.children[con.dst].mdl, con.port.model);
        } else {
            debug::ensure(tn.children[con.dst].tn);
            get_input_models(
              inputs, sc, ids, *tn.children[con.dst].tn, con.port.compo);
        }
    }
}

static void get_input_models(vector<model_port>&     inputs,
                             const simulation_copy&  sc,
                             const component_access& ids,
                             const tree_node&        tn,
                             const graph_component&  graph,
                             const port_id           p) noexcept
{
    for (const auto& con : graph.input_connections) {
        if (con.x != p)
            continue;

        if (not graph.g.nodes.exists(con.v))
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, sc, ids, *tn.children[idx].tn, con.id);
    }
}

static void get_input_models(vector<model_port>&     inputs,
                             const simulation_copy&  sc,
                             const component_access& ids,
                             const tree_node&        tn,
                             const grid_component&   grid,
                             const port_id           p) noexcept
{
    for (const auto& con : grid.input_connections) {
        if (con.x != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children()[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, sc, ids, *tn.children[idx].tn, con.id);
    }
}

static void get_input_pack_models(
  vector<model_port>&                                   inputs,
  const simulation_copy&                                sc,
  const component_access&                               ids,
  const tree_node&                                      tn,
  const component&                                      compo,
  const port_id                                         p,
  const data_array<generic_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.input_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.type == child_type::component and
                c.id.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_input_models(
                  inputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_pack_models(
  vector<model_port>&                                 inputs,
  const simulation_copy&                              sc,
  const component_access&                             ids,
  const tree_node&                                    tn,
  const component&                                    compo,
  const port_id                                       p,
  const data_array<graph_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.input_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_input_models(
                  inputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_pack_models(
  vector<model_port>&                                inputs,
  const simulation_copy&                             sc,
  const component_access&                            ids,
  const tree_node&                                   tn,
  const component&                                   compo,
  const port_id                                      p,
  const data_array<grid_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.input_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_input_models(
                  inputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_models(vector<model_port>&     inputs,
                             const simulation_copy&  sc,
                             const component_access& ids,
                             const tree_node&        tn,
                             const port_id           p) noexcept
{
    if (not ids.exists(tn.id))
        return;

    const auto& compo = ids.components[tn.id];

    switch (compo.type) {
    case component_type::generic: {
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id)) {
            get_input_models(inputs, sc, ids, tn, *g, p);
            get_input_pack_models(inputs, sc, ids, tn, compo, p, g->children);
        }
    } break;

    case component_type::graph: {
        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id)) {
            const auto* gcache = sc.graph_caches.get(compo.id.graph_id);
            fatal::ensure(gcache);
            get_input_models(inputs, sc, ids, tn, *g, p);
            get_input_pack_models(inputs, sc, ids, tn, compo, p, gcache->cache);
        }

    } break;

    case component_type::grid: {
        if (auto* g = ids.grid_components.try_to_get(compo.id.grid_id)) {
            const auto* gcache = sc.grid_caches.get(compo.id.grid_id);
            fatal::ensure(gcache);
            get_input_models(inputs, sc, ids, tn, *g, p);
            get_input_pack_models(inputs, sc, ids, tn, compo, p, gcache->cache);
        }
    } break;

    case component_type::hsm:
    case component_type::none:
    case component_type::simulation:
        break;
    }
}

static void get_output_models(vector<model_port>&     outputs,
                              const simulation_copy&  sc,
                              const component_access& ids,
                              const tree_node&        tn,
                              const port_id           p) noexcept;

static void get_output_models(vector<model_port>&      outputs,
                              const simulation_copy&   sc,
                              const component_access&  ids,
                              const tree_node&         tn,
                              const generic_component& gen,
                              const port_id            p) noexcept
{
    for (const auto& con : gen.output_connections) {
        if (con.y != p)
            continue;

        auto* child = gen.children.try_to_get(con.src);
        if (not child)
            continue;

        if (child->type == child_type::model) {
            debug::ensure(tn.children[con.src].mdl);

            outputs.emplace_back(tn.children[con.src].mdl, con.port.model);
        } else {
            debug::ensure(tn.children[con.src].tn);
            get_output_models(
              outputs, sc, ids, *tn.children[con.src].tn, con.port.compo);
        }
    }
}

static void get_output_models(vector<model_port>&     outputs,
                              const simulation_copy&  sc,
                              const component_access& ids,
                              const tree_node&        tn,
                              const graph_component&  graph,
                              const port_id           p) noexcept
{
    for (const auto& con : graph.output_connections) {
        if (con.y != p)
            continue;

        if (not graph.g.nodes.exists(con.v))
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, sc, ids, *tn.children[idx].tn, con.id);
    }
}

static void get_output_models(vector<model_port>&     outputs,
                              const simulation_copy&  sc,
                              const component_access& ids,
                              const tree_node&        tn,
                              const grid_component&   grid,
                              const port_id           p) noexcept
{
    for (const auto& con : grid.output_connections) {
        if (con.y != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children()[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, sc, ids, *tn.children[idx].tn, con.id);
    }
}

static void get_output_pack_models(
  vector<model_port>&                                   outputs,
  const simulation_copy&                                sc,
  const component_access&                               ids,
  const tree_node&                                      tn,
  const component&                                      compo,
  const port_id                                         p,
  const data_array<generic_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.output_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.type == child_type::component and
                c.id.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_output_models(
                  outputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_pack_models(
  vector<model_port>&                                 outputs,
  const simulation_copy&                              sc,
  const component_access&                             ids,
  const tree_node&                                    tn,
  const component&                                    compo,
  const port_id                                       p,
  const data_array<graph_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.output_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_output_models(
                  outputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_pack_models(
  vector<model_port>&                                outputs,
  const simulation_copy&                             sc,
  const component_access&                            ids,
  const tree_node&                                   tn,
  const component&                                   compo,
  const port_id                                      p,
  const data_array<grid_component::child, child_id>& children) noexcept
{
    for (const auto& con : compo.output_connection_pack) {
        if (con.parent_port != p)
            continue;

        for (const auto& c : children) {
            if (c.compo_id == con.child_component) {
                const auto idx = get_index(children.get_id(c));
                debug::ensure(tn.children[idx].tn);
                get_output_models(
                  outputs, sc, ids, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_models(vector<model_port>&     outputs,
                              const simulation_copy&  sc,
                              const component_access& ids,
                              const tree_node&        tn,
                              const port_id           p) noexcept
{
    if (not ids.exists(tn.id))
        return;

    const auto& compo = ids.components[tn.id];

    switch (compo.type) {
    case component_type::generic: {
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id)) {
            get_output_models(outputs, sc, ids, tn, *g, p);
            get_output_pack_models(outputs, sc, ids, tn, compo, p, g->children);
        }
    } break;

    case component_type::graph: {
        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id)) {
            const auto* gcache = sc.graph_caches.get(compo.id.graph_id);
            fatal::ensure(gcache);
            get_output_models(outputs, sc, ids, tn, *g, p);
            get_output_pack_models(
              outputs, sc, ids, tn, compo, p, gcache->cache);
        }
    } break;

    case component_type::grid: {
        if (auto* g = ids.grid_components.try_to_get(compo.id.grid_id)) {
            const auto* gcache = sc.grid_caches.get(compo.id.grid_id);
            fatal::ensure(gcache);
            get_output_models(outputs, sc, ids, tn, *g, p);
            get_output_pack_models(
              outputs, sc, ids, tn, compo, p, gcache->cache);
        }
    } break;

    case component_type::hsm:
    case component_type::none:
    case component_type::simulation:
        break;
    }
}

/** Build the cache table @a sum_connections and for both input and output.
 * For each input port of type @a sum add a @a
 * sum_connection struct. These structs will be fill during the
 * component connections construct. */
static auto prepare_sum_connections(
  tree_node&                                   tree,
  const data_array<connection, connection_id>& connections,
  simulation_copy&                             sc,
  const component_access&                      ids) -> status
{
    sc.sum_input_connections.clear();
    sc.sum_output_connections.clear();

    auto contains =
      [](const auto& vec, const auto* tn, const auto p_id) noexcept -> bool {
        return std::ranges::any_of(vec,
                                   [tn, p_id](const auto& e) noexcept -> bool {
                                       return e.is_equal(tn, p_id);
                                   });
    };

    for (const auto& cnx : connections) {
        if (tree.is_tree_node(cnx.dst)) {
            const auto  dst_idx  = get_index(cnx.dst);
            auto*       tn       = tree.children[dst_idx].tn;
            const auto  compo_id = tn->id;
            const auto  port_id  = cnx.index_dst.compo;
            const auto& c        = ids.components[compo_id];

            if (c.x.exists(port_id)) {
                if (c.x.get<port_option>(port_id) == port_option::sum) {
                    if (not sc.sum_input_connections.can_alloc(1) and
                        not sc.sum_input_connections.grow<2, 1>())
                        return new_error(project_errc::component_cache_error);

                    if (not contains(sc.sum_input_connections, tn, port_id))
                        sc.sum_input_connections.emplace_back(tn, port_id);
                }
            }
        }

        if (tree.is_tree_node(cnx.src)) {
            const auto  src_idx  = get_index(cnx.src);
            auto*       tn       = tree.children[src_idx].tn;
            const auto  compo_id = tn->id;
            const auto  port_id  = cnx.index_src.compo;
            const auto& c        = ids.components[compo_id];

            if (c.y.exists(port_id)) {
                if (c.y.get<port_option>(port_id) == port_option::sum) {
                    if (not sc.sum_output_connections.can_alloc(1) and
                        not sc.sum_output_connections.grow<2, 1>())
                        return new_error(project_errc::component_cache_error);

                    if (not contains(sc.sum_output_connections, tn, port_id))
                        sc.sum_output_connections.emplace_back(tn, port_id);
                }
            }
        }
    }

    return success();
};

/** Get the @a port_option of the @a p_id port of the @a compo_id
 * component. */
static auto get_input_connection_type(const component_access& ids,
                                      const component_id      compo_id,
                                      const port_id&          p_id) noexcept
  -> port_option
{
    debug::ensure(ids.exists(compo_id));

    return ids.components[compo_id].x.get<port_option>(p_id);
}

/** Get the @a port_option of the @a p_id port of the @a compo_id
 * component. */
static auto get_output_connection_type(const component_access& ids,
                                       const component_id      compo_id,
                                       const port_id&          p_id) noexcept
  -> port_option
{
    debug::ensure(ids.exists(compo_id));

    return ids.components[compo_id].y.get<port_option>(p_id);
}

/** Adds @a qss3_sum_4 connections in place of the @a port to sum all the
 * input connection of the component @a compo.
 *
 * From a component graph:
 * @code
 * ┌───┐
 * │a  ┼───┐     ┌───────────────┐
 * └───┘   │     │               │
 * ┌───┐   │     │               │
 * │b  ┼───┤     │    ┌─────┐    │
 * └───┘   │     │    │ X   │    │
 * ┌───┐   │     │    │     │    │
 * │c  ┼───┼────►│    └─────┘    │
 * └───┘   │     │sum            │
 * ┌───┐   │     │               │
 * │d  ┼───┤     │               │
 * └───┘   │     │     component │
 * ┌───┐   │     └───────────────┘
 * │e  ┼───┘
 * └───┘
 * @endcode
 *
 * To the simulation graph:
 * @code
 * ┌───┐
 * │a  ┼────┐
 * └───┘    │  ┌───┐
 * ┌───┐    └──┼s  │
 * │b  ┼───────┼u  ┼┐          ┌──────┐
 * └───┘    ┌──┼m  ││          │ X    │
 * ┌───┐    │┌─┼1  ││        ┌─┼      │
 * │c  ┼────┘│ └───┘│ ┌───┐  │ └──────┘
 * └───┘     │      └─┤s  │  │
 * ┌───┐     │        │u  ┼──┘
 * │d  ┼─────┘    ┌───┼m  │
 * └───┘          │   │2  │
 * ┌───┐          │   └───┘
 * │e  ┼──────────┘
 * └───┘
 * @endcode
 */
static status simulation_copy_sum_connections(
  const vector<model_port>& inputs,
  const vector<model_port>& outputs,
  const tree_node&          tn,
  const port_id             p_id,
  vector<sum_connection>&   connections,
  simulation&               sim) noexcept
{
    if (auto it = std::ranges::find_if(
          connections,
          [&](auto& elem) noexcept { return elem.is_equal(&tn, p_id); });
        it != connections.end()) {

        for (const auto& dst : inputs) {
            if (auto ret = it->add_output_connection(sim, dst.mdl, dst.port);
                not ret)
                return ret.error();
        }

        for (const auto& src : outputs)
            if (auto ret = it->add_source_connection(sim, src.mdl, src.port);
                not ret)
                return ret.error();
    }

    return success();
}

template<typename Child>
static status simulation_copy_connections(
  simulation_copy&                             sc,
  const component_access&                      ids,
  tree_node&                                   tree,
  const data_array<Child, child_id>&           children,
  const data_array<connection, connection_id>& connections) noexcept
{
    if (auto ret = prepare_sum_connections(tree, connections, sc, ids); not ret)
        return ret.error();

    for (const auto& cnx : connections) {
        sc.inputs.clear();
        sc.outputs.clear();

        auto* src = children.try_to_get(cnx.src);
        auto* dst = children.try_to_get(cnx.dst);

        debug::ensure(src and dst);

        if (not src or not dst)
            continue;

        const auto src_idx = get_index(cnx.src);
        const auto dst_idx = get_index(cnx.dst);

        auto       port        = undefined<port_id>();
        auto       input_type  = port_option::classic;
        auto       output_type = port_option::classic;
        tree_node* tn          = nullptr;

        if (tree.is_model(cnx.src)) {
            sc.outputs.emplace_back(tree.children[src_idx].mdl,
                                    cnx.index_src.model);

            if (tree.is_model(cnx.dst)) {
                sc.inputs.emplace_back(tree.children[dst_idx].mdl,
                                       cnx.index_dst.model);
            } else {
                port = cnx.index_dst.compo;
                tn   = tree.children[dst_idx].tn;
                get_input_models(sc.inputs, sc, ids, *tn, port);

                input_type = get_input_connection_type(ids, tn->id, port);
            }
        } else {
            port = cnx.index_src.compo;
            tn   = tree.children[src_idx].tn;
            get_output_models(sc.outputs, sc, ids, *tn, port);

            output_type = get_output_connection_type(
              ids, tree.children[src_idx].tn->id, cnx.index_src.compo);

            if (tree.is_model(cnx.dst)) {
                sc.inputs.emplace_back(tree.children[dst_idx].mdl,
                                       cnx.index_dst.model);
            } else {
                port = cnx.index_dst.compo;
                tn   = tree.children[dst_idx].tn;
                get_input_models(sc.inputs, sc, ids, *tn, port);

                input_type = get_input_connection_type(ids, tn->id, port);
            }
        }

        if (input_type == port_option::sum and output_type == port_option::sum)
            return new_error(project_errc::component_cache_error);

        if (input_type == port_option::classic) {
            if (output_type == port_option::classic) {
                irt_check(simulation_copy_connections(
                  sc.inputs, sc.outputs, sc.pj.sim));
            } else {
                irt_check(
                  simulation_copy_sum_connections(sc.inputs,
                                                  sc.outputs,
                                                  *tn,
                                                  port,
                                                  sc.sum_output_connections,
                                                  sc.pj.sim));
            }
        } else {
            if (output_type == port_option::classic) {
                irt_check(
                  simulation_copy_sum_connections(sc.inputs,
                                                  sc.outputs,
                                                  *tn,
                                                  port,
                                                  sc.sum_input_connections,
                                                  sc.pj.sim));
            } else {
                return new_error(project_errc::component_cache_error);
            }
        }
    }

    return success();
}

static status simulation_copy_connections(simulation_copy&        sc,
                                          const component_access& ids,
                                          tree_node&              tree,
                                          const component_id      compo_id)
{
    debug::ensure(ids.exists(compo_id));

    const auto& compo = ids.components[compo_id];

    switch (compo.type) {
    case component_type::generic:
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id)) {
            return simulation_copy_connections(
              sc, ids, tree, g->children, g->connections);
        }
        break;

    case component_type::grid:
        if (ids.grid_components.try_to_get(compo.id.grid_id)) {
            const auto* gcache = sc.grid_caches.get(compo.id.grid_id);
            fatal::ensure(gcache);

            return simulation_copy_connections(
              sc, ids, tree, gcache->cache, gcache->cache_connections);
        }
        break;

    case component_type::graph:
        if (ids.graph_components.try_to_get(compo.id.graph_id)) {
            const auto* gcache = sc.graph_caches.get(compo.id.graph_id);
            fatal::ensure(gcache);

            return simulation_copy_connections(
              sc, ids, tree, gcache->cache, gcache->cache_connections);
        }
        break;

    case component_type::none:
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
        break;
    }

    return success();
}

static status simulation_copy_connections(simulation_copy&        sc,
                                          const component_access& ids,
                                          tree_node&              head) noexcept
{
    sc.stack.clear();
    sc.stack.emplace_back(&head);

    while (not sc.stack.empty()) {
        auto cur = sc.stack.back();
        sc.stack.pop_back();

        if (ids.exists(cur->id))
            irt_check(simulation_copy_connections(sc, ids, *cur, cur->id));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            sc.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            sc.stack.emplace_back(child);
    }

    return success();
}

static auto make_tree_from(simulation_copy&                     sc,
                           const component_access&              ids,
                           data_array<tree_node, tree_node_id>& data,
                           const component_id                   parent) noexcept
  -> expected<tree_node_id>
{
    if (not data.can_alloc())
        return new_error(project_errc::memory_error);

    const auto& compo    = ids.components[parent];
    auto&       new_tree = data.alloc(parent, std::string_view{});
    new_tree.tree.set_id(&new_tree);
    new_tree.unique_id = "root";

    if (const auto nb = compo.srcs.data.size(); nb > 0) {
        external_sources_reserve_add(compo.srcs, sc.pj.sim.srcs);

        sc.srcs_mod_to_sim.data.emplace_back(parent, vector<mod_to_sim_srcs>());

        vector_reserve_add(sc.srcs_mod_to_sim.data.back().value, nb);

        irt_check(external_source_copy(sc.mod,
                                       sc.srcs_mod_to_sim.data.back().value,
                                       compo.srcs,
                                       sc.pj.sim.srcs));
    }

    switch (compo.type) {
    case component_type::generic: {
        auto s_id = compo.id.generic_id;
        if (auto* s = ids.generic_components.try_to_get(s_id))
            irt_check(make_tree_recursive(sc, ids, new_tree, *s));
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = ids.grid_components.try_to_get(g_id))
            irt_check(make_tree_recursive(sc, ids, new_tree, *g));
    } break;

    case component_type::graph: {
        auto g_id = compo.id.graph_id;
        if (auto* g = ids.graph_components.try_to_get(g_id))
            irt_check(make_tree_recursive(sc, ids, new_tree, *g));
    } break;

    case component_type::hsm: {
        auto h_id = compo.id.hsm_id;
        if (auto* h = ids.hsm_components.try_to_get(h_id))
            irt_check(make_tree_recursive(sc, ids, new_tree, *h));
        break;
    }

    case component_type::none:
        break;

    case component_type::simulation:
        break;
    }

    return data.get_id(new_tree);
}

project::project(const project_reserve_definition&         res,
                 const simulation_reserve_definition&      sim_res,
                 const external_source_reserve_definition& srcs_res) noexcept
  : sim{ sim_res, srcs_res }
  , tree_nodes{ res.nodes.value() }
  , variable_observers{ res.vars.value() }
  , grid_observers{ res.grids.value() }
  , graph_observers{ res.graphs.value() }
  , parameters{ sim_res.models.value() }
  , observation_dir{ undefined<registred_path_id>() }
{}

status project::load(modeling& mod) noexcept
{
    if (const auto filename = make_file(mod, file); filename.has_value()) {
        auto file = file::open(*filename, file_mode{ file_open_options::read });

        if (file.has_value()) {
            const auto u8str = filename->u8string();
            const auto view  = std::string_view(
              reinterpret_cast<const char*>(u8str.data()), u8str.size());

            json_dearchiver dearc;
            return mod.files.read(
              [&](const auto& files, auto) noexcept -> status {
                  return mod.ids.read(
                    [&](const auto& ids, auto) noexcept -> status {
                        return dearc(*this, mod, sim, files, ids, view, *file);
                    });
              });
        } else
            return file.error();
    }

    return new_error(project_errc::file_access_error);
}

status project::save(modeling& mod) noexcept
{
    if (const auto filename = make_file(mod, file); filename.has_value()) {
        auto file =
          file::open(*filename, file_mode{ file_open_options::write });

        if (file.has_value()) {
            json_archiver arc;

            return mod.files.read(
              [&](const auto& files, auto) noexcept -> status {
                  return mod.ids.read(
                    [&](const auto& ids, auto) noexcept -> status {
                        return arc(
                          *this,
                          mod,
                          files,
                          ids,
                          *file,
                          json_archiver::print_option::indent_2_one_line_array);
                    });
              });
        } else
            return file.error();
    }

    return new_error(project_errc::file_access_error);
}

std::optional<std::filesystem::path> project::get_observation_dir(
  const irt::modeling& mod) const noexcept
{
    return mod.files.read(
      [&](const auto& fs,
          const auto /*vers*/) -> std::optional<std::filesystem::path> {
          if (const auto* dir = fs.registred_paths.try_to_get(observation_dir))
              return std::filesystem::path{ dir->path.sv() };

          return std::nullopt;
      });
}

class treenode_require_computer
{
public:
    table<component_id, project::required_data> map;

    project::required_data compute(const modeling&          mod,
                                   const component_access&  ids,
                                   const generic_component& g) noexcept
    {
        project::required_data ret;

        for (const auto& ch : g.children) {
            if (ch.type == child_type::component) {

                if (ids.exists(ch.id.compo_id))
                    ret += compute(mod, ids, ch.id.compo_id);
            } else {
                ++ret.model_nb;
            }
        }

        return ret;
    }

    project::required_data compute(const modeling&         mod,
                                   const component_access& ids,
                                   const grid_component&   g) noexcept
    {
        project::required_data ret;

        for (auto r = 0; r < g.row(); ++r) {
            for (auto c = 0; c < g.column(); ++c) {

                if (ids.exists(g.children()[g.pos(r, c)]))
                    ret += compute(mod, ids, g.children()[g.pos(r, c)]);
            }
        }

        return ret;
    }

    project::required_data compute(const modeling&         mod,
                                   const component_access& ids,
                                   const graph_component&  g) noexcept
    {
        project::required_data ret;

        for (const auto id : g.g.nodes) {
            const auto idx = get_index(id);

            if (ids.exists(g.g.node_components[idx]))
                ret += compute(mod, ids, g.g.node_components[idx]);
        }

        return ret;
    }

public:
    project::required_data compute(const modeling&         mod,
                                   const component_access& ids,
                                   const component_id      c_id) noexcept
    {
        project::required_data ret{ .tree_node_nb = 1 };
        const auto&            c = ids.components[c_id];

        if (auto* ptr = map.get(c_id)) {
            ret = *ptr;
        } else {
            switch (c.type) {
            case component_type::generic: {
                auto s_id = c.id.generic_id;
                if (auto* s = ids.generic_components.try_to_get(s_id); s)
                    ret += compute(mod, ids, *s);
            } break;

            case component_type::grid: {
                auto g_id = c.id.grid_id;
                if (auto* g = ids.grid_components.try_to_get(g_id); g)
                    ret += compute(mod, ids, *g);
            } break;

            case component_type::graph: {
                auto g_id = c.id.graph_id;
                if (auto* g = ids.graph_components.try_to_get(g_id); g)
                    ret += compute(mod, ids, *g);
            } break;

            case component_type::hsm:
                ret.hsm_nb++;
                break;

            case component_type::none:
                break;

            case component_type::simulation:
                break;
            }

            map.data.emplace_back(c_id, ret);
            map.sort();
        }

        return ret;
    }
};

project::required_data project::compute_memory_required(
  const modeling&    mod,
  const component_id c) const noexcept
{
    return mod.ids.read(
      [&](const auto& ids, auto) noexcept -> project::required_data {
          if (ids.exists(c))
              return treenode_require_computer().compute(mod, ids, c);
          else
              return project::required_data{};
      });
}

static expected<std::pair<tree_node_id, component_id>> set_project_from_hsm(
  simulation_copy&        sc,
  const component_access& ids,
  const component_id      compo_id) noexcept
{
    if (not sc.tree_nodes.can_alloc())
        return new_error(project_errc::memory_error);

    auto& compo = ids.components[compo_id];
    auto& tn    = sc.tree_nodes.alloc(compo_id, std::string_view{});
    tn.tree.set_id(&tn);

    auto* com_hsm = ids.hsm_components.try_to_get(compo.id.hsm_id);
    if (not com_hsm)
        return new_error(project_errc::component_unknown);

    auto* sim_hsm_id = sc.hsm_mod_to_sim.get(compo.id.hsm_id);
    if (not sim_hsm_id)
        return new_error(project_errc::component_unknown);

    auto* sim_hsm = sc.pj.sim.hsms.try_to_get(*sim_hsm_id);
    if (not sim_hsm)
        return new_error(project_errc::component_unknown);

    auto&      dyn     = sc.pj.sim.alloc<hsm_wrapper>();
    auto&      mdl     = irt::get_model(dyn);
    const auto mdl_id  = sc.pj.sim.models.get_id(mdl);
    const auto mdl_idx = get_index(mdl_id);

    sc.pj.sim.parameters[mdl_idx]
      .set_hsm_wrapper(ordinal(*sim_hsm_id))
      .set_hsm_wrapper(
        com_hsm->i1, com_hsm->i2, com_hsm->r1, com_hsm->r2, com_hsm->timeout);

    if (const auto* srcs_mod_to_sim = sc.srcs_mod_to_sim.get(compo_id)) {
        if (const auto opt = convert_mod_to_sim_source_id(
              std::span(srcs_mod_to_sim->data(), srcs_mod_to_sim->size()),
              com_hsm->src);
            opt.has_value())
            sc.pj.sim.parameters[mdl_idx].set_hsm_wrapper_value(opt->type,
                                                                opt->sim_id);
    }

    return std::make_pair(sc.tree_nodes.get_id(tn), compo_id);
}

status project::set(modeling&               mod,
                    const component_access& ids,
                    const component_id      compo_id) noexcept
{
    clear();

    auto numbers = compute_memory_required(mod, compo_id);
    numbers.fix();

    if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity())) {
        tree_nodes.reserve(numbers.tree_node_nb);

        if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity()))
            return new_error(project_errc::memory_error);
    }

    sim.clear();
    sim.grow_models_to(numbers.model_nb);
    sim.grow_connections_to(numbers.model_nb * 8);

    simulation_copy sc(*this, mod, tree_nodes);
    irt_check(sc.make_hsm_mod_to_sim(ids));
    irt_check(sc.make_component_cache(ids));

    const auto& compo = ids.components[compo_id];

    if (compo.type == component_type::hsm) {
        if (auto tn_compo = set_project_from_hsm(sc, ids, compo_id); tn_compo) {
            m_tn_head = tn_compo->first;
            m_head    = tn_compo->second;
        } else
            return tn_compo.error();
    } else {
        if (auto id = make_tree_from(sc, ids, tree_nodes, compo_id); id) {
            m_tn_head = *id;
            m_head    = compo_id;
        } else
            return id.error();
    }

    irt_check(simulation_copy_connections(sc, ids, *tn_head()));

    return success();
}

status project::set(modeling& mod, const component_id compo_id) noexcept
{
    return mod.ids.read([&](const auto& ids, auto) noexcept -> status {
        return set(mod, ids, compo_id);
    });
}

status project::rebuild(modeling& mod) noexcept
{
    const auto exists =
      mod.ids.read([&](const auto& ids, auto) noexcept -> bool {
          return ids.exists(head());
      });

    return exists ? set(mod, head()) : success();
}

void project::clear() noexcept
{
    sim.clear();

    tree_nodes.clear();

    m_head    = undefined<component_id>();
    m_tn_head = undefined<tree_node_id>();

    tree_nodes.clear();
    variable_observers.clear();
    grid_observers.clear();
    graph_observers.clear();
    file_obs.clear();
    parameters.clear();
}

static void project_build_unique_id_path(const std::string_view uid,
                                         unique_id_path&        out) noexcept
{
    out.clear();
    out.emplace_back(uid);
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

    std::reverse(out.begin(), out.end());
}

static void project_build_unique_id_path(const tree_node&       tn,
                                         const std::string_view mdl,
                                         unique_id_path&        out) noexcept
{
    out.clear();
    out.emplace_back(mdl);

    auto* parent = &tn;

    do {
        out.emplace_back(parent->unique_id);
        parent = parent->tree.get_parent();
    } while (parent);

    std::reverse(out.begin(), out.end());
}

auto project::build_relative_path(const tree_node& from,
                                  const tree_node& to,
                                  const model_id   mdl_id) noexcept
  -> relative_id_path
{
    debug::ensure(tree_nodes.get_id(from) != tree_nodes.get_id(to));
    debug::ensure(to.tree.get_parent() != nullptr);

    relative_id_path ret;

    if (const auto mdl_unique_id = to.get_unique_id(mdl_id);
        not mdl_unique_id.empty()) {
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

    if (auto* tn = tree_nodes.try_to_get(tn_id); tn)
        if (const auto uid = tn->get_unique_id(mdl_id); not uid.empty())
            build_unique_id_path(*tn, uid, out);
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
                                   const std::string_view model_unique_id,
                                   unique_id_path&        out) noexcept
{
    out.clear();

    return tree_nodes.get_id(model_unique_id_parent) == m_tn_head
             ? project_build_unique_id_path(model_unique_id, out)
             : project_build_unique_id_path(
                 model_unique_id_parent, model_unique_id, out);
}

auto project::get_model_path(const std::string_view id) const noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    auto model_id_opt = tn_head()->get_model_id(id);

    return model_id_opt.has_value()
             ? std::make_optional(std::make_pair(m_tn_head, *model_id_opt))
             : std::nullopt;
}

auto project::get_model_path(const unique_id_path& path) const noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    std::span<const name_str> stack{ path.data(), path.size() };

    if (const auto* head = tn_head(); head) {
        while (not stack.empty()) {
            if (stack.size() == 1) {
                const auto mdl = head->get_model_id(path[0].sv());
                if (mdl.has_value()) {
                    const auto tn_id  = tree_nodes.get_id(*head);
                    const auto mdl_id = *mdl;

                    return std::make_pair(tn_id, mdl_id);
                }

                return std::nullopt;
            }

            const auto sub = head->get_tree_node_id(stack.front().sv());
            if (sub.has_value()) {
                head  = tree_nodes.try_to_get(*sub);
                stack = stack.subspan(1);
            } else {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

auto project::get_tn_id(const unique_id_path& path) const noexcept
  -> tree_node_id
{
    std::span<const name_str> stack{ path.data(), path.size() };

    if (const auto* head = tn_head(); head) {
        switch (path.ssize()) {
        case 0:
            return undefined<tree_node_id>();

        case 1:
            return m_tn_head;

        default:
            while (stack.size() > 1) {
                const auto sub = head->get_tree_node_id(stack.front().sv());
                if (sub.has_value()) {
                    head  = tree_nodes.try_to_get(*sub);
                    stack = stack.subspan(1);
                } else {
                    return undefined<tree_node_id>();
                }
            }
        }
    }

    return undefined<tree_node_id>();
}

auto project::get_parameter(const tree_node_id tn_id,
                            const model_id     mdl_id) noexcept
  -> global_parameter_id
{
    const auto& tn_ids  = parameters.get<tree_node_id>();
    const auto& mdl_ids = parameters.get<model_id>();

    for (const auto id : parameters) {
        const auto idx = get_index(id);

        if (tn_ids[idx] == tn_id and mdl_ids[idx] == mdl_id)
            return id;
    }

    return undefined<global_parameter_id>();
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
    return std::any_of(
      obs.begin(), obs.end(), [str](const auto& o) noexcept -> bool {
          return o.name == str;
      });
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

template<typename Child>
static status import_in_generic(
  generic_component&                           gen,
  const data_array<Child, child_id>&           children,
  const data_array<connection, connection_id>& connections,
  const std::span<position>  positions  = std::span<position>{},
  const std::span<name_str>  names      = std::span<name_str>{},
  const std::span<parameter> parameters = std::span<parameter>{}) noexcept
{
    table<child_id, child_id> src_to_this;

    if (not gen.children.can_alloc(children.size())) {
        gen.children.reserve(children.size());
        if (not gen.children.can_alloc(children.size()))
            return new_error(modeling_errc::generic_children_container_full);
    }

    for (const auto& c : children) {
        auto new_c_id = undefined<child_id>();

        if constexpr (std::is_same_v<std::decay_t<Child>,
                                     generic_component::child>) {
            auto& new_c = c.type == child_type::component
                            ? gen.children.alloc(c.id.compo_id)
                            : gen.children.alloc(c.id.mdl_type);
            new_c_id    = gen.children.get_id(new_c);
        } else {
            auto& new_c = gen.children.alloc(c.compo_id);
            new_c_id    = gen.children.get_id(new_c);
        }

        src_to_this.data.emplace_back(children.get_id(c), new_c_id);
    }

    src_to_this.sort();

    for (const auto& con : connections) {
        if (auto* child_src = src_to_this.get(con.src); child_src) {
            if (auto* child_dst = src_to_this.get(con.dst); child_dst) {
                gen.connections.alloc(
                  *child_src, con.index_src, *child_dst, con.index_dst);
            }
        }
    }

    if (gen.children_positions.size() <= positions.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = gen.children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(gen.children.get_id(*dst));

                gen.children_positions[dst_idx] = positions[src_idx];
            }
        }
    }

    if (gen.children_names.size() <= names.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = gen.children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(gen.children.get_id(*dst));

                gen.children_names[dst_idx] =
                  gen.exists_child(names[src_idx].sv())
                    ? gen.make_unique_name_id(pair.value)
                    : names[src_idx];
            }
        }
    }

    if (gen.children_parameters.size() < parameters.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = gen.children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(gen.children.get_id(*dst));

                gen.children_parameters[dst_idx] = parameters[src_idx];
            }
        }
    }

    return success();
}

status component_access::import(const generic_component& generic,
                                generic_component&       dst) noexcept
{
    return import_in_generic(dst, generic.children, generic.connections);
}

status component_access::import(const graph_component& graph,
                                generic_component&     dst) noexcept
{
    data_array<graph_component::child, child_id> children;
    data_array<connection, connection_id>        connections;
    vector<name_str>                             cache_names;

    if (not graph.build_cache(*this, children, connections, cache_names))
        return new_error(modeling_errc::graph_children_container_full);

    return import_in_generic(dst, children, connections);
}

status component_access::import(const grid_component& grid,
                                generic_component&    dst) noexcept
{
    data_array<grid_component::child, child_id> children;
    data_array<connection, connection_id>       connections;
    vector<name_str>                            cache_names;

    if (not grid.build_cache(*this, children, connections, cache_names))
        return new_error(modeling_errc::grid_children_container_full);

    return import_in_generic(dst, children, connections);
}

status modeling::import(const component_id src,
                        const component_id dst,
                        const std::span<position> /*positions*/,
                        const std::span<name_str> /*names*/,
                        const std::span<parameter> /*parameters*/) noexcept
{
    return ids.write([&](auto& ids) noexcept -> status {
        if (not ids.exists(src) or not ids.exists(dst))
            return new_error(modeling_errc::component_not_found);

        const auto& src_compo = ids.components[src];
        auto&       dst_compo = ids.components[dst];

        if (dst_compo.type != component_type::generic)
            return new_error(modeling_errc::component_type_mismatch);

        auto& gen = ids.generic_components.get(dst_compo.id.generic_id);

        switch (src_compo.type) {
        case component_type::generic:
            return ids.import(
              ids.generic_components.get(src_compo.id.generic_id), gen);

        case component_type::graph:
            return ids.import(ids.graph_components.get(src_compo.id.graph_id),
                              gen);

        case component_type::grid:
            return ids.import(
              ids.generic_components.get(src_compo.id.generic_id), gen);

        case component_type::hsm:
            return success();

        case component_type::none:
            return success();

        case component_type::simulation:
            return success();
        }

        unreachable();
    });
}

} // namespace irt
