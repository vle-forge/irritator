// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>
#include <random>

#include <boost/ut.hpp>

using namespace std::literals;
using namespace boost::ut;

class temp_path_with_unlink
{
public:
    temp_path_with_unlink() noexcept
    {
        std::error_code ec;
        try {
            p = std::filesystem::temp_directory_path(ec);
            expect(fatal(not ec));
            p /= generate_dir_name("rd-"sv);
            try_do_remove(p);
            expect(fatal(std::filesystem::create_directories(p, ec)));
            b = p.string();
        } catch (...) {
            p.clear();
            b.clear();
        }
    }

    ~temp_path_with_unlink() noexcept { do_remove(p); }

    const char* c_str() const noexcept { return b.c_str(); }

    bool success() const noexcept
    {
        if (p.empty())
            return false;

        std::error_code ec;
        return std::filesystem::exists(p, ec) &&
               std::filesystem::is_directory(p, ec);
    }

private:
    static std::string generate_dir_name(const std::string_view prefix) noexcept
    {
        static constexpr std::size_t           generated_chars = 8;
        static std::random_device              rd;
        static std::minstd_rand                gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        std::string                            name;

        name.reserve(prefix.size() + generated_chars);
        name.assign(prefix);

        for (int i = 0; i < 8; ++i)
            name += "0123456789abcdef"[dis(gen)];

        return name;
    }

    static void try_do_remove(const std::filesystem::path& p) noexcept
    {
        try {
            std::error_code ec;
            (void)std::filesystem::remove_all(p, ec);
        } catch (const std::exception& e) {
            fmt::println(stderr,
                         "Warning: Exception while removing {}: {}",
                         p.string(),
                         e.what());
        } catch (...) {
            fmt::println(
              stderr, "Warning: Unknown error while removing {}", p.string());
        }
    }

    static void do_remove(const std::filesystem::path& p) noexcept
    {
        try {
            std::error_code ec;
            std::uintmax_t  nb = std::filesystem::remove_all(p, ec);
            if (nb == 0 or nb == static_cast<std::uintmax_t>(-1)) {
                fmt::println(stderr,
                             "Warning: Failed to remove {}: {}",
                             p.string(),
                             ec.message());
            }
        } catch (const std::exception& e) {
            fmt::println(stderr,
                         "Warning: Exception while removing {}: {}",
                         p.string(),
                         e.what());
        } catch (...) {
            fmt::println(
              stderr, "Warning: Unknown error while removing {}", p.string());
        }
    }

