// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2026
#define ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_2026

#include <irritator/container.hpp>

#include "node-editor-geometry.hpp"

#include <imgui.h>

#include <optional>
#include <span>
#include <string_view>

namespace irt::node_editor {

/// A port of a node: @c node is the index of the node in the adapter, @c port
/// the index of the input or output port of this node.
struct pin {
    u32 node = 0;
    u32 port = 0;

    constexpr bool operator==(const pin&) const noexcept = default;
};

struct link_request {
    pin out;
    pin in;
};

/// What the user asked during the frame. The editor never changes the graph:
/// the caller applies these requests to its own data.
struct events {
    std::optional<link_request> link_created;
    bool                        delete_selection    = false;
    bool                        duplicate_selection = false;
    bool                        nodes_moved         = false;
};

/**
 * The data shown by the @c editor. The nodes are identified by their index in
 * [0, node_count()[ and the links by their order in @c for_each_link().
 *
 * @c content() draws the optional ImGui widgets of a node; it is called only
 * for the visible nodes at full zoom. An optional @c title_color(node,
 * selected) returning an @c ImU32 overrides the color of the title bar.
 */
template<typename A>
concept graph_adapter = requires(A         a,
                                 const u32 node,
                                 void (*fn)(u32, u32, u32, u32) noexcept) {
    { a.node_count() } -> std::convertible_to<u32>;
    { a.position(node) } -> std::same_as<ImVec2&>;
    { a.title(node) } -> std::convertible_to<std::string_view>;
    { a.input_count(node) } -> std::convertible_to<u32>;
    { a.output_count(node) } -> std::convertible_to<u32>;
    { a.input_name(node, node) } -> std::convertible_to<std::string_view>;
    { a.output_name(node, node) } -> std::convertible_to<std::string_view>;
    a.content(node);
    a.for_each_link(fn);
};

/**
 * A DEVS graph editor drawn directly in the ImGui draw list. The cost of a
 * frame depends on the visible nodes and links: the others are only tested
 * against the viewport. Under a zoom of one, the nodes are drawn without
 * their widgets.
 */
class editor
{
public:
    template<graph_adapter A>
    events show(const char* str_id, A& adapter) noexcept;

    std::span<const u32> selected_nodes() const noexcept
    {
        return std::span(m_selected_nodes.data(), m_selected_nodes.size());
    }

    std::span<const u32> selected_links() const noexcept
    {
        return std::span(m_selected_links.data(), m_selected_links.size());
    }

    void clear_selection() noexcept;

    /// Adds @c node to the selection.
    void select(const u32 node) noexcept
    {
        if (not is_selected(node))
            select_node(node, true);
    }

    /// To call when the nodes of the adapter are rebuilt: the cached sizes and
    /// the selection refer to the previous indices.
    void reset() noexcept;

    /// Centers and zooms the camera on all the nodes at the next frame.
    void request_fit() noexcept { m_fit_requested = true; }

    /// True when the mouse is over the canvas but not over a node, a link or
    /// a widget: the place to open the context menu of the caller.
    bool is_background_hovered() const noexcept { return m_background_hovered; }

    /// The world position of a screen position, for example the mouse
    /// position when a context menu opens.
    ImVec2 to_world(const ImVec2 screen) const noexcept
    {
        return m_camera.to_world(m_canvas.min, screen);
    }

    bool show_minimap = true;

private:
    struct node_cache {
        float width          = 160.f;
        float content_height = 0.f;
    };

    struct layout {
        float title_height  = 0.f;
        float row_height    = 0.f;
        float padding       = 0.f;
        float pin_radius    = 0.f;
        float content_width = 0.f;
    };

    enum class drag_type : u8 { none, nodes, box, link };

    static constexpr const char* item_menu_id = "##node-editor-item-menu";

    bool begin_canvas(const char* str_id, u32 node_count) noexcept;
    void draw_grid() const noexcept;
    void draw_link(const bezier<ImVec2>& b, bool selected) const noexcept;
    void end_canvas(events& ev) noexcept;

