// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include "application.hpp"
#include "editor.hpp"
#include "imgui.h"
#include "imnodes.h"
#include "internal.hpp"

namespace irt {

enum class component_editor_result_type : u8 {
    do_store_component,
    do_store_file_path,
    do_store_external_source,
    do_save_file,
    do_close_menu
};

using component_editor_result = bitflags<component_editor_result_type>;

template<size_t N>
static const char* make_title(small_string<N>&       out,
                              const std::string_view name,
                              const component_id     compo_id) noexcept
{
    format(out, "{}##{}=compo", name, get_index(compo_id));
    return out.c_str();
}

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

static bool push_back_if_not_find(application&          app,
                                  vector<component_id>& vec,
                                  const component_id    id) noexcept
{
    if (std::ranges::none_of(vec,
                             [id](const auto other) { return id == other; })) {
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

static void update_component_list(application& app,
                                  const component_access& /*ids*/,
                                  component_editor&  ed,
                                  const component_id compo_id) noexcept
{
    app.add_gui_task([&]() noexcept {
        ed.component_list.write([&](auto& vec) noexcept {
            vec.clear();

            app.mod.ids.read([&](const auto& ids, auto) noexcept {
                if (not ids.exists(compo_id))
                    return;

                const auto& compo = ids.components[compo_id];

                switch (compo.type) {
                case component_type::generic:
                    for (const auto& c :
                         ids.generic_components.get(compo.id.generic_id)
                           .children)
                        if (c.type == child_type::component)
                            if (not push_back_if_not_find(
                                  app, vec, c.id.compo_id))
                                break;
                    break;

                case component_type::grid:
                    for (const auto id :
                         ids.grid_components.get(compo.id.grid_id).children())
                        if (not push_back_if_not_find(app, vec, id))
                            break;
                    break;

                case component_type::graph:
                    for (const auto id :
                         ids.graph_components.get(compo.id.graph_id).g.nodes) {
                        const auto c_id =
                          ids.graph_components.get(compo.id.graph_id)
                            .g.node_components[id];
                        if (not push_back_if_not_find(app, vec, c_id))
                            break;
                    }
                    break;

                default:
                    break;
                }
            });
        });
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

static auto combobox_component_id(const component_access&     ids,
                                  const char*                 label,
                                  const vector<component_id>& component_list,
                                  component_id                c_id,
                                  port_id                     p_id) noexcept
  -> std::pair<component_id, port_id>
{
    const char* preview =
      ids.exists(c_id) ? ids.components[c_id].name.c_str() : "-";

    if (ImGui::BeginCombo(label, preview)) {
        if (ImGui::Selectable("-", is_undefined(c_id))) {
            c_id = undefined<component_id>();
            p_id = undefined<port_id>();
        }

        for (const auto id : component_list) {
            if (ids.exists(id)) {
                ImGui::PushID(ordinal(id));
                if (ImGui::Selectable(ids.components[id].name.c_str(),
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
static void show_connection_pack(const modeling& /*mod*/,
                                 const component_access&  ids,
                                 component&               compo,
                                 vector<connection_pack>& pack,
                                 PortGetter&& port_getter_fn) noexcept
{
    auto& port_x_or_y = port_getter_fn(compo);
    auto  it          = pack.begin();
    auto  push_id     = 0;

    while (it != pack.end()) {
        if (not port_x_or_y.exists(it->parent_port) or
            not ids.exists(it->child_component) or
            not port_getter_fn(ids.components[it->child_component])
                  .exists(it->child_port)) {
            it = pack.erase(it);
        } else {
            const auto& child  = ids.components[it->child_component];
            auto        to_del = false;

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

static auto get_source_element(component&                     compo,
                               external_source_definition::id id) noexcept
  -> external_source_definition::source_element&
{
    fatal::ensure(compo.srcs.data.exists(id));

    return compo.srcs.data
      .get<external_source_definition::source_element>()[id];
}

static auto get_name(component&                     compo,
                     external_source_definition::id id) noexcept -> name_str&
{
    return compo.srcs.data.get<name_str>()[id];
}

static auto get_cst_source(component&                     compo,
                           external_source_definition::id id) noexcept
  -> external_source_definition::constant_source&
{
    auto& elems =
      compo.srcs.data.get<external_source_definition::source_element>(id);
    fatal::ensure(elems.type == source_type::constant);

    return elems.cst;
}

static auto get_bin_source(component&                     compo,
                           external_source_definition::id id) noexcept
  -> external_source_definition::binary_source&
{
    auto& elems =
      compo.srcs.data.get<external_source_definition::source_element>(id);
    fatal::ensure(elems.type == source_type::binary_file);

    return elems.bin;
}

static auto get_txt_source(component&                     compo,
                           external_source_definition::id id) noexcept
  -> external_source_definition::text_source&
{
    auto& elems =
      compo.srcs.data.get<external_source_definition::source_element>(id);
    fatal::ensure(elems.type == source_type::text_file);

    return elems.txt;
}

static auto get_rnd_source(component&                     compo,
                           external_source_definition::id id) noexcept
  -> external_source_definition::random_source&
{
    auto& elems =
      compo.srcs.data.get<external_source_definition::source_element>(id);
    fatal::ensure(elems.type == source_type::random);

    return elems.rnd;
}

static bool display_constant_source(
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& elem = get_source_element(tab.compo, id);
    auto& name = get_name(tab.compo, id);
    auto& cst  = get_cst_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          &elem, ImGuiTreeNodeFlags_SpanLabelWidth, "%s", name.c_str())) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            u += ImGui::InputFilteredString("name", name);

            auto size = cst.data.size();
            if (ImGui::InputScalar("length", ImGuiDataType_S32, &size)) {
                size = size < 1                     ? 1
                       : size < cst.data.capacity() ? size
                                                    : cst.data.capacity();
                if (size != cst.data.size()) {
                    cst.data.resize(size);
                    ++u;
                }
            }

            const auto columns_sz = 3 < cst.data.size() ? 3 : cst.data.size();
            const auto rows_sz    = (cst.data.size() / columns_sz) +
                                 ((cst.data.size() % columns_sz) > 0 ? 1 : 0);

            const auto columns = static_cast<int>(columns_sz);
            const auto rows    = static_cast<int>(rows_sz);

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
                            if (ImGui::InputDouble("",
                                                   &cst.data[i * columns + j]))
                                ++u;
                            ImGui::PopItemWidth();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();
        ImGui::TreePop();
    }

    return u > 0;
}

static bool display_binary_source(
  application&                         app,
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& elem = get_source_element(tab.compo, id);
    auto& name = get_name(tab.compo, id);
    auto& bin  = get_bin_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          &elem, ImGuiTreeNodeFlags_SpanLabelWidth, "%s", name.c_str())) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            u += ImGui::InputFilteredString("name", name);

            u += app.mod.files.read([&](const auto& fs, auto) noexcept -> int {
                if (const auto* f = fs.file_paths.try_to_get(tab.file.file)) {
                    if (fs.dir_paths.try_to_get(f->parent)) {
                        const auto old = bin.file;
                        bin.file =
                          show_data_file_input(fs, f->parent, bin.file);

                        return old != bin.file;
                    }
                }

                bin.file = undefined<file_path_id>();
                return bin.file != undefined<file_path_id>();
            });

            ImGui::TreePop();
        }
    }

    return u > 0;
}

static bool display_text_source(
  application&                         app,
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& elem = get_source_element(tab.compo, id);
    auto& name = get_name(tab.compo, id);
    auto& txt  = get_txt_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          &elem, ImGuiTreeNodeFlags_SpanLabelWidth, "%s", name.c_str())) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            u += ImGui::InputFilteredString("name", name);

            u += app.mod.files.read([&](const auto& fs, auto) noexcept -> int {
                if (const auto* f = fs.file_paths.try_to_get(tab.file.file)) {
                    if (fs.dir_paths.try_to_get(f->parent)) {
                        const auto old = txt.file;
                        txt.file =
                          show_data_file_input(fs, f->parent, txt.file);

                        return old != txt.file;
                    }
                }

                txt.file = undefined<file_path_id>();
                return txt.file != undefined<file_path_id>();
            });

            ImGui::TreePop();
        }
    }

    return u > 0;
}

static bool display_random_source(
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& elem = get_source_element(tab.compo, id);
    auto& name = get_name(tab.compo, id);
    auto& rnd  = get_rnd_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          &elem, ImGuiTreeNodeFlags_SpanLabelWidth, "%s", name.c_str())) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            u += ImGui::InputFilteredString("name", name);
            u += show_random_distribution_input(rnd);
        }

        ImGui::TreePop();
    }

