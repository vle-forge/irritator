// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "fmt/core.h"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/modeling.hpp"
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
                return false;
            }
        }
        break;

    case connection::connection_type::input:
        if (auto* d = mod.children.try_to_get(con.input.dst); d) {
            ImNodes::Link(con_id,
                          pack_component_input(con.input.index),
                          pack_in(con.input.dst, con.input.index_dst));
            return false;
        }
        break;

    case connection::connection_type::output:
        if (auto* s = mod.children.try_to_get(con.internal.src); s) {
            ImNodes::Link(con_id,
                          pack_out(con.output.src, con.output.index_src),
                          pack_component_output(con.output.index));
            return false;
        }
        break;
    }

    return true;
}

static void show(component_editor&      ed,
                 component_editor_data& data,
                 component&             parent,
                 model&                 mdl,
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

static void show(component_editor& ed,
                 component_editor_data& /*data*/,
                 component& compo,
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

    ImGui::TextUnformatted("Empty component");

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

static void show(component_editor& ed,
                 component_editor_data& /*data*/,
                 component&         compo,
                 generic_component& s_compo,
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

    u32 input  = 0;
    u32 output = 0;

    for (auto connection_id : s_compo.connections) {
        auto* con = app->mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        switch (con->type) {
        case connection::connection_type::input:
            input |= 1 << con->input.index;
            break;

        case connection::connection_type::output:
            output |= 1 << con->output.index;
            break;

        default:
            break;
        }
    }

    for (u8 i = 0; i < 8; ++i) {
        if (input & (1 << i)) {
            auto gid = pack_in(id, static_cast<i8>(i));
            ImNodes::BeginInputAttribute(gid, ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(compo.x_names[i].c_str());
            ImNodes::EndInputAttribute();
        }
    }

    for (u8 i = 0; i < 8; ++i) {
        if (output & (1 << i)) {
            auto gid = pack_out(id, static_cast<i8>(i));
            ImNodes::BeginOutputAttribute(gid, ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(compo.y_names[i].c_str());
            ImNodes::EndOutputAttribute();
        }
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show(component_editor& ed,
                 component_editor_data& /*data*/,
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

static void show_graph(component_editor&      ed,
                       component_editor_data& data,
                       component&             parent,
                       generic_component&     s_parent) noexcept
{
    auto* app      = container_of(&ed, &application::component_ed);
    auto& settings = app->settings_wnd;

    const auto width = ImGui::GetContentRegionAvail().x;

    const auto pos    = ImNodes::EditorContextGetPanning();
    const auto pos_x1 = pos.x + 10.f;
    const auto pos_x2 = pos.x + width - 50.f;

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

            if (data.fix_input_output)
                ImNodes::SetNodeDraggable(pack_component_input(i), false);

            if (data.first_show_input_output) {
                ImNodes::SetNodeEditorSpacePos(
                  pack_component_input(i),
                  ImVec2(pos_x1, (float)i * 50.f + pos.y));
            }
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

            if (data.fix_input_output)
                ImNodes::SetNodeDraggable(pack_component_output(i), false);

            if (data.first_show_input_output) {
                ImNodes::SetNodeEditorSpacePos(
                  pack_component_output(i),
                  ImVec2(pos_x2, (float)i * 50.f + pos.y));
            }
        }

        data.first_show_input_output = false;
    }

    for (auto child_id : s_parent.children) {
        bool  to_place = false;
        auto* c        = app->mod.children.try_to_get(child_id);
        if (!c)
            continue;

        if (c->type == child_type::model) {
            auto id = c->id.mdl_id;
            if (auto* mdl = app->mod.models.try_to_get(id); mdl) {
                show(ed, data, parent, *mdl, *c, child_id);
                to_place = true;
            }
        } else {
            auto id = c->id.compo_id;
            if (auto* compo = app->mod.components.try_to_get(id); compo) {
                switch (compo->type) {
                case component_type::none:
                    show(ed, data, *compo, *c, child_id);
                    to_place = true;
                    break;

                case component_type::simple:
                    if (auto* s_compo = app->mod.simple_components.try_to_get(
                          compo->id.simple_id)) {
                        show(ed, data, *compo, *s_compo, *c, child_id);
                        to_place = true;
                    }
                    break;

                case component_type::grid:
                    if (auto* s_compo = app->mod.grid_components.try_to_get(
                          compo->id.grid_id)) {
                        show(ed, data, *compo, *s_compo, *c, child_id);
                        to_place = true;
                    }
                    break;

                case component_type::internal:
                    break;
                }
            }
        }

        if (data.force_node_position) {
            ImNodes::SetNodeEditorSpacePos(
              pack_node(child_id),
              ImVec2(app->mod.children_positions[get_index(child_id)].x,
                     app->mod.children_positions[get_index(child_id)].y));
        } else {
            if (to_place) {
                auto  pos = ImNodes::GetNodeEditorSpacePos(pack_node(child_id));
                auto& child = app->mod.children_positions[get_index(child_id)];

                if (child.x != pos.x || child.y != pos.y)
                    parent.state = component_status::modified;

                child.x = pos.x;
                child.y = pos.y;
            }
        }
    }

    data.force_node_position = false;

    int i = 0;
    while (i < s_parent.connections.ssize()) {
        auto  connection_id = s_parent.connections[i];
        auto* con           = app->mod.connections.try_to_get(connection_id);
        bool  to_del        = con == nullptr;

        if (!to_del)
            to_del = show_connection(app->mod, *con, connection_id);

        if (to_del) {
            s_parent.connections.swap_pop_back(i);
        } else {
            ++i;
        }
    }
}

static void add_popup_menuitem(component_editor&  ed,
                               component&         parent,
                               generic_component& s_parent,
                               dynamics_type      type,
                               ImVec2             click_pos)
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
        ImNodes::SetNodeScreenSpacePos(pack_node(child_id), click_pos);
        app->mod.children_positions[get_index(child_id)].x = click_pos.x;
        app->mod.children_positions[get_index(child_id)].y = click_pos.y;

        auto* app = container_of(&ed, &application::component_ed);
        auto& n   = app->notifications.alloc();
        n.level   = log_level::debug;
        format(n.title, "new model {} added", ordinal(child_id));
    }
}

static void add_popup_menuitem(component_editor&  ed,
                               component&         parent,
                               generic_component& s_parent,
                               int                type,
                               ImVec2             click_pos)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, parent, s_parent, d_type, click_pos);
}

static void compute_grid_layout(settings_window&   settings,
                                generic_component& s_compo) noexcept
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
            ImNodes::SetNodeGridSpacePos(pack_node(c_id), new_pos);
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
        ImNodes::SetNodeGridSpacePos(pack_node(c_id), new_pos);
        app->mod.children_positions[get_index(c_id)].x = new_pos.x;
        app->mod.children_positions[get_index(c_id)].y = new_pos.y;
    }
}

