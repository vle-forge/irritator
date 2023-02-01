// Copyright (c) 2020-2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021
#define ORG_VLEPROJECT_IRRITATOR_EXTERNAL_SOURCE_2021

#include <irritator/core.hpp>

namespace irt {

static inline const char* external_source_type_string[] = { "binary_file",
                                                            "constant",
                                                            "random",
                                                            "text_file" };

inline const char* external_source_str(const source::source_type type) noexcept
{
    return external_source_type_string[ordinal(type)];
}

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