    std::filesystem::path p;
    std::string           b;
};

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    "simulation_wrapper_lambda_injection"_test = [] {
        const auto temp_path = temp_path_with_unlink{};
        expect(fatal(temp_path.success()));

        irt::journal_handler jn;
        irt::modeling        mod;

        irt::registred_path_id reg_id{ 0 };
        irt::dir_path_id       dir_id{ 0 };
        irt::file_path_id      gen_component_file_id{ 0 };
        irt::file_path_id      project_file_id{ 0 };
        irt::file_path_id      simulation_component_file_id{ 0 };
        irt::file_path_id      simulation_wrapper_file_id{ 0 };

        mod.files.write([&](auto& fs) {
            reg_id                              = fs.alloc_registred("temp", 0);
            fs.registred_paths.get(reg_id).path = temp_path.c_str();

            dir_id                          = fs.alloc_dir(reg_id);
            fs.dir_paths.get(dir_id).parent = reg_id;
            fs.dir_paths.get(dir_id).path   = "test-lambda-injection";

            fs.create_directories(reg_id);
            fs.create_directories(dir_id);

            expect(fatal(fs.file_paths.can_alloc(3)));

            gen_component_file_id = fs.alloc_file(
              dir_id, "gen-compo.irt", irt::file_type::component_file);
            project_file_id = fs.alloc_file(
              dir_id, "project.pirt", irt::file_type::project_file);
        });

        // ---- LEVEL 1: gen_compo -- SAME as before, mu fixed to 2.0 only ----
        const auto gen_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& arrivals   = gen.alloc(irt::dynamics_type::generator);
            auto& service    = gen.alloc(irt::dynamics_type::queue);
            auto& throughput = gen.alloc(irt::dynamics_type::counter);

            arrivals.flags   = irt::child_flags::configurable;
            service.flags    = irt::child_flags::configurable;
            throughput.flags = irt::child_flags::observable;

            const auto arrivals_id   = gen.children.get_id(arrivals);
            const auto service_id    = gen.children.get_id(service);
            const auto throughput_id = gen.children.get_id(throughput);

            gen.children_names[arrivals_id]   = "lambda";
            gen.children_names[service_id]    = "mu";
            gen.children_names[throughput_id] = "throughput";

            // Modelling-time default period = 1.0 (should be overridden
            // to 3.0 by the x[2] injection at Level 3, IF injection works).
            gen.children_parameters[arrivals_id].set_generator_ta(1.0);
            gen.children_parameters[arrivals_id].reals[1] = 1.0;

            gen.children_parameters[service_id].set_queue(2.0);

            gen.connect(arrivals, { .model = 0 }, service, { .model = 0 });
            gen.connect(service, { .model = 0 }, throughput, { .model = 0 });

            ids.component_file_paths[compo_id].file = gen_component_file_id;
            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        mod.ids.write([&](auto& ids) {
            mod.files.write([&](auto& fs) {
                irt::project pj;
                pj.file = project_file_id;
                pj.sim.limits.set_bound(0, 20);
                expect(pj.set(ids, fs, gen_compo).has_value());
                expect(pj.save(fs, ids).has_value());
            });
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 2: sim_compo -- mu = fixed{2.0} (single branch),
        //      lambda = single{1.0} (the modelling-time default) ----
        simulation_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(
              dir_id, "sim-compo.irt", irt::file_type::component_file);
        });

        const auto sim_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_sim_component();
            auto& compo    = ids.components[compo_id];
            auto& sim      = ids.sim_components.get(compo.id.sim_id);

            mod.files.read([&](const auto& fs, auto) {
                auto exp_pj = irt::project::load(fs, ids, project_file_id);
                expect(fatal(exp_pj.has_value()));
                sim.assign(std::move(*exp_pj));
            });

            expect(fatal(eq(sim.factors.size(), 2u)));

            auto mu_id     = irt::factor_id{ 0 };
            auto lambda_id = irt::factor_id{ 0 };

            const auto& names = sim.factors.template get<irt::name_str>();
            for (const auto fid : sim.factors) {
                if (names[fid] == "mu")
                    mu_id = fid;
                if (names[fid] == "lambda")
                    lambda_id = fid;
            }

            sim.factors.template get<irt::factor_type>(mu_id) =
              irt::factor_type::fixed;
            const auto mu_factors = std::array<irt::real, 1>{ 2.0 };
            sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
              mu_factors.begin(), mu_factors.end());

            sim.factors.template get<irt::factor_type>(lambda_id) =
              irt::factor_type::single;
            sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

            const auto throughput_id = [&]() {
                for (const auto sid : sim.selections)
                    if (sim.selections.template get<irt::name_str>(sid) ==
                        "default")
                        return sid;
                return *sim.selections.begin();
            }();

            sim.selections.template get<irt::name_str>(throughput_id) =
              "throughput";
            sim.selections.template get<irt::criteria_type>(throughput_id) =
              irt::criteria_type::max;

            sim.objective.method = irt::optimization_method::simple;
            sim.objective.type   = irt::optimization_type::maximize;
            sim.objective.simple_params.primary = throughput_id;

            sim.file_id = project_file_id;
            ids.component_file_paths[compo_id].file =
              simulation_component_file_id;

            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 3: sim_wrapper_compo -- WITH the x[2] lambda injection
        // ----
        simulation_wrapper_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(
              dir_id, "sim-wrapper.irt", irt::file_type::component_file);
        });

        const auto sim_wrapper_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& cst_init = gen.alloc(irt::dynamics_type::constant);
            auto& cst_run  = gen.alloc(irt::dynamics_type::constant);
            auto& cst_lambda =
              gen.alloc(irt::dynamics_type::constant); // injects x[2]
            auto& sim_w = gen.alloc(irt::dynamics_type::simulation_wrapper);
            auto& throughput = gen.alloc(irt::dynamics_type::counter);

            cst_init.flags   = irt::child_flags::configurable;
            cst_run.flags    = irt::child_flags::configurable;
            cst_lambda.flags = irt::child_flags::configurable;
            throughput.flags = irt::child_flags::observable;

            const auto cst_init_id   = gen.children.get_id(cst_init);
            const auto cst_run_id    = gen.children.get_id(cst_run);
            const auto cst_lambda_id = gen.children.get_id(cst_lambda);
            const auto sim_w_id      = gen.children.get_id(sim_w);

            gen.children_names[cst_init_id]   = "init";
            gen.children_names[cst_run_id]    = "run";
            gen.children_names[cst_lambda_id] = "lambda-override";
            gen.children_names[sim_w_id]      = "psi-delta";

            gen.children_parameters[cst_init_id].set_constant(0.0, 0.0);
            gen.children_parameters[cst_run_id].set_constant(0.0, 1.0);
            // Override lambda's period to 3.0 (vs. the modelling-time
            // default of 1.0 set in sim_compo above).
            gen.children_parameters[cst_lambda_id].set_constant(3.0, 0.5);

            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::run] =
              ordinal(irt::simulation_wrapper::run_type::complete);
            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

            gen.connect(
              cst_init, { .model = 0 }, sim_w, { .model = 0 }); // x[0] init
            gen.connect(
              cst_run, { .model = 0 }, sim_w, { .model = 1 }); // x[1] run
            gen.connect(
              cst_lambda, { .model = 0 }, sim_w, { .model = 2 }); // x[2] lambda

            gen.connect(sim_w,
                        { .model = 0 },
                        throughput,
                        { .model = 0 }); // y[0]=throughput

            ids.component_file_paths[compo_id].file =
              simulation_wrapper_file_id;
            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        // ---- Run and check ----
        irt::project pj;
        mod.ids.read([&](const auto& ids, auto) {
            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(pj.set(ids, fs, sim_wrapper_compo).has_value()));
            });
        });

        pj.sim.limits.set_bound(0, 21);
        expect(pj.simulation_initialize().has_value());

        do {
            expect(pj.simulation_run_bag().has_value());
        } while (not pj.sim.current_time_expired());

        expect(pj.sim.finalize().has_value());

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::counter) {
                const auto& cpt = irt::get_dyn<irt::counter>(mdl);
                fmt::print("throughput: events={} last={}\n",
                           cpt.event_number,
                           cpt.last_value);
                // ~6-7 events if injection works (period 3.0 over horizon 20)
                // ~20 events if injection is ignored (period 1.0, the
                // modelling-time default)
            }
        }
    };

    "omega_b_to_delta_integration"_test = [] {
        const auto temp_path = temp_path_with_unlink{};
        expect(fatal(temp_path.success()));

        irt::journal_handler jn;
        irt::modeling        mod;

        irt::registred_path_id reg_id{ 0 };
        irt::dir_path_id       dir_id{ 0 };
        irt::file_path_id      gen_component_file_id{ 0 };
        irt::file_path_id      project_file_id{ 0 };
        irt::file_path_id      simulation_component_file_id{ 0 };
        irt::file_path_id      hsm_component_file_id{ 0 };
        irt::file_path_id      simulation_wrapper_file_id{ 0 };

        mod.files.write([&](auto& fs) {
            reg_id                              = fs.alloc_registred("temp", 0);
            fs.registred_paths.get(reg_id).path = temp_path.c_str();

            dir_id                          = fs.alloc_dir(reg_id);
            fs.dir_paths.get(dir_id).parent = reg_id;
            fs.dir_paths.get(dir_id).path   = "test-omega-b-integration";

            fs.create_directories(reg_id);
            fs.create_directories(dir_id);

            expect(fatal(fs.file_paths.can_alloc(5)));

            gen_component_file_id = fs.alloc_file(
              dir_id, "gen-compo.irt", irt::file_type::component_file);
            project_file_id = fs.alloc_file(
              dir_id, "project.pirt", irt::file_type::project_file);
        });

        // -----------------------------------------------------------
        // LEVEL 1 -- M_sim(theta): generator("lambda") -> queue("mu")
        //            -> counter("throughput")   [unchanged pattern]
        // -----------------------------------------------------------
        const auto gen_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& arrivals   = gen.alloc(irt::dynamics_type::generator);
            auto& service    = gen.alloc(irt::dynamics_type::queue);
            auto& throughput = gen.alloc(irt::dynamics_type::counter);

            arrivals.flags   = irt::child_flags::configurable;
            service.flags    = irt::child_flags::configurable;
            throughput.flags = irt::child_flags::observable;

            const auto arrivals_id   = gen.children.get_id(arrivals);
            const auto service_id    = gen.children.get_id(service);
            const auto throughput_id = gen.children.get_id(throughput);

            gen.children_names[arrivals_id]   = "lambda";
            gen.children_names[service_id]    = "mu";
            gen.children_names[throughput_id] = "throughput";

            // Modelling-time default period = 1.0. Level 4 (Omega_B) is
            // expected to override this via the "lambda" factor before
            // each deliberation episode.
            gen.children_parameters[arrivals_id].set_generator_ta(1.0);
            gen.children_parameters[arrivals_id].reals[1] = 1.0;

            gen.children_parameters[service_id].set_queue(2.0);

            gen.connect(arrivals, { .model = 0 }, service, { .model = 0 });
            gen.connect(service, { .model = 0 }, throughput, { .model = 0 });

            ids.component_file_paths[compo_id].file = gen_component_file_id;
            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        mod.ids.write([&](auto& ids) {
            mod.files.write([&](auto& fs) {
                irt::project pj;
                pj.file = project_file_id;
                pj.sim.limits.set_bound(0, 20);
                expect(pj.set(ids, fs, gen_compo).has_value());
                expect(pj.save(fs, ids).has_value());
            });
        });

        expect(fatal(mod.fill_components().has_value()));

        // -----------------------------------------------------------
        // LEVEL 2 -- the plan: mu fixed to a single branch (2.0),
        //            lambda left single{1.0} (the modelling-time
        //            default, to be overridden dynamically)
        // -----------------------------------------------------------
        simulation_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(
              dir_id, "sim-compo.irt", irt::file_type::component_file);
        });

        const auto sim_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_sim_component();
            auto& compo    = ids.components[compo_id];
            auto& sim      = ids.sim_components.get(compo.id.sim_id);

            mod.files.read([&](const auto& fs, auto) {
                auto exp_pj = irt::project::load(fs, ids, project_file_id);
                expect(fatal(exp_pj.has_value()));
                sim.assign(std::move(*exp_pj));
            });

            expect(fatal(eq(sim.factors.size(), 2u)));

            auto mu_id     = irt::factor_id{ 0 };
            auto lambda_id = irt::factor_id{ 0 };

            const auto& names = sim.factors.template get<irt::name_str>();
            for (const auto fid : sim.factors) {
                if (names[fid] == "mu")
                    mu_id = fid;
                if (names[fid] == "lambda")
                    lambda_id = fid;
            }

            sim.factors.template get<irt::factor_type>(mu_id) =
              irt::factor_type::fixed;
            const auto mu_factors = std::array<irt::real, 1>{ 2.0 };
            sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
              mu_factors.begin(), mu_factors.end());

            sim.factors.template get<irt::factor_type>(lambda_id) =
              irt::factor_type::single;
            sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

            const auto throughput_id = [&]() {
                for (const auto sid : sim.selections)
                    if (sim.selections.template get<irt::name_str>(sid) ==
                        "default")
                        return sid;
                return *sim.selections.begin();
            }();

            sim.selections.template get<irt::name_str>(throughput_id) =
              "throughput";
            sim.selections.template get<irt::criteria_type>(throughput_id) =
              irt::criteria_type::max;

            sim.objective.method = irt::optimization_method::simple;
            sim.objective.type   = irt::optimization_type::maximize;
            sim.objective.simple_params.primary = throughput_id;

            sim.file_id = project_file_id;
            ids.component_file_paths[compo_id].file =
              simulation_component_file_id;

            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        expect(fatal(mod.fill_components().has_value()));

        // -----------------------------------------------------------
        // LEVEL 4 -- Omega_B: the validated 6-state revision chain,
        //            built at the modelling layer this time.
        // -----------------------------------------------------------
        hsm_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(
              dir_id, "omega-b.irt", irt::file_type::component_file);
        });

        const auto hsm_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_hsm_component();
            auto& compo    = ids.components[compo_id];
            auto& hsm_c    = ids.hsm_components.get(compo.id.hsm_id);
            auto& machine  = hsm_c.machine;

            using var = irt::hierarchical_state_machine::variable;

            machine.constants[0] = 0.1f; // +epsilon_0
            machine.constants[1] =
              1.0f; // seed lambda_hat (matches sim_compo default)
            machine.constants[2] = -0.1f; // -epsilon_0

            expect(!!machine.set_state(
              0u, irt::hierarchical_state_machine::invalid_state_id, 1u));
            machine.states[0u].enter_action.set_affect(var::var_r1,
                                                       var::hsm_constant_1);

            expect(!!machine.set_state(1u, 0u));
            machine.states[1u].condition.set(0b1100u,
                                             0b1100u); // port_0 & port_1
            machine.states[1u].if_transition = 2u;

            expect(!!machine.set_state(2u, 0u));
            machine.states[2u].enter_action.set_affect(var::var_r2,
                                                       var::port_1);
            machine.states[2u].exit_action.set_minus(var::var_r2, var::var_r1);
            machine.states[2u].if_transition = 3u;

            expect(!!machine.set_state(3u, 0u));
            machine.states[3u].condition.set_greater(var::var_r2,
                                                     var::hsm_constant_0);
            machine.states[3u].if_transition   = 5u;
            machine.states[3u].else_transition = 4u;

            expect(!!machine.set_state(4u, 0u));
            machine.states[4u].condition.set_less(var::var_r2,
                                                  var::hsm_constant_2);
            machine.states[4u].if_transition   = 5u;
            machine.states[4u].else_transition = 1u;

            expect(!!machine.set_state(5u, 0u));
            machine.states[5u].enter_action.set_plus(var::var_r1, var::var_r2);
            machine.states[5u].exit_action.set_output(var::port_0, var::var_r1);
            machine.states[5u].if_transition = 1u;

            ids.component_file_paths[compo_id].file = hsm_component_file_id;
            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        // -----------------------------------------------------------
        // LEVEL 3 -- Psi + Delta, WITH Omega_B feeding x[2] (lambda)
        // -----------------------------------------------------------
        simulation_wrapper_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(
              dir_id, "sim-wrapper.irt", irt::file_type::component_file);
        });

        const auto sim_wrapper_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& cst_init = gen.alloc(irt::dynamics_type::constant);
            auto& cst_obs  = gen.alloc(irt::dynamics_type::constant);
            auto& cst_run  = gen.alloc(irt::dynamics_type::constant);
            auto& hsmw     = gen.alloc(irt::dynamics_type::hsm_wrapper);
            auto& sim_w    = gen.alloc(irt::dynamics_type::simulation_wrapper);

            auto& best_throughput = gen.alloc(irt::dynamics_type::counter);
            auto& win_lambda      = gen.alloc(irt::dynamics_type::counter);

            cst_init.flags        = irt::child_flags::configurable;
            cst_obs.flags         = irt::child_flags::configurable;
            cst_run.flags         = irt::child_flags::configurable;
            best_throughput.flags = irt::child_flags::observable;
            win_lambda.flags      = irt::child_flags::observable;

            const auto cst_init_id = gen.children.get_id(cst_init);
            const auto cst_obs_id  = gen.children.get_id(cst_obs);
            const auto cst_run_id  = gen.children.get_id(cst_run);
            const auto hsmw_id     = gen.children.get_id(hsmw);
            const auto sim_w_id    = gen.children.get_id(sim_w);

            gen.children_names[cst_init_id] = "init";
            gen.children_names[cst_obs_id]  = "observation";
            gen.children_names[cst_run_id]  = "run";
            gen.children_names[hsmw_id]     = "omega-b";
            gen.children_names[sim_w_id]    = "psi-delta";

            // init at t=0.
            gen.children_parameters[cst_init_id].set_constant(0.0, 0.0);

            // A single observation, value=4.0, at t=0: error = 4.0 - 1.0
            // (seed) = +3.0, well above epsilon_0=0.1 -> expect a positive
            // revision to lambda_hat = 4.0.
            gen.children_parameters[cst_obs_id].set_constant(4.0, 0.0);

            // run at t=2.0, comfortably after the HSM's internal cascade
            // and the injection into x[2] are expected to have settled
            // (same safety margin principle as the validated init/run
            // separation).
            gen.children_parameters[cst_run_id].set_constant(0.0, 2.0);

            gen.children_parameters[hsmw_id].set_hsm_wrapper(
              ordinal(hsm_compo));
            // TODO(verify): confirmed only at the simulation layer
            // (ordinal(sim.hsms.get_id(hsm))); the modelling-layer argument
            // is inferred by analogy with simulation_wrapper_tag::id.

            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::run] =
              ordinal(irt::simulation_wrapper::run_type::complete);
            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

            // Wiring:
            gen.connect(
              cst_init, { .model = 0 }, sim_w, { .model = 0 }); // x[0] init
            gen.connect(
              cst_run, { .model = 0 }, sim_w, { .model = 1 }); // x[1] run

            // Observation dual-wired into hsmw (confirmed pattern).
            gen.connect(cst_obs, { .model = 0 }, hsmw, { .model = 0 });
            gen.connect(cst_obs, { .model = 0 }, hsmw, { .model = 1 });

            // Omega_B's revised belief feeds the lambda factor.
            gen.connect(
              hsmw, { .model = 0 }, sim_w, { .model = 2 }); // x[2] lambda

            // Outputs: throughput (selection) and winning lambda (factor).
            gen.connect(sim_w, { .model = 0 }, best_throughput, { .model = 0 });
            gen.connect(sim_w, { .model = 1 }, win_lambda, { .model = 0 });

            ids.component_file_paths[compo_id].file =
              simulation_wrapper_file_id;
            mod.files.read(
              [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });
            return compo_id;
        });

        // -----------------------------------------------------------
        // Run and check
        // -----------------------------------------------------------
        irt::project pj;
        mod.ids.read([&](const auto& ids, auto) {
            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(pj.set(ids, fs, sim_wrapper_compo).has_value()));
            });
        });

        pj.sim.limits.set_bound(0, 22);
        expect(pj.simulation_initialize().has_value());

        do {
            expect(pj.simulation_run_bag().has_value());
        } while (not pj.sim.current_time_expired());

        expect(pj.sim.finalize().has_value());

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::counter) {
                const auto& cpt = irt::get_dyn<irt::counter>(mdl);
                fmt::print("counter[{:6}]: events={:6} last={:8}\n",
                           irt::get_index(pj.sim.models.get_id(mdl)),
                           cpt.event_number,
                           cpt.last_value);
                // win_lambda: expect last close to 4.0 if the full chain
                // (observation -> Omega_B revision -> x[2] injection ->
                // Delta using the REVISED belief) works end-to-end.
                // best_throughput: expect a trajectory consistent with
                // period=4.0, not the modelling-time default period=1.0.
            }
        }
    };
}
