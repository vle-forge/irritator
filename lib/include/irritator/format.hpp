// 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP
#define ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP

#include <irritator/core.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

namespace irt {

//! Helper function to assign fmtlib format string to an irt::small_string
//! object. This function ensure to put a null string characters (i.e. '\0') in
//! the last buffer space after the string. The string can be truncate if the
//! buffer is too small to store the formatted string.
template<int N, typename S, typename... Args>
constexpr void format(small_string<N>& str, const S& fmt, Args&&... args)
{
    auto ret = fmt::vformat_to_n(str.begin(),
                                 static_cast<size_t>(N - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    str.resize(ret.size);
}

} //  irt

#endif
