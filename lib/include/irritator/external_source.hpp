// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021
#define ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021

#include <irritator/core.hpp>

#include <array>
#include <filesystem>
#include <fstream>

namespace irt::source {

struct constant
{
    double value;

    bool operator()(external_source& src)
    {
        src.index = 0;
        return true;
    }
};

struct binary_file
{
    std::array<char, 1024 * 1024> buffer;
    std::filesystem::path file_path;
    std::ifstream ifs;
    sz buffer_size = 0;
    bool use_rewind = false;

    binary_file() = default;

    bool init(external_source& src)
    {
        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return false;
        }

        if (!read(src))
            return false;
    }

    bool operator()(external_source& src)
    {
        if (!ifs.good() && !use_rewind)
            return false;

        if (!ifs.good())
            ifs.seekg(0);

        if (!read(src))
            return false;

        return true;
    }

private:
    bool read(external_source& src)
    {
        ifs.read(buffer.data(), std::size(buffer));
        buffer_size = ifs.gcount();

        if (buffer_size % 8 != 0)
            return false;

        src.data = reinterpret_cast<double*>(buffer.data());
        src.index = 0;
        src.size = buffer_size / 8;

        return true;
    }
};

struct text_file
{
    std::array<double, 1024 * 1024 / 8> buffer;
    std::filesystem::path file_path;
    std::ifstream ifs;
    bool use_rewind = false;

    text_file() = default;

    bool init(external_source& src)
    {
        if (!ifs) {
            ifs.open(file_path);

            if (!ifs)
                return false;
        }

        if (!read(src))
            return false;
    }

    bool operator()(external_source& src)
    {
        if (!ifs.good() && !use_rewind)
            return false;

        if (!ifs.good())
            ifs.seekg(0);

        if (!read(src))
            return false;

        return true;
    }

private:
    bool read(external_source& src)
    {
        size_t i = 0;

        for (; i < std::size(buffer) && ifs.good(); ++i) {
            if (!(ifs >> buffer[i]))
                break;
        }

        src.data = buffer.data();
        src.index = 0;
        src.size = i;

        return true;
    }
};

struct random_source
{
    double* buffer = nullptr;
    sz size = 0;
    bool use_rewind;

    random_source() = default;

    ~random_source() noexcept
    {
        if (buffer)
            g_free_fn(buffer);
    }

    template<typename RandomGenerator, typename Distribution>
    bool init(const sz size_, RandomGenerator& gen, Distribution& dist) noexcept
    {
        if (!size_)
            return false;

        if (buffer)
            g_free_fn(buffer);

        size = 0;
        buffer = g_alloc_fn(sizeof(double) * size_);

        if (buffer) {
            std::generate_n(buffer, size, (*dist)(*gen));

            size = size_;
            return true;
        }

        return false;
    }

    bool operator()(external_source& src)
    {
        if (!use_rewind)
            return false;

        size = 0;

        return true;
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

} // namespace irt::source

#endif
