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
}

} // namespace irt