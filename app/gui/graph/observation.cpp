// Copyright (c) 2025 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <imgui.h>
#include <implot.h>
#include <implot_internal.h>

#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "internal.hpp"

namespace irt {

/// The read-only view of a simulated graph for the @c node_editor: the
/// vertices are compact nodes placed at their projected positions.
struct graph_editor::view {
    application&          app;
    graph_editor&         ed;
    const graph&          g;
    const graph_observer* obs;
    std::span<const real> values;

    u32 index(const u32 node) const noexcept
    {
        return get_index(ed.m_nodes[node]);
    }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(ed.m_nodes.size());
    }

    ImVec2& position(const u32 node) noexcept { return ed.m_positions[node]; }

    std::optional<float> compact_size(const u32 node) const noexcept
    {
        return std::max(g.node_areas[index(node)], 1e-3f);
    }

    bool directed() const noexcept
    {
        return g.flags[graph::option_flags::directed];
    }

    ImU32 title_color(const u32 node, const bool /*selected*/) const noexcept
    {
        const auto i = index(node);

        if (not obs)
            return get_component_u32color(app, g.node_components[i]);

        if (i >= values.size())
            return ImGui::GetColorU32(ImGuiCol_TitleBg);

        const auto m = static_cast<double>(obs->scale_min);
        const auto M = static_cast<double>(obs->scale_max);
        const auto t = M > m ? std::clamp((values[i] - m) / (M - m), 0.0, 1.0)
                             : 0.0;

        return ImPlot::SampleColormapU32(static_cast<float>(t), IMPLOT_AUTO);
    }

    std::string_view title(const u32 node) const noexcept
    {
        return g.node_names[index(node)];
    }

    u32 input_count(const u32 /*node*/) const noexcept { return 0; }
    u32 output_count(const u32 /*node*/) const noexcept { return 0; }

    std::string_view input_name(const u32 /*node*/,
                                const u32 /*port*/) const noexcept
    {
        return {};
    }

    std::string_view output_name(const u32 /*node*/,
                                 const u32 /*port*/) const noexcept
    {
        return {};
    }

    void content(const u32 /*node*/) const noexcept {}

    template<typename Fn>
    void for_each_link(Fn&& fn) const noexcept
    {
        for (const auto& [from, to] : ed.m_edges)
            fn(from, 0u, to, 0u);
    }
};

void graph_editor::build(const graph& g) noexcept
{
    constexpr auto none = std::numeric_limits<u32>::max();

    if (std::cmp_not_equal(m_nodes.size(), g.nodes.size())) {
        m_editor.reset();
        m_editor.request_fit();
    }

    m_nodes.clear();
    m_node_indices.clear();
    m_node_indices.resize(g.nodes.capacity(), none);
    for (const auto id : g.nodes) {
        m_node_indices[get_index(id)] = static_cast<u32>(m_nodes.size());
        m_nodes.push_back(id);
    }

    // The vertices rotate around the center of their bounding box: the
    // rotation keeps the graph in place in the canvas.
    std::array<float, 3> min{ std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max() };
    std::array<float, 3> max{ std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest() };

    for (const auto id : m_nodes) {
        const auto  i    = get_index(id);
        const auto& p    = g.node_positions[i];
        const auto  half = g.node_areas[i] / 2.f;

        for (int axis = 0; axis < 3; ++axis) {
            const auto c = axis < 2 ? p[axis] + half : p[axis];
            min[axis]    = std::min(min[axis], c);
            max[axis]    = std::max(max[axis], c);
        }
    }

    if (not m_nodes.empty())
        m_proj.update_matrices(m_proj.angles(), { (min[0] + max[0]) / 2.f,
                                                  (min[1] + max[1]) / 2.f,
                                                  (min[2] + max[2]) / 2.f });

    m_positions.resize(m_nodes.size());
    for (u32 n = 0, e = static_cast<u32>(m_nodes.size()); n != e; ++n) {
        const auto  i    = get_index(m_nodes[n]);
        const auto& p    = g.node_positions[i];
        const auto  half = g.node_areas[i] / 2.f;
        const auto  r    = m_proj.compute(p[0] + half, p[1] + half, p[2]);

        m_positions[n] = ImVec2(r[0] - half, r[1] - half);
    }

    m_edges.clear();
    for (const auto id : g.edges) {
        const auto& ends = g.edges_nodes[get_index(id)];

        if (g.nodes.exists(ends[0].first) and g.nodes.exists(ends[1].first))
            m_edges.emplace_back(m_node_indices[get_index(ends[0].first)],
                                 m_node_indices[get_index(ends[1].first)]);
    }

    m_dirty = false;
}

