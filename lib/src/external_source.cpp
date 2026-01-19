// Copyright (c) 2020-2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/ext.hpp>
#include <irritator/random.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <utility>

namespace irt {

external_source_definition::constant_source&
external_source_definition::alloc_constant_source(
  std::string_view name) noexcept
{
    debug::ensure(data.can_alloc(1));

    const auto id          = data.alloc_id();
    data.get<name_str>(id) = name;
    auto& cst = data.get<external_source_definition::source_element>(id);

    return cst.emplace<external_source_definition::constant_source>();
}

external_source_definition::binary_source&
external_source_definition::alloc_binary_source(std::string_view name) noexcept
{
    debug::ensure(data.can_alloc(1));

    const auto id          = data.alloc_id();
    data.get<name_str>(id) = name;
    return data.get<external_source_definition::source_element>(id)
      .emplace<external_source_definition::binary_source>();
}

external_source_definition::text_source&
external_source_definition::alloc_text_source(std::string_view name) noexcept
{
    debug::ensure(data.can_alloc(1));

    const auto id          = data.alloc_id();
    data.get<name_str>(id) = name;
    return data.get<external_source_definition::source_element>(id)
      .emplace<external_source_definition::text_source>();
}

external_source_definition::random_source&
external_source_definition::alloc_random_source(std::string_view name) noexcept
{
    debug::ensure(data.can_alloc(1));

    const auto id          = data.alloc_id();
    data.get<name_str>(id) = name;
    return data.get<external_source_definition::source_element>(id)
      .emplace<external_source_definition::random_source>();
}

constant_source::constant_source(const constant_source& other) noexcept
  : name(other.name)
  , length(other.length)
{
    std::copy_n(
      std::data(other.buffer), std::size(other.buffer), std::data(buffer));
}

constant_source::constant_source(std::span<const real> src) noexcept
{
    const auto len =
      std::min(src.size(), static_cast<size_t>(external_source_chunk_size));

    std::copy_n(src.data(), len, std::data(buffer));
    length = static_cast<u32>(len);
}

constant_source& constant_source::operator=(
  const constant_source& other) noexcept
{
    if (this != &other) {
        name   = other.name;
        buffer = other.buffer;
        length = other.length;
    }

    return *this;
}

status constant_source::init() noexcept { return success(); }

status constant_source::init(source& src, source_data&) noexcept
{
    src.buffer = std::span(buffer.data(), length);
    src.index  = 0;

    return success();
}

status constant_source::update(source& src, source_data&) noexcept
{
    src.index = 0;

    return success();
}

status constant_source::restore(source& /*src*/, source_data&) noexcept
{
    return success();
}

status constant_source::finalize(source& src, source_data&) noexcept
{
    src.clear();

    return success();
}

binary_file_source::binary_file_source(const std::filesystem::path& p) noexcept
  : file_path(p)
{}

binary_file_source::binary_file_source(const binary_file_source& other) noexcept
  : name(other.name)
  , buffers(other.buffers)
  , offsets(other.offsets)
  , max_clients(other.max_clients)
  , max_reals(other.max_reals)
  , file_path(other.file_path)
  , next_client(other.next_client)
  , next_offset(other.next_offset)
{}

binary_file_source& binary_file_source::operator=(
  const binary_file_source& other) noexcept
{
    if (this != &other) {
        binary_file_source tmp{ other };
        swap(tmp);
    }

    return *this;
}

void binary_file_source::swap(binary_file_source& other) noexcept
{
    std::swap(name, other.name);
    std::swap(buffers, other.buffers);
    std::swap(offsets, other.offsets);
    std::swap(max_clients, other.max_clients);
    std::swap(max_reals, other.max_reals);
    std::swap(file_path, other.file_path);
    std::swap(next_client, other.next_client);
    std::swap(next_offset, other.next_offset);
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
        if (!ec)
            return new_error(external_source_errc::binary_file_access_error);

        auto number = size / sizeof(double);
        if (number <= 0)
            return new_error(external_source_errc::binary_file_size_error);

        auto chunks = number / external_source_chunk_size;
        if (chunks < max_clients)
            return new_error(external_source_errc::binary_file_size_error);

        max_reals = static_cast<u64>(number);
    } catch (const std::exception& /*e*/) {
        return new_error(external_source_errc::binary_file_access_error);
    }

