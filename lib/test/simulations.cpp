// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <fmt/format.h>

#include <cstdio>

#include <boost/ut.hpp>

struct neuron {
    irt::model_id sum;
    irt::model_id prod;
    irt::model_id integrator;
    irt::model_id constant;
    irt::model_id cross;
    irt::model_id constant_cross;
};

struct synapse {
    irt::model_id sum_pre;
    irt::model_id prod_pre;
    irt::model_id integrator_pre;
    irt::model_id cross_pre;

    irt::model_id sum_post;
    irt::model_id prod_post;
    irt::model_id integrator_post;
    irt::model_id cross_post;

    irt::model_id constant_syn;
    irt::model_id accumulator_syn;
};

struct neuron make_neuron(irt::simulation* sim) noexcept
{
    using namespace boost::ut;
    irt::real tau_lif =
      irt::one + static_cast<irt::real>(rand()) /
                   (static_cast<irt::real>(RAND_MAX) / (irt::two - irt::one));
    irt::real Vr_lif = irt::zero;
    irt::real Vt_lif = irt::one;

    auto& sum_lif            = sim->alloc<irt::qss3_wsum_2>();
    auto& prod_lif           = sim->alloc<irt::qss3_wsum_2>();
    auto& integrator_lif     = sim->alloc<irt::qss3_integrator>();
    auto& constant_lif       = sim->alloc<irt::constant>();
    auto& constant_cross_lif = sim->alloc<irt::constant>();
    auto& cross_lif          = sim->alloc<irt::qss3_cross>();

    struct neuron neuron_model = {
        sim->get_id(sum_lif),        sim->get_id(prod_lif),
        sim->get_id(integrator_lif), sim->get_id(constant_lif),
        sim->get_id(cross_lif),      sim->get_id(constant_cross_lif),
    };

    {
        auto& p    = sim->parameters[sim->get_id(sum_lif)];
        p.reals[2] = -irt::one;
        p.reals[3] = irt::two * Vt_lif;
    }
    {
        auto& p    = sim->parameters[sim->get_id(prod_lif)];
        p.reals[2] = irt::one / tau_lif;
        p.reals[3] = irt::zero;
    }
    {
        auto& p    = sim->parameters[sim->get_id(constant_lif)];
        p.reals[0] = irt::one;
    }
    {
        auto& p    = sim->parameters[sim->get_id(constant_cross_lif)];
        p.reals[0] = Vr_lif;
    }
    {
        auto& p    = sim->parameters[sim->get_id(cross_lif)];
        p.reals[0] = Vt_lif;
    }

    // Connections
    expect(!!sim->connect_dynamics(prod_lif, 0, integrator_lif, 0));
    expect(!!sim->connect_dynamics(cross_lif, 0, integrator_lif, 1));
    expect(!!sim->connect_dynamics(cross_lif, 0, sum_lif, 0));
    expect(!!sim->connect_dynamics(integrator_lif, 0, cross_lif, 0));
    expect(!!sim->connect_dynamics(integrator_lif, 0, cross_lif, 2));
    expect(!!sim->connect_dynamics(constant_cross_lif, 0, cross_lif, 1));
    expect(!!sim->connect_dynamics(constant_lif, 0, sum_lif, 1));
    expect(!!sim->connect_dynamics(sum_lif, 0, prod_lif, 0));
    expect(!!sim->connect_dynamics(constant_lif, 0, prod_lif, 1));

    return neuron_model;
}

template<typename Dynamics>
irt::parameter& get_p(irt::simulation* sim, const Dynamics& d) noexcept
{
    return sim->parameters[sim->get_id(d)];
}

