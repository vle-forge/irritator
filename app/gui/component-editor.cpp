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

template<typename T>
concept has_store_function = requires(T t, component_editor& ed) {
    { t.store(ed) } -> std::same_as<void>;
};

struct component_editor::impl {
    application&      app;
    component_editor& ed;

    void display_external_source(component& compo) noexcept
    {
        const auto button_size = ImGui::ComputeButtonSize(4);

        if (not compo.srcs.constant_sources.can_alloc(1))
            compo.srcs.constant_sources.grow<3, 2>();
        if (not compo.srcs.binary_file_sources.can_alloc(1))
            compo.srcs.binary_file_sources.grow<3, 2>();
        if (not compo.srcs.text_file_sources.can_alloc(1))
            compo.srcs.text_file_sources.grow<3, 2>();
        if (not compo.srcs.random_sources.can_alloc(1))
            compo.srcs.random_sources.grow<3, 2>();

        if (compo.srcs.constant_sources.can_alloc(1) and
            ImGui::Button("+constant", button_size)) {
            (void)compo.srcs.constant_sources.alloc();
        }

        ImGui::SameLine();

        if (compo.srcs.binary_file_sources.can_alloc(1) and
            ImGui::Button("+binary", button_size)) {
            (void)compo.srcs.binary_file_sources.alloc();
        }

        ImGui::SameLine();

        if (compo.srcs.text_file_sources.can_alloc(1) and
            ImGui::Button("+text", button_size)) {
            (void)compo.srcs.text_file_sources.alloc();
        }

        ImGui::SameLine();

        if (compo.srcs.random_sources.can_alloc(1) and
            ImGui::Button("+random", button_size)) {
            (void)compo.srcs.random_sources.alloc();
        }

        if (ImGui::TreeNode("Constants")) {
            static decltype(constant_source::name) name_tmp;

            for (auto& s : compo.srcs.constant_sources) {
                const auto id = compo.srcs.constant_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNode(s.name.c_str())) {
                    ImGui::LabelFormat("id", "{}", ordinal(id));
                    ImGui::LabelFormat("name", "{}", s.name.sv());
                    ImGui::LabelFormat("value",
                                       "{} {} {}...",
                                       s.buffer[0],
                                       s.buffer[1],
                                       s.buffer[2]);

                    if (ImGui::Button("Edit")) {
                        ImGui::OpenPopup("Change constant values");
                        name_tmp = s.name;
                    }

                    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                    ImGui::SetNextWindowPos(
                      center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                    if (ImGui::BeginPopupModal(
                          "Change constant values",
                          NULL,
                          ImGuiWindowFlags_AlwaysAutoResize)) {

                        ImGui::InputFilteredString("name", name_tmp);

                        u32 len = s.length;
                        if (ImGui::InputScalar(
                              "length", ImGuiDataType_U32, &len)) {
                            if (len > 0 and len < external_source_chunk_size)
                                s.length = len;
                        }

                        s.length          = s.length == 0 ? 1 : s.length;
                        const int columns = 4 < s.length ? 4 : s.length;
                        const int rows    = (s.length / columns) +
                                         ((s.length % columns) > 0 ? 1 : 0);
                        if (ImGui::BeginChild(
                              "##zone",
                              ImVec2(ImGui::GetContentRegionAvail().x,
                                     200.f))) {

                            if (ImGui::BeginTable("Values", columns)) {
                                for (int j = 0; j < columns; ++j)
                                    ImGui::TableSetupColumn(
                                      "",
                                      ImGuiTableColumnFlags_WidthFixed,
                                      60.f);

                                for (int i = 0; i < rows; ++i) {
                                    ImGui::TableNextRow();
                                    for (int j = 0; j < columns; ++j) {
                                        ImGui::TableSetColumnIndex(j);
                                        ImGui::PushID(i * columns + j);
                                        ImGui::PushItemWidth(-1);
                                        ImGui::InputDouble("",
                                                           s.buffer.data() +
                                                             (i * columns + j));
                                        ImGui::PopItemWidth();
                                        ImGui::PopID();
                                    }
                                }
                                ImGui::EndTable();
                            }
                        }
                        ImGui::EndChild();

                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                            s.name = name_tmp;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SetItemDefaultFocus();
                        ImGui::EndPopup();
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Binary files")) {
            static decltype(constant_source::name) name_tmp;

            for (auto& s : compo.srcs.binary_file_sources) {
                const auto id = compo.srcs.binary_file_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNode(s.name.c_str())) {
                    ImGui::LabelFormat("id", "{}", ordinal(id));
                    ImGui::LabelFormat("name", "{}", s.name.sv());
                    ImGui::LabelFormat(
                      "path", "{}", (const char*)s.file_path.c_str());

                    if (ImGui::Button("Edit")) {
                        ImGui::OpenPopup("Change binary values");
                        name_tmp = s.name;
                    }

                    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                    ImGui::SetNextWindowPos(
                      center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                    if (ImGui::BeginPopupModal(
                          "Change binary values",
                          NULL,
                          ImGuiWindowFlags_AlwaysAutoResize)) {

                        ImGui::InputFilteredString("name", name_tmp);
                        show_data_file_input(app.mod, compo, s.file_id);

                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                            s.name = name_tmp;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SetItemDefaultFocus();
                        ImGui::EndPopup();
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Text files")) {
            static decltype(constant_source::name) name_tmp;

            for (auto& s : compo.srcs.text_file_sources) {
                const auto id = compo.srcs.text_file_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNode(s.name.c_str())) {
                    ImGui::LabelFormat("id", "{}", ordinal(id));
                    ImGui::LabelFormat("name", "{}", s.name.sv());
                    ImGui::LabelFormat(
                      "path", "{}", (const char*)s.file_path.c_str());

                    if (ImGui::Button("Edit")) {
                        ImGui::OpenPopup("Change binary values");
                        name_tmp = s.name;
                    }

                    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                    ImGui::SetNextWindowPos(
                      center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                    if (ImGui::BeginPopupModal(
                          "Change binary values",
                          NULL,
                          ImGuiWindowFlags_AlwaysAutoResize)) {

                        ImGui::InputFilteredString("name", name_tmp);
                        show_data_file_input(app.mod, compo, s.file_id);

                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                            s.name = name_tmp;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SetItemDefaultFocus();
                        ImGui::EndPopup();
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Random")) {
            static decltype(constant_source::name) name_tmp;

            for (auto& s : compo.srcs.random_sources) {
                const auto id = compo.srcs.random_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNode(s.name.c_str())) {
                    ImGui::LabelFormat("id", "{}", ordinal(id));
                    ImGui::LabelFormat("name", "{}", s.name.sv());
                    ImGui::LabelFormat(
                      "name",
                      "{}",
                      distribution_type_string[ordinal(s.distribution)]);

                    if (ImGui::Button("Edit")) {
                        ImGui::OpenPopup("Change constant values");
                        name_tmp = s.name;
                    }

                    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                    ImGui::SetNextWindowPos(
                      center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                    if (ImGui::BeginPopupModal(
                          "Change constant values",
                          NULL,
                          ImGuiWindowFlags_AlwaysAutoResize)) {

                        ImGui::InputFilteredString("name", name_tmp);
                        show_random_distribution_input(s);

                        if (ImGui::Button("OK", ImVec2(120, 0))) {
                            s.name = name_tmp;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SetItemDefaultFocus();
                        ImGui::EndPopup();
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

    template<typename ComponentEditor>
    void show_file_access(application&     app,
                          ComponentEditor& ed,
                          component&       compo) noexcept
    {
        static constexpr const char* empty = "";

        auto* reg_dir = app.mod.registred_paths.try_to_get(compo.reg_path);
        const char* reg_preview = reg_dir ? reg_dir->path.c_str() : empty;

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
                    if (not file->path.sv().ends_with(".irt")) {
                        add_extension(file->path);
                    }
                }

                const auto is_save_enabled =
                  is_valid_irt_filename(file->path.sv());
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

    port_id show_input_port(component& gcompo, port_id g_port_id) noexcept
    {
        const auto* c_str = gcompo.x.exists(g_port_id)
                              ? gcompo.x.get<port_str>(g_port_id).c_str()
                              : "-";

        if (ImGui::BeginCombo("Input ports", c_str)) {
            if (ImGui::Selectable("-", is_undefined(g_port_id))) {
                g_port_id = undefined<port_id>();
            }

            const auto& names = gcompo.x.get<port_str>();
            for (const auto id : gcompo.x) {
                const auto idx = get_index(id);

                if (ImGui::Selectable(names[idx].c_str(), g_port_id == id)) {
                    g_port_id = id;
                }
            }

            ImGui::EndCombo();
        }

        return g_port_id;
    }

    port_id show_output_port(component& gcompo, port_id g_port_id) noexcept
    {
        const auto* c_str = gcompo.y.exists(g_port_id)
                              ? gcompo.y.get<port_str>(g_port_id).c_str()
                              : "-";

        if (ImGui::BeginCombo("Output ports", c_str)) {
            if (ImGui::Selectable("-", is_undefined(g_port_id))) {
                g_port_id = undefined<port_id>();
            }

            const auto& names = gcompo.y.get<port_str>();
            for (const auto id : gcompo.y) {
                const auto idx = get_index(id);

                if (ImGui::Selectable(names[idx].c_str(), g_port_id == id)) {
                    g_port_id = id;
                }
            }

            ImGui::EndCombo();
        }

        return g_port_id;
    }

    child_id show_node_selection(ImGuiTextFilter&   filter,
                                 generic_component& g,
                                 child_id           selected) noexcept
    {
        static std::string temp;

        temp = g.children.try_to_get(selected)
                 ? g.children_names[get_index(selected)].sv()
                 : "-";

        if (ImGui::BeginCombo("Child", temp.c_str())) {
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
                filter.Clear();
                selected = undefined<child_id>();
            }

            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
            filter.Draw("##Filter", -FLT_MIN);

            if (ImGui::Selectable("-", is_undefined(selected))) {
                selected = undefined<child_id>();
            }

            for (const auto& elem : g.children) {
                const auto id  = g.children.get_id(elem);
                const auto idx = get_index(id);
                ImGui::PushID(&elem);

                if (not g.children_names[idx].empty()) {
                    temp = g.children_names[idx].sv();

                    if (filter.PassFilter(temp.c_str())) {
                        ImGui::PushID(idx);
                        if (ImGui::Selectable(temp.c_str(), selected == id)) {
                            selected = id;
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return selected;
    }

    graph_node_id show_node_selection(ImGuiTextFilter& filter,
                                      graph_component& graph,
                                      graph_node_id    selected) noexcept
    {
        static std::string temp;

        temp = graph.g.nodes.exists(selected)
                 ? graph.g.node_ids[get_index(selected)]
                 : "-";

        if (ImGui::BeginCombo("Node", temp.c_str())) {

            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
                filter.Clear();
                selected = undefined<graph_node_id>();
            }

            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
            filter.Draw("##Filter", -FLT_MIN);

            if (ImGui::Selectable("-", is_undefined(selected))) {
                selected = undefined<graph_node_id>();
            }

            for (const auto id : graph.g.nodes) {
                const auto idx = get_index(id);
                ImGui::PushID(idx);

                if (not graph.g.node_ids[idx].empty()) {
                    temp = graph.g.node_ids[idx];

                    if (filter.PassFilter(temp.c_str())) {
                        ImGui::PushID(idx);
                        if (ImGui::Selectable(temp.c_str(), selected == id)) {
                            selected = id;
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        return selected;
    }

    void show_input_connections_new(grid_component& grid,
                                    component&      gcompo) noexcept
    {
        static port_id     g_port_id  = undefined<port_id>();
        static int         selected   = 0;
        static port_id     p_selected = undefined<port_id>();
        static std::string temp;

        g_port_id          = show_input_port(gcompo, g_port_id);
        const auto row_col = grid.pos(selected);
        int        v[2]    = { row_col.first, row_col.second };

        if (is_defined(g_port_id)) {
            if (ImGui::SliderInt("Row", &v[0], 0, grid.row())) {
                if (v[0] < 0)
                    v[0] = 0;
                else if (v[0] >= grid.row())
                    v[0] = grid.row() - 1;
            }

            if (ImGui::SliderInt("Column", &v[1], 0, grid.column())) {
                if (v[1] < 0)
                    v[1] = 0;
                else if (v[1] >= grid.column())
                    v[1] = grid.column() - 1;
            }

            const auto new_selected = grid.pos(v[0], v[1]);
            if (new_selected != selected) {
                selected   = new_selected;
                p_selected = undefined<port_id>();
            }
        }

        if (grid.is_coord_valid(v[0], v[1])) {
            const auto ch_id = grid.children()[selected];

            if (auto* ch = app.mod.components.try_to_get(ch_id)) {
                temp = ch->x.exists(p_selected)
                         ? ch->x.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected = undefined<port_id>();
                    }

                    for (const auto id : ch->x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(ch->x.get<port_str>(id).c_str(),
                                              p_selected == id)) {
                            p_selected = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(p_selected)) {

            if (ImGui::Button("+")) {
                const auto [row, col] = grid.pos(selected);
                if (auto ret =
                      grid.connect_input(g_port_id, row, col, p_selected);
                    not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add input connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_input_connection_already_exists))
                              m = "Input connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_input_connection_container_full))
                              m = "Not enough memory to allocate more input "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id  = undefined<port_id>();
                    selected   = 0;
                    p_selected = undefined<port_id>();
                }
            }
        }
    }

    void show_output_connections_new(grid_component& grid,
                                     component&      gcompo) noexcept
    {
        static port_id     g_port_id  = undefined<port_id>();
        static int         selected   = 0;
        static port_id     p_selected = undefined<port_id>();
        static std::string temp;

        g_port_id          = show_output_port(gcompo, g_port_id);
        const auto row_col = grid.pos(selected);
        int        v[2]    = { row_col.first, row_col.second };

        if (is_defined(g_port_id)) {
            if (ImGui::SliderInt("Row", &v[0], 0, grid.row())) {
                if (v[0] < 0)
                    v[0] = 0;
                else if (v[0] >= grid.row())
                    v[0] = grid.row() - 1;
            }

            if (ImGui::SliderInt("Column", &v[1], 0, grid.column())) {
                if (v[1] < 0)
                    v[1] = 0;
                else if (v[1] >= grid.column())
                    v[1] = grid.column() - 1;
            }

            const auto new_selected = grid.pos(v[0], v[1]);
            if (new_selected != selected) {
                selected   = new_selected;
                p_selected = undefined<port_id>();
            }
        }

        if (grid.is_coord_valid(v[0], v[1])) {
            const auto ch_id = grid.children()[selected];

            if (auto* ch = app.mod.components.try_to_get(ch_id)) {
                temp = ch->y.exists(p_selected)
                         ? ch->y.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected = undefined<port_id>();
                    }

                    for (const auto id : ch->y) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(ch->y.get<port_str>(id).c_str(),
                                              p_selected == id)) {
                            p_selected = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(p_selected)) {

            if (ImGui::Button("+")) {
                const auto [row, col] = grid.pos(selected);
                if (auto ret =
                      grid.connect_input(g_port_id, row, col, p_selected);
                    not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add input connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_input_connection_already_exists))
                              m = "Input connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_input_connection_container_full))
                              m = "Not enough memory to allocate more input "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id  = undefined<port_id>();
                    selected   = 0;
                    p_selected = undefined<port_id>();
                }
            }
        }
    }

    void show_input_connections_new(generic_component& g,
                                    component&         gcompo) noexcept
    {
        static ImGuiTextFilter filter;
        static port_id         g_port_id   = undefined<port_id>();
        static child_id        selected    = undefined<child_id>();
        static port_id         p_selected  = undefined<port_id>();
        static int             pp_selected = -1;
        static std::string     temp;

        g_port_id = show_input_port(gcompo, g_port_id);

        if (is_defined(g_port_id)) {
            const auto new_selected = show_node_selection(filter, g, selected);
            if (new_selected != selected) {
                selected    = new_selected;
                p_selected  = undefined<port_id>();
                pp_selected = -1;
            }
        }

        if (auto* ch = g.children.try_to_get(selected)) {
            if (ch->type == child_type::component) {
                if (auto* sel_compo =
                      app.mod.components.try_to_get(ch->id.compo_id)) {

                    temp = sel_compo->x.exists(p_selected)
                             ? sel_compo->x.get<port_str>(p_selected).sv()
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", is_undefined(p_selected))) {
                            p_selected  = undefined<port_id>();
                            pp_selected = -1;
                        }

                        for (const auto id : sel_compo->x) {
                            ImGui::PushID(get_index(id));
                            if (ImGui::Selectable(
                                  sel_compo->x.get<port_str>(id).c_str(),
                                  p_selected == id)) {
                                p_selected = id;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            } else {
                const auto dyn_type = ch->id.mdl_type;
                const auto dyn_port = get_input_port_names_v(dyn_type);
                const auto dyn_size = static_cast<int>(dyn_port.size());

                if (dyn_size > 0) {
                    temp = 0 <= pp_selected and pp_selected < dyn_size
                             ? dyn_port[pp_selected]
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", is_undefined(p_selected))) {
                            p_selected  = undefined<port_id>();
                            pp_selected = -1;
                        }

                        for (auto i = 0, e = (int)dyn_port.size(); i < e; ++i) {
                            ImGui::PushID(i);
                            if (ImGui::Selectable(dyn_port[i],
                                                  pp_selected == i)) {
                                pp_selected = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(selected) and
            (is_defined(p_selected) or pp_selected >= 0)) {

            if (ImGui::Button("+")) {
                auto&          child = g.children.get(selected);
                expected<void> ret;

                if (child.type == child_type::component)
                    ret =
                      g.connect_input(g_port_id,
                                      g.children.get(selected),
                                      connection::port{ .compo = p_selected });
                else
                    ret =
                      g.connect_input(g_port_id,
                                      child,
                                      connection::port{ .model = pp_selected });

                if (not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add input connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_input_connection_already_exists))
                              m = "Input connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_input_connection_container_full))
                              m = "Not enough memory to allocate more input "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id   = undefined<port_id>();
                    selected    = undefined<child_id>();
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                }
            }
        }
    }

    void show_output_connections_new(generic_component& g,
                                     component&         gcompo) noexcept
    {
        static ImGuiTextFilter filter;
        static port_id         g_port_id   = undefined<port_id>();
        static child_id        selected    = undefined<child_id>();
        static port_id         p_selected  = undefined<port_id>();
        static int             pp_selected = -1;
        static std::string     temp;

        g_port_id = show_output_port(gcompo, g_port_id);

        if (is_defined(g_port_id)) {
            const auto new_selected = show_node_selection(filter, g, selected);
            if (new_selected != selected) {
                selected    = new_selected;
                p_selected  = undefined<port_id>();
                pp_selected = -1;
            }
        }

        if (auto* ch = g.children.try_to_get(selected)) {
            if (ch->type == child_type::component) {
                if (auto* sel_compo =
                      app.mod.components.try_to_get(ch->id.compo_id)) {

                    temp = sel_compo->y.exists(p_selected)
                             ? sel_compo->y.get<port_str>(p_selected).sv()
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", is_undefined(p_selected))) {
                            p_selected  = undefined<port_id>();
                            pp_selected = -1;
                        }

                        for (const auto id : sel_compo->x) {
                            ImGui::PushID(get_index(id));
                            if (ImGui::Selectable(
                                  sel_compo->y.get<port_str>(id).c_str(),
                                  p_selected == id)) {
                                p_selected = id;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            } else {
                const auto dyn_type = ch->id.mdl_type;
                const auto dyn_port = get_output_port_names_v(dyn_type);
                const auto dyn_size = static_cast<int>(dyn_port.size());

                if (dyn_size > 0) {
                    temp = 0 <= pp_selected and pp_selected < dyn_size
                             ? dyn_port[pp_selected]
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", is_undefined(p_selected))) {
                            p_selected  = undefined<port_id>();
                            pp_selected = -1;
                        }

                        for (auto i = 0, e = (int)dyn_port.size(); i < e; ++i) {
                            ImGui::PushID(i);
                            if (ImGui::Selectable(dyn_port[i],
                                                  pp_selected == i)) {
                                pp_selected = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(selected) and
            (is_defined(p_selected) or pp_selected >= 0)) {

            if (ImGui::Button("+")) {
                auto&          child = g.children.get(selected);
                expected<void> ret;

                if (child.type == child_type::component)
                    ret =
                      g.connect_output(g_port_id,
                                       g.children.get(selected),
                                       connection::port{ .compo = p_selected });
                else
                    ret = g.connect_output(
                      g_port_id,
                      child,
                      connection::port{ .model = pp_selected });

                if (not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add output connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_output_connection_already_exists))
                              m = "Output connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_output_connection_container_full))
                              m = "Not enough memory to allocate more output "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id   = undefined<port_id>();
                    selected    = undefined<child_id>();
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                }
            }
        }
    }

    void show_input_connections_new(graph_component& graph,
                                    component&       gcompo) noexcept
    {
        static ImGuiTextFilter filter;
        static port_id         g_port_id  = undefined<port_id>();
        static graph_node_id   selected   = undefined<graph_node_id>();
        static port_id         p_selected = undefined<port_id>();
        static std::string     temp;

        g_port_id = show_input_port(gcompo, g_port_id);

        if (is_defined(g_port_id)) {
            const auto new_selected =
              show_node_selection(filter, graph, selected);
            if (new_selected != selected) {
                selected   = new_selected;
                p_selected = undefined<port_id>();
            }
        }

        if (is_defined(selected)) {
            const auto sel_compo_id =
              graph.g.node_components[get_index(selected)];
            if (auto* sel_compo = app.mod.components.try_to_get(sel_compo_id)) {
                temp = sel_compo->x.exists(p_selected)
                         ? sel_compo->x.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected = undefined<port_id>();
                    }

                    for (const auto id : sel_compo->x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(
                              sel_compo->x.get<port_str>(id).c_str(),
                              p_selected == id)) {
                            p_selected = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(selected) and
            is_defined(p_selected)) {

            if (ImGui::Button("+")) {
                if (auto ret =
                      graph.connect_input(g_port_id, selected, p_selected);
                    not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add input connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_input_connection_already_exists))
                              m = "Input connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_input_connection_container_full))
                              m = "Not enough memory to allocate more input "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id  = undefined<port_id>();
                    selected   = undefined<graph_node_id>();
                    p_selected = undefined<port_id>();
                }
            }
        }
    }

    void show_output_connections_new(graph_component& graph,
                                     component&       gcompo) noexcept
    {
        static ImGuiTextFilter filter;
        static port_id         g_port_id  = undefined<port_id>();
        static graph_node_id   selected   = undefined<graph_node_id>();
        static port_id         p_selected = undefined<port_id>();
        static std::string     temp;

        g_port_id = show_output_port(gcompo, g_port_id);

        if (is_defined(g_port_id)) {
            const auto new_selected =
              show_node_selection(filter, graph, selected);
            if (new_selected != selected) {
                selected   = new_selected;
                p_selected = undefined<port_id>();
            }
        }

        if (is_defined(selected)) {
            const auto sel_compo_id =
              graph.g.node_components[get_index(selected)];
            if (auto* sel_compo = app.mod.components.try_to_get(sel_compo_id)) {
                temp = sel_compo->y.exists(p_selected)
                         ? sel_compo->y.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected = undefined<port_id>();
                    }

                    for (const auto id : sel_compo->x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(
                              sel_compo->y.get<port_str>(id).c_str(),
                              p_selected == id)) {
                            p_selected = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }
        }

        if (is_defined(g_port_id) and is_defined(selected) and
            is_defined(p_selected)) {

            if (ImGui::Button("+")) {
                if (auto ret =
                      graph.connect_output(g_port_id, selected, p_selected);
                    not ret) {
                    app.jn.push(
                      log_level::error,
                      [](auto& t, auto& m, auto ec) {
                          t = "Fail to add output connection";
                          if (ec.value() ==
                              ordinal(modeling_errc::
                                        graph_output_connection_already_exists))
                              m = "Output connection already exists.";
                          else if (ec.value() ==
                                   ordinal(
                                     modeling_errc::
                                       graph_output_connection_container_full))
                              m = "Not enough memory to allocate more output "
                                  "connection.";
                      },
                      ret.error());
                } else {
                    g_port_id  = undefined<port_id>();
                    selected   = undefined<graph_node_id>();
                    p_selected = undefined<port_id>();
                }
            }
        }
    }

    void show_input_output_connections(generic_component& g,
                                       component&         c) noexcept
    {
        static std::string str;

        if (ImGui::TreeNodeEx("Input connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<input_connection_id> to_del;

            for (auto& con : g.input_connections) {
                ImGui::PushID(&con);
                to_del = g.input_connections.get_id(con);

                if (auto* child = g.children.try_to_get(con.dst)) {
                    if (child->type == child_type::component) {
                        if (auto* sub_compo = app.mod.components.try_to_get(
                              child->id.compo_id)) {
                            if (sub_compo->x.exists(con.port.compo)) {
                                ImGui::TextFormat(
                                  "{} connected to component {} ({}) port {}\n",
                                  c.x.get<port_str>(con.x).sv(),
                                  g.children_names[get_index(con.dst)].sv(),
                                  ordinal(con.dst),
                                  sub_compo->x.get<port_str>(con.port.compo)
                                    .sv());

                                to_del.reset();
                            }
                        }
                    } else {
                        ImGui::TextFormat(
                          "{} connected to component {} ({}) port {}\n",
                          c.x.get<port_str>(con.x).sv(),
                          g.children_names[get_index(con.dst)].sv(),
                          ordinal(con.dst),
                          con.port.model);

                        to_del.reset();
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.input_connections.get_id(con);

                ImGui::PopID();
            }

            if (to_del.has_value())
                g.input_connections.free(*to_del);

            show_input_connections_new(g, c);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Output connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<output_connection_id> to_del;

            for (auto& con : g.output_connections) {
                ImGui::PushID(&con);
                to_del = g.output_connections.get_id(con);

                if (auto* child = g.children.try_to_get(con.src)) {
                    if (child->type == child_type::component) {
                        if (auto* sub_compo = app.mod.components.try_to_get(
                              child->id.compo_id)) {
                            if (sub_compo->x.exists(con.port.compo)) {
                                ImGui::TextFormat(
                                  "{} connected to component {} ({}) port {}\n",
                                  c.x.get<port_str>(con.y).sv(),
                                  g.children_names[get_index(con.src)].sv(),
                                  ordinal(con.src),
                                  sub_compo->y.get<port_str>(con.port.compo)
                                    .sv());

                                to_del.reset();
                            }
                        }
                    } else {
                        ImGui::TextFormat(
                          "{} connected to component {} ({}) port {}\n",
                          c.x.get<port_str>(con.y).sv(),
                          g.children_names[get_index(con.src)].sv(),
                          ordinal(con.src),
                          con.port.model);

                        to_del.reset();
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.output_connections.get_id(con);

                ImGui::PopID();
            }

            if (to_del.has_value())
                g.output_connections.free(*to_del);

            show_output_connections_new(g, c);
            ImGui::TreePop();
        }
    }

    void show_input_output_connections(grid_component& g, component& c) noexcept
    {
        static std::string str;

        if (ImGui::TreeNodeEx("Input connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<input_connection_id> to_del;

            for (auto& con : g.input_connections) {
                ImGui::PushID(&con);
                to_del = g.input_connections.get_id(con);

                if (g.is_coord_valid(con.row, con.col)) {
                    const auto pos          = g.pos(con.row, con.col);
                    auto       sub_compo_id = g.children()[pos];
                    if (auto* sub_compo =
                          app.mod.components.try_to_get(sub_compo_id)) {
                        if (sub_compo->x.exists(con.id)) {
                            ImGui::SetNextItemAllowOverlap();

                            ImGui::TextFormat(
                              "{} connected to node {}x{} port {}\n",
                              c.x.get<port_str>(con.x).sv(),
                              con.row,
                              con.col,
                              sub_compo->x.get<port_str>(con.id).sv());

                            to_del.reset();
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.input_connections.get_id(con);

                ImGui::PopID();
            }

            if (to_del.has_value())
                g.input_connections.free(*to_del);

            show_input_connections_new(g, c);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Output connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<output_connection_id> to_del;

            for (auto& con : g.output_connections) {
                ImGui::PushID(&con);
                to_del = g.output_connections.get_id(con);

                if (g.is_coord_valid(con.row, con.col)) {
                    const auto pos          = g.pos(con.row, con.col);
                    auto       sub_compo_id = g.children()[pos];
                    if (auto* sub_compo =
                          app.mod.components.try_to_get(sub_compo_id)) {
                        if (sub_compo->y.exists(con.id)) {
                            ImGui::SetNextItemAllowOverlap();

                            ImGui::TextFormat(
                              "{} connected to node {}x{} port {}\n",
                              c.y.get<port_str>(con.y).sv(),
                              con.row,
                              con.col,
                              sub_compo->y.get<port_str>(con.id).sv());
                            to_del.reset();
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.output_connections.get_id(con);

                ImGui::PopID();
            }

            if (to_del.has_value())
                g.output_connections.free(*to_del);

            show_output_connections_new(g, c);
            ImGui::TreePop();
        }
    }

    void show_input_output_connections(graph_component& g,
                                       component&       c) noexcept
    {
        static std::string str;

        if (ImGui::TreeNodeEx("Input connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<input_connection_id> to_del;

            for (auto& con : g.input_connections) {
                to_del = g.input_connections.get_id(con);

                if (g.g.nodes.exists(con.v)) {
                    auto sub_compo_id = g.g.node_components[get_index(con.v)];
                    if (auto* sub_compo =
                          app.mod.components.try_to_get(sub_compo_id)) {
                        if (sub_compo->x.exists(con.id)) {
                            ImGui::SetNextItemAllowOverlap();

                            ImGui::TextFormat(
                              "{} connected to node {} (name: {}) port {}\n",
                              c.x.get<port_str>(con.x).sv(),
                              g.g.node_ids[get_index(con.v)],
                              g.g.node_names[get_index(con.v)],
                              sub_compo->x.get<port_str>(con.id).sv());
                            to_del.reset();
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.input_connections.get_id(con);
            }

            if (to_del.has_value())
                g.input_connections.free(*to_del);

            show_input_connections_new(g, c);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Output connections",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<output_connection_id> to_del;

            for (auto& con : g.output_connections) {
                to_del = g.output_connections.get_id(con);

                if (g.g.nodes.exists(con.v)) {
                    auto sub_compo_id = g.g.node_components[get_index(con.v)];
                    if (auto* sub_compo =
                          app.mod.components.try_to_get(sub_compo_id)) {
                        if (sub_compo->y.exists(con.id)) {
                            ImGui::SetNextItemAllowOverlap();

                            ImGui::TextFormat(
                              "{} connected to node {} (name: {}) port {}\n",
                              c.y.get<port_str>(con.y).sv(),
                              g.g.node_ids[get_index(con.v)],
                              g.g.node_names[get_index(con.v)],
                              sub_compo->y.get<port_str>(con.id).sv());
                            to_del.reset();
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("del"))
                    to_del = g.output_connections.get_id(con);
            }

            if (to_del.has_value())
                g.output_connections.free(*to_del);

            show_output_connections_new(g, c);
            ImGui::TreePop();
        }
    }

    void show_input_output_connections(component& compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            break;
        case component_type::internal:
            break;
        case component_type::simple:
            if (auto* g =
                  app.mod.generic_components.try_to_get(compo.id.generic_id))
                show_input_output_connections(*g, compo);
            break;
            break;
        case component_type::grid:
            if (auto* g = app.mod.grid_components.try_to_get(compo.id.grid_id))
                show_input_output_connections(*g, compo);
            break;
        case component_type::graph:
            if (auto* g =
                  app.mod.graph_components.try_to_get(compo.id.graph_id))
                show_input_output_connections(*g, compo);
            break;
        case component_type::hsm:
            break;
        }
    }

    void show_input_output_ports(component& compo) noexcept
    {
        if (ImGui::TreeNodeEx("X ports", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<port_id> to_del;
            compo.x.for_each<port_str>([&](auto id, auto& name) noexcept {
                ImGui::PushID(ordinal(id));

                if (ImGui::SmallButton("del"))
                    to_del = id;
                ImGui::SameLine();
                ImGui::PushItemWidth(-1.f);
                ImGui::InputFilteredString("##in-name", name);
                ImGui::PopItemWidth();

                ImGui::PopID();
            });

            if (compo.x.can_alloc(1) && ImGui::SmallButton("+##in-port")) {
                compo.x.alloc([&](auto /*id*/, auto& name, auto& pos) noexcept {
                    name = "-";
                    pos.reset();
                });
            }

            if (to_del.has_value())
                compo.x.free(*to_del);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Y ports", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::optional<port_id> to_del;

            compo.y.for_each([&](auto id, auto& name, auto& /*pos*/) noexcept {
                ImGui::PushID(ordinal(id));

                if (ImGui::SmallButton("del"))
                    to_del = id;
                ImGui::SameLine();
                ImGui::PushItemWidth(-1.f);
                ImGui::InputFilteredString("##out-name", name);
                ImGui::PopItemWidth();

                ImGui::PopID();
            });

            if (compo.y.can_alloc(1) && ImGui::SmallButton("+##out-port")) {
                compo.y.alloc([&](auto /*id*/, auto& name, auto& pos) noexcept {
                    name = "-";
                    pos.reset();
                });
            }

            if (to_del.has_value())
                compo.y.free(*to_del);

            ImGui::TreePop();
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

                if (ImGui::BeginTabItem("External source")) {
                    display_external_source(compo);
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
                        show_input_output_connections(compo);
                    }
                    ImGui::EndTabItem();
                }

                const auto specific_flags = element.need_show_selected_nodes(ed)
                                              ? ImGuiTabItemFlags_SetSelected
                                              : ImGuiTabItemFlags_None;

                if (ImGui::BeginTabItem("Specific", nullptr, specific_flags)) {
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

    const auto have_component = not app.generics.empty() or
                                not app.grids.empty() or
                                not app.graphs.empty() or not app.hsms.empty();

    if (have_component and ImGui::BeginTabBar("Editors")) {
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
