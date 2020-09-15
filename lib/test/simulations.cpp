// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <cstdio>

static void
dot_graph_save(const irt::simulation& sim, std::FILE* os)
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

    expect((os != nullptr) >> boost::ut::fatal);

    fmt::print(os, "digraph graphname {{\n");

    irt::output_port* output_port = nullptr;
    while (sim.output_ports.next(output_port)) {
        for (const irt::input_port_id dst : output_port->connections) {
            if (auto* input_port = sim.input_ports.try_to_get(dst);
                input_port) {
                auto* mdl_src = sim.models.try_to_get(output_port->model);
                auto* mdl_dst = sim.models.try_to_get(input_port->model);

                if (!(mdl_src && mdl_dst))

                    continue;
                if (mdl_src->name.empty())
                    fmt::print(os, "{} -> ", irt::get_key(output_port->model));
                else
                    fmt::print(os, "{} -> ", mdl_src->name.c_str());

                if (mdl_dst->name.empty())
                    fmt::print(os, "{}", irt::get_key(input_port->model));
                else
                    fmt::print(os, "{}", mdl_dst->name.c_str());

                fmt::print(os, " [label=\"");

                if (output_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.output_ports.get_id(*output_port)));
                else
                    fmt::print(os, "{}", output_port->name.c_str());

                fmt::print(os, "-");

                if (input_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.input_ports.get_id(*input_port)));
                else

                    fmt::print(os, "{}", input_port->name.c_str());

                fmt::print(os, "\"];\n");
            }
        }
    }

    fmt::print(os, "}}\n");
}

struct neuron
{
    irt::dynamics_id sum;
    irt::dynamics_id prod;
    irt::dynamics_id integrator;
    irt::dynamics_id quantifier;
    irt::dynamics_id constant;
    irt::dynamics_id cross;
    irt::dynamics_id constant_cross;
};

struct synapse
{
    irt::dynamics_id sum_pre;
    irt::dynamics_id prod_pre;
    irt::dynamics_id integrator_pre;
    irt::dynamics_id quantifier_pre;
    irt::dynamics_id cross_pre;

    irt::dynamics_id sum_post;
    irt::dynamics_id prod_post;
    irt::dynamics_id integrator_post;
    irt::dynamics_id quantifier_post;
    irt::dynamics_id cross_post;

    irt::dynamics_id constant_syn;
    irt::dynamics_id accumulator_syn;
};

