// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace irt {

void show_project_params_menu(graph_component& graph) noexcept
{
    static const char* names[] = { "in-out", "name", "name+suffix" };

    int type = ordinal(graph.type);
    if (ImGui::Combo("connection type", &type, names, length(names))) {
        graph.type = enum_cast<graph_component::connection_type>(type);
    }

    ImGui::SameLine();
    HelpMarker(
      "- in-out: only connect output port `out' to input port `in'.\n"
      "- name: only connect output port to input port with the same name.\n"
      "- name+suffix: connect output port to input port with the same "
      "prefix. The suffix, an integer, is used to allow connection of "
      "output port from varius models to input ports with suffix (ie. two "
      "models m1 and m2 with output port M connect to input port M_0 and "
      "M_1 of a thrid model.");
}

/// The view of a graph component for the @c node_editor: the vertices are
/// compact nodes sized by their area, the edges link their centers.
struct graph_component_editor_data::view {
    application&                 app;
    graph_component_editor_data& ed;
    graph_component&             data;

    graph_node_id id(const u32 node) const noexcept { return ed.m_nodes[node]; }

    u32 node_count() const noexcept
    {
        return static_cast<u32>(ed.m_nodes.size());
    }

    ImVec2& position(const u32 node) noexcept
    {
        const auto idx = get_index(id(node));

        if (ed.m_position_ids[idx] != id(node)) {
            ed.m_position_ids[idx] = id(node);
            ed.m_positions[idx]    = ImVec2(data.g.node_positions[idx][0],
                                            data.g.node_positions[idx][1]);
        }

        return ed.m_positions[idx];
    }

    std::optional<float> compact_size(const u32 node) const noexcept
    {
        return std::max(data.g.node_areas[get_index(id(node))], 1e-3f);
    }

    bool directed() const noexcept
    {
        return data.g.flags[graph::option_flags::directed];
    }

    ImU32 title_color(const u32 node, const bool /*selected*/) const noexcept
    {
        return get_component_u32color(
          app, data.g.node_components[get_index(id(node))]);
    }

    std::string_view title(const u32 node) const noexcept
    {
        return data.g.node_names[get_index(id(node))];
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
        for (const auto e : ed.m_edges) {
            const auto& nodes = data.g.edges_nodes[get_index(e)];
            fn(ed.m_node_indices[get_index(nodes[0].first)], 0u,
               ed.m_node_indices[get_index(nodes[1].first)], 0u);
        }
    }
};

void graph_component_editor_data::build_graph(
  const graph_component& data) noexcept
{
    constexpr auto none = std::numeric_limits<u32>::max();

    vector<graph_node_id> nodes;
    nodes.reserve(data.g.nodes.size());
    for (const auto id : data.g.nodes)
        nodes.push_back(id);

    if (not std::ranges::equal(nodes, m_nodes)) {
        m_nodes = std::move(nodes);
        m_editor.reset();

        m_node_indices.clear();
        m_node_indices.resize(data.g.nodes.capacity(), none);
        for (u32 i = 0, e = static_cast<u32>(m_nodes.size()); i != e; ++i)
            m_node_indices[get_index(m_nodes[i])] = i;
    }

    if (std::cmp_less(m_positions.size(), data.g.nodes.capacity())) {
        m_positions.resize(data.g.nodes.capacity(), ImVec2());
        m_position_ids.resize(data.g.nodes.capacity(),
                              undefined<graph_node_id>());
    }

    const auto old_size = m_edges.size();
    m_edges.clear();
    for (const auto id : data.g.edges) {
        const auto& ends = data.g.edges_nodes[get_index(id)];

        if (data.g.nodes.exists(ends[0].first) and
            data.g.nodes.exists(ends[1].first))
            m_edges.push_back(id);
    }

    if (m_edges.size() != old_size)
        m_editor.clear_selection();
}

