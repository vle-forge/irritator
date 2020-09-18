// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <cstdlib>

#include "gui.hpp"
#include "imnodes.hpp"
#include "implot.h"
#include "node-editor.hpp"

#include <chrono>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/io.hpp>

namespace irt {
static void
observation_output_initialize(const irt::observer& obs,
                              const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    if (match(output->observation_type,
              observation_output::type::multiplot,
              observation_output::type::both)) {
        output->xs.clear();
        output->ys.clear();
        output->tl = t;
        output->min = -1.f;
        output->max = +1.f;
    }

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        if (!output->ofs.is_open()) {
            if (output->observation_type == observation_output::type::both)
                output->observation_type = observation_output::type::multiplot;
            else
                output->observation_type = observation_output::type::none;
        } else
            output->ofs << "t," << output->name << '\n';
    }
}

static void
observation_output_observe(const irt::observer& obs,
                           const irt::time t,
                           const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    const auto value = static_cast<float>(msg[0]);

    if (match(output->observation_type,
              observation_output::type::multiplot,
              observation_output::type::both)) {
        output->min = std::min(output->min, value);
        output->max = std::max(output->max, value);

        for (double to_fill = output->tl; to_fill < t;
             to_fill += obs.time_step) {
            output->ys.emplace_back(value);
            output->xs.emplace_back(static_cast<float>(t));
        }
    }

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        output->ofs << t << ',' << value << '\n';
    }

    output->tl = t;
}

static void
observation_output_free(const irt::observer& obs,
                        const irt::time /*t*/) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        output->ofs.close();
    }
}

static void
initialize_observation(window_logger& log_w, irt::editor& ed) noexcept
{
    ed.observation_outputs.clear();
    observer* obs = nullptr;
    while (ed.sim.observers.next(obs)) {
        auto& output = ed.observation_outputs.emplace_back(obs->name.sv());

        const auto type =
          ed.observation_types[get_index(ed.sim.observers.get_id(*obs))];
        const auto diff = ed.simulation_end - ed.simulation_begin;
        const auto freq = diff / obs->time_step;
        const auto length = static_cast<size_t>(freq);
        output.observation_type = type;

        if (match(type,
                  observation_output::type::multiplot,
                  observation_output::type::both)) {
            output.xs.clear();
            output.ys.clear();
            output.xs.reserve(length);
            output.ys.reserve(length);
        }

        if (!obs->name.empty()) {
            const std::filesystem::path obs_file_path =
              ed.observation_directory / obs->name.c_str();

            if (type == observation_output::type::file ||
                type == observation_output::type::both) {
                if (output.ofs.open(obs_file_path); !output.ofs.is_open())
                    log_w.log(4,
                              "Fail to open "
                              "observation file: %s in "
                              "%s\n",
                              obs->name.c_str(),
#if _WIN32
                              ed.observation_directory.u8string().c_str());
#else
                              reinterpret_cast<const char*>(
                                ed.observation_directory.u8string().c_str()));
#endif
            }
        }

        obs->initialize = &observation_output_initialize;
        obs->observe = &observation_output_observe;
        obs->free = &observation_output_free;
        obs->user_data = static_cast<void*>(&output);
    }
}

static void
run_synchronized_simulation(window_logger& log_w,
                            simulation& sim,
                            double begin,
                            double end,
                            double synchronize_timestep,
                            double& current,
                            editor_status& st,
                            status& sim_st,
                            const bool& stop) noexcept
{
    current = begin;
    st = editor_status::initializing;
    if (sim_st = sim.initialize(current); irt::is_bad(sim_st)) {
        log_w.log(3,
                  "Simulation initialization failure (%s)\n",
                  irt::status_string(sim_st));
        st = editor_status::editing;
        return;
    }

    st = editor_status::running_thread;
    do {
        const double old = current;

        if (sim_st = sim.run(current); irt::is_bad(sim_st)) {
            log_w.log(
              3, "Simulation failure (%s)\n", irt::status_string(sim_st));
            st = editor_status::editing;
            return;
        }

        const auto duration = (current - old) / synchronize_timestep;
        if (duration > 0.)
            std::this_thread::sleep_for(
              std::chrono::duration<double>(duration));
    } while (current < end && !stop);

    sim.clean();

    st = editor_status::running_thread_need_join;
}

static void
run_simulation(window_logger& log_w,
               simulation& sim,
               double begin,
               double end,
               double& current,
               editor_status& st,
               status& sim_st,
               const bool& stop) noexcept
{
    current = begin;
    st = editor_status::initializing;
    if (sim_st = sim.initialize(current); irt::is_bad(sim_st)) {
        log_w.log(3,
                  "Simulation initialization failure (%s)\n",
                  irt::status_string(sim_st));
        st = editor_status::editing;
        return;
    }

    st = editor_status::running_thread;
    do {
        if (sim_st = sim.run(current); irt::is_bad(sim_st)) {
            log_w.log(
              3, "Simulation failure (%s)\n", irt::status_string(sim_st));
            st = editor_status::editing;
            return;
        }
    } while (current < end && !stop);

    sim.clean();

    st = editor_status::running_thread_need_join;
}