struct synapse make_synapse(irt::simulation* sim,
                            long unsigned int /* source */,
                            long unsigned int /* target */,
                            irt::model& presynaptic_model,
                            int         presynaptic_port,
                            irt::model& postsynaptic_model,
                            int         postsynaptic_port)
{
    using namespace boost::ut;
    irt::real taupre(20);
    irt::real taupost = taupre;
    irt::real gamax   = irt::to_real(0.015);
    irt::real dApre   = irt::to_real(0.01);
    irt::real dApost  = -dApre * taupre / taupost * irt::to_real(1.05);
    dApost            = dApost * gamax;
    dApre             = dApre * gamax;

    auto& int_pre   = sim->alloc<irt::qss3_integrator>();
    auto& sum_pre   = sim->alloc<irt::qss3_wsum_2>();
    auto& mult_pre  = sim->alloc<irt::qss3_wsum_2>();
    auto& cross_pre = sim->alloc<irt::qss3_cross>();

    auto& int_post   = sim->alloc<irt::qss3_integrator>();
    auto& sum_post   = sim->alloc<irt::qss3_wsum_2>();
    auto& mult_post  = sim->alloc<irt::qss3_wsum_2>();
    auto& cross_post = sim->alloc<irt::qss3_cross>();

    auto& const_syn       = sim->alloc<irt::constant>();
    auto& accumulator_syn = sim->alloc<irt::accumulator_2>();

    get_p(sim, cross_pre).reals[0] = irt::one;
    get_p(sim, sum_pre).reals[2]   = irt::one;
    get_p(sim, sum_pre).reals[3]   = dApre;
    get_p(sim, mult_pre).reals[2]  = -irt::one / taupre;
    get_p(sim, mult_pre).reals[3]  = irt::zero;

    get_p(sim, cross_post).reals[0] = irt::one;
    get_p(sim, sum_post).reals[2]   = irt::one;
    get_p(sim, sum_post).reals[3]   = dApost;
    get_p(sim, mult_post).reals[2]  = -irt::one / taupost;
    get_p(sim, mult_post).reals[3]  = irt::zero;

    get_p(sim, const_syn).reals[0] = irt::one;

    struct synapse synapse_model = {
        sim->get_id(sum_pre),   sim->get_id(mult_pre),
        sim->get_id(int_pre),   sim->get_id(cross_pre),

        sim->get_id(sum_post),  sim->get_id(mult_post),
        sim->get_id(int_post),  sim->get_id(cross_post),

        sim->get_id(const_syn), sim->get_id(accumulator_syn),
    };

    // Connections
    expect(!!sim->connect_dynamics(cross_pre, 0, int_pre, 0));
    expect(!!sim->connect_dynamics(mult_pre, 0, int_pre, 0));
    expect(!!sim->connect_dynamics(cross_pre, 0, int_pre, 1));
    expect(!!sim->connect_dynamics(int_pre, 0, cross_pre, 2));
    expect(!!sim->connect_dynamics(cross_pre, 0, mult_pre, 0));
    expect(!!sim->connect_dynamics(const_syn, 0, mult_pre, 1));
    expect(!!sim->connect_dynamics(int_pre, 0, sum_pre, 0));
    expect(!!sim->connect_dynamics(const_syn, 0, sum_pre, 1));
    expect(!!sim->connect_dynamics(sum_pre, 0, cross_pre, 1));
    expect(!!sim->connect(
      presynaptic_model, presynaptic_port, irt::get_model(cross_pre), 0))
      << fatal;

    expect(!!sim->connect_dynamics(cross_post, 0, int_post, 0));
    expect(!!sim->connect_dynamics(mult_post, 0, int_post, 0));
    expect(!!sim->connect_dynamics(cross_post, 0, int_post, 1));
    expect(!!sim->connect_dynamics(int_post, 0, cross_post, 2));
    expect(!!sim->connect_dynamics(cross_post, 0, mult_post, 0));
    expect(!!sim->connect_dynamics(const_syn, 0, mult_post, 1));
    expect(!!sim->connect_dynamics(int_post, 0, sum_post, 0));
    expect(!!sim->connect_dynamics(const_syn, 0, sum_post, 1));
    expect(!!sim->connect_dynamics(sum_post, 0, cross_post, 1));
    expect(!!sim->connect(
      postsynaptic_model, postsynaptic_port, irt::get_model(cross_post), 0));

    expect(!!sim->connect(
      presynaptic_model, presynaptic_port, irt::get_model(accumulator_syn), 0));
    expect(!!sim->connect(postsynaptic_model,
                          postsynaptic_port,
                          irt::get_model(accumulator_syn),
                          1));
    expect(!!sim->connect_dynamics(cross_post, 0, accumulator_syn, 2));
    expect(!!sim->connect_dynamics(cross_pre, 0, accumulator_syn, 3));

    return synapse_model;
}

