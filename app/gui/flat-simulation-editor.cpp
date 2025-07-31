// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace irt {

constexpr inline float  MW  = 50.f;
constexpr inline float  MH  = 50.f;
constexpr inline float  MW2 = MW / 2.f;
constexpr inline float  MH2 = MW / 2.f;
constexpr inline ImVec2 model_width_height(MW, MH);

constexpr auto to_float(std::integral auto i) noexcept -> float
{
    return static_cast<float>(i);
}

void flat_simulation_editor::center_camera() noexcept
{
    const auto top_left_z     = top_left * zoom;
    const auto bottom_right_z = bottom_right * zoom;
    const auto space          = canvas_sz - (bottom_right_z - top_left_z);

    scrolling = -top_left * zoom + space / 2.f;
}

void flat_simulation_editor::auto_fit_camera() noexcept
{
    const auto distance    = bottom_right - top_left;
    const auto zoom_factor = canvas_sz / distance;

    zoom = std::min(zoom_factor.x, zoom_factor.y);
    zoom = std::clamp(zoom, 0.01f, 5.f);

    const auto top_left_z     = top_left * zoom;
    const auto bottom_right_z = bottom_right * zoom;
    const auto space          = canvas_sz - (bottom_right_z - top_left_z);

    scrolling = -top_left * zoom + space / 2.f;
}

