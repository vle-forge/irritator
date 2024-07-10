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

    sum_lif.default_input_coeffs[0] = -irt::one;
    sum_lif.default_input_coeffs[1] = irt::two * Vt_lif;

    prod_lif.default_input_coeffs[0] = irt::one / tau_lif;
    prod_lif.default_input_coeffs[1] = irt::zero;

    constant_lif.default_value       = irt::one;
    constant_cross_lif.default_value = Vr_lif;

    integrator_lif.default_X = irt::zero;

    cross_lif.default_threshold = Vt_lif;

    struct neuron neuron_model = {
        sim->get_id(sum_lif),        sim->get_id(prod_lif),
        sim->get_id(integrator_lif), sim->get_id(constant_lif),
        sim->get_id(cross_lif),      sim->get_id(constant_cross_lif),
    };

    // Connections
    expect(!!sim->connect(prod_lif, 0, integrator_lif, 0));
    expect(!!sim->connect(cross_lif, 0, integrator_lif, 1));
    expect(!!sim->connect(cross_lif, 0, sum_lif, 0));
    expect(!!sim->connect(integrator_lif, 0, cross_lif, 0));
    expect(!!sim->connect(integrator_lif, 0, cross_lif, 2));
    expect(!!sim->connect(constant_cross_lif, 0, cross_lif, 1));
    expect(!!sim->connect(constant_lif, 0, sum_lif, 1));
    expect(!!sim->connect(sum_lif, 0, prod_lif, 0));
    expect(!!sim->connect(constant_lif, 0, prod_lif, 1));

    return neuron_model;
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

    cross_pre.default_threshold      = irt::one;
    int_pre.default_X                = irt::zero;
    sum_pre.default_input_coeffs[0]  = irt::one;
    sum_pre.default_input_coeffs[1]  = dApre;
    mult_pre.default_input_coeffs[0] = -irt::one / taupre;
    mult_pre.default_input_coeffs[1] = irt::zero;

    cross_post.default_threshold      = irt::one;
    int_post.default_X                = irt::zero;
    sum_post.default_input_coeffs[0]  = irt::one;
    sum_post.default_input_coeffs[1]  = dApost;
    mult_post.default_input_coeffs[0] = -irt::one / taupost;
    mult_post.default_input_coeffs[1] = irt::zero;

    const_syn.default_value = irt::one;

    struct synapse synapse_model = {
        sim->get_id(sum_pre),   sim->get_id(mult_pre),
        sim->get_id(int_pre),   sim->get_id(cross_pre),

        sim->get_id(sum_post),  sim->get_id(mult_post),
        sim->get_id(int_post),  sim->get_id(cross_post),

        sim->get_id(const_syn), sim->get_id(accumulator_syn),
    };

    // Connections
    expect(!!sim->connect(cross_pre, 0, int_pre, 0));
    expect(!!sim->connect(mult_pre, 0, int_pre, 0));
    expect(!!sim->connect(cross_pre, 0, int_pre, 1));
    expect(!!sim->connect(int_pre, 0, cross_pre, 2));
    expect(!!sim->connect(cross_pre, 0, mult_pre, 0));
    expect(!!sim->connect(const_syn, 0, mult_pre, 1));
    expect(!!sim->connect(int_pre, 0, sum_pre, 0));
    expect(!!sim->connect(const_syn, 0, sum_pre, 1));
    expect(!!sim->connect(sum_pre, 0, cross_pre, 1));
    expect(!!sim->connect(
      presynaptic_model, presynaptic_port, irt::get_model(cross_pre), 0))
      << fatal;

    expect(!!sim->connect(cross_post, 0, int_post, 0));
    expect(!!sim->connect(mult_post, 0, int_post, 0));
    expect(!!sim->connect(cross_post, 0, int_post, 1));
    expect(!!sim->connect(int_post, 0, cross_post, 2));
    expect(!!sim->connect(cross_post, 0, mult_post, 0));
    expect(!!sim->connect(const_syn, 0, mult_post, 1));
    expect(!!sim->connect(int_post, 0, sum_post, 0));
    expect(!!sim->connect(const_syn, 0, sum_post, 1));
    expect(!!sim->connect(sum_post, 0, cross_post, 1));
    expect(!!sim->connect(
      postsynaptic_model, postsynaptic_port, irt::get_model(cross_post), 0));

    expect(!!sim->connect(
      presynaptic_model, presynaptic_port, irt::get_model(accumulator_syn), 0));
    expect(!!sim->connect(postsynaptic_model,
                          postsynaptic_port,
                          irt::get_model(accumulator_syn),
                          1));
    expect(!!sim->connect(cross_post, 0, accumulator_syn, 2));
    expect(!!sim->connect(cross_pre, 0, accumulator_syn, 3));

    return synapse_model;
}

int main()
{
    using namespace boost::ut;

    "song_1_simulation"_test = [] {
        irt::simulation sim(1024 * 1024 * 8);
        const auto      N = 4u;

        expect(sim.can_alloc(N + 2u * N * N + 2u * N * N + 4u * N * N + N +
                             2u * N * N));

        std::vector<struct neuron> neurons;
        for (long unsigned int i = 0; i < N; i++) {
            struct neuron neuron_model = make_neuron(&sim);
            neurons.emplace_back(neuron_model);
        }

        std::vector<struct synapse> synapses;
        for (long unsigned int i = 0; i < N; i++) {
            for (long unsigned int j = 0; j < N; j++) {
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

        sim.t = irt::zero;
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (sim.t < 30);
    };
}
