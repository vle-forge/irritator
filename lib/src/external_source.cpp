// Copyright (c) 2020-2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>

#ifndef R123_USE_CXX11
#define R123_USE_CXX11 1
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <Random123/philox.h>
#include <Random123/uniform.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace irt {

status constant_source::init() noexcept { return status::success; }

status constant_source::init(source& src) noexcept
{
    src.buffer = std::span(buffer.data(), length);
    src.index  = 0;

    return status::success;
}

status constant_source::update(source& src) noexcept
{
    src.index = 0;

    return status::success;
}

status constant_source::restore(source& /*src*/) noexcept
{
    return status::success;
}

status constant_source::finalize(source& src) noexcept
{
    src.clear();

    return status::success;
}

status binary_file_source::init() noexcept
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

static status binary_file_source_fill_buffer(binary_file_source& ext,
                                             source&             src) noexcept
{
    const auto to_seek = src.chunk_id[1] * sizeof(double);

    if (!ext.ifs.seekg(static_cast<long>(to_seek)))
        return status::source_empty;

    auto* s = reinterpret_cast<char*>(src.buffer.data());
    if (!ext.ifs.read(s, external_source_chunk_size))
        return status::source_empty;

    const auto tellg = ext.ifs.tellg();
    if (tellg < 0)
        return status::source_empty;

    const auto current_position = static_cast<u64>(tellg) / sizeof(double);
    src.chunk_id[1]             = current_position;
    ext.offsets[numeric_cast<int>(src.chunk_id[0])] = current_position;

    return status::success;
}

status binary_file_source::init(source& src) noexcept
{
    src.buffer      = std::span(buffers[next_client]);
    src.index       = 0;
    src.chunk_id[0] = to_unsigned(next_client);
    src.chunk_id[1] = to_unsigned(next_offset);
    offsets[numeric_cast<int>(src.chunk_id[0])] = next_offset;

    next_client += 1;
    next_offset += external_source_chunk_size;

    irt_return_if_fail(next_offset < max_reals, status::source_empty);

    return binary_file_source_fill_buffer(*this, src);
}

status binary_file_source::update(source& src) noexcept
{
    const auto distance = external_source_chunk_size * max_clients;
    const auto next     = src.chunk_id[1] + distance;

    irt_return_if_fail(next + external_source_chunk_size < max_reals,
                       status::source_empty);

    src.index = 0;

    return binary_file_source_fill_buffer(*this, src);
}

status binary_file_source::restore(source& src) noexcept
{
    if (offsets[numeric_cast<int>(src.chunk_id[0])] != src.chunk_id[1])
        return binary_file_source_fill_buffer(*this, src);

    return status::success;
}

status binary_file_source::finalize(source& src) noexcept
{
    src.clear();

    return status::success;
}

status text_file_source::init() noexcept
{
    if (ifs.is_open())
        ifs.close();

    ifs.open(file_path);
    if (!ifs)
        return status::source_empty;

    offset = 0;

    return status::success;
}

static status text_file_source_fill_buffer(text_file_source& ext,
                                           source& /*src*/) noexcept
{
    for (int i = 0; i < external_source_chunk_size; ++i)
        if (!(ext.ifs >> ext.buffer[static_cast<sz>(i)]))
            return status::source_empty;

    return status::success;
}

