// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include "utils.hpp"

#include <boost/ut.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

using namespace std::literals;
using namespace boost::ut;

template<typename Dynamics>
inline auto get_p(irt::simulation& sim, const Dynamics& d) noexcept
  -> irt::parameter&
{
    return sim.parameters[sim.get_id(d)];
}

/// The DEVS-DAMA supervisor of a M/M/1 queue: a generator of exponential
/// arrivals (the factor lambda is their mean period), a single server queue
/// (the factor mu is its rate) and the constant cost of the mode (the factor
/// cost). A simulation wrapper selects the scenario with the weighted sum of
/// the criteria of the queue (the number of clients) and of the cost.
struct supervisor_config {
    std::span<const irt::real> mu;
    std::span<const irt::real> cost;
    irt::u32                   mu_link   = 1u;
    irt::u32                   cost_link = 1u;
    irt::real                  lambda    = 0.4; //!< Injected as 1 / lambda.
    irt::real                  horizon   = 100;
    irt::criteria_type         criteria  = irt::criteria_type::max_last;
    irt::norm_type             norm      = irt::norm_type::min_max;
    irt::real                  w_queue   = 0.6;
    irt::real                  w_cost    = 0.4;
    irt::u32                   replications = 1u;
    irt::u64                   seed = irt::simulation_component::default_seed;
};

/// A sub-simulation: its scenario, its replication and what it observed.
struct supervisor_scenario {
    irt::real mu;
    irt::real cost;
    irt::real queue_criteria;
    irt::real cost_criteria;
    irt::u64  seed;
    irt::i64  arrivals;
    irt::i64  departures;
};

struct supervisor_result {
    irt::error_code                  error;
    irt::real                        selected_mu = 0;
    irt::i64                         messages    = 0;
    std::vector<supervisor_scenario> scenarios; //!< One by sub-simulation.

    /// The results of the wrapper by scenario: the means and standard
    /// deviations of the selections (queue, cost) and of the objective.
    std::vector<irt::embedded_simulation::scenario_result> results;
    std::size_t                                            best_scenario = 0;
    double                                                 decision_ms   = 0;
};

