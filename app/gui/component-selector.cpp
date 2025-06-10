// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>

#include "application.hpp"
#include "imgui.h"
#include "internal.hpp"

namespace irt {

static void cs_make_selected_name(const std::string_view reg,
                                  const std::string_view dir,
                                  const std::string_view file,
                                  const component&       compo,
                                  const component_id     id,
                                  file_path_str&         name) noexcept
{
    if (compo.name.empty()) {
        format(name, "{}/{}/{} {}", reg, dir, file, ordinal(id));
    } else {
        format(name, "{}/{}/{} {}", reg, dir, file, compo.name.sv());
    }
}

static void update_lists(
  const modeling&                                 mod,
  vector<std::pair<component_id, name_str>>&      by_names,
  vector<std::pair<component_id, file_path_str>>& by_files,
  vector<std::pair<component_id, name_str>>&      by_generics,
  vector<std::pair<component_id, name_str>>&      by_grids,
  vector<std::pair<component_id, name_str>>&      by_graphs) noexcept
{
    by_names.clear();
    by_files.clear();
    by_generics.clear();
    by_grids.clear();
    by_graphs.clear();

    for_each_component(mod,
                       mod.component_repertories,
                       [&](const auto& reg,
                           const auto& dir,
                           const auto& file,
                           const auto& compo) noexcept {
                           const auto id = mod.components.get_id(compo);

                           by_names.emplace_back(id, compo.name.sv());
                           by_files.emplace_back(id, std::string_view());
                           cs_make_selected_name(reg.name.sv(),
                                                 dir.path.sv(),
                                                 file.path.sv(),
                                                 compo,
                                                 id,
                                                 by_files.back().second);

                           switch (compo.type) {
                           case component_type::none:
                               break;

                           case component_type::generic:
                               by_generics.emplace_back(id, compo.name.sv());
                               break;

                           case component_type::grid:
                               by_grids.emplace_back(id, compo.name.sv());
                               break;

                           case component_type::graph:
                               by_graphs.emplace_back(id, compo.name.sv());
                               break;
                           }
                       });

    auto sort_by_string = [](auto& vec) {
        std::sort(vec.begin(), vec.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.second.sv() < rhs.second.sv();
        });
    };

    sort_by_string(by_names);
    sort_by_string(by_files);
    sort_by_string(by_generics);
    sort_by_string(by_grids);
    sort_by_string(by_graphs);
}

void component_selector::update() noexcept
{
    // Skip the update if a thread is currently running an update.
    scoped_flag_run(updating, [&]() {
        const auto& app = container_of(this, &application::component_sel);
        const auto& mod = app.mod;

        update_lists(mod,
                     by_names_2nd,
                     by_files_2nd,
                     by_generics_2nd,
                     by_grids_2nd,
                     by_graphs_2nd);

        std::unique_lock lock{ m_mutex };
        swap_buffers();
    });
}

void component_selector::swap_buffers() noexcept
{
    by_names.swap(by_names_2nd);
    by_files.swap(by_files_2nd);
    by_generics.swap(by_generics_2nd);
    by_grids.swap(by_grids_2nd);
    by_graphs.swap(by_graphs_2nd);
}

component_selector::result_t component_selector::combobox(
  const char*        label,
  const component_id old_current) const noexcept
{
    auto id      = undefined<component_id>();
    auto is_done = false;

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        const auto& app = container_of(this, &application::component_sel);
        const char* current_name = "-";
        auto        current      = old_current;

        if (is_defined(current) and app.mod.components.exists(current)) {
            current_name =
              app.mod.components.get<component>(current).name.c_str();
        } else {
            current = undefined<component_id>();
        }

        if (ImGui::BeginCombo(label, current_name)) {
            ImGui::ColorButton(
              "Undefined color",
              to_ImVec4(app.config.colors[style_color::component_undefined]),
              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine(50.f);
            ImGui::PushID(-1);
            if (ImGui::Selectable(current_name, is_undefined(current))) {
                id      = undefined<component_id>();
                is_done = true;
            }
            ImGui::PopID();

            for (auto i = 0, e = by_names.ssize(); i != e; ++i) {
                ImGui::PushID(i);
                const auto col = get_component_color(app, by_names[i].first);
                const auto im  = ImVec4{ col[0], col[1], col[2], col[3] };
                ImGui::ColorButton("Component",
                                   im,
                                   ImGuiColorEditFlags_NoInputs |
                                     ImGuiColorEditFlags_NoLabel);
                ImGui::SameLine(50.f);
                if (ImGui::Selectable(by_names[i].second.c_str(),
                                      by_names[i].first == current)) {
                    id      = by_names[i].first;
                    is_done = true;
                }
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }
    }

    return component_selector::result_t{ .id = id, .is_done = is_done };
}

static auto display_menu(const char* title, const auto& vec) noexcept
{
    auto id   = undefined<component_id>();
    auto done = false;

    if (ImGui::BeginMenu(title)) {
        for (int i = 0, e = vec.ssize(); i != e; ++i) {
            ImGui::PushID(i);
            if (ImGui::MenuItem(vec[i].second.c_str())) {
                id   = vec[i].first;
                done = true;
            }
            ImGui::PopID();
        }

        ImGui::EndMenu();
    }

    return component_selector::result_t{ .id = id, .is_done = done };
}

component_selector::result_t component_selector::menu(
  const char* label) const noexcept
{
    component_selector::result_t ret{ .id      = undefined<component_id>(),
                                      .is_done = false };

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        if (ImGui::BeginMenu(label)) {
            const auto& app = container_of(this, &application::component_sel);

            ret = display_menu("Names", by_names);
            if (not ret) {
                ret = display_menu("Files", by_files);
                if (not ret) {
                    ret = display_menu("Generics", by_generics);
                    if (not ret) {
                        ret = display_menu("Graphs", by_graphs);
                        if (not ret) {
                            ret = display_menu("Grids", by_grids);
                        }
                    }
                }
            }
            ImGui::EndMenu();
        }
    }

    return ret;
}

} // namespace irt
