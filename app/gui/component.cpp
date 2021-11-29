// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

static ImVec4 operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

static void print_tree(const data_array<component, component_id>& components,
                       tree_node* top_id) noexcept
{
    struct node
    {
        node(tree_node* tree_) noexcept
          : tree(tree_)
          , i(0)
        {}

        node(tree_node* tree_, int i_) noexcept
          : tree(tree_)
          , i(i_)
        {}

        tree_node* tree;
        int        i = 0;
    };

    vector<node> stack;
    stack.emplace_back(top_id);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        auto  compo_id = cur.tree->id;
        auto* compo    = components.try_to_get(compo_id);

        if (compo) {
            fmt::print("{:{}} {}\n", " ", cur.i, fmt::ptr(compo));
        }

        if (auto* sibling = cur.tree->tree.get_sibling(); sibling) {
            stack.emplace_back(sibling, cur.i);
        }

        if (auto* child = cur.tree->tree.get_child(); child) {
            stack.emplace_back(child, cur.i + 1);
        }
    }
}

static void show_component_hierarchy(component_editor& ed, tree_node& parent)
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (auto* compo = ed.mod.components.try_to_get(parent.id); compo) {
        if (ImGui::TreeNodeEx(&parent, flags, "%s", compo->name.c_str())) {

            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ed.select(ed.mod.tree_nodes.get_id(parent));
                ImGui::TreePop();
                return;
            }

            if (auto* child = parent.tree.get_child(); child) {
                show_component_hierarchy(ed, *child);
            }
            ImGui::TreePop();
        }

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_component_hierarchy(ed, *sibling);
    }
}

static void show_component_hierarchy(component_editor& ed)
{
    if (auto* parent = ed.mod.tree_nodes.try_to_get(ed.mod.head); parent)
        show_component_hierarchy(ed, *parent);
}

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

static void show(component_editor& ed, model& mdl, child_id id) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_model_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_model_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_model_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat(
      "{}\n{}", get_index(id), get_dynamics_type_name(mdl.type));
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

