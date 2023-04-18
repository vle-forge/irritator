// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <utility>

namespace irt {

template<typename Dynamics>
std::pair<Dynamics*, child_id> alloc(
  modeling&              mod,
  generic_component&     parent,
  const std::string_view name  = {},
  child_flags            param = child_flags_none) noexcept
{
    irt_assert(!mod.models.full());
    irt_assert(!mod.children.full());
    irt_assert(!mod.hsms.full());

    auto& child    = mod.alloc(parent, dynamics_typeof<Dynamics>());
    auto  child_id = mod.children.get_id(child);
    child.name     = name;
    child.flags    = param;

    return std::make_pair(&get_dyn<Dynamics>(mod.models.get(child.id.mdl_id)),
                          child_id);
}

template<typename DynamicsSrc, typename DynamicsDst>
status connect(modeling&          mod,
               generic_component& c,
               DynamicsSrc&       src,
               i8                 port_src,
               DynamicsDst&       dst,
               i8                 port_dst) noexcept
{
    model& src_model = get_model(*src.first);
    model& dst_model = get_model(*dst.first);

    irt_return_if_fail(
      is_ports_compatible(src_model, port_src, dst_model, port_dst),
      status::model_connect_bad_dynamics);

    mod.connect(c, src.second, port_src, dst.second, port_dst);

    return status::success;
}

status add_integrator_component_port(modeling&          mod,
                                     generic_component& com,
                                     child_id           id,
                                     i8                 port) noexcept
{
    irt_return_if_bad(mod.connect_input(com, port, id, 1));
    irt_return_if_bad(mod.connect_output(com, id, 0, port));

    return status::success;
}

template<int QssLevel>
status add_lotka_volterra(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "X", child_flags_both);
    integrator_a.first->default_X  = 18.0_r;
    integrator_a.first->default_dQ = 0.1_r;

    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "Y", child_flags_both);
    integrator_b.first->default_X  = 7.0_r;
    integrator_b.first->default_dQ = 0.1_r;

    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);

    auto sum_a = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "X+XY", child_flags_configurable);
    sum_a.first->default_input_coeffs[0] = 2.0_r;
    sum_a.first->default_input_coeffs[1] = -0.4_r;

    auto sum_b = alloc<abstract_wsum<QssLevel, 2>>(
      mod, com, "Y+XY", child_flags_configurable);
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

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_lif(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    bool success = mod.models.can_alloc(5);
    irt_return_if_fail(success, status::simulation_not_enough_model);

    constexpr irt::real tau = 10.0_r;
    constexpr irt::real Vt  = 1.0_r;
    constexpr irt::real V0  = 10.0_r;
    constexpr irt::real Vr  = -V0;

    auto cst                 = alloc<constant>(mod, com);
    cst.first->default_value = 1.0;

    auto cst_cross                 = alloc<constant>(mod, com);
    cst_cross.first->default_value = Vr;

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    sum.first->default_input_coeffs[0] = -irt::one / tau;
    sum.first->default_input_coeffs[1] = V0 / tau;

    auto integrator =
      alloc<abstract_integrator<QssLevel>>(mod, com, "lif", child_flags_both);
    integrator.first->default_X  = 0._r;
    integrator.first->default_dQ = 0.001_r;

    auto cross                     = alloc<abstract_cross<QssLevel>>(mod, com);
    cross.first->default_threshold = Vt;

    connect(mod, com, cross, 0, integrator, 1);
    connect(mod, com, cross, 1, sum, 0);
    connect(mod, com, integrator, 0, cross, 0);
    connect(mod, com, integrator, 0, cross, 2);
    connect(mod, com, cst_cross, 0, cross, 1);
    connect(mod, com, cst, 0, sum, 1);
    connect(mod, com, sum, 0, integrator, 0);

    add_integrator_component_port(mod, com, integrator.second, 0);

    return status::success;
}

template<int QssLevel>
status add_izhikevich(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(12);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto cst     = alloc<constant>(mod, com);
    auto cst2    = alloc<constant>(mod, com);
    auto cst3    = alloc<constant>(mod, com);
    auto sum_a   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_b   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto sum_c   = alloc<abstract_wsum<QssLevel, 4>>(mod, com);
    auto sum_d   = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto product = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "V", child_flags_both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "U", child_flags_both);
    auto cross  = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cross2 = alloc<abstract_cross<QssLevel>>(mod, com);

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

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_van_der_pol(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum      = alloc<abstract_wsum<QssLevel, 3>>(mod, com);
    auto product1 = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto product2 = alloc<abstract_multiplier<QssLevel>>(mod, com);
    auto integrator_a =
      alloc<abstract_integrator<QssLevel>>(mod, com, "X", child_flags_both);
    auto integrator_b =
      alloc<abstract_integrator<QssLevel>>(mod, com, "Y", child_flags_both);

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

    add_integrator_component_port(mod, com, integrator_a.second, 0);
    add_integrator_component_port(mod, com, integrator_b.second, 1);

    return status::success;
}

