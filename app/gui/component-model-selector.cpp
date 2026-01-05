// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static void try_append(const application&                             app,
                       const project&                                 pj,
                       const tree_node&                               tn,
                       vector<std::pair<tree_node_id, component_id>>& out,
                       vector<name_str>& names) noexcept
{
    debug::ensure(out.ssize() == names.ssize());

    const auto idx =
      out.find_if([&](const auto& elem) { return elem.second == tn.id; });

    if (not idx.has_value()) {
        if (auto* compo = app.mod.components.try_to_get<component>(tn.id);
            compo) {
            out.emplace_back(std::make_pair(pj.tree_nodes.get_id(tn), tn.id));
            names.emplace_back(compo->name.sv());
        }
    }
}

static bool update_lists(
  const application&                             app,
  const project&                                 pj,
  const tree_node_id                             parent_id,
  vector<tree_node*>&                            stack,
  vector<std::pair<tree_node_id, component_id>>& components,
  vector<name_str>&                              names) noexcept
{
    stack.clear();
    components.clear();
    names.clear();

    if (auto* tn = pj.tree_nodes.try_to_get(parent_id)) {
        if (auto* child = tn->tree.get_child()) {
            stack.emplace_back(child);
            while (!stack.empty()) {
                auto cur = stack.back();
                stack.pop_back();

                try_append(app, pj, *cur, components, names);

                if (auto* sibling = cur->tree.get_sibling(); sibling)
                    stack.emplace_back(sibling);
            }
        }

        return true;
    }

    return false;
}

void component_model_selector::component_comboxbox(
  const char*      label,
  const data_type& data) noexcept
{
    static constexpr const char* empty = "undefined";

    const auto preview =
      component_selected == -1 ? empty : data.names[component_selected].c_str();

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("undefined", component_selected == -1)) {
            component_selected = -1;
            compo_id           = undefined<component_id>();
        }

        for (int i = 0, e = data.names.ssize(); i != e; ++i) {
            if (ImGui::Selectable(data.names[i].c_str(),
                                  i == component_selected)) {
                component_selected = i;
                tn_id              = data.components[component_selected].first;
                compo_id           = data.components[component_selected].second;
            }
        }

        ImGui::EndCombo();
    }
}

void component_model_selector::observable_model_treenode(const project& pj,
                                                         tree_node& tn) noexcept
{
    auto& app = container_of(this, &application::component_model_sel);

    static constexpr const char* compo_fmt[] = {
        "%.*s (none)",  "%.*s (generic)", "%.*s (grid)",
        "%.*s (graph)", "%.*s (hsm)",
    };

    if (auto* compo = app.mod.components.try_to_get<component>(tn.id)) {
        debug::ensure(ordinal(compo->type) < length(compo_fmt));

        const auto* fmt = compo_fmt[ordinal(compo->type)];

        if (compo->type == component_type::generic) {
            ImGui::PushID(&tn);
            if (ImGui::TreeNodeEx(&tn,
                                  ImGuiTreeNodeFlags_DefaultOpen,
                                  fmt,
                                  compo->name.size(),
                                  compo->name.data())) {
                for_each_model(
                  pj.sim,
                  tn,
                  [&](const std::string_view /*uid*/,
                      const auto& mdl) noexcept {
                      const auto current_mdl_id = pj.sim.models.get_id(mdl);
                      ImGui::PushID(get_index(current_mdl_id));

                      const auto current_tn_id = pj.node(tn);
                      if (ImGui::Selectable(
                            dynamics_type_names[ordinal(mdl.type)],
                            tn_id == current_tn_id && mdl_id == current_mdl_id,
                            ImGuiSelectableFlags_DontClosePopups)) {
                          tn_id  = current_tn_id;
                          mdl_id = current_mdl_id;
                      }

                      ImGui::PopID();
                  });

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

void component_model_selector::observable_model_treenode(
  const project&   pj,
  const data_type& data) noexcept
{
    debug::ensure(0 <= component_selected);
    debug::ensure(component_selected < data.names.ssize());
    debug::ensure(is_defined(compo_id));
    debug::ensure(compo_id == data.components[component_selected].second);
    debug::ensure(is_defined(tn_id));
    debug::ensure(tn_id == data.components[component_selected].first);

    small_vector<tree_node*, max_component_stack_size> stack;

    if (auto* tn_grid = pj.tree_nodes.try_to_get(tn_id)) {
        observable_model_treenode(pj, *tn_grid);

        if (auto* top = tn_grid->tree.get_child(); top) {
            stack.emplace_back(top);

            while (!stack.empty()) {
                auto cur = stack.back();
                stack.pop_back();

                observable_model_treenode(pj, *cur);

                if (auto* sibling = cur->tree.get_sibling(); sibling)
                    stack.emplace_back(sibling);

                if (auto* child = cur->tree.get_child(); child)
                    stack.emplace_back(child);
            }
        }
    }
}

std::optional<component_model_selector::access>
component_model_selector::combobox(const char*    label,
                                   const project& pj) noexcept
{
    std::optional<access> ret;

    data.read([&](const auto& data, const auto /*version*/) noexcept {
        debug::ensure(data.components.ssize() == data.names.ssize());
        debug::ensure(component_selected < data.names.ssize());

        component_comboxbox(label, data);
        if (is_defined(compo_id)) {
            observable_model_treenode(pj, data);
            if (is_defined(tn_id) and is_defined(mdl_id))
                ret = component_model_selector::access{ .parent_id = parent_id,
                                                        .compo_id  = compo_id,
                                                        .tn_id     = tn_id,
                                                        .mdl_id    = mdl_id };
        }
    });

    return ret;
}

void component_model_selector::update(const project&     pj,
                                      const tree_node_id parent_id_,
                                      const component_id compo_id_,
                                      const tree_node_id tn_id_,
                                      const model_id     mdl_id_) noexcept
{
    data.write([&](auto& data) noexcept {
        auto& app = container_of(this, &application::component_model_sel);

        component_selected = -1;
        parent_id          = parent_id_;
        compo_id           = compo_id_;
        tn_id              = tn_id_;
        mdl_id             = mdl_id_;

        debug::ensure(pj.tree_nodes.try_to_get(parent_id));

        if (not update_lists(app,
                             pj,
                             parent_id,
                             data.stack_tree_nodes,
                             data.components,
                             data.names)) {
            component_selected = -1;
            parent_id          = undefined<tree_node_id>();
            compo_id           = undefined<component_id>();
            tn_id              = undefined<tree_node_id>();
            mdl_id             = undefined<model_id>();

            app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                title = "Component model selector error";
                msg   = "Fail to update component list";
            });
        } else {
            component_selected = -1;
            for (int i = 0, e = data.components.ssize(); i != e; ++i) {
                if (data.components[i].second == compo_id &&
                    data.components[i].first == tn_id) {
                    component_selected = i;
                    break;
                }
            }
        }
    });
}

} // namespace irt
