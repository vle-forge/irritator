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

static void cs_make_selected_name(small_string<254>& name) noexcept
{
    name = std::string_view("undefined");
}

static void cs_make_selected_name(const std::string_view reg,
                                  const std::string_view dir,
                                  const std::string_view file,
                                  const component&       compo,
                                  const component_id     id,
                                  small_string<254>&     name) noexcept
{
    if (compo.name.empty()) {
        format(name, "{}/{}/{}/unamed {}", reg, dir, file, ordinal(id));
    } else {
        format(name, "{}/{}/{}/{}", reg, dir, file, compo.name.sv());
    }
}

static void cs_make_selected_name(const std::string_view component_name,
                                  small_string<254>&     name) noexcept
{
    format(name, "internal {}", component_name);
}

static void cs_make_selected_name(const std::string_view component_name,
                                  const component_id     id,
                                  small_string<254>&     name) noexcept
{
    if (component_name.empty()) {
        format(name, "unamed {} (unsaved)", ordinal(id));
    } else {
        format(name, "{} (unsaved)", component_name);
    }
}

static void cs_select(const modeling&    mod,
                      const component_id id,
                      small_string<254>& name) noexcept
{
    cs_make_selected_name(name);

    if (auto* compo = mod.components.try_to_get<component>(id); compo) {
        if (compo->type == component_type::internal) {
            cs_make_selected_name(
              internal_component_names[ordinal(compo->id.internal_id)], name);
        } else {
            if (auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
                reg) {
                if (auto* dir = mod.dir_paths.try_to_get(compo->dir); dir) {
                    if (auto* file = mod.file_paths.try_to_get(compo->file);
                        file) {
                        cs_make_selected_name(reg->name.sv(),
                                              dir->path.sv(),
                                              file->path.sv(),
                                              *compo,
                                              id,
                                              name);
                    }
                }
            }

            cs_make_selected_name(compo->name.sv(), id, name);
        }
    }
}

struct update_t {
    int files;
    int unsaved;
};

static update_t update_lists(const modeling&            mod,
                             vector<component_id>&      ids,
                             vector<small_string<254>>& names) noexcept
{
    ids.clear();
    names.clear();

    ids.emplace_back(undefined<component_id>());
    cs_make_selected_name(names.emplace_back());

    for_each_component(mod,
                       mod.component_repertories,
                       [&](const auto& reg,
                           const auto& dir,
                           const auto& file,
                           const auto& compo) noexcept {
                           ids.emplace_back(file.component);
                           auto& str = names.emplace_back();

                           cs_make_selected_name(reg.name.sv(),
                                                 dir.path.sv(),
                                                 file.path.sv(),
                                                 compo,
                                                 file.component,
                                                 str);
                       });

    const auto saved = ids.ssize();

    const auto& vec = mod.components.get<component>();
    for (const auto id : mod.components) {
        const auto& compo = vec[get_index(id)];

        if (compo.type != component_type::internal &&
            mod.file_paths.try_to_get(compo.file) == nullptr) {
            ids.emplace_back(id);
            auto& str = names.emplace_back();

            cs_make_selected_name(compo.name.sv(), id, str);
        }
    }

    const auto unsaved = ids.ssize() - saved;

    return update_t{ .files = saved, .unsaved = unsaved };
}

void component_selector::update() noexcept
{
    // Skip the update if a thread is currently running an update.
    scoped_flag_run(updating, [&]() {
        const auto& app = container_of(this, &application::component_sel);
        const auto& mod = app.mod;

        auto ret = update_lists(mod, ids_2nd, names_2nd);

        std::unique_lock lock{ m_mutex };
        ids.swap(ids_2nd);
        names.swap(names_2nd);
        files   = ret.files;
        unsaved = ret.unsaved;
    });
}

