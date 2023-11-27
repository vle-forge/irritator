// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <fmt/format.h>

#include <cstdio>

#include <boost/ut.hpp>

static void dot_graph_save(const irt::simulation& /*sim*/, std::FILE* /*os*/)
{
    using namespace boost::ut;

    /* With input and output port.

      digraph graphname{
          graph[rankdir = "LR"];
          node[shape = "record"];
          edge[];

          "sum_a"[label = "sum-a | <f0> | <f1>"];

          "sum_a":f0->int_a[id = 1];
          sum_b->int_b[label = "2-10"];
          prod->sum_b[label = "3-4"];
          prod -> "sum_a":f0[label = "3-2"];
          int_a->qua_a[label = "4-11"];
          int_a->prod[label = "4-5"];
          int_a -> "sum_a":f1[label = "4-1"];
          int_b->qua_b[label = "5-12"];
          int_b->prod[label = "5-6"];
          int_b->sum_b[label = "5-3"];
          qua_a->int_a[label = "6-7"];
          qua_b->int_b[label = "7-9"];
      }
      */

    // expect((os != nullptr) >> boost::ut::fatal);

    // fmt::print(os, "digraph graphname {{\n");

    // irt::output_port* output_port = nullptr;
    // while (sim.output_ports.next(output_port)) {
    //    for (const irt::input_port_id dst : output_port->connections) {
    //        if (auto* input_port = sim.input_ports.try_to_get(dst);
    //            input_port) {
    //            auto* mdl_src = sim.models.try_to_get(output_port->model);
    //            auto* mdl_dst = sim.models.try_to_get(input_port->model);

    //            if (!(mdl_src && mdl_dst))
    //                continue;

    //            fmt::print(os,
    //                       "{} -> {} [label=\"{}-{}\"];\n",
    //                       irt::get_key(output_port->model),
    //                       irt::get_key(input_port->model),
    //                       irt::get_key(sim.output_ports.get_id(*output_port)),
    //                       irt::get_key(sim.input_ports.get_id(*input_port)));
    //        }
    //    }
    //}

    // fmt::print(os, "}}\n");
}

struct neuron
{
    irt::model_id sum;
    irt::model_id prod;
    irt::model_id integrator;
    irt::model_id quantifier;
    irt::model_id constant;
    irt::model_id cross;
    irt::model_id constant_cross;
};

struct synapse
{
    irt::model_id sum_pre;
    irt::model_id prod_pre;
    irt::model_id integrator_pre;
    irt::model_id quantifier_pre;
    irt::model_id cross_pre;

    irt::model_id sum_post;
    irt::model_id prod_post;
    irt::model_id integrator_post;
    irt::model_id quantifier_post;
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

    auto& sum_lif            = sim->alloc<irt::adder_2>();
    auto& prod_lif           = sim->alloc<irt::adder_2>();
    auto& integrator_lif     = sim->alloc<irt::integrator>();
    auto& quantifier_lif     = sim->alloc<irt::quantifier>();
    auto& constant_lif       = sim->alloc<irt::constant>();
    auto& constant_cross_lif = sim->alloc<irt::constant>();
    auto& cross_lif          = sim->alloc<irt::cross>();

    sum_lif.default_input_coeffs[0] = -irt::one;
    sum_lif.default_input_coeffs[1] = irt::two * Vt_lif;

    prod_lif.default_input_coeffs[0] = irt::one / tau_lif;
    prod_lif.default_input_coeffs[1] = irt::zero;

    constant_lif.default_value       = irt::one;
    constant_cross_lif.default_value = Vr_lif;

    integrator_lif.default_current_value = irt::zero;

    quantifier_lif.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_lif.default_zero_init_offset = true;
    quantifier_lif.default_step_size        = irt::real(0.1);
    quantifier_lif.default_past_length      = 3;

    cross_lif.default_threshold = Vt_lif;

    struct neuron neuron_model = {
        sim->get_id(sum_lif),
        sim->get_id(prod_lif),
        sim->get_id(integrator_lif),
        sim->get_id(quantifier_lif),
        sim->get_id(constant_lif),
        sim->get_id(cross_lif),
        sim->get_id(constant_cross_lif),
    };

    // Connections
    expect(!!sim->connect(quantifier_lif, 0, integrator_lif, 0));
    expect(!!sim->connect(prod_lif, 0, integrator_lif, 1));
    expect(!!sim->connect(cross_lif, 0, integrator_lif, 2));
    expect(!!sim->connect(cross_lif, 0, quantifier_lif, 0));
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
                            long unsigned int /*source*/,
                            long unsigned int /*target*/,
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

    auto& int_pre   = sim->alloc<irt::integrator>();
    auto& quant_pre = sim->alloc<irt::quantifier>();
    auto& sum_pre   = sim->alloc<irt::adder_2>();
    auto& mult_pre  = sim->alloc<irt::adder_2>();
    auto& cross_pre = sim->alloc<irt::cross>();

    auto& int_post   = sim->alloc<irt::integrator>();
    auto& quant_post = sim->alloc<irt::quantifier>();
    auto& sum_post   = sim->alloc<irt::adder_2>();
    auto& mult_post  = sim->alloc<irt::adder_2>();
    auto& cross_post = sim->alloc<irt::cross>();

    auto& const_syn       = sim->alloc<irt::constant>();
    auto& accumulator_syn = sim->alloc<irt::accumulator_2>();

