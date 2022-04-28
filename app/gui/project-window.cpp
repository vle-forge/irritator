// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

void project_hierarchy_selection::set(tree_node_id parent,
                                      component_id compo) noexcept
{
    this->parent = parent;
    this->compo  = compo;
    ch           = undefined<child_id>();
}

void project_hierarchy_selection::set(tree_node_id parent,
                                      component_id compo,
                                      child_id     ch) noexcept
{
    this->parent = parent;
    this->compo  = compo;
    this->ch     = ch;
}

bool project_hierarchy_selection::equal(tree_node_id parent,
                                        component_id compo,
                                        child_id     ch) const noexcept
{
    return this->parent == parent && this->compo == compo && this->ch == ch;
}

void project_hierarchy_selection::clear() noexcept
{
    parent = undefined<tree_node_id>();
    compo  = undefined<component_id>();
    ch     = undefined<child_id>();
}

static void show_project_hierarchy_child_observable(simulation_editor& sim_ed,
                                                    tree_node&         parent,
                                                    component&         compo,
                                                    child& ch) noexcept
{
    auto  id  = enum_cast<model_id>(ch.id);
    auto* mdl = compo.models.try_to_get(id);

    if (mdl) {
        auto*                   value       = parent.observables.get(id);
        simulation_observation* obs         = nullptr;
        bool                    is_observed = false;

        if (value) {
            auto output_id = enum_cast<simulation_observation_id>(value->id);
            obs            = sim_ed.sim_obs.try_to_get(output_id);
            if (!obs) {
                parent.observables.erase(id);
                value = nullptr;
            } else {
                is_observed = true;
            }
        }

        if (ImGui::Checkbox("Observation##obs", &is_observed)) {
            if (is_observed) {
                if (sim_ed.sim_obs.can_alloc(1)) {
                    auto& new_obs    = sim_ed.sim_obs.alloc(id, mdl->type, 4096, 4096*4096);
                    auto  new_obs_id = sim_ed.sim_obs.get_id(new_obs);
                    obs              = &new_obs;
                    new_obs.name     = ch.name.sv();

                    parent.observables.set(
                      id,
                      observable{ ordinal(new_obs_id),
                                  observable_type_single });
                } else {
                    is_observed = false;
                }
            } else {
                if (obs)
                    sim_ed.sim_obs.free(*obs);

                parent.observables.erase(id);
            }
        }

        if (is_observed && obs) {
            ImGui::InputFilteredString("name##obs", obs->name);

            if (ImGui::InputReal("window-length##obs", &obs->window)) {
                if (obs->window <= zero)
                    obs->window = one / to_real(100);
            }

            if (ImGui::InputReal("time-step##obs", &obs->time_step)) {
                if (obs->time_step <= zero)
                    obs->time_step = one / to_real(100);
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

static void show_project_hierarchy_child_configuration(component_editor& ed,
                                                       tree_node&        parent,
                                                       component&        compo,
                                                       child& ch) noexcept
{
    auto  id  = enum_cast<model_id>(ch.id);
    auto* mdl = compo.models.try_to_get(id);

    if (mdl) {
        auto*  value         = parent.parameters.get(id);
        model* param         = nullptr;
        bool   is_configured = false;

        if (value) {
            param = ed.mod.parameters.try_to_get(*value);
            if (!param) {
                parent.parameters.erase(id);
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
            if (ImGui::Checkbox("Input##param", &ch.in)) {
                const auto elem = find_id(compo.x, compo.children.get_id(ch));
                if (ch.in) {
                    if (elem < 0)
                        compo.x.emplace_back(compo.children.get_id(ch),
                                             static_cast<i8>(1));
                } else {
                    if (elem >= 0)
                        compo.x.swap_pop_back(elem);
                }
            }

            if (ImGui::Checkbox("Output##param", &ch.out)) {
                const auto elem = find_id(compo.y, compo.children.get_id(ch));
                if (ch.out) {
                    if (elem < 0)
                        compo.y.emplace_back(compo.children.get_id(ch),
                                             static_cast<i8>(0));
                } else {
                    if (elem >= 0)
                        compo.y.swap_pop_back(elem);
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
                    parent.parameters.set(id, new_param_id);
                } else {
                    is_configured = false;
                }
            } else {
                if (param) {
                    ed.mod.parameters.free(*param);
                }
                parent.parameters.erase(id);
            }
        }

        if (is_configured && param) {
            dispatch(*param, [&ed](auto& dyn) {
                show_dynamics_inputs(ed.mod.srcs, dyn);
            });
        }
    }
}

static void show_project_hierarchy(component_editor&            ed,
                                   simulation_editor&           sim_ed,
                                   tree_node&                   parent,
                                   project_hierarchy_selection& data) noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (auto* compo = ed.mod.components.try_to_get(parent.id); compo) {
        if (ImGui::TreeNodeEx(&parent, flags, "%s", compo->name.c_str())) {
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                data.set(ed.mod.tree_nodes.get_id(parent), parent.id);
            }

            if (auto* child = parent.tree.get_child(); child) {
                show_project_hierarchy(ed, sim_ed, *child, data);
            }

            {
                child* pc = nullptr;
                while (compo->children.next(pc)) {
                    if (pc->configurable || pc->observable) {
                        ImGui::PushID(pc);

                        const auto parent_id = ed.mod.tree_nodes.get_id(parent);
                        const auto compo_id  = ed.mod.components.get_id(*compo);
                        const auto ch_id     = compo->children.get_id(*pc);
                        const bool selected =
                          data.equal(parent_id, compo_id, ch_id);

                        if (ImGui::Selectable(pc->name.c_str(), selected)) {
                            data.set(parent_id, compo_id, ch_id);
                        }

                        if (selected) {
                            if (pc->configurable)
                                show_project_hierarchy_child_configuration(
                                  ed, parent, *compo, *pc);
                            if (pc->observable)
                                show_project_hierarchy_child_observable(
                                  sim_ed, parent, *compo, *pc);
                        }

                        ImGui::PopID();
                    }
                }
            }

            ImGui::TreePop();
        }

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(ed, sim_ed, *sibling, data);
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
                small_string<256> dir_name{};

                if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                    auto& new_dir  = ed.mod.dir_paths.alloc();
                    auto  dir_id   = ed.mod.dir_paths.get_id(new_dir);
                    auto  reg_id   = ed.mod.registred_paths.get_id(*reg_dir);
                    new_dir.parent = reg_id;
                    new_dir.path   = dir_name;
                    new_dir.status = dir_path::status_option::unread;
                    reg_dir->children.emplace_back(dir_id);

                    compo->reg_path = reg_id;
                    compo->dir      = dir_id;
                }
            }

            if (dir) {
                small_string<256> file_name{};

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
                        auto* app = container_of(&ed, &application::c_editor);

                        {
                            auto& task = app->gui_tasks.alloc();
                            task.app   = app;
                            task.param_1 =
                              ordinal(ed.mod.components.get_id(*compo));
                            app->task_mgr.task_lists[0].add(save_component,
                                                            &task);
                        }

                        {
                            auto& task = app->gui_tasks.alloc();
                            task.app   = app;
                            task.param_1 =
                              ordinal(ed.mod.components.get_id(*compo));
                            app->task_mgr.task_lists[0].add(save_description,
                                                            &task);
                        }

                        app->task_mgr.task_lists[0].submit();
                    }
                }
            }
        }
    }
}

void application::show_project_window() noexcept
{
    auto* parent = c_editor.mod.tree_nodes.try_to_get(c_editor.mod.head);
    if (!parent) {
        project_selection.clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(c_editor, s_editor, *parent, project_selection);

        if (auto* parent =
              c_editor.mod.tree_nodes.try_to_get(project_selection.parent);
            parent) {
            if (auto* compo =
                  c_editor.mod.components.try_to_get(project_selection.compo);
                compo) {
                if (auto* ch = compo->children.try_to_get(project_selection.ch);
                    !ch) {
                    c_editor.select(project_selection.parent);
                    project_selection.clear();
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Export component", flags))
        show_hierarchy_settings(c_editor, *parent);
}

} // namespace irt
