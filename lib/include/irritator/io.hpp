// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_IO_2020
#define ORG_VLEPROJECT_IRRITATOR_IO_2020

#include <irritator/core.hpp>
#include <irritator/external_source.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <istream>
#include <ostream>
#include <streambuf>
#include <vector>

namespace irt {

static inline const char* dynamics_type_names[] = {
    "qss1_integrator", "qss1_multiplier", "qss1_cross",      "qss1_power",
    "qss1_square",     "qss1_sum_2",      "qss1_sum_3",      "qss1_sum_4",
    "qss1_wsum_2",     "qss1_wsum_3",     "qss1_wsum_4",     "qss2_integrator",
    "qss2_multiplier", "qss2_cross",      "qss2_power",      "qss2_square",
    "qss2_sum_2",      "qss2_sum_3",      "qss2_sum_4",      "qss2_wsum_2",
    "qss2_wsum_3",     "qss2_wsum_4",     "qss3_integrator", "qss3_multiplier",
    "qss3_cross",      "qss3_power",      "qss3_square",     "qss3_sum_2",
    "qss3_sum_3",      "qss3_sum_4",      "qss3_wsum_2",     "qss3_wsum_3",
    "qss3_wsum_4",     "integrator",      "quantifier",      "adder_2",
    "adder_3",         "adder_4",         "mult_2",          "mult_3",
    "mult_4",          "counter",         "queue",           "dynamic_queue",
    "priority_queue",  "generator",       "constant",        "cross",
    "time_func",       "accumulator_2",   "filter",          "flow"
};

static inline const char* component_type_names[] = { "qss1_izhikevich",
                                                     "qss1_lif",
                                                     "qss1_lotka_volterra",
                                                     "qss1_negative_lif",
                                                     "qss1_seir_lineaire",
                                                     "qss1_seir_nonlineaire",
                                                     "qss1_van_der_pol",
                                                     "qss2_izhikevich",
                                                     "qss2_lif",
                                                     "qss2_lotka_volterra",
                                                     "qss2_negative_lif",
                                                     "qss2_seir_lineaire",
                                                     "qss2_seir_nonlineaire",
                                                     "qss2_van_der_pol",
                                                     "qss3_izhikevich",
                                                     "qss3_lif",
                                                     "qss3_lotka_volterra",
                                                     "qss3_negative_lif",
                                                     "qss3_seir_lineaire",
                                                     "qss3_seir_nonlineaire",
                                                     "qss3_van_der_pol",
                                                     "file",
                                                     "memory" };

static_assert(std::size(dynamics_type_names) ==
              static_cast<sz>(dynamics_type_size()));

static inline const char* str_empty[]                 = { "" };
static inline const char* str_integrator[]            = { "x-dot", "reset" };
static inline const char* str_adaptative_integrator[] = { "quanta",
                                                          "x-dot",
                                                          "reset" };
static inline const char* str_in_1[]                  = { "in" };
static inline const char* str_in_2[]                  = { "in-1", "in-2" };
static inline const char* str_in_3[] = { "in-1", "in-2", "in-3" };
static inline const char* str_in_4[] = { "in-1", "in-2", "in-3", "in-4" };
static inline const char* str_value_if_else[] = { "value",
                                                  "if",
                                                  "else",
                                                  "threshold" };
static inline const char* str_in_2_nb_2[] = { "in-1", "in-2", "nb-1", "nb-2" };

template<typename Dynamics>
static constexpr const char** get_input_port_names() noexcept
{
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
                  std::is_same_v<Dynamics, filter> ||
                  std::is_same_v<Dynamics, queue> ||
                  std::is_same_v<Dynamics, dynamic_queue> ||
                  std::is_same_v<Dynamics, priority_queue> ||
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

static constexpr const char** get_input_port_names(
  const dynamics_type type) noexcept
{
    switch (type) {
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
    case dynamics_type::filter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
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

static inline const char* str_out_1[]     = { "out" };
static inline const char* str_out_cross[] = { "if-value",
                                              "else-value",
                                              "event" };

template<typename Dynamics>
static constexpr const char** get_output_port_names() noexcept
{
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
                  std::is_same_v<Dynamics, queue> ||
                  std::is_same_v<Dynamics, dynamic_queue> ||
                  std::is_same_v<Dynamics, priority_queue> ||
                  std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, filter> ||
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

static constexpr const char** get_output_port_names(
  const dynamics_type type) noexcept
{
    switch (type) {
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
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::constant:
    case dynamics_type::time_func:
    case dynamics_type::filter:
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
    std::streambuf* m_stream_buffer    = { nullptr };
    std::streamsize m_file_position    = { 0 };
    int             m_line_number      = { 1 };
    int             m_last_line_number = { 1 };
    int             m_column           = { 0 };
    int             m_prev_column      = { -1 };

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
            m_column      = -1;
        }

        ++m_column;
        ++m_file_position;

        return rc;
    }

    std::streambuf::int_type pbackfail(
      std::streambuf::int_type c) override final
    {
        if (traits_type::eq_int_type(c, traits_type::to_int_type('\n'))) {
            --m_line_number;
            m_last_line_number = m_line_number;
            m_column           = m_prev_column;
            m_prev_column      = 0;
        }

        --m_column;
        --m_file_position;

        if (c != traits_type::eof())
            return m_stream_buffer->sputbackc(traits_type::to_char_type(c));

        return m_stream_buffer->sungetc();
    }

    std::ios::pos_type seekoff(std::ios::off_type      pos,
                               std::ios_base::seekdir  dir,
                               std::ios_base::openmode mode) override final
    {
        if (dir == std::ios_base::beg &&
            pos == static_cast<std::ios::off_type>(0)) {
            m_last_line_number = 1;
            m_line_number      = 1;
            m_column           = 0;
            m_prev_column      = -1;
            m_file_position    = 0;

            return m_stream_buffer->pubseekoff(pos, dir, mode);
        }

        return std::streambuf::seekoff(pos, dir, mode);
    }

    std::ios::pos_type seekpos(std::ios::pos_type      pos,
                               std::ios_base::openmode mode) override final
    {
        if (pos == static_cast<std::ios::pos_type>(0)) {
            m_last_line_number = 1;
            m_line_number      = 1;
            m_column           = 0;
            m_prev_column      = -1;
            m_file_position    = 0;

            return m_stream_buffer->pubseekpos(pos, mode);
        }

        return std::streambuf::seekpos(pos, mode);
    }
};

class reader
{
private:
    streambuf    buf;
    std::istream is;

    struct position
    {
        position() noexcept = default;

        position(const float x_, const float y_) noexcept
          : x(x_)
          , y(y_)
        {}

        float x, y;
    };

    table<int, model_id> map;           // used by simulation reader
    table<int, child_id> child_mapping; // used by component reader
    table<int, u64>      constant_mapping;
    table<int, u64>      binary_file_mapping;
    table<int, u64>      random_mapping;
    table<int, u64>      text_file_mapping;
    vector<position>     positions; // store position model in simulation reader

    int model_number = 0;

    char temp_1[32];
    char temp_2[32];

public:
    reader(std::istream& is_) noexcept
      : buf(is_.rdbuf())
      , is(&buf)
    {
        // map.data.reserve(64);
        child_mapping.data.reserve(64);
        // component_ref_map.data.reserve(64);
        constant_mapping.data.reserve(64);
        binary_file_mapping.data.reserve(64);
        random_mapping.data.reserve(64);
        text_file_mapping.data.reserve(64);
        positions.resize(64);
    }

    ~reader() noexcept = default;

    int model_error      = 0;
    int connection_error = 0;

    int line_error() const noexcept { return buf.m_line_number; }
    int column_error() const noexcept { return buf.m_column; }

    position get_position(const i32 index) const noexcept
    {
        return index < positions.ssize() ? positions[index]
                                         : position{ 0.f, 0.f };
    }

    status operator()(simulation& sim, external_source& srcs) noexcept
    {
        irt_return_if_bad(do_read_data_source(srcs));

        irt_return_if_bad(do_read_model_number());
        for (int i = 0; i != model_number; ++i, ++model_error) {
            int id;
            irt_return_if_bad(do_read_model(sim, &id));
        }

        irt_return_if_bad(do_read_connections(sim));

        return status::success;
    }

    status operator()(simulation&                        sim,
                      external_source&                   srcs,
                      function_ref<void(const model_id)> f) noexcept
    {
        irt_return_if_bad(do_read_data_source(srcs));

        irt_return_if_bad(do_read_model_number());
        for (int i = 0; i != model_number; ++i, ++model_error) {
            int id;
            irt_return_if_bad(do_read_model(sim, &id));
            if (auto* ptr = map.get(id); ptr)
                f(enum_cast<model_id>(*ptr));
        }

        irt_return_if_bad(do_read_connections(sim));

        return status::success;
    }

    status operator()(modeling&        mod,
                      component&       compo,
                      external_source& srcs) noexcept
    {
        std::string buffer;

        if (!(is >> std::quoted(buffer)))
            return status::io_file_format_error;
        compo.name = std::string_view(buffer);

        child_mapping.data.clear();

        irt_return_if_bad(do_read_data_source(srcs));

        irt_return_if_bad(do_read_model_number());
        for (int i = 0; i != model_number; ++i, ++model_error) {
            irt_return_if_bad(do_read_model(mod, compo));
        }

        child_mapping.sort();

        irt_return_if_bad(do_read_ports(compo, compo.y));
        irt_return_if_bad(do_read_ports(compo, compo.x));
        irt_return_if_bad(do_read_connections(mod, compo));

        return status::success;
    }

private:
    status do_read_binary_file_source(external_source& srcs) noexcept
    {
        u32 id;

        if (!(is >> id))
            return status::io_file_format_error;

        auto& elem = srcs.binary_file_sources.alloc();
        if (auto ret = elem.init(srcs.block_size, srcs.block_number);
            is_bad(ret)) {
            srcs.binary_file_sources.free(elem);
            return ret;
        }

        auto elem_id = srcs.binary_file_sources.get_id(elem);

        std::string file_path;
        if (!(is >> std::quoted(file_path)))
            return status::io_file_format_error;

        elem.file_path = file_path;
        binary_file_mapping.data.emplace_back(id, ordinal(elem_id));

        return status::success;
    }

    status do_read_text_file_source(external_source& srcs) noexcept
    {
        u32 id;

        if (!(is >> id))
            return status::io_file_format_error;

        auto& elem = srcs.text_file_sources.alloc();
        if (auto ret = elem.init(srcs.block_size, srcs.block_number);
            is_bad(ret)) {
            srcs.text_file_sources.free(elem);
            return ret;
        }

        auto elem_id = srcs.text_file_sources.get_id(elem);

        std::string file_path;
        if (!(is >> std::quoted(file_path)))
            return status::io_file_format_error;

        elem.file_path = file_path;
        text_file_mapping.data.emplace_back(id, ordinal(elem_id));

        return status::success;
    }

    status do_read_constant_source(external_source& srcs) noexcept
    {
        u32 id;
        sz  size;

        if (!(is >> id >> size))
            return status::io_file_format_error;

        auto& cst = srcs.constant_sources.alloc();
        if (auto ret = cst.init(srcs.block_size); is_bad(ret)) {
            srcs.constant_sources.free(cst);
            return ret;
        }

        auto cst_id = srcs.constant_sources.get_id(cst);

        try {
            constant_mapping.data.emplace_back(id, ordinal(cst_id));
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        for (size_t i = 0; i < size; ++i) {
            if (!(is >> cst.buffer[i]))
                return status::io_file_format_error;
        }

        return status::success;
    }

    status do_read_random_source(external_source& srcs) noexcept
    {
        u32 id;

        if (!(is >> id))
            return status::io_file_format_error;

        char type_str[30];
        if (!(is >> type_str))
            return status::io_file_format_error;

        auto it = binary_find(std::begin(distribution_type_string),
                              std::end(distribution_type_string),
                              type_str,
                              [](const char* left, const char* right) {
                                  return std::strcmp(left, right) == 0;
                              });

        if (it == std::end(distribution_type_string))
            return status::io_file_format_error;

        const auto dist_id =
          std::distance(std::begin(distribution_type_string), it);

        auto& elem = srcs.random_sources.alloc();
        if (auto ret = elem.init(srcs.block_size, srcs.block_number);
            is_bad(ret)) {
            srcs.random_sources.free(elem);
            return ret;
        }

        auto elem_id = srcs.random_sources.get_id(elem);

        try {
            random_mapping.data.emplace_back(id, ordinal(elem_id));
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        elem.distribution = enum_cast<distribution_type>(dist_id);

        switch (elem.distribution) {
        case distribution_type::uniform_int:
            if (!(is >> elem.a32 >> elem.b32))
                return status::io_file_format_error;
            break;

        case distribution_type::uniform_real:
            if (!(is >> elem.a >> elem.b))
                return status::io_file_format_error;
            break;

        case distribution_type::bernouilli:
            if (!(is >> elem.p))
                return status::io_file_format_error;
            break;

        case distribution_type::binomial:
            if (!(is >> elem.t32 >> elem.p))
                return status::io_file_format_error;
            break;

        case distribution_type::negative_binomial:
            if (!(is >> elem.t32 >> elem.p))
                return status::io_file_format_error;
            break;

        case distribution_type::geometric:
            if (!(is >> elem.p))
                return status::io_file_format_error;
            break;

        case distribution_type::poisson:
            if (!(is >> elem.mean))
                return status::io_file_format_error;
            break;

        case distribution_type::exponential:
            if (!(is >> elem.lambda))
                return status::io_file_format_error;
            break;

        case distribution_type::gamma:
            if (!(is >> elem.alpha >> elem.beta))
                return status::io_file_format_error;
            break;

        case distribution_type::weibull:
            if (!(is >> elem.a >> elem.b))
                return status::io_file_format_error;
            break;

        case distribution_type::exterme_value:
            if (!(is >> elem.a >> elem.b))
                return status::io_file_format_error;
            break;

        case distribution_type::normal:
            if (!(is >> elem.mean >> elem.stddev))
                return status::io_file_format_error;
            break;

        case distribution_type::lognormal:
            if (!(is >> elem.m >> elem.s))
                return status::io_file_format_error;
            break;

        case distribution_type::chi_squared:
            if (!(is >> elem.n))
                return status::io_file_format_error;
            break;

        case distribution_type::cauchy:
            if (!(is >> elem.a >> elem.b))
                return status::io_file_format_error;
            break;

        case distribution_type::fisher_f:
            if (!(is >> elem.m >> elem.n))
                return status::io_file_format_error;
            break;

        case distribution_type::student_t:
            if (!(is >> elem.n))
                return status::io_file_format_error;
            break;
        }

        return status::success;
    }

    status do_read_data_source(external_source& srcs) noexcept
    {
        size_t number;

        /* Read the constant sources */
        if (!(is >> number))
            return status::io_file_format_source_number_error;

        if (number > 0u) {
            if (!srcs.constant_sources.can_alloc(number))
                return status::io_file_source_full;

            for (sz i = 0; i < number; ++i)
                irt_return_if_bad(do_read_constant_source(srcs));
        }

        /* Read the binary sources */
        if (!(is >> number))
            return status::io_file_format_error;

        if (number > 0u) {
            if (!srcs.binary_file_sources.can_alloc(number))
                return status::io_file_source_full;

            for (sz i = 0; i < number; ++i)
                irt_return_if_bad(do_read_binary_file_source(srcs));
        }

        /* Read the text file sources */
        if (!(is >> number))
            return status::io_file_format_error;

        if (number > 0u) {
            if (!srcs.text_file_sources.can_alloc(number))
                return status::io_file_source_full;

            for (sz i = 0; i < number; ++i)
                irt_return_if_bad(do_read_text_file_source(srcs));
        }

        /* Read the random sources */
        if (!(is >> number))
            return status::io_file_format_error;

        if (number > 0u) {
            if (!srcs.random_sources.can_alloc(number))
                return status::io_file_source_full;

            for (sz i = 0; i < number; ++i) {
                irt_return_if_bad(do_read_random_source(srcs));
            }
        }

        constant_mapping.sort();
        binary_file_mapping.sort();
        text_file_mapping.sort();
        random_mapping.sort();

        return status::success;
    }

    status do_read_model_number() noexcept
    {
        model_number = 0;

        irt_return_if_fail((is >> model_number), status::io_file_format_error);

        irt_return_if_fail(model_number >= 0,
                           status::io_file_format_model_number_error);

        if (model_number > 0) {
            map.data.reserve(model_number);
            constant_mapping.data.reserve(model_number);
            binary_file_mapping.data.reserve(model_number);
            random_mapping.data.reserve(model_number);
            text_file_mapping.data.reserve(model_number);
            positions.resize(model_number);
        }

        return status::success;
    }

    status do_read_model_pre(int* id) noexcept
    {
        irt_return_if_fail((is >> *id), status::io_file_format_model_error);

        irt_return_if_fail(0 <= *id && *id < model_number,
                           status::io_file_format_model_error);

        irt_return_if_fail((is >> positions[*id].x >> positions[*id].y),
                           status::io_file_format_model_error);

        return status::success;
    }

    status do_read_model(simulation& sim, int* id) noexcept
    {
        irt_return_if_bad(do_read_model_pre(id));

        irt_return_if_fail((is >> temp_1), status::io_file_format_model_error);

        irt_return_if_bad(do_read_dynamics(sim, *id, temp_1));

        return status::success;
    }

    status do_read_model(modeling& mod, component& compo) noexcept
    {
        int id = 0;
        irt_return_if_bad(do_read_model_pre(&id));

        std::string name;
        if (!(is >> std::quoted(name)))
            return status::io_file_format_error;

        irt_return_if_fail((is >> temp_1), status::io_file_format_model_error);

        irt_return_if_bad(do_read_dynamics(mod, compo, id, name, temp_1));

        return status::success;
    }

    status do_read_ports(component&             compo,
                         small_vector<port, 8>& ports) noexcept
    {
        int nb = 0;

        irt_return_if_fail((is >> nb), status::io_file_format_model_error);
        irt_return_if_fail(nb >= 0, status::io_file_format_model_error);

        for (int i = 0; i < nb; ++i) {
            int child_index, port_index;

            irt_return_if_fail((is >> child_index >> port_index),
                               status::io_file_format_model_error);

            auto* c = child_mapping.get(child_index);
            irt_return_if_fail(c, status::io_file_format_model_error);
            irt_return_if_fail(0 <= port_index && port_index < INT8_MAX,
                               status::io_file_format_model_error);

            auto* child = compo.children.try_to_get(*c);
            irt_return_if_fail(child, status::io_file_format_model_error);
            irt_return_if_fail(child->type == child_type::model,
                               status::io_file_format_model_error);

            ports.emplace_back(enum_cast<model_id>(child->id), static_cast<i8>(port_index));
        }

        return status::success;
    }

    status do_read_connections(modeling& mod, component& compo) noexcept
    {
        while (is) {
            int mdl_src_id, port_src_index, mdl_dst_id, port_dst_index;

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

            auto* m_src_id = child_mapping.get(mdl_src_id);
            irt_return_if_fail(m_src_id, status::io_file_format_model_unknown);

            auto* m_dst_id = child_mapping.get(mdl_dst_id);
            irt_return_if_fail(m_dst_id, status::io_file_format_model_unknown);

            irt_return_if_fail(0 <= port_src_index && port_src_index < INT8_MAX,
                               status::io_file_format_model_unknown);
            irt_return_if_fail(0 <= port_dst_index && port_dst_index < INT8_MAX,
                               status::io_file_format_model_unknown);

            irt_return_if_bad(mod.connect(compo,
                                          *m_src_id,
                                          static_cast<i8>(port_src_index),
                                          *m_dst_id,
                                          static_cast<i8>(port_dst_index)));

            ++connection_error;
        }

        return status::success;
    }

    status do_read_connections(simulation& sim) noexcept
    {
        while (is) {
            int mdl_src_id, port_src_index, mdl_dst_id, port_dst_index;

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

            auto* mapped_mdl_src_id = map.get(mdl_src_id);
            irt_return_if_fail(mapped_mdl_src_id,
                               status::io_file_format_model_unknown);

            auto* mdl_src =
              sim.models.try_to_get(enum_cast<model_id>(*mapped_mdl_src_id));
            irt_return_if_fail(mdl_src, status::io_file_format_model_unknown);

            auto* mapped_mdl_dst_id = map.get(mdl_dst_id);
            auto* mdl_dst =
              sim.models.try_to_get(enum_cast<model_id>(*mapped_mdl_dst_id));
            irt_return_if_fail(mdl_dst, status::io_file_format_model_unknown);

            output_port* out = nullptr;
            input_port*  in  = nullptr;

            irt_return_if_bad(get_output_port(*mdl_src, port_src_index, out));
            irt_return_if_bad(get_input_port(*mdl_dst, port_dst_index, in));

            irt_return_if_bad(
              sim.connect(*mdl_src, port_src_index, *mdl_dst, port_dst_index));

            ++connection_error;
        }

        return status::success;
    }

    bool convert(const std::string_view dynamics_name,
                 dynamics_type*         type) noexcept
    {
        struct string_to_type
        {
            constexpr string_to_type(const std::string_view n,
                                     const dynamics_type    t)
              : name(n)
              , type(t)
            {}

            const std::string_view name;
            dynamics_type          type;
        };

        static constexpr string_to_type table[] = {
            { "accumulator_2", dynamics_type::accumulator_2 },
            { "adder_2", dynamics_type::adder_2 },
            { "adder_3", dynamics_type::adder_3 },
            { "adder_4", dynamics_type::adder_4 },
            { "constant", dynamics_type::constant },
            { "counter", dynamics_type::counter },
            { "cross", dynamics_type::cross },
            { "dynamic_queue", dynamics_type::dynamic_queue },
            { "filter", dynamics_type::filter },
            { "flow", dynamics_type::flow },
            { "generator", dynamics_type::generator },
            { "integrator", dynamics_type::integrator },
            { "mult_2", dynamics_type::mult_2 },
            { "mult_3", dynamics_type::mult_3 },
            { "mult_4", dynamics_type::mult_4 },
            { "priority_queue", dynamics_type::priority_queue },
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
            { "queue", dynamics_type::queue },
            { "time_func", dynamics_type::time_func }
        };

        static_assert(std::size(table) ==
                      static_cast<sz>(dynamics_type_size()));

        const auto it =
          std::lower_bound(std::begin(table),
                           std::end(table),
                           dynamics_name,
                           [](const string_to_type&  l,
                              const std::string_view r) { return l.name < r; });

        if (it != std::end(table) && it->name == dynamics_name) {
            *type = it->type;
            return true;
        }

        return false;
    }

    status do_read_dynamics(simulation& sim,
                            int         id,
                            const char* dynamics_name) noexcept
    {
        dynamics_type type;

        irt_return_if_fail(convert(dynamics_name, &type),
                           status::io_file_format_dynamics_unknown);

        auto& mdl = sim.alloc(type);

        auto ret =
          dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> status {
              irt_return_if_fail(this->read(dyn),
                                 status::io_file_format_dynamics_init_error);

              return status::success;
          });

        irt_return_if_bad(ret);
        map.set(id, sim.models.get_id(mdl));

        return status::success;
    }

    status do_read_dynamics(modeling&        mod,
                            component&       compo,
                            int              id,
                            std::string_view name,
                            const char*      dynamics_name) noexcept
    {
        dynamics_type type;
        child_id      last = undefined<child_id>();

        if (std::strcmp(dynamics_name, "component") == 0) {
            char        arg_1[64];
            std::string arg_2;

            if (!(is >> arg_1))
                return status::io_file_format_error;

            if (!(is >> std::quoted(arg_2)))
                return status::io_file_format_error;

            auto  compo_id  = mod.search_component(arg_2.c_str(), arg_1);
            auto* sub_compo = mod.components.try_to_get(compo_id);
            irt_return_if_fail(sub_compo, status::io_file_format_error);

            irt_return_if_fail(compo.children.can_alloc(),
                               status::io_not_enough_memory);

            auto& child = compo.children.alloc(compo_id);
            last        = compo.children.get_id(child);
        } else {
            irt_return_if_fail(convert(dynamics_name, &type),
                               status::io_file_format_dynamics_unknown);

            auto& child = mod.alloc(compo, type);
            last        = compo.children.get_id(child);

            irt_assert(compo.models.try_to_get(enum_cast<model_id>(child.id)) !=
                       nullptr);

            auto& mdl = compo.models.get(enum_cast<model_id>(child.id));

            auto ret =
              dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> status {
                  irt_return_if_fail(
                    this->read(dyn),
                    status::io_file_format_dynamics_init_error);

                  return status::success;
              });

            irt_return_if_bad(ret);
        }

        child_mapping.set(id, last);
        auto& child = compo.children.get(last);
        child.name  = name;
        child.x     = positions[id].x;
        child.y     = positions[id].y;

        return status::success;
    }

    bool read(qss1_integrator& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_X));
        real& x2 = *(const_cast<real*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss2_integrator& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_X));
        real& x2 = *(const_cast<real*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss3_integrator& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_X));
        real& x2 = *(const_cast<real*>(&dyn.default_dQ));

        return !!(is >> x1 >> x2);
    }