static void show(component_editor& ed, component& compo, child_id id) noexcept
{
    ImNodes::PushColorStyle(
      ImNodesCol_TitleBar,
      ImGui::ColorConvertFloat4ToU32(ed.settings.gui_component_color));

    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                            ed.settings.gui_hovered_component_color);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                            ed.settings.gui_selected_component_color);

    ImNodes::BeginNode(pack_node(id));
    ImNodes::BeginNodeTitleBar();
    ImGui::TextFormat("{}\n{}", get_index(id), compo.name.c_str());
    ImNodes::EndNodeTitleBar();

    irt_assert(length(compo.x) < INT8_MAX);
    irt_assert(length(compo.y) < INT8_MAX);

    for (int i = 0; i < length(compo.x); ++i) {
        ImNodes::BeginInputAttribute(pack_in(id, static_cast<i8>(i)),
                                     ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    for (int i = 0; i < length(compo.y); ++i) {
        ImNodes::BeginOutputAttribute(pack_out(id, static_cast<i8>(i)),
                                      ImNodesPinShape_TriangleFilled);
        ImGui::TextFormat("{}", i);
        ImNodes::EndInputAttribute();
    }

    ImNodes::EndNode();

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
}

static void show_opened_component_ref(component_editor& ed,
                                      tree_node& /*ref*/,
                                      component& parent) noexcept
{
    child* c = nullptr;

    while (parent.children.next(c)) {
        const auto child_id = parent.children.get_id(c);

        if (c->type == child_type::model) {
            auto id = enum_cast<model_id>(c->id);
            if (auto* mdl = parent.models.try_to_get(id); mdl) {
                show(ed, *mdl, child_id);

                if (ed.force_node_position) {
                    ImNodes::SetNodeEditorSpacePos(pack_node(child_id),
                                                   ImVec2(c->x, c->y));
                } else {
                    auto pos =
                      ImNodes::GetNodeEditorSpacePos(pack_node(child_id));

                    if (c->x != pos.x || c->y != pos.y)
                        parent.state = component_status::modified;

                    c->x = pos.x;
                    c->y = pos.y;
                }
            }
        } else {
            auto id = enum_cast<component_id>(c->id);
            if (auto* compo = ed.mod.components.try_to_get(id); compo) {
                show(ed, *compo, child_id);

                if (ed.force_node_position) {
                    ImNodes::SetNodeEditorSpacePos(pack_node(child_id),
                                                   ImVec2(c->x, c->y));
                } else {
                    auto pos =
                      ImNodes::GetNodeEditorSpacePos(pack_node(child_id));

                    if (c->x != pos.x || c->y != pos.y)
                        parent.state = component_status::modified;

                    c->x = pos.x;
                    c->y = pos.y;
                }
            }
        }

        ed.force_node_position = false;
    }

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
        log_w.log(2, "can not allocate a new model");
        return;
    }

    if (ImGui::MenuItem(get_dynamics_type_name(type))) {
        auto& child  = ed.mod.alloc(parent, type);
        *new_model   = parent.children.get_id(child);
        parent.state = component_status::modified;

        log_w.log(7, "new model %zu\n", ordinal(parent.children.get_id(child)));
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

void remove_nodes(component_editor& ed,
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

void remove_links(component_editor& ed, component& parent) noexcept
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

static void show_opened_component(component_editor& ed) noexcept
{
    auto* tree = ed.mod.tree_nodes.try_to_get(ed.selected_component);
    if (!tree)
        return;

    component* compo = ed.mod.components.try_to_get(tree->id);
    if (!compo)
        return;

    ImNodes::EditorContextSet(ed.context);
    ImNodes::BeginNodeEditor();

    ImVec2   click_pos;
    child_id new_model;

    show_opened_component_ref(ed, *tree, *compo);
    show_popup_menuitem(ed, *compo, &click_pos, &new_model);

    if (ed.show_minimap)
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

    ImNodes::EndNodeEditor();

    if (auto* child = compo->children.try_to_get(new_model); child) {
        compo->state = component_status::modified;
        ImNodes::SetNodeScreenSpacePos(pack_node(new_model), click_pos);
        child->x = click_pos.x;
        child->y = click_pos.y;
    }

    is_link_created(*compo);

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
            remove_nodes(ed, *tree, *compo);
        else if (num_selected_links > 0)
            remove_links(ed, *compo);
    }
}

static void settings_compute_colors(
  component_editor::settings_manager& settings) noexcept
{
    settings.gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color * 1.25f);
    settings.gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_color * 1.5f);

    settings.gui_hovered_component_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color * 1.25f);
    settings.gui_selected_component_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_component_color * 1.5f);

    settings.gui_hovered_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_transition_color *
                                     1.25f);
    settings.gui_selected_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(settings.gui_model_transition_color *
                                     1.5f);
}

template<class T, class M>
constexpr std::ptrdiff_t offset_of(const M T::*member)
{
    return reinterpret_cast<std::ptrdiff_t>(
      &(reinterpret_cast<T*>(0)->*member));
}

template<class T, class M>
constexpr T* container_of(M* ptr, const M T::*member)
{
    return reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                                offset_of(member));
}