bool graph_component_editor_data::show_graph(application& app,
                                             component& /*compo*/,
                                             graph_component& data) noexcept
{
    auto update = 0;

    build_graph(data);

    auto       v  = view{ .app = app, .ed = *this, .data = data };
    const auto ev = m_editor.show("graph-editor", v);

    selected_nodes.clear();
    for (const auto i : m_editor.selected_nodes())
        selected_nodes.emplace_back(m_nodes[i]);

    selected_edges.clear();
    for (const auto i : m_editor.selected_links())
        selected_edges.emplace_back(m_edges[i]);

    if (m_editor.is_background_hovered() and
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("Canvas-Context");

    if (ImGui::BeginPopup("Canvas-Context")) {
        const auto click = m_editor.to_world(
          ImGui::GetMousePosOnOpeningCurrentPopup());

        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("New node")) {
                if (auto id = data.g.alloc_node(); id.has_value()) {
                    data.g.node_positions[get_index(*id)] = { click.x, click.y,
                                                              0.f };
                    ++update;
                } else {
                    log(
                      log_level::error,
                      [](auto& m, auto& id) {
                          format(m, "graph-editor: fail to add more node {}",
                                 id.error());
                      },
                      id);
                }
            }

            if (not selected_nodes.empty() and ImGui::MenuItem("Connect")) {
                const auto e = selected_nodes.size();
                for (sz i = 0; i < e; ++i) {
                    for (sz j = i + 1; j < e; ++j) {
                        if (const auto ret = data.g.alloc_edge(
                              selected_nodes[i], selected_nodes[j]);
                            ret.has_error()) {
                            log(log_level::error, [ret](auto& m) {
                                format(m,
                                       "graph-editor: fail to add more edge {}",
                                       ret.error());
                            });
                            break;
                        } else {
                            ++update;
                        }
                    }
                }
                m_editor.clear_selection();
            }

            if (not selected_nodes.empty() and
                ImGui::MenuItem("Delete nodes")) {
                for (auto id : selected_nodes)
                    if (data.g.nodes.exists(id)) {
                        data.g.nodes.free(id);
                        ++update;
                    }
                m_editor.clear_selection();
            }

            if (not selected_edges.empty() and
                ImGui::MenuItem("Delete edges")) {
                for (auto id : selected_edges)
                    if (data.g.edges.exists(id)) {
                        data.g.edges.free(id);
                        ++update;
                    }
                m_editor.clear_selection();
            }

            ImGui::EndMenu();
        }

        ImGui::MenuItem("Show minimap", "", &m_editor.show_minimap);
        ImGui::TextDisabled("Shift + drag a node: new edge");

        ImGui::EndPopup();
    }

    if (ev.link_created) {
        if (const auto ret = data.g.alloc_edge(
              m_nodes[ev.link_created->out.node],
              m_nodes[ev.link_created->in.node]);
            ret.has_error())
            log(log_level::error, [ret](auto& m) {
                format(m, "graph-editor: fail to add more edge {}",
                       ret.error());
            });
        else
            ++update;
    }

    if (ev.delete_selection) {
        for (const auto id : selected_edges)
            if (data.g.edges.exists(id)) {
                data.g.edges.free(id);
                ++update;
            }

        for (const auto id : selected_nodes)
            if (data.g.nodes.exists(id)) {
                data.g.nodes.free(id);
                ++update;
            }

        m_editor.clear_selection();
    }

    if (ev.nodes_moved) {
        for (const auto id : m_nodes) {
            const auto idx = get_index(id);
            if (m_position_ids[idx] == id) {
                data.g.node_positions[idx][0] = m_positions[idx].x;
                data.g.node_positions[idx][1] = m_positions[idx].y;
            }
        }
        ++update;
    }

    return update > 0;
}

