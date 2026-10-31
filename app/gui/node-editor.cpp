// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "node-editor.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace irt::node_editor {

bool editor::begin_canvas(const char* str_id, const u32 node_count) noexcept
{
    if (not ImGui::BeginChild(str_id, ImVec2(0.f, 0.f), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse |
                                ImGuiWindowFlags_NoMove)) {
        ImGui::EndChild();
        return false;
    }

    const auto& style     = ImGui::GetStyle();
    m_layout.title_height = ImGui::GetFrameHeight();
    m_layout.row_height   = ImGui::GetTextLineHeightWithSpacing();
    m_layout.padding      = style.ItemSpacing.x;
    m_layout.pin_radius   = 4.f;
    m_layout.content_width = 12.f * ImGui::GetFontSize();

    const auto origin = ImGui::GetCursorScreenPos();
    const auto avail  = ImGui::GetContentRegionAvail();
    const auto size = ImVec2(std::max(avail.x, 50.f), std::max(avail.y, 50.f));
    m_canvas        = rect<ImVec2>{ origin,
                                    ImVec2(origin.x + size.x, origin.y + size.y) };

    // The button captures the clicks on the empty canvas: without it a click
    // drags the parent window. The widgets of the nodes, submitted after,
    // take the hover over it.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight |
                             ImGuiButtonFlags_MouseButtonMiddle);
    m_mouse_enabled = ImGui::IsItemHovered() or ImGui::IsItemActive();

    const auto& io = ImGui::GetIO();
    if (ImGui::IsItemActive() and ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        m_camera.scrolling.x += io.MouseDelta.x;
        m_camera.scrolling.y += io.MouseDelta.y;
    }

    if (ImGui::IsItemHovered() and io.MouseWheel != 0.f) {
        const auto zoom = std::clamp(
          m_camera.zoom * (1.f + io.MouseWheel * 0.1f), 0.05f, 1.f);
        m_camera.zoom_at(m_canvas.min, io.MousePos, zoom);
    }

    if (m_cache.size() != node_count) {
        m_cache.resize(node_count);
        m_node_selected.resize(node_count, u8{ 0 });
    }

    m_hovered_node.reset();
    m_hovered_input.reset();
    m_hovered_output.reset();

    return true;
}

void editor::begin_content(const u32    node,
                           const ImVec2 pos,
                           const float  limit) noexcept
{
    auto* window = ImGui::GetCurrentWindow();

    m_saved_work_max_x    = window->WorkRect.Max.x;
    m_saved_content_max_x = window->ContentRegionRect.Max.x;

    window->WorkRect.Max.x          = pos.x + limit;
    window->ContentRegionRect.Max.x = pos.x + limit;

    ImGui::SetCursorScreenPos(pos);

    // EndGroup() includes the rectangle of the last item in the group bounds:
    // without an item in the content, a node would take the size of the
    // previous node or of the canvas.
    ImGui::GetCurrentContext()->LastItemData.Rect = ImRect(pos, pos);

    ImGui::PushID(static_cast<int>(node));
    ImGui::PushItemWidth(limit * 0.65f);
    ImGui::BeginGroup();
}

ImVec2 editor::end_content() noexcept
{
    ImGui::EndGroup();
    const auto size = ImGui::GetItemRectSize();

    ImGui::PopItemWidth();
    ImGui::PopID();

    auto* window                    = ImGui::GetCurrentWindow();
    window->WorkRect.Max.x          = m_saved_work_max_x;
    window->ContentRegionRect.Max.x = m_saved_content_max_x;

    return size;
}