    ifs.open(file_path);

    if (!ifs)
        return new_error(external_source_errc::binary_file_access_error);

    return success();
}

void binary_file_source::finalize() noexcept
{
    if (ifs)
        ifs.close();
}

bool binary_file_source::seekg(const long to_seek) noexcept
{
    return ifs.seekg(to_seek).good();
}

bool binary_file_source::read(source& src, const int length) noexcept
{
    return ifs.read(reinterpret_cast<char*>(src.buffer.data()), length).good();
}

int binary_file_source::tellg() noexcept { return static_cast<int>(ifs.tellg()); }

static status binary_file_source_fill_buffer(binary_file_source& ext,
                                             source&             src,
                                             source_data&        data) noexcept
{
    const auto to_seek = data.chunk_id[1] * sizeof(double);

    if (!ext.seekg(static_cast<long>(to_seek)))
        return new_error(external_source_errc::binary_file_eof_error);

    if (!ext.read(src, external_source_chunk_size))
        return new_error(external_source_errc::binary_file_eof_error);

    const auto tellg = ext.tellg();
    if (tellg < 0)
        return new_error(external_source_errc::binary_file_eof_error);

    const auto current_position = static_cast<u64>(tellg) / sizeof(double);
    data.chunk_id[1]            = current_position;
    ext.offsets[numeric_cast<int>(data.chunk_id[0])] = current_position;

    return success();
}

status binary_file_source::init(source& src, source_data& data) noexcept
{
    src.buffer       = std::span(buffers[next_client]);
    src.index        = 0;
    data.chunk_id[0] = next_client;
    data.chunk_id[1] = next_offset;
    offsets[numeric_cast<int>(data.chunk_id[0])] = next_offset;

    next_client += 1;
    next_offset += external_source_chunk_size;

    if (!(next_offset < max_reals))
        return new_error(external_source_errc::binary_file_eof_error);

    return binary_file_source_fill_buffer(*this, src, data);
}

status binary_file_source::update(source& src, source_data& data) noexcept
{
    const auto distance = external_source_chunk_size * max_clients;
    const auto next     = data.chunk_id[1] + distance;

    if (!(next + external_source_chunk_size < max_reals))
        return new_error(external_source_errc::binary_file_eof_error);

    src.index = 0;

    return binary_file_source_fill_buffer(*this, src, data);
}

status binary_file_source::restore(source& src, source_data& data) noexcept
{
    if (offsets[numeric_cast<int>(data.chunk_id[0])] != data.chunk_id[1])
        return binary_file_source_fill_buffer(*this, src, data);

    return success();
}

status binary_file_source::finalize(source& src, source_data&) noexcept
{
    src.clear();

    return success();
}

text_file_source::text_file_source(const std::filesystem::path& p) noexcept
  : file_path(p)
{}

text_file_source::text_file_source(const text_file_source& other) noexcept
  : name(other.name)
  , buffer(other.buffer)
  , offset(other.offset)
  , file_path(other.file_path)
{}

void text_file_source::swap(text_file_source& other) noexcept
{
    std::swap(name, other.name);
    std::swap(buffer, other.buffer);
    std::swap(offset, other.offset);
    std::swap(file_path, other.file_path);
}

status text_file_source::init() noexcept
{
    if (ifs.is_open())
        ifs.close();

    ifs.open(file_path);
    if (!ifs)
        return new_error(external_source_errc::text_file_access_error);

    offset = 0;

    return success();
}

bool text_file_source::read_chunk() noexcept
{
    for (int i = 0; i < external_source_chunk_size; ++i)
        if (not(ifs >> buffer[static_cast<sz>(i)]))
            return false;
    return true;
}

static status text_file_source_fill_buffer(text_file_source& ext,
                                           source& /*src*/) noexcept
{
    if (not ext.read_chunk())
        return new_error(external_source_errc::text_file_eof_error);

    return success();
}

