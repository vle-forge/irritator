// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <hayai.hpp>

#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <cstdio>

#include <chrono>

static void
dot_graph_save(const irt::simulation& sim, std::FILE* os)
{
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

    !boost::ut::expect(os != nullptr);

    std::fputs("digraph graphname {\n", os);

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

                std::fputs(" [label=\"", os);

                if (output_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.output_ports.get_id(*output_port)));
                else
                    fmt::print(os, "{}", output_port->name.c_str());

                std::fputs("-", os);

                if (input_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.input_ports.get_id(*input_port)));
                else
                    fmt::print(os, "{}", input_port->name.c_str());

                std::fputs("\"];\n", os);
            }
        }
    }
}

struct neuron {
  irt::dynamics_id  sum;
  irt::dynamics_id  prod;
  irt::dynamics_id  integrator;
  irt::dynamics_id  quantifier;
  irt::dynamics_id  constant;
  irt::dynamics_id  cross;
  irt::dynamics_id  constant_cross;
};

struct synapse {
  irt::dynamics_id  sum_pre;
  irt::dynamics_id  prod_pre;
  irt::dynamics_id  integrator_pre;
  irt::dynamics_id  quantifier_pre;
  irt::dynamics_id  cross_pre;

  irt::dynamics_id  sum_post;
  irt::dynamics_id  prod_post;
  irt::dynamics_id  integrator_post;
  irt::dynamics_id  quantifier_post;
  irt::dynamics_id  cross_post;

  irt::dynamics_id  constant_syn;
  irt::dynamics_id  accumulator_syn;
};

