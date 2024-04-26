// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/io.hpp>
#include <type_traits>

namespace irt {

struct file_header {
    enum class mode_type : i32 {
        none = 0,
        all  = 1,
    };

    i32       code    = 0x11223344;
    i32       length  = sizeof(file_header);
    i32       version = 1;
    mode_type type    = mode_type::all;
};

struct archiver_tag_type {};
struct dearchiver_tag_type {};

template<typename T>
concept IOTag = std::is_same_v<T, archiver_tag_type> or
                std::is_same_v<T, dearchiver_tag_type>;

template<typename Stream>
struct write_operator {
    template<typename T>
    status operator()(Stream& f, T t) noexcept
    {
        return f.write(t);
    }

    template<typename T>
    status operator()(Stream& f, T* t, const std::integral auto size) noexcept
    {
        return f.write(reinterpret_cast<void*>(t), sizeof(T) * size);
    }
};

template<typename Stream>
struct read_operator {
    template<typename T>
    status operator()(Stream& f, T& t) noexcept
    {
        return f.read(t);
    }

    template<typename T>
    status operator()(Stream& f, T* t, const std::integral auto size) noexcept
    {
        return f.read(reinterpret_cast<void*>(t), sizeof(T) * size);
    }
};

template<typename Stream, typename ReadOrWriteOperator>
struct read_write_operator {
    read_write_operator(Stream& io_) noexcept
      : io(io_)
    {}

    bool is_eof() const noexcept { return io.is_eof(); }

    template<typename T>
        requires(std::is_same_v<ReadOrWriteOperator, read_operator<Stream>> and
                 (std::is_arithmetic_v<T> or std::is_enum_v<T>))
    void operator()(T& value) noexcept
    {
        ReadOrWriteOperator op{};

        if (state)
            state = op(io, value);
    }

    template<typename T>
        requires(std::is_same_v<ReadOrWriteOperator, write_operator<Stream>> and
                 (std::is_arithmetic_v<T> or std::is_enum_v<T>))
    void operator()(const T value) noexcept
    {
        ReadOrWriteOperator op{};

        if (state)
            state = op(io, value);
    }

    template<class T>
    void operator()(T* t, const std::integral auto size) noexcept
    {
        ReadOrWriteOperator op{};

        if (state)
            state = op(io, t, size);
    }

    Stream& io;
    status  state = success();
};

template<typename Stream>
using write_binary_simulation =
  read_write_operator<Stream, write_operator<Stream>>;

template<typename Stream>
using read_binary_simulation =
  read_write_operator<Stream, read_operator<Stream>>;

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss1_integrator& dyn) noexcept
{
    io(dyn.default_X);
    io(dyn.default_dQ);
    io(dyn.X);
    io(dyn.q);
    io(dyn.u);
    io(dyn.sigma);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss2_integrator& dyn) noexcept
{
    io(dyn.default_X);
    io(dyn.default_dQ);
    io(dyn.X);
    io(dyn.u);
    io(dyn.mu);
    io(dyn.q);
    io(dyn.mq);
    io(dyn.sigma);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss3_integrator& dyn) noexcept
{
    io(dyn.default_X);
    io(dyn.default_dQ);
    io(dyn.X);
    io(dyn.u);
    io(dyn.mu);
    io(dyn.pu);
    io(dyn.q);
    io(dyn.mq);
    io(dyn.pq);
    io(dyn.sigma);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss1_power& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.default_n);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss2_power& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.default_n);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss3_power& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.default_n);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss1_square& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss2_square& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss3_square& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.value), std::size(dyn.value));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss1_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss1_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss1_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss2_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss2_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss2_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss3_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss3_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss3_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss1_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss1_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss1_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss2_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss2_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss2_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss3_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss3_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss3_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
    io(std::data(dyn.default_input_coeffs),
       std::size(dyn.default_input_coeffs));
    io(std::data(dyn.default_values), std::size(dyn.default_values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss1_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss2_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&              io,
                                  qss3_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.values), std::size(dyn.values));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&      io,
                                  counter& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.number);
}

