// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

void memory_window::show() noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin(memory_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto* app      = container_of(this, &application::memory_wnd);
    auto& c_editor = app->component_ed;
    auto& s_editor = app->simulation_ed;

    if (ImGui::CollapsingHeader("Component usage",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("tree_nodes: {} / {} / {}",
                          c_editor.mod.tree_nodes.size(),
                          c_editor.mod.tree_nodes.max_used(),
                          c_editor.mod.tree_nodes.capacity());
        ImGui::TextFormat("descriptions: {} / {} / {}",
                          c_editor.mod.descriptions.size(),
                          c_editor.mod.descriptions.max_used(),
                          c_editor.mod.descriptions.capacity());
        ImGui::TextFormat("components: {} / {} / {}",
                          c_editor.mod.components.size(),
                          c_editor.mod.components.max_used(),
                          c_editor.mod.components.capacity());
        ImGui::TextFormat("registred_paths: {} / {} / {}",
                          c_editor.mod.registred_paths.size(),
                          c_editor.mod.registred_paths.max_used(),
                          c_editor.mod.registred_paths.capacity());
        ImGui::TextFormat("dir_paths: {} / {} / {}",
                          c_editor.mod.dir_paths.size(),
                          c_editor.mod.dir_paths.max_used(),
                          c_editor.mod.dir_paths.capacity());
        ImGui::TextFormat("file_paths: {} / {} / {}",
                          c_editor.mod.file_paths.size(),
                          c_editor.mod.file_paths.max_used(),
                          c_editor.mod.file_paths.capacity());
    }

    if (ImGui::CollapsingHeader("Simulation usage",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("models: {}", s_editor.sim.models.size());
        ImGui::TextFormat("hsms: {}", s_editor.sim.hsms.size());
        ImGui::TextFormat("observers: {}", s_editor.sim.observers.size());

        ImGui::TextFormat("immediate_models: {}",
                          s_editor.sim.immediate_models.size());
        ImGui::TextFormat("immediate_observers: {}",
                          s_editor.sim.immediate_observers.size());

        ImGui::TextFormat("message_alloc: {}",
                          s_editor.sim.message_alloc.size());
        ImGui::TextFormat("node: {}", s_editor.sim.node_alloc.size());
        ImGui::TextFormat("record: {}", s_editor.sim.record_alloc.size());

        ImGui::TextFormat("dated_message_alloc: {}",
                          s_editor.sim.dated_message_alloc.size());
        ImGui::TextFormat("emitting_output_ports: {}",
                          s_editor.sim.emitting_output_ports.size());

        ImGui::TextFormat("contant sources: {}",
                          s_editor.sim.srcs.constant_sources.size());
        ImGui::TextFormat("text sources: {}",
                          s_editor.sim.srcs.text_file_sources.size());
        ImGui::TextFormat("binary sources: {}",
                          s_editor.sim.srcs.binary_file_sources.size());
        ImGui::TextFormat("random sources: {}",
                          s_editor.sim.srcs.random_sources.size());
    }

    if (ImGui::CollapsingHeader("Components")) {
        component* compo = nullptr;
        while (c_editor.mod.components.next(compo)) {
            ImGui::PushID(compo);
            if (ImGui::TreeNode(compo->name.c_str())) {
                if (auto* s_compo = c_editor.mod.simple_components.try_to_get(
                      compo->id.simple_id);
                    s_compo) {
                    ImGui::TextFormat("children: {}/{}",
                                      s_compo->children.size(),
                                      s_compo->children.capacity());
                    ImGui::TextFormat("connections: {}/{}",
                                      s_compo->connections.size(),
                                      s_compo->connections.capacity());
                    ImGui::Separator();

                    ImGui::TextFormat("Dir: {}", ordinal(compo->dir));
                    ImGui::TextFormat("Description: {}", ordinal(compo->desc));
                    ImGui::TextFormat("File: {}", ordinal(compo->file));

                    ImGui::Separator();

                    int x = 0, y = 0;
                    for (auto connection_id : s_compo->connections) {
                        auto* con =
                          c_editor.mod.connections.try_to_get(connection_id);
                        if (!con)
                            continue;

                        switch (con->type) {
                        case connection::connection_type::input:
                            ++x;
                            break;
                        case connection::connection_type::output:
                            ++y;
                            break;
                        default:
                            break;
                        }
                    }

                    ImGui::TextFormat("X: {}", x);
                    ImGui::TextFormat("Y: {}", y);

                    ImGui::TreePop();
                }
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Registred directoriess")) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            registred_path* dir = nullptr;
            while (c_editor.mod.registred_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat(
                  "{}", ordinal(c_editor.mod.registred_paths.get_id(*dir)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dir->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Directories")) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            dir_path* dir = nullptr;
            while (c_editor.mod.dir_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat("{}",
                                  ordinal(c_editor.mod.dir_paths.get_id(*dir)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dir->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Files")) {
        if (ImGui::BeginTable("Table", 2)) {
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();
            file_path* file = nullptr;
            while (c_editor.mod.file_paths.next(file)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat(
                  "{}", ordinal(c_editor.mod.file_paths.get_id(*file)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(file->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // irt
