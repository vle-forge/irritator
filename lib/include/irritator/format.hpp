// 2022 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP
#define ORG_VLEPROJECT_IRRITATOR_2022_LIB_FORMAT_HPP

#include <irritator/core.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <cstdio>

namespace irt {

static inline constexpr const std::string_view log_level_names[] = {
    "emergency", "alert",  "critical", "error",
    "warning",   "notice", "info",     "debug",
};

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
{}
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
{}
#endif

/// Debug log with indent support. Use the underlying @c fmt::print function.
///
///     debug_logi(4, "to-do {}\n", 1); /* -> "    to-do 1\n */
#ifdef IRRITATOR_ENABLE_DEBUG
template<typename S, typename... Args>
constexpr void dlog(int indent, const S& s, Args&&... args) noexcept
{
    fmt::print(stderr, "{:{}}", "", indent);
    fmt::vprint(stderr, s, fmt::make_format_args(args...));
}

template<typename S, typename... Args>
constexpr void dlogln(int indent, const S& s, Args&&... args) noexcept
{
    fmt::print(stderr, "{:{}}", "", indent);
    fmt::vprintln(stderr, s, fmt::make_format_args(args...));
}

#else
template<typename S, typename... Args>
constexpr void dlog([[maybe_unused]] int      indent,
                    [[maybe_unused]] const S& s,
                    [[maybe_unused]] Args&&... args) noexcept
{}

template<typename S, typename... Args>
constexpr void dlogln([[maybe_unused]] int      indent,
                      [[maybe_unused]] const S& s,
                      [[maybe_unused]] Args&&... args) noexcept
{}
#endif

//! Helper function to assign fmtlib format string to an irt::small_string
//! object. This function ensure to put a null string characters (i.e. '\0') in
//! the last buffer space after the string. The string can be truncate if the
//! buffer is too small to store the formatted string.
//! \param str Output buffer.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<std::size_t N, typename... Args>
inline void format(small_string<N>&            str,
                   fmt::format_string<Args...> fmt_str,
                   Args&&... args) noexcept
{
    auto result = fmt::vformat_to_n(str.data(),
                                    static_cast<std::size_t>(str.capacity()),
                                    fmt_str,
                                    fmt::make_format_args(args...));

    str.resize(static_cast<typename small_string<N>::index_type>(
      std::min<std::size_t>(result.size, str.capacity())));
}

template<std::size_t N, typename... Args>
inline small_string<N> format_n(fmt::format_string<Args...> fmt_str,
                                Args&&... args) noexcept
{
    small_string<N> str;

    auto result = fmt::vformat_to_n(
      str.data(), str.capacity(), fmt_str, fmt::make_format_args(args...));

    str.resize(static_cast<typename small_string<N>::index_type>(
      std::min<std::size_t>(result.size, str.capacity())));

    return str;
}

} //  irt

/**
 * @brief Format @c resampled_sample
 *
 * @code
 * fmt::print("{:.0f;.6g}\n", r);   // t use .0f, value use .6g
 * fmt::print("{:.3f}\n", r);       // no ';' -> t use .3f, value as default
 * @endcode
 */
template<>
struct fmt::formatter<::irt::resampled_sample> {
    fmt::formatter<double> t_formatter;
    fmt::formatter<double> value_formatter;

    constexpr auto parse(format_parse_context& ctx) noexcept
      -> format_parse_context::iterator
    {
        auto it  = ctx.begin();
        auto end = ctx.end();

        auto sep = it;
        while (sep != end && *sep != '}' && *sep != ';')
            ++sep;

        {
            format_parse_context t_ctx(
              fmt::string_view(it, static_cast<size_t>(sep - it)));
            auto t_end = t_formatter.parse(t_ctx);

            if (t_end != t_ctx.end()) {
                using namespace std::literals;

                log(::irt::log_level::critical,
                    "format.hpp"sv
                    "invalid spec for resampled-sample::t\n"sv);

                return ctx.begin();
            }
        }

        if (sep == end || *sep == '}')
            return sep;

        ++sep;
        auto vend = sep;
        while (vend != end && *vend != '}')
            ++vend;

        {
            format_parse_context v_ctx(
              fmt::string_view(sep, static_cast<size_t>(vend - sep)));
            auto v_end = value_formatter.parse(v_ctx);
            if (v_end != v_ctx.end()) {
                using namespace std::literals;

                log(::irt::log_level::critical,
                    "format.hpp"sv
                    "invalid spec for resampled-sample::value\n"sv);

                return ctx.begin();
            }
        }

        return vend;
    }

    constexpr auto format(const ::irt::resampled_sample& r,
                          format_context&                ctx) const noexcept
      -> format_context::iterator
    {
        auto out = t_formatter.format(r.t, ctx);
        *out++   = ' ';
        ctx.advance_to(out);
        return value_formatter.format(r.value, ctx);
    }
};

template<>
struct fmt::formatter<::irt::human_readable_bytes> {

    constexpr auto parse(format_parse_context& ctx) noexcept
      -> format_parse_context::iterator
    {
        return ctx.begin();
    }

    auto format(const ::irt::human_readable_bytes& hr,
                format_context& ctx) const noexcept -> format_context::iterator
    {
        switch (hr.type) {
        case ::irt::human_readable_bytes::display_type::B:
            return format_to(ctx.out(), "{:.4f} B", hr.size);
        case ::irt::human_readable_bytes::display_type::KB:
            return format_to(ctx.out(), "{:.4f} KB", hr.size);
        case ::irt::human_readable_bytes::display_type::MB:
            return format_to(ctx.out(), "{:.4f} MB", hr.size);
        case ::irt::human_readable_bytes::display_type::GB:
            return format_to(ctx.out(), "{:.4f} GB", hr.size);
        }

        irt::unreachable();
    }
};

template<>
struct fmt::formatter<::irt::human_readable_time> {

    constexpr auto parse(format_parse_context& ctx) noexcept
      -> format_parse_context::iterator
    {
        return ctx.begin();
    }

    auto format(const ::irt::human_readable_time& hr,
                format_context& ctx) const noexcept -> format_context::iterator
    {
        switch (hr.type) {
        case irt::human_readable_time::display_type::nanoseconds:
            return format_to(ctx.out(), "{:.4f} ns", hr.value);
        case irt::human_readable_time::display_type::microseconds:
            return format_to(ctx.out(), "{:.4f} us", hr.value);
        case irt::human_readable_time::display_type::milliseconds:
            return format_to(ctx.out(), "{:.4f} ms", hr.value);
        case irt::human_readable_time::display_type::seconds:
            return format_to(ctx.out(), "{:.4f} s", hr.value);
        case irt::human_readable_time::display_type::minutes:
            return format_to(ctx.out(), "{:.4f} m", hr.value);
        case irt::human_readable_time::display_type::hours:
            return format_to(ctx.out(), "{:.4f} h", hr.value);
        }

        irt::unreachable();
    }
};

#endif
