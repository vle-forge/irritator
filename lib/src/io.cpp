// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

using namespace std::string_view_literals;

namespace irt {

auto get_internal_component_type(std::string_view name) noexcept
  -> std::optional<internal_component>
{
    struct string_to_type {
        constexpr string_to_type(const std::string_view   n,
                                 const internal_component t) noexcept
          : name(n)
          , type(t)
        {}

        std::string_view   name;
        internal_component type;
    };

    static constexpr string_to_type table[] = {
        { "qss1_izhikevich", internal_component::qss1_izhikevich },
        { "qss1_lif", internal_component::qss1_lif },
        { "qss1_lotka_volterra", internal_component::qss1_lotka_volterra },
        { "qss1_negative_lif", internal_component::qss1_negative_lif },
        { "qss1_seirs", internal_component::qss1_seirs },
        { "qss1_van_der_pol", internal_component::qss1_van_der_pol },
        { "qss2_izhikevich", internal_component::qss2_izhikevich },
        { "qss2_lif", internal_component::qss2_lif },
        { "qss2_lotka_volterra", internal_component::qss2_lotka_volterra },
        { "qss2_negative_lif", internal_component::qss2_negative_lif },
        { "qss2_seirs", internal_component::qss2_seirs },
        { "qss2_van_der_pol", internal_component::qss2_van_der_pol },
        { "qss3_izhikevich", internal_component::qss3_izhikevich },
        { "qss3_lif", internal_component::qss3_lif },
        { "qss3_lotka_volterra", internal_component::qss3_lotka_volterra },
        { "qss3_negative_lif", internal_component::qss3_negative_lif },
        { "qss3_seirs", internal_component::qss3_seirs },
        { "qss3_van_der_pol", internal_component::qss3_van_der_pol },
    };

    auto it = binary_find(
      std::begin(table),
      std::end(table),
      name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == std::end(table) ? std::nullopt : std::make_optional(it->type);
}

auto get_component_type(std::string_view name) noexcept
  -> std::optional<component_type>
{
    struct string_to_type {
        constexpr string_to_type(const std::string_view n,
                                 const component_type   t) noexcept
          : name(n)
          , type(t)
        {}

        std::string_view name;
        component_type   type;
    };

    static constexpr string_to_type table[] = {
        { "graph", component_type::graph },    { "grid", component_type::grid },
        { "hsm", component_type::hsm },        { "none", component_type::none },
        { "simple", component_type::generic },
    };

    auto it = binary_find(
      std::begin(table),
      std::end(table),
      name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == std::end(table) ? std::nullopt : std::make_optional(it->type);
}

auto get_dynamics_type(std::string_view dynamics_name) noexcept
  -> std::optional<dynamics_type>
{
    struct string_to_type {
        constexpr string_to_type(const std::string_view n,
                                 const dynamics_type    t) noexcept
          : name(n)
          , type(t)
        {}

        std::string_view name;
        dynamics_type    type;
    };

    static constexpr string_to_type table[] = {
        { "accumulator_2", dynamics_type::accumulator_2 },
        { "constant", dynamics_type::constant },
        { "counter", dynamics_type::counter },
        { "dynamic_queue", dynamics_type::dynamic_queue },
        { "generator", dynamics_type::generator },
        { "hsm_wrapper", dynamics_type::hsm_wrapper },
        { "logical_and_2", dynamics_type::logical_and_2 },
        { "logical_and_3", dynamics_type::logical_and_3 },
        { "logical_invert", dynamics_type::logical_invert },
        { "logical_or_2", dynamics_type::logical_or_2 },
        { "logical_or_3", dynamics_type::logical_or_3 },
        { "priority_queue", dynamics_type::priority_queue },
        { "qss1_compare", dynamics_type::qss1_compare },
        { "qss1_cross", dynamics_type::qss1_cross },
        { "qss1_filter", dynamics_type::qss1_filter },
        { "qss1_integer", dynamics_type::qss1_integer },
        { "qss1_integrator", dynamics_type::qss1_integrator },
        { "qss1_invert", dynamics_type::qss1_invert },
        { "qss1_multiplier", dynamics_type::qss1_multiplier },
        { "qss1_power", dynamics_type::qss1_power },
        { "qss1_square", dynamics_type::qss1_square },
        { "qss1_sum_2", dynamics_type::qss1_sum_2 },
        { "qss1_sum_3", dynamics_type::qss1_sum_3 },
        { "qss1_sum_4", dynamics_type::qss1_sum_4 },
        { "qss1_wsum_2", dynamics_type::qss1_wsum_2 },
        { "qss1_wsum_3", dynamics_type::qss1_wsum_3 },
        { "qss1_wsum_4", dynamics_type::qss1_wsum_4 },
        { "qss2_compare", dynamics_type::qss2_compare },
        { "qss2_cross", dynamics_type::qss2_cross },
        { "qss2_filter", dynamics_type::qss2_filter },
        { "qss2_integer", dynamics_type::qss2_integer },
        { "qss2_integrator", dynamics_type::qss2_integrator },
        { "qss2_invert", dynamics_type::qss2_invert },
        { "qss2_multiplier", dynamics_type::qss2_multiplier },
        { "qss2_power", dynamics_type::qss2_power },
        { "qss2_square", dynamics_type::qss2_square },
        { "qss2_sum_2", dynamics_type::qss2_sum_2 },
        { "qss2_sum_3", dynamics_type::qss2_sum_3 },
        { "qss2_sum_4", dynamics_type::qss2_sum_4 },
        { "qss2_wsum_2", dynamics_type::qss2_wsum_2 },
        { "qss2_wsum_3", dynamics_type::qss2_wsum_3 },
        { "qss2_wsum_4", dynamics_type::qss2_wsum_4 },
        { "qss3_compare", dynamics_type::qss3_compare },
        { "qss3_cross", dynamics_type::qss3_cross },
        { "qss3_filter", dynamics_type::qss3_filter },
        { "qss3_integer", dynamics_type::qss3_integer },
        { "qss3_integrator", dynamics_type::qss3_integrator },
        { "qss3_invert", dynamics_type::qss3_invert },
        { "qss3_multiplier", dynamics_type::qss3_multiplier },
        { "qss3_power", dynamics_type::qss3_power },
        { "qss3_square", dynamics_type::qss3_square },
        { "qss3_sum_2", dynamics_type::qss3_sum_2 },
        { "qss3_sum_3", dynamics_type::qss3_sum_3 },
        { "qss3_sum_4", dynamics_type::qss3_sum_4 },
        { "qss3_wsum_2", dynamics_type::qss3_wsum_2 },
        { "qss3_wsum_3", dynamics_type::qss3_wsum_3 },
        { "qss3_wsum_4", dynamics_type::qss3_wsum_4 },
        { "queue", dynamics_type::queue },
        { "time_func", dynamics_type::time_func }
    };

    static_assert(std::size(table) == static_cast<sz>(dynamics_type_size()));

    auto it = binary_find(
      std::begin(table),
      std::end(table),
      dynamics_name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == std::end(table) ? std::nullopt : std::make_optional(it->type);
}

auto get_distribution_type(std::string_view name) noexcept
  -> std::optional<distribution_type>
{
    struct string_to_type {
        constexpr string_to_type(const std::string_view  n,
                                 const distribution_type t) noexcept
          : name(n)
          , type(t)
        {}

        std::string_view  name;
        distribution_type type;
    };

    static constexpr string_to_type table[] = {
        { "bernouilli", distribution_type::bernouilli },
        { "binomial", distribution_type::binomial },
        { "cauchy", distribution_type::cauchy },
        { "chi_squared", distribution_type::chi_squared },
        { "exponential", distribution_type::exponential },
        { "exterme_value", distribution_type::exterme_value },
        { "fisher_f", distribution_type::fisher_f },
        { "gamma", distribution_type::gamma },
        { "geometric", distribution_type::geometric },
        { "lognormal", distribution_type::lognormal },
        { "negative_binomial", distribution_type::negative_binomial },
        { "normal", distribution_type::normal },
        { "poisson", distribution_type::poisson },
        { "student_t", distribution_type::student_t },
        { "uniform_int", distribution_type::uniform_int },
        { "uniform_real", distribution_type::uniform_real },
        { "weibull", distribution_type::weibull }
    };

    auto it = binary_find(
      std::begin(table),
      std::end(table),
      name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == std::end(table) ? std::nullopt : std::make_optional(it->type);
}

void write_dot_graph_simulation(std::ostream&     os,
                                const simulation& sim) noexcept
{
    os << "digraph simulation {\n";

    for (const auto& mdl : sim.models) {
        const auto id  = sim.models.get_id(mdl);
        const auto idx = get_index(id);
        const auto dyn = dynamics_type_names[ordinal(mdl.type)];

        os << ' ' << idx << " [dynamics=" << dyn << "];\n";
    }

    for (const auto& mdl : sim.models) {
        dispatch(
          mdl,
          []<typename Dynamics>(
            Dynamics& dyn, const auto& sim, const auto src_mdl_id, auto& os) {
              if constexpr (has_output_port<Dynamics>) {
                  for (int i = 0, e = length(dyn.y); i != e; ++i) {
                      for (auto* block = sim.nodes.try_to_get(dyn.y[i]); block;
                           block       = sim.nodes.try_to_get(block->next)) {

                          const auto src_idx  = get_index(src_mdl_id);
                          const auto port_out = get_output_port_names<Dynamics>(
                            dot_output_names)[i];

                          for (auto it = block->nodes.begin(),
                                    et = block->nodes.end();
                               it != et;
                               ++it) {
                              if (const auto* dst =
                                    sim.models.try_to_get(it->model)) {
                                  os << " " << src_idx << ":" << port_out
                                     << " -> " << get_index(it->model) << ":"
                                     << get_input_port_names(
                                          dst->type,
                                          dot_input_names)[it->port_index]
                                     << "\n";
                              }
                          }
                      }
                  }
              }
          },
          sim,
          sim.models.get_id(mdl),
          os);
    }

    os << "}\n";
}

static void write_test_simulation_header(std::ostream&          os,
                                         const std::string_view name,
                                         const simulation&      sim) noexcept
{
    fmt::print(os,
               R"(
"{}"_test = [] {{
    irt::simulation sim;

    expect(fatal(sim.can_alloc({})));
    expect(fatal(sim.hsms.can_alloc({})));
)",
               name,
               sim.models.ssize(),
               sim.hsms.ssize());
}

struct override_parameter {
    override_parameter() = default;

    override_parameter& operator=(const parameter& p) noexcept
    {
        for (sz i = 0, e = reals.size(); i != e; ++i) {
            reals[i].clear();

            if (p.reals[i] == std::numeric_limits<real>::infinity())
                reals[i] = "std::numeric_limits<real>::infinity()";
            else if (p.reals[i] == -std::numeric_limits<real>::infinity())
                reals[i] = "-std::numeric_limits<real>::infinity()";
            else
                fmt::format_to(std::back_inserter(reals[i]),
                               "{:.{}f}",
                               p.reals[i],
                               std::numeric_limits<real>::max_digits10);
        }

        for (sz i = 0, e = integers.size(); i != e; ++i)
            integers[i] = fmt::format("{}", p.integers[i]);

        return *this;
    }

    std::array<std::string, 8> reals    = {};
    std::array<std::string, 8> integers = {};
};

static void write_test_simulation_model(
  std::ostream&             os,
  const simulation&         sim,
  const model&              mdl,
  const override_parameter& params) noexcept
{
    const auto idx = get_index(sim.models.get_id(mdl));

    fmt::print(os,
               R"(
    auto& mdl_{} = sim.alloc<irt::{}>();
    sim.parameters[sim.get_id(mdl_{})].reals    = {{ {}, {}, {}, {}, {}, {}, {}, {} }};
    sim.parameters[sim.get_id(mdl_{})].integers = {{ {}, {}, {}, {}, {}, {}, {}, {} }};
)",
               idx,
               irt::dynamics_type_names[ordinal(mdl.type)],
               idx,
               params.reals[0],
               params.reals[1],
               params.reals[2],
               params.reals[3],
               params.reals[4],
               params.reals[5],
               params.reals[6],
               params.reals[7],
               idx,
               params.integers[0],
               params.integers[1],
               params.integers[2],
               params.integers[3],
               params.integers[4],
               params.integers[5],
               params.integers[6],
               params.integers[7]);
}

static void assign_constant_source(std::string&   id,
                                   std::string&   type,
                                   const irt::u64 cpp) noexcept
{
    id.clear();
    fmt::format_to(std::back_inserter(id),
                   "sim.srcs.constant_sources.get_id(constant_src_{})",
                   cpp);

    type.clear();
    fmt::format_to(
      std::back_inserter(type), "{}", ordinal(source::source_type::constant));
}

static void write_test_simulation_model_hsm(
  std::ostream&                         os,
  override_parameter&                   override_param,
  const simulation&                     sim,
  const model&                          mdl,
  const table<u64, u64>&                hsm_sim_to_cpp,
  const table<constant_source_id, u64>& constant_source_sim_to_cpp) noexcept
{
    const auto& params     = sim.parameters[sim.models.get_id(mdl)];
    const auto  sim_hsm_id = enum_cast<hsm_id>(params.integers[0]);
    const auto* hsm        = sim.hsms.try_to_get(sim_hsm_id);
    debug::ensure(hsm);

    override_param = params;

    if (auto* cpp = hsm_sim_to_cpp.get(to_unsigned(params.integers[0]))) {
        override_param.integers[0].clear();
        fmt::format_to(std::back_inserter(override_param.integers[0]),
                       "sim.hsms.get_id(hsm_{})",
                       *cpp);
    }

    if (hsm->flags[hierarchical_state_machine::option::use_source]) {
        if (auto* cpp = constant_source_sim_to_cpp.get(
              enum_cast<constant_source_id>(params.integers[3]))) {
            assign_constant_source(
              override_param.integers[3], override_param.integers[4], *cpp);
        }
    }

    write_test_simulation_model(os, sim, mdl, override_param);
}

static void write_test_simulation_model_source(
  std::ostream&                         os,
  override_parameter&                   override_param,
  const simulation&                     sim,
  const model&                          mdl,
  const table<constant_source_id, u64>& sim_to_cpp,
  const int                             index_1,
  const int                             index_2) noexcept
{
    const auto idx = get_index(sim.models.get_id(mdl));
    override_param = sim.parameters[idx];

    if (const auto* cpp_1 = sim_to_cpp.get(enum_cast<constant_source_id>(
          sim.parameters[idx].integers[index_1]))) {
        assign_constant_source(override_param.integers[index_1],
                               override_param.integers[index_1 + 1],
                               *cpp_1);
    }

    if (const auto* cpp_2 = sim_to_cpp.get(enum_cast<constant_source_id>(
          sim.parameters[idx].integers[index_2]))) {
        assign_constant_source(override_param.integers[index_2],
                               override_param.integers[index_2 + 1],
                               *cpp_2);
    }

    write_test_simulation_model(os, sim, mdl, override_param);
}

static void write_test_simulation_models(
  std::ostream&                         os,
  const simulation&                     sim,
  const table<u64, u64>&                hsm_sim_to_cpp,
  const table<constant_source_id, u64>& constant_source_sim_to_cpp) noexcept
{
    override_parameter params;

    for (const auto& mdl : sim.models) {
        switch (mdl.type) {
        case dynamics_type::hsm_wrapper:
            write_test_simulation_model_hsm(
              os, params, sim, mdl, hsm_sim_to_cpp, constant_source_sim_to_cpp);
            break;

        case dynamics_type::dynamic_queue:
            write_test_simulation_model_source(
              os, params, sim, mdl, constant_source_sim_to_cpp, 1, -1);
            break;

        case dynamics_type::priority_queue:
            write_test_simulation_model_source(
              os, params, sim, mdl, constant_source_sim_to_cpp, 1, -1);
            break;

        case dynamics_type::generator: {
            const auto& dyn = get_dyn<generator>(mdl);
            write_test_simulation_model_source(
              os,
              params,
              sim,
              mdl,
              constant_source_sim_to_cpp,
              dyn.flags[generator::option::ta_use_source] ? 1 : -1,
              dyn.flags[generator::option::value_use_source] ? 3 : -1);
        } break;

        default:
            params = sim.parameters[sim.get_id(mdl)];
            write_test_simulation_model(os, sim, mdl, params);
            break;
        }
    }
}

static void write_test_simulation_hsm_state(
  std::ostream&                                   os,
  const sz                                        hsm_index,
  const std::string_view                          action_name,
  const hierarchical_state_machine::state_action& state,
  sz                                              state_index) noexcept
{
    fmt::print(os,
               R"(
    hsm_{}.states[{}].{}.var1 = irt::enum_cast<irt::hierarchical_state_machine::variable>({});
    hsm_{}.states[{}].{}.var2 = irt::enum_cast<irt::hierarchical_state_machine::variable>({});
    hsm_{}.states[{}].{}.type = irt::enum_cast<irt::hierarchical_state_machine::action_type>({});)",
               hsm_index,
               state_index,
               action_name,
               ordinal(state.var1),
               hsm_index,
               state_index,
               action_name,
               ordinal(state.var2),
               hsm_index,
               state_index,
               action_name,
               ordinal(state.type));

    if (state.var2 == hierarchical_state_machine::variable::constant_i) {
        fmt::print(os,
                   R"(
    hsm_{}.states[{}].{}.constant.i = {};)",
                   hsm_index,
                   state_index,
                   action_name,
                   state.constant.i);
    } else if (state.var2 == hierarchical_state_machine::variable::constant_r) {
        fmt::print(os,
                   R"(
    hsm_{}.states[{}].{}.constant.f = {};)",
                   hsm_index,
                   state_index,
                   action_name,
                   state.constant.f);
    }
}

static void write_test_simulation_hsm_condition(
  std::ostream&                                       os,
  const sz                                            hsm_index,
  const hierarchical_state_machine::condition_action& condition,
  sz                                                  state_index) noexcept
{
    if (condition.type == hierarchical_state_machine::condition_type::port) {
        fmt::print(os,
                   R"(
    hsm_{}.states[{}].condition.type = irt::enum_cast<irt::hierarchical_state_machine::condition_type>({});
    hsm_{}.states[{}].condition.constant.u = {};)",
                   hsm_index,
                   state_index,
                   ordinal(condition.type),
                   hsm_index,
                   state_index,
                   condition.constant.u);
    } else {
        fmt::print(os,
                   R"(
    hsm_{}.states[{}].condition.var1 = irt::enum_cast<irt::hierarchical_state_machine::variable>({});
    hsm_{}.states[{}].condition.var2 = irt::enum_cast<irt::hierarchical_state_machine::variable>({});
    hsm_{}.states[{}].condition.type = irt::enum_cast<irt::hierarchical_state_machine::condition_type>({});)",
                   hsm_index,
                   state_index,
                   ordinal(condition.var1),
                   hsm_index,
                   state_index,
                   ordinal(condition.var2),
                   hsm_index,
                   state_index,
                   ordinal(condition.type));

        if (condition.var2 ==
            hierarchical_state_machine::variable::constant_i) {
            fmt::print(os,
                       R"(
    hsm_{}.states[{}].condition.constant.i = {};)",
                       hsm_index,
                       state_index,
                       condition.constant.i);
        } else if (condition.var2 ==
                   hierarchical_state_machine::variable::constant_r) {
            fmt::print(os,
                       R"(
    hsm_{}.states[{}].condition.constant.f = {};)",
                       hsm_index,
                       state_index,
                       condition.constant.f);
        }
    }
}

