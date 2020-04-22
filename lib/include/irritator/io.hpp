// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>

#include <algorithm>

#include <cstdio>

namespace irt {

class reader
{
private:
    ::std::FILE* file = stdin;

    array<model_id> map;
    int model_error = 0;
    int connection_error = 0;

public:
    reader(::std::FILE* file_ = stdin) noexcept
      : file(file_ == nullptr ? stdin : file_)
    {}

    ~reader() noexcept
    {
        if (file && file != stdin)
            ::std::fclose(file);
    }

    status operator()(simulation& sim) noexcept
    {
        int model_number = 0;

        irt_return_if_fail(std::fscanf(file, "%d", &model_number) == 1,
                           status::io_file_format_error);
        irt_return_if_fail(model_number > 0,
                           status::io_file_format_model_number_error);

        irt_return_if_bad(map.init(model_number));

        std::fill_n(std::begin(map), std::size(map), static_cast<model_id>(0));

        int id;
        char name[8];
        char dynamics[11];

        for (int i = 0; i != model_number; ++i, ++model_error) {
            irt_return_if_fail(
              3 == std::fscanf(file, "%d %7s %10s", &id, name, dynamics),
              status::io_file_format_model_error);

            irt_return_if_fail(0 <= id && id < model_number,
                               status::io_file_format_model_error);

            irt_return_if_bad(read(sim, id, name, dynamics));
        }

        while (!std::feof(file)) {
            int mdl_src_index, port_src_index, mdl_dst_index, port_dst_index;

            int read = std::fscanf(file,
                                   "%d %d %d %d",
                                   &mdl_src_index,
                                   &port_src_index,
                                   &mdl_dst_index,
                                   &port_dst_index);

            irt_return_if_fail(read == -1 || read == 0 || read == 4,
                               status::io_file_format_error);

            if (read == -1)
                break;

            if (read == 4) {
                auto* mdl_src = sim.models.try_to_get(mdl_src_index);
                irt_return_if_fail(mdl_src,
                                   status::io_file_format_model_unknown);

                auto* mdl_dst = sim.models.try_to_get(mdl_dst_index);
                irt_return_if_fail(mdl_dst,
                                   status::io_file_format_model_unknown);

                output_port_id output_port;
                input_port_id input_port;

                irt_return_if_bad(sim.get_output_port_id(
                  *mdl_src, port_src_index, &output_port));
                irt_return_if_bad(
                  sim.get_input_port_id(*mdl_dst, port_dst_index, &input_port));

                irt_return_if_bad(sim.connect(output_port, input_port));
                ++connection_error;
            }
        }

        return status::success;
    }

private:
    bool convert(const char* dynamics_name, dynamics_type* type) noexcept
    {
        if (std::strcmp(dynamics_name, "none") == 0) {
            *type = dynamics_type::none;
            return true;
        }

        if (std::strcmp(dynamics_name, "integrator") == 0) {
            *type = dynamics_type::integrator;
            return true;
        }

        if (std::strcmp(dynamics_name, "quantifier") == 0) {
            *type = dynamics_type::quantifier;
            return true;
        }

        if (std::strcmp(dynamics_name, "adder_2") == 0) {
            *type = dynamics_type::adder_2;
            return true;
        }

        if (std::strcmp(dynamics_name, "adder_3") == 0) {
            *type = dynamics_type::adder_3;
            return true;
        }

        if (std::strcmp(dynamics_name, "adder_4") == 0) {
            *type = dynamics_type::adder_4;
            return true;
        }

        if (std::strcmp(dynamics_name, "mult_2") == 0) {
            *type = dynamics_type::mult_2;
            return true;
        }

        if (std::strcmp(dynamics_name, "mult_3") == 0) {
            *type = dynamics_type::mult_3;
            return true;
        }

        if (std::strcmp(dynamics_name, "mult_4") == 0) {
            *type = dynamics_type::mult_4;
            return true;
        }

        if (std::strcmp(dynamics_name, "counter") == 0) {
            *type = dynamics_type::counter;
            return true;
        }

        if (std::strcmp(dynamics_name, "generator") == 0) {
            *type = dynamics_type::generator;
            return true;
        }

        if (std::strcmp(dynamics_name, "constant") == 0) {
            *type = dynamics_type::constant;
            return true;
        }

        if (std::strcmp(dynamics_name, "cross") == 0) {
            *type = dynamics_type::cross;
            return true;
        }

        if (std::strcmp(dynamics_name, "time_func") == 0) {
            *type = dynamics_type::time_func;
            return true;
        }

        return false;
    }

    status read(simulation& sim,
                int id,
                const char* name,
                const char* dynamics_name) noexcept
    {
        dynamics_type type;

        irt_return_if_fail(convert(dynamics_name, &type),
                           status::io_file_format_dynamics_unknown);

        model_id mdl = static_cast<model_id>(0);
        auto ret = sim.dispatch(type, [this, &sim, name](auto& dyn_models) {
            irt_return_if_fail(dyn_models.can_alloc(1),
                               status::io_file_format_dynamics_limit_reach);
            auto& dyn = dyn_models.alloc();
            auto dyn_id = dyn_models.get_id(dyn);

            sim.alloc(dyn, dyn_id, name);

            irt_return_if_fail(read(dyn),
                               status::io_file_format_dynamics_init_error);

            return status::success;
        });

        irt_return_if_bad(ret);

        map[id] = mdl;

        return status::success;
    }