static status add_component_to_current(component_editor&  ed,
                                       component&         parent,
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

    ImNodes::SetNodeScreenSpacePos(pack_node(c_id), click_pos);
    app->mod.children_positions[get_index(c_id)].x = click_pos.x;
    app->mod.children_positions[get_index(c_id)].y = click_pos.y;

    return status::success;
}

static void show_popup_all_component_menuitem(
  component_editor&  ed,
  component&         parent,
  generic_component& s_parent) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    // if (ImGui::BeginMenu("Internal library")) {
    //     component* compo = nullptr;
    //     while (data.mod.components.next(compo)) {
    //         if (ImGui::MenuItem(compo->name.c_str())) {
    //             add_component_to_current(data, tree, s_parent, *compo,
    //             click_pos);
    //         }
    //     }

    //    ImGui::EndMenu();
    //}

    for (auto id : app->mod.component_repertories) {
        static small_string<32> s; //! @TODO remove this variable
        small_string<32>*       select;

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
                              ed, parent, s_parent, *compo);
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
                    add_component_to_current(ed, parent, s_parent, *compo);
                }
                ImGui::PopID();
            }
        }

        ImGui::EndMenu();
    }
}

static void show_popup_menuitem(component_editor&      ed,
                                component_editor_data& data,
                                component&             parent,
                                generic_component&     s_parent) noexcept
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
            compute_grid_layout(app->settings_wnd, s_parent);
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
                  ed, parent, s_parent, compo, click_pos);
            }
        }

        ImGui::Separator();

        show_popup_all_component_menuitem(ed, parent, s_parent);

        ImGui::Separator();

        if (ImGui::MenuItem("Grid generator"))
            app->grid_dlg.load(app, &s_parent);

        ImGui::Separator();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, s_parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::integrator, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::quantifier, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::adder_2, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::adder_3, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::adder_4, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::mult_2, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::mult_3, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::mult_4, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::cross, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::logical_and_2, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::logical_or_2, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::logical_and_3, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::logical_or_3, click_pos);
            add_popup_menuitem(
              ed, parent, s_parent, dynamics_type::logical_invert, click_pos);
            ImGui::EndMenu();
        }

        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::counter, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::queue, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::dynamic_queue, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::priority_queue, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::generator, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::constant, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::time_func, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::accumulator_2, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::filter, click_pos);
        add_popup_menuitem(
          ed, parent, s_parent, dynamics_type::hsm_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

// static bool is_ports_compatible(const component_editor& /*data*/,
//                                 const model& output,
//                                 const int    output_port,
//                                 const model& input,
//                                 const int    input_port) noexcept
// {
//     return is_ports_compatible(output, output_port, input, input_port);
// }

// static bool is_ports_compatible(const component_editor& data,
//                                 const model&            output,
//                                 const int               output_port,
//                                 const component&        input,
//                                 const int               input_port) noexcept
// {
//     irt_assert(0 <= input_port && input_port < component::port_number);

//     // loop over all internal input connection of input component and check
//     if
//     // connection these models (only models) connection is possible using the
//     // global is_ports_compoatible function.
//     connection* con{};
//     while (input.connections.next(con)) {
//         if (con->type == connection::connection_type::input &&
//             std::cmp_equal(con->input.index, input_port)) {

//             auto* child = input.children.try_to_get(con->input.index_dst);
//             if (child == nullptr || child->type != child_type::model)
//                 return false;

//             // con->input.dst
//         }
//     }

//     child_id input_id         = input.x[input_port].id;
//     int      input_port_index = input.x[input_port].index;
//     auto*    child            = input.children.try_to_get(input_id);

//     if (child) {
//         if (child->type == child_type::model) {
//             auto  mdl_id = enum_cast<model_id>(child->id);
//             auto* mdl    = input.models.try_to_get(mdl_id);

//             if (mdl)
//                 return is_ports_compatible(
//                   data, output, output_port, *mdl, input_port_index);
//         } else {
//             auto  compo_id = enum_cast<component_id>(child->id);
//             auto* compo    = data.mod.components.try_to_get(compo_id);

//             if (compo)
//                 return is_ports_compatible(
//                   data, output, output_port, *compo, input_port_index);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& data,
//                                 const component&        output,
//                                 const int               output_port,
//                                 const model&            input,
//                                 const int               input_port) noexcept
// {
//     irt_assert(0 <= output_port && output_port < output.y.ssize());

//     child_id output_id         = output.y[output_port].id;
//     int      output_port_index = output.y[output_port].index;
//     auto*    child             = output.children.try_to_get(output_id);

//     if (child) {
//         if (child->type == child_type::model) {
//             auto  mdl_id = enum_cast<model_id>(child->id);
//             auto* mdl    = output.models.try_to_get(mdl_id);

//             if (mdl)
//                 return is_ports_compatible(
//                   data, *mdl, output_port_index, input, input_port);
//         } else {
//             auto  compo_id = enum_cast<component_id>(child->id);
//             auto* compo    = data.mod.components.try_to_get(compo_id);

//             if (compo)
//                 return is_ports_compatible(
//                   data, *compo, output_port_index, input, input_port);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& data,
//                                 const component&        output,
//                                 const int               output_port,
//                                 const component&        input,
//                                 const int               input_port) noexcept
// {
//     irt_assert(0 <= input_port && input_port < input.x.ssize());
//     irt_assert(0 <= output_port && output_port < output.y.ssize());

//     child_id input_id         = input.x[input_port].id;
//     int      input_port_index = input.x[input_port].index;
//     auto*    input_child      = input.children.try_to_get(input_id);

//     child_id output_id         = output.y[output_port].id;
//     int      output_port_index = output.y[output_port].index;
//     auto*    output_child      = output.children.try_to_get(output_id);

//     if (!input_child || !output_child)
//         return false;

//     if (input_child->type == child_type::model) {
//         auto  input_mdl_id = enum_cast<model_id>(input_child->id);
//         auto* input_mdl    = input.models.try_to_get(input_mdl_id);

//         if (!input_mdl)
//             return false;

//         if (output_child->type == child_type::model) {
//             auto  output_mdl_id = enum_cast<model_id>(output_child->id);
//             auto* output_mdl    = output.models.try_to_get(output_mdl_id);

//             if (output_mdl)
//                 return is_ports_compatible(data,
//                                            *output_mdl,
//                                            output_port_index,
//                                            *input_mdl,
//                                            input_port_index);
//         } else {
//             auto  output_compo_id =
//             enum_cast<component_id>(output_child->id); auto* output_compo =
//             data.mod.components.try_to_get(output_compo_id);

//             if (output_compo)
//                 return is_ports_compatible(data,
//                                            *output_compo,
//                                            output_port_index,
//                                            *input_mdl,
//                                            input_port_index);
//         }
//     } else {
//         auto  input_compo_id = enum_cast<component_id>(input_child->id);
//         auto* input_compo    =
//         data.mod.components.try_to_get(input_compo_id);

//         if (!input_compo)
//             return false;

//         if (output_child->type == child_type::model) {
//             auto  output_mdl_id = enum_cast<model_id>(output_child->id);
//             auto* output_mdl    = output.models.try_to_get(output_mdl_id);

//             if (output_mdl)
//                 return is_ports_compatible(data,
//                                            *output_mdl,
//                                            output_port_index,
//                                            *input_compo,
//                                            input_port_index);
//         } else {
//             auto  output_compo_id =
//             enum_cast<component_id>(output_child->id); auto* output_compo =
//             data.mod.components.try_to_get(output_compo_id);

//             if (output_compo)
//                 return is_ports_compatible(data,
//                                            *output_compo,
//                                            output_port_index,
//                                            *input_compo,
//                                            input_port_index);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& data,
//                                 const component&        parent,
//                                 const child&            output,
//                                 const int               output_port,
//                                 const child&            input,
//                                 const int               input_port) noexcept
// {
//     if (output.type == child_type::model) {
//         auto  mdl_output_id = enum_cast<model_id>(output.id);
//         auto* mdl_output    = parent.models.try_to_get(mdl_output_id);

//         if (!mdl_output)
//             return false;

//         if (input.type == child_type::model) {
//             auto  mdl_input_id = enum_cast<model_id>(input.id);
//             auto* mdl_input    = parent.models.try_to_get(mdl_input_id);

//             if (!mdl_input)
//                 return false;

//             return is_ports_compatible(
//               data, *mdl_output, output_port, *mdl_input, input_port);
//         } else {
//             auto  compo_input_id = enum_cast<component_id>(input.id);
//             auto* compo_input    =
//             data.mod.components.try_to_get(compo_input_id);

//             if (!compo_input)
//                 return false;

//             return is_ports_compatible(
//               data, *mdl_output, output_port, *compo_input, input_port);
//         }
//     } else {
//         auto  compo_output_id = enum_cast<component_id>(output.id);
//         auto* compo_output    =
//         data.mod.components.try_to_get(compo_output_id);

//         if (!compo_output)
//             return false;

//         if (input.type == child_type::model) {
//             auto  mdl_input_id = enum_cast<model_id>(input.id);
//             auto* mdl_input    = parent.models.try_to_get(mdl_input_id);

//             if (!mdl_input)
//                 return false;

//             return is_ports_compatible(
//               data, *compo_output, output_port, *mdl_input, input_port);
//         } else {
//             auto  compo_input_id = enum_cast<component_id>(input.id);
//             auto* compo_input    =
//             data.mod.components.try_to_get(compo_input_id);

//             if (!compo_input)
//                 return false;

//             return is_ports_compatible(
//               data, *compo_output, output_port, *compo_input, input_port);
//         }
//     }
// }

static void is_link_created(application& app,
                            component_editor_data& /*data*/,
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

static void remove_nodes(modeling&              mod,
                         component_editor_data& data,
                         component&             parent) noexcept
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

static void remove_links(modeling&              mod,
                         component_editor_data& data,
                         component&             parent,
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

static void show_component_editor(component_editor&      ed,
                                  component_editor_data& data,
                                  component&             compo,
                                  generic_component&     s_compo) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    ImNodes::EditorContextSet(data.context);
    ImNodes::BeginNodeEditor();

    show_popup_menuitem(ed, data, compo, s_compo);

    if (app->grid_dlg.is_running) {
        app->grid_dlg.show();

        if (app->grid_dlg.is_ok && !app->grid_dlg.is_running) {
            auto size = s_compo.children.size();
            app->grid_dlg.save();
            app->grid_dlg.is_ok = false;

            for (sz i = size, e = s_compo.children.size(); i != e; ++i) {
                auto* c = app->mod.children.try_to_get(s_compo.children[i]);
                if (!c)
                    continue;

                if (c->type == child_type::model) {
                    auto* mdl = app->mod.models.try_to_get(c->id.mdl_id);
                    if (!mdl)
                        continue;
                    else
                        ImNodes::SetNodeEditorSpacePos(
                          pack_node(s_compo.children[i]), ImVec2(0.f, 0.f));

                } else {
                    auto* compo =
                      app->mod.components.try_to_get(c->id.compo_id);
                    if (!compo)
                        continue;

                    switch (compo->type) {
                    case component_type::none:
                        break;

                    case component_type::simple:
                        if (auto* tmp = app->mod.simple_components.try_to_get(
                              compo->id.simple_id);
                            tmp)
                            ImNodes::SetNodeEditorSpacePos(
                              pack_node(s_compo.children[i]), ImVec2(0.f, 0.f));
                        break;

                    case component_type::grid:
                        if (auto* tmp = app->mod.grid_components.try_to_get(
                              compo->id.grid_id);
                            tmp)
                            ImNodes::SetNodeEditorSpacePos(
                              pack_node(s_compo.children[i]), ImVec2(0.f, 0.f));
                        break;

                    case component_type::internal:
                        break;
                    }
                }
            }
        }
    }

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

component_editor_data::component_editor_data() noexcept
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
}

component_editor_data::~component_editor_data() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void component_editor_data::show(component_editor& ed) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    if (auto* compo = app->mod.components.try_to_get(id); compo) {
        const auto s_id = compo->id.simple_id;

        if (auto* s = app->mod.simple_components.try_to_get(s_id); s)
            show_component_editor(ed, *this, *compo, *s);
    }
}

template<typename T, typename Identifier>
T* find(data_array<T, Identifier>& data,
        vector<Identifier>&        container,
        std::string_view           name) noexcept
{
    int i = 0;
    while (i < container.ssize()) {
        auto  test_id = container[i];
        auto* test    = data.try_to_get(test_id);

        if (test) {
            if (test->path.sv() == name)
                return test;

            ++i;
        } else {
            container.swap_pop_back(i);
        }
    }

    return nullptr;
}

template<typename T, typename Identifier>
bool exist(data_array<T, Identifier>& data,
           vector<Identifier>&        container,
           std::string_view           name) noexcept
{
    return find(data, container, name) != nullptr;
}

static void show_file_access(application& app, component& compo) noexcept
{
    static constexpr const char* empty = "";

    auto*       reg_dir = app.mod.registred_paths.try_to_get(compo.reg_path);
    const char* reg_preview = reg_dir ? reg_dir->path.c_str() : empty;

    if (ImGui::BeginCombo("Path", reg_preview)) {
        registred_path* list = nullptr;
        while (app.mod.registred_paths.next(list)) {
            if (list->status == registred_path::state::error)
                continue;

            if (ImGui::Selectable(list->path.c_str(),
                                  reg_dir == list,
                                  ImGuiSelectableFlags_None)) {
                compo.reg_path = app.mod.registred_paths.get_id(list);
                reg_dir        = list;
            }
        }
        ImGui::EndCombo();
    }

    if (reg_dir) {
        auto* dir         = app.mod.dir_paths.try_to_get(compo.dir);
        auto* dir_preview = dir ? dir->path.c_str() : empty;

        if (ImGui::BeginCombo("Dir", dir_preview)) {
            if (ImGui::Selectable("##empty-dir", dir == nullptr)) {
                compo.dir = undefined<dir_path_id>();
                dir       = nullptr;
            }

            dir_path* list = nullptr;
            while (app.mod.dir_paths.next(list)) {
                if (ImGui::Selectable(list->path.c_str(), dir == list)) {
                    compo.dir = app.mod.dir_paths.get_id(list);
                    dir       = list;
                }
            }
            ImGui::EndCombo();
        }

        if (dir == nullptr) {
            small_string<256> dir_name{};

            if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                if (!exist(
                      app.mod.dir_paths, reg_dir->children, dir_name.sv())) {
                    auto& new_dir  = app.mod.dir_paths.alloc();
                    auto  dir_id   = app.mod.dir_paths.get_id(new_dir);
                    auto  reg_id   = app.mod.registred_paths.get_id(*reg_dir);
                    new_dir.parent = reg_id;
                    new_dir.path   = dir_name;
                    new_dir.status = dir_path::state::unread;
                    reg_dir->children.emplace_back(dir_id);
                    compo.reg_path = reg_id;
                    compo.dir      = dir_id;

                    if (!app.mod.create_directories(new_dir)) {
                        log_w(app,
                              log_level::error,
                              "Fail to create directory `%.*s'",
                              new_dir.path.ssize(),
                              new_dir.path.begin());
                    }
                }
            }
        }

        if (dir) {
            auto* file = app.mod.file_paths.try_to_get(compo.file);
            if (!file) {
                auto& f     = app.mod.file_paths.alloc();
                auto  id    = app.mod.file_paths.get_id(f);
                f.component = app.mod.components.get_id(compo);
                f.parent    = app.mod.dir_paths.get_id(*dir);
                compo.file  = id;
                dir->children.emplace_back(id);
                file = &f;
            }

            if (ImGui::InputFilteredString("File##text", file->path)) {
                if (!exist(
                      app.mod.file_paths, dir->children, file->path.sv())) {
                }
            }

            auto* desc = app.mod.descriptions.try_to_get(compo.desc);
            if (!desc) {
                if (app.mod.descriptions.can_alloc(1) &&
                    ImGui::Button("Add description")) {
                    auto& new_desc = app.mod.descriptions.alloc();
                    compo.desc     = app.mod.descriptions.get_id(new_desc);
                }
            } else {
                constexpr ImGuiInputTextFlags flags =
                  ImGuiInputTextFlags_AllowTabInput;

                ImGui::InputSmallStringMultiline(
                  "##source",
                  desc->data,
                  ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                  flags);

                if (ImGui::Button("Remove")) {
                    app.mod.descriptions.free(*desc);
                    compo.desc = undefined<description_id>();
                }
            }

            if (file && dir) {
                if (ImGui::Button("Save")) {
                    auto id = ordinal(app.mod.components.get_id(compo));

                    app.add_simulation_task(task_save_component, id);
                    app.add_simulation_task(task_save_description, id);
                }
            }
        }
    }
}

