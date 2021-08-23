// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "application.hpp"
#include "dialog.hpp"
#include "internal.hpp"

#include <cinttypes>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/examples.hpp>
#include <irritator/io.hpp>

namespace irt {

static ImVec4
operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

inline int
make_input_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline int
make_output_node_id(const irt::model_id mdl, const int port) noexcept
{
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
    index |= port_index;

    return static_cast<int>(index);
}

inline std::pair<irt::u32, irt::u32>
get_model_input_port(const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask  = ~(15u << 28u);
    irt::u32           index = real_node_id & mask;

    return std::make_pair(index, port);
}

inline std::pair<irt::u32, irt::u32>
get_model_output_port(const int node_id) noexcept
{
    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);

    irt::u32 port = real_node_id >> 28u;
    irt_assert(port >= 8u && port < 16u);
    port -= 8u;
    irt_assert(port < 8u);

    constexpr irt::u32 mask = ~(15u << 28u);

    irt::u32 index = real_node_id & mask;

    return std::make_pair(index, port);
}

editor::editor() noexcept
{
    context = ImNodes::EditorContextCreate();
    ImNodes::PushAttributeFlag(
      ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    ImNodesIO& io                           = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

    settings.compute_colors();
}

editor::~editor() noexcept
{
    if (context) {
        ImNodes::EditorContextSet(context);
        ImNodes::PopAttributeFlag();
        ImNodes::EditorContextFree(context);
    }
}

void
editor::settings_manager::compute_colors() noexcept
{
    gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.25f);
    gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.5f);

    gui_hovered_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_transition_color * 1.25f);
    gui_selected_model_transition_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_transition_color * 1.5f);
}

void
editor::clear() noexcept
{
    sim.clear();
}

void
editor::free_children(const ImVector<int>& nodes) noexcept
{
    for (int i = 0, e = nodes.size(); i != e; ++i) {
        const auto* mdl = sim.models.try_to_get(nodes[i]);
        if (!mdl)
            continue;

        const auto child_id = sim.models.get_id(mdl);
        log_w.log(7, "delete %" PRIu64 "\n", child_id);
        sim.deallocate(child_id);

        observation_dispatch(
          get_index(child_id),
          [this](auto& outs, const auto id) { outs.free(id); });

        observation_outputs[get_index(child_id)] = std::monostate{};
    }
}

static void
compute_connection_distance(const model_id src,
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

static void
compute_connection_distance(const model& mdl, editor& ed, const float k)
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

void
editor::compute_grid_layout() noexcept
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

void
editor::compute_automatic_layout() noexcept
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
      line + (remaining > 0) ? settings.automatic_layout_y_distance : 0.f;
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

        auto sum = 0.f;
        mdl      = nullptr;
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
                sum += t;
            } else {
                sum += d;
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

static status
copy_port(simulation&                    sim,
          const map<model_id, model_id>& mapping,
          output_port&                   src,
          output_port&                   dst) noexcept
{
    if (src == static_cast<u64>(-1)) {
        dst = src;
        return status::success;
    }

    auto src_list = get_node(sim, src);
    auto dst_list = append_node(sim, dst);

    auto it = src_list.begin();
    auto et = src_list.end();

    while (it != et) {
        if (auto found = mapping.find(it->model); found != mapping.end()) {
            irt_return_if_fail(sim.can_connect(1u),
                               status::simulation_not_enough_connection);
            dst_list.emplace_back(found->v, it->port_index);
        } else {
            if (model* mdl = sim.models.try_to_get(it->model); mdl) {
                irt_return_if_fail(sim.can_connect(1u),
                                   status::simulation_not_enough_connection);

                dst_list.emplace_back(it->model, it->port_index);
            }
        }

        ++it;
    }

    return status::success;
}

status
editor::copy(const ImVector<int>& nodes) noexcept
{
    map<model_id, model_id> mapping;
    irt_return_if_bad(mapping.init(nodes.size()));

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        auto* src_mdl = sim.models.try_to_get(nodes[i]);
        if (!src_mdl)
            continue;

        irt_return_if_fail(sim.can_alloc(1),
                           status::simulation_not_enough_model);

        auto& dst_mdl    = sim.clone(*src_mdl);
        auto  src_mdl_id = sim.models.get_id(src_mdl);
        auto  dst_mdl_id = sim.models.get_id(dst_mdl);

        sim.make_initialize(dst_mdl, simulation_current);

        mapping.emplace_back(src_mdl_id, dst_mdl_id);
    }

    mapping.sort();

    for (int i = 0, e = length(mapping); i != e; ++i) {
        auto& src_mdl = sim.models.get(mapping[i].u);
        auto& dst_mdl = sim.models.get(mapping[i].v);

        dispatch(src_mdl,
                 [this, &mapping, &dst_mdl]<typename Dynamics>(Dynamics& dyn) {
                     if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                         for (int i = 0, e = length(dyn.y); i != e; ++i) {
                             auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);
                             irt_return_if_bad(
                               copy_port(sim, mapping, dyn.y[i], dst_dyn.y[i]));
                         }
                     }
                     return status::success;
                 });
    }

    return status::success;
}