    /// Lays out the widgets of a node in a region of width @c limit: the
    /// widgets sized from the window, the default width or a negative width,
    /// must not depend on the canvas size or on the node position.
    void   begin_content(u32 node, ImVec2 pos, float limit) noexcept;
    ImVec2 end_content() noexcept;

    ImVec2 node_size(u32 node, u32 inputs, u32 outputs) const noexcept;
    ImVec2 input_pin(ImVec2 node_pos, u32 port) const noexcept;
    ImVec2 output_pin(ImVec2 node_pos,
                      u32    node,
                      u32    inputs,
                      u32    port) const noexcept;

    bool is_selected(const u32 node) const noexcept
    {
        return node < m_node_selected.size() and m_node_selected[node] != 0;
    }

    void select_node(u32 node, bool add) noexcept;
    void select_link(u32 link, bool add) noexcept;

    template<graph_adapter A>
    void fit(A& adapter) noexcept;

    template<graph_adapter A>
    void draw_links(A& adapter) noexcept;

    template<graph_adapter A>
    void draw_node(A& adapter, u32 node) noexcept;

    template<graph_adapter A>
    void handle_mouse(A& adapter, events& ev) noexcept;

    template<graph_adapter A>
    void draw_minimap(A& adapter) noexcept;

    vector<node_cache> m_cache;
    vector<u8>         m_node_selected;
    vector<u32>        m_selected_nodes;
    vector<u32>        m_selected_links;
    vector<u32>        m_visible;

    camera<ImVec2> m_camera;
    rect<ImVec2>   m_canvas;
    layout         m_layout;

    std::optional<u32> m_hovered_node;
    std::optional<u32> m_hovered_link;
    std::optional<pin> m_hovered_input;
    std::optional<pin> m_hovered_output;
    float              m_hovered_link_distance = 0.f;

    drag_type m_drag = drag_type::none;
    pin       m_drag_pin;
    bool      m_drag_from_output = false;
    bool      m_drag_moved       = false;
    ImVec2    m_drag_start;

    float m_saved_work_max_x    = 0.f;
    float m_saved_content_max_x = 0.f;

