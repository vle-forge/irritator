// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"
#include "irritator/helpers.hpp"

#include <cstring>

namespace irt {

template<typename T, typename Identifier>
T* find(data_array<T, Identifier>& data,
        vector<Identifier>&        container,
        std::string_view           name) noexcept
{
    int i = 0;
    while (i < container.ssize()) {
        auto  test_id = container[i];
        auto* test    = data.try_to_get(test_id);

        if (test) {
            if (test->path.sv() == name)
                return test;

            ++i;
        } else {
            container.swap_pop_back(i);
        }
    }

    return nullptr;
}

template<typename T, typename Identifier>
bool exist(data_array<T, Identifier>& data,
           vector<Identifier>&        container,
           std::string_view           name) noexcept
{
    return find(data, container, name) != nullptr;
}

void add_extension(file_path& file) noexcept
{
    const auto          sv = file.path.sv();
    decltype(file.path) tmp;

    if (auto last = sv.find_last_of('.'); last == std::string_view::npos) {
        debug::ensure(last != 0);
        tmp.assign(sv.substr(0, last - 1));
    } else {
        tmp.assign(sv);
    }

    format(file.path, "{}.irt", tmp.sv());
}

static bool all_char_valid(std::string_view v) noexcept
{
    for (auto c : v)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;

    return true;
}

static bool end_with_irt(std::string_view v) noexcept
{
    return v.ends_with(".irt");
}

static bool is_valid_irt_filename(std::string_view v) noexcept
{
    return !v.empty() && v[0] != '.' && v[0] != '-' && all_char_valid(v) &&
           end_with_irt(v);
}

static void show_file_access(application& app, component& compo) noexcept
{
    static constexpr const char* empty = "";

    auto*       reg_dir = app.mod.registred_paths.try_to_get(compo.reg_path);
    const char* reg_preview     = reg_dir ? reg_dir->path.c_str() : empty;
    bool        is_save_enabled = false;

    if (ImGui::BeginCombo("Path", reg_preview)) {
        registred_path* list = nullptr;
        while (app.mod.registred_paths.next(list)) {
            if (list->status == registred_path::state::error)
                continue;

            if (ImGui::Selectable(list->path.c_str(),
                                  reg_dir == list,
                                  ImGuiSelectableFlags_None)) {
                compo.reg_path = app.mod.registred_paths.get_id(list);
                reg_dir        = list;
            }
        }
        ImGui::EndCombo();
    }

    if (reg_dir) {
        auto* dir         = app.mod.dir_paths.try_to_get(compo.dir);
        auto* dir_preview = dir ? dir->path.c_str() : empty;

        if (ImGui::BeginCombo("Dir", dir_preview)) {
            if (ImGui::Selectable("##empty-dir", dir == nullptr)) {
                compo.dir = undefined<dir_path_id>();
                dir       = nullptr;
            }

            dir_path* list = nullptr;
            while (app.mod.dir_paths.next(list)) {
                if (ImGui::Selectable(list->path.c_str(), dir == list)) {
                    compo.dir = app.mod.dir_paths.get_id(list);
                    dir       = list;
                }
            }
            ImGui::EndCombo();
        }

        if (dir == nullptr) {
            directory_path_str dir_name;

            if (ImGui::InputFilteredString("New dir.##dir", dir_name)) {
                if (!exist(
                      app.mod.dir_paths, reg_dir->children, dir_name.sv())) {
                    auto& new_dir  = app.mod.dir_paths.alloc();
                    auto  dir_id   = app.mod.dir_paths.get_id(new_dir);
                    auto  reg_id   = app.mod.registred_paths.get_id(*reg_dir);
                    new_dir.parent = reg_id;
                    new_dir.path   = dir_name;
                    new_dir.status = dir_path::state::unread;
                    reg_dir->children.emplace_back(dir_id);
                    compo.reg_path = reg_id;
                    compo.dir      = dir_id;

                    if (!app.mod.create_directories(new_dir)) {
                        log_w(app,
                              log_level::error,
                              "Fail to create directory `%.*s'",
                              new_dir.path.ssize(),
                              new_dir.path.begin());
                    }
                }
            }
        }

        if (dir) {
            auto* file = app.mod.file_paths.try_to_get(compo.file);
            if (!file) {
                auto& f     = app.mod.file_paths.alloc();
                auto  id    = app.mod.file_paths.get_id(f);
                f.component = app.mod.components.get_id(compo);
                f.parent    = app.mod.dir_paths.get_id(*dir);
                compo.file  = id;
                dir->children.emplace_back(id);
                file = &f;
            }

            if (ImGui::InputFilteredString("File##text", file->path))
                if (!end_with_irt(file->path.sv()))
                    add_extension(*file);

            is_save_enabled = is_valid_irt_filename(file->path.sv());

            auto* desc = app.mod.descriptions.try_to_get(compo.desc);
            if (!desc) {
                if (app.mod.descriptions.can_alloc(1) &&
                    ImGui::Button("Add description")) {
                    auto& new_desc = app.mod.descriptions.alloc();
                    compo.desc     = app.mod.descriptions.get_id(new_desc);
                }
            } else {
                constexpr ImGuiInputTextFlags flags =
                  ImGuiInputTextFlags_AllowTabInput;

                ImGui::InputSmallStringMultiline(
                  "##source",
                  desc->data,
                  ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                  flags);

                if (ImGui::Button("Remove")) {
                    app.mod.descriptions.free(*desc);
                    compo.desc = undefined<description_id>();
                }
            }

            ImGui::BeginDisabled(!is_save_enabled);
            if (ImGui::Button("Save"))
                app.start_save_component(app.mod.components.get_id(compo));
            ImGui::EndDisabled();
        }
    }
}

static void show_input_output_ports(component& compo) noexcept
{
    if (ImGui::CollapsingHeader("X ports")) {
        if (ImGui::BeginTable("X",
                              3,
                              ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_NoSavedSettings |
                                ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn(
              "id", ImGuiTableColumnFlags_WidthFixed, 32.f);
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("action");
            ImGui::TableHeadersRow();

            std::optional<port_id> to_del;
            for (const auto id : compo.x) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", ordinal(id));

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1.f);
                ImGui::PushID(ordinal(id));

                ImGui::InputFilteredString("##in-name",
                                           compo.x_names[get_index(id)]);

                ImGui::PopID();
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();
                if (ImGui::Button("del"))
                    to_del = id;
            }

            if (to_del.has_value())
                compo.x.free(*to_del);

            ImGui::EndTable();

            if (compo.x.can_alloc(1) && ImGui::Button("+##in-port")) {
                const auto id                = compo.x.alloc();
                compo.x_names[get_index(id)] = "-";
            }
        }
    }