struct neuron 
make_neuron(irt::simulation* sim, long unsigned int i) noexcept
{
  using namespace boost::ut;
  double vt = -56*0.001;
  double Vrest = -70*0.001;
  double reset = -60*0.001;
  double taum = 20.0*0.001;


  auto& sum_lif = sim->adder_2_models.alloc();
  auto& prod_lif = sim->mult_2_models.alloc();
  auto& integrator_lif = sim->integrator_models.alloc();
  auto& quantifier_lif = sim->quantifier_models.alloc();
  auto& constant_lif = sim->constant_models.alloc();
  auto& constant_cross_lif = sim->constant_models.alloc();
  auto& cross_lif = sim->cross_models.alloc();


  sum_lif.default_input_coeffs[0] = -1.0;
  sum_lif.default_input_coeffs[1] = Vrest;
  
  prod_lif.default_input_coeffs[0] = 1.0;
  prod_lif.default_input_coeffs[1] = 1.0/taum;

  constant_lif.default_value = 1.0;
  constant_cross_lif.default_value = reset;
  
  integrator_lif.default_current_value = 0.0;

  quantifier_lif.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quantifier_lif.default_zero_init_offset = true;
  quantifier_lif.default_step_size = 0.001;
  quantifier_lif.default_past_length = 3;

  cross_lif.default_threshold = vt;

  char crosslif[7];char ctecrosslif[7];char intlif[7];
  char quantlif[7];char sumlif[7];char prodlif[7];char ctelif[7];

  snprintf(crosslif, 7,"croli%ld", i);snprintf(ctecrosslif, 7,"ctcli%ld", i);snprintf(intlif, 7,"intli%ld", i);
  snprintf(quantlif, 7,"quali%ld", i);snprintf(sumlif, 7,"sumli%ld", i);snprintf(prodlif, 7,"prdli%ld", i);
  snprintf(ctelif, 7,"cteli%ld", i);
  

  sim->alloc(sum_lif, sim->adder_2_models.get_id(sum_lif), sumlif);
  sim->alloc(prod_lif, sim->mult_2_models.get_id(prod_lif), prodlif);
  sim->alloc(integrator_lif, sim->integrator_models.get_id(integrator_lif), intlif);
  sim->alloc(quantifier_lif, sim->quantifier_models.get_id(quantifier_lif), quantlif);
  sim->alloc(constant_lif, sim->constant_models.get_id(constant_lif), ctelif);
  sim->alloc(cross_lif, sim->cross_models.get_id(cross_lif), crosslif);
  sim->alloc(constant_cross_lif, sim->constant_models.get_id(constant_cross_lif), ctecrosslif);

  struct neuron neuron_model = {sim->adder_2_models.get_id(sum_lif),
                                sim->mult_2_models.get_id(prod_lif),
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
  expect(sim->connect(cross_lif.y[0], sum_lif.x[0]) ==
          irt::status::success);
  expect(sim->connect(integrator_lif.y[0],cross_lif.x[0]) ==
          irt::status::success);     
  expect(sim->connect(integrator_lif.y[0],cross_lif.x[2]) ==
          irt::status::success);
  expect(sim->connect(constant_cross_lif.y[0],cross_lif.x[1]) ==
          irt::status::success);     
  expect(sim->connect(constant_lif.y[0], sum_lif.x[1]) ==
          irt::status::success);  
  expect(sim->connect(sum_lif.y[0],prod_lif.x[0]) ==
          irt::status::success);   
  expect(sim->connect(constant_lif.y[0],prod_lif.x[1]) ==
          irt::status::success);
  return neuron_model;
}

struct synapse make_synapse(irt::simulation* sim, long unsigned int source, long unsigned int target,
                              irt::output_port_id presynaptic,irt::output_port_id postsynaptic,double quantum)
{
  using namespace boost::ut;
  double taupre = 20.0*1;
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
  quant_pre.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quant_pre.default_zero_init_offset = true;
  quant_pre.default_step_size = quantum;
  quant_pre.default_past_length = 3;
  sum_pre.default_input_coeffs[0] = 1.0;
  sum_pre.default_input_coeffs[1] = dApre;
  mult_pre.default_input_coeffs[0] = -1.0/taupre;
  mult_pre.default_input_coeffs[1] = 0.0;

  cross_post.default_threshold = 1.0;
  int_post.default_current_value = 0.0;
  quant_post.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quant_post.default_zero_init_offset = true;
  quant_post.default_step_size = quantum;
  quant_post.default_past_length = 3;
  sum_post.default_input_coeffs[0] = 1.0;
  sum_post.default_input_coeffs[1] = dApost;
  mult_post.default_input_coeffs[0] = -1.0/taupost;
  mult_post.default_input_coeffs[1] = 0.0;

  const_syn.default_value = 1.0;





  !expect(irt::is_success(sim->alloc(int_pre, sim->integrator_models.get_id(int_pre))));
  !expect(irt::is_success(sim->alloc(quant_pre, sim->quantifier_models.get_id(quant_pre))));
  !expect(irt::is_success(sim->alloc(sum_pre, sim->adder_2_models.get_id(sum_pre))));
  !expect(irt::is_success(sim->alloc(mult_pre, sim->adder_2_models.get_id(mult_pre))));
  !expect(irt::is_success(sim->alloc(cross_pre, sim->cross_models.get_id(cross_pre))));

  !expect(irt::is_success(sim->alloc(int_post, sim->integrator_models.get_id(int_post))));
  !expect(irt::is_success(sim->alloc(quant_post, sim->quantifier_models.get_id(quant_post))));
  !expect(irt::is_success(sim->alloc(sum_post, sim->adder_2_models.get_id(sum_post))));
  !expect(irt::is_success(sim->alloc(mult_post, sim->adder_2_models.get_id(mult_post))));
  !expect(irt::is_success(sim->alloc(cross_post, sim->cross_models.get_id(cross_post))));

  !expect(irt::is_success(sim->alloc(const_syn, sim->constant_models.get_id(const_syn))));
  !expect(irt::is_success(sim->alloc(accumulator_syn, sim->accumulator_2_models.get_id(accumulator_syn))));

  struct synapse synapse_model = {sim->adder_2_models.get_id(sum_pre),
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
  expect(sim->connect(quant_pre.y[0], 
                      int_pre.x[0]) ==
                        irt::status::success);
  expect(sim->connect(mult_pre.y[0], 
                      int_pre.x[1]) ==
                        irt::status::success);
  expect(sim->connect(cross_pre.y[0], 
                      int_pre.x[2]) ==
                        irt::status::success);
  expect(sim->connect(int_pre.y[0], 
                      cross_pre.x[2]) ==
                        irt::status::success);
  expect(sim->connect(cross_pre.y[0], 
                      quant_pre.x[0]) ==
                        irt::status::success);
  expect(sim->connect(cross_pre.y[0], 
                      mult_pre.x[0]) ==
                        irt::status::success);
  expect(sim->connect(const_syn.y[0], 
                      mult_pre.x[1]) ==
                        irt::status::success);
  expect(sim->connect(int_pre.y[0], 
                      sum_pre.x[0]) ==
                        irt::status::success);
  expect(sim->connect(const_syn.y[0], 
                      sum_pre.x[1]) ==
                        irt::status::success);
  expect(sim->connect(sum_pre.y[0], 
                      cross_pre.x[1]) ==
                        irt::status::success);
  expect(sim->connect(presynaptic, 
                      cross_pre.x[0]) ==
                        irt::status::success);


  expect(sim->connect(quant_post.y[0], 
                      int_post.x[0]) ==
                        irt::status::success);
  expect(sim->connect(mult_post.y[0], 
                      int_post.x[1]) ==
                        irt::status::success);
  expect(sim->connect(cross_post.y[0], 
                      int_post.x[2]) ==
                        irt::status::success);
  expect(sim->connect(int_post.y[0], 
                      cross_post.x[2]) ==
                        irt::status::success);
  expect(sim->connect(cross_post.y[0], 
                      quant_post.x[0]) ==
                        irt::status::success);
  expect(sim->connect(cross_post.y[0], 
                      mult_post.x[0]) ==
                        irt::status::success);
  expect(sim->connect(const_syn.y[0], 
                      mult_post.x[1]) ==
                        irt::status::success);
  expect(sim->connect(int_post.y[0], 
                      sum_post.x[0]) ==
                        irt::status::success);
  expect(sim->connect(const_syn.y[0], 
                      sum_post.x[1]) ==
                        irt::status::success);
  expect(sim->connect(sum_post.y[0], 
                      cross_post.x[1]) ==
                        irt::status::success);
  expect(sim->connect(postsynaptic, 
                      cross_post.x[0]) ==
                        irt::status::success);


  expect(sim->connect(presynaptic, 
                      accumulator_syn.x[0]) ==
                        irt::status::success);
  expect(sim->connect(postsynaptic, 
                      accumulator_syn.x[1]) ==
                        irt::status::success);
  expect(sim->connect(cross_post.y[0], 
                      accumulator_syn.x[2]) ==
                        irt::status::success);
  expect(sim->connect(cross_pre.y[0], 
                      accumulator_syn.x[3]) ==
                        irt::status::success);
  
  return synapse_model;
} 

void network(long unsigned int N, double simulation_duration, double quantum)
{
 using namespace boost::ut;

   

      irt::simulation sim;
      // Neuron constants
      //long unsigned int N = 10;


      expect(irt::is_success(sim.init(1000000lu, 1000000lu)));


      //struct neuron neuron_model0 = make_neuron(&sim,0lu);
      
      printf(">> Allocating neurones ... ");
      auto start = std::chrono::steady_clock::now();
      // Neurons
      std::vector<irt::dynamics_id> generators;
      for (long unsigned int i = 0 ; i < N; i++) {
        auto& gen = sim.generator_models.alloc();
        gen.default_value = 3.0;
        gen.default_offset = i+1;
        gen.default_period = N+1;


        !expect(irt::is_success(sim.alloc(gen, sim.generator_models.get_id(gen))));
        
        generators.emplace_back(sim.generator_models.get_id(gen));
      } 
      auto end = std::chrono::steady_clock::now();
      printf(" [%f] ms.\n" ,static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));

      printf(">> Allocating synapses ... ");
      start = std::chrono::steady_clock::now();
      std::vector<struct synapse> synapses;
      synapses.reserve(256*N);
      for (long unsigned int i = 0 ; i < N; i++) {
        for (long unsigned int j = 0 ; j < N; j++) {

          struct synapse synapse_model = make_synapse(&sim,i,j,
                                          sim.generator_models.get(generators[i]).y[0],
                                            sim.generator_models.get(generators[j]).y[0],quantum);
          synapses.emplace_back(synapse_model);
        }
      }
      end = std::chrono::steady_clock::now();
      printf(" [%f] s.\n" ,static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(end - start).count()));
      printf(">> synapses size %ld \n",synapses.capacity());

      irt::time t = 0.0;


      printf(">> Initializing simulation ... \n");
      start = std::chrono::steady_clock::now();
      expect(irt::status::success == sim.initialize(t));
      end = std::chrono::steady_clock::now();
      printf(">> Simulation initialized in : %f ms.\n" ,static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));

      printf(">> Start running ... \n");
      start = std::chrono::steady_clock::now();
      do {

          irt::status st = sim.run(t);
          expect(st == irt::status::success);



      } while (t < simulation_duration);
      end = std::chrono::steady_clock::now();
      printf(">> Simulation done in : %f s.\n" ,static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(end - start).count()));
            
}

BENCHMARK_P(Network, N, 10, 1,(long unsigned int N, double simulation_duration, double quantum))
{
  network(N,simulation_duration,quantum);
}
BENCHMARK_P_INSTANCE(Network, N, (10,30,1e-5));
BENCHMARK_P_INSTANCE(Network, N, (100,30,1e-5));
BENCHMARK_P_INSTANCE(Network, N, (500,30,1e-5));
int
main()
{
      hayai::ConsoleOutputter consoleOutputter;

      hayai::Benchmarker::AddOutputter(consoleOutputter);
      hayai::Benchmarker::RunAllTests();

}


