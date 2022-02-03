// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

template<typename Dynamics>
static void add_input_attribute(const Dynamics& dyn, child_id id) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
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
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();

        irt_assert(length(dyn.y) < 8);

        for (int i = 0; i < length(dyn.y); ++i) {
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
    auto* src = parent.children.try_to_get(con.src);
    if (!src)
        return false;

    auto* dst = parent.children.try_to_get(con.dst);
    if (!dst)
        return false;

    ImNodes::Link(get_index(parent.connections.get_id(con)),
                  pack_out(con.src, con.index_src),
                  pack_in(con.dst, con.index_dst));

    return true;
}

static void show(const application::settings_manager& settings,
                 component_editor&                    ed,
                 model&                               mdl,
                 child&                               c,
                 child_id                             id) noexcept
{
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
      "{}\n{}", c.name.c_str(), get_dynamics_type_name(mdl.type));
    ImNodes::EndNodeTitleBar();

    dispatch(mdl, [&ed, id](auto& dyn) {
        add_input_attribute(dyn, id);
        ImGui::PushItemWidth(120.0f);
        show_dynamics_inputs(ed.mod.srcs, dyn);
        ImGui::PopItemWidth();
        add_output_attribute(dyn, id);
    });

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show(const application::settings_manager& settings,
                 component&                           compo,
                 child&                               c,
                 child_id                             id) noexcept
{
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

    irt_assert(length(compo.x) < INT8_MAX);
    irt_assert(length(compo.y) < INT8_MAX);

    {
        int i = 0;
        while (i < compo.x.ssize()) {
            const auto  chld_id = compo.x[i].id;
            const auto* chld    = compo.children.try_to_get(chld_id);

            irt_assert(chld);
            irt_assert(chld->type == child_type::model);

            const auto  mdl_id = enum_cast<model_id>(chld->id);
            const auto* mdl    = compo.models.try_to_get(mdl_id);

            if (mdl) {
                ImNodes::BeginInputAttribute(pack_in(id, static_cast<i8>(i)),
                                             ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted(chld->name.c_str());
                ImNodes::EndInputAttribute();

                ++i;
            } else {
                compo.x.swap_pop_back(i);
            }
        }
    }

    {
        int i = 0;
        while (i < compo.y.ssize()) {
            const auto  chld_id = compo.y[i].id;
            const auto* chld    = compo.children.try_to_get(chld_id);

            irt_assert(chld);
            irt_assert(chld->type == child_type::model);

            const auto  mdl_id = enum_cast<model_id>(chld->id);
            const auto* mdl    = compo.models.try_to_get(mdl_id);

            if (mdl) {
                ImNodes::BeginOutputAttribute(pack_out(id, static_cast<i8>(i)),
                                              ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted(chld->name.c_str());
                ImNodes::EndOutputAttribute();

                ++i;
            } else {
                compo.y.swap_pop_back(i);
            }
        }
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_opened_component_ref(
  const application::settings_manager& settings,
  component_editor&                    ed,
  tree_node& /*ref*/,
  component& parent) noexcept
{
    child* c = nullptr;

    while (parent.children.next(c)) {
        const auto child_id = parent.children.get_id(c);

        if (c->type == child_type::model) {
            auto id = enum_cast<model_id>(c->id);
            if (auto* mdl = parent.models.try_to_get(id); mdl)
                show(settings, ed, *mdl, *c, child_id);
        } else {
            auto id = enum_cast<component_id>(c->id);
            if (auto* compo = ed.mod.components.try_to_get(id); compo)
                show(settings, *compo, *c, child_id);
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
                               child_id*         new_model)
{
    if (!parent.models.can_alloc(1)) {
        auto* app = container_of(&ed, &application::c_editor);
        app->log_w.log(2, "can not allocate a new model");
        return;
    }

    if (ImGui::MenuItem(get_dynamics_type_name(type))) {
        auto& child  = ed.mod.alloc(parent, type);
        *new_model   = parent.children.get_id(child);
        parent.state = component_status::modified;

        auto* app = container_of(&ed, &application::c_editor);
        app->log_w.log(
          7, "new model %zu\n", ordinal(parent.children.get_id(child)));
    }
}

static void add_popup_menuitem(component_editor& ed,
                               component&        parent,
                               int               type,
                               child_id*         new_model)
{
    auto d_type = enum_cast<dynamics_type>(type);
    add_popup_menuitem(ed, parent, d_type, new_model);
}

static void show_popup_menuitem(component_editor& ed,
                                component&        parent,
                                ImVec2*           click_pos,
                                child_id*         new_model) noexcept
{
    const bool open_popup =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

    *new_model = undefined<child_id>();
    *click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
    if (!ImGui::IsAnyItemHovered() && open_popup)
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        if (ImGui::BeginMenu("QSS1")) {
            auto       i = ordinal(dynamics_type::qss1_integrator);
            const auto e = ordinal(dynamics_type::qss1_wsum_4);
            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS2")) {
            auto       i = ordinal(dynamics_type::qss2_integrator);
            const auto e = ordinal(dynamics_type::qss2_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("QSS3")) {
            auto       i = ordinal(dynamics_type::qss3_integrator);
            const auto e = ordinal(dynamics_type::qss3_wsum_4);

            for (; i < e; ++i)
                add_popup_menuitem(ed, parent, i, new_model);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AQSS (experimental)")) {
            add_popup_menuitem(
              ed, parent, dynamics_type::integrator, new_model);
            add_popup_menuitem(
              ed, parent, dynamics_type::quantifier, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_2, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_3, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::adder_4, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_2, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_3, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::mult_4, new_model);
            add_popup_menuitem(ed, parent, dynamics_type::cross, new_model);
            ImGui::EndMenu();
        }

        add_popup_menuitem(ed, parent, dynamics_type::counter, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::queue, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::dynamic_queue, new_model);
        add_popup_menuitem(
          ed, parent, dynamics_type::priority_queue, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::generator, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::constant, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::time_func, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::accumulator_2, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::filter, new_model);
        add_popup_menuitem(ed, parent, dynamics_type::flow, new_model);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

static void is_link_created(component& parent) noexcept
{
    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end)) {
        u32 index_src, index_dst;
        i8  port_src_index, port_dst_index;

        unpack_out(start, &index_src, &port_src_index);
        unpack_in(end, &index_dst, &port_dst_index);

        auto* child_src = parent.children.try_to_get(index_src);
        auto* child_dst = parent.children.try_to_get(index_dst);

        if (child_src != nullptr && child_dst != nullptr) {
            auto child_src_id = parent.children.get_id(*child_src);
            auto child_dst_id = parent.children.get_id(*child_dst);
            parent.connections.alloc(
              child_src_id, port_src_index, child_dst_id, port_dst_index);

            parent.state = component_status::modified;
        }
    }
}

static void remove_nodes(component_editor& ed,
                         tree_node&        tree,
                         component&        parent) noexcept
{
    for (i32 i = 0, e = ed.selected_nodes.ssize(); i != e; ++i) {
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

    for (i32 i = 0, e = ed.selected_links.ssize(); i != e; ++i) {
        auto* con = parent.connections.try_to_get(ed.selected_links[i]);
        if (con) {
            parent.connections.free(*con);
            parent.state = component_status::modified;
        }
    }

    ed.selected_links.clear();
    ImNodes::ClearLinkSelection();

    parent.state = component_status::modified;
}

static void show_modeling_widget(const application::settings_manager& settings,
                                 component_editor&                    ed,
                                 tree_node&                           tree,
                                 component& compo) noexcept
{
    ImNodes::EditorContextSet(ed.context);
    ImNodes::BeginNodeEditor();

    ImVec2   click_pos;
    child_id new_model;

    show_opened_component_ref(settings, ed, tree, compo);
    show_popup_menuitem(ed, compo, &click_pos, &new_model);

    if (ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    if (auto* child = compo.children.try_to_get(new_model); child) {
        compo.state = component_status::modified;
        ImNodes::SetNodeScreenSpacePos(pack_node(new_model), click_pos);
        child->x = click_pos.x;
        child->y = click_pos.y;
    }

    is_link_created(compo);

    int num_selected_links = ImNodes::NumSelectedLinks();
    int num_selected_nodes = ImNodes::NumSelectedNodes();
    if (num_selected_nodes > 0) {
        ed.selected_nodes.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(ed.selected_nodes.begin());
    } else {
        ed.selected_nodes.clear();
    }

    if (num_selected_links > 0) {
        ed.selected_links.resize(num_selected_links);
        ImNodes::GetSelectedLinks(ed.selected_links.begin());
    } else {
        ed.selected_links.clear();
    }

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
        if (num_selected_nodes > 0)
            remove_nodes(ed, tree, compo);
        else if (num_selected_links > 0)
            remove_links(ed, compo);
    }
}

static void show_output_widget(component_editor& ed) noexcept
{
    if (ImGui::BeginTable("Observations", 5)) {
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("time-step", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("capacity", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableHeadersRow();
        memory_output* out = nullptr;
        while (ed.outputs.next(out)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::TextFormat("{}", ordinal(ed.outputs.get_id(*out)));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(out->name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->time_step);
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->xs.ssize());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", out->xs.capacity());
        }

        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImPlot::BeginPlot("simulation", "t", "s")) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            memory_output* obs = nullptr;
            while (ed.outputs.next(obs)) {
                const auto sz = obs->ys.ssize();

                if (sz) {
                    if (obs->interpolate) {
                        ImPlot::PlotLine(obs->name.c_str(),
                                         obs->xs.data(),
                                         obs->ys.data(),
                                         sz);

                    } else {
                        ImPlot::PlotScatter(obs->name.c_str(),
                                            obs->xs.data(),
                                            obs->ys.data(),
                                            sz);
                    }
                }
            }

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }
}

void application::show_main_as_tabbar(ImVec2           position,
                                      ImVec2           size,
                                      ImGuiWindowFlags window_flags,
                                      ImGuiCond        position_flags,
                                      ImGuiCond        size_flags) noexcept
{

    ImGui::SetNextWindowPos(position, position_flags);
    ImGui::SetNextWindowSize(size, size_flags);
    if (ImGui::Begin("Main", 0, window_flags)) {
        auto* tree =
          c_editor.mod.tree_nodes.try_to_get(c_editor.selected_component);
        if (!tree) {
            ImGui::End();
            return;
        }

        component* compo = c_editor.mod.components.try_to_get(tree->id);
        if (!compo) {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("##ModelingTabBar")) {
            if (ImGui::BeginTabItem("Modeling editor")) {
                show_modeling_widget(settings, c_editor, *tree, *compo);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Simulation editor")) {
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Output editor")) {
                show_output_widget(c_editor);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void application::show_main_as_window(ImVec2 position, ImVec2 size) noexcept
{
    size.x -= 50;
    size.y -= 50;

    if (show_modeling_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Modeling editor", &show_modeling_editor)) {
            auto* tree =
              c_editor.mod.tree_nodes.try_to_get(c_editor.selected_component);
            if (tree) {
                component* compo = c_editor.mod.components.try_to_get(tree->id);
                if (compo)
                    show_modeling_widget(settings, c_editor, *tree, *compo);
            }
        }
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_simulation_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Simulation editor", &show_simulation_editor)) {
        }
        ImGui::End();
    }

    position.x += 25;
    position.y += 25;

    if (show_output_editor) {
        ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        if (ImGui::Begin("Output editor", &show_output_editor)) {
            show_output_widget(c_editor);
        }
        ImGui::End();
    }
}

} // namespace irt