struct neuron
make_neuron(irt::simulation* sim) noexcept
{
    using namespace boost::ut;
    double tau_lif = 1.0 + static_cast<double>(rand()) /
                             (static_cast<double>(RAND_MAX / (2.0 - 1.0)));
    double Vr_lif = 0.0;
    double Vt_lif = 1.0;

    auto& sum_lif = sim->adder_2_models.alloc();
    auto& prod_lif = sim->adder_2_models.alloc();
    auto& integrator_lif = sim->integrator_models.alloc();
    auto& quantifier_lif = sim->quantifier_models.alloc();
    auto& constant_lif = sim->constant_models.alloc();
    auto& constant_cross_lif = sim->constant_models.alloc();
    auto& cross_lif = sim->cross_models.alloc();

    sum_lif.default_input_coeffs[0] = -1.0;
    sum_lif.default_input_coeffs[1] = 2 * Vt_lif;

    prod_lif.default_input_coeffs[0] = 1.0 / tau_lif;
    prod_lif.default_input_coeffs[1] = 0.0;

    constant_lif.default_value = 1.0;
    constant_cross_lif.default_value = Vr_lif;

    integrator_lif.default_current_value = 0.0;

    quantifier_lif.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_lif.default_zero_init_offset = true;
    quantifier_lif.default_step_size = 0.1;
    quantifier_lif.default_past_length = 3;

    cross_lif.default_threshold = Vt_lif;

    sim->alloc(sum_lif, sim->adder_2_models.get_id(sum_lif));
    sim->alloc(prod_lif, sim->adder_2_models.get_id(prod_lif));
    sim->alloc(integrator_lif, sim->integrator_models.get_id(integrator_lif));
    sim->alloc(quantifier_lif, sim->quantifier_models.get_id(quantifier_lif));
    sim->alloc(constant_lif, sim->constant_models.get_id(constant_lif));
    sim->alloc(cross_lif, sim->cross_models.get_id(cross_lif));
    sim->alloc(constant_cross_lif,
               sim->constant_models.get_id(constant_cross_lif));

    struct neuron neuron_model = {
        sim->adder_2_models.get_id(sum_lif),
        sim->adder_2_models.get_id(prod_lif),
        sim->integrator_models.get_id(integrator_lif),
        sim->quantifier_models.get_id(quantifier_lif),
        sim->constant_models.get_id(constant_lif),
        sim->cross_models.get_id(cross_lif),
        sim->constant_models.get_id(constant_cross_lif),
    };

    // Connections
    expect(sim->connect(quantifier_lif.y[0], integrator_lif.x[0]) ==
           irt::status::success);
    expect(sim->connect(prod_lif.y[0], integrator_lif.x[1]) ==
           irt::status::success);
    expect(sim->connect(cross_lif.y[0], integrator_lif.x[2]) ==
           irt::status::success);
    expect(sim->connect(cross_lif.y[0], quantifier_lif.x[0]) ==
           irt::status::success);
    expect(sim->connect(cross_lif.y[0], sum_lif.x[0]) == irt::status::success);
    expect(sim->connect(integrator_lif.y[0], cross_lif.x[0]) ==
           irt::status::success);
    expect(sim->connect(integrator_lif.y[0], cross_lif.x[2]) ==
           irt::status::success);
    expect(sim->connect(constant_cross_lif.y[0], cross_lif.x[1]) ==
           irt::status::success);
    expect(sim->connect(constant_lif.y[0], sum_lif.x[1]) ==
           irt::status::success);
    expect(sim->connect(sum_lif.y[0], prod_lif.x[0]) == irt::status::success);
    expect(sim->connect(constant_lif.y[0], prod_lif.x[1]) ==
           irt::status::success);
    return neuron_model;
}