static void
show_simulation_run_once(window_logger& log_w, editor& ed)
{
    if (ed.st == editor_status::running_thread_need_join) {
        if (ed.simulation_thread.joinable()) {
            ed.simulation_thread.join();
            ed.st = editor_status::editing;
        }
    } else {
        ImGui::Checkbox("Time synchronization", &ed.use_real_time);

        if (ed.use_real_time)
            ImGui::InputDouble("t/s", &ed.synchronize_timestep);

        if (ImGui::Button("run")) {
            initialize_observation(log_w, ed);
            ed.stop = false;

            if (ed.use_real_time)
                ed.simulation_thread =
                  std::thread(&run_synchronized_simulation,
                              std::ref(log_w),
                              std::ref(ed.sim),
                              ed.simulation_begin,
                              ed.simulation_end,
                              ed.synchronize_timestep,
                              std::ref(ed.simulation_current),
                              std::ref(ed.st),
                              std::ref(ed.sim_st),
                              std::cref(ed.stop));
            else
                ed.simulation_thread =
                  std::thread(&run_simulation,
                              std::ref(log_w),
                              std::ref(ed.sim),
                              ed.simulation_begin,
                              ed.simulation_end,
                              std::ref(ed.simulation_current),
                              std::ref(ed.st),
                              std::ref(ed.sim_st),
                              std::cref(ed.stop));
        }
    }

    if (ed.st == editor_status::running_thread) {
        ImGui::SameLine();
        if (ImGui::Button("Force stop"))
            ed.stop = true;
    }
}

static void
simulation_init(window_logger& log_w, editor& ed)
{
    ed.sim.clean();

    initialize_observation(log_w, ed);

    ed.simulation_current = ed.simulation_begin;
    ed.simulation_during_date = ed.simulation_begin;
    ed.st = editor_status::initializing;

    ed.models_make_transition.resize(ed.sim.models.max_used(), false);

    if (ed.sim_st = ed.sim.initialize(ed.simulation_current);
        irt::is_bad(ed.sim_st)) {
        log_w.log(3,
                  "Simulation initialisation failure (%s)\n",
                  irt::status_string(ed.sim_st));
        ed.st = editor_status::editing;
    } else {
        ed.simulation_next_time = ed.sim.sched.empty()
                                    ? time_domain<time>::infinity
                                    : ed.sim.sched.tn();
        ed.simulation_bag_id = 0;
        ed.st = editor_status::running_debug;
    }
}

static void
show_simulation_run_debug(window_logger& log_w, editor& ed)
{
    ImGui::Text("Current time %g", ed.simulation_current);
    ImGui::Text("Current bag %ld", ed.simulation_bag_id);
    ImGui::Text("Next time %g", ed.simulation_next_time);
    ImGui::Text("Model %lu", (unsigned long)ed.sim.sched.size());

    if (ImGui::Button("init."))
        simulation_init(log_w, ed);

    ImGui::SameLine();

    if (ImGui::Button("Next bag")) {
        if (ed.sim_st = ed.sim.run(ed.simulation_current); is_bad(ed.sim_st)) {
            ed.st = editor_status::editing;
            log_w.log(3,
                      "Simulation next bag failure (%s)\n",
                      irt::status_string(ed.sim_st));
        }
        ++ed.simulation_bag_id;
    }

    ImGui::SameLine();

    if (ImGui::Button("Bag >> 10")) {
        for (int i = 0; i < 10; ++i) {
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                log_w.log(3,
                          "Simulation next bag failure (%s)\n",
                          irt::status_string(ed.sim_st));
                break;
            }
            ++ed.simulation_bag_id;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Bag >> 100")) {
        for (int i = 0; i < 100; ++i) {
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                log_w.log(3,
                          "Simulation next bag failure (%s)\n",
                          irt::status_string(ed.sim_st));
                break;
            }
            ++ed.simulation_bag_id;
        }
    }

    ImGui::InputDouble("##date", &ed.simulation_during_date);

    ImGui::SameLine();

    if (ImGui::Button("run##date")) {
        const auto end = ed.simulation_current + ed.simulation_during_date;
        while (ed.simulation_current < end) {
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                log_w.log(3,
                          "Simulation next bag failure (%s)\n",
                          irt::status_string(ed.sim_st));
                break;
            }
            ++ed.simulation_bag_id;
        }
    }

    ImGui::InputInt("##bag", &ed.simulation_during_bag);

    ImGui::SameLine();

    if (ImGui::Button("run##bag")) {
        for (int i = 0, e = ed.simulation_during_bag; i != e; ++i) {
            if (ed.sim_st = ed.sim.run(ed.simulation_current);
                is_bad(ed.sim_st)) {
                ed.st = editor_status::editing;
                log_w.log(3,
                          "Simulation next bag failure (%s)\n",
                          irt::status_string(ed.sim_st));
                break;
            }
            ++ed.simulation_bag_id;
        }
    }

    if (ed.st == editor_status::running_debug) {
        ed.simulation_next_time = ed.sim.sched.empty()
                                    ? time_domain<time>::infinity
                                    : ed.sim.sched.tn();

        const auto& l = ed.sim.sched.list_model_id();

        ed.models_make_transition.resize(ed.sim.models.max_used(), false);
        for (auto it = l.begin(), e = l.end(); it != e; ++it)
            ed.models_make_transition[get_index(*it)] = true;
    } else {
        ed.simulation_next_time = time_domain<time>::infinity;
    }
}

void
show_simulation_box(window_logger& log_w, bool* show_simulation)
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Simulation", show_simulation)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        ImGui::InputDouble("Begin", &ed->simulation_begin);
        ImGui::InputDouble("End", &ed->simulation_end);
        ImGui::Checkbox("Show values", &ed->simulation_show_value);

        if (ImGui::CollapsingHeader("Simulation run one"))
            show_simulation_run_once(log_w, *ed);

        if (ImGui::CollapsingHeader("Debug simulation"))
            show_simulation_run_debug(log_w, *ed);

        if (ed->st != editor_status::editing) {
            ImGui::Text("Current: %g", ed->simulation_current);

            const double duration = ed->simulation_end - ed->simulation_begin;
            const double elapsed =
              ed->simulation_current - ed->simulation_begin;
            const double fraction = elapsed / duration;
            ImGui::ProgressBar(static_cast<float>(fraction));
        }
    }

    ImGui::End();
}

} // namesapce irt
