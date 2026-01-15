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
        { "qss1_cos", dynamics_type::qss1_cos },
        { "qss1_cross", dynamics_type::qss1_cross },
        { "qss1_exp", dynamics_type::qss1_exp },
        { "qss1_filter", dynamics_type::qss1_filter },
        { "qss1_flipflop", dynamics_type::qss1_flipflop },
        { "qss1_gain", dynamics_type::qss1_gain },
        { "qss1_integer", dynamics_type::qss1_integer },
        { "qss1_integrator", dynamics_type::qss1_integrator },
        { "qss1_inverse", dynamics_type::qss1_inverse },
        { "qss1_log", dynamics_type::qss1_log },
        { "qss1_multiplier", dynamics_type::qss1_multiplier },
        { "qss1_power", dynamics_type::qss1_power },
        { "qss1_sin", dynamics_type::qss1_sin },
        { "qss1_square", dynamics_type::qss1_square },
        { "qss1_sum_2", dynamics_type::qss1_sum_2 },
        { "qss1_sum_3", dynamics_type::qss1_sum_3 },
        { "qss1_sum_4", dynamics_type::qss1_sum_4 },
        { "qss1_wsum_2", dynamics_type::qss1_wsum_2 },
        { "qss1_wsum_3", dynamics_type::qss1_wsum_3 },
        { "qss1_wsum_4", dynamics_type::qss1_wsum_4 },
        { "qss2_compare", dynamics_type::qss2_compare },
        { "qss2_cos", dynamics_type::qss2_cos },
        { "qss2_cross", dynamics_type::qss2_cross },
        { "qss2_exp", dynamics_type::qss2_exp },
        { "qss2_filter", dynamics_type::qss2_filter },
        { "qss2_flipflop", dynamics_type::qss2_flipflop },
        { "qss2_gain", dynamics_type::qss2_gain },
        { "qss2_integer", dynamics_type::qss2_integer },
        { "qss2_integrator", dynamics_type::qss2_integrator },
        { "qss2_inverse", dynamics_type::qss2_inverse },
        { "qss2_log", dynamics_type::qss2_log },
        { "qss2_multiplier", dynamics_type::qss2_multiplier },
        { "qss2_power", dynamics_type::qss2_power },
        { "qss2_sin", dynamics_type::qss2_sin },
        { "qss2_square", dynamics_type::qss2_square },
        { "qss2_sum_2", dynamics_type::qss2_sum_2 },
        { "qss2_sum_3", dynamics_type::qss2_sum_3 },
        { "qss2_sum_4", dynamics_type::qss2_sum_4 },
        { "qss2_wsum_2", dynamics_type::qss2_wsum_2 },
        { "qss2_wsum_3", dynamics_type::qss2_wsum_3 },
        { "qss2_wsum_4", dynamics_type::qss2_wsum_4 },
        { "qss3_compare", dynamics_type::qss3_compare },
        { "qss3_cos", dynamics_type::qss3_cos },
        { "qss3_cross", dynamics_type::qss3_cross },
        { "qss3_exp", dynamics_type::qss3_exp },
        { "qss3_filter", dynamics_type::qss3_filter },
        { "qss3_flipflop", dynamics_type::qss3_flipflop },
        { "qss3_gain", dynamics_type::qss3_gain },
        { "qss3_integer", dynamics_type::qss3_integer },
        { "qss3_integrator", dynamics_type::qss3_integrator },
        { "qss3_inverse", dynamics_type::qss3_inverse },
        { "qss3_log", dynamics_type::qss3_log },
        { "qss3_multiplier", dynamics_type::qss3_multiplier },
        { "qss3_power", dynamics_type::qss3_power },
        { "qss3_sin", dynamics_type::qss3_sin },
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
                      const auto  y_id = dyn.y[i];
                      const auto& y    = sim.output_ports.get(y_id);

                      const auto src_idx = get_index(src_mdl_id);
                      const auto port_out =
                        get_output_port_names<Dynamics>(dot_output_names)[i];

                      for (auto it = y.connections.begin(),
                                et = y.connections.end();
                           it != et;
                           ++it) {
                          if (const auto* dst =
                                sim.models.try_to_get(it->model)) {
                              os
                                << " " << src_idx << ":" << port_out << " -> "
                                << get_index(it->model) << ":"
                                << get_input_port_names(
                                     dst->type, dot_input_names)[it->port_index]
                                << "\n";
                          }
                      }

                      for (auto* block = sim.nodes.try_to_get(y.next); block;
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

class parameter_to_string
{
public:
    parameter_to_string() noexcept
    {
        for (auto& str : m_reals)
            str.reserve(64);

        for (auto& str : m_integers)
            str.reserve(32);
    }

    template<typename Enum>
        requires(std::is_enum_v<Enum>)
    constexpr auto reals(const parameter& p, const Enum index) noexcept
      -> std::string_view
    {
        m_reals[ordinal(index)].clear();

        if (p.reals[ordinal(index)] == std::numeric_limits<real>::infinity())
            m_reals[ordinal(index)] = "std::numeric_limits<real>::infinity()";
        else if (p.reals[ordinal(index)] ==
                 -std::numeric_limits<real>::infinity())
            m_reals[ordinal(index)] = "-std::numeric_limits<real>::infinity()";
        else
            fmt::format_to(std::back_inserter(m_reals[ordinal(index)]),
                           "{:.{}g}",
                           p.reals[ordinal(index)],
                           std::numeric_limits<real>::max_digits10);

        return m_reals[ordinal(index)];
    }

    template<typename Enum>
        requires(std::is_enum_v<Enum>)
    constexpr auto integers(const parameter& p, const Enum index) noexcept
      -> std::string_view
    {
        m_integers[ordinal(index)].clear();

        fmt::format_to(std::back_inserter(m_integers[ordinal(index)]),
                       "{}",
                       p.integers[ordinal(index)]);

        return m_integers[ordinal(index)];
    }

private:
    std::array<std::string, 4> m_reals    = {};
    std::array<std::string, 4> m_integers = {};
};

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

static void write_test_simulation_model(std::ostream&        os,
                                        const simulation&    sim,
                                        const model&         mdl,
                                        const parameter&     param,
                                        parameter_to_string& o_param) noexcept
{
    const auto idx = get_index(sim.models.get_id(mdl));

    fmt::print(os,
               R"(
    auto& mdl_{} = sim.alloc<irt::{}>();
)",
               idx,
               irt::dynamics_type_names[ordinal(mdl.type)]);

    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss3_integrator:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_integrator({}, {});)",
          idx,
          o_param.reals(param, qss_integrator_tag::X),
          o_param.reals(param, qss_integrator_tag::dQ));
        break;

    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_cross({}, {}, {});)",
          idx,
          o_param.reals(param, qss_cross_tag::threshold),
          o_param.reals(param, qss_cross_tag::up_value),
          o_param.reals(param, qss_cross_tag::bottom_value));
        break;

    case dynamics_type::qss1_filter:
    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_filter({}, {});)",
          idx,
          o_param.reals(param, qss_filter_tag::lower_bound),
          o_param.reals(param, qss_filter_tag::upper_bound));
        break;

    case dynamics_type::qss1_power:
    case dynamics_type::qss2_power:
    case dynamics_type::qss3_power:
        fmt::print(os,
                   R"(    sim.parameters[sim.get_id(mdl_{})].set_power({});)",
                   idx,
                   o_param.reals(param, qss_power_tag::exponent));
        break;

    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss3_wsum_2:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_wsum2({}, {});)",
          idx,
          o_param.reals(param, qss_wsum_2_tag::coeff1),
          o_param.reals(param, qss_wsum_2_tag::coeff2));
        break;

    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_wsum_3:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_wsum3({}, {}, {});)",
          idx,
          o_param.reals(param, qss_wsum_3_tag::coeff1),
          o_param.reals(param, qss_wsum_3_tag::coeff2),
          o_param.reals(param, qss_wsum_3_tag::coeff3));
        break;

    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_wsum_4:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_wsum4({}, {}, {}, {});)",
          idx,
          o_param.reals(param, qss_wsum_4_tag::coeff1),
          o_param.reals(param, qss_wsum_4_tag::coeff2),
          o_param.reals(param, qss_wsum_4_tag::coeff3),
          o_param.reals(param, qss_wsum_4_tag::coeff4));
        break;

    case dynamics_type::qss1_compare:
    case dynamics_type::qss2_compare:
    case dynamics_type::qss3_compare:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_compare({}, {});)",
          idx,
          o_param.reals(param, qss_compare_tag::equal),
          o_param.reals(param, qss_compare_tag::not_equal));
        break;

    case dynamics_type::qss1_gain:
    case dynamics_type::qss2_gain:
    case dynamics_type::qss3_gain:
        fmt::print(os,
                   R"(    sim.parameters[sim.get_id(mdl_{})].set_gain({});)",
                   idx,
                   o_param.reals(param, qss_gain_tag::k));
        break;

    case dynamics_type::queue:
        fmt::print(os,
                   R"(    sim.parameters[sim.get_id(mdl_{})].set_queue({});)",
                   idx,
                   o_param.reals(param, queue_tag::sigma));
        break;

    case dynamics_type::dynamic_queue:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_dynamic_queue_ta(get_source({}));)",
          idx,
          o_param.integers(param, dynamic_queue_tag::source_ta));
        break;

    case dynamics_type::priority_queue:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_priority_queue({});)",
          R"(    sim.parameters[sim.get_id(mdl_{})].set_priority_queue_ta(get_source({}));)",
          idx,
          o_param.reals(param, priority_queue_tag::sigma),
          o_param.integers(param, priority_queue_tag::source_ta));
        break;

    case dynamics_type::generator: {
        const auto flags =
          bitflags<generator::option>(param.integers[generator_tag::i_options]);

        if (flags[generator::option::ta_use_source]) {
            fmt::print(
              os,
              R"(    sim.parameters[sim.get_id(mdl_{})].set_generator_ta(get_source({}));)",
              idx,
              o_param.integers(param, generator_tag::source_ta));
        }

        if (flags[generator::option::value_use_source]) {
            fmt::print(
              os,
              R"(    sim.parameters[sim.get_id(mdl_{})].set_generator_value(get_source({}));)",
              idx,
              o_param.integers(param, generator_tag::source_value));
        }
    } break;

    case dynamics_type::constant:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_constant({}, {});)",
          idx,
          o_param.reals(param, constant_tag::value),
          o_param.reals(param, constant_tag::offset));
        break;

    case dynamics_type::time_func:
        fmt::print(
          os,
          R"(    sim.parameters[sim.get_id(mdl_{})].set_time_func({}, {}, {});)",
          idx,
          o_param.reals(param, time_func_tag::offset),
          o_param.reals(param, time_func_tag::timestep),
          o_param.integers(param, time_func_tag::i_type));
        break;

    case dynamics_type::hsm_wrapper: {
        const auto& dyn = get_dyn<hsm_wrapper>(mdl);
        if (const auto* hsm = sim.hsms.try_to_get(dyn.id)) {
            if (hsm->flags[hierarchical_state_machine::option::use_source]) {
                fmt::print(
                  os,
                  R"(    sim.parameters[sim.get_id(mdl_{})].set_hsm_wrapper_value(get_source({}));)",
                  idx,
                  o_param.integers(param, hsm_wrapper_tag::source_value));
                break;
            }
        }

        fmt::print(os,
                   R"(
    sim.parameters[sim.get_id(mdl_{})].set_hsm_wrapper({});
    sim.parameters[sim.get_id(mdl_{})].set_hsm_wrapper({}, {}, {}, {}, {});
)",
                   idx,
                   o_param.integers(param, hsm_wrapper_tag::id),
                   idx,
                   o_param.integers(param, hsm_wrapper_tag::i1),
                   o_param.integers(param, hsm_wrapper_tag::i2),
                   o_param.reals(param, hsm_wrapper_tag::r1),
                   o_param.reals(param, hsm_wrapper_tag::r2),
                   o_param.reals(param, hsm_wrapper_tag::timer));
    } break;

    default:
        break;
    }
}

