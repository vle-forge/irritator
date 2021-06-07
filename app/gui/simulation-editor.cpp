// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <cstdlib>

#include "application.hpp"
#include "internal.hpp"

#include "imnodes.hpp"
#include "implot.h"

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

    while (!xs.empty() && static_cast<double>(xs.back()) == tl)
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
                        const irt::time /*tl*/,
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
show_simulation_run(window_logger& /*log_w*/, editor& ed)
{
    ImGui::Text("Current time %g", ed.simulation_current);
    ImGui::Text("Current bag %ld", ed.simulation_bag_id);
    ImGui::Text("Next time %g", ed.simulation_next_time);
    ImGui::Text("Model %lu", (unsigned long)ed.sim.sched.size());

    ImGui::SliderFloat("Speed",
                       &ed.synchronize_timestep,
                       0.00001f,
                       1.0f,
                       "%.6f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    HelpMarker("1.0 means maximum speed to run simulation for all editors. "
               "Smaller values slow down the simulation speed.");

    if (ImGui::Button("[]")) {
        ed.sim.finalize(ed.simulation_current);
        ed.simulation_current = ed.simulation_begin;
        ed.simulation_bag_id = -1;
        ed.simulation_during_date = ed.simulation_begin;
        ed.st = editor_status::editing;
    }
    ImGui::SameLine();
    if (ImGui::Button("||")) {
        if (ed.is_running())
            ed.st = editor_status::running_pause;
        else
            ed.st = editor_status::running;
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        if (ed.st == editor_status::editing)
            ed.simulation_bag_id = -1;
        ed.st = editor_status::running;
    }
    ImGui::SameLine();
    if (ImGui::Button("+1")) {
        if (ed.st == editor_status::editing)
            ed.simulation_bag_id = -1;
        ed.st = editor_status::running_1_step;
        ed.step_by_step_bag = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("+10")) {
        if (ed.st == editor_status::editing)
            ed.simulation_bag_id = -1;
        ed.st = editor_status::running_10_step;
        ed.step_by_step_bag = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("+100")) {
        if (ed.st == editor_status::editing)
            ed.simulation_bag_id = -1;
        ed.st = editor_status::running_100_step;
        ed.step_by_step_bag = 0;
    }
}

void
application::show_simulation_window()
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Simulation", &show_simulation)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        ImGui::InputDouble("Begin", &ed->simulation_begin);
        ImGui::InputDouble("End", &ed->simulation_end);
        ImGui::Checkbox("Show values", &ed->simulation_show_value);

        if (ImGui::Button("Output files"))
            ed->show_select_directory_dialog = true;

        ImGui::Text("output directory:");
#if _WIN32
        ImGui::InputText(
          "Path",
          const_cast<char*>(ed->observation_directory.u8string().c_str()),
          ed->observation_directory.u8string().size(),
          ImGuiInputTextFlags_ReadOnly);
#else
        ImGui::InputText("Path",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           ed->observation_directory.u8string().c_str())),
                         ed->observation_directory.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);
#endif

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        show_simulation_run(log_w, *ed);

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
