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

namespace irt {

enum class external_source_type
{
    none = -1,
    binary_file, /* Best solution to reproductible simulation. Each client take
                    a part of the stream. */
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

struct constant_source
{
    small_string<23> name;
    chunk_type       buffer;
    i32              length = 0;

    constant_source() noexcept = default;

    status init(source& src) noexcept
    {
        src.buffer   = std::span(buffer.data(), length);
        src.index    = 0;
        src.chuck_id = 0;

        return status::success;
    }

    status update(source& src) noexcept
    {
        src.index    = 0;
        src.chuck_id = 0;

        return status::success;
    }

    status restore(source& src) noexcept
    {
        src.index    = 0;
        src.chuck_id = 0;

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
};

struct binary_file_source
{
    small_string<23>   name;
    i32                max_clients; // >= 1.
    vector<chunk_type> buffer;      // size is defined by max_clients

    std::filesystem::path file_path;
    std::ifstream         ifs;

    binary_file_source(
      int max_client_number = default_max_client_number) noexcept
      : max_clients{ max_client_number }
    {
        if (max_client_number > INT32_MAX || max_client_number <= 1)
            max_clients = default_max_client_number;

        buffer.resize(max_clients);
    }

    status init(source& src) noexcept
    {
        auto src_index = src.chuck_id % max_clients;

        src.buffer = std::span(buffer[src_index]);
        src.index  = 0;

        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return status::source_empty;
        } else {
            ifs.seekg(0);
        }

        return fill_buffer(src);
    }

    status update(source& src) noexcept
    {
        if (!ifs)
            return status::source_empty;

        src.index = 0;
        src.chuck_id += max_clients;

        return fill_buffer(src);
    }

    status restore(source& src) noexcept
    {
        if (!ifs)
            return status::source_empty;

        return fill_buffer(src);
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
        if (!ifs)
            return status::source_empty;

        const i64 chuck_id            = src.chuck_id;
        const i64 chunk_size          = external_source_chunk_size;
        const i64 chunk_size_in_bytes = chunk_size * sizeof(double);
        const i64 start_read_in_bytes = chuck_id * chunk_size_in_bytes;

        if (!ifs.seekg(start_read_in_bytes, std::ios_base::beg))
            return status::source_empty;

        auto* s = reinterpret_cast<char*>(src.buffer.data());
        if (!ifs.read(s, chunk_size_in_bytes))
            return status::source_empty;

        return status::success;
    }
};

struct text_file_source
{
    small_string<23>   name;
    i32                max_clients; // >= 1.
    vector<chunk_type> buffer;      // size == max_clients
    vector<i64>        position;    // size == max_clients, value =

    std::filesystem::path file_path;
    std::ifstream         ifs;

    text_file_source(int max_client_number = default_max_client_number) noexcept
      : max_clients{ max_client_number }
    {
        if (max_client_number > INT32_MAX || max_client_number <= 1)
            max_clients = default_max_client_number;

        buffer.resize(max_clients);
    }

    status init(source& src) noexcept
    {
        auto  src_index = src.chuck_id % max_clients;
        auto* data      = buffer.data() + src_index;

        src.buffer = std::span(buffer[src_index]);
        src.index  = 0;

        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return status::source_empty;
        } else {
            ifs.seekg(0);
        }

        return fill_buffer(src);
    }

    status update(source& src) noexcept
    {
        if (!ifs)
            return status::source_empty;

        auto  src_index = src.chuck_id % max_clients;
        auto* data      = buffer.data() + src_index;

        src.index = 0;
        src.chuck_id += max_clients;

        return fill_buffer(src);
    }

