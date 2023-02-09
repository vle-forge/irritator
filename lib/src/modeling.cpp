// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>
#include <optional>

namespace irt {

enum class alloc_parameter
{
    none         = 0,
    configurable = 1 << 1,
    observable   = 1 << 2,
    both         = configurable | observable
};

template<typename Dynamics>
std::pair<Dynamics*, child_id> alloc(
  component&             parent,
  const std::string_view name  = {},
  alloc_parameter        param = alloc_parameter::none) noexcept
{
    irt_assert(!parent.models.full());

    auto& mdl  = parent.models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = nullptr;

    new (&mdl.dyn) Dynamics{};
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (has_input_port<Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = static_cast<u64>(-1);

    if constexpr (has_output_port<Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = static_cast<u64>(-1);

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
        irt_assert(parent.hsms.can_alloc());
        auto& machine = parent.hsms.alloc();
        auto  id      = parent.hsms.get_id(machine);
        dyn.id        = id;
    }

    auto& child      = parent.children.alloc(parent.models.get_id(mdl));
    child.name       = name;
    child.observable = ordinal(param) & ordinal(alloc_parameter::observable);
    child.configurable =
      ordinal(param) & ordinal(alloc_parameter::configurable);

    return std::make_pair(&dyn, parent.children.get_id(child));
}

template<typename DynamicsSrc, typename DynamicsDst>
status connect(modeling&    mod,
               component&   c,
               DynamicsSrc& src,
               i8           port_src,
               DynamicsDst& dst,
               i8           port_dst) noexcept
{
    model& src_model = get_model(*src.first);
    model& dst_model = get_model(*dst.first);

    irt_return_if_fail(
      is_ports_compatible(src_model, port_src, dst_model, port_dst),
      status::model_connect_bad_dynamics);

    irt_return_if_fail(c.connections.can_alloc(1),
                       status::simulation_not_enough_connection);

    mod.connect(c, src.second, port_src, dst.second, port_dst);

    return status::success;
}

status add_integrator_component_port(component& com, child_id id) noexcept
{
    auto* child = com.children.try_to_get(id);
    irt_assert(child);
    irt_assert(child->type == child_type::model);

    irt_return_if_bad(com.connect_input(0, id, 1));
    irt_return_if_bad(com.connect_output(id, 0, 0));

    return status::success;
}

template<int QssLevel>
status add_lotka_volterra(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = com.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(com, "X", alloc_parameter::both);
    integrator_a.first->default_X  = 18.0_r;
    integrator_a.first->default_dQ = 0.1_r;

    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(com, "Y", alloc_parameter::both);
    integrator_b.first->default_X  = 7.0_r;
    integrator_b.first->default_dQ = 0.1_r;

    auto product = alloc<abstract_multiplier<QssLevel>>(com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      com, "X+XY", alloc_parameter::configurable);
    sum_a.first->default_input_coeffs[0] = 2.0_r;
    sum_a.first->default_input_coeffs[1] = -0.4_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      com, "Y+XY", alloc_parameter::configurable);
    sum_b.first->default_input_coeffs[0] = -1.0_r;
    sum_b.first->default_input_coeffs[1] = 0.1_r;

    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);
    connect(mod, com, integrator_a, 0, sum_a, 0);
    connect(mod, com, integrator_b, 0, sum_b, 0);
    connect(mod, com, integrator_a, 0, product, 0);
    connect(mod, com, integrator_b, 0, product, 1);
    connect(mod, com, product, 0, sum_a, 1);
    connect(mod, com, product, 0, sum_b, 1);

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = com.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst                 = alloc<constant>(com);
    cst.first->default_value = 1.0;

    auto cst_cross                 = alloc<constant>(com);
    cst_cross.first->default_value = Vr;

    auto sum                           = alloc<abstract_wsum<QssLevel, 2>>(com);
    sum.first->default_input_coeffs[0] = -irt::one / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    auto integrator =
      alloc<abstract_integrator<QssLevel>>(com, "lif", alloc_parameter::both);
    integrator.first->default_X  = 0._r;
    integrator.first->default_dQ = 0.001_r;

