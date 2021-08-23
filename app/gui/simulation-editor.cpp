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

#include "imnodes.h"
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
plot_output_callback(const irt::observer& obs,
                     const irt::dynamics_type type,
                     const irt::time tl,
                     const irt::time t,
                     const irt::observer::status s)
{
    auto* plot_output = reinterpret_cast<irt::plot_output*>(obs.user_data);

    if (s == irt::observer::status::initialize) {
        plot_output->xs.clear();
        plot_output->ys.clear();
        plot_output->xs.reserve(4096u * 4096u);
        plot_output->ys.reserve(4096u * 4096u);
        return;
    }

    while (!plot_output->xs.empty() && plot_output->xs.back() == tl)
        pop_data(plot_output->xs, plot_output->ys);

    switch (type) {
    case irt::dynamics_type::qss1_integrator: {
        for (auto td = tl; td < t; td += plot_output->time_step) {
            const auto e = td - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e;
            push_data(plot_output->xs, plot_output->ys, td, value);
        }
        const auto e = t - tl;
        const auto value = obs.msg[0] + obs.msg[1] * e;
        push_data(plot_output->xs, plot_output->ys, t, value);
    } break;

    case irt::dynamics_type::qss2_integrator: {
        for (auto td = tl; td < t; td += plot_output->time_step) {
            const auto e = td - tl;
            const auto value =
              obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
            push_data(plot_output->xs, plot_output->ys, td, value);
        }
        const auto e = t - tl;
        const auto value =
          obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
        push_data(plot_output->xs, plot_output->ys, t, value);
    } break;

    case irt::dynamics_type::qss3_integrator: {
        for (auto td = tl; td < t; td += plot_output->time_step) {
            const auto e = td - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e +
                               (obs.msg[2] * e * e / two) +
                               (obs.msg[3] * e * e * e / three);
            push_data(plot_output->xs, plot_output->ys, td, value);
        }
        const auto e = t - tl;
        const auto value = obs.msg[0] + obs.msg[1] * e +
                           (obs.msg[2] * e * e / two) +
                           (obs.msg[3] * e * e * e / three);
        push_data(plot_output->xs, plot_output->ys, t, value);
    } break;

    default:
        push_data(plot_output->xs, plot_output->ys, t, obs.msg[0]);
        break;
    }
}