    bool m_fit_requested      = true;
    bool m_background_hovered = false;
    bool m_minimap_hovered    = false;
    bool m_mouse_enabled      = false;
};

template<graph_adapter A>
events editor::show(const char* str_id, A& adapter) noexcept
{
    events ev;

    if (begin_canvas(str_id, adapter.node_count())) {
        if (m_fit_requested) {
            fit(adapter);
            m_fit_requested = false;
        }

        draw_grid();
        draw_links(adapter);

        m_visible.clear();
        const auto viewport = m_canvas;
        for (u32 i = 0, e = adapter.node_count(); i != e; ++i) {
            const auto p    = m_camera.to_screen(m_canvas.min,
                                                 adapter.position(i));
            const auto size = node_size(i, adapter.input_count(i),
                                        adapter.output_count(i));
            const auto r    = rect<ImVec2>{ p,
                                            ImVec2(p.x + size.x * m_camera.zoom,
                                                   p.y + size.y * m_camera.zoom) };

            if (overlaps(r, viewport))
                m_visible.push_back(i);
        }

        // The selected nodes are drawn last to stay over the others.
        for (const auto i : m_visible)
            if (not is_selected(i))
                draw_node(adapter, i);

        for (const auto i : m_visible)
            if (is_selected(i))
                draw_node(adapter, i);

        if (show_minimap)
            draw_minimap(adapter);

        handle_mouse(adapter, ev);
        end_canvas(ev);
    }

    return ev;
}

template<graph_adapter A>
void editor::fit(A& adapter) noexcept
{
    const auto n = adapter.node_count();
    if (n == 0)
        return;

    auto r = rect<ImVec2>{ adapter.position(0), adapter.position(0) };
    for (u32 i = 0; i != n; ++i) {
        const auto p = adapter.position(i);
        const auto s = node_size(i, adapter.input_count(i),
                                 adapter.output_count(i));

        r.min.x = std::min(r.min.x, p.x);
        r.min.y = std::min(r.min.y, p.y);
        r.max.x = std::max(r.max.x, p.x + s.x);
        r.max.y = std::max(r.max.y, p.y + s.y);
    }

    const auto w        = std::max(r.max.x - r.min.x, 1.f);
    const auto h        = std::max(r.max.y - r.min.y, 1.f);
    const auto canvas_w = m_canvas.max.x - m_canvas.min.x;
    const auto canvas_h = m_canvas.max.y - m_canvas.min.y;

    m_camera.zoom = std::clamp(std::min(canvas_w / w, canvas_h / h) * 0.9f,
                               0.05f, 1.f);
    m_camera.scrolling.x = canvas_w * 0.5f -
                           (r.min.x + w * 0.5f) * m_camera.zoom;
    m_camera.scrolling.y = canvas_h * 0.5f -
                           (r.min.y + h * 0.5f) * m_camera.zoom;
}

template<graph_adapter A>
void editor::draw_links(A& adapter) noexcept
{
    const auto mouse   = ImGui::GetIO().MousePos;
    const auto tangent = 50.f * m_camera.zoom;
    u32        link    = 0;

    m_hovered_link.reset();
    m_hovered_link_distance = 6.f;

    adapter.for_each_link([&](const u32 out_node, const u32 out_port,
                              const u32 in_node, const u32 in_port) noexcept {
        const auto index = link++;
        const auto from  = output_pin(
          m_camera.to_screen(m_canvas.min, adapter.position(out_node)),
          out_node, adapter.input_count(out_node), out_port);
        const auto to = input_pin(
          m_camera.to_screen(m_canvas.min, adapter.position(in_node)), in_port);
        const auto b = make_link_bezier(from, to, tangent);

        if (not overlaps(bounds(b), m_canvas))
            return;

        const auto selected = std::ranges::find(m_selected_links, index) !=
                              m_selected_links.end();
        draw_link(b, selected);

        if (m_mouse_enabled and contains(bounds(b), mouse)) {
            if (const auto d = distance_to(b, mouse);
                d < m_hovered_link_distance) {
                m_hovered_link_distance = d;
                m_hovered_link          = index;
            }
        }
    });
}

template<graph_adapter A>
void editor::draw_node(A& adapter, const u32 node) noexcept
{
    auto*      dl      = ImGui::GetWindowDrawList();
    const auto inputs  = adapter.input_count(node);
    const auto outputs = adapter.output_count(node);
    const auto zoom    = m_camera.zoom;
    const auto full    = zoom >= 0.99f;
    const auto p = m_camera.to_screen(m_canvas.min, adapter.position(node));
    const auto s = node_size(node, inputs, outputs);
    const auto q = ImVec2(p.x + s.x * zoom, p.y + s.y * zoom);
    const auto title_bottom = p.y + m_layout.title_height * zoom;
    const auto selected     = is_selected(node);
    const auto rounding     = 4.f * zoom;
    const auto mouse        = ImGui::GetIO().MousePos;

    dl->AddRectFilled(p, q, ImGui::GetColorU32(ImGuiCol_WindowBg), rounding);
    auto title_color = ImGui::GetColorU32(selected ? ImGuiCol_TitleBgActive
                                                   : ImGuiCol_TitleBg);
    if constexpr (requires { adapter.title_color(node, selected); })
        title_color = adapter.title_color(node, selected);

    dl->AddRectFilled(p, ImVec2(q.x, title_bottom), title_color, rounding,
                      ImDrawFlags_RoundCornersTop);
    dl->AddRect(
      p, q,
      ImGui::GetColorU32(selected ? ImGuiCol_ButtonActive : ImGuiCol_Border),
      rounding, 0, selected ? 2.f : 1.f);

    // Integer sizes only: each new size bakes a new set of glyphs.
    const auto font_size = std::floor(ImGui::GetFontSize() * zoom);
    const auto pad       = m_layout.padding * zoom;
    const auto title     = std::string_view(adapter.title(node));
    const auto text_col  = ImGui::GetColorU32(ImGuiCol_Text);
    auto width = ImGui::CalcTextSize(title.data(), title.data() + title.size())
                   .x;

    // Under this size the text is unreadable and only costs.
    const auto draw_text = font_size >= 6.f;

    if (draw_text)
        dl->AddText(ImGui::GetFont(), font_size,
                    ImVec2(p.x + pad, p.y + pad * 0.5f), text_col, title.data(),
                    title.data() + title.size());

    auto in_width = 0.f;
    for (u32 i = 0; i != inputs; ++i) {
        const auto c = input_pin(p, i);
        dl->AddCircleFilled(c, m_layout.pin_radius * zoom, text_col);

        if (m_mouse_enabled and
            std::abs(mouse.x - c.x) <= m_layout.row_height * zoom * 0.5f and
            std::abs(mouse.y - c.y) <= m_layout.row_height * zoom * 0.5f)
            m_hovered_input = pin{ node, i };

        if (full) {
            const auto name = std::string_view(adapter.input_name(node, i));
            dl->AddText(ImVec2(c.x + pad, c.y - ImGui::GetFontSize() * 0.5f),
                        text_col, name.data(), name.data() + name.size());
            in_width = std::max(
              in_width,
              ImGui::CalcTextSize(name.data(), name.data() + name.size()).x);
        }
    }

    auto out_width = 0.f;
    for (u32 i = 0; i != outputs; ++i) {
        const auto c = output_pin(p, node, inputs, i);
        dl->AddCircleFilled(c, m_layout.pin_radius * zoom, text_col);

        if (m_mouse_enabled and
            std::abs(mouse.x - c.x) <= m_layout.row_height * zoom * 0.5f and
            std::abs(mouse.y - c.y) <= m_layout.row_height * zoom * 0.5f)
            m_hovered_output = pin{ node, i };

        if (full) {
            const auto name = std::string_view(adapter.output_name(node, i));
            const auto
              w = ImGui::CalcTextSize(name.data(), name.data() + name.size()).x;
            dl->AddText(
              ImVec2(c.x - pad - w, c.y - ImGui::GetFontSize() * 0.5f),
              text_col, name.data(), name.data() + name.size());
            out_width = std::max(out_width, w);
        }
    }

    if (full) {
        width = std::max(width, in_width + out_width + pad);

        const auto limit = std::max(width, m_layout.content_width);
        begin_content(
          node,
          ImVec2(p.x + pad, title_bottom +
                              static_cast<float>(inputs) * m_layout.row_height),
          limit);
        adapter.content(node);
        const auto content = end_content();

        auto& cache = m_cache[node];
        cache.width = std::max(width, content.x) + 2.f * m_layout.padding;
        cache.content_height = content.y;
    }

    if (m_mouse_enabled and contains(rect<ImVec2>{ p, q }, mouse))
        m_hovered_node = node;
}

template<graph_adapter A>
void editor::handle_mouse(A& adapter, events& ev) noexcept
{
    const auto& io   = ImGui::GetIO();
    const auto  ctrl = io.KeyCtrl;

    // The canvas button loses the hover when a widget of a node is hovered or
    // active: the editor must not move or select when the user edits a value.
    const auto free_mouse = m_mouse_enabled and not m_minimap_hovered;

    m_background_hovered = free_mouse and not m_hovered_node and
                           not m_hovered_link and not m_hovered_input and
                           not m_hovered_output;

    // A right click on a link or a node selects it and opens its menu: the
    // right click on the background stays for the menu of the caller.
    if (free_mouse and ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (m_hovered_node) {
            if (not is_selected(*m_hovered_node))
                select_node(*m_hovered_node, false);
            ImGui::OpenPopup(item_menu_id);
        } else if (m_hovered_link) {
            if (std::ranges::find(m_selected_links, *m_hovered_link) ==
                m_selected_links.end())
                select_link(*m_hovered_link, false);
            ImGui::OpenPopup(item_menu_id);
        }
    }

    if (free_mouse and ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_hovered_output) {
            m_drag             = drag_type::link;
            m_drag_pin         = *m_hovered_output;
            m_drag_from_output = true;
        } else if (m_hovered_input) {
            m_drag             = drag_type::link;
            m_drag_pin         = *m_hovered_input;
            m_drag_from_output = false;
        } else if (m_hovered_node) {
            if (not is_selected(*m_hovered_node) or ctrl)
                select_node(*m_hovered_node, ctrl);
            m_drag       = drag_type::nodes;
            m_drag_moved = false;
        } else if (m_hovered_link) {
            select_link(*m_hovered_link, ctrl);
        } else {
            if (not ctrl)
                clear_selection();
            m_drag       = drag_type::box;
            m_drag_start = io.MousePos;
        }
    }

