// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <irritator/ext.hpp>

#include "application.hpp"
#include "dialog.hpp"
#include "editor.hpp"
#include "internal.hpp"

#include <cinttypes>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/examples.hpp>
#include <irritator/io.hpp>

namespace irt {

static void compute_connection_distance(const model_id src,
                                        const model_id dst,
                                        editor&        ed,
                                        const float    k)
{
    const auto v = get_index(src);
    const auto u = get_index(dst);

    const float dx = ed.positions[v].x - ed.positions[u].x;
    const float dy = ed.positions[v].y - ed.positions[u].y;
    if (dx && dy) {
        const float d2    = dx * dx / dy * dy;
        const float coeff = std::sqrt(d2) / k;

        ed.displacements[v].x -= dx * coeff;
        ed.displacements[v].y -= dy * coeff;
        ed.displacements[u].x += dx * coeff;
        ed.displacements[u].y += dy * coeff;
    }
}

static void compute_connection_distance(const model& mdl,
                                        editor&      ed,
                                        const float  k)
{
    dispatch(mdl, [&mdl, &ed, k]<typename Dynamics>(Dynamics& dyn) -> void {
        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (const auto elem : dyn.y) {
                auto list = get_node(ed.sim, elem);

                for (auto& dst : list)
                    compute_connection_distance(
                      ed.sim.get_id(mdl), dst.model, ed, k);
            }
        }
    });
}

void editor::compute_grid_layout() noexcept
{
    const auto size = sim.models.size();

    if (size == 0 || size > std::numeric_limits<int>::max())
        return;

    const auto tmp       = std::sqrt(size);
    const auto column    = static_cast<int>(tmp);
    auto       line      = column;
    auto       remaining = static_cast<int>(size) - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const auto panning = ImNodes::EditorContextGetPanning();
    auto       new_pos = panning;

    model* mdl  = nullptr;
    int    elem = 0;

    for (int i = 0; i < column; ++i) {
        if (!sim.models.next(mdl))
            break;

        auto mdl_id    = sim.models.get_id(mdl);
        auto mdl_index = get_index(mdl_id);

        new_pos.y =
          panning.y + static_cast<float>(i) * settings.grid_layout_y_distance;
        for (int j = 0; j < line; ++j) {
            new_pos.x = panning.x +
                        static_cast<float>(j) * settings.grid_layout_x_distance;
            ImNodes::SetNodeGridSpacePos(mdl_index, new_pos);
            positions[elem].x = new_pos.x;
            positions[elem].y = new_pos.y;
            ++elem;
        }
    }

    new_pos.x = panning.x;
    new_pos.y =
      panning.y + static_cast<float>(column) * settings.grid_layout_y_distance;
    for (int j = 0; j < remaining; ++j) {
        if (!sim.models.next(mdl))
            break;

        auto mdl_id    = sim.models.get_id(mdl);
        auto mdl_index = get_index(mdl_id);

        new_pos.x =
          panning.x + static_cast<float>(j) * settings.grid_layout_x_distance;
        ImNodes::SetNodeGridSpacePos(mdl_index, new_pos);
        positions[elem].x = new_pos.x;
        positions[elem].y = new_pos.y;
        ++elem;
    }

    ImNodes::EditorContextResetPanning(positions[0]);
}