    return u > 0;
}

static component_editor_result display_external_source(
  application&           app,
  component_editor::tab& tab) noexcept
{
    const auto  do_nothing = component_editor_result{};
    static auto do_srcs    = component_editor_result{
        component_editor_result_type::do_store_external_source
    };

    auto u = 0;

    if (ImGui::TreeNodeEx("Sources", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (tab.compo.srcs.data.empty())
            ImGui::TextUnformatted("No source defined");

        for (const auto id : tab.compo.srcs.data) {
            switch (get_source_element(tab.compo, id).type) {
            case source_type::constant:
                u += display_constant_source(tab, id);
                break;

            case source_type::binary_file:
                u += display_binary_source(app, tab, id);
                break;

            case source_type::text_file:
                u += display_text_source(app, tab, id);
                break;

            case source_type::random:
                u += display_random_source(tab, id);
                break;
            }
        }

        ImGui::TreePop();
    }

    return u > 0 ? do_srcs : do_nothing;
}

static port_id show_input_port(component& gcompo, port_id g_port_id) noexcept
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

static port_id show_output_port(component& gcompo, port_id g_port_id) noexcept
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
             ? graph.g.node_names[get_index(selected)]
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

            if (not graph.g.node_names[idx].empty()) {
                temp = graph.g.node_names[idx];

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

static void show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       grid_component&         grid,
                                       component&              gcompo) noexcept
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

        if (ids.exists(ch_id)) {
            auto& ch = ids.components[ch_id];
            temp = ch.x.exists(p_selected) ? ch.x.get<port_str>(p_selected).sv()
                                           : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(p_selected))) {
                    p_selected = undefined<port_id>();
                }

                for (const auto id : ch.x) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(ch.x.get<port_str>(id).c_str(),
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
            if (auto ret = grid.connect_input(g_port_id, row, col, p_selected);
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
                               ordinal(modeling_errc::
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

static void show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        grid_component&         grid,
                                        component&              gcompo) noexcept
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

        if (ids.exists(ch_id)) {
            auto& ch = ids.components[ch_id];
            temp = ch.y.exists(p_selected) ? ch.y.get<port_str>(p_selected).sv()
                                           : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(p_selected))) {
                    p_selected = undefined<port_id>();
                }

                for (const auto id : ch.y) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(ch.y.get<port_str>(id).c_str(),
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
            if (auto ret = grid.connect_input(g_port_id, row, col, p_selected);
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
                               ordinal(modeling_errc::
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

static bool connect_input(const port_id      g_port_id,
                          const child_id     selected,
                          const port_id      p_selected,
                          const int          pp_selected,
                          generic_component& g,
                          application&       app);

static bool connect_output(const port_id      g_port_id,
                           const child_id     selected,
                           const port_id      p_selected,
                           int                pp_selected,
                           generic_component& g,
                           application&       app);

static void show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       generic_component&      g,
                                       component&              gcompo) noexcept
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
            if (ids.exists(ch->id.compo_id)) {
                auto& sel_compo = ids.components[ch->id.compo_id];

                temp = sel_compo.x.exists(p_selected)
                         ? sel_compo.x.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected  = undefined<port_id>();
                        pp_selected = -1;
                    }

                    for (const auto id : sel_compo.x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(
                              sel_compo.x.get<port_str>(id).c_str(),
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
                        if (ImGui::Selectable(name.c_str(), pp_selected == i)) {
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
                  ordinal(modeling_errc::graph_input_connection_already_exists))
                  m = "Input connection already exists.";
              else if (ec.value() ==
                       ordinal(
                         modeling_errc::graph_input_connection_container_full))
                  m = "Not enough memory to allocate more "
                      "input "
                      "connection.";
          },
          ret.error());

        return false;
    }

    return true;
}

static void show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        generic_component&      g,
                                        component&              gcompo) noexcept
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
            if (ids.exists(ch->id.compo_id)) {
                auto& sel_compo = ids.components[ch->id.compo_id];

                temp = sel_compo.y.exists(p_selected)
                         ? sel_compo.y.get<port_str>(p_selected).sv()
                         : "-";

                if (ImGui::BeginCombo("Port", temp.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(p_selected))) {
                        p_selected  = undefined<port_id>();
                        pp_selected = -1;
                    }

                    for (const auto id : sel_compo.x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(
                              sel_compo.y.get<port_str>(id).c_str(),
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
                        if (ImGui::Selectable(name.c_str(), pp_selected == i)) {
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
                       ordinal(
                         modeling_errc::graph_output_connection_container_full))
                  m = "Not enough memory to allocate more "
                      "output "
                      "connection.";
          },
          ret.error());

        return false;
    }

    return true;
}

