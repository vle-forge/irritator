// Skeleton unit test for a DEVS-DAMA instantiation in Irritator.
//
//   Level 1 (M_sim)        : generator("lambda") -> queue("mu") ->
//   counter("throughput") Level 2 (plan)         : sim_compo -- factors,
//   selections, objective Level 3 (Psi + Delta)  : sim_wrapper_compo --
//   simulation_wrapper Level 4 (Omega_B)      : hsm_wrapper -- sketched
//
// -----------------------------------------------------------------------
// DESIGN NOTE -- why deterministic delays, not exponential ones
// -----------------------------------------------------------------------
// `embedded_sims_copy_parameters` (simulation.cpp) only overwrites
// `parameters[mdl_idx].reals[0]` of the model targeted by a factor.
// A `generator`/`dynamic_queue` driven by a `random_source` keeps its
// rate in `external_source_definition::random_source.reals[0]`, a
// SEPARATE object -- the factor mechanism cannot reach it directly.
//
// To keep every construct below grounded in confirmed API, this skeleton
// uses:
//   - `generator` WITHOUT a ta-source: per the documented contract
//     ("sigma is initialized with params.reals[0]") and the transition
//     logic (sigma is only touched inside `if (flags[ta_use_source])`),
//     this yields a *periodic* generator with period == reals[0], which
//     factors CAN reach.
//   - `queue` (not `dynamic_queue`): its `ta` field is set via the
//     confirmed `parameter::set_queue(real)` setter and is a plain model
//     parameter, also reachable by factors.
//
// lambda_hat (the belief about arrival rate) is therefore NOT a `factor`
// of the deliberation plan -- it is theta, updated by Omega_B/Omega_theta
// BETWEEN episodes (Level 4, sketched only). mu (service delay) IS a
// factor -- it is the deliberated action set {mu_1, mu_2} that Delta
// chooses over.
//
// Everything in Levels 1-3 mirrors patterns already exercised by
// Irritator's own test suite (`simulation-component` test in
// mod-to-sim.cpp) and should compile with only nominal adjustment.
// Level 4 (Omega_B via hsm_wrapper) is a sketch: the allocation pattern
// (`gen.alloc(irt::dynamics_type::hsm_wrapper)`) is confirmed, but the
// exact tag enum used to bind an hsm_wrapper to its hsm_id is inferred
// by analogy with `simulation_wrapper_tag` and MUST be checked against
// the real declaration before this compiles.
// -----------------------------------------------------------------------

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

#include <boost/ut.hpp>
#include <fmt/format.h>

using namespace std::literals;

template<std::size_t length>
static bool get_temp_registred_path(irt::small_string<length>& str) noexcept
{
    std::error_code ec;
    try {
        auto p = std::filesystem::temp_directory_path(ec) / "reg-temp-dama";
        str    = p.string();
        return true;
    } catch (...) {
        return false;
    }
}

