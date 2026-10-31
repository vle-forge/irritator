// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irt {

// The layout reserves the place of a DEVS node with a few ports for each
// model.
constexpr inline float MW  = 200.f;
constexpr inline float MH  = 120.f;
constexpr inline float MW2 = MW / 2.f;
constexpr inline float MH2 = MH / 2.f;

constexpr inline ImVec2 model_width_height(MW, MH);

constexpr auto to_float(std::integral auto i) noexcept -> float
{
    return static_cast<float>(i);
}

/// The view of all the models of the simulation for the @c node_editor, read
/// only: the nodes can be moved but the simulation does not change.
struct flat_simulation_editor::graph {
    project_editor&         pj_ed;
    flat_simulation_editor& ed;

    const model* mdl(const u32 node) const noexcept
    {
        return pj_ed.pj.sim.models.try_to_get(ed.nodes[node].mdl);
    }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(ed.nodes.size());
    }

    ImVec2& position(const u32 node) noexcept { return ed.positions[node]; }

    ImU32 title_color(const u32 node, const bool /*selected*/) const noexcept
    {
        return ed.nodes[node].color;
    }

    std::string_view title(const u32 node) const noexcept
    {
        const auto* m = mdl(node);
        return m ? dynamics_type_names[ordinal(m->type)] : "?";
    }

    u32 input_count(const u32 node) const noexcept
    {
        const auto* m = mdl(node);
        return m ? input_port_count(*m) : 0u;
    }

    u32 output_count(const u32 node) const noexcept
    {
        const auto* m = mdl(node);
        return m ? output_port_count(*m) : 0u;
    }

    std::string_view input_name(const u32 node, const u32 port) const noexcept
    {
        const auto* m = mdl(node);
        return m ? input_port_name(*m, port) : std::string_view();
    }

    std::string_view output_name(const u32 node, const u32 port) const noexcept
    {
        const auto* m = mdl(node);
        return m ? output_port_name(*m, port) : std::string_view();
    }

    void content(const u32 /*node*/) const noexcept {}

    template<typename Fn>
    void for_each_link(Fn&& fn) const noexcept
    {
        for (const auto& l : ed.links)
            fn(l.out_node, l.out_port, l.in_node, l.in_port);
    }
};