static void show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       graph_component&        graph,
                                       component&              gcompo) noexcept
{
    static ImGuiTextFilter filter;
    static port_id         g_port_id  = undefined<port_id>();
    static graph_node_id   selected   = undefined<graph_node_id>();
    static port_id         p_selected = undefined<port_id>();
    static std::string     temp;

    g_port_id = show_input_port(gcompo, g_port_id);

    if (is_defined(g_port_id)) {
        const auto new_selected = show_node_selection(filter, graph, selected);
        if (new_selected != selected) {
            selected   = new_selected;
            p_selected = undefined<port_id>();
        }
    }

    if (is_defined(selected)) {
        const auto sel_compo_id = graph.g.node_components[get_index(selected)];
        if (ids.exists(sel_compo_id)) {
            auto& sel_compo = ids.components[sel_compo_id];
            temp            = sel_compo.x.exists(p_selected)
                                ? sel_compo.x.get<port_str>(p_selected).sv()
                                : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(p_selected))) {
                    p_selected = undefined<port_id>();
                }

                for (const auto id : sel_compo.x) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(sel_compo.x.get<port_str>(id).c_str(),
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

            if (auto ret = graph.connect_input(g_port_id, selected, p_selected);
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
                               ordinal(modeling_errc::
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

static void show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        graph_component&        graph,
                                        component&              gcompo) noexcept
{
    static ImGuiTextFilter filter;
    static port_id         g_port_id  = undefined<port_id>();
    static graph_node_id   selected   = undefined<graph_node_id>();
    static port_id         p_selected = undefined<port_id>();
    static std::string     temp;

    g_port_id = show_output_port(gcompo, g_port_id);

    if (is_defined(g_port_id)) {
        const auto new_selected = show_node_selection(filter, graph, selected);
        if (new_selected != selected) {
            selected   = new_selected;
            p_selected = undefined<port_id>();
        }
    }

    if (is_defined(selected)) {
        const auto sel_compo_id = graph.g.node_components[get_index(selected)];
        if (ids.exists(sel_compo_id)) {
            auto& sel_compo = ids.components[sel_compo_id];
            temp            = sel_compo.y.exists(p_selected)
                                ? sel_compo.y.get<port_str>(p_selected).sv()
                                : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(p_selected))) {
                    p_selected = undefined<port_id>();
                }

                for (const auto id : sel_compo.x) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(sel_compo.y.get<port_str>(id).c_str(),
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

static void show_input_connections(application&            app,
                                   const component_access& ids,
                                   generic_component&      g,
                                   component&              c) noexcept
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
                if (ids.exists(child->id.compo_id)) {
                    auto& sub_compo = ids.components[child->id.compo_id];
                    if (sub_compo.x.exists(con.port.compo)) {
                        ImGui::TextFormat(
                          "{} connected to component {} ({}) "
                          "port "
                          "{}\n",
                          c.x.get<port_str>(con.x).sv(),
                          g.children_names[get_index(con.dst)].sv(),
                          ordinal(con.dst),
                          sub_compo.x.get<port_str>(con.port.compo).sv());
                    } else
                        to_del = g.input_connections.get_id(con);
                } else
                    to_del = g.input_connections.get_id(con);
            } else {
                ImGui::TextFormat("{} connected to component {} ({}) port {}\n",
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

    show_input_connections_new(app, ids, g, c);
}

static void show_output_connections(application&            app,
                                    const component_access& ids,
                                    generic_component&      g,
                                    component&              c) noexcept
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
                if (ids.exists(child->id.compo_id)) {
                    auto& sub_compo = ids.components[child->id.compo_id];
                    if (sub_compo.x.exists(con.port.compo)) {
                        ImGui::TextFormat(
                          "{} connected to component {} ({}) "
                          "port "
                          "{}\n",
                          c.x.get<port_str>(con.y).sv(),
                          g.children_names[get_index(con.src)].sv(),
                          ordinal(con.src),
                          sub_compo.y.get<port_str>(con.port.compo).sv());
                    } else
                        to_del = g.output_connections.get_id(con);
                } else
                    to_del = g.output_connections.get_id(con);
            } else {
                ImGui::TextFormat("{} connected to component {} ({}) port {}\n",
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

    show_output_connections_new(app, ids, g, c);
}

static void show_input_connections(application&            app,
                                   const component_access& ids,
                                   grid_component&         g,
                                   component&              c) noexcept
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

            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];
                if (sub_compo.x.exists(con.id)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {}x{} port {}\n",
                                      c.x.get<port_str>(con.x).sv(),
                                      con.row,
                                      con.col,
                                      sub_compo.x.get<port_str>(con.id).sv());
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

    show_input_connections_new(app, ids, g, c);
}

static void show_output_connections(application&            app,
                                    const component_access& ids,
                                    grid_component&         g,
                                    component&              c) noexcept
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

            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];

                if (sub_compo.y.exists(con.id)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {}x{} port {}\n",
                                      c.y.get<port_str>(con.y).sv(),
                                      con.row,
                                      con.col,
                                      sub_compo.y.get<port_str>(con.id).sv());
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

    show_output_connections_new(app, ids, g, c);
}

static void show_input_connections(application&            app,
                                   const component_access& ids,
                                   graph_component&        g,
                                   component&              c) noexcept
{
    std::optional<input_connection_id> to_del;

    for (auto& con : g.input_connections) {
        ImGui::PushID(&con);

        if (ImGui::Button("X"))
            to_del = g.input_connections.get_id(con);
        ImGui::SameLine();

        if (g.g.nodes.exists(con.v)) {
            auto sub_compo_id = g.g.node_components[get_index(con.v)];
            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];

                if (sub_compo.x.exists(con.id) and c.x.exists(con.x)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {} port "
                                      "{}\n",
                                      c.x.get<port_str>(con.x).sv(),
                                      g.g.node_names[get_index(con.v)],
                                      sub_compo.x.get<port_str>(con.id).sv());
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

    show_input_connections_new(app, ids, g, c);
}

static void show_output_connections(application&            app,
                                    const component_access& ids,
                                    graph_component&        g,
                                    component&              c) noexcept
{
    std::optional<output_connection_id> to_del;

    for (auto& con : g.output_connections) {
        ImGui::PushID(&con);

        if (ImGui::Button("X"))
            to_del = g.output_connections.get_id(con);
        ImGui::SameLine();

        if (g.g.nodes.exists(con.v)) {
            auto sub_compo_id = g.g.node_components[get_index(con.v)];
            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];
                if (sub_compo.y.exists(con.id) and c.y.exists(con.y)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {} port "
                                      "{}\n",
                                      c.y.get<port_str>(con.y).sv(),
                                      g.g.node_names[get_index(con.v)],
                                      sub_compo.y.get<port_str>(con.id).sv());
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

    show_output_connections_new(app, ids, g, c);
}

static void show_input_connections(application&            app,
                                   const component_access& ids,
                                   component&              compo) noexcept
{
    switch (compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id))
            show_input_connections(app, ids, *g, compo);
        break;

    case component_type::grid:
        if (auto* g = ids.grid_components.try_to_get(compo.id.grid_id))
            show_input_connections(app, ids, *g, compo);
        break;

    case component_type::graph:
        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id))
            show_input_connections(app, ids, *g, compo);
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
        break;
    }
}

