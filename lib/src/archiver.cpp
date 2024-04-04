// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/io.hpp>

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

static inline constexpr archiver_tag_type   archiver;
static inline constexpr dearchiver_tag_type dearchiver;

template<typename Stream>
struct write_operator {
    template<typename T>
    bool operator()(Stream& f, T t) noexcept
    {
        return f.write(t);
    }

    template<typename T>
    bool operator()(Stream& f, std::span<T> t) noexcept
    {
        return f.write(t.data(), t.size());
    }
};

template<typename Stream>
struct read_operator {
    template<typename T>
    bool operator()(Stream& f, T& t) noexcept
    {
        return f.read(t);
    }

    template<typename T>
    bool operator()(Stream& f, std::span<T> t) noexcept
    {
        return f.read(t.data(), t.size());
    }
};

template<typename Stream, typename ReadOrWriteOperator>
struct read_write_operator {
    read_write_operator(Stream& io_) noexcept
      : io(io_)
    {}

    template<typename T>
        requires(std::is_same_v<ReadOrWriteOperator, read_operator<Stream>>)
    void operator()(T& value) noexcept
    {
        ReadOrWriteOperator op{};

        if (success)
            success = op(io, value);
    }

    template<typename T>
        requires(std::is_same_v<ReadOrWriteOperator, write_operator<Stream>>)
    void operator()(const T value) noexcept
    {
        ReadOrWriteOperator op{};

        if (success)
            success = op(io, value);
    }

    template<class T, size_t N>
    void operator()(T (&array)[N]) noexcept
    {
        ReadOrWriteOperator op{};

        if (success) {
            for (size_t i = 0; i < N; ++i) {
                if (!op(io, array[i])) {
                    success = false;
                    break;
                }
            }
        }
    }

    template<class T>
    void operator()(std::span<T> values) noexcept
    {
        ReadOrWriteOperator op{};

        if (success) {
            if (!op(io, values))
                success = false;
        }
    }

    Stream& io;
    bool    success = true;
};

template<typename Stream>
using write_binary_simulation =
  read_write_operator<Stream, write_operator<Stream>>;

template<typename Stream>
using read_binary_simulation =
  read_write_operator<Stream, read_operator<Stream>>;

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
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

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
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

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
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

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss1_power& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
    io(dyn.default_n);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss2_power& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
    io(dyn.default_n);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss3_power& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
    io(dyn.default_n);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss1_square& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss2_square& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss3_square& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss1_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss1_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss1_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss2_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss2_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss2_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss3_sum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss3_sum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&         io,
                                  qss3_sum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss1_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss1_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss1_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss2_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss2_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss2_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss3_wsum_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss3_wsum_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss3_wsum_4& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
    io(dyn.default_input_coeffs);
    io(dyn.default_values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&              io,
                                  qss1_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&              io,
                                  qss2_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&              io,
                                  qss3_multiplier& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.values);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&      io,
                                  counter& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.number);
}

u32 get_source_index(u64 id) noexcept { return unpack_doubleword_right(id); }

