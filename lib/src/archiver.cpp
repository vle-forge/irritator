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

template<class T>
concept scoped_enum = std::is_enum_v<T>;

template<typename Support>
struct input_stream {
    static const dearchiver_tag_type type;

    input_stream(Support& f_) noexcept
      : stream(f_)
    {}

    bool operator()(std::integral auto& v) noexcept { return !!stream.read(v); }

    bool operator()(bool& v) noexcept
    {
        auto value   = static_cast<u8>(v);
        auto success = !!stream.read(value);

        if (success)
            v = value != 0;

        return success;
    }

    template<typename T>
    bool operator()(bitflags<T>& v) noexcept
    {
        auto value   = static_cast<u32>(v.to_unsigned());
        auto success = !!stream.read(value);

        if (success)
            v = bitflags<T>(value);

        return success;
    }

    bool operator()(scoped_enum auto& v) noexcept
    {
        auto value   = ordinal(v);
        auto success = !!stream.read(value);

        if (success)
            v = enum_cast<std::decay_t<decltype(v)>>(value);

        return success;
    }

    bool operator()(std::floating_point auto& v) noexcept
    {
        return !!stream.read(v);
    }

    template<typename T>
    bool operator()(T* buffer, std::integral auto size) noexcept
    {
        return !!stream.read(buffer, size);
    }

    template<std::integral auto Size>
    bool operator()(small_string<Size>& str) noexcept
    {
        if (auto ret = stream.read(str.data(), Size); ret) {
            if (auto ptr = std::strchr(str.data(), '\0'); ptr) {
                auto dif = ptr - str.data();
                str.resize(dif);
                return true;
            }
        }

        str.assign(std::string_view());
        return false;
    }

    Support& stream;
    int      errno;
};

template<typename Support>
struct output_stream {
    static const archiver_tag_type type;

    output_stream(Support& f_) noexcept
      : stream(f_)
    {}

    bool operator()(const std::integral auto v) noexcept
    {
        return !!stream.write(v);
    }

    bool operator()(const bool v) noexcept
    {
        const auto value = static_cast<u8>(v);

        return !!stream.write(value);
    }

    template<typename T>
    bool operator()(const bitflags<T> v) noexcept
    {
        const auto value = static_cast<u32>(v.to_unsigned());

        return !!stream.write(value);
    }

    bool operator()(const scoped_enum auto v) noexcept
    {
        const auto value = ordinal(v);

        return !!stream.write(value);
    }

    bool operator()(const std::floating_point auto v) noexcept
    {
        return !!stream.write(v);
    }

    template<typename T>
    bool operator()(const T* buffer, std::integral auto size) noexcept
    {
        return !!stream.write(buffer, size);
    }

    template<std::integral auto Size>
    bool operator()(const small_string<Size>& str) noexcept
    {
        if (auto ret = stream.write(str.data(), str.size()); ret) {
            for (auto i = str.size(); i < Size; ++i)
                if (not stream.write(static_cast<i8>('\0')))
                    return false;

            return true;
        }

        return false;
    }

    Support& stream;
};

using ifile = input_stream<file>;
using ofile = output_stream<file>;
using imem  = input_stream<memory>;
using omem  = output_stream<memory>;

template<typename T>
concept IsInputStream = std::is_same_v<T, ifile> or std::is_same_v<T, imem>;

template<typename T>
concept IsOutputStream = std::is_same_v<T, ofile> or std::is_same_v<T, omem>;

template<typename T>
concept IsStream = IsInputStream<T> or IsOutputStream<T>;

template<typename Stream>
bool io(Stream& stream, const std::integral auto t) noexcept
{
    static_assert(IsOutputStream<Stream>);

    return stream(t);
}

template<typename Stream>
bool io(Stream& stream, std::integral auto& t) noexcept
{
    static_assert(IsInputStream<Stream>);

    return stream(t);
}

template<typename Stream>
bool io(Stream&                   stream,
        const std::integral auto* buf,
        const std::integral auto  size) noexcept
{
    static_assert(IsOutputStream<Stream>);

    debug::ensure(buf != nullptr);
    debug::ensure(size > 0);

    return stream(reinterpret_cast<const void*>(buf),
                  sizeof(decltype(*buf)) * size);
}

struct binary_archiver::impl {
    binary_archiver& self;

    impl(binary_archiver& self_) noexcept
      : self(self_)
    {}