static supervisor_result run_supervisor(const supervisor_config& cfg) noexcept
{
    supervisor_result result;

    const auto temp_path = irt::temp_path_with_unlink{ "supervisor"sv };
    expect(fatal(temp_path.success()));

    irt::modeling     mod;
    irt::dir_path_id  dir_id{ 0 };
    irt::file_path_id gen_file{ 0 };
    irt::file_path_id project_file{ 0 };
    irt::file_path_id sim_file{ 0 };
    irt::file_path_id wrapper_file{ 0 };

    mod.files.write([&](auto& fs) {
        const auto reg_id                   = fs.alloc_registred("temp", 0);
        fs.registred_paths.get(reg_id).path = temp_path.c_str();

        dir_id                          = fs.alloc_dir(reg_id);
        fs.dir_paths.get(dir_id).parent = reg_id;
        fs.dir_paths.get(dir_id).path   = "test-supervisor";

        fs.create_directories(reg_id);
        fs.create_directories(dir_id);

        gen_file     = fs.alloc_file(dir_id, "gen-compo.irt",
                                     irt::file_type::component_file);
        project_file = fs.alloc_file(dir_id, "project.pirt",
                                     irt::file_type::project_file);
    });

    // ---- LEVEL 1: arrivals Exp(1) * period -> queue Exp(1) / mu, and the
    // cost of the mode.
    const auto gen_compo = mod.ids.write([&](auto& ids) {
        auto  compo_id = ids.alloc_generic_component();
        auto& compo    = ids.components[compo_id];
        auto& gen      = ids.generic_components.get(compo.id.generic_id);

        compo.srcs.data.reserve(2);
        for (const auto* name : { "arrivals", "services" }) {
            auto& rnd    = compo.srcs.alloc_random_source(name);
            rnd.type     = irt::distribution_type::exponential;
            rnd.reals[0] = 1.0;
        }

        auto       src_it      = compo.srcs.data.begin();
        const auto arrival_src = *src_it;
        const auto service_src = *++src_it;

        auto& arrivals = gen.alloc(irt::dynamics_type::generator);
        auto& queue    = gen.alloc(irt::dynamics_type::single_server_queue);
        auto& cost     = gen.alloc(irt::dynamics_type::constant);

        arrivals.flags = irt::child_flags::configurable;
        queue.flags.set(irt::child_flags::configurable);
        queue.flags.set(irt::child_flags::observable);
        cost.flags.set(irt::child_flags::configurable);
        cost.flags.set(irt::child_flags::observable);

        const auto arrivals_id = gen.children.get_id(arrivals);
        const auto queue_id    = gen.children.get_id(queue);
        const auto cost_id     = gen.children.get_id(cost);

        gen.children_names[arrivals_id] = "lambda";
        gen.children_names[queue_id]    = "mu";
        gen.children_names[cost_id]     = "cost";

        gen.children_parameters[arrivals_id]
          .set_generator_ta(arrival_src)
          .set_generator_ta_scale(1.0);
        gen.children_parameters[queue_id]
          .set_single_server_queue_ta(service_src)
          .set_single_server_queue_rate(1.0);
        gen.children_parameters[cost_id].set_constant(0.0, 0.0);

        // The outputs of the generator and of the queue are the internal
        // transitions of a sub-simulation.
        auto& arrival_count   = gen.alloc(irt::dynamics_type::counter);
        auto& departure_count = gen.alloc(irt::dynamics_type::counter);

        (void)gen.connect(arrivals, { .model = 0 }, queue, { .model = 0 });
        (void)gen.connect(arrivals, { .model = 0 }, arrival_count,
                          { .model = 0 });
        (void)gen.connect(queue, { .model = 0 }, departure_count,
                          { .model = 0 });

        ids.component_file_paths[compo_id].file = gen_file;
        mod.files.read([&](const auto& fs, auto) {
            expect(mod.save(ids, fs, compo_id).has_value());
        });
        return compo_id;
    });

    mod.ids.write([&](auto& ids) {
        mod.files.write([&](auto& fs) {
            irt::project pj;
            pj.project_file = project_file;
            pj.sim.limits.set_bound(0, cfg.horizon);
            expect(pj.set(ids, fs, gen_compo).has_value());
            expect(pj.save(fs, ids).has_value());
        });
    });

    expect(fatal(mod.fill_components().has_value()));

    // ---- LEVEL 2: the factors, their links and the objective. A file is
    // allocated when written: fill_components() loads all the files.
    sim_file = mod.files.write([&](auto& fs) {
        return fs.alloc_file(dir_id, "sim-compo.irt",
                             irt::file_type::component_file);
    });

    int lambda_pos = -1;
    int mu_pos     = -1;

    const auto sim_compo = mod.ids.write([&](auto& ids) {
        auto  compo_id = ids.alloc_sim_component();
        auto& compo    = ids.components[compo_id];
        auto& sim      = ids.sim_components.get(compo.id.sim_id);

        mod.files.read([&](const auto& fs, auto) {
            auto exp_pj = irt::project::load(fs, ids, project_file);
            expect(fatal(exp_pj.has_value()));
            expect(fatal(sim.assign(std::move(*exp_pj)).has_value()));
        });

        expect(fatal(eq(sim.factors.size(), 3u)));

        const auto set_fixed = [&](const irt::factor_id       fid,
                                   std::span<const irt::real> values,
                                   const irt::u32             link) {
            sim.factors.template get<irt::factor_type>(
              fid) = irt::factor_type::fixed;
            sim.factors.template get<irt::fixed_factor>(fid).values.assign(
              values.begin(), values.end());
            sim.factors.template get<irt::factor_link>(fid).group = link;
        };

        int pos = 0;
        for (const auto fid : sim.factors) {
            const auto name = sim.factors.template get<irt::name_str>(fid).sv();

            if (name == "mu") {
                mu_pos = pos;
                set_fixed(fid, cfg.mu, cfg.mu_link);
            } else if (name == "cost") {
                set_fixed(fid, cfg.cost, cfg.cost_link);
            } else if (name == "lambda") {
                lambda_pos = pos;
                sim.factors.template get<irt::factor_type>(
                  fid) = irt::factor_type::single;
                sim.factors.template get<irt::single_factor>(fid).value = 1.0;
            }
            ++pos;
        }

        expect(fatal(mu_pos >= 0 and lambda_pos >= 0));
        expect(fatal(eq(sim.selections.size(), 2u)));

        auto& obj                    = sim.objective;
        obj.method                   = irt::optimization_method::weighted_sum;
        obj.type                     = irt::optimization_type::minimize;
        obj.weighted_sum_params.norm = cfg.norm;
        obj.weighted_sum_params.weights.resize(2);
        obj.weighted_sum_params.types.resize(2);

        for (const auto sid : sim.selections) {
            const auto idx  = get_index(sid);
            const auto name = sim.selections.template get<irt::name_str>(sid);

            sim.selections.template get<irt::criteria_type>(sid) = cfg.criteria;
            obj.weighted_sum_params
              .types[idx] = irt::optimization_type::minimize;
            obj.weighted_sum_params.weights[idx] = name.sv() == "mu"
                                                     ? cfg.w_queue
                                                     : cfg.w_cost;
        }

        sim.replications                        = cfg.replications;
        sim.seed                                = cfg.seed;
        sim.file_id                             = project_file;
        ids.component_file_paths[compo_id].file = sim_file;

        mod.files.read([&](const auto& fs, auto) {
            expect(fatal(mod.save(ids, fs, compo_id).has_value()));
        });

        return compo_id;
    });

    expect(fatal(mod.fill_components().has_value()));

    // ---- LEVEL 3: the wrapper, lambda injected as the mean period.
    wrapper_file = mod.files.write([&](auto& fs) {
        return fs.alloc_file(dir_id, "sim-wrapper.irt",
                             irt::file_type::component_file);
    });

    const auto wrapper_compo = mod.ids.write([&](auto& ids) {
        auto  compo_id = ids.alloc_generic_component();
        auto& compo    = ids.components[compo_id];
        auto& gen      = ids.generic_components.get(compo.id.generic_id);

        auto& cst_init    = gen.alloc(irt::dynamics_type::constant);
        auto& cst_run     = gen.alloc(irt::dynamics_type::constant);
        auto& cst_lambda  = gen.alloc(irt::dynamics_type::constant);
        auto& sim_w       = gen.alloc(irt::dynamics_type::simulation_wrapper);
        auto& selected_mu = gen.alloc(irt::dynamics_type::counter);

        const auto sim_w_id = gen.children.get_id(sim_w);

        gen.children_parameters[gen.children.get_id(cst_init)].set_constant(
          0.0, 0.0);
        gen.children_parameters[gen.children.get_id(cst_run)].set_constant(0.0,
                                                                           1.0);
        gen.children_parameters[gen.children.get_id(cst_lambda)].set_constant(
          1.0 / cfg.lambda, 0.5);

        gen.children_parameters[sim_w_id]
          .integers[irt::simulation_wrapper_tag::run] = ordinal(
          irt::simulation_wrapper::run_type::complete);
        gen.children_parameters[sim_w_id]
          .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

        expect(gen.connect(cst_init, { .model = 0 }, sim_w, { .model = 0 })
                 .has_value());
        expect(gen.connect(cst_run, { .model = 0 }, sim_w, { .model = 1 })
                 .has_value());
        expect(gen
                 .connect(cst_lambda, { .model = 0 }, sim_w,
                          { .model = 2 + lambda_pos })
                 .has_value());
        expect(
          gen.connect(sim_w, { .model = mu_pos }, selected_mu, { .model = 0 })
            .has_value());

        ids.component_file_paths[compo_id].file = wrapper_file;
        mod.files.read([&](const auto& fs, auto) {
            expect(mod.save(ids, fs, compo_id).has_value());
        });
        return compo_id;
    });

    irt::project pj;
    mod.ids.read([&](const auto& ids, auto) {
        mod.files.read([&](const auto& fs, auto) {
            expect(fatal(pj.set(ids, fs, wrapper_compo).has_value()));
        });
    });

    const auto begin = std::chrono::steady_clock::now();

    pj.sim.limits.set_bound(0, 2);
    if (auto ret = pj.simulation_init(mod); not ret) {
        result.error = ret.error();
        return result;
    }

    do {
        if (auto ret = pj.sim.run(); not ret) {
            result.error = ret.error();
            return result;
        }
    } while (not pj.sim.current_time_expired());

    result.decision_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - begin)
                           .count();

    expect(pj.sim.finalize().has_value());

    for (const auto& mdl : pj.sim.models) {
        if (mdl.type == irt::dynamics_type::counter) {
            const auto& cpt    = irt::get_dyn<irt::counter>(mdl);
            result.selected_mu = cpt.last_value;
            result.messages    = cpt.event_number;
        }
    }

    // The scenarios from the parameters and the observations of the
    // sub-simulations.
    for (const auto& embed : pj.sim.sims) {
        irt::model_id mu_mdl   = irt::undefined<irt::model_id>();
        irt::model_id cost_mdl = irt::undefined<irt::model_id>();

        for (const auto fid : embed.factors) {
            const auto name = embed.factors.get<irt::name_str>(fid).sv();
            if (name == "mu")
                mu_mdl = embed.factors.get<irt::model_id>(fid);
            if (name == "cost")
                cost_mdl = embed.factors.get<irt::model_id>(fid);
        }

        const auto& sims = embed.embedded_sims.get<irt::simulation>();
        const auto& obs  = embed.embedded_sims.get<
           irt::embedded_simulation::simulation_observation>();

        const auto criteria = [&](const auto& o, const irt::model_id id) {
            const auto* ptr = o.get(id);
            return ptr ? ptr->compute_result(cfg.criteria)
                       : std::numeric_limits<irt::real>::quiet_NaN();
        };

        for (const auto sub : embed.embedded_sims) {
            const auto idx = get_index(sub);

            // The arrivals are never fewer than the departures.
            irt::i64 counts[2] = { 0, 0 };
            int      n         = 0;
            for (const auto& mdl : sims[idx].models)
                if (mdl.type == irt::dynamics_type::counter and n < 2)
                    counts[n++] = irt::get_dyn<irt::counter>(mdl).event_number;

            result.scenarios.push_back(supervisor_scenario{
              .mu   = sims[idx].parameters[get_index(mu_mdl)].reals[0],
              .cost = sims[idx].parameters[get_index(cost_mdl)].reals[0],
              .queue_criteria = criteria(obs[idx], mu_mdl),
              .cost_criteria  = criteria(obs[idx], cost_mdl),
              .seed           = sims[idx].srcs.seed,
              .arrivals       = std::max(counts[0], counts[1]),
              .departures     = std::min(counts[0], counts[1]) });
        }

        for (const auto& r : embed.results)
            result.results.push_back(r);
        result.best_scenario = embed.best_scenario;
    }

    return result;
}