    switch (m_drag) {
    case drag_type::none:
        break;

    case drag_type::nodes:
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const auto dx = io.MouseDelta.x / m_camera.zoom;
            const auto dy = io.MouseDelta.y / m_camera.zoom;
            for (const auto node : m_selected_nodes) {
                auto& pos = adapter.position(node);
                pos.x += dx;
                pos.y += dy;
            }
            m_drag_moved = m_drag_moved or dx != 0.f or dy != 0.f;
        } else {
            ev.nodes_moved = m_drag_moved;
            m_drag         = drag_type::none;
        }
        break;

    case drag_type::box: {
        const auto box = make_rect(m_drag_start, io.MousePos);
        auto*      dl  = ImGui::GetWindowDrawList();
        dl->AddRectFilled(box.min, box.max,
                          ImGui::GetColorU32(ImGuiCol_DragDropTarget, 0.2f));
        dl->AddRect(box.min, box.max,
                    ImGui::GetColorU32(ImGuiCol_DragDropTarget));

        if (not ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            for (u32 i = 0, e = adapter.node_count(); i != e; ++i) {
                const auto p = m_camera.to_screen(m_canvas.min,
                                                  adapter.position(i));
                const auto s = node_size(i, adapter.input_count(i),
                                         adapter.output_count(i));
                const auto r = rect<ImVec2>{ p, ImVec2(
                                                  p.x + s.x * m_camera.zoom,
                                                  p.y + s.y * m_camera.zoom) };

                if (overlaps(r, box) and not is_selected(i))
                    select_node(i, true);
            }
            m_drag = drag_type::none;
        }
    } break;