text_file_source& text_file_source::operator=(
  const text_file_source& other) noexcept
{
    if (this != &other) {
        text_file_source tmp{ other };
        swap(tmp);
    }

    return *this;
}

status text_file_source::init(source& src, source_data& data) noexcept
{
    src.buffer = std::span(buffer);
    src.index  = 0;

    const auto tellg = ifs.tellg();
    if (tellg == -1)
        return new_error(external_source_errc::text_file_eof_error);

    data.chunk_id[0] = static_cast<u64>(tellg);
    offset           = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

void text_file_source::finalize() noexcept
{
    if (ifs)
        ifs.close();
}

status text_file_source::update(source& src, source_data& data) noexcept
{
    src.index = 0;

    const auto tellg = ifs.tellg();
    if (tellg == -1)
        return new_error(external_source_errc::text_file_eof_error);

    data.chunk_id[0] = static_cast<u64>(tellg);
    offset           = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::restore(source& src, source_data& data) noexcept
{
    src.buffer = std::span(buffer);

    if (offset != data.chunk_id[0]) {
        if (!(is_numeric_castable<std::ifstream::off_type>(data.chunk_id[0])))
            return new_error(external_source_errc::text_file_eof_error);

        if (!ifs.seekg(numeric_cast<std::ifstream::off_type>(data.chunk_id[0])))
            return new_error(external_source_errc::text_file_eof_error);

        const auto tellg = ifs.tellg();
        if (!(tellg < 0))
            return new_error(external_source_errc::text_file_eof_error);

        offset = static_cast<u64>(tellg);
    }

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::finalize(source& src, source_data&) noexcept
{
    src.clear();
    offset = 0;

    return success();
}

random_source::random_source(const distribution_type  type,
                             std::span<const real, 2> reals_,
                             std::span<const i32, 2>  ints_) noexcept
  : reals{ reals_[0], reals_[1] }
  , ints{ ints_[0], ints_[1] }
  , distribution{ type }
{}

random_source::random_source(const random_source& other) noexcept
  : name(other.name)
  , reals(other.reals)
  , ints(other.ints)
  , distribution(other.distribution)
{}

random_source& random_source::operator=(const random_source& other) noexcept
{
    if (this != &other) {
        random_source copy{ other };
        swap(copy);
    }

    return *this;
}

void random_source::swap(random_source& other) noexcept
{
    std::swap(name, other.name);
    std::swap(reals[0], other.reals[0]);
    std::swap(reals[1], other.reals[1]);
    std::swap(ints[0], other.ints[0]);
    std::swap(ints[1], other.ints[1]);
    std::swap(distribution, other.distribution);
}

status random_source::init() noexcept { return success(); }

status random_source::finalize(source& /*src*/, source_data& data) noexcept
{
    data.chunk_id[2] = 0;

    return success();
}

status random_source::init(const u64      sim_seed,
                           const model_id mdl_id,
                           source&        src,
                           source_data&   data) noexcept
{
    src.buffer = std::span(data.chunk_real);
    src.index  = 0;

    data.chunk_id[0] = sim_seed;
    data.chunk_id[1] = ordinal(mdl_id);
    data.chunk_id[2] = 0u; // step
    data.chunk_id[3] = 0u; // index
    data.chunk_id[4] = 0u; // First random generator
    data.chunk_id[5] = 0u; // Second random generator

    data.chunk_real[0] = 0.0;
    data.chunk_real[1] = 0.0;

    return success();
}

status random_source::update(source& src, source_data& data) noexcept
{
    philox_64_view rng{ data.chunk_id };
    src.index = 0;

    switch (distribution) {
    case distribution_type::uniform_int:
        data.chunk_real[0] =
          std::uniform_int_distribution(ints[0], ints[1])(rng);
        data.chunk_real[1] =
          std::uniform_int_distribution(ints[0], ints[1])(rng);
        break;

    case distribution_type::uniform_real:
        data.chunk_real[0] =
          std::uniform_real_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] =
          std::uniform_real_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::bernouilli:
        data.chunk_real[0] = std::bernoulli_distribution(reals[0])(rng);
        data.chunk_real[1] = std::bernoulli_distribution(reals[0])(rng);
        break;

    case distribution_type::binomial:
        data.chunk_real[0] = std::binomial_distribution(ints[0], reals[0])(rng);
        data.chunk_real[1] = std::binomial_distribution(ints[0], reals[0])(rng);
        break;

    case distribution_type::negative_binomial:
        data.chunk_real[0] =
          std::negative_binomial_distribution(ints[0], reals[0])(rng);
        data.chunk_real[0] =
          std::negative_binomial_distribution(ints[0], reals[0])(rng);
        break;

    case distribution_type::geometric:
        data.chunk_real[0] = std::geometric_distribution(reals[0])(rng);
        data.chunk_real[1] = std::geometric_distribution(reals[0])(rng);
        break;

    case distribution_type::poisson:
        data.chunk_real[0] = std::poisson_distribution(reals[0])(rng);
        data.chunk_real[1] = std::poisson_distribution(reals[0])(rng);
        break;

    case distribution_type::exponential:
        data.chunk_real[0] = std::exponential_distribution(reals[0])(rng);
        data.chunk_real[1] = std::exponential_distribution(reals[0])(rng);
        break;

    case distribution_type::gamma:
        data.chunk_real[0] = std::gamma_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] = std::gamma_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::weibull:
        data.chunk_real[0] = std::weibull_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] = std::weibull_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::exterme_value:
        data.chunk_real[0] =
          std::extreme_value_distribution(reals[0], reals[1])(rng);
        data.chunk_real[0] =
          std::extreme_value_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::normal:
        data.chunk_real[0] = std::normal_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] = std::normal_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::lognormal:
        data.chunk_real[0] =
          std::lognormal_distribution(reals[0], reals[1])(rng);
        data.chunk_real[0] =
          std::lognormal_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::chi_squared:
        data.chunk_real[0] = std::chi_squared_distribution(reals[0])(rng);
        data.chunk_real[1] = std::chi_squared_distribution(reals[0])(rng);
        break;

    case distribution_type::cauchy:
        data.chunk_real[0] = std::cauchy_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] = std::cauchy_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::fisher_f:
        data.chunk_real[0] =
          std::fisher_f_distribution(reals[0], reals[1])(rng);
        data.chunk_real[1] =
          std::fisher_f_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::student_t:
        data.chunk_real[0] = std::student_t_distribution(reals[0])(rng);
        data.chunk_real[1] = std::student_t_distribution(reals[0])(rng);
        break;
    }

    return success();
}

