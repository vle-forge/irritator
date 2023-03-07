// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/modeling.hpp"

namespace irt {

void project::set(tree_node_id parent, component_id compo) noexcept
{
    m_parent = parent;
    m_compo  = compo;
    m_ch     = undefined<child_id>();
}

void project::set(tree_node_id parent, component_id compo, child_id ch) noexcept
{
    m_parent = parent;
    m_compo  = compo;
    m_ch     = ch;
}

bool project::equal(tree_node_id parent,
                    component_id compo,
                    child_id     ch) const noexcept
{
    return m_parent == parent && m_compo == compo && m_ch == ch;
}

static void do_clear(modeling& mod, project& wnd) noexcept
{
    wnd.m_parent = undefined<tree_node_id>();
    wnd.m_compo  = undefined<component_id>();
    wnd.m_ch     = undefined<child_id>();

    mod.tree_nodes.clear();
}

void project::clear() noexcept
{
    auto* app = container_of(this, &application::project);

    do_clear(app->mod, *this);
}

static void show_project_hierarchy_child_observable(modeling&  mod,
                                                    tree_node& parent,
                                                    simple_component& /*compo*/,
                                                    child& ch) noexcept
{
    if (ch.type == child_type::model) {
        if (auto* mdl = mod.models.try_to_get(ch.id.mdl_id); mdl) {
            auto* value       = parent.observables.get(ch.id.mdl_id);
            bool  is_observed = false;

            if (value) {
                if (*value == observable_type::none) {
                    parent.observables.erase(ch.id.mdl_id);
                    value = nullptr;
                } else {
                    is_observed = true;
                }
            }

            if (ImGui::Checkbox("Observation##obs", &is_observed)) {
                if (is_observed) {
                    parent.observables.set(ch.id.mdl_id,
                                           observable_type::single);
                } else {
                    parent.observables.erase(ch.id.mdl_id);
                    is_observed = false;
                }
            }
        }
    }
}

static void show_project_hierarchy_child_configuration(
  component_editor& ed,
  tree_node&        parent,
  component&        compo,
  simple_component& s_compo,
  child&            ch) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    auto  id  = ch.id.mdl_id;
    auto* mdl = app->mod.models.try_to_get(id);

    if (mdl) {
        auto*  value         = parent.parameters.get(id);
        model* param         = nullptr;
        bool   is_configured = false;

        if (value) {
            param = app->mod.parameters.try_to_get(*value);
            if (!param) {
                parent.parameters.erase(id);
                value = nullptr;
            } else {
                is_configured = true;
            }
        }

        if (ImGui::Checkbox("Configuration##param", &is_configured)) {
            if (is_configured) {
                if (app->mod.parameters.can_alloc(1)) {
                    auto& new_param    = app->mod.parameters.alloc();
                    auto  new_param_id = app->mod.parameters.get_id(new_param);
                    param              = &new_param;
                    copy(*mdl, new_param);
                    parent.parameters.set(id, new_param_id);
                } else {
                    is_configured = false;
                }
            } else {
                if (param) {
                    app->mod.parameters.free(*param);
                }
                parent.parameters.erase(id);
            }
        }

        if (is_configured && param) {
            dispatch(*param,
                     [&app, &compo, &s_compo, param]<typename Dynamics>(
                       Dynamics& dyn) {
                         if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                             if (auto* machine =
                                   app->mod.hsms.try_to_get(dyn.id);
                                 machine) {
                                 show_dynamics_inputs(
                                   *app,
                                   app->mod.components.get_id(compo),
                                   app->mod.models.get_id(*param),
                                   *machine);
                             }
                         } else
                             show_dynamics_inputs(app->mod.srcs, dyn);
                     });
        }
    }
}