    bool save(simulation& sim, file& f) noexcept
    {
        ofile       ofs(f);
        file_header fh;

        if (not(ofs(fh.code) and ofs(fh.length) and ofs(fh.version) and
                ofs(fh.type)))
            self.report_error(error_code::header_error);

        return do_serialize(sim, ofs);
    }

    bool save(simulation& sim, memory& m) noexcept
    {
        omem ofs(m);

        return do_serialize(sim, ofs);
    }

    bool load(simulation& sim, file& f) noexcept
    {
        ifile ifs(f);

        file_header fh;

        if (not(ifs(fh.code) and ifs(fh.length) and ifs(fh.version) and
                ifs(fh.type)))
            return self.report_error(error_code::format_error);

        if (not(fh.code == 0x11223344 and fh.length == sizeof(file_header) and
                fh.version == 1 and fh.type == file_header::mode_type::all))
            return self.report_error(error_code::header_error);

        return do_deserialize(self, sim, ifs);
    }

    bool load(simulation& sim, memory& m) noexcept
    {
        imem ifs(m);

        return do_deserialize(self, sim, ifs);
    }

    template<typename Stream>
    bool io(Stream&                  stream,
            std::integral auto*      buf,
            const std::integral auto size) noexcept
    {
        static_assert(IsInputStream<Stream>);

        debug::ensure(buf != nullptr);
        debug::ensure(size > 0);

        return stream(reinterpret_cast<void*>(buf),
                      sizeof(decltype(*buf)) * size);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_integrator& dyn) noexcept
    {
        return io(dyn.default_X) and io(dyn.default_dQ) and io(dyn.X) and
               io(dyn.q) and io(dyn.u) and io(dyn.sigma);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_integrator& dyn) noexcept
    {
        return io(dyn.default_X) and io(dyn.default_dQ) and io(dyn.X) and
               io(dyn.u) and io(dyn.mu) and io(dyn.q) and io(dyn.mq) and
               io(dyn.sigma);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_integrator& dyn) noexcept
    {
        return io(dyn.default_X) and io(dyn.default_dQ) and io(dyn.X) and
               io(dyn.u) and io(dyn.mu) and io(dyn.pu) and io(dyn.q) and
               io(dyn.mq) and io(dyn.pq) and io(dyn.sigma);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_power& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.default_n);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_power& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.default_n);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_power& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.default_n);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_square& dyn) noexcept
    {
        return io(dyn.sigma) and io(std::data(dyn.value), std::size(dyn.value));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_square& dyn) noexcept
    {
        return io(dyn.sigma) and io(std::data(dyn.value), std::size(dyn.value));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_square& dyn) noexcept
    {
        return io(dyn.sigma) and io(std::data(dyn.value), std::size(dyn.value));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_sum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_sum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_sum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_sum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_sum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_sum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_sum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_sum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_sum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_wsum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_wsum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_wsum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_wsum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_wsum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_wsum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_wsum_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_wsum_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_wsum_4& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(std::data(dyn.default_input_coeffs),
                  std::size(dyn.default_input_coeffs)) and
               io(std::data(dyn.default_values), std::size(dyn.default_values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_multiplier& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_multiplier& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_multiplier& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.values), std::size(dyn.values));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, counter& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.number);
    }

    u32 get_source_index(u64 id) noexcept { return right(id); }

    template<typename Stream>
    bool do_serialize_source(Stream& io, source& src) noexcept
    {
        return io(src.id) and io(src.type) and io(src.index) and
               io(src.chunk_id.data(), src.chunk_id.size());
    }

    template<typename Stream>
    bool do_serialize_generator_flags(Stream&                      io,
                                      bitflags<generator::option>& opt) noexcept
    {
        static_assert(IsStream<Stream>);

        if constexpr (IsInputStream<Stream>) {
            u32 value;
            if (io(opt)) {
                opt = generator::option(value);
                return true;
            } else
                return false;
        } else {
            const u32 value = static_cast<u32>(opt.to_unsigned());
            return io(value);
        }
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, generator& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.value) and io(dyn.default_offset) and
               io(dyn.flags) and
               (dyn.flags[generator::option::ta_use_source]
                  ? do_serialize_source(io, dyn.default_source_ta)
                  : true) and
               (dyn.flags[generator::option::value_use_source]
                  ? do_serialize_source(io, dyn.default_source_value)
                  : true);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, constant& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.default_value) and
               io(dyn.default_offset) and io(dyn.value);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, logical_and_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.default_values),
                  std::size(dyn.default_values)) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(dyn.is_valid) and io(dyn.value_changed);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, logical_and_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.default_values),
                  std::size(dyn.default_values)) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(dyn.is_valid) and io(dyn.value_changed);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, logical_or_2& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.default_values),
                  std::size(dyn.default_values)) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(dyn.is_valid) and io(dyn.value_changed);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, logical_or_3& dyn) noexcept
    {
        return io(dyn.sigma) and
               io(std::data(dyn.default_values),
                  std::size(dyn.default_values)) and
               io(std::data(dyn.values), std::size(dyn.values)) and
               io(dyn.is_valid) and io(dyn.value_changed);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, logical_invert& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.default_value) and io(dyn.value) and
               io(dyn.value_changed);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, hsm_wrapper& dyn) noexcept
    {
        return io(dyn.exec.current_state) and io(dyn.exec.next_state) and
               io(dyn.exec.source_state) and
               io(dyn.exec.current_source_state) and
               io(dyn.exec.previous_state) and
               io(dyn.exec.disallow_transition) and
               io(dyn.exec.previous_state) and io(dyn.sigma);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, accumulator_2& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.number) and
               io(std::data(dyn.numbers), std::size(dyn.numbers));
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_cross& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_threshold) and
               io(dyn.default_detect_up) and io(dyn.threshold) and
               io(std::data(dyn.if_value), std::size(dyn.if_value)) and
               io(std::data(dyn.else_value), std::size(dyn.else_value)) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.last_reset) and io(dyn.reach_threshold) and
               io(dyn.detect_up);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_cross& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_threshold) and
               io(dyn.default_detect_up) and io(dyn.threshold) and
               io(std::data(dyn.if_value), std::size(dyn.if_value)) and
               io(std::data(dyn.else_value), std::size(dyn.else_value)) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.last_reset) and io(dyn.reach_threshold) and
               io(dyn.detect_up);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_cross& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_threshold) and
               io(dyn.default_detect_up) and io(dyn.threshold) and
               io(std::data(dyn.if_value), std::size(dyn.if_value)) and
               io(std::data(dyn.else_value), std::size(dyn.else_value)) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.last_reset) and io(dyn.reach_threshold) and
               io(dyn.detect_up);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss1_filter& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_lower_threshold) and
               io(dyn.default_upper_threshold) and io(dyn.lower_threshold) and
               io(dyn.upper_threshold) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.reach_lower_threshold) and io(dyn.reach_upper_threshold);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss2_filter& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_lower_threshold) and
               io(dyn.default_upper_threshold) and io(dyn.lower_threshold) and
               io(dyn.upper_threshold) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.reach_lower_threshold) and io(dyn.reach_upper_threshold);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, qss3_filter& dyn)
    {
        return io(dyn.sigma) and io(dyn.default_lower_threshold) and
               io(dyn.default_upper_threshold) and io(dyn.lower_threshold) and
               io(dyn.upper_threshold) and
               io(std::data(dyn.value), std::size(dyn.value)) and
               io(dyn.reach_lower_threshold) and io(dyn.reach_upper_threshold);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, time_func& dyn) noexcept
    {
        static_assert(IsStream<Stream>);

        if (not(io(dyn.sigma) and io(dyn.default_sigma)))
            return false;

        if constexpr (IsInputStream<Stream>) {
            i32 default_f = 0;
            i32 f         = 0;

            if (not(io(default_f) and io(f)))
                return false;

            dyn.default_f = default_f == 0   ? sin_time_function
                            : default_f == 1 ? square_time_function
                                             : time_function;

            dyn.f = f == 0   ? sin_time_function
                    : f == 1 ? square_time_function
                             : time_function;

            return true;
        } else {
            u32 default_f = dyn.default_f == sin_time_function      ? 0u
                            : dyn.default_f == square_time_function ? 1u
                                                                    : 2u;
            u32 f         = dyn.f == sin_time_function      ? 0u
                            : dyn.f == square_time_function ? 1u
                                                            : 2u;

            return io(default_f) and io(f);
        }
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, queue& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.fifo) and io(dyn.default_ta);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, dynamic_queue& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.fifo) and
               do_serialize_source(io, dyn.default_source_ta) and
               io(dyn.stop_on_error);
    }

    template<typename Stream>
    bool do_serialize_dynamics(Stream& io, priority_queue& dyn) noexcept
    {
        return io(dyn.sigma) and io(dyn.fifo) and io(dyn.default_ta) and
               do_serialize_source(io, dyn.default_source_ta) and
               io(dyn.stop_on_error);
    }

    template<typename Stream>
    bool do_serialize(Stream&                                   io,
                      hierarchical_state_machine::state_action& action) noexcept
    {
        return io(action.var1) and io(action.var2) and io(action.type) and
               io(action.constant.i);
    }

    template<typename Stream>
    bool do_serialize(
      Stream&                                       io,
      hierarchical_state_machine::condition_action& action) noexcept
    {
        return io(action.var1) and io(action.var2) and io(action.type) and
               io(action.constant.i);
    }

    template<typename Stream>
    bool do_serialize(Stream& io, hierarchical_state_machine& hsm) noexcept
    {
        for (int i = 0, e = length(hsm.states); i != e; ++i) {
            if (not((io(hsm.names[i])) and
                    do_serialize(io, hsm.states[i].enter_action) and
                    do_serialize(io, hsm.states[i].exit_action) and
                    do_serialize(io, hsm.states[i].if_action) and
                    do_serialize(io, hsm.states[i].else_action) and
                    do_serialize(io, hsm.states[i].condition) and
                    io(hsm.states[i].if_transition) and
                    io(hsm.states[i].else_transition) and
                    io(hsm.states[i].super_id) and io(hsm.states[i].sub_id)))
                return false;
        }

        return io(hsm.top_state);
    }

    template<typename Stream>
    bool do_serialize_model(Stream& io, model& mdl) noexcept
    {
        static_assert(IsStream<Stream>);

        if constexpr (IsInputStream<Stream>) {
            auto type = ordinal(dynamics_type::counter);
            if (not io(type))
                return false;

            mdl.type = enum_cast<dynamics_type>(type);
        } else {
            auto type = ordinal(mdl.type);
            if (not io(type))
                return false;
        }

        return dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
            return do_serialize_dynamics(io, dyn);
        });
    }

    template<typename Stream>
    bool do_serialize_external_source(Stream& io, constant_source& src) noexcept
    {
        return io(src.buffer.data(), src.buffer.size());
    }

    template<typename Stream>
    bool do_serialize_external_source(Stream&             io,
                                      binary_file_source& src) noexcept
    {
        static_assert(IsStream<Stream>);

        if constexpr (IsInputStream<Stream>) {
            vector<char> buffer;

            i32 size = 0;
            if (not(io(size) and size > 0))
                return false;

            buffer.resize(size);
            io(buffer.data(), size);
            buffer[size] = '\0';
            src.file_path.assign(buffer.begin(), buffer.end());
            return true;
        } else {
            auto str       = src.file_path.string();
            auto orig_size = str.size();
            debug::ensure(orig_size < INT32_MAX);
            i32 size = static_cast<i32>(orig_size);

            return io(size) and io(str.data(), str.size());
        }
    }

    template<typename Stream>
    bool do_serialize_external_source(Stream&           io,
                                      text_file_source& src) noexcept
    {
        static_assert(IsStream<Stream>);

        if constexpr (IsInputStream<Stream>) {
            vector<char> buffer;

            i32 size = 0;

            if (not(io(size) and size > 0))
                return false;

            buffer.resize(size);
            io(buffer.data(), size);
            buffer[size] = '\0';
            src.file_path.assign(buffer.begin(), buffer.end());
            return true;
        } else {
            auto str       = src.file_path.string();
            auto orig_size = str.size();
            debug::ensure(orig_size < INT32_MAX);
            i32 size = static_cast<i32>(orig_size);

            return io(size) and io(str.data(), str.size());
        }
    }

    template<typename Stream>
    bool do_serialize_external_source(Stream& io, random_source& src) noexcept
    {
        return io(src.distribution) and io(src.a) and io(src.b) and
               io(src.p) and io(src.mean) and io(src.lambda) and
               io(src.alpha) and io(src.beta) and io(src.stddev) and
               io(src.m) and io(src.s) and io(src.n) and io(src.a32) and
               io(src.b32) and io(src.t32) and io(src.k32);
    }

    template<typename Stream>
    bool do_serialize(simulation& sim, Stream& io) noexcept
    {
        {
            auto constant_external_source = sim.srcs.constant_sources.ssize();
            auto binary_external_source = sim.srcs.binary_file_sources.ssize();
            auto text_external_source   = sim.srcs.text_file_sources.ssize();
            auto random_external_source = sim.srcs.random_sources.ssize();
            auto models                 = sim.models.ssize();
            auto hsms                   = sim.hsms.ssize();

            if (not(io(constant_external_source) and
                    io(binary_external_source) and io(text_external_source) and
                    io(random_external_source) and io(models) and io(hsms)))
                return false;
        }

        {
            constant_source* src = nullptr;
            while (sim.srcs.constant_sources.next(src)) {
                auto id    = sim.srcs.constant_sources.get_id(src);
                auto index = get_index(id);

                if (not(io(index) and io(src->length) and
                        do_serialize_external_source(io, *src)))
                    return false;
            }
        }

        {
            binary_file_source* src = nullptr;
            while (sim.srcs.binary_file_sources.next(src)) {
                auto id    = sim.srcs.binary_file_sources.get_id(src);
                auto index = get_index(id);

                if (not(io(index) and io(src->max_clients) and
                        do_serialize_external_source(io, *src)))
                    return false;
            }
        }

        {
            text_file_source* src = nullptr;
            while (sim.srcs.text_file_sources.next(src)) {
                auto id    = sim.srcs.text_file_sources.get_id(src);
                auto index = get_index(id);

                if (not(io(index) and do_serialize_external_source(io, *src)))
                    return false;
            }
        }

        {
            random_source* src = nullptr;
            while (sim.srcs.random_sources.next(src)) {
                auto id    = sim.srcs.random_sources.get_id(src);
                auto index = get_index(id);

                if (not(io(index) and do_serialize_external_source(io, *src)))
                    return false;
            }
        }

        {
            hierarchical_state_machine* hsm = nullptr;
            while (sim.hsms.next(hsm)) {
                auto id    = sim.hsms.get_id(*hsm);
                auto index = get_index(id);

                if (not(io(index) and do_serialize(io, *hsm)))
                    return false;
            }
        }

        {
            model* mdl = nullptr;
            while (sim.models.next(mdl)) {
                const auto id    = sim.models.get_id(*mdl);
                const auto index = get_index(id);

                if (not(io(index) and do_serialize_model(io, *mdl)))
                    return false;
            }
        }

        {
            for (auto& mdl : sim.models) {
                dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                    if constexpr (has_output_port<Dynamics>) {
                        i8         i      = 0;
                        const auto out_id = sim.models.get_id(mdl);

                        for (const auto elem : dyn.y) {
                            if (auto* lst = sim.nodes.try_to_get(elem); lst) {
                                for (const auto& cnt : *lst) {
                                    auto* dst =
                                      sim.models.try_to_get(cnt.model);
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

        return true;
    }

    template<typename Stream>
    bool do_deserialize(binary_archiver& bin,
                        simulation&      sim,
                        Stream&          io) noexcept
    {
        i32 constant_external_source = 0;
        i32 binary_external_source   = 0;
        i32 text_external_source     = 0;
        i32 random_external_source   = 0;
        i32 models                   = 0;
        i32 hsms                     = 0;

        using error_code = binary_archiver::error_code;

        {
            if (not(io(constant_external_source) and
                    io(binary_external_source) and io(text_external_source) and
                    io(random_external_source) and io(models) and io(hsms)))
                return bin.report_error(error_code::format_error);

            if (constant_external_source < 0)
                return bin.report_error(error_code::format_error);
            if (binary_external_source < 0)
                return bin.report_error(error_code::format_error);
            if (text_external_source < 0)
                return bin.report_error(error_code::format_error);
            if (random_external_source < 0)
                return bin.report_error(error_code::format_error);
            if (models < 0)
                return bin.report_error(error_code::format_error);
            if (hsms < 0)
                return bin.report_error(error_code::format_error);

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
                return bin.report_error(
                  binary_archiver::error_code::not_enough_memory);
        }

        if (constant_external_source > 0) {
            self.to_constant.data.reserve(constant_external_source);
            for (i32 i = 0; i < constant_external_source; ++i) {
                u32 index = 0u;
                io(index);

                auto& src = sim.srcs.constant_sources.alloc();
                auto  id  = sim.srcs.constant_sources.get_id(src);
                self.to_constant.data.emplace_back(index, id);

                do_serialize_external_source(io, src);
            }
            self.to_constant.sort();
        }

        if (binary_external_source > 0) {
            self.to_binary.data.reserve(binary_external_source);
            for (i32 i = 0; i < binary_external_source; ++i) {
                u32 index = 0u;
                io(index);

                u32 max_clients = 0;
                io(max_clients);

                auto& src = sim.srcs.binary_file_sources.alloc();
                auto  id  = sim.srcs.binary_file_sources.get_id(src);
                self.to_binary.data.emplace_back(index, id);
                src.max_clients = max_clients;

                do_serialize_external_source(io, src);
            }
            self.to_binary.sort();
        }

        if (text_external_source > 0) {
            self.to_text.data.reserve(text_external_source);
            for (i32 i = 0u; i < text_external_source; ++i) {
                u32 index = 0u;
                io(index);

                auto& src = sim.srcs.text_file_sources.alloc();
                auto  id  = sim.srcs.text_file_sources.get_id(src);
                self.to_text.data.emplace_back(index, id);

                do_serialize_external_source(io, src);
            }
            self.to_text.sort();
        }

        if (random_external_source > 0) {
            self.to_random.data.reserve(random_external_source);
            for (i32 i = 0u; i < random_external_source; ++i) {
                u32 index = 0u;
                io(index);

                auto& src = sim.srcs.random_sources.alloc();
                auto  id  = sim.srcs.random_sources.get_id(src);
                self.to_random.data.emplace_back(index, id);

                do_serialize_external_source(io, src);
            }
            self.to_random.sort();
        }

        if (models > 0) {
            self.to_models.data.reserve(models);
            for (i32 i = 0u; i < models; ++i) {
                u32 index = 0;
                io(index);

                auto& mdl = sim.models.alloc();
                auto  id  = sim.models.get_id(mdl);
                self.to_models.data.emplace_back(index, id);
                do_serialize_model(io, mdl);
            }
            self.to_models.sort();
        }

        while (not io.stream.is_eof()) {
            u32 out = 0, in = 0;
            i8  port_out = 0, port_in = 0;

            if (not(io(out) and io(port_out) and io(in) and io(port_in)))
                return bin.report_error(error_code::unknown_model_error);

            auto* out_id = self.to_models.get(out);
            auto* in_id  = self.to_models.get(in);
            if (!out_id || !in_id)
                return bin.report_error(error_code::unknown_model_error);

            debug::ensure(out_id && in_id);

            auto* mdl_src = sim.models.try_to_get(enum_cast<model_id>(*out_id));
            if (!mdl_src)
                return bin.report_error(error_code::unknown_model_error);

            auto* mdl_dst = sim.models.try_to_get(enum_cast<model_id>(*in_id));
            if (!mdl_dst)
                return bin.report_error(error_code::unknown_model_error);

            node_id*    pout = nullptr;
            message_id* pin  = nullptr;

            if (auto ret = get_output_port(*mdl_src, port_out, pout); !ret)
                return bin.report_error(error_code::unknown_model_port_error);
            if (auto ret = get_input_port(*mdl_dst, port_in, pin); !ret)
                return bin.report_error(error_code::unknown_model_port_error);
            if (auto ret = sim.connect(*mdl_src, port_out, *mdl_dst, port_in);
                !ret)
                return bin.report_error(error_code::unknown_model_port_error);
        }

        return true;
    }
};

bool binary_archiver::report_error(error_code ec_) noexcept
{
    ec = ec_;
    debug::breakpoint();

    return false;
}

bool binary_archiver::report_error(error_code ec_) const noexcept
{
    const_cast<binary_archiver*>(this)->ec = ec_;
    debug::breakpoint();

    return false;
}

bool binary_archiver::simulation_save(simulation& sim, file& f) noexcept
{
    debug::ensure(f.is_open());
    debug::ensure(any_equal(f.get_mode(), open_mode::write, open_mode::append));

    binary_archiver::impl impl(*this);

    return impl.save(sim, f);
}

bool binary_archiver::simulation_save(simulation& sim, memory& m) noexcept
{
    binary_archiver::impl impl(*this);

    return impl.save(sim, m);
}

bool binary_archiver::simulation_load(simulation& sim, file& f) noexcept
{
    debug::ensure(f.is_open());
    debug::ensure(any_equal(f.get_mode(), open_mode::write, open_mode::append));

    binary_archiver::impl impl(*this);

    return impl.load(sim, f);
}

bool binary_archiver::simulation_load(simulation& sim, memory& m) noexcept
{
    binary_archiver::impl impl(*this);

    return impl.load(sim, m);
}

void binary_archiver::clear_cache() noexcept
{
    to_models.data.clear();
    to_constant.data.clear();
    to_binary.data.clear();
    to_text.data.clear();
    to_random.data.clear();
}

} //  irt
