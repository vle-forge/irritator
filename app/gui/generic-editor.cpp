// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"

#include <cstring>

namespace irt {

static inline const u32 component_input_ports[8] = {
    0b11111111111111111111111111100000, 0b11111111111111111111111111100001,
    0b11111111111111111111111111100010, 0b11111111111111111111111111100011,
    0b11111111111111111111111111100100, 0b11111111111111111111111111100101,
    0b11111111111111111111111111100110, 0b11111111111111111111111111100111,
};

static inline const u32 component_output_ports[8] = {
    0b11111111111111111111111111110000, 0b11111111111111111111111111110001,
    0b11111111111111111111111111110010, 0b11111111111111111111111111110011,
    0b11111111111111111111111111110100, 0b11111111111111111111111111110101,
    0b11111111111111111111111111110110, 0b11111111111111111111111111110111,
};

inline bool is_component_input_or_output(const int node_id) noexcept
{
    return static_cast<u32>(node_id) >= 0b11111111111111111111111111100000;
}

inline int pack_component_input(const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);
    return static_cast<int>(component_input_ports[port]);
}

inline int pack_component_output(const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);
    return static_cast<int>(component_output_ports[port]);
}

inline int unpack_component_input(const int node_id) noexcept
{
    irt_assert(is_component_input_or_output(node_id));

    const u32 index = static_cast<u32>(node_id);
    const u32 mask  = 0b11111;
    const u32 raw   = index & mask;

    irt_assert(raw < 8);
    return static_cast<int>(raw);
}

inline int unpack_component_output(const int node_id) noexcept
{
    irt_assert(is_component_input_or_output(node_id));

    const u32 index = static_cast<u32>(node_id);
    const u32 mask  = 0b11111;
    const u32 raw   = index & mask;

    irt_assert(raw >= 16);

    return static_cast<int>(raw - 16);
}

inline int pack_in(const child_id id, const i8 port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    u32 port_index = static_cast<u32>(port);
    u32 index      = static_cast<u32>(get_index(id));

    return static_cast<int>((index << 5u) | port_index);
}

inline int pack_out(const child_id id, const i8 port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    u32 port_index = 8u + static_cast<u32>(port);
    u32 index      = static_cast<u32>(get_index(id));

    return static_cast<int>((index << 5u) | port_index);
}

inline void unpack_in(const int node_id, u32* index, i8* port) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    *port  = static_cast<i8>(real_node_id & 7u);
    *index = static_cast<u32>(real_node_id >> 5u);

    irt_assert((real_node_id & 8u) == 0);
}

inline void unpack_out(const int node_id, u32* index, i8* port) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    *port  = static_cast<i8>(real_node_id & 7u);
    *index = static_cast<u32>(real_node_id >> 5u);

    irt_assert((real_node_id & 8u) != 0);
}

inline int pack_node(const child_id id) noexcept
{
    return static_cast<int>(get_index(id));
}

inline child* unpack_node(const int                          node_id,
                          const data_array<child, child_id>& data) noexcept
{
    return data.try_to_get(static_cast<u32>(node_id));
}

