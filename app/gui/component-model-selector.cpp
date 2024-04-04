// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/file.hpp"

#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

namespace irt {

static void try_append(const application&                             app,
                       const tree_node&                               tn,
                       vector<std::pair<tree_node_id, component_id>>& out,
                       vector<small_string<254>>& names) noexcept
{
    debug::ensure(out.ssize() == names.ssize());

    if (auto it = std::find_if(
          out.begin(),
          out.end(),
          [&](const auto& elem) noexcept { return elem.second == tn.id; });
        it == out.end()) {
        if (auto* compo = app.mod.components.try_to_get(tn.id); compo) {
            out.emplace_back(
              std::make_pair(app.pj.tree_nodes.get_id(tn), tn.id));
            names.emplace_back(compo->name.sv());
        }
    }
}

static void build_component_list(
  const application&                             app,
  const tree_node&                               tn,
  vector<tree_node*>&                            cache,
  vector<std::pair<tree_node_id, component_id>>& out,
  vector<small_string<254>>&                     names) noexcept
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

        try_append(app, *cur, out, names);

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            cache.emplace_back(sibling);
    }
}

void component_model_selector::select(const tree_node_id parent_id) noexcept
{

    if (parent_id != current_tree_node) {
        auto& app = container_of(this, &application::component_model_sel);

        components.clear();
        names.clear();
        stack_tree_nodes.clear();
        component_selected = -1;
        current_tree_node  = parent_id;

        if (auto* tn = app.pj.tree_nodes.try_to_get(parent_id); tn) {
            build_component_list(app, *tn, stack_tree_nodes, components, names);
        } else {
            current_tree_node = undefined<tree_node_id>();
        }
    }
}

void component_model_selector::select(const tree_node_id parent_id,
                                      const component_id compo_id,
                                      const tree_node_id tn_id,
                                      const model_id /*mdl_id*/) noexcept

{
    select(parent_id);

    component_selected = -1;
    for (int i = 0, e = components.ssize(); i != e; ++i) {
        if (components[i].second == compo_id && components[i].first == tn_id) {
            component_selected = i;
            break;
        }
    }
}

bool component_model_selector::component_comboxbox(const char*   label,
                                                   component_id& compo_id,
                                                   tree_node_id& tn_id) noexcept
{
    static constexpr const char* empty = "undefined";

    const auto preview =
      component_selected == -1 ? empty : names[component_selected].c_str();
    auto ret = false;

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("undefined", component_selected == -1)) {
            component_selected = -1;
            compo_id           = undefined<component_id>();
            ret                = true;
        }

        for (int i = 0, e = names.ssize(); i != e; ++i) {
            if (ImGui::Selectable(names[i].c_str(), i == component_selected)) {
                component_selected = i;
                tn_id              = components[component_selected].first;
                compo_id           = components[component_selected].second;
                ret                = true;
            }
        }

        ImGui::EndCombo();
    }

    return ret;
}

bool component_model_selector::observable_model_treenode(
  tree_node& tn,
  component_id& /*compo_id*/,
  tree_node_id& tn_id,
  model_id&     mdl_id) noexcept
{
    auto& app = container_of(this, &application::component_model_sel);
    bool  ret = false;

    if_data_exists_do(app.mod.components, tn.id, [&](auto& compo) noexcept {
        small_string<64> str;

        switch (compo.type) {
        case component_type::simple:
            format(str, "{} generic", compo.name.sv());
            break;
        case component_type::grid:
            format(str, "{} grid", compo.name.sv());
            break;
        case component_type::graph:
            format(str, "{} graph", compo.name.sv());
            break;
        default:
            format(str, "{} unknown", compo.name.sv());
            break;
        }

        if (compo.type == component_type::simple) {
            ImGui::PushID(&tn);
            if (ImGui::TreeNodeEx(str.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
                for_each_model(
                  app.sim, tn, [&](u64 /*uid*/, auto& mdl) noexcept {
                      const auto current_mdl_id = app.sim.models.get_id(mdl);
                      ImGui::PushID(get_index(current_mdl_id));

                      const auto current_tn_id = app.pj.node(tn);
                      str = dynamics_type_names[ordinal(mdl.type)];
                      if (ImGui::Selectable(
                            str.c_str(),
                            tn_id == current_tn_id && mdl_id == current_mdl_id,
                            ImGuiSelectableFlags_DontClosePopups)) {
                          tn_id  = current_tn_id;
                          mdl_id = current_mdl_id;
                          ret    = true;
                      }

                      ImGui::PopID();
                  });

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    });

    return ret;
}

bool component_model_selector::observable_model_treenode(
  component_id& compo_id,
  tree_node_id& tn_id,
  model_id&     mdl_id) noexcept
{
    debug::ensure(0 <= component_selected);
    debug::ensure(component_selected < names.ssize());
    debug::ensure(is_defined(compo_id));
    debug::ensure(compo_id == components[component_selected].second);
    debug::ensure(is_defined(tn_id));
    debug::ensure(tn_id == components[component_selected].first);

    stack_tree_nodes.clear();

    auto& app = container_of(this, &application::component_model_sel);
    bool  ret = false;

    if_data_exists_do(app.pj.tree_nodes, tn_id, [&](auto& tn_grid) noexcept {
        auto selected =
          observable_model_treenode(tn_grid, compo_id, tn_id, mdl_id);
        if (!ret)
            ret = selected;

        if (auto* top = tn_grid.tree.get_child(); top) {
            stack_tree_nodes.emplace_back(top);

            while (!stack_tree_nodes.empty()) {
                auto cur = stack_tree_nodes.back();
                stack_tree_nodes.pop_back();

                auto selected =
                  observable_model_treenode(*cur, compo_id, tn_id, mdl_id);
                if (!ret)
                    ret = selected;

                if (auto* sibling = cur->tree.get_sibling(); sibling)
                    stack_tree_nodes.emplace_back(sibling);

                if (auto* child = cur->tree.get_child(); child)
                    stack_tree_nodes.emplace_back(child);
            }
        }
    });

    return ret;
}

bool component_model_selector::combobox(const char*   label,
                                        tree_node_id& parent_id,
                                        component_id& compo_id,
                                        tree_node_id& tn_id,
                                        model_id&     mdl_id) noexcept
{
    debug::ensure(components.ssize() == names.ssize());
    debug::ensure(component_selected < names.ssize());
    debug::ensure(parent_id == current_tree_node);

    bool ret = component_comboxbox(label, compo_id, tn_id);

    if (is_defined(compo_id))
        if (observable_model_treenode(compo_id, tn_id, mdl_id))
            ret = true;

    return ret;
}

} // namespace irt
