// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

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

/**
 * Reads csv file into table, exported as a vector of vector of doubles.
 * @param inputFileName input file name (full path).
 * @return data as vector of vector of doubles.
 */
vector<vector<double>> parse2DCsvFile(string inputFileName) {
 
    vector<vector<double> > data;
    ifstream inputFile(inputFileName);
    if (!inputFile.is_open())
        return {};

    int l = 0;
 
    while (inputFile) {
        l++;
        string s;
        if (!getline(inputFile, s)) break;
        if (s[0] != '#') {
            istringstream ss(s);
            vector<double> record;
 
            while (ss) {
                string line;
                if (!getline(ss, line, ','))
                    break;
                try {
                    record.push_back(stof(line));
                }
                catch (const std::invalid_argument e) {
                    cout << "NaN found in file " << inputFileName << " line " << l
                         << ' ' << e.what() << endl;
                }
            }
 
            data.push_back(record);
        }
    }
 
    return data;
}
struct quantized_data {
   vector<vector<double>> data;
   vector<vector<double>>  sigmas;
   double total_time;
};
/**
 * Quantifies data following the rule delta * floor( x / delta + 1/2) .
 * @param data input data, each ligne will be quantized.
 * @param delta the quantization step.
 * @param default_samplerate the quantization step.
 * @return quantized data as vector of vector of doubles.
 */
struct quantized_data 
quantify_data(vector<vector<double>> data, double delta, double default_samplerate) {
   double default_frequency = 1.0/default_samplerate;
   // Initialize the matrix of quantified data
   vector<vector<double>> quantified_data;
   vector<vector<double>> quantified_sigmas;

   // Loop over all the lines, each ligne will be quantized
   for (int i = 1; i < data.size(); i++) {
        double prev = data[i][0];
        double sigma = default_frequency;
        double accu_sigma = 0.0;
        vector<double> quantified_vector;
        vector<double> sigma_vector;
        for (int j = 1; j < data[i].size(); j++) {
                double q = delta * (static_cast<int>(data[i][j]/delta) + 0.5);
                sigma += default_frequency;
                if (prev != q) {
                        quantified_vector.push_back(prev);
                        quantified_vector.push_back(q);
                        sigma_vector.push_back(sigma);                        
                        sigma_vector.push_back(default_frequency);

                        accu_sigma += sigma+default_frequency;
                        sigma = 0.0;

                }
                prev = q;
        }
        quantified_vector.push_back(quantified_vector[quantified_vector.size() - 1]);
        sigma_vector.push_back(std::abs(data[i].size() * default_frequency - accu_sigma));

        quantified_data.push_back(quantified_vector);
        quantified_sigmas.push_back(sigma_vector);
   }

    struct quantized_data result = {quantified_data,
                                    quantified_sigmas,
                                    data[0].size() * default_frequency };
    return result;
}
// Global data
double samplerate = 44100.0;
struct quantized_data quantified_inputs =  quantify_data(parse2DCsvFile("output_cochlea_small.csv"),1e-2,samplerate);
vector<vector<double>> sound_data  = quantified_inputs.data;
vector<vector<double>> sound_data_sigmas  = quantified_inputs.sigmas;

vector<vector<double>> link_data  = parse2DCsvFile("output_link_small.csv");


struct neuron {
  irt::dynamics_id  sum;
  irt::dynamics_id  prod;
  irt::dynamics_id  integrator;
  irt::dynamics_id  quantifier;
  irt::dynamics_id  constant;
  irt::dynamics_id  flow;
  irt::dynamics_id  cross;
  irt::dynamics_id  constant_cross;
};

struct neuron_adaptive {
  irt::dynamics_id  sum1;
  irt::dynamics_id  sum2;
  irt::dynamics_id  integrator1;
  irt::dynamics_id  integrator2;
  irt::dynamics_id  quantifier1;
  irt::dynamics_id  quantifier2;
  irt::dynamics_id  constant;
  irt::dynamics_id  cross;
  irt::dynamics_id  constant_cross;
};

struct synapse {
  irt::dynamics_id  sum_pre;
  irt::dynamics_id  cross_pre;

  irt::dynamics_id  constant_syn;
};

