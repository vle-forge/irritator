// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <hayai.hpp>

#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <cstdio>

#include <fstream>


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
    fmt::print(output->os, "{},{}\n", t, msg.real[0]);
}


struct neuron {
  irt::dynamics_id  sum;
  irt::dynamics_id  integrator;
  irt::dynamics_id  constant;
  irt::dynamics_id  cross;
  irt::dynamics_id  constant_cross;
};


struct neuron 
make_neuron(irt::simulation* sim, long unsigned int i, double quantum) noexcept
{
  using namespace boost::ut;
  double tau_lif = 10.0;
  double Vr_lif = 0.0;
  double Vt_lif = 10.0;
  double V0 = 20.0;


  auto& sum_lif = sim->qss1_wsum_2_models.alloc();
  auto& integrator_lif = sim->qss1_integrator_models.alloc();
  auto& constant_lif = sim->constant_models.alloc();
  auto& constant_cross_lif = sim->constant_models.alloc();
  auto& cross_lif = sim->qss1_cross_models.alloc();


  sum_lif.default_input_coeffs[0] = -1.0/tau_lif;
  sum_lif.default_input_coeffs[1] = V0/tau_lif;
  


  constant_lif.default_value = 1.0;
  constant_cross_lif.default_value = Vr_lif;
  
  integrator_lif.default_X = 0.0;
  integrator_lif.default_dQ = quantum;

  cross_lif.default_threshold = Vt_lif;


  

  sim->alloc(sum_lif, sim->qss1_wsum_2_models.get_id(sum_lif));
  sim->alloc(integrator_lif, sim->qss1_integrator_models.get_id(integrator_lif));
  sim->alloc(constant_lif, sim->constant_models.get_id(constant_lif));
  sim->alloc(cross_lif, sim->qss1_cross_models.get_id(cross_lif));
  sim->alloc(constant_cross_lif, sim->constant_models.get_id(constant_cross_lif));

  struct neuron neuron_model = {sim->qss1_wsum_2_models.get_id(sum_lif),
                                sim->qss1_integrator_models.get_id(integrator_lif),
                                sim->constant_models.get_id(constant_lif),
                                sim->qss1_cross_models.get_id(cross_lif),                                                                                                
                                sim->constant_models.get_id(constant_cross_lif),                                                                
                                }; 


  // Connections
  expect(sim->connect(cross_lif.y[0], integrator_lif.x[1]) ==
        irt::status::success);
  expect(sim->connect(cross_lif.y[1], sum_lif.x[0]) ==
        irt::status::success);
  expect(sim->connect(integrator_lif.y[0],cross_lif.x[0]) ==
        irt::status::success);     
  expect(sim->connect(integrator_lif.y[0],cross_lif.x[2]) ==
        irt::status::success);
  expect(sim->connect(constant_cross_lif.y[0],cross_lif.x[1]) ==
        irt::status::success);     
  expect(sim->connect(constant_lif.y[0], sum_lif.x[1]) ==
        irt::status::success); 
  expect(sim->connect(sum_lif.y[0], integrator_lif.x[0]) ==
        irt::status::success); 
  return neuron_model;
}