status
editor::initialize(u32 id) noexcept
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

status
editor::add_lotka_volterra() noexcept
{
    using namespace irt::literals;

    if (!sim.models.can_alloc(10))
        return status::simulation_not_enough_model;

    auto& sum_a        = sim.alloc<adder_2>();
    auto& sum_b        = sim.alloc<adder_2>();
    auto& product      = sim.alloc<mult_2>();
    auto& integrator_a = sim.alloc<integrator>();
    auto& integrator_b = sim.alloc<integrator>();
    auto& quantifier_a = sim.alloc<quantifier>();
    auto& quantifier_b = sim.alloc<quantifier>();

    integrator_a.default_current_value = 18.0_r;

    quantifier_a.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_a.default_zero_init_offset = true;
    quantifier_a.default_step_size        = 0.01_r;
    quantifier_a.default_past_length      = 3;

    integrator_b.default_current_value = 7.0;

    quantifier_b.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_b.default_zero_init_offset = true;
    quantifier_b.default_step_size        = 0.01_r;
    quantifier_b.default_past_length      = 3;

    product.default_input_coeffs[0] = 1.0_r;
    product.default_input_coeffs[1] = 1.0_r;
    sum_a.default_input_coeffs[0]   = 2.0_r;
    sum_a.default_input_coeffs[1]   = -0.4_r;
    sum_b.default_input_coeffs[0]   = -1.0_r;
    sum_b.default_input_coeffs[1]   = 0.1_r;

    irt_return_if_bad(sim.connect(sum_a, 0, integrator_a, 1));
    irt_return_if_bad(sim.connect(sum_b, 0, integrator_b, 1));

    irt_return_if_bad(sim.connect(integrator_a, 0, sum_a, 0));
    irt_return_if_bad(sim.connect(integrator_b, 0, sum_b, 0));

    irt_return_if_bad(sim.connect(integrator_a, 0, product, 0));
    irt_return_if_bad(sim.connect(integrator_b, 0, product, 1));

    irt_return_if_bad(sim.connect(product, 0, sum_a, 1));
    irt_return_if_bad(sim.connect(product, 0, sum_b, 1));

    irt_return_if_bad(sim.connect(quantifier_a, 0, integrator_a, 0));
    irt_return_if_bad(sim.connect(quantifier_b, 0, integrator_b, 0));
    irt_return_if_bad(sim.connect(integrator_a, 0, quantifier_a, 0));
    irt_return_if_bad(sim.connect(integrator_b, 0, quantifier_b, 0));

    return status::success;
}

