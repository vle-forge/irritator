// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>

#include <algorithm>
#include <istream>
#include <ostream>
#include <streambuf>
#include <vector>

namespace irt {

static inline const char* dynamics_type_names[] = { "none",
                                                    "qss1_integrator",
                                                    "qss1_multiplier",
                                                    "qss1_cross",
                                                    "qss1_power",
                                                    "qss1_square",
                                                    "qss1_sum_2",
                                                    "qss1_sum_3",
                                                    "qss1_sum_4",
                                                    "qss1_wsum_2",
                                                    "qss1_wsum_3",
                                                    "qss1_wsum_4",
                                                    "qss2_integrator",
                                                    "qss2_multiplier",
                                                    "qss2_cross",
                                                    "qss2_power",
                                                    "qss2_square",
                                                    "qss2_sum_2",
                                                    "qss2_sum_3",
                                                    "qss2_sum_4",
                                                    "qss2_wsum_2",
                                                    "qss2_wsum_3",
                                                    "qss2_wsum_4",
                                                    "qss3_integrator",
                                                    "qss3_multiplier",
                                                    "qss3_cross",
                                                    "qss3_power",
                                                    "qss3_square",
                                                    "qss3_sum_2",
                                                    "qss3_sum_3",
                                                    "qss3_sum_4",
                                                    "qss3_wsum_2",
                                                    "qss3_wsum_3",
                                                    "qss3_wsum_4",
                                                    "integrator",
                                                    "quantifier",
                                                    "adder_2",
                                                    "adder_3",
                                                    "adder_4",
                                                    "mult_2",
                                                    "mult_3",
                                                    "mult_4",
                                                    "counter",
                                                    "generator",
                                                    "constant",
                                                    "cross",
                                                    "time_func",
                                                    "accumulator_2",
                                                    "flow" };

static_assert(std::size(dynamics_type_names) ==
              static_cast<sz>(dynamics_type_size()));

static inline const char* str_empty[] = { "" };
static inline const char* str_integrator[] = { "x-dot", "reset" };
static inline const char* str_adaptative_integrator[] = { "quanta",
                                                          "x-dot",
                                                          "reset" };
static inline const char* str_in_1[] = { "in" };
static inline const char* str_in_2[] = { "in-1", "in-2" };
static inline const char* str_in_3[] = { "in-1", "in-2", "in-3" };
static inline const char* str_in_4[] = { "in-1", "in-2", "in-3", "in-4" };
static inline const char* str_value_if_else[] = { "value",
                                                  "if",
                                                  "else",
                                                  "threshold" };
static inline const char* str_in_2_nb_2[] = { "in-1", "in-2", "nb-1", "nb-2" };

template<typename Dynamics>
static constexpr const char**
get_input_port_names() noexcept
{
    if constexpr (std::is_same_v<Dynamics, none>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss2_integrator> ||
                  std::is_same_v<Dynamics, qss3_integrator>)
        return str_integrator;

    if constexpr (std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, qss3_multiplier> ||
                  std::is_same_v<Dynamics, qss3_sum_2> ||
                  std::is_same_v<Dynamics, qss3_wsum_2> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, mult_2>)
        return str_in_2;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss3_sum_3> ||
                  std::is_same_v<Dynamics, qss3_wsum_3> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, mult_3>)
        return str_in_3;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, qss3_sum_4> ||
                  std::is_same_v<Dynamics, qss3_wsum_4> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_4>)
        return str_in_4;

    if constexpr (std::is_same_v<Dynamics, integrator>)
        return str_adaptative_integrator;

    if constexpr (std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, qss1_power> ||
                  std::is_same_v<Dynamics, qss2_power> ||
                  std::is_same_v<Dynamics, qss3_power> ||
                  std::is_same_v<Dynamics, qss1_square> ||
                  std::is_same_v<Dynamics, qss2_square> ||
                  std::is_same_v<Dynamics, qss3_square>)
        return str_in_1;

    if constexpr (std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, flow>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, qss3_cross> ||
                  std::is_same_v<Dynamics, cross>)
        return str_value_if_else;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_in_2_nb_2;

    irt_unreachable();
}