static void write_test_simulation_hsm_state(
  std::ostream&                            os,
  const sz                                 hsm_index,
  const hierarchical_state_machine::state& state,
  sz                                       state_index) noexcept
{
    fmt::print(os,
               R"(
    hsm_{}.set_state({}, {}, {});
    hsm_{}.states[{}].if_transition = {};
    hsm_{}.states[{}].else_transition = {};)",
               hsm_index,
               state_index,
               state.super_id,
               state.sub_id,
               hsm_index,
               state_index,
               state.if_transition,
               hsm_index,
               state_index,
               state.else_transition);

    write_test_simulation_hsm_state(
      os, hsm_index, "enter_action", state.enter_action, state_index);
    write_test_simulation_hsm_state(
      os, hsm_index, "exit_action", state.exit_action, state_index);
    write_test_simulation_hsm_state(
      os, hsm_index, "if_action", state.if_action, state_index);
    write_test_simulation_hsm_state(
      os, hsm_index, "else_action", state.else_action, state_index);
    write_test_simulation_hsm_condition(
      os, hsm_index, state.condition, state_index);
}

static void write_test_simulation_hsm(
  std::ostream&                     os,
  const u64                         hsm_index,
  const hierarchical_state_machine& hsm) noexcept
{
    fmt::print(os,
               R"(
    auto& hsm_{} = sim.hsms.alloc();
    hsm_{}.top_state = {};
    hsm_{}.constants = {{ {}, {}, {}, {}, {}, {}, {}, {} }};
)",
               hsm_index,
               hsm_index,
               hsm.top_state,
               hsm_index,
               hsm.constants[0],
               hsm.constants[1],
               hsm.constants[2],
               hsm.constants[3],
               hsm.constants[4],
               hsm.constants[5],
               hsm.constants[6],
               hsm.constants[7]);

    constexpr auto length  = hierarchical_state_machine::max_number_of_state;
    constexpr auto invalid = hierarchical_state_machine::invalid_state_id;

    std::array<bool, length> states_to_write{};
    states_to_write.fill(false);

    if (hsm.top_state != invalid)
        states_to_write[hsm.top_state] = true;

    for (auto i = 0; i != length; ++i) {
        if (hsm.states[i].if_transition != invalid)
            states_to_write[hsm.states[i].if_transition] = true;
        if (hsm.states[i].else_transition != invalid)
            states_to_write[hsm.states[i].else_transition] = true;
        if (hsm.states[i].super_id != invalid)
            states_to_write[hsm.states[i].super_id] = true;
        if (hsm.states[i].sub_id != invalid)
            states_to_write[hsm.states[i].sub_id] = true;
    }

    for (auto i = 0; i != length; ++i)
        if (states_to_write[i])
            write_test_simulation_hsm_state(os, hsm_index, hsm.states[i], i);
}

