// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

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

    if constexpr (is_detected_v<has_input_port_t, Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = static_cast<u64>(-1);

    if constexpr (is_detected_v<has_output_port_t, Dynamics>)
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

    child->in  = true;
    child->out = true;

    com.x.emplace_back(id, i8(1));
    com.y.emplace_back(id, i8(0));

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

#if 0
static bool get_component_type(const char*     type_string,
                               component_type* type_found) noexcept
{
    struct cpp_component_entry
    {
        const char*    name;
        component_type type;
    };

    static const cpp_component_entry tab[] = {
        { "qss1_izhikevich", component_type::qss1_izhikevich },
        { "qss1_lif", component_type::qss1_lif },
        { "qss1_lotka_volterra", component_type::qss1_lotka_volterra },
        { "qss1_negative_lif", component_type::qss1_negative_lif },
        { "qss1_seir_linear", component_type::qss1_seir_linear },
        { "qss1_seir_nonlinear", component_type::qss1_seir_nonlinear },
        { "qss1_van_der_pol", component_type::qss1_van_der_pol },
        { "qss2_izhikevich", component_type::qss2_izhikevich },
        { "qss2_lif", component_type::qss2_lif },
        { "qss2_lotka_volterra", component_type::qss2_lotka_volterra },
        { "qss2_negative_lif", component_type::qss2_negative_lif },
        { "qss2_seir_linear", component_type::qss2_seir_linear },
        { "qss2_seir_nonlinear", component_type::qss2_seir_nonlinear },
        { "qss2_van_der_pol", component_type::qss2_van_der_pol },
        { "qss3_izhikevich", component_type::qss3_izhikevich },
        { "qss3_lif", component_type::qss3_lif },
        { "qss3_lotka_volterra", component_type::qss3_lotka_volterra },
        { "qss3_negative_lif", component_type::qss3_negative_lif },
        { "qss3_seir_linear", component_type::qss3_seir_linear },
        { "qss3_seir_nonlinear", component_type::qss3_seir_nonlinear },
        { "qss3_van_der_pol", component_type::qss3_van_der_pol },
        { "file", component_type::file },
        { "memory", component_type::memory }
    };

    auto it = std::lower_bound(std::begin(tab),
                               std::end(tab),
                               type_string,
                               [](const auto& entry, const char* buffer) {
                                   return 0 == std::strcmp(entry.name, buffer);
                               });

    if (it == std::end(tab) || std::strcmp(it->name, type_string))
        return false;

    *type_found = it->type;

    return true;
}

static component* find_file_component(modeling&  mod,
                                      dir_path&  dir,
                                      file_path& file) noexcept
{
    auto dir_id  = mod.dir_paths.get_id(dir);
    auto file_id = mod.file_paths.get_id(file);

    component* compo = nullptr;
    while (mod.components.next(compo)) {
        if (compo->dir == dir_id && compo->file == file_id)
            return compo;
    }

    return nullptr;
}

static component* load_component(modeling&   mod,
                                 dir_path&   dir,
                                 const char* file) noexcept
{
    component* ret = nullptr;

    try {
        auto* u8str = reinterpret_cast<const char8_t*>(dir.path.c_str());
        std::filesystem::path irt_file(u8str);
        irt_file /= reinterpret_cast<const char8_t*>(file);

        std::error_code ec;
        if (std::filesystem::is_regular_file(irt_file, ec)) {
            if (std::ifstream ifs(irt_file); ifs) {
                auto&  compo = mod.components.alloc();
                reader r(ifs);
                auto   st = r(mod, compo, mod.srcs);
                if (is_bad(st)) {
                    if (!mod.report.empty()) {
                        auto u8str  = irt_file.u8string();
                        auto u8cstr = u8str.c_str();
                        auto cstr   = reinterpret_cast<const char*>(u8cstr);
                        mod.report(3, "Fail to read file", cstr);
                    }
                } else {
                    ret = &compo;
                }
            }
        }
    } catch (const std::exception& /*e*/) {
    }

    return ret;
}

static component* find_cpp_component(modeling&   mod,
                                     const char* type_str) noexcept
{
    component_type type;
    if (get_component_type(type_str, &type)) {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            if (compo->type == type)
                return compo;
        }
    }

    irt_unreachable();
}

static bool is_valid(const modeling_initializer& params) noexcept
{
    return params.model_capacity > 0 && params.tree_capacity > 0 &&
           params.parameter_capacity > 0 && params.description_capacity > 0 &&
           params.component_capacity > 0 && params.dir_path_capacity > 0 &&
           params.file_path_capacity > 0 && params.children_capacity > 0 &&
           params.connection_capacity > 0 &&
           params.constant_source_capacity > 0 &&
           params.binary_file_source_capacity > 0 &&
       params.text_file_source_capacity > 0 &&
           params.random_source_capacity > 0;
}
#endif

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