template<typename Archiver, typename IO>
static void do_serialize_source(const Archiver /*s*/,
                                IO&     io,
                                source& src) noexcept
{
    auto ta_id = get_source_index(src.id);
    io(ta_id);
    io(src.type);
    io(src.index);
    io(std::span(src.chunk_id.data(), src.chunk_id.size()));
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver s,
                                  IO&            io,
                                  generator&     dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.value);
    io(dyn.default_offset);

    do_serialize_source(s, io, dyn.default_source_ta);
    do_serialize_source(s, io, dyn.default_source_value);

    io(dyn.stop_on_error);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&       io,
                                  constant& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_value);
    io(dyn.default_offset);
    io(dyn.value);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&            io,
                                  logical_and_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_values);
    io(dyn.values);
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&            io,
                                  logical_and_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_values);
    io(dyn.values);
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&           io,
                                  logical_or_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_values);
    io(dyn.values);
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&           io,
                                  logical_or_3& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_values);
    io(dyn.values);
    io(dyn.is_valid);
    io(dyn.value_changed);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&             io,
                                  logical_invert& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_value);
    io(dyn.value);
    io(dyn.value_changed);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver s,
                                  IO&            io,
                                  hsm_wrapper&   dyn) noexcept
{
    if constexpr (std::is_same_v<Archiver, dearchiver_tag_type>) {
        i32 index{ 0 };
        io(index);

        if (auto* id = s.u32_to_hsms.get(index); id) {
            dyn.id = *id;
        } else {
            io.success = false;
            return;
        }
    } else {
        auto index = get_index(dyn.id);
        io(index);
    }

    {
        auto size = dyn.exec.outputs.size();
        io(size);

        if (size > 0u)
            io(std::span(dyn.exec.outputs.data(), dyn.exec.outputs.size()));
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

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&            io,
                                  accumulator_2& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.number);
    io(dyn.numbers);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/, IO& io, qss1_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(dyn.if_value);
    io(dyn.else_value);
    io(dyn.value);
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/, IO& io, qss2_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(dyn.if_value);
    io(dyn.else_value);
    io(dyn.value);
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/, IO& io, qss3_cross& dyn)
{
    io(dyn.sigma);
    io(dyn.default_threshold);
    io(dyn.default_detect_up);
    io(dyn.threshold);
    io(dyn.if_value);
    io(dyn.else_value);
    io(dyn.value);
    io(dyn.last_reset);
    io(dyn.reach_threshold);
    io(dyn.detect_up);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss1_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(dyn.value);
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss2_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(dyn.value);
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&          io,
                                  qss3_filter& dyn)
{
    io(dyn.sigma);
    io(dyn.default_lower_threshold);
    io(dyn.default_upper_threshold);
    io(dyn.lower_threshold);
    io(dyn.upper_threshold);
    io(dyn.value);
    io(dyn.reach_lower_threshold);
    io(dyn.reach_upper_threshold);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&        io,
                                  time_func& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.default_sigma);

    if constexpr (std::is_same_v<Archiver, dearchiver_tag_type>) {
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
        i32 default_f = dyn.default_f == sin_time_function      ? 0
                        : dyn.default_f == square_time_function ? 1
                                                                : 2;
        i32 f         = dyn.f == sin_time_function      ? 0
                        : dyn.f == square_time_function ? 1
                                                        : 2;

        io(default_f);
        io(f);
    }
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver /*s*/,
                                  IO&    io,
                                  queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);
    io(dyn.default_ta);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver s,
                                  IO&            io,
                                  dynamic_queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);

    do_serialize_source(s, io, dyn.default_source_ta);

    io(dyn.stop_on_error);
}

template<typename Archiver, typename IO>
static void do_serialize_dynamics(const Archiver  s,
                                  IO&             io,
                                  priority_queue& dyn) noexcept
{
    io(dyn.sigma);
    io(dyn.fifo);
    io(dyn.default_ta);

    do_serialize_source(s, io, dyn.default_source_ta);

    io(dyn.stop_on_error);
}

template<typename Archiver, typename IO>
static void do_serialize(
  const Archiver /*s*/,
  IO&                                       io,
  hierarchical_state_machine::state_action& action) noexcept
{
    io(action.parameter);
    io(action.var1);
    io(action.var2);
    io(action.type);
}

template<typename Archiver, typename IO>
static void do_serialize(
  const Archiver /*s*/,
  IO&                                           io,
  hierarchical_state_machine::condition_action& action) noexcept
{
    io(action.parameter);
    io(action.type);
    io(action.port);
    io(action.mask);
}