static void show_output_connections(application&            app,
                                    const component_access& ids,
                                    component&              compo) noexcept
{
    switch (compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if (auto* g = ids.generic_components.try_to_get(compo.id.generic_id))
            show_output_connections(app, ids, *g, compo);
        break;

    case component_type::grid:
        if (auto* g = ids.grid_components.try_to_get(compo.id.grid_id))
            show_output_connections(app, ids, *g, compo);
        break;

    case component_type::graph:
        if (auto* g = ids.graph_components.try_to_get(compo.id.graph_id))
            show_output_connections(app, ids, *g, compo);
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
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

static void show_input_connection_packs(application&            app,
                                        const component_access& ids,
                                        component&              compo) noexcept
{
    auto& ed = app.component_ed;

    show_connection_pack(app.mod,
                         ids,
                         compo,
                         compo.input_connection_pack,
                         [](auto& compo) noexcept -> auto& { return compo.x; });

    static auto p   = undefined<port_id>();
    static auto c_p = undefined<port_id>();
    static auto c   = undefined<component_id>();

    p = combobox_port_id("input port", compo.x, p);

    ed.component_list.read([&](const auto& vec, auto /*version*/) noexcept {
        const auto child =
          combobox_component_id(ids, "child component", vec, c, c_p);
        c   = child.first;
        c_p = ids.exists(c) ? combobox_component_port_id(
                                "child port", ids.components[c].x, child.second)
                            : combobox_component_port_id_empty("child port");

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
    });
}

static void show_output_connection_packs(application&            app,
                                         const component_access& ids,
                                         component&              compo) noexcept
{
    auto& ed = app.component_ed;

    show_connection_pack(app.mod,
                         ids,
                         compo,
                         compo.output_connection_pack,
                         [](auto& compo) noexcept -> auto& { return compo.y; });

    static auto p   = undefined<port_id>();
    static auto c_p = undefined<port_id>();
    static auto c   = undefined<component_id>();

    p = combobox_port_id("output port", compo.y, p);

    ed.component_list.read([&](const auto& vec,
                               const auto /*version*/) noexcept {
        const auto child =
          combobox_component_id(ids, "child component", vec, c, c_p);
        c   = child.first;
        c_p = ids.exists(c) ? combobox_component_port_id(
                                "child port", ids.components[c].y, child.second)
                            : combobox_component_port_id_empty("child port");

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
    });
}

template<typename ComponentEditor>
static component_editor_result display_component_editor_subtable(
  application&           app,
  ComponentEditor&       element,
  component_editor::tab& tab) noexcept
{
    auto&                   ed = app.component_ed;
    component_editor_result action;

    if (ImGui::BeginTable("##ed", 2)) {
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
            if (ImGui::BeginMenu("File")) {
                if (ImGui::BeginMenu("Save")) {
                    action |= app.mod.files.read(
                      [&](const auto& fs,
                          auto) noexcept -> component_editor_result {
                          const auto selected = tab.file_select.combobox(
                            app,
                            fs,
                            tab.reg_id,
                            tab.dir_id,
                            tab.file_id,
                            file_path::file_type::component_file,
                            file_selector::flags(
                              file_selector::flag::show_save_button,
                              file_selector::flag::show_cancel_button));

                          tab.reg_id  = selected.reg_id;
                          tab.dir_id  = selected.dir_id;
                          tab.file_id = selected.file_id;
                          component_editor_result ret;

                          if (selected.save) {
                              ret |= component_editor_result_type::do_save_file;
                              ret |=
                                component_editor_result_type::do_close_menu;
                          }

                          if (selected.close)
                              ret |=
                                component_editor_result_type::do_close_menu;

                          return ret;
                      });

                    if (action[component_editor_result_type::do_close_menu])
                        ImGui::CloseCurrentPopup();
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Description")) {
                    const auto height = 20 * ImGui::GetTextLineHeight();
                    const auto width  = height;

                    ImGui::InputSmallStringMultiline(
                      "##description",
                      tab.desc,
                      ImVec2(width, height),
                      ImGuiInputTextFlags_AllowTabInput);

                    const auto size = ImGui::ComputeButtonSize(2);
                    if (ImGui::Button("Save", size)) {
                        action |=
                          component_editor_result_type::do_store_component;
                        action |= component_editor_result_type::do_save_file;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", size))
                        ImGui::CloseCurrentPopup();

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Sources")) {
                if (ImGui::BeginMenu("New")) {
                    const auto size =
                      ImVec2(ImGui::CalcTextSize("new binary file").x +
                               ImGui::GetStyle().FramePadding.x * 2.0f,
                             0);

                    if (not tab.compo.srcs.data.can_alloc(1))
                        ImGui::TextUnformatted("Not Enough memor");

                    if (ImGui::Button("new constant", size))
                        app.add_gui_task([&app, compo_id = tab.id]() noexcept {
                            app.mod.ids.write([&compo_id](auto& ids) noexcept {
                                if (not ids.exists(compo_id))
                                    return;
                                auto& compo = ids.components[compo_id];
                                compo.srcs.alloc_constant_source();
                            });
                        });

                    if (ImGui::Button("new binary file", size))
                        app.add_gui_task([&app, compo_id = tab.id]() noexcept {
                            app.mod.ids.write([&compo_id](auto& ids) noexcept {
                                if (not ids.exists(compo_id))
                                    return;
                                auto& compo = ids.components[compo_id];
                                compo.srcs.alloc_constant_source();
                            });
                        });

                    if (ImGui::Button("new text file", size))
                        app.add_gui_task([&app, compo_id = tab.id]() noexcept {
                            app.mod.ids.write([&compo_id](auto& ids) noexcept {
                                if (not ids.exists(compo_id))
                                    return;
                                auto& compo = ids.components[compo_id];
                                compo.srcs.alloc_constant_source();
                            });
                        });

                    if (ImGui::Button("new random", size))
                        app.add_gui_task([&app, compo_id = tab.id]() noexcept {
                            app.mod.ids.write([&compo_id](auto& ids) noexcept {
                                if (not ids.exists(compo_id))
                                    return;
                                auto& compo = ids.components[compo_id];
                                compo.srcs.alloc_constant_source();
                            });
                        });

                    ImGui::EndMenu();
                }

                if (not tab.compo.srcs.data.empty()) {
                    ImGui::Separator();
                    action |= display_external_source(app, tab);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("i/o")) {
                if (tab.compo.type == component_type::hsm) {
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

                    if (tab.compo.x.can_alloc(1) and
                        ImGui::Button("Input port", size)) {
                        app.add_gui_task([&app, cid = tab.id]() {
                            app.mod.ids.write([&](auto& ids) noexcept {
                                if (not ids.exists(cid))
                                    return;

                                auto& compo = ids.components[cid];
                                auto  id    = compo.x.alloc_id();
                                compo.x.template get<port_str>(id) = "in";
                                compo.x.template get<port_option>(id) =
                                  port_option::classic;
                                compo.x.template get<position>(id).reset();
                            });
                        });
                    }

                    if (tab.compo.y.can_alloc(1) and
                        ImGui::Button("Output port", size)) {
                        app.add_gui_task([&app, cid = tab.id]() {
                            app.mod.ids.write([&](auto& ids) noexcept {
                                if (not ids.exists(cid))
                                    return;

                                auto& compo = ids.components[cid];
                                auto  id    = compo.y.alloc_id();
                                compo.y.template get<port_str>(id) = "out";
                                compo.y.template get<port_option>(id) =
                                  port_option::classic;
                                compo.y.template get<position>(id).reset();
                            });
                        });
                    }

                    if (ImGui::Button("refresh i/o", size)) {
                        app.mod.ids.read([&](const auto& ids, auto) noexcept {
                            update_component_list(app, ids, ed, tab.id);
                        });
                    }

                    if (not tab.compo.x.empty() or not tab.compo.y.empty()) {
                        if (ImGui::TreeNode("Ports")) {
                            if (not tab.compo.x.empty() and
                                ImGui::TreeNode("Input port")) {
                                show_input_ports(tab.compo);
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output port")) {
                                show_output_ports(tab.compo);
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNode("Connections")) {
                            if (not tab.compo.x.empty() and
                                ImGui::TreeNode("Input connection")) {
                                app.mod.ids.read([&](const auto& ids,
                                                     auto) noexcept {
                                    show_input_connections(app, ids, tab.compo);
                                });
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output connection")) {
                                app.mod.ids.read(
                                  [&](const auto& ids, auto) noexcept {
                                      show_output_connections(
                                        app, ids, tab.compo);
                                  });
                                ImGui::TreePop();
                            }

                            if (not tab.compo.x.empty() and
                                ImGui::TreeNode("Input Connection pack")) {
                                app.mod.ids.read(
                                  [&](const auto& ids, auto) noexcept {
                                      show_input_connection_packs(
                                        app, ids, tab.compo);
                                  });
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output Connection pack")) {
                                app.mod.ids.read(
                                  [&](const auto& ids, auto) noexcept {
                                      show_output_connection_packs(
                                        app, ids, tab.compo);
                                  });
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

        name_str copy_name = tab.compo.name;
        if (ImGui::InputFilteredString("Name", copy_name)) {
            app.component_ed.request_to_open(tab.id);
            tab.compo.name = copy_name;
            action |= component_editor_result_type::do_store_component;
        }

        if (app.mod.ids.read([&](const auto& ids, auto) noexcept -> bool {
                return element.show_selected_nodes(ed, ids, tab.compo);
            }))
            action |= component_editor_result_type::do_store_component;

        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (app.mod.ids.read([&](const auto& ids, auto) noexcept -> bool {
                return element.show(ed, ids, tab.compo);
            }))
            action = component_editor_result_type::do_store_component;

        ImGui::EndTable();
    }

    return action;
}

template<typename T, typename ID>
static auto display_component_editor(component_editor&      ed,
                                     data_array<T, ID>&     data,
                                     const ID               id,
                                     component_editor::tab& tab) noexcept
  -> component_editor::show_result_t
{
    auto& app = container_of(&ed, &application::component_ed);

    const auto do_close = app.mod.ids.read(
      [&](const auto& ids, const auto version) noexcept -> bool {
          if (version != tab.version) {
              tab.version = version;

              if (ids.exists(tab.id)) {
                  tab.compo = ids.components[tab.id];
                  tab.file  = ids.component_file_paths[tab.id];
                  tab.desc  = ids.component_descriptions[tab.id];
                  return false;
              }

              return true;
          }

          return false;
      });

    if (do_close)
        return component_editor::show_result_t::request_to_close;

    if (app.component_ed.need_to_open(tab.id))
        app.component_ed.clear_request_to_open();

    if (not tab.is_dock_init) {
        ImGui::SetNextWindowDockID(app.get_main_dock_id());
        tab.is_dock_init = true;
    }

    bool              is_open = true;
    small_string<127> title;

    if (not ImGui::Begin(make_title(title, tab.compo.name.sv(), tab.id),
                         &is_open)) {
        ImGui::End();
        return is_open ? component_editor::show_result_t::success
                       : component_editor::show_result_t::request_to_close;
    }

    component_editor_result action;

    if (auto* element = data.try_to_get(id)) {
        action |= display_component_editor_subtable(app, *element, tab);

        if (is_defined(tab.file.file)) {
            const auto is_valid_filename =
              app.mod.files.read([&](const auto& fs, auto) noexcept -> bool {
                  if (const auto* f = fs.file_paths.try_to_get(tab.file.file))
                      return is_valid_irt_filename(f->path.sv());

                  return false;
              });

            if (is_valid_filename and
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
                action |= component_editor_result_type::do_save_file;
            }
        }

        if (action.any()) {
            app.add_gui_task([&app, &tab, action]() {
                app.mod.ids.write([&](auto& ids) {
                    if (not ids.exists(tab.id))
                        return;

                    if (action[component_editor_result_type::do_save_file]) {
                        ids.components[tab.id]             = tab.compo;
                        ids.component_file_paths[tab.id]   = tab.file;
                        ids.component_descriptions[tab.id] = tab.desc;
                        ids.components[tab.id]             = tab.compo;

                        app.mod.files.read([&](const auto& fs, auto) noexcept {
                            if (auto ret = app.mod.save(ids, fs, tab.id);
                                not ret) {
                                app.jn.push(
                                  log_level::error,
                                  [&](auto& title, auto& msg) {
                                      title = "Component save error";
                                      format(msg,
                                             "Fail to save {} (part: {} {})",
                                             tab.compo.name.sv(),
                                             ordinal(ret.error().cat()),
                                             ret.error().value());
                                  });
                            } else {
                                app.jn.push(log_level::notice,
                                            [&](auto& title, auto& msg) {
                                                title = "Component save";
                                                format(msg,
                                                       "Save {} success",
                                                       tab.compo.name.sv());
                                            });
                            }
                        });

                        return;
                    }

                    if (action
                          [component_editor_result_type::do_store_component]) {
                        ids.components[tab.id] = tab.compo;
                    }

                    if (action
                          [component_editor_result_type::do_store_file_path]) {
                        ids.component_file_paths[tab.id]   = tab.file;
                        ids.component_descriptions[tab.id] = tab.desc;
                    }

                    if (action[component_editor_result_type::
                                 do_store_external_source]) {
                        ids.components[tab.id] = tab.compo;
                    }
                });
            });

            app.component_sel.task_update();
        }
    }

    ImGui::End();

    return is_open ? component_editor::show_result_t::success
                   : component_editor::show_result_t::request_to_close;
}

auto component_editor::display_tab_content(tab& t) noexcept -> show_result_t
{
    auto& app = container_of(this, &application::component_ed);

    show_result_t ret = show_result_t::success;

    switch (t.type) {
    case component_type::generic:
        if (ret =
              display_component_editor(*this, app.generics, t.data.generic, t);
            ret == component_editor::show_result_t::request_to_close) {
            app.generics.free(t.data.generic);
        }
        break;

    case component_type::grid:
        if (ret = display_component_editor(*this, app.grids, t.data.grid, t);
            ret == component_editor::show_result_t::request_to_close) {
            app.grids.free(t.data.grid);
        }
        break;

    case component_type::graph:
        if (ret = display_component_editor(*this, app.graphs, t.data.graph, t);
            ret == component_editor::show_result_t::request_to_close) {
            app.graphs.free(t.data.graph);
        }
        break;

    case component_type::hsm:
        if (ret = display_component_editor(*this, app.hsms, t.data.hsm, t);
            ret == component_editor::show_result_t::request_to_close) {
            app.hsms.free(t.data.hsm);
        }
        break;

    case component_type::simulation:
        if (ret = display_component_editor(*this, app.sims, t.data.sim, t);
            ret == component_editor::show_result_t::request_to_close) {
            app.sims.free(t.data.sim);
        }
        break;

    default:
        break;
    }

    return ret;
}

void component_editor::display_tabs() noexcept
{
    auto it = tabs.begin();

    while (it != tabs.end()) {
        auto to_del = false;

        if (display_tab_content(*it) == show_result_t::request_to_close)
            to_del = true;

        if (to_del)
            it = tabs.erase(it);
        else
            ++it;
    }
}

component_editor::component_editor() noexcept
  : tabs(32, reserve_tag)
{}

void component_editor::display() noexcept
{
    if (not tabs.empty())
        display_tabs();
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
    auto& app = container_of(this, &application::component_ed);

    if (auto it = find_in_tabs(tabs, id); it != tabs.end()) {
        switch (it->type) {
        case component_type::generic:
            app.generics.free(it->data.generic);
            break;

        case component_type::grid:
            app.grids.free(it->data.grid);
            break;

        case component_type::graph:
            app.graphs.free(it->data.graph);
            break;

        case component_type::hsm:
            app.hsms.free(it->data.hsm);
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
    auto& app = container_of(this, &application::component_ed);

    app.mod.ids.read([&](const auto& ids, auto) noexcept {
        const auto& compo = ids.components[id];

        if (auto it = find_in_tabs(tabs, id); it == tabs.end()) {
            auto reg_id = undefined<registred_path_id>();
            auto dir_id = undefined<dir_path_id>();
            auto file_id = ids.component_file_paths[id].file;

            app.mod.files.read([&](const auto& fs, const auto /*vers*/) {
                if (const auto* f = fs.file_paths.try_to_get(file_id)) {
                    if (const auto* d = fs.dir_paths.try_to_get(f->parent)) {
                        if (const auto* r =
                              fs.registred_paths.try_to_get(d->parent)) {
                            reg_id = d->parent;
                        }
                        dir_id = f->parent;
                    }
                }
            });

            switch (compo.type) {
            case component_type::generic:
                if (app.generics.can_alloc(1)) {
                    if (auto* t = tabs.emplace_back()) {
                        t->id          = id;
                        t->type        = component_type::generic;
                        t->reg_id      = reg_id;
                        t->dir_id      = dir_id;
                        t->file_id     = file_id;
                        t->data.generic =
                          app.generics.get_id(app.generics.alloc(
                            id,
                            compo,
                            compo.id.generic_id,
                            ids.generic_components.get(compo.id.generic_id)));
                        m_request_to_open = id;
                    }
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::grid:
                if (app.grids.can_alloc(1)) {
                    if (auto* t = tabs.emplace_back()) {
                        t->id          = id;
                        t->type        = component_type::grid;
                        t->reg_id      = reg_id;
                        t->dir_id      = dir_id;
                        t->file_id     = file_id;
                        t->data.grid   = app.grids.get_id(
                          app.grids.alloc(id, compo.id.grid_id));
                        m_request_to_open = id;
                    }
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::graph:
                if (app.graphs.can_alloc(1)) {
                    if (auto* t = tabs.emplace_back()) {
                        t->id          = id;
                        t->type        = component_type::graph;
                        t->reg_id      = reg_id;
                        t->dir_id      = dir_id;
                        t->file_id     = file_id;
                        t->data.graph  = app.graphs.get_id(
                          app.graphs.alloc(id, compo.id.graph_id));
                        m_request_to_open = id;
                    }
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::hsm:
                if (app.hsms.can_alloc(1)) {
                    if (auto* t = tabs.emplace_back()) {
                        t->id             = id;
                        t->type           = component_type::hsm;
                        t->reg_id         = reg_id;
                        t->dir_id         = dir_id;
                        t->file_id        = file_id;
                        t->data.hsm       = app.hsms.get_id(app.hsms.alloc(
                          id,
                          compo.id.hsm_id,
                          ids.hsm_components.get(compo.id.hsm_id)));
                        m_request_to_open = id;
                    }
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::simulation:
                if (app.sims.can_alloc(1)) {
                    if (auto* t = tabs.emplace_back()) {
                        t->id             = id;
                        t->type           = component_type::simulation;
                        t->reg_id         = reg_id;
                        t->dir_id         = dir_id;
                        t->file_id        = file_id;
                        t->data.sim       = app.sims.get_id(app.sims.alloc(
                          id,
                          compo.id.sim_id,
                          ids.sim_components.get(compo.id.sim_id)));
                        m_request_to_open = id;
                    }
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::none:
                break;
            }
        } else {
            small_string<127> title;
            ImGui::SetWindowFocus(make_title(title, compo.name.sv(), id));
        }
    });
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
    auto& app = container_of(this, &application::component_ed);

    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) noexcept {
            if (const auto compo_id = ids.alloc_generic_component();
                is_defined(compo_id)) {
                request_to_open(compo_id);
                app.component_sel.update();
            }
        });
    });
}

void component_editor::add_grid_component_data() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) noexcept {
            if (const auto compo_id = ids.alloc_grid_component();
                is_defined(compo_id)) {
                request_to_open(compo_id);
                app.component_sel.update();
            }
        });
    });
}

void component_editor::add_graph_component_data() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) noexcept {
            if (const auto compo_id = ids.alloc_graph_component();
                is_defined(compo_id)) {
                request_to_open(compo_id);
                app.component_sel.update();
            }
        });
    });
}

void component_editor::add_hsm_component_data() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) noexcept {
            if (const auto compo_id = ids.alloc_hsm_component();
                is_defined(compo_id)) {
                request_to_open(compo_id);
                app.component_sel.update();
            }
        });
    });
}

void component_editor::add_simulation_component_data() noexcept
{
    auto& app = container_of(this, &application::component_ed);

    app.add_gui_task([&]() {
        app.mod.ids.write([&](auto& ids) noexcept {
            if (const auto compo_id = ids.alloc_sim_component();
                is_defined(compo_id)) {
                request_to_open(compo_id);
                app.component_sel.update();
            }
        });
    });
}

} // namespace irt
