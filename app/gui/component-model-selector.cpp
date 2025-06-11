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

    if (auto it = std::find_if(
          out.begin(),
          out.end(),
          [&](const auto& elem) noexcept { return elem.second == tn.id; });
        it == out.end()) {
        if (auto* compo = app.mod.components.try_to_get<component>(tn.id);
            compo) {
            out.emplace_back(std::make_pair(pj.tree_nodes.get_id(tn), tn.id));
            names.emplace_back(compo->name.sv());
        }
    }
}

static void build_component_list(
  const application&                             app,
  const project&                                 pj,
  const tree_node&                               tn,
  vector<tree_node*>&                            cache,
  vector<std::pair<tree_node_id, component_id>>& out,
  vector<name_str>&                              names) noexcept
{
    debug::ensure(cache.empty());
    debug::ensure(out.empty());

    auto* child = tn.tree.get_child();
    if (!child)
        return;

    cache.emplace_back(child);
    while (!cache.empty()) {
        auto cur = cache.back();
        cache.pop_back();

        try_append(app, pj, *cur, out, names);

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.emplace_back(sibling);
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
        build_component_list(app, pj, *tn, stack, components, names);
        return true;
    } else {
        return false;
    }
}

void component_model_selector::component_comboxbox(const char* label) noexcept
{
    static constexpr const char* empty = "undefined";

    const auto preview =
      component_selected == -1 ? empty : names[component_selected].c_str();

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("undefined", component_selected == -1)) {
            component_selected = -1;
            compo_id           = undefined<component_id>();
        }

        for (int i = 0, e = names.ssize(); i != e; ++i) {
            if (ImGui::Selectable(names[i].c_str(), i == component_selected)) {
                component_selected = i;
                tn_id              = components[component_selected].first;
                compo_id           = components[component_selected].second;
            }
        }

        ImGui::EndCombo();
    }
}

void component_model_selector::observable_model_treenode(const project& pj,
                                                         tree_node& tn) noexcept
{
    auto& app = container_of(this, &application::component_model_sel);

    if (auto* compo = app.mod.components.try_to_get<component>(tn.id)) {
        small_string<64> str;

        switch (compo->type) {
        case component_type::generic:
            format(str, "{} generic", compo->name.sv());
            break;
        case component_type::grid:
            format(str, "{} grid", compo->name.sv());
            break;
        case component_type::graph:
            format(str, "{} graph", compo->name.sv());
            break;
        default:
            format(str, "{} unknown", compo->name.sv());
            break;
        }

        if (compo->type == component_type::generic) {
            ImGui::PushID(&tn);
            if (ImGui::TreeNodeEx(str.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
                for_each_model(
                  pj.sim,
                  tn,
                  [&](const std::string_view /*uid*/,
                      const auto& mdl) noexcept {
                      const auto current_mdl_id = pj.sim.models.get_id(mdl);
                      ImGui::PushID(get_index(current_mdl_id));

                      const auto current_tn_id = pj.node(tn);
                      str = dynamics_type_names[ordinal(mdl.type)];
                      if (ImGui::Selectable(
                            str.c_str(),
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
  const project& pj) noexcept
{
    debug::ensure(0 <= component_selected);
    debug::ensure(component_selected < names.ssize());
    debug::ensure(is_defined(compo_id));
    debug::ensure(compo_id == components[component_selected].second);
    debug::ensure(is_defined(tn_id));
    debug::ensure(tn_id == components[component_selected].first);

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        small_vector<tree_node*, max_component_stack_size> stack_tree_nodes;

        if (auto* tn_grid = pj.tree_nodes.try_to_get(tn_id)) {
            observable_model_treenode(pj, *tn_grid);

            if (auto* top = tn_grid->tree.get_child(); top) {
                stack_tree_nodes.emplace_back(top);

                while (!stack_tree_nodes.empty()) {
                    auto cur = stack_tree_nodes.back();
                    stack_tree_nodes.pop_back();

                    observable_model_treenode(pj, *cur);

                    if (auto* sibling = cur->tree.get_sibling(); sibling)
                        stack_tree_nodes.emplace_back(sibling);

                    if (auto* child = cur->tree.get_child(); child)
                        stack_tree_nodes.emplace_back(child);
                }
            }
        }
    }
}

std::optional<component_model_selector::access>
component_model_selector::combobox(const char*    label,
                                   const project& pj) noexcept
{
    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        debug::ensure(components.ssize() == names.ssize());
        debug::ensure(component_selected < names.ssize());

        component_comboxbox(label);
        if (is_defined(compo_id)) {
            observable_model_treenode(pj);
            if (is_defined(tn_id) and is_defined(mdl_id))
                return component_model_selector::access{ .parent_id = parent_id,
                                                         .compo_id  = compo_id,
                                                         .tn_id     = tn_id,
                                                         .mdl_id    = mdl_id };
        }
    }

    return std::nullopt;
}

void component_model_selector::update(const project&     pj,
                                      const tree_node_id parent_id_,
                                      const component_id compo_id_,
                                      const tree_node_id tn_id_,
                                      const model_id     mdl_id_) noexcept
{
    scoped_flag_run(updating, [&]() {
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
                             stack_tree_nodes,
                             components_2nd,
                             names_2nd)) {
            clear_selection();
            app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
                title = "Component model selector error";
                msg   = "Fail to update component list";
            });
        } else {
            component_selected = -1;
            for (int i = 0, e = components.ssize(); i != e; ++i) {
                if (components_2nd[i].second == compo_id &&
                    components_2nd[i].first == tn_id) {
                    component_selected = i;
                    break;
                }
            }

            std::unique_lock lock{ m_mutex };
            swap_buffers();
        }
    });
}

void component_model_selector::swap_buffers() noexcept
{
    std::swap(components, components_2nd);
    std::swap(names, names_2nd);
}

void component_model_selector::clear_selection() noexcept
{
    component_selected = -1;
    parent_id          = undefined<tree_node_id>();
    compo_id           = undefined<component_id>();
    tn_id              = undefined<tree_node_id>();
    mdl_id             = undefined<model_id>();
}

} // namespace irt
