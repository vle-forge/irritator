// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

namespace irt {

#if 0
constexpr static inline const char* plot_type_str[] = { "None",
                                                        "Plot line",
                                                        "Plot dot" };
#endif

#if 0
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
#endif

#if 0
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
#endif

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

static void write(project&                        pj,
                  std::ofstream&                  ofs,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id sub_id) noexcept
{
    using namespace std::literals;

    ofs.imbue(std::locale::classic());

    if (const auto* vobs = pj.variable_observers.try_to_get(vobs_id);
        vobs and vobs->exists(sub_id))
        write(pj, ofs, *vobs, sub_id);
    else
        log(log_level::error, "Output editor"sv, "Unknown observation"sv);
}

static void write(project&                        pj,
                  const std::filesystem::path&    file_path,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id obs_id) noexcept
{
    if (auto ofs = std::ofstream{ file_path }; ofs.is_open())
        write(pj, ofs, vobs_id, obs_id);
    else
        log(log_level::error, [&](auto& title, auto& msg) noexcept {
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
        log(log_level::error, [](auto& title, auto& msg) noexcept {
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
        log(log_level::error, [&](auto& title, auto& msg) noexcept {
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

void output_editor::force_display(const project_id pj_id,
                                  const bool       display_or_not) noexcept
{
    auto& app = container_of(this, &application::output_ed);
    auto* pj  = app.pjs.try_to_get(pj_id);

    if (not pj)
        return;

    auto  tn_head_id = pj->pj.tn_head();
    auto* tn         = pj->pj.tree_nodes.try_to_get(tn_head_id);

    if (not tn)
        return;

    force_display(pj_id, tn_head_id, display_or_not);
}

void output_editor::force_display(const project_id   pj_id,
                                  const tree_node_id tn_id,
                                  const bool         display_or_not) noexcept
{
    auto& app = container_of(this, &application::output_ed);
    auto* pj  = app.pjs.try_to_get(pj_id);

    if (not pj)
        return;

    auto* tn = pj->pj.tree_nodes.try_to_get(tn_id);

    if (not tn)
        return;

    auto& tree_nodes      = pj->pj.tree_nodes;
    auto& outputs         = tree_nodes.get<project::output_state>();
    auto& show_in_outputs = pj->pj.observables.get<project::show_in_output>();
    auto  stack = small_vector<const tree_node*, max_component_stack_size>{};

    for (const auto& g_obs : tn->observables_ids.data) {
        const auto g_obs_id  = g_obs.value;
        const auto g_obs_idx = get_index(g_obs_id);

        if (pj->pj.observables.exists(g_obs.value))
            show_in_outputs[g_obs_idx] = display_or_not;
    }

    outputs[get_index(tn_id)] =
      display_or_not ? tn->observables_ids.data.size() : 0u;

    if (auto* child = tn->tree.get_child())
        stack.emplace_back(child);

    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();

        const auto tn_id  = tree_nodes.get_id(*cur);
        const auto tn_idx = get_index(tn_id);

        outputs[tn_idx] =
          display_or_not ? cur->observables_ids.data.size() : 0u;

        for (const auto& g_obs : cur->observables_ids.data) {
            const auto g_obs_id  = g_obs.value;
            const auto g_obs_idx = get_index(g_obs_id);

            if (pj->pj.observables.exists(g_obs.value))
                show_in_outputs[g_obs_idx] = display_or_not;
        }

        if (auto* sibling = cur->tree.get_sibling(); sibling)
            stack.emplace_back(sibling);

        if (auto* child = cur->tree.get_child(); child)
            stack.emplace_back(child);
    }
}

void output_editor::display_project(const project_id pj_id) noexcept
{
    auto& app = container_of(this, &application::output_ed);
    auto* pj  = app.pjs.try_to_get(pj_id);

    if (not pj)
        return;

    const auto pj_idx     = get_index(pj_id);
    const auto checkbox_x = ImGui::GetContentRegionAvail().x - 30.0f;
    const auto j_name     = format_n<32>("{}##{}", pj->title.sv(), pj_idx);
    const auto c_name     = format_n<32>("##chkbox-{}", pj_idx);

    ImGui::PushID(pj_idx);
    ImGui::AlignTextToFramePadding();

    const auto is_project_open =
      ImGui::TreeNodeEx(j_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::SameLine();
    ImGui::SetCursorPosX(checkbox_x);

    if (ImGui::Checkbox(c_name.c_str(), &m_display_selected_project[pj_idx]))
        force_display(pj_id, m_display_selected_project[pj_idx]);

    auto& outputs = pj->pj.tree_nodes.get<project::output_state>();

    if (is_project_open) {
        struct ss {
            const tree_node* tn              = nullptr;
            bool             have_read_child = false;
            bool             is_open         = false;
        };

        auto stack = small_vector<ss, max_component_stack_size>{};

        stack.push_back(
          ss{ .tn = pj->pj.tree_nodes.try_to_get(pj->pj.tn_head()) });

        while (not stack.empty()) {
            auto       cur    = stack.back();
            const auto tn_id  = pj->pj.tree_nodes.get_id(*cur.tn);
            const auto tn_idx = get_index(tn_id);
            const auto name =
              format_n<32>("{}##{}", cur.tn->unique_id.sv(), tn_idx);
            stack.pop_back();

            if (not cur.have_read_child) {
                ImGui::AlignTextToFramePadding();

                cur.have_read_child = true;
                cur.is_open = ImGui::TreeNodeEx(name.c_str(),
                                                ImGuiTreeNodeFlags_DefaultOpen);

                if (cur.tn->observables_ids.data.size() > 0) {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(checkbox_x);

                    auto v =
                      outputs[tn_idx] == 0 ? 0
                      : outputs[tn_idx] == cur.tn->observables_ids.data.size()
                        ? 1
                        : -1;

                    if (ImGui::CheckBoxTristate(name.c_str(), &v)) {
                        force_display(pj_id, tn_id, v == 0 ? false : true);
                    }
                }

                if (cur.is_open) {
                    if (auto* child = cur.tn->tree.get_child()) {
                        stack.push_back(cur);
                        stack.push_back(ss{ .tn = child });
                        continue;
                    }
                }
            }

            if (cur.is_open) {
                for (const auto& g_obs : cur.tn->observables_ids.data) {
                    const auto g_obs_id  = g_obs.value;
                    const auto g_obs_idx = get_index(g_obs_id);
                    auto&      show_in_outputs =
                      pj->pj.observables.get<project::show_in_output>();

                    if (pj->pj.observables.exists(g_obs.value)) {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(g_obs.id.c_str());

                        const auto cname =
                          format_n<32>("##chkbox-{}-{}", tn_idx, g_obs_idx);

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(checkbox_x);
                        if (ImGui::Checkbox(cname.c_str(),
                                            &show_in_outputs[g_obs_idx])) {
                            outputs[tn_idx] +=
                              show_in_outputs[g_obs_idx] ? 1 : -1;
                        }
                    }
                }

                ImGui::TreePop();
            }

            if (auto* sibling = cur.tn->tree.get_sibling()) {
                stack.emplace_back(sibling);
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void output_editor::display_plot(const project_id pj_id) noexcept
{
    auto&       app    = container_of(this, &application::output_ed);
    const auto  pj_idx = get_index(pj_id);
    const auto* pj     = app.pjs.try_to_get(pj_id);

    if (not pj)
        return;

    ImGui::PushID(pj_idx);

    struct elem {
        explicit constexpr elem(const tree_node_id id) noexcept
          : tn(id)
        {}

        tree_node_id tn;

        bool children_read = false;
        bool sibling_read  = false;
        bool pop_required  = false;
    };

    auto       stack      = small_vector<elem, max_component_stack_size>{};
    const auto tn_head_id = pj->pj.tn_head();

    stack.emplace_back(tn_head_id);

    while (not stack.empty()) {
        if (stack.back().children_read and stack.back().sibling_read) {
            stack.pop_back();
            continue;
        }

        const auto  tn_id  = stack.back().tn;
        const auto* tn_ptr = pj->pj.tree_nodes.try_to_get(tn_id);
        const auto& tn     = *tn_ptr;

        if (not stack.back().children_read) {
            stack.back().children_read = true;
            if (not tn.tree.get_child()) {
                auto& show_in_outputs =
                  pj->pj.observables.get<project::show_in_output>();
                auto& models = pj->pj.observables.get<model_id>();

                for (const auto& g_obs : tn.observables_ids.data) {
                    const auto g_obs_id  = g_obs.value;
                    const auto g_obs_idx = get_index(g_obs_id);

                    if (show_in_outputs[g_obs_idx]) {
                        const auto mdl_id = models[g_obs_idx];

                        if (const auto* mdl =
                              pj->pj.sim.models.try_to_get(mdl_id)) {
                            const auto obs_id = mdl->obs_id;

                            if (const auto* obs =
                                  pj->pj.sim.observers.try_to_get(obs_id)) {
                                const auto obs_idx = get_index(obs_id);
                                const auto name = format_n<64>("{}-{}##{}",
                                                               pj->pj.name.sv(),
                                                               g_obs.id.sv(),
                                                               obs_idx);

                                app.plot_obs.show_plot_line(
                                  *obs, plot_type_options::line, name.c_str());
                            }
                        }
                    }
                }
            } else {
                stack.back().pop_required = true;
                stack.emplace_back(
                  pj->pj.tree_nodes.get_id(*tn.tree.get_child()));
            }
            continue;
        }

        if (not stack.back().sibling_read) {
            stack.back().sibling_read = true;

            if (stack.back().children_read and not stack.back().pop_required)
                stack.pop_back();

            if (tn.tree.get_sibling())
                stack.emplace_back(
                  pj->pj.tree_nodes.get_id(*tn.tree.get_sibling()));
        }
    }

    ImGui::PopID();
}

void output_editor::show() noexcept
{
    if (!ImGui::Begin(output_editor::name, &is_open)) {
        ImGui::End();
        return;
    }

    auto& app = container_of(this, &application::output_ed);

    if (ImGui::BeginTable("OutputEditorTable", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn(
          "projects", ImGuiTableColumnFlags_WidthStretch, 10.f);
        ImGui::TableSetupColumn(
          "plot", ImGuiTableColumnFlags_WidthStretch, 90.f);

        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();

        if (not app.pjs.empty()) {
            if (ImGui::BeginChild("ProjectsTreeNode")) {
                for (auto& pj : app.pjs) {
                    const auto pj_id = app.pjs.get_id(pj);

                    display_project(pj_id);
                }
            }

            ImGui::EndChild();
        }

        ImGui::TableNextColumn();

        ImPlot::SetCurrentContext(app.output_ed.m_ctx);

        if (ImPlot::BeginPlot("Plots", ImVec2(-1, -1))) {
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
            ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 1.f);

            ImPlot::SetupAxes(nullptr,
                              nullptr,
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);

            if (not app.pjs.empty()) {
                for (auto& pj : app.pjs) {
                    const auto pj_id  = app.pjs.get_id(pj);
                    const auto pj_idx = get_index(pj_id);

                    if (m_display_selected_project[pj_idx])
                        display_plot(pj_id);
                }
            }

            for (auto& p : app.copy_obs)
                if (p.plot_type != simulation_plot_type::none)
                    app.plot_copy_wgt.show_plot_line(p);

            ImPlot::PopStyleVar(2);
            ImPlot::EndPlot();
        }

        ImGui::EndTable();
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
                        write(pj->pj, m_file, m_vobs_id, m_sub_id);
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