template<int QssLevel>
status add_negative_lif(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(5);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto sum = alloc<abstract_wsum<QssLevel, 2>>(mod, com);
    auto integrator =
      alloc<abstract_integrator<QssLevel>>(mod, com, "V", child_flags_both);
    auto cross     = alloc<abstract_cross<QssLevel>>(mod, com);
    auto cst       = alloc<constant>(mod, com);
    auto cst_cross = alloc<constant>(mod, com);

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

    add_integrator_component_port(mod, com, integrator.second, 0);

    return status::success;
}

template<int QssLevel>
status add_seirs(modeling& mod, generic_component& com) noexcept
{
    using namespace irt::literals;
    bool success = mod.models.can_alloc(17);

    irt_return_if_fail(success, status::simulation_not_enough_model);

    auto dS =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dS", child_flags_both);
    dS.first->default_X  = 0.999_r;
    dS.first->default_dQ = 0.0001_r;

    auto dE =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dE", child_flags_both);
    dE.first->default_X  = 0.0_r;
    dE.first->default_dQ = 0.0001_r;

    auto dI =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dI", child_flags_both);
    dI.first->default_X  = 0.001_r;
    dI.first->default_dQ = 0.0001_r;

    auto dR =
      alloc<abstract_integrator<QssLevel>>(mod, com, "dR", child_flags_both);
    dR.first->default_X  = 0.0_r;
    dR.first->default_dQ = 0.0001_r;

    auto beta                  = alloc<constant>(mod, com, "beta");
    beta.first->default_value  = 0.5_r;
    auto rho                   = alloc<constant>(mod, com, "rho");
    rho.first->default_value   = 0.00274397_r;
    auto sigma                 = alloc<constant>(mod, com, "sigma");
    sigma.first->default_value = 0.33333_r;
    auto gamma                 = alloc<constant>(mod, com, "gamma");
    gamma.first->default_value = 0.142857_r;

    auto rho_R    = alloc<abstract_multiplier<QssLevel>>(mod, com, "rho R");
    auto beta_S   = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S");
    auto beta_S_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "beta S I");

    auto rho_R_beta_S_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "rho R - beta S I");
    rho_R_beta_S_I.first->default_input_coeffs[0] = 1.0_r;
    rho_R_beta_S_I.first->default_input_coeffs[1] = -1.0_r;
    auto beta_S_I_sigma_E =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "beta S I - sigma E");
    beta_S_I_sigma_E.first->default_input_coeffs[0] = 1.0_r;
    beta_S_I_sigma_E.first->default_input_coeffs[1] = -1.0_r;

    auto sigma_E = alloc<abstract_multiplier<QssLevel>>(mod, com, "sigma E");
    auto gamma_I = alloc<abstract_multiplier<QssLevel>>(mod, com, "gamma I");

    auto sigma_E_gamma_I =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "sigma E - gamma I");
    sigma_E_gamma_I.first->default_input_coeffs[0] = 1.0_r;
    sigma_E_gamma_I.first->default_input_coeffs[1] = -1.0_r;
    auto gamma_I_rho_R =
      alloc<abstract_wsum<QssLevel, 2>>(mod, com, "gamma I - rho R");
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

    add_integrator_component_port(mod, com, dS.second, 0);
    add_integrator_component_port(mod, com, dE.second, 1);
    add_integrator_component_port(mod, com, dI.second, 2);
    add_integrator_component_port(mod, com, dR.second, 3);

    return status::success;
}

component::component() noexcept
{
    static const std::string_view port_names[] = { "0", "1", "2", "3",
                                                   "4", "5", "6", "7" };

    for (sz i = 0; i < std::size(port_names); ++i) {
        x_names[i] = port_names[i];
        y_names[i] = port_names[i];
    }
}

modeling::modeling() noexcept
  : log_entries{ 16 }
{
}

