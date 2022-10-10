// Copyright (c) 2020-2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021
#define ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>

#ifndef R123_USE_CXX11
#define R123_USE_CXX11 1
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

#include <Random123/philox.h>
#include <Random123/uniform.hpp>

#pragma GCC diagnostic pop

namespace irt {

enum class external_source_type : i16
{
    binary_file, /* Best solution to reproductible simulation. Each client take
                    a part of the stream (substream). */
    constant,    /* Just an easy source to use mode. */
    random,      /* How to retrieve old position in debug mode? */
    text_file    /* How to retreive old position in debug mode? */
};

static constexpr int default_max_client_number = 32;
using chunk_type = std::array<double, external_source_chunk_size>;

inline bool external_source_type_cast(int                   value,
                                      external_source_type* type) noexcept
{
    if (value < 0 || value > 3)
        return false;

    *type = enum_cast<external_source_type>(value);
    return true;
}

static inline const char* external_source_type_string[] = { "binary_file",
                                                            "constant",
                                                            "random",
                                                            "text_file" };

inline const char* external_source_str(const external_source_type type) noexcept
{
    return external_source_type_string[ordinal(type)];
}

enum class distribution_type
{
    bernouilli,
    binomial,
    cauchy,
    chi_squared,
    exponential,
    exterme_value,
    fisher_f,
    gamma,
    geometric,
    lognormal,
    negative_binomial,
    normal,
    poisson,
    student_t,
    uniform_int,
    uniform_real,
    weibull,
};

static const char* distribution_type_string[] = {
    "bernouilli",        "binomial", "cauchy",  "chi_squared", "exponential",
    "exterme_value",     "fisher_f", "gamma",   "geometric",   "lognormal",
    "negative_binomial", "normal",   "poisson", "student_t",   "uniform_int",
    "uniform_real",      "weibull"
};

inline const char* distribution_str(const distribution_type type) noexcept
{
    return distribution_type_string[static_cast<int>(type)];
}

template<typename T>
    requires(std::is_integral_v<T>)
inline int to_int(T value) noexcept
{
    if constexpr (std::is_signed_v<T>) {
        irt_assert(INT_MIN <= value && value <= INT_MAX);
        return static_cast<int>(value);
    }

    irt_assert(value <= INT_MAX);
    return static_cast<int>(value);
}

template<typename T>
    requires(std::is_integral_v<T>)
inline i16 to_i16(T value) noexcept
{
    if constexpr (std::is_signed_v<T>) {
        irt_assert(INT16_MIN <= value && value <= INT16_MAX);
        return static_cast<i16>(value);
    }

    irt_assert(value <= INT16_MAX);
    return static_cast<i16>(value);
}

//! Use a buffer with a set of double real to produce external data. This
//! external source can be shared between @c undefined number of  @c source.
struct constant_source
{
    small_string<23> name;
    chunk_type       buffer;
    i32              length = 0;

    constant_source() noexcept = default;

    status init() noexcept { return status::success; }

    status init(source& src) noexcept
    {
        src.buffer = std::span(buffer.data(), length);
        src.index  = 0;

        return status::success;
    }

    status update(source& src) noexcept
    {
        src.index = 0;

        return status::success;
    }

    status restore(source& /*src*/) noexcept { return status::success; }

