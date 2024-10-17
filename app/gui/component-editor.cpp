// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"

namespace irt {

template<typename T, typename Identifier>
constexpr T* find(data_array<T, Identifier>& data,
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
constexpr bool exist(data_array<T, Identifier>& data,
                     vector<Identifier>&        container,
                     std::string_view           name) noexcept
{
    return find(data, container, name) != nullptr;
}

static void add_extension(file_path_str& file) noexcept
{
    const std::decay_t<decltype(file)> tmp(file);

    if (auto dot = tmp.sv().find_last_of('.'); dot == std::string_view::npos) {
        format(file, "{}.irt", tmp.sv().substr(0, dot));
    } else {
        format(file, "{}.irt", tmp.sv());
    }
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

template<typename T>
concept has_store_function = requires(T t, component_editor& ed) {
    {
        t.store(ed)
    } -> std::same_as<void>;
};

struct component_editor::impl {
    application&      app;
    component_editor& ed;

    template<typename ComponentEditor>
    void show_file_access(application&     app,
                          ComponentEditor& ed,
                          component&       compo) noexcept
    {
        static constexpr const char* empty = "";

        auto* reg_dir = app.mod.registred_paths.try_to_get(compo.reg_path);
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
                    if (!exist(app.mod.dir_paths,
                               reg_dir->children,
                               dir_name.sv())) {
                        auto& new_dir = app.mod.dir_paths.alloc();
                        auto  dir_id  = app.mod.dir_paths.get_id(new_dir);
                        auto  reg_id = app.mod.registred_paths.get_id(*reg_dir);
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

                if (ImGui::InputFilteredString("File##text", file->path)) {
                    if (not end_with_irt(file->path.sv())) {
                        add_extension(file->path);
                    }
                }

                is_save_enabled = is_valid_irt_filename(file->path.sv());

                if (not app.mod.descriptions.exists(compo.desc)) {
                    if (app.mod.descriptions.can_alloc(1) &&
                        ImGui::Button("Add description")) {
                        compo.desc = app.mod.descriptions.alloc(
                          [](auto /*id*/, auto& str, auto& status) noexcept {
                              str.clear();
                              status = description_status::modified;
                          });
                    }
                } else {
                    constexpr ImGuiInputTextFlags flags =
                      ImGuiInputTextFlags_AllowTabInput;
                    auto& str = app.mod.descriptions.get<0>(compo.desc);

                    ImGui::InputSmallStringMultiline(
                      "##source",
                      str,
                      ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                      flags);

                    if (ImGui::Button("Remove")) {
                        app.mod.descriptions.free(compo.desc);
                        compo.desc = undefined<description_id>();
                    }
                }

                ImGui::BeginDisabled(!is_save_enabled);
                if (ImGui::Button("Save")) {
                    if constexpr (has_store_function<ComponentEditor>) {
                        ed.store(app.component_ed);
                    }
                    app.start_save_component(app.mod.components.get_id(compo));

                    app.add_gui_task(
                      [&]() noexcept { app.component_sel.update(); });
                }
                ImGui::EndDisabled();
            }
        }
    }

    void show_input_output_ports(component& compo) noexcept
    {
        if (ImGui::CollapsingHeader("X ports",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
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
                compo.x.for_each<port_str>([&](auto id, auto& name) noexcept {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormat("{}", ordinal(id));

                    ImGui::TableNextColumn();
                    ImGui::PushItemWidth(-1.f);
                    ImGui::PushID(ordinal(id));

                    ImGui::InputFilteredString("##in-name", name);

                    ImGui::PopID();
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();
                    if (ImGui::Button("del"))
                        to_del = id;
                });

                if (to_del.has_value())
                    compo.x.free(*to_del);

                ImGui::EndTable();

                if (compo.x.can_alloc(1) && ImGui::Button("+##in-port")) {
                    compo.x.alloc(
                      [&](auto /*id*/, auto& name, auto& pos) noexcept {
                          name = "-";
                          pos.reset();
                      });
                }
            }
        }

        if (ImGui::CollapsingHeader("Y ports",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
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
                compo.y.for_each(
                  [&](auto id, auto& name, auto& /*pos*/) noexcept {
                      ImGui::TableNextRow();
                      ImGui::TableNextColumn();
                      ImGui::TextFormat("{}", ordinal(id));

                      ImGui::TableNextColumn();
                      ImGui::PushItemWidth(-1.f);
                      ImGui::PushID(ordinal(id));

                      ImGui::InputFilteredString("##out-name", name);

                      ImGui::PopID();
                      ImGui::PopItemWidth();

                      ImGui::TableNextColumn();
                      if (ImGui::Button("del"))
                          to_del = id;
                  });

                if (to_del.has_value())
                    compo.y.free(*to_del);

                ImGui::EndTable();

                if (compo.y.can_alloc(1) && ImGui::Button("+##out-port")) {
                    compo.y.alloc(
                      [&](auto /*id*/, auto& name, auto& pos) noexcept {
                          name = "-";
                          pos.reset();
                      });
                }
            }
        }
    }

    template<typename ComponentEditor>
    void display_component_editor_subtable(ComponentEditor& element,
                                           component&       compo,
                                           ImGuiTableFlags  flags) noexcept
    {
        if (ImGui::BeginTable("##ed", 2, flags)) {
            ImGui::TableSetupColumn(
              "Component settings", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableSetupColumn(
              "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            auto copy_name = compo.name;
            if (ImGui::InputFilteredString("Name", copy_name)) {
                ed.request_to_open(app.mod.components.get_id(compo));
                compo.name = copy_name;
            }

            if (ImGui::BeginTabBar("Settings", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Save")) {
                    if (not ed.tabitem_open
                              [component_editor::tabitem_open_save]) {
                        element.clear_selected_nodes();
                        ed.tabitem_open.flip();
                    }

                    show_file_access(app, element, compo);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("In/Out")) {
                    if (not ed.tabitem_open
                              [component_editor::tabitem_open_in_out]) {
                        element.clear_selected_nodes();
                        ed.tabitem_open.flip();
                    }

                    if (compo.type == component_type::hsm) {
                        ImGui::TextWrapped(
                          "HSM component have four input and four output "
                          "ports. You can select input ports from the "
                          "state "
                          "conditions and the output ports from the state "
                          "actions.");
                    } else {
                        element.clear_selected_nodes();
                        show_input_output_ports(compo);
                    }
                    ImGui::EndTabItem();
                }

                const auto flags = element.need_show_selected_nodes(ed)
                                     ? ImGuiTabItemFlags_SetSelected
                                     : ImGuiTabItemFlags_None;

                if (ImGui::BeginTabItem("Specific", nullptr, flags)) {
                    ed.tabitem_open.reset();

                    if (ImGui::BeginChild("##zone",
                                          ImGui::GetContentRegionAvail())) {
                        element.show_selected_nodes(ed);
                    }
                    ImGui::EndChild();

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::TableSetColumnIndex(1);
            element.show(ed);

            ImGui::EndTable();
        }
    };

    template<typename T, typename ID>
    void display_component_editor(data_array<T, ID>& data,
                                  std::string_view /*title*/) noexcept
    {
        std::optional<ID> to_del;

        for (auto& element : data) {
            const auto compo_id = element.get_id();
            auto&      compo    = app.mod.components.get(compo_id);

            auto tab_item_flags = ImGuiTabItemFlags_None;
            format(ed.title, "{}##{}", compo.name.c_str(), get_index(compo_id));

            if (ed.need_to_open(compo_id)) {
                tab_item_flags = ImGuiTabItemFlags_SetSelected;
                ed.clear_request_to_open();
            }

            auto open = true;
            if (ImGui::BeginTabItem(ed.title.c_str(), &open, tab_item_flags)) {
                constexpr auto flags =
                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
                  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                  ImGuiTableFlags_Reorderable;

                if (compo.is_file_defined() and
                    ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
                    if constexpr (has_store_function<T>) {
                        element.store(ed);
                    }
                    app.start_save_component(compo_id);
                }

                display_component_editor_subtable(element, compo, flags);

                ImGui::EndTabItem();
            }

            if (open == false)
                to_del = data.get_id(element);
        }

        if (to_del.has_value())
            data.free(*to_del);
    }
};

void component_editor::display() noexcept
{
    if (!ImGui::Begin(component_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::component_ed);

    component_editor::impl impl{ .app = app, .ed = *this };

    if (ImGui::BeginTabBar("Editors")) {
        impl.display_component_editor(app.generics, "generic");
        impl.display_component_editor(app.grids, "grid");
        impl.display_component_editor(app.graphs, "graph");
        impl.display_component_editor(app.hsms, "hsm");
        ImGui::EndTabBar();
    }

    ImGui::End();
}

template<typename T, typename ID>
static void close_component(data_array<T, ID>& data,
                            const component_id id) noexcept
{
    auto it = std::find_if(
      data.begin(), data.end(), [&](const auto& d) noexcept -> bool {
          return id == d.get_id();
      });

    if (it != data.end())
        data.free(*it);
}

void component_editor::close(const component_id id) noexcept
{
    auto& app = container_of(this, &application::component_ed);

    if_data_exists_do(app.mod.components, id, [&](auto& compo) noexcept {
        switch (compo.type) {
        case component_type::simple:
            close_component(app.generics, id);
            break;
        case component_type::grid:
            close_component(app.grids, id);
            break;
        case component_type::graph:
            close_component(app.graphs, id);
            break;
        case component_type::hsm:
            close_component(app.hsms, id);
            break;
        default:
            break;
        }
    });
}

} // namespace irt
