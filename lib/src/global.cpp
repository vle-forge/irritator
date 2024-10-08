// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/global.hpp>

#include <istream>
#include <mutex>
#include <streambuf>

#include <fmt/format.h>
#include <fmt/ostream.h>

namespace irt {

struct membuf : std::streambuf {
    membuf(char const* base, size_t size)
    {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
};
struct imemstream
  : virtual membuf
  , std::istream {
    imemstream(char const* base, size_t size)
      : membuf(base, size)
      , std::istream(static_cast<std::streambuf*>(this))
    {}
};

template<typename Iterator, typename T, typename Compare>
constexpr Iterator sorted_vector_find(Iterator begin,
                                      Iterator end,
                                      const T& value,
                                      Compare  comp)
{
    begin = std::lower_bound(begin, end, value, comp);
    return (begin != end && !comp(value, *begin)) ? begin : end;
}

enum {
    Irt_Col_Text,
    Irt_Col_TextDisabled,
    Irt_Col_WindowBg,
    Irt_Col_ChildBg,
    Irt_Col_PopupBg,
    Irt_Col_Border,
    Irt_Col_BorderShadow,
    Irt_Col_FrameBg,
    Irt_Col_FrameBgHovered,
    Irt_Col_FrameBgActive,
    Irt_Col_TitleBg,
    Irt_Col_TitleBgActive,
    Irt_Col_TitleBgCollapsed,
    Irt_Col_MenuBarBg,
    Irt_Col_ScrollbarBg,
    Irt_Col_ScrollbarGrab,
    Irt_Col_ScrollbarGrabHovered,
    Irt_Col_ScrollbarGrabActive,
    Irt_Col_CheckMark,
    Irt_Col_SliderGrab,
    Irt_Col_SliderGrabActive,
    Irt_Col_Button,
    Irt_Col_ButtonHovered,
    Irt_Col_ButtonActive,
    Irt_Col_Header,
    Irt_Col_HeaderHovered,
    Irt_Col_HeaderActive,
    Irt_Col_Separator,
    Irt_Col_SeparatorHovered,
    Irt_Col_SeparatorActive,
    Irt_Col_ResizeGrip,
    Irt_Col_ResizeGripHovered,
    Irt_Col_ResizeGripActive,
    Irt_Col_TabHovered,
    Irt_Col_Tab,
    Irt_Col_TabSelected,
    Irt_Col_TabSelectedOverline,
    Irt_Col_TabDimmed,
    Irt_Col_TabDimmedSelected,
    Irt_Col_TabDimmedSelectedOverline,
    Irt_Col_DockingPreview,
    Irt_Col_DockingEmptyBg,
    Irt_Col_PlotLines,
    Irt_Col_PlotLinesHovered,
    Irt_Col_PlotHistogram,
    Irt_Col_PlotHistogramHovered,
    Irt_Col_TableHeaderBg,
    Irt_Col_TableBorderStrong,
    Irt_Col_TableBorderLight,
    Irt_Col_TableRowBg,
    Irt_Col_TableRowBgAlt,
    Irt_Col_TextLink,
    Irt_Col_TextSelectedBg,
    Irt_Col_DragDropTarget,
    Irt_Col_NavHighlight,
    Irt_Col_NavWindowingHighlight,
    Irt_Col_NavWindowingDimBg,
    Irt_Col_ModalWindowDimBg,
    Irt_Col_COUNT,
};

static std::shared_ptr<variables> do_build_default() noexcept
{
    auto v = std::make_shared<variables>();

    v->g_themes.ids.reserve(16);
    v->g_themes.colors.resize(16);
    v->g_themes.names.resize(16);

    {
        const auto id          = v->g_themes.ids.alloc();
        const auto idx         = get_index(id);
        v->g_themes.names[idx] = "Default";
        auto& colors           = v->g_themes.colors[idx];

        colors[Irt_Col_Text]           = rgba_color(0.90f, 0.90f, 0.90f, 1.00f);
        colors[Irt_Col_TextDisabled]   = rgba_color(0.60f, 0.60f, 0.60f, 1.00f);
        colors[Irt_Col_WindowBg]       = rgba_color(0.00f, 0.00f, 0.00f, 0.85f);
        colors[Irt_Col_ChildBg]        = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_PopupBg]        = rgba_color(0.11f, 0.11f, 0.14f, 0.92f);
        colors[Irt_Col_Border]         = rgba_color(0.50f, 0.50f, 0.50f, 0.50f);
        colors[Irt_Col_BorderShadow]   = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_FrameBg]        = rgba_color(0.43f, 0.43f, 0.43f, 0.39f);
        colors[Irt_Col_FrameBgHovered] = rgba_color(0.47f, 0.47f, 0.69f, 0.40f);
        colors[Irt_Col_FrameBgActive]  = rgba_color(0.42f, 0.41f, 0.64f, 0.69f);
        colors[Irt_Col_TitleBg]        = rgba_color(0.27f, 0.27f, 0.54f, 0.83f);
        colors[Irt_Col_TitleBgActive]  = rgba_color(0.32f, 0.32f, 0.63f, 0.87f);
        colors[Irt_Col_TitleBgCollapsed] =
          rgba_color(0.40f, 0.40f, 0.80f, 0.20f);
        colors[Irt_Col_MenuBarBg]     = rgba_color(0.40f, 0.40f, 0.55f, 0.80f);
        colors[Irt_Col_ScrollbarBg]   = rgba_color(0.20f, 0.25f, 0.30f, 0.60f);
        colors[Irt_Col_ScrollbarGrab] = rgba_color(0.40f, 0.40f, 0.80f, 0.30f);
        colors[Irt_Col_ScrollbarGrabHovered] =
          rgba_color(0.40f, 0.40f, 0.80f, 0.40f);
        colors[Irt_Col_ScrollbarGrabActive] =
          rgba_color(0.41f, 0.39f, 0.80f, 0.60f);
        colors[Irt_Col_CheckMark]  = rgba_color(0.90f, 0.90f, 0.90f, 0.50f);
        colors[Irt_Col_SliderGrab] = rgba_color(1.00f, 1.00f, 1.00f, 0.30f);
        colors[Irt_Col_SliderGrabActive] =
          rgba_color(0.41f, 0.39f, 0.80f, 0.60f);
        colors[Irt_Col_Button]        = rgba_color(0.35f, 0.40f, 0.61f, 0.62f);
        colors[Irt_Col_ButtonHovered] = rgba_color(0.40f, 0.48f, 0.71f, 0.79f);
        colors[Irt_Col_ButtonActive]  = rgba_color(0.46f, 0.54f, 0.80f, 1.00f);
        colors[Irt_Col_Header]        = rgba_color(0.40f, 0.40f, 0.90f, 0.45f);
        colors[Irt_Col_HeaderHovered] = rgba_color(0.45f, 0.45f, 0.90f, 0.80f);
        colors[Irt_Col_HeaderActive]  = rgba_color(0.53f, 0.53f, 0.87f, 0.80f);
        colors[Irt_Col_Separator]     = rgba_color(0.50f, 0.50f, 0.50f, 0.60f);
        colors[Irt_Col_SeparatorHovered] =
          rgba_color(0.60f, 0.60f, 0.70f, 1.00f);
        colors[Irt_Col_SeparatorActive] =
          rgba_color(0.70f, 0.70f, 0.90f, 1.00f);
        colors[Irt_Col_ResizeGrip] = rgba_color(1.00f, 1.00f, 1.00f, 0.10f);
        colors[Irt_Col_ResizeGripHovered] =
          rgba_color(0.78f, 0.82f, 1.00f, 0.60f);
        colors[Irt_Col_ResizeGripActive] =
          rgba_color(0.78f, 0.82f, 1.00f, 0.90f);
        colors[Irt_Col_TabHovered] = colors[Irt_Col_HeaderHovered];
        colors[Irt_Col_Tab] =
          lerp(colors[Irt_Col_Header], colors[Irt_Col_TitleBgActive], 0.80f);
        colors[Irt_Col_TabSelected] = lerp(
          colors[Irt_Col_HeaderActive], colors[Irt_Col_TitleBgActive], 0.60f);
        colors[Irt_Col_TabSelectedOverline] = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TabDimmed] =
          lerp(colors[Irt_Col_Tab], colors[Irt_Col_TitleBg], 0.80f);
        colors[Irt_Col_TabDimmedSelected] =
          lerp(colors[Irt_Col_TabSelected], colors[Irt_Col_TitleBg], 0.40f);
        colors[Irt_Col_TabDimmedSelectedOverline] =
          colors[Irt_Col_HeaderActive];
        colors[Irt_Col_DockingPreview] =
          colors[Irt_Col_Header] * rgba_color(1.0f, 1.0f, 1.0f, 0.7f);
        colors[Irt_Col_DockingEmptyBg] = rgba_color(0.20f, 0.20f, 0.20f, 1.00f);
        colors[Irt_Col_PlotLines]      = rgba_color(1.00f, 1.00f, 1.00f, 1.00f);
        colors[Irt_Col_PlotLinesHovered] =
          rgba_color(0.90f, 0.70f, 0.00f, 1.00f);
        colors[Irt_Col_PlotHistogram] = rgba_color(0.90f, 0.70f, 0.00f, 1.00f);
        colors[Irt_Col_PlotHistogramHovered] =
          rgba_color(1.00f, 0.60f, 0.00f, 1.00f);
        colors[Irt_Col_TableHeaderBg] = rgba_color(0.27f, 0.27f, 0.38f, 1.00f);
        colors[Irt_Col_TableBorderStrong] =
          rgba_color(0.31f, 0.31f, 0.45f, 1.00f);
        colors[Irt_Col_TableBorderLight] =
          rgba_color(0.26f, 0.26f, 0.28f, 1.00f);
        colors[Irt_Col_TableRowBg]     = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_TableRowBgAlt]  = rgba_color(1.00f, 1.00f, 1.00f, 0.07f);
        colors[Irt_Col_TextLink]       = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TextSelectedBg] = rgba_color(0.00f, 0.00f, 1.00f, 0.35f);
        colors[Irt_Col_DragDropTarget] = rgba_color(1.00f, 1.00f, 0.00f, 0.90f);
        colors[Irt_Col_NavHighlight]   = colors[Irt_Col_HeaderHovered];
        colors[Irt_Col_NavWindowingHighlight] =
          rgba_color(1.00f, 1.00f, 1.00f, 0.70f);
        colors[Irt_Col_NavWindowingDimBg] =
          rgba_color(0.80f, 0.80f, 0.80f, 0.20f);
        colors[Irt_Col_ModalWindowDimBg] =
          rgba_color(0.20f, 0.20f, 0.20f, 0.35f);
    }

    {
        const auto id          = v->g_themes.ids.alloc();
        const auto idx         = get_index(id);
        v->g_themes.names[idx] = "Dark";
        auto& colors           = v->g_themes.colors[idx];

        colors[Irt_Col_Text]           = rgba_color(1.00f, 1.00f, 1.00f, 1.00f);
        colors[Irt_Col_TextDisabled]   = rgba_color(0.50f, 0.50f, 0.50f, 1.00f);
        colors[Irt_Col_WindowBg]       = rgba_color(0.06f, 0.06f, 0.06f, 0.94f);
        colors[Irt_Col_ChildBg]        = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_PopupBg]        = rgba_color(0.08f, 0.08f, 0.08f, 0.94f);
        colors[Irt_Col_Border]         = rgba_color(0.43f, 0.43f, 0.50f, 0.50f);
        colors[Irt_Col_BorderShadow]   = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_FrameBg]        = rgba_color(0.16f, 0.29f, 0.48f, 0.54f);
        colors[Irt_Col_FrameBgHovered] = rgba_color(0.26f, 0.59f, 0.98f, 0.40f);
        colors[Irt_Col_FrameBgActive]  = rgba_color(0.26f, 0.59f, 0.98f, 0.67f);
        colors[Irt_Col_TitleBg]        = rgba_color(0.04f, 0.04f, 0.04f, 1.00f);
        colors[Irt_Col_TitleBgActive]  = rgba_color(0.16f, 0.29f, 0.48f, 1.00f);
        colors[Irt_Col_TitleBgCollapsed] =
          rgba_color(0.00f, 0.00f, 0.00f, 0.51f);
        colors[Irt_Col_MenuBarBg]     = rgba_color(0.14f, 0.14f, 0.14f, 1.00f);
        colors[Irt_Col_ScrollbarBg]   = rgba_color(0.02f, 0.02f, 0.02f, 0.53f);
        colors[Irt_Col_ScrollbarGrab] = rgba_color(0.31f, 0.31f, 0.31f, 1.00f);
        colors[Irt_Col_ScrollbarGrabHovered] =
          rgba_color(0.41f, 0.41f, 0.41f, 1.00f);
        colors[Irt_Col_ScrollbarGrabActive] =
          rgba_color(0.51f, 0.51f, 0.51f, 1.00f);
        colors[Irt_Col_CheckMark]  = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_SliderGrab] = rgba_color(0.24f, 0.52f, 0.88f, 1.00f);
        colors[Irt_Col_SliderGrabActive] =
          rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_Button]        = rgba_color(0.26f, 0.59f, 0.98f, 0.40f);
        colors[Irt_Col_ButtonHovered] = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_ButtonActive]  = rgba_color(0.06f, 0.53f, 0.98f, 1.00f);
        colors[Irt_Col_Header]        = rgba_color(0.26f, 0.59f, 0.98f, 0.31f);
        colors[Irt_Col_HeaderHovered] = rgba_color(0.26f, 0.59f, 0.98f, 0.80f);
        colors[Irt_Col_HeaderActive]  = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_Separator]     = colors[Irt_Col_Border];
        colors[Irt_Col_SeparatorHovered] =
          rgba_color(0.10f, 0.40f, 0.75f, 0.78f);
        colors[Irt_Col_SeparatorActive] =
          rgba_color(0.10f, 0.40f, 0.75f, 1.00f);
        colors[Irt_Col_ResizeGrip] = rgba_color(0.26f, 0.59f, 0.98f, 0.20f);
        colors[Irt_Col_ResizeGripHovered] =
          rgba_color(0.26f, 0.59f, 0.98f, 0.67f);
        colors[Irt_Col_ResizeGripActive] =
          rgba_color(0.26f, 0.59f, 0.98f, 0.95f);
        colors[Irt_Col_TabHovered] = colors[Irt_Col_HeaderHovered];
        colors[Irt_Col_Tab] =
          lerp(colors[Irt_Col_Header], colors[Irt_Col_TitleBgActive], 0.80f);
        colors[Irt_Col_TabSelected] = lerp(
          colors[Irt_Col_HeaderActive], colors[Irt_Col_TitleBgActive], 0.60f);
        colors[Irt_Col_TabSelectedOverline] = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TabDimmed] =
          lerp(colors[Irt_Col_Tab], colors[Irt_Col_TitleBg], 0.80f);
        colors[Irt_Col_TabDimmedSelected] =
          lerp(colors[Irt_Col_TabSelected], colors[Irt_Col_TitleBg], 0.40f);
        colors[Irt_Col_TabDimmedSelectedOverline] =
          rgba_color(0.50f, 0.50f, 0.50f, 1.00f);
        colors[Irt_Col_DockingPreview] =
          colors[Irt_Col_HeaderActive] * rgba_color(1.0f, 1.0f, 1.0f, 0.7f);
        colors[Irt_Col_DockingEmptyBg] = rgba_color(0.20f, 0.20f, 0.20f, 1.00f);
        colors[Irt_Col_PlotLines]      = rgba_color(0.61f, 0.61f, 0.61f, 1.00f);
        colors[Irt_Col_PlotLinesHovered] =
          rgba_color(1.00f, 0.43f, 0.35f, 1.00f);
        colors[Irt_Col_PlotHistogram] = rgba_color(0.90f, 0.70f, 0.00f, 1.00f);
        colors[Irt_Col_PlotHistogramHovered] =
          rgba_color(1.00f, 0.60f, 0.00f, 1.00f);
        colors[Irt_Col_TableHeaderBg] = rgba_color(0.19f, 0.19f, 0.20f, 1.00f);
        colors[Irt_Col_TableBorderStrong] =
          rgba_color(0.31f, 0.31f, 0.35f, 1.00f);
        colors[Irt_Col_TableBorderLight] =
          rgba_color(0.23f, 0.23f, 0.25f, 1.00f);
        colors[Irt_Col_TableRowBg]     = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_TableRowBgAlt]  = rgba_color(1.00f, 1.00f, 1.00f, 0.06f);
        colors[Irt_Col_TextLink]       = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TextSelectedBg] = rgba_color(0.26f, 0.59f, 0.98f, 0.35f);
        colors[Irt_Col_DragDropTarget] = rgba_color(1.00f, 1.00f, 0.00f, 0.90f);
        colors[Irt_Col_NavHighlight]   = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_NavWindowingHighlight] =
          rgba_color(1.00f, 1.00f, 1.00f, 0.70f);
        colors[Irt_Col_NavWindowingDimBg] =
          rgba_color(0.80f, 0.80f, 0.80f, 0.20f);
        colors[Irt_Col_ModalWindowDimBg] =
          rgba_color(0.80f, 0.80f, 0.80f, 0.35f);

        v->g_themes.selected = id;
    }

    {
        const auto id          = v->g_themes.ids.alloc();
        const auto idx         = get_index(id);
        v->g_themes.names[idx] = "Light";
        auto& colors           = v->g_themes.colors[idx];

        colors[Irt_Col_Text]           = rgba_color(0.00f, 0.00f, 0.00f, 1.00f);
        colors[Irt_Col_TextDisabled]   = rgba_color(0.60f, 0.60f, 0.60f, 1.00f);
        colors[Irt_Col_WindowBg]       = rgba_color(0.94f, 0.94f, 0.94f, 1.00f);
        colors[Irt_Col_ChildBg]        = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_PopupBg]        = rgba_color(1.00f, 1.00f, 1.00f, 0.98f);
        colors[Irt_Col_Border]         = rgba_color(0.00f, 0.00f, 0.00f, 0.30f);
        colors[Irt_Col_BorderShadow]   = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_FrameBg]        = rgba_color(1.00f, 1.00f, 1.00f, 1.00f);
        colors[Irt_Col_FrameBgHovered] = rgba_color(0.26f, 0.59f, 0.98f, 0.40f);
        colors[Irt_Col_FrameBgActive]  = rgba_color(0.26f, 0.59f, 0.98f, 0.67f);
        colors[Irt_Col_TitleBg]        = rgba_color(0.96f, 0.96f, 0.96f, 1.00f);
        colors[Irt_Col_TitleBgActive]  = rgba_color(0.82f, 0.82f, 0.82f, 1.00f);
        colors[Irt_Col_TitleBgCollapsed] =
          rgba_color(1.00f, 1.00f, 1.00f, 0.51f);
        colors[Irt_Col_MenuBarBg]     = rgba_color(0.86f, 0.86f, 0.86f, 1.00f);
        colors[Irt_Col_ScrollbarBg]   = rgba_color(0.98f, 0.98f, 0.98f, 0.53f);
        colors[Irt_Col_ScrollbarGrab] = rgba_color(0.69f, 0.69f, 0.69f, 0.80f);
        colors[Irt_Col_ScrollbarGrabHovered] =
          rgba_color(0.49f, 0.49f, 0.49f, 0.80f);
        colors[Irt_Col_ScrollbarGrabActive] =
          rgba_color(0.49f, 0.49f, 0.49f, 1.00f);
        colors[Irt_Col_CheckMark]  = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_SliderGrab] = rgba_color(0.26f, 0.59f, 0.98f, 0.78f);
        colors[Irt_Col_SliderGrabActive] =
          rgba_color(0.46f, 0.54f, 0.80f, 0.60f);
        colors[Irt_Col_Button]        = rgba_color(0.26f, 0.59f, 0.98f, 0.40f);
        colors[Irt_Col_ButtonHovered] = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_ButtonActive]  = rgba_color(0.06f, 0.53f, 0.98f, 1.00f);
        colors[Irt_Col_Header]        = rgba_color(0.26f, 0.59f, 0.98f, 0.31f);
        colors[Irt_Col_HeaderHovered] = rgba_color(0.26f, 0.59f, 0.98f, 0.80f);
        colors[Irt_Col_HeaderActive]  = rgba_color(0.26f, 0.59f, 0.98f, 1.00f);
        colors[Irt_Col_Separator]     = rgba_color(0.39f, 0.39f, 0.39f, 0.62f);
        colors[Irt_Col_SeparatorHovered] =
          rgba_color(0.14f, 0.44f, 0.80f, 0.78f);
        colors[Irt_Col_SeparatorActive] =
          rgba_color(0.14f, 0.44f, 0.80f, 1.00f);
        colors[Irt_Col_ResizeGrip] = rgba_color(0.35f, 0.35f, 0.35f, 0.17f);
        colors[Irt_Col_ResizeGripHovered] =
          rgba_color(0.26f, 0.59f, 0.98f, 0.67f);
        colors[Irt_Col_ResizeGripActive] =
          rgba_color(0.26f, 0.59f, 0.98f, 0.95f);
        colors[Irt_Col_TabHovered] = colors[Irt_Col_HeaderHovered];
        colors[Irt_Col_Tab] =
          lerp(colors[Irt_Col_Header], colors[Irt_Col_TitleBgActive], 0.90f);
        colors[Irt_Col_TabSelected] = lerp(
          colors[Irt_Col_HeaderActive], colors[Irt_Col_TitleBgActive], 0.60f);
        colors[Irt_Col_TabSelectedOverline] = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TabDimmed] =
          lerp(colors[Irt_Col_Tab], colors[Irt_Col_TitleBg], 0.80f);
        colors[Irt_Col_TabDimmedSelected] =
          lerp(colors[Irt_Col_TabSelected], colors[Irt_Col_TitleBg], 0.40f);
        colors[Irt_Col_TabDimmedSelectedOverline] =
          rgba_color(0.26f, 0.59f, 1.00f, 1.00f);
        colors[Irt_Col_DockingPreview] =
          colors[Irt_Col_Header] * rgba_color(1.0f, 1.0f, 1.0f, 0.7f);
        colors[Irt_Col_DockingEmptyBg] = rgba_color(0.20f, 0.20f, 0.20f, 1.00f);
        colors[Irt_Col_PlotLines]      = rgba_color(0.39f, 0.39f, 0.39f, 1.00f);
        colors[Irt_Col_PlotLinesHovered] =
          rgba_color(1.00f, 0.43f, 0.35f, 1.00f);
        colors[Irt_Col_PlotHistogram] = rgba_color(0.90f, 0.70f, 0.00f, 1.00f);
        colors[Irt_Col_PlotHistogramHovered] =
          rgba_color(1.00f, 0.45f, 0.00f, 1.00f);
        colors[Irt_Col_TableHeaderBg] = rgba_color(0.78f, 0.87f, 0.98f, 1.00f);
        colors[Irt_Col_TableBorderStrong] =
          rgba_color(0.57f, 0.57f, 0.64f, 1.00f); // Prefer using Alpha=1.0 here
        colors[Irt_Col_TableBorderLight] =
          rgba_color(0.68f, 0.68f, 0.74f, 1.00f); // Prefer using Alpha=1.0 here
        colors[Irt_Col_TableRowBg]     = rgba_color(0.00f, 0.00f, 0.00f, 0.00f);
        colors[Irt_Col_TableRowBgAlt]  = rgba_color(0.30f, 0.30f, 0.30f, 0.09f);
        colors[Irt_Col_TextLink]       = colors[Irt_Col_HeaderActive];
        colors[Irt_Col_TextSelectedBg] = rgba_color(0.26f, 0.59f, 0.98f, 0.35f);
        colors[Irt_Col_DragDropTarget] = rgba_color(0.26f, 0.59f, 0.98f, 0.95f);
        colors[Irt_Col_NavHighlight]   = colors[Irt_Col_HeaderHovered];
        colors[Irt_Col_NavWindowingHighlight] =
          rgba_color(0.70f, 0.70f, 0.70f, 0.70f);
        colors[Irt_Col_NavWindowingDimBg] =
          rgba_color(0.20f, 0.20f, 0.20f, 0.20f);
        colors[Irt_Col_ModalWindowDimBg] =
          rgba_color(0.20f, 0.20f, 0.20f, 0.35f);
    }

    return v;
}