    auto cross                     = alloc<abstract_cross<QssLevel>>(com);
    cross.first->default_threshold = Vt;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(com, integrator.second);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto cst     = alloc<constant>(com);
    auto cst2    = alloc<constant>(com);
    auto cst3    = alloc<constant>(com);
    auto sum_a   = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto sum_b   = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto sum_c   = alloc<abstract_wsum<QssLevel, 4>>(com);
    auto sum_d   = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto product = alloc<abstract_multiplier<QssLevel>>(com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(com, "V", alloc_parameter::both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(com, "U", alloc_parameter::both);
    auto cross  = alloc<abstract_cross<QssLevel>>(com);
    auto cross2 = alloc<abstract_cross<QssLevel>>(com);

    constexpr irt::real a  = 0.2_r;
    constexpr irt::real b  = 2.0_r;
    constexpr irt::real c  = -56.0_r;
    constexpr irt::real d  = -16.0_r;
    constexpr irt::real I  = -99.0_r;
    constexpr irt::real vt = 30.0_r;

    cst.first->default_value  = 1.0_r;
    cst2.first->default_value = c;
    cst3.first->default_value = I;

    cross.first->default_threshold  = vt;
    cross2.first->default_threshold = vt;

    integrator_a.first->default_X  = 0.0_r;
    integrator_a.first->default_dQ = 0.01_r;

    integrator_b.first->default_X  = 0.0_r;
    integrator_b.first->default_dQ = 0.01_r;

    sum_a.first->default_input_coeffs[0] = 1.0_r;
    sum_a.first->default_input_coeffs[1] = -1.0_r;
    sum_b.first->default_input_coeffs[0] = -a;
    sum_b.first->default_input_coeffs[1] = a * b;
    sum_c.first->default_input_coeffs[0] = 0.04_r;
    sum_c.first->default_input_coeffs[1] = 5.0_r;
    sum_c.first->default_input_coeffs[2] = 140.0_r;
    sum_c.first->default_input_coeffs[3] = 1.0_r;
    sum_d.first->default_input_coeffs[0] = 1.0_r;
    sum_d.first->default_input_coeffs[1] = d;

    connect(mod, com, integrator_a, 0, cross, 0);
    connect(mod, com, cst2, 0, cross, 1);
    connect(mod, com, integrator_a, 0, cross, 2);

    connect(mod, com, cross, 1, product, 0);
    connect(mod, com, cross, 1, product, 1);
    connect(mod, com, product, 0, sum_c, 0);
    connect(mod, com, cross, 1, sum_c, 1);
    connect(mod, com, cross, 1, sum_b, 1);

    connect(mod, com, cst, 0, sum_c, 2);
    connect(mod, com, cst3, 0, sum_c, 3);

    connect(mod, com, sum_c, 0, sum_a, 0);
    connect(mod, com, cross2, 1, sum_a, 1);
    connect(mod, com, sum_a, 0, integrator_a, 0);
    connect(mod, com, cross, 0, integrator_a, 1);

    connect(mod, com, cross2, 1, sum_b, 0);
    connect(mod, com, sum_b, 0, integrator_b, 0);

    connect(mod, com, cross2, 0, integrator_b, 1);
    connect(mod, com, integrator_a, 0, cross2, 0);
    connect(mod, com, integrator_b, 0, cross2, 2);
    connect(mod, com, sum_d, 0, cross2, 1);
    connect(mod, com, integrator_b, 0, sum_d, 0);
    connect(mod, com, cst, 0, sum_d, 1);

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum      = alloc<abstract_wsum<QssLevel, 3>>(com);
    auto product1 = alloc<abstract_multiplier<QssLevel>>(com);
    auto product2 = alloc<abstract_multiplier<QssLevel>>(com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(com, "X", alloc_parameter::both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(com, "Y", alloc_parameter::both);

    integrator_a.first->default_X  = 0.0_r;
    integrator_a.first->default_dQ = 0.001_r;

    integrator_b.first->default_X  = 10.0_r;
    integrator_b.first->default_dQ = 0.001_r;

    constexpr double mu                = 4.0_r;
    sum.first->default_input_coeffs[0] = mu;
    sum.first->default_input_coeffs[1] = -mu;
    sum.first->default_input_coeffs[2] = -1.0_r;

    connect(mod, com, integrator_b, 0, integrator_a, 0);
    connect(mod, com, sum, 0, integrator_b, 0);
    connect(mod, com, integrator_b, 0, sum, 0);
    connect(mod, com, product2, 0, sum, 1);
    connect(mod, com, integrator_a, 0, sum, 2);
    connect(mod, com, integrator_b, 0, product1, 0);
    connect(mod, com, integrator_a, 0, product1, 1);
    connect(mod, com, product1, 0, product2, 0);
    connect(mod, com, integrator_a, 0, product2, 1);

    add_integrator_component_port(com, integrator_a.second);
    add_integrator_component_port(com, integrator_b.second);

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(com);
    auto integrator =
      alloc<abstract_integrator<QssLevel>>(com, "V", alloc_parameter::both);
    auto cross     = alloc<abstract_cross<QssLevel>>(com);
    auto cst       = alloc<constant>(com);
    auto cst_cross = alloc<constant>(com);

    constexpr real tau = 10.0_r;
    constexpr real Vt  = -1.0_r;
    constexpr real V0  = -10.0_r;
    constexpr real Vr  = 0.0_r;

    sum.first->default_input_coeffs[0] = -1.0_r / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    cst.first->default_value       = 1.0_r;
    cst_cross.first->default_value = Vr;

    integrator.first->default_X  = 0.0_r;
    integrator.first->default_dQ = 0.001_r;

    cross.first->default_threshold = Vt;
    cross.first->default_detect_up = false;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(com, integrator.second);

    return status::success;
}

template<int QssLevel>
status add_seirs(modeling& mod, component& com) noexcept
{
    using namespace irt::literals;
    bool success = com.models.can_alloc(17);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto dS =
      alloc<abstract_integrator<QssLevel>>(com, "dS", alloc_parameter::both);
    dS.first->default_X  = 0.999_r;
    dS.first->default_dQ = 0.0001_r;

    auto dE =
      alloc<abstract_integrator<QssLevel>>(com, "dE", alloc_parameter::both);
    dE.first->default_X  = 0.0_r;
    dE.first->default_dQ = 0.0001_r;

    auto dI =
      alloc<abstract_integrator<QssLevel>>(com, "dI", alloc_parameter::both);
    dI.first->default_X  = 0.001_r;
    dI.first->default_dQ = 0.0001_r;

    auto dR =
      alloc<abstract_integrator<QssLevel>>(com, "dR", alloc_parameter::both);
    dR.first->default_X  = 0.0_r;
    dR.first->default_dQ = 0.0001_r;

    auto beta                  = alloc<constant>(com, "beta");
    beta.first->default_value  = 0.5_r;
    auto rho                   = alloc<constant>(com, "rho");
    rho.first->default_value   = 0.00274397_r;
    auto sigma                 = alloc<constant>(com, "sigma");
    sigma.first->default_value = 0.33333_r;
    auto gamma                 = alloc<constant>(com, "gamma");
    gamma.first->default_value = 0.142857_r;

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(com, "rho R - beta S I");
    rho_R_beta_S_I.first->default_input_coeffs[0] = 1.0_r;
    rho_R_beta_S_I.first->default_input_coeffs[1] = -1.0_r;
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(com, "beta S I - sigma E");
    beta_S_I_sigma_E.first->default_input_coeffs[0] = 1.0_r;
    beta_S_I_sigma_E.first->default_input_coeffs[1] = -1.0_r;

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(com, "sigma E - gamma I");
    sigma_E_gamma_I.first->default_input_coeffs[0] = 1.0_r;
    sigma_E_gamma_I.first->default_input_coeffs[1] = -1.0_r;
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(com, "gamma I - rho R");
    gamma_I_rho_R.first->default_input_coeffs[0] = -1.0_r;
    gamma_I_rho_R.first->default_input_coeffs[1] = 1.0_r;

    connect(mod, com, rho, 0, rho_R, 0);
    connect(mod, com, beta, 0, rho_R, 1);
    connect(mod, com, beta, 0, beta_S, 1);
    connect(mod, com, dS, 0, beta_S, 0);
    connect(mod, com, dI, 0, beta_S_I, 0);
    connect(mod, com, beta_S, 0, beta_S_I, 1);
    connect(mod, com, rho_R, 0, rho_R_beta_S_I, 0);
    connect(mod, com, beta_S_I, 0, rho_R_beta_S_I, 1);
    connect(mod, com, rho_R_beta_S_I, 0, dS, 0);
    connect(mod, com, dE, 0, sigma_E, 0);
    connect(mod, com, sigma, 0, sigma_E, 1);
    connect(mod, com, beta_S_I, 0, beta_S_I_sigma_E, 0);
    connect(mod, com, sigma_E, 0, beta_S_I_sigma_E, 1);
    connect(mod, com, beta_S_I_sigma_E, 0, dE, 0);
    connect(mod, com, dI, 0, gamma_I, 0);
    connect(mod, com, gamma, 0, gamma_I, 1);
    connect(mod, com, sigma_E, 0, sigma_E_gamma_I, 0);
    connect(mod, com, gamma_I, 0, sigma_E_gamma_I, 1);
    connect(mod, com, sigma_E_gamma_I, 0, dI, 0);
    connect(mod, com, rho_R, 0, gamma_I_rho_R, 0);
    connect(mod, com, gamma_I, 0, gamma_I_rho_R, 1);
    connect(mod, com, gamma_I_rho_R, 0, dR, 0);

    add_integrator_component_port(com, dS.second);
    add_integrator_component_port(com, dE.second);
    add_integrator_component_port(com, dI.second);
    add_integrator_component_port(com, dR.second);

    return status::success;
}

auto get_component_type(std::string_view name) noexcept
  -> std::optional<component_type>
{
    struct entry
    {
        std::string_view name;
        component_type   type;
    };

    static const entry entries[] = {
        { "file", component_type::file },
        { "memory", component_type::memory },
        { "qss1_izhikevich", component_type::qss1_izhikevich },
        { "qss1_lif", component_type::qss1_lif },
        { "qss1_lotka_volterra", component_type::qss1_lotka_volterra },
        { "qss1_negative_lif", component_type::qss1_negative_lif },
        { "qss1_seirs", component_type::qss1_seirs },
        { "qss1_van_der_pol", component_type::qss1_van_der_pol },
        { "qss2_izhikevich", component_type::qss2_izhikevich },
        { "qss2_lif", component_type::qss2_lif },
        { "qss2_lotka_volterra", component_type::qss2_lotka_volterra },
        { "qss2_negative_lif", component_type::qss2_negative_lif },
        { "qss2_seirs", component_type::qss2_seirs },
        { "qss2_van_der_pol", component_type::qss2_van_der_pol },
        { "qss3_izhikevich", component_type::qss3_izhikevich },
        { "qss3_lif", component_type::qss3_lif },
        { "qss3_lotka_volterra", component_type::qss3_lotka_volterra },
        { "qss3_negative_lif", component_type::qss3_negative_lif },
        { "qss3_seirs", component_type::qss3_seirs },
        { "qss3_van_der_pol", component_type::qss3_van_der_pol },
    };

    auto it = binary_find(
      std::begin(entries),
      std::end(entries),
      name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == std::end(entries) ? std::nullopt
                                   : std::make_optional(it->type);
}

static bool make_path(std::string_view sv) noexcept
{
    bool ret = false;

    try {
        std::error_code       ec;
        std::filesystem::path p(sv);
        std::filesystem::create_directories(p, ec);
        ret = true;
    } catch (...) {
    }

    return ret;
}

static bool exists_path(std::string_view sv) noexcept
{
    bool ret = false;

    try {
        std::error_code       ec;
        std::filesystem::path p(sv);
        std::filesystem::create_directories(p, ec);
        ret = true;
    } catch (...) {
    }

    return ret;
}

status component::connect(child_id src,
                          i8       port_src,
                          child_id dst,
                          i8       port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto& con              = connections.alloc();
    con.internal.src       = src;
    con.internal.dst       = dst;
    con.internal.index_src = port_src;
    con.internal.index_dst = port_dst;
    con.type               = connection::connection_type::internal;

    return status::success;
}

status component::connect_input(i8 port_src, child_id dst, i8 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto& con           = connections.alloc();
    con.input.dst       = dst;
    con.input.index     = port_src;
    con.input.index_dst = port_dst;
    con.type            = connection::connection_type::input;

    return status::success;
}

status component::connect_output(child_id src,
                                 i8       port_src,
                                 i8       port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto& con            = connections.alloc();
    con.output.src       = src;
    con.output.index_src = port_src;
    con.output.index     = port_dst;
    con.type             = connection::connection_type::output;

    return status::success;
}

bool registred_path::make() const noexcept { return make_path(path.sv()); }
bool registred_path::exists() const noexcept { return exists_path(path.sv()); }
bool dir_path::make() const noexcept { return exists_path(path.sv()); }
bool dir_path::exists() const noexcept { return exists_path(path.sv()); }

modeling::modeling() noexcept
  : log_entries(16)
{
}

status modeling::init(modeling_initializer& p) noexcept
{
    irt_return_if_bad(tree_nodes.init(p.tree_capacity));
    irt_return_if_bad(descriptions.init(p.description_capacity));
    irt_return_if_bad(parameters.init(p.parameter_capacity));
    irt_return_if_bad(components.init(p.component_capacity));
    irt_return_if_bad(dir_paths.init(p.dir_path_capacity));
    irt_return_if_bad(file_paths.init(p.file_path_capacity));
    irt_return_if_bad(srcs.constant_sources.init(p.constant_source_capacity));
    irt_return_if_bad(srcs.text_file_sources.init(p.text_file_source_capacity));
    irt_return_if_bad(srcs.random_sources.init(p.random_source_capacity));
    irt_return_if_bad(
      srcs.binary_file_sources.init(p.binary_file_source_capacity));

    return status::success;
}

component_id modeling::search_internal_component(component_type type) noexcept
{
    component* compo = nullptr;
    while (components.next(compo))
        if (compo->type == type)
            return components.get_id(compo);

    return undefined<component_id>();
}

component_id modeling::search_component(const char* directory_name,
                                        const char* file_name) noexcept
{
    for (auto reg_dir_id : component_repertories) {
        auto& reg_dir = registred_paths.get(reg_dir_id);

        for (auto dir_id : reg_dir.children) {
            auto& dir = dir_paths.get(dir_id);

            if (dir.path == directory_name) {
                for (auto file_id : dir.children) {
                    auto& file = file_paths.get(file_id);

                    if (file.path == file_name) {
                        if (components.try_to_get(file.component) != nullptr)
                            return file.component;
                    }
                }
            }
        }
    }

    log_warning(*this,
                log_level::notice,
                status::modeling_file_access_error,
                "Missing file {} in directory {}",
                file_name,
                directory_name);

    return undefined<component_id>();
}

status modeling::fill_internal_components() noexcept
{
    irt_return_if_fail(components.can_alloc(21), status::success);

    {
        auto& c = components.alloc();
        c.name  = "QSS1 lotka volterra";
        c.type  = component_type::qss1_lotka_volterra;
        c.state = component_status::read_only;
        if (auto ret = add_lotka_volterra<1>(*this, c); is_bad(ret))
            return ret;
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 lotka volterra";
        c.type  = component_type::qss2_lotka_volterra;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 lotka volterra";
        c.type  = component_type::qss2_lotka_volterra;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lotka_volterra<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 lif";
        c.type  = component_type::qss1_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 lif";
        c.type  = component_type::qss2_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 lif";
        c.type  = component_type::qss3_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_lif<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 izhikevich";
        c.type  = component_type::qss1_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 izhikevich";
        c.type  = component_type::qss2_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 izhikevich";
        c.type  = component_type::qss3_izhikevich;
        c.state = component_status::read_only;
        irt_return_if_bad(add_izhikevich<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 van der pol";
        c.type  = component_type::qss1_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 van der pol";
        c.type  = component_type::qss2_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 van der pol";
        c.type  = component_type::qss3_van_der_pol;
        c.state = component_status::read_only;
        irt_return_if_bad(add_van_der_pol<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 negative lif";
        c.type  = component_type::qss1_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 negative lif";
        c.type  = component_type::qss2_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 negative lif";
        c.type  = component_type::qss3_lif;
        c.state = component_status::read_only;
        irt_return_if_bad(add_negative_lif<3>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS1 seirs";
        c.type  = component_type::qss1_seirs;
        c.state = component_status::read_only;
        irt_return_if_bad(add_seirs<1>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS2 seirs";
        c.type  = component_type::qss2_seirs;
        c.state = component_status::read_only;
        irt_return_if_bad(add_seirs<2>(*this, c));
    }

    {
        auto& c = components.alloc();
        c.name  = "QSS3 seirs";
        c.type  = component_type::qss3_seirs;
        c.state = component_status::read_only;
        irt_return_if_bad(add_seirs<3>(*this, c));
    }

    return status::success;
}

static void prepare_component_loading(modeling&             mod,
                                      registred_path&       reg_dir,
                                      dir_path&             dir,
                                      file_path&            file,
                                      std::filesystem::path path) noexcept
{
    namespace fs = std::filesystem;

    auto& compo    = mod.components.alloc();
    compo.reg_path = mod.registred_paths.get_id(reg_dir);
    compo.dir      = mod.dir_paths.get_id(dir);
    compo.file     = mod.file_paths.get_id(file);
    file.component = mod.components.get_id(compo);
    compo.type     = component_type::file;
    compo.state    = component_status::unread;

    try {
        auto desc_file = path;
        desc_file.replace_extension(".desc");

        std::error_code ec;
        if (fs::exists(desc_file, ec)) {
            if (mod.descriptions.can_alloc()) {
                auto& desc  = mod.descriptions.alloc();
                desc.status = description_status::unread;
                compo.desc  = mod.descriptions.get_id(desc);
            } else {
                log_warning(mod,
                            log_level::error,
                            status::modeling_too_many_description_open);
            }
        }
    } catch (const std::exception& /*e*/) {
        log_warning(mod,
                    log_level::error,
                    status::io_filesystem_error,
                    reg_dir.path.c_str());
    }
}

static void prepare_component_loading(modeling&             mod,
                                      registred_path&       reg_dir,
                                      dir_path&             dir,
                                      std::filesystem::path path) noexcept
{
    namespace fs = std::filesystem;

    try {
        std::error_code ec;
        auto            it            = fs::directory_iterator{ path, ec };
        auto            et            = fs::directory_iterator{};
        bool            too_many_file = false;

        while (it != et) {
            if (it->is_regular_file() && it->path().extension() == ".irt") {
                if (mod.file_paths.can_alloc() && mod.components.can_alloc()) {
                    auto  u8str = it->path().filename().u8string();
                    auto* cstr  = reinterpret_cast<const char*>(u8str.c_str());
                    auto& file  = mod.file_paths.alloc();
                    auto  file_id = mod.file_paths.get_id(file);
                    file.path     = cstr;
                    file.parent   = mod.dir_paths.get_id(dir);

                    dir.children.emplace_back(file_id);

                    prepare_component_loading(
                      mod, reg_dir, dir, file, it->path());
                } else {
                    too_many_file = true;
                    break;
                }
            }

            it = it.increment(ec);
        }

        if (too_many_file) {
            log_warning(mod,
                        log_level::error,
                        status::modeling_too_many_file_open,
                        "registred path {}, directory {}",
                        reg_dir.path.sv(),
                        dir.path.sv());
        }
    } catch (...) {
        log_warning(mod,
                    log_level::error,
                    status::modeling_file_access_error,
                    "registred path {}",
                    reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling&              mod,
                                      registred_path&        reg_dir,
                                      std::filesystem::path& path) noexcept
{
    namespace fs = std::filesystem;

    try {
        std::error_code ec;

        if (fs::is_directory(path, ec)) {
            auto it                 = fs::directory_iterator{ path, ec };
            auto et                 = fs::directory_iterator{};
            bool too_many_directory = false;

            while (it != et) {
                if (it->is_directory()) {
                    if (mod.dir_paths.can_alloc()) {
                        auto u8str = it->path().filename().u8string();
                        auto cstr =
                          reinterpret_cast<const char*>(u8str.c_str());
                        auto& dir    = mod.dir_paths.alloc();
                        auto  dir_id = mod.dir_paths.get_id(dir);
                        dir.path     = cstr;
                        dir.status   = dir_path::state::unread;
                        dir.parent   = mod.registred_paths.get_id(reg_dir);

                        reg_dir.children.emplace_back(dir_id);
                        prepare_component_loading(
                          mod, reg_dir, dir, it->path());
                    } else {
                        too_many_directory = true;
                        break;
                    }
                }

                it = it.increment(ec);
            }

            if (too_many_directory)
                log_warning(mod,
                            log_level::error,
                            status::modeling_too_many_directory_open,
                            "registred path {}",
                            reg_dir.path.sv());
        } else {
            log_warning(mod,
                        log_level::error,
                        status::modeling_file_access_error,
                        "registred path {}",
                        reg_dir.path.sv());
        }
    } catch (...) {
        log_warning(mod,
                    log_level::error,
                    status::modeling_file_access_error,
                    "registred path {}",
                    reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling&       mod,
                                      registred_path& reg_dir) noexcept
{
    namespace fs = std::filesystem;

    try {
        fs::path        p(reg_dir.path.c_str());
        std::error_code ec;

        if (std::filesystem::exists(p, ec)) {
            prepare_component_loading(mod, reg_dir, p);
            reg_dir.status = registred_path::state::read;
        } else {
            reg_dir.status = registred_path::state::error;

            log_warning(mod,
                        log_level::debug,
                        status::modeling_file_access_error,
                        "registred path does not exist {} ",
                        reg_dir.path.sv());
        }
    } catch (...) {
        log_warning(mod,
                    log_level::debug,
                    status::modeling_file_access_error,
                    "registred path: {} ",
                    reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling& mod) noexcept
{
    for (auto reg_dir_id : mod.component_repertories) {
        auto& reg_dir = mod.registred_paths.get(reg_dir_id);

        prepare_component_loading(mod, reg_dir);
    }
}

static status load_component(modeling& mod, component& compo) noexcept
{
    try {
        auto* reg  = mod.registred_paths.try_to_get(compo.reg_path);
        auto* dir  = mod.dir_paths.try_to_get(compo.dir);
        auto* file = mod.file_paths.try_to_get(compo.file);

        if (reg && dir && file) {
            std::filesystem::path file_path = reg->path.u8sv();
            file_path /= dir->path.u8sv();
            file_path /= file->path.u8sv();

            bool read_description = false;

            if (mod.components.can_alloc()) {
                json_cache cache; // TODO move into modeling or parameter
                auto       ret =
                  component_load(mod, compo, cache, file_path.string().c_str());

                if (is_success(ret)) {
                    read_description = true;
                    compo.state      = component_status::unmodified;
                } else {
                    log_warning(mod,
                                log_level::error,
                                ret,
                                "Fail to load component {} ({})",
                                file_path.string(),
                                status_string(ret));
                    mod.components.free(compo);
                    irt_bad_return(ret);
                }
            }

            if (read_description) {
                std::filesystem::path desc_file(file_path);
                desc_file.replace_extension(".desc");

                if (std::ifstream ifs{ desc_file }; ifs) {
                    auto* desc = mod.descriptions.try_to_get(compo.desc);
                    if (!desc) {
                        auto& d    = mod.descriptions.alloc();
                        desc       = &d;
                        compo.desc = mod.descriptions.get_id(d);
                    }

                    if (!ifs.read(desc->data.begin(), desc->data.capacity())) {
                        mod.descriptions.free(*desc);
                        compo.desc = undefined<description_id>();
                    }
                }
            }
        }
    } catch (const std::bad_alloc& /*e*/) {
        return status::io_not_enough_memory;
    } catch (...) {
        return status::io_filesystem_error;
    }

    return status::success;
}

status modeling::fill_components() noexcept
{
    prepare_component_loading(*this);

    component* compo  = nullptr;
    component* to_del = nullptr;

    while (components.next(compo)) {
        if (to_del) {
            free(*to_del);
            to_del = nullptr;
        }

        if (compo->type == component_type::file &&
            compo->state == component_status::unread) {
            if (is_bad(load_component(*this, *compo)))
                to_del = compo;
        }
    }

    if (to_del)
        free(*to_del);

    return status::success;
}

status modeling::fill_components(registred_path& path) noexcept
{
    for (auto dir_id : path.children)
        if (auto* dir = dir_paths.try_to_get(dir_id); dir)
            free(*dir);

    path.children.clear();

    try {
        std::filesystem::path p(path.path.c_str());
        std::error_code       ec;

        if (std::filesystem::exists(p, ec) &&
            std::filesystem::is_directory(p, ec)) {
            prepare_component_loading(*this, path, p);
        }
    } catch (...) {
    }

    return status::success;
}

bool modeling::can_alloc_file(i32 number) const noexcept
{
    return file_paths.can_alloc(number);
}

bool modeling::can_alloc_dir(i32 number) const noexcept
{
    return dir_paths.can_alloc(number);
}

bool modeling::can_alloc_registred(i32 number) const noexcept
{
    return registred_paths.can_alloc(number);
}

static bool exist_file(const dir_path& dir, const file_path_id id) noexcept
{
    return std::find(dir.children.begin(), dir.children.end(), id) !=
           dir.children.end();
}

file_path& modeling::alloc_file(dir_path& dir) noexcept
{
    auto& file     = file_paths.alloc();
    auto  id       = file_paths.get_id(file);
    file.component = undefined<component_id>();
    file.parent    = dir_paths.get_id(dir);

    dir.children.emplace_back(id);

    return file;
}

dir_path& modeling::alloc_dir(registred_path& reg) noexcept
{
    auto& dir  = dir_paths.alloc();
    auto  id   = dir_paths.get_id(dir);
    dir.parent = registred_paths.get_id(reg);
    dir.status = dir_path::state::unread;

    reg.children.emplace_back(id);

    return dir;
}

registred_path& modeling::alloc_registred() noexcept
{
    return registred_paths.alloc();
}

void modeling::remove_file(registred_path& reg,
                           dir_path&       dir,
                           file_path&      file) noexcept
{
    try {
        std::filesystem::path p{ reg.path.sv() };
        p /= dir.path.sv();
        p /= file.path.sv();

        std::error_code ec;
        std::filesystem::remove(p, ec);
    } catch (...) {
    }
}

void modeling::move_file(registred_path& /*reg*/,
                         dir_path&  from,
                         dir_path&  to,
                         file_path& file) noexcept
{
    auto id = file_paths.get_id(file);

    if (auto it = std::find(from.children.begin(), from.children.end(), id);
        it != from.children.end())
        from.children.erase(it);

    irt_assert(!exist_file(to, id));

    file.parent = dir_paths.get_id(to);
    to.children.emplace_back(id);
}

status modeling::connect(component& parent,
                         child_id   src,
                         i8         port_src,
                         child_id   dst,
                         i8         port_dst) noexcept
{
    irt_return_if_fail(parent.connections.can_alloc(1),
                       status::data_array_not_enough_memory);

    auto& con              = parent.connections.alloc();
    con.internal.src       = src;
    con.internal.dst       = dst;
    con.internal.index_src = port_src;
    con.internal.index_dst = port_dst;
    con.type               = connection::connection_type::internal;

    return status::success;
}

static void free_child(data_array<child, child_id>& children,
                       data_array<model, model_id>& models,
                       child&                       c) noexcept
{
    if (c.type == child_type::model) {
        auto id = enum_cast<model_id>(c.id);
        if (auto* mdl = models.try_to_get(id); mdl)
            models.free(*mdl);
    }

    children.free(c);
}

void modeling::free(component& c) noexcept
{
    c.children.clear();
    c.models.clear();
    c.connections.clear();

    if (auto* desc = descriptions.try_to_get(c.desc); desc)
        descriptions.free(*desc);

    if (auto* path = file_paths.try_to_get(c.file); path)
        file_paths.free(*path);

    components.free(c);
}

void modeling::free(tree_node& node) noexcept
{
    // for (i32 i = 0, e = node.parameters.data.ssize(); i != e; ++i) {
    //     auto mdl_id = node.parameters.data[i].value;
    //     if (auto* mdl = models.try_to_get(mdl_id); mdl)
    //         models.free(*mdl);
    // }

    tree_nodes.free(node);
}

void modeling::free(component& parent, child& c) noexcept
{
    free_child(parent.children, parent.models, c);
}

void modeling::free(component& parent, connection& c) noexcept
{
    parent.connections.free(c);
}

status modeling::copy(component& src, component& dst) noexcept
{
    table<child_id, child_id> mapping;

    child* c = nullptr;
    while (src.children.next(c)) {
        const auto c_id = src.children.get_id(*c);

        if (c->type == child_type::model) {
            auto id = enum_cast<model_id>(c->id);
            if (auto* mdl = src.models.try_to_get(id); mdl) {
                irt_return_if_fail(dst.models.can_alloc(1),
                                   status::data_array_not_enough_memory);

                auto& new_child    = alloc(dst, mdl->type);
                auto  new_child_id = dst.children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(c_id, new_child_id);
            }
        } else {
            auto compo_id = enum_cast<component_id>(c->id);
            if (auto* compo = components.try_to_get(compo_id); compo) {
                auto& new_child    = dst.children.alloc(compo_id);
                auto  new_child_id = dst.children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(c_id, new_child_id);
            }
        }
    }

    mapping.sort();

    connection* con = nullptr;
    while (src.connections.next(con)) {
        if (con->type == connection::connection_type::internal) {
            if (auto* child_src = mapping.get(con->internal.src); child_src) {
                if (auto* child_dst = mapping.get(con->internal.dst);
                    child_dst) {
                    irt_return_if_bad(dst.connect(*child_src,
                                                  con->internal.index_src,
                                                  *child_dst,
                                                  con->internal.index_dst));
                }
            }
        }
    }

    return status::success;
}

static status make_tree_recursive(
  data_array<component, component_id>& components,
  data_array<tree_node, tree_node_id>& trees,
  tree_node&                           parent,
  child_id                             compo_child_id,
  child&                               compo_child) noexcept
{
    auto compo_id = enum_cast<component_id>(compo_child.id);

    if (auto* compo = components.try_to_get(compo_id); compo) {
        irt_return_if_fail(trees.can_alloc(),
                           status::data_array_not_enough_memory);

        auto& new_tree = trees.alloc(compo_id, compo_child_id);
        new_tree.tree.set_id(&new_tree);
        new_tree.tree.parent_to(parent.tree);

        child* c = nullptr;
        while (compo->children.next(c)) {
            if (c->type == child_type::component) {
                irt_return_if_bad(make_tree_recursive(
                  components, trees, new_tree, compo->children.get_id(*c), *c));
            }
        }
    }

    return status::success;
}

void modeling::free(file_path& file) noexcept
{
    if (auto* compo = components.try_to_get(file.component); compo)
        free(*compo);
}

void modeling::free(dir_path& dir) noexcept
{
    for (auto file_id : dir.children)
        if (auto* file = file_paths.try_to_get(file_id); file)
            free(*file);

    dir_paths.free(dir);
}

void modeling::free(registred_path& reg_dir) noexcept
{
    for (auto dir_id : reg_dir.children)
        if (auto* dir = dir_paths.try_to_get(dir_id); dir)
            free(*dir);

    registred_paths.free(reg_dir);
}

void modeling::init_project(component& compo) noexcept
{
    clear_project();

    tree_node_id tn = undefined<tree_node_id>();
    if (is_success(make_tree_from(compo, &tn)))
        head = tn;
}

status modeling::make_tree_from(component& parent, tree_node_id* out) noexcept
{
    irt_return_if_fail(tree_nodes.can_alloc(),
                       status::data_array_not_enough_memory);

    auto  compo_id    = components.get_id(parent);
    auto& tree_parent = tree_nodes.alloc(compo_id, undefined<child_id>());

    tree_parent.tree.set_id(&tree_parent);

    child* c = nullptr;
    while (parent.children.next(c)) {
        if (c->type == child_type::component) {
            irt_return_if_bad(make_tree_recursive(components,
                                                  tree_nodes,
                                                  tree_parent,
                                                  parent.children.get_id(c),
                                                  *c));
        }
    }

    *out = tree_nodes.get_id(tree_parent);

    return status::success;
}

void component::clear() noexcept
{
    models.clear();
    hsms.clear();
    children.clear();
    connections.clear();

    child_mapping_io.data.clear();

    desc     = description_id{ 0 };
    reg_path = registred_path_id{ 0 };
    dir      = dir_path_id{ 0 };
    file     = file_path_id{ 0 };

    name.clear();

    type  = component_type::memory;
    state = component_status::modified;
}

status modeling::save(component& c) noexcept
{
    auto* dir = dir_paths.try_to_get(c.dir);
    irt_return_if_fail(dir, status::modeling_directory_access_error);

    auto* file = file_paths.try_to_get(c.file);
    irt_return_if_fail(file, status::modeling_file_access_error);

    {
        std::filesystem::path p{ dir->path.sv() };
        std::error_code       ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return status::io_filesystem_make_directory_error;
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return status::io_filesystem_not_directory_error;
            }
        }

        p /= file->path.c_str();
        p.replace_extension(".irt");

        json_cache cache;
        irt_return_if_bad(component_save(*this, c, cache, p.string().c_str()));

        return status::success;
    }

    if (auto* desc = descriptions.try_to_get(c.desc); desc) {
        std::filesystem::path p{ dir->path.c_str() };
        p /= file->path.c_str();
        p.replace_extension(".desc");
        std::ofstream ofs{ p };
        ofs.write(desc->data.c_str(), desc->data.size());
    }

    c.state = component_status::unmodified;
    c.type  = component_type::file;

    return status::success;
}

void modeling::clear_project() noexcept
{
    if (auto* tree = tree_nodes.try_to_get(head); tree)
        free(*tree);

    head = undefined<tree_node_id>();
    tree_nodes.clear();
}

static status simulation_copy_model(simulation&           sim,
                                    tree_node&            tree,
                                    simulation_tree_node& sim_tree,
                                    component&            src)
{
    sim_tree.children.clear();

    child* to_del = nullptr;
    child* c      = nullptr;
    while (src.children.next(c)) {
        if (to_del)
            src.children.free(*to_del);

        to_del = c;

        if (c->type == child_type::model) {
            auto  mdl_id = enum_cast<model_id>(c->id);
            auto* mdl    = src.models.try_to_get(mdl_id);

            if (!mdl)
                continue;

            irt_return_if_fail(sim.models.can_alloc(),
                               status::simulation_not_enough_model);

            auto& new_mdl    = sim.clone(*mdl);
            auto  new_mdl_id = sim.models.get_id(new_mdl);

            sim_tree.children.emplace_back(new_mdl_id);
            tree.sim.data.emplace_back(mdl_id, new_mdl_id);
        }

        to_del = nullptr;
    }

    if (to_del)
        src.children.free(*to_del);

    return status::success;
}

void modeling_to_simulation::clear() noexcept
{
    stack.clear();
    inputs.clear();
    outputs.clear();
    sim_tree_nodes.clear();

    head = undefined<simulation_tree_node_id>();
}

void modeling_to_simulation::destroy() noexcept
{
    // @TODO homogeneize destroy functions in all irritator container.

    // stack.destroy();
    // inputs.destroy();
    // outputs.destroy();
    // sim_tree_nodes.destroy();

    clear();
    head = undefined<simulation_tree_node_id>();
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
            auto* sim_tree =
              cache.sim_tree_nodes.try_to_get(cur->sim_tree_node);
            irt_assert(sim_tree);

            irt_return_if_bad(
              simulation_copy_model(sim, *cur, *sim_tree, *compo));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    tree_node* tree = nullptr;
    while (mod.tree_nodes.next(tree))
        tree->sim.sort();

    return status::success;
}

static auto get_treenode(tree_node& parent, child_id to_search) noexcept
  -> tree_node*
{
    auto* children = parent.tree.get_child();
    while (children) {
        if (children->id_in_parent == to_search)
            break;

        children = children->tree.get_sibling();
    }

    return children;
}

static auto get_component(modeling& mod, child& c) noexcept -> component*
{
    auto compo_id = enum_cast<component_id>(c.id);

    return mod.components.try_to_get(compo_id);
}

static auto get_simulation_model(tree_node&  parent,
                                 simulation& sim,
                                 child&      ch) noexcept -> model*
{
    auto mod_model_id = enum_cast<model_id>(ch.id);
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

static auto input_connect(model_to_component_connect& ic,
                          component&                  compo,
                          tree_node&                  tree,
                          i8 port_dst) noexcept -> status
{
    connection* con    = nullptr;
    connection* to_del = nullptr;

    while (compo.connections.next(con)) {
        if (to_del)
            compo.connections.free(*to_del);

        to_del = con;

        if (!(con->type == connection::connection_type::input &&
              port_dst == con->input.index))
            continue;

        auto* child = compo.children.try_to_get(con->input.dst);
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

        to_del = nullptr;
    }

    if (to_del)
        compo.connections.free(*to_del);

    return status::success;
}

static auto output_connect(model_to_component_connect& ic,
                           component&                  compo,
                           tree_node&                  tree,
                           i8 port_dst) noexcept -> status
{
    connection* con    = nullptr;
    connection* to_del = nullptr;

    while (compo.connections.next(con)) {
        if (to_del)
            compo.connections.free(*to_del);

        to_del = con;

        if (!(con->type == connection::connection_type::output &&
              port_dst == con->output.index))
            continue;

        auto* child = compo.children.try_to_get(con->output.src);
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

        to_del = nullptr;
    }

    if (to_del)
        compo.connections.free(*to_del);

    return status::success;
}

static auto get_input_model_from_component(modeling_to_simulation& cache,
                                           simulation&             sim,
                                           component&              compo,
                                           tree_node&              tree,
                                           i8 port) noexcept -> status
{
    cache.inputs.clear();
    connection* con    = nullptr;
    connection* to_del = nullptr;

    while (compo.connections.next(con)) {
        if (to_del)
            compo.connections.free(*to_del);

        to_del = con;

        if (con->type == connection::connection_type::input &&
            con->input.index == port) {
            auto  child_id = con->input.dst;
            auto* child    = compo.children.try_to_get(child_id);
            if (!child)
                continue;

            irt_assert(child->type == child_type::model);

            auto  mod_model_id = enum_cast<model_id>(child->id);
            auto* sim_model_id = tree.sim.get(mod_model_id);
            irt_assert(sim_model_id);

            auto* sim_model = sim.models.try_to_get(*sim_model_id);
            irt_assert(sim_model);

            cache.inputs.emplace_back(
              std::make_pair(*sim_model_id, con->input.index_dst));
        }

        to_del = nullptr;
    }

    return status::success;
}

static auto get_output_model_from_component(modeling_to_simulation& cache,
                                            simulation&             sim,
                                            component&              compo,
                                            tree_node&              tree,
                                            i8 port) noexcept -> status
{
    cache.outputs.clear();

    connection* con    = nullptr;
    connection* to_del = nullptr;

    while (compo.connections.next(con)) {
        if (to_del)
            compo.connections.free(*to_del);

        to_del = con;

        if (con->type == connection::connection_type::output &&
            con->output.index == port) {
            auto  child_id = con->output.src;
            auto* child    = compo.children.try_to_get(child_id);
            if (!child)
                continue;

            irt_assert(child->type == child_type::model);

            auto  mod_model_id = enum_cast<model_id>(child->id);
            auto* sim_model_id = tree.sim.get(mod_model_id);
            irt_assert(sim_model_id);

            auto* sim_model = sim.models.try_to_get(*sim_model_id);
            irt_assert(sim_model);

            cache.outputs.emplace_back(
              std::make_pair(*sim_model_id, con->output.index_src));
        }

        to_del = nullptr;
    }

    return status::success;
}

static status simulation_copy_connections(modeling_to_simulation& cache,
                                          modeling&               mod,
                                          simulation&             sim,
                                          tree_node&              tree,
                                          component&              compo)
{
    connection* to_del = nullptr;
    connection* con    = nullptr;

    while (compo.connections.next(con)) {
        if (to_del)
            compo.connections.free(*to_del);

        to_del = con;

        if (con->type == connection::connection_type::internal) {
            child* src = compo.children.try_to_get(con->internal.src);
            child* dst = compo.children.try_to_get(con->internal.dst);

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

                    model_to_component_connect ic{ .mod    = mod,
                                                   .sim    = sim,
                                                   .mdl_id = sim.get_id(*m_src),
                                                   .port =
                                                     con->internal.index_src };

                    irt_return_if_bad(input_connect(
                      ic, *c_dst, *t_dst, con->internal.index_dst));
                }
            } else {
                auto* c_src = get_component(mod, *src);
                auto* t_src = get_treenode(tree, con->internal.src);

                if (!(c_src && t_src))
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
                      oc, *c_src, *t_src, con->internal.index_src));
                } else {
                    auto* c_dst = get_component(mod, *dst);
                    auto* t_dst = get_treenode(tree, con->internal.dst);
                    auto* c_src = get_component(mod, *src);
                    auto* t_src = get_treenode(tree, con->internal.src);

                    if (!(c_dst && t_dst && c_src && t_src))
                        continue;

                    irt_return_if_bad(get_input_model_from_component(
                      cache, sim, *c_dst, *t_dst, con->internal.index_dst));
                    irt_return_if_bad(get_output_model_from_component(
                      cache, sim, *c_src, *t_src, con->internal.index_src));

                    for (auto& out : cache.outputs) {
                        for (auto& in : cache.inputs) {
                            irt_return_if_bad(sim.connect(
                              out.first, out.second, in.first, in.second));
                        }
                    }
                }
            }
        }

        to_del = nullptr;
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

        if (compo) {
            irt_return_if_bad(
              simulation_copy_connections(cache, mod, sim, *cur, *compo));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    return status::success;
}

static auto compute_simulation_copy_models_number(modeling_to_simulation& cache,
                                                  const modeling&         mod,
                                                  tree_node& head) noexcept
  -> std::pair<int, int>
{
    int models      = 0;
    int connections = 0;

    cache.stack.clear();
    cache.stack.emplace_back(&head);

    while (!cache.stack.empty()) {
        auto cur = cache.stack.back();
        cache.stack.pop_back();

        auto  compo_id = cur->id;
        auto* compo    = mod.components.try_to_get(compo_id);

        if (compo) {
            models += compo->children.ssize();
            connections += compo->connections.ssize();
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            cache.stack.emplace_back(child);
    }

    return std::make_pair(models, connections);
}

static status simulation_copy_tree(modeling_to_simulation& cache,
                                   modeling&               mod,
                                   tree_node&              head) noexcept
{
    tree_node* tree = nullptr;
    while (mod.tree_nodes.next(tree)) {
        auto& sim_tree      = cache.sim_tree_nodes.alloc();
        tree->sim_tree_node = cache.sim_tree_nodes.get_id(sim_tree);
        tree->sim.data.clear();
    }

    tree = nullptr;
    while (mod.tree_nodes.next(tree)) {
        auto* sim_tree = cache.sim_tree_nodes.try_to_get(tree->sim_tree_node);
        irt_assert(sim_tree);

        if (auto* parent = tree->tree.get_parent(); parent) {
            const auto parent_sim_tree_id = parent->sim_tree_node;
            auto*      parent_sim_tree =
              cache.sim_tree_nodes.try_to_get(parent_sim_tree_id);
            irt_assert(parent_sim_tree);

            sim_tree->tree.parent_to(parent_sim_tree->tree);
        }
    }

    cache.head = head.sim_tree_node;

    return status::success;
}

bool modeling::can_export_to(modeling_to_simulation& cache,
                             const simulation&       sim) const noexcept
{
    if (tree_node* top = tree_nodes.try_to_get(head); top) {
        auto numbers =
          compute_simulation_copy_models_number(cache, *this, *top);
        return numbers.first <= sim.models.capacity() &&
               numbers.second <= sim.message_alloc.capacity();
    }

    return true;
}

status modeling::export_to(modeling_to_simulation& cache,
                           simulation&             sim) noexcept
{
    cache.clear();
    cache.sim_tree_nodes.init(tree_nodes.capacity());

    sim.clear();

    tree_node* top = tree_nodes.try_to_get(head);
    irt_return_if_fail(top, status::simulation_not_enough_connection);

    irt_return_if_bad(simulation_copy_tree(cache, *this, *top));
    irt_return_if_bad(simulation_copy_models(cache, *this, sim, *top));
    irt_return_if_bad(simulation_copy_connections(cache, *this, sim, *top));

    return status::success;
}

} // namespace irt
