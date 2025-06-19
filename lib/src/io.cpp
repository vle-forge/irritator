// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/io.hpp>

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
        { "graph", component_type::graph },
        { "grid", component_type::grid },
        { "hsm", component_type::hsm },
        { "internal", component_type::internal },
        { "none", component_type::none },
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
        { "qss1_cross", dynamics_type::qss1_cross },
        { "qss1_filter", dynamics_type::qss1_filter },
        { "qss1_integrator", dynamics_type::qss1_integrator },
        { "qss1_multiplier", dynamics_type::qss1_multiplier },
        { "qss1_power", dynamics_type::qss1_power },
        { "qss1_square", dynamics_type::qss1_square },
        { "qss1_sum_2", dynamics_type::qss1_sum_2 },
        { "qss1_sum_3", dynamics_type::qss1_sum_3 },
        { "qss1_sum_4", dynamics_type::qss1_sum_4 },
        { "qss1_wsum_2", dynamics_type::qss1_wsum_2 },
        { "qss1_wsum_3", dynamics_type::qss1_wsum_3 },
        { "qss1_wsum_4", dynamics_type::qss1_wsum_4 },
        { "qss2_cross", dynamics_type::qss2_cross },
        { "qss2_filter", dynamics_type::qss2_filter },
        { "qss2_integrator", dynamics_type::qss2_integrator },
        { "qss2_multiplier", dynamics_type::qss2_multiplier },
        { "qss2_power", dynamics_type::qss2_power },
        { "qss2_square", dynamics_type::qss2_square },
        { "qss2_sum_2", dynamics_type::qss2_sum_2 },
        { "qss2_sum_3", dynamics_type::qss2_sum_3 },
        { "qss2_sum_4", dynamics_type::qss2_sum_4 },
        { "qss2_wsum_2", dynamics_type::qss2_wsum_2 },
        { "qss2_wsum_3", dynamics_type::qss2_wsum_3 },
        { "qss2_wsum_4", dynamics_type::qss2_wsum_4 },
        { "qss3_cross", dynamics_type::qss3_cross },
        { "qss3_filter", dynamics_type::qss3_filter },
        { "qss3_integrator", dynamics_type::qss3_integrator },
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

}
