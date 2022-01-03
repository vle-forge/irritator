// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020

#include <irritator/core.hpp>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <imgui.h>

namespace irt {

/// Helper to display a little (?) mark which shows a tooltip when hovered. In
/// your own code you may want to display an actual icon if you are using a
/// merged icon fonts (see docs/FONTS.md)
void HelpMarker(const char* desc) noexcept;

/// Return the description string for each status.
const char* status_string(const status s) noexcept;

/// Helper to assign fmtlib format string to a small_string.
template<size_t N, typename S, typename... Args>
constexpr void format(small_string<N>& str, const S& fmt, Args&&... args)
{
    using size_type = typename small_string<N>::size_type;

    auto ret = fmt::vformat_to_n(
      str.begin(), N - 1, fmt, fmt::make_format_args(args...));
    str.resize(static_cast<size_type>(ret.size));
}

inline int portable_filename_dirname_callback(
  ImGuiInputTextCallbackData* data) noexcept
{
    ImWchar c = data->EventChar;

    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
             ? 0
             : 1;
}

template<typename Target, typename Source>
inline bool is_numeric_castable(Source arg) noexcept
{
    static_assert(std::is_integral<Source>::value, "Integer required.");
    static_assert(std::is_integral<Target>::value, "Integer required.");

    using arg_traits    = std::numeric_limits<Source>;
    using result_traits = std::numeric_limits<Target>;

    if (result_traits::digits == arg_traits::digits &&
        result_traits::is_signed == arg_traits::is_signed)
        return true;

    if (result_traits::digits > arg_traits::digits)
        return result_traits::is_signed || arg >= 0;

    if (arg_traits::is_signed &&
        arg < static_cast<Source>(result_traits::min()))
        return false;

    return arg <= static_cast<Source>(result_traits::max());
}

} // namespace irt

namespace ImGui {

template<typename RealType>
bool InputReal(const char*         label,
               RealType*           v,
               RealType            step      = irt::zero,
               RealType            step_fast = irt::zero,
               const char*         format    = "%.6f",
               ImGuiInputTextFlags flags     = 0)
{
    if constexpr (std::is_same_v<RealType, float>) {
        return InputFloat(label, v, step, step_fast, format, flags);
    }

    if constexpr (std::is_same_v<RealType, double>) {
        return InputDouble(label, v, step, step_fast, format, flags);
    }
}

template<size_t Length>
bool InputSmallString(const char*                label,
                      irt::small_string<Length>& string,
                      ImGuiInputTextFlags        flags     = 0,
                      ImGuiInputTextCallback     callback  = nullptr,
                      void*                      user_data = nullptr)
{
    const bool ret = ImGui::InputText(
      label, string.begin(), string.capacity(), flags, callback, user_data);

    string.resize(std::strlen(string.begin()));

    return ret;
}

template<size_t Length>
bool InputFilteredString(const char*                label,
                         irt::small_string<Length>& string,
                         ImGuiInputTextFlags        flags = 0)
{
    flags |= ImGuiInputTextFlags_CallbackCharFilter |
             ImGuiInputTextFlags_EnterReturnsTrue;

    const bool ret = ImGui::InputText(label,
                                      string.begin(),
                                      string.capacity(),
                                      flags,
                                      irt::portable_filename_dirname_callback,
                                      nullptr);

    string.resize(std::strlen(string.begin()));

    return ret;
}

template<size_t Length>
bool InputSmallStringMultiline(const char*                label,
                               irt::small_string<Length>& string,
                               const ImVec2&              size  = ImVec2(0, 0),
                               ImGuiInputTextFlags        flags = 0,
                               ImGuiInputTextCallback     callback  = nullptr,
                               void*                      user_data = nullptr)
{
    const bool ret = ImGui::InputTextMultiline(label,
                                               string.begin(),
                                               string.capacity(),
                                               size,
                                               flags,
                                               callback,
                                               user_data);

    string.resize(std::strlen(string.begin()));

    return ret;
}

template<typename... Args>
void TextFormat(const char* fmt, const Args&... args)
{
    irt::small_string<64> buffer;
    irt::format(buffer, fmt, args...);
    ImGui::TextUnformatted(buffer.c_str());
}

} // namespace ImGui

#endif