    status restore(source& src) noexcept
    {
        if (!ifs)
            return status::source_empty;

        auto  src_index = src.chuck_id % max_clients;
        auto* data      = buffer.data() + src_index;

        return fill_buffer(src);
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
        if (!ifs)
            return status::source_empty;

        auto src_index      = src.chuck_id % max_clients;
        position[src_index] = ifs.tellg();

        for (auto i = 0; i < external_source_chunk_size; ++i)
            if (!(ifs >> src.buffer.data()[i]))
                return status::source_empty;

        return status::success;
    }
};

struct random_source
{
    small_string<23>   name;
    i32                max_clients; // >= 1.
    vector<chunk_type> buffer;      // size is defined by max_clients
    u64                seed = 1289U;
    std::mt19937_64*   gen  = nullptr;

    distribution_type distribution = distribution_type::uniform_int;
    double a = 0, b = 1, p, mean, lambda, alpha, beta, stddev, m, s, n;
    int    a32, b32, t32, k32;

    random_source(int max_client_number = default_max_client_number) noexcept
      : max_clients{ max_client_number }
    {
        if (max_client_number > INT32_MAX || max_client_number <= 1)
            max_clients = default_max_client_number;

        buffer.resize(max_clients);
    }

    template<typename Distribution>
    void generate(Distribution dist, std::span<double> buffer) noexcept
    {
        for (sz i = 0, e = buffer.size(); i < e; ++i)
            buffer[i] = dist(*gen);
    }

    void generate(std::span<double> buffer) noexcept
    {
        switch (distribution) {
        case distribution_type::uniform_int:
            generate(std::uniform_int_distribution(a32, b32), buffer);
            break;

        case distribution_type::uniform_real:
            generate(std::uniform_real_distribution(a, b), buffer);
            break;

        case distribution_type::bernouilli:
            generate(std::bernoulli_distribution(p), buffer);
            break;

        case distribution_type::binomial:
            generate(std::binomial_distribution(t32, p), buffer);
            break;

        case distribution_type::negative_binomial:
            generate(std::negative_binomial_distribution(t32, p), buffer);
            break;

        case distribution_type::geometric:
            generate(std::geometric_distribution(p), buffer);
            break;

        case distribution_type::poisson:
            generate(std::poisson_distribution(mean), buffer);
            break;

        case distribution_type::exponential:
            generate(std::exponential_distribution(lambda), buffer);
            break;

        case distribution_type::gamma:
            generate(std::gamma_distribution(alpha, beta), buffer);
            break;

        case distribution_type::weibull:
            generate(std::weibull_distribution(a, b), buffer);
            break;

        case distribution_type::exterme_value:
            generate(std::extreme_value_distribution(a, b), buffer);
            break;

        case distribution_type::normal:
            generate(std::normal_distribution(mean, stddev), buffer);
            break;

        case distribution_type::lognormal:
            generate(std::lognormal_distribution(m, s), buffer);
            break;

        case distribution_type::chi_squared:
            generate(std::chi_squared_distribution(n), buffer);
            break;

        case distribution_type::cauchy:
            generate(std::cauchy_distribution(a, b), buffer);
            break;

        case distribution_type::fisher_f:
            generate(std::fisher_f_distribution(m, n), buffer);
            break;

        case distribution_type::student_t:
            generate(std::student_t_distribution(n), buffer);
            break;
        }
    }

    status init(source& src) noexcept
    {
        auto src_index = src.chuck_id % max_clients;

        src.buffer = std::span(buffer[src_index]);
        src.index  = 0;

        return fill_buffer(src);
    }

    status update(source& src) noexcept { return fill_buffer(src); }

    status restore(source& src) noexcept { return fill_buffer(src); }

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
        if (!gen)
            return status::source_empty;

        generate(src.buffer);

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
    std::mt19937_64                                       generator;

    // Chunk of memory used by a model.
    sz block_size = 512;

    // Number of models can be attached to an external source.
    sz block_number = 1024;

    status init(const sz size) noexcept
    {
        irt_return_if_bad(constant_sources.init(size));
        irt_return_if_bad(binary_file_sources.init(size));
        irt_return_if_bad(text_file_sources.init(size));
        irt_return_if_bad(random_sources.init(size));

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