struct synapse
make_synapse(irt::simulation* sim,
             long unsigned int source,
             long unsigned int target,
             irt::output_port_id presynaptic,
             irt::output_port_id postsynaptic)
{
    using namespace boost::ut;
    double taupre = 20.0 * 1;
    double taupost = taupre;
    double gamax = 0.015;
    double dApre = 0.01;
    double dApost = -dApre * taupre / taupost * 1.05;
    dApost = dApost * gamax;
    dApre = dApre * gamax;

    auto& int_pre = sim->integrator_models.alloc();
    auto& quant_pre = sim->quantifier_models.alloc();
    auto& sum_pre = sim->adder_2_models.alloc();
    auto& mult_pre = sim->adder_2_models.alloc();
    auto& cross_pre = sim->cross_models.alloc();

    auto& int_post = sim->integrator_models.alloc();
    auto& quant_post = sim->quantifier_models.alloc();
    auto& sum_post = sim->adder_2_models.alloc();
    auto& mult_post = sim->adder_2_models.alloc();
    auto& cross_post = sim->cross_models.alloc();

    auto& const_syn = sim->constant_models.alloc();
    auto& accumulator_syn = sim->accumulator_2_models.alloc();

    cross_pre.default_threshold = 1.0;
    int_pre.default_current_value = 0.0;
    quant_pre.default_adapt_state = irt::quantifier::adapt_state::possible;
    quant_pre.default_zero_init_offset = true;
    quant_pre.default_step_size = 1e-6;
    quant_pre.default_past_length = 3;
    sum_pre.default_input_coeffs[0] = 1.0;
    sum_pre.default_input_coeffs[1] = dApre;
    mult_pre.default_input_coeffs[0] = -1.0 / taupre;
    mult_pre.default_input_coeffs[1] = 0.0;

    cross_post.default_threshold = 1.0;
    int_post.default_current_value = 0.0;
    quant_post.default_adapt_state = irt::quantifier::adapt_state::possible;
    quant_post.default_zero_init_offset = true;
    quant_post.default_step_size = 1e-6;
    quant_post.default_past_length = 3;
    sum_post.default_input_coeffs[0] = 1.0;
    sum_post.default_input_coeffs[1] = dApost;
    mult_post.default_input_coeffs[0] = -1.0 / taupost;
    mult_post.default_input_coeffs[1] = 0.0;

    const_syn.default_value = 1.0;

    char intpre[7];
    char quapre[7];
    char addpre[7];
    char propre[7];
    char crosspre[7];
    char intpost[7];
    char quapost[7];
    char addpost[7];
    char propost[7];
    char crosspost[7];
    char ctesyn[7];
    char accumsyn[7];

    snprintf(intpre, 7, "int%ld%ld-", source, target);
    snprintf(quapre, 7, "qua%ld%ld-", source, target);
    snprintf(addpre, 7, "add%ld%ld-", source, target);
    snprintf(propre, 7, "pro%ld%ld-", source, target);
    snprintf(crosspre, 7, "cro%ld%ld-", source, target);

    snprintf(intpost, 7, "int%ld%ld+", source, target);
    snprintf(quapost, 7, "qua%ld%ld+", source, target);
    snprintf(addpost, 7, "add%ld%ld+", source, target);
    snprintf(propost, 7, "pro%ld%ld+", source, target);
    snprintf(crosspost, 7, "cro%ld%ld+", source, target);

    snprintf(ctesyn, 7, "cte%ld%ld", source, target);
    snprintf(accumsyn, 7, "acc%ld%ld", source, target);

    !expect(irt::is_success(
      sim->alloc(int_pre, sim->integrator_models.get_id(int_pre), intpre)));
    !expect(irt::is_success(
      sim->alloc(quant_pre, sim->quantifier_models.get_id(quant_pre), quapre)));
    !expect(irt::is_success(
      sim->alloc(sum_pre, sim->adder_2_models.get_id(sum_pre), addpre)));
    !expect(irt::is_success(
      sim->alloc(mult_pre, sim->adder_2_models.get_id(mult_pre), propre)));
    !expect(irt::is_success(
      sim->alloc(cross_pre, sim->cross_models.get_id(cross_pre), crosspre)));

    !expect(irt::is_success(
      sim->alloc(int_post, sim->integrator_models.get_id(int_post), intpost)));
    !expect(irt::is_success(sim->alloc(
      quant_post, sim->quantifier_models.get_id(quant_post), quapost)));
    !expect(irt::is_success(
      sim->alloc(sum_post, sim->adder_2_models.get_id(sum_post), addpost)));
    !expect(irt::is_success(
      sim->alloc(mult_post, sim->adder_2_models.get_id(mult_post), propost)));
    !expect(irt::is_success(
      sim->alloc(cross_post, sim->cross_models.get_id(cross_post), crosspost)));

    !expect(irt::is_success(
      sim->alloc(const_syn, sim->constant_models.get_id(const_syn), ctesyn)));
    !expect(irt::is_success(
      sim->alloc(accumulator_syn,
                 sim->accumulator_2_models.get_id(accumulator_syn),
                 accumsyn)));

    struct synapse synapse_model = {
        sim->adder_2_models.get_id(sum_pre),
        sim->adder_2_models.get_id(mult_pre),
        sim->integrator_models.get_id(int_pre),
        sim->quantifier_models.get_id(quant_pre),
        sim->cross_models.get_id(cross_pre),

        sim->adder_2_models.get_id(sum_post),
        sim->adder_2_models.get_id(mult_post),
        sim->integrator_models.get_id(int_post),
        sim->quantifier_models.get_id(quant_post),
        sim->cross_models.get_id(cross_post),

        sim->constant_models.get_id(const_syn),
        sim->accumulator_2_models.get_id(accumulator_syn),
    };

    // Connections
    expect(sim->connect(quant_pre.y[0], int_pre.x[0]) == irt::status::success);
    expect(sim->connect(mult_pre.y[0], int_pre.x[1]) == irt::status::success);
    expect(sim->connect(cross_pre.y[0], int_pre.x[2]) == irt::status::success);
    expect(sim->connect(int_pre.y[0], cross_pre.x[2]) == irt::status::success);
    expect(sim->connect(cross_pre.y[0], quant_pre.x[0]) ==
           irt::status::success);
    expect(sim->connect(cross_pre.y[0], mult_pre.x[0]) == irt::status::success);
    expect(sim->connect(const_syn.y[0], mult_pre.x[1]) == irt::status::success);
    expect(sim->connect(int_pre.y[0], sum_pre.x[0]) == irt::status::success);
    expect(sim->connect(const_syn.y[0], sum_pre.x[1]) == irt::status::success);
    expect(sim->connect(sum_pre.y[0], cross_pre.x[1]) == irt::status::success);
    expect(sim->connect(presynaptic, cross_pre.x[0]) == irt::status::success);

    expect(sim->connect(quant_post.y[0], int_post.x[0]) ==
           irt::status::success);
    expect(sim->connect(mult_post.y[0], int_post.x[1]) == irt::status::success);
    expect(sim->connect(cross_post.y[0], int_post.x[2]) ==
           irt::status::success);
    expect(sim->connect(int_post.y[0], cross_post.x[2]) ==
           irt::status::success);
    expect(sim->connect(cross_post.y[0], quant_post.x[0]) ==
           irt::status::success);
    expect(sim->connect(cross_post.y[0], mult_post.x[0]) ==
           irt::status::success);
    expect(sim->connect(const_syn.y[0], mult_post.x[1]) ==
           irt::status::success);
    expect(sim->connect(int_post.y[0], sum_post.x[0]) == irt::status::success);
    expect(sim->connect(const_syn.y[0], sum_post.x[1]) == irt::status::success);
    expect(sim->connect(sum_post.y[0], cross_post.x[1]) ==
           irt::status::success);
    expect(sim->connect(postsynaptic, cross_post.x[0]) == irt::status::success);

    expect(sim->connect(presynaptic, accumulator_syn.x[0]) ==
           irt::status::success);
    expect(sim->connect(postsynaptic, accumulator_syn.x[1]) ==
           irt::status::success);
    expect(sim->connect(cross_post.y[0], accumulator_syn.x[2]) ==
           irt::status::success);
    expect(sim->connect(cross_pre.y[0], accumulator_syn.x[3]) ==
           irt::status::success);

    return synapse_model;
}