// =========================================================================
// devs_dama_queue_supervisor_tester
//
// service_delays : candidate values for the "mu" factor (1/mu_1, 1/mu_2),
//                  i.e. the deliberated action set A.
// arrival_period : fixed initial value of theta (1/lambda_hat).
// horizon        : simulation time bound for both the inner model and
//                  the outer wrapper.
// =========================================================================
static void devs_dama_queue_supervisor_tester(
  std::span<const irt::real> service_delays,
  irt::real                  arrival_period,
  irt::i32                   horizon) noexcept
{
    using namespace boost::ut;

    irt::journal_handler jn;
    irt::modeling        mod;

    irt::registred_path_id reg_id{ 0 };
    irt::dir_path_id       dir_id{ 0 };
    irt::file_path_id      gen_component_file_id{ 0 };
    irt::file_path_id      project_file_id{ 0 };
    irt::file_path_id      simulation_component_file_id{ 0 };
    irt::file_path_id      simulation_wrapper_file_id{ 0 };

    mod.files.write([&](auto& fs) {
        irt::registred_path_str temp_path;
        expect(fatal(get_temp_registred_path(temp_path)));

        reg_id                              = fs.alloc_registred("temp", 0);
        fs.registred_paths.get(reg_id).path = temp_path;

        dir_id                          = fs.alloc_dir(reg_id);
        fs.dir_paths.get(dir_id).parent = reg_id;
        fs.dir_paths.get(dir_id).path   = "test-devs-dama";

        fs.create_directories(reg_id);
        fs.create_directories(dir_id);

        expect(fatal(fs.file_paths.can_alloc(3)));

        gen_component_file_id = fs.alloc_file(
          dir_id, "gen-compo.irt", irt::file_type::component_file);
        project_file_id =
          fs.alloc_file(dir_id, "project.pirt", irt::file_type::project_file);
    });

    // ---------------------------------------------------------------
    // LEVEL 1 -- M_sim(theta): generator("lambda") -> queue("mu")
    //            -> counter("throughput")
    // ---------------------------------------------------------------
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

        // Periodic generator, no external source: period == reals[0].
        gen.children_parameters[arrivals_id].set_generator_ta(arrival_period);
        gen.children_parameters[arrivals_id].reals[1] = 1.0; // arrival token

        // Deterministic service delay; overwritten per-branch by the
        // "mu" factor of sim_compo (Level 2).
        gen.children_parameters[service_id].set_queue(service_delays[0]);

        gen.connect(arrivals,
                    irt::connection::port{ .model = 0 },
                    service,
                    irt::connection::port{ .model = 0 });

        gen.connect(service,
                    irt::connection::port{ .model = 0 },
                    throughput,
                    irt::connection::port{ .model = 0 });

        ids.component_file_paths[compo_id].file = gen_component_file_id;

        mod.files.read(
          [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });

        return compo_id;
    });

    mod.ids.write([&](auto& ids) {
        mod.files.write([&](auto& fs) {
            irt::project pj;
            pj.file = project_file_id;
            pj.sim.limits.set_bound(0, horizon);

            expect(pj.set(ids, fs, gen_compo).has_value());
            expect(pj.save(fs, ids).has_value());
        });
    });

    expect(fatal(mod.fill_components().has_value()));

    // ---------------------------------------------------------------
    // LEVEL 2 -- the plan: factors, selection, objective
    //   factor "mu" = A = {mu_1, mu_2}   (deliberated action set)
    //   factor "lambda" left at its single default value = theta
    //   selection "throughput", criteria_type::max
    //   objective: simple / maximize / primary = throughput
    // ---------------------------------------------------------------

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

        expect(fatal(eq(sim.factors.size(), 2u))); // "lambda", "mu"

        auto mu_id     = irt::factor_id{ 0 };
        auto lambda_id = irt::factor_id{ 0 };

        const auto& names = sim.factors.template get<irt::name_str>();
        for (const auto fid : sim.factors) {
            if (names[fid] == "mu")
                mu_id = fid;

            if (names[fid] == "lambda")
                lambda_id = fid;
        }

        // "mu": the deliberated action set A = {mu_1, mu_2}.
        sim.factors.template get<irt::factor_type>(mu_id) =
          irt::factor_type::fixed;
        sim.factors.template get<irt::fixed_factor>(mu_id).values.assign(
          service_delays.begin(), service_delays.end());

        sim.factors.template get<irt::factor_type>(lambda_id) =
          irt::factor_type::single;
        sim.factors.template get<irt::single_factor>(lambda_id).value = 1.0;

        // "lambda" is intentionally left at factor_type::single (its
        // default): it represents theta, not an element of A. Omega_B /
        // Omega_theta (Level 4) update it between episodes by rewriting
        // sim.factors directly at the modelling layer, not by adding it
        // to the Cartesian product evaluated within a single episode.

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

        sim.objective.method                = irt::optimization_method::simple;
        sim.objective.type                  = irt::optimization_type::maximize;
        sim.objective.simple_params.primary = throughput_id;

        sim.file_id                             = project_file_id;
        ids.component_file_paths[compo_id].file = simulation_component_file_id;

        mod.files.read(
          [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });

        return compo_id;
    });

    expect(fatal(mod.fill_components().has_value()));

    // ---------------------------------------------------------------
    // LEVEL 3 -- Psi + Delta: simulation_wrapper
    //   y[0] = best throughput (the single selection)
    //   y[1] = best mu value   (the single factor)
    // ---------------------------------------------------------------

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
        auto& sim_w    = gen.alloc(irt::dynamics_type::simulation_wrapper);

        auto& best_throughput = gen.alloc(irt::dynamics_type::counter);
        auto& best_mu         = gen.alloc(irt::dynamics_type::counter);
        auto& win_mu          = gen.alloc(irt::dynamics_type::counter);

        cst_init.flags        = irt::child_flags::configurable;
        cst_run.flags         = irt::child_flags::configurable;
        best_throughput.flags = irt::child_flags::observable;
        best_mu.flags         = irt::child_flags::observable;
        win_mu.flags          = irt::child_flags::observable;

        const auto cst_init_id = gen.children.get_id(cst_init);
        const auto cst_run_id  = gen.children.get_id(cst_run);
        const auto sim_w_id    = gen.children.get_id(sim_w);

        gen.children_names[cst_init_id] = "init";
        gen.children_names[cst_run_id]  = "run";
        gen.children_names[sim_w_id]    = "psi-delta";

        gen.children_parameters[cst_init_id].set_constant(0.0, 0.0);
        gen.children_parameters[cst_run_id].set_constant(0.0, 1.0);

        // pi = reset / step(k): run_type::complete runs each branch of
        // T_k to completion within the horizon set on the embedded
        // simulation (inherited from gen_compo's project bound above).
        gen.children_parameters[sim_w_id]
          .integers[irt::simulation_wrapper_tag::run] =
          ordinal(irt::simulation_wrapper::run_type::complete);
        gen.children_parameters[sim_w_id]
          .integers[irt::simulation_wrapper_tag::id] = ordinal(sim_compo);

        gen.connect(cst_init,
                    irt::connection::port{ .model = 0 },
                    sim_w,
                    irt::connection::port{ .model = 0 });

        gen.connect(cst_run,
                    irt::connection::port{ .model = 0 },
                    sim_w,
                    irt::connection::port{ .model = 1 });

        // Output ordering per `send()` in simulation.cpp: selections
        // first, then factors. One selection ("throughput") + one
        // factor ("mu") => y[0] = throughput, y[1] = mu.
        gen.connect(sim_w,
                    irt::connection::port{ .model = 0 },
                    best_throughput,
                    irt::connection::port{ .model = 0 });

        gen.connect(sim_w,
                    irt::connection::port{ .model = 1 },
                    best_mu,
                    irt::connection::port{ .model = 0 });

        gen.connect(sim_w,
                    irt::connection::port{ .model = 2 },
                    win_mu,
                    irt::connection::port{ .model = 0 });

        ids.component_file_paths[compo_id].file = simulation_wrapper_file_id;

        mod.files.read(
          [&](const auto& fs, auto) { mod.save(ids, fs, compo_id); });

        return compo_id;
    });

    // ---------------------------------------------------------------
    // Run and check
    // ---------------------------------------------------------------
    irt::project pj;

    mod.ids.read([&](const auto& ids, auto) {
        mod.files.read([&](const auto& fs, auto) {
            expect(fatal(pj.set(ids, fs, sim_wrapper_compo).has_value()));
        });
    });

    pj.sim.limits.set_bound(0, horizon + 1);
    expect(pj.simulation_initialize().has_value());

    auto sim_w_mdl_id = irt::undefined<irt::model_id>();
    auto cpts         = irt::small_vector<irt::model_id, 8>{};

    for (const auto& mdl : pj.sim.models) {
        const auto mdl_id = pj.sim.models.get_id(mdl);

        if (mdl.type == irt::dynamics_type::simulation_wrapper) {
            sim_w_mdl_id = mdl_id;

            const auto& sim_w = irt::get_dyn<irt::simulation_wrapper>(mdl);
            expect(eq(length(sim_w.x), 4)); // init, run, + no forced factors
            expect(eq(length(sim_w.y), 3)); // throughput, mu
        } else if (mdl.type == irt::dynamics_type::counter) {
            cpts.push_back(mdl_id);
        }
    }

    expect(fatal(eq(cpts.size(), 3u)));
    expect(irt::is_defined(sim_w_mdl_id));

    do {
        expect(pj.simulation_run_bag().has_value());
    } while (not pj.sim.current_time_expired());

    expect(pj.sim.finalize().has_value());

    const auto result = std::array{ 1.0, 4.0, 1.0, 1.0, 1.0, 2.0 };
    auto       i      = 0u;

    for (const auto mdl_id : cpts) {
        const auto& mdl = pj.sim.models.get(mdl_id);
        const auto& cpt = irt::get_dyn<irt::counter>(mdl);

        fmt::print("counter[{:6}]: events={:6} last={:8}\n",
                   irt::get_index(mdl_id),
                   cpt.event_number,
                   cpt.last_value);

        expect(approx(cpt.event_number, result[i * 2u], 1e-5));
        expect(approx(cpt.last_value, result[i * 2u + 1u], 1e-5));

        // TODO: replace with `expect(approx(..., ..., 1e-5))` once the
        // expected (event_number, last_value) pairs are known from a
        // first successful run -- see simulation_component_tester in
        // mod-to-sim.cpp for the exact pattern.

        ++i;
    }
}

