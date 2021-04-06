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
#include <filesystem>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <irritator/core.hpp>
#include <irritator/io.hpp>

namespace irt {

static void
push_data(std::vector<float>& xs,
          std::vector<float>& ys,
          const double x,
          const double y) noexcept
{
    if (xs.size() < xs.capacity()) {
        xs.emplace_back(static_cast<float>(x));
        ys.emplace_back(static_cast<float>(y));
    }
}

static void
pop_data(std::vector<float>& xs, std::vector<float>& ys) noexcept
{
    ys.pop_back();
    xs.pop_back();
}

void
plot_output::operator()(const irt::observer& obs,
                        const irt::dynamics_type type,
                        const irt::time tl,
                        const irt::time t,
                        const irt::observer::status s)
{
    if (s == irt::observer::status::initialize) {
        xs.clear();
        ys.clear();
        xs.reserve(4096u * 4096u);
        ys.reserve(4096u * 4096u);
        return;
    }

    while (!xs.empty() && xs.back() == tl)
        pop_data(xs, ys);

    switch (type) {
    case irt::dynamics_type::qss1_integrator: {
        for (auto td = tl; td < t; td += time_step) {
            const auto e = td - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e;
            push_data(xs, ys, td, value);
        }
        const auto e = t - tl;
        const auto value = obs.msg[0] + obs.msg[1] * e;
        push_data(xs, ys, t, value);
    } break;

    case irt::dynamics_type::qss2_integrator: {
        for (auto td = tl; td < t; td += time_step) {
            const auto e = td - tl;
            const auto value =
              obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / 2.0);
            push_data(xs, ys, td, value);
        }
        const auto e = t - tl;
        const auto value =
          obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / 2.0);
        push_data(xs, ys, t, value);
    } break;

    case irt::dynamics_type::qss3_integrator: {
        for (auto td = tl; td < t; td += time_step) {
            const auto e = td - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e +
                               (obs.msg[2] * e * e / 2.0) +
                               (obs.msg[3] * e * e * e / 3.0);
            push_data(xs, ys, td, value);
        }
        const auto e = t - tl;
        const auto value = obs.msg[0] + obs.msg[1] * e +
                           (obs.msg[2] * e * e / 2.0) +
                           (obs.msg[3] * e * e * e / 3.0);
        push_data(xs, ys, t, value);
    } break;

    default:
        push_data(xs, ys, t, obs.msg[0]);
        break;
    }
}