static std::error_code do_write(const variables& vars,
                                std::ostream&    os) noexcept
{
    os << "[paths]\n";

    for (const auto id : vars.rec_paths.ids) {
        const auto idx = get_index(id);

        os << vars.rec_paths.names[idx].sv() << ' '
           << vars.rec_paths.priorities[idx] << ' '
           << vars.rec_paths.paths[idx].sv() << '\n';
    }

    os << "[themes]\n";

    // Classic, Dark and Light style can not be free. We avoid to read/write it.
    if (vars.g_themes.ids.size() > 3) {
        const gui_theme_id* id = nullptr;
        vars.g_themes.ids.next(id);
        vars.g_themes.ids.next(id);
        vars.g_themes.ids.next(id);

        while (vars.g_themes.ids.next(id)) {
            const auto idx = get_index(*id);

            os << vars.g_themes.names[idx].sv() << ' ';
            for (const auto elem : vars.g_themes.colors[idx]) {
                const auto val =
                  elem.r << 24 | elem.g << 16 | elem.b << 8 | elem.a;
                fmt::print(os, " #%08x", val);
            }
            os << '\n';
        }
    }

    if (vars.g_themes.ids.get(vars.g_themes.selected)) {
        const auto idx = get_index(vars.g_themes.selected);
        os << "selected=" << vars.g_themes.names[idx].sv() << '\n';
    }

    if (os.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return std::error_code();
}

static std::error_code do_save(const char*      filename,
                               const variables& vars) noexcept
{
    auto file = std::ofstream{ filename };
    if (!file.is_open())
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    file << "# irritator 0.x\n;\n";
    if (file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, file);
}

enum { section_colors, section_paths, section_COUNT };

static bool do_read_section(variables& /*vars*/,
                            std::bitset<2>&  current_section,
                            std::string_view section) noexcept
{
    struct map {
        std::string_view name;
        int              section;
    };

    static const map m[] = { { "colors", section_colors },
                             { "paths", section_paths } };

    auto it = sorted_vector_find(
      std::begin(m), std::end(m), section, [](auto a, auto b) noexcept {
          if constexpr (std::is_same_v<decltype(a), std::string_view>)
              return a < b.name;
          else
              return a.name < b;
      });

    if (it == std::end(m))
        return false;

    current_section.reset();
    current_section.set(it->section);
    return true;
}

static bool do_read_affect(variables&       vars,
                           std::bitset<2>&  current_section,
                           std::string_view key,
                           std::string_view val) noexcept
{
    if (current_section.test(section_colors) and key == "themes") {
        for (const auto id : vars.g_themes.ids) {
            const auto idx = get_index(id);

            if (vars.g_themes.names[idx] == val) {
                vars.g_themes.selected = id;
                return true;
            }
        }

        if (vars.g_themes.ids.ssize() != 0)
            vars.g_themes.selected = *(vars.g_themes.ids.begin());
    }

    return false;
}

std::istream& operator>>(std::istream& is, rgba_color& color) noexcept
{
    char c = 0;
    while (is.get(c))
        if (c == '#')
            break;

    int value = 0;
    if (is and is >> value) {
        if (value > 0xffffff) {
            color.r = value >> 24;
            color.g = value >> 16 & 0xff;
            color.b = value >> 8 & 0xff;
            color.a = value & 0xff;
        } else {
            color.r = value >> 16;
            color.g = value >> 8 & 0xff;
            color.b = value & 0xff;
            color.a = 0u;
        }
    }

    return is;
}

static bool do_read_elem(variables&       vars,
                         std::bitset<2>&  current_section,
                         std::string_view element) noexcept
{
    if (current_section.test(section_colors)) {
        if (not vars.g_themes.ids.can_alloc(1))
            return false;

        const auto id  = vars.g_themes.ids.alloc();
        const auto idx = get_index(id);

        std::fill_n(vars.g_themes.colors[idx].data(),
                    vars.g_themes.colors[idx].size(),
                    rgba_color{ 255, 255, 255, 255 });

        vars.g_themes.names[idx].clear();

        imemstream in(element.data(), element.size());

        if (not(in >> vars.g_themes.names[idx]))
            return false;

        small_string<32> str;

        for (std::size_t i = 0, e = std::size(vars.g_themes.colors); i != e;
             ++i) {
            if (not(in >> vars.g_themes.colors[idx][i]))
                return false;
        }

        return true;
    }

    if (current_section.test(section_paths)) {
        if (not vars.rec_paths.ids.can_alloc(1))
            return false;

        const auto id  = vars.rec_paths.ids.alloc();
        const auto idx = get_index(id);

        vars.rec_paths.priorities[idx] = 0;
        vars.rec_paths.names[idx].clear();
        vars.rec_paths.paths[idx].clear();

        imemstream in(element.data(), element.size());
        in >> vars.rec_paths.priorities[idx] >> vars.rec_paths.names[idx] >>
          vars.rec_paths.paths[idx];

        return in.good();
    }

    return false;
}

static std::error_code do_parse(std::shared_ptr<variables>& v,
                                std::string_view            buffer) noexcept
{
    std::bitset<2> s;

    auto pos = buffer.find_first_of(";#[=\n");
    if (pos != std::string_view::npos) {

        if (buffer[pos] == '#' or buffer[pos] == ';') { // A comment
            if (pos + 1u >= buffer.size()) {
                if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            auto new_line = buffer.find('\n', pos + 1);
            if (new_line == std::string_view::npos or
                new_line + 1u >= buffer.size()) {
                if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                return std::make_error_code(std::errc::argument_out_of_domain);
            buffer = buffer.substr(new_line);
        } else if (buffer[pos] == '[') { // Read a new section
            if (pos + 1u >= buffer.size())
                return std::make_error_code(std::errc::io_error);

            const auto end_section = buffer.find(']', pos + 1u);
            if (end_section == std::string_view::npos)
                return std::make_error_code(std::errc::io_error);

            auto sec = buffer.substr(pos + 1u, end_section - pos + 1u);
            if (not do_read_section(*v, s, sec))
                return std::make_error_code(std::errc::argument_out_of_domain);
            buffer = buffer.substr(end_section + 1u);

        } else if (buffer[pos] == '=') { // Read affectation
            if (pos + 1u >= buffer.size()) {
                if (not do_read_affect(
                      *v, s, buffer.substr(0, pos), std::string_view()))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            auto new_line = buffer.find('\n', pos + 1u);
            if (new_line == std::string_view::npos or
                new_line + 1 >= buffer.size()) {
                if (not do_read_affect(
                      *v, s, buffer.substr(0, pos), buffer.substr(pos + 1u)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            if (not do_read_affect(*v,
                                   s,
                                   buffer.substr(0, pos),
                                   buffer.substr(pos + 1u, new_line)))
                return std::make_error_code(std::errc::argument_out_of_domain);

            buffer = buffer.substr(new_line + 1u);

        } else if (buffer[pos] == '\n') { // Read elemet in list
            if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                return std::make_error_code(std::errc::argument_out_of_domain);

            if (pos + 1u >= buffer.size())
                return std::error_code();

            buffer = buffer.substr(pos + 1u);
        }
    }

    return std::error_code();
}

static std::error_code do_load(const char*                 filename,
                               std::shared_ptr<variables>& vars) noexcept
{
    auto file = std::ifstream{ filename };
    if (not file.is_open())
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    auto latest =
      std::string{ (std::istreambuf_iterator<std::string::value_type>(file)),
                   std::istreambuf_iterator<std::string::value_type>() };

    if (latest.empty() or file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    file.close();

    return do_parse(vars, latest);
}

config_manager::config_manager() noexcept
  : m_vars{ do_build_default() }
{}

config_manager::config_manager(const std::string config_path) noexcept
  : m_path{ config_path }
  , m_vars{ do_build_default() }
{
    (void)do_load(config_path.c_str(), m_vars);
}

std::error_code config_manager::save() const noexcept
{
    std::shared_lock lock(m_mutex);

    return do_save(m_path.c_str(), *m_vars);
}

std::error_code config_manager::load() noexcept
{
    auto ret             = std::make_shared<variables>();
    auto is_load_success = do_load(m_path.c_str(), ret);
    if (not is_load_success)
        return is_load_success;

    std::unique_lock lock(m_mutex);
    std::swap(ret, m_vars);

    return std::error_code();
}

void config_manager::swap(std::shared_ptr<variables>& other) noexcept
{
    std::unique_lock lock(m_mutex);

    std::swap(m_vars, other);
}

std::shared_ptr<variables> config_manager::copy() const noexcept
{
    std::shared_lock lock(m_mutex);

    return std::make_shared<variables>(*m_vars.get());
}

} // namespace irt