static constexpr const char**
get_input_port_names(const dynamics_type type) noexcept
{
    switch (type) {
    case dynamics_type::none:
        return str_empty;

    case dynamics_type::qss1_integrator:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss3_integrator:
        return str_integrator;

    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::adder_2:
    case dynamics_type::mult_2:
        return str_in_2;

    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::adder_3:
    case dynamics_type::mult_3:
        return str_in_3;

    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::adder_4:
    case dynamics_type::mult_4:
        return str_in_4;

    case dynamics_type::integrator:
        return str_adaptative_integrator;

    case dynamics_type::quantifier:
    case dynamics_type::counter:
    case dynamics_type::qss1_power:
    case dynamics_type::qss2_power:
    case dynamics_type::qss3_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss2_square:
    case dynamics_type::qss3_square:
        return str_in_1;

    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
    case dynamics_type::flow:
        return str_empty;

    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
    case dynamics_type::cross:
        return str_value_if_else;

    case dynamics_type::accumulator_2:
        return str_in_2_nb_2;
    }

    irt_unreachable();
}

static inline const char* str_out_1[] = { "out" };
static inline const char* str_out_cross[] = { "if-value",
                                              "else-value",
                                              "event" };

template<typename Dynamics>
static constexpr const char**
get_output_port_names() noexcept
{
    if constexpr (std::is_same_v<Dynamics, none>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_power> ||
                  std::is_same_v<Dynamics, qss1_square> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_integrator> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_power> ||
                  std::is_same_v<Dynamics, qss2_square> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, qss3_integrator> ||
                  std::is_same_v<Dynamics, qss3_multiplier> ||
                  std::is_same_v<Dynamics, qss3_power> ||
                  std::is_same_v<Dynamics, qss3_square> ||
                  std::is_same_v<Dynamics, qss3_sum_2> ||
                  std::is_same_v<Dynamics, qss3_sum_3> ||
                  std::is_same_v<Dynamics, qss3_sum_4> ||
                  std::is_same_v<Dynamics, qss3_wsum_2> ||
                  std::is_same_v<Dynamics, qss3_wsum_3> ||
                  std::is_same_v<Dynamics, qss3_wsum_4> ||
                  std::is_same_v<Dynamics, integrator> ||
                  std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_2> ||
                  std::is_same_v<Dynamics, mult_3> ||
                  std::is_same_v<Dynamics, mult_4> ||
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, flow>)
        return str_out_1;

    if constexpr (std::is_same_v<Dynamics, cross> ||
                  std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, qss3_cross>)
        return str_out_cross;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_empty;

    irt_unreachable();
}

static constexpr const char**
get_output_port_names(const dynamics_type type) noexcept
{
    switch (type) {
    case dynamics_type::none:
        return str_empty;

    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::integrator:
    case dynamics_type::quantifier:
    case dynamics_type::adder_2:
    case dynamics_type::adder_3:
    case dynamics_type::adder_4:
    case dynamics_type::mult_2:
    case dynamics_type::mult_3:
    case dynamics_type::mult_4:
    case dynamics_type::counter:
    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
    case dynamics_type::flow:
        return str_out_1;

    case dynamics_type::cross:
    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
        return str_out_cross;

    case dynamics_type::accumulator_2:
        return str_empty;
    }

    irt_unreachable();
}

class streambuf : public std::streambuf
{
public:
    std::streambuf* m_stream_buffer = { nullptr };
    std::streamsize m_file_position = { 0 };
    int m_line_number = { 1 };
    int m_last_line_number = { 1 };
    int m_column = { 0 };
    int m_prev_column = { -1 };

    streambuf(std::streambuf* sbuf)
      : m_stream_buffer(sbuf)
    {}

    streambuf(const streambuf&) = delete;
    streambuf& operator=(const streambuf&) = delete;

protected:
    std::streambuf::int_type underflow() override final
    {
        return m_stream_buffer->sgetc();
    }

    std::streambuf::int_type uflow() override final
    {
        int_type rc = m_stream_buffer->sbumpc();

        m_last_line_number = m_line_number;
        if (traits_type::eq_int_type(rc, traits_type::to_int_type('\n'))) {
            ++m_line_number;
            m_prev_column = m_column + 1;
            m_column = -1;
        }

        ++m_column;
        ++m_file_position;

        return rc;
    }

    std::streambuf::int_type pbackfail(std::streambuf::int_type c) override final
    {
        if (traits_type::eq_int_type(c, traits_type::to_int_type('\n'))) {
            --m_line_number;
            m_last_line_number = m_line_number;
            m_column = m_prev_column;
            m_prev_column = 0;
        }

        --m_column;
        --m_file_position;

        if (c != traits_type::eof())
            return m_stream_buffer->sputbackc(traits_type::to_char_type(c));

        return m_stream_buffer->sungetc();
    }

