// 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP
#define ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP

#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <cstdio>

namespace irt {

/// Debug log. Use the underlying @c fmt::print function.
///
///     debug_log("to-do {}\n", 1); /* -> "to-do 1\n"
#ifdef IRRITATOR_ENABLE_DEBUG
template<typename S, typename... Args>
constexpr void debug_log(const S& s, Args&&... args) noexcept
{
    fmt::vprint(stderr, s, fmt::make_format_args(args...));
}
#else
template<typename S, typename... Args>
constexpr void debug_log([[maybe_unused]] const S& s,
                         [[maybe_unused]] Args&&... args) noexcept
{
}
#endif

/// Debug log with indent support. Use the underlying @c fmt::print function.
///
///     debug_logi(4, "to-do {}\n", 1); /* -> "    to-do 1\n */
#ifdef IRRITATOR_ENABLE_DEBUG
template<typename S, typename... Args>
constexpr void debug_logi(int indent, const S& s, Args&&... args) noexcept
{
    fmt::print(stderr, "{:{}}", "", indent);
    fmt::vprint(stderr, s, fmt::make_format_args(args...));
}
#else
template<typename S, typename... Args>
constexpr void debug_logi([[maybe_unused]] int      indent,
                          [[maybe_unused]] const S& s,
                          [[maybe_unused]] Args&&... args) noexcept
{
}
#endif

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

//! Use @c debug_log to display component data into debug console.
inline void debug_component(const modeling& mod, const component& c) noexcept
{
    constexpr std::string_view empty_path = "empty";

    auto* reg  = mod.registred_paths.try_to_get(c.reg_path);
    auto* dir  = mod.dir_paths.try_to_get(c.dir);
    auto* file = mod.file_paths.try_to_get(c.file);

    debug_log(
      "component id {} in registered path {} directory {} file {} status {}\n",
      ordinal(mod.components.get_id(c)),
      reg ? reg->path.sv() : empty_path,
      dir ? dir->path.sv() : empty_path,
      file ? file->path.sv() : empty_path,
      component_status_string[ordinal(c.state)]);
}

//! Use @c debug_log to display component data into debug console.
inline void debug_component(const modeling& mod, const component_id id) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        debug_component(mod, *compo);
    } else {
        debug_log("component id {} unknown\n", ordinal(id));
    }
}

//! Copy a formatted string into the \c modeling warnings.
//!
//! The formatted string in take from the \c modeling \c ring-buffer.
//! \param mode A reference to a modeling object.
//! \param status The \c irt::status attached to the error.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<typename S, typename... Args>
constexpr void log_warning(modeling& mod,
                           log_level level,
                           const S&  fmt,
                           Args&&... args) noexcept
{
    using size_type = typename log_str::size_type;

    if (mod.log_entries.full())
        mod.log_entries.pop_head();

    mod.log_entries.push_tail({ .buffer = "", .level = level });
    auto& warning = mod.log_entries.back();

    auto ret = fmt::vformat_to_n(warning.buffer.begin(),
                                 warning.buffer.capacity() - 1,
                                 fmt,
                                 fmt::make_format_args(args...));

    warning.buffer.resize(static_cast<size_type>(ret.size));
}

//! Copy a formatted string into the \c modeling log_entries.
//!
//! The formatted string in take from the \c modeling \c ring-buffer.
//! \param mode A reference to a modeling object.
//! \param status The \c irt::status attached to the error.
constexpr void log_warning(modeling& mod, log_level level) noexcept
{
    if (mod.log_entries.full())
        mod.log_entries.pop_head();

    mod.log_entries.push_tail({ .buffer = "", .level = level });
}

} //  irt

#endif