void editor::compute_automatic_layout() noexcept
{
    /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
       Fruchterman and Edward M. Reingold in Software--Pratice and
       Experience, Vol. 21(1 1), 1129-1164 (november 1991).
       */

    const auto orig_size = sim.models.size();

    if (orig_size == 0 || orig_size > std::numeric_limits<int>::max())
        return;

    const auto size      = static_cast<int>(orig_size);
    const auto tmp       = std::sqrt(size);
    const auto column    = static_cast<int>(tmp);
    auto       line      = column;
    auto       remaining = static_cast<int>(size) - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const float W =
      static_cast<float>(column) * settings.automatic_layout_x_distance;
    const float L =
      static_cast<float>(line) +
      ((remaining > 0) ? settings.automatic_layout_y_distance : 0.f);
    const float area     = W * L;
    const float k_square = area / static_cast<float>(sim.models.size());
    const float k        = std::sqrt(k_square);

    // float t = 1.f - static_cast<float>(iteration) /
    //                   static_cast<float>(automatic_layout_iteration_limit);
    // t *= t;

    float t =
      1.f - 1.f / static_cast<float>(settings.automatic_layout_iteration_limit);

    for (int iteration = 0;
         iteration < settings.automatic_layout_iteration_limit;
         ++iteration) {
        for (int i_v = 0; i_v < size; ++i_v) {
            const int v = i_v;

            displacements[v].x = displacements[v].y = 0.f;

            for (int i_u = 0; i_u < size; ++i_u) {
                const int u = i_u;

                if (u != v) {
                    const ImVec2 delta{ positions[v].x - positions[u].x,
                                        positions[v].y - positions[u].y };

                    if (delta.x && delta.y) {
                        const float d2 = delta.x * delta.x + delta.y * delta.y;
                        const float coeff = k_square / d2;

                        displacements[v].x += coeff * delta.x;
                        displacements[v].y += coeff * delta.y;
                    }
                }
            }
        }

        model* mdl = nullptr;
        while (sim.models.next(mdl)) {
            compute_connection_distance(*mdl, *this, k);
        }

        // auto sum = 0.f;
        mdl = nullptr;
        for (int i_v = 0; i_v < size; ++i_v) {
            irt_assert(sim.models.next(mdl));
            const int v = i_v;

            const float d2 = displacements[v].x * displacements[v].x +
                             displacements[v].y * displacements[v].y;
            const float d = std::sqrt(d2);

            if (d > t) {
                const float coeff = t / d;
                displacements[v].x *= coeff;
                displacements[v].y *= coeff;
                // sum += t;
                // } else {
                // sum += d;
            }

            positions[v].x += displacements[v].x;
            positions[v].y += displacements[v].y;

            const auto mdl_id    = sim.models.get_id(mdl);
            const auto mdl_index = get_index(mdl_id);

            ImNodes::SetNodeGridSpacePos(mdl_index, positions[v]);
        }
    }

    ImNodes::EditorContextResetPanning(positions[0]);
}

status editor::initialize(u32 id) noexcept
{
    irt_return_if_bad(sim.init(to_unsigned(settings.kernel_model_cache),
                               to_unsigned(settings.kernel_message_cache)));
    irt_return_if_bad(plot_outs.init(to_unsigned(settings.kernel_model_cache)));
    irt_return_if_bad(file_outs.init(to_unsigned(settings.kernel_model_cache)));
    irt_return_if_bad(
      file_discrete_outs.init(to_unsigned(settings.kernel_model_cache)));

    irt_return_if_bad(srcs.init(50));
    sim.source_dispatch = srcs;

    try {
        observation_outputs.resize(sim.models.capacity());
        models_make_transition.resize(sim.models.capacity(), false);

        positions.resize(sim.models.capacity(), ImVec2{ 0.f, 0.f });
        displacements.resize(sim.models.capacity(), ImVec2{ 0.f, 0.f });

        observation_directory = std::filesystem::current_path();
    } catch (const std::bad_alloc& /*e*/) {
        return status::gui_not_enough_memory;
    }

    use_real_time        = false;
    synchronize_timestep = 0.;

    format(name, "Editor {}", id);

    return status::success;
}

template<typename Dynamics>
static status make_input_tooltip(simulation&  sim,
                                 Dynamics&    dyn,
                                 std::string& out)
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        int i = 0;
        for (auto& elem : dyn.x) {
            if (!have_message(elem))
                continue;

            fmt::format_to(std::back_inserter(out), "x[{}]: ", i);

            auto list = get_message(sim, dyn.x[i]);
            for (const auto& msg : list) {
                switch (msg.size()) {
                case 0:
                    fmt::format_to(std::back_inserter(out), "() ");
                    break;
                case 1:
                    fmt::format_to(std::back_inserter(out), "({}) ", msg[0]);
                    break;
                case 2:
                    fmt::format_to(
                      std::back_inserter(out), "({},{}) ", msg[0], msg[1]);
                    break;
                case 3:
                    fmt::format_to(std::back_inserter(out),
                                   "({},{},{}) ",
                                   msg[0],
                                   msg[1],
                                   msg[2]);
                    break;
                default:
                    break;
                }
            }

            ++i;
        }
    }

    return status::success;
}

