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

static small_string<64> make_title(const std::string_view name,
                                   const component_id     compo_id) noexcept
{
    return format_n<64>("{}##{}=compo", name, get_index(compo_id));
}

static bool can_add(const vector<component_id>& vec,
                    const component_id          id) noexcept
{
    return std::ranges::none_of(vec,
                                [id](const auto other) { return id == other; });
}

static bool push_back_if_not_find(application&          app,
                                  vector<component_id>& vec,
                                  const component_id    id) noexcept
{
    if (can_add(vec, id)) {
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

static void update_child_component_task(
  application&                   app,
  const component_editor::tab_id tab_id) noexcept
{
    app.add_gui_task([&app, tab_id]() noexcept {
        if (auto* tab = app.component_ed.tabs.try_to_get(tab_id)) {

            tab->uniq_component_children.write([&](auto& vec) noexcept {
                vec.clear();

                app.mod.ids.read([&](const auto& ids, auto) noexcept {
                    if (not ids.exists(tab->id))
                        return;

                    const auto& compo = ids.components[tab->id];

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
                             ids.grid_components.get(compo.id.grid_id)
                               .children())
                            if (not push_back_if_not_find(app, vec, id))
                                break;
                        break;

                    case component_type::graph:
                        for (const auto id :
                             ids.graph_components.get(compo.id.graph_id)
                               .g.nodes) {
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

                    std::ranges::sort(vec);
                });
            });
        }
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
static bool show_connection_pack(const component_access&  ids,
                                 component_editor::tab&   tab,
                                 vector<connection_pack>& pack,
                                 PortGetter&& port_getter_fn) noexcept
{
    auto& port_x_or_y = port_getter_fn(tab.compo);
    auto  it          = pack.begin();
    auto  push_id     = 0;
    auto  u           = 0;

    while (it != pack.end()) {
        if (not port_x_or_y.exists(it->parent_port) or
            not ids.exists(it->child_component) or
            not port_getter_fn(ids.components[it->child_component])
                  .exists(it->child_port)) {
            it = pack.erase(it);
            ++u;
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

            if (to_del) {
                it = pack.erase(it);
                ++u;
            } else
                ++it;
            ImGui::PopID();
        }
    }

    return u > 0;
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
    auto& name = get_name(tab.compo, id);
    auto& cst  = get_cst_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          format_n<32>("{}##{}", name.sv(), ordinal(id)).c_str(),
          ImGuiTreeNodeFlags_SpanLabelWidth)) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            name_str new_name = name;
            if (ImGui::InputFilteredString("name", new_name)) {
                if (new_name != name) {
                    name = new_name;
                    ++u;
                }
            }

            auto new_size = [&]() {
                auto size = static_cast<int>(cst.data.size());
                if (ImGui::InputInt("length", &size)) {
                    sz new_size = size <= 1         ? 1u
                                  : size > INT8_MAX ? INT8_MAX
                                                    : size;

                    if (new_size != cst.data.size()) {
                        cst.data.resize(new_size, 0.0);
                        ++u;
                    }
                }
                return cst.data.size();
            }();

            if (new_size) {
                const auto columns_sz =
                  3 < cst.data.size() ? 3 : cst.data.size();
                const auto rows_sz =
                  (cst.data.size() / columns_sz) +
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
                              (ImGui::GetContentRegionAvail().x -
                               (2 * ImGui::GetStyle().ItemSpacing.x)) /
                                3.f);

                        int i = 0;
                        int j = 0;

                        ImGui::TableNextRow();
                        for (sz elem = 0; elem < new_size; ++elem) {
                            ImGui::TableSetColumnIndex(j);
                            ImGui::PushID(i * columns + j);
                            ImGui::PushItemWidth(-1);
                            if (ImGui::InputDouble("",
                                                   &cst.data[i * columns + j]))
                                ++u;
                            ImGui::PopItemWidth();
                            ImGui::PopID();

                            ++j;
                            if (j == columns) {
                                j = 0;
                                ImGui::TableNextRow();
                                ++i;
                            }
                        }

                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();
            }
        }

        ImGui::TreePop();
    }

    return u > 0;
}

static bool display_binary_source(
  application&                         app,
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& name = get_name(tab.compo, id);
    auto& bin  = get_bin_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          format_n<32>("{}##{}", name.sv(), ordinal(id)).c_str(),
          ImGuiTreeNodeFlags_SpanLabelWidth)) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            name_str new_name = name;
            if (ImGui::InputFilteredString("name", new_name)) {
                if (new_name != name) {
                    name = new_name;
                    ++u;
                }
            }

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
        }

        ImGui::TreePop();
    }

    return u > 0;
}

