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

status constant_source::init(source& src) noexcept
{
    src.buffer = std::span(buffer.data(), length);
    src.index  = 0;

    return success();
}

status constant_source::update(source& src) noexcept
{
    src.index = 0;

    return success();
}

status constant_source::restore(source& /*src*/) noexcept { return success(); }

status constant_source::finalize(source& src) noexcept
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

int binary_file_source::tellg() noexcept { return ifs.tellg(); }

static status binary_file_source_fill_buffer(binary_file_source& ext,
                                             source&             src) noexcept
{
    const auto to_seek = src.chunk_id[1] * sizeof(double);

    if (!ext.seekg(static_cast<long>(to_seek)))
        return new_error(external_source_errc::binary_file_eof_error);

    if (!ext.read(src, external_source_chunk_size))
        return new_error(external_source_errc::binary_file_eof_error);

    const auto tellg = ext.tellg();
    if (tellg < 0)
        return new_error(external_source_errc::binary_file_eof_error);

    const auto current_position = static_cast<u64>(tellg) / sizeof(double);
    src.chunk_id[1]             = current_position;
    ext.offsets[numeric_cast<int>(src.chunk_id[0])] = current_position;

    return success();
}

status binary_file_source::init(source& src) noexcept
{
    src.buffer      = std::span(buffers[next_client]);
    src.index       = 0;
    src.chunk_id[0] = next_client;
    src.chunk_id[1] = next_offset;
    offsets[numeric_cast<int>(src.chunk_id[0])] = next_offset;

    next_client += 1;
    next_offset += external_source_chunk_size;

    if (!(next_offset < max_reals))
        return new_error(external_source_errc::binary_file_eof_error);

    return binary_file_source_fill_buffer(*this, src);
}

status binary_file_source::update(source& src) noexcept
{
    const auto distance = external_source_chunk_size * max_clients;
    const auto next     = src.chunk_id[1] + distance;

    if (!(next + external_source_chunk_size < max_reals))
        return new_error(external_source_errc::binary_file_eof_error);

    src.index = 0;

    return binary_file_source_fill_buffer(*this, src);
}

status binary_file_source::restore(source& src) noexcept
{
    if (offsets[numeric_cast<int>(src.chunk_id[0])] != src.chunk_id[1])
        return binary_file_source_fill_buffer(*this, src);

    return success();
}

status binary_file_source::finalize(source& src) noexcept
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

