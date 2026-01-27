// Copyright (c) 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/io.hpp>
#include <type_traits>

namespace irt {

struct file_header {
    enum class mode_type : u32 {
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

    u32 get_source_index(u64 id) noexcept { return right(id); }

    template<typename Stream>
    bool do_serialize_source(Stream& io, source& src) noexcept
    {
        return io(src.id) and io(src.type) and io(src.index);
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
            if (not(do_serialize(io, hsm.states[i].enter_action) and
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
    bool do_serialize_model(Stream& io, model& mdl, parameter& p) noexcept
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

        return io(p.reals.data(), p.reals.size()) and
               io(p.integers.data(), p.integers.size());
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
        return io(src.distribution) and io(src.reals[0]) and
               io(src.reals[1]) and io(src.ints[0]) and io(src.ints[1]);
    }

    template<typename Stream>
    bool do_serialize(simulation& sim, Stream& io) noexcept
    {
        auto constant_external_source = sim.srcs.constant_sources.ssize();
        auto binary_external_source   = sim.srcs.binary_file_sources.ssize();
        auto text_external_source     = sim.srcs.text_file_sources.ssize();
        auto random_external_source   = sim.srcs.random_sources.ssize();
        auto models                   = sim.models.ssize();
        auto hsms                     = sim.hsms.ssize();

        if (not(io(constant_external_source) and io(binary_external_source) and
                io(text_external_source) and io(random_external_source) and
                io(models) and io(hsms)))
            return false;

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

                if (not(io(index) and
                        do_serialize_model(io, *mdl, sim.parameters[index])))
                    return false;
            }
        }

        //{
        //    for (auto& mdl : sim.models) {
        //        dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
        //            if constexpr (has_output_port<Dynamics>) {
        //                i8         i      = 0;
        //                const auto out_id = sim.models.get_id(mdl);

        //                for (const auto& elem : dyn.y) {
        //                    sim.for_each(
        //                      elem,
        //                      [&](const auto& mdl, const auto port_index) {
        //                          auto in = get_index(sim.get_id(mdl));

        //                          io(out_id);
        //                          io(i);
        //                          io(in);
        //                          io(port_index);
        //                          ++i;
        //                      });
        //                }
        //            }
        //        });
        //    }
        //}

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
                do_serialize_model(io, mdl, sim.parameters[index]);
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

            // block_node_id* pout = nullptr;
            // message_id*    pin  = nullptr;

            // if (auto ret = get_output_port(*mdl_src, port_out, pout); !ret)
            //     return
            //     bin.report_error(error_code::unknown_model_port_error);
            // if (auto ret = get_input_port(*mdl_dst, port_in, pin); !ret)
            //     return
            //     bin.report_error(error_code::unknown_model_port_error);
            // if (auto ret = sim.connect(*mdl_src, port_out, *mdl_dst,
            // port_in);
            //     !ret)
            //     return
            //     bin.report_error(error_code::unknown_model_port_error);
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
    debug::ensure(f.get_mode()[file_open_options::write]);
    debug::ensure(not f.get_mode()[file_open_options::text]);

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
    debug::ensure(f.get_mode()[file_open_options::read]);
    debug::ensure(not f.get_mode()[file_open_options::text]);

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
