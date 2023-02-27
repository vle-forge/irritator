// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

namespace irt {

void task_window::show_widgets() noexcept
{
    auto* app = container_of(this, &application::t_window);

    int workers = app->task_mgr.temp_workers.ssize();
    ImGui::InputInt("workers", &workers, 1, 100, ImGuiInputTextFlags_ReadOnly);

    int lists = app->task_mgr.temp_task_lists.ssize();
    ImGui::InputInt("lists", &lists, 1, 100, ImGuiInputTextFlags_ReadOnly);

    if (ImGui::CollapsingHeader("Tasks list", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Main Tasks", 3)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("Submitted tasks");
            ImGui::TableSetupColumn("finished tasks");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("simulation");
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", app->task_mgr.main_task_lists_stats[0].num_submitted_tasks);
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", app->task_mgr.main_task_lists_stats[0].num_executed_tasks);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("gui");
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", app->task_mgr.main_task_lists_stats[1].num_submitted_tasks);
            ImGui::TableNextColumn();
            ImGui::TextFormat(
              "{}", app->task_mgr.main_task_lists_stats[1].num_executed_tasks);

            for (int i = 0, e = app->task_mgr.temp_task_lists.ssize(); i != e;
                 ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("generic-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}",
                  app->task_mgr.temp_task_lists_stats[i].num_submitted_tasks);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}",
                  app->task_mgr.temp_task_lists_stats[i].num_executed_tasks);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Worker list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::BeginTable("Workers", 3)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("execution duration");
            ImGui::TableHeadersRow();

            int i = 0;
            for (int e = app->task_mgr.main_workers.ssize(); i != e; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("main-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{} ms",
                  app->task_mgr.main_workers[i].exec_time.count() / 1000);
                ImGui::TableNextColumn();
            }

            int j = 0;
            for (int e = app->task_mgr.temp_workers.ssize(); j != e; ++j, ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("generic-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{} ms",
                  app->task_mgr.temp_workers[j].exec_time.count() / 1000);
                ImGui::TableNextColumn();
            }

            ImGui::EndTable();
        }
    }
}

void task_window::show() noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);

    if (!ImGui::Begin(task_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    show_widgets();

    ImGui::End();
}

} // irt