    bool read(qss1_multiplier& /*dyn*/) noexcept { return true; }

    bool read(qss1_sum_2& /*dyn*/) noexcept { return true; }

    bool read(qss1_sum_3& /*dyn*/) noexcept { return true; }

    bool read(qss1_sum_4& /*dyn*/) noexcept { return true; }

    bool read(qss1_wsum_2& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss1_wsum_3& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss1_wsum_4& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));
        real& x4 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3 >> x4);
    }

    bool read(qss2_multiplier& /*dyn*/) noexcept { return true; }

    bool read(qss2_sum_2& /*dyn*/) noexcept { return true; }

    bool read(qss2_sum_3& /*dyn*/) noexcept { return true; }

    bool read(qss2_sum_4& /*dyn*/) noexcept { return true; }

    bool read(qss2_wsum_2& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss2_wsum_3& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss2_wsum_4& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));
        real& x4 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3 >> x4);
    }

    bool read(qss3_multiplier& /*dyn*/) noexcept { return true; }

    bool read(qss3_sum_2& /*dyn*/) noexcept { return true; }

    bool read(qss3_sum_3& /*dyn*/) noexcept { return true; }

    bool read(qss3_sum_4& /*dyn*/) noexcept { return true; }

    bool read(qss3_wsum_2& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));

        return !!(is >> x1 >> x2);
    }

    bool read(qss3_wsum_3& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

        return !!(is >> x1 >> x2 >> x3);
    }

    bool read(qss3_wsum_4& dyn) noexcept
    {
        real& x1 = *(const_cast<real*>(&dyn.default_input_coeffs[0]));
        real& x2 = *(const_cast<real*>(&dyn.default_input_coeffs[1]));
        real& x3 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));
        real& x4 = *(const_cast<real*>(&dyn.default_input_coeffs[2]));

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

    bool read(counter& /*dyn*/) noexcept { return true; }

    bool read(queue& dyn) noexcept { return !!(is >> dyn.default_ta); }

    bool read_source(source& src, int index, external_source_type type)
    {
        switch (type) {
        case external_source_type::binary_file: {
            if (auto* ptr = binary_file_mapping.get(index); ptr) {
                src.type = ordinal(type);
                src.id   = *ptr;
                return true;
            }

            return false;
        }

        case external_source_type::constant: {
            if (auto* ptr = constant_mapping.get(index); ptr) {
                src.type = ordinal(type);
                src.id   = *ptr;
                return true;
            }

            return false;
        };

        case external_source_type::text_file: {
            if (auto* ptr = text_file_mapping.get(index); ptr) {
                src.type = ordinal(type);
                src.id   = *ptr;
                return true;
            }

            return false;
        };

        case external_source_type::random: {
            if (auto* ptr = random_mapping.get(index); ptr) {
                src.type = ordinal(type);
                src.id   = *ptr;
                return true;
            }
            return false;
        };
        }

        irt_unreachable();
    }

    bool read(dynamic_queue& dyn) noexcept
    {
        int index;
        int type;

        if (!(is >> index >> type))
            return false;

        external_source_type source_type;
        if (external_source_type_cast(type, &source_type)) {
            if (!read_source(dyn.default_source_ta, index, source_type))
                return false;
        }

        return true;
    }

    bool read(priority_queue& dyn) noexcept
    {
        int index;
        int type;

        if (!(is >> index >> type))
            return false;

        external_source_type source_type;
        if (external_source_type_cast(type, &source_type)) {
            if (!read_source(dyn.default_source_ta, index, source_type))
                return false;
        }

        return true;
    }

    bool read(generator& dyn) noexcept
    {
        int index[2];
        int type[2];

        if (!(is >> dyn.default_offset >> index[0] >> type[0] >> index[1] >>
              type[1]))
            return false;

        external_source_type source_type;
        if (external_source_type_cast(type[0], &source_type)) {
            if (!read_source(dyn.default_source_ta, index[0], source_type))
                return false;
        }

        if (external_source_type_cast(type[1], &source_type)) {
            if (!read_source(dyn.default_source_value, index[1], source_type))
                return false;
        }

        return true;
    }

    bool read(constant& dyn) noexcept { return !!(is >> dyn.default_value); }

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

    bool read(qss1_power& dyn) noexcept { return !!(is >> dyn.default_n); }

    bool read(qss2_power& dyn) noexcept { return !!(is >> dyn.default_n); }

    bool read(qss3_power& dyn) noexcept { return !!(is >> dyn.default_n); }

    bool read(qss1_square& /*dyn*/) noexcept { return true; }

    bool read(qss2_square& /*dyn*/) noexcept { return true; }

    bool read(qss3_square& /*dyn*/) noexcept { return true; }

    bool read(cross& dyn) noexcept { return !!(is >> dyn.default_threshold); }

    bool read(accumulator_2& /*dyn*/) noexcept { return true; }

    bool read(time_func& dyn) noexcept
    {
        if (!(is >> temp_1))
            return false;

        if (std::strcmp(temp_1, "sin") == 0)
            dyn.default_f = &sin_time_function;
        else if (std::strcmp(temp_1, "square") == 0)
            dyn.default_f = &square_time_function;
        else
            dyn.default_f = &time_function;

        return true;
    }

    bool read(filter& dyn) noexcept
    {
        return !!(is >> dyn.default_lower_threshold >>
                  dyn.default_upper_threshold);
    }

    bool read(flow& dyn) noexcept { return !!(is >> dyn.default_samplerate); }
};