status
editor::add_izhikevitch() noexcept
{
    using namespace irt::literals;

    if (!sim.models.can_alloc(14))
        return status::simulation_not_enough_model;

    auto& constant     = sim.alloc<irt::constant>();
    auto& constant2    = sim.alloc<irt::constant>();
    auto& constant3    = sim.alloc<irt::constant>();
    auto& sum_a        = sim.alloc<irt::adder_2>();
    auto& sum_b        = sim.alloc<irt::adder_2>();
    auto& sum_c        = sim.alloc<irt::adder_4>();
    auto& sum_d        = sim.alloc<irt::adder_2>();
    auto& product      = sim.alloc<irt::mult_2>();
    auto& integrator_a = sim.alloc<irt::integrator>();
    auto& integrator_b = sim.alloc<irt::integrator>();
    auto& quantifier_a = sim.alloc<irt::quantifier>();
    auto& quantifier_b = sim.alloc<irt::quantifier>();
    auto& cross        = sim.alloc<irt::cross>();
    auto& cross2       = sim.alloc<irt::cross>();

    real a  = 0.2_r;
    real b  = 2.0_r;
    real c  = -56.0_r;
    real d  = -16.0_r;
    real I  = -99.0_r;
    real vt = 30.0_r;

    constant.default_value  = 1.0_r;
    constant2.default_value = c;
    constant3.default_value = I;

    cross.default_threshold  = vt;
    cross2.default_threshold = vt;

    integrator_a.default_current_value = 0.0_r;

    quantifier_a.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_a.default_zero_init_offset = true;
    quantifier_a.default_step_size        = 0.01_r;
    quantifier_a.default_past_length      = 3;

    integrator_b.default_current_value = 0.0_r;

    quantifier_b.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_b.default_zero_init_offset = true;
    quantifier_b.default_step_size        = 0.01_r;
    quantifier_b.default_past_length      = 3;

    product.default_input_coeffs[0] = 1.0_r;
    product.default_input_coeffs[1] = 1.0_r;

    sum_a.default_input_coeffs[0] = 1.0_r;
    sum_a.default_input_coeffs[1] = -1.0_r;
    sum_b.default_input_coeffs[0] = -a;
    sum_b.default_input_coeffs[1] = a * b;
    sum_c.default_input_coeffs[0] = 0.04_r;
    sum_c.default_input_coeffs[1] = 5.0_r;
    sum_c.default_input_coeffs[2] = 140.0_r;
    sum_c.default_input_coeffs[3] = 1.0_r;
    sum_d.default_input_coeffs[0] = 1.0_r;
    sum_d.default_input_coeffs[1] = d;

    irt_return_if_bad(sim.connect(integrator_a, 0, cross, 0));
    irt_return_if_bad(sim.connect(constant2, 0, cross, 1));
    irt_return_if_bad(sim.connect(integrator_a, 0, cross, 2));

    irt_return_if_bad(sim.connect(cross, 0, quantifier_a, 0));
    irt_return_if_bad(sim.connect(cross, 0, product, 0));
    irt_return_if_bad(sim.connect(cross, 0, product, 1));
    irt_return_if_bad(sim.connect(product, 0, sum_c, 0));
    irt_return_if_bad(sim.connect(cross, 0, sum_c, 1));
    irt_return_if_bad(sim.connect(cross, 0, sum_b, 1));

    irt_return_if_bad(sim.connect(constant, 0, sum_c, 2));
    irt_return_if_bad(sim.connect(constant3, 0, sum_c, 3));

    irt_return_if_bad(sim.connect(sum_c, 0, sum_a, 0));
    irt_return_if_bad(sim.connect(integrator_b, 0, sum_a, 1));
    irt_return_if_bad(sim.connect(cross2, 0, sum_a, 1));
    irt_return_if_bad(sim.connect(sum_a, 0, integrator_a, 1));
    irt_return_if_bad(sim.connect(cross, 0, integrator_a, 2));
    irt_return_if_bad(sim.connect(quantifier_a, 0, integrator_a, 0));

    irt_return_if_bad(sim.connect(cross2, 0, quantifier_b, 0));
    irt_return_if_bad(sim.connect(cross2, 0, sum_b, 0));
    irt_return_if_bad(sim.connect(quantifier_b, 0, integrator_b, 0));
    irt_return_if_bad(sim.connect(sum_b, 0, integrator_b, 1));

    irt_return_if_bad(sim.connect(cross2, 0, integrator_b, 2));
    irt_return_if_bad(sim.connect(integrator_a, 0, cross2, 0));
    irt_return_if_bad(sim.connect(integrator_b, 0, cross2, 2));
    irt_return_if_bad(sim.connect(sum_d, 0, cross2, 1));
    irt_return_if_bad(sim.connect(integrator_b, 0, sum_d, 0));
    irt_return_if_bad(sim.connect(constant, 0, sum_d, 1));

    return status::success;
}

