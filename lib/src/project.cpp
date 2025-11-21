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
    mod_to_sim_srcs(constant_source_id mid, constant_source_id sid) noexcept
      : mod_id{ ordinal(mid) }
      , sim_id{ ordinal(sid) }
      , type(source::source_type::constant)
    {}

    mod_to_sim_srcs(binary_file_source_id mid,
                    binary_file_source_id sid) noexcept
      : mod_id{ ordinal(mid) }
      , sim_id{ ordinal(sid) }
      , type(source::source_type::binary_file)
    {}

    mod_to_sim_srcs(text_file_source_id mid, text_file_source_id sid) noexcept
      : mod_id{ ordinal(mid) }
      , sim_id{ ordinal(sid) }
      , type(source::source_type::text_file)
    {}

    mod_to_sim_srcs(random_source_id mid, random_source_id sid) noexcept
      : mod_id{ ordinal(mid) }
      , sim_id{ ordinal(sid) }
      , type(source::source_type::random)
    {}

    u64 mod_id;
    u64 sim_id;

    source::source_type type;
};

u64 convert_source_id(const std::span<const mod_to_sim_srcs> srcs,
                      const source::source_type              type,
                      const u64                              id) noexcept
{
    const auto it = std::find_if(
      srcs.begin(), srcs.end(), [id, type](const auto& s) noexcept -> bool {
          return type == s.type and id == s.mod_id;
      });

    return it != srcs.end() ? it->sim_id : 0u;

    irt::unreachable();

    return 0u;
}