status text_file_source::init(source& src) noexcept
{
    src.buffer = std::span(buffer);
    src.index  = 0;

    const auto tellg = ifs.tellg();
    irt_return_if_fail(tellg != -1, status::source_empty);

    src.chunk_id[0] = static_cast<u64>(tellg);
    offset          = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::update(source& src) noexcept
{
    src.index = 0;

    const auto tellg = ifs.tellg();
    irt_return_if_fail(tellg != -1, status::source_empty);

    src.chunk_id[0] = static_cast<u64>(tellg);
    offset          = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::restore(source& src) noexcept
{
    src.buffer = std::span(buffer);

    if (offset != src.chunk_id[0]) {
        irt_return_if_fail(
          is_numeric_castable<std::ifstream::off_type>(src.chunk_id[0]),
          status::source_empty);

        if (!ifs.seekg(numeric_cast<std::ifstream::off_type>(src.chunk_id[0])))
            return status::source_empty;

        const auto tellg = ifs.tellg();
        irt_return_if_fail(tellg < 0, status::source_empty);

        offset = static_cast<u64>(tellg);
    }

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::finalize(source& src) noexcept
{
    src.clear();
    offset = 0;

    return status::success;
}

struct local_rng
{
    using rng          = r123::Philox4x64;
    using counter_type = r123::Philox4x64::ctr_type;
    using key_type     = r123::Philox4x64::key_type;
    using result_type  = counter_type::value_type;

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

    local_rng(const random_source::counter_type& c0,
              const random_source::key_type&     uk) noexcept
      : n(0)
      , last_elem(0)
    {
        std::copy_n(c0.data(), c0.size(), c.data());
        std::copy_n(uk.data(), uk.size(), k.data());
    }

    constexpr static result_type min R123_NO_MACRO_SUBST() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    constexpr static result_type max R123_NO_MACRO_SUBST() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

    counter_type c;
    key_type     k;
    counter_type rdata;
    u64          n;
    sz           last_elem;
};

void random_source_start_source(random_source& ext, source& src) noexcept
{
    ext.ctr = {
        { src.chunk_id[0], src.chunk_id[1], src.chunk_id[2], src.chunk_id[3] }
    };
}

void random_source_end_source(random_source& ext, source& src) noexcept
{
    src.chunk_id[0] = ext.ctr[0];
    src.chunk_id[1] = ext.ctr[1];
    src.chunk_id[2] = ext.ctr[2];
    src.chunk_id[3] = ext.ctr[3];

    const auto client = numeric_cast<int>(src.chunk_id[3] - ext.start_counter);

    ext.counters[client][0] = ext.ctr[0];
    ext.counters[client][1] = ext.ctr[1];
    ext.counters[client][2] = ext.ctr[2];
    ext.counters[client][3] = ext.ctr[3];
}

template<typename Distribution>
void random_source_generate(random_source& ext,
                            Distribution   dist,
                            source&        src) noexcept
{
    random_source_start_source(ext, src);
    local_rng gen(ext.ctr, ext.key);

    const auto client = numeric_cast<int>(src.chunk_id[3] - ext.start_counter);

    for (sz i = 0, e = ext.buffers[client].size(); i < e; ++i)
        src.buffer[i] = dist(gen);

    std::copy_n(gen.c.data(), gen.c.size(), ext.ctr.data());
    random_source_end_source(ext, src);
}

status random_source::init() noexcept
{
    next_client = 0;

    if (max_clients <= 1)
        max_clients = default_max_client_number;

    buffers.resize(max_clients);
    counters.resize(max_clients);

    return status::success;
}

static status random_source_fill_buffer(random_source& ext,
                                        source&        src) noexcept
{
    switch (ext.distribution) {
    case distribution_type::uniform_int:
        random_source_generate(
          ext, std::uniform_int_distribution(ext.a32, ext.b32), src);
        break;

    case distribution_type::uniform_real:
        random_source_generate(
          ext, std::uniform_real_distribution(ext.a, ext.b), src);
        break;

    case distribution_type::bernouilli:
        random_source_generate(ext, std::bernoulli_distribution(ext.p), src);
        break;

    case distribution_type::binomial:
        random_source_generate(
          ext, std::binomial_distribution(ext.t32, ext.p), src);
        break;

    case distribution_type::negative_binomial:
        random_source_generate(
          ext, std::negative_binomial_distribution(ext.t32, ext.p), src);
        break;

    case distribution_type::geometric:
        random_source_generate(ext, std::geometric_distribution(ext.p), src);
        break;

    case distribution_type::poisson:
        random_source_generate(ext, std::poisson_distribution(ext.mean), src);
        break;

    case distribution_type::exponential:
        random_source_generate(
          ext, std::exponential_distribution(ext.lambda), src);
        break;

    case distribution_type::gamma:
        random_source_generate(
          ext, std::gamma_distribution(ext.alpha, ext.beta), src);
        break;

    case distribution_type::weibull:
        random_source_generate(
          ext, std::weibull_distribution(ext.a, ext.b), src);
        break;

    case distribution_type::exterme_value:
        random_source_generate(
          ext, std::extreme_value_distribution(ext.a, ext.b), src);
        break;

    case distribution_type::normal:
        random_source_generate(
          ext, std::normal_distribution(ext.mean, ext.stddev), src);
        break;

    case distribution_type::lognormal:
        random_source_generate(
          ext, std::lognormal_distribution(ext.m, ext.s), src);
        break;

    case distribution_type::chi_squared:
        random_source_generate(ext, std::chi_squared_distribution(ext.n), src);
        break;

    case distribution_type::cauchy:
        random_source_generate(
          ext, std::cauchy_distribution(ext.a, ext.b), src);
        break;

    case distribution_type::fisher_f:
        random_source_generate(
          ext, std::fisher_f_distribution(ext.m, ext.n), src);
        break;

    case distribution_type::student_t:
        random_source_generate(ext, std::student_t_distribution(ext.n), src);
        break;
    }

    return status::success;
}

status random_source::init(source& src) noexcept
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

    return random_source_fill_buffer(*this, src);
}

status random_source::update(source& src) noexcept
{
    src.index = 0;

    return random_source_fill_buffer(*this, src);
}

status random_source::restore(source& src) noexcept
{
    irt_assert(src.chunk_id[3] >= start_counter);

    const auto client = numeric_cast<int>(src.chunk_id[3] - start_counter);

    if (!(counters[client][0] == src.chunk_id[0] &&
          counters[client][1] == src.chunk_id[1] &&
          counters[client][2] == src.chunk_id[2] &&
          counters[client][3] == src.chunk_id[3]))
        return random_source_fill_buffer(*this, src);

    return status::success;
}

status random_source::finalize(source& src) noexcept
{
    src.clear();

    return status::success;
}

// status external_source::init(std::integral auto size) noexcept
// {
//     irt_return_if_fail(is_numeric_castable<u32>(size),
//                       status::data_array_init_capacity_error);
//
//     irt_return_if_bad(constant_sources.init(size));
//     irt_return_if_bad(binary_file_sources.init(size));
//     irt_return_if_bad(text_file_sources.init(size));
//     irt_return_if_bad(random_sources.init(size));
//
//     return status::success;
// }

status external_source::prepare() noexcept
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

template<typename Source>
static status external_source_dispatch(Source&                s,
                                       source&                src,
                                       source::operation_type op) noexcept
{
    switch (op) {
    case source::operation_type::initialize:
        return s.init(src);

    case source::operation_type::update:
        return s.update(src);

    case source::operation_type::restore:
        return s.restore(src);

    case source::operation_type::finalize:
        return s.finalize(src);
    }

    irt_unreachable();
}

status external_source::dispatch(source&                      src,
                                 const source::operation_type op) noexcept
{
    switch (src.type) {
    case source::source_type::none:
        return status::success;

    case source::source_type::binary_file: {
        const auto src_id = enum_cast<binary_file_source_id>(src.id);
        if (auto* bin_src = binary_file_sources.try_to_get(src_id); bin_src) {
            return external_source_dispatch(*bin_src, src, op);
        } else {
            irt_bad_return(status::source_empty);
        }
    } break;

    case source::source_type::constant: {
        const auto src_id = enum_cast<constant_source_id>(src.id);
        if (auto* cst_src = constant_sources.try_to_get(src_id); cst_src) {
            return external_source_dispatch(*cst_src, src, op);
        } else {
            irt_bad_return(status::source_empty);
        }
    } break;

    case source::source_type::random: {
        const auto src_id = enum_cast<random_source_id>(src.id);
        if (auto* rnd_src = random_sources.try_to_get(src_id); rnd_src) {
            return external_source_dispatch(*rnd_src, src, op);
        } else {
            irt_bad_return(status::source_empty);
        }
    } break;

    case source::source_type::text_file: {
        const auto src_id = enum_cast<text_file_source_id>(src.id);
        if (auto* txt_src = text_file_sources.try_to_get(src_id); txt_src) {
            return external_source_dispatch(*txt_src, src, op);
        } else {
            irt_bad_return(status::source_empty);
        }
    } break;
    }

    irt_unreachable();
}

} // namespace irt