static inline const char* port_labels[] = { "1", "2", "3", "4",
                                            "5", "6", "7", "8" };

static void show_input_output(component& compo) noexcept
{
    if (ImGui::BeginTable("##io-table",
                          3,
                          ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 32.f);
        ImGui::TableSetupColumn("in");
        ImGui::TableSetupColumn("out");

        ImGui::TableHeadersRow();

        for (int i = 0; i < component::port_number; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(port_labels[i]);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1.f);
            ImGui::PushID(i);
            ImGui::InputFilteredString("##in", compo.x_names[i]);
            ImGui::PopID();
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1.f);
            ImGui::PushID(i + 16);
            ImGui::InputFilteredString("##out", compo.y_names[i]);
            ImGui::PopID();
            ImGui::PopItemWidth();
        }

        ImGui::EndTable();
    }
}

static void show_selected_children(application& /*app*/,
                                   component& /*compo*/,
                                   grid_editor_data& /*data*/) noexcept
{
}

static void show_selected_children(application&           app,
                                   component&             compo,
                                   component_editor_data& data) noexcept
{
    if (auto* s_compo =
          app.mod.simple_components.try_to_get(compo.id.simple_id);
        s_compo) {
        for (int i = 0, e = data.selected_nodes.size(); i != e; ++i) {
            auto* child = app.mod.children.try_to_get(
              static_cast<u32>(data.selected_nodes[i]));
            if (!child)
                continue;

            if (ImGui::TreeNodeEx(child,
                                  ImGuiTreeNodeFlags_DefaultOpen,
                                  "%d",
                                  data.selected_nodes[i])) {
                bool is_modified = false;
                ImGui::TextFormat(
                  "position {},{}",
                  app.mod.children_positions[data.selected_nodes[i]].x,
                  app.mod.children_positions[data.selected_nodes[i]].y);

                bool configurable = child->flags & child_flags_configurable;
                if (ImGui::Checkbox("configurable", &configurable)) {
                    if (configurable)
                        child->flags |= child_flags_configurable;
                    is_modified = true;
                }

                bool observable = child->flags & child_flags_observable;
                if (ImGui::Checkbox("observables", &observable)) {
                    if (observable)
                        child->flags |= child_flags_observable;
                    is_modified = true;
                }

                if (ImGui::InputSmallString(
                      "name", app.mod.children_names[data.selected_nodes[i]]))
                    is_modified = true;

                if (is_modified)
                    compo.state = component_status::modified;

                ImGui::TextFormat("name: {}", compo.name.sv());
                ImGui::TreePop();
            }
        }
    }
}