void editor::draw_grid() const noexcept
{
    auto*      dl   = ImGui::GetWindowDrawList();
    const auto step = 64.f * m_camera.zoom;

    dl->AddRectFilled(m_canvas.min, m_canvas.max,
                      ImGui::GetColorU32(ImGuiCol_FrameBg));

    // Too dense lines cost without helping.
    if (step < 8.f)
        return;

    const auto col = ImGui::GetColorU32(ImGuiCol_Border, 0.5f);
    for (auto x = std::fmod(m_camera.scrolling.x, step);
         x < m_canvas.max.x - m_canvas.min.x; x += step)
        dl->AddLine(ImVec2(m_canvas.min.x + x, m_canvas.min.y),
                    ImVec2(m_canvas.min.x + x, m_canvas.max.y), col);

    for (auto y = std::fmod(m_camera.scrolling.y, step);
         y < m_canvas.max.y - m_canvas.min.y; y += step)
        dl->AddLine(ImVec2(m_canvas.min.x, m_canvas.min.y + y),
                    ImVec2(m_canvas.max.x, m_canvas.min.y + y), col);
}

void editor::draw_link(const bezier<ImVec2>& b,
                       const bool            selected) const noexcept
{
    ImGui::GetWindowDrawList()->AddBezierCubic(
      b.p0, b.p1, b.p2, b.p3,
      selected ? ImGui::GetColorU32(ImGuiCol_PlotLinesHovered)
               : ImGui::GetColorU32(ImGuiCol_Text, 0.6f),
      selected ? 3.f : 1.5f);
}

void editor::end_canvas(events& ev) noexcept
{
    const auto has_selection = not m_selected_nodes.empty() or
                               not m_selected_links.empty();

    if (has_selection and
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) and
        not ImGui::GetIO().WantTextInput) {
        ev.delete_selection    = ImGui::IsKeyReleased(ImGuiKey_Delete);
        ev.duplicate_selection = ImGui::IsKeyReleased(ImGuiKey_D);
    }

    if (ImGui::BeginPopup(item_menu_id)) {
        if (ImGui::MenuItem("Delete", "Del"))
            ev.delete_selection = true;
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

ImVec2 editor::node_size(const u32 node,
                         const u32 inputs,
                         const u32 outputs) const noexcept
{
    const auto& c = m_cache[node];

    return ImVec2(c.width,
                  m_layout.title_height +
                    static_cast<float>(inputs + outputs) * m_layout.row_height +
                    c.content_height + m_layout.padding);
}

ImVec2 editor::input_pin(const ImVec2 node_pos, const u32 port) const noexcept
{
    return ImVec2(node_pos.x, node_pos.y + (m_layout.title_height +
                                            (static_cast<float>(port) + 0.5f) *
                                              m_layout.row_height) *
                                             m_camera.zoom);
}

ImVec2 editor::output_pin(const ImVec2 node_pos,
                          const u32    node,
                          const u32    inputs,
                          const u32    port) const noexcept
{
    const auto& c = m_cache[node];

    return ImVec2(node_pos.x + c.width * m_camera.zoom,
                  node_pos.y +
                    (m_layout.title_height +
                     static_cast<float>(inputs) * m_layout.row_height +
                     c.content_height +
                     (static_cast<float>(port) + 0.5f) * m_layout.row_height) *
                      m_camera.zoom);
}

void editor::select_node(const u32 node, const bool add) noexcept
{
    if (not add)
        clear_selection();

    if (is_selected(node)) {
        m_node_selected[node] = 0;
        if (auto it = std::ranges::find(m_selected_nodes, node);
            it != m_selected_nodes.end())
            m_selected_nodes.erase(it);
    } else if (node < m_node_selected.size()) {
        m_node_selected[node] = 1;
        m_selected_nodes.push_back(node);
    }
}

void editor::select_link(const u32 link, const bool add) noexcept
{
    if (not add)
        clear_selection();

    if (auto it = std::ranges::find(m_selected_links, link);
        it != m_selected_links.end())
        m_selected_links.erase(it);
    else
        m_selected_links.push_back(link);
}

void editor::clear_selection() noexcept
{
    for (const auto node : m_selected_nodes)
        if (node < m_node_selected.size())
            m_node_selected[node] = 0;

    m_selected_nodes.clear();
    m_selected_links.clear();
}

void editor::reset() noexcept
{
    clear_selection();
    m_cache.clear();
    m_node_selected.clear();
    m_drag = drag_type::none;
}

} // namespace irt::node_editor
