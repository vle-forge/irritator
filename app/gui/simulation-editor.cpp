// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "gui.hpp"
#include "imnodes.hpp"
#include "implot.h"
#include "node-editor.hpp"

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
        std::fill_n(output->xs.data(), output->xs.size(), 0.f);
        std::fill_n(output->ys.data(), output->ys.size(), 0.f);
        output->tl = t;
        output->min = -1.f;
        output->max = +1.f;
        output->id = 0;
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
            if (static_cast<size_t>(output->id) < output->xs.size()) {
                output->ys[output->id] = value;
                output->xs[output->id] = static_cast<float>(t);
                ++output->id;
            }
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
        auto* output = ed.observation_outputs.emplace_back(obs->name.c_str());

        const auto type =
          ed.observation_types[get_index(ed.sim.observers.get_id(*obs))];
        const auto diff = ed.simulation_end - ed.simulation_begin;
        const auto freq = diff / obs->time_step;
        const auto length = static_cast<size_t>(freq);
        output->observation_type = type;

        if (match(type,
                  observation_output::type::multiplot,
                  observation_output::type::both)) {
            output->xs.init(length);
            output->ys.init(length);
        }

        if (!obs->name.empty()) {
            const std::filesystem::path obs_file_path =
              ed.observation_directory / obs->name.c_str();

            if (type == observation_output::type::file ||
                type == observation_output::type::both) {
                if (output->ofs.open(obs_file_path); !output->ofs.is_open())
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
        obs->user_data = static_cast<void*>(output);
    }
}

static void
run_simulation(window_logger& log_w,
               simulation& sim,
               double begin,
               double end,
               double& current,
               simulation_status& st,
               const bool& stop) noexcept
{
    current = begin;
    if (auto ret = sim.initialize(current); irt::is_bad(ret)) {
        log_w.log(3,
                  "Simulation initialization failure (%s)\n",
                  irt::status_string(ret));
        st = simulation_status::success;
        return;
    }

    do {
        if (auto ret = sim.run(current); irt::is_bad(ret)) {
            log_w.log(3, "Simulation failure (%s)\n", irt::status_string(ret));
            st = simulation_status::success;
            return;
        }
    } while (current < end && !stop);

    sim.clean();

    st = simulation_status::running_once_need_join;
}