static void write_test_simulation_models(std::ostream&     os,
                                         const simulation& sim) noexcept
{
    parameter_to_string params;

    for (const auto& mdl : sim.models)
        write_test_simulation_model(
          os, sim, mdl, sim.parameters[sim.get_id(mdl)], params);
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
    hsm_{}.states[{}].{}.constant.f = {:g};)",
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
    hsm_{}.states[{}].condition.constant.f = {:g};)",
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

static void write_test_simulation_hsm(std::ostream&                     os,
                                      const hierarchical_state_machine& hsm,
                                      const hsm_id id) noexcept
{
    const auto hsm_index = get_index(id);

    fmt::print(os,
               R"(
    auto& hsm_{} = sim.hsms.alloc();
    hsm_{}.top_state = {};
    hsm_{}.constants = {{ {:g}, {:g}, {:g}, {:g}, {:g}, {:g}, {:g}, {:g} }};
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
  std::ostream&            os,
  const constant_source&   src,
  const constant_source_id id) noexcept
{
    const auto src_index = get_index(id);

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

static bool store_if_constant(table<constant_source_id, u64>& sim_to_cpp,
                              u64                             param) noexcept
{
    const auto src = get_source(param);

    if (src.first == source_type::constant) {
        if (auto* ptr = sim_to_cpp.get(src.second.constant_id)) {
            (*ptr)++;
            return true;
        }
    }

    return false;
}

static bool write_constant_sources(std::ostream&     os,
                                   const simulation& sim) noexcept
{
    table<constant_source_id, u64> sim_to_cpp(sim.srcs.constant_sources.size(),
                                              reserve_tag);

    for (const auto& cst : sim.srcs.constant_sources)
        sim_to_cpp.set(sim.srcs.constant_sources.get_id(cst), 0);

    // Build the list of constant source used in the simulation
    for (const auto& mdl : sim.models) {
        switch (mdl.type) {
        case dynamics_type::hsm_wrapper: {
            const auto& dyn = get_dyn<hsm_wrapper>(mdl);
            if (const auto* hsm = sim.hsms.try_to_get(dyn.id)) {
                if (hsm
                      ->flags[hierarchical_state_machine::option::use_source]) {
                    const auto& params = sim.parameters[sim.get_id(mdl)];

                    if (not store_if_constant(
                          sim_to_cpp,
                          params.integers[hsm_wrapper_tag::source_value]))
                        return false;
                }
            }
        } break;

        case dynamics_type::dynamic_queue: {
            const auto& params = sim.parameters[sim.get_id(mdl)];

            if (not store_if_constant(
                  sim_to_cpp, params.integers[dynamic_queue_tag::source_ta]))
                return false;
        } break;

        case dynamics_type::priority_queue: {
            const auto& params = sim.parameters[sim.get_id(mdl)];

            if (not store_if_constant(
                  sim_to_cpp, params.integers[dynamic_queue_tag::source_ta]))
                return false;
        } break;

        case dynamics_type::generator: {
            const auto& params = sim.parameters[sim.get_id(mdl)];
            const auto  flags  = bitflags<generator::option>(
              params.integers[generator_tag::i_options]);

            if (flags[generator::option::ta_use_source]) {
                if (not store_if_constant(
                      sim_to_cpp, params.integers[generator_tag::source_ta]))
                    return false;
            }

            if (flags[generator::option::value_use_source]) {
                if (not store_if_constant(
                      sim_to_cpp, params.integers[generator_tag::source_value]))
                    return false;
            }
        } break;

        default:
            break;
        }
    }

    // Write the list of constant source where the number of uses is greater
    // than 0.
    for (const auto& pair : sim_to_cpp.data) {
        if (pair.value > 0) {
            const auto& src = sim.srcs.constant_sources.get(pair.id);
            write_test_simulation_constant_source(os, src, pair.id);
        }
    }

    return true;
}

static bool write_test_simulation_hsm(std::ostream&     os,
                                      const simulation& sim) noexcept
{
    table<hsm_id, u64> sim_to_cpp(sim.hsms.size(), reserve_tag);

    for (const auto& hsm : sim.hsms)
        sim_to_cpp.set(sim.hsms.get_id(hsm), 0);

    // Build the list of hierarchical_state_machine used in the
    // simulation and prepare a new identifier for hsm_wrapper
    for (const auto& mdl : sim.models) {
        if (mdl.type == dynamics_type::hsm_wrapper) {
            const auto& params = sim.parameters[sim.get_id(mdl)];
            const auto  uid = to_unsigned(params.integers[hsm_wrapper_tag::id]);
            const auto  id  = enum_cast<hsm_id>(uid);

            if (auto* ptr = sim_to_cpp.get(id))
                (*ptr)++;
            else
                return false;
        }
    }

    // Write this new list of hierarchical_state_machine with the new
    // identifier.
    for (const auto& pair : sim_to_cpp.data) {
        if (pair.value > 0) {
            const auto& src = sim.hsms.get(pair.id);
            write_test_simulation_hsm(os, src, pair.id);
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
                      const auto  y_id = dyn.y[i];
                      const auto& y    = sim.output_ports.get(y_id);

                      const auto src_idx = get_index(src_mdl_id);
                      for (auto it = y.connections.begin(),
                                et = y.connections.end();
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

                      for (auto* block = sim.nodes.try_to_get(y.next); block;
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
    sim.limits.set_bound({:g}, {:g});
    expect(fatal(sim.srcs.prepare().has_value()));
    expect(fatal(sim.initialize().has_value()));

    do {{
        expect(sim.run().has_value());
    }} while (not sim.current_time_expired());

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
    write_test_simulation_header(os, name, sim);

    if (not sim.srcs.constant_sources.empty() and
        not write_constant_sources(os, sim))
        return write_test_simulation_result::external_source_error;

    if (not sim.hsms.empty() and not write_test_simulation_hsm(os, sim))
        return write_test_simulation_result::hsm_error;

    write_test_simulation_models(os, sim);
    write_test_simulation_connections(os, sim);
    write_test_simulation_loop(os, begin, end);

    if (opts == write_test_simulation_options::test_finish) {
        for (const auto& mdl : sim.models) {
            if (mdl.type == dynamics_type::counter) {
                dispatch(
                  mdl,
                  []<typename Dynamics>(
                    const Dynamics& dyn, auto& os, const auto idx) {
                      if constexpr (std::is_same_v<Dynamics, irt::counter>) {
                          fmt::print(os,
                                     R"(
    expect(eq(mdl_{}.number, static_cast<irt::i64>({})));
    expect(eq(mdl_{}.last_value, {:g}));
)",
                                     idx,
                                     dyn.event_number,
                                     idx,
                                     dyn.last_value);
                      }
                  },
                  os,
                  get_index(sim.get_id(mdl)));
            }
        }
    }

    fmt::print(os, "}};\n");

    return os.good() ? write_test_simulation_result::success
                     : write_test_simulation_result::output_error;
}

} // namespace irt
