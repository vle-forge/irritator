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

static void update_component_list(application&          app,
                                  spin_mutex&           mutex,
                                  vector<component_id>& component_list,
                                  std::atomic_flag&     updating,
                                  const component&      compo) noexcept
{
    app.add_gui_task([&]() noexcept {
        std::unique_lock lock(mutex);
        component_list.clear();

        auto push_back_if_not_find =
          [](auto& app, auto& vec, const auto id) noexcept {
              if (std::ranges::none_of(
                    vec, [id](const auto other) { return id == other; })) {
                  if (not vec.can_alloc(1) and not vec.template grow<2, 1>()) {
                      app.jn.push(log_level::error,
                                  [&vec](auto& title, auto& msg) noexcept {
                                      title = "Adding connection pack error";
                                      format(msg,
                                             "Not enough memory to allocate "
                                             "more connection pack ({})",
                                             vec.capacity());
                                  });

                      return false;
                  }

                  vec.emplace_back(id);
              }

              return true;
          };

        switch (compo.type) {
        case component_type::generic:
            for (const auto& c :
                 app.mod.generic_components.get(compo.id.generic_id).children)
                if (c.type == child_type::component)
                    if (not push_back_if_not_find(
                          app, component_list, c.id.compo_id))
                        break;
            break;

        case component_type::grid:
            for (const auto id :
                 app.mod.grid_components.get(compo.id.grid_id).children())
                if (not push_back_if_not_find(app, component_list, id))
                    break;
            break;

        case component_type::graph:
            for (const auto id :
                 app.mod.graph_components.get(compo.id.graph_id).g.nodes) {
                const auto c_id =
                  app.mod.graph_components.get(compo.id.graph_id)
                    .g.node_components[id];
                if (not push_back_if_not_find(app, component_list, c_id))
                    break;
            }
            break;

        default:
            break;
        }

        updating.clear();
    });
}