int main(int argc, const char* argv[])
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    // A name runs only this test.
    if (argc > 1)
        cfg<override> = { .filter = argv[1] };

    "simulation_wrapper_lambda_injection"_test = [] {
        const auto temp_path = irt::temp_path_with_unlink{
            "lambda_injection"sv
        };

        expect(fatal(temp_path.success()));

        irt::modeling          mod;
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
            project_file_id = fs.alloc_file(dir_id, "project.pirt",
                                            irt::file_type::project_file);
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

            (void)gen.connect(arrivals, { .model = 0 }, service,
                              { .model = 0 });
            (void)gen.connect(service, { .model = 0 }, throughput,
                              { .model = 0 });

            ids.component_file_paths[compo_id].file = gen_component_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(mod.save(ids, fs, compo_id).has_value());
            });
            return compo_id;
        });

        mod.ids.write([&](auto& ids) {
            mod.files.write([&](auto& fs) {
                irt::project pj;
                pj.project_file = project_file_id;
                pj.sim.limits.set_bound(0, 20);
                expect(pj.set(ids, fs, gen_compo).has_value());
                expect(pj.save(fs, ids).has_value());
            });
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 2: sim_compo -- mu = fixed{2.0} (single branch),
        //      lambda = single{1.0} (the modelling-time default) ----
        simulation_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "sim-compo.irt",
                                 irt::file_type::component_file);
        });

        const auto sim_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_sim_component();
            auto& compo    = ids.components[compo_id];
            auto& sim      = ids.sim_components.get(compo.id.sim_id);

            mod.files.read([&](const auto& fs, auto) {
                auto exp_pj = irt::project::load(fs, ids, project_file_id);
                expect(fatal(exp_pj.has_value()));
                expect(fatal(sim.assign(std::move(*exp_pj)).has_value()));
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

            sim.factors.template get<irt::factor_type>(
              mu_id)              = irt::factor_type::fixed;
            const auto mu_factors = std::array<irt::real, 1>{ 2.0 };
            sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
              mu_factors.begin(), mu_factors.end());

            sim.factors.template get<irt::factor_type>(
              lambda_id) = irt::factor_type::single;
            sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

            const auto throughput_id = [&]() {
                for (const auto sid : sim.selections)
                    if (sim.selections.template get<irt::name_str>(sid) ==
                        "default")
                        return sid;
                return *sim.selections.begin();
            }();

            sim.selections.template get<irt::name_str>(
              throughput_id) = "throughput";
            sim.selections.template get<irt::criteria_type>(
              throughput_id) = irt::criteria_type::max;

            sim.objective.method = irt::optimization_method::simple;
            sim.objective.type   = irt::optimization_type::maximize;
            sim.objective.simple_params.primary = throughput_id;

            sim.file_id = project_file_id;
            ids.component_file_paths[compo_id]
              .file = simulation_component_file_id;

            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(mod.save(ids, fs, compo_id).has_value()));
            });

            return compo_id;
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 3: sim_wrapper_compo -- WITH the x[2] lambda injection
        // ----
        simulation_wrapper_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "sim-wrapper.irt",
                                 irt::file_type::component_file);
        });

        const auto sim_wrapper_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& cst_init   = gen.alloc(irt::dynamics_type::constant);
            auto& cst_run    = gen.alloc(irt::dynamics_type::constant);
            auto& cst_lambda = gen.alloc(
              irt::dynamics_type::constant); // injects x[2]
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
              .integers[irt::simulation_wrapper_tag::run] = ordinal(
              irt::simulation_wrapper::run_type::complete);
            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

            expect(gen.connect(cst_init, { .model = 0 }, sim_w, { .model = 0 })
                     .has_value()); // x[0] init
            expect(gen.connect(cst_run, { .model = 0 }, sim_w, { .model = 1 })
                     .has_value()); // x[1] run
            expect(
              gen.connect(cst_lambda, { .model = 0 }, sim_w, { .model = 2 })
                .has_value()); // x[2] lambda

            expect(
              gen.connect(sim_w, { .model = 0 }, throughput, { .model = 0 })
                .has_value()); // y[0]=throughput

            ids.component_file_paths[compo_id]
              .file = simulation_wrapper_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(mod.save(ids, fs, compo_id).has_value());
            });
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
        expect(pj.simulation_init(mod).has_value());

        do {
            expect(pj.sim.run().has_value());
        } while (not pj.sim.current_time_expired());

        expect(pj.sim.finalize().has_value());

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::counter) {
                const auto& cpt = irt::get_dyn<irt::counter>(mdl);
                fmt::print("throughput: events={} last={}\n", cpt.event_number,
                           cpt.last_value);
                // ~6-7 events if injection works (period 3.0 over horizon 20)
                // ~20 events if injection is ignored (period 1.0, the
                // modelling-time default)
            }
        }
    };

    "simulation_wrapper_single_server_queue_selects_mu"_test = [] {
        const auto temp_path = irt::temp_path_with_unlink{
            "single_server_queue"sv
        };

        expect(fatal(temp_path.success()));

        irt::modeling          mod;
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
            fs.dir_paths.get(dir_id).path   = "test-single-server-queue";

            fs.create_directories(reg_id);
            fs.create_directories(dir_id);

            expect(fatal(fs.file_paths.can_alloc(3)));

            gen_component_file_id = fs.alloc_file(
              dir_id, "gen-compo.irt", irt::file_type::component_file);
            project_file_id = fs.alloc_file(dir_id, "project.pirt",
                                            irt::file_type::project_file);
        });

        // ---- LEVEL 1: arrivals -> single server queue -> throughput. The
        // service is Exp(1) divided by the rate of the queue: Exp(mu).
        const auto gen_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            compo.srcs.data.reserve(1);
            auto& rnd              = compo.srcs.alloc_random_source("service");
            rnd.type               = irt::distribution_type::exponential;
            rnd.reals[0]           = 1.0;
            const auto service_src = *compo.srcs.data.begin();

            auto& arrivals = gen.alloc(irt::dynamics_type::generator);
            auto& service  = gen.alloc(irt::dynamics_type::single_server_queue);
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

            // The first real of the generator is its period: the factor
            // lambda gives deterministic arrivals of period 1 / lambda.
            gen.children_parameters[arrivals_id].set_generator_ta(1.0);
            gen.children_parameters[service_id]
              .set_single_server_queue_ta(service_src)
              .set_single_server_queue_rate(1.0);

            (void)gen.connect(arrivals, { .model = 0 }, service,
                              { .model = 0 });
            (void)gen.connect(service, { .model = 0 }, throughput,
                              { .model = 0 });

            ids.component_file_paths[compo_id].file = gen_component_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(mod.save(ids, fs, compo_id).has_value());
            });
            return compo_id;
        });

        mod.ids.write([&](auto& ids) {
            mod.files.write([&](auto& fs) {
                irt::project pj;
                pj.project_file = project_file_id;
                pj.sim.limits.set_bound(0, 2000);
                expect(pj.set(ids, fs, gen_compo).has_value());
                expect(pj.save(fs, ids).has_value());
            });
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 2: mu = fixed{0.6, 1.2}, lambda = single{1.0} replaced by
        // the injection, selection: the maximal throughput.
        simulation_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "sim-compo.irt",
                                 irt::file_type::component_file);
        });

        int mu_pos     = -1;
        int lambda_pos = -1;

        const auto sim_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_sim_component();
            auto& compo    = ids.components[compo_id];
            auto& sim      = ids.sim_components.get(compo.id.sim_id);

            mod.files.read([&](const auto& fs, auto) {
                auto exp_pj = irt::project::load(fs, ids, project_file_id);
                expect(fatal(exp_pj.has_value()));
                expect(fatal(sim.assign(std::move(*exp_pj)).has_value()));
            });

            expect(fatal(eq(sim.factors.size(), 2u)));

            auto mu_id     = irt::factor_id{ 0 };
            auto lambda_id = irt::factor_id{ 0 };

            // The ports of the wrapper follow the order of the factors.
            const auto& names = sim.factors.template get<irt::name_str>();
            int         pos   = 0;
            for (const auto fid : sim.factors) {
                if (names[fid] == "mu") {
                    mu_id  = fid;
                    mu_pos = pos;
                }
                if (names[fid] == "lambda") {
                    lambda_id  = fid;
                    lambda_pos = pos;
                }
                ++pos;
            }

            expect(fatal(mu_pos >= 0 and lambda_pos >= 0));

            sim.factors.template get<irt::factor_type>(
              mu_id)              = irt::factor_type::fixed;
            const auto mu_factors = std::array<irt::real, 2>{ 0.6, 1.2 };
            sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
              mu_factors.begin(), mu_factors.end());

            sim.factors.template get<irt::factor_type>(
              lambda_id) = irt::factor_type::single;
            sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

            const auto throughput_id = [&]() {
                for (const auto sid : sim.selections)
                    if (sim.selections.template get<irt::name_str>(sid) ==
                        "default")
                        return sid;
                return *sim.selections.begin();
            }();

            sim.selections.template get<irt::name_str>(
              throughput_id) = "throughput";
            sim.selections.template get<irt::criteria_type>(
              throughput_id) = irt::criteria_type::max;

            sim.objective.method = irt::optimization_method::simple;
            sim.objective.type   = irt::optimization_type::maximize;
            sim.objective.simple_params.primary = throughput_id;

            sim.file_id = project_file_id;
            ids.component_file_paths[compo_id]
              .file = simulation_component_file_id;

            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(mod.save(ids, fs, compo_id).has_value()));
            });

            return compo_id;
        });

        expect(fatal(mod.fill_components().has_value()));

        // ---- LEVEL 3: the wrapper with lambda = 0.8 injected as the period
        // 1 / 0.8 = 1.25, a counter reads the selected mu.
        simulation_wrapper_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "sim-wrapper.irt",
                                 irt::file_type::component_file);
        });

        const auto sim_wrapper_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& cst_init   = gen.alloc(irt::dynamics_type::constant);
            auto& cst_run    = gen.alloc(irt::dynamics_type::constant);
            auto& cst_lambda = gen.alloc(irt::dynamics_type::constant);
            auto& sim_w = gen.alloc(irt::dynamics_type::simulation_wrapper);
            auto& selected_mu = gen.alloc(irt::dynamics_type::counter);

            cst_init.flags   = irt::child_flags::configurable;
            cst_run.flags    = irt::child_flags::configurable;
            cst_lambda.flags = irt::child_flags::configurable;

            const auto cst_init_id    = gen.children.get_id(cst_init);
            const auto cst_run_id     = gen.children.get_id(cst_run);
            const auto cst_lambda_id  = gen.children.get_id(cst_lambda);
            const auto sim_w_id       = gen.children.get_id(sim_w);
            const auto selected_mu_id = gen.children.get_id(selected_mu);

            gen.children_names[cst_init_id]    = "init";
            gen.children_names[cst_run_id]     = "run";
            gen.children_names[cst_lambda_id]  = "lambda-burst";
            gen.children_names[sim_w_id]       = "psi-delta";
            gen.children_names[selected_mu_id] = "selected-mu";

            gen.children_parameters[cst_init_id].set_constant(0.0, 0.0);
            gen.children_parameters[cst_run_id].set_constant(0.0, 1.0);
            gen.children_parameters[cst_lambda_id].set_constant(1.0 / 0.8, 0.5);

            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::run] = ordinal(
              irt::simulation_wrapper::run_type::complete);
            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

            expect(gen.connect(cst_init, { .model = 0 }, sim_w, { .model = 0 })
                     .has_value());
            expect(gen.connect(cst_run, { .model = 0 }, sim_w, { .model = 1 })
                     .has_value());
            expect(gen
                     .connect(cst_lambda, { .model = 0 }, sim_w,
                              { .model = 2 + lambda_pos })
                     .has_value());
            expect(gen
                     .connect(sim_w, { .model = mu_pos }, selected_mu,
                              { .model = 0 })
                     .has_value());

            ids.component_file_paths[compo_id]
              .file = simulation_wrapper_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(mod.save(ids, fs, compo_id).has_value());
            });
            return compo_id;
        });

        irt::project pj;
        mod.ids.read([&](const auto& ids, auto) {
            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(pj.set(ids, fs, sim_wrapper_compo).has_value()));
            });
        });

        pj.sim.limits.set_bound(0, 21);
        expect(fatal(pj.simulation_init(mod).has_value()));

        do {
            expect(fatal(pj.sim.run().has_value()));
        } while (not pj.sim.current_time_expired());

        expect(pj.sim.finalize().has_value());

        auto found = 0;
        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::counter) {
                const auto& cpt = irt::get_dyn<irt::counter>(mdl);
                fmt::print("single server queue, lambda = 0.8: selected mu = "
                           "{} ({} message)\n",
                           cpt.last_value, cpt.event_number);

                // mu = 0.6 < lambda: the queue saturates and its throughput
                // is mu. mu = 1.2 serves all the arrivals.
                expect(eq(cpt.event_number, 1));
                expect(eq(cpt.last_value, 1.2));
                ++found;
            }
        }
        expect(eq(found, 1));
    };

    "omega_b_to_delta_integration"_test = [] {
        const auto temp_path = irt::temp_path_with_unlink{ "omega_b"sv };
        expect(fatal(temp_path.success()));

        irt::modeling          mod;
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
            project_file_id = fs.alloc_file(dir_id, "project.pirt",
                                            irt::file_type::project_file);
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

            expect(fatal(
              gen.connect(arrivals, { .model = 0 }, service, { .model = 0 })
                .has_value()));
            expect(fatal(
              gen.connect(service, { .model = 0 }, throughput, { .model = 0 })
                .has_value()));

            ids.component_file_paths[compo_id].file = gen_component_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(mod.save(ids, fs, compo_id).has_value());
            });
            return compo_id;
        });

        mod.ids.write([&](auto& ids) {
            mod.files.write([&](auto& fs) {
                irt::project pj;
                pj.project_file = project_file_id;
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
            return fs.alloc_file(dir_id, "sim-compo.irt",
                                 irt::file_type::component_file);
        });

        const auto sim_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_sim_component();
            auto& compo    = ids.components[compo_id];
            auto& sim      = ids.sim_components.get(compo.id.sim_id);

            mod.files.read([&](const auto& fs, auto) {
                auto exp_pj = irt::project::load(fs, ids, project_file_id);
                expect(fatal(exp_pj.has_value()));
                expect(fatal(sim.assign(std::move(*exp_pj)).has_value()));
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

            sim.factors.template get<irt::factor_type>(
              mu_id)              = irt::factor_type::fixed;
            const auto mu_factors = std::array<irt::real, 1>{ 2.0 };
            sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
              mu_factors.begin(), mu_factors.end());

            sim.factors.template get<irt::factor_type>(
              lambda_id) = irt::factor_type::single;
            sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

            const auto throughput_id = [&]() {
                for (const auto sid : sim.selections)
                    if (sim.selections.template get<irt::name_str>(sid) ==
                        "default")
                        return sid;
                return *sim.selections.begin();
            }();

            sim.selections.template get<irt::name_str>(
              throughput_id) = "throughput";
            sim.selections.template get<irt::criteria_type>(
              throughput_id) = irt::criteria_type::max;

            sim.objective.method = irt::optimization_method::simple;
            sim.objective.type   = irt::optimization_type::maximize;
            sim.objective.simple_params.primary = throughput_id;

            sim.file_id = project_file_id;
            ids.component_file_paths[compo_id]
              .file = simulation_component_file_id;

            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(mod.save(ids, fs, compo_id).has_value()));
            });
            return compo_id;
        });

        expect(fatal(mod.fill_components().has_value()));

        // -----------------------------------------------------------
        // LEVEL 4 -- Omega_B: the validated 6-state revision chain,
        //            built at the modelling layer this time.
        // -----------------------------------------------------------
        hsm_component_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "omega-b.irt",
                                 irt::file_type::component_file);
        });

        const auto hsm_compo = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_hsm_component();
            auto& compo    = ids.components[compo_id];
            auto& hsm_c    = ids.hsm_components.get(compo.id.hsm_id);
            auto& machine  = hsm_c.machine;

            using var = irt::hierarchical_state_machine::variable;

            machine.constants[0] = 0.1f;  // +epsilon_0
            machine.constants[1] = 1.0f;  // seed lambda_hat (matches sim_compo
                                          // default)
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
            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(mod.save(ids, fs, compo_id).has_value()));
            });
            return compo_id;
        });

        // -----------------------------------------------------------
        // LEVEL 3 -- Psi + Delta, WITH Omega_B feeding x[2] (lambda)
        // -----------------------------------------------------------
        simulation_wrapper_file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, "sim-wrapper.irt",
                                 irt::file_type::component_file);
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
              .integers[irt::simulation_wrapper_tag::run] = ordinal(
              irt::simulation_wrapper::run_type::complete);
            gen.children_parameters[sim_w_id]
              .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

            // Wiring:
            expect(
              fatal(gen.connect(cst_init, { .model = 0 }, sim_w, { .model = 0 })
                      .has_value())); // x[0] init
            expect(
              fatal(gen.connect(cst_run, { .model = 0 }, sim_w, { .model = 1 })
                      .has_value())); // x[1] run

            // Observation dual-wired into hsmw (confirmed pattern).
            expect(
              fatal(gen.connect(cst_obs, { .model = 0 }, hsmw, { .model = 0 })
                      .has_value()));
            expect(
              fatal(gen.connect(cst_obs, { .model = 0 }, hsmw, { .model = 1 })
                      .has_value()));

            // Omega_B's revised belief feeds the lambda factor.
            expect(
              fatal(gen.connect(hsmw, { .model = 0 }, sim_w, { .model = 2 })
                      .has_value())); // x[2] lambda

            // Outputs: throughput (selection) and winning lambda (factor).
            expect(fatal(
              gen
                .connect(sim_w, { .model = 0 }, best_throughput, { .model = 0 })
                .has_value()));
            expect(fatal(
              gen.connect(sim_w, { .model = 1 }, win_lambda, { .model = 0 })
                .has_value()));

            ids.component_file_paths[compo_id]
              .file = simulation_wrapper_file_id;
            mod.files.read([&](const auto& fs, auto) {
                expect(fatal(mod.save(ids, fs, compo_id).has_value()));
            });
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
        expect(pj.simulation_init(mod).has_value());

        do {
            expect(pj.sim.run().has_value());
        } while (not pj.sim.current_time_expired());

        expect(pj.sim.finalize().has_value());

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::counter) {
                const auto& cpt = irt::get_dyn<irt::counter>(mdl);
                fmt::print("counter[{:6}]: events={:6} last={:8}\n",
                           irt::get_index(pj.sim.models.get_id(mdl)),
                           cpt.event_number, cpt.last_value);
                // win_lambda: expect last close to 4.0 if the full chain
                // (observation -> Omega_B revision -> x[2] injection ->
                // Delta using the REVISED belief) works end-to-end.
                // best_throughput: expect a trajectory consistent with
                // period=4.0, not the modelling-time default period=1.0.
            }
        }
    };

    "omega_b_revision_cycle"_test = [] {
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);
        expect(sim.srcs.constant_sources.can_alloc(2u) >> fatal);

        // Three observations, driving all three logical branches of the
        // revision chain in a single run:
        //   t=1: obs=1.50 -> error = 1.50 - 1.00 (seed) = +0.50 -> REVISE (+)
        //   t=2: obs=1.52 -> error = 1.52 - 1.50        = +0.02 -> HOLD
        //   t=3: obs=1.00 -> error = 1.00 - 1.50        = -0.50 -> REVISE (-)
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.length = 3;
        cst_value.buffer = { 1.50, 1.52, 1.00 };

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.length = 3;
        cst_ta.buffer = { 1.0, 1.0, 1.0 };

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen = sim.alloc<irt::generator>();
        get_p(sim, gen)
          .clear()
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta))
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value));

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();
        using var = irt::hierarchical_state_machine::variable;

        hsm.constants[0] = 0.1f;  // +epsilon_0
        hsm.constants[1] = 1.0f;  // seed lambda_hat
        hsm.constants[2] = -0.1f; // -epsilon_0

        // State 0 (top): seed the belief on entry.
        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));
        hsm.states[0u].enter_action.set_affect(var::var_r1,
                                               var::hsm_constant_1);

        // State 1: wait for an observation, dual-wired on port_0 (presence)
        // and port_1 (value).
        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        // State 2: error = observed(port_1) - belief(var_r1).
        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_affect(var::var_r2, var::port_1);
        hsm.states[2u].exit_action.set_minus(var::var_r2, var::var_r1);
        hsm.states[2u].if_transition = 3u;

        // State 3: positive branch, error > +epsilon_0.
        expect(!!hsm.set_state(3u, 0u));
        hsm.states[3u].condition.set_greater(var::var_r2, var::hsm_constant_0);
        hsm.states[3u].if_transition   = 5u;
        hsm.states[3u].else_transition = 4u;

        // State 4: negative branch, error < -epsilon_0.
        expect(!!hsm.set_state(4u, 0u));
        hsm.states[4u].condition.set_less(var::var_r2, var::hsm_constant_2);
        hsm.states[4u].if_transition   = 5u;
        hsm.states[4u].else_transition = 1u; // hold: back to waiting

        // State 5: shared revision -- emit, return to waiting.
        expect(!!hsm.set_state(5u, 0u));
        hsm.states[5u].enter_action.set_plus(var::var_r1, var::var_r2);
        hsm.states[5u].exit_action.set_output(var::port_0, var::var_r1);
        hsm.states[5u].if_transition = 1u;

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 4);

        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;
        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        fmt::print(
          "omega_b_revision_cycle: events={} last={:.3f} belief={:.3f}\n",
          cnt.event_number, cnt.last_value, hsmw.exec.r1);

        // Two revisions emitted (t=1 positive, t=3 negative); the t=2
        // observation falls under epsilon_0 and correctly holds without
        // emitting. Belief settles back at 1.0.
        expect(eq(cnt.event_number, static_cast<irt::i64>(2)));
        expect(approx(static_cast<double>(cnt.last_value), 1.0, 1e-5));
        expect(approx(static_cast<double>(hsmw.exec.r1), 1.0, 1e-5));
    };
    "simulation_wrapper: the linked factors are read together"_test = [] {
        const irt::real mu[]   = { 0.6, 1.2 };
        const irt::real cost[] = { 0.0, 3.0 };

        // Linked: 2 scenarios (0.6, 0) and (1.2, 3).
        const auto linked = run_supervisor(
          { .mu = mu, .cost = cost, .mu_link = 1u, .cost_link = 1u });
        expect(linked.error == irt::error_code{});
        expect(fatal(eq(linked.scenarios.size(), 2u)));
        for (const auto& s : linked.scenarios)
            expect(s.mu == 0.6 ? s.cost == 0.0 : s.mu == 1.2 and s.cost == 3.0);

        // Without link: the cartesian product, 4 scenarios.
        const auto product = run_supervisor(
          { .mu = mu, .cost = cost, .mu_link = 0u, .cost_link = 0u });
        expect(product.error == irt::error_code{});
        expect(eq(product.scenarios.size(), 4u));

        // Two groups are also combined in a cartesian product.
        const auto groups = run_supervisor(
          { .mu = mu, .cost = cost, .mu_link = 1u, .cost_link = 2u });
        expect(groups.error == irt::error_code{});
        expect(eq(groups.scenarios.size(), 4u));

        // The factors of a group need the same number of values. The error is
        // expected: no breakpoint.
        const auto callback           = irt::on_error_callback;
        irt::on_error_callback        = nullptr;
        const irt::real three_costs[] = { 0.0, 3.0, 5.0 };
        const auto      mismatch      = run_supervisor(
          { .mu = mu, .cost = three_costs, .mu_link = 1u, .cost_link = 1u });
        irt::on_error_callback = callback;
        expect(mismatch.error ==
               irt::error_code(
                 irt::simulation_errc::simulation_wrapper_factor_link_error));
        expect(mismatch.scenarios.empty());
    };
    "embedded_model_observation: time mean of the raw samples"_test = [] {
        irt::embedded_simulation::embedded_model_observation o;
        o.reset(0.0);

        // A sample is the value since the previous one: 2 on ]0, 1], 4 on
        // ]1, 3] and 1 on ]3, 4].
        const irt::raw_sample raws[] = { { 1.0, 2.0 },
                                         { 3.0, 4.0 },
                                         { 4.0, 1.0 } };
        o.integrate(std::span(raws, 2));
        o.integrate(std::span(raws + 2, 1));

        expect(eq(o.compute_result(irt::criteria_type::time_mean),
                  (2.0 * 1 + 4.0 * 2 + 1.0 * 1) / 4.0));

        o.reset(10.0);
        expect(std::isnan(o.compute_result(irt::criteria_type::time_mean)));
    };

    "simulation_wrapper: the DEVS-DAMA supervisor of a M/M/1 queue"_test = [] {
        const irt::real mu[]   = { 0.6, 1.2 };
        const irt::real cost[] = { 0.0, 3.0 };

        const auto utility = [](const supervisor_scenario& s) {
            return 0.6 * s.queue_criteria + 0.4 * s.cost_criteria;
        };

        const auto run = [&](const irt::real      lambda,
                             const irt::norm_type norm) {
            const auto r = run_supervisor(
              { .mu       = mu,
                .cost     = cost,
                .lambda   = lambda,
                .horizon  = 2e5,
                .criteria = irt::criteria_type::time_mean,
                .norm     = norm });

            expect(r.error == irt::error_code{});
            expect(eq(r.messages, 1));
            expect(eq(r.scenarios.size(), 2u));

            fmt::print("supervisor lambda={} norm={}: selected mu={}\n", lambda,
                       norm == irt::norm_type::none ? "none" : "min-max",
                       r.selected_mu);
            for (const auto& s : r.scenarios)
                fmt::print("  mu={} cost={} L={:.4f} U={:.4f}\n", s.mu,
                           s.cost_criteria, s.queue_criteria, utility(s));

            return r;
        };

        // Stationary values with lambda = 0.4: U1 = 0.6 * 2 = 1.2 and
        // U2 = 0.6 * 0.5 + 0.4 * 3 = 1.5.
        const auto nominal = run(0.4, irt::norm_type::none);
        expect(eq(nominal.selected_mu, 0.6));
        for (const auto& s : nominal.scenarios) {
            const auto expected = s.mu == 0.6 ? 1.2 : 1.5;
            expect(std::abs(utility(s) - expected) <= 0.1 * expected);
        }

        // mu1 < lambda: L grows with the horizon.
        const auto burst = run(0.8, irt::norm_type::none);
        expect(eq(burst.selected_mu, 1.2));
        for (const auto& s : burst.scenarios)
            if (s.mu == 0.6)
                expect(s.queue_criteria > 100.0);

        // With two scenarios, min-max gives 0 and 1 to each criteria: the
        // weights decide alone and mu2 wins with lambda = 0.4.
        const auto min_max = run(0.4, irt::norm_type::min_max);
        expect(eq(min_max.selected_mu, 1.2));
    };
    "simulation_wrapper: the replications use common random numbers"_test = [] {
        const irt::real mu[]   = { 0.6, 1.2 };
        const irt::real cost[] = { 0.0, 3.0 };
        constexpr auto  R      = 3u;

        const auto r = run_supervisor(
          { .mu           = mu,
            .cost         = cost,
            .lambda       = 0.4,
            .horizon      = 200,
            .criteria     = irt::criteria_type::time_mean,
            .norm         = irt::norm_type::none,
            .replications = R,
            .seed         = 42u });

        expect(r.error == irt::error_code{});
        expect(fatal(eq(r.scenarios.size(), 2u * R)));
        expect(fatal(eq(r.results.size(), 2u)));

        // The sub-simulation k is the replication k % R of the scenario
        // k / R: same seed and same arrivals for the same replication.
        for (std::size_t k = 0; k < r.scenarios.size(); ++k) {
            const auto& s     = r.scenarios[k];
            const auto& first = r.scenarios[k % R];

            expect(eq(s.mu, mu[k / R]));
            expect(eq(s.cost, cost[k / R]));
            expect(eq(s.seed, first.seed));
            expect(eq(s.arrivals, first.arrivals));
        }

        expect(neq(r.scenarios[0].seed, r.scenarios[1].seed));
        expect(neq(r.scenarios[1].seed, r.scenarios[2].seed));
        expect(neq(r.scenarios[0].arrivals, r.scenarios[1].arrivals));

        // The means and standard deviations of the wrapper are those of the
        // replications. The objective of a weighted sum is the score to
        // maximize: -U for two criteria to minimize.
        for (std::size_t sc = 0; sc < 2; ++sc) {
            double q = 0, c = 0, u = 0;
            for (std::size_t i = 0; i < R; ++i) {
                const auto& s = r.scenarios[sc * R + i];
                q += s.queue_criteria / R;
                c += s.cost_criteria / R;
                u += (0.6 * s.queue_criteria + 0.4 * s.cost_criteria) / R;
            }

            double sq_q = 0, sq_u = 0;
            for (std::size_t i = 0; i < R; ++i) {
                const auto& s  = r.scenarios[sc * R + i];
                const auto  ui = 0.6 * s.queue_criteria + 0.4 * s.cost_criteria;
                sq_q += (s.queue_criteria - q) * (s.queue_criteria - q);
                sq_u += (ui - u) * (ui - u);
            }

            const auto& res = r.results[sc];
            expect(fatal(eq(res.means.size(), 2u)));

            // The selections are in the order of the observables: the
            // queue then the cost.
            const auto queue_j = res.means[0] == c ? 1u : 0u;
            const auto cost_j  = 1u - queue_j;

            expect(approx(res.means[queue_j], q, 1e-9));
            expect(approx(res.means[cost_j], c, 1e-9));
            expect(
              approx(res.stddevs[queue_j], std::sqrt(sq_q / (R - 1)), 1e-9));
            expect(approx(res.stddevs[cost_j], 0.0, 1e-12));
            expect(approx(res.objective_mean, -u, 1e-9));
            expect(
              approx(res.objective_stddev, std::sqrt(sq_u / (R - 1)), 1e-9));
        }

        expect(eq(r.selected_mu, mu[r.best_scenario]));
    };

    "simulation_wrapper: the supervisor with 30 replications"_test = [] {
        const irt::real mu[]   = { 0.6, 1.2 };
        const irt::real cost[] = { 0.0, 3.0 };

        const auto r = run_supervisor(
          { .mu           = mu,
            .cost         = cost,
            .lambda       = 0.4,
            .horizon      = 2e4,
            .criteria     = irt::criteria_type::time_mean,
            .norm         = irt::norm_type::none,
            .replications = 30u });

        expect(r.error == irt::error_code{});
        expect(fatal(eq(r.results.size(), 2u)));
        expect(eq(r.selected_mu, 0.6));

        fmt::print("supervisor lambda=0.4 T=2e4 R=30 ({:.0f} ms):\n",
                   r.decision_ms);
        for (std::size_t sc = 0; sc < 2; ++sc)
            fmt::print("  mu={} U={:.4f} sd={:.4f}\n", mu[sc],
                       -r.results[sc].objective_mean,
                       r.results[sc].objective_stddev);
    };
}