struct neuron 
make_neuron(irt::simulation* sim, long unsigned int i) noexcept
{
  using namespace boost::ut;
  double tau_lif = 1.5*0.001;
  double Vr_lif = 0.0;
  double Vt_lif = 1.0;
  /*double ref_lif = 0.5*1e-3;
  double sigma_lif = 0.02;*/




  auto& sum_lif = sim->adder_2_models.alloc();
  auto& prod_lif = sim->adder_2_models.alloc();
  auto& integrator_lif = sim->integrator_models.alloc();
  auto& quantifier_lif = sim->quantifier_models.alloc();
  auto& constant_lif = sim->constant_models.alloc();
  auto& flow_lif = sim->flow_models.alloc();
  auto& constant_cross_lif = sim->constant_models.alloc();
  auto& cross_lif = sim->cross_models.alloc();


  sum_lif.default_input_coeffs[0] = -1.0;
  sum_lif.default_input_coeffs[1] = 1.0;
  
  prod_lif.default_input_coeffs[0] = 1.0/tau_lif;
  prod_lif.default_input_coeffs[1] = 0.0;

  constant_lif.default_value = 1.0;
  flow_lif.default_data = sound_data[i];
  flow_lif.default_samplerate = samplerate;
  flow_lif.default_sigmas = sound_data_sigmas[i];
  constant_cross_lif.default_value = Vr_lif;
  
  integrator_lif.default_current_value = 0.0;

  quantifier_lif.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quantifier_lif.default_zero_init_offset = true;
  quantifier_lif.default_step_size = 0.1;
  quantifier_lif.default_past_length = 3;

  cross_lif.default_threshold = Vt_lif;

  

  sim->alloc(sum_lif, sim->adder_2_models.get_id(sum_lif));
  sim->alloc(prod_lif, sim->adder_2_models.get_id(prod_lif));
  sim->alloc(integrator_lif, sim->integrator_models.get_id(integrator_lif));
  sim->alloc(quantifier_lif, sim->quantifier_models.get_id(quantifier_lif));
  sim->alloc(constant_lif, sim->constant_models.get_id(constant_lif));
  sim->alloc(flow_lif, sim->flow_models.get_id(flow_lif));
  sim->alloc(cross_lif, sim->cross_models.get_id(cross_lif));
  sim->alloc(constant_cross_lif, sim->constant_models.get_id(constant_cross_lif));

  struct neuron neuron_model = {sim->adder_2_models.get_id(sum_lif),
                                sim->adder_2_models.get_id(prod_lif),
                                sim->integrator_models.get_id(integrator_lif),
                                sim->quantifier_models.get_id(quantifier_lif),
                                sim->constant_models.get_id(constant_lif),
                                sim->flow_models.get_id(flow_lif),
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
  expect(sim->connect(flow_lif.y[0], sum_lif.x[1]) ==
          irt::status::success);  
  expect(sim->connect(sum_lif.y[0],prod_lif.x[0]) ==
          irt::status::success);   
  expect(sim->connect(constant_lif.y[0],prod_lif.x[1]) ==
          irt::status::success);
  return neuron_model;
}

struct neuron_adaptive 
make_neuron_adaptive(irt::simulation* sim, long unsigned int i) noexcept
{
  using namespace boost::ut;
  double tau_lif = 0.5*0.001;
  double Vr_lif = 0.0;
  double Vt_lif = 2.0;

  double tau_threshold = 5*0.001;


  auto& sum_lif = sim->adder_2_models.alloc();
  auto& integrator_lif = sim->integrator_models.alloc();
  auto& quantifier_lif = sim->quantifier_models.alloc();
  auto& constant_cross_lif = sim->constant_models.alloc();
  auto& cross_lif = sim->cross_models.alloc();

  auto& sum_threshold = sim->adder_3_models.alloc();
  auto& integrator_threshold = sim->integrator_models.alloc();
  auto& quantifier_threshold = sim->quantifier_models.alloc();

  auto& constant = sim->constant_models.alloc();

  //LIF
  sum_lif.default_input_coeffs[0] = -1.0/tau_lif;
  sum_lif.default_input_coeffs[1] = 0.0/tau_lif;
  
  integrator_lif.default_current_value = 0.0;

  quantifier_lif.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quantifier_lif.default_zero_init_offset = true;
  quantifier_lif.default_step_size = 0.1;
  quantifier_lif.default_past_length = 3;

  constant_cross_lif.default_value = Vr_lif;
  
  cross_lif.default_threshold = Vt_lif;


  //Threshold
  sum_threshold.default_input_coeffs[0] = -1.0/tau_threshold;
  sum_threshold.default_input_coeffs[1] = 1.0/tau_threshold;
  sum_threshold.default_input_coeffs[2] = 0.0/tau_threshold;
  
  integrator_threshold.default_current_value = Vt_lif;

  quantifier_threshold.default_adapt_state =
    irt::quantifier::adapt_state::possible;
  quantifier_threshold.default_zero_init_offset = true;
  quantifier_threshold.default_step_size = 0.1;
  quantifier_threshold.default_past_length = 3;

  constant.default_value = 1.0;


  sim->alloc(sum_lif, sim->adder_2_models.get_id(sum_lif));
  sim->alloc(integrator_lif, sim->integrator_models.get_id(integrator_lif));
  sim->alloc(quantifier_lif, sim->quantifier_models.get_id(quantifier_lif));
  sim->alloc(cross_lif, sim->cross_models.get_id(cross_lif));
  sim->alloc(constant_cross_lif, sim->constant_models.get_id(constant_cross_lif));

  sim->alloc(sum_threshold, sim->adder_3_models.get_id(sum_threshold));
  sim->alloc(integrator_threshold, sim->integrator_models.get_id(integrator_threshold));
  sim->alloc(quantifier_threshold, sim->quantifier_models.get_id(quantifier_threshold));

  sim->alloc(constant, sim->constant_models.get_id(constant));

  struct neuron_adaptive neuron_model = {sim->adder_2_models.get_id(sum_lif),
                                sim->adder_3_models.get_id(sum_threshold),
                                sim->integrator_models.get_id(integrator_lif),
                                sim->integrator_models.get_id(integrator_threshold),
                                sim->quantifier_models.get_id(quantifier_lif),
                                sim->quantifier_models.get_id(quantifier_threshold),                                
                                sim->constant_models.get_id(constant),
                                sim->cross_models.get_id(cross_lif),                                                                                                                             
                                sim->constant_models.get_id(constant_cross_lif),                                                                
                                }; 


  // Connections
  expect(sim->connect(quantifier_lif.y[0], integrator_lif.x[0]) ==
          irt::status::success);
  expect(sim->connect(cross_lif.y[0], integrator_lif.x[2]) ==
          irt::status::success);
  expect(sim->connect(cross_lif.y[0], quantifier_lif.x[0]) ==
          irt::status::success);
  expect(sim->connect(cross_lif.y[0], sum_lif.x[0]) ==
          irt::status::success);
  /*expect(sim->connect(integrator_lif.y[0],cross_lif.x[0]) ==
          irt::status::success);     
  expect(sim->connect(integrator_lif.y[0],cross_lif.x[2]) ==
          irt::status::success);*/
  expect(sim->connect(constant_cross_lif.y[0],cross_lif.x[1]) ==
          irt::status::success); 
  expect(sim->connect(sum_lif.y[0],integrator_lif.x[1]) ==
          irt::status::success);

  expect(sim->connect(quantifier_threshold.y[0], integrator_threshold.x[0]) ==
          irt::status::success);
  expect(sim->connect(integrator_threshold.y[0], quantifier_threshold.x[0]) ==
          irt::status::success);
  expect(sim->connect(integrator_threshold.y[0], sum_threshold.x[0]) ==
          irt::status::success);
  expect(sim->connect(sum_threshold.y[0],integrator_threshold.x[1]) ==
          irt::status::success);

  expect(sim->connect(constant.y[0],sum_lif.x[1]) ==
          irt::status::success);     
  expect(sim->connect(constant.y[0],sum_threshold.x[1]) ==
          irt::status::success);   
  //expect(sim->connect(integrator_lif.y[0],sum_threshold.x[2]) ==
          //irt::status::success);   

  expect(sim->connect(integrator_threshold.y[0],cross_lif.x[3]) ==
          irt::status::success);

  return neuron_model;
}

struct synapse make_synapse(irt::simulation* sim, long unsigned int source, long unsigned int target,
                              irt::output_port_id presynaptic,irt::input_port_id postsynaptic1,irt::input_port_id postsynaptic2,irt::output_port_id other)
{
  using namespace boost::ut;


  double w = 0.7;

  auto& sum_pre = sim->adder_2_models.alloc();
  auto& cross_pre = sim->cross_models.alloc();

  auto& const_syn = sim->constant_models.alloc();



  cross_pre.default_threshold = 1.0;
  sum_pre.default_input_coeffs[0] = 1.0;
  sum_pre.default_input_coeffs[1] = w;

  const_syn.default_value = 1.0;





  !expect(irt::is_success(sim->alloc(sum_pre, sim->adder_2_models.get_id(sum_pre))));
  !expect(irt::is_success(sim->alloc(cross_pre, sim->cross_models.get_id(cross_pre))));


  !expect(irt::is_success(sim->alloc(const_syn, sim->constant_models.get_id(const_syn))));


  struct synapse synapse_model = {sim->adder_2_models.get_id(sum_pre),
                                  sim->cross_models.get_id(cross_pre),  

                                  sim->constant_models.get_id(const_syn),                                                                                                                                            
                                  }; 


  // Connections
  expect(sim->connect(other, 
                      sum_pre.x[0]) ==
                        irt::status::success);
  expect(sim->connect(other, 
                      cross_pre.x[2]) ==
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
  expect(sim->connect(cross_pre.y[0], 
                      postsynaptic1) ==
                        irt::status::success);
  expect(sim->connect(cross_pre.y[0], 
                      postsynaptic2) ==
                        irt::status::success);
  
  return synapse_model;
} 

int
main()
{
    using namespace boost::ut;
    /*std::FILE* os = std::fopen("output_nm.csv", "w");
    !expect(os != nullptr);
    std::string s = "t,data";

    fmt::print(os, s + "\n");
    double t = 0;
    for (int j = 0; j < sound_data[100].size(); j++) {
        t  = t + sound_data_sigmas[100][j];
        std::string s = std::to_string(t)+","+std::to_string(sound_data[100][j])+",";

        fmt::print(os, s + "\n");

    }
    std::fclose(os);*/

    if (sound_data.empty() || sound_data_sigmas.empty() || link_data.empty())
        return 0;
   
    "laudanski_1_simulation"_test = [] {
        irt::simulation sim;

        // Neuron constants
        long unsigned int N = sound_data.size() ;
        long unsigned int M = link_data[0].size();

        expect(irt::is_success(sim.init(1000000lu, 100000lu)));

        // Neurons
        std::vector<struct neuron> first_layer_neurons;
        for (long unsigned int i = 0 ; i < N; i++) {

          struct neuron neuron_model = make_neuron(&sim,i);
          first_layer_neurons.emplace_back(neuron_model);
        } 

        std::vector<struct neuron_adaptive> second_layer_neurons;
        for (long unsigned int i = 0 ; i < M; i++) {

          struct neuron_adaptive neuron_adaptive_model = make_neuron_adaptive(&sim,i);
          second_layer_neurons.emplace_back(neuron_adaptive_model);
        } 

        std::vector<struct synapse> synapses;
        for (long unsigned int i = 0 ; i < N; i++) {
          for (long unsigned int j = 0 ; j < M; j++) {
            if(link_data[i][j] == 1.0){
                //printf("%ld is linked to %ld\n",i,j);
                       
                struct synapse synapse_model = make_synapse(&sim,i,j,
                                                sim.cross_models.get(first_layer_neurons[i].cross).y[1],
                                                sim.cross_models.get(second_layer_neurons[j].cross).x[0],
                                                sim.cross_models.get(second_layer_neurons[j].cross).x[2],
                                                        sim.integrator_models.get(second_layer_neurons[j].integrator1).y[0]);
                synapses.emplace_back(synapse_model);
            }
          }
        }

        //dot_graph_save(sim, stdout);


        file_output fo_a("output_laudanski.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc(0.0001,
                                "A",
                                static_cast<void*>(&fo_a),
                                file_output_initialize,
                                &file_output_observe,
                                nullptr);

        sim.observe(sim.models.get(sim.integrator_models.get(first_layer_neurons[0].integrator).id), obs_a);

        irt::time t = 0.0;
        expect(irt::status::success == sim.initialize(t));

        do {

            irt::status st = sim.run(t);
            expect(st == irt::status::success);

        } while (t <  quantified_inputs.total_time);

      };

   
}


