// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/io.hpp"
#include "irritator/modeling.hpp"

namespace irt {

void project_window::set(tree_node_id parent, component_id compo) noexcept
{
    m_parent = parent;
    m_compo  = compo;
    m_ch     = undefined<child_id>();
}

void project_window::set(tree_node_id parent,
                         component_id compo,
                         child_id     ch) noexcept
{
    m_parent = parent;
    m_compo  = compo;
    m_ch     = ch;
}

bool project_window::equal(tree_node_id parent,
                           component_id compo,
                           child_id     ch) const noexcept
{
    return m_parent == parent && m_compo == compo && m_ch == ch;
}

static void do_clear(project& pj, project_window& wnd) noexcept
{
    wnd.m_parent = undefined<tree_node_id>();
    wnd.m_compo  = undefined<component_id>();
    wnd.m_ch     = undefined<child_id>();

    project_clear(pj);
}

void project_window::clear() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    do_clear(app->pj, *this);
}

static void show_project_hierarchy_child_observable(
  modeling&  mod,
  tree_node& parent,
  generic_component& /*compo*/,
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
  generic_component& /*s_compo*/,
  child& ch) noexcept
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
            dispatch(
              *param, [&app, &compo, param]<typename Dynamics>(Dynamics& dyn) {
                  if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                      if (auto* machine = app->mod.hsms.try_to_get(dyn.id);
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

static void show_project_hierarchy(project_window&    pj_wnd,
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
                pj_wnd.set(app->pj.tree_nodes.get_id(parent), parent.id);
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
                              app->pj.tree_nodes.get_id(parent);
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

void project_window::open_as_main(component_id id) noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    if (auto* compo = app->mod.components.try_to_get(id); compo) {
        do_clear(app->pj, *this);

        if (auto ret = project_init(app->pj, app->mod, *compo); is_bad(ret)) {
            auto& n = app->notifications.alloc(log_level::error);
            n.title = "Failed to open component as project";
            format(n.message, "Error: {}", status_string(ret));
            app->notifications.enable(n);
        } else {
            selected_component = undefined<tree_node_id>();
        }
    }
}

void project_window::select(tree_node_id id) noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    if (auto* tree = app->pj.tree_nodes.try_to_get(id); tree)
        if (auto* compo = app->mod.components.try_to_get(tree->id); compo)
            selected_component = id;
}

void project_window::show() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    auto* parent = app->pj.tree_nodes.try_to_get(app->pj.tn_head);
    if (!parent) {
        clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(
          *this, app->component_ed, app->simulation_ed, *parent);

        if (auto* parent = app->pj.tree_nodes.try_to_get(m_parent); parent) {
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
}

} // namespace irt