int main()
{
    using namespace boost::ut;

    "song_1_simulation"_test = [] {
#if 0
        const int N = 4;

        irt::simulation sim(irt::simulation_reserve_definition{
          .models         = N * N * 8,
          .connections    = N * N * N,
          .hsms           = 0,
          .dated_messages = 0,
        });

        expect(sim.can_alloc(N + 2u * N * N + 2u * N * N + 4u * N * N + N +
                             2u * N * N));

        std::vector<struct neuron> neurons;
        for (int i = 0; i < N; i++) {
            struct neuron neuron_model = make_neuron(&sim);
            neurons.emplace_back(neuron_model);
        }

        std::vector<struct synapse> synapses;
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                struct synapse synapse_model =
                  make_synapse(&sim,
                               i,
                               j,
                               sim.models.get(neurons[i].cross),
                               1,
                               sim.models.get(neurons[j].cross),
                               1);
                synapses.emplace_back(synapse_model);
            }
        }

        sim.limits.set_bound(0, 30);
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (not sim.current_time_expired());
    };
#endif
    };

    "bouncing-ball"_test = [] {
        irt::simulation sim;

        expect(fatal(sim.can_alloc(28)));
        expect(fatal(sim.hsms.can_alloc(3)));

        auto& mdl_0 = sim.alloc<irt::qss3_integrator>();
        sim.parameters[sim.get_id(mdl_0)].reals = {
            0.575, 0.0100000000000000, 0.00000000000000000, 0.00000000000000000
        };

        auto& mdl_1 = sim.alloc<irt::qss3_integrator>();
        sim.parameters[sim.get_id(mdl_1)].reals = { 0.50000000000000000,
                                                    0.01000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_2                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_2)].reals = { -0.10000000000000001,
                                                    0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_3 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_3)].reals = { 0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_4 = sim.alloc<irt::qss3_integrator>();
        sim.parameters[sim.get_id(mdl_4)].reals = { 0.00000000000000000,
                                                    0.01000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_5 = sim.alloc<irt::qss3_integrator>();
        sim.parameters[sim.get_id(mdl_5)].reals = { 10.50000000000000000,
                                                    0.00010000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_6 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_6)].reals = { 0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_7                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_7)].reals = { 9.81000000000000050,
                                                    0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_8                             = sim.alloc<irt::qss3_wsum_3>();
        sim.parameters[sim.get_id(mdl_8)].reals = { -1.00000000000000000,
                                                    1.00000000000000000,
                                                    -1.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_9 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_9)].reals = { 0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000,
                                                    0.00000000000000000 };

        auto& mdl_10                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_10)].reals = { 30.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_11                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_11)].reals = { 100000.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_12 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_12)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_13 = sim.alloc<irt::qss3_wsum_2>();
        sim.parameters[sim.get_id(mdl_13)].reals = { 1.00000000000000000,
                                                     -1.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_14 = sim.alloc<irt::qss3_wsum_2>();
        sim.parameters[sim.get_id(mdl_14)].reals = { 1.00000000000000000,
                                                     -1.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_15                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_15)].reals = { 11.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_16 = sim.alloc<irt::qss3_integer>();
        sim.parameters[sim.get_id(mdl_16)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_17                             = sim.alloc<irt::qss3_sum_2>();
        sim.parameters[sim.get_id(mdl_17)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_18                             = sim.alloc<irt::qss3_cross>();
        sim.parameters[sim.get_id(mdl_18)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_19 = sim.alloc<irt::qss3_flipflop>();
        sim.parameters[sim.get_id(mdl_19)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_20                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_20)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_21                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_21)].reals = { 1.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_22 = sim.alloc<irt::qss3_flipflop>();
        sim.parameters[sim.get_id(mdl_22)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_23 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_23)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_24                             = sim.alloc<irt::constant>();
        sim.parameters[sim.get_id(mdl_24)].reals = { 1.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_25 = sim.alloc<irt::qss3_multiplier>();
        sim.parameters[sim.get_id(mdl_25)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_26                             = sim.alloc<irt::counter>();
        sim.parameters[sim.get_id(mdl_26)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        auto& mdl_27                             = sim.alloc<irt::counter>();
        sim.parameters[sim.get_id(mdl_27)].reals = { 0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000,
                                                     0.00000000000000000 };

        expect(sim.connect_dynamics(mdl_0, 0, mdl_14, 1).has_value());
        expect(sim.connect_dynamics(mdl_1, 0, mdl_0, 0).has_value());
        expect(sim.connect_dynamics(mdl_1, 0, mdl_3, 1).has_value());
        expect(sim.connect_dynamics(mdl_2, 0, mdl_3, 0).has_value());
        expect(sim.connect_dynamics(mdl_2, 0, mdl_6, 0).has_value());
        expect(sim.connect_dynamics(mdl_3, 0, mdl_1, 0).has_value());
        expect(sim.connect_dynamics(mdl_4, 0, mdl_5, 0).has_value());
        expect(sim.connect_dynamics(mdl_4, 0, mdl_6, 1).has_value());
        expect(sim.connect_dynamics(mdl_4, 0, mdl_9, 1).has_value());
        expect(sim.connect_dynamics(mdl_5, 0, mdl_13, 0).has_value());
        expect(sim.connect_dynamics(mdl_6, 0, mdl_8, 1).has_value());
        expect(sim.connect_dynamics(mdl_7, 0, mdl_8, 0).has_value());
        expect(sim.connect_dynamics(mdl_8, 0, mdl_4, 0).has_value());
        expect(sim.connect_dynamics(mdl_9, 0, mdl_17, 0).has_value());
        expect(sim.connect_dynamics(mdl_10, 0, mdl_9, 0).has_value());
        expect(sim.connect_dynamics(mdl_11, 0, mdl_12, 0).has_value());
        expect(sim.connect_dynamics(mdl_12, 0, mdl_17, 1).has_value());
        expect(sim.connect_dynamics(mdl_13, 0, mdl_12, 1).has_value());
        expect(sim.connect_dynamics(mdl_13, 0, mdl_18, 0).has_value());
        expect(sim.connect_dynamics(mdl_14, 0, mdl_16, 0).has_value());
        expect(sim.connect_dynamics(mdl_15, 0, mdl_14, 0).has_value());
        expect(sim.connect_dynamics(mdl_16, 0, mdl_13, 1).has_value());
        expect(sim.connect_dynamics(mdl_17, 0, mdl_25, 1).has_value());
        expect(sim.connect_dynamics(mdl_18, 0, mdl_19, 1).has_value());
        expect(sim.connect_dynamics(mdl_18, 1, mdl_22, 1).has_value());
        expect(sim.connect_dynamics(mdl_19, 0, mdl_23, 0).has_value());
        expect(sim.connect_dynamics(mdl_19, 0, mdl_26, 0).has_value());
        expect(sim.connect_dynamics(mdl_20, 0, mdl_19, 0).has_value());
        expect(sim.connect_dynamics(mdl_21, 0, mdl_22, 0).has_value());
        expect(sim.connect_dynamics(mdl_22, 0, mdl_23, 0).has_value());
        expect(sim.connect_dynamics(mdl_22, 0, mdl_27, 0).has_value());
        expect(sim.connect_dynamics(mdl_23, 0, mdl_25, 0).has_value());
        expect(sim.connect_dynamics(mdl_24, 0, mdl_23, 1).has_value());
        expect(sim.connect_dynamics(mdl_25, 0, mdl_8, 2).has_value());
        sim.limits.set_bound(0, 10);
        expect(fatal(sim.srcs.prepare().has_value()));
        expect(fatal(sim.initialize().has_value()));

        do {
            expect(sim.run().has_value());
        } while (not sim.current_time_expired());

        expect(ge(mdl_26.number, static_cast<irt::i64>(42))); // 39
        expect(eq(mdl_26.last_value, 0));

        expect(ge(mdl_27.number, static_cast<irt::i64>(40))); // 39
        expect(eq(mdl_27.last_value, 1));
    };
}