void lif_benchmark(double simulation_duration, double quantum)
{
    using namespace boost::ut;   

    irt::simulation sim;
    expect(irt::is_success(sim.init(2600lu, 40000lu)));

    struct neuron neuron_model = make_neuron(&sim,0,quantum);


    irt::time t = 0.0;
    std::string file_name = "output_lif_qss1_sd_"+
                            std::to_string(simulation_duration)+
                            "_q_"+std::to_string(quantum)+
                            ".csv";
    file_output fo_a(file_name.c_str());
    expect(fo_a.os != nullptr);

    auto& obs_a = sim.observers.alloc(0.1,
                                        "A",
                                        static_cast<void*>(&fo_a),
                                        &file_output_initialize,
                                        &file_output_observe,
                                        nullptr);
    sim.observe(sim.models.get(sim.qss1_integrator_models.get(neuron_model.integrator).id), obs_a);

    expect(irt::status::success == sim.initialize(t));

    do {

        irt::status st = sim.run(t);
        expect(st == irt::status::success);

    } while (t < simulation_duration);
}
void izhikevich_benchmark(double simulation_duration, double quantum, double a, double b, double c, double d, double I, double vini)
{
  using namespace boost::ut;   
  irt::simulation sim;
  expect(irt::is_success(sim.init(1000lu, 1000lu)));
  auto& constant = sim.constant_models.alloc();
  auto& constant2 = sim.constant_models.alloc();
  auto& constant3 = sim.constant_models.alloc();
  auto& sum_a = sim.qss1_wsum_2_models.alloc();
  auto& sum_b = sim.qss1_wsum_2_models.alloc();
  auto& sum_c = sim.qss1_wsum_4_models.alloc();
  auto& sum_d = sim.qss1_wsum_2_models.alloc();
  auto& product = sim.qss1_multiplier_models.alloc();
  auto& integrator_a = sim.qss1_integrator_models.alloc();
  auto& integrator_b = sim.qss1_integrator_models.alloc();
  auto& cross = sim.qss1_cross_models.alloc();
  auto& cross2 = sim.qss1_cross_models.alloc();


  double vt = 30.0;

  constant.default_value = 1.0;
  constant2.default_value = c;
  constant3.default_value = I;

  cross.default_threshold = vt;
  cross2.default_threshold = vt;

  integrator_a.default_X = vini;
  integrator_a.default_dQ = quantum;

  integrator_b.default_X = vini;
  integrator_b.default_dQ = quantum;

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


  sim.alloc(constant3, sim.constant_models.get_id(constant3));
  sim.alloc(constant, sim.constant_models.get_id(constant));
  sim.alloc(constant2, sim.constant_models.get_id(constant2));
  sim.alloc(sum_a, sim.qss1_wsum_2_models.get_id(sum_a));
  sim.alloc(sum_b, sim.qss1_wsum_2_models.get_id(sum_b));
  sim.alloc(sum_c, sim.qss1_wsum_4_models.get_id(sum_c));
  sim.alloc(sum_d, sim.qss1_wsum_2_models.get_id(sum_d));
  sim.alloc(product, sim.qss1_multiplier_models.get_id(product));
  sim.alloc(integrator_a, sim.qss1_integrator_models.get_id(integrator_a));
  sim.alloc(integrator_b, sim.qss1_integrator_models.get_id(integrator_b));
  sim.alloc(cross, sim.qss1_cross_models.get_id(cross));
  sim.alloc(cross2, sim.qss1_cross_models.get_id(cross2));


  expect(sim.connect(integrator_a.y[0], cross.x[0]) ==
          irt::status::success);
  expect(sim.connect(constant2.y[0], cross.x[1]) == irt::status::success);
  expect(sim.connect(integrator_a.y[0], cross.x[2]) ==
          irt::status::success);

  expect(sim.connect(cross.y[1], product.x[0]) == irt::status::success);
  expect(sim.connect(cross.y[1], product.x[1]) == irt::status::success);
  expect(sim.connect(product.y[0], sum_c.x[0]) == irt::status::success);
  expect(sim.connect(cross.y[1], sum_c.x[1]) == irt::status::success);
  expect(sim.connect(cross.y[1], sum_b.x[1]) == irt::status::success);

  expect(sim.connect(constant.y[0], sum_c.x[2]) == irt::status::success);
  expect(sim.connect(constant3.y[0], sum_c.x[3]) == irt::status::success);

  expect(sim.connect(sum_c.y[0], sum_a.x[0]) == irt::status::success);
  // expect(sim.connect(integrator_b.y[0], sum_a.x[1]) ==
  // irt::status::success);
  expect(sim.connect(cross2.y[1], sum_a.x[1]) == irt::status::success);
  expect(sim.connect(sum_a.y[0], integrator_a.x[0]) ==
          irt::status::success);
  expect(sim.connect(cross.y[0], integrator_a.x[1]) ==
          irt::status::success);

  expect(sim.connect(cross2.y[1], sum_b.x[0]) == irt::status::success);
  expect(sim.connect(sum_b.y[0], integrator_b.x[0]) ==
          irt::status::success);

  expect(sim.connect(cross2.y[0], integrator_b.x[1]) ==
          irt::status::success);
  expect(sim.connect(integrator_a.y[0], cross2.x[0]) ==
          irt::status::success);
  expect(sim.connect(integrator_b.y[0], cross2.x[2]) ==
          irt::status::success);
  expect(sim.connect(sum_d.y[0], cross2.x[1]) == irt::status::success);
  expect(sim.connect(integrator_b.y[0], sum_d.x[0]) ==
          irt::status::success);
  expect(sim.connect(constant.y[0], sum_d.x[1]) == irt::status::success);



  std::string file_name = "output_izhikevitch_qss1_a_sd_"+
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
  file_name = "output_izhikevitch_qss1_b_sd_"+
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
  !expect(sim.sched.size() == 12_ul);

  do {
      irt::status st = sim.run(t);
      expect(st == irt::status::success);
  } while (t < simulation_duration);
};

BENCHMARK_P(LIF, QSS1, 10, 1,( double simulation_duration, double quantum))
{
  lif_benchmark(simulation_duration,quantum);
}
BENCHMARK_P(Izhikevich, QSS1, 1, 1,( double simulation_duration, double quantum, double a, double b, double c, double d, double I, double vini))
{
  izhikevich_benchmark(simulation_duration,quantum,a,b,c,d,I,vini);
}

BENCHMARK_P_INSTANCE(LIF, QSS1, (1000,1e-2));
// Regular spiking (RS)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.02,0.2,-65.0,8.0,10.0,0.0));
// Intrinsical bursting (IB)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.02,0.2,-55.0,4.0,10.0,0.0));
// Chattering spiking (CH)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.02,0.2,-50.0,2.0,10.0,0.0));
// Fast spiking (FS)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.1,0.2,-65.0,2.0,10.0,0.0));
// Thalamo-Cortical (TC)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.02,0.25,-65.0,0.05,10.0,-87.0));
// Rezonator (RZ)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.1,0.26,-65.0,2.0,10.0,-63.0));
// Low-threshold spiking (LTS)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.02,0.25,-65.0,2.0,10.0,-63.0));
// Problematic (P)
BENCHMARK_P_INSTANCE(Izhikevich, QSS1, (1000,1e-2,0.2,2,-56.0,-16.0,-99.0,0.0));

int
main()
{
      hayai::ConsoleOutputter consoleOutputter;

      hayai::Benchmarker::AddOutputter(consoleOutputter);
      hayai::Benchmarker::RunAllTests();

}