bool flat_simulation_editor::display(application& app) noexcept
{
    const auto canvas_p0 = ImGui::GetCursorScreenPos();
    canvas_sz            = ImGui::GetContentRegionAvail();

    if (not is_ready.test_and_set()) {
        rebuild(app);
        return false;
    }

    if (not ImGui::BeginChild(
          "flat-simulation",
          ImVec2(canvas_sz.x, canvas_sz.y - ImGui::GetFrameHeight()))) {
        ImGui::EndChild();
        return false;
    }

    if (canvas_sz.x < 50.0f)
        canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f)
        canvas_sz.y = 50.0f;

    ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const ImGuiIO& io        = ImGui::GetIO();
    ImDrawList*    draw_list = ImGui::GetWindowDrawList();

    if (actions[action::camera_center])
        center_camera();

    if (actions[action::camera_auto_fit])
        auto_fit_camera();

    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    ImGui::InvisibleButton("Canvas",
                           canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active  = ImGui::IsItemActive();

    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    if (is_active and ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        scrolling += io.MouseDelta;

    if (is_hovered and io.MouseWheel != 0.f) {
        zoom += io.MouseWheel * zoom * 0.1f;
        zoom = std::clamp(zoom, 0.01f, 5.f);
    }

    const auto drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (drag_delta.x == 0.0f and drag_delta.y == 0.0f)
        ImGui::OpenPopupOnItemClick("Flat simulation menu",
                                    ImGuiPopupFlags_MouseButtonRight);

    if (ImGui::BeginPopup("Flat simulation menu")) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Auto fit camera"))
                auto_fit_camera();
            if (ImGui::MenuItem("Center camera"))
                center_camera();

            if (bool enabled = actions[action::use_grid];
                ImGui::MenuItem("show grid", nullptr, &enabled))
                actions.set(action::use_grid, enabled);

            if (bool enabled = actions[action::use_bezier];
                ImGui::MenuItem("bezier lines", nullptr, &enabled))
                actions.set(action::use_bezier, enabled);

            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    if (is_hovered) {
        if (not run_selection and ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            run_selection   = true;
            start_selection = io.MousePos;
        }

        if (run_selection and ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            run_selection = false;
            end_selection = io.MousePos;

            if (start_selection == end_selection) {
                selected_nodes.clear();
            } else {
                ImVec2 bmin{
                    std::min(start_selection.x, end_selection.x),
                    std::min(start_selection.y, end_selection.y),
                };

                ImVec2 bmax{
                    std::max(start_selection.x, end_selection.x),
                    std::max(start_selection.y, end_selection.y),
                };

                selected_nodes.clear();

                data.try_read_only([&](const auto& d) {
                    auto& pj_ed = container_of(this, &project_editor::flat_sim);
                    for (const auto& mdl : pj_ed.pj.sim.models) {
                        const auto mdl_id = pj_ed.pj.sim.models.get_id(mdl);
                        const auto i      = get_index(mdl_id);

                        ImVec2 p_min(
                          origin.x + ((d.positions[i][0] - MW2) * zoom),
                          origin.y + ((d.positions[i][1] - MH2) * zoom));

                        ImVec2 p_max(
                          origin.x + ((d.positions[i][0] + MW2) * zoom),
                          origin.y + ((d.positions[i][1] + MH2) * zoom));

                        if (p_min.x >= bmin.x and p_max.x < bmax.x and
                            p_min.y >= bmin.y and p_max.y < bmax.y) {
                            selected_nodes.emplace_back(mdl_id);
                        }
                    }
                });
            }
        }
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);

    if (actions[action::use_grid]) {
        const float GRID_STEP = 64.0f;

        for (float x = std::fmod(scrolling.x, GRID_STEP); x < canvas_sz.x;
             x += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                               ImVec2(canvas_p0.x + x, canvas_p1.y),
                               IM_COL32(200, 200, 200, 40));

        for (float y = std::fmod(scrolling.y, GRID_STEP); y < canvas_sz.y;
             y += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                               ImVec2(canvas_p1.x, canvas_p0.y + y),
                               IM_COL32(200, 200, 200, 40));
    }

    data.try_read_only([&](const auto& d) {
        auto& pj_ed = container_of(this, &project_editor::flat_sim);

        if (d.positions.empty())
            return;

        small_vector<tree_node*, max_component_stack_size> stack;
        stack.emplace_back(pj_ed.pj.tn_head());

        while (!stack.empty()) {
            auto cur   = stack.back();
            auto tn_id = pj_ed.pj.tree_nodes.get_id(*cur);
            stack.pop_back();

            for (const auto& ch : cur->children) {
                if (ch.type == tree_node::child_node::type::model) {
                    const auto  mdl_id = ch.mdl;
                    const auto* mdl    = pj_ed.pj.sim.models.try_to_get(mdl_id);
                    const auto  i      = get_index(mdl_id);

                    if (not mdl)
                        continue;

                    ImVec2 p_min(origin.x + ((d.positions[i][0] - MW2) * zoom),
                                 origin.y + ((d.positions[i][1] - MH2) * zoom));

                    ImVec2 p_max(origin.x + ((d.positions[i][0] + MW2) * zoom),
                                 origin.y + ((d.positions[i][1] + MH2) * zoom));

                    if (canvas_p0.x <= p_min.x and p_min.x <= canvas_p1.x and
                        canvas_p0.y <= p_max.y and p_max.y <= canvas_p1.y) {
                        draw_list->AddRectFilled(
                          p_min, p_max, d.tn_colors[tn_id]);
                        if (zoom > 3.f)
                            draw_list->AddText(
                              p_min + ImVec2(5.f, 5.f),
                              IM_COL32(0, 0, 0, 255),
                              dynamics_type_names[ordinal(mdl->type)]);
                    }

                    dispatch(
                      *mdl,
                      [&]<typename Dynamics>(
                        Dynamics& dyn, const auto& sim, const auto src_mdl_id) {
                          if constexpr (has_output_port<Dynamics>) {
                              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                                  for (auto* block =
                                         sim.nodes.try_to_get(dyn.y[i]);
                                       block;
                                       block =
                                         sim.nodes.try_to_get(block->next)) {

                                      const auto src_idx =
                                        get_index(src_mdl_id);
                                      for (auto it = block->nodes.begin(),
                                                et = block->nodes.end();
                                           it != et;
                                           ++it) {
                                          if (const auto* dst =
                                                sim.models.try_to_get(
                                                  it->model)) {

                                              ImVec2 from(
                                                origin.x +
                                                  (d.positions[src_idx][0] *
                                                   zoom),
                                                origin.y +
                                                  (d.positions[src_idx][1] *
                                                   zoom));

                                              ImVec2 to(
                                                origin.x +
                                                  (d.positions[get_index(
                                                     it->model)][0] *
                                                   zoom),
                                                origin.y +
                                                  (d.positions[get_index(
                                                     it->model)][1] *
                                                   zoom));

                                              if (actions[action::use_bezier])
                                                  draw_list->AddBezierCubic(
                                                    from,
                                                    from + ImVec2(+50.f, 0.f),
                                                    to + ImVec2(-50.f, 0.f),
                                                    to,
                                                    IM_COL32(0, 127, 0, 255),
                                                    1.0f);
                                              else
                                                  draw_list->AddLine(
                                                    from,
                                                    to,
                                                    IM_COL32(0, 127, 0, 255),
                                                    1.0f);
                                          }
                                      }
                                  }
                              }
                          }
                      },
                      pj_ed.pj.sim,
                      pj_ed.pj.sim.models.get_id(mdl));
                }
            }

            if (auto* sibling = cur->tree.get_sibling(); sibling)
                stack.emplace_back(sibling);

            if (auto* child = cur->tree.get_child(); child)
                stack.emplace_back(child);
        }
    });

    if (run_selection) {
        end_selection = io.MousePos;

        if (start_selection == end_selection) {
            selected_nodes.clear();
        } else {
            ImVec2 bmin{
                std::min(start_selection.x, io.MousePos.x),
                std::min(start_selection.y, io.MousePos.y),
            };

            ImVec2 bmax{
                std::max(start_selection.x, io.MousePos.x),
                std::max(start_selection.y, io.MousePos.y),
            };

            draw_list->AddRectFilled(bmin, bmax, IM_COL32(200, 0, 0, 127));
        }
    }

    draw_list->PopClipRect();
    ImGui::EndChild();

    display_status();

    return true;
}