    std::ios::pos_type seekoff(std::ios::off_type pos,
                               std::ios_base::seekdir dir,
                               std::ios_base::openmode mode) override final
    {
        if (dir == std::ios_base::beg &&
            pos == static_cast<std::ios::off_type>(0)) {
            m_last_line_number = 1;
            m_line_number = 1;
            m_column = 0;
            m_prev_column = -1;
            m_file_position = 0;

            return m_stream_buffer->pubseekoff(pos, dir, mode);
        }

        return std::streambuf::seekoff(pos, dir, mode);
    }

    std::ios::pos_type seekpos(std::ios::pos_type pos,
                               std::ios_base::openmode mode) override final
    {
        if (pos == static_cast<std::ios::pos_type>(0)) {
            m_last_line_number = 1;
            m_line_number = 1;
            m_column = 0;
            m_prev_column = -1;
            m_file_position = 0;

            return m_stream_buffer->pubseekpos(pos, mode);
        }

        return std::streambuf::seekpos(pos, mode);
    }
};

class reader
{
private:
    streambuf buf;
    std::istream is;

    std::vector<model_id> map;
    int model_number = 0;

    char temp_1[32];
    char temp_2[32];

public:
    reader(std::istream& is_) noexcept
      : buf(is_.rdbuf())
      , is(&buf)
    {}

    ~reader() noexcept = default;

    int model_error = 0;
    int connection_error = 0;
    int line_error = 0;
    int column_error = 0;

    status operator()(simulation& sim) noexcept
    {
        irt_return_if_bad(do_read_model_number());

        for (int i = 0; i != model_number; ++i, ++model_error) {
            int id;
            irt_return_if_bad(do_read_model(sim, &id));
        }

        irt_return_if_bad(do_read_connections(sim));

        return status::success;
    }

    template<typename CallBackFunction>
    status operator()(simulation& sim, CallBackFunction f) noexcept
    {
        irt_return_if_bad(do_read_model_number());

        for (int i = 0; i != model_number; ++i, ++model_error) {
            int id;
            irt_return_if_bad(do_read_model(sim, &id));
            f(map[id]);
        }

        irt_return_if_bad(do_read_connections(sim));

        return status::success;
    }

private:
    void update_error_report() noexcept
    {
        line_error = buf.m_line_number;
        column_error = buf.m_column;
    }

