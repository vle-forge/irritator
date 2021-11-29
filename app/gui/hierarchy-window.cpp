// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

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

void component_editor::show_hierarchy_window() noexcept
{
    if (auto* parent = mod.tree_nodes.try_to_get(mod.head); parent)
        show_component_hierarchy(*this, *parent);

    ImGui::Separator();

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Attributes", flags)) {
        auto* ref = mod.tree_nodes.try_to_get(selected_component);
        if (ref) {
            if (auto* compo = mod.components.try_to_get(ref->id); compo) {
                ImGui::InputSmallString("name", compo->name);
                if (compo->type == component_type::memory ||
                    compo->type == component_type::file) {
                    static constexpr const char* empty = "";

                    auto*       dir     = mod.dir_paths.try_to_get(compo->dir);
                    const char* preview = dir ? dir->path.c_str() : empty;
                    if (ImGui::BeginCombo(
                          "Select directory", preview, ImGuiComboFlags_None)) {
                        dir_path* list = nullptr;
                        while (mod.dir_paths.next(list)) {
                            if (ImGui::Selectable(list->path.c_str(),
                                                  preview == list->path.c_str(),
                                                  ImGuiSelectableFlags_None)) {
                                compo->dir = mod.dir_paths.get_id(list);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    auto* file = mod.file_paths.try_to_get(compo->file);
                    if (file) {
                        ImGui::InputSmallString("File##text", file->path);
                    } else {
                        ImGui::Text("File cannot be sav");
                        if (ImGui::Button("Add file")) {
                            auto& f     = mod.file_paths.alloc();
                            compo->file = mod.file_paths.get_id(f);
                        }
                    }

                    auto* desc = mod.descriptions.try_to_get(compo->desc);
                    if (!desc && mod.descriptions.can_alloc(1)) {
                        if (ImGui::Button("Add description")) {
                            auto& new_desc = mod.descriptions.alloc();
                            compo->desc    = mod.descriptions.get_id(new_desc);
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
                            mod.descriptions.free(*desc);
                            compo->desc = undefined<description_id>();
                        }
                    }

                    if (file && dir) {
                        if (ImGui::Button("Save")) {
                            {
                                auto& task = gui_tasks.alloc();
                                task.ed    = this;
                                task.param_1 =
                                  ordinal(mod.components.get_id(*compo));
                                task_mgr.task_lists[0].add(save_component,
                                                           &task);
                            }

                            {
                                auto& task = gui_tasks.alloc();
                                task.ed    = this;
                                task.param_1 =
                                  ordinal(mod.components.get_id(*compo));
                                task_mgr.task_lists[0].add(save_description,
                                                           &task);
                            }

                            task_mgr.task_lists[0].submit();
                        }
                    }
                }
            }
        }
    }
}

} // namespace irt