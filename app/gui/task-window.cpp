// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <fmt/core.h>
#include <fmt/format.h>

namespace irt {

constexpr static inline std::string_view main_task_strings[] = { "simulation-0",
                                                                 "simulation-1",
                                                                 "simulation-2",
                                                                 "Gui" };

void task_window::show_widgets() noexcept
{
    auto& app = container_of(this, &application::task_wnd);

    ImGui::LabelFormat(
      "workers", "{}", app.task_mgr.unordered_task_workers.ssize());
    ImGui::LabelFormat(
      "lists", "{}", app.task_mgr.unordered_task_lists.ssize());

    if (ImGui::CollapsingHeader("Tasks list", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Tasks list", 3)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("Submitted tasks");
            ImGui::TableSetupColumn("finished tasks");
            ImGui::TableHeadersRow();

            auto i = 0;
            for (const auto& stats : app.task_mgr.ordered_task_stats) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(main_task_strings[i].data(),
                                       main_task_strings[i].data() +
                                         main_task_strings[i].size());
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", stats.num_submitted_tasks);
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", stats.num_executed_tasks);
                ++i;
            }

            for (const auto& stats : app.task_mgr.unordered_task_stats) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("generic-{}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", stats.num_submitted_tasks);
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", stats.num_executed_tasks);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Worker list",
                                ImGuiTreeNodeFlags_DefaultOpen)) {

        if (ImGui::BeginTable("Workers", 2)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("execution duration");
            ImGui::TableHeadersRow();

            int i = 0;
            for (int e = app.task_mgr.ordered_task_workers.ssize(); i != e;
                 ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("ordered {}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}",
                  human_readable_time(
                    app.task_mgr.ordered_task_workers[i].exec_time.count()));
                ImGui::TableNextColumn();
            }

            int j = 0;
            for (int e = app.task_mgr.unordered_task_workers.ssize(); j != e;
                 ++j, ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("unordered {}", i);
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}",
                  human_readable_time(
                    app.task_mgr.unordered_task_workers[j].exec_time.count()));
                ImGui::TableNextColumn();
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Memory usage",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        using simalloc = allocator<new_delete_memory_resource>;
        using th1alloc = allocator<monotonic_small_buffer<1024 * 1024, 1>>;
        using th2alloc = allocator<monotonic_small_buffer<1024 * 1024, 2>>;

        if (ImGui::BeginTable("Threads", 4)) {
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("allocated");
            ImGui::TableSetupColumn("deallocated");
            ImGui::TableSetupColumn("remaining");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();

            {
                const auto [allocated, deallocated] =
                  simalloc::get_memory_usage();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("global");
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(allocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(deallocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}", human_readable_bytes(allocated - deallocated));
            }

            ImGui::TableNextRow();

            {
                const auto [allocated, deallocated] =
                  th1alloc::get_memory_usage();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("thread-1");
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(allocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(deallocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}", human_readable_bytes(allocated - deallocated));
            }

            ImGui::TableNextRow();

            {
                const auto [allocated, deallocated] =
                  th2alloc::get_memory_usage();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("thread-2");
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(allocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", human_readable_bytes(deallocated));
                ImGui::TableNextColumn();
                ImGui::TextFormat(
                  "{}", human_readable_bytes(allocated - deallocated));
            }

            ImGui::EndTable();
        }
    }
}

void task_window::show() noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_Once);

    if (!ImGui::Begin(task_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    show_widgets();

    ImGui::End();
}

} // irt