static bool display_text_source(
  application&                         app,
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& name = get_name(tab.compo, id);
    auto& txt  = get_txt_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          format_n<32>("{}##{}", name.sv(), ordinal(id)).c_str(),
          ImGuiTreeNodeFlags_SpanLabelWidth)) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            name_str new_name = name;
            if (ImGui::InputFilteredString("name", new_name)) {
                if (name != new_name) {
                    name = new_name;
                    ++u;
                }
            }

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
        }

        ImGui::TreePop();
    }

    return u > 0;
}

static bool display_random_source(
  component_editor::tab&               tab,
  const external_source_definition::id id) noexcept
{
    auto& name = get_name(tab.compo, id);
    auto& rnd  = get_rnd_source(tab.compo, id);

    auto u = 0;

    if (ImGui::TreeNodeEx(
          format_n<32>("{}##{}", name.sv(), ordinal(id)).c_str(),
          ImGuiTreeNodeFlags_SpanLabelWidth)) {
        ImGui::SameLine();

        if (ImGui::SmallButton("x")) {
            tab.compo.srcs.data.free(id);
            ++u;
        } else {
            name_str new_name = name;
            if (ImGui::InputFilteredString("name", new_name)) {
                if (name != new_name) {
                    name = new_name;
                    ++u;
                }
            }
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
    const auto do_nothing = component_editor_result{};
    const auto do_srcs    = component_editor_result{
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
    const auto label =
      std::string{ g.children.try_to_get(selected)
                     ? g.children_names[get_index(selected)].sv()
                     : "-" };

    if (ImGui::BeginCombo("Child", label.c_str())) {
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
                const auto temp = std::string{ g.children_names[idx].sv() };

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
    const auto label = std::string{ graph.g.nodes.exists(selected)
                                      ? graph.g.node_names[get_index(selected)]
                                      : std::string_view{ "-" } };

    if (ImGui::BeginCombo("Node", label.c_str())) {
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
                const auto temp = std::string{ graph.g.node_names[idx] };

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

static bool show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       component_editor::tab&  tab,
                                       grid_component&         grid) noexcept
{
    auto& con = tab.input_grid_con;
    auto  u   = 0;

    con.x = show_input_port(tab.compo, con.x);

    if (is_defined(con.x)) {
        int v[2] = { con.row, con.col };

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

        if (v[0] != con.row or v[1] != con.col) {
            con.row = v[0];
            con.col = v[1];
            con.id  = undefined<port_id>();
        }

        if (grid.is_coord_valid(v[0], v[1])) {
            const auto pos   = grid.pos(con.row, con.col);
            const auto ch_id = grid.children()[pos];

            if (ids.exists(ch_id)) {
                auto&      ch    = ids.components[ch_id];
                const auto label = port_str{ ch.x.exists(con.id)
                                               ? ch.x.get<port_str>(con.id).sv()
                                               : std::string_view("-") };

                if (ImGui::BeginCombo("Port", label.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(con.id))) {
                        con.id = undefined<port_id>();
                    }

                    for (const auto id : ch.x) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(ch.x.get<port_str>(id).c_str(),
                                              con.id == id)) {
                            con.id = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }

            if (is_defined(con.x) and is_defined(con.id)) {
                if (auto ret =
                      grid.connect_input(con.x, con.row, con.col, con.id);
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
                } else {
                    ++u;
                }
            }
        }
    }

    return u > 0;
}

static bool show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        component_editor::tab&  tab,
                                        grid_component&         grid) noexcept
{
    auto& con = tab.output_grid_con;
    auto  u   = 0;

    con.y = show_output_port(tab.compo, con.y);

    if (is_defined(con.y)) {
        int v[2] = { con.row, con.col };

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

        if (v[0] != con.row or v[1] != con.col) {
            con.row = v[0];
            con.col = v[1];
            con.id  = undefined<port_id>();
        }

        if (grid.is_coord_valid(v[0], v[1])) {
            const auto pos   = grid.pos(con.row, con.col);
            const auto ch_id = grid.children()[pos];

            if (ids.exists(ch_id)) {
                auto&      ch    = ids.components[ch_id];
                const auto label = port_str{ ch.y.exists(con.id)
                                               ? ch.y.get<port_str>(con.id).sv()
                                               : std::string_view("-") };

                if (ImGui::BeginCombo("Port", label.c_str())) {
                    if (ImGui::Selectable("-", is_undefined(con.id))) {
                        con.id = undefined<port_id>();
                    }

                    for (const auto id : ch.y) {
                        ImGui::PushID(get_index(id));
                        if (ImGui::Selectable(ch.y.get<port_str>(id).c_str(),
                                              con.id == id)) {
                            con.id = id;
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }

            if (is_defined(con.y) and is_defined(con.id)) {
                if (auto ret =
                      grid.connect_input(con.y, con.row, con.col, con.id);
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
                } else {
                    ++u;
                }
            }
        }
    }

    return u > 0;
}

struct connection_pack_info {
    port_id            g_port_id;
    child_id           selected;
    port_id            p_selected;
    int                pp_selected;
    generic_component& g;
};

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

static bool show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       component_editor::tab&  tab,
                                       generic_component&      g) noexcept
{
    auto& con    = tab.input_gen_con;
    auto  temp   = std::string{};
    auto  filter = ImGuiTextFilter{};
    auto  u      = 0;

    con.x = show_input_port(tab.compo, con.x);

    if (is_defined(con.x)) {
        const auto new_selected = show_node_selection(filter, g, con.dst);
        if (new_selected != con.dst) {
            con.dst = new_selected;
            con.port.clear();
        }

        if (auto* ch = g.children.try_to_get(con.dst)) {
            if (ch->type == child_type::component) {
                if (ids.exists(ch->id.compo_id)) {
                    auto& sel_compo = ids.components[ch->id.compo_id];

                    temp = sel_compo.x.exists(con.port.compo)
                             ? sel_compo.x.get<port_str>(con.port.compo).sv()
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-",
                                              is_undefined(con.port.compo))) {
                            con.port.clear();
                        }

                        for (const auto id : sel_compo.x) {
                            ImGui::PushID(get_index(id));
                            if (ImGui::Selectable(
                                  sel_compo.x.get<port_str>(id).c_str(),
                                  con.port.compo == id)) {
                                con.port.compo = id;
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
                    temp = 0 <= con.port.model and con.port.model < dyn_size
                             ? dyn_port[con.port.model]
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", con.port.model == -1)) {
                            con.port.clear();
                        }

                        for (auto i = 0, e = (int)dyn_port.size(); i < e; ++i) {
                            ImGui::PushID(i);
                            name_str name(dyn_port[i]);
                            if (ImGui::Selectable(name.c_str(),
                                                  con.port.model == i)) {
                                con.port.model = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }
        }

        if (is_defined(con.x) and is_defined(con.dst) and
            (is_defined(con.port.compo) or con.port.model >= 0)) {
            if (connect_input(
                  con.x, con.dst, con.port.compo, con.port.model, g, app)) {
                con.clear();
                ++u;
            }
        }
    } else {
        con.clear();
    }

    return u > 0;
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

static bool show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        component_editor::tab&  tab,
                                        generic_component&      g) noexcept
{
    auto& con    = tab.output_gen_con;
    auto  filter = ImGuiTextFilter{};
    auto  temp   = std::string{};
    auto  u      = 0;

    con.y = show_output_port(tab.compo, con.y);

    if (is_defined(con.y)) {
        const auto new_selected = show_node_selection(filter, g, con.src);
        if (new_selected != con.src) {
            con.src = new_selected;
            con.port.clear();
        }

        if (auto* ch = g.children.try_to_get(con.src)) {
            if (ch->type == child_type::component) {
                if (ids.exists(ch->id.compo_id)) {
                    auto& sel_compo = ids.components[ch->id.compo_id];

                    temp = sel_compo.y.exists(con.port.compo)
                             ? sel_compo.y.get<port_str>(con.port.compo).sv()
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-",
                                              is_undefined(con.port.compo))) {
                            con.port.clear();
                        }

                        for (const auto id : sel_compo.x) {
                            ImGui::PushID(get_index(id));
                            if (ImGui::Selectable(
                                  sel_compo.y.get<port_str>(id).c_str(),
                                  con.port.compo == id)) {
                                con.port.compo = id;
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
                    temp = 0 <= con.port.model and con.port.model < dyn_size
                             ? dyn_port[con.port.model]
                             : "-";

                    if (ImGui::BeginCombo("Port", temp.c_str())) {
                        if (ImGui::Selectable("-", con.port.model == -1)) {
                            con.port.clear();
                        }

                        for (auto i = 0, e = (int)dyn_port.size(); i < e; ++i) {
                            ImGui::PushID(i);
                            name_str name(dyn_port[i]);
                            if (ImGui::Selectable(name.c_str(),
                                                  con.port.model == i)) {
                                con.port.model = i;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }
            }
        }

        if (is_defined(con.y) and is_defined(con.src) and
            (is_defined(con.port.compo) or con.port.model >= 0)) {
            if (connect_output(
                  con.y, con.src, con.port.compo, con.port.model, g, app)) {
                con.clear();
                ++u;
            }
        }
    } else {
        con.clear();
    }

    return u > 0;
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

static bool show_input_connections_new(application&            app,
                                       const component_access& ids,
                                       component_editor::tab&  tab,
                                       graph_component&        graph) noexcept
{
    auto& con    = tab.input_graph_con;
    auto  filter = ImGuiTextFilter{};
    auto  temp   = std::string{};
    auto  u      = 0;

    con.x = show_input_port(tab.compo, con.x);

    if (is_defined(con.x)) {
        const auto new_selected = show_node_selection(filter, graph, con.v);
        if (new_selected != con.v) {
            con.v  = new_selected;
            con.id = undefined<port_id>();
            ++u;
        }
    }

    if (is_defined(con.v)) {
        const auto sel_compo_id = graph.g.node_components[get_index(con.v)];
        if (ids.exists(sel_compo_id)) {
            auto& sel_compo = ids.components[sel_compo_id];
            temp            = sel_compo.x.exists(con.id)
                                ? sel_compo.x.get<port_str>(con.id).sv()
                                : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(con.id))) {
                    con.id = undefined<port_id>();
                    ++u;
                }

                for (const auto id : sel_compo.x) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(sel_compo.x.get<port_str>(id).c_str(),
                                          con.id == id)) {
                        con.id = id;
                        ++u;
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
        }

        if (is_defined(con.x) and is_defined(con.v) and is_defined(con.id)) {
            if (auto ret = graph.connect_input(con.x, con.v, con.id); not ret) {
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
            } else {
                ++u;
            }
        }
    }

    return u > 0;
}

static bool show_output_connections_new(application&            app,
                                        const component_access& ids,
                                        component_editor::tab&  tab,
                                        graph_component&        graph) noexcept
{
    auto& con    = tab.output_graph_con;
    auto  filter = ImGuiTextFilter{};
    auto  temp   = std::string{};
    auto  u      = 0;

    con.y = show_output_port(tab.compo, con.y);

    if (is_defined(con.y)) {
        const auto new_selected = show_node_selection(filter, graph, con.v);
        if (new_selected != con.v) {
            con.v  = new_selected;
            con.id = undefined<port_id>();
        }
    }

    if (is_defined(con.id)) {
        const auto sel_compo_id = graph.g.node_components[get_index(con.v)];

        if (ids.exists(sel_compo_id)) {
            auto& sel_compo = ids.components[sel_compo_id];
            temp            = sel_compo.y.exists(con.id)
                                ? sel_compo.y.get<port_str>(con.id).sv()
                                : "-";

            if (ImGui::BeginCombo("Port", temp.c_str())) {
                if (ImGui::Selectable("-", is_undefined(con.id))) {
                    con.id = undefined<port_id>();
                    ++u;
                }

                for (const auto id : sel_compo.x) {
                    ImGui::PushID(get_index(id));
                    if (ImGui::Selectable(sel_compo.y.get<port_str>(id).c_str(),
                                          con.id == id)) {
                        con.id = id;
                        ++u;
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
        }

        if (is_defined(con.y) and is_defined(con.v) and is_defined(con.id)) {
            if (auto ret = graph.connect_output(con.y, con.v, con.id);
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
            } else {
                ++u;
            }
        }
    }

    return u > 0;
}

static bool show_input_connections(application&            app,
                                   const component_access& ids,
                                   component_editor::tab&  tab,
                                   generic_component&      g) noexcept
{
    auto to_del = input_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.input_connections) {
        const auto con_id = g.input_connections.get_id(con);

        ImGui::PushID(ordinal(con_id));
        if (auto* child = g.children.try_to_get(con.dst);
            child and tab.compo.x.exists(con.x)) {

            if (ImGui::Button("X"))
                to_del = con_id;

            ImGui::SameLine();

            if (child->type == child_type::component) {
                if (ids.exists(child->id.compo_id)) {
                    const auto& sub_compo = ids.components[child->id.compo_id];

                    if (sub_compo.x.exists(con.port.compo)) {
                        ImGui::TextFormat(
                          "{} connected to {}:{}",
                          tab.compo.x.get<port_str>(con.x).sv(),
                          g.children_names[get_index(con.dst)].sv(),
                          sub_compo.x.get<port_str>(con.port.compo).sv());
                    } else
                        to_del = con_id;
                } else
                    to_del = con_id;
            } else {
                const auto d_type = child->id.mdl_type;
                const auto xnames = get_input_port_names(d_type);

                ImGui::TextFormat("{} connected to {}:{}",
                                  tab.compo.x.get<port_str>(con.x).sv(),
                                  g.children_names[get_index(con.dst)].sv(),
                                  xnames[con.port.model]);
            }
        } else {
            to_del = con_id;
        }

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.input_connections.free(to_del);
        ++u;
    }

    u += show_input_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_output_connections(application&            app,
                                    const component_access& ids,
                                    component_editor::tab&  tab,
                                    generic_component&      g) noexcept
{
    auto to_del = output_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.output_connections) {
        const auto con_id = g.output_connections.get_id(con);
        ImGui::PushID(ordinal(con_id));

        if (auto* child = g.children.try_to_get(con.src);
            child and tab.compo.y.exists(con.y)) {

            if (ImGui::Button("X"))
                to_del = con_id;
            ImGui::SameLine();

            if (child->type == child_type::component) {
                if (ids.exists(child->id.compo_id)) {
                    const auto& sub_compo = ids.components[child->id.compo_id];

                    if (sub_compo.x.exists(con.port.compo)) {
                        ImGui::TextFormat(
                          "{} connected to component {}:{}",
                          tab.compo.x.get<port_str>(con.y).sv(),
                          g.children_names[get_index(con.src)].sv(),
                          sub_compo.y.get<port_str>(con.port.compo).sv());
                    } else
                        to_del = con_id;
                } else
                    to_del = con_id;
            } else {
                const auto d_type = child->id.mdl_type;
                const auto ynames = get_output_port_names(d_type);

                ImGui::TextFormat("{} connected to {}:{}",
                                  tab.compo.y.get<port_str>(con.y).sv(),
                                  g.children_names[get_index(con.src)].sv(),
                                  ynames[con.port.model]);
            }
        } else
            to_del = con_id;

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.output_connections.free(to_del);
        ++u;
    }

    u += show_output_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_input_connections(application&            app,
                                   const component_access& ids,
                                   component_editor::tab&  tab,
                                   grid_component&         g) noexcept
{
    auto to_del = input_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.input_connections) {
        const auto con_id = g.input_connections.get_id(con);
        ImGui::PushID(ordinal(con_id));

        if (ImGui::Button("X"))
            to_del = con_id;
        ImGui::SameLine();

        if (g.is_coord_valid(con.row, con.col)) {
            const auto pos          = g.pos(con.row, con.col);
            auto       sub_compo_id = g.children()[pos];

            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];
                if (sub_compo.x.exists(con.id)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {}x{} port {}\n",
                                      tab.compo.x.get<port_str>(con.x).sv(),
                                      con.row,
                                      con.col,
                                      sub_compo.x.get<port_str>(con.id).sv());
                } else
                    to_del = con_id;
            } else
                to_del = con_id;
        } else
            to_del = con_id;

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.input_connections.free(to_del);
        ++u;
    }

    u += show_input_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_output_connections(application&            app,
                                    const component_access& ids,
                                    component_editor::tab&  tab,
                                    grid_component&         g) noexcept
{
    auto to_del = output_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.output_connections) {
        const auto con_id = g.output_connections.get_id(con);
        ImGui::PushID(ordinal(con_id));

        ImGui::SameLine();
        if (ImGui::Button("X"))
            to_del = con_id;

        if (g.is_coord_valid(con.row, con.col)) {
            const auto pos          = g.pos(con.row, con.col);
            auto       sub_compo_id = g.children()[pos];

            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];

                if (sub_compo.y.exists(con.id)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {}x{} port {}\n",
                                      tab.compo.y.get<port_str>(con.y).sv(),
                                      con.row,
                                      con.col,
                                      sub_compo.y.get<port_str>(con.id).sv());
                } else
                    to_del = con_id;
            } else
                to_del = con_id;
        } else
            to_del = con_id;

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.output_connections.free(to_del);
        ++u;
    }

    u += show_output_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_input_connections(application&            app,
                                   const component_access& ids,
                                   component_editor::tab&  tab,
                                   graph_component&        g) noexcept
{
    auto to_del = input_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.input_connections) {
        const auto con_id = g.input_connections.get_id(con);
        ImGui::PushID(ordinal(con_id));

        if (ImGui::Button("X"))
            to_del = con_id;
        ImGui::SameLine();

        if (g.g.nodes.exists(con.v)) {
            auto sub_compo_id = g.g.node_components[get_index(con.v)];
            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];

                if (sub_compo.x.exists(con.id) and tab.compo.x.exists(con.x)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {} port "
                                      "{}\n",
                                      tab.compo.x.get<port_str>(con.x).sv(),
                                      g.g.node_names[get_index(con.v)],
                                      sub_compo.x.get<port_str>(con.id).sv());
                } else
                    to_del = con_id;
            } else
                to_del = con_id;
        } else
            to_del = con_id;

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.input_connections.free(to_del);
        ++u;
    }

    u += show_input_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_output_connections(application&            app,
                                    const component_access& ids,
                                    component_editor::tab&  tab,
                                    graph_component&        g) noexcept
{
    auto to_del = output_connection_id{ 0 };
    auto u      = 0;

    for (auto& con : g.output_connections) {
        const auto con_id = g.output_connections.get_id(con);
        ImGui::PushID(ordinal(con_id));

        if (ImGui::Button("X"))
            to_del = con_id;
        ImGui::SameLine();

        if (g.g.nodes.exists(con.v)) {
            auto sub_compo_id = g.g.node_components[get_index(con.v)];
            if (ids.exists(sub_compo_id)) {
                auto& sub_compo = ids.components[sub_compo_id];
                if (sub_compo.y.exists(con.id) and tab.compo.y.exists(con.y)) {
                    ImGui::SetNextItemAllowOverlap();

                    ImGui::TextFormat("{} connected to node {} port "
                                      "{}\n",
                                      tab.compo.y.get<port_str>(con.y).sv(),
                                      g.g.node_names[get_index(con.v)],
                                      sub_compo.y.get<port_str>(con.id).sv());
                } else
                    to_del = con_id;
            } else
                to_del = con_id;
        } else
            to_del = con_id;

        ImGui::PopID();
    }

    if (is_defined(to_del)) {
        g.output_connections.free(to_del);
        ++u;
    }

    u += show_output_connections_new(app, ids, tab, g);

    return u > 0;
}

static bool show_input_connections(application&            app,
                                   const component_access& ids,
                                   component_editor::tab&  tab) noexcept
{
    switch (tab.compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if (auto* g =
              ids.generic_components.try_to_get(tab.compo.id.generic_id))
            return show_input_connections(app, ids, tab, *g);
        break;

    case component_type::grid:
        if (auto* g = ids.grid_components.try_to_get(tab.compo.id.grid_id))
            return show_input_connections(app, ids, tab, *g);
        break;

    case component_type::graph:
        if (auto* g = ids.graph_components.try_to_get(tab.compo.id.graph_id))
            return show_input_connections(app, ids, tab, *g);
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
        break;
    }

    return false;
}

static bool show_output_connections(application&            app,
                                    const component_access& ids,
                                    component_editor::tab&  tab) noexcept
{
    switch (tab.compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if (auto* g =
              ids.generic_components.try_to_get(tab.compo.id.generic_id))
            return show_output_connections(app, ids, tab, *g);
        break;

    case component_type::grid:
        if (auto* g = ids.grid_components.try_to_get(tab.compo.id.grid_id))
            return show_output_connections(app, ids, tab, *g);
        break;

    case component_type::graph:
        if (auto* g = ids.graph_components.try_to_get(tab.compo.id.graph_id))
            return show_output_connections(app, ids, tab, *g);
        break;

    case component_type::hsm:
        break;

    case component_type::simulation:
        break;
    }

    return false;
}

static bool show_input_ports(component& compo) noexcept
{
    auto to_del = std::optional<port_id>{};
    auto u      = 0;

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
                    ++u;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    if (to_del.has_value()) {
        compo.x.free(*to_del);
        ++u;
    }

    return u > 0;
}

static bool show_output_ports(component& compo) noexcept
{
    auto to_del = std::optional<port_id>{};
    auto u      = 0;

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
                    ++u;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    if (to_del.has_value()) {
        compo.y.free(*to_del);
        ++u;
    }

    return u > 0;
}

static bool show_input_connection_packs(const component_access& ids,
                                        component_editor::tab&  tab) noexcept
{
    auto u = 0;

    u += show_connection_pack(
      ids,
      tab,
      tab.compo.input_connection_pack,
      [](auto& compo) noexcept -> auto& { return compo.x; });

    auto& con = tab.input_pack_con;
    con.parent_port =
      combobox_port_id("input port", tab.compo.x, con.parent_port);

    if (is_defined(con.parent_port)) {
        tab.uniq_component_children.read([&](const auto& vec,
                                             auto /*version*/) noexcept {
            const auto child = combobox_component_id(
              ids, "child component", vec, con.child_component, con.child_port);

            con.child_component = child.first;
            con.child_port      = ids.exists(con.child_component)
                                    ? combobox_component_port_id(
                                   "child port",
                                   ids.components[con.child_component].x,
                                   child.second)
                                    : combobox_component_port_id_empty("child port");

            if (is_defined(con.child_component) and
                is_defined(con.child_port) and is_defined(con.parent_port) and
                std::ranges::none_of(tab.compo.input_connection_pack,
                                     [&](const auto& c) { return con == c; })) {
                tab.compo.input_connection_pack.push_back(con);
                con.clear();
                ++u;
            }
        });
    } else {
        con.clear();
    }

    return u > 0;
}

static bool show_output_connection_packs(const component_access& ids,
                                         component_editor::tab&  tab) noexcept
{
    auto u = 0;

    u += show_connection_pack(
      ids,
      tab,
      tab.compo.output_connection_pack,
      [](auto& compo) noexcept -> auto& { return compo.y; });

    auto& con = tab.output_pack_con;
    con.parent_port =
      combobox_port_id("output port", tab.compo.y, con.parent_port);

    if (is_defined(con.parent_port)) {
        tab.uniq_component_children.read([&](const auto& vec, auto) noexcept {
            const auto child = combobox_component_id(
              ids, "child component", vec, con.child_component, con.child_port);

            con.child_component = child.first;
            con.child_port      = ids.exists(con.child_component)
                                    ? combobox_component_port_id(
                                   "child port",
                                   ids.components[con.child_component].y,
                                   child.second)
                                    : combobox_component_port_id_empty("child port");

            if (is_defined(con.child_component) and
                is_defined(con.child_port) and is_defined(con.parent_port) and
                std::ranges::none_of(tab.compo.output_connection_pack,
                                     [&](const auto& c) { return con == c; })) {
                tab.compo.output_connection_pack.push_back(con);
                con.clear();
                ++u;
            }
        });
    } else {
        con.clear();
    }

    return u > 0;
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
                            file_path::file_type::component_file,
                            file_selector::flags(
                              file_selector::flag::show_save_button,
                              file_selector::flag::show_cancel_button));

                          if (selected.save) {
                              tab.file.file = selected.file_id;
                              return component_editor_result{
                                  component_editor_result_type::do_save_file,
                                  component_editor_result_type::do_close_menu
                              };
                          }

                          if (selected.close)
                              return component_editor_result{
                                  component_editor_result_type::do_close_menu
                              };

                          return component_editor_result{};
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

                    if (ImGui::Button("new constant", size)) {
                        tab.compo.srcs.alloc_constant_source();
                        action |=
                          component_editor_result_type::do_store_component;
                    }

                    if (ImGui::Button("new binary file", size)) {
                        tab.compo.srcs.alloc_binary_source();
                        action |=
                          component_editor_result_type::do_store_component;
                    }

                    if (ImGui::Button("new text file", size)) {
                        tab.compo.srcs.alloc_text_source();
                        action |=
                          component_editor_result_type::do_store_component;
                    }

                    if (ImGui::Button("new random", size)) {
                        tab.compo.srcs.alloc_random_source();
                        action |=
                          component_editor_result_type::do_store_component;
                    }

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
                        if (tab.compo.x.can_alloc(1) or
                            tab.compo.x.template grow<3, 2>()) {
                            const auto id = tab.compo.x.alloc_id();

                            tab.compo.x.template get<port_str>(id) = "in";
                            tab.compo.x.template get<port_option>(id) =
                              port_option::classic;
                            tab.compo.x.template get<position>(id).reset();

                            action |=
                              component_editor_result_type::do_store_component;
                        }
                    }

                    if (tab.compo.y.can_alloc(1) and
                        ImGui::Button("Output port", size)) {
                        if (tab.compo.y.can_alloc(1) or
                            tab.compo.y.template grow<3, 2>()) {
                            const auto id = tab.compo.y.alloc_id();

                            tab.compo.y.template get<port_str>(id) = "out";
                            tab.compo.y.template get<port_option>(id) =
                              port_option::classic;
                            tab.compo.y.template get<position>(id).reset();

                            action |=
                              component_editor_result_type::do_store_component;
                        }
                    }

                    if (ImGui::Button("refresh i/o", size)) {
                        update_child_component_task(app, ed.tabs.get_id(tab));
                    }

                    if (not tab.compo.x.empty() or not tab.compo.y.empty()) {
                        if (ImGui::TreeNode("Ports")) {
                            if (not tab.compo.x.empty() and
                                ImGui::TreeNode("Input port")) {
                                if (show_input_ports(tab.compo))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output port")) {
                                if (show_output_ports(tab.compo))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }

                        app.mod.ids.read([&](const auto& ids, auto) noexcept {
                            if (ImGui::TreeNode("Connections")) {
                                if (show_input_connections(app, ids, tab))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output connection")) {
                                if (show_output_connections(app, ids, tab))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }

                            if (not tab.compo.x.empty() and
                                ImGui::TreeNode("Input Connection pack")) {
                                if (show_input_connection_packs(ids, tab))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }

                            if (not tab.compo.y.empty() and
                                ImGui::TreeNode("Output Connection pack")) {
                                if (show_output_connection_packs(ids, tab))
                                    action |= component_editor_result_type::
                                      do_store_component;
                                ImGui::TreePop();
                            }

                            ImGui::TreePop();
                        });
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        name_str copy_name = tab.compo.name;
        if (ImGui::InputFilteredString("Name", copy_name)) {
            app.component_ed.request_to_open(tab.id);
            tab.compo.name   = copy_name;
            tab.is_dock_init = false;
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
            action |= component_editor_result_type::do_store_component;

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

    bool is_open = true;

    if (not ImGui::Begin(make_title(tab.compo.name.sv(), tab.id).c_str(),
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
            const auto tab_id = ed.tabs.get_id(tab);

            app.add_gui_task([&app, tab_id, action]() {
                auto* tab = app.component_ed.tabs.try_to_get(tab_id);
                if (not tab)
                    return;

                app.mod.ids.write([&](auto& ids) {
                    if (not ids.exists(tab->id))
                        return;

                    if (action[component_editor_result_type::do_save_file]) {
                        ids.components[tab->id]             = tab->compo;
                        ids.component_file_paths[tab->id]   = tab->file;
                        ids.component_descriptions[tab->id] = tab->desc;
                        ids.components[tab->id]             = tab->compo;

                        app.mod.files.read([&](const auto& fs, auto) noexcept {
                            if (auto ret =
                                  app.mod.save(ids, fs, tab->id, app.jn);
                                not ret) {
                                app.jn.push(
                                  log_level::error,
                                  [&](auto& title, auto& msg) {
                                      title = "Component save error";
                                      format(msg,
                                             "Fail to save {} (part: {} {})",
                                             tab->compo.name.sv(),
                                             ordinal(ret.error().cat()),
                                             ret.error().value());
                                  });
                            } else {
                                app.jn.push(log_level::notice,
                                            [&](auto& title, auto& msg) {
                                                title = "Component save";
                                                format(msg,
                                                       "Save {} success",
                                                       tab->compo.name.sv());
                                            });
                            }
                        });

                        return;
                    }

                    if (action
                          [component_editor_result_type::do_store_component]) {
                        ids.components[tab->id] = tab->compo;
                        update_child_component_task(app, tab_id);
                    }

                    if (action
                          [component_editor_result_type::do_store_file_path]) {
                        ids.component_file_paths[tab->id]   = tab->file;
                        ids.component_descriptions[tab->id] = tab->desc;
                    }

                    if (action[component_editor_result_type::
                                 do_store_external_source]) {
                        ids.components[tab->id] = tab->compo;
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
        if (display_tab_content(*it) == show_result_t::request_to_close)
            tabs.free(*it);

        ++it;
    }
}

component_editor::component_editor() noexcept
  : tabs{ component_editor::max_tab }
{}

void component_editor::display() noexcept
{
    if (not tabs.empty())
        display_tabs();
}

static auto find_in_tabs(
  const data_array<component_editor::tab, component_editor::tab_id>& v,
  const component_id id) noexcept -> component_editor::tab_id
{
    for (const auto& d : v)
        if (d.id == id)
            return v.get_id(d);

    return undefined<component_editor::tab_id>();
}

void component_editor::close(const component_id id) noexcept
{
    const auto tab_id = find_in_tabs(tabs, id);

    if (auto* t = tabs.try_to_get(tab_id)) {
        auto& app = container_of(this, &application::component_ed);

        switch (t->type) {
        case component_type::generic:
            app.generics.free(t->data.generic);
            break;

        case component_type::grid:
            app.grids.free(t->data.grid);
            break;

        case component_type::graph:
            app.graphs.free(t->data.graph);
            break;

        case component_type::hsm:
            app.hsms.free(t->data.hsm);
            break;

        default:
            break;
        }

        tabs.free(*t);
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
        const auto& compo  = ids.components[id];
        const auto  tab_id = find_in_tabs(tabs, id);

        if (tabs.try_to_get(tab_id)) {
            ImGui::SetWindowFocus(make_title(compo.name.sv(), id).c_str());
        } else {
            const auto file_access = app.mod.files.read(
              [id = ids.component_file_paths[id].file](
                const auto& fs,
                auto) noexcept -> file_access::full_file_access_result {
                  return fs.get_full_access(id);
              });

            switch (compo.type) {
            case component_type::generic:
                if (tabs.can_alloc(1) and app.generics.can_alloc(1)) {
                    auto& t =
                      tabs.alloc(id, component_type::generic, file_access);
                    t.data.generic    = app.generics.get_id(app.generics.alloc(
                      id,
                      compo,
                      compo.id.generic_id,
                      ids.generic_components.get(compo.id.generic_id)));
                    m_request_to_open = id;
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::grid:
                if (tabs.can_alloc(1) and app.grids.can_alloc(1)) {
                    auto& t = tabs.alloc(id, component_type::grid, file_access);
                    t.data.grid =
                      app.grids.get_id(app.grids.alloc(id, compo.id.grid_id));
                    m_request_to_open = id;
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::graph:
                if (tabs.can_alloc(1) and app.graphs.can_alloc(1)) {
                    auto& t =
                      tabs.alloc(id, component_type::graph, file_access);
                    t.data.graph = app.graphs.get_id(
                      app.graphs.alloc(id, compo.id.graph_id));
                    m_request_to_open = id;
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::hsm:
                if (tabs.can_alloc(1) and app.hsms.can_alloc(1)) {
                    auto& t = tabs.alloc(id, component_type::hsm, file_access);
                    t.data.hsm = app.hsms.get_id(
                      app.hsms.alloc(id,
                                     compo.id.hsm_id,
                                     ids.hsm_components.get(compo.id.hsm_id)));
                    m_request_to_open = id;
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::simulation:
                if (tabs.can_alloc(1) and app.sims.can_alloc(1)) {
                    auto& t =
                      tabs.alloc(id, component_type::simulation, file_access);
                    t.data.sim = app.sims.get_id(
                      app.sims.alloc(id,
                                     compo.id.sim_id,
                                     ids.sim_components.get(compo.id.sim_id)));
                    m_request_to_open = id;
                } else
                    app.jn.push(log_level::error, log_not_enough_memory);
                break;

            case component_type::none:
                break;
            }
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
    return is_defined(find_in_tabs(tabs, id));
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
