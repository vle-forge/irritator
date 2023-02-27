// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/modeling.hpp"

namespace irt {

static inline const char* port_names[] = { "0", "1", "2", "3",
                                           "4", "5", "6", "7" };

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
    u32 index      = get_index(id);

    return static_cast<int>((index << 5u) | port_index);
}

inline int pack_out(const child_id id, const i8 port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    u32 port_index = 8u + static_cast<u32>(port);
    u32 index      = get_index(id);

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

static bool show_connection(const component&  parent,
                            const connection& con) noexcept
{
    const auto idx    = get_index(parent.connections.get_id(con));
    const auto con_id = static_cast<int>(idx);

    switch (con.type) {
    case connection::connection_type::internal:
        if (auto* s = parent.children.try_to_get(con.internal.src); s) {
            if (auto* d = parent.children.try_to_get(con.internal.dst); d) {
                ImNodes::Link(
                  con_id,
                  pack_out(con.internal.src, con.internal.index_src),
                  pack_in(con.internal.dst, con.internal.index_dst));
                return true;
            }
        }
        break;

    case connection::connection_type::input:
        if (auto* d = parent.children.try_to_get(con.input.dst); d) {
            ImNodes::Link(con_id,
                          pack_component_input(con.input.index),
                          pack_in(con.input.dst, con.input.index_dst));
            return true;
        }
        break;

    case connection::connection_type::output:
        if (auto* s = parent.children.try_to_get(con.internal.src); s) {
            ImNodes::Link(con_id,
                          pack_out(con.output.src, con.output.index_src),
                          pack_component_output(con.output.index));
            return true;
        }
        break;
    }

    return false;
}

static void show(component_editor& ed,
                 component&        parent,
                 model&            mdl,
                 child&            c,
                 child_id          id) noexcept
{
    auto* app      = container_of(&ed, &application::c_editor);
    auto& settings = app->settings;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_model_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat(
      "{}\n{}", c.name.c_str(), dynamics_type_names[ordinal(mdl.type)]);
    ImNodes::EndNodeTitleBar();

    dispatch(mdl, [&mdl, &ed, &parent, id]<typename Dynamics>(Dynamics& dyn) {
        add_input_attribute(dyn, id);
        ImGui::PushItemWidth(120.0f);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            if (auto* machine = parent.hsms.try_to_get(dyn.id); machine) {
                auto* app = container_of(&ed, &application::c_editor);
                show_dynamics_inputs(*app,
                                     ed.mod.components.get_id(parent),
                                     parent.models.get_id(mdl),
                                     *machine);
                ImNodes::EditorContextSet(ed.context);
            }
        } else {
            show_dynamics_inputs(ed.mod.srcs, dyn);
        }

        ImGui::PopItemWidth();
        add_output_attribute(dyn, id);
    });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show(component_editor& ed,
                 component&        compo,
                 child&            c,
                 child_id          id) noexcept
{
    auto* app      = container_of(&ed, &application::c_editor);
    auto& settings = app->settings;

    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", c.name.c_str(), compo.name.c_str());
    ImNodes::EndNodeTitleBar();

    connection* con    = nullptr;
    u32         input  = 0;
    u32         output = 0;

    while (compo.connections.next(con)) {
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
            ImGui::TextUnformatted(port_names[i]);
            ImNodes::EndInputAttribute();
        }
    }

    for (u8 i = 0; i < 8; ++i) {
        if (output & (1 << i)) {
            auto gid = pack_out(id, static_cast<i8>(i));
            ImNodes::BeginOutputAttribute(gid, ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(port_names[i]);
            ImNodes::EndOutputAttribute();
        }
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_opened_component_ref(component_editor& ed,
                                      tree_node& /*ref*/,
                                      component& parent) noexcept
{
    auto* app      = container_of(&ed, &application::c_editor);
    auto& settings = app->settings;

    const auto width = ImGui::GetWindowContentRegionWidth();
    const auto pos   = ImNodes::EditorContextGetPanning();
    child*     c     = nullptr;

    if (ed.show_input_output) {
        for (int i = 0, e = length(component_input_ports); i != e; ++i) {
            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_component_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_component_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_component_color);

            ImNodes::BeginNode(pack_component_input(i));
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted("in");
            ImNodes::EndNodeTitleBar();
            ImNodes::BeginOutputAttribute(pack_component_input(i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(port_names[i]);
            ImNodes::EndOutputAttribute();
            ImNodes::EndNode();

            if (ed.fix_input_output)
                ImNodes::SetNodeDraggable(pack_component_input(i), false);

            if (ed.first_show_input_output) {
                ImNodes::SetNodeEditorSpacePos(
                  pack_component_input(i),
                  ImVec2(pos.x + 10.f, (float)i * 80.f + pos.y));
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
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted("out");
            ImNodes::EndNodeTitleBar();
            ImNodes::BeginInputAttribute(pack_component_output(i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(port_names[i]);
            ImNodes::EndInputAttribute();
            ImNodes::EndNode();

            if (ed.fix_input_output)
                ImNodes::SetNodeDraggable(pack_component_output(i), false);

            if (ed.first_show_input_output) {
                ImNodes::SetNodeEditorSpacePos(
                  pack_component_output(i),
                  ImVec2(pos.x + width - 50.f, (float)i * 80.f + pos.y));
            }
        }

        ed.first_show_input_output = false;
    }

    while (parent.children.next(c)) {
        const auto child_id = parent.children.get_id(c);

        if (c->type == child_type::model) {
            auto id = enum_cast<model_id>(c->id);
            if (auto* mdl = parent.models.try_to_get(id); mdl)
                show(ed, parent, *mdl, *c, child_id);
        } else {
            auto id = enum_cast<component_id>(c->id);
            if (auto* compo = ed.mod.components.try_to_get(id); compo)
                show(ed, *compo, *c, child_id);
        }

        if (ed.force_node_position) {
            ImNodes::SetNodeEditorSpacePos(pack_node(child_id),
                                           ImVec2(c->x, c->y));
        } else {
            auto pos = ImNodes::GetNodeEditorSpacePos(pack_node(child_id));

            if (c->x != pos.x || c->y != pos.y)
                parent.state = component_status::modified;

            c->x = pos.x;
            c->y = pos.y;
        }
    }

    ed.force_node_position = false;

    {
        connection* con    = nullptr;
        connection* to_del = nullptr;
        while (parent.connections.next(con)) {
            if (to_del) {
                parent.connections.free(*to_del);
                to_del = nullptr;
            }

            if (!show_connection(parent, *con)) {
                to_del = con;
            }
        }

        if (to_del)
            parent.connections.free(*to_del);
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               dynamics_type     type,
                               ImVec2            click_pos)
{
    if (!parent.models.can_alloc(1)) {
        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc();
        n.level   = log_level::error;
        n.title   = "can not allocate a new model";
        return;
    }

    if (ImGui::MenuItem(dynamics_type_names[ordinal(type)])) {
        auto& child    = ed.mod.alloc(parent, type);
        auto  child_id = parent.children.get_id(child);

        parent.state = component_status::modified;
        ImNodes::SetNodeScreenSpacePos(pack_node(child_id), click_pos);
        child.x = click_pos.x;
        child.y = click_pos.y;

        auto* app = container_of(&ed, &application::c_editor);
        auto& n   = app->notifications.alloc();
        n.level   = log_level::debug;
        format(n.title,
               "new model {} added",
               ordinal(parent.children.get_id(child)));
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               int               type,
                               ImVec2            click_pos)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, parent, d_type, click_pos);
}

static void compute_grid_layout(settings_window& settings,
                                component&       compo) noexcept
{
    const auto size  = compo.children.ssize();
    const auto fsize = static_cast<float>(size);

    if (size == 0)
        return;

    const auto column    = std::floor(std::sqrt(fsize));
    auto       line      = column;
    auto       remaining = fsize - (column * line);

    const auto panning = ImNodes::EditorContextGetPanning();
    auto       new_pos = panning;

    child* c = nullptr;
    for (float i = 0.f; i < line; ++i) {
        new_pos.y = panning.y + i * settings.grid_layout_y_distance;

        for (float j = 0.f; j < column; ++j) {
            if (!compo.children.next(c))
                break;

            const auto c_id = compo.children.get_id(c);
            new_pos.x       = panning.x + j * settings.grid_layout_x_distance;
            ImNodes::SetNodeGridSpacePos(pack_node(c_id), new_pos);
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + column * settings.grid_layout_y_distance;

    for (float j = 0.f; j < remaining; ++j) {
        if (!compo.children.next(c))
            break;

        const auto c_id = compo.children.get_id(c);
        new_pos.x       = panning.x + j * settings.grid_layout_x_distance;
        ImNodes::SetNodeGridSpacePos(pack_node(c_id), new_pos);
    }
}

static bool can_add_this_component(component_editor&  ed,
                                   const component_id id) noexcept
{
    auto* head_tree = ed.mod.tree_nodes.try_to_get(ed.mod.head);
    irt_assert(head_tree);

    if (head_tree->id == id)
        return false;

    if (auto* parent = head_tree->tree.get_parent(); parent) {
        do {
            if (parent->id == id)
                return false;

            parent = parent->tree.get_parent();
        } while (parent);
    }

    return true;
}

static status add_component_to_current(component_editor& ed,
                                       tree_node&        parent,
                                       component&        parent_compo,
                                       component&        compo_to_add,
                                       ImVec2 /*click_pos*/)
{
    const auto compo_to_add_id = ed.mod.components.get_id(compo_to_add);

    if (!can_add_this_component(ed, compo_to_add_id)) {
        auto* app   = container_of(&ed, &application::c_editor);
        auto& notif = app->notifications.alloc(log_level::error);
        notif.title = "Fail to add component";
        format(notif.message,
               "Irritator does not accept recursive component {}",
               compo_to_add.name.sv());
        app->notifications.enable(notif);
        return status::gui_not_enough_memory; //! @TODO replace with correct
                                              //! error
    }

    tree_node_id tree_id;
    irt_return_if_bad(ed.mod.make_tree_from(compo_to_add, &tree_id));

    auto& c            = parent_compo.children.alloc(compo_to_add_id);
    parent_compo.state = component_status::modified;

    auto& tree        = ed.mod.tree_nodes.get(tree_id);
    tree.id_in_parent = parent_compo.children.get_id(c);
    tree.tree.set_id(&tree);
    tree.tree.parent_to(parent.tree);

    return status::success;
}

static void show_popup_all_component_menuitem(component_editor& ed,
                                              tree_node&        tree,
                                              component&        parent,
                                              ImVec2 click_pos) noexcept
{
    if (ImGui::BeginMenu("Internal library")) {
        component* compo = nullptr;
        while (ed.mod.components.next(compo)) {
            const bool is_internal =
              !match(compo->type, component_type::file, component_type::memory);

            if (is_internal) {
                if (ImGui::MenuItem(compo->name.c_str())) {
                    add_component_to_current(
                      ed, tree, parent, *compo, click_pos);
                }
            }
        }

        ImGui::EndMenu();
    }

    for (auto id : ed.mod.component_repertories) {
        static small_string<32> s; //! @TODO remove this variable
        small_string<32>*       select;

        auto& reg_dir = ed.mod.registred_paths.get(id);
        if (reg_dir.name.empty()) {
            format(s, "{}", ordinal(id));
            select = &s;
        } else {
            select = &reg_dir.name;
        }

        ImGui::PushID(&reg_dir);
        if (ImGui::BeginMenu(select->c_str())) {
            for (auto dir_id : reg_dir.children) {
                auto* dir = ed.mod.dir_paths.try_to_get(dir_id);
                if (!dir)
                    break;

                if (ImGui::BeginMenu(dir->path.c_str())) {
                    for (auto file_id : dir->children) {
                        auto* file = ed.mod.file_paths.try_to_get(file_id);
                        if (!file)
                            break;

                        auto* compo =
                          ed.mod.components.try_to_get(file->component);
                        if (!compo)
                            break;

                        if (ImGui::MenuItem(file->path.c_str())) {
                            add_component_to_current(
                              ed, tree, parent, *compo, click_pos);
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
        while (ed.mod.components.next(compo)) {
            const bool is_not_saved =
              match(compo->type, component_type::memory);

            if (is_not_saved) {
                if (ImGui::MenuItem(compo->name.c_str())) {
                    add_component_to_current(
                      ed, tree, parent, *compo, click_pos);
                }
            }
        }

        ImGui::EndMenu();
    }
}

static void show_popup_menuitem(component_editor& ed,
                                tree_node&        tree,
                                component&        parent) noexcept
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
                            &ed.show_input_output)) {
            ed.first_show_input_output = true;
        }

        ImGui::MenuItem(
          "Fix component input/output ports", nullptr, &ed.fix_input_output);

        ImGui::Separator();

        if (ImGui::MenuItem("Force grid layout")) {
            auto* app = container_of(&ed, &application::c_editor);
            compute_grid_layout(app->settings, parent);
        }

        ImGui::Separator();

        show_popup_all_component_menuitem(ed, tree, parent, click_pos);

        ImGui::Separator();

        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              ed, parent, dynamics_type::integrator, click_pos);
            add_popup_menuitem(
              ed, parent, dynamics_type::quantifier, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::adder_2, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::adder_3, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::adder_4, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::mult_2, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::mult_3, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::mult_4, click_pos);
            add_popup_menuitem(ed, parent, dynamics_type::cross, click_pos);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Logical")) {
            add_popup_menuitem(
              ed, parent, dynamics_type::logical_and_2, click_pos);
            add_popup_menuitem(
              ed, parent, dynamics_type::logical_or_2, click_pos);
            add_popup_menuitem(
              ed, parent, dynamics_type::logical_and_3, click_pos);
            add_popup_menuitem(
              ed, parent, dynamics_type::logical_or_3, click_pos);
            add_popup_menuitem(
              ed, parent, dynamics_type::logical_invert, click_pos);
            ImGui::EndMenu();
        }

        add_popup_menuitem(ed, parent, dynamics_type::counter, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::queue, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::dynamic_queue, click_pos);
        add_popup_menuitem(
          ed, parent, dynamics_type::priority_queue, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::generator, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::constant, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::time_func, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::accumulator_2, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::filter, click_pos);
        add_popup_menuitem(ed, parent, dynamics_type::hsm_wrapper, click_pos);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

// static bool is_ports_compatible(const component_editor& /*ed*/,
//                                 const model& output,
//                                 const int    output_port,
//                                 const model& input,
//                                 const int    input_port) noexcept
// {
//     return is_ports_compatible(output, output_port, input, input_port);
// }

// static bool is_ports_compatible(const component_editor& ed,
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
//                   ed, output, output_port, *mdl, input_port_index);
//         } else {
//             auto  compo_id = enum_cast<component_id>(child->id);
//             auto* compo    = ed.mod.components.try_to_get(compo_id);

//             if (compo)
//                 return is_ports_compatible(
//                   ed, output, output_port, *compo, input_port_index);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& ed,
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
//                   ed, *mdl, output_port_index, input, input_port);
//         } else {
//             auto  compo_id = enum_cast<component_id>(child->id);
//             auto* compo    = ed.mod.components.try_to_get(compo_id);

//             if (compo)
//                 return is_ports_compatible(
//                   ed, *compo, output_port_index, input, input_port);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& ed,
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
//                 return is_ports_compatible(ed,
//                                            *output_mdl,
//                                            output_port_index,
//                                            *input_mdl,
//                                            input_port_index);
//         } else {
//             auto  output_compo_id =
//             enum_cast<component_id>(output_child->id); auto* output_compo =
//             ed.mod.components.try_to_get(output_compo_id);

//             if (output_compo)
//                 return is_ports_compatible(ed,
//                                            *output_compo,
//                                            output_port_index,
//                                            *input_mdl,
//                                            input_port_index);
//         }
//     } else {
//         auto  input_compo_id = enum_cast<component_id>(input_child->id);
//         auto* input_compo    = ed.mod.components.try_to_get(input_compo_id);

//         if (!input_compo)
//             return false;

//         if (output_child->type == child_type::model) {
//             auto  output_mdl_id = enum_cast<model_id>(output_child->id);
//             auto* output_mdl    = output.models.try_to_get(output_mdl_id);

//             if (output_mdl)
//                 return is_ports_compatible(ed,
//                                            *output_mdl,
//                                            output_port_index,
//                                            *input_compo,
//                                            input_port_index);
//         } else {
//             auto  output_compo_id =
//             enum_cast<component_id>(output_child->id); auto* output_compo =
//             ed.mod.components.try_to_get(output_compo_id);

//             if (output_compo)
//                 return is_ports_compatible(ed,
//                                            *output_compo,
//                                            output_port_index,
//                                            *input_compo,
//                                            input_port_index);
//         }
//     }

//     return false;
// }

// static bool is_ports_compatible(const component_editor& ed,
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
//               ed, *mdl_output, output_port, *mdl_input, input_port);
//         } else {
//             auto  compo_input_id = enum_cast<component_id>(input.id);
//             auto* compo_input    =
//             ed.mod.components.try_to_get(compo_input_id);

//             if (!compo_input)
//                 return false;

//             return is_ports_compatible(
//               ed, *mdl_output, output_port, *compo_input, input_port);
//         }
//     } else {
//         auto  compo_output_id = enum_cast<component_id>(output.id);
//         auto* compo_output    =
//         ed.mod.components.try_to_get(compo_output_id);

//         if (!compo_output)
//             return false;

//         if (input.type == child_type::model) {
//             auto  mdl_input_id = enum_cast<model_id>(input.id);
//             auto* mdl_input    = parent.models.try_to_get(mdl_input_id);

//             if (!mdl_input)
//                 return false;

//             return is_ports_compatible(
//               ed, *compo_output, output_port, *mdl_input, input_port);
//         } else {
//             auto  compo_input_id = enum_cast<component_id>(input.id);
//             auto* compo_input    =
//             ed.mod.components.try_to_get(compo_input_id);

//             if (!compo_input)
//                 return false;

//             return is_ports_compatible(
//               ed, *compo_output, output_port, *compo_input, input_port);
//         }
//     }
// }

static void is_link_created(component_editor& ed, component& parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        u32 index_src, index_dst;
        i8  port_src_index, port_dst_index;

        if (!parent.connections.can_alloc()) {
            auto* app = container_of(&ed, &application::c_editor);
            auto& n   = app->notifications.alloc(log_level::error);
            n.title   = "Not enough connection slot in this component";
            format(n.message,
                   "All connections slots ({}) are used.",
                   parent.connections.capacity());
            app->notifications.enable(n);
            return;
        }

        if (is_component_input_or_output(start)) {
            if (is_component_input_or_output(end)) {
                auto* app = container_of(&ed, &application::c_editor);
                auto& n   = app->notifications.alloc(log_level::error);
                n.title   = "Can not connect component input on output ports";
                app->notifications.enable(n);
                return;
            }

            auto index = unpack_component_input(start);
            unpack_in(end, &index_dst, &port_dst_index);

            auto* child_dst = parent.children.try_to_get(index_dst);
            if (child_dst == nullptr)
                return;

            auto child_dst_id = parent.children.get_id(*child_dst);
            if (is_success(ed.mod.connect_input(parent,
                                                static_cast<i8>(index),
                                                child_dst_id,
                                                port_dst_index)))
                parent.state = component_status::modified;
        } else {
            if (is_component_input_or_output(end)) {
                auto index = unpack_component_output(end);
                unpack_out(start, &index_src, &port_src_index);
                auto* child_src = parent.children.try_to_get(index_src);
                if (child_src == nullptr)
                    return;

                auto child_src_id = parent.children.get_id(*child_src);
                if (is_success(ed.mod.connect_output(parent,
                                                     child_src_id,
                                                     port_src_index,
                                                     static_cast<i8>(index))))
                    parent.state = component_status::modified;
            } else {
                unpack_out(start, &index_src, &port_src_index);
                unpack_in(end, &index_dst, &port_dst_index);

                auto* child_src = parent.children.try_to_get(index_src);
                auto* child_dst = parent.children.try_to_get(index_dst);

                if (!(child_src != nullptr && child_dst != nullptr))
                    return;

                auto child_src_id = parent.children.get_id(*child_src);
                auto child_dst_id = parent.children.get_id(*child_dst);

                if (is_success(ed.mod.connect(parent,
                                              child_src_id,
                                              port_src_index,
                                              child_dst_id,
                                              port_dst_index)))
                    parent.state = component_status::modified;
            }
        }
    }
}

static void is_link_destroyed(component& parent) noexcept
{
    int link_id;
    if (ImNodes::IsLinkDestroyed(&link_id)) {
        const auto link_id_correct = static_cast<u32>(link_id);
        if (auto* con = parent.connections.try_to_get(link_id_correct); con) {
            parent.connections.free(*con);
            parent.state = component_status::modified;
        }
    }
}

static void remove_nodes(component_editor& ed,
                         tree_node&        tree,
                         component&        parent) noexcept
{
    for (i32 i = 0, e = ed.selected_nodes.size(); i != e; ++i) {
        if (auto* child = unpack_node(ed.selected_nodes[i], parent.children);
            child) {
            if (child->type == child_type::component) {
                if (auto* c = tree.tree.get_child(); c) {
                    do {
                        if (c->id == enum_cast<component_id>(child->id)) {
                            c->tree.remove_from_hierarchy();
                            c = nullptr;
                        } else {
                            c = c->tree.get_sibling();
                        }
                    } while (c);
                }
            }

            ed.mod.free(parent, *child);
            parent.state = component_status::modified;
        }
    }

    ed.selected_nodes.clear();
    ImNodes::ClearNodeSelection();

    parent.state = component_status::modified;
}

static void remove_links(component_editor& ed, component& parent) noexcept
{
    std::sort(
      ed.selected_links.begin(), ed.selected_links.end(), std::greater<int>());

    for (i32 i = 0, e = ed.selected_links.size(); i != e; ++i) {
        const auto link_id = static_cast<u32>(ed.selected_links[i]);
        auto*      con     = parent.connections.try_to_get(link_id);
        if (con) {
            parent.connections.free(*con);
            parent.state = component_status::modified;
        }
    }

    ed.selected_links.clear();
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

static void show_component_editor(component_editor& ed,
                                  tree_node&        tree,
                                  component&        compo) noexcept
{
    ImNodes::EditorContextSet(ed.context);
    ImNodes::BeginNodeEditor();

    show_popup_menuitem(ed, tree, compo);
    show_opened_component_ref(ed, tree, compo);

    if (ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    is_link_created(ed, compo);
    is_link_destroyed(compo);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        ed.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(ed.selected_nodes.begin());
        remove_component_input_output(ed.selected_nodes);
    } else {
        ed.selected_nodes.clear();
    }

    if (num_selected_links > 0) {
        ed.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(ed.selected_links.begin());
    } else {
        ed.selected_links.clear();
    }

    if (ImGui::IsKeyReleased(ImGuiKey_Delete)) {
        if (num_selected_nodes > 0)
            remove_nodes(ed, tree, compo);
        else if (num_selected_links > 0)
            remove_links(ed, compo);
    }
}

component_editor::component_editor() noexcept
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
}

component_editor::~component_editor() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void component_editor::show() noexcept
{
    if (!ImGui::Begin(component_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (auto* tree = mod.tree_nodes.try_to_get(selected_component); tree) {
        if (auto* compo = mod.components.try_to_get(tree->id); compo)
            show_component_editor(*this, *tree, *compo);
    }

    ImGui::End();
}

} // namespace irt