void
file_discrete_output::operator()(const irt::observer& obs,
                                 const irt::dynamics_type type,
                                 const irt::time tl,
                                 const irt::time t,
                                 const irt::observer::status s)
{
    switch (s) {
    case irt::observer::status::initialize: {
        std::filesystem::path file;
        if (ed && !ed->observation_directory.empty())
            file = ed->observation_directory;

        file.append(obs.name.begin());
        file.replace_extension(".dat");

        ofs.open(file);
        fmt::print(ofs, "t,{}\n", name.c_str());
    } break;

    default:
        switch (type) {
        case irt::dynamics_type::qss1_integrator: {
            for (auto td = tl; td < t; td += time_step) {
                const auto e = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e;
                fmt::print(ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e;
            fmt::print(ofs, "{},{}\n", t, value);
        } break;

        case irt::dynamics_type::qss2_integrator: {
            for (auto td = tl; td < t; td += time_step) {
                const auto e = td - tl;
                const auto value =
                  obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / 2.0);
                fmt::print(ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value =
              obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / 2.0);
            fmt::print(ofs, "{},{}\n", t, value);
        } break;

        case irt::dynamics_type::qss3_integrator: {
            for (auto td = tl; td < t; td += time_step) {
                const auto e = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e +
                                   (obs.msg[2] * e * e / 2.0) +
                                   (obs.msg[3] * e * e * e / 3.0);
                fmt::print(ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e +
                               (obs.msg[2] * e * e / 2.0) +
                               (obs.msg[3] * e * e * e / 3.0);
            fmt::print(ofs, "{},{}\n", t, value);
        } break;

        default:
            fmt::print(ofs, "{},{}\n", t, obs.msg[0]);
            break;
        }
    }

    if (s == irt::observer::status::finalize)
        ofs.close();
}

void
file_output::operator()(const irt::observer& obs,
                        const irt::dynamics_type type,
                        const irt::time tl,
                        const irt::time t,
                        const irt::observer::status s)
{
    std::filesystem::path file;

    switch (s) {
    case irt::observer::status::initialize:
        if (ed && !ed->observation_directory.empty())
            file = ed->observation_directory;

        file.append(obs.name.begin());
        file.replace_extension(".dat");

        ofs.open(file);

        switch (type) {
        case irt::dynamics_type::qss1_integrator:
            fmt::print(ofs, "t,{0},{0}'\n", name.c_str());
            break;
        case irt::dynamics_type::qss2_integrator:
            fmt::print(ofs, "t,{0},{0}',{0}''\n", name.c_str());
            break;
        case irt::dynamics_type::qss3_integrator:
            fmt::print(ofs, "t,{0},{0}',{0}'',{0}'''\n", name.c_str());
            break;
        default:
            fmt::print(ofs, "t,{}\n", name.c_str());
            break;
        }
        break;

    case irt::observer::status::run:
    case irt::observer::status::finalize:
        switch (type) {
        case irt::dynamics_type::qss1_integrator:
            fmt::print(ofs, "{},{},{}\n", t, obs.msg[0], obs.msg[1]);
            break;
        case irt::dynamics_type::qss2_integrator:
            fmt::print(
              ofs, "{},{},{},{}\n", t, obs.msg[0], obs.msg[1], obs.msg[2]);
            break;
        case irt::dynamics_type::qss3_integrator:
            fmt::print(ofs,
                       "{},{},{},{},{}\n",
                       t,
                       obs.msg[0],
                       obs.msg[1],
                       obs.msg[2],
                       obs.msg[3]);
            break;
        default:
            fmt::print(ofs, "{},{}\n", t, obs.msg[0]);
            break;
        }
        break;
    }

    if (s == irt::observer::status::finalize)
        ofs.close();
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

    sim.finalize(end);

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

    sim.finalize(end);

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
    } else if (ed.st == editor_status::editing ||
               ed.st == editor_status::running_debug) {
        ImGui::Checkbox("Time synchronization", &ed.use_real_time);

        if (ed.use_real_time) {
            ImGui::InputDouble("t/s", &ed.synchronize_timestep);

            if (ed.synchronize_timestep > 0. && ImGui::Button("run")) {
                // initialize_observation(log_w, ed);
                ed.stop = false;

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
            }
        } else {
            if (ImGui::Button("run")) {
                // initialize_observation(log_w, ed);
                ed.stop = false;

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

    // initialize_observation(log_w, ed);

    ed.simulation_current = ed.simulation_begin;
    ed.simulation_during_date = ed.simulation_begin;
    ed.st = editor_status::initializing;

    ed.models_make_transition.resize(ed.sim.models.capacity(), false);

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

        std::fill_n(ed.models_make_transition.begin(),
                    ed.models_make_transition.size(),
                    false);

        for (auto it = l.begin(), e = l.end(); it != e; ++it)
            ed.models_make_transition[get_index(*it)] = true;
    } else {
        ed.simulation_next_time = time_domain<time>::infinity;
    }
}

void
show_simulation_box(editor& ed, bool* show_simulation)
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Simulation", show_simulation)) {
        ImGui::End();
        return;
    }

    ImGui::InputDouble("Begin", &ed.simulation_begin);
    ImGui::InputDouble("End", &ed.simulation_end);
    ImGui::Checkbox("Show values", &ed.simulation_show_value);

    if (ImGui::Button("Output files"))
        ed.show_select_directory_dialog = true;

    ImGui::Text("output directory: ");
#if _WIN32
    ImGui::Text("%s", ed.observation_directory.u8string().c_str());
#else
    ImGui::Text("%s",
                reinterpret_cast<const char*>(
                  ed.observation_directory.u8string().c_str()));
#endif

    if (ImGui::CollapsingHeader("Simulation run one"))
        show_simulation_run_once(log_w, ed);

    if (ImGui::CollapsingHeader("Debug simulation"))
        show_simulation_run_debug(log_w, ed);

    if (ed.st != editor_status::editing) {
        ImGui::Text("Current: %g", ed.simulation_current);

        const double duration = ed.simulation_end - ed.simulation_begin;
        const double elapsed = ed.simulation_current - ed.simulation_begin;
        const double fraction = elapsed / duration;
        ImGui::ProgressBar(static_cast<float>(fraction));
    }

    ImGui::End();
}

} // namesapce irt