    bool read(none& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(integrator& dyn) noexcept
    {
        return 2 == std::fscanf(file,
                                "%lf %lf",
                                &dyn.default_current_value,
                                &dyn.default_reset_value);
    }

    bool read(quantifier& dyn) noexcept
    {
        char state[16];
        char init[8];

        int ret = std::fscanf(file,
                              "%lf %d %15s %7s",
                              &dyn.default_step_size,
                              &dyn.default_past_length,
                              state,
                              init);

        if (ret != 4)
            return false;

        if (std::strcmp(state, "possible") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::possible;
        else if (std::strcmp(state, "impossible") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::impossible;
        else if (std::strcmp(state, "done") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::done;
        else
            return false;

        if (std::strcmp(init, "true") == 0)
            dyn.default_zero_init_offset = true;
        else if (std::strcmp(init, "false") == 0)
            dyn.default_zero_init_offset = false;
        else
            return false;

        return true;
    }

    bool read(adder_2& dyn) noexcept
    {
        return 4 == std::fscanf(file,
                                "%lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1]);
    }

    bool read(adder_3& dyn) noexcept
    {
        return 6 == std::fscanf(file,
                                "%lf %lf %lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_values[2],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1],
                                &dyn.default_input_coeffs[2]);
    }

    bool read(adder_4& dyn) noexcept
    {
        return 8 == std::fscanf(file,
                                "%lf %lf %lf %lf %lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_values[2],
                                &dyn.default_values[3],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1],
                                &dyn.default_input_coeffs[2],
                                &dyn.default_input_coeffs[3]);
    }

    bool read(mult_2& dyn) noexcept
    {
        return 4 == std::fscanf(file,
                                "%lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1]);
    }

    bool read(mult_3& dyn) noexcept
    {
        return 6 == std::fscanf(file,
                                "%lf %lf %lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_values[2],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1],
                                &dyn.default_input_coeffs[2]);
    }

    bool read(mult_4& dyn) noexcept
    {
        return 8 == std::fscanf(file,
                                "%lf %lf %lf %lf %lf %lf %lf %lf",
                                &dyn.default_values[0],
                                &dyn.default_values[1],
                                &dyn.default_values[2],
                                &dyn.default_values[3],
                                &dyn.default_input_coeffs[0],
                                &dyn.default_input_coeffs[1],
                                &dyn.default_input_coeffs[2],
                                &dyn.default_input_coeffs[3]);
    }

    bool read(counter& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(generator& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(constant& dyn) noexcept
    {
        return 1 == std::fscanf(file, "%lf", &dyn.default_value);
    }

    bool read(cross& dyn) noexcept
    {
        return 1 == std::fscanf(file, "%lf", &dyn.default_threshold);
    }

    bool read(time_func& dyn) noexcept
    {
        char fn[10];

        if (1 != std::fscanf(file, "%9s", fn))
            return false;

        if (std::strcmp(fn, "square") == 0)
            dyn.default_f = nullptr;
        else if (std::strcmp(fn, "nullptr") == 0)
            dyn.default_f = nullptr;
        else
            return false;

        return true;
    }
};

struct writer
{
    std::FILE* file = stdout;

    array<model_id> map;

    writer(std::FILE* file_ = stdout) noexcept
      : file(file_)
    {}

    ~writer() noexcept
    {
        if (file && file != stderr && file != stdout)
            std::fclose(file);
    }

    status operator()(const simulation& sim) noexcept
    {
        std::fprintf(
          file, "%lu\n", static_cast<long unsigned>(sim.models.size()));

        irt_return_if_bad(map.init(sim.models.size()));

        std::fill_n(std::begin(map), std::size(map), static_cast<model_id>(0));

        model* mdl = nullptr;
        int id = 0;
        while (sim.models.next(mdl)) {
            const auto mdl_id = sim.models.get_id(mdl);
            const auto mdl_index = irt::get_index(mdl_id);

            std::fprintf(file, "%d %s ", id, mdl->name.c_str());
            map[id] = mdl_id;

            sim.dispatch(mdl->type, [this, mdl](auto& dyn_models) {
                write(dyn_models.get(mdl->id));
                return status::success;
            });

            ++id;
        }

        irt::output_port* out = nullptr;
        while (sim.output_ports.next(out)) {
            for (auto dst : out->connections) {
                if (auto* in = sim.input_ports.try_to_get(dst); in) {
                    auto* mdl_src = sim.models.try_to_get(out->model);
                    auto* mdl_dst = sim.models.try_to_get(in->model);

                    if (!(mdl_src && mdl_dst))
                        continue;

                    int src_index = -1;
                    int dst_index = -1;

                    irt_return_if_bad(
                      sim.get_input_port_index(*mdl_dst, dst, &dst_index));

                    irt_return_if_bad(sim.get_output_port_index(
                      *mdl_src, sim.output_ports.get_id(out), &src_index));

                    auto it_out = std::find(map.begin(), map.end(), out->model);
                    auto it_in = std::find(map.begin(), map.end(), in->model);

                    assert(it_out != map.end());
                    assert(it_in != map.end());

                    std::fprintf(file,
                                 "%u %d %u %d\n",
                                 std::distance(map.begin(), it_out),
                                 src_index,
                                 std::distance(map.begin(), it_in),
                                 dst_index);
                }
            }
        }

        return status::success;
    }

private:
    void write(const none& /*dyn*/) noexcept
    {
        std::fprintf(file, "none\n");
    }

    void write(const integrator& dyn) noexcept
    {
        std::fprintf(file,
                     "integrator %f %f\n",
                     dyn.default_current_value,
                     dyn.default_reset_value);
    }

    void write(const quantifier& dyn) noexcept
    {
        std::fprintf(
          file,
          "quantifier %f %d %s %s\n",
          dyn.default_step_size,
          dyn.default_past_length,
          dyn.default_adapt_state == quantifier::adapt_state::possible
            ? "possible"
            : dyn.default_adapt_state == quantifier::adapt_state::impossible
                ? "impossibe"
                : "done",
          dyn.default_zero_init_offset == true ? "true" : "false");
    }

    void write(const adder_2& dyn) noexcept
    {
        std::fprintf(file,
                     "adder_2 %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1]);
    }

    void write(const adder_3& dyn) noexcept
    {
        std::fprintf(file,
                     "adder_3 %f %f %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_values[2],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1],
                     dyn.default_input_coeffs[2]);
    }

    void write(const adder_4& dyn) noexcept
    {
        std::fprintf(file,
                     "adder_4 %f %f %f %f %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_values[2],
                     dyn.default_values[3],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1],
                     dyn.default_input_coeffs[2],
                     dyn.default_input_coeffs[3]);
    }

    void write(const mult_2& dyn) noexcept
    {
        std::fprintf(file,
                     "mult_2 %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1]);
    }

    void write(const mult_3& dyn) noexcept
    {
        std::fprintf(file,
                     "mult_3 %f %f %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_values[2],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1],
                     dyn.default_input_coeffs[2]);
    }

    void write(const mult_4& dyn) noexcept
    {
        std::fprintf(file,
                     "mult_4 %f %f %f %f %f %f %f %f\n",
                     dyn.default_values[0],
                     dyn.default_values[1],
                     dyn.default_values[2],
                     dyn.default_values[3],
                     dyn.default_input_coeffs[0],
                     dyn.default_input_coeffs[1],
                     dyn.default_input_coeffs[2],
                     dyn.default_input_coeffs[3]);
    }

    void write(const counter& /*dyn*/) noexcept
    {
        std::fprintf(file, "counter\n");
    }

    void write(const generator& /*dyn*/) noexcept
    {
        std::fprintf(file, "generator\n");
    }

    void write(const constant& dyn) noexcept
    {
        std::fprintf(file, "constant %f\n", dyn.default_value);
    }

    void write(const cross& dyn) noexcept
    {
        std::fprintf(file, "cross %f\n", dyn.default_threshold);
    }

    void write(const time_func& dyn) noexcept
    {
        std::fprintf(file,
                     "time_func %s\n",
                     dyn.default_f == nullptr ? "nullptr" : "square");
    }
};

class dot_writer
{
private:
    std::FILE* file = stdout;

public:
    dot_writer(std::FILE* file_ = stdout)
      : file(file_ == nullptr ? stdout : file_)
    {}

    ~dot_writer() noexcept
    {
        if (file && file != stdout)
            std::fclose(file);
    }

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

    void operator()(const simulation& sim) noexcept
    {
        std::fputs("digraph graphname {\n", file);

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
                        std::fprintf(
                          file, "%u -> ", irt::get_key(output_port->model));
                    else
                        std::fprintf(file, "%s -> ", mdl_src->name.c_str());

                    if (mdl_dst->name.empty())
                        std::fprintf(
                          file, "%u", irt::get_key(input_port->model));
                    else
                        std::fprintf(file, "%s", mdl_dst->name.c_str());

                    std::fputs(" [label=\"", file);

                    if (output_port->name.empty())
                        std::fprintf(
                          file,
                          "%u",
                          irt::get_key(sim.output_ports.get_id(*output_port)));
                    else
                        std::fprintf(file, "%s", output_port->name.c_str());

                    std::fputs("-", file);

                    if (input_port->name.empty())
                        std::fprintf(
                          file,
                          "%u",
                          irt::get_key(sim.input_ports.get_id(*input_port)));
                    else
                        std::fprintf(file, "%s", input_port->name.c_str());

                    std::fputs("\"];\n", file);
                }
            }
        }
    }
};

} // namespace irt

#endif