static void write_test_simulation_constant_source(
  std::ostream&          os,
  const u64              src_index,
  const constant_source& src) noexcept
{
    fmt::print(os,
               R"(
    auto& constant_src_{} = sim.srcs.constant_sources.alloc());
    format(constant_src_{}.name, "source-{}", src_index);
    constant_src_{}.length = {};
    constant_src_{}.buffer = {{ )",
               src_index,
               src_index,
               src_index,
               src_index,
               src.length,
               src_index);

    for (auto i = 0u; i < src.length; ++i)
        os << src.buffer[i] << ((i + 1 >= src.length) ? "} " : ", ");

    os << '\n';
}

static bool write_test_simulation_constant_sources(
  std::ostream&                   os,
  const simulation&               sim,
  table<constant_source_id, u64>& sim_to_cpp) noexcept
{
    debug::ensure(sim_to_cpp.data.empty());

    // Build the list of constant source used in the simulation and
    // prepare a new identifier for hsm_wrapper parameter[0].
    u64 i = 0;
    for (const auto& mdl : sim.models) {
        switch (mdl.type) {
        case dynamics_type::hsm_wrapper: {
            const auto& dyn = get_dyn<hsm_wrapper>(mdl);
            if (const auto* hsm = sim.hsms.try_to_get(dyn.id)) {
                if (hsm
                      ->flags[hierarchical_state_machine::option::use_source]) {
                    if (dyn.exec.source_value.type !=
                        source::source_type::constant)
                        return false;

                    sim_to_cpp.set(dyn.exec.source_value.id.constant_id, i++);
                }
            }
        } break;

        case dynamics_type::dynamic_queue:
        case dynamics_type::priority_queue: {
            const auto& params = sim.parameters[sim.get_id(mdl)];
            if (enum_cast<source::source_type>(params.integers[2]) !=
                source::source_type::constant)
                return false;
            sim_to_cpp.set(enum_cast<constant_source_id>(params.integers[1]),
                           i++);
        } break;

        case dynamics_type::generator: {
            const auto& params = sim.parameters[sim.get_id(mdl)];
            const auto  flags = bitflags<generator::option>(params.integers[0]);

            if (flags[generator::option::ta_use_source]) {
                if (enum_cast<source::source_type>(params.integers[2]) !=
                    source::source_type::constant)
                    return false;
                sim_to_cpp.set(
                  enum_cast<constant_source_id>(params.integers[1]), i++);
            }

            if (flags[generator::option::value_use_source]) {
                if (enum_cast<source::source_type>(params.integers[4]) !=
                    source::source_type::constant)
                    return false;
                sim_to_cpp.set(
                  enum_cast<constant_source_id>(params.integers[3]), i++);
            }
        } break;

        default:
            break;
        }
    }

    // Write this new list of hierarchical_state_machine with the new
    // identifier.
    for (const auto& pair : sim_to_cpp.data) {
        if (const auto* src = sim.srcs.constant_sources.try_to_get(pair.id)) {
            write_test_simulation_constant_source(os, pair.value, *src);
        } else {
            return false;
        }
    }

    return true;
}

