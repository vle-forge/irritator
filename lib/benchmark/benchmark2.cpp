// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <cstdio>

#include <fstream>

#include <hayai.hpp>

using namespace std;

struct file_output
{
    file_output(const char* file_path) noexcept
      : os(std::fopen(file_path, "w"))
    {}

    ~file_output() noexcept
    {
        if (os)
            std::fclose(os);
    }

    std::FILE* os = nullptr;
};

void
file_output_initialize(const irt::observer& obs, const irt::time /*t*/) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<file_output*>(obs.user_data);
    fmt::print(output->os, "t,{}\n", obs.name.c_str());
}

void
file_output_observe(const irt::observer& obs,
                    const irt::time t,
                    const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<file_output*>(obs.user_data);
    fmt::print(output->os, "{},{}\n", t, msg.to_real_64(0));
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



struct neuron 
make_neuron(irt::simulation* sim, long unsigned int i, double quantum) noexcept
{
  using namespace boost::ut;
  double tau_lif = 10;
  double Vr_lif = 0.0;
  double Vt_lif = 10.0;
  /*double ref_lif = 0.5*1e-3;
  double sigma_lif = 0.02;*/




  auto& sum_lif = sim->adder_2_models.alloc();
  auto& prod_lif = sim->adder_2_models.alloc();
  auto& integrator_lif = sim->integrator_models.alloc();
  auto& quantifier_lif = sim->quantifier_models.alloc();
  auto& constant_lif = sim->constant_models.alloc();
  auto& constant_cross_lif = sim->constant_models.alloc();
  auto& cross_lif = sim->cross_models.alloc();


  sum_lif.default_input_coeffs[0] = -1.0;
  sum_lif.default_input_coeffs[1] = 20.0;
  
  prod_lif.default_input_coeffs[0] = 1.0/tau_lif;
  prod_lif.default_input_coeffs[1] = 0.0;

  constant_lif.default_value = 1.0;
  constant_cross_lif.default_value = Vr_lif;
  
  integrator_lif.default_current_value = 0.0;

  quantifier_lif.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quantifier_lif.default_zero_init_offset = true;
  quantifier_lif.default_step_size = quantum;
  quantifier_lif.default_past_length = 3;

  cross_lif.default_threshold = Vt_lif;


  

  sim->alloc(sum_lif, sim->adder_2_models.get_id(sum_lif));
  sim->alloc(prod_lif, sim->adder_2_models.get_id(prod_lif));
  sim->alloc(integrator_lif, sim->integrator_models.get_id(integrator_lif));
  sim->alloc(quantifier_lif, sim->quantifier_models.get_id(quantifier_lif));
  sim->alloc(constant_lif, sim->constant_models.get_id(constant_lif));
  sim->alloc(cross_lif, sim->cross_models.get_id(cross_lif));
  sim->alloc(constant_cross_lif, sim->constant_models.get_id(constant_cross_lif));

  struct neuron neuron_model = {sim->adder_2_models.get_id(sum_lif),
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


void lif_benchmark(double simulation_duration, double quantum)
{
    using namespace boost::ut;   

    irt::simulation sim;

    long unsigned int N = 1;
    
    expect(irt::is_success(sim.init(2600lu, 40000lu)));


    // Neurons
    std::vector<struct neuron> first_layer_neurons;
    for (long unsigned int i = 0 ; i < N; i++) {

      struct neuron neuron_model = make_neuron(&sim,i,quantum);
      first_layer_neurons.emplace_back(neuron_model);
    } 


    irt::time t = 0.0;
    std::string file_name = "output_lif_sd_"+
                            std::to_string(simulation_duration)+
                            "_q_"+std::to_string(quantum)+
                            ".csv";
    std::FILE* os = std::fopen(file_name.c_str(), "w");
    !expect(os != nullptr);
    
    std::string s = "t,";
    for (long unsigned int i = 0; i < N; i++)
    {
      s =  s + "spikes" 
            + ","+ "v" 
            + ",";
    }
    fmt::print(os, s + "\n");

    expect(irt::status::success == sim.initialize(t));

    do {

        irt::status st = sim.run(t);
        expect(st == irt::status::success);

        std::string s = std::to_string(t)+",";
        for (long unsigned int i = 0; i < N; i++)
        {
          s =  s + std::to_string(sim.cross_models.get(first_layer_neurons[i].cross).event)
            + ",";
          s =  s + std::to_string(sim.integrator_models.get(first_layer_neurons[i].integrator).last_output_value)
            + ",";

        }
        fmt::print(os, s + "\n");

    } while (t < simulation_duration);

    std::fclose(os);
}
void izhikevich_benchmark(double simulation_duration, double quantum, double a, double b, double c, double d)
{
    using namespace boost::ut;   
  irt::simulation sim;

        expect(irt::is_success(sim.init(1000lu, 1000lu)));
        expect(sim.constant_models.can_alloc(3));
        expect(sim.adder_2_models.can_alloc(3));
        expect(sim.adder_4_models.can_alloc(1));
        expect(sim.mult_2_models.can_alloc(1));
        expect(sim.integrator_models.can_alloc(2));
        expect(sim.quantifier_models.can_alloc(2));
        expect(sim.cross_models.can_alloc(2));

        auto& constant = sim.constant_models.alloc();
        auto& constant2 = sim.constant_models.alloc();
        auto& constant3 = sim.constant_models.alloc();
        auto& sum_a = sim.adder_2_models.alloc();
        auto& sum_b = sim.adder_2_models.alloc();
        auto& sum_c = sim.adder_4_models.alloc();
        auto& sum_d = sim.adder_2_models.alloc();
        auto& product = sim.mult_2_models.alloc();
        auto& integrator_a = sim.integrator_models.alloc();
        auto& integrator_b = sim.integrator_models.alloc();
        auto& quantifier_a = sim.quantifier_models.alloc();
        auto& quantifier_b = sim.quantifier_models.alloc();
        auto& cross = sim.cross_models.alloc();
        auto& cross2 = sim.cross_models.alloc();


        double I = 10.0;
        double vt = 30.0;

        constant.default_value = 1.0;
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold = vt;
        cross2.default_threshold = vt;

        integrator_a.default_current_value = 0.0;

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size = quantum;
        quantifier_a.default_past_length = 3;

        integrator_b.default_current_value = 0.0;

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size = quantum;
        quantifier_b.default_past_length = 3;

        product.default_input_coeffs[0] = 1.0;
        product.default_input_coeffs[1] = 1.0;

        sum_a.default_input_coeffs[0] = 1.0;
        sum_a.default_input_coeffs[1] = -1.0;
        sum_b.default_input_coeffs[0] = -a;
        sum_b.default_input_coeffs[1] = a * b;
        sum_c.default_input_coeffs[0] = 0.04;
        sum_c.default_input_coeffs[1] = 5.0;
        sum_c.default_input_coeffs[2] = 140.0;
        sum_c.default_input_coeffs[3] = 1.0;
        sum_d.default_input_coeffs[0] = 1.0;
        sum_d.default_input_coeffs[1] = d;

        expect(sim.models.can_alloc(14));
        !expect(irt::is_success(
          sim.alloc(constant3, sim.constant_models.get_id(constant3), "tfun")));
        !expect(irt::is_success(
          sim.alloc(constant, sim.constant_models.get_id(constant), "1.0")));
        !expect(irt::is_success(sim.alloc(
          constant2, sim.constant_models.get_id(constant2), "-56.0")));

        !expect(irt::is_success(
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a")));
        !expect(irt::is_success(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b")));
        !expect(irt::is_success(
          sim.alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c")));
        !expect(irt::is_success(
          sim.alloc(sum_d, sim.adder_2_models.get_id(sum_d), "sum_d")));

        !expect(irt::is_success(
          sim.alloc(product, sim.mult_2_models.get_id(product), "prod")));
        !expect(irt::is_success(sim.alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a")));
        !expect(irt::is_success(sim.alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b")));
        !expect(irt::is_success(sim.alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a")));
        !expect(irt::is_success(sim.alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b")));
        !expect(irt::is_success(
          sim.alloc(cross, sim.cross_models.get_id(cross), "cross")));
        !expect(irt::is_success(
          sim.alloc(cross2, sim.cross_models.get_id(cross2), "cross2")));

        !expect(sim.models.size() == 14_ul);

        expect(sim.connect(integrator_a.y[0], cross.x[0]) ==
               irt::status::success);
        expect(sim.connect(constant2.y[0], cross.x[1]) == irt::status::success);
        expect(sim.connect(integrator_a.y[0], cross.x[2]) ==
               irt::status::success);

        expect(sim.connect(cross.y[0], quantifier_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(cross.y[0], product.x[0]) == irt::status::success);
        expect(sim.connect(cross.y[0], product.x[1]) == irt::status::success);
        expect(sim.connect(product.y[0], sum_c.x[0]) == irt::status::success);
        expect(sim.connect(cross.y[0], sum_c.x[1]) == irt::status::success);
        expect(sim.connect(cross.y[0], sum_b.x[1]) == irt::status::success);

        expect(sim.connect(constant.y[0], sum_c.x[2]) == irt::status::success);
        expect(sim.connect(constant3.y[0], sum_c.x[3]) == irt::status::success);

        expect(sim.connect(sum_c.y[0], sum_a.x[0]) == irt::status::success);
        expect(sim.connect(integrator_b.y[0], sum_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(cross2.y[0], sum_a.x[1]) == irt::status::success);
        expect(sim.connect(sum_a.y[0], integrator_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(cross.y[0], integrator_a.x[2]) ==
               irt::status::success);
        expect(sim.connect(quantifier_a.y[0], integrator_a.x[0]) ==
               irt::status::success);

        expect(sim.connect(cross2.y[0], quantifier_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(cross2.y[0], sum_b.x[0]) == irt::status::success);
        expect(sim.connect(quantifier_b.y[0], integrator_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(sum_b.y[0], integrator_b.x[1]) ==
               irt::status::success);

        expect(sim.connect(cross2.y[0], integrator_b.x[2]) ==
               irt::status::success);
        expect(sim.connect(integrator_a.y[0], cross2.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0], cross2.x[2]) ==
               irt::status::success);
        expect(sim.connect(sum_d.y[0], cross2.x[1]) == irt::status::success);
        expect(sim.connect(integrator_b.y[0], sum_d.x[0]) ==
               irt::status::success);
        expect(sim.connect(constant.y[0], sum_d.x[1]) == irt::status::success);

        std::string file_name = "output_izhikevitch_a_sd_"+
                                std::to_string(simulation_duration)+
                                "_q_"+std::to_string(quantum)+
                                "_a_"+std::to_string(a)+
                                "_b_"+std::to_string(b)+
                                "_c_"+std::to_string(c)+
                                "_d_"+std::to_string(d)+
                                ".csv";
        file_output fo_a(file_name.c_str());
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc(0.01,
                                          "A",
                                          static_cast<void*>(&fo_a),
                                          &file_output_initialize,
                                          &file_output_observe,
                                          nullptr);
        file_name = "output_izhikevitch_b_sd_"+
                                std::to_string(simulation_duration)+
                                "_q_"+std::to_string(quantum)+
                                "_a_"+std::to_string(a)+
                                "_b_"+std::to_string(b)+
                                "_c_"+std::to_string(c)+
                                "_d_"+std::to_string(d)+
                                ".csv";
        file_output fo_b(file_name.c_str());
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc(0.01,
                                          "B",
                                          static_cast<void*>(&fo_b),
                                          &file_output_initialize,
                                          &file_output_observe,
                                          nullptr);

        sim.observe(sim.models.get(integrator_a.id), obs_a);
        sim.observe(sim.models.get(integrator_b.id), obs_b);

        irt::time t = 0.0;

        expect(irt::status::success == sim.initialize(t));
        !expect(sim.sched.size() == 14_ul);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < simulation_duration);
};
BENCHMARK_P(LIF, 1, 10, 1,( double simulation_duration, double quantum))
{
  lif_benchmark(simulation_duration,quantum);
}
BENCHMARK_P(Izhikevich, Type, 1, 1,( double simulation_duration, double quantum, double a, double b, double c, double d))
{
  izhikevich_benchmark(simulation_duration,quantum,a,b,c,d);
}

BENCHMARK_P_INSTANCE(LIF, 1, (30,1e-2));
// Regular spiking (RS)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.02,0.2,-65.0,8.0));
// Intrinsical bursting (IB)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.02,0.2,-55.0,4.0));
// Chattering spiking (CH)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.02,0.2,-50.0,2.0));
// Fast spiking (FS)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.1,0.2,-65.0,2.0));
// Thalamo-Cortical (TC)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.02,0.25,-65.0,0.05));
// Rezonator (RZ)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.1,0.26,-65.0,2.0));
// Low-threshold spiking (LTS)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.02,0.25,-65.0,2.0));
// Problematic (P)
BENCHMARK_P_INSTANCE(Izhikevich, Type, (1000,1e-2,0.2,2,-56.0,-16.0));

int
main()
{
      hayai::ConsoleOutputter consoleOutputter;

      hayai::Benchmarker::AddOutputter(consoleOutputter);
      hayai::Benchmarker::RunAllTests();

}


