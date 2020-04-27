// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>

#include <algorithm>
#include <istream>
#include <ostream>

namespace irt {

class reader
{
private:
    std::istream& is;

    array<model_id> map;
    int model_error = 0;
    int connection_error = 0;

    char temp_1[32];
    char temp_2[32];

public:
    reader(std::istream& is_) noexcept
      : is(is_)
    {}

    ~reader() noexcept = default;

    status operator()(simulation& sim) noexcept
    {
        int model_number = 0;

        irt_return_if_fail((is >> model_number), status::io_file_format_error);
        irt_return_if_fail(model_number > 0,
                           status::io_file_format_model_number_error);

        irt_return_if_bad(map.init(model_number));

        std::fill_n(std::begin(map), std::size(map), static_cast<model_id>(0));

        int id;

        for (int i = 0; i != model_number; ++i, ++model_error) {
            irt_return_if_fail((is >> id >> temp_1 >> temp_2),
                               status::io_file_format_model_error);

            irt_return_if_fail(0 <= id && id < model_number,
                               status::io_file_format_model_error);

            irt_return_if_bad(read(sim, id, temp_1, temp_2));
        }

        while (is) {
            int mdl_src_index, port_src_index, mdl_dst_index, port_dst_index;

            if (!(is >> mdl_src_index >> port_src_index >> mdl_dst_index >>
                  port_dst_index)) {
                if (is.eof())
                    break;

                irt_bad_return(status::io_file_format_error);
            }

            auto* mdl_src = sim.models.try_to_get(mdl_src_index);
            irt_return_if_fail(mdl_src, status::io_file_format_model_unknown);

            auto* mdl_dst = sim.models.try_to_get(mdl_dst_index);
            irt_return_if_fail(mdl_dst, status::io_file_format_model_unknown);

            output_port_id output_port;
            input_port_id input_port;

            irt_return_if_bad(
              sim.get_output_port_id(*mdl_src, port_src_index, &output_port));
            irt_return_if_bad(
              sim.get_input_port_id(*mdl_dst, port_dst_index, &input_port));

            irt_return_if_bad(sim.connect(output_port, input_port));
            ++connection_error;
        }

        return status::success;
    }

private:
    bool convert(const std::string_view dynamics_name,
                 dynamics_type* type) noexcept
    {
        if (dynamics_name == "none") {
            *type = dynamics_type::none;
            return true;
        }

        if (dynamics_name == "integrator") {
            *type = dynamics_type::integrator;
            return true;
        }

        if (dynamics_name == "quantifier") {
            *type = dynamics_type::quantifier;
            return true;
        }

        if (dynamics_name == "adder_2") {
            *type = dynamics_type::adder_2;
            return true;
        }

        if (dynamics_name == "adder_3") {
            *type = dynamics_type::adder_3;
            return true;
        }

        if (dynamics_name == "adder_4") {
            *type = dynamics_type::adder_4;
            return true;
        }

        if (dynamics_name == "mult_2") {
            *type = dynamics_type::mult_2;
            return true;
        }

        if (dynamics_name == "mult_3") {
            *type = dynamics_type::mult_3;
            return true;
        }

        if (dynamics_name == "mult_4") {
            *type = dynamics_type::mult_4;
            return true;
        }

        if (dynamics_name == "counter") {
            *type = dynamics_type::counter;
            return true;
        }

        if (dynamics_name == "generator") {
            *type = dynamics_type::generator;
            return true;
        }

        if (dynamics_name == "constant") {
            *type = dynamics_type::constant;
            return true;
        }

        if (dynamics_name == "cross") {
            *type = dynamics_type::cross;
            return true;
        }

        if (dynamics_name == "time_func") {
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
        return !!(is >> dyn.default_current_value >> dyn.default_reset_value);
    }

    bool read(quantifier& dyn) noexcept
    {
        if (!(is >> dyn.default_step_size >> dyn.default_past_length >>
              temp_1 >> temp_2))
            return false;

        if (std::strcmp(temp_1, "possible") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::possible;
        else if (std::strcmp(temp_1, "impossible") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::impossible;
        else if (std::strcmp(temp_1, "done") == 0)
            dyn.default_adapt_state = quantifier::adapt_state::done;
        else
            return false;

        if (std::strcmp(temp_2, "true") == 0)
            dyn.default_zero_init_offset = true;
        else if (std::strcmp(temp_2, "false") == 0)
            dyn.default_zero_init_offset = false;
        else
            return false;

        return true;
    }

    bool read(adder_2& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_input_coeffs[0] >> dyn.default_input_coeffs[1]);
    }

    bool read(adder_3& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_values[2] >> dyn.default_input_coeffs[0] >>
                  dyn.default_input_coeffs[1] >> dyn.default_input_coeffs[2]);
    }

    bool read(adder_4& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_values[2] >> dyn.default_values[3] >>
                  dyn.default_input_coeffs[0] >> dyn.default_input_coeffs[1] >>
                  dyn.default_input_coeffs[2] >> dyn.default_input_coeffs[3]);
    }

    bool read(mult_2& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_input_coeffs[0] >> dyn.default_input_coeffs[1]);
    }