static auto combobox_port_id(const auto* label,
                             const auto& compo_x_or_y,
                             auto        p_id) noexcept -> port_id
{
    const char* preview = compo_x_or_y.exists(p_id)
                            ? compo_x_or_y.template get<port_str>(p_id).c_str()
                            : "-";

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("-", is_undefined(p_id))) {
            p_id = undefined<port_id>();
        }

        for (const auto id : compo_x_or_y) {
            ImGui::PushID(ordinal(id));
            if (ImGui::Selectable(
                  compo_x_or_y.template get<port_str>(id).c_str(),
                  id == p_id)) {
                p_id = id;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    return p_id;
}

static auto combobox_component_id(const auto& mod,
                                  const auto* label,
                                  const auto& component_list,
                                  auto        c_id,
                                  auto        p_id) noexcept
  -> std::pair<component_id, port_id>
{
    const char* preview =
      mod.components.exists(c_id)
        ? mod.components.template get<component>(c_id).name.c_str()
        : "-";

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("-", is_undefined(c_id))) {
            c_id = undefined<component_id>();
            p_id = undefined<port_id>();
        }

        for (const auto id : component_list) {
            if (mod.components.exists(id)) {
                ImGui::PushID(ordinal(id));
                if (ImGui::Selectable(
                      mod.components.template get<component>(id).name.c_str(),
                      id == c_id)) {
                    c_id = id;
                    p_id = undefined<port_id>();
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    return std::make_pair(c_id, p_id);
}

static auto combobox_component_port_id_empty(const auto* label) noexcept
  -> port_id
{
    ImGui::BeginDisabled();
    if (ImGui::BeginCombo(label, "-"))
        ImGui::EndCombo();
    ImGui::EndDisabled();

    return undefined<port_id>();
}

static auto combobox_component_port_id(const auto* label,
                                       const auto& compo_x_or_y,
                                       auto        p_id) noexcept -> port_id
{
    const char* preview = compo_x_or_y.exists(p_id)
                            ? compo_x_or_y.template get<port_str>(p_id).c_str()
                            : "-";

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("-", is_undefined(p_id))) {
            p_id = undefined<port_id>();
        }

        for (const auto id : compo_x_or_y) {
            ImGui::PushID(ordinal(id));
            if (ImGui::Selectable(
                  compo_x_or_y.template get<port_str>(id).c_str(),
                  id == p_id)) {
                p_id = id;
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    return p_id;
}

template<typename PortGetter>
static void show_connection_pack(const modeling&          mod,
                                 component&               compo,
                                 vector<connection_pack>& pack,
                                 PortGetter&& port_getter_fn) noexcept
{
    auto& port_x_or_y = port_getter_fn(compo);
    auto  it          = pack.begin();
    auto  push_id     = 0;

    while (it != pack.end()) {
        if (not port_x_or_y.exists(it->parent_port) or
            not mod.components.exists(it->child_component) or
            not port_getter_fn(
                  mod.components.get<component>(it->child_component))
                  .exists(it->child_port)) {
            it = pack.erase(it);
        } else {
            const auto& child =
              mod.components.get<component>(it->child_component);
            auto to_del = false;

            ImGui::PushID(push_id++);
            if (ImGui::Button("-"))
                to_del = true;
            ImGui::SameLine();
            ImGui::TextFormat(
              "x: {} to component {} port {}",
              port_x_or_y.template get<port_str>(it->parent_port).sv(),
              child.name.sv(),
              port_getter_fn(child)
                .template get<port_str>(it->child_port)
                .sv());

            if (to_del)
                it = pack.erase(it);
            else
                ++it;
            ImGui::PopID();
        }
    }
}

struct component_editor::impl {

    static void display_external_source(application& app,
                                        component&   compo) noexcept
    {
        constexpr auto tn_def = ImGuiTreeNodeFlags_DefaultOpen;

        if (not compo.srcs.constant_sources.empty() and
            ImGui::TreeNodeEx("Constants", tn_def)) {
            static constant_source r_src;

            auto to_del = undefined<constant_source_id>();
            for (auto& s : compo.srcs.constant_sources) {
                r_src         = s;
                const auto id = compo.srcs.constant_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNodeEx(&s,
                                      ImGuiTreeNodeFlags_SpanLabelWidth,
                                      "%s",
                                      r_src.name.c_str())) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        to_del = id;

                    if (ImGui::InputFilteredString("name", r_src.name)) {
                        s.name = r_src.name;
                    }

                    u32 len = s.length;
                    if (ImGui::InputScalar("length", ImGuiDataType_U32, &len)) {
                        if (len > 0 and len < external_source_chunk_size)
                            s.length = len;
                    }

                    s.length          = s.length == 0 ? 1 : s.length;
                    const int columns = 3 < s.length ? 3 : s.length;
                    const int rows =
                      (s.length / columns) + ((s.length % columns) > 0 ? 1 : 0);
                    if (ImGui::BeginChild(
                          "##zone",
                          ImVec2(ImGui::GetContentRegionAvail().x,
                                 ImGui::GetFrameHeightWithSpacing() * rows))) {
                        if (ImGui::BeginTable("Values", columns)) {
                            for (int j = 0; j < columns; ++j)
                                ImGui::TableSetupColumn(
                                  "",
                                  ImGuiTableColumnFlags_WidthFixed,
                                  ImGui::GetContentRegionAvail().x / 3.f);

                            for (int i = 0; i < rows; ++i) {
                                ImGui::TableNextRow();
                                for (int j = 0; j < columns; ++j) {
                                    ImGui::TableSetColumnIndex(j);
                                    ImGui::PushID(i * columns + j);
                                    ImGui::PushItemWidth(-1);
                                    ImGui::InputDouble(
                                      "", s.buffer.data() + (i * columns + j));
                                    ImGui::PopItemWidth();
                                    ImGui::PopID();
                                }
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (is_defined(to_del))
                compo.srcs.constant_sources.free(to_del);

            ImGui::TreePop();
            ImGui::Separator();
        }

        if (not compo.srcs.binary_file_sources.empty() and
            ImGui::TreeNodeEx("Binary files", tn_def)) {
            static decltype(constant_source::name) name_tmp;
            static file_path_id id_tmp = undefined<file_path_id>();

            auto to_del = undefined<binary_file_source_id>();
            for (auto& s : compo.srcs.binary_file_sources) {
                name_tmp      = s.name;
                id_tmp        = s.file_id;
                const auto id = compo.srcs.binary_file_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNodeEx(&s,
                                      ImGuiTreeNodeFlags_SpanLabelWidth,
                                      "%s",
                                      s.name.c_str())) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        to_del = id;

                    if (ImGui::InputFilteredString("name", name_tmp)) {
                        s.name = name_tmp;
                    }

                    s.file_id =
                      show_data_file_input(app.mod, compo.dir, id_tmp);

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (is_defined(to_del))
                compo.srcs.binary_file_sources.free(to_del);

            ImGui::TreePop();
            ImGui::Separator();
        }

        if (not compo.srcs.text_file_sources.empty() and
            ImGui::TreeNodeEx("Text files", tn_def)) {
            static decltype(constant_source::name) name_tmp;
            static file_path_id id_tmp = undefined<file_path_id>();

            auto to_del = undefined<text_file_source_id>();
            for (auto& s : compo.srcs.text_file_sources) {
                name_tmp      = s.name;
                id_tmp        = s.file_id;
                const auto id = compo.srcs.text_file_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNodeEx(&s,
                                      ImGuiTreeNodeFlags_SpanLabelWidth,
                                      "%s",
                                      s.name.c_str())) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        to_del = id;

                    if (ImGui::InputFilteredString("name", name_tmp)) {
                        s.name = name_tmp;
                    }

                    s.file_id =
                      show_data_file_input(app.mod, compo.dir, id_tmp);

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (is_defined(to_del))
                compo.srcs.text_file_sources.free(to_del);

            ImGui::TreePop();
            ImGui::Separator();
        }

        if (not compo.srcs.random_sources.empty() and
            ImGui::TreeNodeEx("Random", tn_def)) {
            // @TODO providing a better API to fill the distribution.
            static random_source r_src;

            auto to_del = undefined<random_source_id>();
            for (auto& s : compo.srcs.random_sources) {
                r_src         = s;
                const auto id = compo.srcs.random_sources.get_id(s);
                ImGui::PushID(&s);
                if (ImGui::TreeNodeEx(&s,
                                      ImGuiTreeNodeFlags_SpanLabelWidth,
                                      "%s",
                                      r_src.name.c_str())) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x"))
                        to_del = id;

                    if (ImGui::InputFilteredString("name", r_src.name)) {
                        s.name = r_src.name.sv();
                    }

                    if (show_random_distribution_input(r_src)) {
                        s = r_src;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (is_defined(to_del))
                compo.srcs.random_sources.free(to_del);

            ImGui::TreePop();
        }
    }

    template<typename ComponentEditor>
    static void show_file_access(application&     app,
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
                            app.jn.push(
                              log_level::error,
                              [&](auto& title, auto& /*msg*/) noexcept {
                                  format(title,
                                         "Fail to create directory {}",
                                         new_dir.path.sv());
                              });
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

    static port_id show_input_port(component& gcompo,
                                   port_id    g_port_id) noexcept
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
                ImGui::PushID(idx);
                if (ImGui::Selectable(names[idx].c_str(), g_port_id == id))
                    g_port_id = id;
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return g_port_id;
    }

    static port_id show_output_port(component& gcompo,
                                    port_id    g_port_id) noexcept
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
                ImGui::PushID(idx);
                if (ImGui::Selectable(names[idx].c_str(), g_port_id == id))
                    g_port_id = id;
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return g_port_id;
    }

    static child_id show_node_selection(ImGuiTextFilter&   filter,
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

    static graph_node_id show_node_selection(ImGuiTextFilter& filter,
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

    static void show_input_connections_new(application&    app,
                                           grid_component& grid,
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

            if (auto* ch = app.mod.components.try_to_get<component>(ch_id)) {
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

            if (is_defined(g_port_id) and is_defined(p_selected)) {
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
                              m = "Not enough memory to allocate more "
                                  "input "
                                  "connection.";
                      },
                      ret.error());
                }

                g_port_id  = undefined<port_id>();
                selected   = 0;
                p_selected = undefined<port_id>();
            }
        }
    }

    static void show_output_connections_new(application&    app,
                                            grid_component& grid,
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

            if (auto* ch = app.mod.components.try_to_get<component>(ch_id)) {
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

            if (is_defined(g_port_id) and is_defined(p_selected)) {
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
                              m = "Not enough memory to allocate more "
                                  "input "
                                  "connection.";
                      },
                      ret.error());
                }

                g_port_id  = undefined<port_id>();
                selected   = 0;
                p_selected = undefined<port_id>();
            }
        }
    }

    static void show_input_connections_new(application&       app,
                                           generic_component& g,
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
                if (auto* sel_compo = app.mod.components.try_to_get<component>(
                      ch->id.compo_id)) {

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
                const auto dyn_port = get_input_port_names(dyn_type);
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
                            name_str name(dyn_port[i]);
                            if (ImGui::Selectable(name.c_str(),
                                                  pp_selected == i)) {
                                pp_selected = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }

            if (is_defined(g_port_id) and is_defined(selected) and
                (is_defined(p_selected) or pp_selected >= 0)) {
                if (connect_input(
                      g_port_id, selected, p_selected, pp_selected, g, app)) {
                    g_port_id   = undefined<port_id>();
                    selected    = undefined<child_id>();
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                } else {
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                }
            }
        }
    }

    static bool connect_input(const port_id      g_port_id,
                              const child_id     selected,
                              const port_id      p_selected,
                              const int          pp_selected,
                              generic_component& g,
                              application&       app)
    {
        const auto& child = g.children.get(selected);
        const auto  ret =
          (child.type == child_type::component)
             ? g.connect_input(g_port_id,
                              g.children.get(selected),
                              connection::port{ .compo = p_selected })
             : g.connect_input(
                g_port_id, child, connection::port{ .model = pp_selected });

        if (not ret) {
            app.jn.push(
              log_level::error,
              [](auto& t, auto& m, auto ec) {
                  t = "Fail to add input connection";
                  if (ec.value() ==
                      ordinal(
                        modeling_errc::graph_input_connection_already_exists))
                      m = "Input connection already exists.";
                  else if (ec.value() ==
                           ordinal(modeling_errc::
                                     graph_input_connection_container_full))
                      m = "Not enough memory to allocate more "
                          "input "
                          "connection.";
              },
              ret.error());

            return false;
        }

        return true;
    }

    static void show_output_connections_new(application&       app,
                                            generic_component& g,
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
                if (auto* sel_compo = app.mod.components.try_to_get<component>(
                      ch->id.compo_id)) {

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
                const auto dyn_port = get_output_port_names(dyn_type);
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
                            name_str name(dyn_port[i]);
                            if (ImGui::Selectable(name.c_str(),
                                                  pp_selected == i)) {
                                pp_selected = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }

            if (is_defined(g_port_id) and is_defined(selected) and
                (is_defined(p_selected) or pp_selected >= 0)) {
                if (connect_output(
                      g_port_id, selected, p_selected, pp_selected, g, app)) {
                    g_port_id   = undefined<port_id>();
                    selected    = undefined<child_id>();
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                } else {
                    p_selected  = undefined<port_id>();
                    pp_selected = -1;
                }
            }
        }
    }

    static bool connect_output(const port_id      g_port_id,
                               const child_id     selected,
                               const port_id      p_selected,
                               int                pp_selected,
                               generic_component& g,
                               application&       app)
    {

        auto&          child = g.children.get(selected);
        expected<void> ret;

        if (child.type == child_type::component)
            ret = g.connect_output(g_port_id,
                                   g.children.get(selected),
                                   connection::port{ .compo = p_selected });
        else
            ret = g.connect_output(
              g_port_id, child, connection::port{ .model = pp_selected });

        if (not ret) {
            app.jn.push(
              log_level::error,
              [](auto& t, auto& m, auto ec) {
                  t = "Fail to add output connection";
                  if (ec.value() ==
                      ordinal(
                        modeling_errc::graph_output_connection_already_exists))
                      m = "Output connection already exists.";
                  else if (ec.value() ==
                           ordinal(modeling_errc::
                                     graph_output_connection_container_full))
                      m = "Not enough memory to allocate more "
                          "output "
                          "connection.";
              },
              ret.error());

            return false;
        }

        return true;
    }

    static void show_input_connections_new(application&     app,
                                           graph_component& graph,
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
            if (auto* sel_compo =
                  app.mod.components.try_to_get<component>(sel_compo_id)) {
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

            if (is_defined(g_port_id) and is_defined(selected) and
                is_defined(p_selected)) {

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
                              m = "Not enough memory to allocate more "
                                  "input "
                                  "connection.";
                      },
                      ret.error());
                }

                g_port_id  = undefined<port_id>();
                selected   = undefined<graph_node_id>();
                p_selected = undefined<port_id>();
            }
        }
    }

    static void show_output_connections_new(application&     app,
                                            graph_component& graph,
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
            if (auto* sel_compo =
                  app.mod.components.try_to_get<component>(sel_compo_id)) {
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

            if (is_defined(g_port_id) and is_defined(selected) and
                is_defined(p_selected)) {

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
                              m = "Not enough memory to allocate more "
                                  "output "
                                  "connection.";
                      },
                      ret.error());
                }

                g_port_id  = undefined<port_id>();
                selected   = undefined<graph_node_id>();
                p_selected = undefined<port_id>();
            }
        }
    }

    static void show_input_connections(application&       app,
                                       generic_component& g,
                                       component&         c) noexcept
    {
        std::optional<input_connection_id> to_del;

        for (auto& con : g.input_connections) {
            ImGui::PushID(&con);

            if (auto* child = g.children.try_to_get(con.dst);
                child and c.x.exists(con.x)) {

                if (ImGui::Button("X"))
                    to_del = g.input_connections.get_id(con);
                ImGui::SameLine();

                if (child->type == child_type::component) {
                    if (auto* sub_compo =
                          app.mod.components.try_to_get<component>(
                            child->id.compo_id)) {
                        if (sub_compo->x.exists(con.port.compo)) {
                            ImGui::TextFormat(
                              "{} connected to component {} ({}) "
                              "port "
                              "{}\n",
                              c.x.get<port_str>(con.x).sv(),
                              g.children_names[get_index(con.dst)].sv(),
                              ordinal(con.dst),
                              sub_compo->x.get<port_str>(con.port.compo).sv());
                        } else
                            to_del = g.input_connections.get_id(con);
                    } else
                        to_del = g.input_connections.get_id(con);
                } else {
                    ImGui::TextFormat(
                      "{} connected to component {} ({}) port {}\n",
                      c.x.get<port_str>(con.x).sv(),
                      g.children_names[get_index(con.dst)].sv(),
                      ordinal(con.dst),
                      con.port.model);
                }
            } else
                to_del = g.input_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.input_connections.free(*to_del);

        show_input_connections_new(app, g, c);
    }

    static void show_output_connections(application&       app,
                                        generic_component& g,
                                        component&         c) noexcept
    {
        std::optional<output_connection_id> to_del;

        for (auto& con : g.output_connections) {
            ImGui::PushID(&con);

            if (auto* child = g.children.try_to_get(con.src);
                child and c.y.exists(con.y)) {

                if (ImGui::Button("X"))
                    to_del = g.output_connections.get_id(con);

                ImGui::SameLine();

                if (child->type == child_type::component) {
                    if (auto* sub_compo =
                          app.mod.components.try_to_get<component>(
                            child->id.compo_id)) {
                        if (sub_compo->x.exists(con.port.compo)) {
                            ImGui::TextFormat(
                              "{} connected to component {} ({}) "
                              "port "
                              "{}\n",
                              c.x.get<port_str>(con.y).sv(),
                              g.children_names[get_index(con.src)].sv(),
                              ordinal(con.src),
                              sub_compo->y.get<port_str>(con.port.compo).sv());
                        } else
                            to_del = g.output_connections.get_id(con);
                    } else
                        to_del = g.output_connections.get_id(con);
                } else {
                    ImGui::TextFormat(
                      "{} connected to component {} ({}) port {}\n",
                      c.y.get<port_str>(con.y).sv(),
                      g.children_names[get_index(con.src)].sv(),
                      ordinal(con.src),
                      con.port.model);
                }
            } else
                to_del = g.output_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.output_connections.free(*to_del);

        show_output_connections_new(app, g, c);
    }

    static void show_input_connections(application&    app,
                                       grid_component& g,
                                       component&      c) noexcept
    {
        std::optional<input_connection_id> to_del;

        for (auto& con : g.input_connections) {
            ImGui::PushID(&con);

            if (ImGui::Button("X"))
                to_del = g.input_connections.get_id(con);
            ImGui::SameLine();

            if (g.is_coord_valid(con.row, con.col)) {
                const auto pos          = g.pos(con.row, con.col);
                auto       sub_compo_id = g.children()[pos];

                if (auto* sub_compo =
                      app.mod.components.try_to_get<component>(sub_compo_id)) {
                    if (sub_compo->x.exists(con.id)) {
                        ImGui::SetNextItemAllowOverlap();

                        ImGui::TextFormat(
                          "{} connected to node {}x{} port {}\n",
                          c.x.get<port_str>(con.x).sv(),
                          con.row,
                          con.col,
                          sub_compo->x.get<port_str>(con.id).sv());
                    } else
                        to_del = g.input_connections.get_id(con);
                } else
                    to_del = g.input_connections.get_id(con);
            } else
                to_del = g.input_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.input_connections.free(*to_del);

        show_input_connections_new(app, g, c);
    }

    static void show_output_connections(application&    app,
                                        grid_component& g,
                                        component&      c) noexcept
    {
        std::optional<output_connection_id> to_del;

        for (auto& con : g.output_connections) {
            ImGui::PushID(&con);

            ImGui::SameLine();
            if (ImGui::Button("X"))
                to_del = g.output_connections.get_id(con);

            if (g.is_coord_valid(con.row, con.col)) {
                const auto pos          = g.pos(con.row, con.col);
                auto       sub_compo_id = g.children()[pos];

                if (auto* sub_compo =
                      app.mod.components.try_to_get<component>(sub_compo_id)) {
                    if (sub_compo->y.exists(con.id)) {
                        ImGui::SetNextItemAllowOverlap();

                        ImGui::TextFormat(
                          "{} connected to node {}x{} port {}\n",
                          c.y.get<port_str>(con.y).sv(),
                          con.row,
                          con.col,
                          sub_compo->y.get<port_str>(con.id).sv());
                        to_del.reset();
                    } else
                        to_del = g.output_connections.get_id(con);
                } else
                    to_del = g.output_connections.get_id(con);
            } else
                to_del = g.output_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.output_connections.free(*to_del);

        show_output_connections_new(app, g, c);
    }

    static void show_input_connections(application&     app,
                                       graph_component& g,
                                       component&       c) noexcept
    {
        std::optional<input_connection_id> to_del;

        for (auto& con : g.input_connections) {
            ImGui::PushID(&con);

            if (ImGui::Button("X"))
                to_del = g.input_connections.get_id(con);
            ImGui::SameLine();

            if (g.g.nodes.exists(con.v)) {
                auto sub_compo_id = g.g.node_components[get_index(con.v)];
                if (auto* sub_compo =
                      app.mod.components.try_to_get<component>(sub_compo_id)) {

                    if (sub_compo->x.exists(con.id) and c.x.exists(con.x)) {
                        ImGui::SetNextItemAllowOverlap();

                        ImGui::TextFormat(
                          "{} connected to node {} (name: {}) port "
                          "{}\n",
                          c.x.get<port_str>(con.x).sv(),
                          g.g.node_ids[get_index(con.v)],
                          g.g.node_names[get_index(con.v)],
                          sub_compo->x.get<port_str>(con.id).sv());
                    } else
                        to_del = g.input_connections.get_id(con);
                } else
                    to_del = g.input_connections.get_id(con);
            } else
                to_del = g.input_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.input_connections.free(*to_del);

        show_input_connections_new(app, g, c);
    }

    static void show_output_connections(application&     app,
                                        graph_component& g,
                                        component&       c) noexcept
    {
        std::optional<output_connection_id> to_del;

        for (auto& con : g.output_connections) {
            ImGui::PushID(&con);

            if (ImGui::Button("X"))
                to_del = g.output_connections.get_id(con);
            ImGui::SameLine();

            if (g.g.nodes.exists(con.v)) {
                auto sub_compo_id = g.g.node_components[get_index(con.v)];
                if (auto* sub_compo =
                      app.mod.components.try_to_get<component>(sub_compo_id)) {
                    if (sub_compo->y.exists(con.id) and c.y.exists(con.y)) {
                        ImGui::SetNextItemAllowOverlap();

                        ImGui::TextFormat(
                          "{} connected to node {} (name: {}) port "
                          "{}\n",
                          c.y.get<port_str>(con.y).sv(),
                          g.g.node_ids[get_index(con.v)],
                          g.g.node_names[get_index(con.v)],
                          sub_compo->y.get<port_str>(con.id).sv());
                    } else
                        to_del = g.output_connections.get_id(con);
                } else
                    to_del = g.output_connections.get_id(con);
            } else
                to_del = g.output_connections.get_id(con);

            ImGui::PopID();
        }

        if (to_del.has_value())
            g.output_connections.free(*to_del);

        show_output_connections_new(app, g, c);
    }

    static void show_input_connections(application& app,
                                       component&   compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            break;

        case component_type::generic:
            if (auto* g =
                  app.mod.generic_components.try_to_get(compo.id.generic_id))
                show_input_connections(app, *g, compo);
            break;

        case component_type::grid:
            if (auto* g = app.mod.grid_components.try_to_get(compo.id.grid_id))
                show_input_connections(app, *g, compo);
            break;

        case component_type::graph:
            if (auto* g =
                  app.mod.graph_components.try_to_get(compo.id.graph_id))
                show_input_connections(app, *g, compo);
            break;

        case component_type::hsm:
            break;
        }
    }

    static void show_output_connections(application& app,
                                        component&   compo) noexcept
    {
        switch (compo.type) {
        case component_type::none:
            break;

        case component_type::generic:
            if (auto* g =
                  app.mod.generic_components.try_to_get(compo.id.generic_id))
                show_output_connections(app, *g, compo);
            break;

        case component_type::grid:
            if (auto* g = app.mod.grid_components.try_to_get(compo.id.grid_id))
                show_output_connections(app, *g, compo);
            break;

        case component_type::graph:
            if (auto* g =
                  app.mod.graph_components.try_to_get(compo.id.graph_id))
                show_output_connections(app, *g, compo);
            break;

        case component_type::hsm:
            break;
        }
    }

    static void show_input_ports(component& compo) noexcept
    {
        std::optional<port_id> to_del;

        auto&      names = compo.x.get<port_str>();
        auto&      types = compo.x.get<port_option>();
        const auto width = ImGui::CalcTextSize("88888888").x +
                           ImGui::GetStyle().FramePadding.x * 2.0f;

        for (const auto id : compo.x) {
            const auto idx = get_index(id);
            ImGui::PushID(idx);

            if (ImGui::Button("X"))
                to_del = id;

            ImGui::SameLine();
            ImGui::PushItemWidth(width);
            ImGui::InputFilteredString("##in-name", names[idx]);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::PushItemWidth(width);
            const auto* preview = port_option_names[ordinal(types[idx])];
            if (ImGui::BeginCombo("##in-type", preview)) {
                for (auto i = 0, e = length(port_option_names); i != e; ++i) {
                    if (ImGui::Selectable(port_option_names[i],
                                          i == ordinal(types[idx]))) {
                        types[idx] = enum_cast<port_option>(i);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        if (to_del.has_value())
            compo.x.free(*to_del);
    }

    static void show_output_ports(component& compo) noexcept
    {
        std::optional<port_id> to_del;

        auto&      names = compo.y.get<port_str>();
        auto&      types = compo.y.get<port_option>();
        const auto width = ImGui::CalcTextSize("88888888").x +
                           ImGui::GetStyle().FramePadding.x * 2.0f;

        for (const auto id : compo.y) {
            const auto idx = get_index(id);
            ImGui::PushID(idx);

            if (ImGui::Button("X"))
                to_del = id;

            ImGui::SameLine();
            ImGui::PushItemWidth(width);
            ImGui::InputFilteredString("##out-name", names[idx]);
            ImGui::SameLine();
            ImGui::PushItemWidth(width);
            const auto* preview = port_option_names[ordinal(types[idx])];
            if (ImGui::BeginCombo("##in-type", preview)) {
                for (auto i = 0, e = length(port_option_names); i != e; ++i) {
                    if (ImGui::Selectable(port_option_names[i],
                                          i == ordinal(types[idx]))) {
                        types[idx] = enum_cast<port_option>(i);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        if (to_del.has_value())
            compo.y.free(*to_del);
    }

    static void show_input_connection_packs(application& app,
                                            component&   compo) noexcept
    {
        auto& ed = app.component_ed;

        show_connection_pack(
          app.mod,
          compo,
          compo.input_connection_pack,
          [](auto& compo) noexcept -> auto& { return compo.x; });

        static auto p   = undefined<port_id>();
        static auto c_p = undefined<port_id>();
        static auto c   = undefined<component_id>();

        if (ed.component_list_updating.test()) {
            ImGui::TextUnformatted("Component update in progress");
        } else {
            p = combobox_port_id("input port", compo.x, p);

            const auto child = combobox_component_id(
              app.mod, "child component", ed.component_list, c, c_p);
            c   = child.first;
            c_p = app.mod.components.exists(c)
                    ? combobox_component_port_id(
                        "child port",
                        app.mod.components.get<component>(c).x,
                        child.second)
                    : combobox_component_port_id_empty("child port");
        }

        if (is_defined(p) and is_defined(c_p) and is_defined(c) and
            std::ranges::none_of(
              compo.input_connection_pack, [&](const auto& con) {
                  return con.parent_port == p and con.child_port == c_p and
                         con.child_component == c;
              })) {
            compo.input_connection_pack.push_back(connection_pack{
              .parent_port = p, .child_port = c_p, .child_component = c });
            p   = undefined<port_id>();
            c_p = undefined<port_id>();
            c   = undefined<component_id>();
        }
    }

    static void show_output_connection_packs(application& app,
                                             component&   compo) noexcept
    {
        auto& ed = app.component_ed;

        show_connection_pack(
          app.mod,
          compo,
          compo.output_connection_pack,
          [](auto& compo) noexcept -> auto& { return compo.y; });

        static auto p   = undefined<port_id>();
        static auto c_p = undefined<port_id>();
        static auto c   = undefined<component_id>();

        if (ed.component_list_updating.test()) {
            ImGui::TextUnformatted("Component update in progress");
        } else {
            p = combobox_port_id("output port", compo.y, p);

            const auto child = combobox_component_id(
              app.mod, "child component", ed.component_list, c, c_p);
            c   = child.first;
            c_p = app.mod.components.exists(c)
                    ? combobox_component_port_id(
                        "child port",
                        app.mod.components.get<component>(c).y,
                        child.second)
                    : combobox_component_port_id_empty("child port");
        }

        if (is_defined(p) and is_defined(c_p) and is_defined(c) and
            std::ranges::none_of(
              compo.output_connection_pack, [&](const auto& con) {
                  return con.parent_port == p and con.child_port == c_p and
                         con.child_component == c;
              })) {
            compo.output_connection_pack.push_back(connection_pack{
              .parent_port = p, .child_port = c_p, .child_component = c });
            p   = undefined<port_id>();
            c_p = undefined<port_id>();
            c   = undefined<component_id>();
        }
    }

    template<typename ComponentEditor>
    static void display_component_editor_subtable(
      application&     app,
      ComponentEditor& element,
      component&       compo,
      ImGuiTableFlags  flags) noexcept
    {
        auto& ed = app.component_ed;

        if (ImGui::BeginTable("##ed", 2, flags)) {
            ImGui::TableSetupColumn(
              "Component settings", ImGuiTableColumnFlags_WidthStretch, 0.2f);
            ImGui::TableSetupColumn(
              "Graph", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::BeginChild("ChildR",
                              ImVec2(0, 0),
                              ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_MenuBar);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("Save")) {
                    show_file_access(app, element, compo);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Sources")) {
                    if (ImGui::BeginMenu("New")) {
                        const ImVec2 size{
                            ImGui::CalcTextSize("new binary file").x +
                              ImGui::GetStyle().FramePadding.x * 2.0f,
                            0
                        };

                        if (ImGui::Button("new constant", size)) {
                            auto& news = compo.srcs.constant_sources.alloc();
                            news.name  = "cst";
                        }

                        if (ImGui::Button("new binary file", size)) {
                            auto& news = compo.srcs.binary_file_sources.alloc();
                            news.name  = "bin";
                        }

                        if (ImGui::Button("new text file", size)) {
                            auto& news = compo.srcs.text_file_sources.alloc();
                            news.name  = "txt";
                        }

                        if (ImGui::Button("new random", size)) {
                            auto& news = compo.srcs.random_sources.alloc();
                            news.name  = "rng";
                        }

                        ImGui::EndMenu();
                    }

                    if (not compo.srcs.constant_sources.empty() or
                        not compo.srcs.binary_file_sources.empty() or
                        not compo.srcs.text_file_sources.empty() or
                        not compo.srcs.random_sources.empty()) {
                        ImGui::Separator();
                        display_external_source(app, compo);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("i/o")) {
                    if (compo.type == component_type::hsm) {
                        ImGui::TextWrapped(
                          "HSM component have four input and four "
                          "output ports. You can select input ports from "
                          "the state conditions and the output ports from "
                          "the "
                          "state actions.");
                    } else {
                        const auto size =
                          ImVec2{ ImGui::CalcTextSize("XXXXXXXXXXXX").x +
                                    ImGui::GetStyle().FramePadding.x * 2.0f,
                                  0 };

                        if (compo.x.can_alloc(1) and
                            ImGui::Button("Input port", size)) {
                            compo.x.alloc([&](auto /*id*/,
                                              auto& type,
                                              auto& name,
                                              auto& pos) noexcept {
                                name = "in";
                                type = port_option::classic;
                                pos.reset();
                            });
                        }

                        if (compo.y.can_alloc(1) and
                            ImGui::Button("Output port", size)) {
                            compo.y.alloc([&](auto /*id*/,
                                              auto& type,
                                              auto& name,
                                              auto& pos) noexcept {
                                name = "out";
                                type = port_option::classic;
                                pos.reset();
                            });
                        }

                        if (ImGui::Button("refresh i/o", size))
                            update_component_list(app,
                                                  ed.component_list_mutex,
                                                  ed.component_list,
                                                  ed.component_list_updating,
                                                  compo);

                        if (not compo.x.empty() or not compo.y.empty()) {
                            if (ImGui::TreeNode("Ports")) {
                                if (not compo.x.empty() and
                                    ImGui::TreeNode("Input port")) {
                                    show_input_ports(compo);
                                    ImGui::TreePop();
                                }

                                if (not compo.y.empty() and
                                    ImGui::TreeNode("Output port")) {
                                    show_output_ports(compo);
                                    ImGui::TreePop();
                                }
                                ImGui::TreePop();
                            }

                            if (ImGui::TreeNode("Connections")) {
                                if (not compo.x.empty() and
                                    ImGui::TreeNode("Input connection")) {
                                    show_input_connections(app, compo);
                                    ImGui::TreePop();
                                }

                                if (not compo.y.empty() and
                                    ImGui::TreeNode("Output connection")) {
                                    show_output_connections(app, compo);
                                    ImGui::TreePop();
                                }

                                if (not compo.x.empty() and
                                    ImGui::TreeNode("Input Connection pack")) {
                                    show_input_connection_packs(app, compo);
                                    ImGui::TreePop();
                                }

                                if (not compo.y.empty() and
                                    ImGui::TreeNode("Output Connection pack")) {
                                    show_output_connection_packs(app, compo);
                                    ImGui::TreePop();
                                }

                                ImGui::TreePop();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            name_str copy_name = compo.name;
            if (ImGui::InputFilteredString("Name", copy_name)) {
                app.component_ed.request_to_open(
                  app.mod.components.get_id(compo));
                compo.name = copy_name;
            }

            element.show_selected_nodes(ed);
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            element.show(ed);

            ImGui::EndTable();
        }
    }

    template<typename T, typename ID>
    static bool display_component_editor(application&       app,
                                         data_array<T, ID>& data,
                                         const ID           id,
                                         const component_id compo_id) noexcept
    {
        auto* compo = app.mod.components.try_to_get<component>(compo_id);
        if (not compo)
            return true;

        auto* element = data.try_to_get(id);
        if (not element)
            return true;

        auto& ed = app.component_ed;

        auto tab_item_flags = ImGuiTabItemFlags_None;
        format(ed.title, "{}##{}", compo->name.c_str(), get_index(compo_id));

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

            if (compo->is_file_defined() and
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
                if constexpr (has_store_function<T>) {
                    element->store(ed);
                }
                app.start_save_component(compo_id);
            }

            display_component_editor_subtable(app, *element, *compo, flags);

            ImGui::EndTabItem();
        }

        return (open == false);
    }

    static void display_tabs(application& app) noexcept
    {
        auto it = app.component_ed.tabs.begin();

        while (it != app.component_ed.tabs.end()) {
            auto to_del = false;

            switch (it->type) {
            case component_type::generic:
                if (display_component_editor(app,
                                             app.component_ed.generics,
                                             it->data.generic,
                                             it->id)) {
                    app.component_ed.generics.free(it->data.generic);
                    to_del = true;
                }
                break;

            case component_type::grid:
                if (display_component_editor(
                      app, app.component_ed.grids, it->data.grid, it->id)) {
                    app.component_ed.grids.free(it->data.grid);
                    to_del = true;
                }
                break;

            case component_type::graph:
                if (display_component_editor(
                      app, app.component_ed.graphs, it->data.graph, it->id)) {
                    app.component_ed.graphs.free(it->data.graph);
                    to_del = true;
                }
                break;

            case component_type::hsm:
                if (display_component_editor(
                      app, app.component_ed.hsms, it->data.hsm, it->id)) {
                    app.component_ed.hsms.free(it->data.hsm);
                    to_del = true;
                }
                break;

            default:
                to_del = true;
                break;
            }

            if (to_del)
                it = app.component_ed.tabs.erase(it);
            else
                ++it;
        }
    }
};

component_editor::component_editor() noexcept
  : grids(16)
  , graphs(16)
  , generics(16)
  , hsms(16)
  , tabs(32, reserve_tag)
{}

void component_editor::display() noexcept
{
    if (!ImGui::Begin(component_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    if (not tabs.empty()) {
        if (ImGui::BeginTabBar("Editors")) {
            auto& app = container_of(this, &application::component_ed);
            component_editor::impl::display_tabs(app);
            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}

static auto find_in_tabs(const vector<component_editor::tab>& v,
                         const component_id                   id) noexcept
  -> vector<component_editor::tab>::const_iterator
{
    const auto is_id = [id](const auto& d) { return id == d.id; };

    return std::ranges::find_if(v, is_id);
}

void component_editor::close(const component_id id) noexcept
{
    if (auto it = find_in_tabs(tabs, id); it != tabs.end()) {
        switch (it->type) {
        case component_type::generic:
            generics.free(it->data.generic);
            break;

        case component_type::grid:
            grids.free(it->data.grid);
            break;

        case component_type::graph:
            graphs.free(it->data.graph);
            break;

        case component_type::hsm:
            hsms.free(it->data.hsm);
            break;

        default:
            break;
        }

        tabs.erase(it);
    }
}

auto log_not_enough_memory = [](auto& title, auto& msg) noexcept {
    title = "Component editor failure";
    msg   = "Fail to allocate more component editor";
};

void component_editor::request_to_open(const component_id id) noexcept
{
    if (auto it = find_in_tabs(tabs, id); it == tabs.end()) {
        auto& app   = container_of(this, &application::component_ed);
        auto& compo = app.mod.components.get<component>(id);

        switch (compo.type) {
        case component_type::generic:
            if (generics.can_alloc(1)) {
                tabs.push_back(component_editor::tab{
                  .id   = id,
                  .type = component_type::generic,
                  .data{ .generic = generics.get_id(
                           generics.alloc(id,
                                          compo,
                                          compo.id.generic_id,
                                          app.mod.generic_components.get(
                                            compo.id.generic_id))) } });
                m_request_to_open = id;
            } else
                app.jn.push(log_level::error, log_not_enough_memory);
            break;

        case component_type::grid:
            if (grids.can_alloc(1)) {
                tabs.push_back(component_editor::tab{
                  .id   = id,
                  .type = component_type::grid,
                  .data{ .grid =
                           grids.get_id(grids.alloc(id, compo.id.grid_id)) } });
                m_request_to_open = id;
            } else
                app.jn.push(log_level::error, log_not_enough_memory);
            break;

        case component_type::graph:
            if (graphs.can_alloc(1)) {
                tabs.push_back(component_editor::tab{
                  .id   = id,
                  .type = component_type::graph,
                  .data{ .graph = graphs.get_id(
                           graphs.alloc(id, compo.id.graph_id)) } });
                m_request_to_open = id;
            } else
                app.jn.push(log_level::error, log_not_enough_memory);
            break;

        case component_type::hsm:
            if (hsms.can_alloc(1)) {
                tabs.push_back(component_editor::tab{
                  .id   = id,
                  .type = component_type::hsm,
                  .data{ .hsm = hsms.get_id(hsms.alloc(
                           id,
                           compo.id.hsm_id,
                           app.mod.hsm_components.get(compo.id.hsm_id))) } });
                m_request_to_open = id;
            } else
                app.jn.push(log_level::error, log_not_enough_memory);
            break;

        case component_type::none:
            break;
        }
    } else {
        m_request_to_open = id;
    }
}

bool component_editor::need_to_open(const component_id id) const noexcept
{
    return m_request_to_open == id;
};

void component_editor::clear_request_to_open() noexcept
{
    m_request_to_open = undefined<component_id>();
}

bool component_editor::is_component_open(const component_id id) const noexcept
{
    return find_in_tabs(tabs, id) != tabs.end();
}

void component_editor::add_generic_component_data() noexcept
{
    auto& app      = container_of(this, &application::component_ed);
    auto& compo    = app.mod.alloc_generic_component();
    auto  compo_id = app.mod.components.get_id(compo);

    request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

void component_editor::add_grid_component_data() noexcept
{
    auto& app      = container_of(this, &application::component_ed);
    auto& compo    = app.mod.alloc_grid_component();
    auto  compo_id = app.mod.components.get_id(compo);

    request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

void component_editor::add_graph_component_data() noexcept
{
    auto& app      = container_of(this, &application::component_ed);
    auto& compo    = app.mod.alloc_graph_component();
    auto  compo_id = app.mod.components.get_id(compo);

    request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

void component_editor::add_hsm_component_data() noexcept
{
    auto& app      = container_of(this, &application::component_ed);
    auto& compo    = app.mod.alloc_hsm_component();
    auto  compo_id = app.mod.components.get_id(compo);

    request_to_open(compo_id);

    app.add_gui_task([&app]() noexcept { app.component_sel.update(); });
}

} // namespace irt