bool graph_editor::show_canvas(application&          app,
                               const graph&          g,
                               const graph_observer* obs,
                               const bool            can_clone) noexcept
{
    if (m_dirty or std::cmp_not_equal(m_nodes.size(), g.nodes.size()) or
        std::cmp_not_equal(m_edges.size(), g.edges.size()))
        build(g);

    m_editor.read_only = true;
    m_editor.min_zoom  = 0.01f;
    m_editor.max_zoom  = 1000.f;

    if (obs)
        ImPlot::PushColormap(obs->color_map);

    auto v = view{ .app    = app,
                   .ed     = *this,
                   .g      = g,
                   .obs    = obs,
                   .values = obs ? obs->get_values()
                                 : std::span<const real>{} };
    (void)m_editor.show("graph-observation", v);

    if (obs)
        ImPlot::PopColormap();

    // A right drag on the background rotates the graph and a right click
    // without drag opens the menu.
    if (m_editor.is_background_hovered() and
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        m_rotating = true;

    if (m_rotating) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const auto& io     = ImGui::GetIO();
            auto        angles = m_proj.angles();
            angles[0] += io.MouseDelta.y * 0.01f;
            angles[1] += io.MouseDelta.x * 0.01f;
            m_proj.update_matrices(angles, m_proj.center());
            m_dirty = true;
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            // The delta is null below the drag threshold of ImGui.
            const auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
            if (delta.x == 0.f and delta.y == 0.f)
                ImGui::OpenPopup("Graph-Editor#Popup");
            m_rotating = false;
        }
    }

    auto clone = false;
    if (ImGui::BeginPopup("Graph-Editor#Popup")) {
        if (ImGui::MenuItem("Fit"))
            m_editor.request_fit();

        if (ImGui::MenuItem("Reset rotation")) {
            m_proj.update_matrices({ 0.f, 0.f, 0.f }, m_proj.center());
            m_dirty = true;
            m_editor.request_fit();
        }

        ImGui::MenuItem("Show minimap", "", &m_editor.show_minimap);

        if (can_clone and ImGui::MenuItem("Clone display"))
            clone = true;

        ImGui::Separator();
        ImGui::TextDisabled("Right drag: rotate the graph");
        ImGui::EndPopup();
    }

    return clone;
}

void graph_editor::clone(application&    app,
                         project_editor& ed,
                         tree_node&      tn,
                         graph_observer& obs) noexcept
{
    if (not app.graph_eds.can_alloc())
        return;

    auto& new_g = app.graph_eds.alloc();
    new_g.m_proj.update_matrices(m_proj.angles(), m_proj.center());

    app.sim_wnds.push_back(global_simulation_window{
      .pj_id        = app.pjs.get_id(ed),
      .tn_id        = ed.pj.tree_nodes.get_id(tn),
      .graph_ed_id  = app.graph_eds.get_id(new_g),
      .graph_obs_id = ed.pj.graph_observers.get_id(obs),
    });
}

/// @return The graph of the tree node @c tn or @c nullptr if the component
/// is not a graph component.
static const graph* get_graph(const component_access& ids,
                              const tree_node&        tn) noexcept
{
    if (not ids.exists(tn.id))
        return nullptr;

    const auto& compo = ids.components[tn.id];
    if (compo.type != component_type::graph)
        return nullptr;

    return &ids.graph_components.get(compo.id.graph_id).g;
}

auto graph_editor::show(const char*     name,
                        application&    app,
                        project_editor& ed,
                        tree_node&      tn,
                        graph_observer& obs) noexcept
  -> graph_editor::show_result_type
{
    if (not m_dock_init) {
        ImGui::SetNextWindowDockID(app.get_main_dock_id());
        m_dock_init = true;
    }

    bool is_open = true;
    if (not ImGui::Begin(name, &is_open)) {
        ImGui::End();
        return is_open ? show_result_type::none
                       : show_result_type::request_to_close;
    }

    obs.update(ed.pj);

    const auto found = app.mod.ids.read([&](const auto& ids, auto) noexcept {
        const auto* g = get_graph(ids, tn);
        if (g and show_canvas(app, *g, &obs, true))
            clone(app, ed, tn, obs);

        return g != nullptr;
    });

    ImGui::End();

    return is_open and found ? show_result_type::none
                             : show_result_type::request_to_close;
}

void graph_editor::show(application&    app,
                        project_editor& ed,
                        tree_node&      tn) noexcept
{
    const auto name = format_n<64>("{}##{}", ed.pj.name.sv(),
                                   get_index(ed.pj.tree_nodes.get_id(tn)));

    if (ImGui::BeginChild(name.c_str())) {
        app.mod.ids.read([&](const auto& ids, auto) noexcept {
            if (const auto* g = get_graph(ids, tn))
                (void)show_canvas(app, *g, nullptr, false);
        });
    }

    ImGui::EndChild();
}

void graph_editor::show(application&    app,
                        project_editor& ed,
                        tree_node&      tn,
                        graph_observer& obs) noexcept
{
    const auto name = format_n<64>(
      "{}##{}", ed.pj.name.sv(), get_index(ed.pj.graph_observers.get_id(obs)));

    if (ImGui::BeginChild(name.c_str())) {
        obs.update(ed.pj);

        app.mod.ids.read([&](const auto& ids, auto) noexcept {
            const auto* g = get_graph(ids, tn);
            if (g and show_canvas(app, *g, &obs, true))
                clone(app, ed, tn, obs);
        });
    }

    ImGui::EndChild();
}

} // namespace irt
