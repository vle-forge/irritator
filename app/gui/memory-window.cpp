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
        ImGui::TextFormat("components: {} / {}",
                          app.mod.components.size(),
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

    if (ImGui::CollapsingHeader("Graph")) {
        for (const auto& g : app.mod.graphs) {
            const auto id = app.mod.graphs.get_id(g);

            if (ImGui::TreeNode(&g, "%u", ordinal(id))) {
                ImGui::LabelFormat("main-id", "{}", g.main_id);
                ImGui::LabelFormat("nodes", "{}", g.nodes.size());
                ImGui::LabelFormat("edges", "{}", g.edges.size());
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("HSM")) {
        for (const auto& g : app.mod.hsms) {
            const auto id = app.mod.hsms.get_id(g);

            if (ImGui::TreeNode(&g, "%u", ordinal(id))) {
                ImGui::LabelFormat(
                  "max state", "{}", g.compute_max_state_used());
                ImGui::LabelFormat("edges", "{}", g.top_state);
                ImGui::LabelFormat("using source", "{}", g.is_using_source());
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& p : app.pjs) {
            if (p.disable_access)
                continue;

            const auto id  = app.pjs.get_id(p);
            const auto idx = get_index(id);

            ImGui::PushID(idx);
            if (ImGui::TreeNode(p.pj.name.c_str())) {
                ImGui::LabelFormat("models", "{}", p.pj.sim.models.size());
                ImGui::LabelFormat("hsms", "{}", p.pj.sim.hsms.size());
                ImGui::LabelFormat(
                  "observers", "{}", p.pj.sim.observers.size());
                ImGui::LabelFormat(
                  "immediate models", "{}", p.pj.sim.immediate_models.size());
                ImGui::LabelFormat("immediate observers",
                                   "{}",
                                   p.pj.sim.immediate_observers.size());
                ImGui::LabelFormat(
                  "message buffer alloc", "{}", p.pj.sim.message_buffer.size());
                ImGui::LabelFormat("node", "{}", p.pj.sim.nodes.size());
                ImGui::LabelFormat(
                  "dated message alloc", "{}", p.pj.sim.dated_messages.size());
                ImGui::LabelFormat("active output ports",
                                   "{}",
                                   p.pj.sim.active_output_ports.size());

                ImGui::LabelFormat("contant sources",
                                   "{}",
                                   p.pj.sim.srcs.constant_sources.size());
                ImGui::LabelFormat(
                  "text sources", "{}", p.pj.sim.srcs.text_file_sources.size());
                ImGui::LabelFormat("binary sources",
                                   "{}",
                                   p.pj.sim.srcs.binary_file_sources.size());
                ImGui::LabelFormat(
                  "random sources", "{}", p.pj.sim.srcs.random_sources.size());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Components")) {
        const auto& vec = app.mod.components.get<component>();
        for (const auto id : app.mod.components) {
            const auto& compo = vec[id];

            ImGui::PushID(get_index(id));
            if (ImGui::TreeNode(compo.name.c_str())) {
                ImGui::LabelFormat(
                  "type", "{}", component_type_names[ordinal(compo.type)]);

                switch (compo.type) {
                case component_type::generic:
                    ImGui::LabelFormat(
                      "id", "{}", ordinal(compo.id.generic_id));
                    break;

                case component_type::grid:
                    ImGui::LabelFormat("id", "{}", ordinal(compo.id.grid_id));
                    break;

                case component_type::graph:
                    ImGui::LabelFormat("id", "{}", ordinal(compo.id.graph_id));
                    break;

                case component_type::hsm:
                    ImGui::LabelFormat("id", "{}", ordinal(compo.id.hsm_id));
                    break;

                case component_type::simulation:
                    ImGui::LabelFormat("id", "{}", ordinal(compo.id.sim_id));
                    break;

                case component_type::none:
                    break;
                }

                if (const auto* f = app.mod.file_paths.try_to_get(compo.file)) {
                    ImGui::LabelFormat("File", "{}", f->path.sv());
                    if (const auto* d =
                          app.mod.dir_paths.try_to_get(f->parent)) {
                        ImGui::LabelFormat("Dir", "{}", d->path.sv());
                        if (const auto* r =
                              app.mod.registred_paths.try_to_get(d->parent)) {
                            ImGui::LabelFormat("Reg", "{}", r->path.sv());
                        }
                    }
                }

                ImGui::LabelFormat("Description", "{}", ordinal(compo.desc));

                ImGui::TreePop();
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