    if (ImGui::CollapsingHeader("Y ports")) {
        if (ImGui::BeginTable("Y",
                              3,
                              ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_NoSavedSettings |
                                ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn(
              "id", ImGuiTableColumnFlags_WidthFixed, 32.f);
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("action");
            ImGui::TableHeadersRow();

            std::optional<port_id> to_del;
            for (const auto id : compo.y) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextFormat("{}", ordinal(id));

                ImGui::TableNextColumn();
                ImGui::PushItemWidth(-1.f);
                ImGui::PushID(ordinal(id));

                ImGui::InputFilteredString("##out-name",
                                           compo.y_names[get_index(id)]);

                ImGui::PopID();
                ImGui::PopItemWidth();

                ImGui::TableNextColumn();
                if (ImGui::Button("del"))
                    to_del = id;
            }

            if (to_del.has_value())
                compo.y.free(*to_del);

            ImGui::EndTable();

            if (compo.y.can_alloc(1) && ImGui::Button("+##out-port")) {
                const auto id                = compo.y.alloc();
                compo.y_names[get_index(id)] = "-";
            }
        }
    }
}

template<typename T, typename ID>
static void show_data(application&       app,
                      component_editor&  ed,
                      data_array<T, ID>& data,
                      std::string_view /*title*/) noexcept
{
    T* del     = nullptr;
    T* element = nullptr;

    while (data.next(element)) {
        if (del) {
            data.free(*del);
            del = nullptr;
        }

        auto tab_item_flags = ImGuiTabItemFlags_None;
        if (auto* c = app.mod.components.try_to_get(element->get_id()); c) {
            format(ed.title,
                   "{}##{}",
                   c->name.c_str(),
                   get_index(app.mod.components.get_id(c)));

            if (ed.need_to_open(app.mod.components.get_id(c))) {
                tab_item_flags = ImGuiTabItemFlags_SetSelected;
                ed.clear_request_to_open();
            }

            bool open = true;
            if (ImGui::BeginTabItem(ed.title.c_str(), &open, tab_item_flags)) {
                static ImGuiTableFlags flags =
                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
                  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                  ImGuiTableFlags_Reorderable;

                if (ImGui::BeginTable("##ed", 2, flags)) {
                    ImGui::TableSetupColumn(
                      "Settings", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                    ImGui::TableSetupColumn(
                      "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);
                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    auto copy_name = c->name;
                    if (ImGui::InputFilteredString("Name", copy_name))
                        c->name = copy_name;

                    if (ImGui::CollapsingHeader("path"))
                        show_file_access(app, *c);

                    show_input_output_ports(*c);

                    if (ImGui::CollapsingHeader("selected"))
                        element->show_selected_nodes(ed);

                    ImGui::TableSetColumnIndex(1);
                    element->show(ed);

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (!open) {
                del = element;
            }
        } else {
            del = element;
        }
    }

    if (del)
        data.free(*del);
}

void component_editor::show() noexcept
{
    if (!ImGui::Begin(component_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::component_ed);

    if (ImGui::BeginTabBar("Editors")) {
        show_data(app, *this, app.generics, "generic");
        show_data(app, *this, app.grids, "grid");
        show_data(app, *this, app.graphs, "graph");
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace irt
