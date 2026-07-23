// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <algorithm>
#include <optional>

namespace irt {

constexpr static inline const char* plot_type_str[] = { "None",
                                                        "Plot line",
                                                        "Plot dot" };

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

static void write(application&                    app,
                  project&                        pj,
                  std::ofstream&                  ofs,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id sub_id) noexcept
{
    ofs.imbue(std::locale::classic());

    if (const auto* vobs = pj.variable_observers.try_to_get(vobs_id);
        vobs and vobs->exists(sub_id))
        write(pj, ofs, *vobs, sub_id);
    else
        app.jn.push(log_level::error, [](auto& title, auto& msg) noexcept {
            title = "Output editor";
            msg   = "Unknown observation";
        });
}

static void write(application&                    app,
                  project&                        pj,
                  const std::filesystem::path&    file_path,
                  const variable_observer_id      vobs_id,
                  const variable_observer::sub_id obs_id) noexcept
{
    if (auto ofs = std::ofstream{ file_path }; ofs.is_open())
        write(app, pj, ofs, vobs_id, obs_id);
    else
        app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
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
        app.jn.push(log_level::error, [](auto& title, auto& msg) noexcept {
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
        app.jn.push(log_level::error, [&](auto& title, auto& msg) noexcept {
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

static int compute_tree_node_state(const tree_node& tn,
                                   const project&   pj) noexcept
{
    const auto& show_in_outputs = pj.observables.get<project::show_in_output>();
    auto        nb_show         = 0;
    auto        nb_hide         = 0;

    for (const auto& g_obs : tn.observables_ids.data) {
        const auto g_obs_id  = g_obs.value;
        const auto g_obs_idx = get_index(g_obs_id);

        if (pj.observables.exists(g_obs.value)) {
            if (show_in_outputs[g_obs_idx])
                nb_show++;
            else
                nb_hide++;
        }
    }

    return nb_show > 0 ? nb_hide > 0 ? -1 : 1 : nb_hide > 0 ? 0 : -1;
}

static void display_project_observation(
  irt::application&    app,
  irt::project_editor& pj,
  vector<bool>&        display_selected_project) noexcept
{
    const auto pj_id      = app.pjs.get_id(pj);
    const auto pj_idx     = get_index(pj_id);
    const auto checkbox_x = ImGui::GetContentRegionAvail().x - 30.0f;
    const auto j_name     = format_n<32>("{}##{}", pj.title.sv(), pj_idx);
    const auto c_name     = format_n<32>("##chkbox-{}", pj_idx);

    ImGui::PushID(pj_idx);
    ImGui::AlignTextToFramePadding();

    const auto is_project_open =
      ImGui::TreeNodeEx(j_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    // ImGui::SameLine();
    // ImGui::SetCursorPosX(checkbox_x);

    // bool force_true  = false;
    // bool force_false = false;

    // if (ImGui::Checkbox(c_name.c_str(), &display_selected_project[pj_idx])) {
    //     if (display_selected_project[pj_idx])
    //         force_true = true;
    //     else
    //         force_false = true;
    // }

    if (is_project_open) {

        struct stack_elem {
            stack_elem(tree_node* tn_, bool child = false, bool sibling = false)
              : tn(tn_)
              , read_child(child)
              , read_sibling(sibling)
            {}

            tree_node* tn           = nullptr;
            bool       read_child   = false;
            bool       read_sibling = false;
            bool       read_model   = false;

            // bool force_true  = false;
            // bool force_false = false;
            // int  state       = -1;
        };

        auto*      tn_head    = pj.pj.tn_head();
        const auto tn_head_id = pj.pj.tree_nodes.get_id(*tn_head);
        // auto       stack = small_vector<stack_elem,
        // max_component_stack_size>{};
        auto stack = vector<stack_elem>{};

        stack.emplace_back(tn_head);

        while (not stack.empty()) {
            const stack_elem cur = stack.back();

            if (cur.read_child and not cur.read_model and
                not cur.read_sibling) {
                stack.back().read_model = true;

                for (const auto& g_obs : cur.tn->observables_ids.data) {
                    const auto g_obs_id  = g_obs.value;
                    const auto g_obs_idx = get_index(g_obs_id);
                    auto&      show_in_outputs =
                      pj.pj.observables.get<project::show_in_output>();

                    if (pj.pj.observables.exists(g_obs.value)) {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(g_obs.id.c_str());

                        const auto cname =
                          format_n<32>("{}##{}", g_obs.id.c_str(), g_obs_idx);

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(checkbox_x);
                        ImGui::Checkbox(cname.c_str(),
                                        &show_in_outputs[g_obs_idx]);
                    }
                }

                ImGui::TreePop();
                ImGui::PopID();
                continue;
            }

            if (not cur.read_child and not cur.read_sibling) {
                const auto tn_id  = pj.pj.tree_nodes.get_id(stack.back().tn);
                const auto tn_idx = get_index(tn_id);
                const auto name   = format_n<64>(
                  "{}##{}", stack.back().tn->unique_id.sv(), tn_idx);

                ImGui::PushID(tn_idx);
                ImGui::AlignTextToFramePadding();

                const auto is_open = ImGui::TreeNodeEx(
                  name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

                // ImGui::SameLine();
                // ImGui::SetCursorPosX(checkbox_x);

                if (not is_open) {
                    stack.back().read_child = true;
                    stack.back().read_model = true;
                    ImGui::PopID();
                    continue;
                }
            }

            if (cur.read_child) {
                if (cur.read_sibling) {
                    stack.pop_back();
                } else {
                    stack.back().read_sibling = true;
                    if (auto* sibling = cur.tn->tree.get_sibling(); sibling) {
                        stack.emplace_back(sibling);
                    }
                }
            } else {
                stack.back().read_child = true;
                if (auto* child = cur.tn->tree.get_child()) {
                    stack.emplace_back(child);
                }
            }
        }

        ImGui::TreePop();
    }

    // struct elem {
    //     explicit constexpr elem(const tree_node_id id) noexcept
    //       : tn(id)
    //     {}

    //    tree_node_id tn;

    //    bool children_read = false;
    //    bool sibling_read  = false;
    //    bool model_read    = false;
    //    bool pop_required  = false;

    //    bool force_true  = false;
    //    bool force_false = false;

    //    int state = -1;
    //};

    // auto        stack      = small_vector<elem, max_component_stack_size>{};
    // const auto& tn_head    = *pj.pj.tn_head();
    // const auto  tn_head_id = pj.pj.tree_nodes.get_id(tn_head);
    // stack.emplace_back(tn_head_id);
    // stack.back().force_true  = force_true;
    // stack.back().force_false = force_false;
    // stack.back().state       = compute_tree_node_state(tn_head, pj.pj);

    // while (not stack.empty()) {
    //     if (stack.back().children_read and stack.back().sibling_read and
    //         stack.back().model_read) {
    //         if (stack.back().pop_required)
    //             ImGui::TreePop();
    //         stack.pop_back();
    //         continue;
    //     }

    //    const auto  tn_id  = stack.back().tn;
    //    const auto  tn_idx = get_index(tn_id);
    //    const auto& tn     = pj.pj.tree_nodes.get(tn_id);
    //    const auto name = format_n<64>("{}##{}", tn.unique_id.sv(), tn_idx);

    //    if (not stack.back().children_read) {
    //        stack.back().children_read = true;

    //        ImGui::PushID(pj_idx);
    //        ImGui::AlignTextToFramePadding();

    //        const auto is_open = ImGui::TreeNodeEx(
    //          name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    //        ImGui::SameLine();
    //        ImGui::SetCursorPosX(checkbox_x);

    //        auto old_state = stack.back().state;
    //        if (ImGui::CheckBoxTristate(j_name.c_str(), &old_state)) {
    //            if (old_state == 1) {
    //                stack.back().force_true  = true;
    //                stack.back().force_false = false;
    //                stack.back().state       = 1;
    //            } else if (old_state == 0) {
    //                stack.back().force_true  = false;
    //                stack.back().force_false = true;
    //                stack.back().state       = 0;
    //            } else {
    //                stack.back().force_true  = false;
    //                stack.back().force_false = false;
    //                stack.back().state       = -1;
    //            }
    //        }

    //        if (is_open) {
    //            stack.back().pop_required = true;
    //            const auto force_true     = stack.back().force_true;
    //            const auto force_false    = stack.back().force_false;

    //            if (const auto* child = tn.tree.get_child()) {
    //                const auto child_id = pj.pj.tree_nodes.get_id(*child);
    //                stack.emplace_back(child_id);
    //                stack.back().force_true  = force_true;
    //                stack.back().force_false = force_false;
    //                stack.back().state =
    //                  force_true ? 1
    //                  : force_false
    //                    ? 0
    //                    : compute_tree_node_state(*child, pj.pj);
    //            }
    //        }

    //        ImGui::PopID();
    //        continue;
    //    }

    //    if (not stack.back().model_read) {
    //        stack.back().model_read = true;

    //        for (const auto& g_obs : tn.observables_ids.data) {
    //            const auto g_obs_id  = g_obs.value;
    //            const auto g_obs_idx = get_index(g_obs_id);
    //            auto&      show_in_outputs =
    //              pj.pj.observables.get<project::show_in_output>();

    //            if (pj.pj.observables.exists(g_obs.value)) {
    //                ImGui::AlignTextToFramePadding();
    //                ImGui::TextUnformatted(g_obs.id.c_str());

    //                const auto cname =
    //                  format_n<32>("{}##{}", g_obs.id.c_str(), g_obs_idx);

    //                if (stack.back().force_true)
    //                    show_in_outputs[g_obs_idx] = true;
    //                if (stack.back().force_false)
    //                    show_in_outputs[g_obs_idx] = false;

    //                ImGui::SameLine();
    //                ImGui::SetCursorPosX(checkbox_x);
    //                ImGui::Checkbox(cname.c_str(),
    //                                &show_in_outputs[g_obs_idx]);
    //            }
    //        }

    //        continue;
    //    }

    //    if (not stack.back().sibling_read) {
    //        stack.back().sibling_read = true;

    //        const auto force_true     = stack.back().force_true;
    //        const auto force_false    = stack.back().force_false;

    //        if (stack.back().children_read and
    //            not stack.back().pop_required)
    //            stack.pop_back();

    //        if (auto* sibling = tn.tree.get_sibling()) {
    //            stack.emplace_back(
    //              pj.pj.tree_nodes.get_id(*tn.tree.get_sibling()));
    //            stack.back().force_true  = force_true;
    //            stack.back().force_false = force_false;
    //            stack.back().state       = force_true ? 1
    //                                       : force_false
    //                                         ? 0
    //                                         : compute_tree_node_state(
    //                                             *tn.tree.get_sibling(),
    //                                             pj.pj);
    //        }
    //    }
    //}

    ImGui::PopID();
}

static void display_project_plot(irt::application&    app,
                                 irt::project_editor& pj) noexcept
{
    const auto pj_id  = app.pjs.get_id(pj);
    const auto pj_idx = get_index(pj_id);

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

    auto stack = small_vector<elem, max_component_stack_size>{};
    stack.emplace_back(pj.pj.tree_nodes.get_id(*pj.pj.tn_head()));

    while (not stack.empty()) {
        if (stack.back().children_read and stack.back().sibling_read) {
            stack.pop_back();
            continue;
        }

        const auto  tn_id  = stack.back().tn;
        const auto  tn_idx = get_index(tn_id);
        const auto& tn     = pj.pj.tree_nodes.get(tn_id);
        const auto  name   = format_n<64>("{}##{}", tn.unique_id.sv(), tn_idx);

        if (not stack.back().children_read) {
            stack.back().children_read = true;
            if (not tn.tree.get_child()) {
                auto& show_in_outputs =
                  pj.pj.observables.get<project::show_in_output>();
                auto& models = pj.pj.observables.get<model_id>();

                for (const auto& g_obs : tn.observables_ids.data) {
                    const auto g_obs_id  = g_obs.value;
                    const auto g_obs_idx = get_index(g_obs_id);

                    if (show_in_outputs[g_obs_idx]) {
                        const auto mdl_id = models[g_obs_idx];

                        if (const auto* mdl =
                              pj.pj.sim.models.try_to_get(mdl_id)) {
                            const auto obs_id = mdl->obs_id;

                            if (const auto* obs =
                                  pj.pj.sim.observers.try_to_get(obs_id)) {
                                const auto obs_idx = get_index(obs_id);
                                const auto name = format_n<64>("{}-{}##{}",
                                                               pj.pj.name.sv(),
                                                               g_obs.id.sv(),
                                                               obs_idx);

                                obs->read_history(
                                  [&](const auto& h, const auto) {
                                      app.plot_obs.show_plot_line(
                                        *obs,
                                        plot_type_options::line,
                                        name.c_str());
                                  });
                            }
                        }
                    }
                }
            } else {
                stack.back().pop_required = true;
                stack.emplace_back(
                  pj.pj.tree_nodes.get_id(*tn.tree.get_child()));
            }
            continue;
        }

        if (not stack.back().sibling_read) {
            stack.back().sibling_read = true;

            if (stack.back().children_read and not stack.back().pop_required)
                stack.pop_back();

            if (auto* sibling = tn.tree.get_sibling())
                stack.emplace_back(
                  pj.pj.tree_nodes.get_id(*tn.tree.get_sibling()));
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
                for (auto& pj : app.pjs)
                    display_project_observation(
                      app, pj, m_display_selected_project);
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
                        display_project_plot(app, pj);
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
                        write(app, pj->pj, m_file, m_vobs_id, m_sub_id);
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
