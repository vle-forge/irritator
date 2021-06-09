// Copyright (c) 2020-2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021
#define ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021

#include <irritator/core.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <vector>

namespace irt {

enum class external_source_type
{
    binary_file,
    constant,
    random,
    text_file
};

enum class block_vector_policy
{
    not_reuse_free_list,
    reuse_free_list
};

template<block_vector_policy P>
struct block_vector_base
{
    double* buffer = nullptr;
    sz size = 0;
    sz max_size = 0;
    sz capacity = 0;
    sz block_size = 0;
    sz free_head = static_cast<sz>(-1);

    block_vector_base() = default;

    block_vector_base(const block_vector_base&) = delete;
    block_vector_base& operator=(const block_vector_base&) = delete;

    ~block_vector_base() noexcept
    {
        if (buffer)
            g_free_fn(buffer);
    }

    bool empty() const noexcept
    {
        return size == 0;
    }

    status init(sz block_size_, sz capacity_) noexcept
    {
        if (capacity_ == 0)
            return status::block_allocator_bad_capacity;

        if (capacity_ != capacity) {
            if (buffer)
                g_free_fn(buffer);

            buffer = static_cast<double*>(g_alloc_fn(capacity_ * block_size_));
            if (buffer == nullptr)
                return status::block_allocator_not_enough_memory;
        }

        size = 0;
        max_size = 0;
        capacity = capacity_;
        block_size = block_size_;
        free_head = static_cast<sz>(-1);

        return status::success;
    }

    bool can_alloc() const noexcept
    {
        if constexpr (P == block_vector_policy::reuse_free_list)
            return free_head != static_cast<sz>(-1) || max_size < capacity;
        else
            return max_size < capacity;
    }

    double* alloc() noexcept
    {
        sz new_block;

        if constexpr (P == block_vector_policy::reuse_free_list) {
            if (free_head != static_cast<sz>(-1)) {
                new_block = free_head;
                free_head = static_cast<sz>(buffer[free_head * block_size]);
            } else {
                new_block = max_size * block_size;
                ++max_size;
            }
        } else {
            new_block = max_size * block_size;
            ++max_size;
        }

        ++size;
        return &buffer[new_block];
    }