u32 get_source_index(u64 id) noexcept { return unpack_doubleword_right(id); }

template<typename IO>
static void do_serialize_source(const IOTag auto /*s*/,
                                IO&     io,
                                source& src) noexcept
{
    auto ta_id = get_source_index(src.id);
    io(ta_id);
    io(src.type);
    io(src.index);
    io(src.chunk_id.data(), src.chunk_id.size());
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto s,
                                  binary_archiver::cache& /*c*/,
                                  IO&        io,
                                  generator& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
    io(dyn.default_offset);

    do_serialize_source(s, io, dyn.default_source_ta);
    do_serialize_source(s, io, dyn.default_source_value);

    io(dyn.stop_on_error);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&       io,
                                  constant& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_value);
    io(dyn.default_offset);
    io(dyn.value);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&            io,
                                  logical_and_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.default_values), std::size(dyn.default_values));
    io(std::data(dyn.values), std::size(dyn.values));
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&            io,
                                  logical_and_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.default_values), std::size(dyn.default_values));
    io(std::data(dyn.values), std::size(dyn.values));
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&           io,
                                  logical_or_2& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.default_values), std::size(dyn.default_values));
    io(std::data(dyn.values), std::size(dyn.values));
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&           io,
                                  logical_or_3& dyn) noexcept
{
    io(dyn.sigma);
    io(std::data(dyn.default_values), std::size(dyn.default_values));
    io(std::data(dyn.values), std::size(dyn.values));
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&             io,
                                  logical_invert& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_value);
    io(dyn.value);
    io(dyn.value_changed);
}

template<IOTag T, typename IO>
static void do_serialize_dynamics(const T /*s*/,
                                  binary_archiver::cache& c,
                                  IO&                     io,
                                  hsm_wrapper&            dyn) noexcept
{
    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        u32 index{ 0 };
        io(index);

        auto* id = c.to_hsms.get(index);
        debug::ensure(id != nullptr);
        dyn.id = *id;
    } else {
        u32 index = get_index(dyn.id);
        io(index);
    }

    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        u32 size = {};
        io(size);

        dyn.exec.outputs.resize(size);

        for (u32 i = 0u; i < size; ++i)
            io(dyn.exec.outputs[i].value);
        for (u32 i = 0u; i < size; ++i)
            io(dyn.exec.outputs[i].port);
    } else {
        u32 size = dyn.exec.outputs.size();
        io(size);

        for (u32 i = 0u; i < size; ++i)
            io(dyn.exec.outputs[i].value);
        for (u32 i = 0u; i < size; ++i)
            io(dyn.exec.outputs[i].port);
    }

    io(dyn.exec.current_state);
    io(dyn.exec.next_state);
    io(dyn.exec.source_state);
    io(dyn.exec.current_source_state);
    io(dyn.exec.previous_state);
    io(dyn.exec.disallow_transition);
    io(dyn.exec.previous_state);
    io(dyn.sigma);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&            io,
                                  accumulator_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.number);
    io(std::data(dyn.numbers), std::size(dyn.numbers));
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss1_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(std::data(dyn.if_value), std::size(dyn.if_value));
    io(std::data(dyn.else_value), std::size(dyn.else_value));
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss2_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(std::data(dyn.if_value), std::size(dyn.if_value));
    io(std::data(dyn.else_value), std::size(dyn.else_value));
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&         io,
                                  qss3_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(std::data(dyn.if_value), std::size(dyn.if_value));
    io(std::data(dyn.else_value), std::size(dyn.else_value));
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss1_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss2_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&          io,
                                  qss3_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(std::data(dyn.value), std::size(dyn.value));
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<IOTag T, typename IO>
static void do_serialize_dynamics(const T /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&        io,
                                  time_func& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_sigma);

    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        i32 default_f = 0;
        i32 f         = 0;

        io(default_f);
        io(f);

        dyn.default_f = default_f == 0   ? sin_time_function
                        : default_f == 1 ? square_time_function
                                         : time_function;

        dyn.f = f == 0   ? sin_time_function
                : f == 1 ? square_time_function
                         : time_function;

    } else {
        u32 default_f = dyn.default_f == sin_time_function      ? 0u
                        : dyn.default_f == square_time_function ? 1u
                                                                : 2u;
        u32 f         = dyn.f == sin_time_function      ? 0u
                        : dyn.f == square_time_function ? 1u
                                                        : 2u;

        io(default_f);
        io(f);
    }
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto /*s*/,
                                  binary_archiver::cache& /*c*/,
                                  IO&    io,
                                  queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);
    io(dyn.default_ta);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto s,
                                  binary_archiver::cache& /*c*/,
                                  IO&            io,
                                  dynamic_queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);

    do_serialize_source(s, io, dyn.default_source_ta);

    io(dyn.stop_on_error);
}

