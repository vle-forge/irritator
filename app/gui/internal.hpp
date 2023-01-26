// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020

#include <irritator/core.hpp>
#include <irritator/format.hpp>

#include "application.hpp"

#include <imgui.h>

namespace irt {

//! Helper to display a little (?) mark which shows a tooltip when hovered.
//! In your own code you may want to display an actual icon if you are using
//! a merged icon fonts (see docs/FONTS.md)
void HelpMarker(const char* desc) noexcept;

inline int portable_filename_dirname_callback(
  ImGuiInputTextCallbackData* data) noexcept
{
    ImWchar c = data->EventChar;

    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
             ? 0
             : 1;
}

//! Copy a formatted string into the \c logger window.
//!
//! The formatted string in take from the \c logger-window \c ring-buffer.
//! \param app A reference to the global application.
//! \param level A level between 0 and 7.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<typename S, typename... Args>
constexpr void log_w(application&         app,
                     [[maybe_unused]] int level,
                     const S&             fmt,
                     Args&&... args) noexcept
{
    using size_type = typename window_logger::string_t::size_type;

    auto& str = app.log_window.enqueue();
    auto  ret = fmt::vformat_to_n(
      str.begin(), str.capacity() - 1, fmt, fmt::make_format_args(args...));

    str.resize(static_cast<size_type>(ret.size));
}

} // namespace irt

namespace ImGui {

//        static int tristate = -1;
//        ImGui::CheckBoxTristate("Tristate", &tristate);
//        if (ImGui::SmallButton("reset to -1"))
//            tristate = -1;
bool CheckBoxTristate(const char* label, int* v_tristate) noexcept;

template<typename RealType>
bool InputReal(const char*         label,
               RealType*           v,
               RealType            step      = irt::zero,
               RealType            step_fast = irt::zero,
               const char*         format    = "%.6f",
               ImGuiInputTextFlags flags     = 0)
{
    static_assert(std::is_same_v<RealType, float> ||
                  std::is_same_v<RealType, double>);

    if constexpr (std::is_same_v<RealType, float>) {
        return InputFloat(label, v, step, step_fast, format, flags);
    } else {
        return InputDouble(label, v, step, step_fast, format, flags);
    }
}

template<int Length>
bool InputSmallString(const char*                label,
                      irt::small_string<Length>& string,
                      ImGuiInputTextFlags        flags     = 0,
                      ImGuiInputTextCallback     callback  = nullptr,
                      void*                      user_data = nullptr)
{
    const bool ret = ImGui::InputText(label,
                                      string.begin(),
                                      static_cast<size_t>(string.capacity()),
                                      flags,
                                      callback,
                                      user_data);

    string.resize(static_cast<int>(std::strlen(string.begin())));

    return ret;
}

template<int Length>
bool InputFilteredString(const char*                label,
                         irt::small_string<Length>& string,
                         ImGuiInputTextFlags        flags = 0)
{
    flags |= ImGuiInputTextFlags_CallbackCharFilter |
             ImGuiInputTextFlags_EnterReturnsTrue;

    const bool ret = ImGui::InputText(label,
                                      string.begin(),
                                      static_cast<size_t>(string.capacity()),
                                      flags,
                                      irt::portable_filename_dirname_callback,
                                      nullptr);

    string.resize(static_cast<int>(std::strlen(string.begin())));

    return ret;
}

template<int Length>
bool InputSmallStringMultiline(const char*                label,
                               irt::small_string<Length>& string,
                               const ImVec2&              size  = ImVec2(0, 0),
                               ImGuiInputTextFlags        flags = 0,
                               ImGuiInputTextCallback     callback  = nullptr,
                               void*                      user_data = nullptr)
{
    const bool ret =
      ImGui::InputTextMultiline(label,
                                string.begin(),
                                static_cast<size_t>(string.capacity()),
                                size,
                                flags,
                                callback,
                                user_data);

    string.resize(static_cast<int>(std::strlen(string.begin())));

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