    status finalize(source& src) noexcept
    {
        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op) noexcept
    {
        switch (op) {
        case source::operation_type::initialize:
            return init(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::restore:
            return restore(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }
};

//! Use a file with a set of double real in binary mode (little endian) to
//! produce external data. This external source can be shared between @c
//! max_clients sources. Each client read a @c external_source_chunk_size real
//! of the set.
//!
//! source::chunk_id[0] is used to store the client identifier.
//! source::chunk_id[1] is used to store the current position in the file.
struct binary_file_source
{
    small_string<23>   name;
    vector<chunk_type> buffers; // buffer, size is defined by max_clients
    vector<u64>        offsets; // offset, size is defined by max_clients
    u32                max_clients = 1; // number of source max (must be >= 1).
    u64                max_reals   = 0; // number of real in the file.

    std::filesystem::path file_path;
    std::ifstream         ifs;
    u32                   next_client = 0;
    u64                   next_offset = 0;

    binary_file_source() noexcept = default;

    status init() noexcept
    {
        next_client = 0;
        next_offset = 0;

        if (max_clients <= 1)
            max_clients = default_max_client_number;

        buffers.resize(max_clients);
        offsets.resize(max_clients);

        if (ifs.is_open())
            ifs.close();

        try {
            std::error_code ec;

            auto size = std::filesystem::file_size(file_path, ec);
            irt_return_if_fail(ec, status::source_empty);

            auto number = size / sizeof(double);
            irt_return_if_fail(number > 0, status::source_empty);

            auto chunks = number / external_source_chunk_size;
            irt_return_if_fail(chunks >= max_clients, status::source_empty);

            max_reals = static_cast<u64>(number);
        } catch (const std::exception& /*e*/) {
            irt_bad_return(status::source_empty);
        }

        ifs.open(file_path);

        if (!ifs)
            return status::source_empty;

        return status::success;
    }

    status init(source& src) noexcept
    {
        src.buffer                       = std::span(buffers[next_client]);
        src.index                        = 0;
        src.chunk_id[0]                  = to_unsigned(next_client);
        src.chunk_id[1]                  = to_unsigned(next_offset);
        offsets[to_int(src.chunk_id[0])] = next_offset;

        next_client += 1;
        next_offset += external_source_chunk_size;

        irt_return_if_fail(next_offset < max_reals, status::source_empty);

        return fill_buffer(src);
    }

    status update(source& src) noexcept
    {
        const auto distance = external_source_chunk_size * max_clients;
        const auto next     = src.chunk_id[1] + distance;

        irt_return_if_fail(next + external_source_chunk_size < max_reals,
                           status::source_empty);

        src.index = 0;

        return fill_buffer(src);
    }

    status restore(source& src) noexcept
    {
        if (offsets[to_int(src.chunk_id[0])] != src.chunk_id[1])
            return fill_buffer(src);

        return status::success;
    }

    status finalize(source& src) noexcept
    {
        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op) noexcept
    {
        switch (op) {
        case source::operation_type::initialize:
            return init(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::restore:
            return restore(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }

private:
    status fill_buffer(source& src) noexcept
    {
        if (!ifs.seekg(src.chunk_id[1] * sizeof(double)))
            return status::source_empty;

        auto* s = reinterpret_cast<char*>(src.buffer.data());
        if (!ifs.read(s, external_source_chunk_size))
            return status::source_empty;

        const auto current_position      = ifs.tellg() / sizeof(double);
        src.chunk_id[1]                  = current_position;
        offsets[to_int(src.chunk_id[0])] = current_position;

        return status::success;
    }
};

//! Use a file with a set of double real in ascii text file to produce
//! external data. This external source can not be shared between @c source.
//!
//! source::chunk_id[0] is used to store the current position in the file to
//! simplify restore operation.
struct text_file_source
{
    small_string<23> name;
    chunk_type       buffer;
    u64              offset;

    std::filesystem::path file_path;
    std::ifstream         ifs;

    text_file_source() noexcept = default;

    status init() noexcept
    {
        if (ifs.is_open())
            ifs.close();

        ifs.open(file_path);
        if (!ifs)
            return status::source_empty;

        offset = 0;

        return status::success;
    }

    status init(source& src) noexcept
    {
        src.buffer      = std::span(buffer);
        src.index       = 0;
        src.chunk_id[0] = ifs.tellg();
        offset          = ifs.tellg();

        return fill_buffer(src);
    }

    status update(source& src) noexcept
    {
        src.index       = 0;
        src.chunk_id[0] = ifs.tellg();
        offset          = ifs.tellg();

        return fill_buffer(src);
    }

    status restore(source& src) noexcept
    {
        src.buffer = std::span(buffer);

        if (offset != src.chunk_id[0]) {
            if (!ifs.seekg(src.chunk_id[0]))
                return status::source_empty;

            offset = ifs.tellg();
        }

        return fill_buffer(src);
    }

    status finalize(source& src) noexcept
    {
        src.clear();
        offset = 0;

        return status::success;
    }

    status operator()(source& src, source::operation_type op) noexcept
    {
        switch (op) {
        case source::operation_type::initialize:
            return init(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::restore:
            return restore(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }

private:
    status fill_buffer(source& /*src*/) noexcept
    {
        for (auto i = 0; i < external_source_chunk_size; ++i)
            if (!(ifs >> buffer[i]))
                return status::source_empty;

        return status::success;
    }
};

//! Use a prng to produce set of double real. This external source can be
//! shared between @c max_clients sources. Each client read a @c
//  external_source_chunk_size real of the set.
//!
//! The source::chunk_id[0-5] array is used to store the prng state.
struct random_source
{
    using rng          = r123::Philox4x64;
    using counter_type = r123::Philox4x64::ctr_type;
    using key_type     = r123::Philox4x64::key_type;
    using result_type  = r123::Philox4x64::ctr_type;

    small_string<23>           name;
    vector<chunk_type>         buffers;
    vector<std::array<u64, 4>> counters;
    u32          max_clients   = 1; // number of source max (must be >= 1).
    u32          start_counter = 0; // provided by @c external_source class.
    u32          next_client   = 0;
    counter_type ctr;
    key_type     key; // provided by @c external_source class.

    distribution_type distribution = distribution_type::uniform_int;
    double a = 0, b = 1, p, mean, lambda, alpha, beta, stddev, m, s, n;
    int    a32, b32, t32, k32;

    class local_rng
    {
    public:
        using result_type = counter_type::value_type;

        static_assert(counter_type::static_size == 4);
        static_assert(key_type::static_size == 2);

        static_assert(std::numeric_limits<result_type>::digits >= 64,
                      "The result_type must have at least 32 bits");

        result_type operator()() noexcept
        {
            if (last_elem == 0) {
                c.incr();

                rng b;
                rdata = b(c, k);

                n++;
                last_elem = rdata.size();
            }

            return rdata[--last_elem];
        }

        local_rng(const counter_type& c0, const key_type& uk) noexcept
          : c(c0)
          , k(uk)
          , n(0)
          , last_elem(0)
        {
        }

        constexpr static result_type min R123_NO_MACRO_SUBST() noexcept
        {
            return std::numeric_limits<result_type>::min();
        }

        constexpr static result_type max R123_NO_MACRO_SUBST() noexcept
        {
            return std::numeric_limits<result_type>::max();
        }

        const counter_type& counter() const { return c; }

    private:
        counter_type c;
        key_type     k;
        counter_type rdata;
        u64          n;
        sz           last_elem;
    };

    void start_source(source& src) noexcept
    {
        ctr = { { src.chunk_id[0],
                  src.chunk_id[1],
                  src.chunk_id[2],
                  src.chunk_id[3] } };
    }

    void end_source(source& src) noexcept
    {
        src.chunk_id[0] = ctr[0];
        src.chunk_id[1] = ctr[1];
        src.chunk_id[2] = ctr[2];
        src.chunk_id[3] = ctr[3];

        const auto client = to_int(src.chunk_id[3] - start_counter);

        counters[client][0] = ctr[0];
        counters[client][1] = ctr[1];
        counters[client][2] = ctr[2];
        counters[client][3] = ctr[3];
    }

    random_source() noexcept = default;

    template<typename Distribution>
    void generate(Distribution dist, source& src) noexcept
    {
        start_source(src);
        local_rng gen(ctr, key);

        const auto client = to_int(src.chunk_id[3] - start_counter);

        for (sz i = 0, e = buffers[client].size(); i < e; ++i)
            src.buffer[i] = dist(gen);

        ctr = gen.counter();
        end_source(src);
    }

    status init() noexcept
    {
        next_client = 0;

        if (max_clients <= 1)
            max_clients = default_max_client_number;

        buffers.resize(max_clients);
        counters.resize(max_clients);

        return status::success;
    }

    status init(source& src) noexcept
    {
        src.buffer = std::span(buffers[next_client]);
        src.index  = 0;

        src.chunk_id[0] = 0;
        src.chunk_id[1] = 0;
        src.chunk_id[2] = 0;
        src.chunk_id[3] = start_counter + next_client;

        counters[next_client][0] = src.chunk_id[0];
        counters[next_client][1] = src.chunk_id[1];
        counters[next_client][2] = src.chunk_id[2];
        counters[next_client][3] = src.chunk_id[3];

        next_client += 1;

        return fill_buffer(src);
    }

    status update(source& src) noexcept
    {
        src.index = 0;

        return fill_buffer(src);
    }

    status restore(source& src) noexcept
    {
        irt_assert(src.chunk_id[3] >= start_counter);

        const auto client = to_int(src.chunk_id[3] - start_counter);

        if (!(counters[client][0] == src.chunk_id[0] &&
              counters[client][1] == src.chunk_id[1] &&
              counters[client][2] == src.chunk_id[2] &&
              counters[client][3] == src.chunk_id[3]))
            return fill_buffer(src);

        return status::success;
    }

    status finalize(source& src) noexcept
    {
        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op)
    {
        switch (op) {
        case source::operation_type::initialize:
            return init(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::restore:
            return restore(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }

    status fill_buffer(source& src) noexcept
    {
        switch (distribution) {
        case distribution_type::uniform_int:
            generate(std::uniform_int_distribution(a32, b32), src);
            break;

        case distribution_type::uniform_real:
            generate(std::uniform_real_distribution(a, b), src);
            break;

        case distribution_type::bernouilli:
            generate(std::bernoulli_distribution(p), src);
            break;

        case distribution_type::binomial:
            generate(std::binomial_distribution(t32, p), src);
            break;

        case distribution_type::negative_binomial:
            generate(std::negative_binomial_distribution(t32, p), src);
            break;

        case distribution_type::geometric:
            generate(std::geometric_distribution(p), src);
            break;

        case distribution_type::poisson:
            generate(std::poisson_distribution(mean), src);
            break;

        case distribution_type::exponential:
            generate(std::exponential_distribution(lambda), src);
            break;

        case distribution_type::gamma:
            generate(std::gamma_distribution(alpha, beta), src);
            break;

        case distribution_type::weibull:
            generate(std::weibull_distribution(a, b), src);
            break;

        case distribution_type::exterme_value:
            generate(std::extreme_value_distribution(a, b), src);
            break;

        case distribution_type::normal:
            generate(std::normal_distribution(mean, stddev), src);
            break;

        case distribution_type::lognormal:
            generate(std::lognormal_distribution(m, s), src);
            break;

        case distribution_type::chi_squared:
            generate(std::chi_squared_distribution(n), src);
            break;

        case distribution_type::cauchy:
            generate(std::cauchy_distribution(a, b), src);
            break;

        case distribution_type::fisher_f:
            generate(std::fisher_f_distribution(m, n), src);
            break;

        case distribution_type::student_t:
            generate(std::student_t_distribution(n), src);
            break;
        }

        return status::success;
    }
};

enum class constant_source_id : u64;
enum class binary_file_source_id : u64;
enum class text_file_source_id : u64;
enum class random_source_id : u64;

struct external_source
{
    data_array<constant_source, constant_source_id>       constant_sources;
    data_array<binary_file_source, binary_file_source_id> binary_file_sources;
    data_array<text_file_source, text_file_source_id>     text_file_sources;
    data_array<random_source, random_source_id>           random_sources;

    u64 seed[2] = { 0xdeadbeef12345678U, 0xdeadbeef12345678U };

    status init(const sz size) noexcept
    {
        irt_return_if_bad(constant_sources.init(size));
        irt_return_if_bad(binary_file_sources.init(size));
        irt_return_if_bad(text_file_sources.init(size));
        irt_return_if_bad(random_sources.init(size));

        return status::success;
    }

    status prepare() noexcept
    {
        {
            constant_source* src = nullptr;
            while (constant_sources.next(src))
                irt_return_if_bad(src->init());
        }

        {
            binary_file_source* src = nullptr;
            while (binary_file_sources.next(src))
                irt_return_if_bad(src->init());
        }

        {
            text_file_source* src = nullptr;
            while (text_file_sources.next(src))
                irt_return_if_bad(src->init());
        }

        {
            u32            start_counter = 0;
            random_source* src           = nullptr;
            while (random_sources.next(src)) {
                src->key           = { { seed[0], seed[1] } };
                src->start_counter = start_counter;
                start_counter += src->max_clients;

                irt_return_if_bad(src->init());
            }
        }

        return status::success;
    }

    status operator()(source& src, const source::operation_type op) noexcept
    {
        external_source_type type;
        if (!external_source_type_cast(src.type, &type))
            return status::source_unknown;

        switch (type) {
        case external_source_type::binary_file: {
            const auto src_id = enum_cast<binary_file_source_id>(src.id);
            if (auto* bin_src = binary_file_sources.try_to_get(src_id);
                bin_src) {
                return (*bin_src)(src, op);
            }
        } break;
        case external_source_type::constant: {
            const auto src_id = enum_cast<constant_source_id>(src.id);
            if (auto* cst_src = constant_sources.try_to_get(src_id); cst_src) {
                return (*cst_src)(src, op);
            }
        } break;

        case external_source_type::random: {
            const auto src_id = enum_cast<random_source_id>(src.id);
            if (auto* rnd_src = random_sources.try_to_get(src_id); rnd_src) {
                return (*rnd_src)(src, op);
            }
        } break;

        case external_source_type::text_file: {
            const auto src_id = enum_cast<text_file_source_id>(src.id);
            if (auto* txt_src = text_file_sources.try_to_get(src_id); txt_src) {
                return (*txt_src)(src, op);
            }
        } break;
        }

        irt_unreachable();
    }
};

enum class random_file_type
{
    binary,
    text,
};

template<typename RandomGenerator, typename Distribution>
inline int generate_random_file(std::ostream&          os,
                                RandomGenerator&       gen,
                                Distribution&          dist,
                                const std::size_t      size,
                                const random_file_type type) noexcept
{
    switch (type) {
    case random_file_type::text: {
        if (!os)
            return -1;

        for (std::size_t sz = 0; sz < size; ++sz)
            if (!(os << dist(gen) << '\n'))
                return -2;
    } break;

    case random_file_type::binary: {
        if (!os)
            return -1;

        for (std::size_t sz = 0; sz < size; ++sz) {
            const double value = dist(gen);
            os.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    } break;
    }

    return 0;
}

} // namespace irt

#endif