static bool write_test_simulation_hsm(std::ostream&     os,
                                      const simulation& sim,
                                      table<u64, u64>&  sim_to_cpp) noexcept
{
    debug::ensure(sim_to_cpp.data.empty());

    // Build the list of hierarchical_state_machine used in the
    // simulation and prepare a new identifier for hsm_wrapper
    // parameter[0].
    u64 i = 0;
    for (const auto& mdl : sim.models) {
        if (mdl.type == dynamics_type::hsm_wrapper) {
            const auto& params = sim.parameters[sim.get_id(mdl)];
            const auto  hsm_id = to_unsigned(params.integers[0]);
            if (sim_to_cpp.get(hsm_id) == nullptr) {
                sim_to_cpp.set(hsm_id, i++);
            }
        }
    }

    // Write this new list of hierarchical_state_machine with the new
    // identifier.
    for (const auto& pair : sim_to_cpp.data) {
        if (const auto* hsm = sim.hsms.try_to_get(enum_cast<hsm_id>(pair.id))) {
            write_test_simulation_hsm(os, pair.value, *hsm);
        } else {
            return false;
        }
    }

    return true;
}

static void write_test_simulation_connections(std::ostream&     os,
                                              const simulation& sim) noexcept
{
    for (const auto& mdl : sim.models) {
        dispatch(
          mdl,
          []<typename Dynamics>(
            Dynamics& dyn, const auto& sim, const auto src_mdl_id, auto& os) {
              if constexpr (has_output_port<Dynamics>) {
                  for (int i = 0, e = length(dyn.y); i != e; ++i) {
                      for (auto* block = sim.nodes.try_to_get(dyn.y[i]); block;
                           block       = sim.nodes.try_to_get(block->next)) {

                          const auto src_idx = get_index(src_mdl_id);
                          for (auto it = block->nodes.begin(),
                                    et = block->nodes.end();
                               it != et;
                               ++it) {
                              if (const auto* dst =
                                    sim.models.try_to_get(it->model)) {
                                  fmt::print(os,
                                             R"(
    expect(sim.connect_dynamics(mdl_{}, {}, mdl_{}, {}).has_value());)",
                                             src_idx,
                                             i,
                                             get_index(it->model),
                                             it->port_index);
                              }
                          }
                      }
                  }
              }
          },
          sim,
          sim.models.get_id(mdl),
          os);
    }
}

