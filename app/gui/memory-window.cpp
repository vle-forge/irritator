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

    auto& app = container_of(this, &application::memory_wnd);

    if (ImGui::CollapsingHeader("Component usage",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("descriptions: {} / {}",
                          app.mod.descriptions.size(),
                          app.mod.descriptions.capacity());
        ImGui::TextFormat("components: {} / {} / {}",
                          app.mod.components.size(),
                          app.mod.components.max_used(),
                          app.mod.components.capacity());
        ImGui::TextFormat("registred_paths: {} / {} / {}",
                          app.mod.registred_paths.size(),
                          app.mod.registred_paths.max_used(),
                          app.mod.registred_paths.capacity());
        ImGui::TextFormat("dir_paths: {} / {} / {}",
                          app.mod.dir_paths.size(),
                          app.mod.dir_paths.max_used(),
                          app.mod.dir_paths.capacity());
        ImGui::TextFormat("file_paths: {} / {} / {}",
                          app.mod.file_paths.size(),
                          app.mod.file_paths.max_used(),
                          app.mod.file_paths.capacity());
    }

    if (ImGui::CollapsingHeader("Simulation usage",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextFormat("models: {}", app.pj.sim.models.size());
        ImGui::TextFormat("hsms: {}", app.pj.sim.hsms.size());
        ImGui::TextFormat("observers: {}", app.pj.sim.observers.size());

        ImGui::TextFormat("immediate_models: {}",
                          app.pj.sim.immediate_models.size());
        ImGui::TextFormat("immediate_observers: {}",
                          app.pj.sim.immediate_observers.size());

        ImGui::TextFormat("message_alloc: {}", app.pj.sim.messages.size());
        ImGui::TextFormat("node: {}", app.pj.sim.nodes.size());
        ImGui::TextFormat("dated_message_alloc: {}",
                          app.pj.sim.dated_messages.size());
        ImGui::TextFormat("emitting_output_ports: {}",
                          app.pj.sim.emitting_output_ports.size());

        ImGui::TextFormat("contant sources: {}",
                          app.pj.sim.srcs.constant_sources.size());
        ImGui::TextFormat("text sources: {}",
                          app.pj.sim.srcs.text_file_sources.size());
        ImGui::TextFormat("binary sources: {}",
                          app.pj.sim.srcs.binary_file_sources.size());
        ImGui::TextFormat("random sources: {}",
                          app.pj.sim.srcs.random_sources.size());
    }

    if (ImGui::CollapsingHeader("Components")) {
        component* compo = nullptr;
        while (app.mod.components.next(compo)) {
            ImGui::PushID(compo);
            if (ImGui::TreeNode(compo->name.c_str())) {
                if (auto* s_compo = app.mod.generic_components.try_to_get(
                      compo->id.generic_id);
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
            while (app.mod.registred_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat(
                  "{}", ordinal(app.mod.registred_paths.get_id(*dir)));
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
            while (app.mod.dir_paths.next(dir)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat("{}",
                                  ordinal(app.mod.dir_paths.get_id(*dir)));
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
            while (app.mod.file_paths.next(file)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextFormat("{}",
                                  ordinal(app.mod.file_paths.get_id(*file)));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(file->path.c_str());
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // irt