static int
show_connection(editor& ed, model& mdl, int connection_id)
{
    dispatch(
      mdl,
      [&ed, &mdl, &connection_id]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              for (int i = 0, e = length(dyn.y); i != e; ++i) {
                  auto list = append_node(ed.sim, dyn.y[i]);
                  auto it   = list.begin();
                  auto et   = list.end();

                  while (it != et) {
                      if (auto* mdl_dst = ed.sim.models.try_to_get(it->model);
                          mdl_dst) {
                          int out = make_output_node_id(ed.sim.get_id(dyn), i);
                          int in =
                            make_input_node_id(it->model, it->port_index);
                          ImNodes::Link(connection_id++, out, in);
                          ++it;
                      } else {
                          it = list.erase(it);
                      }
                  }
              }
          }
      });

    return connection_id;
}

void
editor::show_connections() noexcept
{
    int connection_id = 0;

    for (model* mdl = nullptr; sim.models.next(mdl);)
        connection_id = show_connection(*this, *mdl, connection_id);
}

template<typename Dynamics>
static void
add_input_attribute(editor& ed, const Dynamics& dyn) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        sz i = 0;
        for ([[maybe_unused]] auto& elem : dyn.x) {
            irt_assert(i < 8u);
            const auto& mdl    = get_model(dyn);
            const auto  mdl_id = ed.sim.models.get_id(mdl);

            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginInputAttribute(make_input_node_id(mdl_id, (int)i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
            ++i;
        }
    }
}