template<typename Archiver, typename IO>
static void do_serialize(const Archiver              s,
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

template<typename Archiver, typename IO>
static void do_serialize(const Archiver s, IO& io, model& mdl) noexcept
{
    io(mdl.tl);
    io(mdl.tn);

    if constexpr (std::is_same_v<Archiver, dearchiver_tag_type>) {
        i32 type = 0;
        io(type);

        if (type < 0 || type > (i32)dynamics_type_size()) {
            assert(false);
            io.success = false;
        }

        mdl.type = enum_cast<dynamics_type>(type);
    } else {
        i32 type = ordinal(mdl.type);
        io(type);
    }

    if (io.success) {
        dispatch(mdl, [&s, &io]<typename Dynamics>(Dynamics& dyn) {
            do_serialize_dynamics<Archiver, IO>(s, io, dyn);
        });
    }
}

template<typename Archiver, typename IO>
static void do_serialize_external_source(const Archiver /*s*/,
                                         IO&              io,
                                         constant_source& src) noexcept
{
    io(std::span(src.buffer.data(), src.buffer.size()));
}

template<typename Archiver, typename IO>
static void do_serialize_external_source(const Archiver /*s*/,
                                         IO&                 io,
                                         binary_file_source& src) noexcept
{
    if constexpr (std::is_same_v<Archiver, dearchiver_tag_type>) {
        vector<char> buffer;

        i32 size = 0;
        io(size);
        debug::ensure(size > 0);
        buffer.resize(size);
        io(std::span(buffer.data(), static_cast<size_t>(size)));
        buffer[size] = '\0';
        src.file_path.assign(buffer.begin(), buffer.end());
    } else {
        auto str       = src.file_path.string();
        auto orig_size = str.size();
        debug::ensure(orig_size < INT32_MAX);
        i32 size = static_cast<i32>(orig_size);
        io(size);
        io(std::span(str.data(), str.size()));
    }
}

template<typename Archiver, typename IO>
static void do_serialize_external_source(const Archiver /*s*/,
                                         IO&               io,
                                         text_file_source& src) noexcept
{
    if constexpr (std::is_same_v<Archiver, dearchiver_tag_type>) {
        vector<char> buffer;

        i32 size = 0;
        io(size);
        debug::ensure(size > 0);
        buffer.resize(size);
        io(std::span(buffer.data(), static_cast<size_t>(size)));
        buffer[size] = '\0';
        src.file_path.assign(buffer.begin(), buffer.end());
    } else {
        auto str       = src.file_path.string();
        auto orig_size = str.size();
        debug::ensure(orig_size < INT32_MAX);
        i32 size = static_cast<i32>(orig_size);
        io(size);
        io(std::span(str.data(), str.size()));
    }
}

template<typename Archiver, typename IO>
static void do_serialize_external_source(const Archiver /*s*/,
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

template<typename Archiver, typename IO>
static status do_serialize(const Archiver arc, simulation& sim, IO& io) noexcept
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
            do_serialize(arc, io, *hsm);
        }
    }

    {
        model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            auto id    = sim.models.get_id(*mdl);
            auto index = get_index(id);
            io(index);
            do_serialize(arc, io, *mdl);
        }
    }

    {
        model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            dispatch(*mdl, [&sim, &mdl, &io]<typename Dynamics>(Dynamics& dyn) {
                if constexpr (has_output_port<Dynamics>) {
                    i8         i      = 0;
                    const auto out_id = sim.models.get_id(mdl);

                    for (const auto elem : dyn.y) {
                        if (auto* lst = sim.nodes.try_to_get(elem); lst) {
                            for (const auto& cnt : *lst) {
                                auto* dst = sim.models.try_to_get(cnt.model);
                                if (dst) {
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

template<typename Dearchiver, typename IO>
static status do_deserialize(Dearchiver&             arc,
                             simulation&             sim,
                             IO&                     io,
                             binary_archiver::cache& cache) noexcept
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
        if (models <= 0)
            return new_error(binary_archiver::file_format_error{});
        if (hsms < 0)
            return new_error(binary_archiver::file_format_error{});
        if (!io.success)
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

    cache.clear();

    if (constant_external_source > 0) {
        cache.to_constant.data.reserve(constant_external_source);
        for (i32 i = 0; i < constant_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.constant_sources.alloc();
            auto  id  = sim.srcs.constant_sources.get_id(src);
            cache.to_constant.data.emplace_back(index, id);

            do_serialize_external_source(arc, io, src);
        }
        cache.to_constant.sort();
    }

    if (binary_external_source > 0) {
        cache.to_binary.data.reserve(binary_external_source);
        for (i32 i = 0; i < binary_external_source; ++i) {
            u32 index = 0u;
            io(index);

            u32 max_clients = 0;
            io(max_clients);

            auto& src = sim.srcs.binary_file_sources.alloc();
            auto  id  = sim.srcs.binary_file_sources.get_id(src);
            cache.to_binary.data.emplace_back(index, id);
            src.max_clients = max_clients;

            do_serialize_external_source(arc, io, src);
        }
        cache.to_binary.sort();
    }

    if (text_external_source > 0) {
        cache.to_text.data.reserve(text_external_source);
        for (i32 i = 0u; i < text_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.text_file_sources.alloc();
            auto  id  = sim.srcs.text_file_sources.get_id(src);
            cache.to_text.data.emplace_back(index, id);

            do_serialize_external_source(cache, io, src);
        }
        cache.to_text.sort();
    }

    if (random_external_source > 0) {
        cache.to_random.data.reserve(random_external_source);
        for (i32 i = 0u; i < random_external_source; ++i) {
            u32 index = 0u;
            io(index);

            auto& src = sim.srcs.random_sources.alloc();
            auto  id  = sim.srcs.random_sources.get_id(src);
            cache.to_random.data.emplace_back(index, id);

            do_serialize_external_source(arc, io, src);
        }
        cache.to_random.sort();
    }

    if (hsms > 0) {
        cache.to_hsms.data.reserve(hsms);
        for (i32 i = 0u; i < hsms; ++i) {
            u32 index = 0;
            io(index);

            auto& hsm = sim.hsms.alloc();
            auto  id  = sim.hsms.get_id(hsm);
            cache.to_hsms.data.emplace_back(index, id);

            do_serialize(arc, io, hsm);
        }
        cache.to_hsms.sort();
    }

    if (models > 0) {
        cache.to_models.data.reserve(models);
        for (i32 i = 0u; i < models; ++i) {
            u32 index = 0;
            io(index);

            auto& hsm = sim.models.alloc();
            auto  id  = sim.models.get_id(hsm);
            cache.to_models.data.emplace_back(index, id);

            do_serialize(cache, io, hsm);
        }
        cache.to_models.sort();
    }

    while (io.success) {
        u32 out = 0, in = 0;
        i8  port_out = 0, port_in = 0;

        io(out);
        io(port_out);
        io(in);
        io(port_in);

        if (!io.success)
            break;

        auto* out_id = cache.to_models.get(out);
        auto* in_id  = cache.to_models.get(in);
        if (!out_id || !in_id)
            return new_error(binary_archiver::unknown_model_error{});

        debug::ensure(out_id && in_id);

        auto* mdl_src = sim.models.try_to_get(enum_cast<model_id>(*out_id));
        if (!mdl_src)
            return new_error(binary_archiver::unknown_model_error{});

        auto* mdl_dst = sim.models.try_to_get(enum_cast<model_id>(*in_id));
        if (!mdl_dst)
            return new_error(binary_archiver::unknown_model_error{});

        node_id*    pout = nullptr;
        message_id* pin  = nullptr;

        if (auto ret = get_output_port(*mdl_src, port_out, pout); !ret)
            return new_error(binary_archiver::unknown_model_port_error{});
        if (auto ret = get_input_port(*mdl_dst, port_in, pin); !ret)
            return new_error(binary_archiver::unknown_model_port_error{});
        if (auto ret = sim.connect(*mdl_src, port_out, *mdl_dst, port_in); !ret)
            return new_error(binary_archiver::unknown_model_port_error{});
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

    if (!writer.success)
        return new_error(write_file_error{});

    return do_serialize(archiver, sim, writer);
}

status binary_archiver::simulation_save(simulation& sim, memory& m) noexcept
{
    write_binary_simulation<memory> writer{ m };

    return do_serialize(archiver, sim, writer);
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

    if (!reader.success)
        return new_error(file_format_error{});

    if (!(fh.code == 0x11223344))
        return new_error(file_format_error{});
    if (!(fh.length == sizeof(file_header)))
        return new_error(file_format_error{});
    if (!(fh.version == 1))
        return new_error(file_format_error{});
    if (!(fh.type == file_header::mode_type::all))
        return new_error(file_format_error{});

    return do_deserialize(dearchiver, sim, reader, c);
}

status binary_archiver::simulation_load(simulation& sim, memory& m) noexcept
{
    read_binary_simulation<memory> reader{ m };

    return do_deserialize(dearchiver, sim, reader, c);
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
