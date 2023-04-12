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

static void show_project_hierarchy_child_observable(modeling&  mod,
                                                    tree_node& parent,
                                                    child&     ch) noexcept
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

static void show_project_hierarchy_child_configuration(application& app,
                                                       tree_node&   parent,
                                                       component&   compo,
                                                       child&       ch) noexcept
{
    auto  id  = ch.id.mdl_id;
    auto* mdl = app.mod.models.try_to_get(id);

    if (mdl) {
        auto*  value         = parent.parameters.get(id);
        model* param         = nullptr;
        bool   is_configured = false;

        if (value) {
            param = app.mod.parameters.try_to_get(*value);
            if (!param) {
                parent.parameters.erase(id);
                value = nullptr;
            } else {
                is_configured = true;
            }
        }

        if (ImGui::Checkbox("Configuration##param", &is_configured)) {
            if (is_configured) {
                if (app.mod.parameters.can_alloc(1)) {
                    auto& new_param    = app.mod.parameters.alloc();
                    auto  new_param_id = app.mod.parameters.get_id(new_param);
                    param              = &new_param;
                    copy(*mdl, new_param);
                    parent.parameters.set(id, new_param_id);
                } else {
                    is_configured = false;
                }
            } else {
                if (param) {
                    app.mod.parameters.free(*param);
                }
                parent.parameters.erase(id);
            }
        }

        if (is_configured && param) {
            dispatch(
              *param, [&app, &compo, param]<typename Dynamics>(Dynamics& dyn) {
                  if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
                      if (auto* machine = app.mod.hsms.try_to_get(dyn.id);
                          machine) {
                          show_dynamics_inputs(app,
                                               app.mod.components.get_id(compo),
                                               app.mod.models.get_id(*param),
                                               *machine);
                      }
                  } else
                      show_dynamics_inputs(app.mod.srcs, dyn);
              });
        }
    }
}

static void show_project_hierarchy(application&       app,
                                   tree_node&         parent,
                                   component&         compo,
                                   generic_component& generic) noexcept
{
    for (auto child_id : generic.children) {
        if (auto* c = app.mod.children.try_to_get(child_id); c) {
            if (c->configurable || c->observable) {
                ImGui::PushID(c);

                bool selected = app.project_wnd.is_selected(child_id);

                if (ImGui::Selectable(c->name.c_str(), &selected))
                    app.project_wnd.select(child_id);

                if (selected) {
                    if (c->configurable)
                        show_project_hierarchy_child_configuration(
                          app, parent, compo, *c);
                    if (c->observable)
                        show_project_hierarchy_child_observable(
                          app.mod, parent, *c);
                }

                ImGui::PopID();
            }
        }
    }
}

static void show_project_hierarchy(application& app, tree_node& parent) noexcept
{
    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (auto* compo = app.mod.components.try_to_get(parent.id); compo) {
        if (ImGui::TreeNodeEx(&parent, flags, "%s", compo->name.c_str())) {
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                app.project_wnd.select(parent);
            }

            if (auto* child = parent.tree.get_child(); child) {
                show_project_hierarchy(app, *child);
            }

            switch (compo->type) {
            case component_type::simple: {
                if (auto* g =
                      app.mod.simple_components.try_to_get(compo->id.simple_id);
                    g) {
                    show_project_hierarchy(app, parent, *compo, *g);
                }
            } break;

            default:
                break;
            }

            ImGui::TreePop();
        }

        if (auto* sibling = parent.tree.get_sibling(); sibling)
            show_project_hierarchy(app, *sibling);
    }
}

void project_window::clear() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    app->pj.clear();
}

bool project_window::is_selected(tree_node_id id) const noexcept
{
    return selected_component == id;
}

bool project_window::is_selected(child_id id) const noexcept
{
    return selected_child == id;
}

void project_window::select(tree_node_id id) noexcept
{
    if (id != selected_component) {
        auto* app = container_of(this, &application::project_wnd);

        if (auto* tree = app->pj.node(id); tree) {
            if (auto* compo = app->mod.components.try_to_get(tree->id); compo) {
                selected_component = id;
                selected_child     = undefined<child_id>();
            }
        }
    }
}

void project_window::select(tree_node& node) noexcept
{
    auto* app = container_of(this, &application::project_wnd);
    auto  id  = app->pj.node(node);

    if (id != selected_component) {
        if (auto* compo = app->mod.components.try_to_get(node.id); compo) {
            selected_component = id;
            selected_child     = undefined<child_id>();
        }
    }
}

void project_window::select(child_id id) noexcept
{
    if (id != selected_child)
        selected_child = id;
}

void project_window::show() noexcept
{
    auto* app = container_of(this, &application::project_wnd);

    auto* parent = app->pj.tn_head();
    if (!parent) {
        clear();
        return;
    }

    constexpr ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen;

    if (ImGui::CollapsingHeader("Hierarchy", flags)) {
        show_project_hierarchy(*app, *parent);
    }
}

} // namespace irt
