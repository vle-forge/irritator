// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"

#include <cstring>

namespace irt {

constexpr auto child_tag  = 0b11;
constexpr auto input_tag  = 0b10;
constexpr auto output_tag = 0b01;
constexpr auto shift_tag  = 2;
constexpr auto mask_tag   = 0b11;

inline bool is_node_child(const int node) noexcept
{
    return (mask_tag & node) == child_tag;
}

inline bool is_node_X(const int node) noexcept
{
    return (mask_tag & node) == input_tag;
}

inline bool is_node_Y(const int node) noexcept
{
    return (mask_tag & node) == output_tag;
}

inline int pack_node_child(const child_id child) noexcept
{
    return static_cast<int>((get_index(child) << shift_tag) | child_tag);
}

inline int unpack_node_child(const int node) noexcept
{
    debug::ensure(is_node_child(node));
    return node >> shift_tag;
}

inline int pack_node_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | input_tag);
}

inline int unpack_node_X(const int node) noexcept
{
    debug::ensure(is_node_X(node));
    return node >> shift_tag;
}

inline int pack_node_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);
    return static_cast<int>((get_index(port) << shift_tag) | output_tag);
}

inline int unpack_node_Y(const int node) noexcept
{
    debug::ensure(is_node_Y(node));
    return node >> shift_tag;
}

constexpr auto mask_port               = 0b111;
constexpr auto input_component_port    = 0b111;
constexpr auto input_child_model_port  = 0b110;
constexpr auto input_child_compo_port  = 0b101;
constexpr auto output_component_port   = 0b011;
constexpr auto output_child_model_port = 0b010;
constexpr auto output_child_compo_port = 0b001;
constexpr auto shift_port              = 3;
constexpr auto shift_child_port        = 16;

inline bool is_input_X(const int attribute) noexcept
{
    return (attribute & mask_port) == input_component_port;
}

inline bool is_output_Y(const int attribute) noexcept
{
    return (attribute & mask_port) == output_component_port;
}

inline bool is_input_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_model_port;
}

inline bool is_input_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == input_child_compo_port;
}

inline bool is_output_child_model(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_model_port;
}

inline bool is_output_child_component(const int attribute) noexcept
{
    return (attribute & mask_port) == output_child_compo_port;
}

inline int pack_X(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>((get_index(port) << shift_port) |
                            input_component_port);
}