void random_source::fill(std::span<real>    buffer,
                         const source&      src,
                         const source_data& data) noexcept
{
    source      source_copy{ src };
    source_data source_data{ data };

    for (sz i = 0u; i < buffer.size(); i += 2u) {
        update(source_copy, source_data);

        buffer[i]     = source_data.chunk_real[0];
        buffer[i + 1] = source_data.chunk_real[1];
    }
}

status random_source::restore(source& /*src*/, source_data& /*data*/) noexcept
{
    return success();
}

status external_source::prepare() noexcept
{
    for (auto& src : constant_sources)
        irt_check(src.init());

    for (auto& src : binary_file_sources)
        irt_check(src.init());

    for (auto& src : text_file_sources)
        irt_check(src.init());

    for (auto& src : random_sources)
        irt_check(src.init());

    return success();
}

void external_source::finalize() noexcept
{
    for (auto& src : binary_file_sources)
        src.finalize();

    for (auto& src : text_file_sources)
        src.finalize();
}

external_source::external_source(
  const external_source_reserve_definition& res) noexcept
  : constant_sources{ res.constant_nb.value() }
  , binary_file_sources{ res.text_file_nb.value() }
  , text_file_sources{ res.binary_file_nb.value() }
  , random_sources{ res.random_nb.value() }
  , binary_file_max_client{ res.binary_file_max_client.value() }
  , random_max_client{ res.random_max_client.value() }
{}

void external_source::clear() noexcept
{
    constant_sources.clear();
    binary_file_sources.clear();
    text_file_sources.clear();
    random_sources.clear();
}

void external_source::destroy() noexcept
{
    constant_sources.destroy();
    binary_file_sources.destroy();
    text_file_sources.destroy();
    random_sources.destroy();
}

} // namespace irt