template<typename T, typename ID>
static void show_data(application&       app,
                      component_editor&  ed,
                      data_array<T, ID>& data,
                      std::string_view   title) noexcept
{
    T* del     = nullptr;
    T* element = nullptr;

    while (data.next(element)) {
        if (del) {
            data.free(*del);
            del = nullptr;
        }

        auto tab_item_flags = ImGuiTabItemFlags_None;
        if (auto* c = app.mod.components.try_to_get(element->id); c) {
            format(ed.title,
                   "{} {}",
                   title,
                   get_index(app.mod.components.get_id(c)));

            if (ed.request_to_open == app.mod.components.get_id(c)) {
                tab_item_flags     = ImGuiTabItemFlags_SetSelected;
                ed.request_to_open = undefined<component_id>();
            }

            bool open = true;
            if (ImGui::BeginTabItem(ed.title.c_str(), &open, tab_item_flags)) {
                static ImGuiTableFlags flags =
                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
                  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                  ImGuiTableFlags_Reorderable;

                if (ImGui::BeginTable("##ed", 2, flags)) {
                    ImGui::TableSetupColumn(
                      "Settings", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                    ImGui::TableSetupColumn(
                      "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);
                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::InputFilteredString(
                      "Name", c->name, ImGuiInputTextFlags_EnterReturnsTrue);

                    if (ImGui::CollapsingHeader("path"))
                        show_file_access(app, *c);

                    if (ImGui::CollapsingHeader("i/o ports names"))
                        show_input_output(*c);

                    if (ImGui::CollapsingHeader("selected"))
                        show_selected_children(app, *c, *element);

                    ImGui::TableSetColumnIndex(1);
                    element->show(ed);

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (!open) {
                del = element;
            }
        } else {
            del = element;
        }
    }

    if (del)
        data.free(*del);
}

void component_editor::show() noexcept
{
    if (!ImGui::Begin(component_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto* app = container_of(this, &application::component_ed);

    if (ImGui::BeginTabBar("Editors")) {
        show_data(*app, *this, app->generics, "generic");
        show_data(*app, *this, app->grids, "grid");
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace irt