    status do_read_model_number() noexcept
    {
        model_number = 0;

        update_error_report();
        irt_return_if_fail((is >> model_number), status::io_file_format_error);

        irt_return_if_fail(model_number > 0,
                           status::io_file_format_model_number_error);

        try {
            map.resize(model_number, model_id{ 0 });
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        return status::success;
    }

    status do_read_model(simulation& sim, int* id) noexcept
    {
        update_error_report();
        irt_return_if_fail((is >> *id >> temp_1),
                           status::io_file_format_model_error);

        irt_return_if_fail(0 <= *id && *id < model_number,
                           status::io_file_format_model_error);

        irt_return_if_bad(do_read_dynamics(sim, *id, temp_1));

        return status::success;
    }

    status do_read_connections(simulation& sim) noexcept
    {
        while (is) {
            int mdl_src_id, port_src_index, mdl_dst_id, port_dst_index;

            update_error_report();
            if (!(is >> mdl_src_id >> port_src_index >> mdl_dst_id >>
                  port_dst_index)) {
                if (is.eof())
                    break;

                irt_bad_return(status::io_file_format_error);
            }

            irt_return_if_fail(0 <= mdl_src_id && mdl_src_id < model_number,
                               status::io_file_format_model_error);
            irt_return_if_fail(0 <= mdl_dst_id && mdl_dst_id < model_number,
                               status::io_file_format_model_error);

            auto* mdl_src = sim.models.try_to_get(map[mdl_src_id]);
            irt_return_if_fail(mdl_src, status::io_file_format_model_unknown);

            auto* mdl_dst = sim.models.try_to_get(map[mdl_dst_id]);
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

    bool convert(const std::string_view dynamics_name,
                 dynamics_type* type) noexcept
    {
        struct string_to_type
        {
            constexpr string_to_type(const std::string_view n,
                                     const dynamics_type t)
              : name(n)
              , type(t)
            {}

            const std::string_view name;
            dynamics_type type;
        };

        static constexpr string_to_type table[] = {
            { "accumulator_2", dynamics_type::accumulator_2 },
            { "adder_2", dynamics_type::adder_2 },
            { "adder_3", dynamics_type::adder_3 },
            { "adder_4", dynamics_type::adder_4 },
            { "constant", dynamics_type::constant },
            { "counter", dynamics_type::counter },
            { "cross", dynamics_type::cross },
            { "flow", dynamics_type::flow },
            { "generator", dynamics_type::generator },
            { "integrator", dynamics_type::integrator },
            { "mult_2", dynamics_type::mult_2 },
            { "mult_3", dynamics_type::mult_3 },
            { "mult_4", dynamics_type::mult_4 },
            { "none", dynamics_type::none },
            { "qss1_cross", dynamics_type::qss1_cross },
            { "qss1_integrator", dynamics_type::qss1_integrator },
            { "qss1_multiplier", dynamics_type::qss1_multiplier },
            { "qss1_power", dynamics_type::qss1_power },
            { "qss1_square", dynamics_type::qss1_square },
            { "qss1_sum_2", dynamics_type::qss1_sum_2 },
            { "qss1_sum_3", dynamics_type::qss1_sum_3 },
            { "qss1_sum_4", dynamics_type::qss1_sum_4 },
            { "qss1_wsum_2", dynamics_type::qss1_wsum_2 },
            { "qss1_wsum_3", dynamics_type::qss1_wsum_3 },
            { "qss1_wsum_4", dynamics_type::qss1_wsum_4 },
            { "qss2_cross", dynamics_type::qss2_cross },
            { "qss2_integrator", dynamics_type::qss2_integrator },
            { "qss2_multiplier", dynamics_type::qss2_multiplier },
            { "qss2_power", dynamics_type::qss2_power },
            { "qss2_square", dynamics_type::qss2_square },
            { "qss2_sum_2", dynamics_type::qss2_sum_2 },
            { "qss2_sum_3", dynamics_type::qss2_sum_3 },
            { "qss2_sum_4", dynamics_type::qss2_sum_4 },
            { "qss2_wsum_2", dynamics_type::qss2_wsum_2 },
            { "qss2_wsum_3", dynamics_type::qss2_wsum_3 },
            { "qss2_wsum_4", dynamics_type::qss2_wsum_4 },
            { "qss3_cross", dynamics_type::qss3_cross },
            { "qss3_integrator", dynamics_type::qss3_integrator },
            { "qss3_multiplier", dynamics_type::qss3_multiplier },
            { "qss3_power", dynamics_type::qss3_power },
            { "qss3_square", dynamics_type::qss3_square },
            { "qss3_sum_2", dynamics_type::qss3_sum_2 },
            { "qss3_sum_3", dynamics_type::qss3_sum_3 },
            { "qss3_sum_4", dynamics_type::qss3_sum_4 },
            { "qss3_wsum_2", dynamics_type::qss3_wsum_2 },
            { "qss3_wsum_3", dynamics_type::qss3_wsum_3 },
            { "qss3_wsum_4", dynamics_type::qss3_wsum_4 },
            { "quantifier", dynamics_type::quantifier },
            { "time_func", dynamics_type::time_func }
        };

        static_assert(std::size(table) ==
                      static_cast<sz>(dynamics_type_size()));

        const auto it =
          std::lower_bound(std::begin(table),
                           std::end(table),
                           dynamics_name,
                           [](const string_to_type& l,
                              const std::string_view r) { return l.name < r; });

        if (it != std::end(table) && it->name == dynamics_name) {
            *type = it->type;
            return true;
        }

        return false;
    }

    status do_read_dynamics(simulation& sim,
                            int id,
                            const char* dynamics_name) noexcept
    {
        dynamics_type type;

        irt_return_if_fail(convert(dynamics_name, &type),
                           status::io_file_format_dynamics_unknown);

        model_id mdl_id = static_cast<model_id>(0);
        auto ret = sim.dispatch(type, [this, &sim, &mdl_id](auto& dyn_models) {
            irt_return_if_fail(dyn_models.can_alloc(1),
                               status::io_file_format_dynamics_limit_reach);
            auto& dyn = dyn_models.alloc();
            auto dyn_id = dyn_models.get_id(dyn);

            sim.alloc(dyn, dyn_id);
            mdl_id = dyn.id;

            update_error_report();
            irt_return_if_fail(read(dyn),
                               status::io_file_format_dynamics_init_error);

            return status::success;
        });

        irt_return_if_bad(ret);

        map[id] = mdl_id;

        return status::success;
    }

    bool read(none& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss1_integrator& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_X));
        double& x2 = *(const_cast<double*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss2_integrator& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_X));
        double& x2 = *(const_cast<double*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss3_integrator& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_X));
        double& x2 = *(const_cast<double*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss1_multiplier& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss1_sum_2& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss1_sum_3& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss1_sum_4& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss1_wsum_2& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss1_wsum_3& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss1_wsum_4& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));
        double& x4 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3 >> x4);
    }

    bool read(qss2_multiplier& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss2_sum_2& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss2_sum_3& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss2_sum_4& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss2_wsum_2& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss2_wsum_3& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss2_wsum_4& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));
        double& x4 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3 >> x4);
    }

    bool read(qss3_multiplier& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss3_sum_2& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss3_sum_3& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss3_sum_4& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss3_wsum_2& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss3_wsum_3& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss3_wsum_4& dyn) noexcept
    {
        double& x1 = *(const_cast<double*>(&dyn.default_input_coeffs[0]));
        double& x2 = *(const_cast<double*>(&dyn.default_input_coeffs[1]));
        double& x3 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));
        double& x4 = *(const_cast<double*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3 >> x4);
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
                  dyn.default_offset);
    }

    bool read(constant& dyn) noexcept
    {
        return !!(is >> dyn.default_value);
    }

    bool read(qss1_cross& dyn) noexcept
    {
        return !!(is >> dyn.default_threshold);
    }

    bool read(qss2_cross& dyn) noexcept
    {
        return !!(is >> dyn.default_threshold);
    }

    bool read(qss3_cross& dyn) noexcept
    {
        return !!(is >> dyn.default_threshold);
    }

    bool read(qss1_power& dyn) noexcept
    {
        return !!(is >> dyn.default_n);
    }

    bool read(qss2_power& dyn) noexcept
    {
        return !!(is >> dyn.default_n);
    }

    bool read(qss3_power& dyn) noexcept
    {
        return !!(is >> dyn.default_n);
    }

    bool read(qss1_square& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss2_square& /*dyn*/) noexcept
    {
        return true;
    }

    bool read(qss3_square& /*dyn*/) noexcept
    {
        return true;
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

    bool read(flow& dyn) noexcept
    {
        return !!(is >> dyn.default_samplerate);
    }
};