    case drag_type::link: {
        const auto start   = m_drag_from_output
                               ? output_pin(m_camera.to_screen(
                                            m_canvas.min,
                                            adapter.position(m_drag_pin.node)),
                                            m_drag_pin.node,
                                            adapter.input_count(m_drag_pin.node),
                                            m_drag_pin.port)
                               : input_pin(m_camera.to_screen(
                                           m_canvas.min,
                                           adapter.position(m_drag_pin.node)),
                                           m_drag_pin.port);
        const auto tangent = 50.f * m_camera.zoom;

        draw_link(m_drag_from_output
                    ? make_link_bezier(start, io.MousePos, tangent)
                    : make_link_bezier(io.MousePos, start, tangent),
                  true);

        if (not ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // A DEVS coupling never connects a model to itself.
            if (m_drag_from_output and m_hovered_input and
                m_hovered_input->node != m_drag_pin.node)
                ev.link_created = link_request{ m_drag_pin, *m_hovered_input };
            else if (not m_drag_from_output and m_hovered_output and
                     m_hovered_output->node != m_drag_pin.node)
                ev.link_created = link_request{ *m_hovered_output, m_drag_pin };

            m_drag = drag_type::none;
        }
    } break;
    }
}

template<graph_adapter A>
void editor::draw_minimap(A& adapter) noexcept
{
    const auto n      = adapter.node_count();
    m_minimap_hovered = false;

    if (n == 0)
        return;

    auto world = rect<ImVec2>{ adapter.position(0), adapter.position(0) };
    for (u32 i = 0; i != n; ++i) {
        const auto p = adapter.position(i);
        const auto s = node_size(i, adapter.input_count(i),
                                 adapter.output_count(i));
        world.min.x  = std::min(world.min.x, p.x);
        world.min.y  = std::min(world.min.y, p.y);
        world.max.x  = std::max(world.max.x, p.x + s.x);
        world.max.y  = std::max(world.max.y, p.y + s.y);
    }

    const auto canvas_w = m_canvas.max.x - m_canvas.min.x;
    const auto canvas_h = m_canvas.max.y - m_canvas.min.y;
    const auto map      = rect<ImVec2>{
        ImVec2(m_canvas.min.x + 8.f, m_canvas.max.y - canvas_h * 0.2f - 8.f),
        ImVec2(m_canvas.min.x + canvas_w * 0.2f + 8.f, m_canvas.max.y - 8.f)
    };

    // The viewport is part of the bounds: otherwise its rectangle overflows
    // the minimap when it is larger than the nodes or far from them.
    const auto view = rect<ImVec2>{ to_world(m_canvas.min),
                                    to_world(m_canvas.max) };
    world.min.x     = std::min(world.min.x, view.min.x);
    world.min.y     = std::min(world.min.y, view.min.y);
    world.max.x     = std::max(world.max.x, view.max.x);
    world.max.y     = std::max(world.max.y, view.max.y);

    const auto ww    = std::max(world.max.x - world.min.x, 1.f);
    const auto wh    = std::max(world.max.y - world.min.y, 1.f);
    const auto mw    = map.max.x - map.min.x;
    const auto mh    = map.max.y - map.min.y;
    const auto scale = std::min(mw / ww, mh / wh);

    // Centers the world in the minimap since the scale keeps its aspect.
    const auto offset = ImVec2(map.min.x + (mw - ww * scale) * 0.5f,
                               map.min.y + (mh - wh * scale) * 0.5f);
    const auto to_map = [&](const ImVec2 w) noexcept {
        return ImVec2(offset.x + (w.x - world.min.x) * scale,
                      offset.y + (w.y - world.min.y) * scale);
    };

    auto* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(map.min, map.max, true);
    dl->AddRectFilled(map.min, map.max,
                      ImGui::GetColorU32(ImGuiCol_WindowBg, 0.8f));

    for (u32 i = 0; i != n; ++i) {
        const auto p = adapter.position(i);
        const auto s = node_size(i, adapter.input_count(i),
                                 adapter.output_count(i));
        dl->AddRectFilled(to_map(p), to_map(ImVec2(p.x + s.x, p.y + s.y)),
                          ImGui::GetColorU32(is_selected(i)
                                               ? ImGuiCol_TitleBgActive
                                               : ImGuiCol_TitleBg));
    }

    dl->AddRect(to_map(view.min), to_map(view.max),
                ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
    dl->PopClipRect();
    dl->AddRect(map.min, map.max, ImGui::GetColorU32(ImGuiCol_Border));

    const auto mouse  = ImGui::GetIO().MousePos;
    m_minimap_hovered = m_mouse_enabled and contains(map, mouse);

    if (m_minimap_hovered and ImGui::IsMouseDown(ImGuiMouseButton_Left) and
        m_drag == drag_type::none) {
        const auto target = ImVec2(world.min.x + (mouse.x - offset.x) / scale,
                                   world.min.y + (mouse.y - offset.y) / scale);
        m_camera.scrolling.x = canvas_w * 0.5f - target.x * m_camera.zoom;
        m_camera.scrolling.y = canvas_h * 0.5f - target.y * m_camera.zoom;
    }
}

} // namespace irt::node_editor

#endif
