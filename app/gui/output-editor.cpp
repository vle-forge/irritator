// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"
#include "irritator/core.hpp"
#include "irritator/modeling.hpp"

#include <optional>

namespace irt {

constexpr static inline const char* plot_type_str[] = { "None",
                                                        "Plot line",
                                                        "Plot dot" };

static void show_observers_table(application& app, project_editor& ed) noexcept
{
    for (auto& vobs : ed.pj.variable_observers) {
        auto to_copy = std::optional<variable_observer::sub_id>();

        for (const auto id : vobs.subs) {
            const auto idx = get_index(id);
            ImGui::PushID(idx);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::InputFilteredString("##name",
                                       vobs.subs.template get<name_str>(id));
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::TextFormat("{}", ordinal(id));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("-");

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("-");

            ImGui::TableNextColumn();
            int plot_type =
              ordinal(vobs.subs.template get<plot_type_options>(id));
            if (ImGui::Combo("##plot",
                             &plot_type,
                             plot_type_str,
                             IM_ARRAYSIZE(plot_type_str)))
                vobs.subs.template get<plot_type_options>(id) =
                  enum_cast<plot_type_options>(plot_type);

            ImGui::TableNextColumn();
            const bool can_copy = app.copy_obs.can_alloc(1);
            ImGui::BeginDisabled(!can_copy);
            if (ImGui::Button("copy"))
                to_copy = id;
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("write"))
                app.output_ed.save_obs(app.pjs.get_id(ed),
                                       ed.pj.variable_observers.get_id(vobs),
                                       id);

            ImGui::PopID();
        }

        if (to_copy.has_value()) {
            const auto copy = *to_copy;

            const auto obs_id = vobs.subs.template get<observer_id>(copy);
            const auto obs    = ed.pj.sim.observers.try_to_get(obs_id);

            auto& new_obs = app.copy_obs.alloc();
            new_obs.name  = vobs.subs.template get<name_str>(copy).sv();

            obs->read_history(
              [&](const auto& lbuf, const auto /*version*/) noexcept {
                  new_obs.linear_outputs = lbuf;
              });
        }
    }
}

static void show_copy_table(application& app) noexcept
{
    auto to_del = std::optional<plot_copy_id>();

    for (auto& copy : app.copy_obs) {
        const auto id = app.copy_obs.get_id(copy);

        ImGui::PushID(&copy);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        ImGui::InputFilteredString("##name", copy.name);
        ImGui::PopItemWidth();

        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", ordinal(id));

        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");

        ImGui::TableNextColumn();
        ImGui::TextFormat("{}", copy.linear_outputs.size());

        ImGui::TableNextColumn();
        int plot_type = ordinal(copy.plot_type);
        if (ImGui::Combo(
              "##plot", &plot_type, plot_type_str, IM_ARRAYSIZE(plot_type_str)))
            copy.plot_type = enum_cast<simulation_plot_type>(plot_type);

        ImGui::TableNextColumn();

        if (ImGui::Button("del"))
            to_del = id;
        ImGui::SameLine();
        if (ImGui::Button("write"))
            app.output_ed.save_copy(id);

        ImGui::PopID();
    }

    if (to_del.has_value())
        app.copy_obs.free(*to_del);
}