template<typename IO>
static void do_serialize_dynamics(const IOTag auto s,
                                  binary_archiver::cache& /*c*/,
                                  IO&             io,
                                  priority_queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);
    io(dyn.default_ta);

    do_serialize_source(s, io, dyn.default_source_ta);

    io(dyn.stop_on_error);
}

template<typename IO>
static void do_serialize(
  const IOTag auto /*s*/,
  IO&                                       io,
  hierarchical_state_machine::state_action& action) noexcept
{
    io(action.parameter);
    io(action.var1);
    io(action.var2);
    io(action.type);
}

template<typename IO>
static void do_serialize(
  const IOTag auto /*s*/,
  IO&                                           io,
  hierarchical_state_machine::condition_action& action) noexcept
{
    io(action.parameter);
    io(action.type);
    io(action.port);
    io(action.mask);
}

template<typename IO>
static void do_serialize(const IOTag auto s,
                         binary_archiver::cache& /*c*/,
                         IO&                         io,
                         hierarchical_state_machine& hsm) noexcept
{
    {
        for (int i = 0, e = length(hsm.states); i != e; ++i) {
            do_serialize(s, io, hsm.states[i].enter_action);
            do_serialize(s, io, hsm.states[i].exit_action);
            do_serialize(s, io, hsm.states[i].if_action);
            do_serialize(s, io, hsm.states[i].else_action);
            do_serialize(s, io, hsm.states[i].condition);

            io(hsm.states[i].if_transition);
            io(hsm.states[i].else_transition);
            io(hsm.states[i].super_id);
            io(hsm.states[i].sub_id);
        }
    }

    io(hsm.top_state);
}

template<IOTag T, typename IO>
static void do_serialize_model(const T                 s,
                               binary_archiver::cache& c,
                               IO&                     io,
                               model&                  mdl) noexcept
{
    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        auto type = ordinal(dynamics_type::counter);
        io(type);
        mdl.type = enum_cast<dynamics_type>(type);
    }

    if constexpr (std::is_same_v<T, archiver_tag_type>) {
        auto type = ordinal(mdl.type);
        io(type);
    }

    dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
        do_serialize_dynamics(s, c, io, dyn);
    });
}

template<typename IO>
static void do_serialize_external_source(const IOTag auto /*s*/,
                                         IO&              io,
                                         constant_source& src) noexcept
{
    io(src.buffer.data(), src.buffer.size());
}

template<IOTag T, typename IO>
static void do_serialize_external_source(const T /*s*/,
                                         IO&                 io,
                                         binary_file_source& src) noexcept
{
    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        vector<char> buffer;

        i32 size = 0;
        io(size);
        debug::ensure(size > 0);
        buffer.resize(size);
        io(buffer.data(), size);
        buffer[size] = '\0';
        src.file_path.assign(buffer.begin(), buffer.end());
    } else {
        auto str       = src.file_path.string();
        auto orig_size = str.size();
        debug::ensure(orig_size < INT32_MAX);
        i32 size = static_cast<i32>(orig_size);
        io(size);
        io(str.data(), str.size());
    }
}