bool registred_path::make() const noexcept { return make_path(path.sv()); }
bool registred_path::exists() const noexcept { return exists_path(path.sv()); }
bool dir_path::make() const noexcept { return exists_path(path.sv()); }
bool dir_path::exists() const noexcept { return exists_path(path.sv()); }

modeling::modeling() noexcept {}

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
                mod.log(4,
                        status::modeling_too_many_description_open,
                        desc_file.generic_string());
            }
        }
    } catch (const std::exception& /*e*/) {
        mod.log(4, status::io_filesystem_error, reg_dir.path.c_str());
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
            mod.log(4, status::modeling_too_many_file_open, reg_dir.path.sv());
        }
    } catch (...) {
        mod.log(4, status::modeling_file_access_error, reg_dir.path.sv());
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
                        dir.status   = dir_path::status_option::unread;
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
                mod.log(4,
                        status::modeling_too_many_directory_open,
                        reg_dir.path.sv());
        } else {
            mod.log(4,
                    status::modeling_registred_path_access_error,
                    reg_dir.path.sv());
        }
    } catch (...) {
        mod.log(4, status::modeling_file_access_error, reg_dir.path.sv());
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
            reg_dir.status = registred_path::status_option::read;
        }
    } catch (...) {
        mod.log(4, status::modeling_file_access_error, reg_dir.path.sv());
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
                auto&      compo = mod.components.alloc();
                json_cache cache; // TODO move into modeling or parameter
                auto       ret =
                  component_load(mod, compo, cache, file_path.string().c_str());

                if (is_success(ret)) {
                    read_description = true;
                    compo.state      = component_status::unmodified;
                } else {
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
    dir.status = dir_path::status_option::unread;

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

    parent.connections.alloc(src, port_src, dst, port_dst);

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
        if (auto* child_src = mapping.get(con->src); child_src) {
            if (auto* child_dst = mapping.get(con->dst); child_dst) {
                irt_return_if_fail(dst.connections.can_alloc(1),
                                   status::data_array_not_enough_memory);

                dst.connections.alloc(
                  *child_src, con->index_src, *child_dst, con->index_dst);
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

status modeling::clean(component& compo) noexcept
{
    child* c              = nullptr;
    child* to_del_c       = nullptr;
    bool   need_more_loop = false;

    do {
        while (compo.children.next(c)) {
            if (to_del_c) {
                compo.children.free(*to_del_c);
                to_del_c = nullptr;
            }

            if (c->type == child_type::model) {
                const auto id = enum_cast<model_id>(c->id);

                if (auto* model = compo.models.try_to_get(id); !model) {
                    to_del_c       = c;
                    need_more_loop = true;
                }
            } else {
                const auto id = enum_cast<component_id>(c->id);
                if (auto* compo = components.try_to_get(id); !compo) {
                    to_del_c       = c;
                    need_more_loop = true;
                }
            }
        }

        if (to_del_c)
            compo.children.free(*to_del_c);

        connection* con        = nullptr;
        connection* to_del_con = nullptr;

        while (compo.connections.next(con)) {
            if (to_del_con) {
                compo.connections.free(*to_del_con);
                to_del_con = nullptr;
            }

            auto* src = compo.children.try_to_get(con->src);
            auto* dst = compo.children.try_to_get(con->dst);

            if (!src || !dst) {
                to_del_con = con;
                continue;
            }

            if (src->type == child_type::model) {
                auto id = enum_cast<model_id>(src->id);
                if (auto* model = compo.models.try_to_get(id); !model) {
                    compo.children.free(*src);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_id>(src->id);
                if (auto* c = components.try_to_get(id); !c) {
                    compo.children.free(*src);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            }

            if (dst->type == child_type::model) {
                auto id = enum_cast<model_id>(dst->id);
                if (auto* model = compo.models.try_to_get(id); !model) {
                    compo.children.free(*dst);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            } else {
                auto id = enum_cast<component_id>(dst->id);
                if (auto* c = components.try_to_get(id); !c) {
                    compo.children.free(*dst);
                    to_del_con     = con;
                    need_more_loop = true;
                    continue;
                }
            }
        }

        if (to_del_con)
            compo.connections.free(*to_del_con);

    } while (need_more_loop);

    return status::success;
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

void modeling::log(int level, std::string_view message) noexcept
{
    if (log_cb)
        log_cb(level, message, log_user_data);
}

void modeling::log(int level, status s, std::string_view message) noexcept
{
    if (log_cb) {
        small_string<256> buffer;
        format(buffer, "{}: {}", status_string(s), message);
        log(level, buffer.sv());
    }
}

void modeling::register_log_callback(log_callback cb, void* user_data) noexcept
{
    small_string<256> buffer;

    format(buffer, "unregistring modeling log callback: {}", (void*)log_cb);
    log(1, buffer.sv());

    log_cb        = cb;
    log_user_data = user_data;
    format(buffer, "registring modeling log callback: {}", (void*)log_cb);
    log(1, buffer.sv());
}

} // namespace irt
