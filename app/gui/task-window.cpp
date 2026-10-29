// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "internal.hpp"

#include <fmt/chrono.h>

#include <thread>

namespace irt {

constexpr static inline const char* unordered_phase_names[] = {
    "accepting", "executing", "shutting down"
};

constexpr static auto table_flags = ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_BordersInnerV |
                                    ImGuiTableFlags_Resizable;

constexpr static auto sample_period = std::chrono::seconds(1);

/// The list 0 is used by the GUI, the others by the simulations (see @c
/// application::get_task_manager_list).
static void show_list_name(const sz i) noexcept
{
    if (i == 0)
        ImGui::TextUnformatted("gui");
    else
        ImGui::TextFormat("simulation {}", i);
}

static void show_rate(const task_window::sample& s) noexcept
{
    ImGui::TextFormat("{:.1f}", s.rate);
}

static void show_load(const task_window::sample& s) noexcept
{
    small_string<15> str;
    format(str, "{:.0f}%", s.load * 100.f);
    ImGui::ProgressBar(s.load, ImVec2(-FLT_MIN, 0.f), str.c_str());
}

static void show_mean_time(const std::chrono::milliseconds total,
                           const u64                       tasks) noexcept
{
    if (tasks == 0)
        ImGui::TextUnformatted("-");
    else
        ImGui::TextFormat("{:.3f} ms", static_cast<double>(total.count()) /
                                         static_cast<double>(tasks));
}

/// Updates the rates and the loads of the lists and of the workers once per
/// @c sample_period.
static void update_samples(task_window& wnd, task_manager& tm) noexcept
{
    const auto now     = std::chrono::steady_clock::now();
    const auto lists   = tm.list_size() * 2u;
    const auto workers = tm.ordered_worker_size() + tm.unordered_worker_size();
    const bool first   = std::cmp_not_equal(wnd.lists.size(), lists) or
                       std::cmp_not_equal(wnd.workers.size(), workers);

    if (not first and now - wnd.last_sample < sample_period)
        return;

    if (first) {
        wnd.lists.clear();
        wnd.workers.clear();
        if (not wnd.lists.resize(lists) or not wnd.workers.resize(workers)) {
            wnd.lists.clear();
            wnd.workers.clear();
            return;
        }
    }

    const auto period  = std::chrono::duration<float>(now - wnd.last_sample);
    const auto seconds = first ? 0.f : period.count();
    wnd.last_sample    = now;

    const auto update = [seconds](task_window::sample& s, const u64 tasks,
                                  const u64 time_ms) noexcept {
        if (seconds > 0.f) {
            s.rate = static_cast<float>(tasks - s.tasks) / seconds;
            s.load = std::clamp(static_cast<float>(time_ms - s.time_ms) /
                                  (seconds * 1000.f),
                                0.f, 1.f);
        }
        s.tasks   = tasks;
        s.time_ms = time_ms;
    };

    const auto n = tm.list_size();
    for (sz i = 0; i < n; ++i) {
        update(wnd.lists[i], tm.ordered(i).tasks_completed(), 0);
        update(wnd.lists[n + i], tm.unordered(i).tasks_completed(), 0);
    }

    const auto o = tm.ordered_worker_size();
    for (sz i = 0; i < o; ++i)
        update(wnd.workers[i], tm.wordered_tasks_completed(i),
               static_cast<u64>(tm.wordered_execution_time(i).count()));

    for (sz i = 0, e = tm.unordered_worker_size(); i < e; ++i)
        update(wnd.workers[o + i], tm.wunordered_tasks_completed(i),
               static_cast<u64>(tm.wunordered_execution_time(i).count()));
}

static void show_overview(const task_window& wnd, task_manager& tm) noexcept
{
    u64 submitted = 0, completed = 0;
    for (sz i = 0, e = tm.list_size(); i < e; ++i) {
        submitted += tm.ordered(i).tasks_submitted();
        submitted += tm.unordered(i).tasks_submitted();
        completed += tm.ordered(i).tasks_completed();
        completed += tm.unordered(i).tasks_completed();
    }

    auto time = std::chrono::milliseconds::zero();
    for (sz i = 0, e = tm.ordered_worker_size(); i < e; ++i)
        time += tm.wordered_execution_time(i);
    for (sz i = 0, e = tm.unordered_worker_size(); i < e; ++i)
        time += tm.wunordered_execution_time(i);

    float rate = 0.f, load = 0.f;
    for (const auto& s : wnd.workers) {
        rate += s.rate;
        load += s.load;
    }

    ImGui::SeparatorText("Configuration");
    ImGui::LabelFormat("hardware threads", "{}",
                       std::thread::hardware_concurrency());
    ImGui::LabelFormat("lists", "{} ordered, {} unordered", tm.list_size(),
                       tm.list_size());
    ImGui::LabelFormat("workers", "{} ordered, {} unordered",
                       tm.ordered_worker_size(), tm.unordered_worker_size());

    ImGui::SeparatorText("Tasks");
    ImGui::LabelFormat("submitted", "{}", submitted);
    ImGui::LabelFormat("completed", "{}", completed);
    ImGui::LabelFormat("in progress", "{}", submitted - completed);
    ImGui::LabelFormat("execution time", "{}", time);
    ImGui::LabelFormat("tasks/s", "{:.1f}", rate);
    ImGui::LabelFormat("busy workers", "{:.2f} / {}", load, wnd.workers.size());
}

static void show_lists(const task_window& wnd, task_manager& tm) noexcept
{
    const auto n = tm.list_size();

    ImGui::SeparatorText("Ordered lists (one worker per list)");
    if (ImGui::BeginTable("Ordered lists", 7, table_flags)) {
        ImGui::TableSetupColumn("list");
        ImGui::TableSetupColumn("submitted");
        ImGui::TableSetupColumn("completed");
        ImGui::TableSetupColumn("queued");
        ImGui::TableSetupColumn("running");
        ImGui::TableSetupColumn("tasks/s");
        ImGui::TableSetupColumn("state");
        ImGui::TableHeadersRow();

        for (sz i = 0; i < n; ++i) {
            const auto& lst = tm.ordered(i);
            const auto  st  = lst.get_status();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            show_list_name(i);
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", lst.tasks_submitted());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", lst.tasks_completed());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{} / {}", st.queued,
                              ordered_task_list::task_max);
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", st.running);
            ImGui::TableNextColumn();
            if (i < wnd.lists.size())
                show_rate(wnd.lists[i]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(st.stopping ? "stopping" : "running");
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Unordered lists (batches shared by the workers)");
    if (ImGui::BeginTable("Unordered lists", 8, table_flags)) {
        ImGui::TableSetupColumn("list");
        ImGui::TableSetupColumn("submitted");
        ImGui::TableSetupColumn("completed");
        ImGui::TableSetupColumn("phase");
        ImGui::TableSetupColumn("batch");
        ImGui::TableSetupColumn("pending");
        ImGui::TableSetupColumn("incoming");
        ImGui::TableSetupColumn("tasks/s");
        ImGui::TableHeadersRow();

        for (sz i = 0; i < n; ++i) {
            const auto& lst = tm.unordered(i);
            const auto  st  = lst.get_status();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            show_list_name(i);
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", lst.tasks_submitted());
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", lst.tasks_completed());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(unordered_phase_names[ordinal(st.current)]);
            ImGui::TableNextColumn();
            if (st.current == unordered_task_list::phase::executing) {
                ImGui::TextFormat("{} done, {} started / {}", st.completed,
                                  st.started, st.batch_size);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("completed, taken by a worker / size "
                                      "of the batch");
            } else {
                ImGui::TextUnformatted("-");
            }
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", st.pending);
            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", st.incoming);
            ImGui::TableNextColumn();
            if (n + i < wnd.lists.size())
                show_rate(wnd.lists[n + i]);
        }

        ImGui::EndTable();
    }
}

/// The unordered lists only serve the parallel tick of the observers: a
/// project with few observers never uses them.
static void show_players(application& app) noexcept
{
    ImGui::SeparatorText("Simulation players (tick of the observers)");
    if (not ImGui::BeginTable("Players", 4, table_flags))
        return;

    ImGui::TableSetupColumn("project");
    ImGui::TableSetupColumn("list");
    ImGui::TableSetupColumn("observers");
    ImGui::TableSetupColumn("tick", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto& ed : app.pjs) {
        const auto& player    = ed.sim_player.player;
        const auto  threshold = player.parallel_tick_threshold();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ed.pj.name.c_str());
        ImGui::TableNextColumn();
        show_list_name(app.get_task_manager_list(app.pjs.get_id(ed)));
        ImGui::TableNextColumn();

        if (not ed.sim_player.simulation_readable()) {
            ImGui::TextUnformatted("-");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("restoring");
            continue;
        }

        const auto observers = static_cast<std::size_t>(
          ed.pj.sim.observers.size());
        ImGui::TextFormat("{}", observers);
        ImGui::TableNextColumn();

        if (threshold == 0)
            ImGui::TextUnformatted("sequential: no unordered worker");
        else if (observers < threshold)
            ImGui::TextFormat("sequential: {} < {} observers", observers,
                              threshold);
        else
            ImGui::TextFormat("parallel from {} observers resampled together",
                              threshold);
    }

    ImGui::EndTable();
}

static void show_workers(const task_window& wnd, task_manager& tm) noexcept
{
    if (not ImGui::BeginTable("Workers", 7, table_flags))
        return;

    ImGui::TableSetupColumn("worker");
    ImGui::TableSetupColumn("lists");
    ImGui::TableSetupColumn("tasks");
    ImGui::TableSetupColumn("execution time");
    ImGui::TableSetupColumn("mean time");
    ImGui::TableSetupColumn("tasks/s");
    ImGui::TableSetupColumn("load", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    const auto o = tm.ordered_worker_size();

    for (sz i = 0; i < o; ++i) {
        const auto tasks = tm.wordered_tasks_completed(i);
        const auto time  = tm.wordered_execution_time(i);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextFormat("ordered {}", i);
        ImGui::TableNextColumn();
        show_list_name(i);
        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", tasks);
        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", time);
        ImGui::TableNextColumn();
        show_mean_time(time, tasks);
        if (i < wnd.workers.size()) {
            ImGui::TableNextColumn();
            show_rate(wnd.workers[i]);
            ImGui::TableNextColumn();
            show_load(wnd.workers[i]);
        }
    }

    for (sz i = 0, e = tm.unordered_worker_size(); i < e; ++i) {
        const auto tasks = tm.wunordered_tasks_completed(i);
        const auto time  = tm.wunordered_execution_time(i);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextFormat("unordered {}", i);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("all");
        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", tasks);
        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", time);
        ImGui::TableNextColumn();
        show_mean_time(time, tasks);
        if (o + i < wnd.workers.size()) {
            ImGui::TableNextColumn();
            show_rate(wnd.workers[o + i]);
            ImGui::TableNextColumn();
            show_load(wnd.workers[o + i]);
        }
    }

    ImGui::EndTable();
}

template<typename Allocator>
static void show_memory_row(const char* name) noexcept
{
    const auto [allocated, deallocated] = Allocator::get_memory_usage();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    ImGui::TextFormat("{}", human_readable_bytes(allocated));
    ImGui::TableNextColumn();
    ImGui::TextFormat("{}", human_readable_bytes(deallocated));
    ImGui::TableNextColumn();
    ImGui::TextFormat("{}", human_readable_bytes(allocated - deallocated));
}

static void show_memory() noexcept
{
    if (not ImGui::BeginTable("Threads", 4, table_flags))
        return;

    ImGui::TableSetupColumn("name");
    ImGui::TableSetupColumn("allocated");
    ImGui::TableSetupColumn("deallocated");
    ImGui::TableSetupColumn("remaining");
    ImGui::TableHeadersRow();

    show_memory_row<allocator<new_delete_memory_resource>>("global");
    show_memory_row<allocator<monotonic_small_buffer<1024 * 1024, 1>>>(
      "thread-1");
    show_memory_row<allocator<monotonic_small_buffer<1024 * 1024, 2>>>(
      "thread-2");

    ImGui::EndTable();
}

void task_window::show_widgets() noexcept
{
    auto& app = container_of(this, &application::task_wnd);

    update_samples(*this, app.task_mgr);

    if (not ImGui::BeginTabBar("Tasks"))
        return;

    if (ImGui::BeginTabItem("Overview")) {
        show_overview(*this, app.task_mgr);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Lists")) {
        show_lists(*this, app.task_mgr);
        show_players(app);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Workers")) {
        show_workers(*this, app.task_mgr);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Memory")) {
        show_memory();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

void task_window::show() noexcept
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Once);

    if (!ImGui::Begin(task_window::name, &is_open)) {
        ImGui::End();
        return;
    }

    show_widgets();

    ImGui::End();
}

} // irt