template<IOTag T, typename IO>
static void do_serialize_external_source(const T /*s*/,
                                         IO&               io,
                                         text_file_source& src) noexcept
{
    if constexpr (std::is_same_v<T, dearchiver_tag_type>) {
        vector<char> buffer;

        i32 size = 0;
        io(size);
        debug::ensure(size > 0);
        buffer.resize(size);
        io(buffer.data(), size);
        buffer[size] = '\0';
        src.file_path.assign(buffer.begin(), buffer.end());
    } else {
        auto str       = src.file_path.string();
        auto orig_size = str.size();
        debug::ensure(orig_size < INT32_MAX);
        i32 size = static_cast<i32>(orig_size);
        io(size);
        io(str.data(), str.size());
    }
}

template<typename IO>
static void do_serialize_external_source(const IOTag auto /*s*/,
                                         IO&            io,
                                         random_source& src) noexcept
{
    io(src.distribution);

    io(src.a);
    io(src.b);
    io(src.p);
    io(src.mean);
    io(src.lambda);
    io(src.alpha);
    io(src.beta);
    io(src.stddev);
    io(src.m);
    io(src.s);
    io(src.n);
    io(src.a32);
    io(src.b32);
    io(src.t32);
    io(src.k32);
}

template<typename IO>
static status do_serialize(const IOTag auto        arc,
                           binary_archiver::cache& c,
                           simulation&             sim,
                           IO&                     io) noexcept
{
    {
        auto constant_external_source = sim.srcs.constant_sources.ssize();
        auto binary_external_source   = sim.srcs.binary_file_sources.ssize();
        auto text_external_source     = sim.srcs.text_file_sources.ssize();
        auto random_external_source   = sim.srcs.random_sources.ssize();
        auto models                   = sim.models.ssize();
        auto hsms                     = sim.hsms.ssize();

        io(constant_external_source);
        io(binary_external_source);
        io(text_external_source);
        io(random_external_source);
        io(models);
        io(hsms);
    }

    {
        constant_source* src = nullptr;
        while (sim.srcs.constant_sources.next(src)) {
            auto id    = sim.srcs.constant_sources.get_id(src);
            auto index = get_index(id);

            io(index);
            io(src->length);
            do_serialize_external_source(arc, io, *src);
        }
    }

    {
        binary_file_source* src = nullptr;
        while (sim.srcs.binary_file_sources.next(src)) {
            auto id    = sim.srcs.binary_file_sources.get_id(src);
            auto index = get_index(id);

            io(index);
            io(src->max_clients);
            do_serialize_external_source(arc, io, *src);
        }
    }

    {
        text_file_source* src = nullptr;
        while (sim.srcs.text_file_sources.next(src)) {
            auto id    = sim.srcs.text_file_sources.get_id(src);
            auto index = get_index(id);

            io(index);
            do_serialize_external_source(arc, io, *src);
        }
    }

    {
        random_source* src = nullptr;
        while (sim.srcs.random_sources.next(src)) {
            auto id    = sim.srcs.random_sources.get_id(src);
            auto index = get_index(id);

            io(index);
            do_serialize_external_source(arc, io, *src);
        }
    }

    {
        hierarchical_state_machine* hsm = nullptr;
        while (sim.hsms.next(hsm)) {
            auto id    = sim.hsms.get_id(*hsm);
            auto index = get_index(id);
            io(index);
            do_serialize(arc, c, io, *hsm);
        }
    }

    {
        model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            const auto id    = sim.models.get_id(*mdl);
            const auto index = get_index(id);
            io(index);
            do_serialize_model(arc, c, io, *mdl);
        }
    }

     {
         model* mdl = nullptr;
         while (sim.models.next(mdl)) {
             dispatch(*mdl, [&sim, &mdl, &io]<typename Dynamics>(Dynamics&
             dyn) {
                 if constexpr (has_output_port<Dynamics>) {
                     i8         i      = 0;
                     const auto out_id = sim.models.get_id(mdl);

                     for (const auto elem : dyn.y) {
                         if (auto* lst = sim.nodes.try_to_get(elem); lst)
                         {
                             for (const auto& cnt : *lst) {
                                 auto* dst =
                                 sim.models.try_to_get(cnt.model); if
                                 (dst) {
                                     auto out = get_index(out_id);
                                     auto in  = get_index(cnt.model);

                                     io(out);
                                     io(i);
                                     io(in);
                                     io(cnt.port_index);
                                 }
                             }

                             ++i;
                         }
                     }
                 }
             });
         }
     }

    return success();
}