struct writer
{
    std::ostream&         os;
    std::vector<model_id> map;

    table<child_id, i32> child_mapping;

    table<model_id, int>     modeling_model_map;
    table<component_id, int> modeling_component_ref_map;

    writer(std::ostream& os_) noexcept
      : os(os_)
    {}

    void write_constant_sources(
      const data_array<constant_source, constant_source_id>& srcs)
    {
        os << srcs.size() << '\n';
        if (srcs.size() > 0u) {
            constant_source* src = nullptr;
            while (srcs.next(src)) {
                const auto id    = srcs.get_id(src);
                const auto index = get_index(id);
                os << index << ' ' << src->buffer.size();

                for (const auto elem : src->buffer)
                    os << ' ' << elem;

                os << '\n';
            }
        }
    }

    void write_binary_file_sources(
      const data_array<binary_file_source, binary_file_source_id>& srcs)
    {
        os << srcs.size() << '\n';
        if (srcs.size() > 0u) {
            binary_file_source* src = nullptr;
            while (srcs.next(src)) {
                const auto id    = srcs.get_id(src);
                const auto index = get_index(id);

                os << index << ' ' << std::quoted(src->file_path.string())
                   << '\n';
            }
        }
    }

    void write_text_file_sources(
      const data_array<text_file_source, text_file_source_id>& srcs)
    {
        os << srcs.size() << '\n';
        if (srcs.size() > 0u) {
            text_file_source* src = nullptr;
            while (srcs.next(src)) {
                const auto id    = srcs.get_id(src);
                const auto index = get_index(id);

                os << index << ' ' << std::quoted(src->file_path.string())
                   << '\n';
            }
        }
    }

