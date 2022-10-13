// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

// static void print_tree(const data_array<component, component_id>& components,
//                        tree_node* top_id) noexcept
// {
//     struct node
//     {
//         node(tree_node* tree_) noexcept
//           : tree(tree_)
//           , i(0)
//         {
//         }

//         node(tree_node* tree_, int i_) noexcept
//           : tree(tree_)
//           , i(i_)
//         {
//         }

//         tree_node* tree;
//         int        i = 0;
//     };

//     vector<node> stack;
//     stack.emplace_back(top_id);

//     while (!stack.empty()) {
//         auto cur = stack.back();
//         stack.pop_back();

//         auto  compo_id = cur.tree->id;
//         auto* compo    = components.try_to_get(compo_id);

//         if (compo) {
//             fmt::print("{:{}} {}\n", " ", cur.i, fmt::ptr(compo));
//         }

//         if (auto* sibling = cur.tree->tree.get_sibling(); sibling) {
//             stack.emplace_back(sibling, cur.i);
//         }

//         if (auto* child = cur.tree->tree.get_child(); child) {
//             stack.emplace_back(child, cur.i + 1);
//         }
//     }
// }

void settings_manager::show(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_Once);
    if (!ImGui::Begin("Component settings", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Dir paths");

    static const char* dir_status[] = { "none", "read", "unread" };

    auto* app      = container_of(this, &application::settings);
    auto& c_editor = app->c_editor;

    if (ImGui::BeginTable("Component directories", 6)) {
        ImGui::TableSetupColumn(
          "Path", ImGuiTableColumnFlags_WidthStretch, -FLT_MIN);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Refresh", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        registred_path* dir       = nullptr;
        registred_path* to_delete = nullptr;
        while (c_editor.mod.registred_paths.next(dir)) {
            if (to_delete) {
                const auto id = c_editor.mod.registred_paths.get_id(*to_delete);
                c_editor.mod.registred_paths.free(*to_delete);
                to_delete = nullptr;

                i32 i = 0, e = c_editor.mod.component_repertories.ssize();
                for (; i != e; ++i) {
                    if (c_editor.mod.component_repertories[i] == id) {
                        c_editor.mod.component_repertories.swap_pop_back(i);
                        break;
                    }
                }
            }

            ImGui::PushID(dir);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputSmallString(
              "##path", dir->path, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(150.f);
            ImGui::InputSmallString("##name", dir->name);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            constexpr i8 p_min = INT8_MIN;
            constexpr i8 p_max = INT8_MAX;
            ImGui::SliderScalar(
              "##input", ImGuiDataType_S8, &dir->priority, &p_min, &p_max);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            ImGui::TextUnformatted(dir_status[ordinal(dir->status)]);
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            if (ImGui::Button("Refresh")) {
                c_editor.mod.fill_components(*dir);
            }
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(60.f);
            if (ImGui::Button("Delete"))
                to_delete = dir;
            ImGui::PopItemWidth();

            ImGui::PopID();
        }

        if (to_delete) {
            c_editor.mod.free(*to_delete);
        }

        ImGui::EndTable();

        if (c_editor.mod.registred_paths.can_alloc(1) &&
            ImGui::Button("Add directory")) {
            auto& dir    = c_editor.mod.registred_paths.alloc();
            auto  id     = c_editor.mod.registred_paths.get_id(dir);
            dir.status   = registred_path::status_option::none;
            dir.path     = "";
            dir.priority = 127;
            app->show_select_directory_dialog = true;
            app->select_dir_path              = id;
            c_editor.mod.component_repertories.emplace_back(id);
        }
    }

    ImGui::Separator();
    ImGui::Text("Graphics");

    {
        if (ImGui::Combo(
              "Style selector", &style_selector, "Dark\0Light\0Classic\0")) {
            switch (style_selector) {
            case 0:
                ImGui::StyleColorsDark();
                ImNodes::StyleColorsDark();
                break;
            case 1:
                ImGui::StyleColorsLight();
                ImNodes::StyleColorsLight();
                break;
            case 2:
                ImGui::StyleColorsClassic();
                ImNodes::StyleColorsClassic();
                break;
            }
        }
    }

    if (ImGui::ColorEdit3(
          "model", (float*)&gui_model_color, ImGuiColorEditFlags_NoOptions))
        update();

    if (ImGui::ColorEdit3("component",
                          (float*)&gui_component_color,
                          ImGuiColorEditFlags_NoOptions))
        update();

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

void application::show_memory_box(bool* is_open) noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Component memory", is_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Modeling", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("tree_nodes: {} / {} / {}",
                          c_editor.mod.tree_nodes.size(),
                          c_editor.mod.tree_nodes.max_used(),
                          c_editor.mod.tree_nodes.capacity());
        ImGui::TextFormat("descriptions: {} / {} / {}",
                          c_editor.mod.descriptions.size(),
                          c_editor.mod.descriptions.max_used(),
                          c_editor.mod.descriptions.capacity());
        ImGui::TextFormat("components: {} / {} / {}",
                          c_editor.mod.components.size(),
                          c_editor.mod.components.max_used(),
                          c_editor.mod.components.capacity());
        ImGui::TextFormat("registred_paths: {} / {} / {}",
                          c_editor.mod.registred_paths.size(),
                          c_editor.mod.registred_paths.max_used(),
                          c_editor.mod.registred_paths.capacity());
        ImGui::TextFormat("dir_paths: {} / {} / {}",
                          c_editor.mod.dir_paths.size(),
                          c_editor.mod.dir_paths.max_used(),
                          c_editor.mod.dir_paths.capacity());
        ImGui::TextFormat("file_paths: {} / {} / {}",
                          c_editor.mod.file_paths.size(),
                          c_editor.mod.file_paths.max_used(),
                          c_editor.mod.file_paths.capacity());
    }

    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        component* compo = nullptr;
        while (c_editor.mod.components.next(compo)) {
            ImGui::PushID(compo);
            if (ImGui::TreeNode(compo->name.c_str())) {
                ImGui::TextFormat("children: {}/{}",
                                  compo->children.size(),
                                  compo->children.capacity());
                ImGui::TextFormat("models: {}/{}",
                                  compo->models.size(),
                                  compo->models.capacity());
                ImGui::TextFormat(
                  "hsm: {}/{}", compo->hsms.size(), compo->hsms.capacity());
                ImGui::TextFormat("connections: {}/{}",
                                  compo->connections.size(),
                                  compo->connections.capacity());
                ImGui::Separator();

                ImGui::TextFormat("Dir: {}", ordinal(compo->dir));
                ImGui::TextFormat("Description: {}", ordinal(compo->desc));
                ImGui::TextFormat("File: {}", ordinal(compo->file));
                ImGui::TextFormat("Type: {}",
                                  component_type_names[ordinal(compo->type)]);

                ImGui::Separator();
                ImGui::TextFormat("X: {}", compo->x.ssize());
                ImGui::TextFormat("Y: {}", compo->y.ssize());

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Registred directoriess",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            registred_path* dir = nullptr;
            while (c_editor.mod.registred_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat(
                  "{}", ordinal(c_editor.mod.registred_paths.get_id(*dir)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dir->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Directories",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            dir_path* dir = nullptr;
            while (c_editor.mod.dir_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat("{}",
                                  ordinal(c_editor.mod.dir_paths.get_id(*dir)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dir->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Files", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            file_path* file = nullptr;
            while (c_editor.mod.file_paths.next(file)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat(
                  "{}", ordinal(c_editor.mod.file_paths.get_id(*file)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(file->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

static void unselect_editor_component_ref(component_editor& ed) noexcept
{
    ImNodes::EditorContextSet(ed.context);
    ed.selected_component = undefined<tree_node_id>();

    ImNodes::ClearLinkSelection();
    ImNodes::ClearNodeSelection();
    ed.selected_links.clear();
    ed.selected_nodes.clear();
}

component_id component_editor::add_empty_component() noexcept
{
    auto ret = undefined<component_id>();

    if (mod.components.can_alloc()) {
        auto& new_compo = mod.components.alloc();
        new_compo.name.assign("New component");
        new_compo.type  = component_type::memory;
        new_compo.state = component_status::modified;

        ret = mod.components.get_id(new_compo);
    } else {
        auto* app   = container_of(this, &application::c_editor);
        auto& notif = app->notifications.alloc(notification_type::error);
        notif.title = "Can not allocate new container";
        format(notif.message,
               "Components allocated: {}\nTree nodes allocated: {}",
               mod.components.size(),
               mod.tree_nodes.size());
    }

    return ret;
}

void component_editor::unselect() noexcept
{
    mod.clear_project();
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

void component_editor::show(bool* /*is_show*/) noexcept {}

//
// task implementation
//

static status save_component_impl(const modeling&       mod,
                                  const component&      compo,
                                  const registred_path& reg_path,
                                  const dir_path&       dir,
                                  const file_path&      file) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ reg_path.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return status::io_filesystem_make_directory_error;
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return status::io_filesystem_not_directory_error;
            }
        }

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
    auto* g_task       = reinterpret_cast<gui_task*>(param);
    g_task->state      = gui_task_status::started;
    g_task->app->state = application_status_read_only_modeling;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->app->c_editor.mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg =
          g_task->app->c_editor.mod.registred_paths.try_to_get(compo->reg_path);
        auto* dir = g_task->app->c_editor.mod.dir_paths.try_to_get(compo->dir);
        auto* file =
          g_task->app->c_editor.mod.file_paths.try_to_get(compo->file);

        if (reg && dir && file) {
            if (is_bad(save_component_impl(
                  g_task->app->c_editor.mod, *compo, *reg, *dir, *file))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
                compo->type  = component_type::file;
            }
        }
    }

    g_task->state = gui_task_status::finished;
}

static status save_component_description_impl(const registred_path& reg_dir,
                                              const dir_path&       dir,
                                              const file_path&      file,
                                              const description& desc) noexcept
{
    status ret = status::success;

    try {
        std::filesystem::path p{ reg_dir.path.sv() };
        p /= dir.path.sv();
        std::error_code ec;

        if (!std::filesystem::exists(p, ec)) {
            if (!std::filesystem::create_directory(p, ec)) {
                return status::io_filesystem_make_directory_error;
            }
        } else {
            if (!std::filesystem::is_directory(p, ec)) {
                return status::io_filesystem_not_directory_error;
            }
        }

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
    auto* g_task       = reinterpret_cast<gui_task*>(param);
    g_task->state      = gui_task_status::started;
    g_task->app->state = application_status_read_only_modeling;

    auto  compo_id = enum_cast<component_id>(g_task->param_1);
    auto* compo    = g_task->app->c_editor.mod.components.try_to_get(compo_id);

    if (compo) {
        auto* reg =
          g_task->app->c_editor.mod.registred_paths.try_to_get(compo->reg_path);
        auto* dir = g_task->app->c_editor.mod.dir_paths.try_to_get(compo->dir);
        auto* file =
          g_task->app->c_editor.mod.file_paths.try_to_get(compo->file);
        auto* desc =
          g_task->app->c_editor.mod.descriptions.try_to_get(compo->desc);

        if (dir && file && desc) {
            if (is_bad(
                  save_component_description_impl(*reg, *dir, *file, *desc))) {
                compo->state = component_status::modified;
            } else {
                compo->state = component_status::unmodified;
            }
        }
    }

    g_task->state = gui_task_status::finished;
}

} // irt
