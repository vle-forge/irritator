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

static void show_project_hierarchy_child_observable(
  component_editor&       ed,
  tree_node&              parent,
  project_hierarchy_data& data) noexcept
{
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

        if (ImGui::Checkbox("Observation##obs", &is_observed)) {
            if (is_observed) {
                if (ed.outputs.can_alloc(1)) {
                    auto& new_obs    = ed.outputs.alloc();
                    auto  new_obs_id = ed.outputs.get_id(new_obs);
                    obs              = &new_obs;
                    new_obs.name     = data.ch->name.sv();
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
            ImGui::InputFilteredString("name##obs", obs->name);
            if (ImGui::InputReal("time-step##obs", &obs->time_step)) {
                if (obs->time_step <= zero)
                    obs->time_step = one / to_real(100);
            }

            ImGui::Checkbox("interpolate##obs", &obs->interpolate);

            if (obs->xs.capacity() == 0) {
                obs->xs.reserve(1000);
                obs->ys.reserve(1000);
            }

            int old_current = obs->xs.capacity() <= 1000    ? 0
                              : obs->xs.capacity() <= 10000 ? 1
                                                            : 2;
            int current     = old_current;

            ImGui::TextUnformatted("number");
            ImGui::RadioButton("1,000", &current, 0);
            ImGui::SameLine();
            ImGui::RadioButton("10,000", &current, 1);
            ImGui::SameLine();
            ImGui::RadioButton("100,000", &current, 2);

            if (current != old_current) {
                i32 capacity = current == 0   ? 1000
                               : current == 1 ? 10000
                                              : 100000;

                obs->xs.destroy();
                obs->ys.destroy();
                obs->xs.reserve(capacity);
                obs->ys.reserve(capacity);
            }
        }
    }
}

static i32 find_id(const small_vector<port, 8>& vec, const child_id id) noexcept
{
    for (i32 i = 0, e = vec.ssize(); i != e; ++i)
        if (vec[i].id == id)
            return i;

    return -1;
}

static void show_project_hierarchy_child_configuration(
  component_editor&       ed,
  tree_node&              parent,
  project_hierarchy_data& data) noexcept
{
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

        const bool is_integrator = match(mdl->type,
                                         dynamics_type::qss1_integrator,
                                         dynamics_type::qss2_integrator,
                                         dynamics_type::qss3_integrator,
                                         dynamics_type::integrator);

        if (is_integrator) {
            if (ImGui::Checkbox("Input##param", &data.ch->in)) {
                const auto elem =
                  find_id(data.compo->x, data.compo->children.get_id(*data.ch));
                if (data.ch->in) {
                    if (elem < 0)
                        data.compo->x.emplace_back(
                          data.compo->children.get_id(*data.ch), 1);
                } else {
                    if (elem >= 0)
                        data.compo->x.swap_pop_back(elem);
                }
            }

            if (ImGui::Checkbox("Output##param", &data.ch->out)) {
                const auto elem =
                  find_id(data.compo->y, data.compo->children.get_id(*data.ch));
                if (data.ch->out) {
                    if (elem < 0)
                        data.compo->y.emplace_back(
                          data.compo->children.get_id(*data.ch), 0);
                } else {
                    if (elem >= 0)
                        data.compo->y.swap_pop_back(elem);
                }
            }
        }

        if (ImGui::Checkbox("Configuration##param", &is_configured)) {
            if (is_configured) {
                if (ed.mod.parameters.can_alloc(1)) {
                    auto& new_param    = ed.mod.parameters.alloc();
                    auto  new_param_id = ed.mod.parameters.get_id(new_param);
                    param              = &new_param;
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

                        if (selected) {
                            if (pc->configurable)
                                show_project_hierarchy_child_configuration(
                                  ed, parent, data);
                            if (pc->observable)
                                show_project_hierarchy_child_observable(
                                  ed, parent, data);
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
        ImGui::InputFilteredString("Name", compo->name);
        static constexpr const char* empty = "";

        auto* reg_dir = ed.mod.registred_paths.try_to_get(compo->reg_path);
        const char* reg_preview = reg_dir ? reg_dir->path.c_str() : empty;
        if (ImGui::BeginCombo("Path", reg_preview)) {
            registred_path* list = nullptr;
            while (ed.mod.registred_paths.next(list)) {
                if (ImGui::Selectable(list->path.c_str(),
                                      reg_dir == list,
                                      ImGuiSelectableFlags_None)) {
                    compo->reg_path = ed.mod.registred_paths.get_id(list);
                    reg_dir         = list;
                }
            }
            ImGui::EndCombo();
        }

        if (reg_dir) {
            auto* dir         = ed.mod.dir_paths.try_to_get(compo->dir);
            auto* dir_preview = dir ? dir->path.c_str() : empty;

            if (ImGui::BeginCombo("Dir", dir_preview)) {
                if (ImGui::Selectable("##empty-dir", dir == nullptr)) {
                    compo->dir = undefined<dir_path_id>();
                    dir        = nullptr;
                }

                dir_path* list = nullptr;
                while (ed.mod.dir_paths.next(list)) {
                    if (ImGui::Selectable(list->path.c_str(), dir == list)) {
                        compo->dir = ed.mod.dir_paths.get_id(list);
                        dir        = list;
                    }
                }
                ImGui::EndCombo();
            }

            if (dir == nullptr) {
                static small_string<256> dir_name{};

                if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                    auto& new_dir  = ed.mod.dir_paths.alloc();
                    auto  dir_id   = ed.mod.dir_paths.get_id(new_dir);
                    auto  reg_id   = ed.mod.registred_paths.get_id(*reg_dir);
                    new_dir.parent = reg_id;
                    new_dir.path   = dir_name;
                    dir_name.clear();
                    new_dir.status = dir_path::status_option::unread;
                    reg_dir->children.emplace_back(dir_id);

                    compo->reg_path = reg_id;
                    compo->dir      = dir_id;
                }
            }

            if (dir) {
                static small_string<256> file_name{};
                auto* file = ed.mod.file_paths.try_to_get(compo->file);
                if (!file) {
                    if (ImGui::InputFilteredString("File##text", file_name)) {
                        auto& f     = ed.mod.file_paths.alloc();
                        auto  id    = ed.mod.file_paths.get_id(f);
                        f.component = ed.mod.components.get_id(*compo);
                        f.parent    = ed.mod.dir_paths.get_id(*dir);
                        f.path      = file_name;
                        compo->file = id;
                        dir->children.emplace_back(id);
                        file = &f;
                    }
                } else {
                    ImGui::InputFilteredString("File##text", file->path);
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
                            task.app =
                              container_of(&ed, &application::c_editor);
                            task.param_1 =
                              ordinal(ed.mod.components.get_id(*compo));
                            ed.task_mgr.task_lists[0].add(save_component,
                                                          &task);
                        }

                        {
                            auto& task = ed.gui_tasks.alloc();
                            task.app =
                              container_of(&ed, &application::c_editor);
                            task.param_1 =
                              ordinal(ed.mod.components.get_id(*compo));
                            ed.task_mgr.task_lists[0].add(save_description,
                                                          &task);
                        }

                        ed.task_mgr.task_lists[0].submit();
                    }
                }
            }
        }
    }
}

void component_editor::show_project_window() noexcept
{
    static project_hierarchy_data data;

    auto* parent = mod.tree_nodes.try_to_get(mod.head);
    if (!parent) {
        data.clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(*this, *parent, data);

        if (data.parent && data.compo && !data.ch) {
            select(mod.tree_nodes.get_id(parent));
            data.clear();
        }
    }

    if (ImGui::CollapsingHeader("Export component", flags))
        show_hierarchy_settings(*this, *parent);
}

} // namespace irt