int
main()
{
    using namespace boost::ut;

    "song_1_simulation"_test = [] {
        irt::simulation sim;
        // Neuron constants
        long unsigned int N = 4;
        /*double F = 15.0;

        double Eex = 0.0;
        double Ein = -70*0.001;
        double tauex = 5*0.001;
        double tauin = tauex;*/

        // Synapse constants

        // double ginbar = 0.05;

        expect(irt::is_success(sim.init(10000lu, 10000lu)));

        expect(sim.generator_models.can_alloc(N));
        expect(sim.integrator_models.can_alloc(2 * N * N));
        expect(sim.quantifier_models.can_alloc(2 * N * N));
        expect(sim.adder_2_models.can_alloc(4 * N * N));
        expect(sim.constant_models.can_alloc(N));
        expect(sim.cross_models.can_alloc(2 * N * N));

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
                               sim.cross_models.get(neurons[i].cross).y[1],
                               sim.cross_models.get(neurons[j].cross).y[1]);
                synapses.emplace_back(synapse_model);
            }
        }

        dot_graph_save(sim, stdout);

        irt::time t = 0.0;
        std::FILE* os = std::fopen("output_song.csv", "w");
        !expect(os != nullptr);

        std::string s = "t,";
        for (long unsigned int i = 0; i < N * N; i++) {
            s = s + "Apre" + std::to_string(i) + ",Apost" + std::to_string(i) +
                ",";
        }
        for (long unsigned int i = 0; i < N * N; i++)
            s = s + "W" + std::to_string(i) + ",";
        fmt::print(os, "{}\n", s);
        expect(irt::status::success == sim.initialize(t));

        do {

            irt::status st = sim.run(t);
            expect(st == irt::status::success);

            std::string s = std::to_string(t) + ",";
            for (long unsigned int i = 0; i < N * N; i++) {
                s = s +
                    std::to_string(
                      sim.integrator_models.get(synapses[i].integrator_pre)
                        .last_output_value) +
                    "," +
                    std::to_string(
                      sim.integrator_models.get(synapses[i].integrator_post)
                        .last_output_value) +
                    ",";
            }
            for (long unsigned int i = 0; i < N * N; i++)
                s = s +
                    std::to_string(
                      sim.accumulator_2_models.get(synapses[i].accumulator_syn)
                        .number) +
                    ",";
            fmt::print(os, "{}\n", s);

        } while (t < 30);

        std::fclose(os);
    };
}
