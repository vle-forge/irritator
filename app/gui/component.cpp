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
        show_hierarchy_window();
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
        show_modeling_window();
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
