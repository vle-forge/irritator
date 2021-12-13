// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

struct project_hierarchy_data
{
    tree_node* parent = nullptr;
    component* compo  = nullptr;
    child*     ch     = nullptr;

    void set(tree_node* parent_, component* compo_) noexcept
    {
        parent = parent_;
        compo  = compo_;
        ch     = nullptr;
    }

    void set(tree_node* parent_, component* compo_, child* ch_) noexcept
    {
        parent = parent_;
        compo  = compo_;
        ch     = ch_;
    }

    bool is_current(tree_node* parent_,
                    component* compo_,
                    child*     ch_) const noexcept
    {
        return parent == parent_ && compo == compo_ && ch == ch_;
    }

    void clear() noexcept
    {
        parent = nullptr;
        compo  = nullptr;
        ch     = nullptr;
    }
};

static void show_project_hierarchy(component_editor&       ed,
                                   tree_node&              parent,
                                   project_hierarchy_data& data) noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (auto* compo = ed.mod.components.try_to_get(parent.id); compo) {
        if (ImGui::TreeNodeEx(&parent, flags, "%s", compo->name.c_str())) {
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                data.set(&parent, compo);
            }

            if (auto* child = parent.tree.get_child(); child) {
                show_project_hierarchy(ed, *child, data);
            }

            {
                child* pc = nullptr;
                while (compo->children.next(pc)) {
                    if (pc->configurable || pc->observable) {
                        ImGui::PushID(pc);

                        bool selected = data.is_current(&parent, compo, pc);

                        if (ImGui::Selectable(pc->name.c_str(), selected)) {
                            data.set(&parent, compo, pc);
                        }
                        ImGui::PopID();
                    }
                }
            }

            ImGui::TreePop();
        }

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(ed, *sibling, data);
    }
}

static void show_hierarchy_settings(component_editor& ed,
                                    tree_node&        parent) noexcept
{
    if (auto* compo = ed.mod.components.try_to_get(parent.id); compo) {
        ImGui::InputSmallString("name", compo->name);
        if (compo->type == component_type::memory ||
            compo->type == component_type::file) {
            static constexpr const char* empty = "";

            auto*       dir     = ed.mod.dir_paths.try_to_get(compo->dir);
            const char* preview = dir ? dir->path.c_str() : empty;
            if (ImGui::BeginCombo(
                  "Select directory", preview, ImGuiComboFlags_None)) {
                dir_path* list = nullptr;
                while (ed.mod.dir_paths.next(list)) {
                    if (ImGui::Selectable(list->path.c_str(),
                                          preview == list->path.c_str(),
                                          ImGuiSelectableFlags_None)) {
                        compo->dir = ed.mod.dir_paths.get_id(list);
                    }
                }
                ImGui::EndCombo();
            }

            auto* file = ed.mod.file_paths.try_to_get(compo->file);
            if (file) {
                ImGui::InputSmallString("File##text", file->path);
            } else {
                ImGui::Text("File cannot be sav");
                if (ImGui::Button("Add file")) {
                    auto& f     = ed.mod.file_paths.alloc();
                    compo->file = ed.mod.file_paths.get_id(f);
                }
            }

            auto* desc = ed.mod.descriptions.try_to_get(compo->desc);
            if (!desc) {
                if (ed.mod.descriptions.can_alloc(1) &&
                    ImGui::Button("Add description")) {
                    auto& new_desc = ed.mod.descriptions.alloc();
                    compo->desc    = ed.mod.descriptions.get_id(new_desc);
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
                    ed.mod.descriptions.free(*desc);
                    compo->desc = undefined<description_id>();
                }
            }

            if (file && dir) {
                if (ImGui::Button("Save")) {
                    {
                        auto& task = ed.gui_tasks.alloc();
                        task.ed    = &ed;
                        task.param_1 =
                          ordinal(ed.mod.components.get_id(*compo));
                        ed.task_mgr.task_lists[0].add(save_component, &task);
                    }

                    {
                        auto& task = ed.gui_tasks.alloc();
                        task.ed    = &ed;
                        task.param_1 =
                          ordinal(ed.mod.components.get_id(*compo));
                        ed.task_mgr.task_lists[0].add(save_description, &task);
                    }

                    ed.task_mgr.task_lists[0].submit();
                }
            }
        }
    }
}