    void free([[maybe_unused]] double* block) noexcept
    {
        if constexpr (P == block_vector_policy::reuse_free_list) {
            auto ptr_diff = buffer - block;
            auto block_index = ptr_diff / block_size;

            block[0] = static_cast<double>(free_head);
            free_head = static_cast<sz>(block_index);
            --size;

            if (size == 0) {
                max_size = 0;
                free_head = static_cast<sz>(-1);
            }
        }
    }
};

using limited_block_vector =
  block_vector_base<block_vector_policy::not_reuse_free_list>;
using block_vector = block_vector_base<block_vector_policy::reuse_free_list>;

inline bool
external_source_type_cast(int value, external_source_type* type) noexcept
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

inline const char*
external_source_str(const external_source_type type) noexcept
{
    return external_source_type_string[static_cast<int>(type)];
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

inline const char*
distribution_str(const distribution_type type) noexcept
{

    return distribution_type_string[static_cast<int>(type)];
}

struct constant_source
{
    small_string<23> name;
    std::vector<double> buffer;

    status init(sz block_size) noexcept
    {
        try {
            buffer.reserve(block_size);
            return status::success;
        } catch (const std::bad_alloc& /*e*/) {
            return status::gui_not_enough_memory;
        }
    }

    status start_or_restart() noexcept
    {
        return status::success;
    }

    status update(source& src) noexcept
    {
        src.buffer = buffer.data();
        src.size = static_cast<int>(buffer.size());
        src.index = 0;

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
            return update(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }
};

struct binary_file_source
{
    small_string<23> name;
    limited_block_vector buffer;
    std::filesystem::path file_path;
    std::ifstream ifs;
    sz size = 0; // Number of double read

    status init(sz block_size, sz capacity) noexcept
    {
        file_path.clear();

        if (ifs.is_open())
            ifs.close();

        return buffer.init(block_size, capacity);
    }

    status start_or_restart() noexcept
    {
        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return status::source_empty;
        } else {
            ifs.seekg(0);
        }

        return fill_buffer();
    }

    status update(source& src)
    {
        if (!buffer.can_alloc()) {
            if (src.buffer)
                buffer.free(src.buffer);

            irt_return_if_bad(fill_buffer());
        }

        src.buffer = buffer.alloc();
        src.size = static_cast<int>(buffer.block_size);
        src.index = 0;

        return status::success;
    }

    status finalize(source& src)
    {
        if (src.buffer)
            buffer.free(src.buffer);

        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op)
    {
        switch (op) {
        case source::operation_type::initialize:
            return update(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }

private:
    status fill_buffer() noexcept
    {
        ifs.read(reinterpret_cast<char*>(buffer.buffer),
                 buffer.capacity * buffer.block_size);
        const auto read = ifs.gcount();

        buffer.capacity = static_cast<sz>(read) / buffer.block_size;
        if (buffer.capacity == 0)
            return status::source_empty;

        return status::success;
    }
};

struct text_file_source
{
    small_string<23> name;
    limited_block_vector buffer;
    std::filesystem::path file_path;
    std::ifstream ifs;

    status init(sz block_size, sz capacity) noexcept
    {
        return buffer.init(block_size, capacity);
        file_path.clear();

        if (ifs.is_open())
            ifs.close();
    }

    status start_or_restart() noexcept
    {
        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return status::source_empty;
        } else {
            ifs.seekg(0);
        }

        return fill_buffer();
    }

    status update(source& src)
    {
        if (!buffer.can_alloc()) {
            if (src.buffer)
                buffer.free(src.buffer);

            irt_return_if_bad(fill_buffer());
        }

        src.buffer = buffer.alloc();
        src.size = static_cast<int>(buffer.block_size);
        src.index = 0;

        return status::success;
    }

    status finalize(source& src)
    {
        if (src.buffer)
            buffer.free(src.buffer);

        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op)
    {
        switch (op) {
        case source::operation_type::initialize:
            return update(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }

private:
    status fill_buffer() noexcept
    {
        size_t i = 0;
        const size_t e = buffer.capacity * buffer.block_size;
        for (; i < e && ifs.good(); ++i) {
            if (!(ifs >> buffer.buffer[i]))
                break;
        }

        buffer.capacity = i / buffer.block_size;
        if (buffer.capacity == 0)
            return status::source_empty;

        return status::success;
    }
};

struct random_source
{
    small_string<23> name;
    block_vector buffer;
    distribution_type distribution = distribution_type::uniform_int;
    double a, b, p, mean, lambda, alpha, beta, stddev, m, s, n;
    int a32, b32, t32, k32;

    template<typename RandomGenerator, typename Distribution>
    void generate(RandomGenerator& gen,
                  Distribution dist,
                  double* ptr,
                  sz size) noexcept
    {
        for (sz i = 0; i < size; ++i)
            ptr[i] = dist(gen);
    }

    template<typename RandomGenerator>
    void generate(RandomGenerator& gen, double* ptr, sz size) noexcept
    {
        switch (distribution) {
        case distribution_type::uniform_int:
            generate(gen, std::uniform_int_distribution(a32, b32), ptr, size);
            break;

        case distribution_type::uniform_real:
            generate(gen, std::uniform_real_distribution(a, b), ptr, size);
            break;

        case distribution_type::bernouilli:
            generate(gen, std::bernoulli_distribution(p), ptr, size);
            break;

        case distribution_type::binomial:
            generate(gen, std::binomial_distribution(t32, p), ptr, size);
            break;

        case distribution_type::negative_binomial:
            generate(
              gen, std::negative_binomial_distribution(t32, p), ptr, size);
            break;

        case distribution_type::geometric:
            generate(gen, std::geometric_distribution(p), ptr, size);
            break;

        case distribution_type::poisson:
            generate(gen, std::poisson_distribution(mean), ptr, size);
            break;

        case distribution_type::exponential:
            generate(gen, std::exponential_distribution(lambda), ptr, size);
            break;

        case distribution_type::gamma:
            generate(gen, std::gamma_distribution(alpha, beta), ptr, size);
            break;

        case distribution_type::weibull:
            generate(gen, std::weibull_distribution(a, b), ptr, size);
            break;

        case distribution_type::exterme_value:
            generate(gen, std::extreme_value_distribution(a, b), ptr, size);
            break;

        case distribution_type::normal:
            generate(gen, std::normal_distribution(mean, stddev), ptr, size);
            break;

        case distribution_type::lognormal:
            generate(gen, std::lognormal_distribution(m, s), ptr, size);
            break;

        case distribution_type::chi_squared:
            generate(gen, std::chi_squared_distribution(n), ptr, size);
            break;

        case distribution_type::cauchy:
            generate(gen, std::cauchy_distribution(a, b), ptr, size);
            break;

        case distribution_type::fisher_f:
            generate(gen, std::fisher_f_distribution(m, n), ptr, size);
            break;

        case distribution_type::student_t:
            generate(gen, std::student_t_distribution(n), ptr, size);
            break;
        }
    }

    status init(sz block_size, sz capacity) noexcept
    {
        distribution = distribution_type::uniform_int;
        a = 0;
        b = 100;

        return buffer.init(block_size, capacity);
    }

    status update(source& src) noexcept
    {
        if (src.buffer == nullptr) {
            if (!buffer.can_alloc())
                return status::source_empty;

            src.buffer = buffer.alloc();
            src.size = static_cast<int>(buffer.block_size);
            src.index = 0;
        }

        std::mt19937_64 gen;
        generate(gen, src.buffer, src.size);

        return status::success;
    }

    status finalize(source& src)
    {
        if (src.buffer)
            buffer.free(src.buffer);

        src.clear();

        return status::success;
    }

    status operator()(source& src, source::operation_type op)
    {
        switch (op) {
        case source::operation_type::initialize:
            return update(src);
        case source::operation_type::update:
            return update(src);
        case source::operation_type::finalize:
            return finalize(src);
        }

        irt_unreachable();
    }
};

enum class constant_source_id : u64;
enum class binary_file_source_id : u64;
enum class text_file_source_id : u64;
enum class random_source_id : u64;

struct external_source
{
    data_array<constant_source, constant_source_id> constant_sources;
    data_array<binary_file_source, binary_file_source_id> binary_file_sources;
    data_array<text_file_source, text_file_source_id> text_file_sources;
    data_array<random_source, random_source_id> random_sources;
    std::mt19937_64 generator;

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
inline int
generate_random_file(std::ostream& os,
                     RandomGenerator& gen,
                     Distribution& dist,
                     const std::size_t size,
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