template<typename Dynamics>
static void add_input_attribute(const Dynamics& dyn, child_id id) noexcept
{
    if constexpr (has_input_port<Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        irt_assert(length(dyn.x) < 8);

        for (int i = 0; i < length(dyn.x); ++i) {
            ImNodes::BeginInputAttribute(pack_in(id, static_cast<i8>(i)),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
        }
    }
}

template<typename Dynamics>
static void add_output_attribute(const Dynamics& dyn, child_id id) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();
        const auto   e     = length(dyn.y);

        irt_assert(names != nullptr);
        irt_assert(0 <= e && e < 8);

        for (int i = 0; i != e; ++i) {
            ImNodes::BeginOutputAttribute(pack_out(id, static_cast<i8>(i)),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

static bool show_connection(modeling&         mod,
                            const connection& con,
                            connection_id     id) noexcept
{
    const auto idx    = get_index(id);
    const auto con_id = static_cast<int>(idx);

    switch (con.type) {
    case connection::connection_type::internal:
        if (auto* s = mod.children.try_to_get(con.internal.src); s) {
            if (auto* d = mod.children.try_to_get(con.internal.dst); d) {
                ImNodes::Link(
                  con_id,
                  pack_out(con.internal.src, con.internal.index_src),
                  pack_in(con.internal.dst, con.internal.index_dst));
                return true;
            }
        }
        break;

    case connection::connection_type::input:
        if (auto* d = mod.children.try_to_get(con.input.dst); d) {
            ImNodes::Link(con_id,
                          pack_component_input(con.input.index),
                          pack_in(con.input.dst, con.input.index_dst));
            return true;
        }
        break;

    case connection::connection_type::output:
        if (auto* s = mod.children.try_to_get(con.internal.src); s) {
            ImNodes::Link(con_id,
                          pack_out(con.output.src, con.output.index_src),
                          pack_component_output(con.output.index));
            return true;
        }
        break;
    }

    return false;
}

static void show(component_editor&              ed,
                 generic_component_editor_data& data,
                 component&                     parent,
                 model&                         mdl,
                 child& /*c*/,
                 child_id id) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_model_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      app->mod.children_names[get_index(id)].sv(),
                      dynamics_type_names[ordinal(mdl.type)]);
    ImNodes::EndNodeTitleBar();

    dispatch(
      mdl, [&mdl, &app, &data, &parent, id]<typename Dynamics>(Dynamics& dyn) {
          add_input_attribute(dyn, id);
          ImGui::PushItemWidth(120.0f);

          if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
              auto  s_compo_id = parent.id.simple_id;
              auto* s_compo = app->mod.simple_components.try_to_get(s_compo_id);
              if (s_compo) {
                  if (auto* machine = app->mod.hsms.try_to_get(dyn.id);
                      machine) {
                      show_dynamics_inputs(*app,
                                           app->mod.components.get_id(parent),
                                           app->mod.models.get_id(mdl),
                                           *machine);
                      ImNodes::EditorContextSet(data.context);
                  }
              }
          } else {
              show_dynamics_inputs(app->mod.srcs, dyn);
          }

          ImGui::PopItemWidth();
          add_output_attribute(dyn, id);
      });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_generic(component_editor& ed,
                         generic_component_editor_data& /*data*/,
                         component& compo,
                         generic_component& /*s_compo*/,
                         child& /*c*/,
                         child_id id) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      app->mod.children_names[get_index(id)].sv(),
                      compo.name.c_str());
    ImNodes::EndNodeTitleBar();

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_in(id, static_cast<i8>(i));
        ImNodes::BeginInputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.x_names[i].c_str());
        ImNodes::EndInputAttribute();
    }

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_out(id, static_cast<i8>(i));
        ImNodes::BeginOutputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.y_names[i].c_str());
        ImNodes::EndOutputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_grid(component_editor& ed,
                      generic_component_editor_data& /*data*/,
                      component&      compo,
                      grid_component& grid,
                      child& /*c*/,
                      child_id id) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      app->mod.children_names[get_index(id)].sv(),
                      compo.name.c_str());
    ImGui::TextFormat("{}x{}", grid.row, grid.column);
    ImNodes::EndNodeTitleBar();

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_in(id, static_cast<i8>(i));
        ImNodes::BeginInputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.x_names[i].c_str());
        ImNodes::EndInputAttribute();
    }

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_out(id, static_cast<i8>(i));
        ImNodes::BeginOutputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.y_names[i].c_str());
        ImNodes::EndOutputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_graph(component_editor& ed,
                       generic_component_editor_data& /*data*/,
                       component&       compo,
                       graph_component& graph,
                       child& /*c*/,
                       child_id id) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}",
                      app->mod.children_names[get_index(id)].sv(),
                      compo.name.c_str());
    ImGui::TextFormat("{}", graph.children.size());
    ImNodes::EndNodeTitleBar();

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_in(id, static_cast<i8>(i));
        ImNodes::BeginInputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.x_names[i].c_str());
        ImNodes::EndInputAttribute();
    }

    for (u8 i = 0; i < 8; ++i) {
        auto gid = pack_out(id, static_cast<i8>(i));
        ImNodes::BeginOutputAttribute(gid, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.y_names[i].c_str());
        ImNodes::EndOutputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void update_position(application&                   app,
                            generic_component_editor_data& data,
                            generic_component&             generic) noexcept
{
    for_specified_data(
      app.mod.children, generic.children, [&app](auto& grid) noexcept {
          const auto id  = app.mod.children.get_id(grid);
          const auto idx = get_index(id);

          ImNodes::SetNodeEditorSpacePos(
            pack_node(id),
            ImVec2(app.mod.children_positions[idx].x,
                   app.mod.children_positions[idx].y));
      });

    data.force_update_position = false;
}

static void update_input_output_draggable(bool draggable) noexcept
{
    for (int i = 0, e = length(component_input_ports); i != e; ++i)
        ImNodes::SetNodeDraggable(pack_component_input(i), draggable);

    for (int i = 0, e = length(component_output_ports); i != e; ++i)
        ImNodes::SetNodeDraggable(pack_component_output(i), draggable);
}

static void update_input_output_position(generic_component_editor_data& data,
                                         float                          x1,
                                         float                          x2,
                                         float y) noexcept
{
    for (int i = 0, e = length(component_input_ports); i != e; ++i)
        ImNodes::SetNodeEditorSpacePos(
          pack_component_input(i),
          ImVec2(x1, static_cast<float>(i) * 50.f + y));

    for (int i = 0, e = length(component_output_ports); i != e; ++i)
        ImNodes::SetNodeEditorSpacePos(
          pack_component_output(i),
          ImVec2(x2, static_cast<float>(i) * 50.f + y));

    data.first_show_input_output = false;
}

static void show_graph(component_editor&              ed,
                       generic_component_editor_data& data,
                       component&                     parent,
                       generic_component&             s_parent) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    const auto width  = ImGui::GetContentRegionAvail().x;
    const auto pos    = ImNodes::EditorContextGetPanning();
    const auto pos_x1 = pos.x + 10.f;
    const auto pos_x2 = pos.x + width - 50.f;

    if (data.force_update_position)
        update_position(*app, data, s_parent);

    if (data.show_input_output) {
        update_input_output_draggable(data.fix_input_output);

        if (data.first_show_input_output)
            update_input_output_position(data, pos_x1, pos_x2, pos.y);
    }

    if (data.show_input_output) {
        for (int i = 0, e = length(component_input_ports); i != e; ++i) {
            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_component_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_component_color);

            ImNodes::BeginNode(pack_component_input(i));
            ImNodes::BeginOutputAttribute(pack_component_input(i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(parent.x_names[i].c_str());
            ImNodes::EndOutputAttribute();
            ImNodes::EndNode();
        }

        for (int i = 0, e = length(component_output_ports); i != e; ++i) {
            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_component_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_component_color);

            ImNodes::BeginNode(pack_component_output(i));
            ImNodes::BeginInputAttribute(pack_component_output(i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(parent.y_names[i].c_str());
            ImNodes::EndInputAttribute();
            ImNodes::EndNode();
        }
    }

    for (auto child_id : s_parent.children) {
        auto* c = app->mod.children.try_to_get(child_id);
        if (!c)
            continue;

        if (c->type == child_type::model) {
            auto id = c->id.mdl_id;
            if (auto* mdl = app->mod.models.try_to_get(id); mdl)
                show(ed, data, parent, *mdl, *c, child_id);
        } else {
            auto id = c->id.compo_id;
            if (auto* compo = app->mod.components.try_to_get(id); compo) {
                switch (compo->type) {
                case component_type::none:
                    break;

                case component_type::simple:
                    if (auto* s_compo = app->mod.simple_components.try_to_get(
                          compo->id.simple_id)) {
                        show_generic(ed, data, *compo, *s_compo, *c, child_id);
                    }
                    break;

                case component_type::grid:
                    if (auto* s_compo = app->mod.grid_components.try_to_get(
                          compo->id.grid_id)) {
                        show_grid(ed, data, *compo, *s_compo, *c, child_id);
                    }
                    break;

                case component_type::graph:
                    if (auto* s_compo = app->mod.graph_components.try_to_get(
                          compo->id.graph_id)) {
                        show_graph(ed, data, *compo, *s_compo, *c, child_id);
                    }
                    break;

                case component_type::internal:
                    break;
                }
            }
        }
    }

    for_specified_data(
      app->mod.connections, s_parent.connections, [&](auto& con) noexcept {
          auto connection_id = app->mod.connections.get_id(con);

          if (!show_connection(app->mod, con, connection_id))
              app->mod.connections.free(con);
      });
}

static void add_popup_menuitem(component_editor&              ed,
                               generic_component_editor_data& data,
                               component&                     parent,
                               generic_component&             s_parent,
                               dynamics_type                  type,
                               ImVec2                         click_pos)
{
    auto* app = container_of(&ed, &application::component_ed);

    if (!app->mod.models.can_alloc(1)) {
        auto* app = container_of(&ed, &application::component_ed);
        auto& n   = app->notifications.alloc();
        n.level   = log_level::error;
        n.title   = "can not allocate a new model";
        return;
    }

    if (ImGui::MenuItem(dynamics_type_names[ordinal(type)])) {
        auto& child    = app->mod.alloc(s_parent, type);
        auto  child_id = app->mod.children.get_id(child);

        parent.state = component_status::modified;
        app->mod.children_positions[get_index(child_id)].x = click_pos.x;
        app->mod.children_positions[get_index(child_id)].y = click_pos.y;
        data.update_position();

        auto* app = container_of(&ed, &application::component_ed);
        auto& n   = app->notifications.alloc();
        n.level   = log_level::debug;
        format(n.title, "new model {} added", ordinal(child_id));
    }
}

static void add_popup_menuitem(component_editor&              ed,
                               generic_component_editor_data& data,
                               component&                     parent,
                               generic_component&             s_parent,
                               int                            type,
                               ImVec2                         click_pos)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, data, parent, s_parent, d_type, click_pos);
}