static void show_tooltip(editor& ed, const model& mdl, const model_id id)
{
    ed.tooltip.clear();

    if (ed.models_make_transition[get_index(id)]) {
        fmt::format_to(std::back_inserter(ed.tooltip),
                       "Transition\n- last time: {}\n- next time:{}\n",
                       mdl.tl,
                       mdl.tn);

        auto ret = dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (is_detected_v<has_input_port_t, Dynamics>)
                return make_input_tooltip(ed.sim, dyn, ed.tooltip);

            return status::success;
        });

        if (is_bad(ret))
            ed.tooltip += "error\n";
    } else {
        fmt::format_to(std::back_inserter(ed.tooltip),
                       "Not in transition\n- last time: {}\n- next time:{}\n",
                       mdl.tl,
                       mdl.tn);
    }

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(ed.tooltip.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void editor::settings_manager::show(bool* is_open)
{
    ImGui::SetNextWindowPos(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_Once);
    if (!ImGui::Begin("Settings", is_open)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Kernel");
    ImGui::DragInt("model cache", &kernel_model_cache, 1.f, 1024, 1024 * 1024);
    ImGui::DragInt("msg cache", &kernel_message_cache, 1.f, 1024, 1024 * 1024);

    ImGui::Text("Graphics");
    ImGui::DragInt("node cache", &gui_node_cache, 1.f, 1024, 1024 * 1024);
    if (ImGui::ColorEdit3(
          "model", (float*)&gui_model_color, ImGuiColorEditFlags_NoOptions))
        compute_colors();

    ImGui::Text("Automatic layout parameters");
    ImGui::DragInt(
      "max iteration", &automatic_layout_iteration_limit, 1.f, 0, 1000);
    ImGui::DragFloat(
      "a-x-distance", &automatic_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "a-y-distance", &automatic_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::Text("Grid layout parameters");
    ImGui::DragFloat(
      "g-x-distance", &grid_layout_x_distance, 1.f, 150.f, 500.f);
    ImGui::DragFloat(
      "g-y-distance", &grid_layout_y_distance, 1.f, 150.f, 500.f);

    ImGui::End();
}

void editor::show_editor(window_logger& log_w) noexcept
{

    ImGui::Text("X -- delete selected nodes and/or connections / "
                "D -- duplicate selected nodes / "
                "G -- group model");

    constexpr ImGuiTableFlags flags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_BordersV;

    if (ImGui::BeginTable("Editor", 2, flags)) {
        ImNodes::EditorContextSet(context);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImNodes::BeginNodeEditor();

        show_top();
        show_connections();

        const bool open_popup =
          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
          ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1);

        model_id   new_model = undefined<model_id>();
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
        if (!ImGui::IsAnyItemHovered() && open_popup)
            ImGui::OpenPopup("Context menu");

        if (ImGui::BeginPopup("Context menu")) {

            if (ImGui::BeginMenu("QSS1")) {
                auto       i = static_cast<int>(dynamics_type::qss1_integrator);
                const auto e = static_cast<int>(dynamics_type::qss1_wsum_4) + 1;
                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS2")) {
                auto       i = static_cast<int>(dynamics_type::qss2_integrator);
                const auto e = static_cast<int>(dynamics_type::qss2_wsum_4) + 1;

                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS3")) {
                auto       i = static_cast<int>(dynamics_type::qss3_integrator);
                const auto e = static_cast<int>(dynamics_type::qss3_wsum_4) + 1;

                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("AQSS (experimental)")) {
                add_popup_menuitem(
                  *this, dynamics_type::integrator, &new_model);
                add_popup_menuitem(
                  *this, dynamics_type::quantifier, &new_model);
                add_popup_menuitem(*this, dynamics_type::adder_2, &new_model);
                add_popup_menuitem(*this, dynamics_type::adder_3, &new_model);
                add_popup_menuitem(*this, dynamics_type::adder_4, &new_model);
                add_popup_menuitem(*this, dynamics_type::mult_2, &new_model);
                add_popup_menuitem(*this, dynamics_type::mult_3, &new_model);
                add_popup_menuitem(*this, dynamics_type::mult_4, &new_model);
                add_popup_menuitem(*this, dynamics_type::cross, &new_model);
                ImGui::EndMenu();
            }

            add_popup_menuitem(*this, dynamics_type::counter, &new_model);
            add_popup_menuitem(*this, dynamics_type::queue, &new_model);
            add_popup_menuitem(*this, dynamics_type::dynamic_queue, &new_model);
            add_popup_menuitem(
              *this, dynamics_type::priority_queue, &new_model);
            add_popup_menuitem(*this, dynamics_type::generator, &new_model);
            add_popup_menuitem(*this, dynamics_type::constant, &new_model);
            add_popup_menuitem(*this, dynamics_type::time_func, &new_model);
            add_popup_menuitem(*this, dynamics_type::accumulator_2, &new_model);
            add_popup_menuitem(*this, dynamics_type::filter, &new_model);
            add_popup_menuitem(*this, dynamics_type::hsm_wrapper, &new_model);

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        if (show_minimap)
            ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomLeft);

        ImNodes::EndNodeEditor();

        if (new_model != undefined<model_id>()) {
            const auto mdl_index = get_index(new_model);
            ImNodes::SetNodeScreenSpacePos(mdl_index, click_pos);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

        int node_id;
        if (ImNodes::IsNodeHovered(&node_id) && is_running()) {
            auto* mdl = sim.models.try_to_get(node_id);
            if (mdl) {
                const auto mdl_id = sim.models.get_id(mdl);
                show_tooltip(*this, *mdl, mdl_id);
            }
        } else
            tooltip.clear();

        {
            int start = 0, end = 0;
            if (ImNodes::IsLinkCreated(&start, &end)) {
                const gport out = get_out(start);
                const gport in  = get_in(end);

                if (out.model && in.model && sim.can_connect(1)) {
                    if (auto status = sim.connect(
                          *out.model, out.port_index, *in.model, in.port_index);
                        is_bad(status))
                        log_w.log(6,
                                  "Fail to connect these models: %s\n",
                                  status_string(status));
                }
            }
        }

        ImGui::PopStyleVar();

        int                  num_selected_links = ImNodes::NumSelectedLinks();
        int                  num_selected_nodes = ImNodes::NumSelectedNodes();
        static ImVector<int> selected_nodes;
        static ImVector<int> selected_links;

        if (num_selected_nodes > 0) {
            selected_nodes.resize(num_selected_nodes, -1);

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
                ImNodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(7, "%d model(s) to delete\n", num_selected_nodes);
                free_children(selected_nodes);
                selected_nodes.clear();
                num_selected_nodes = 0;
                ImNodes::ClearNodeSelection();
            } else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('D')) {
                ImNodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(7, "%d model(s) to copy\n", num_selected_nodes);
                copy(selected_nodes);
                selected_nodes.clear();
                num_selected_nodes = 0;
                ImNodes::ClearNodeSelection();
            }
        } else if (num_selected_links > 0) {
            selected_links.resize(static_cast<size_t>(num_selected_links));

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
                std::fill_n(selected_links.begin(), selected_links.size(), -1);
                ImNodes::GetSelectedLinks(selected_links.begin());
                std::sort(selected_links.begin(),
                          selected_links.end(),
                          std::less<int>());

                int link_id_to_delete = selected_links[0];
                int current_link_id   = 0;
                int i                 = 0;

                log_w.log(
                  7, "%d connection(s) to delete\n", num_selected_links);

                auto selected_links_ptr  = selected_links.Data;
                auto selected_links_size = selected_links.Size;

                model* mdl = nullptr;
                while (sim.models.next(mdl) && link_id_to_delete != -1) {
                    dispatch(
                      *mdl,
                      [this,
                       &i,
                       &current_link_id,
                       selected_links_ptr,
                       selected_links_size,
                       &link_id_to_delete]<typename Dynamics>(Dynamics& dyn) {
                          if constexpr (is_detected_v<has_output_port_t,
                                                      Dynamics>) {
                              const int e = length(dyn.y);
                              int       j = 0;

                              while (j < e && link_id_to_delete != -1) {
                                  auto list = append_node(sim, dyn.y[j]);
                                  auto it   = list.begin();
                                  auto et   = list.end();

                                  while (it != et && link_id_to_delete != -1) {
                                      if (current_link_id ==
                                          link_id_to_delete) {

                                          it = list.erase(it);

                                          ++i;

                                          if (i != selected_links_size)
                                              link_id_to_delete =
                                                selected_links_ptr[i];
                                          else
                                              link_id_to_delete = -1;
                                      } else {
                                          ++it;
                                      }

                                      ++current_link_id;
                                  }

                                  ++j;
                              }
                          }
                      });
                }

                num_selected_links = 0;
                selected_links.resize(0);
                ImNodes::ClearLinkSelection();
            }
        }

        ImGui::TableSetColumnIndex(1);

        if (ImGui::CollapsingHeader("Selected Models",
                                    ImGuiTreeNodeFlags_CollapsingHeader |
                                      ImGuiTreeNodeFlags_DefaultOpen) &&
            num_selected_nodes) {
            selected_nodes.resize(num_selected_nodes, -1);
            ImNodes::GetSelectedNodes(selected_nodes.begin());

            static std::vector<std::string> names;
            names.clear();
            names.resize(selected_nodes.size());

            for (int i = 0, e = selected_nodes.size(); i != e; ++i)
                names[i] = fmt::format("{}", selected_nodes[i]);

            for (int i = 0, e = selected_nodes.size(); i != e; ++i) {
                auto* mdl = sim.models.try_to_get(selected_nodes[i]);
                if (!mdl)
                    continue;

                const auto id = sim.models.get_id(mdl);

                if (ImGui::TreeNodeEx(names[i].c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                    const auto index      = get_index(id);
                    auto&      out        = observation_outputs[index];
                    auto       old_choose = static_cast<int>(out.index());
                    auto       choose     = old_choose;

                    ImGui::Text(
                      "%s", dynamics_type_names[static_cast<int>(mdl->type)]);

                    const char* items[] = {
                        "none", "plot", "file", "file dt "
                    };
                    ImGui::Combo("type", &choose, items, IM_ARRAYSIZE(items));

                    if (choose == 1) {
                        if (old_choose == 2 || old_choose == 3) {
                            sim.observers.free(mdl->obs_id);
                            observation_outputs_free(index);
                        }

                        plot_output* plot;
                        if (old_choose != 1) {
                            plot_output& tf = plot_outs.alloc(names[i].c_str());
                            plot            = &tf;
                            tf.ed           = this;
                            observation_outputs[index] = plot_outs.get_id(tf);
                            auto& o                    = sim.observers.alloc(
                              names[i].c_str(), plot_output_callback, &tf);
                            sim.observe(*mdl, o);
                        } else {
                            plot = plot_outs.try_to_get(
                              std::get<plot_output_id>(out));
                            irt_assert(plot);
                        }

                        ImGui::InputText("name##plot",
                                         plot->name.begin(),
                                         plot->name.capacity());
                        ImGui::InputReal("dt##plot",
                                         &plot->time_step,
                                         to_real(0.001),
                                         to_real(1.0),
                                         "%.8f");
                    } else if (choose == 2) {
                        if (old_choose == 1 || old_choose == 3) {
                            sim.observers.free(mdl->obs_id);
                            observation_outputs_free(index);
                        }

                        file_output* file;
                        if (old_choose != 2) {
                            file_output& tf = file_outs.alloc(names[i].c_str());
                            file            = &tf;
                            tf.ed           = this;
                            observation_outputs[index] = file_outs.get_id(tf);
                            auto& o                    = sim.observers.alloc(
                              names[i].c_str(), file_output_callback, &tf);
                            sim.observe(*mdl, o);
                        } else {
                            file = file_outs.try_to_get(
                              std::get<file_output_id>(out));
                            irt_assert(file);
                        }

                        ImGui::InputText("name##file",
                                         file->name.begin(),
                                         file->name.capacity());
                    } else if (choose == 3) {
                        if (old_choose == 1 || old_choose == 2) {
                            sim.observers.free(mdl->obs_id);
                            observation_outputs_free(index);
                        }

                        file_discrete_output* file;
                        if (old_choose != 3) {
                            file_discrete_output& tf =
                              file_discrete_outs.alloc(names[i].c_str());
                            file  = &tf;
                            tf.ed = this;
                            observation_outputs[index] =
                              file_discrete_outs.get_id(tf);
                            auto& o =
                              sim.observers.alloc(names[i].c_str(),
                                                  file_discrete_output_callback,
                                                  &tf);
                            sim.observe(*mdl, o);
                        } else {
                            file = file_discrete_outs.try_to_get(
                              std::get<file_discrete_output_id>(out));
                            irt_assert(file);
                        }

                        ImGui::InputText("name##filedt",
                                         file->name.begin(),
                                         file->name.capacity());
                        ImGui::InputReal("dt##filedt",
                                         &file->time_step,
                                         to_real(0.001),
                                         to_real(1.0),
                                         "%.8f");
                    } else if (old_choose != choose) {
                        sim.observers.free(mdl->obs_id);
                        observation_outputs_free(index);
                    }

                    dispatch(*mdl, [this]<typename Dynamics>(Dynamics& dyn) {
                        ImGui::Spacing();
                        show_dynamics_inputs(this->srcs, dyn);
                    });

                    ImGui::TreePop();
                }
            }
        }

        ImGui::EndTable();
    }
}

bool editor::show_window(window_logger& log_w) noexcept
{
    ImGuiWindowFlags windows_flags = 0;
    windows_flags |= ImGuiWindowFlags_MenuBar;

    ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Once);
    if (!ImGui::Begin(name.c_str(), &show, windows_flags)) {
        ImGui::End();
        return true;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open"))
                show_load_file_dialog = true;

            if (!path.empty() && ImGui::MenuItem("Save")) {
                log_w.log(3,
                          "Write into file %s\n",
                          (const char*)path.u8string().c_str());
                if (auto os = std::ofstream(path); os.is_open()) {
                    writer w(os);
                    auto   ret = w(sim, srcs);
                    if (is_success(ret))
                        log_w.log(5, "success\n");
                    else
                        log_w.log(4, "error writing\n");
                }
            }

            if (ImGui::MenuItem("Save as..."))
                show_save_file_dialog = true;

            if (ImGui::MenuItem("Close")) {
                ImGui::EndMenu();
                ImGui::EndMenuBar();
                ImGui::End();
                return false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edition")) {
            ImGui::MenuItem("Show minimap", nullptr, &show_minimap);
            ImGui::MenuItem("Show parameter in models",
                            nullptr,
                            &settings.show_dynamics_inputs_in_editor);
            ImGui::Separator();
            if (ImGui::MenuItem("Clear"))
                clear();
            ImGui::Separator();
            if (ImGui::MenuItem("Grid Reorder"))
                compute_grid_layout();
            if (ImGui::MenuItem("Automatic Layout"))
                compute_automatic_layout();
            ImGui::Separator();
            if (ImGui::MenuItem("Settings"))
                show_settings = true;

            ImGui::EndMenu();
        }

        auto empty_fun = [](irt::model_id /*id*/) {};

        if (ImGui::BeginMenu("Examples")) {
            if (ImGui::MenuItem("Insert example AQSS lotka_volterra"))
                if (auto ret = add_lotka_volterra(); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize a Lotka Volterra "
                              "model (%s)\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert Izhikevitch model"))
                if (auto ret = add_izhikevitch(); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize an Izhikevitch "
                              "model (%s)\n",
                              status_string(ret));

            if (ImGui::MenuItem("Insert example QSS1 lotka_volterra"))
                if (auto ret = example_qss_lotka_volterra<1>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_lotka_volterra<1>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS1 negative_lif"))
                if (auto ret = example_qss_negative_lif<1>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_negative_lif<1>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS1 lif"))
                if (auto ret = example_qss_lif<1>(sim, empty_fun); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize example_qss_lif<1>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS1 van_der_pol"))
                if (auto ret = example_qss_van_der_pol<1>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_van_der_pol<1>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS1 izhikevich"))
                if (auto ret = example_qss_izhikevich<1>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_izhikevich<1>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS1 seir_nonlinear"))
                if (auto ret = example_qss_seir_nonlinear<1>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_seir_nonlinear<1>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 lotka_volterra"))
                if (auto ret = example_qss_lotka_volterra<2>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_lotka_volterra<2>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 negative_lif"))
                if (auto ret = example_qss_negative_lif<2>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_negative_lif<2>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 lif"))
                if (auto ret = example_qss_lif<2>(sim, empty_fun); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize example_qss_lif<2>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 van_der_pol"))
                if (auto ret = example_qss_van_der_pol<2>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_van_der_pol<2>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 izhikevich"))
                if (auto ret = example_qss_izhikevich<2>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_izhikevich<2>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS2 seir_nonlinear"))
                if (auto ret = example_qss_seir_nonlinear<2>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_seir_nonlinear<2>: %s\n",
                      status_string(ret));

            if (ImGui::MenuItem("Insert example QSS3 lotka_volterra"))
                if (auto ret = example_qss_lotka_volterra<3>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_lotka_volterra<3>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS3 negative_lif"))
                if (auto ret = example_qss_negative_lif<3>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize "
                              "example_qss_negative_lif<3>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS3 lif"))
                if (auto ret = example_qss_lif<3>(sim, empty_fun); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize example_qss_lif<3>: %s\n",
                              status_string(ret));
            if (ImGui::MenuItem("Insert example QSS3 van_der_pol"))
                if (auto ret = example_qss_van_der_pol<3>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_van_der_pol<3>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS3 izhikevich"))
                if (auto ret = example_qss_izhikevich<3>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_izhikevich<3>: %s\n",
                      status_string(ret));
            if (ImGui::MenuItem("Insert example QSS3 seir_nonlinear"))
                if (auto ret = example_qss_seir_nonlinear<3>(sim, empty_fun);
                    is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize example_qss_seir_nonlinear<3>: %s\n",
                      status_string(ret));

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

#if 0
    if (show_select_directory_dialog) {
        ImGui::OpenPopup("Select directory");
        if (select_directory_dialog(observation_directory)) {
            show_select_directory_dialog = false;

            log_w.log(
              5, "Output directory: %s", (const char*)path.u8string().c_str());
        }
    }

    if (show_load_file_dialog) {
        const char*    title     = "Select file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (load_file_dialog(path, title, filters)) {
            show_load_file_dialog = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)path.u8string().c_str());
            if (auto is = std::ifstream(path); is.is_open()) {
                reader r(is);
                auto   ret = r(sim, srcs, [&r](model_id id) {
                    const auto index = get_index(id);
                    const auto pos   = r.get_position(index);

                    ImNodes::SetNodeEditorSpacePos(index, ImVec2(pos.x, pos.y));
                });

                if (is_success(ret))
                    log_w.log(5, "success\n");
                else
                    log_w.log(4, "fail\n");
            }
        }
    }

    if (show_save_file_dialog) {
        if (sim.models.size()) {
            const char*    title     = "Select file path to save";
            const char8_t* filters[] = { u8".irt", nullptr };

            ImGui::OpenPopup(title);
            if (save_file_dialog(path, title, filters)) {
                show_save_file_dialog = false;
                log_w.log(
                  5, "Save file to %s\n", (const char*)path.u8string().c_str());

                log_w.log(3,
                          "Write into file %s\n",
                          (const char*)path.u8string().c_str());
                if (auto os = std::ofstream(path); os.is_open()) {
                    writer w(os);

                    auto ret =
                      w(sim, srcs, [](model_id mdl_id, float& x, float& y) {
                          const auto index = irt::get_index(mdl_id);
                          const auto pos   = ImNodes::GetNodeEditorSpacePos(
                            static_cast<int>(index));
                          x = pos.x;
                          y = pos.y;
                      });

                    if (is_success(ret))
                        log_w.log(5, "success\n");
                    else
                        log_w.log(4, "error writing\n");
                }
            }
        }
    }
#endif

    constexpr ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("editor bar", tab_bar_flags)) {
        if (ImGui::BeginTabItem("model")) {
            show_editor(log_w);
            ImGui::EndTabItem();
        }
#if 0
        if (ImGui::BeginTabItem("sources")) {
            auto* app = container_of(&log_w, &application::log_w);
            show_external_sources(*app, srcs);
            ImGui::EndTabItem();
        }
#endif
        ImGui::EndTabBar();
    }

    ImGui::End();

    return true;
}

} // namespace irt