bool component_selector::combobox(const char*   label,
                                  component_id* new_selected) const noexcept
{
    bool ret = false;

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        const auto&       app = container_of(this, &application::component_sel);
        small_string<254> selected_name("undefined");

        cs_select(app.mod, *new_selected, selected_name);

        if (ImGui::BeginCombo(label, selected_name.c_str())) {
            ImGui::ColorButton(
              "Undefined color",
              to_ImVec4(app.config.colors[style_color::component_undefined]),
              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine(50.f);
            ImGui::PushID(0);
            if (ImGui::Selectable(names[0].c_str(), ids[0] == *new_selected)) {
                *new_selected = ids[0];
                ret           = true;
            }
            ImGui::PopID();

            for (sz i = 1, e = ids.size(); i != e; ++i) {
                ImGui::PushID(i);
                const auto col = get_component_color(app, ids[i]);
                const auto im  = ImVec4{ col[0], col[1], col[2], col[3] };
                ImGui::ColorButton("Component",
                                   im,
                                   ImGuiColorEditFlags_NoInputs |
                                     ImGuiColorEditFlags_NoLabel);
                ImGui::SameLine(50.f);
                if (ImGui::Selectable(names[i].c_str(),
                                      ids[i] == *new_selected)) {
                    *new_selected = ids[i];
                    ret           = true;
                }
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }
    }

    return ret;
}

bool component_selector::combobox(const char*   label,
                                  component_id* new_selected,
                                  bool*         hyphen) const noexcept
{
    bool ret = false;

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        const auto&       app = container_of(this, &application::component_sel);
        small_string<254> selected_name("undefined");

        cs_select(app.mod, *new_selected, selected_name);

        if (ImGui::BeginCombo(label, selected_name.c_str())) {
            ImGui::ColorButton(
              "Undefined color",
              to_ImVec4(app.config.colors[style_color::component_undefined]),
              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine(50.f);
            ImGui::PushID(0);
            if (ImGui::Selectable(names[0].c_str(),
                                  *hyphen == false &&
                                    ids[0] == *new_selected)) {
                *new_selected = ids[0];
                *hyphen       = false;
                ret           = true;
            }
            ImGui::PopID();

            for (sz i = 1, e = ids.size(); i != e; ++i) {
                ImGui::PushID(i);
                const auto col = get_component_color(app, ids[i]);
                const auto im  = ImVec4(col[0], col[1], col[2], col[3]);

                ImGui::ColorButton("#color",
                                   im,
                                   ImGuiColorEditFlags_NoInputs |
                                     ImGuiColorEditFlags_NoLabel);
                ImGui::SameLine(50.f);
                if (ImGui::Selectable(names[i].c_str(),
                                      *hyphen == false &&
                                        ids[i] == *new_selected)) {
                    *new_selected = ids[i];
                    *hyphen       = false;
                    ret           = true;
                }
                ImGui::PopID();
            }

            if (ImGui::Selectable("-", *hyphen == true)) {
                ret = true;
            }

            ImGui::EndCombo();
        }
    }

    return ret;
}

bool component_selector::menu(const char*   label,
                              component_id* new_selected) const noexcept
{
    auto ret = false;

    if (std::shared_lock lock(m_mutex, std::try_to_lock); lock.owns_lock()) {
        if (ImGui::BeginMenu(label)) {
            const auto& app = container_of(this, &application::component_sel);
            small_string<254> selected_name("undefined");

            cs_select(app.mod, *new_selected, selected_name);
            int push_id = 0;

            if (ImGui::BeginMenu("Files components")) {
                ImGui::PushID(push_id++);
                for (int i = 0, e = files; i < e; ++i) {
                    if (ImGui::MenuItem(names[i].c_str())) {
                        ret           = true;
                        *new_selected = ids[i];
                    }
                }
                ImGui::PopID();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Unsaved components")) {
                ImGui::PushID(push_id++);
                for (int i = files, e = unsaved; i < e; ++i) {
                    if (ImGui::MenuItem(names[i].c_str())) {
                        ret           = true;
                        *new_selected = ids[i];
                    }
                }
                ImGui::PopID();
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
    }

    return ret;
}

} // namespace irt
