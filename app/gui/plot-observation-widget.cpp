// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "irritator/macros.hpp"

#include <irritator/format.hpp>
#include <irritator/helpers.hpp>

namespace irt {

void plot_observation_widget::init(application& /*app*/) noexcept
{
    // const auto len = app.pj.variable_observers.size();
    // clear();

    // observers.reserve(len);
    // ids.reserve(len);

    // for_each_data(app.pj.variable_observers, [&](auto& var) noexcept {
    //     const auto var_id = app.pj.variable_observers.get_id(var);

    //     if_data_exists_do(app.sim.models, var.mdl_id, [&](auto& mdl) noexcept
    //     {
    //         auto& obs =
    //           app.sim.observers.alloc(var.name.sv(), ordinal(var_id), 0);
    //         app.sim.observe(mdl, obs);

    //         observers.emplace_back(mdl.obs_id);
    //         ids.emplace_back(app.pj.variable_observers.get_id(var));
    //     });
    // });
}

void plot_observation_widget::clear() noexcept
{
    // observers.clear();
    // ids.clear();
}

void plot_observation_widget::show(application& app) noexcept
{
    for (auto& v_obs : app.pj.variable_observers) {
        const auto id  = app.pj.variable_observers.get_id(v_obs);
        const auto idx = get_index(id);

        small_string<32> name;
        format(name, "{}##{}", v_obs.name.sv(), idx);

        if (ImPlot::BeginPlot(name.c_str(), ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupAxes(nullptr,
                              nullptr,
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);

            v_obs.for_each([&](const auto id) noexcept {
                const auto idx    = get_index(id);
                const auto obs_id = v_obs.get_obs_ids()[idx];
                auto*      obs    = app.sim.observers.try_to_get(obs_id);

                if (obs->linearized_buffer.size() > 0) {
                    switch (v_obs.get_options()[idx]) {
                    case variable_observer::type_options::line:
                        ImPlot::PlotLineG(name.c_str(),
                                          ring_buffer_getter,
                                          &obs->linearized_buffer,
                                          obs->linearized_buffer.ssize());
                        break;

                    case variable_observer::type_options::dash:
                        ImPlot::PlotScatterG(name.c_str(),
                                             ring_buffer_getter,
                                             &obs->linearized_buffer,
                                             obs->linearized_buffer.ssize());
                        break;

                    default:
                        unreachable();
                    }
                }
            });

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }
}

static void plot_observation_widget_write(
  plot_observation_widget& /*plot_widget*/,
  application& /*app*/,
  std::ofstream& /*ofs*/) noexcept
{
    // ofs.imbue(std::locale::classic());

    // bool first_column = true;
    // int  size         = std::numeric_limits<int>::max();

    // for_specified_data(
    //   app.sim.observers, plot_widget.observers, [&](auto& obs) noexcept {
    //       if (first_column) {
    //           ofs << "t," << obs.name.sv();
    //           first_column = false;
    //       } else {
    //           ofs << "," << obs.name.sv();
    //       }

    //       size = std::min(size, obs.linearized_buffer.ssize());
    //   });

    // ofs << '\n';

    // for (int i = 0; i < size; ++i) {
    //     first_column = true;

    //     for_specified_data(
    //       app.sim.observers, plot_widget.observers, [&](auto& obs)
    //       noexcept {
    //           auto idx = obs.linearized_buffer.index_from_begin(i);
    //           if (first_column) {
    //               ofs << obs.linearized_buffer[idx].x << ","
    //                   << obs.linearized_buffer[idx].y;
    //               first_column = false;
    //           } else {
    //               ofs << "," << obs.linearized_buffer[idx].y;
    //           }
    //       });

    //     ofs << '\n';
    // }
}

static void notification_fail_open_file(application&                 app,
                                        const std::filesystem::path& file_path,
                                        std::string_view title) noexcept
{
    auto& n = app.notifications.alloc(log_level::error);
    n.title = title;
    format(
      n.message, "The file `{}` is not openable", file_path.generic_string());
    app.notifications.enable(n);
}

void plot_observation_widget::write(
  application&                 app,
  const std::filesystem::path& file_path) noexcept
{
    auto ofs = std::ofstream{ file_path };

    return ofs.is_open()
             ? plot_observation_widget_write(*this, app, ofs)
             : notification_fail_open_file(
                 app, file_path, "Fail to open plot observation file");
}

} // irt