status text_file_source::init(source& src) noexcept
{
    src.buffer = std::span(buffer);
    src.index  = 0;

    const auto tellg = ifs.tellg();
    if (tellg == -1)
        return new_error(external_source_errc::text_file_eof_error);

    src.chunk_id[0] = static_cast<u64>(tellg);
    offset          = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

void text_file_source::finalize() noexcept
{
    if (ifs)
        ifs.close();
}

status text_file_source::update(source& src) noexcept
{
    src.index = 0;

    const auto tellg = ifs.tellg();
    if (tellg == -1)
        return new_error(external_source_errc::text_file_eof_error);

    src.chunk_id[0] = static_cast<u64>(tellg);
    offset          = static_cast<u64>(tellg);

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::restore(source& src) noexcept
{
    src.buffer = std::span(buffer);

    if (offset != src.chunk_id[0]) {
        if (!(is_numeric_castable<std::ifstream::off_type>(src.chunk_id[0])))
            return new_error(external_source_errc::text_file_eof_error);

        if (!ifs.seekg(numeric_cast<std::ifstream::off_type>(src.chunk_id[0])))
            return new_error(external_source_errc::text_file_eof_error);

        const auto tellg = ifs.tellg();
        if (!(tellg < 0))
            return new_error(external_source_errc::text_file_eof_error);

        offset = static_cast<u64>(tellg);
    }

    return text_file_source_fill_buffer(*this, src);
}

status text_file_source::finalize(source& src) noexcept
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

status random_source::finalize(source& src) noexcept
{
    src.chunk_id[2] = 0;

    return success();
}

status random_source::init(source& src) noexcept
{
    src.buffer = std::span(src.chunk_real);
    src.index  = 0;

    src.chunk_id[0] = 0u; // seed ?
    src.chunk_id[1] = 0u; // ordinal(model_id) ?
    src.chunk_id[2] = 0u; // step
    src.chunk_id[3] = 0u; // index
    src.chunk_id[4] = 0u; // First random generator
    src.chunk_id[5] = 0u; // Second random generator

    src.chunk_real[0] = 0.0;
    src.chunk_real[1] = 0.0;

    return success();
}

status random_source::update(source& src) noexcept
{
    philox_64_view rng{ src.chunk_id };
    src.index = 0;

    switch (distribution) {
    case distribution_type::uniform_int:
        src.chunk_real[0] =
          std::uniform_int_distribution(ints[0], ints[1])(rng);
        src.chunk_real[1] =
          std::uniform_int_distribution(ints[0], ints[1])(rng);
        break;

    case distribution_type::uniform_real:
        src.chunk_real[0] =
          std::uniform_real_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] =
          std::uniform_real_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::bernouilli:
        src.chunk_real[0] = std::bernoulli_distribution(reals[0])(rng);
        src.chunk_real[1] = std::bernoulli_distribution(reals[0])(rng);
        break;

    case distribution_type::binomial:
        src.chunk_real[0] = std::binomial_distribution(ints[0], reals[0])(rng);
        src.chunk_real[1] = std::binomial_distribution(ints[0], reals[0])(rng);
        break;

    case distribution_type::negative_binomial:
        src.chunk_real[0] =
          std::negative_binomial_distribution(ints[0], reals[0])(rng);
        src.chunk_real[0] =
          std::negative_binomial_distribution(ints[0], reals[0])(rng);
        break;

    case distribution_type::geometric:
        src.chunk_real[0] = std::geometric_distribution(reals[0])(rng);
        src.chunk_real[1] = std::geometric_distribution(reals[0])(rng);
        break;

    case distribution_type::poisson:
        src.chunk_real[0] = std::poisson_distribution(reals[0])(rng);
        src.chunk_real[1] = std::poisson_distribution(reals[0])(rng);
        break;

    case distribution_type::exponential:
        src.chunk_real[0] = std::exponential_distribution(reals[0])(rng);
        src.chunk_real[1] = std::exponential_distribution(reals[0])(rng);
        break;

    case distribution_type::gamma:
        src.chunk_real[0] = std::gamma_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] = std::gamma_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::weibull:
        src.chunk_real[0] = std::weibull_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] = std::weibull_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::exterme_value:
        src.chunk_real[0] =
          std::extreme_value_distribution(reals[0], reals[1])(rng);
        src.chunk_real[0] =
          std::extreme_value_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::normal:
        src.chunk_real[0] = std::normal_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] = std::normal_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::lognormal:
        src.chunk_real[0] =
          std::lognormal_distribution(reals[0], reals[1])(rng);
        src.chunk_real[0] =
          std::lognormal_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::chi_squared:
        src.chunk_real[0] = std::chi_squared_distribution(reals[0])(rng);
        src.chunk_real[1] = std::chi_squared_distribution(reals[0])(rng);
        break;

    case distribution_type::cauchy:
        src.chunk_real[0] = std::cauchy_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] = std::cauchy_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::fisher_f:
        src.chunk_real[0] = std::fisher_f_distribution(reals[0], reals[1])(rng);
        src.chunk_real[1] = std::fisher_f_distribution(reals[0], reals[1])(rng);
        break;

    case distribution_type::student_t:
        src.chunk_real[0] = std::student_t_distribution(reals[0])(rng);
        src.chunk_real[1] = std::student_t_distribution(reals[0])(rng);
        break;
    }

    return success();
}

status random_source::restore(source& /*src*/) noexcept { return success(); }

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

    unreachable();
}

status external_source::dispatch(source&                      src,
                                 const source::operation_type op) noexcept
{
    switch (src.type) {
    case source_type::binary_file: {
        if (auto* bin_src =
              binary_file_sources.try_to_get(src.id.binary_file_id))
            return external_source_dispatch(*bin_src, src, op);

        return new_error(external_source_errc::binary_file_unknown);
    } break;

    case source_type::constant: {
        if (auto* cst_src = constant_sources.try_to_get(src.id.constant_id))
            return external_source_dispatch(*cst_src, src, op);

        return new_error(external_source_errc::constant_unknown);
    } break;

    case source_type::random: {
        if (auto* rnd_src = random_sources.try_to_get(src.id.random_id))
            return external_source_dispatch(*rnd_src, src, op);

        return new_error(external_source_errc::random_unknown);

    } break;

    case source_type::text_file: {
        if (auto* txt_src = text_file_sources.try_to_get(src.id.text_file_id))
            return external_source_dispatch(*txt_src, src, op);

        return new_error(external_source_errc::text_file_unknown);
    } break;
    }

    unreachable();
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