    void write_random_sources(
      const data_array<random_source, random_source_id>& srcs)
    {
        os << srcs.size() << '\n';
        if (srcs.size() > 0u) {
            random_source* src = nullptr;
            while (srcs.next(src)) {
                const auto id    = srcs.get_id(src);
                const auto index = get_index(id);

                os << index << ' ' << distribution_str(src->distribution);

                write(*src);

                os << '\n';
            }
        }
    }

    void write_connections(const simulation& sim)
    {
        model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            dispatch(
              *mdl, [this, &sim, &mdl]<typename Dynamics>(Dynamics& dyn) {
                  if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                      int i = 0;
                      for (const auto elem : dyn.y) {
                          auto list = get_node(sim, elem);
                          for (const auto& cnt : list) {
                              auto* dst = sim.models.try_to_get(cnt.model);
                              if (dst) {
                                  auto it_out =
                                    std::find(map.begin(),
                                              map.end(),
                                              sim.models.get_id(*mdl));
                                  auto it_in = std::find(
                                    map.begin(), map.end(), cnt.model);

                                  irt_assert(it_out != map.end());
                                  irt_assert(it_in != map.end());

                                  os << std::distance(map.begin(), it_out)
                                     << ' ' << i << ' '
                                     << std::distance(map.begin(), it_in) << ' '
                                     << static_cast<int>(cnt.port_index)
                                     << '\n';
                              }
                          }

                          ++i;
                      }
                  }
              });
        }
    }

    void write_model(const simulation& sim)
    {
        model* mdl = nullptr;
        int    id  = 0;
        while (sim.models.next(mdl)) {
            const auto mdl_id = sim.models.get_id(mdl);
            map[id]           = mdl_id;

            os << id << " 0.0 0.0 ";

            dispatch(*mdl, [this](auto& dyn) -> void { this->write(dyn); });

            ++id;
        }
    }

    void do_write_model_dynamics(const model& mdl) noexcept
    {
        dispatch(mdl, [this](auto& dyn) -> void { this->write(dyn); });
    }

    void do_write_component_dynamics(const modeling&  mod,
                                     const component& compo) noexcept
    {
        os << "component ";

        if (compo.type == component_type::file ||
            compo.type == component_type::memory) {
            os << "file "
               << std::quoted(mod.file_paths.get(compo.file).path.c_str())
               << '\n';
        } else {
            os << "cpp " << component_type_names[ordinal(compo.type)] << '\n';
        }
    }

    void write_model(const simulation&                                sim,
                     function_ref<void(model_id, float& x, float& y)> get_pos)
    {
        model* mdl = nullptr;
        int    id  = 0;

        while (sim.models.next(mdl)) {
            const auto mdl_id = sim.models.get_id(mdl);
            map[id]           = mdl_id;

            float x = 0.f, y = 0.f;
            if (!get_pos.empty())
                get_pos(mdl_id, x, y);

            os << id << ' ' << x << ' ' << y << ' ';
            do_write_model_dynamics(*mdl);
            ++id;
        }
    }

    status operator()(const simulation&      sim,
                      const external_source& srcs) noexcept
    {
        write_constant_sources(srcs.constant_sources);
        write_binary_file_sources(srcs.binary_file_sources);
        write_text_file_sources(srcs.text_file_sources);
        write_random_sources(srcs.random_sources);

        child_mapping.data.reserve(64);

        try {
            map.resize(sim.models.size(), undefined<model_id>());
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        os << sim.models.size() << '\n';
        write_model(sim);
        write_connections(sim);

        return status::success;
    }

    status operator()(
      const simulation&                            sim,
      const external_source&                       srcs,
      function_ref<void(model_id, float&, float&)> get_pos) noexcept
    {
        write_constant_sources(srcs.constant_sources);
        write_binary_file_sources(srcs.binary_file_sources);
        write_text_file_sources(srcs.text_file_sources);
        write_random_sources(srcs.random_sources);

        child_mapping.data.reserve(64);

        try {
            map.resize(sim.models.size(), undefined<model_id>());
        } catch (const std::bad_alloc& /*e*/) {
            return status::io_not_enough_memory;
        }

        os << sim.models.size() << '\n';
        write_model(sim, get_pos);
        write_connections(sim);

        return status::success;
    }

    status operator()(const modeling&        mod,
                      const component&       compo,
                      const external_source& srcs) noexcept
    {
        os << std::quoted(compo.name.sv()) << '\n';

        write_constant_sources(srcs.constant_sources);
        write_binary_file_sources(srcs.binary_file_sources);
        write_text_file_sources(srcs.text_file_sources);
        write_random_sources(srcs.random_sources);

        do_write_children(mod, compo);
        do_write_ports(compo.x);
        do_write_ports(compo.y);
        do_write_connection(compo);

        return status::success;
    }

private:
    void do_write_connection(const component& compo) noexcept
    {
        connection* c = nullptr;
        while (compo.connections.next(c)) {
            os << get_index(c->src) << ' ' << static_cast<int>(c->index_src)
               << ' ' << get_index(c->dst) << ' '
               << static_cast<int>(c->index_dst) << '\n';
        }
    }

    void do_write_ports(const small_vector<port, 8>& ports) noexcept
    {
        os << ports.ssize() << '\n';

        for (int i = 0; i != ports.ssize(); ++i)
            os << get_index(ports[i].id) << ' ' << ports[i].index << '\n';
    }

    void do_write_children(const modeling& mod, const component& compo) noexcept
    {
        os << compo.children.size() << '\n';

        child* c = nullptr;
        while (compo.children.next(c)) {
            const auto id = get_index(compo.children.get_id(*c));
            os << id << ' ' << c->x << ' ' << c->y << ' ';

            if (c->name.empty()) {
                os << " \"\" ";
            } else {
                os << std::quoted(c->name.sv()) << ' ';
            }

            if (c->type == child_type::component) {
                auto  cpn_id = enum_cast<component_id>(c->id);
                auto* cpn    = mod.components.try_to_get(cpn_id);
                irt_assert(cpn && "cleanup all component vectors before save");

                do_write_component_dynamics(mod, *cpn);
            } else {
                auto  mdl_id = enum_cast<model_id>(c->id);
                auto* mdl    = compo.models.try_to_get(mdl_id);
                irt_assert(mdl && "cleanup all component vectors before save");

                do_write_model_dynamics(*mdl);
            }
        }
    }

    void write(const random_source& src) noexcept
    {
        switch (src.distribution) {
        case distribution_type::uniform_int:
            os << src.a32 << ' ' << src.b32;
            break;

        case distribution_type::uniform_real:
            os << src.a << ' ' << src.b;
            break;

        case distribution_type::bernouilli:
            os << src.p;
            break;

        case distribution_type::binomial:
            os << src.t32 << ' ' << src.p;
            break;

        case distribution_type::negative_binomial:
            os << src.t32 << ' ' << src.p;
            break;

        case distribution_type::geometric:
            os << src.p;
            break;

        case distribution_type::poisson:
            os << src.mean;
            break;

        case distribution_type::exponential:
            os << src.lambda;
            break;

        case distribution_type::gamma:
            os << src.alpha << ' ' << src.beta;
            break;

        case distribution_type::weibull:
            os << src.a << ' ' << src.b;
            break;

        case distribution_type::exterme_value:
            os << src.a << ' ' << src.b;
            break;

        case distribution_type::normal:
            os << src.mean << ' ' << src.stddev;
            break;

        case distribution_type::lognormal:
            os << src.m << ' ' << src.s;
            break;

        case distribution_type::chi_squared:
            os << src.n;
            break;

        case distribution_type::cauchy:
            os << src.a << ' ' << src.b;
            break;

        case distribution_type::fisher_f:
            os << src.m << ' ' << src.n;
            break;

        case distribution_type::student_t:
            os << src.n;
            break;
        }
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

    void write(const qss1_sum_2& /*dyn*/) noexcept { os << "qss1_sum_2\n"; }

    void write(const qss1_sum_3& /*dyn*/) noexcept { os << "qss1_sum_3\n"; }

    void write(const qss1_sum_4& /*dyn*/) noexcept { os << "qss1_sum_4\n"; }

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

    void write(const qss2_sum_2& /*dyn*/) noexcept { os << "qss2_sum_2\n"; }

    void write(const qss2_sum_3& /*dyn*/) noexcept { os << "qss2_sum_3\n"; }

    void write(const qss2_sum_4& /*dyn*/) noexcept { os << "qss2_sum_4\n"; }

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

    void write(const qss3_sum_2& /*dyn*/) noexcept { os << "qss3_sum_2\n"; }

    void write(const qss3_sum_3& /*dyn*/) noexcept { os << "qss3_sum_3\n"; }

    void write(const qss3_sum_4& /*dyn*/) noexcept { os << "qss3_sum_4\n"; }

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
               : dyn.default_adapt_state == quantifier::adapt_state::impossible
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

    void write(const counter& /*dyn*/) noexcept { os << "counter\n"; }

    void write(const source& src) noexcept
    {
        u32 a, b;
        unpack_doubleword(src.id, &a, &b);
        os << b << ' ' << src.type;
    }

    void write(const queue& dyn) noexcept
    {
        os << "queue " << dyn.default_ta << '\n';
    }

    void write(const dynamic_queue& dyn) noexcept
    {
        os << "dynamic_queue ";
        write(dyn.default_source_ta);
        os << '\n';
    }

    void write(const priority_queue& dyn) noexcept
    {
        os << "priority_queue ";
        write(dyn.default_source_ta);
        os << '\n';
    }

    void write(const generator& dyn) noexcept
    {
        os << "generator " << dyn.default_offset << ' ';

        write(dyn.default_source_ta);
        os << ' ';
        write(dyn.default_source_value);
        os << '\n';
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

    void write(const qss1_square& /*dyn*/) noexcept { os << "qss1_square\n"; }

    void write(const qss2_square& /*dyn*/) noexcept { os << "qss2_square\n"; }

    void write(const qss3_square& /*dyn*/) noexcept { os << "qss3_square\n"; }

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

    void write(const filter& dyn) noexcept
    {
        os << "filter " << dyn.default_lower_threshold << ' '
           << dyn.default_upper_threshold << '\n';
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

        irt::model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            dispatch(
              *mdl, [this, &sim, &mdl]<typename Dynamics>(Dynamics& dyn) {
                  if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                      int i = 0;
                      for (const auto elem : dyn.y) {
                          auto list = get_node(sim, elem);
                          for (const auto& cnt : list) {
                              auto* dst = sim.models.try_to_get(cnt.model);
                              if (dst) {
                                  auto src_id = sim.models.get_id(*mdl);

                                  os << irt::get_key(src_id) << " -> "
                                     << irt::get_key(cnt.model) << " [label=\""
                                     << i << " - " << cnt.port_index
                                     << "\"];\n";
                              }
                          }

                          ++i;
                      }
                  }
              });
        }
    }
};

} // namespace irt

#endif