status modeling::init(modeling_initializer& p) noexcept
{
    irt_return_if_bad(descriptions.init(p.description_capacity));
    irt_return_if_bad(parameters.init(p.parameter_capacity));
    irt_return_if_bad(components.init(p.component_capacity));
    irt_return_if_bad(grid_components.init(p.component_capacity));
    irt_return_if_bad(simple_components.init(p.component_capacity));
    irt_return_if_bad(dir_paths.init(p.dir_path_capacity));
    irt_return_if_bad(file_paths.init(p.file_path_capacity));

    irt_return_if_bad(models.init(p.component_capacity * 16));
    irt_return_if_bad(hsms.init(p.component_capacity));
    irt_return_if_bad(children.init(p.component_capacity * 16));
    irt_return_if_bad(connections.init(p.component_capacity * 32));

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
    compo.type     = component_type::none;
    compo.state    = component_status::unread;

    try {
        std::filesystem::path desc_file{ path };
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

            io_cache cache; // TODO move into modeling or parameter
            auto     ret =
              component_load(mod, compo, cache, file_path.string().c_str());

            if (is_success(ret)) {
                read_description = true;
                compo.state      = component_status::unmodified;
            } else {
                compo.state = component_status::unreadable;
                log_warning(mod,
                            log_level::error,
                            ret,
                            "Fail to load component {} ({})",
                            file_path.string(),
                            status_string(ret));
                irt_bad_return(ret);
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

status modeling::fill_internal_components() noexcept
{
    irt_return_if_fail(components.can_alloc(internal_component_count),
                       status::modeling_too_many_file_open);

    for (int i = 0, e = internal_component_count; i < e; ++i) {
        auto& compo          = components.alloc();
        compo.type           = component_type::internal;
        compo.id.internal_id = enum_cast<internal_component>(i);
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

        if (is_bad(load_component(*this, *compo)))
            to_del = compo;
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

bool modeling::exists(const registred_path& reg) noexcept
{
    try {
        std::error_code       ec;
        std::filesystem::path p{ reg.path.sv() };
        return std::filesystem::exists(p, ec);
    } catch (...) {
        return false;
    }
}

bool modeling::create_directories(const registred_path& reg) noexcept
{
    try {
        std::error_code       ec;
        std::filesystem::path p{ reg.path.sv() };
        if (!std::filesystem::exists(p, ec))
            return std::filesystem::create_directories(p, ec);
    } catch (...) {
    }

    return false;
}

bool modeling::exists(const dir_path& dir) noexcept
{
    if (auto* reg = registred_paths.try_to_get(dir.parent); reg) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            if (std::filesystem::exists(p, ec)) {
                p /= dir.path.sv();
                if (!std::filesystem::exists(p, ec))
                    return true;
            }
        } catch (...) {
        }
    }

    return false;
}

bool modeling::create_directories(const dir_path& dir) noexcept
{
    if (auto* reg = registred_paths.try_to_get(dir.parent); reg) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            if (!std::filesystem::exists(p, ec))
                if (!std::filesystem::create_directories(p, ec))
                    return false;

            p /= dir.path.sv();
            if (!std::filesystem::exists(p, ec))
                return std::filesystem::create_directories(p, ec);
        } catch (...) {
        }
    }

    return false;
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

bool modeling::can_alloc_grid_component() const noexcept
{
    return components.can_alloc() && grid_components.can_alloc();
}

bool modeling::can_alloc_simple_component() const noexcept
{
    return components.can_alloc() && simple_components.can_alloc();
}

component& modeling::alloc_grid_component() noexcept
{
    irt_assert(can_alloc_grid_component());

    auto& new_compo    = components.alloc();
    auto  new_compo_id = components.get_id(new_compo);
    format(new_compo.name, "grid {}", get_index(new_compo_id));
    new_compo.type  = component_type::grid;
    new_compo.state = component_status::modified;

    auto& grid           = grid_components.alloc();
    new_compo.id.grid_id = grid_components.get_id(grid);

    return new_compo;
}

component& modeling::alloc_simple_component() noexcept
{
    irt_assert(can_alloc_simple_component());

    auto& new_compo    = components.alloc();
    auto  new_compo_id = components.get_id(new_compo);
    format(new_compo.name, "simple {}", get_index(new_compo_id));
    new_compo.type  = component_type::simple;
    new_compo.state = component_status::modified;

    auto& new_s_compo      = simple_components.alloc();
    new_compo.id.simple_id = simple_components.get_id(new_s_compo);

    return new_compo;
}

static bool fill_child(const modeling&       mod,
                       const child&          c,
                       vector<component_id>& out,
                       component_id          search) noexcept
{
    if (c.type == child_type::component) {
        if (c.id.compo_id == search)
            return true;

        if (auto* compo = mod.components.try_to_get(c.id.compo_id); compo)
            out.emplace_back(c.id.compo_id);
    }

    return false;
}

static bool fill_children(const modeling&       mod,
                          const component&      compo,
                          vector<component_id>& out,
                          component_id          search) noexcept
{
    switch (compo.type) {
    case component_type::grid: {
        auto id = compo.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(id); g) {
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    if (fill_child(
                          mod, g->default_children[row][col], out, search))
                        return true;

            for (const auto& ch : g->specific_children)
                if (fill_child(mod, ch.ch, out, search))
                    return true;
        }
    } break;

    case component_type::simple: {
        auto id = compo.id.simple_id;
        if (auto* s = mod.simple_components.try_to_get(id); s) {
            for (auto id : s->children)
                if (auto* ch = mod.children.try_to_get(id); ch)
                    if (fill_child(mod, *ch, out, search))
                        return true;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

bool modeling::can_add(const component& parent,
                       const component& child) const noexcept
{
    vector<component_id> stack;
    component_id         parent_id = components.get_id(parent);
    component_id         child_id  = components.get_id(child);

    if (parent_id == child_id)
        return false;

    if (fill_children(*this, child, stack, parent_id))
        return true;

    while (!stack.empty()) {
        auto id = stack.back();
        stack.pop_back();

        if (auto* compo = components.try_to_get(id); compo) {
            if (fill_children(*this, *compo, stack, parent_id))
                return true;
        }
    }

    return false;
}

static bool is_ports_compatible(modeling&          mod,
                                model&             mdl_src,
                                i8                 port_src,
                                generic_component& compo_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_dst.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_dst) {
            auto* sub_child_dst = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_dst);
            irt_assert(sub_child_dst->type == child_type::model);

            auto  sub_model_dst_id = sub_child_dst->id.mdl_id;
            auto* sub_model_dst    = mod.models.try_to_get(sub_model_dst_id);

            if (!is_ports_compatible(
                  mdl_src, port_src, *sub_model_dst, con->output.index_src)) {
                is_compatible = false;
                break;
            }
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                i8                 port_src,
                                model&             mdl_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::input &&
            con->input.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->input.dst);
            irt_assert(sub_child_src);
            if (sub_child_src->type == child_type::model) {

                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(*sub_model_src,
                                         con->input.index_dst,
                                         mdl_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                i8                 port_src,
                                generic_component& compo_dst,
                                i8                 port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_src);

            if (sub_child_src->type == child_type::model) {
                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(mod,
                                         *sub_model_src,
                                         con->output.index_src,
                                         compo_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling& mod,
                                generic_component& /*parent*/,
                                child_id src,
                                i8       port_src,
                                child_id dst,
                                i8       port_dst) noexcept
{
    auto* child_src = mod.children.try_to_get(src);
    auto* child_dst = mod.children.try_to_get(dst);
    irt_assert(child_src);
    irt_assert(child_dst);

    if (child_src->type == child_type::model) {
        auto  mdl_src_id = child_src->id.mdl_id;
        auto* mdl_src    = mod.models.try_to_get(mdl_src_id);

        if (child_dst->type == child_type::model) {
            auto  mdl_dst_id = child_dst->id.mdl_id;
            auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
            return is_ports_compatible(*mdl_src, port_src, *mdl_dst, port_dst);

        } else {
            auto  compo_dst_id = child_dst->id.compo_id;
            auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
            irt_assert(compo_dst);
            auto* s_compo_dst =
              mod.simple_components.try_to_get(compo_dst->id.simple_id);
            irt_assert(s_compo_dst);

            return is_ports_compatible(
              mod, *mdl_src, port_src, *s_compo_dst, port_dst);
        }
    } else {
        auto  compo_src_id = child_src->id.compo_id;
        auto* compo_src    = mod.components.try_to_get(compo_src_id);
        irt_assert(compo_src);
        auto* s_compo_src =
          mod.simple_components.try_to_get(compo_src->id.simple_id);
        irt_assert(s_compo_src);

        if (child_dst->type == child_type::model) {
            auto  mdl_dst_id = child_dst->id.mdl_id;
            auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
            irt_assert(mdl_dst);

            return is_ports_compatible(
              mod, *s_compo_src, port_src, *mdl_dst, port_dst);
        } else {
            auto  compo_dst_id = child_dst->id.compo_id;
            auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
            irt_assert(compo_dst);
            auto* s_compo_dst =
              mod.simple_components.try_to_get(compo_dst->id.simple_id);
            irt_assert(s_compo_dst);

            return is_ports_compatible(
              mod, *s_compo_src, port_src, *s_compo_dst, port_dst);
        }
    }
}

status modeling::connect_input(generic_component& parent,
                               i8                 port_src,
                               child_id           dst,
                               i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto* child = children.try_to_get(dst);
    irt_assert(child);

    if (child->type == child_type::model) {
        auto  mdl_id  = child->id.mdl_id;
        auto* mdl_dst = models.try_to_get(mdl_id);
        irt_assert(mdl_dst);

        irt_return_if_fail(
          is_ports_compatible(*this, parent, port_src, *mdl_dst, port_dst),
          status::model_connect_bad_dynamics);
    }

    auto& con           = connections.alloc();
    auto  con_id        = connections.get_id(con);
    con.input.dst       = dst;
    con.input.index     = port_src;
    con.input.index_dst = port_dst;
    con.type            = connection::connection_type::input;

    parent.connections.emplace_back(con_id);

    return status::success;
}

status modeling::connect_output(generic_component& parent,
                                child_id           src,
                                i8                 port_src,
                                i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    auto* child = children.try_to_get(src);
    irt_assert(child);

    if (child->type == child_type::model) {
        auto  mdl_id  = child->id.mdl_id;
        auto* mdl_src = models.try_to_get(mdl_id);
        irt_assert(mdl_src);

        irt_return_if_fail(
          is_ports_compatible(*this, *mdl_src, port_src, parent, port_dst),
          status::model_connect_bad_dynamics);
    }

    auto& con            = connections.alloc();
    auto  con_id         = connections.get_id(con);
    con.output.src       = src;
    con.output.index_src = port_src;
    con.output.index     = port_dst;
    con.type             = connection::connection_type::output;

    parent.connections.emplace_back(con_id);

    return status::success;
}

status modeling::connect(generic_component& parent,
                         child_id           src,
                         i8                 port_src,
                         child_id           dst,
                         i8                 port_dst) noexcept
{
    irt_return_if_fail(connections.can_alloc(1),
                       status::data_array_not_enough_memory);

    irt_return_if_fail(
      is_ports_compatible(*this, parent, src, port_src, dst, port_dst),
      status::model_connect_bad_dynamics);

    auto& con              = connections.alloc();
    auto  con_id           = connections.get_id(con);
    con.internal.src       = src;
    con.internal.dst       = dst;
    con.internal.index_src = port_src;
    con.internal.index_dst = port_dst;
    con.type               = connection::connection_type::internal;

    parent.connections.emplace_back(con_id);

    return status::success;
}

void modeling::clean_simulation() noexcept
{
    grid_component* grid = nullptr;
    while (grid_components.next(grid))
        clear_grid_component_cache(*grid);
}

void modeling::clear(child& c) noexcept
{
    if (c.type == child_type::model)
        if (auto* mdl = models.try_to_get(c.id.mdl_id); mdl)
            models.free(*mdl);

    c.id.mdl_id = undefined<model_id>();
    c.type      = child_type::model;
    c.name.clear();
}

void modeling::free(child& c) noexcept
{
    clear(c);

    children.free(c);
}

void modeling::free(connection& c) noexcept { connections.free(c); }

void modeling::free(component& compo) noexcept
{
    switch (compo.type) {
    case component_type::simple:
        if (auto* s = simple_components.try_to_get(compo.id.simple_id); s) {
            for (auto child_id : s->children)
                if (auto* child = children.try_to_get(child_id); child)
                    free(*child);

            for (auto connection_id : s->connections)
                if (auto* con = connections.try_to_get(connection_id); con)
                    free(*con);

            if (auto* desc = descriptions.try_to_get(compo.desc); desc)
                descriptions.free(*desc);

            if (auto* path = file_paths.try_to_get(compo.file); path)
                file_paths.free(*path);

            simple_components.free(*s);
        }
        break;

    case component_type::grid:
        if (auto* g = grid_components.try_to_get(compo.id.grid_id); g) {
            g->specific_children.clear();

            grid_components.free(*g);
        }
        break;

    default:
        break;
    }

    components.free(compo);
}

child& modeling::alloc(generic_component& parent, component_id id) noexcept
{
    irt_assert(children.can_alloc());

    auto& child       = children.alloc(id);
    auto  child_id    = children.get_id(child);
    child.unique_id   = parent.make_next_unique_id();
    child.type        = child_type::component;
    child.id.compo_id = id;
    parent.children.emplace_back(child_id);

    return child;
}

child& modeling::alloc(generic_component& parent, dynamics_type type) noexcept
{
    irt_assert(models.can_alloc());
    irt_assert(children.can_alloc());

    auto& mdl    = models.alloc();
    auto  mdl_id = models.get_id(mdl);
    mdl.type     = type;
    mdl.handle   = nullptr;

    dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> void {
        new (&dyn) Dynamics{};

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            irt_assert(this->hsms.can_alloc());

            auto& machine = this->hsms.alloc();
            dyn.id        = this->hsms.get_id(machine);
        }
    });

    auto& child     = children.alloc(mdl_id);
    auto  child_id  = children.get_id(child);
    child.unique_id = parent.make_next_unique_id();
    child.type      = child_type::model;
    child.id.mdl_id = mdl_id;
    parent.children.emplace_back(child_id);

    return child;
}

child& modeling::alloc(generic_component& parent, model_id id) noexcept
{
    irt_assert(children.can_alloc());

    auto& child     = children.alloc(id);
    auto  child_id  = children.get_id(child);
    child.unique_id = parent.make_next_unique_id();
    child.type      = child_type::model;
    child.id.mdl_id = id;
    parent.children.emplace_back(child_id);

    return child;
}

constexpr static inline int pos(int row, int col, int rows) noexcept
{
    return col * rows + row;
}

status modeling::build_grid_component_cache(grid_component& grid) noexcept
{
    clear_grid_component_cache(grid);

    irt_return_if_fail(grid.row && grid.column, status::io_project_file_error);
    irt_return_if_fail(children.can_alloc(grid.row * grid.column),
                       status::data_array_not_enough_memory);
    irt_return_if_fail(connections.can_alloc(grid.row * grid.column * 4),
                       status::data_array_not_enough_memory);

    grid.cache.resize(grid.row * grid.column);

    int x = 0, y = 0;

    for (int row = 0; row < grid.row; ++row) {
        if (row == 0)
            y = 0;
        else if (1 <= row && row + 1 < grid.row)
            y = 1;
        else
            y = 2;

        for (int col = 0; col < grid.column; ++col) {
            if (col == 0)
                x = 0;
            else if (1 <= col && col + 1 < grid.column)
                x = 1;
            else
                x = 2;

            if (components.try_to_get(grid.default_children[x][y])) {
                auto& ch    = children.alloc(grid.default_children[x][y]);
                auto  ch_id = children.get_id(ch);
                grid.cache[pos(row, col, grid.row)] = ch_id;
            } else {
                grid.cache[pos(row, col, grid.row)] = undefined<child_id>();
            }
        }
    }

    for (const auto& elem : grid.specific_children) {
        irt_assert(0 <= elem.row && elem.row < grid.row);
        irt_assert(0 <= elem.column && elem.column < grid.column);

        auto ch_id = grid.cache[pos(elem.row, elem.column, grid.row)];
        if (components.try_to_get(elem.ch)) {
            if (auto* ch = children.try_to_get(ch_id); ch) {
                ch->type        = child_type::component;
                ch->id.compo_id = elem.ch;
            } else {
                auto& new_ch    = children.alloc(elem.ch);
                auto  new_ch_id = children.get_id(new_ch);
                grid.cache[pos(elem.row, elem.column, grid.row)] = new_ch_id;
            }
        }
    }

    if (grid.connection_type == grid_component::type::number) {
        for (int row = 0; row < grid.row; ++row) {
            for (int col = 0; col < grid.column; ++col) {
                const auto src_id = grid.cache[pos(row, col, grid.row)];
                if (src_id == undefined<child_id>())
                    continue;

                const auto row_min = row == 0 ? 0 : row - 1;
                const auto row_max = row == grid.row - 1 ? row : row + 1;
                const auto col_min = col == 0 ? 0 : col - 1;
                const auto col_max = col == grid.column - 1 ? col : col + 1;

                for (int i = row_min; i <= row_max; ++i) {
                    for (int j = col_min; j <= col_max; ++j) {
                        if (!(i == row && j == col)) {
                            const auto dst_id = grid.cache[pos(i, j, grid.row)];
                            if (dst_id == undefined<child_id>())
                                continue;

                            auto& c    = connections.alloc();
                            auto  c_id = connections.get_id(c);
                            c.type     = connection::connection_type::internal;
                            c.internal.src       = src_id;
                            c.internal.index_src = 0;
                            c.internal.dst       = dst_id;
                            c.internal.index_dst = 0;
                            grid.cache_connections.emplace_back(c_id);
                        }
                    }
                }
            }
        }
    } else {
        for (int row = 0; row < grid.row; ++row) {
            for (int col = 0; col < grid.column; ++col) {
                const auto src_id = grid.cache[pos(row, col, grid.row)];
                if (src_id == undefined<child_id>())
                    continue;

                const auto row_min = row == 0 ? 0 : row - 1;
                const auto row_max = row == grid.row - 1 ? row : row + 1;
                const auto col_min = col == 0 ? 0 : col - 1;
                const auto col_max = col == grid.column - 1 ? col : col + 1;

                for (int i = row_min; i <= row_max; ++i) {
                    for (int j = col_min; j <= col_max; ++j) {
                        if (!(i == row && j == col)) {
                            const auto dst_id = grid.cache[pos(i, j, grid.row)];
                            if (dst_id == undefined<child_id>())
                                continue;

                            auto& c    = connections.alloc();
                            auto  c_id = connections.get_id(c);
                            auto  port = (col - col_min) * 3 + (row - row_min);

                            c.type = connection::connection_type::internal;
                            c.internal.src       = src_id;
                            c.internal.index_src = port;
                            c.internal.dst       = dst_id;
                            c.internal.index_dst = port;
                            grid.cache_connections.emplace_back(c_id);
                        }
                    }
                }
            }
        }
    }

    const auto use_row_cylinder = match(grid.opts,
                                        grid_component::options::row_cylinder,
                                        grid_component::options::torus);

    if (use_row_cylinder) {
        for (int row = 0; row < grid.row; ++row) {
            const auto src_id = grid.cache[pos(row, 0, grid.row)];
            const auto dst_id = grid.cache[pos(row, grid.column - 1, grid.row)];

            auto& c1 = connections.alloc();
            grid.cache_connections.emplace_back(connections.get_id(c1));
            c1.type               = connection::connection_type::internal;
            c1.internal.src       = src_id;
            c1.internal.index_src = 0;
            c1.internal.dst       = dst_id;
            c1.internal.index_dst = 0;

            auto& c2 = connections.alloc();
            grid.cache_connections.emplace_back(connections.get_id(c2));
            c2.type               = connection::connection_type::internal;
            c2.internal.src       = dst_id;
            c2.internal.index_src = 0;
            c2.internal.dst       = src_id;
            c2.internal.index_dst = 0;
        }
    }

    const auto use_column_cylinder =
      match(grid.opts,
            grid_component::options::column_cylinder,
            grid_component::options::torus);

    if (use_column_cylinder) {
        for (int col = 0; col < grid.column; ++col) {
            const auto src_id = grid.cache[pos(0, col, grid.row)];
            const auto dst_id = grid.cache[pos(grid.row - 1, col, grid.row)];

            auto& c1 = connections.alloc();
            grid.cache_connections.emplace_back(connections.get_id(c1));
            c1.internal.src       = src_id;
            c1.internal.index_src = 0;
            c1.internal.dst       = dst_id;
            c1.internal.index_dst = 0;

            auto& c2 = connections.alloc();
            grid.cache_connections.emplace_back(connections.get_id(c2));
            c2.internal.src       = dst_id;
            c2.internal.index_src = 0;
            c2.internal.dst       = src_id;
            c2.internal.index_dst = 0;
        }
    }

    return status::success;
}

void modeling::clear_grid_component_cache(grid_component& grid) noexcept
{
    for (auto id : grid.cache)
        children.free(id);

    for (auto id : grid.cache_connections)
        connections.free(id);

    grid.cache.clear();
    grid.cache_connections.clear();
}

status modeling::copy(const child& src, child& dst) noexcept
{
    clear(dst);

    dst.name  = src.name;
    dst.x     = src.x;
    dst.y     = src.y;
    dst.flags = src.flags;

    if (src.type == child_type::model) {
        irt_return_if_fail(models.can_alloc(),
                           status::simulation_not_enough_model);

        if (auto* src_mdl = models.try_to_get(src.id.mdl_id); src_mdl) {
            auto& dst_mdl = models.alloc();
            irt::copy(*src_mdl, dst_mdl);
            dst.type      = child_type::model;
            dst.id.mdl_id = models.get_id(dst_mdl);
        }
    } else {
        dst.type        = child_type::component;
        dst.id.compo_id = src.id.compo_id;
    }

    return status::success;
}

status modeling::copy(const generic_component& src,
                      generic_component&       dst) noexcept
{
    table<child_id, child_id> mapping;

    for (auto child_id : src.children) {
        auto* c = children.try_to_get(child_id);
        if (!c)
            continue;

        if (c->type == child_type::model) {
            auto id = c->id.mdl_id;
            if (auto* mdl = models.try_to_get(id); mdl) {
                auto& new_child    = alloc(dst, mdl->type);
                auto  new_child_id = children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(child_id, new_child_id);
            }
        } else {
            auto compo_id = c->id.compo_id;
            if (auto* compo = components.try_to_get(compo_id); compo) {
                auto& new_child    = alloc(dst, compo_id);
                auto  new_child_id = children.get_id(new_child);
                new_child.name     = c->name;
                new_child.x        = c->x;
                new_child.y        = c->y;

                mapping.data.emplace_back(child_id, new_child_id);
            }
        }
    }

    mapping.sort();

    for (auto connection_id : src.connections) {
        auto* con = connections.try_to_get(connection_id);

        if (con->type == connection::connection_type::internal) {
            if (auto* child_src = mapping.get(con->internal.src); child_src) {
                if (auto* child_dst = mapping.get(con->internal.dst);
                    child_dst) {
                    irt_return_if_bad(connect(dst,
                                              *child_src,
                                              con->internal.index_src,
                                              *child_dst,
                                              con->internal.index_dst));
                }
            }
        }
    }

    return status::success;
}

status modeling::copy(grid_component& grid, generic_component& s) noexcept
{
    irt_return_if_bad(build_grid_component_cache(grid));

    const auto children_size = s.children.size();
    s.children.resize(children_size + grid.cache.size());
    std::copy_n(
      grid.cache.data(), grid.cache.size(), s.children.data() + children_size);

    const auto connection_size = s.connections.size();
    s.connections.resize(connection_size + grid.cache_connections.size());
    std::copy_n(grid.cache_connections.data(),
                grid.cache_connections.size(),
                s.connections.data() + connection_size);

    grid.cache.clear();
    grid.cache_connections.clear();

    return status::success;
}

status modeling::copy(internal_component src, component& dst) noexcept
{
    irt_return_if_fail(simple_components.can_alloc(),
                       status::data_array_not_enough_memory);

    auto& s_compo    = simple_components.alloc();
    auto  s_compo_id = simple_components.get_id(s_compo);
    dst.type         = component_type::simple;
    dst.id.simple_id = s_compo_id;

    switch (src) {
    case internal_component::qss1_izhikevich:
        return add_izhikevich<1>(*this, s_compo);
    case internal_component::qss1_lif:
        return add_lif<1>(*this, s_compo);
    case internal_component::qss1_lotka_volterra:
        return add_lotka_volterra<1>(*this, s_compo);
    case internal_component::qss1_negative_lif:
        return add_negative_lif<1>(*this, s_compo);
    case internal_component::qss1_seirs:
        return add_seirs<1>(*this, s_compo);
    case internal_component::qss1_van_der_pol:
        return add_van_der_pol<1>(*this, s_compo);
    case internal_component::qss2_izhikevich:
        return add_izhikevich<2>(*this, s_compo);
    case internal_component::qss2_lif:
        return add_lif<2>(*this, s_compo);
    case internal_component::qss2_lotka_volterra:
        return add_lotka_volterra<2>(*this, s_compo);
    case internal_component::qss2_negative_lif:
        return add_negative_lif<2>(*this, s_compo);
    case internal_component::qss2_seirs:
        return add_seirs<2>(*this, s_compo);
    case internal_component::qss2_van_der_pol:
        return add_van_der_pol<2>(*this, s_compo);
    case internal_component::qss3_izhikevich:
        return add_izhikevich<3>(*this, s_compo);
    case internal_component::qss3_lif:
        return add_lif<3>(*this, s_compo);
    case internal_component::qss3_lotka_volterra:
        return add_lotka_volterra<3>(*this, s_compo);
    case internal_component::qss3_negative_lif:
        return add_negative_lif<3>(*this, s_compo);
    case internal_component::qss3_seirs:
        return add_seirs<3>(*this, s_compo);
    case internal_component::qss3_van_der_pol:
        return add_van_der_pol<3>(*this, s_compo);
    }

    irt_unreachable();
}

status modeling::copy(const component& src, component& dst) noexcept
{
    dst.x_names = src.x_names;
    dst.y_names = src.y_names;
    dst.name    = src.name;
    dst.id      = src.id;
    dst.type    = src.type;
    dst.state   = src.state;

    switch (src.type) {
    case component_type::none:
        break;

    case component_type::simple:
        if (const auto* s_src = simple_components.try_to_get(src.id.simple_id);
            s_src) {
            irt_return_if_fail(simple_components.can_alloc(),
                               status::data_array_not_enough_memory);

            auto& s_dst      = simple_components.alloc();
            auto  s_dst_id   = simple_components.get_id(s_dst);
            dst.id.simple_id = s_dst_id;
            dst.type         = component_type::simple;

            irt_return_if_bad(copy(*s_src, s_dst));
        }
        break;

    case component_type::grid:
        if (const auto* s = grid_components.try_to_get(src.id.grid_id); s) {
            irt_return_if_fail(grid_components.can_alloc(),
                               status::data_array_not_enough_memory);

            auto& d        = grid_components.alloc();
            auto  d_id     = grid_components.get_id(d);
            dst.id.grid_id = d_id;
            dst.type       = component_type::grid;
            d              = *s;
        }

        break;

    case component_type::internal:
        break;
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

status modeling::save(component& c) noexcept
{
    auto* reg = registred_paths.try_to_get(c.reg_path);
    irt_return_if_fail(reg, status::modeling_directory_access_error);

    auto* dir = dir_paths.try_to_get(c.dir);
    irt_return_if_fail(dir, status::modeling_directory_access_error);

    auto* file = file_paths.try_to_get(c.file);
    irt_return_if_fail(file, status::modeling_file_access_error);

    {
        std::filesystem::path p{ reg->path.sv() };
        p /= dir->path.sv();
        p /= file->path.sv();
        p.replace_extension(".irt");

        std::error_code ec;
        io_cache        cache;

        irt_return_if_bad(component_save(*this, c, cache, p.string().c_str()));
    }

    if (auto* desc = descriptions.try_to_get(c.desc); desc) {
        std::filesystem::path p{ dir->path.c_str() };
        p /= file->path.c_str();
        p.replace_extension(".desc");
        std::ofstream ofs{ p };
        ofs.write(desc->data.c_str(), desc->data.size());
    }

    c.state = component_status::unmodified;
    c.type  = component_type::simple;

    return status::success;
}

} // namespace irt