void component_editor::settings_manager::show(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Component settings", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Dir paths");

    static const char* dir_status[] = { "none", "read", "unread" };

    auto* c_ed = container_of(this, &component_editor::settings);
    if (ImGui::BeginTable("Component directories", 5)) {
        ImGui::TableSetupColumn(
          "Path", ImGuiTableColumnFlags_WidthStretch, -FLT_MIN);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Refresh", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        dir_path* dir       = nullptr;
        dir_path* to_delete = nullptr;
        while (c_ed->mod.dir_paths.next(dir)) {
            if (to_delete) {
                c_ed->mod.dir_paths.free(*to_delete);
                to_delete = nullptr;
            }

            ImGui::PushID(dir);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputSmallString(
              "##name", dir->path, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            constexpr i8 p_min = INT8_MIN;
            constexpr i8 p_max = INT8_MAX;
            ImGui::SliderScalar(
              "##input", ImGuiDataType_S8, &dir->priority, &p_min, &p_max);
            ImGui::PopItemWidth();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(dir_status[ordinal(dir->status)]);
            ImGui::TableNextColumn();
            if (ImGui::Button("Refresh")) {
                c_ed->mod.fill_components(*dir);
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("Delete"))
                to_delete = dir;
            ImGui::PopID();
        }

        if (to_delete) {
            c_ed->mod.dir_paths.free(*to_delete);
        }

        ImGui::EndTable();

        if (c_ed->mod.dir_paths.can_alloc(1) &&
            ImGui::Button("Add directory")) {
            auto& dir                          = c_ed->mod.dir_paths.alloc();
            dir.status                         = dir_path::status_option::none;
            dir.path                           = "";
            dir.priority                       = 127;
            c_ed->show_select_directory_dialog = true;
            c_ed->select_dir_path = c_ed->mod.dir_paths.get_id(dir);
        }
    }

    ImGui::Separator();
    ImGui::Text("Graphics");
    if (ImGui::ColorEdit3(
          "model", (float*)&gui_model_color, ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);
    if (ImGui::ColorEdit3(
          "model", (float*)&gui_component_color, ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);
    if (ImGui::ColorEdit3("model",
                          (float*)&gui_model_transition_color,
                          ImGuiColorEditFlags_NoOptions))
        settings_compute_colors(*this);

    ImGui::Separator();
    ImGui::Text("Automatic layout parameters");
    ImGui::DragInt(
      "max iteration", &automatic_layout_iteration_limit, 1.f, 0, 1000);
    ImGui::DragFloat(
      "a-x-distance", &automatic_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "a-y-distance", &automatic_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::Separator();
    ImGui::Text("Grid layout parameters");
    ImGui::DragFloat(
      "g-x-distance", &grid_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "g-y-distance", &grid_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::End();
}

void component_editor::show_memory_box(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Component memory", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::TextFormat("tree_nodes: {} / {} / {}",
                      mod.tree_nodes.size(),
                      mod.tree_nodes.max_used(),
                      mod.tree_nodes.capacity());
    ImGui::TextFormat("descriptions: {} / {} / {}",
                      mod.descriptions.size(),
                      mod.descriptions.max_used(),
                      mod.descriptions.capacity());
    ImGui::TextFormat("components: {} / {} / {}",
                      mod.components.size(),
                      mod.components.max_used(),
                      mod.components.capacity());
    ImGui::TextFormat("observers: {} / {} / {}",
                      mod.observers.size(),
                      mod.observers.max_used(),
                      mod.observers.capacity());
    ImGui::TextFormat("dir_paths: {} / {} / {}",
                      mod.dir_paths.size(),
                      mod.dir_paths.max_used(),
                      mod.dir_paths.capacity());
    ImGui::TextFormat("file_paths: {} / {} / {}",
                      mod.file_paths.size(),
                      mod.file_paths.max_used(),
                      mod.file_paths.capacity());

    if (ImGui::CollapsingHeader("Components")) {
        component* compo = nullptr;
        while (mod.components.next(compo)) {
            ImGui::PushID(compo);
            if (ImGui::TreeNode(compo->name.c_str())) {
                ImGui::Text("children: %d", (int)compo->children.size());
                ImGui::Text("models: %d", (int)compo->models.size());
                ImGui::Text("connections: %d", (int)compo->connections.size());
                ImGui::Text("x: %d", compo->x.ssize());
                ImGui::Text("y: %d", compo->y.ssize());

                ImGui::TextFormat("description: {}", ordinal(compo->desc));
                ImGui::TextFormat("dir: {}", ordinal(compo->dir));
                ImGui::TextFormat("file: {}", ordinal(compo->file));
                ImGui::TextFormat("type: {}",
                                  component_type_names[ordinal(compo->type)]);

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

static void unselect_editor_component_ref(component_editor& ed) noexcept
{
    ed.selected_component = undefined<tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();
    ed.selected_links.clear();
    ed.selected_nodes.clear();
}

void component_editor::unselect() noexcept
{
    mod.head = undefined<tree_node_id>();
    mod.tree_nodes.clear();

    unselect_editor_component_ref(*this);
}

void component_editor::select(tree_node_id id) noexcept
{
    if (auto* tree = mod.tree_nodes.try_to_get(id); tree) {
        if (auto* compo = mod.components.try_to_get(tree->id); compo) {
            unselect_editor_component_ref(*this);

            selected_component  = id;
            force_node_position = true;
        }
    }
}

void component_editor::open_as_main(component_id id) noexcept
{
    if (auto* compo = mod.components.try_to_get(id); compo) {
        unselect();

        tree_node_id parent_id;
        if (is_success(mod.make_tree_from(*compo, &parent_id))) {
            mod.head            = parent_id;
            selected_component  = parent_id;
            force_node_position = true;
        }
    }
}

static constexpr const task_manager_parameters tm_params = {
    .thread_number           = 3,
    .simple_task_list_number = 1,
    .multi_task_list_number  = 0,
};

component_editor::component_editor() noexcept
  : task_mgr(tm_params)
{
    task_mgr.workers[0].task_lists.emplace_back(&task_mgr.task_lists[0]);
    task_mgr.workers[1].task_lists.emplace_back(&task_mgr.task_lists[0]);
    task_mgr.workers[2].task_lists.emplace_back(&task_mgr.task_lists[0]);

    task_mgr.start();
}

void component_editor::init() noexcept
{
    if (!context) {
        context = ImNodes::EditorContextCreate();
        ImNodes::PushAttributeFlag(
          ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodesIO& io                           = ImNodes::GetIO();
        io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

        settings_compute_colors(settings);

        gui_tasks.init(64);
    }
}

void component_editor::shutdown() noexcept
{
    if (context) {
        task_mgr.finalize();

        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
        context = nullptr;
    }
}

static component_editor_status gui_task_clean_up(component_editor& ed) noexcept
{
    component_editor_status ret    = 0;
    gui_task*               task   = nullptr;
    gui_task*               to_del = nullptr;

    while (ed.gui_tasks.next(task)) {
        if (to_del) {
            ed.gui_tasks.free(*to_del);
            to_del = nullptr;
        }

        if (task->state == gui_task_status::finished) {
            to_del = task;
        } else {
            ret |= task->editor_state;
        }
    }

    if (to_del) {
        ed.gui_tasks.free(*to_del);
    }

    return ret;
}

void component_editor::show(bool* /*is_show*/) noexcept
{
    simulation_update_state();
    state = gui_task_clean_up(*this);

    constexpr ImGuiWindowFlags flag =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;

    const auto* viewport   = ImGui::GetMainViewport();
    const auto  region     = viewport->WorkSize;
    const float width_1_10 = region.x / 10.f;

    ImVec2 current_component_size(width_1_10 * 2.f, region.y / 2.f);
    ImVec2 simulation_size(width_1_10 * 2.f, region.y / 2.f);
    ImVec2 drawing_zone_size(width_1_10 * 6.f, region.y);
    ImVec2 component_list_size(width_1_10 * 2.f, region.y);

    ImVec2 current_component_pos(0.f, viewport->WorkPos.y);
    ImVec2 simulation_pos(0.f, viewport->WorkPos.y + region.y / 2.f);
    ImVec2 drawing_zone_pos(current_component_size.x, viewport->WorkPos.y);
    ImVec2 component_list_pos(current_component_size.x + drawing_zone_size.x,
                              viewport->WorkPos.y);

    ImGui::SetNextWindowPos(current_component_pos);
    ImGui::SetNextWindowSize(current_component_size);
    if (ImGui::Begin("Modeling component", 0, flag)) {
        show_component_hierarchy(*this);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(simulation_pos);
    ImGui::SetNextWindowSize(simulation_size);
    if (ImGui::Begin("Simulation#Window", 0, flag)) {
        show_simulation_window();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(drawing_zone_pos);
    ImGui::SetNextWindowSize(drawing_zone_size);
    if (ImGui::Begin("Component editor##Window", 0, flag)) {
        show_opened_component(*this);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(component_list_pos);
    ImGui::SetNextWindowSize(component_list_size);
    if (ImGui::Begin("Components list##Window", 0, flag)) {
        show_components_window();
        ImGui::End();
    }

    if (show_memory)
        show_memory_box(&show_memory);

    if (show_settings)
        settings.show(&show_settings);

    if (show_select_directory_dialog) {
        ImGui::OpenPopup("Select directory");
        if (select_directory_dialog(select_directory)) {
            auto* dir_path = mod.dir_paths.try_to_get(select_dir_path);
            if (dir_path) {
                auto str = select_directory.string();
                dir_path->path.assign(str);
            }

            show_select_directory_dialog = false;
            select_dir_path              = undefined<dir_path_id>();
            select_directory.clear();
        }
    }
}

//
// task implementation
//

static status save_component_impl(const modeling&  mod,
                                  const component& compo,
                                  const dir_path&  dir,
                                  const file_path& file) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ dir.path.sv() };
        p /= file.path.sv();
        p.replace_extension(".irt");

        std::ofstream ofs{ p };
        if (ofs.is_open()) {
            writer w{ ofs };
            ret = w(mod, compo, mod.srcs);
        } else {
            ret = status::io_file_format_error;
        }
    } catch (...) {
        ret = status::io_not_enough_memory;
    }

    return ret;
}

void save_component(void* param) noexcept
{
    auto* g_task      = reinterpret_cast<gui_task*>(param);
    g_task->state     = gui_task_status::started;
    g_task->ed->state = component_editor_status_read_only_modeling;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->ed->mod.components.try_to_get(compo_id);

    if (compo) {
        auto* dir  = g_task->ed->mod.dir_paths.try_to_get(compo->dir);
        auto* file = g_task->ed->mod.file_paths.try_to_get(compo->file);

        if (dir && file) {
            if (is_bad(
                  save_component_impl(g_task->ed->mod, *compo, *dir, *file))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
            }
        }
    }

    g_task->state = gui_task_status::finished;
}

static status save_component_description_impl(const dir_path&    dir,
                                              const file_path&   file,
                                              const description& desc) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ dir.path.sv() };
        p /= file.path.sv();
        p.replace_extension(".desc");

        std::ofstream ofs{ p };
        if (ofs.is_open()) {
            ofs.write(desc.data.c_str(), desc.data.size());
        } else {
            ret = status::io_file_format_error;
        }
    } catch (...) {
        ret = status::io_not_enough_memory;
    }

    return ret;
}

void save_description(void* param) noexcept
{
    auto* g_task      = reinterpret_cast<gui_task*>(param);
    g_task->state     = gui_task_status::started;
    g_task->ed->state = component_editor_status_read_only_modeling;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->ed->mod.components.try_to_get(compo_id);

    if (compo) {
        auto* dir  = g_task->ed->mod.dir_paths.try_to_get(compo->dir);
        auto* file = g_task->ed->mod.file_paths.try_to_get(compo->file);
        auto* desc = g_task->ed->mod.descriptions.try_to_get(compo->desc);

        if (dir && file && desc) {
            if (is_bad(save_component_description_impl(*dir, *file, *desc))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
            }
        }
    }

    g_task->state = gui_task_status::finished;
}

} // irt