constexpr void convert_source(const std::span<const mod_to_sim_srcs> srcs,
                              const dynamics_type                    type,
                              parameter&                             p) noexcept
{
    switch (type) {
    case dynamics_type::dynamic_queue:
        p.integers[1] =
          convert_source_id(srcs,
                            enum_cast<source::source_type>(p.integers[2]),
                            static_cast<u64>(p.integers[1]));
        break;

    case dynamics_type::priority_queue:
        p.integers[1] =
          convert_source_id(srcs,
                            enum_cast<source::source_type>(p.integers[2]),
                            static_cast<u64>(p.integers[1]));
        break;

    case dynamics_type::generator: {
        const auto flags = bitflags<generator::option>(p.integers[0]);
        if (flags[generator::option::ta_use_source]) {
            p.integers[1] =
              convert_source_id(srcs,
                                enum_cast<source::source_type>(p.integers[2]),
                                static_cast<u64>(p.integers[1]));
        }

        if (flags[generator::option::value_use_source]) {
            p.integers[3] =
              convert_source_id(srcs,
                                enum_cast<source::source_type>(p.integers[4]),
                                static_cast<u64>(p.integers[3]));
        }
    } break;

    case dynamics_type::hsm_wrapper:
        p.integers[3] =
          convert_source_id(srcs,
                            enum_cast<source::source_type>(p.integers[4]),
                            static_cast<u64>(p.integers[3]));
        break;

    default:
        break;
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

struct simulation_copy {
    simulation_copy(project&                             pj_,
                    modeling&                            mod_,
                    data_array<tree_node, tree_node_id>& tree_nodes_) noexcept
      : pj{ pj_ }
      , mod(mod_)
      , tree_nodes(tree_nodes_)
    {
        /* For all hsm component, make a copy from modeling::hsm into
         * simulation::hsm to ensure link between simulation and modeling. */
        for (auto& modhsm : mod.hsm_components) {
            if (not pj.sim.hsms.can_alloc())
                break;

            auto& sim_hsm     = pj.sim.hsms.alloc(modhsm.machine);
            sim_hsm.parent_id = ordinal(mod.hsm_components.get_id(modhsm));

            const auto hsm_id = mod.hsm_components.get_id(modhsm);
            const auto sim_id = pj.sim.hsms.get_id(sim_hsm);

            hsm_mod_to_sim.data.emplace_back(hsm_id, sim_id);
        }

        hsm_mod_to_sim.sort();
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
};

static auto make_tree_recursive(simulation_copy&       sc,
                                tree_node&             parent,
                                component&             compo,
                                const std::string_view uid) noexcept
  -> expected<tree_node_id>;

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
                                    const port_id id) noexcept -> expected<int>
{
    const auto* compo = mod.components.try_to_get<component>(tn.id);
    if (not compo)
        return new_error(project_errc::import_error);

    if (not compo->x.exists(id))
        return new_error(project_errc::import_error);

    if (compo->type == component_type::generic) {
        auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
        return gen ? get_incoming_connection(*gen, id) : 0;
    }

    return new_error(project_errc::import_error);
}

static auto get_incoming_connection(const modeling&  mod,
                                    const tree_node& tn) noexcept
  -> expected<int>
{
    const auto* compo = mod.components.try_to_get<component>(tn.id);
    if (not compo)
        return new_error(project_errc::import_error);

    int nb = 0;
    compo->x.for_each_id([&](auto id) noexcept {
        if (compo->type == component_type::generic) {
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
                                     const port_id id) noexcept -> expected<int>
{
    const auto* compo = mod.components.try_to_get<component>(tn.id);
    if (not compo)
        return new_error(project_errc::import_error);

    if (not compo->y.exists(id))
        return new_error(project_errc::import_error);

    if (compo->type == component_type::generic) {
        auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
        return gen ? get_outcoming_connection(*gen, id) : 0;
    }

    return new_error(project_errc::import_error);
}

static auto get_outcoming_connection(const modeling&  mod,
                                     const tree_node& tn) noexcept
  -> expected<int>
{
    const auto* compo = mod.components.try_to_get<component>(tn.id);
    if (not compo)
        return new_error(project_errc::import_error);

    int nb = 0;
    compo->y.for_each_id([&](auto id) noexcept {
        if (compo->type == component_type::generic) {
            auto* gen = mod.generic_components.try_to_get(compo->id.generic_id);
            if (gen)
                nb += get_outcoming_connection(*gen, id);
        }
    });

    return nb;
}

static auto make_tree_hsm_leaf(simulation_copy&   sc,
                               generic_component& gen,
                               const child_id     ch_id,
                               const model_id     new_mdl_id,
                               hsm_wrapper&       dyn) noexcept -> status
{
    const auto child_index = get_index(ch_id);
    const auto compo_id =
      enum_cast<component_id>(gen.children_parameters[child_index].integers[0]);

    if (const auto* compo = sc.mod.components.try_to_get<component>(compo_id)) {
        if (compo->type == component_type::hsm) {
            const auto hsm_id = compo->id.hsm_id;
            debug::ensure(sc.mod.hsm_components.try_to_get(hsm_id));
            const auto* shsm    = sc.hsm_mod_to_sim.get(hsm_id);
            const auto  shsm_id = *shsm;
            sc.pj.sim.parameters[new_mdl_id].integers[0] = ordinal(shsm_id);
            dyn.id                                       = shsm_id;
            debug::ensure(sc.pj.sim.hsms.try_to_get(shsm_id));

            dyn.exec.i1 = static_cast<i32>(
              gen.children_parameters[child_index].integers[1]);
            dyn.exec.i2 = static_cast<i32>(
              gen.children_parameters[child_index].integers[2]);
            // @TODO missing external source in integers[3] and [4].

            dyn.exec.r1    = gen.children_parameters[child_index].reals[0];
            dyn.exec.r2    = gen.children_parameters[child_index].reals[1];
            dyn.exec.timer = gen.children_parameters[child_index].reals[2];

            return success();
        }
    }

    return new_error(project_errc::component_unknown);
}

static auto make_tree_constant_leaf(simulation_copy&   sc,
                                    tree_node&         parent,
                                    generic_component& gen,
                                    const child_id     ch_id,
                                    constant&          dyn) noexcept -> status
{
    const auto child_index = get_index(ch_id);

    switch (dyn.type) {
    case constant::init_type::constant:
        break;

    case constant::init_type::incoming_component_all: {
        if (auto nb = get_incoming_connection(sc.mod, parent); nb.has_value()) {
            gen.children_parameters[child_index].reals[0] = *nb;
            dyn.value                                     = *nb;
        } else
            return nb.error();
    } break;

    case constant::init_type::outcoming_component_all: {
        if (auto nb = get_outcoming_connection(sc.mod, parent);
            nb.has_value()) {
            gen.children_parameters[child_index].reals[0] = *nb;
            dyn.value                                     = *nb;
        } else
            return nb.error();
    } break;

    case constant::init_type::incoming_component_n: {
        const auto id = enum_cast<port_id>(dyn.port);
        if (not sc.mod.components.get<component>(parent.id).x.exists(id))
            return new_error(project_errc::component_port_x_unknown);

        if (auto nb = get_incoming_connection(sc.mod, parent, id);
            nb.has_value()) {
            gen.children_parameters[child_index].reals[0] = *nb;
            dyn.value                                     = *nb;
        } else
            return nb.error();
    } break;

    case constant::init_type::outcoming_component_n: {
        const auto id = enum_cast<port_id>(dyn.port);
        if (not sc.mod.components.get<component>(parent.id).y.exists(id))
            return new_error(project_errc::component_port_y_unknown);

        if (auto nb = get_outcoming_connection(sc.mod, parent, id);
            nb.has_value()) {
            gen.children_parameters[child_index].reals[0] = *nb;
            dyn.value                                     = *nb;
        } else
            return nb.error();
    } break;
    }

    return success();
}

static auto make_tree_leaf(simulation_copy&          sc,
                           tree_node&                parent,
                           generic_component&        gen,
                           const std::string_view    uid,
                           dynamics_type             mdl_type,
                           child_id                  ch_id,
                           generic_component::child& ch) noexcept
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

          if (auto* compo = sc.srcs_mod_to_sim.get(parent.id)) {
              convert_source(std::span(compo->data(), compo->size()),
                             new_mdl.type,
                             sc.pj.sim.parameters[new_mdl_id]);
          }

          sc.pj.sim.parameters[new_mdl_id].copy_to(new_mdl);

          if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
              irt_check(make_tree_hsm_leaf(sc, gen, ch_id, new_mdl_id, dyn));
          }

          if constexpr (std::is_same_v<Dynamics, constant>) {
              irt_check(make_tree_constant_leaf(sc, parent, gen, ch_id, dyn));
          }

          return success();
      }));

    const auto is_public = (ch.flags[child_flags::configurable] or
                            ch.flags[child_flags::observable]);

    // @TODO Add a test function in bitflags to allow:
    // if (ch.flags.test(child_flags::configurable, child_flags::configurable);

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

static status make_tree_recursive(simulation_copy&   sc,
                                  tree_node&         new_tree,
                                  generic_component& src) noexcept
{
    new_tree.children.resize(src.children.max_used());

    for (auto& child : src.children) {
        const auto child_id  = src.children.get_id(child);
        const auto child_idx = get_index(child_id);

        if (child.type == child_type::component) {
            const auto compo_id = child.id.compo_id;

            if (auto* compo = sc.mod.components.try_to_get<component>(compo_id);
                compo) {
                auto tn_id = make_tree_recursive(
                  sc, new_tree, *compo, src.children_names[child_idx].sv());

                if (not tn_id.has_value())
                    return tn_id.error();

                new_tree.children[child_id].set(
                  sc.tree_nodes.try_to_get(*tn_id));
            }
        } else {
            const auto mdl_type = child.id.mdl_type;
            auto       mdl_id   = make_tree_leaf(sc,
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

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  grid_component&  src) noexcept
{
    new_tree.children.resize(src.cache.max_used());

    for (auto& child : src.cache) {
        const auto child_id = src.cache.get_id(child);

        const auto compo_id = child.compo_id;

        if (auto* compo = sc.mod.components.try_to_get<component>(compo_id);
            compo) {
            auto tn_id = make_tree_recursive(
              sc, new_tree, *compo, src.cache_names[child_id].sv());
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

static status make_tree_recursive(simulation_copy& sc,
                                  tree_node&       new_tree,
                                  graph_component& src) noexcept
{
    new_tree.children.resize(src.cache.max_used());

    for (auto& child : src.cache) {
        const auto child_id = src.cache.get_id(child);
        const auto compo_id = child.compo_id;

        if (auto* compo = sc.mod.components.try_to_get<component>(compo_id);
            compo) {
            auto tn_id = make_tree_recursive(
              sc, new_tree, *compo, src.cache_names[child_id].sv());

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

static int count_sources(const external_source& srcs) noexcept
{
    return srcs.constant_sources.size() + srcs.binary_file_sources.size() +
           srcs.text_file_sources.size() + srcs.random_sources.size();
}

static bool external_sources_reserve_add(const external_source& src,
                                         external_source&       dst) noexcept
{
    return data_array_reserve_add(dst.constant_sources,
                                  src.constant_sources.size()) and
           data_array_reserve_add(dst.binary_file_sources,
                                  src.binary_file_sources.size()) and
           data_array_reserve_add(dst.text_file_sources,
                                  src.text_file_sources.size()) and
           data_array_reserve_add(dst.random_sources,
                                  src.random_sources.size());
}

static status external_source_copy(vector<mod_to_sim_srcs>& v,
                                   const external_source&   src,
                                   external_source&         dst) noexcept
{
    if (not external_sources_reserve_add(src, dst) or
        not vector_reserve_add(v, count_sources(src)))
        return new_error(external_source_errc::memory_error);

    for (const auto& rs : src.constant_sources) {
        auto& n_res    = dst.constant_sources.alloc(rs);
        auto  n_res_id = dst.constant_sources.get_id(n_res);
        auto  res_id   = src.constant_sources.get_id(rs);
        v.emplace_back(res_id, n_res_id);
    }

    for (const auto& rs : src.binary_file_sources) {
        auto& n_res    = dst.binary_file_sources.alloc(rs);
        auto  n_res_id = dst.binary_file_sources.get_id(n_res);
        auto  res_id   = src.binary_file_sources.get_id(rs);
        v.emplace_back(res_id, n_res_id);
    }

    for (const auto& rs : src.text_file_sources) {
        auto& n_res    = dst.text_file_sources.alloc(rs);
        auto  n_res_id = dst.text_file_sources.get_id(n_res);
        auto  res_id   = src.text_file_sources.get_id(rs);
        v.emplace_back(res_id, n_res_id);
    }

    for (const auto& rs : src.random_sources) {
        auto& n_res    = dst.random_sources.alloc(rs);
        auto  n_res_id = dst.random_sources.get_id(n_res);
        auto  res_id   = src.random_sources.get_id(rs);
        v.emplace_back(res_id, n_res_id);
    }

    return success();
}

static status make_tree_recursive([[maybe_unused]] simulation_copy& sc,
                                  [[maybe_unused]] tree_node&       new_tree,
                                  [[maybe_unused]] hsm_component& src) noexcept
{
    debug::ensure(sc.pj.sim.hsms.can_alloc());

    return success();
}

static status update_external_source(simulation_copy& sc,
                                     component&       compo) noexcept
{
    const auto compo_id = sc.mod.components.get_id(compo);

    if (const auto* exist = sc.srcs_mod_to_sim.get(compo_id); not exist) {
        if (const auto nb = count_sources(compo.srcs); nb > 0) {
            external_sources_reserve_add(compo.srcs, sc.pj.sim.srcs);
            sc.srcs_mod_to_sim.data.emplace_back(compo_id,
                                                 vector<mod_to_sim_srcs>());
            vector_reserve_add(sc.srcs_mod_to_sim.data.back().value, nb);

            irt_check(external_source_copy(sc.srcs_mod_to_sim.data.back().value,
                                           compo.srcs,
                                           sc.pj.sim.srcs));

            sc.srcs_mod_to_sim.sort();
        }
    }

    return success();
}

static auto make_tree_recursive(simulation_copy&       sc,
                                tree_node&             parent,
                                component&             compo,
                                const std::string_view unique_id) noexcept
  -> expected<tree_node_id>
{
    if (not sc.tree_nodes.can_alloc())
        return new_error(project_errc::memory_error);

    auto& new_tree =
      sc.tree_nodes.alloc(sc.mod.components.get_id(compo), unique_id);
    const auto tn_id = sc.tree_nodes.get_id(new_tree);
    new_tree.tree.set_id(&new_tree);
    new_tree.tree.parent_to(parent.tree);

    irt_check(update_external_source(sc, compo));

    switch (compo.type) {
    case component_type::generic: {
        auto s_id = compo.id.generic_id;
        if (auto* s = sc.mod.generic_components.try_to_get(s_id); s)
            irt_check(make_tree_recursive(sc, new_tree, *s));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::grid: {
        auto g_id = compo.id.grid_id;
        if (auto* g = sc.mod.grid_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::graph: {
        auto g_id = compo.id.graph_id;
        if (auto* g = sc.mod.graph_components.try_to_get(g_id); g)
            irt_check(make_tree_recursive(sc, new_tree, *g));
        parent.unique_id_to_tree_node_id.data.emplace_back(name_str(unique_id),
                                                           tn_id);
    } break;

    case component_type::none:
        break;

    case component_type::hsm:
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

static void get_input_models(vector<model_port>& inputs,
                             const simulation&   sim,
                             const modeling&     mod,
                             const tree_node&    tn,
                             const port_id       p) noexcept;

static void get_input_models(vector<model_port>&      inputs,
                             const simulation&        sim,
                             const modeling&          mod,
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
              inputs, sim, mod, *tn.children[con.dst].tn, con.port.compo);
        }
    }
}

static void get_input_models(vector<model_port>&    inputs,
                             const simulation&      sim,
                             const modeling&        mod,
                             const tree_node&       tn,
                             const graph_component& graph,
                             const port_id          p) noexcept
{
    for (const auto& con : graph.input_connections) {
        if (con.x != p)
            continue;

        if (not graph.g.nodes.exists(con.v))
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, sim, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_input_models(vector<model_port>&   inputs,
                             const simulation&     sim,
                             const modeling&       mod,
                             const tree_node&      tn,
                             const grid_component& grid,
                             const port_id         p) noexcept
{
    for (const auto& con : grid.input_connections) {
        if (con.x != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children()[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_input_models(inputs, sim, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_input_pack_models(
  vector<model_port>&                                   inputs,
  const simulation&                                     sim,
  const modeling&                                       mod,
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
                  inputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_pack_models(
  vector<model_port>&                                 inputs,
  const simulation&                                   sim,
  const modeling&                                     mod,
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
                  inputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_pack_models(
  vector<model_port>&                                inputs,
  const simulation&                                  sim,
  const modeling&                                    mod,
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
                  inputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_input_models(vector<model_port>& inputs,
                             const simulation&   sim,
                             const modeling&     mod,
                             const tree_node&    tn,
                             const port_id       p) noexcept
{
    auto* compo = mod.components.try_to_get<component>(tn.id);
    debug::ensure(compo != nullptr);
    if (not compo)
        return;

    switch (compo->type) {
    case component_type::generic: {
        if (auto* g = mod.generic_components.try_to_get(compo->id.generic_id)) {
            get_input_models(inputs, sim, mod, tn, *g, p);
            get_input_pack_models(inputs, sim, mod, tn, *compo, p, g->children);
        }
    } break;

    case component_type::graph: {
        if (auto* g = mod.graph_components.try_to_get(compo->id.graph_id)) {
            get_input_models(inputs, sim, mod, tn, *g, p);
            get_input_pack_models(inputs, sim, mod, tn, *compo, p, g->cache);
        }

    } break;

    case component_type::grid: {
        if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id)) {
            get_input_models(inputs, sim, mod, tn, *g, p);
            get_input_pack_models(inputs, sim, mod, tn, *compo, p, g->cache);
        }
    } break;

    case component_type::hsm:
    case component_type::none:
        break;
    }
}

static void get_output_models(vector<model_port>& outputs,
                              const simulation&   sim,
                              const modeling&     mod,
                              const tree_node&    tn,
                              const port_id       p) noexcept;

static void get_output_models(vector<model_port>&      outputs,
                              const simulation&        sim,
                              const modeling&          mod,
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
              outputs, sim, mod, *tn.children[con.src].tn, con.port.compo);
        }
    }
}

static void get_output_models(vector<model_port>&    outputs,
                              const simulation&      sim,
                              const modeling&        mod,
                              const tree_node&       tn,
                              const graph_component& graph,
                              const port_id          p) noexcept
{
    for (const auto& con : graph.output_connections) {
        if (con.y != p)
            continue;

        if (not graph.g.nodes.exists(con.v))
            continue;

        const auto idx = get_index(con.v);
        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, sim, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_output_models(vector<model_port>&   outputs,
                              const simulation&     sim,
                              const modeling&       mod,
                              const tree_node&      tn,
                              const grid_component& grid,
                              const port_id         p) noexcept
{
    for (const auto& con : grid.output_connections) {
        if (con.y != p)
            continue;

        const auto idx = grid.pos(con.row, con.col);
        if (is_undefined(grid.children()[idx]))
            continue;

        debug::ensure(tn.children[idx].tn);
        get_output_models(outputs, sim, mod, *tn.children[idx].tn, con.id);
    }
}

static void get_output_pack_models(
  vector<model_port>&                                   outputs,
  const simulation&                                     sim,
  const modeling&                                       mod,
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
                  outputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_pack_models(
  vector<model_port>&                                 outputs,
  const simulation&                                   sim,
  const modeling&                                     mod,
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
                  outputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_pack_models(
  vector<model_port>&                                outputs,
  const simulation&                                  sim,
  const modeling&                                    mod,
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
                  outputs, sim, mod, *tn.children[idx].tn, con.child_port);
            }
        }
    }
}

static void get_output_models(vector<model_port>& outputs,
                              const simulation&   sim,
                              const modeling&     mod,
                              const tree_node&    tn,
                              const port_id       p) noexcept
{
    auto* compo = mod.components.try_to_get<component>(tn.id);
    debug::ensure(compo != nullptr);
    if (not compo)
        return;

    switch (compo->type) {
    case component_type::generic: {
        if (auto* g = mod.generic_components.try_to_get(compo->id.generic_id)) {
            get_output_models(outputs, sim, mod, tn, *g, p);
            get_output_pack_models(
              outputs, sim, mod, tn, *compo, p, g->children);
        }
    } break;

    case component_type::graph: {
        if (auto* g = mod.graph_components.try_to_get(compo->id.graph_id)) {
            get_output_models(outputs, sim, mod, tn, *g, p);
            get_output_pack_models(outputs, sim, mod, tn, *compo, p, g->cache);
        }
    } break;

    case component_type::grid: {
        if (auto* g = mod.grid_components.try_to_get(compo->id.grid_id)) {
            get_output_models(outputs, sim, mod, tn, *g, p);
            get_output_pack_models(outputs, sim, mod, tn, *compo, p, g->cache);
        }
    } break;

    case component_type::hsm:
    case component_type::none:
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
  simulation_copy&                             sc) -> status
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
            const auto& c        = sc.mod.components.get<component>(compo_id);

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
            const auto& c        = sc.mod.components.get<component>(compo_id);

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
static auto get_input_connection_type(const modeling&    mod,
                                      const component_id compo_id,
                                      const port_id&     p_id) noexcept
  -> port_option
{
    const auto& compo = mod.components.get<component>(compo_id);
    return compo.x.get<port_option>(p_id);
}

/** Get the @a port_option of the @a p_id port of the @a compo_id
 * component. */
static auto get_output_connection_type(const modeling&    mod,
                                       const component_id compo_id,
                                       const port_id&     p_id) noexcept
  -> port_option
{
    const auto& compo = mod.components.get<component>(compo_id);
    return compo.y.get<port_option>(p_id);
}

/** Adds @a qss3_sum_4 connections in place of the @a port to sum all the input
 * connection of the component @a compo.
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
  tree_node&                                   tree,
  const data_array<Child, child_id>&           children,
  const data_array<connection, connection_id>& connections) noexcept
{
    if (auto ret = prepare_sum_connections(tree, connections, sc); not ret)
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
                get_input_models(sc.inputs, sc.pj.sim, sc.mod, *tn, port);

                input_type = get_input_connection_type(sc.mod, tn->id, port);
            }
        } else {
            port = cnx.index_src.compo;
            tn   = tree.children[src_idx].tn;
            get_output_models(sc.outputs, sc.pj.sim, sc.mod, *tn, port);

            output_type = get_output_connection_type(
              sc.mod, tree.children[src_idx].tn->id, cnx.index_src.compo);

            if (tree.is_model(cnx.dst)) {
                sc.inputs.emplace_back(tree.children[dst_idx].mdl,
                                       cnx.index_dst.model);
            } else {
                port = cnx.index_dst.compo;
                tn   = tree.children[dst_idx].tn;
                get_input_models(sc.inputs, sc.pj.sim, sc.mod, *tn, port);

                input_type = get_input_connection_type(sc.mod, tn->id, port);
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

static status simulation_copy_connections(simulation_copy& sc,
                                          tree_node&       tree,
                                          component&       compo)
{
    switch (compo.type) {
    case component_type::generic: {
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
    sc.stack.clear();
    sc.stack.emplace_back(&head);

    while (not sc.stack.empty()) {
        auto cur = sc.stack.back();
        sc.stack.pop_back();

        if (auto* compo = sc.mod.components.try_to_get<component>(cur->id);
            compo)
            irt_check(simulation_copy_connections(sc, *cur, *compo));

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            sc.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            sc.stack.emplace_back(child);
    }

    return success();
}

static status make_component_cache(project& /*pj*/, modeling& mod) noexcept
{
    for (auto& grid : mod.grid_components)
        irt_check(grid.build_cache(mod));

    for (auto& graph : mod.graph_components) {
        if (auto ret = graph.build_cache(mod); not ret.has_value())
            return new_error(project_errc::component_cache_error);
    }

    return success();
}

static auto make_tree_from(simulation_copy&                     sc,
                           data_array<tree_node, tree_node_id>& data,
                           component& parent) noexcept -> expected<tree_node_id>
{
    if (not data.can_alloc())
        return new_error(project_errc::memory_error);

    auto& new_tree =
      data.alloc(sc.mod.components.get_id(parent), std::string_view{});
    new_tree.tree.set_id(&new_tree);
    new_tree.unique_id = "root";

    if (const auto nb = count_sources(parent.srcs); nb > 0) {
        external_sources_reserve_add(parent.srcs, sc.pj.sim.srcs);

        sc.srcs_mod_to_sim.data.emplace_back(sc.mod.components.get_id(parent),
                                             vector<mod_to_sim_srcs>());

        vector_reserve_add(sc.srcs_mod_to_sim.data.back().value, nb);

        irt_check(external_source_copy(
          sc.srcs_mod_to_sim.data.back().value, parent.srcs, sc.pj.sim.srcs));
    }

    switch (parent.type) {
    case component_type::generic: {
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

    case component_type::none:
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

std::string_view project::get_observation_dir(
  const irt::modeling& mod) const noexcept
{
    const auto* dir = mod.registred_paths.try_to_get(observation_dir);
    return dir ? dir->path.sv() : std::string_view{};
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
                const auto* sub_c =
                  mod.components.try_to_get<component>(ch.id.compo_id);
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

        for (auto r = 0; r < g.row(); ++r) {
            for (auto c = 0; c < g.column(); ++c) {
                const auto* sub_c = mod.components.try_to_get<component>(
                  g.children()[g.pos(r, c)]);
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

        for (const auto id : g.g.nodes) {
            const auto  idx = get_index(id);
            const auto* sub_c =
              mod.components.try_to_get<component>(g.g.node_components[idx]);

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
            case component_type::generic: {
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

            case component_type::none:
                break;
            }

            map.data.emplace_back(mod.components.get_id(c), ret);
            map.sort();
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

static expected<std::pair<tree_node_id, component_id>> set_project_from_hsm(
  simulation_copy& sc,
  const component& compo) noexcept
{
    const auto compo_id = sc.mod.components.get_id(compo);

    if (not sc.tree_nodes.can_alloc())
        return new_error(project_errc::memory_error);

    auto& tn = sc.tree_nodes.alloc(compo_id, std::string_view{});
    tn.tree.set_id(&tn);

    auto* com_hsm = sc.mod.hsm_components.try_to_get(compo.id.hsm_id);
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
      .set_hsm_wrapper_value(com_hsm->src)
      .set_hsm_wrapper(
        com_hsm->i1, com_hsm->i2, com_hsm->r1, com_hsm->r2, com_hsm->timeout);

    return std::make_pair(sc.tree_nodes.get_id(tn), compo_id);
}

status project::set(modeling& mod, component& compo) noexcept
{
    clear();

    auto numbers = compute_memory_required(mod, compo);
    numbers.fix();

    if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity())) {
        tree_nodes.reserve(numbers.tree_node_nb);

        if (std::cmp_greater(numbers.tree_node_nb, tree_nodes.capacity()))
            return new_error(project_errc::memory_error);
    }

    irt_check(make_component_cache(*this, mod));

    sim.clear();
    sim.grow_models_to(numbers.model_nb);
    sim.grow_connections_to(numbers.model_nb * 8);

    simulation_copy sc(*this, mod, tree_nodes);

    if (compo.type == component_type::hsm) {
        if (auto tn_compo = set_project_from_hsm(sc, compo); tn_compo) {
            m_tn_head = tn_compo->first;
            m_head    = tn_compo->second;
        } else
            return tn_compo.error();
    } else {
        if (auto id = make_tree_from(sc, tree_nodes, compo); id) {
            const auto compo_id = sc.mod.components.get_id(compo);
            m_tn_head           = *id;
            m_head              = compo_id;
        } else
            return id.error();
    }

    irt_check(simulation_copy_connections(sc, *tn_head()));

    return success();
}

status project::rebuild(modeling& mod) noexcept
{
    auto* compo = mod.components.try_to_get<component>(head());

    return compo ? set(mod, *compo) : success();
}

void project::clear() noexcept
{
    name.clear();
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

static auto project_get_model_path(const project&         pj,
                                   const tree_node&       head,
                                   const std::string_view path) noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    if (auto mdl_id_opt = head.get_model_id(path); mdl_id_opt.has_value()) {
        return std::make_pair(pj.tree_nodes.get_id(head), *mdl_id_opt);
    }

    return std::nullopt;
}

static auto project_get_model_path(const project&        pj,
                                   const tree_node&      head,
                                   const unique_id_path& path) noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    debug::ensure(path.ssize() > 2);
    debug::ensure(path[0] == "root");

    const auto* tn = &head;

    for (int i = 1, e = path.ssize() - 1; i < e; ++i) {
        if (auto tn_id_opt = tn->get_tree_node_id(path[i].sv());
            tn_id_opt.has_value()) {
            if (tn = pj.tree_nodes.try_to_get(*tn_id_opt); not tn)
                return std::nullopt;
        } else {
            return std::nullopt;
        }
    }

    if (auto mdl_id_opt = tn->get_model_id(path.back().sv());
        mdl_id_opt.has_value()) {
        return std::make_pair(pj.tree_nodes.get_id(*tn), *mdl_id_opt);
    }

    return std::nullopt;
}

auto project::get_model_path(const unique_id_path& path) const noexcept
  -> std::optional<std::pair<tree_node_id, model_id>>
{
    if (const auto* head = tn_head(); head) {
        switch (path.ssize()) {
        case 0:
        case 1:
            return std::nullopt;

        case 2:
            return path[0] == "root"
                     ? project_get_model_path(*this, *head, path[1].sv())
                     : std::nullopt;

        default:
            return path[0] == "root"
                     ? project_get_model_path(*this, *head, path)
                     : std::nullopt;
        }

        unreachable();
    }

    return std::nullopt;
}

static auto project_get_tn_id(const project&        pj,
                              const tree_node&      head,
                              const unique_id_path& ids) noexcept
  -> tree_node_id
{
    debug::ensure(ids.ssize() > 1);
    debug::ensure(ids[0] == "root");

    const auto* tn  = &head;
    auto        ret = undefined<tree_node_id>();

    for (int i = 1; i < ids.ssize(); ++i) {
        if (auto tn_id_opt = tn->get_tree_node_id(ids[i].sv());
            tn_id_opt.has_value()) {
            if (tn = pj.tree_nodes.try_to_get(*tn_id_opt); not tn)
                return undefined<tree_node_id>();

            ret = *tn_id_opt;
        } else {
            return undefined<tree_node_id>();
        }
    }

    return ret;
}

auto project::get_tn_id(const unique_id_path& path) const noexcept
  -> tree_node_id
{
    if (const auto* head = tn_head(); head) {
        switch (path.ssize()) {
        case 0:
            return undefined<tree_node_id>();

        case 1:
            return path[0] == "root" ? m_tn_head : undefined<tree_node_id>();

        default:
            return path[0] == "root" ? project_get_tn_id(*this, *head, path)
                                     : undefined<tree_node_id>();
        }
    }

    return undefined<tree_node_id>();
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

} // namespace irt