static void show_project_hierarchy(project&           pj_wnd,
                                   component_editor&  ed,
                                   simulation_editor& sim_ed,
                                   tree_node&         parent) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (auto* compo = app->mod.components.try_to_get(parent.id); compo) {
        if (ImGui::TreeNodeEx(&parent, flags, "%s", compo->name.c_str())) {
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                pj_wnd.set(app->mod.tree_nodes.get_id(parent), parent.id);
            }

            if (auto* child = parent.tree.get_child(); child) {
                show_project_hierarchy(pj_wnd, ed, sim_ed, *child);
            }

            if (compo->type == component_type::simple) {
                if (auto* s_compo = app->mod.simple_components.try_to_get(
                      compo->id.simple_id);
                    s_compo) {

                    for (auto child_id : s_compo->children) {
                        auto* pc = app->mod.children.try_to_get(child_id);
                        if (!pc)
                            continue;

                        if (pc->configurable || pc->observable) {
                            ImGui::PushID(pc);

                            const auto parent_id =
                              app->mod.tree_nodes.get_id(parent);
                            const auto compo_id =
                              app->mod.components.get_id(*compo);
                            const bool selected =
                              pj_wnd.equal(parent_id, compo_id, child_id);

                            if (ImGui::Selectable(pc->name.c_str(), selected)) {
                                pj_wnd.set(parent_id, compo_id, child_id);
                            }

                            if (selected) {
                                if (pc->configurable)
                                    show_project_hierarchy_child_configuration(
                                      ed, parent, *compo, *s_compo, *pc);
                                if (pc->observable)
                                    show_project_hierarchy_child_observable(
                                      app->mod, parent, *s_compo, *pc);
                            }

                            ImGui::PopID();
                        }
                    }
                }
            }

            ImGui::TreePop();
        }

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(pj_wnd, ed, sim_ed, *sibling);
    }
}

template<typename T, typename Identifier>
T* find(data_array<T, Identifier>& data,
        vector<Identifier>&        container,
        std::string_view           name) noexcept
{
    int i = 0;
    while (i < container.ssize()) {
        auto  test_id = container[i];
        auto* test    = data.try_to_get(test_id);

        if (test) {
            if (test->path.sv() == name)
                return test;

            ++i;
        } else {
            container.swap_pop_back(i);
        }
    }

    return nullptr;
}

template<typename T, typename Identifier>
bool exist(data_array<T, Identifier>& data,
           vector<Identifier>&        container,
           std::string_view           name) noexcept
{
    return find(data, container, name) != nullptr;
}