static void write_test_simulation_loop(std::ostream& os,
                                       const time    begin,
                                       const time    end) noexcept
{
    fmt::print(os,
               R"(
    sim.t = {};
    expect(fatal(sim.srcs.prepare().has_value()));
    expect(fatal(sim.initialize().has_value()));

    irt::status st;

    do {{
        st = sim.run();
        expect(fatal(st.has_value()));
    }} while (sim.t < {});

    expect(fatal(st.has_value()));
}};
)",
               begin,
               end);
}

auto write_test_simulation(std::ostream&                       os,
                           const std::string_view              name,
                           const simulation&                   sim,
                           const time                          begin,
                           const time                          end,
                           const write_test_simulation_options opts) noexcept
  -> write_test_simulation_result
{
    table<u64, u64>                hsm_sim_to_cpp;
    table<constant_source_id, u64> constant_sim_to_cpp;

    write_test_simulation_header(os, name, sim);

    if (not write_test_simulation_constant_sources(
          os, sim, constant_sim_to_cpp))
        return write_test_simulation_result::external_source_error;

    if (not write_test_simulation_hsm(os, sim, hsm_sim_to_cpp))
        return write_test_simulation_result::hsm_error;

    write_test_simulation_models(os, sim, hsm_sim_to_cpp, constant_sim_to_cpp);
    write_test_simulation_connections(os, sim);
    write_test_simulation_loop(os, begin, end);

    if (opts == write_test_simulation_options::test_finish) {
        for (const auto& mdl : sim.models) {
            if (mdl.type == dynamics_type::constant) {
                dispatch(
                  mdl,
                  []<typename Dynamics>(
                    const Dynamics& dyn, auto& os, const auto idx) {
                      if constexpr (std::is_same_v<Dynamics, irt::counter>) {
                          fmt::print(os,
                                     R"(
    expect(eq(mdl_{}.number, static_cast<i64>({})));
    expect(eq(mdl_{}.last_value, {}));
)",
                                     idx,
                                     dyn.number,
                                     idx,
                                     dyn.last_value);
                      }
                  },
                  os,
                  get_index(sim.get_id(mdl)));
            }
        }
    }

    return os.good() ? write_test_simulation_result::success
                     : write_test_simulation_result::output_error;
}

} // namespace irt