template<typename IO>
static status do_deserialize(const IOTag auto        s,
                             binary_archiver::cache& c,
                             simulation&             sim,
                             IO&                     io) noexcept
{
    i32 constant_external_source = 0;
    i32 binary_external_source   = 0;
    i32 text_external_source     = 0;
    i32 random_external_source   = 0;
    i32 models                   = 0;
    i32 hsms                     = 0;

    {
        io(constant_external_source);
        io(binary_external_source);
        io(text_external_source);
        io(random_external_source);
        io(models);
        io(hsms);

        if (constant_external_source < 0)
            return new_error(binary_archiver::file_format_error{});
        if (binary_external_source < 0)
            return new_error(binary_archiver::file_format_error{});
        if (text_external_source < 0)
            return new_error(binary_archiver::file_format_error{});
        if (random_external_source < 0)
            return new_error(binary_archiver::file_format_error{});
        if (models < 0)
            return new_error(binary_archiver::file_format_error{});
        if (hsms < 0)
            return new_error(binary_archiver::file_format_error{});
        if (!io.state)
            return new_error(binary_archiver::file_format_error{});

        sim.clean();

        sim.srcs.constant_sources.clear();
        sim.srcs.binary_file_sources.clear();
        sim.srcs.text_file_sources.clear();
        sim.srcs.random_sources.clear();
        sim.models.clear();
        sim.hsms.clear();

        sim.srcs.constant_sources.reserve(constant_external_source);
        sim.srcs.binary_file_sources.reserve(binary_external_source);
        sim.srcs.text_file_sources.reserve(text_external_source);
        sim.srcs.random_sources.reserve(random_external_source);
        sim.models.reserve(models);
        sim.hsms.reserve(hsms);

        if (not sim.srcs.constant_sources.can_alloc() or
            not sim.srcs.binary_file_sources.can_alloc() or
            not sim.srcs.text_file_sources.can_alloc() or
            not sim.srcs.random_sources.can_alloc() or
            not sim.models.can_alloc() or not sim.hsms.can_alloc())
            return new_error(binary_archiver::not_enough_memory{});
    }

    c.clear();

    if (constant_external_source > 0) {
        c.to_constant.data.reserve(constant_external_source);
        for (i32 i = 0; i < constant_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.constant_sources.alloc();
            auto  id  = sim.srcs.constant_sources.get_id(src);
            c.to_constant.data.emplace_back(index, id);

            do_serialize_external_source(s, io, src);
        }
        c.to_constant.sort();
    }

    if (binary_external_source > 0) {
        c.to_binary.data.reserve(binary_external_source);
        for (i32 i = 0; i < binary_external_source; ++i) {
            u32 index = 0u;
            io(index);

            u32 max_clients = 0;
            io(max_clients);

            auto& src = sim.srcs.binary_file_sources.alloc();
            auto  id  = sim.srcs.binary_file_sources.get_id(src);
            c.to_binary.data.emplace_back(index, id);
            src.max_clients = max_clients;

            do_serialize_external_source(s, io, src);
        }
        c.to_binary.sort();
    }

    if (text_external_source > 0) {
        c.to_text.data.reserve(text_external_source);
        for (i32 i = 0u; i < text_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.text_file_sources.alloc();
            auto  id  = sim.srcs.text_file_sources.get_id(src);
            c.to_text.data.emplace_back(index, id);

            do_serialize_external_source(s, io, src);
        }
        c.to_text.sort();
    }

    if (random_external_source > 0) {
        c.to_random.data.reserve(random_external_source);
        for (i32 i = 0u; i < random_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.random_sources.alloc();
            auto  id  = sim.srcs.random_sources.get_id(src);
            c.to_random.data.emplace_back(index, id);

            do_serialize_external_source(s, io, src);
        }
        c.to_random.sort();
    }

    if (hsms > 0) {
        c.to_hsms.data.reserve(hsms);
        for (i32 i = 0u; i < hsms; ++i) {
            u32 index = 0;
            io(index);

            auto& hsm = sim.hsms.alloc();
            auto  id  = sim.hsms.get_id(hsm);
            c.to_hsms.data.emplace_back(index, id);

            do_serialize(s, c, io, hsm);
        }
        c.to_hsms.sort();
    }

    if (models > 0) {
        c.to_models.data.reserve(models);
        for (i32 i = 0u; i < models; ++i) {
            u32 index = 0;
            io(index);

            auto& mdl = sim.models.alloc();
            auto  id  = sim.models.get_id(mdl);
            c.to_models.data.emplace_back(index, id);
            do_serialize_model(s, c, io, mdl);
        }
        c.to_models.sort();
    }

     while (not io.is_eof()) {
         u32 out = 0, in = 0;
         i8  port_out = 0, port_in = 0;

         io(out);
         io(port_out);
         io(in);
         io(port_in);

         if (!io.state)
             break;

         auto* out_id = c.to_models.get(out);
         auto* in_id  = c.to_models.get(in);
         if (!out_id || !in_id)
             return new_error(binary_archiver::unknown_model_error{});

         debug::ensure(out_id && in_id);

         auto* mdl_src =
         sim.models.try_to_get(enum_cast<model_id>(*out_id)); if
         (!mdl_src)
             return new_error(binary_archiver::unknown_model_error{});

         auto* mdl_dst =
         sim.models.try_to_get(enum_cast<model_id>(*in_id)); if (!mdl_dst)
             return new_error(binary_archiver::unknown_model_error{});

         node_id*    pout = nullptr;
         message_id* pin  = nullptr;

         if (auto ret = get_output_port(*mdl_src, port_out, pout); !ret)
             return
             new_error(binary_archiver::unknown_model_port_error{});
         if (auto ret = get_input_port(*mdl_dst, port_in, pin); !ret)
             return
             new_error(binary_archiver::unknown_model_port_error{});
         if (auto ret = sim.connect(*mdl_src, port_out, *mdl_dst,
         port_in); !ret)
             return
             new_error(binary_archiver::unknown_model_port_error{});
     }

    return success();
}