// =========================================================================
// LEVEL 4 (sketch, NOT verified) -- Omega_B via hsm_wrapper
// =========================================================================
//
// The allocation pattern itself is confirmed:
//
//   auto& belief_revision = gen.alloc(irt::dynamics_type::hsm_wrapper);
//   const auto br_id = gen.children.get_id(belief_revision);
//
// Binding this hsm_wrapper to a hierarchical_state_machine requires an
// hsm_component allocated independently (mirroring how sim_wrapper
// references sim_compo via `simulation_wrapper_tag::id`):
//
//   const auto hsm_compo = mod.ids.write([&](auto& ids) {
//       auto  compo_id = ids.alloc_hsm_component();
//       auto& compo    = ids.components[compo_id];
//       auto& hsm      = ids.hsm_components.get(compo.id.hsm_id);
//
//       // State 0 (top): wait for an observation on port_0.
//       expect(!!hsm.machine.set_state(
//           0u, irt::hierarchical_state_machine::invalid_state_id, 1u));
//
//       // State 1: compute error = port_0 - hsm_constant_0 (belief),
//       // transition unconditionally (condition_type::none) to state 2.
//       expect(!!hsm.machine.set_state(1u, 0u));
//       hsm.machine.states[1u].enter_action.set_affect(
//           irt::hierarchical_state_machine::variable::var_r1,
//           irt::hierarchical_state_machine::variable::port_0);
//       hsm.machine.states[1u].exit_action.set_minus(
//           irt::hierarchical_state_machine::variable::var_r1,
//           irt::hierarchical_state_machine::variable::hsm_constant_0);
//       hsm.machine.states[1u].if_transition = 2u;
//
//       // State 2: test |error| > epsilon_0 (two branches for sign),
//       // revise hsm_constant_0 and emit on port_0 if exceeded.
//       expect(!!hsm.machine.set_state(2u, 0u));
//       hsm.machine.states[2u].condition.set_greater(
//           irt::hierarchical_state_machine::variable::var_r1,
//           irt::hierarchical_state_machine::variable::hsm_constant_1); //
//           epsilon_0
//       // if_action / else_action / if_transition / else_transition:
//       // TODO(verify): exact wiring for the "revise" vs "hold" branches.
//
//       return compo_id;
//   });
//
//   // Inside sim_wrapper_compo's generic component:
//   auto& belief_revision = gen.alloc(irt::dynamics_type::hsm_wrapper);
//   const auto br_id = gen.children.get_id(belief_revision);
//   gen.children_parameters[br_id].integers[/* TODO: hsm_wrapper_tag::id,
//       name unconfirmed -- check declaration near simulation_wrapper_tag
//       */] = ordinal(hsm_compo);
//
// The two points marked TODO above -- the exact tag enum for binding
// hsm_wrapper to an hsm_id, and the if/else wiring for a two-branch
// numeric threshold test -- are the only unconfirmed pieces of this
// entire skeleton. Everything else (Levels 1-3) follows patterns
// directly exercised by Irritator's own test suite.

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    using namespace boost::ut;

    "devs-dama-queue-supervisor-mu1"_test = [] {
        devs_dama_queue_supervisor_tester(
          std::array{ 2.0 }, // mu_1: service delay 2.0 (single branch)
          1.0,               // lambda_hat: arrival period 1.0
          20);
    };

    "devs-dama-queue-supervisor-two-modes"_test = [] {
        devs_dama_queue_supervisor_tester(
          std::array{ 2.0, 0.8 }, // {mu_1, mu_2}: Normal vs Turbo delay
          1.0,
          20);
    };
}