void flat_simulation_editor::reset() noexcept
{
    distance        = { 15.f, 15.f };
    scrolling       = { 0.f, 0.f };
    zoom            = 1.f;
    start_selection = { 0.f, 0.f };
    end_selection   = { 0.f, 0.f };

    selected_nodes.clear();
    run_selection = false;
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

constexpr static void move_tn(auto&              data,
                              const tree_node_id tn_id,
                              const float        shift_x,
                              const float        shift_y) noexcept
{
    data.tn_centers[tn_id].x += shift_x;
    data.tn_centers[tn_id].y += shift_y;
}

constexpr static void move_model(auto&          data,
                                 const model_id mdl_id,
                                 const float    shift_x,
                                 const float    shift_y) noexcept
{
    data.positions[mdl_id][0] += shift_x;
    data.positions[mdl_id][1] += shift_y;
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
        child_id id;
        float    width;
        float    height;
        float    x;
        float    y;
    };

    // To keep the grid position, we compute the greater width,height from
    // children and affect this tuple for each following nodes.

    const auto max_width_height   = compute_max_rect(data.tn_rects, pj, tn);
    const auto max_width_height_2 = max_width_height / 2.f;
    const auto grid_width_height =
      ImVec2(max_width_height.x * to_float(gen.column()),
             max_width_height.y * to_float(gen.row()));
    const auto grid_width_height_2 = grid_width_height / 2.f;

    vector<node> nodes(gen.cache.ssize(), reserve_tag);
    rect_bound   bound;
    for (const auto& c : gen.cache) {
        const auto c_id = gen.cache.get_id(c);

        nodes.push_back(node{ .id     = c_id,
                              .width  = max_width_height.x,
                              .height = max_width_height.y,
                              .x = (max_width_height.x * to_float(c.col + 1)) -
                                   max_width_height_2.x - grid_width_height_2.x,
                              .y = (max_width_height.y * to_float(c.row + 1)) -
                                   max_width_height_2.y -
                                   grid_width_height_2.y });

        bound.update(nodes.back().x - max_width_height_2.x,
                     nodes.back().y - max_width_height_2.y);
        bound.update(nodes.back().x + max_width_height_2.x,
                     nodes.back().y + max_width_height_2.y);
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
    unsigned hpoints;
    unsigned vpoints;
};

static auto max_point_in_vh_lines(const graph_component& g,
                                  const ImVec2           dist) noexcept
  -> max_point_in_vh_lines_result
{
    vector<float> hlines(g.cache.size(), reserve_tag);
    vector<float> vlines(g.cache.size(), reserve_tag);

    for (const auto& child : g.cache) {
        const auto  graph_node_id = child.node_id;
        const auto& pos           = g.g.node_positions[graph_node_id];

        hlines.emplace_back(pos[0]);
        vlines.emplace_back(pos[1]);
    }

    const auto cx = dist.x / to_float(g.cache.size());
    const auto cy = dist.y / to_float(g.cache.size());

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
        child_id id;
        float    width;
        float    height;
        float    x;
        float    y;
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

    vector<node> nodes(gen.cache.ssize(), reserve_tag);
    rect_bound   bound;
    for (const auto& c : gen.cache) {
        const auto c_id = gen.cache.get_id(c);

        nodes.push_back(
          node{ .id     = c_id,
                .width  = max_width_height.x,
                .height = max_width_height.y,
                .x      = (h_v_lines.hpoints * center_width_height.x *
                      gen.g.node_positions[c.node_id][0] / width_height.x) -
                     graph_center.x,
                .y = (h_v_lines.vpoints * center_width_height.y *
                      gen.g.node_positions[c.node_id][1] / width_height.y) -
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

    vector<node>         nodes(gen.children.ssize(), reserve_tag);
    table<child_id, u32> map(gen.children.ssize(), reserve_tag);
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

    for (auto i = 1, e = nodes.ssize(); i < e; ++i) {
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

static void compute_rect(auto&            data,
                         const project&   pj,
                         const modeling&  mod,
                         const tree_node& tn,
                         const component& compo) noexcept
{
    switch (compo.type) {
    case component_type::generic:
        if (auto* g = mod.generic_components.try_to_get(compo.id.generic_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case component_type::graph:
        if (auto* g = mod.graph_components.try_to_get(compo.id.graph_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case component_type::grid:
        if (auto* g = mod.grid_components.try_to_get(compo.id.grid_id))
            compute_automatic_layout(pj, tn, *g, data);
        break;

    case component_type::hsm:
    case component_type::none:
        break;
    }
}

void flat_simulation_editor::compute_rects(application& app,
                                           data_type&   d) noexcept
{
    auto& pj_ed = container_of(this, &project_editor::flat_sim);

    struct stack_elem {
        const tree_node* tn;
        bool             read_child   = false;
        bool             read_sibling = false;
    };

    vector<stack_elem> stack(max_component_stack_size, reserve_tag);
    stack.push_back(stack_elem{ .tn = pj_ed.pj.tn_head() });

    while (not stack.empty()) {
        const auto cur = stack.back();

        if (cur.read_child) {
            if (cur.read_sibling) {
                stack.pop_back();
                auto& c = app.mod.components.get<component>(cur.tn->id);
                compute_rect(d, pj_ed.pj, app.mod, *cur.tn, c);
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
}

void flat_simulation_editor::rebuild(application& app) noexcept
{
    app.add_gui_task([&]() {
        data.read_write([&](auto& d) {
            auto&      pj_ed = container_of(this, &project_editor::flat_sim);
            const auto mdls  = pj_ed.pj.sim.models.size();
            const auto tns   = pj_ed.pj.tree_nodes.size();

            clear(d, mdls, tns);

            compute_rects(app, d);
            compute_colors(d, pj_ed.pj.tree_nodes);

            rect_bound bound;
            for (const auto& p : d.positions) {
                bound.update(p.x - MW2, p.y - MH2);
                bound.update(p.x + MW2, p.y + MH2);
            }

            top_left     = bound.top_left();
            bottom_right = bound.bottom_right();

            auto_fit_camera();
        });
    });
}

void flat_simulation_editor::display_status() noexcept
{
    ImGui::TextFormat(
      "zoom: {} position: {},{} top-left: {},{} bottom-right: {},{}",
      zoom,
      top_left.x + scrolling.x,
      top_left.y + scrolling.y,
      top_left.x,
      top_left.y,
      bottom_right.x,
      bottom_right.y);
}
}