template<typename Dynamics>
static void
add_output_attribute(editor& ed, const Dynamics& dyn) noexcept
{
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();

        sz i = 0;
        for ([[maybe_unused]] auto& elem : dyn.y) {
            irt_assert(i < 8u);
            const auto& mdl    = get_model(dyn);
            const auto  mdl_id = ed.sim.models.get_id(mdl);

            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_id, (int)i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
            ++i;
        }
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const none& /*dyn*/)
{}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_integrator& dyn)
{
    ImGui::TextFormat("X {}", dyn.X);
    ImGui::TextFormat("dQ {}", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_multiplier& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_2& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_3& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_4& dyn)
{
    ImGui::TextFormat("{}", dyn.values[0]);
    ImGui::TextFormat("{}", dyn.values[1]);
    ImGui::TextFormat("{}", dyn.values[2]);
    ImGui::TextFormat("{}", dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_multiplier& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_2& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[2]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_3& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[3]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_4& dyn)
{
    ImGui::TextFormat("{} {}", dyn.values[0], dyn.values[4]);
    ImGui::TextFormat("{} {}", dyn.values[1], dyn.values[5]);
    ImGui::TextFormat("{} {}", dyn.values[2], dyn.values[6]);
    ImGui::TextFormat("{} {}", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const integrator& dyn)
{
    ImGui::TextFormat("value {}", dyn.current_value);
}

static void
show_dynamics_values(simulation& /*sim*/, const quantifier& dyn)
{
    ImGui::TextFormat("up threshold {}", dyn.m_upthreshold);
    ImGui::TextFormat("down threshold {}", dyn.m_downthreshold);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_2& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_3& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_4& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::TextFormat("{} * {}", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_2& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_3& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_4& dyn)
{
    ImGui::TextFormat("{} * {}", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::TextFormat("{} * {}", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::TextFormat("{} * {}", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::TextFormat("{} * {}", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const counter& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
}

static void
show_dynamics_values(simulation& sim, const queue& dyn)
{
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
    }
}

static void
show_dynamics_values(simulation& sim, const dynamic_queue& dyn)
{
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
    }
}

static void
show_dynamics_values(simulation& sim, const priority_queue& dyn)
{
    if (dyn.fifo == u64(-1)) {
        ImGui::TextFormat("empty");
    } else {
        auto list = get_dated_message(sim, dyn.fifo);
        ImGui::TextFormat("next ta {}", list.front().data[0]);
        ImGui::TextFormat("next value {}", list.front().data[1]);
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const generator& dyn)
{
    ImGui::TextFormat("next {}", dyn.sigma);
}

static void
show_dynamics_values(simulation& /*sim*/, const constant& dyn)
{
    ImGui::TextFormat("next {}", dyn.sigma);
    ImGui::TextFormat("value {}", dyn.value);
}

template<int QssLevel>
static void
show_dynamics_values(simulation& /*sim*/, const abstract_cross<QssLevel>& dyn)
{
    ImGui::TextFormat("threshold: {}", dyn.threshold);
    ImGui::TextFormat("value: {}", dyn.value[0]);
    ImGui::TextFormat("if-value: {}", dyn.if_value[0]);
    ImGui::TextFormat("else-value: {}", dyn.else_value[0]);

    if (dyn.detect_up)
        ImGui::TextFormat("up detection");
    else
        ImGui::TextFormat("down detection");
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_power& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_power& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_power& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_square& dyn)
{
    ImGui::TextFormat("{}", dyn.value[0]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_square& dyn)
{
    ImGui::TextFormat("{} {}", dyn.value[0], dyn.value[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_square& dyn)
{
    ImGui::TextFormat("{} {} {}", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const cross& dyn)
{
    ImGui::TextFormat("threshold: {}", dyn.threshold);
    ImGui::TextFormat("value: {}", dyn.value);
    ImGui::TextFormat("if-value: {}", dyn.if_value);
    ImGui::TextFormat("else-value: {}", dyn.else_value);
}

static void
show_dynamics_values(simulation& /*sim*/, const accumulator_2& dyn)
{
    ImGui::TextFormat("number {}", dyn.number);
    ImGui::TextFormat("- 0: {}", dyn.numbers[0]);
    ImGui::TextFormat("- 1: {}", dyn.numbers[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const time_func& dyn)
{
    ImGui::TextFormat("value {}", dyn.value);
}

static void
show_dynamics_inputs(editor& /*ed*/, none& /*dyn*/)
{}

static void
show_dynamics_values(simulation& /*sim*/, const filter& dyn)
{
    ImGui::Text("%.3f", dyn.lower_threshold);
    ImGui::Text("%.3f", dyn.upper_threshold);
    ImGui::Text("%.3f", dyn.inValue[0]);
}

static void
show_dynamics_values(simulation& /*sim*/, const flow& dyn)
{
    if (dyn.i < dyn.default_size)
        ImGui::TextFormat("value {}", dyn.default_data[dyn.i]);
    else
        ImGui::TextFormat("no data");
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_X);
    ImGui::InputReal("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_wsum_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_wsum_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_wsum_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, integrator& dyn)
{
    ImGui::InputReal("value", &dyn.default_current_value);
    ImGui::InputReal("reset", &dyn.default_reset_value);
}

static void
show_dynamics_inputs(editor& /*ed*/, quantifier& dyn)
{
    ImGui::InputReal("quantum", &dyn.default_step_size);
    ImGui::SliderInt("archive length", &dyn.default_past_length, 3, 100);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_2& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_3& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_4& dyn)
{
    ImGui::InputReal("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputReal("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputReal("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputReal("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, counter& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, queue& dyn)
{
    ImGui::InputReal("delay", &dyn.default_ta);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");
}

static void
show_external_sources_combo(external_source& srcs,
                            const char*      title,
                            source&          src)
{
    small_string<63> label("None");

    external_source_type type;
    if (!(external_source_type_cast(src.type, &type))) {
        src.reset();
    } else {
        switch (type) {
        case external_source_type::binary_file: {
            const auto id    = enum_cast<binary_file_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.binary_file_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::binary_file),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::constant: {
            const auto id    = enum_cast<constant_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.constant_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::constant),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::random: {
            const auto id    = enum_cast<random_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.random_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::random),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        case external_source_type::text_file: {
            const auto id    = enum_cast<text_file_source_id>(src.id);
            const auto index = get_index(id);
            if (auto* es = srcs.text_file_sources.try_to_get(id)) {
                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::text_file),
                       index,
                       es->name.c_str());
            } else {
                src.reset();
            }
        } break;
        default:
            irt_unreachable();
        }
    }

    if (ImGui::BeginCombo(title, label.c_str())) {
        {
            bool is_selected = src.type == -1;
            if (ImGui::Selectable("None", is_selected)) {
                src.reset();
            }
        }

        {
            constant_source* s = nullptr;
            while (srcs.constant_sources.next(s)) {
                const auto id    = srcs.constant_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::constant),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::constant) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::constant);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            binary_file_source* s = nullptr;
            while (srcs.binary_file_sources.next(s)) {
                const auto id    = srcs.binary_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::binary_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::binary_file) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::binary_file);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            random_source* s = nullptr;
            while (srcs.random_sources.next(s)) {
                const auto id    = srcs.random_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::random),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::random) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::random);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            text_file_source* s = nullptr;
            while (srcs.text_file_sources.next(s)) {
                const auto id    = srcs.text_file_sources.get_id(s);
                const auto index = get_index(id);

                format(label,
                       "{}-{} {}",
                       ordinal(external_source_type::text_file),
                       index,
                       s->name.c_str());

                bool is_selected =
                  src.type == ordinal(external_source_type::text_file) &&
                  src.id == ordinal(id);
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    src.type = ordinal(external_source_type::text_file);
                    src.id   = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        ImGui::EndCombo();
    }
}

static void
show_dynamics_inputs(editor& ed, dynamic_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the dynamic queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(ed.srcs, "time", dyn.default_source_ta);
}

static void
show_dynamics_inputs(editor& ed, priority_queue& dyn)
{
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker(
      "Unchecked, the priority queue stops to send data if the source are "
      "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(ed.srcs, "time", dyn.default_source_ta);
}

static void
show_dynamics_inputs(editor& ed, generator& dyn)
{
    ImGui::InputReal("offset", &dyn.default_offset);
    ImGui::Checkbox("Stop on error", &dyn.stop_on_error);
    ImGui::SameLine();
    HelpMarker("Unchecked, the generator stops to send data if the source are "
               "empty or undefined. Checked, the simulation will stop.");

    show_external_sources_combo(ed.srcs, "source", dyn.default_source_value);
    show_external_sources_combo(ed.srcs, "time", dyn.default_source_ta);
}

static void
show_dynamics_inputs(editor& ed, constant& dyn)
{
    ImGui::InputReal("value", &dyn.default_value);
    ImGui::InputReal("offset", &dyn.default_offset);

    if (ed.is_running()) {
        if (ImGui::Button("Send now")) {
            dyn.value = dyn.default_value;
            dyn.sigma = dyn.default_offset;

            auto& mdl = get_model(dyn);
            mdl.tl    = ed.simulation_current;
            mdl.tn    = ed.simulation_current + dyn.sigma;
            if (dyn.sigma && mdl.tn == ed.simulation_current)
                mdl.tn = std::nextafter(ed.simulation_current,
                                        ed.simulation_current + to_real(1.));

            ed.sim.sched.update(mdl, mdl.tn);
        }
    }
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_power& dyn)
{
    ImGui::InputReal("n", &dyn.default_n);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_square& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_square& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_square& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, cross& dyn)
{
    ImGui::InputReal("threshold", &dyn.default_threshold);
}

static void
show_dynamics_inputs(editor& /*ed*/, accumulator_2& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, filter& dyn)
{
    ImGui::InputDouble("Lower threshold ", &dyn.default_lower_threshold);
    ImGui::InputDouble("Upper threshold ", &dyn.default_upper_threshold);
}

static void
show_dynamics_inputs(editor& /*ed*/, flow& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, time_func& dyn)
{
    static const char* items[] = { "time", "square", "sin" };

    ImGui::PushItemWidth(120.0f);
    int item_current = dyn.default_f == &time_function          ? 0
                       : dyn.default_f == &square_time_function ? 1
                                                                : 2;

    if (ImGui::Combo("function", &item_current, items, IM_ARRAYSIZE(items))) {
        dyn.default_f = item_current == 0   ? &time_function
                        : item_current == 1 ? &square_time_function
                                            : sin_time_function;
    }
    ImGui::PopItemWidth();
}

void
editor::show_model_dynamics(model& mdl) noexcept
{
    if (simulation_show_value && st != editor_status::editing) {
        dispatch(mdl, [&](const auto& dyn) {
            add_input_attribute(*this, dyn);
            ImGui::PushItemWidth(120.0f);
            show_dynamics_values(sim, dyn);
            ImGui::PopItemWidth();
            add_output_attribute(*this, dyn);
        });
    } else {
        dispatch(mdl, [&](auto& dyn) {
            add_input_attribute(*this, dyn);
            ImGui::PushItemWidth(120.0f);

            if (settings.show_dynamics_inputs_in_editor)
                show_dynamics_inputs(*this, dyn);
            ImGui::PopItemWidth();
            add_output_attribute(*this, dyn);
        });
    }
}

template<typename Dynamics>
static status
make_input_tooltip(simulation& sim, Dynamics& dyn, std::string& out)
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

static void
show_tooltip(editor& ed, const model& mdl, const model_id id)
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

void
editor::show_top() noexcept
{
    model* mdl = nullptr;
    while (sim.models.next(mdl)) {
        const auto mdl_id    = sim.models.get_id(mdl);
        const auto mdl_index = get_index(mdl_id);

        if (st != editor_status::editing && models_make_transition[mdl_index]) {

            ImNodes::PushColorStyle(ImNodesCol_TitleBar,
                                    ImGui::ColorConvertFloat4ToU32(
                                      settings.gui_model_transition_color));

            ImNodes::PushColorStyle(
              ImNodesCol_TitleBarHovered,
              settings.gui_hovered_model_transition_color);
            ImNodes::PushColorStyle(
              ImNodesCol_TitleBarSelected,
              settings.gui_selected_model_transition_color);
        } else {
            ImNodes::PushColorStyle(
              ImNodesCol_TitleBar,
              ImGui::ColorConvertFloat4ToU32(settings.gui_model_color));

            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                    settings.gui_hovered_model_color);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                    settings.gui_selected_model_color);
        }

        ImNodes::BeginNode(mdl_index);
        ImNodes::BeginNodeTitleBar();
        // ImGui::TextUnformatted(mdl->name.c_str());
        // ImGui::OpenPopupOnItemClick("Rename model", 1);

        // bool is_rename = true;
        // ImGui::SetNextWindowSize(ImVec2(250, 200),
        // ImGuiCond_Always); if (ImGui::BeginPopupModal("Rename
        // model", &is_rename)) {
        //    ImGui::InputText(
        //      "Name##edit-1", mdl->name.begin(),
        //      mdl->name.capacity());
        //    if (ImGui::Button("Close"))
        //        ImGui::CloseCurrentPopup();
        //    ImGui::EndPopup();
        //}

        ImGui::TextFormat("{}\n{}",
                          mdl_index,
                          dynamics_type_names[static_cast<int>(mdl->type)]);

        ImNodes::EndNodeTitleBar();
        show_model_dynamics(*mdl);
        ImNodes::EndNode();

        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }
}

void
editor::settings_manager::show(bool* is_open)
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

status
add_popup_menuitem(editor& ed, dynamics_type type, model_id* new_model)
{
    if (!ed.sim.models.can_alloc(1))
        return status::data_array_not_enough_memory;

    if (ImGui::MenuItem(dynamics_type_names[static_cast<i8>(type)])) {
        auto& mdl  = ed.sim.alloc(type);
        *new_model = ed.sim.models.get_id(mdl);

        return ed.sim.make_initialize(mdl, ed.simulation_current);
    }

    return status::success;
}

void
editor::show_editor() noexcept
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
                auto i = static_cast<int>(dynamics_type::qss1_integrator);
                const auto e = static_cast<int>(dynamics_type::qss1_wsum_4) + 1;
                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS2")) {
                auto i = static_cast<int>(dynamics_type::qss2_integrator);
                const auto e = static_cast<int>(dynamics_type::qss2_wsum_4) + 1;

                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS3")) {
                auto i = static_cast<int>(dynamics_type::qss3_integrator);
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
            add_popup_menuitem(*this, dynamics_type::flow, &new_model);

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
                       &mdl,
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
                        show_dynamics_inputs(*this, dyn);
                    });

                    ImGui::TreePop();
                }
            }
        }

        ImGui::EndTable();
    }
}

bool
editor::show_window() noexcept
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

        auto empty_fun = [this](irt::model_id /*id*/) {};

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
                auto   ret = r(sim, srcs, [&r, this](model_id id) {
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

    constexpr ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("editor bar", tab_bar_flags)) {
        if (ImGui::BeginTabItem("model")) {
            show_editor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("sources")) {
            show_sources();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    return true;
}

} // namespace irt