static void show_project_observations(component_editor& ed,
                                      tree_node& /*parent*/,
                                      project_hierarchy_data& data) noexcept
{
    if (data.parent && data.compo && data.ch) {
        auto  id  = enum_cast<model_id>(data.ch->id);
        auto* mdl = data.compo->models.try_to_get(id);

        if (mdl) {
            auto*          value       = data.parent->observables.get(id);
            memory_output* obs         = nullptr;
            bool           is_observed = false;

            if (value) {
                auto output_id = enum_cast<memory_output_id>(*value);
                obs            = ed.outputs.try_to_get(output_id);
                if (!obs) {
                    data.parent->observables.erase(id);
                    value = nullptr;
                } else {
                    is_observed = true;
                }
            }

            if (ImGui::Checkbox("Enable##obs", &is_observed)) {
                if (is_observed) {
                    if (ed.outputs.can_alloc(1)) {
                        auto& new_obs    = ed.outputs.alloc();
                        auto  new_obs_id = ed.outputs.get_id(new_obs);
                        obs              = &new_obs;
                        data.parent->observables.set(id, ordinal(new_obs_id));
                    } else {
                        is_observed = false;
                    }
                } else {
                    if (obs) {
                        ed.outputs.free(*obs);
                    }
                    data.parent->observables.erase(id);
                }
            }

            if (is_observed && obs) {
                ImGui::InputSmallString("name##obs", obs->name);
                if (ImGui::InputReal("time-step##obs", &obs->time_step)) {
                    if (obs->time_step <= zero)
                        obs->time_step = one / to_real(100);
                }

                int old_current = obs->xs.capacity() <= 1000               ? 0
                                  : obs->xs.capacity() <= 1000 * 1000      ? 1
                                  : obs->xs.capacity() <= 16 * 1000 * 1000 ? 2
                                                                           : 3;
                int current     = old_current;

                ImGui::RadioButton("1 KB", &current, 0);
                ImGui::SameLine();
                ImGui::RadioButton("1 MB", &current, 1);
                ImGui::SameLine();
                ImGui::RadioButton("16 MB", &current, 2);
                ImGui::SameLine();
                ImGui::RadioButton("64 MB", &current, 3);

                if (current != old_current) {
                    i32 capacity = current == 0   ? 1000
                                   : current == 1 ? 1000 * 1000
                                   : current == 2 ? 16 * 1000 * 1000
                                                  : 64 * 1000 * 1000;

                    obs->xs.destroy();
                    obs->ys.destroy();
                    obs->xs.reserve(capacity);
                    obs->ys.reserve(capacity);
                }
            }
        }
    }
}

static void show_project_parameters(component_editor& ed,
                                    tree_node& /*parent*/,
                                    project_hierarchy_data& data) noexcept
{
    if (data.parent && data.compo && data.ch) {
        auto  id  = enum_cast<model_id>(data.ch->id);
        auto* mdl = data.compo->models.try_to_get(id);

        if (mdl) {
            auto*  value         = data.parent->parameters.get(id);
            model* param         = nullptr;
            bool   is_configured = false;

            if (value) {
                param = ed.mod.parameters.try_to_get(*value);
                if (!param) {
                    data.parent->parameters.erase(id);
                    value = nullptr;
                } else {
                    is_configured = true;
                }
            }

            if (ImGui::Checkbox("Enable##param", &is_configured)) {
                if (is_configured) {
                    if (ed.mod.parameters.can_alloc(1)) {
                        auto& new_param   = ed.mod.parameters.alloc();
                        auto new_param_id = ed.mod.parameters.get_id(new_param);
                        param             = &new_param;
                        copy(*mdl, new_param);
                        data.parent->parameters.set(id, new_param_id);
                    } else {
                        is_configured = false;
                    }
                } else {
                    if (param) {
                        ed.mod.parameters.free(*param);
                    }
                    data.parent->parameters.erase(id);
                }
            }

            if (is_configured && param) {
                dispatch(*param, [&ed](auto& dyn) {
                    show_dynamics_inputs(ed.mod.srcs, dyn);
                });
            }
        }
    }
}

void component_editor::show_project_window() noexcept
{
    auto* parent = mod.tree_nodes.try_to_get(mod.head);
    if (!parent)
        return;

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    static project_hierarchy_data data;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(*this, *parent, data);

        if (data.parent && data.compo && !data.ch) {
            select(mod.tree_nodes.get_id(parent));
            data.clear();
        }
    }

    if (ImGui::CollapsingHeader("Observations", flags))
        show_project_observations(*this, *parent, data);

    if (ImGui::CollapsingHeader("Parameters", flags))
        show_project_parameters(*this, *parent, data);

    if (ImGui::CollapsingHeader("Operation", flags)) {
        if (ImGui::Button("save")) {
            mod.save_project("/tmp/toto.json");
        }
    }

    if (ImGui::CollapsingHeader("Export component", flags)) {
        show_hierarchy_settings(*this, *parent);
    }
}

} // namespace irt