static void
show_simulation_run_once(window_logger& log_w, editor& ed)
{
    if (ed.st == simulation_status::running_once_need_join) {
        if (ed.simulation_thread.joinable()) {
            ed.simulation_thread.join();
            ed.st = simulation_status::success;
        }
    } else {
        if (ImGui::Button("run")) {
            initialize_observation(log_w, ed);

            ed.st = simulation_status::running_once;
            ed.stop = false;

            ed.simulation_thread = std::thread(&run_simulation,
                                               std::ref(log_w),
                                               std::ref(ed.sim),
                                               ed.simulation_begin,
                                               ed.simulation_end,
                                               std::ref(ed.simulation_current),
                                               std::ref(ed.st),
                                               std::cref(ed.stop));
        }
    }

    if (ed.st == simulation_status::running_once) {
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
    ed.simulation_until = static_cast<float>(ed.simulation_begin);

    if (auto ret = ed.sim.initialize(ed.simulation_current); irt::is_bad(ret)) {
        log_w.log(3,
                  "Simulation initialisation failure (%s)\n",
                  irt::status_string(ret));
        ed.st = simulation_status::success;
    } else {
        ed.simulation_next_time = ed.sim.sched.empty()
                                    ? time_domain<time>::infinity
                                    : ed.sim.sched.tn();
        ed.st = simulation_status::running_step;
    }
}

static void
show_simulation_run_debug(window_logger& log_w, editor& ed)
{
    if (ed.st == simulation_status::success) {
        ImGui::Text("simulation finished");

        if (ImGui::Button("init."))
            simulation_init(log_w, ed);
    } else {
        ImGui::Text("Current time %g", ed.simulation_current);
        ImGui::Text("Next time %g", ed.simulation_next_time);
        ImGui::Text("Bag size %lu", (unsigned long)ed.sim.sched.size());

        if (ImGui::Button("re-init."))
            simulation_init(log_w, ed);

        ImGui::SameLine();

        if (ImGui::Button("Next bag")) {
            auto ret = ed.sim.run(ed.simulation_current);
            if (is_bad(ret)) {
                ed.st = simulation_status::success;
                ed.simulation_next_time = time_domain<time>::infinity;
            } else {
                ed.simulation_next_time = ed.sim.sched.empty()
                                            ? time_domain<time>::infinity
                                            : ed.sim.sched.tn();
            }
        }
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

        // if (match(ed->st,
        //           simulation_status::success,
        //           simulation_status::running_step)) {
        //     if (ImGui::CollapsingHeader("Simulation step by step")) {
        //         if (ImGui::Button("init")) {
        //             ed->sim.clean();
        //             initialize_observation(log_w, ed);
        //             ed->simulation_current = ed->simulation_begin;
        //             if (auto ret =
        //             ed->sim.initialize(ed->simulation_current);
        //                 irt::is_bad(ret)) {
        //                 log_w.log(3,
        //                           "Simulation initialization failure (%s)\n",
        //                           irt::status_string(ret));
        //                 ed->st = simulation_status::success;
        //             } else {
        //                 ed->st = simulation_status::running_step;
        //             }
        //         }

        //         if (ed->st == simulation_status::running_step) {
        //             ImGui::SameLine();
        //             if (ImGui::Button("step")) {
        //                 if (ed->simulation_current < ed->simulation_end) {
        //                     if (auto ret =
        //                     ed->sim.run(ed->simulation_current);
        //                         irt::is_bad(ret)) {
        //                         log_w.log(3,
        //                                   "Simulation failure (%s)\n",
        //                                   irt::status_string(ret));

        //                         ed->st = simulation_status::success;
        //                     }
        //                 }
        //             }
        //         }

        //         if (ed->st == simulation_status::running_step) {
        //             ImGui::SameLine();
        //             if (ImGui::Button("step x10")) {
        //                 for (int i = 0; i < 10; ++i) {
        //                     if (ed->simulation_current < ed->simulation_end)
        //                     {
        //                         if (auto ret =
        //                               ed->sim.run(ed->simulation_current);
        //                             irt::is_bad(ret)) {
        //                             log_w.log(3,
        //                                       "Simulation failure (%s)\n",
        //                                       irt::status_string(ret));

        //                             ed->st = simulation_status::success;
        //                         }
        //                     }
        //                 }
        //             }
        //         }

        //         if (ed->st == simulation_status::running_step) {
        //             ImGui::SameLine();
        //             if (ImGui::Button("step x100")) {
        //                 for (int i = 0; i < 100; ++i) {
        //                     if (ed->simulation_current < ed->simulation_end)
        //                     {
        //                         if (auto ret =
        //                               ed->sim.run(ed->simulation_current);
        //                             irt::is_bad(ret)) {
        //                             log_w.log(3,
        //                                       "Simulation failure (%s)\n",
        //                                       irt::status_string(ret));

        //                             ed->st = simulation_status::success;
        //                         }
        //                     }
        //                 }
        //             }
        //         }
        //     }

        // if (ImGui::CollapsingHeader("Simulation until")) {
        //     if (ImGui::Button("init")) {
        //         ed->sim.clean();
        //         initialize_observation(log_w, ed);
        //         ed->simulation_current = ed->simulation_begin;
        //         ed->simulation_until =
        //         static_cast<float>(ed->simulation_begin); if (auto ret =
        //         ed->sim.initialize(ed->simulation_current);
        //             irt::is_bad(ret)) {
        //             log_w.log(3,
        //                       "Simulation initialization failure (%s)\n",
        //                       irt::status_string(ret));
        //             ed->st = simulation_status::success;
        //         } else {
        //             ed->st = simulation_status::running_step;
        //         }
        //     }

        //     if (ed->st == simulation_status::running_step) {
        //         ImGui::SliderFloat("date",
        //                            &ed->simulation_until,
        //                            static_cast<float>(ed->simulation_current),
        //                            static_cast<float>(ed->simulation_end));

        //         if (ImGui::Button("run until")) {
        //             while (ed->simulation_current <
        //                    static_cast<double>(ed->simulation_until)) {
        //                 if (auto ret = ed->sim.run(ed->simulation_current);
        //                     irt::is_bad(ret)) {
        //                     log_w.log(3,
        //                               "Simulation failure (%s)\n",
        //                               irt::status_string(ret));

        //                     ed->st = simulation_status::success;
        //                 }
        //             }
        //         }
        //     }

        if (match(ed->st,
                  simulation_status::success,
                  simulation_status::running_once,
                  simulation_status::running_once_need_join,
                  simulation_status::running_step)) {
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