static void show_observation_table(application&  app,
                                   bool&         display_all,
                                   vector<bool>& display_by_project) noexcept
{
    constexpr static auto flags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_Reorderable;

    if (ImGui::BeginChild("Table")) {
        if (ImGui::BeginTable("Observations", 6, flags)) {
            ImGui::TableSetupColumn(
              "name", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn(
              "id", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn(
              "time-step", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn(
              "size", ImGuiTableColumnFlags_WidthFixed, 60.f);
            ImGui::TableSetupColumn(
              "plot", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("actions",
                                    ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();

            if (display_all) {
                for (auto& pj : app.pjs) {
                    const auto id = app.pjs.get_id(pj);
                    ImGui::PushID(get_index(id));
                    show_observers_table(app, pj);
                    show_copy_table(app);
                    ImGui::PopID();
                }
            } else {
                for (auto& pj : app.pjs) {
                    const auto id  = app.pjs.get_id(pj);
                    const auto idx = get_index(id);

                    if (display_by_project[idx]) {
                        ImGui::PushID(idx);
                        show_observers_table(app, pj);
                        show_copy_table(app);
                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
}

static void write(project&                        pj,
                  std::ofstream&                  ofs,
                  const variable_observer&        vobs,
                  const variable_observer::sub_id id) noexcept
{
    const auto  obs_id = vobs.subs.template get<observer_id>(id);
    const auto* obs    = pj.sim.observers.try_to_get(obs_id);

    ofs.imbue(std::locale::classic());
    ofs << "t," << vobs.subs.template get<name_str>(id).sv() << '\n';

    obs->read_history([&](const auto& lbuf, const auto /*version*/) noexcept {
        for (const auto& v : lbuf)
            ofs << v.t << ',' << v.value << '\n';
    });
}

static void write(application&                    app,
                  project&                        pj,
                  std::ofstream&                  ofs,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id sub_id) noexcept
{
    ofs.imbue(std::locale::classic());

    if (const auto* vobs = pj.variable_observers.try_to_get(vobs_id);
        vobs and vobs->exists(sub_id))
        write(pj, ofs, *vobs, sub_id);
    else
        app.jn.push(log_level::error, [](auto& title, auto& msg) noexcept {
            title = "Output editor";
            msg   = "Unknown observation";
        });
}

static void write(application&                    app,
                  project&                        pj,
                  const std::filesystem::path&    file_path,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id obs_id) noexcept
{
    if (auto ofs = std::ofstream{ file_path }; ofs.is_open())
        write(app, pj, ofs, vobs_id, obs_id);
    else
        app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
            title = "Output editor";
            format(msg,
                   "Failed to open file `{}' to write observation",
                   file_path.string());
        });
}

static void write(std::ofstream& ofs, const plot_copy& p) noexcept
{
    ofs.imbue(std::locale::classic());

    ofs << "t," << p.name.sv() << '\n';

    for (auto& v : p.linear_outputs)
        ofs << v.t << ',' << v.value << '\n';
}

static void write(application&       app,
                  std::ofstream&     ofs,
                  const plot_copy_id id) noexcept
{
    if (auto* p = app.copy_obs.try_to_get(id); p)
        write(ofs, *p);
    else
        app.jn.push(log_level::error, [](auto& title, auto& msg) noexcept {
            title = "Output editor";
            msg   = "Unknown copy observation";
        });
}

static void write(application&                 app,
                  const std::filesystem::path& file_path,
                  const plot_copy_id           id) noexcept
{
    if (auto ofs = std::ofstream{ file_path }; ofs.is_open())
        write(app, ofs, id);
    else
        app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
            title = "Output editor";
            format(msg,
                   "Failed to open file `{}' to write observation",
                   file_path.string());
        });
}

output_editor::output_editor() noexcept
  : m_ctx{ ImPlot::CreateContext() }
{}

output_editor::~output_editor() noexcept
{
    if (m_ctx)
        ImPlot::DestroyContext(m_ctx);
}

void output_editor::show() noexcept
{
    if (!ImGui::Begin(
          output_editor::name, &is_open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::output_ed);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Selections")) {
            ImGui::MenuItem("Display all", nullptr, &m_display_all);

            for (const auto& pj : app.pjs) {
                const auto id  = app.pjs.get_id(pj);
                const auto idx = get_index(id);

                ImGui::MenuItem(
                  pj.title.c_str(), nullptr, &m_display_selected_project[idx]);
            }

            if (ImGui::MenuItem("clear")) {
                std::fill_n(m_display_selected_project.data(),
                            m_display_selected_project.size(),
                            false);

                m_display_all = false;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (ImGui::CollapsingHeader("Observations list"))
        show_observation_table(app, m_display_all, m_display_selected_project);

    if (ImGui::CollapsingHeader("Plots outputs",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImPlot::SetCurrentContext(app.output_ed.m_ctx);

        if (ImPlot::BeginPlot("Plots", ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupAxes(nullptr,
                              nullptr,
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);
            if (m_display_all == true) {
                for (auto& pj : app.pjs) {
                    const auto id = app.pjs.get_id(pj);
                    ImGui::PushID(get_index(id));

                    for (const auto& obs : pj.pj.sim.observers) {
                        const auto obs_id = pj.pj.sim.observers.get_id(obs);
                        const auto name   = format_n<32>(
                          "{}-{}", pj.pj.name.sv(), get_index(obs_id));

                        obs.read_history([&](const auto& h, const auto) {
                            app.plot_obs.show_plot_line(
                              obs, plot_type_options::line, name.c_str());
                        });
                    }

                    ImGui::PopID();
                }
            } else {
                for (auto& pj : app.pjs) {
                    const auto id  = app.pjs.get_id(pj);
                    const auto idx = get_index(id);

                    if (m_display_selected_project[idx]) {
                        ImGui::PushID(idx);
                        for (const auto& obs : pj.pj.sim.observers) {
                            const auto obs_id = pj.pj.sim.observers.get_id(obs);
                            const auto name   = format_n<32>(
                              "{}-{}", pj.pj.name.sv(), get_index(obs_id));

                            obs.read_history([&](const auto& h, const auto) {
                                app.plot_obs.show_plot_line(
                                  obs, plot_type_options::line, name.c_str());
                            });
                        }
                        ImGui::PopID();
                    }
                }
            }

            for (auto& p : app.copy_obs)
                if (p.plot_type != simulation_plot_type::none)
                    app.plot_copy_wgt.show_plot_line(p);

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }
    }

    ImGui::End();

    if (m_need_save != save_option::none) {
        const char*              title = "Select file path to save observation";
        const std::u8string_view default_filename = u8"example.txt";
        const char8_t* filters[] = { u8".txt", u8".dat", u8".csv", nullptr };

        ImGui::OpenPopup(title);
        if (app.f_dialog.show_save_file(title, default_filename, filters)) {
            if (app.f_dialog.state == file_dialog::status::ok) {
                m_file = app.f_dialog.result;
                if (m_need_save == save_option::copy) {
                    if (auto* pj = app.pjs.try_to_get(m_pj_id); pj)
                        write(app, m_file, m_copy_id);
                } else if (m_need_save == save_option::obs) {
                    if (auto* pj = app.pjs.try_to_get(m_pj_id); pj)
                        write(app, pj->pj, m_file, m_vobs_id, m_sub_id);
                }
            }

            app.f_dialog.clear();
            m_need_save = save_option::none;
        }
    }
}

void output_editor::save_obs(const project_id                pj_id,
                             const variable_observer_id      vobs,
                             const variable_observer::sub_id svobs) noexcept
{
    m_pj_id     = pj_id;
    m_vobs_id   = vobs;
    m_sub_id    = svobs;
    m_need_save = save_option::obs;
}

void output_editor::save_copy(const plot_copy_id id) noexcept
{
    m_copy_id   = id;
    m_need_save = save_option::copy;
}

void output_editor::add_project(const project_id id) noexcept
{
    debug::ensure(container_of(this, &application::output_ed).pjs.exists(id));

    const auto idx = get_index(id);

    if (idx >= m_display_selected_project.ssize())
        if (not m_display_selected_project.grow<2, 1>(1))
            return;

    m_display_selected_project[idx] = true;
}

void output_editor::remove_project(const project_id id) noexcept
{
    debug::ensure(container_of(this, &application::output_ed).pjs.exists(id));
    debug::ensure(get_index(id) < m_display_selected_project.ssize());

    const auto idx = get_index(id);

    m_display_selected_project[idx] = false;
}

} // namespace irt