void
file_discrete_output_callback(const irt::observer& obs,
                              const irt::dynamics_type type,
                              const irt::time tl,
                              const irt::time t,
                              const irt::observer::status s)
{
    auto* out = reinterpret_cast<file_discrete_output*>(obs.user_data);

    switch (s) {
    case irt::observer::status::initialize: {
        std::filesystem::path file;
        if (out->ed && !out->ed->observation_directory.empty())
            file = out->ed->observation_directory;

        file.append(obs.name.begin());
        file.replace_extension(".dat");

        out->ofs.open(file);
        fmt::print(out->ofs, "t,{}\n", out->name.c_str());
    } break;

    default:
        switch (type) {
        case irt::dynamics_type::qss1_integrator: {
            for (auto td = tl; td < t; td += out->time_step) {
                const auto e = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e;
                fmt::print(out->ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e;
            fmt::print(out->ofs, "{},{}\n", t, value);
        } break;

        case irt::dynamics_type::qss2_integrator: {
            for (auto td = tl; td < t; td += out->time_step) {
                const auto e = td - tl;
                const auto value =
                  obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
                fmt::print(out->ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value =
              obs.msg[0] + obs.msg[1] * e + (obs.msg[2] * e * e / two);
            fmt::print(out->ofs, "{},{}\n", t, value);
        } break;

        case irt::dynamics_type::qss3_integrator: {
            for (auto td = tl; td < t; td += out->time_step) {
                const auto e = td - tl;
                const auto value = obs.msg[0] + obs.msg[1] * e +
                                   (obs.msg[2] * e * e / two) +
                                   (obs.msg[3] * e * e * e / three);
                fmt::print(out->ofs, "{},{}\n", td, value);
            }
            const auto e = t - tl;
            const auto value = obs.msg[0] + obs.msg[1] * e +
                               (obs.msg[2] * e * e / two) +
                               (obs.msg[3] * e * e * e / three);
            fmt::print(out->ofs, "{},{}\n", t, value);
        } break;

        default:
            fmt::print(out->ofs, "{},{}\n", t, obs.msg[0]);
            break;
        }
    }

    if (s == irt::observer::status::finalize)
        out->ofs.close();
}

void
file_output_callback(const irt::observer& obs,
                     const irt::dynamics_type type,
                     const irt::time /*tl*/,
                     const irt::time t,
                     const irt::observer::status s)
{
    auto* out = reinterpret_cast<file_output*>(obs.user_data);
    std::filesystem::path file;

    switch (s) {
    case irt::observer::status::initialize:
        if (out->ed && !out->ed->observation_directory.empty())
            file = out->ed->observation_directory;

        file.append(obs.name.begin());
        file.replace_extension(".dat");

        out->ofs.open(file);

        switch (type) {
        case irt::dynamics_type::qss1_integrator:
            fmt::print(out->ofs, "t,{0},{0}'\n", out->name.c_str());
            break;
        case irt::dynamics_type::qss2_integrator:
            fmt::print(out->ofs, "t,{0},{0}',{0}''\n", out->name.c_str());
            break;
        case irt::dynamics_type::qss3_integrator:
            fmt::print(
              out->ofs, "t,{0},{0}',{0}'',{0}'''\n", out->name.c_str());
            break;
        default:
            fmt::print(out->ofs, "t,{}\n", out->name.c_str());
            break;
        }
        break;

    case irt::observer::status::run:
    case irt::observer::status::finalize:
        switch (type) {
        case irt::dynamics_type::qss1_integrator:
            fmt::print(out->ofs, "{},{},{}\n", t, obs.msg[0], obs.msg[1]);
            break;
        case irt::dynamics_type::qss2_integrator:
            fmt::print(
              out->ofs, "{},{},{},{}\n", t, obs.msg[0], obs.msg[1], obs.msg[2]);
            break;
        case irt::dynamics_type::qss3_integrator:
            fmt::print(out->ofs,
                       "{},{},{},{},{}\n",
                       t,
                       obs.msg[0],
                       obs.msg[1],
                       obs.msg[2],
                       obs.msg[3]);
            break;
        default:
            fmt::print(out->ofs, "{},{}\n", t, obs.msg[0]);
            break;
        }
        break;
    }

    if (s == irt::observer::status::finalize)
        out->ofs.close();
}

static void
show_simulation_run(window_logger& /*log_w*/, editor& ed)
{
    ImGui::TextFormat("Current time {:.6f}", ed.simulation_current);
    ImGui::TextFormat("Current bag {}", ed.simulation_bag_id);
    ImGui::TextFormat("Next time {:.6f}", ed.simulation_next_time);
    ImGui::TextFormat("Model {}", (unsigned long)ed.sim.sched.size());

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
        ImGui::InputReal("Begin", &ed->simulation_begin);
        ImGui::InputReal("End", &ed->simulation_end);

        ImGui::Checkbox("Show values", &ed->simulation_show_value);

        if (ImGui::Button("Output files"))
            ed->show_select_directory_dialog = true;

        ImGui::Text("output directory:");
        ImGui::InputText("Path",
                         const_cast<char*>(reinterpret_cast<const char*>(
                           ed->observation_directory.u8string().c_str())),
                         ed->observation_directory.u8string().size(),
                         ImGuiInputTextFlags_ReadOnly);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        show_simulation_run(log_w, *ed);

        if (ed->st != editor_status::editing) {
            ImGui::TextFormat("Current: {:.6f}", ed->simulation_current);

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
