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

constexpr static inline i32 simulation_task_number = 64;

inline ImVec4 to_ImVec4(std::span<const float, 4>& array) noexcept
{
    return ImVec4(array[0], array[1], array[2], array[3]);
}

inline ImVec4& to_ImVec4(std::array<float, 4>& array) noexcept
{
    return reinterpret_cast<ImVec4&>(array);
}

inline const ImVec4& to_ImVec4(const std::array<float, 4>& array) noexcept
{
    return reinterpret_cast<const ImVec4&>(array);
}

inline const float* to_float_ptr(std::span<float, 4>& array) noexcept
{
    return array.data();
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

constexpr ImU32 to_ImU32(const std::span<const float, 4> in) noexcept
{
    ImU32 out = float_to_u8_sat(in[0]) << IM_COL32_R_SHIFT;
    out |= float_to_u8_sat(in[1]) << IM_COL32_G_SHIFT;
    out |= float_to_u8_sat(in[2]) << IM_COL32_B_SHIFT;
    out |= float_to_u8_sat(in[3]) << IM_COL32_A_SHIFT;

    return out;
}

constexpr ImVec4 to_ImVec4(const std::span<const float, 4> in) noexcept
{
    return ImVec4(in[0], in[1], in[2], in[3]);
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

template<std::size_t Length>
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

template<std::size_t Length>
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

template<std::size_t Length>
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

template<typename... Args>
void LabelFormat(const char* label,
                 const char* fmt,
                 const Args&... args) noexcept
{
    irt::small_string<256> buffer;
    irt::format(buffer, fmt, args...);
    ImGui::LabelText(label, "%s", buffer.c_str());
}

inline auto SelectableWithHint(const char*          label,
                               const char*          hint,
                               bool*                p_selected,
                               ImGuiSelectableFlags flags = 0,
                               const ImVec2& size_arg = ImVec2(0, 0)) noexcept
  -> bool
{
    ::irt::debug::ensure(label);
    ::irt::debug::ensure(hint);

    bool ret = ImGui::Selectable(label, p_selected, flags, size_arg);

    ImGui::SameLine();

    const auto avail =
      ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(hint).x;
    const auto max_avail = std::max(0.f, avail);
    const auto pos       = ImGui::GetCursorPosX() + max_avail;

    ImGui::SetCursorPosX(pos);
    ImGui::TextDisabled("%s", hint);

    return ret;
}

inline auto TreeNodeExWithHint(const char*        label,
                               const char*        hint,
                               ImGuiTreeNodeFlags flags = 0) noexcept -> bool
{
    ::irt::debug::ensure(label);
    ::irt::debug::ensure(hint);

    bool ret = ImGui::TreeNodeEx(label, flags);

    ImGui::SameLine();

    const auto avail =
      ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(hint).x;
    const auto max_avail = std::max(0.f, avail);
    const auto pos       = ImGui::GetCursorPosX() + max_avail;

    ImGui::SetCursorPosX(pos);
    ImGui::TextDisabled("%s", hint);

    return ret;
}

inline auto TreeNodeExSelectableWithHint(const char*        label,
                                         const char*        hint,
                                         bool*              p_selected,
                                         ImGuiTreeNodeFlags flags = 0) noexcept
  -> bool
{
    ::irt::debug::ensure(label);
    ::irt::debug::ensure(hint);

    if (*p_selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool ret = ImGui::TreeNodeEx(label, flags);

    if (ImGui::IsItemClicked())
        *p_selected = !*p_selected;

    ImGui::SameLine();

    const auto avail =
      ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(hint).x;
    const auto max_avail = std::max(0.f, avail);
    const auto pos       = ImGui::GetCursorPosX() + max_avail;

    ImGui::SetCursorPosX(pos);
    ImGui::TextDisabled("%s", hint);

    return ret;
}

inline auto ComputeButtonSize(int button_number) noexcept -> ImVec2
{
    ::irt::debug::ensure(button_number > 1);

    return ImVec2{ ((ImGui::GetContentRegionAvail().x -
                     (static_cast<float>(button_number) *
                      ImGui::GetStyle().ItemSpacing.x)) /
                    static_cast<float>(button_number)),
                   0.f };
}

} // namespace ImGui

#endif