status binary_archiver::simulation_save(simulation& sim, file& f) noexcept
{
    if (f.is_open())
        return new_error(open_file_error{});

    if (any_equal(f.get_mode(), open_mode::write, open_mode::append))
        return new_error(write_file_error{});

    write_binary_simulation<file> writer{ f };

    file_header fh;
    writer(fh.code);
    writer(fh.length);
    writer(fh.version);
    writer(fh.type);

    if (!writer.state)
        return writer.state.error();

    return do_serialize(archiver_tag_type{}, c, sim, writer);
}

status binary_archiver::simulation_save(simulation& sim, memory& m) noexcept
{
    write_binary_simulation<memory> writer{ m };

    return do_serialize(archiver_tag_type{}, c, sim, writer);
}

status binary_archiver::simulation_load(simulation& sim, file& f) noexcept
{
    if (f.is_open())
        return new_error(open_file_error{});

    if (any_equal(f.get_mode(), open_mode::write, open_mode::append))
        return new_error(write_file_error{});

    read_binary_simulation<file> reader{ f };

    file_header fh;
    reader(fh.code);
    reader(fh.length);
    reader(fh.version);
    reader(fh.type);

    if (!reader.state)
        return reader.state.error();

    if (!(fh.code == 0x11223344))
        return new_error(file_format_error{});
    if (!(fh.length == sizeof(file_header)))
        return new_error(file_format_error{});
    if (!(fh.version == 1))
        return new_error(file_format_error{});
    if (!(fh.type == file_header::mode_type::all))
        return new_error(file_format_error{});

    return do_deserialize(dearchiver_tag_type{}, c, sim, reader);
}

status binary_archiver::simulation_load(simulation& sim, memory& m) noexcept
{
    read_binary_simulation<memory> reader{ m };

    return do_deserialize(dearchiver_tag_type{}, c, sim, reader);
}

void binary_archiver::cache::clear() noexcept
{
    to_models.data.clear();
    to_hsms.data.clear();
    to_constant.data.clear();
    to_binary.data.clear();
    to_text.data.clear();
    to_random.data.clear();
}

} //  irt