static void compute_grid_layout(settings_window&               settings,
                                generic_component_editor_data& data,
                                generic_component&             s_compo) noexcept
{
    auto*      app   = container_of(&settings, &application::settings_wnd);
    const auto size  = s_compo.children.ssize();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return;

    const auto column    = std::floor(std::sqrt(fsize));
    auto       line      = column;
    auto       remaining = fsize - (column * line);

    const auto panning = ImNodes::EditorContextGetPanning();
    auto       new_pos = panning;

    child_id c_id    = undefined<child_id>();
    int      c_index = 0;

    for (float i = 0.f; i < line; ++i) {
        new_pos.y = panning.y + i * settings.grid_layout_y_distance;

        for (float j = 0.f; j < column; ++j) {
            if (c_index >= s_compo.children.ssize())
                break;

            c_id = s_compo.children[c_index++];

            new_pos.x = panning.x + j * settings.grid_layout_x_distance;
            app->mod.children_positions[get_index(c_id)].x = new_pos.x;
            app->mod.children_positions[get_index(c_id)].y = new_pos.y;
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + column * settings.grid_layout_y_distance;

    for (float j = 0.f; j < remaining; ++j) {
        if (c_index >= s_compo.children.ssize())
            break;

        c_id = s_compo.children[c_index++];

        new_pos.x = panning.x + j * settings.grid_layout_x_distance;
        app->mod.children_positions[get_index(c_id)].x = new_pos.x;
        app->mod.children_positions[get_index(c_id)].y = new_pos.y;
    }

    data.update_position();
}

static status add_component_to_current(component_editor&              ed,
                                       generic_component_editor_data& data,
                                       component&                     parent,
                                       generic_component& parent_compo,
                                       component&         compo_to_add,
                                       ImVec2             click_pos = ImVec2())
{
    auto*      app             = container_of(&ed, &application::component_ed);
    const auto compo_to_add_id = app->mod.components.get_id(compo_to_add);

    if (app->mod.can_add(parent, compo_to_add)) {
        auto* app   = container_of(&ed, &application::component_ed);
        auto& notif = app->notifications.alloc(log_level::error);
        notif.title = "Fail to add component";
        format(notif.message,
               "Irritator does not accept recursive component {}",
               compo_to_add.name.sv());
        app->notifications.enable(notif);
        return status::gui_not_enough_memory; //! @TODO replace with correct
                                              //! error
    }

    auto& c    = app->mod.alloc(parent_compo, compo_to_add_id);
    auto  c_id = app->mod.children.get_id(c);

    app->mod.children_positions[get_index(c_id)].x = click_pos.x;
    app->mod.children_positions[get_index(c_id)].y = click_pos.y;
    data.update_position();

    return status::success;
}

static void show_popup_all_component_menuitem(
  component_editor&              ed,
  generic_component_editor_data& data,
  component&                     parent,
  generic_component&             s_parent) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    for (auto id : app->mod.component_repertories) {
        static small_string<31> s; //! @TODO remove this variable
        small_string<31>*       select;

        auto& reg_dir = app->mod.registred_paths.get(id);
        if (reg_dir.name.empty()) {
            format(s, "{}", ordinal(id));
            select = &s;
        } else {
            select = &reg_dir.name;
        }

        ImGui::PushID(&reg_dir);
        if (ImGui::BeginMenu(select->c_str())) {
            for (auto dir_id : reg_dir.children) {
                auto* dir = app->mod.dir_paths.try_to_get(dir_id);
                if (!dir)
                    break;

                if (ImGui::BeginMenu(dir->path.c_str())) {
                    for (auto file_id : dir->children) {
                        auto* file = app->mod.file_paths.try_to_get(file_id);
                        if (!file)
                            break;

                        auto* compo =
                          app->mod.components.try_to_get(file->component);
                        if (!compo)
                            break;

                        if (ImGui::MenuItem(file->path.c_str())) {
                            add_component_to_current(
                              ed, data, parent, s_parent, *compo);
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::PopID();
    }

    if (ImGui::BeginMenu("Not saved")) {
        component* compo = nullptr;
        while (app->mod.components.next(compo)) {
            if (compo->state == component_status::modified) {
                ImGui::PushID(compo);
                if (ImGui::MenuItem(compo->name.c_str())) {
                    add_component_to_current(
                      ed, data, parent, s_parent, *compo);
                }
                ImGui::PopID();
            }
        }

        ImGui::EndMenu();
    }
}

static void show_popup_menuitem(component_editor&              ed,
                                generic_component_editor_data& data,
                                component&                     parent,
                                generic_component& s_parent) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        if (ImGui::MenuItem("Show component input/output ports",
                            nullptr,
                            &data.show_input_output)) {
            data.first_show_input_output = true;
        }

        ImGui::MenuItem(
          "Fix component input/output ports", nullptr, &data.fix_input_output);

        ImGui::Separator();

        if (ImGui::MenuItem("Force grid layout")) {
            auto* app = container_of(&ed, &application::component_ed);
            compute_grid_layout(app->settings_wnd, data, s_parent);
        }

        ImGui::Separator();

        auto* app = container_of(&ed, &application::component_ed);
        if (ImGui::MenuItem("Add grid component")) {
            if (!app->mod.grid_components.can_alloc() ||
                !app->mod.components.can_alloc() ||
                !app->mod.children.can_alloc()) {
                auto* app = container_of(&ed, &application::component_ed);
                auto& n   = app->notifications.alloc();
                n.level   = log_level::error;
                n.title   = "can not allocate a new grid component";
            } else {
                auto& grid    = app->mod.grid_components.alloc();
                auto  grid_id = app->mod.grid_components.get_id(grid);
                grid.row      = 4;
                grid.column   = 4;

                auto& compo      = app->mod.components.alloc();
                compo.name       = "Grid";
                compo.type       = component_type::grid;
                compo.id.grid_id = grid_id;

                add_component_to_current(
                  ed, data, parent, s_parent, compo, click_pos);
            }
        }

        ImGui::Separator();

        show_popup_all_component_menuitem(ed, data, parent, s_parent);

        ImGui::Separator();

        if (ImGui::MenuItem("Grid generator"))
            app->grid_dlg.load(app, &s_parent);
        if (ImGui::MenuItem("Graph generator"))
            app->graph_dlg.load(app, &s_parent);

        ImGui::Separator();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i < e; ++i)
                add_popup_menuitem(ed, data, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, data, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, data, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::integrator, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::quantifier, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::adder_2, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::adder_3, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::adder_4, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::mult_2, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::mult_3, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::mult_4, click_pos);
            add_popup_menuitem(
              ed, data, parent, s_parent, dynamics_type::cross, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            add_popup_menuitem(ed,
                               data,
                               parent,
                               s_parent,
                               dynamics_type::logical_and_2,
                               click_pos);
            add_popup_menuitem(ed,
                               data,
                               parent,
                               s_parent,
                               dynamics_type::logical_or_2,
                               click_pos);
            add_popup_menuitem(ed,
                               data,
                               parent,
                               s_parent,
                               dynamics_type::logical_and_3,
                               click_pos);
            add_popup_menuitem(ed,
                               data,
                               parent,
                               s_parent,
                               dynamics_type::logical_or_3,
                               click_pos);
            add_popup_menuitem(ed,
                               data,
                               parent,
                               s_parent,
                               dynamics_type::logical_invert,
                               click_pos);
            ImGui::EndMenu();
        }

        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::counter, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::queue, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::dynamic_queue, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::priority_queue, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::generator, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::constant, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::time_func, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::accumulator_2, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::filter, click_pos);
        add_popup_menuitem(
          ed, data, parent, s_parent, dynamics_type::hsm_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

static void is_link_created(application& app,
                            generic_component_editor_data& /*data*/,
                            component&         parent,
                            generic_component& s_parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        u32 index_src, index_dst;
        i8  port_src_index, port_dst_index;

        if (!app.mod.connections.can_alloc()) {
            auto& n = app.notifications.alloc(log_level::error);
            n.title = "Not enough connection slot in this component";
            format(n.message,
                   "All connections slots ({}) are used.",
                   s_parent.connections.capacity());
            app.notifications.enable(n);
            return;
        }

        if (is_component_input_or_output(start)) {
            if (is_component_input_or_output(end)) {
                auto& n = app.notifications.alloc(log_level::error);
                n.title = "Can not connect component input on output ports";
                app.notifications.enable(n);
                return;
            }

            auto index = unpack_component_input(start);
            unpack_in(end, &index_dst, &port_dst_index);

            auto* child_dst = app.mod.children.try_to_get(index_dst);
            if (child_dst == nullptr)
                return;

            auto child_dst_id = app.mod.children.get_id(*child_dst);
            if (is_success(app.mod.connect_input(s_parent,
                                                 static_cast<i8>(index),
                                                 child_dst_id,
                                                 port_dst_index)))
                parent.state = component_status::modified;
        } else {
            if (is_component_input_or_output(end)) {
                auto index = unpack_component_output(end);
                unpack_out(start, &index_src, &port_src_index);
                auto* child_src = app.mod.children.try_to_get(index_src);
                if (child_src == nullptr)
                    return;

                auto child_src_id = app.mod.children.get_id(*child_src);
                if (is_success(app.mod.connect_output(s_parent,
                                                      child_src_id,
                                                      port_src_index,
                                                      static_cast<i8>(index))))
                    parent.state = component_status::modified;
            } else {
                unpack_out(start, &index_src, &port_src_index);
                unpack_in(end, &index_dst, &port_dst_index);

                auto* child_src = app.mod.children.try_to_get(index_src);
                auto* child_dst = app.mod.children.try_to_get(index_dst);

                if (!(child_src != nullptr && child_dst != nullptr))
                    return;

                auto child_src_id = app.mod.children.get_id(*child_src);
                auto child_dst_id = app.mod.children.get_id(*child_dst);

                if (is_success(app.mod.connect(s_parent,
                                               child_src_id,
                                               port_src_index,
                                               child_dst_id,
                                               port_dst_index)))
                    parent.state = component_status::modified;
            }
        }
    }
}

static void is_link_destroyed(modeling&  mod,
                              component& parent,
                              generic_component& /*s_parent*/) noexcept
{
    int link_id;
    if (ImNodes::IsLinkDestroyed(&link_id)) {
        const auto link_id_correct = static_cast<u32>(link_id);
        if (auto* con = mod.connections.try_to_get(link_id_correct); con) {
            mod.connections.free(*con);
            parent.state = component_status::modified;
        }
    }
}

static void remove_nodes(modeling&                      mod,
                         generic_component_editor_data& data,
                         component&                     parent) noexcept
{
    for (i32 i = 0, e = data.selected_nodes.size(); i != e; ++i) {
        if (auto* child = unpack_node(data.selected_nodes[i], mod.children);
            child) {
            mod.free(*child);
            parent.state = component_status::modified;
        }
    }

    data.selected_nodes.clear();
    ImNodes::ClearNodeSelection();

    parent.state = component_status::modified;
}

static void remove_links(modeling&                      mod,
                         generic_component_editor_data& data,
                         component&                     parent,
                         generic_component& /*s_parent*/) noexcept
{
    std::sort(data.selected_links.begin(),
              data.selected_links.end(),
              std::greater<int>());

    for (i32 i = 0, e = data.selected_links.size(); i != e; ++i) {
        const auto link_id = static_cast<u32>(data.selected_links[i]);

        if (auto* con = mod.connections.try_to_get(link_id); con) {
            mod.connections.free(*con);
            parent.state = component_status::modified;
        }
    }

    data.selected_links.clear();
    ImNodes::ClearLinkSelection();

    parent.state = component_status::modified;
}

static void remove_component_input_output(ImVector<int>& v) noexcept
{
    int i = 0;
    while (i < v.size()) {
        if (is_component_input_or_output(v[i]))
            v.erase(v.Data + i);
        else
            ++i;
    };
}

static void show_component_editor(component_editor&              ed,
                                  generic_component_editor_data& data,
                                  component&                     compo,
                                  generic_component& s_compo) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    ImNodes::EditorContextSet(data.context);
    ImNodes::BeginNodeEditor();

    if (app->grid_dlg.is_running) {
        app->grid_dlg.show();

        if (app->grid_dlg.is_ok && !app->grid_dlg.is_running) {
            auto size = s_compo.children.size();
            app->grid_dlg.save();
            app->grid_dlg.is_ok = false;
            data.update_position();

            for (sz i = size, e = s_compo.children.size(); i != e; ++i) {
                if_data_exists_do(
                  app->mod.children,
                  s_compo.children[i],
                  [&](auto& c) noexcept {
                      if ((c.type == child_type::model &&
                           app->mod.models.try_to_get(c.id.mdl_id) !=
                             nullptr) ||
                          (c.type == child_type::component &&
                           app->mod.components.try_to_get(c.id.compo_id) !=
                             nullptr))
                          app->mod.children_positions[get_index(
                            s_compo.children[i])] = { i * 30.f, i * 10.f };
                  });
            }
        }
    }

    if (app->graph_dlg.is_running) {
        app->graph_dlg.show();

        if (app->graph_dlg.is_ok && !app->graph_dlg.is_running) {
            auto size = s_compo.children.size();
            app->graph_dlg.save();
            app->graph_dlg.is_ok = false;
            data.update_position();

            for (sz i = size, e = s_compo.children.size(); i != e; ++i) {
                if_data_exists_do(
                  app->mod.children,
                  s_compo.children[i],
                  [&](auto& c) noexcept {
                      if ((c.type == child_type::model &&
                           app->mod.models.try_to_get(c.id.mdl_id) !=
                             nullptr) ||
                          (c.type == child_type::component &&
                           app->mod.components.try_to_get(c.id.compo_id) !=
                             nullptr))
                          app->mod.children_positions[get_index(
                            s_compo.children[i])] = { i * 30.f, i * 10.f };
                  });
            }
        }
    }

    show_popup_menuitem(ed, data, compo, s_compo);
    show_graph(ed, data, compo, s_compo);

    if (data.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    is_link_created(*app, data, compo, s_compo);
    is_link_destroyed(app->mod, compo, s_compo);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        data.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(data.selected_nodes.begin());
        remove_component_input_output(data.selected_nodes);
    } else {
        data.selected_nodes.clear();
    }

    if (num_selected_links > 0) {
        data.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(data.selected_links.begin());
    } else {
        data.selected_links.clear();
    }

    if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
        if (num_selected_nodes > 0)
            remove_nodes(app->mod, data, compo);
        else if (num_selected_links > 0)
            remove_links(app->mod, data, compo, s_compo);
    }
}

generic_component_editor_data::generic_component_editor_data(
  const component_id id_) noexcept
  : m_id{ id_ }
{
    context = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |=
      ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;

    first_show_input_output = true;
    update_position();
}

generic_component_editor_data::~generic_component_editor_data() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void generic_component_editor_data::update_position() noexcept
{
    force_update_position = true;
}

void generic_component_editor_data::show(component_editor& ed) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    if (auto* compo = app->mod.components.try_to_get(get_id()); compo) {
        const auto s_id = compo->id.simple_id;

        if (auto* s = app->mod.simple_components.try_to_get(s_id); s)
            show_component_editor(ed, *this, *compo, *s);
    }
}

} // namespace irt