static void show_hierarchy_settings(component_editor& ed,
                                    tree_node&        parent) noexcept
{
    auto* app = container_of(&ed, &application::component_ed);

    if (auto* compo = app->mod.components.try_to_get(parent.id); compo) {
        ImGui::InputFilteredString("Name", compo->name);
        static constexpr const char* empty = "";

        auto* reg_dir = app->mod.registred_paths.try_to_get(compo->reg_path);
        const char* reg_preview = reg_dir ? reg_dir->path.c_str() : empty;
        if (ImGui::BeginCombo("Path", reg_preview)) {
            registred_path* list = nullptr;
            while (app->mod.registred_paths.next(list)) {
                if (list->status == registred_path::state::error)
                    continue;

                if (ImGui::Selectable(list->path.c_str(),
                                      reg_dir == list,
                                      ImGuiSelectableFlags_None)) {
                    compo->reg_path = app->mod.registred_paths.get_id(list);
                    reg_dir         = list;
                }
            }
            ImGui::EndCombo();
        }

        if (reg_dir) {
            auto* dir         = app->mod.dir_paths.try_to_get(compo->dir);
            auto* dir_preview = dir ? dir->path.c_str() : empty;

            if (ImGui::BeginCombo("Dir", dir_preview)) {
                if (ImGui::Selectable("##empty-dir", dir == nullptr)) {
                    compo->dir = undefined<dir_path_id>();
                    dir        = nullptr;
                }

                dir_path* list = nullptr;
                while (app->mod.dir_paths.next(list)) {
                    if (ImGui::Selectable(list->path.c_str(), dir == list)) {
                        compo->dir = app->mod.dir_paths.get_id(list);
                        dir        = list;
                    }
                }
                ImGui::EndCombo();
            }

            if (dir == nullptr) {
                small_string<256> dir_name{};

                if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                    if (!exist(app->mod.dir_paths,
                               reg_dir->children,
                               dir_name.sv())) {
                        auto& new_dir = app->mod.dir_paths.alloc();
                        auto  dir_id  = app->mod.dir_paths.get_id(new_dir);
                        auto reg_id = app->mod.registred_paths.get_id(*reg_dir);
                        new_dir.parent = reg_id;
                        new_dir.path   = dir_name;
                        new_dir.status = dir_path::state::unread;
                        reg_dir->children.emplace_back(dir_id);
                        compo->reg_path = reg_id;
                        compo->dir      = dir_id;

                        if (!new_dir.make()) {
                            auto* app =
                              container_of(&ed, &application::component_ed);
                            log_w(*app,
                                  log_level::error,
                                  "Fail to create directory `%.*s'",
                                  new_dir.path.ssize(),
                                  new_dir.path.begin());
                        }
                    }
                }
            }

            if (dir) {
                auto* file = app->mod.file_paths.try_to_get(compo->file);
                if (!file) {
                    auto& f     = app->mod.file_paths.alloc();
                    auto  id    = app->mod.file_paths.get_id(f);
                    f.component = app->mod.components.get_id(*compo);
                    f.parent    = app->mod.dir_paths.get_id(*dir);
                    compo->file = id;
                    dir->children.emplace_back(id);
                    file = &f;
                }

                if (ImGui::InputFilteredString("File##text", file->path)) {
                    if (!exist(app->mod.file_paths,
                               dir->children,
                               file->path.sv())) {
                    }
                }

                auto* desc = app->mod.descriptions.try_to_get(compo->desc);
                if (!desc) {
                    if (app->mod.descriptions.can_alloc(1) &&
                        ImGui::Button("Add description")) {
                        auto& new_desc = app->mod.descriptions.alloc();
                        compo->desc    = app->mod.descriptions.get_id(new_desc);
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
                        app->mod.descriptions.free(*desc);
                        compo->desc = undefined<description_id>();
                    }
                }

                if (file && dir) {
                    if (ImGui::Button("Save")) {
                        auto* app =
                          container_of(&ed, &application::component_ed);
                        auto compo_id = app->mod.components.get_id(*compo);
                        auto compo    = ordinal(compo_id);

                        app->add_simulation_task(task_save_component, compo);
                        app->add_simulation_task(task_save_description, compo);
                    }
                }
            }
        }
    }
}

void project::open_as_main(component_id id) noexcept
{
    auto* app = container_of(this, &application::project);

    if (auto* compo = app->mod.components.try_to_get(id); compo) {
        do_clear(app->mod, *this);

        tree_node_id parent_id;
        if (is_success(app->mod.make_tree_from(*compo, &parent_id))) {
            app->mod.head      = parent_id;
            selected_component = parent_id;
        }
    }
}

void project::select(tree_node_id id) noexcept
{
    auto* app = container_of(this, &application::project);

    if (auto* tree = app->mod.tree_nodes.try_to_get(id); tree)
        if (auto* compo = app->mod.components.try_to_get(tree->id); compo)
            selected_component = id;
}

void project::show() noexcept
{
    auto* app = container_of(this, &application::project);

    auto* parent = app->mod.tree_nodes.try_to_get(app->mod.head);
    if (!parent) {
        clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(
          *this, app->component_ed, app->simulation_ed, *parent);

        if (auto* parent = app->mod.tree_nodes.try_to_get(m_parent); parent) {
            if (auto* compo = app->mod.components.try_to_get(m_compo);
                compo && compo->type == component_type::simple) {
                if (auto* s_compo = app->mod.simple_components.try_to_get(
                      compo->id.simple_id);
                    s_compo) {
                    // if (auto* ch = s_compo->children.try_to_get(m_ch); ch) {
                    // app->component_ed.select(m_parent);
                    // clear();
                    //} @TODO useless ?
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Export component", flags))
        show_hierarchy_settings(app->component_ed, *parent);

    if (auto* compo = app->mod.components.try_to_get(parent->id); compo) {
        if (auto* s_compo =
              app->mod.simple_components.try_to_get(compo->id.simple_id);
            s_compo) {
            ImGui::TextFormat("component: {}", compo->name.sv());
            ImGui::TextFormat("children: {}", s_compo->children.size());
            ImGui::TextFormat("connections: {}", s_compo->connections.size());
        }
    }
}

} // namespace irt
