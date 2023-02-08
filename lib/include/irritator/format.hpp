// 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP
#define ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

namespace irt {

//! Helper function to assign fmtlib format string to an irt::small_string
//! object. This function ensure to put a null string characters (i.e. '\0') in
//! the last buffer space after the string. The string can be truncate if the
//! buffer is too small to store the formatted string.
//! \param str Output buffer.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<int N, typename S, typename... Args>
constexpr void format(small_string<N>& str, const S& fmt, Args&&... args)
{
    auto ret = fmt::vformat_to_n(str.begin(),
                                 static_cast<size_t>(N - 1),
                                 fmt,
                                 fmt::make_format_args(args...));
    str.resize(ret.size);
}

//! Copy a formatted string into the \c modeling warnings.
//!
//! The formatted string in take from the \c modeling \c ring-buffer.
//! \param mode A reference to a modeling object.
//! \param status The \c irt::status attached to the error.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<typename S, typename... Args>
constexpr void log_warning(modeling&                 mod,
                           modeling_warning::level_t level,
                           status                    st,
                           const S&                  fmt,
                           Args&&... args) noexcept
{
    using size_type = typename modeling_warning::string_t::size_type;

    if (mod.warnings.full())
        mod.warnings.pop_front();

    mod.warnings.push_back(
      { .buffer = "", .level = level, .st = status::success });
    auto& warning = mod.warnings.back();

    auto ret = fmt::vformat_to_n(warning.buffer.begin(),
                                 warning.buffer.capacity() - 1,
                                 fmt,
                                 fmt::make_format_args(args...));

    warning.buffer.resize(static_cast<size_type>(ret.size));
    warning.st = st;
}

//! Copy a formatted string into the \c modeling warnings.
//!
//! The formatted string in take from the \c modeling \c ring-buffer.
//! \param mode A reference to a modeling object.
//! \param status The \c irt::status attached to the error.
constexpr void log_warning(modeling&                 mod,
                           modeling_warning::level_t level,
                           status                    st) noexcept
{
    if (mod.warnings.full())
        mod.warnings.pop_front();

    mod.warnings.push_back(
      { .buffer = "", .level = level, .st = status::success });
    auto& warning = mod.warnings.back();
    warning.st    = st;
}

} //  irt

#endif