    cross_pre.default_threshold        = irt::one;
    int_pre.default_current_value      = irt::zero;
    quant_pre.default_adapt_state      = irt::quantifier::adapt_state::possible;
    quant_pre.default_zero_init_offset = true;
    quant_pre.default_step_size        = irt::to_real(1e-6);
    quant_pre.default_past_length      = 3;
    sum_pre.default_input_coeffs[0]    = irt::one;
    sum_pre.default_input_coeffs[1]    = dApre;
    mult_pre.default_input_coeffs[0]   = -irt::one / taupre;
    mult_pre.default_input_coeffs[1]   = irt::zero;

    cross_post.default_threshold   = irt::one;
    int_post.default_current_value = irt::zero;
    quant_post.default_adapt_state = irt::quantifier::adapt_state::possible;
    quant_post.default_zero_init_offset = true;
    quant_post.default_step_size        = irt::to_real(1e-6);
    quant_post.default_past_length      = 3;
    sum_post.default_input_coeffs[0]    = irt::one;
    sum_post.default_input_coeffs[1]    = dApost;
    mult_post.default_input_coeffs[0]   = -irt::one / taupost;
    mult_post.default_input_coeffs[1]   = irt::zero;

    const_syn.default_value = irt::one;

    struct synapse synapse_model = {
        sim->get_id(sum_pre),    sim->get_id(mult_pre),
        sim->get_id(int_pre),    sim->get_id(quant_pre),
        sim->get_id(cross_pre),

        sim->get_id(sum_post),   sim->get_id(mult_post),
        sim->get_id(int_post),   sim->get_id(quant_post),
        sim->get_id(cross_post),

        sim->get_id(const_syn),  sim->get_id(accumulator_syn),
    };

    // Connections
    expect(!!sim->connect(quant_pre, 0, int_pre, 0));
    expect(!!sim->connect(mult_pre, 0, int_pre, 1));
    expect(!!sim->connect(cross_pre, 0, int_pre, 2));
    expect(!!sim->connect(int_pre, 0, cross_pre, 2));
    expect(!!sim->connect(cross_pre, 0, quant_pre, 0));
    expect(!!sim->connect(cross_pre, 0, mult_pre, 0));
    expect(!!sim->connect(const_syn, 0, mult_pre, 1));
    expect(!!sim->connect(int_pre, 0, sum_pre, 0));
    expect(!!sim->connect(const_syn, 0, sum_pre, 1));
    expect(!!sim->connect(sum_pre, 0, cross_pre, 1));
    expect(!!sim->connect(
      presynaptic_model, presynaptic_port, irt::get_model(cross_pre), 0));

    expect(!!sim->connect(quant_post, 0, int_post, 0));
    expect(!!sim->connect(mult_post, 0, int_post, 1));
    expect(!!sim->connect(cross_post, 0, int_post, 2));
    expect(!!sim->connect(int_post, 0, cross_post, 2));
    expect(!!sim->connect(cross_post, 0, quant_post, 0));
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
        irt::simulation sim;
        // Neuron constants
        const auto N = 4u;
        /*double F = 15.0;

        double Eex = 0.0;
        double Ein = -70*0.001;
        double tauex = 5*0.001;
        double tauin = tauex;*/

        // Synapse constants

        // double ginbar = 0.05;

        expect(!!sim.init(10000lu, 10000lu));

        expect(sim.can_alloc(N + 2u * N * N + 2u * N * N + 4u * N * N + N +
                             2u * N * N));

        std::vector<struct neuron> neurons;
        for (long unsigned int i = 0; i < N; i++) {
            struct neuron neuron_model = make_neuron(&sim);
            neurons.emplace_back(neuron_model);
        }

        // Neurons
        /*std::vector<irt::dynamics_id> generators;
        for (long unsigned int i = 0 ; i < N; i++) {
          auto& gen = sim.generator_models.alloc();
          gen.default_value = 3.0;
          gen.default_offset = i+1;
          gen.default_period = 10.0;

          char genstr[5];
          snprintf(genstr, 5,"gen%ld", i);
          !expect(irt::is_success(sim.alloc(gen,
        sim.generator_models.get_id(gen), genstr)));

          generators.emplace_back(sim.generator_models.get_id(gen));
        } */

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

        dot_graph_save(sim, stdout);

        irt::time  t  = 0.0;
        std::FILE* os = std::fopen("output_song.csv", "w");
        expect((os != nullptr) >> fatal);

        std::string s = "t,";
        for (long unsigned int i = 0; i < N * N; i++) {
            s = s + "Apre" + std::to_string(i) + ",Apost" + std::to_string(i) +
                ",";
        }
        for (long unsigned int i = 0; i < N * N; i++)
            s = s + "W" + std::to_string(i) + ",";
        fmt::print(os, "{}\n", s);
        expect(!!sim.initialize(t));

        do {
            expect(!!sim.run(t));

            fmt::print(os, "{},", t);

            for (long unsigned int i = 0; i < N * N; i++) {
                auto& pre  = sim.models.get(synapses[i].integrator_pre);
                auto& post = sim.models.get(synapses[i].integrator_post);

                auto& pre_dyn  = irt::get_dyn<irt::integrator>(pre);
                auto& post_dyn = irt::get_dyn<irt::integrator>(post);

                fmt::print(os,
                           "{},{},",
                           pre_dyn.last_output_value,
                           post_dyn.last_output_value);
            }
            for (long unsigned int i = 0; i < N * N; i++) {
                auto& acc     = sim.models.get(synapses[i].accumulator_syn);
                auto& acc_dyn = irt::get_dyn<irt::accumulator_2>(acc);

                fmt::print(os, "{},", acc_dyn.number);
            }

            fmt::print(os, "{}\n", s);

        } while (t < 30);

        std::fclose(os);
    };
}