    bool read(mult_3& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_values[2] >> dyn.default_input_coeffs[0] >>
                  dyn.default_input_coeffs[1] >> dyn.default_input_coeffs[2]);
    }

    bool read(mult_4& dyn) noexcept
    {
        return !!(is >> dyn.default_values[0] >> dyn.default_values[1] >>
                  dyn.default_values[2] >> dyn.default_values[3] >>
                  dyn.default_input_coeffs[0] >> dyn.default_input_coeffs[1] >>
                  dyn.default_input_coeffs[2] >> dyn.default_input_coeffs[3]);
    }

    bool read(counter& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(generator& dyn) noexcept
    {
        return !!(is >> dyn.default_value >> dyn.default_period >>
                  dyn.default_offset);;

    }

    bool read(constant& dyn) noexcept
    {
        return !!(is >> dyn.default_value);
    }

    bool read(cross& dyn) noexcept
    {
        return !!(is >> dyn.default_threshold);
    }

    bool read(accumulator_2& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(time_func& dyn) noexcept
    {
        if (!(is >> temp_1))
            return false;

        if (std::strcmp(temp_1, "square") == 0)
            dyn.default_f = &square_time_function;
        else
            dyn.default_f = &time_function;

        return true;
    }
};

struct writer
{
    std::ostream& os;

    array<model_id> map;

    writer(std::ostream& os_) noexcept
      : os(os_)
    {}

    status operator()(const simulation& sim) noexcept
    {
        os << sim.models.size() << '\n';

        irt_return_if_bad(map.init(sim.models.size()));

        std::fill_n(std::begin(map), std::size(map), static_cast<model_id>(0));

        model* mdl = nullptr;
        int id = 0;
        while (sim.models.next(mdl)) {
            const auto mdl_id = sim.models.get_id(mdl);

            os << id << ' ' << mdl->name.c_str() << ' ';
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

                    os << std::distance(map.begin(), it_out) << ' ' << src_index
                       << std::distance(map.begin(), it_in) << ' ' << dst_index
                       << '\n';
                }
            }
        }

        return status::success;
    }

private:
    void write(const none& /*dyn*/) noexcept
    {
        os << "none\n";
    }

    void write(const integrator& dyn) noexcept
    {
        os << "integrator " << dyn.default_current_value << ' '
           << dyn.default_reset_value << '\n';
    }

    void write(const quantifier& dyn) noexcept
    {
        os << "quantifier " << dyn.default_step_size << ' '
           << dyn.default_past_length << ' '
           << ((dyn.default_adapt_state == quantifier::adapt_state::possible)
                 ? "possible "
                 : dyn.default_adapt_state ==
                       quantifier::adapt_state::impossible
                     ? "impossibe "
                     : "done ")
           << (dyn.default_zero_init_offset == true ? "true\n" : "false\n");
    }

    void write(const adder_2& dyn) noexcept
    {
        os << "adder_2 " << dyn.default_values[0] << ' '
           << dyn.default_values[1] << ' ' << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << '\n';
    }

    void write(const adder_3& dyn) noexcept
    {
        os << "adder_3 " << dyn.default_values[0] << ' '
           << dyn.default_values[1] << ' ' << dyn.default_values[2] << ' '
           << dyn.default_input_coeffs[0] << ' ' << dyn.default_input_coeffs[1]
           << ' ' << dyn.default_input_coeffs[2] << '\n';
    }

    void write(const adder_4& dyn) noexcept
    {
        os << "adder_4 " << dyn.default_values[0] << ' '
           << dyn.default_values[1] << ' ' << dyn.default_values[2] << ' '
           << dyn.default_values[3] << ' ' << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << ' ' << dyn.default_input_coeffs[3] << '\n';
    }

    void write(const mult_2& dyn) noexcept
    {
        os << "mult_2 " << dyn.default_values[0] << ' ' << dyn.default_values[1]
           << ' ' << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << '\n';
    }

    void write(const mult_3& dyn) noexcept
    {
        os << "mult_3 " << dyn.default_values[0] << ' ' << dyn.default_values[1]
           << ' ' << dyn.default_values[2] << ' ' << dyn.default_input_coeffs[0]
           << ' ' << dyn.default_input_coeffs[1] << ' '
           << dyn.default_input_coeffs[2] << '\n';
    }

    void write(const mult_4& dyn) noexcept
    {
        os << "mult_4 " << dyn.default_values[0] << ' ' << dyn.default_values[1]
           << ' ' << dyn.default_values[2] << ' ' << dyn.default_values[3]
           << ' ' << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << ' ' << dyn.default_input_coeffs[3] << '\n';
    }

    void write(const counter& /*dyn*/) noexcept
    {
        os << "counter\n";
    }

    void write(const generator& dyn) noexcept
    {
        os << "generator"<< dyn.default_value << ' ' << dyn.default_period
           << ' ' << dyn.default_offset << '\n';;
    }

    void write(const constant& dyn) noexcept
    {
        os << "constant " << dyn.default_value << '\n';
    }

    void write(const cross& dyn) noexcept
    {
        os << "cross " << dyn.default_threshold << '\n';
    }

    void write(const accumulator_2& /*dyn*/) noexcept
    {
        os << "accumulator_2\n";
    }

    void write(const time_func& dyn) noexcept
    {
        os << "time_func "
           << (dyn.default_f == &time_function ? "time\n" : "square\n");
    }
};

class dot_writer
{
private:
    std::ostream& os;

public:
    dot_writer(std::ostream& os_)
      : os(os_)
    {}

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
        os << "digraph graphname {\n";

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
                        os << irt::get_key(output_port->model);
                    else
                        os << mdl_src->name.c_str();

                    os << " -> ";

                    if (mdl_dst->name.empty())
                        os << irt::get_key(input_port->model);
                    else
                        os << mdl_dst->name.c_str();

                    os << " [label=\"";

                    if (output_port->name.empty())
                        os << irt::get_key(
                          sim.output_ports.get_id(*output_port));
                    else
                        os << output_port->name.c_str();

                    os << " - ";

                    if (input_port->name.empty())
                        os << irt::get_key(sim.input_ports.get_id(*input_port));
                    else
                        os << input_port->name.c_str();

                    os << "\"];\n";
                }
            }
        }
    }
};

} // namespace irt

#endif
