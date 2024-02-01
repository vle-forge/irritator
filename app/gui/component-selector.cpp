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

static void cs_make_selected_name(std::string_view   reg,
                                  std::string_view   dir,
                                  std::string_view   file,
                                  component&         compo,
                                  component_id       id,
                                  small_string<254>& name) noexcept
{
    if (compo.name.empty()) {
        format(name, "{}/{}/{}/unamed {}", reg, dir, file, ordinal(id));
    } else {
        format(name, "{}/{}/{}/{}", reg, dir, file, compo.name.sv());
    }
}

static void cs_make_selected_name(std::string_view   component_name,
                                  small_string<254>& name) noexcept
{
    format(name, "internal {}", component_name);
}

static void cs_make_selected_name(std::string_view   component_name,
                                  component_id       id,
                                  small_string<254>& name) noexcept
{
    if (component_name.empty()) {
        format(name, "unamed {} (unsaved)", ordinal(id));
    } else {
        format(name, "{} (unsaved)", component_name);
    }
}

static void cs_select(modeling&          mod,
                      component_id       id,
                      small_string<254>& name) noexcept
{
    cs_make_selected_name(name);

    if (auto* compo = mod.components.try_to_get(id); compo) {
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

void component_selector::update() noexcept
{
    auto& app = container_of(this, &application::component_sel);
    auto& mod = app.mod;

    ids.clear();
    names.clear();

    files = 0;
    // internals = 0;
    unsaved = 0;

    ids.emplace_back(undefined<component_id>());
    cs_make_selected_name(names.emplace_back());

    for_each_component(
      mod,
      mod.component_repertories,
      [&](auto& reg, auto& dir, auto& file, auto& compo) noexcept {
          ids.emplace_back(file.component);
          auto& str = names.emplace_back();

          cs_make_selected_name(reg.name.sv(),
                                dir.path.sv(),
                                file.path.sv(),
                                compo,
                                file.component,
                                str);
      });

    files = ids.size();

    // for_each_data(mod.components, [&](auto& compo) noexcept {
    //     if (compo.type == component_type::internal) {
    //         ids.emplace_back(mod.components.get_id(compo));
    //         auto& str = names.emplace_back();

    //         cs_make_selected_name(
    //           internal_component_names[ordinal(compo.id.internal_id)], str);
    //     }
    // });

    // internals = ids.size();

    for_each_data(mod.components, [&](auto& compo) noexcept {
        if (compo.type != component_type::internal &&
            mod.file_paths.try_to_get(compo.file) == nullptr) {

            const auto id = mod.components.get_id(compo);
            ids.emplace_back(id);
            auto& str = names.emplace_back();

            cs_make_selected_name(compo.name.sv(), id, str);
        }
    });

    unsaved = ids.size();
}

bool component_selector::combobox(const char*   label,
                                  component_id* new_selected) noexcept
{
    auto& mod = container_of(this, &application::component_sel).mod;

    if (*new_selected != selected_id) {
        auto& app   = container_of(this, &application::component_sel);
        selected_id = *new_selected;

        cs_select(app.mod, selected_id, selected_name);
    }

    bool ret = false;

    if (ImGui::BeginCombo(label, selected_name.c_str())) {
        ImGui::ColorButton("Undefined color", to_ImVec4(black_color));
        ImGui::SameLine(50.f);
        if (ImGui::Selectable(names[0].c_str(), ids[0] == *new_selected)) {
            *new_selected = ids[0];
            ret           = true;
        }

        for (sz i = 1, e = ids.size(); i != e; ++i) {
            ImGui::ColorButton(
              "Component", to_ImVec4(mod.component_colors[get_index(ids[i])]));
            ImGui::SameLine(50.f);
            if (ImGui::Selectable(names[i].c_str(), ids[i] == *new_selected)) {
                *new_selected = ids[i];
                ret           = true;
            }
        }

        ImGui::EndCombo();
    }

    return ret;
}

bool component_selector::combobox(const char*   label,
                                  component_id* new_selected,
                                  bool*         hyphen) noexcept
{
    auto& mod = container_of(this, &application::component_sel).mod;

    bool ret = false;

    if (ImGui::BeginCombo(label, selected_name.c_str())) {
        ImGui::ColorButton("Undefined color", to_ImVec4(black_color));
        ImGui::SameLine(50.f);
        if (ImGui::Selectable(names[0].c_str(),
                              *hyphen == false && ids[0] == *new_selected)) {
            *new_selected = ids[0];
            *hyphen       = false;
            ret           = true;
        }

        for (sz i = 1, e = ids.size(); i != e; ++i) {
            ImGui::ColorButton(
              "#color", to_ImVec4(mod.component_colors[get_index(ids[i])]));
            ImGui::SameLine(50.f);
            if (ImGui::Selectable(names[i].c_str(),
                                  *hyphen == false &&
                                    ids[i] == *new_selected)) {
                *new_selected = ids[i];
                *hyphen       = false;
                ret           = true;
            }
        }

        if (ImGui::Selectable("-", *hyphen == true)) {
            ret = true;
        }

        ImGui::EndCombo();
    }

    return ret;
}

bool component_selector::menu(const char*   label,
                              component_id* new_selected) noexcept
{
    if (*new_selected != selected_id) {
        auto& app   = container_of(this, &application::component_sel);
        selected_id = *new_selected;

        cs_select(app.mod, selected_id, selected_name);
    }

    bool ret = false;

    if (ImGui::BeginMenu(label)) {
        if (ImGui::BeginMenu("Files components")) {
            for (int i = 0, e = files; i < e; ++i) {
                if (ImGui::MenuItem(names[i].c_str())) {
                    ret           = true;
                    *new_selected = ids[i];
                }
            }
            ImGui::EndMenu();
        }

        // if (ImGui::BeginMenu("Internal components")) {
        //     for (int i = files, e = internals; i < e; ++i) {
        //         if (ImGui::MenuItem(names[i].c_str())) {
        //             ret           = true;
        //             *new_selected = ids[i];
        //         }
        //     }
        //     ImGui::EndMenu();
        // }

        if (ImGui::BeginMenu("Unsaved components")) {
            for (int i = files, e = unsaved; i < e; ++i) {
                if (ImGui::MenuItem(names[i].c_str())) {
                    ret           = true;
                    *new_selected = ids[i];
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    return ret;
}

} // namespace irt