struct writer
{
    std::ostream& os;

    std::vector<model_id> map;

    writer(std::ostream& os_) noexcept
      : os(os_)
    {}

    status operator()(const simulation& sim) noexcept
    {
        os << sim.models.size() << '\n';

        try {
            map.resize(sim.models.size(), model_id{ 0 });
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        model* mdl = nullptr;
        int id = 0;
        while (sim.models.next(mdl)) {
            const auto mdl_id = sim.models.get_id(mdl);

            os << id << ' ';
            map[id] = mdl_id;

            sim.dispatch(mdl->type, [this, mdl](auto& dyn_models) {
                write(dyn_models.get(mdl->id));
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
                       << ' ' << std::distance(map.begin(), it_in) << ' '
                       << dst_index << '\n';
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

    void write(const qss1_integrator& dyn) noexcept
    {
        os << "qss1_integrator " << dyn.default_X << ' ' << dyn.default_dQ
           << '\n';
    }

    void write(const qss2_integrator& dyn) noexcept
    {
        os << "qss2_integrator " << dyn.default_X << ' ' << dyn.default_dQ
           << '\n';
    }

    void write(const qss3_integrator& dyn) noexcept
    {
        os << "qss3_integrator " << dyn.default_X << ' ' << dyn.default_dQ
           << '\n';
    }

    void write(const qss1_multiplier& /*dyn*/) noexcept
    {
        os << "qss1_multiplier\n";
    }

    void write(const qss1_sum_2& /*dyn*/) noexcept
    {
        os << "qss1_sum_2\n";
    }

    void write(const qss1_sum_3& /*dyn*/) noexcept
    {
        os << "qss1_sum_3\n";
    }

    void write(const qss1_sum_4& /*dyn*/) noexcept
    {
        os << "qss1_sum_4\n";
    }

    void write(const qss1_wsum_2& dyn) noexcept
    {
        os << "qss1_wsum_2 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << '\n';
    }

    void write(const qss1_wsum_3& dyn) noexcept
    {
        os << "qss1_wsum_3 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << '\n';
    }

    void write(const qss1_wsum_4& dyn) noexcept
    {
        os << "qss1_wsum_4 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << ' ' << dyn.default_input_coeffs[3] << '\n';
    }

    void write(const qss2_multiplier& /*dyn*/) noexcept
    {
        os << "qss2_multiplier\n";
    }

    void write(const qss2_sum_2& /*dyn*/) noexcept
    {
        os << "qss2_sum_2\n";
    }

    void write(const qss2_sum_3& /*dyn*/) noexcept
    {
        os << "qss2_sum_3\n";
    }

    void write(const qss2_sum_4& /*dyn*/) noexcept
    {
        os << "qss2_sum_4\n";
    }

    void write(const qss2_wsum_2& dyn) noexcept
    {
        os << "qss2_wsum_2 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << '\n';
    }

    void write(const qss2_wsum_3& dyn) noexcept
    {
        os << "qss2_wsum_3 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << '\n';
    }

    void write(const qss2_wsum_4& dyn) noexcept
    {
        os << "qss2_wsum_4 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << ' ' << dyn.default_input_coeffs[3] << '\n';
    }

    void write(const qss3_multiplier& /*dyn*/) noexcept
    {
        os << "qss3_multiplier\n";
    }

    void write(const qss3_sum_2& /*dyn*/) noexcept
    {
        os << "qss3_sum_2\n";
    }

    void write(const qss3_sum_3& /*dyn*/) noexcept
    {
        os << "qss3_sum_3\n";
    }

    void write(const qss3_sum_4& /*dyn*/) noexcept
    {
        os << "qss3_sum_4\n";
    }

    void write(const qss3_wsum_2& dyn) noexcept
    {
        os << "qss3_wsum_2 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << '\n';
    }

    void write(const qss3_wsum_3& dyn) noexcept
    {
        os << "qss3_wsum_3 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << '\n';
    }

    void write(const qss3_wsum_4& dyn) noexcept
    {
        os << "qss3_wsum_4 " << dyn.default_input_coeffs[0] << ' '
           << dyn.default_input_coeffs[1] << ' ' << dyn.default_input_coeffs[2]
           << ' ' << dyn.default_input_coeffs[3] << '\n';
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
        os << "generator " << dyn.default_value << ' ' << dyn.default_period
           << ' ' << dyn.default_offset << '\n';
        ;
    }

    void write(const constant& dyn) noexcept
    {
        os << "constant " << dyn.default_value << '\n';
    }

    void write(const qss1_cross& dyn) noexcept
    {
        os << "qss1_cross " << dyn.default_threshold << '\n';
    }

    void write(const qss2_cross& dyn) noexcept
    {
        os << "qss2_cross " << dyn.default_threshold << '\n';
    }

    void write(const qss3_cross& dyn) noexcept
    {
        os << "qss3_cross " << dyn.default_threshold << '\n';
    }

    void write(const qss1_power& dyn) noexcept
    {
        os << "qss1_power " << dyn.default_n << '\n';
    }

    void write(const qss2_power& dyn) noexcept
    {
        os << "qss2_power " << dyn.default_n << '\n';
    }

    void write(const qss3_power& dyn) noexcept
    {
        os << "qss3_power " << dyn.default_n << '\n';
    }

    void write(const qss1_square& /*dyn*/) noexcept
    {
        os << "qss1_square\n";
    }

    void write(const qss2_square& /*dyn*/) noexcept
    {
        os << "qss2_square\n";
    }

    void write(const qss3_square& /*dyn*/) noexcept
    {
        os << "qss3_square\n";
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

    void write(const flow& dyn) noexcept
    {
        os << "flow " << dyn.default_samplerate << '\n';
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

                    os << irt::get_key(output_port->model);
                    os << " -> ";
                    os << irt::get_key(input_port->model);

                    os << " [label=\"";
                    os << irt::get_key(sim.output_ports.get_id(*output_port));
                    os << " - ";
                    os << irt::get_key(sim.input_ports.get_id(*input_port));

                    os << "\"];\n";
                }
            }
        }
    }
};

} // namespace irt

#endif