void flat_simulation_editor::build_graph(const project_editor& pj_ed,
                                         const data_type&      d) noexcept
{
    constexpr auto none = std::numeric_limits<u32>::max();
    const auto&    sim  = pj_ed.pj.sim;

    nodes.clear();
    positions.clear();
    links.clear();

    vector<u32> indices(sim.models.capacity(), none);

    small_vector<const tree_node*, max_component_stack_size> stack;
    if (const auto* head = pj_ed.pj.tree_nodes.try_to_get(pj_ed.pj.tn_head()))
        stack.emplace_back(head);

    while (not stack.empty()) {
        const auto* cur   = stack.back();
        const auto  tn_id = pj_ed.pj.tree_nodes.get_id(*cur);
        stack.pop_back();

        for (const auto& ch : cur->children) {
            if (ch.type != tree_node::child_node::type::model or
                not sim.models.exists(ch.mdl))
                continue;

            const auto idx    = get_index(ch.mdl);
            const auto center = idx < d.positions.size() ? d.positions[idx]
                                                         : ImVec2();

            const auto color = get_index(tn_id) < d.tn_colors.size()
                                 ? d.tn_colors[tn_id]
                                 : ImGui::GetColorU32(ImGuiCol_TitleBg);

            indices[idx] = static_cast<u32>(nodes.size());
            nodes.push_back(node{ .mdl = ch.mdl, .color = color });
            positions.push_back(ImVec2(center.x - MW2, center.y - MH2));
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }

    for (u32 i = 0, e = static_cast<u32>(nodes.size()); i != e; ++i) {
        dispatch(sim.models.get(nodes[i].mdl), [&]<typename Dynamics>(
                                                 const Dynamics& dyn) noexcept {
            if constexpr (has_output_port<Dynamics>) {
                for (int p = 0, pe = length(dyn.y); p != pe; ++p) {
                    sim.for_each(dyn.y[p], [&](auto& dst, auto dst_port) {
                        const auto in = indices[get_index(sim.get_id(dst))];
                        if (in != none)
                            links.push_back(
                              link{ .out_node = i,
                                    .out_port = static_cast<u32>(p),
                                    .in_node  = in,
                                    .in_port  = static_cast<u32>(dst_port) });
                    });
                }
            }
        });
    }

    editor.reset();
    editor.request_fit();
}

bool flat_simulation_editor::display(application&    app,
                                     project_editor& pj_ed) noexcept
{
    if (not is_ready.test_and_set()) {
        rebuild(app, pj_ed);
        return false;
    }

    // The layout task may not have written the data yet: the empty data are
    // skipped to build the graph when they arrive.
    data.read([&](const auto& d, const auto v) {
        if (v != version and not d.tn_colors.empty()) {
            version = v;
            build_graph(pj_ed, d);
        }
    });

    editor.read_only = true;
    editor.min_zoom  = 0.01f;

    if (ImGui::BeginChild("flat-simulation",
                          ImVec2(0.f, -ImGui::GetFrameHeightWithSpacing()))) {
        auto g = graph{ .pj_ed = pj_ed, .ed = *this };
        (void)editor.show("flat-simulation-graph", g);

        if (editor.is_background_hovered() and
            ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("Flat simulation menu");

        if (ImGui::BeginPopup("Flat simulation menu")) {
            if (ImGui::MenuItem("Auto fit camera"))
                editor.request_fit();
            ImGui::MenuItem("Show minimap", "", &editor.show_minimap);
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();

    display_status();

    return true;
}

constexpr static auto clear(auto&      data,
                            const auto models,
                            const auto tns) noexcept
{
    data.positions.resize(models, ImVec2(0.f, 0.f));

    data.tn_rects.resize(tns, ImVec2(0.f, 0.f));
    data.tn_centers.resize(tns, ImVec2(0.f, 0.f));
    data.tn_factors.resize(tns, ImVec2(1.f, 1.f));
    data.tn_colors.resize(tns, IM_COL32(255, 255, 255, 255));
}

static ImU32 compute_color(const float t) noexcept
{
    static const ImColor tables[] = {
        IM_COL32(103, 0, 31, 255),    IM_COL32(178, 24, 43, 255),
        IM_COL32(214, 96, 77, 255),   IM_COL32(244, 165, 130, 255),
        IM_COL32(253, 219, 199, 255), IM_COL32(247, 247, 247, 255),
        IM_COL32(209, 229, 240, 255), IM_COL32(146, 197, 222, 255),
        IM_COL32(67, 147, 195, 255),  IM_COL32(33, 102, 172, 255),
        IM_COL32(5, 48, 97, 255)
    };

    const auto i1 = static_cast<int>(to_float(length(tables) - 1) * t);
    const auto i2 = i1 + 1;

    if (i2 == length(tables) || length(tables) == 1)
        return tables[i1];

    const float den = 1.0f / (length(tables) - 1);
    const float t1  = to_float(i1) * den;
    const float t2  = to_float(i2) * den;
    const float tr  = (t - t1) / (t2 - t1);
    const auto  s   = static_cast<ImU32>(tr * 256.f);
    const ImU32 af  = 256 - s;
    const ImU32 bf  = s;
    const ImU32 al  = (tables[i1] & 0x00ff00ff);
    const ImU32 ah  = (tables[i1] & 0xff00ff00) >> 8;
    const ImU32 bl  = (tables[i2] & 0x00ff00ff);
    const ImU32 bh  = (tables[i2] & 0xff00ff00) >> 8;
    const ImU32 ml  = (al * af + bl * bf);
    const ImU32 mh  = (ah * af + bh * bf);
    return (mh & 0xff00ff00) | ((ml & 0xff00ff00) >> 8);
}

constexpr static void move_models(auto&            data,
                                  const tree_node& tn,
                                  const float      shift_x,
                                  const float      shift_y) noexcept
{
    for (const auto& c : tn.children) {
        if (c.is_model()) {
            data.positions[c.mdl][0] += shift_x;
            data.positions[c.mdl][1] += shift_y;
        }
    }
}

static void shift_tn_and_models(auto&            data,
                                const project&   pj,
                                const tree_node& tn,
                                const float      shift_x,
                                const float      shift_y) noexcept
{

    move_models(data, tn, shift_x, shift_y);
    data.tn_centers[pj.tree_nodes.get_id(tn)].x += shift_x;
    data.tn_centers[pj.tree_nodes.get_id(tn)].y += shift_y;

    if (tn.tree.get_child()) {
        vector<const tree_node*> stack(max_component_stack_size, reserve_tag);
        stack.emplace_back(tn.tree.get_child());

        while (not stack.empty()) {
            const auto cur = stack.back();
            stack.pop_back();

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child())
                stack.emplace_back(child);
        }
    }
}

class rect_bound
{
    float x_min = +INFINITY;
    float x_max = -INFINITY;
    float y_min = +INFINITY;
    float y_max = -INFINITY;

public:
    constexpr void update(const float x, const float y) noexcept
    {
        x_min = std::min(x_min, x);
        x_max = std::max(x_max, x);
        y_min = std::min(y_min, y);
        y_max = std::max(y_max, y);
    }

    constexpr auto width_height() const noexcept
    {
        return ImVec2(std::abs(x_max - x_min), std::abs(y_max - y_min));
    }

    constexpr auto center() const noexcept
    {
        return ImVec2((x_max + x_min) / 2.f + x_min,
                      (y_max + y_min) / 2.f + y_min);
    }

    constexpr auto top_left() const noexcept { return ImVec2(x_min, y_min); }

    constexpr auto bottom_right() const noexcept
    {
        return ImVec2(x_max, y_max);
    }
};

constexpr static void compute_colors(auto&       data,
                                     const auto& tree_nodes) noexcept
{
    const auto tns_f = to_float(tree_nodes.size());

    for (const auto& tn : tree_nodes) {
        const auto tn_id   = tree_nodes.get_id(tn);
        const auto tn_id_f = to_float(get_index(tn_id));

        data.tn_colors[tn_id] = compute_color(tn_id_f / tns_f);
    }
}

static auto compute_max_rect(const vector<ImVec2>& tn_rects,
                             const project&        pj,
                             const tree_node&      parent) noexcept
{
    auto ret = model_width_height;

    for (const auto& child : parent.children) {
        if (child.is_tree_node()) {
            const auto* sub_tn    = child.tn;
            const auto  sub_tn_id = pj.tree_nodes.get_id(sub_tn);

            ret = ImMax(ret, tn_rects[sub_tn_id]);
        }
    }

    return ret;
}

static auto compute_automatic_layout(const project&        pj,
                                     const tree_node&      tn,
                                     const grid_component& gen,
                                     auto&                 data) noexcept
{
    struct node {
        component_id id;
        float        width;
        float        height;
        float        x;
        float        y;
    };

    // To keep the grid position, we compute the greater width,height from
    // children and affect this tuple for each following nodes.

    const auto max_width_height   = compute_max_rect(data.tn_rects, pj, tn);
    const auto max_width_height_2 = max_width_height / 2.f;
    const auto grid_width_height =
      ImVec2(max_width_height.x * to_float(gen.column()),
             max_width_height.y * to_float(gen.row()));
    const auto grid_width_height_2 = grid_width_height / 2.f;

    vector<node> nodes(gen.cells_number(), reserve_tag);
    rect_bound   bound;
    for (auto x = 0; x < gen.column(); ++x) {
        for (auto y = 0; y < gen.row(); ++y) {
            nodes.push_back(
              node{ .id     = gen.children()[gen.pos(x, y)],
                    .width  = max_width_height.x,
                    .height = max_width_height.y,
                    .x      = (max_width_height.x * to_float(x + 1)) -
                              max_width_height_2.x - grid_width_height_2.x,
                    .y      = (max_width_height.y * to_float(y + 1)) -
                              max_width_height_2.y - grid_width_height_2.y });

            bound.update(nodes.back().x - max_width_height_2.x,
                         nodes.back().y - max_width_height_2.y);
            bound.update(nodes.back().x + max_width_height_2.x,
                         nodes.back().y + max_width_height_2.y);
        }
    }

    for (auto& node : nodes) {
        const auto* sub_tn = tn.children[node.id].tn;

        shift_tn_and_models(data, pj, *sub_tn, node.x, node.y);
    }

    data.tn_rects[pj.tree_nodes.get_id(tn)] = bound.width_height();
    data.tn_centers[pj.tree_nodes.get_id(tn)] += bound.center();
}

static auto compute_width_height(const graph_component& g) noexcept
{
    rect_bound bound;
    for (const auto node_id : g.g.nodes)
        bound.update(g.g.node_positions[node_id][0],
                     g.g.node_positions[node_id][1]);

    return bound.width_height();
}

struct max_point_in_vh_lines_result {
    sz hpoints;
    sz vpoints;
};

static auto max_point_in_vh_lines(const graph_component& g,
                                  const ImVec2           dist) noexcept
  -> max_point_in_vh_lines_result
{
    vector<float> hlines(g.g.nodes.size(), reserve_tag);
    vector<float> vlines(g.g.nodes.size(), reserve_tag);

    for (const auto& graph_node_id : g.g.nodes) {
        const auto& pos = g.g.node_positions[graph_node_id];

        hlines.emplace_back(pos[0]);
        vlines.emplace_back(pos[1]);
    }

    const auto cx = dist.x / to_float(g.g.nodes.size());
    const auto cy = dist.y / to_float(g.g.nodes.size());

    std::ranges::sort(hlines);
    std::ranges::sort(vlines);

    {
        auto it = hlines.begin();
        while (it != hlines.end()) {
            auto next = it + 1;
            if (next == hlines.end())
                break;

            if (*next - *it < cx)
                hlines.erase(next);
            else
                ++it;
        }
    }

    {
        auto it = vlines.begin();
        while (it != vlines.end()) {
            auto next = it + 1;
            if (next == vlines.end())
                break;
            if (*next - *it < cy)
                vlines.erase(next);
            else
                ++it;
        }
    }

    return { .hpoints = hlines.size(), .vpoints = vlines.size() };
}

static void compute_automatic_layout(const project&         pj,
                                     const tree_node&       tn,
                                     const graph_component& gen,
                                     auto&                  data) noexcept
{
    struct node {
        graph_node_id id;
        float         width;
        float         height;
        float         x;
        float         y;
    };

    // To keep the graph position, we compute (1) the greater width,height
    // from children and the positions factors from the underlying
    // graph_component::graph width, height.

    const auto max_width_height    = compute_max_rect(data.tn_rects, pj, tn);
    const auto center_width_height = max_width_height / 2.f;
    const auto width_height        = compute_width_height(gen);
    const auto h_v_lines           = max_point_in_vh_lines(gen, width_height);
    const auto graph_width_height =
      ImVec2(h_v_lines.hpoints * max_width_height.x,
             h_v_lines.vpoints * max_width_height.y);
    const auto graph_center = graph_width_height / 2.f;

    vector<node> nodes(gen.g.nodes.size(), reserve_tag);
    rect_bound   bound;
    for (const auto& c : gen.g.nodes) {
        nodes.push_back(
          node{ .id     = c,
                .width  = max_width_height.x,
                .height = max_width_height.y,
                .x      = (h_v_lines.hpoints * center_width_height.x *
                           gen.g.node_positions[c][0] / width_height.x) -
                          graph_center.x,
                .y      = (h_v_lines.vpoints * center_width_height.y *
                           gen.g.node_positions[c][1] / width_height.y) -
                          graph_center.y });

        bound.update(nodes.back().x - nodes.back().width,
                     nodes.back().y - nodes.back().height);
        bound.update(nodes.back().x + nodes.back().width,
                     nodes.back().y + nodes.back().height);
    }

    for (auto& node : nodes) {
        const auto* sub_tn = tn.children[node.id].tn;

        shift_tn_and_models(data, pj, *sub_tn, node.x, node.y);
    }

    data.tn_rects[pj.tree_nodes.get_id(tn)] = bound.width_height();
    data.tn_centers[pj.tree_nodes.get_id(tn)] += bound.center();
}

static void compute_automatic_layout(const project&           pj,
                                     const tree_node&         tn,
                                     const generic_component& gen,
                                     auto&                    data) noexcept
{
    struct node {
        child_id id;
        float    width;
        float    height;
        float    x;
        float    y;
    };

    vector<node>        nodes(gen.children.ssize(), reserve_tag);
    table<child_id, sz> map(gen.children.ssize(), reserve_tag);
    for (const auto& c : gen.children) {
        const auto c_id = gen.children.get_id(c);
        map.data.emplace_back(c_id, nodes.size());

        switch (c.type) {
        case child_type::model:
            nodes.push_back(node{ .id     = c_id,
                                  .width  = MW,
                                  .height = MH,
                                  .x      = gen.children_positions[c_id].x,
                                  .y      = gen.children_positions[c_id].y });
            break;

        case child_type::component: {
            const auto* sub_tn    = tn.children[c_id].tn;
            const auto  sub_tn_id = pj.tree_nodes.get_id(*sub_tn);

            nodes.push_back(node{ .id     = c_id,
                                  .width  = data.tn_rects[sub_tn_id].x,
                                  .height = data.tn_rects[sub_tn_id].y,
                                  .x      = gen.children_positions[c_id].x,
                                  .y      = gen.children_positions[c_id].y });
        } break;
        }
    }
    map.sort();

    std::ranges::sort(nodes, [](const auto& left, const auto& rigth) {
        return left.y < rigth.y;
    });

    for (sz i = 1, e = nodes.size(); i < e; ++i) {
        auto& prev    = nodes[i - 1];
        auto& current = nodes[i];

        if (current.y < prev.y + prev.height)
            current.y = prev.y + prev.height;

        if (std::abs(current.x - prev.x) < (current.width + prev.width) / 2.f) {
            if (current.x < prev.x) {
                current.x = prev.x - (current.width + prev.width) / 2.f;
            } else {
                current.x = prev.x + (current.width + prev.width) / 2.f;
            }
        }
    }

    rect_bound bound;
    for (auto& node : nodes) {
        for (const auto& con : gen.connections) {
            if (con.src == node.id) {
                if (const auto* dst_ptr = map.get(con.dst); dst_ptr) {
                    const auto& neighbor = nodes[*dst_ptr];

                    if (std::abs(node.x - neighbor.x) <
                        (node.width + neighbor.width) / 2.f) {
                        if (node.x < neighbor.x) {
                            node.x =
                              neighbor.x - (node.width + neighbor.width) / 2.f;
                        } else {
                            node.x =
                              neighbor.x + (node.width + neighbor.width) / 2.f;
                        }
                    }

                    if (std::abs(node.y - neighbor.y) <
                        (node.height + neighbor.height) / 2.f) {
                        if (node.y < neighbor.y) {
                            node.y = neighbor.y -
                                     (node.height + neighbor.height) / 2.f;
                        } else {
                            node.y = neighbor.y +
                                     (node.height + neighbor.height) / 2.f;
                        }
                    }
                }
            }
        }

        bound.update(node.x - node.width / 2.f, node.y - node.height / 2.f);
        bound.update(node.x + node.width / 2.f, node.y + node.height / 2.f);
    }

    for (auto& node : nodes) {
        const auto& c = gen.children.get(node.id);

        if (c.type == child_type::model) {
            data.positions[tn.children[node.id].mdl].x += node.x;
            data.positions[tn.children[node.id].mdl].y += node.y;
        } else {
            const auto* sub_tn = tn.children[node.id].tn;

            shift_tn_and_models(data, pj, *sub_tn, node.x, node.y);
        }
    }

    data.tn_rects[pj.tree_nodes.get_id(tn)] = bound.width_height();
    data.tn_centers[pj.tree_nodes.get_id(tn)] += bound.center();
}

static void compute_rect(auto&                   data,
                         const project&          pj,
                         const component_access& ids,
                         const tree_node&        tn,
                         const component&        compo) noexcept
{
    switch (compo.type) {
    case component_type::generic:
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case component_type::graph:
        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case component_type::grid:
        if (auto* g = ids.grid_components.try_to_get(compo.id.grid_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case irt::component_type::simulation:
    case component_type::hsm:
    case component_type::none:
        break;
    }
}

void flat_simulation_editor::compute_rects(application&    app,
                                           project_editor& pj_ed,
                                           data_type&      d) noexcept
{
    struct stack_elem {
        const tree_node* tn;
        bool             read_child   = false;
        bool             read_sibling = false;
    };

    app.mod.ids.read([&](const auto& ids, auto) noexcept {
        vector<stack_elem> stack(max_component_stack_size, reserve_tag);
        stack.push_back(stack_elem{
          .tn = pj_ed.pj.tree_nodes.try_to_get(pj_ed.pj.tn_head()) });

        while (not stack.empty()) {
            const stack_elem cur = stack.back();

            if (cur.read_child) {
                if (cur.read_sibling) {
                    stack.pop_back();
                    auto& c = ids.components[cur.tn->id];
                    compute_rect(d, pj_ed.pj, ids, *cur.tn, c);
                } else {
                    stack.back().read_sibling = true;
                    if (auto* sibling = cur.tn->tree.get_sibling(); sibling) {
                        stack.push_back(stack_elem{ .tn = sibling });
                    }
                }
            } else {
                stack.back().read_child = true;
                if (auto* child = cur.tn->tree.get_child()) {
                    stack.push_back(stack_elem{ .tn = child });
                }
            }
        }
    });
}

void flat_simulation_editor::rebuild(application&    app,
                                     project_editor& pj_ed) noexcept
{
    app.add_gui_task([&]() {
        data.write([&](auto& d) {
            // Indexed by identifiers: the capacity covers the holes.
            const auto mdls = pj_ed.pj.sim.models.capacity();
            const auto tns  = pj_ed.pj.tree_nodes.capacity();

            clear(d, mdls, tns);

            compute_rects(app, pj_ed, d);
            compute_colors(d, pj_ed.pj.tree_nodes);
        });
    });
}

void flat_simulation_editor::display_status() noexcept
{
    ImGui::TextFormat("models: {} connections: {}", nodes.size(), links.size());
}
}