bool graph_component_editor_data::show_scale_free_menu(
  application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Generate scale free graph")) {
        HelpMarker("scale_free: graph typically has a very skewed degree "
                   "distribution, where few vertices have a very high "
                   "degree and a large number of vertices have a very "
                   "small degree. Many naturally evolving networks, such as "
                   "the World Wide Web, are scale-free graphs, making these "
                   "graphs a good model for certain networking.");

        if (ImGui::InputInt("size", &m_graph.scale.nodes)) {
            m_graph.scale.nodes = std::clamp(m_graph.scale.nodes, 1,
                                             graph_component::children_max);
            ++u;
        }

        if (ImGui::InputDouble("alpha", &m_graph.scale.alpha)) {
            m_graph.scale.alpha = std::clamp(m_graph.scale.alpha, 0.0, 1000.0);
            ++u;
        }

        if (ImGui::InputDouble("beta", &m_graph.scale.beta)) {
            m_graph.scale.beta = std::clamp(m_graph.scale.beta, 0.0, 1000.0);
            ++u;
        }

        if (auto r = app.component_sel.combobox("component", m_graph.scale.id);
            r) {
            m_graph.scale.id = r.id;
            ++u;
        }

        const auto size = ImGui::ComputeButtonSize(2);
        ImGui::BeginDisabled(task_is_running.test());
        if (ImGui::Button("generate", size)) {
            clear_selected_nodes();

            if (not task_is_running.test_and_set()) {
                if (m_graph_2nd.try_request()) {
                    app.add_gui_task([this]() {
                        graph_component new_graph;
                        auto            ret = new_graph.g.init_scale_free_graph(
                          m_graph.scale.alpha, m_graph.scale.beta,
                          m_graph.scale.id, m_graph.scale.nodes, new_graph.rng);

                        if (ret.has_error()) {
                            log(log_level::error, [ret](auto& m) {
                                format(m,
                                       "graph-editor: fail to generate scale "
                                       "free graph {}",
                                       ret.error());
                            });
                        }

                        new_graph.scale = m_graph.scale;
                        new_graph
                          .g_type = graph_component::graph_type::scale_free;
                        new_graph.update_position();
                        new_graph.assign_grid_position(distance.x, distance.y);

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("cancel", size)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    return u > 0;
}

bool graph_component_editor_data::show_small_world_menu(
  application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Generate Small world graph")) {
        HelpMarker(
          "small_world: consists of a ring graph (where each vertex is "
          "connected to its k nearest neighbors).Edges in the graph are "
          "randomly rewired to different vertices with a probability p.");

        if (ImGui::InputInt("size", &m_graph.small.nodes)) {
            ++u;
            m_graph.small.nodes = std::clamp(m_graph.small.nodes, 1,
                                             graph_component::children_max);
        }

        if (ImGui::InputDouble("probability", &m_graph.small.probability)) {
            ++u;
            m_graph.small.probability = std::clamp(m_graph.small.probability,
                                                   0.0, 1.0);
        }

        if (ImGui::InputInt("k", &m_graph.small.k, 1, 2)) {
            ++u;
            m_graph.small.k = std::clamp(m_graph.small.k, 1, 8);
        }

        if (auto r = app.component_sel.combobox("component", m_graph.small.id);
            r) {
            ++u;
            m_graph.small.id = r.id;
        }

        const auto size = ImGui::ComputeButtonSize(2);
        ImGui::BeginDisabled(task_is_running.test());
        if (ImGui::Button("generate", size)) {
            clear_selected_nodes();

            if (not task_is_running.test_and_set()) {
                if (m_graph_2nd.try_request()) {
                    app.add_gui_task([this]() {
                        graph_component new_graph;
                        if (const auto ret = new_graph.g.init_small_world_graph(
                              m_graph.small.probability, m_graph.small.k,
                              m_graph.small.id, m_graph.small.nodes,
                              new_graph.rng);
                            ret.has_error()) {
                            log(log_level::error, [ret](auto& m) {
                                format(m,
                                       "graph-editor: fail to generate small "
                                       "world graph {}",
                                       ret.error());
                            });
                        }
                        new_graph.small = m_graph.small;
                        new_graph
                          .g_type = graph_component::graph_type::small_world;
                        new_graph.update_position();
                        new_graph.assign_grid_position(distance.x, distance.y);

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("cancel", size)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    return u > 0;
}

bool graph_component_editor_data::show_dot_file_menu(application& app) noexcept
{
    auto u = 0;

    if (ImGui::BeginMenu("Read Dot graph")) {
        HelpMarker("dot: consists of a file defining the graph "
                   "according to the dot file format.");

        auto close = app.mod.files.read([&](const auto& fs,
                                            auto) noexcept -> bool {
            const auto selected = file_select.combobox_ro(
              fs, file_type::dot_file,
              file_selector::flags(file_selector::flag::show_load_button,
                                   file_selector::flag::show_cancel_button));

            if (selected.save and not task_is_running.test_and_set()) {
                m_graph.dot.file = selected.file_id;

                if (m_graph_2nd.try_request()) {
                    app.add_gui_task([&app, this]() {
                        graph_component new_graph;

                        app.mod.files.read([&](const auto& fs, auto) noexcept {
                            if (auto path = fs.get_fs_path(m_graph.dot.file)) {
                                app.mod.ids.read(
                                  [&](const auto& ids, auto) noexcept {
                                      if (auto dot_graph = parse_dot_file(
                                            fs, ids, *path)) {

                                          new_graph.g   = std::move(*dot_graph);
                                          new_graph.dot = m_graph.dot;
                                          new_graph.g_type = graph_component::
                                            graph_type::dot_file;
                                          new_graph.update_position();
                                      }
                                  });
                            }
                        });

                        m_graph_2nd.fulfill(std::move(new_graph));
                        task_is_running.clear();
                    });
                }
            }

            return selected.close;
        });

        if (close)
            ImGui::CloseCurrentPopup();

        ImGui::EndMenu();
    }

    return u > 0;
}

void save_dot_file(application&       app,
                   const graph&       g,
                   const file_path_id file_id) noexcept
{
    auto g_ptr = std::make_unique<graph>(g);

    app.add_gui_task([&app, g = std::move(g_ptr), file_id]() {
        app.mod.files.read([&](const auto& fs, auto) {
            app.mod.ids.read([&](const auto& ids, auto) {
                const auto file = fs.get_fs_path(file_id);
                if (file.has_error()) {
                    log(log_level::error, [&](auto& m) {
                        format(m, "graph-editor: Fail to open file {}\n",
                               ordinal(file_id));
                    });

                    return;
                }

                const auto ret = write_dot_file(fs, ids, *g, *file);
                if (ret.has_error()) {
                    log(log_level::error, [&](auto& m) {
                        format(m, "graph-editor: Fail to write file {}\n",
                               file->sv());
                    });
                }
            });
        });
    });
}

bool graph_component_editor_data::graph_component_editor_data::show(
  application& app,
  component&   compo) noexcept
{
    int u = 0;

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Model")) {
            u += show_scale_free_menu(app);
            u += show_small_world_menu(app);
            u += show_dot_file_menu(app);
            ImGui::Separator();

            const auto already_saved_defined = is_defined(m_graph.dot.file);
            auto       close                 = false;

            ImGui::BeginDisabled(not already_saved_defined);
            if (ImGui::MenuItem("Save")) {
                save_dot_file(app, m_graph.g, m_graph.dot.file);
            }
            ImGui::EndDisabled();

            if (ImGui::BeginMenu("Save as...")) {
                close = app.mod.files.read(
                  [&](const auto& fs, auto) noexcept -> bool {
                      const auto selected = file_select.combobox(
                        app, fs, file_type::dot_file,
                        file_selector::flags(
                          file_selector::flag::show_save_button,
                          file_selector::flag::show_cancel_button));

                      if (selected.save) {
                          m_graph.dot.file = selected.file_id;
                          save_dot_file(app, m_graph.g, m_graph.dot.file);
                      }

                      return selected.close;
                  });

                if (close)
                    ImGui::CloseCurrentPopup();

                ImGui::EndMenu();
            }

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                    ImVec2(0.5f, 0.5f));

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Display##Menu")) {
            if (ImGui::MenuItem("Grid layout"))
                m_editor.request_grid_layout();

            if (ImGui::MenuItem("Automatic layout"))
                m_editor.request_force_layout();

            if (ImGui::MenuItem("Auto-fit"))
                m_editor.request_fit();

            ImGui::SeparatorText("Drawing settings");

            float wdistance[2] = { distance.x, distance.y };
            if (ImGui::InputFloat2("Distance between node", wdistance)) {
                distance.x = ImClamp(wdistance[0], 0.1f, 100.f);
                distance.y = ImClamp(wdistance[0], 0.1f, 100.f);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Connectivity")) {
            show_project_params_menu(m_graph);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    u += show_graph(app, compo, m_graph);

    return u > 0;
}

graph_component_editor_data::graph_component_editor_data(
  const component_id       id_,
  const graph_component_id graph_id_) noexcept
  : graph_id(graph_id_)
  , m_id(id_)
{
    // The vertices have a unit area: the layouts place them in a small world.
    m_editor.min_zoom = 0.01f;
    m_editor.max_zoom = 1000.f;
}

bool graph_component_editor_data::show(component_editor& ed,
                                       const component_access& /*ids*/,
                                       component& compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);
    int   u   = 0;

    if (auto new_graph_opt = m_graph_2nd.try_take()) {
        const auto t_l = m_graph.top_left_limit;
        const auto b_r = m_graph.bottom_right_limit;
        m_graph        = *new_graph_opt;

        // The positions of the view are read again from the new graph.
        std::ranges::fill(m_position_ids, undefined<graph_node_id>());

        if (t_l != m_graph.top_left_limit or b_r != m_graph.bottom_right_limit)
            m_editor.request_fit();

        ++u;
    } else {
        read(app, compo);
    }

    if (ImGui::BeginChild("##graph-ed", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_MenuBar)) {
        u += show(app, compo);
    }
    ImGui::EndChild();

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

bool graph_component_editor_data::show_selected_nodes(
  component_editor&       ed,
  const component_access& ids,
  component&              compo) noexcept
{
    auto& app = container_of(&ed, &application::component_ed);

    read(app, compo);

    int u = 0;
    if (ImGui::TreeNodeEx("selected nodes")) {
        for (const auto id : selected_nodes) {
            if (m_graph.g.nodes.exists(id)) {
                const auto idx = get_index(id);
                ImGui::PushID(idx);

                name_str name = m_graph.g.node_names[idx];
                if (ImGui::InputFilteredString("name", name)) {
                    ++u;
                    m_graph.g.node_names[idx] = m_graph.g.buffer.append(
                      name.sv());
                }

                if (auto area = m_graph.g.node_areas[idx];
                    ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
                    m_graph.g.node_areas[idx] = area;
                    ++u;
                }

                if (auto r = app.component_sel.combobox(
                      "component", m_graph.g.node_components[idx]);
                    r) {
                    m_graph.g.node_components[idx] = r.id;
                    ++u;
                }

                ImGui::LabelFormat("position", "x: {} y: {}",
                                   m_graph.g.node_positions[idx][0],
                                   m_graph.g.node_positions[idx][1]);

                if (ids.exists(m_graph.g.node_components[idx])) {
                    auto& compo = ids
                                    .components[m_graph.g.node_components[idx]];
                    if (not compo.x.empty()) {
                        if (ImGui::TreeNodeEx("input ports")) {
                            const auto& xnames = compo.x
                                                   .template get<port_str>();
                            for (const auto id : compo.x) {
                                ImGui::TextFormat("{}", xnames[id].sv());
                            }
                            ImGui::TreePop();
                        }
                    }

                    if (not compo.y.empty()) {
                        if (ImGui::TreeNodeEx("output ports")) {
                            const auto& ynames = compo.y
                                                   .template get<port_str>();
                            for (const auto id : compo.y) {
                                ImGui::TextFormat("{}", ynames[id].sv());
                            }
                            ImGui::TreePop();
                        }
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::TreePop();
    }

    ImGui::Spacing();

    if (ImGui::TreeNodeEx("apply for all selected")) {
        if (auto r = app.component_sel.combobox("component",
                                                undefined<component_id>());
            r) {
            for (const auto id : selected_nodes) {
                if (m_graph.g.nodes.exists(id)) {
                    m_graph.g.node_components[id] = r.id;
                    ++u;
                }
            }
        }

        static auto area = 1.f;
        if (ImGui::DragFloat("area", &area, 0.001f, 0.f, FLT_MAX)) {
            for (const auto id : selected_nodes) {
                if (m_graph.g.nodes.exists(id)) {
                    m_graph.g.node_areas[id] = area;
                    ++u;
                }
            }
        }

        ImGui::TreePop();
    }

    if (u > 0) {
        write(app, compo);
        return true;
    }

    return false;
}

void graph_component_editor_data::clear_selected_nodes() noexcept
{
    selected_edges.clear();
    selected_nodes.clear();
}

void graph_component_editor_data::read(application& app,
                                       component&   compo) noexcept
{
    app.mod.ids.read([&](const auto& ids, auto version) noexcept {
        if (m_version != version)
            m_version = version;

        if (not ids.exists(m_id))
            return;

        debug::ensure(compo.type == component_type::graph);

        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id)) {
            m_graph = *g;
        }
    });
}

void graph_component_editor_data::write(application& app,
                                        component&   compo) noexcept
{
    auto c_ptr = std::make_unique<component>(std::move(compo));
    auto g_ptr = std::make_unique<graph_component>(std::move(m_graph));

    app.add_gui_task(
      [&app, c = std::move(c_ptr), g = std::move(g_ptr), id = m_id]() {
          app.mod.ids.write([&](auto& ids) {
              if (not ids.exists(id))
                  return;

              ids.components[id] = *c;

              if (c->type == component_type::graph) {
                  const auto graph_id = c->id.graph_id;

                  if (auto* graph = ids.graph_components.try_to_get(graph_id))
                      *graph = *g;
              }
          });
      });
}

} // namespace irt
