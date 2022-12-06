// 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP
#define ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP

#include <irritator/core.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

namespace irt {

//! Helper to assign fmtlib format string to a small_string.
template<int N, typename S, typename... Args>
constexpr void format(small_string<N>& str, const S& fmt, Args&&... args)
{
    using size_type = typename small_string<N>::size_type;

    auto ret = fmt::vformat_to_n(str.begin(),
                                 static_cast<size_t>(N - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    str.resize(static_cast<size_type>(ret.size));
}

} //  irt

#endif