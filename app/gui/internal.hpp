// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020
#define ORG_VLEPROJECT_IRRITATOR_APP_INTERNAL_2020

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>

#include "application.hpp"

#include <imgui.h>

#include <utility>

namespace irt {

constexpr static inline i32                  simulation_task_number = 64;
constexpr static inline std::array<float, 4> black_color{ 0.f, 0.f, 0.f, 0.f };
constexpr static inline std::array<float, 4> default_component_color{ 0.5f,
                                                                      0.5f,
                                                                      0.5f,
                                                                      0.0f };

inline ImVec4& to_ImVec4(std::array<float, 4>& array) noexcept
{
    return reinterpret_cast<ImVec4&>(array);
}

inline const ImVec4& to_ImVec4(const std::array<float, 4>& array) noexcept
{
    return reinterpret_cast<const ImVec4&>(array);
}

inline float* to_float_ptr(std::array<float, 4>& array) noexcept
{
    return &array[0];
}

inline const float* to_float_ptr(const std::array<float, 4>& array) noexcept
{
    return &array[0];
}

constexpr inline float saturate(const float v) noexcept
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

constexpr inline u8 float_to_u8_sat(const float v) noexcept
{
    return static_cast<u8>(saturate(v) * 255.0f + 0.5f);
}

constexpr ImU32 to_ImU32(const std::array<float, 4>& in) noexcept
{
    ImU32 out = float_to_u8_sat(in[0]) << IM_COL32_R_SHIFT;
    out |= float_to_u8_sat(in[1]) << IM_COL32_G_SHIFT;
    out |= float_to_u8_sat(in[2]) << IM_COL32_B_SHIFT;
    out |= float_to_u8_sat(in[3]) << IM_COL32_A_SHIFT;

    return out;
}

//! Helper to display a little (?) mark which shows a tooltip when hovered.
//! In your own code you may want to display an actual icon if you are using
//! a merged icon fonts (see docs/FONTS.md)
void HelpMarker(const char* desc) noexcept;

inline int portable_filename_dirname_callback(
  ImGuiInputTextCallbackData* data) noexcept
{
    ImWchar c = data->EventChar;

    if (data->BufTextLen <= 1 &&
        ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_'))
        return 0;

    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
             ? 0
             : 1;
}

//! Copy a formatted string into the \c logger window.
//!
//! The formatted string in take from the \c logger-window \c ring-buffer.
//! \param app A reference to the global application.
//! \param level Log level of the message 0 and 7.
//! \param fmt A format string for the fmtlib library.
//! \param args Arguments for the fmtlib library.
template<typename S, typename... Args>
constexpr void log_w(application& app,
                     log_level    level,
                     const S&     fmt,
                     Args&&... args) noexcept
{
    using size_type = typename window_logger::string_t::size_type;

    auto level_msg     = log_level_names[ordinal(level)];
    auto level_msg_len = level_msg.size();

    auto& str = app.log_wnd.enqueue();
    debug::ensure(std::cmp_greater(str.capacity(), level_msg_len));

    std::copy_n(level_msg.data(), level_msg.size(), str.begin());
    str[level_msg_len] = ' ';
    str.resize(++level_msg_len);

    auto remaining = static_cast<sz>(str.capacity()) - level_msg_len;
    if (remaining > 1) {
        auto ret = fmt::vformat_to_n(str.begin() + level_msg_len,
                                     remaining - 1,
                                     fmt,
                                     fmt::make_format_args(args...));

        str.resize(static_cast<size_type>(ret.size + level_msg_len));
    }
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
void TextFormat(const char* fmt, const Args&... args) noexcept
{
    irt::small_string<256> buffer;
    irt::format(buffer, fmt, args...);
    ImGui::TextUnformatted(buffer.c_str());
}

template<typename... Args>
void TextFormatDisabled(const char* fmt, const Args&... args) noexcept
{
    irt::small_string<256> buffer;

    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    irt::format(buffer, fmt, args...);
    ImGui::TextUnformatted(buffer.c_str());
    ImGui::PopStyleColor();
}

inline auto ComputeButtonSize(int button_number) noexcept -> ImVec2
{
    return ImVec2{ (ImGui::GetContentRegionAvail().x -
                    ImGui::GetStyle().ItemSpacing.x) /
                     static_cast<float>(button_number),
                   0.f };
}

} // namespace ImGui

#endif