inline u32 unpack_X(const int attribute) noexcept
{
    debug::ensure(is_input_X(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

inline int pack_Y(const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    return static_cast<int>(
      (get_index(port) << shift_port | output_component_port));
}

inline u32 unpack_Y(const int attribute) noexcept
{
    debug::ensure(is_output_Y(attribute));
    return static_cast<u32>(attribute >> shift_port);
}

inline int pack_in(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_model_port;
    return ret;
}

inline int pack_in(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= input_child_compo_port;
    return ret;
}

inline std::pair<u32, u32> unpack_in(const int attribute) noexcept
{
    debug::ensure(!is_input_X(attribute));
    debug::ensure(is_input_child_model(attribute) ||
                  is_input_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

inline int pack_out(const child_id id, const int port) noexcept
{
    debug::ensure(0 <= port && port < 8);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_model_port;
    return ret;
}

inline int pack_out(const child_id id, const port_id port) noexcept
{
    debug::ensure(get_index(port) <= 0x1fff);

    auto ret = get_index(id) << shift_child_port;
    ret |= static_cast<u16>(port) << 3;
    ret |= output_child_compo_port;
    return ret;
}

inline std::pair<u32, u32> unpack_out(const int attribute) noexcept
{
    debug::ensure(!is_output_Y(attribute));
    debug::ensure(is_output_child_model(attribute) ||
                  is_output_child_component(attribute));

    const auto child = attribute >> 16;
    const auto port  = (attribute >> 3) & 0x1fff;
    return std::make_pair(static_cast<u32>(child), static_cast<u32>(port));
}

static void add_input_attribute(const std::span<const std::string_view> names,
                                const child_id id) noexcept
{
    for (size_t i = 0, e = names.size(); i != e; ++i) {
        ImNodes::BeginInputAttribute(pack_in(id, static_cast<int>(i)),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", names[i]);
        ImNodes::EndInputAttribute();
    }
}

static void add_output_attribute(const std::span<const std::string_view> names,
                                 const child_id id) noexcept
{
    for (size_t i = 0, e = names.size(); i != e; ++i) {
        ImNodes::BeginOutputAttribute(pack_out(id, static_cast<int>(i)),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", names[i]);
        ImNodes::EndOutputAttribute();
    }
}

static bool show_connection(
  const component&                           compo,
  const generic_component&                   gen,
  const generic_component::input_connection& con) noexcept
{
    const auto id     = gen.input_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 4096 + static_cast<int>(idx);

    if (compo.x.exists(con.x)) {
        if (auto* c = gen.children.try_to_get(con.dst); c) {
            const auto id_src = pack_X(con.x);
            const auto id_dst = c->type == child_type::model
                                  ? pack_in(con.dst, con.port.model)
                                  : pack_in(con.dst, con.port.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return true;
        }
    }

    return false;
}

static bool show_connection(
  const component&                            compo,
  const generic_component&                    gen,
  const generic_component::output_connection& con) noexcept
{
    const auto id     = gen.output_connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = 8192 + static_cast<int>(idx);

    if (compo.y.exists(con.y)) {
        if (auto* c = gen.children.try_to_get(con.src); c) {
            const auto id_dst = pack_Y(con.y);
            const auto id_src = c->type == child_type::model
                                  ? pack_out(con.src, con.port.model)
                                  : pack_out(con.src, con.port.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return true;
        }
    }

    return false;
}

static bool show_connection(const generic_component& compo,
                            const connection&        con) noexcept
{
    const auto id     = compo.connections.get_id(con);
    const auto idx    = get_index(id);
    const auto con_id = static_cast<int>(idx);

    if (auto* s = compo.children.try_to_get(con.src); s) {
        if (auto* d = compo.children.try_to_get(con.dst); d) {
            const auto id_src = s->type == child_type::model
                                  ? pack_out(con.src, con.index_src.model)
                                  : pack_out(con.src, con.index_src.compo);

            const auto id_dst = d->type == child_type::model
                                  ? pack_in(con.dst, con.index_dst.model)
                                  : pack_in(con.dst, con.index_dst.compo);

            ImNodes::Link(con_id, id_src, id_dst);
            return true;
        }
    }

    return false;
}

static void show(component_editor& ed,
                 std::string_view  name,
                 parameter&        p,
                 child&            c,
                 child_id          id) noexcept
{
    auto& app      = container_of(&ed, &application::component_ed);
    auto& settings = app.settings_wnd;

    const auto type = c.id.mdl_type;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_model_color);

    ImNodes::BeginNode(pack_node_child(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat(
      "{}\n{}", name, dynamics_type_names[ordinal(c.id.mdl_type)]);
    ImNodes::EndNodeTitleBar();

    [[maybe_unused]] auto changed =
      dispatcher(type, [&](const auto tag) noexcept -> bool {
          const auto X = get_dynamics_input_names(tag);
          const auto Y = get_dynamics_output_names(tag);

          add_input_attribute(X, id);
          ImGui::PushItemWidth(120.0f);
          bool updated = show_parameter(tag, app, p);
          ImGui::PopItemWidth();
          add_output_attribute(Y, id);

          return updated;
      });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_input_an_output_ports(component&     compo,
                                       const child_id c_id) noexcept
{
    for (const auto id : compo.x) {
        const auto pack_id = pack_in(c_id, id);

        ImNodes::BeginInputAttribute(pack_id, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.x_names[get_index(id)].c_str());
        ImNodes::EndInputAttribute();
    }

    for (const auto id : compo.y) {
        const auto pack_id = pack_out(c_id, id);

        ImNodes::BeginOutputAttribute(pack_id, ImNodesPinShape_TriangleFilled);
        ImGui::TextUnformatted(compo.y_names[get_index(id)].c_str());
        ImNodes::EndOutputAttribute();
    }
}

static void show_generic_node(application&       app,
                              std::string_view   name,
                              component&         compo,
                              generic_component& s_compo,
                              child&             c) noexcept
{
    const auto c_id = s_compo.children.get_id(c);

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(app.settings_wnd.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            app.settings_wnd.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            app.settings_wnd.gui_selected_component_color);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.c_str());
    ImNodes::EndNodeTitleBar();
    show_input_an_output_ports(compo, c_id);
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_grid_node(application&     app,
                           std::string_view name,
                           component&       compo,
                           grid_component&  grid,
                           child_id         c_id) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(app.settings_wnd.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            app.settings_wnd.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            app.settings_wnd.gui_selected_component_color);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.sv());
    ImGui::TextFormat("{}x{}", grid.row, grid.column);
    ImNodes::EndNodeTitleBar();
    show_input_an_output_ports(compo, c_id);
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_graph_node(application&     app,
                            std::string_view name,
                            component&       compo,
                            graph_component& graph,
                            child_id         c_id) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(app.settings_wnd.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            app.settings_wnd.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            app.settings_wnd.gui_selected_component_color);

    ImNodes::BeginNode(pack_node_child(c_id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", name, compo.name.sv());
    ImGui::TextFormat("{}", graph.children.size());
    ImNodes::EndNodeTitleBar();
    show_input_an_output_ports(compo, c_id);
    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void update_position(generic_component_editor_data& data,
                            generic_component&             generic) noexcept
{
    for_each_data(generic.children, [&](auto& child) {
        const auto id  = generic.children.get_id(child);
        const auto idx = get_index(id);

        ImNodes::SetNodeEditorSpacePos(
          pack_node_child(id),
          ImVec2(generic.children_positions[idx].x,
                 generic.children_positions[idx].y));
    });

    data.force_update_position = false;
}

static void update_input_output_draggable(component& parent,
                                          bool       draggable) noexcept
{
    for (const auto id : parent.x)
        ImNodes::SetNodeDraggable(pack_node_X(id), draggable);

    for (const auto id : parent.y)
        ImNodes::SetNodeDraggable(pack_node_Y(id), draggable);
}

static void update_input_output_position(component&                     parent,
                                         generic_component_editor_data& data,
                                         float                          x1,
                                         float                          x2,
                                         float y) noexcept
{
    int i = 0;
    for (const auto id : parent.x)
        ImNodes::SetNodeEditorSpacePos(
          pack_node_X(id), ImVec2(x1, static_cast<float>(i++) * 50.f + y));

    i = 0;
    for (const auto id : parent.y)
        ImNodes::SetNodeEditorSpacePos(
          pack_node_Y(id), ImVec2(x2, static_cast<float>(i++) * 50.f + y));

    data.first_show_input_output = false;
}

static void show_graph(component_editor&              ed,
                       generic_component_editor_data& data,
                       component&                     parent,
                       generic_component&             s_parent) noexcept
{
    auto& app      = container_of(&ed, &application::component_ed);
    auto& settings = app.settings_wnd;

    const auto width  = ImGui::GetContentRegionAvail().x;
    const auto pos    = ImNodes::EditorContextGetPanning();
    const auto pos_x1 = pos.x + 10.f;
    const auto pos_x2 = pos.x + width - 50.f;

    if (data.force_update_position)
        update_position(data, s_parent);

    if (data.show_input_output) {
        update_input_output_draggable(parent, data.fix_input_output);

        if (data.first_show_input_output)
            update_input_output_position(parent, data, pos_x1, pos_x2, pos.y);
    }

    if (data.show_input_output) {
        for (const auto id : parent.x) {
            const auto idx = get_index(id);

            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_component_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_component_color);

            ImNodes::BeginNode(pack_node_X(id));
            ImNodes::BeginOutputAttribute(pack_X(id),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(parent.x_names[idx].c_str());
            ImNodes::EndOutputAttribute();
            ImNodes::EndNode();
        }

        for (const auto id : parent.y) {
            const auto idx = get_index(id);

            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_component_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_component_color);

            ImNodes::BeginNode(pack_node_Y(id));
            ImNodes::BeginInputAttribute(pack_Y(id),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(parent.y_names[idx].c_str());
            ImNodes::EndInputAttribute();
            ImNodes::EndNode();
        }
    }

    for_each_data(s_parent.children, [&](auto& c) noexcept {
        const auto cid  = s_parent.children.get_id(c);
        const auto cidx = get_index(cid);

        if (c.type == child_type::model) {
            show(ed,
                 s_parent.children_names[cidx].sv(),
                 s_parent.children_parameters[cidx],
                 c,
                 cid);
        } else {
            auto id = c.id.compo_id;
            if (auto* compo = app.mod.components.try_to_get(id); compo) {
                switch (compo->type) {
                case component_type::none:
                    break;

                case component_type::simple:
                    if (auto* s_compo = app.mod.generic_components.try_to_get(
                          compo->id.generic_id)) {
                        show_generic_node(app,
                                          s_parent.children_names[cidx].sv(),
                                          *compo,
                                          *s_compo,
                                          c);
                    }
                    break;

                case component_type::grid:
                    if (auto* s_compo = app.mod.grid_components.try_to_get(
                          compo->id.grid_id)) {
                        show_grid_node(app,
                                       s_parent.children_names[cidx].sv(),
                                       *compo,
                                       *s_compo,
                                       cid);
                    }
                    break;

                case component_type::graph:
                    if (auto* s_compo = app.mod.graph_components.try_to_get(
                          compo->id.graph_id)) {
                        show_graph_node(app,
                                        s_parent.children_names[cidx].sv(),
                                        *compo,
                                        *s_compo,
                                        cid);
                    }
                    break;

                case component_type::internal:
                    break;

                case component_type::hsm:
                    break;
                }
            }
        }
    });

    remove_data_if(s_parent.connections, [&](auto& con) noexcept -> bool {
        return show_connection(s_parent, con);
    });

    remove_data_if(s_parent.input_connections, [&](auto& con) noexcept -> bool {
        return show_connection(parent, s_parent, con);
    });

    remove_data_if(s_parent.output_connections,
                   [&](auto& con) noexcept -> bool {
                       return show_connection(parent, s_parent, con);
                   });
}

static void add_popup_menuitem(component_editor&              ed,
                               generic_component_editor_data& data,
                               component&                     parent,
                               generic_component&             s_parent,
                               dynamics_type                  type,
                               ImVec2                         click_pos)
{
    if (not s_parent.children.can_alloc(1)) {
        auto& app = container_of(&ed, &application::component_ed);
        auto& n   = app.notifications.alloc();
        n.level   = log_level::error;
        n.title   = "can not allocate a new model";
        return;
    }

    if (ImGui::MenuItem(dynamics_type_names[ordinal(type)])) {
        auto& child    = s_parent.children.alloc(type);
        auto  child_id = s_parent.children.get_id(child);

        s_parent.children_positions[get_index(child_id)].x = click_pos.x;
        s_parent.children_positions[get_index(child_id)].y = click_pos.y;

        parent.state = component_status::modified;
        data.update_position();

        auto& app = container_of(&ed, &application::component_ed);
        auto& n   = app.notifications.alloc();
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
    const auto size  = s_compo.children.ssize();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return;

    const auto column  = std::floor(std::sqrt(fsize));
    const auto panning = ImNodes::EditorContextGetPanning();
    auto       i       = 0.f;
    auto       j       = 0.f;

    for_each_data(s_compo.children, [&](auto& c) noexcept {
        const auto id  = s_compo.children.get_id(c);
        const auto idx = get_index(id);

        s_compo.children_positions[idx].x =
          panning.y + i * settings.grid_layout_y_distance;
        s_compo.children_positions[idx].y =
          panning.x + j * settings.grid_layout_x_distance;
        ++j;

        if (j >= static_cast<int>(column)) {
            j = 0.f;
            ++i;
        }
    });

    data.update_position();
}

static void add_component_to_current(component_editor&              ed,
                                     generic_component_editor_data& data,
                                     component&                     parent,
                                     generic_component& parent_compo,
                                     component&         compo_to_add,
                                     ImVec2             click_pos = ImVec2())
{
    auto&      app             = container_of(&ed, &application::component_ed);
    const auto compo_to_add_id = app.mod.components.get_id(compo_to_add);

    if (app.mod.can_add(parent, compo_to_add)) {
        auto& notif = app.notifications.alloc(log_level::error);
        notif.title = "Fail to add component";
        format(notif.message,
               "Irritator does not accept recursive component {}",
               compo_to_add.name.sv());
        app.notifications.enable(notif);
    }

    auto&      c     = parent_compo.children.alloc(compo_to_add_id);
    const auto c_id  = parent_compo.children.get_id(c);
    const auto c_idx = get_index(c_id);

    parent_compo.children_positions[c_idx].x = click_pos.x;
    parent_compo.children_positions[c_idx].y = click_pos.y;
    data.update_position();
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
            auto& app = container_of(&ed, &application::component_ed);
            compute_grid_layout(app.settings_wnd, data, s_parent);
        }

        ImGui::Separator();

        auto& app = container_of(&ed, &application::component_ed);
        if (ImGui::MenuItem("Add grid component")) {
            if (!app.mod.grid_components.can_alloc() ||
                !app.mod.components.can_alloc() ||
                !s_parent.children.can_alloc()) {
                auto& app = container_of(&ed, &application::component_ed);
                auto& n   = app.notifications.alloc();
                n.level   = log_level::error;
                n.title   = "can not allocate a new grid component";
            } else {
                auto& grid    = app.mod.grid_components.alloc();
                auto  grid_id = app.mod.grid_components.get_id(grid);
                grid.row      = 4;
                grid.column   = 4;

                auto& compo      = app.mod.components.alloc();
                compo.name       = "Grid";
                compo.type       = component_type::grid;
                compo.id.grid_id = grid_id;

                add_component_to_current(
                  ed, data, parent, s_parent, compo, click_pos);
            }
        }

        ImGui::Separator();

        component_id c_id = undefined<component_id>();
        app.component_sel.menu("Component?", &c_id);
        if (c_id != undefined<component_id>())
            if_data_exists_do(
              app.mod.components, c_id, [&](auto& compo) noexcept {
                  add_component_to_current(ed, data, parent, s_parent, compo);
              });

        ImGui::Separator();

        if (ImGui::MenuItem("Grid generator"))
            app.grid_dlg.load(app, &s_parent);
        if (ImGui::MenuItem("Graph generator"))
            app.graph_dlg.load(app, &s_parent);

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
          ed, data, parent, s_parent, dynamics_type::hsm_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

static void error_not_enough_connections(application& app, sz capacity) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = "Not enough connection slot in this component";
    format(n.message, "All connections slots ({}) are used.", capacity);
    app.notifications.enable(n);
}

static void error_not_connection_auth(application& app) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = "Can not connect component input on output ports";
    app.notifications.enable(n);
}

static void is_link_created(application& app,
                            generic_component_editor_data& /*data*/,
                            component&         parent,
                            generic_component& s_parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        if (!s_parent.connections.can_alloc()) {
            error_not_enough_connections(app, s_parent.connections.capacity());
            return;
        }

        if (is_input_X(start)) {
            auto port_idx = unpack_X(start);
            auto port_opt = parent.x.get_from_index(port_idx);
            debug::ensure(port_opt.has_value());

            if (is_output_Y(end)) {
                error_not_connection_auth(app);
                return;
            }

            auto  child_port = unpack_in(end);
            auto* child      = s_parent.children.try_to_get(child_port.first);
            debug::ensure(child);

            if (child->type == child_type::model) {
                auto port_in = static_cast<int>(child_port.second);

                if (auto ret = s_parent.connect_input(
                      *port_opt, *child, connection::port{ .model = port_in });
                    !ret)
                    debug_log("fail to create link");
                parent.state = component_status::modified;
            } else {
                const auto compo_dst =
                  app.mod.components.try_to_get(child->id.compo_id);
                if (not compo_dst)
                    return;

                const auto port_dst_opt =
                  compo_dst->x.get_from_index(child_port.second);
                if (not port_dst_opt.has_value())
                    return;

                if (auto ret = s_parent.connect_input(
                      *port_opt,
                      *child,
                      connection::port{ .compo = *port_dst_opt });
                    !ret)
                    debug_log("fail to create link\n");
                parent.state = component_status::modified;
            }
        } else {
            auto  ch_port_src = unpack_out(start);
            auto* ch_src      = s_parent.children.try_to_get(ch_port_src.first);
            debug::ensure(ch_src);

            if (is_output_Y(end)) {
                auto port_idx = unpack_Y(end);
                auto port_opt = parent.y.get_from_index(port_idx);
                debug::ensure(port_opt.has_value());

                if (ch_src->type == child_type::model) {
                    auto port_out = static_cast<int>(ch_port_src.second);

                    if (auto ret = s_parent.connect_output(
                          *port_opt,
                          *ch_src,
                          connection::port{ .model = port_out });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                } else {
                    const auto compo_src =
                      app.mod.components.try_to_get(ch_src->id.compo_id);
                    if (not compo_src)
                        return;

                    const auto port_out =
                      compo_src->y.get_from_index(ch_port_src.second);
                    if (not port_out.has_value())
                        return;

                    if (auto ret = s_parent.connect_output(
                          *port_opt,
                          *ch_src,
                          connection::port{ .compo = *port_opt });
                        !ret)
                        debug_log("fail to create link\n");
                    parent.state = component_status::modified;
                }
            } else {
                auto  ch_port_dst = unpack_in(end);
                auto* ch_dst = s_parent.children.try_to_get(ch_port_dst.first);
                debug::ensure(ch_dst);

                if (ch_src->type == child_type::model) {
                    auto port_out = static_cast<int>(ch_port_src.second);
                    if (ch_dst->type == child_type::model) {
                        auto port_in = static_cast<int>(ch_port_dst.second);

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .model = port_out },
                              *ch_dst,
                              connection::port{ .model = port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    } else {
                        const auto compo_dst =
                          app.mod.components.try_to_get(ch_dst->id.compo_id);
                        if (not compo_dst)
                            return;

                        const auto port_dst_opt =
                          compo_dst->x.get_from_index(ch_port_dst.second);
                        if (not port_dst_opt.has_value())
                            return;

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .model = port_out },
                              *ch_dst,
                              connection::port{ .compo = *port_dst_opt });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    }
                } else {
                    const auto compo_src =
                      app.mod.components.try_to_get(ch_src->id.compo_id);
                    if (not compo_src)
                        return;

                    const auto port_out =
                      compo_src->y.get_from_index(ch_port_src.second);
                    if (not port_out.has_value())
                        return;

                    if (ch_dst->type == child_type::model) {
                        auto port_in = static_cast<int>(ch_port_dst.second);

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .compo = *port_out },
                              *ch_dst,
                              connection::port{ .model = port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    } else {
                        const auto compo_dst =
                          app.mod.components.try_to_get(ch_dst->id.compo_id);
                        if (not compo_dst)
                            return;

                        const auto port_in =
                          compo_dst->x.get_from_index(ch_port_dst.second);
                        if (not port_in.has_value())
                            return;

                        if (auto ret = s_parent.connect(
                              app.mod,
                              *ch_src,
                              connection::port{ .compo = *port_out },
                              *ch_dst,
                              connection::port{ .compo = *port_in });
                            !ret)
                            debug_log("fail to create link\n");
                        parent.state = component_status::modified;
                    }
                }
            }
        }
    }
}

static void is_link_destroyed(component&         parent,
                              generic_component& s_parent) noexcept
{
    int link_id;
    if (ImNodes::IsLinkDestroyed(&link_id)) {
        const auto link_id_correct = static_cast<u32>(link_id);
        if (auto* con = s_parent.connections.try_to_get(link_id_correct); con) {
            s_parent.connections.free(*con);
            parent.state = component_status::modified;
        }
    }
}

static void remove_nodes(modeling&                      mod,
                         generic_component_editor_data& data,
                         component&                     parent) noexcept
{
    if (parent.type == component_type::simple) {
        if_data_exists_do(
          mod.generic_components,
          parent.id.generic_id,
          [&](auto& generic) noexcept {
              for (i32 i = 0, e = data.selected_nodes.size(); i != e; ++i) {
                  if (is_node_child(data.selected_nodes[i])) {
                      auto idx = unpack_node_child(data.selected_nodes[i]);

                      if (auto* child = generic.children.try_to_get(idx);
                          child) {
                          generic.children.free(*child);
                          parent.state = component_status::modified;
                      }
                  }
              }
          });
    }

    data.selected_nodes.clear();
    ImNodes::ClearNodeSelection();
}

static void remove_links(generic_component_editor_data& data,
                         component&                     parent,
                         generic_component&             s_parent) noexcept
{
    std::sort(data.selected_links.begin(),
              data.selected_links.end(),
              std::greater<int>());

    for (i32 i = 0, e = data.selected_links.size(); i != e; ++i) {
        const auto link_id = static_cast<u32>(data.selected_links[i]);

        if (auto* con = s_parent.connections.try_to_get(link_id); con) {
            s_parent.connections.free(*con);
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
        if (!is_node_child(v[i]))
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
    auto& app = container_of(&ed, &application::component_ed);

    ImNodes::EditorContextSet(data.context);
    ImNodes::BeginNodeEditor();

    if (app.grid_dlg.is_running) {
        app.grid_dlg.show();

        if (app.grid_dlg.is_ok && !app.grid_dlg.is_running) {
            app.grid_dlg.save();
            app.grid_dlg.is_ok = false;
            data.update_position();

            for_each_data(s_compo.children, [&](auto& c) noexcept {
                const auto id  = s_compo.children.get_id(c);
                const auto idx = get_index(id);

                s_compo.children_positions[idx] = {
                    static_cast<float>(idx) * 30.f,
                    static_cast<float>(idx) * 10.f
                };
            });
        }
    }

    if (app.graph_dlg.is_running) {
        app.graph_dlg.show();

        if (app.graph_dlg.is_ok && !app.graph_dlg.is_running) {
            app.graph_dlg.save();
            app.graph_dlg.is_ok = false;
            data.update_position();

            for_each_data(s_compo.children, [&](auto& c) noexcept {
                const auto id  = s_compo.children.get_id(c);
                const auto idx = get_index(id);

                s_compo.children_positions[idx] = {
                    static_cast<float>(idx) * 30.f,
                    static_cast<float>(idx) * 10.f
                };
            });
        }
    }

    show_popup_menuitem(ed, data, compo, s_compo);
    show_graph(ed, data, compo, s_compo);

    if (data.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    is_link_created(app, data, compo, s_compo);
    is_link_destroyed(compo, s_compo);

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
            remove_nodes(app.mod, data, compo);
        else if (num_selected_links > 0)
            remove_links(data, compo, s_compo);
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
    auto& app = container_of(&ed, &application::component_ed);

    if (auto* compo = app.mod.components.try_to_get(get_id()); compo) {
        const auto s_id = compo->id.generic_id;

        if (auto* s = app.mod.generic_components.try_to_get(s_id); s)
            show_component_editor(ed, *this, *compo, *s);
    }
}

static void update_unique_id(generic_component& gen, child& ch) noexcept
{
    const auto configurable = ch.flags[child_flags::configurable];
    const auto observable   = ch.flags[child_flags::observable];

    if (ch.unique_id == 0) {
        if (configurable || observable)
            ch.unique_id = gen.make_next_unique_id();
    } else {
        if (!configurable && !observable)
            ch.unique_id = 0;
    }
}

void generic_component_editor_data::show_selected_nodes(
  component_editor& ed) noexcept
{
    if (selected_nodes.empty())
        return;

    auto& app = container_of(&ed, &application::component_ed);

    if_component_is_generic(
      app.mod, m_id, [&](auto& compo, auto& gen) noexcept {
          for (int i = 0, e = selected_nodes.size(); i != e; ++i) {
              if (is_node_X(selected_nodes[i]) || is_node_Y(selected_nodes[i]))
                  continue;

              auto  id    = unpack_node_child(selected_nodes[i]);
              auto* child = gen.children.try_to_get(id);

              if (!child)
                  continue;

              if (ImGui::TreeNodeEx(child,
                                    ImGuiTreeNodeFlags_DefaultOpen,
                                    "%d",
                                    selected_nodes[i])) {
                  bool is_modified = false;
                  ImGui::TextFormat(
                    "position {},{}",
                    gen.children_positions[selected_nodes[i]].x,
                    gen.children_positions[selected_nodes[i]].y);

                  bool configurable = child->flags[child_flags::configurable];
                  if (ImGui::Checkbox("configurable", &configurable)) {
                      child->flags.set(child_flags::configurable, configurable);
                      is_modified = true;
                  }

                  bool observable = child->flags[child_flags::observable];
                  if (ImGui::Checkbox("observables", &observable)) {
                      child->flags.set(child_flags::observable, observable);
                      is_modified = true;
                  }

                  if (ImGui::InputSmallString(
                        "name", gen.children_names[selected_nodes[i]]))
                      is_modified = true;

                  update_unique_id(gen, *child);

                  if (is_modified)
                      compo.state = component_status::modified;

                  ImGui::TextFormat("name: {}", compo.name.sv());
                  ImGui::TreePop();
              }
          }
      });
}

} // namespace irt
