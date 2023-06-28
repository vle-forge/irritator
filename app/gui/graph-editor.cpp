// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"
#include "irritator/format.hpp"

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace irt {

graph_editor_data::graph_editor_data(
  const component_id       id_,
  const graph_component_id graph_id_) noexcept
  : graph_id(graph_id_)
  , id(id_)
{
}

void graph_editor_data::clear() noexcept
{
    selected.clear();
    scale = 10.f;

    graph_id = undefined<graph_component_id>();
    id       = undefined<component_id>();
}

void graph_editor_data::show(component_editor& ed) noexcept
{
    auto* app   = container_of(&ed, &application::component_ed);
    auto* compo = app->mod.components.try_to_get(id);
    auto* graph = app->mod.graph_components.try_to_get(graph_id);

    irt_assert(compo && graph);

    ImGui::TextFormatDisabled("graph-editor-data size: {}",
                              graph->children.size());
}

graph_editor_dialog::graph_editor_dialog() noexcept
{
    graph.resize(30, undefined<component_id>());
}

void graph_editor_dialog::load(application*       app_,
                               generic_component* compo_) noexcept
{
    app        = app_;
    compo      = compo_;
    is_running = true;
    is_ok      = false;
}

void graph_editor_dialog::save() noexcept
{
    irt_assert(app && compo);

    app->mod.copy(graph, *compo);
}

static bool show_size_widget(graph_component& graph) noexcept
{
    int size = graph.children.ssize();

    if (ImGui::InputInt("size", &size)) {
        size = std::clamp(size, 1, graph_component::children_max);
        if (size != graph.children.ssize()) {
            graph.resize(size, undefined<component_id>());
            return true;
        }
    }
    return false;
}

constexpr inline auto get_default_component_id(const vector<component_id>& g) noexcept
  -> component_id
{
    return g.empty() ? undefined<component_id>() : g.front();
}

static bool show_default_component_widgets(application&     app,
                                           graph_component& graph) noexcept
{
    auto id = get_default_component_id(graph.children);
    if (app.component_sel.combobox("Default component", &id)) {
        std::fill_n(graph.children.data(), graph.children.ssize(), id);
        return true;
    }

    return false;
}

void graph_editor_dialog::show() noexcept
{
    ImGui::OpenPopup(graph_editor_dialog::name);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(graph_editor_dialog::name)) {
        is_ok        = false;
        bool is_show = true;

        const auto item_spacing  = ImGui::GetStyle().ItemSpacing.x;
        const auto region_width  = ImGui::GetContentRegionAvail().x;
        const auto region_height = ImGui::GetContentRegionAvail().y;
        const auto button_size =
          ImVec2{ (region_width - item_spacing) / 2.f, 0 };
        const auto child_size =
          region_height - ImGui::GetFrameHeightWithSpacing();

        ImGui::BeginChild("##dialog", ImVec2(0.f, child_size), true);
        show_size_widget(graph);

        show_default_component_widgets(
          *container_of(this, &application::graph_dlg), graph);
        ImGui::EndChild();

        if (ImGui::Button("Ok", button_size)) {
            is_ok   = true;
            is_show = false;
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            is_show = false;
        }

        if (is_show == false) {
            ImGui::CloseCurrentPopup();
            is_running = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace irt
