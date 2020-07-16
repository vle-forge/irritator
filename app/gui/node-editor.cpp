// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef _WIN32
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#endif

#include "node-editor.hpp"
#include "gui.hpp"
#include "imnodes.hpp"
#include "implot.h"

#include <fstream>
#include <string>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/io.hpp>

namespace irt {

static int kernel_model_cache = 1024;
static int kernel_message_cache = 32768;

static int gui_node_cache = 1024;
static ImVec4 gui_model_color{ .27f, .27f, .54f, 1.f };
static ImVec4 gui_cluster_color{ .27f, .54f, .27f, 1.f };

static ImU32 gui_hovered_model_color;
static ImU32 gui_hovered_cluster_color;
static ImU32 gui_selected_model_color;
static ImU32 gui_selected_cluster_color;

static int automatic_layout_iteration_limit = 200;
static auto automatic_layout_x_distance = 350.f;
static auto automatic_layout_y_distance = 350.f;
static auto grid_layout_x_distance = 250.f;
static auto grid_layout_y_distance = 250.f;

static ImVec4
operator*(const ImVec4& lhs, const float rhs) noexcept
{
    return ImVec4(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs);
}

static void
compute_color() noexcept
{
    gui_hovered_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.25f);
    gui_selected_model_color =
      ImGui::ColorConvertFloat4ToU32(gui_model_color * 1.5f);

    gui_hovered_cluster_color =
      ImGui::ColorConvertFloat4ToU32(gui_cluster_color * 1.25f);
    gui_selected_cluster_color =
      ImGui::ColorConvertFloat4ToU32(gui_cluster_color * 1.5f);
}

template<size_t N, typename... Args>
void
format(small_string<N>& str, const char* fmt, const Args&... args)
{
    auto ret = fmt::format_to_n(str.begin(), N, fmt, args...);
    str.size(ret.size);
}

template<typename DataArray, typename Container, typename Function>
void
for_each(DataArray& d_array, Container& container, Function f) noexcept
{
    using identifier_type = typename DataArray::identifier_type;

    static_assert(
      std::is_same<identifier_type, typename Container::value_type>::value,
      "Container must store same identifier_type as DataArray");

    auto first = std::begin(container);
    [[maybe_unused]] auto previous = first;
    auto last = std::end(container);

    while (first != last) {
        if (auto* ptr = d_array.try_to_get(*first); ptr) {
            f(*ptr, *first);

            if constexpr (std::is_same_v<std::vector<identifier_type>,
                                         std::remove_cvref_t<Container>>) {
                ++first;
            } else if constexpr (std::is_same_v<
                                   flat_list<identifier_type>,
                                   std::remove_cvref_t<Container>>) {
                previous = first++;
            } else {
                abort();
            }
        } else {
            if constexpr (std::is_same_v<std::vector<identifier_type>,
                                         std::remove_cvref_t<Container>>) {
                std::swap(*first, container.back());
                container.pop_back();
                last = std::end(container);
            } else if constexpr (std::is_same_v<
                                   flat_list<identifier_type>,
                                   std::remove_cvref_t<Container>>) {
                if (previous == first) {
                    container.pop_front();
                    first = container.begin();
                    previous = first;
                } else {
                    first = container.erase_after(previous);
                }
            } else {
                abort();
            }
        }
    }
}

static window_logger log_w;
static data_array<editor, editor_id> editors;

static void
observation_output_initialize(const irt::observer& obs,
                              const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    if (match(output->observation_type,
              observation_output::type::multiplot,
              observation_output::type::both)) {
        std::fill_n(output->xs.data(), output->xs.size(), 0.f);
        std::fill_n(output->ys.data(), output->ys.size(), 0.f);
        output->tl = t;
        output->min = -1.f;
        output->max = +1.f;
        output->id = 0;
    }

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        if (!output->ofs.is_open()) {
            if (output->observation_type == observation_output::type::both)
                output->observation_type = observation_output::type::multiplot;
            else
                output->observation_type = observation_output::type::none;
        } else
            output->ofs << "t," << output->name << '\n';
    }
}

static void
observation_output_observe(const irt::observer& obs,
                           const irt::time t,
                           const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    const auto value = static_cast<float>(msg[0]);

    if (match(output->observation_type,
              observation_output::type::multiplot,
              observation_output::type::both)) {
        output->min = std::min(output->min, value);
        output->max = std::max(output->max, value);

        for (double to_fill = output->tl; to_fill < t;
             to_fill += obs.time_step) {
            if (static_cast<size_t>(output->id) < output->xs.size()) {
                output->ys[output->id] = value;
                output->xs[output->id] = static_cast<float>(t);
                ++output->id;
            }
        }
    }

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        output->ofs << t << ',' << value << '\n';
    }

    output->tl = t;
}

static void
observation_output_free(const irt::observer& obs,
                        const irt::time /*t*/) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);

    if (match(output->observation_type,
              observation_output::type::file,
              observation_output::type::both)) {
        output->ofs.close();
    }
}

static void
run_simulation(simulation& sim,
               double begin,
               double end,
               double& current,
               simulation_status& st,
               const bool& stop) noexcept
{
    current = begin;
    if (auto ret = sim.initialize(current); irt::is_bad(ret)) {
        log_w.log(3,
                  "Simulation initialization failure (%s)\n",
                  irt::status_string(ret));
        st = simulation_status::success;
        return;
    }

    do {
        if (auto ret = sim.run(current); irt::is_bad(ret)) {
            log_w.log(3, "Simulation failure (%s)\n", irt::status_string(ret));
            st = simulation_status::success;
            return;
        }
    } while (current < end && !stop);

    sim.clean();

    st = simulation_status::running_once_need_join;
}

void
editor::clear() noexcept
{
    observation_outputs.clear();
    observation_types.clear();
    observation_directory.clear();

    clusters.clear();
    sim.clear();

    std::fill(std::begin(clusters_mapper),
              std::end(clusters_mapper),
              undefined<cluster_id>());

    std::fill(std::begin(models_mapper),
              std::end(models_mapper),
              undefined<cluster_id>());

    top.clear();
}

cluster_id
editor::ancestor(const child_id child) const noexcept
{
    if (child.index() == 0) {
        const auto mdl_id = std::get<model_id>(child);
        auto parent = models_mapper[get_index(mdl_id)];
        auto ret = parent;

        while (parent != undefined<cluster_id>()) {
            ret = parent;
            parent = clusters_mapper[get_index(parent)];
        }

        return ret;
    } else {
        const auto gp_id = std::get<cluster_id>(child);
        auto parent = clusters_mapper[get_index(gp_id)];
        auto ret = parent;

        while (parent != undefined<cluster_id>()) {
            ret = parent;
            parent = clusters_mapper[get_index(parent)];
        }

        return ret;
    }
}

int
editor::get_top_group_ref(const child_id child) const noexcept
{
    const auto top_ref = ancestor(child);

    return top_ref == undefined<cluster_id>() ? top.get_index(child)
                                              : top.get_index(top_ref);
}

bool
editor::is_in_hierarchy(const cluster& group,
                        const cluster_id group_to_search) const noexcept
{
    if (clusters.get_id(group) == group_to_search) {
        log_w.log(7, "clusters.get_id(group) == group_to_search\n");
        return true;
    }

    // TODO: Derecursive this part of the function.
    for (const auto elem : group.children) {
        if (elem.index() == 1) {
            const auto id = std::get<cluster_id>(elem);

            if (id == group_to_search) {
                log_w.log(7, "id == group_to_search\n");
                return true;
            }

            if (const auto* gp = clusters.try_to_get(id); gp) {
                if (is_in_hierarchy(*gp, group_to_search)) {
                    log_w.log(7, "is_in_hierarchy = true\n");
                    return true;
                }
            }
        }
    }

    return false;
}

void
editor::group(const ImVector<int>& nodes) noexcept
{
    if (!clusters.can_alloc(1)) {
        log_w.log(5, "Fail to allocate a new group.");
        return;
    }

    /* First, move children models and groups from the current cluster into the
       newly allocated cluster. */

    auto& new_cluster = clusters.alloc();
    auto new_cluster_id = clusters.get_id(new_cluster);
    format(new_cluster.name, "Group {}", new_cluster_id);
    parent(new_cluster_id, undefined<cluster_id>());

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        if (auto index = top.get_index(nodes[i]); index != not_found) {
            new_cluster.children.push_back(top.children[index].first);
            top.pop(index);
        }
    }

    top.emplace_back(new_cluster_id);

    for (const auto child : new_cluster.children) {
        if (child.index() == 0) {
            const auto id = std::get<model_id>(child);
            parent(id, new_cluster_id);
        } else {
            const auto id = std::get<cluster_id>(child);
            parent(id, new_cluster_id);
        }
    }

    /* For all input and output ports of the remaining models in the current
       cluster, we try to detect if the corresponding model is or is not in the
       same cluster. */

    for (const auto child : top.children) {
        if (child.first.index() == 0) {
            const auto child_id = std::get<model_id>(child.first);

            if (auto* model = sim.models.try_to_get(child_id); model) {
                sim.for_all_input_port(
                  *model,
                  [this, &new_cluster](const input_port& port,
                                       input_port_id /*pid*/) {
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.output_ports.try_to_get(id);
                              p)
                              if (is_in_hierarchy(new_cluster,
                                                  this->parent(p->model)))
                                  new_cluster.output_ports.emplace_back(id);
                      }
                  });

                sim.for_all_output_port(
                  *model,
                  [this, &new_cluster](const output_port& port,
                                       output_port_id /*pid*/) {
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.input_ports.try_to_get(id); p)
                              if (is_in_hierarchy(new_cluster,
                                                  this->parent(p->model)))
                                  new_cluster.input_ports.emplace_back(id);
                      }
                  });
            }
        } else {
            const auto child_id = std::get<cluster_id>(child.first);

            if (auto* group = clusters.try_to_get(child_id); group) {
                for (const auto id : group->input_ports) {
                    if (auto* p = sim.input_ports.try_to_get(id); p) {
                        for (const auto d_id : p->connections) {
                            if (auto* d_p = sim.output_ports.try_to_get(d_id);
                                d_p) {
                                if (is_in_hierarchy(new_cluster,
                                                    this->parent(d_p->model)))
                                    new_cluster.output_ports.emplace_back(d_id);
                            }
                        }
                    }
                }

                for (const auto id : group->output_ports) {
                    if (auto* p = sim.output_ports.try_to_get(id); p) {
                        for (const auto d_id : p->connections) {
                            if (auto* d_p = sim.input_ports.try_to_get(d_id);
                                d_p) {
                                if (is_in_hierarchy(new_cluster,
                                                    this->parent(d_p->model)))
                                    new_cluster.input_ports.emplace_back(d_id);
                            }
                        }
                    }
                }
            }
        }
    }
}

void
editor::ungroup(const int node) noexcept
{
    const auto index = top.get_index(node);

    if (index == not_found) {
        log_w.log(5, "ungroup model not in top\n");
        return;
    }

    if (top.children[index].first.index() == 0) {
        log_w.log(5, "node is not a group\n");
        return;
    }

    auto* group =
      clusters.try_to_get(std::get<cluster_id>(top.children[index].first));
    if (!group) {
        log_w.log(5, "group does not exist\n");
        return;
    }

    const auto group_id = clusters.get_id(*group);
    top.pop(index);

    for (size_t i = 0, e = group->children.size(); i != e; ++i) {
        if (group->children[i].index() == 0) {
            const auto id = std::get<model_id>(group->children[i]);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                parent(id, undefined<cluster_id>());
                top.emplace_back(group->children[i]);
            }
        } else {
            auto id = std::get<cluster_id>(group->children[i]);
            if (auto* gp = clusters.try_to_get(id); gp) {
                parent(id, undefined<cluster_id>());
                top.emplace_back(group->children[i]);
            }
        }
    }

    clusters.free(*group);
    parent(group_id, undefined<cluster_id>());
}

void
editor::free_group(cluster& group) noexcept
{
    const auto group_id = clusters.get_id(group);

    for (const auto child : group.children) {
        if (child.index() == 0) {
            auto id = std::get<model_id>(child);
            models_mapper[get_index(id)] = undefined<cluster_id>();
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                log_w.log(7, "delete model %s\n", mdl->name.c_str());
                sim.deallocate(id);
            }
        } else {
            auto id = std::get<cluster_id>(child);
            clusters_mapper[get_index(id)] = undefined<cluster_id>();
            if (auto* gp = clusters.try_to_get(id); gp) {
                log_w.log(7, "delete group %s\n", gp->name.c_str());
                free_group(*gp);
            }
        }
    }

    parent(group_id, undefined<cluster_id>());
    clusters.free(group);
}

void
editor::free_children(const ImVector<int>& nodes) noexcept
{
    for (int i = 0, e = nodes.size(); i != e; ++i) {
        const auto index = top.get_index(nodes[i]);
        if (index == not_found)
            continue;

        const auto child = top.children[index];

        if (child.first.index() == 0) {
            const auto id = std::get<model_id>(child.first);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                models_mapper[get_index(id)] = undefined<cluster_id>();
                log_w.log(7, "delete %s\n", mdl->name.c_str());
                parent(id, undefined<cluster_id>());
                sim.deallocate(id);
            }
        } else {
            const auto id = std::get<cluster_id>(child.first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                clusters_mapper[get_index(id)] = undefined<cluster_id>();
                log_w.log(7, "delete group %s\n", gp->name.c_str());
                free_group(*gp);
            }
        }

        top.pop(index);
    }
}

struct copier
{
    struct copy_model
    {
        copy_model() = default;

        copy_model(const model_id src_, const model_id dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        model_id src, dst;
    };

    struct copy_cluster
    {
        copy_cluster() = default;

        copy_cluster(const cluster_id src_, const cluster_id dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        cluster_id src, dst;
    };

    struct copy_input_port
    {
        copy_input_port() = default;

        copy_input_port(const input_port_id src_,
                        const input_port_id dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        input_port_id src, dst;
    };

    struct copy_output_port
    {
        copy_output_port() = default;

        copy_output_port(const output_port_id src_,
                         const output_port_id dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        output_port_id src, dst;
    };

    std::vector<copy_model> c_models;
    std::vector<copy_cluster> c_clusters;
    std::vector<copy_input_port> c_input_ports;
    std::vector<copy_output_port> c_output_ports;

    void sort() noexcept
    {
        std::sort(std::begin(c_models),
                  std::end(c_models),
                  [](const auto left, const auto right) {
                      return static_cast<u64>(left.src) <
                             static_cast<u64>(right.src);
                  });

        std::sort(std::begin(c_clusters),
                  std::end(c_clusters),
                  [](const auto left, const auto right) {
                      return static_cast<u64>(left.src) <
                             static_cast<u64>(right.src);
                  });

        std::sort(std::begin(c_input_ports),
                  std::end(c_input_ports),
                  [](const auto left, const auto right) {
                      return static_cast<u64>(left.src) <
                             static_cast<u64>(right.src);
                  });

        std::sort(std::begin(c_output_ports),
                  std::end(c_output_ports),
                  [](const auto left, const auto right) {
                      return static_cast<u64>(left.src) <
                             static_cast<u64>(right.src);
                  });
    }

    template<typename Container, typename T>
    static int get(const Container& c, const T src) noexcept
    {
        const typename Container::value_type val = { src, undefined<T>() };

        auto it = std::lower_bound(std::begin(c),
                                   std::end(c),
                                   val,
                                   [](const auto& left, const auto& right) {
                                       return static_cast<u64>(left.src) <
                                              static_cast<u64>(right.src);
                                   });

        return (it != std::end(c) &&
                static_cast<u64>(src) == static_cast<u64>(it->src))
                 ? static_cast<int>(std::distance(std::begin(c), it))
                 : not_found;
    }

    int get_model(const model_id src) const noexcept
    {
        return get(c_models, src);
    }

    int get_cluster(const cluster_id src) const noexcept
    {
        return get(c_clusters, src);
    }

    int get_input_port(const input_port_id src) const noexcept
    {
        return get(c_input_ports, src);
    }

    int get_output_port(const output_port_id src) const noexcept
    {
        return get(c_output_ports, src);
    }

    status copy(editor& ed,
                const size_t models_to_merge_with_top,
                const size_t clusters_to_merge_with_top)
    {
        auto& sim = ed.sim;

        for (size_t i = 0, e = std::size(c_models); i != e; ++i) {
            auto* mdl = sim.models.try_to_get(c_models[i].src);
            auto* mdl_id_dst = &c_models[i].dst;

            auto ret = sim.dispatch(
              mdl->type,
              [this, &sim, mdl, &mdl_id_dst]<typename DynamicsM>(
                DynamicsM& dynamics_models) -> status {
                  using Dynamics = typename DynamicsM::value_type;

                  irt_return_if_fail(dynamics_models.can_alloc(1),
                                     status::dynamics_not_enough_memory);

                  auto* dyn_ptr = dynamics_models.try_to_get(mdl->id);
                  irt_return_if_fail(dyn_ptr, status::dynamics_unknown_id);

                  auto& new_dyn = dynamics_models.alloc(*dyn_ptr);
                  auto new_dyn_id = dynamics_models.get_id(new_dyn);

                  if constexpr (is_detected_v<has_input_port_t, Dynamics>)
                      std::fill_n(new_dyn.x,
                                  std::size(new_dyn.x),
                                  static_cast<input_port_id>(0));

                  if constexpr (is_detected_v<has_output_port_t, Dynamics>)
                      std::fill_n(new_dyn.y,
                                  std::size(new_dyn.y),
                                  static_cast<output_port_id>(0));

                  irt_return_if_bad(
                    sim.alloc(new_dyn, new_dyn_id, mdl->name.c_str()));

                  *mdl_id_dst = new_dyn.id;

                  if constexpr (is_detected_v<has_input_port_t, Dynamics>)
                      for (size_t j = 0, ej = std::size(new_dyn.x); j != ej;
                           ++j)
                          this->c_input_ports.emplace_back(dyn_ptr->x[j],
                                                           new_dyn.x[j]);

                  if constexpr (is_detected_v<has_output_port_t, Dynamics>)
                      for (size_t j = 0, ej = std::size(new_dyn.y); j != ej;
                           ++j)
                          this->c_output_ports.emplace_back(dyn_ptr->y[j],
                                                            new_dyn.y[j]);

                  return status::success;
              });

            irt_return_if_bad(ret);
        }

        for (size_t i = 0, e = std::size(c_clusters); i != e; ++i) {
            auto* gp_src = ed.clusters.try_to_get(c_clusters[i].src);
            auto& gp_dst = ed.clusters.alloc(*gp_src);
            c_clusters[i].dst = ed.clusters.get_id(gp_dst);
        }

        sort();

        for (size_t i = 0, e = std::size(c_clusters); i != e; ++i) {
            auto* gp_src = ed.clusters.try_to_get(c_clusters[i].src);
            auto* gp_dst = ed.clusters.try_to_get(c_clusters[i].dst);

            for (size_t j = 0, ej = gp_src->children.size(); j != ej; ++j) {
                if (gp_src->children[j].index() == 0) {
                    const auto id = std::get<model_id>(gp_src->children[j]);
                    const auto index = get_model(id);
                    gp_dst->children[j] = c_models[index].dst;
                } else {
                    const auto id = std::get<cluster_id>(gp_src->children[j]);
                    const auto index = get_cluster(id);
                    gp_dst->children[j] = c_clusters[index].dst;
                }
            }

            for (size_t j = 0, ej = gp_src->input_ports.size(); j != ej; ++j) {
                const auto index = get_input_port(gp_src->input_ports[j]);
                gp_dst->input_ports[j] = c_input_ports[index].dst;
            }

            for (size_t j = 0, ej = gp_src->output_ports.size(); j != ej; ++j) {
                const auto index = get_output_port(gp_src->output_ports[j]);
                gp_dst->output_ports[j] = c_output_ports[index].dst;
            }
        }

        for (size_t i = 0, e = std::size(c_input_ports); i != e; ++i) {
            const auto* src = sim.input_ports.try_to_get(c_input_ports[i].src);
            auto* dst = sim.input_ports.try_to_get(c_input_ports[i].dst);

            assert(dst->connections.empty());

            for (const auto port : src->connections) {
                const auto index = get_output_port(port);
                dst->connections.emplace_front(c_output_ports[index].dst);
            }
        }

        for (size_t i = 0, e = std::size(c_output_ports); i != e; ++i) {
            const auto* src =
              sim.output_ports.try_to_get(c_output_ports[i].src);
            auto* dst = sim.output_ports.try_to_get(c_output_ports[i].dst);

            assert(dst->connections.empty());

            for (const auto port : src->connections) {
                const auto index = get_input_port(port);
                dst->connections.emplace_front(c_input_ports[index].dst);
            }
        }

        for (size_t i = 0, e = std::size(c_models); i != e; ++i) {
            const auto parent_src = ed.parent(c_models[i].src);
            const auto index = get_cluster(parent_src);

            if (index == not_found)
                ed.parent(c_models[i].dst, parent_src);
            else
                ed.parent(c_models[i].dst, c_clusters[index].dst);
        }

        for (size_t i = 0, e = std::size(c_clusters); i != e; ++i) {
            const auto parent_src = ed.parent(c_clusters[i].src);
            const auto index = get_cluster(parent_src);

            if (index == not_found)
                ed.parent(c_models[i].dst, parent_src);
            else
                ed.parent(c_models[i].dst, c_clusters[index].dst);
        }

        /* Finally, merge clusters and models from user selection into the
           editor.top structure. */

        for (size_t i = 0; i != models_to_merge_with_top; ++i) {
            ed.top.emplace_back(c_models[i].dst);
            ed.parent(c_models[i].dst, undefined<cluster_id>());
        }

        for (size_t i = 0; i != clusters_to_merge_with_top; ++i) {
            ed.top.emplace_back(c_clusters[i].dst);
            ed.parent(c_clusters[i].dst, undefined<cluster_id>());
        }

        return status::success;
    }
};

static void
compute_connection_distance(output_port& port, editor& ed, const float k)
{
    for_each(ed.sim.input_ports,
             port.connections,
             [&](const auto& i_port, const auto /*i_id*/) {
                 const auto v = ed.get_top_group_ref(port.model);
                 const auto u = ed.get_top_group_ref(i_port.model);

                 const float dx = ed.positions[v].x - ed.positions[u].x;
                 const float dy = ed.positions[v].y - ed.positions[u].y;
                 if (dx && dy) {
                     const float d2 = dx * dx / dy * dy;
                     const float coeff = std::sqrt(d2) / k;

                     ed.displacements[v].x -= dx * coeff;
                     ed.displacements[v].y -= dy * coeff;
                     ed.displacements[u].x += dx * coeff;
                     ed.displacements[u].y += dy * coeff;
                 }
             });
}

void
editor::compute_grid_layout() noexcept
{
    const auto size = length(top.children);
    const auto tmp = std::sqrt(size);
    const auto column = static_cast<int>(tmp);
    auto line = column;
    auto remaining = size - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const auto panning = imnodes::EditorContextGetPanning();
    auto new_pos = panning;

    int elem = 0;

    for (int i = 0; i < column; ++i) {
        new_pos.y = panning.y + static_cast<float>(i) * grid_layout_y_distance;
        for (int j = 0; j < line; ++j) {
            new_pos.x =
              panning.x + static_cast<float>(j) * grid_layout_x_distance;
            imnodes::SetNodeScreenSpacePos(top.children[elem].second, new_pos);
            positions[elem].x = new_pos.x;
            positions[elem].y = new_pos.y;
            ++elem;
        }
    }

    new_pos.x = panning.x;
    new_pos.y = panning.y + static_cast<float>(column) * grid_layout_y_distance;
    for (int j = 0; j < remaining; ++j) {
        new_pos.x = panning.x + static_cast<float>(j) * grid_layout_x_distance;
        imnodes::SetNodeScreenSpacePos(top.children[elem].second, new_pos);
        positions[elem].x = new_pos.x;
        positions[elem].y = new_pos.y;
        ++elem;
    }
}

void
editor::compute_automatic_layout() noexcept
{
    /* See. Graph drawing by Forced-directed Placement by Thomas M. J.
       Fruchterman and Edward M. Reingold in Software--Pratice and
       Experience, Vol. 21(1 1), 1129-1164 (november 1991).
       */

    const auto size = length(top.children);
    const auto tmp = std::sqrt(size);
    const auto column = static_cast<int>(tmp);
    auto line = column;
    auto remaining = size - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const float W = static_cast<float>(column) * automatic_layout_x_distance;
    const float L = line + (remaining > 0) ? automatic_layout_y_distance : 0.f;
    const float area = W * L;
    const float k_square = area / static_cast<float>(top.children.size());
    const float k = std::sqrt(k_square);

    // float t = 1.f - static_cast<float>(iteration) /
    //                   static_cast<float>(automatic_layout_iteration_limit);
    // t *= t;

    float t = 1.f - 1.f / static_cast<float>(automatic_layout_iteration_limit);

    for (int iteration = 0; iteration < automatic_layout_iteration_limit;
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

        for (size_t i = 0, e = top.children.size(); i != e; ++i) {
            if (top.children[i].first.index() == 0) {
                const auto id = std::get<model_id>(top.children[i].first);
                if (const auto* mdl = sim.models.try_to_get(id); mdl) {
                    sim.for_all_output_port(
                      *mdl,
                      [this, k](output_port& port, output_port_id /*id*/) {
                          compute_connection_distance(port, *this, k);
                      });
                }
            } else {
                const auto id = std::get<cluster_id>(top.children[i].first);
                if (auto* gp = clusters.try_to_get(id); gp) {
                    for_each(
                      sim.output_ports,
                      gp->output_ports,
                      [this, k](output_port& port, output_port_id /*id*/) {
                          compute_connection_distance(port, *this, k);
                      });
                }
            }
        }

        auto sum = 0.f;
        for (int i_v = 0; i_v < size; ++i_v) {
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

            imnodes::SetNodeGridSpacePos(top.children[v].second, positions[v]);
        }
    }
}

status
editor::copy(const ImVector<int>& nodes) noexcept
{
    copier cp;

    std::vector<cluster_id> copy_stack;

    for (int i = 0, e = nodes.size(); i != e; ++i) {
        const auto index = top.get_index(nodes[i]);
        if (index == not_found)
            continue;

        const auto child = top.children[index];

        if (child.first.index() == 0) {
            const auto id = std::get<model_id>(child.first);
            if (auto* mdl = sim.models.try_to_get(id); mdl)
                cp.c_models.emplace_back(id, undefined<model_id>());
        } else {
            const auto id = std::get<cluster_id>(child.first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                cp.c_clusters.emplace_back(id, undefined<cluster_id>());
                copy_stack.emplace_back(id);
            }
        }
    }

    const auto models_to_merge_with_top = std::size(cp.c_models);
    const auto clusters_to_merge_with_top = std::size(cp.c_clusters);

    while (!copy_stack.empty()) {
        const auto gp_id = copy_stack.back();
        copy_stack.pop_back();

        if (auto* gp = clusters.try_to_get(gp_id); gp) {
            for (const auto child : gp->children) {
                if (child.index() == 0) {
                    const auto id = std::get<model_id>(child);
                    if (auto* mdl = sim.models.try_to_get(id); mdl)
                        cp.c_models.emplace_back(id, undefined<model_id>());
                } else {
                    const auto id = std::get<cluster_id>(child);
                    if (auto* gp = clusters.try_to_get(id); gp) {
                        cp.c_clusters.emplace_back(id, undefined<cluster_id>());
                        copy_stack.emplace_back(id);
                    }
                }
            }
        }
    }

    return cp.copy(*this, models_to_merge_with_top, clusters_to_merge_with_top);
}

status
editor::initialize(u32 id) noexcept
{
    if (is_bad(sim.init(static_cast<unsigned>(kernel_model_cache),
                        static_cast<unsigned>(kernel_message_cache))) ||
        is_bad(observation_outputs.init(sim.models.capacity())) ||
        is_bad(observation_types.init(sim.models.capacity())) ||
        is_bad(clusters.init(sim.models.capacity())) ||
        is_bad(models_mapper.init(sim.models.capacity())) ||
        is_bad(clusters_mapper.init(sim.models.capacity())) ||
        is_bad(top.init(static_cast<unsigned>(gui_node_cache))))
        return status::gui_not_enough_memory;

    positions.resize(sim.models.capacity() + clusters.capacity());
    displacements.resize(sim.models.capacity() + clusters.capacity(),
                         ImVec2{ 0.f, 0.f });

    format(name, "Editor {}", id);

    initialized = true;

    return status::success;
}

status
editor::add_lotka_volterra() noexcept
{
    if (!sim.adder_2_models.can_alloc(2) || !sim.mult_2_models.can_alloc(2) ||
        !sim.integrator_models.can_alloc(2) ||
        !sim.quantifier_models.can_alloc(2) || !sim.models.can_alloc(10))
        return status::simulation_not_enough_model;

    auto& sum_a = sim.adder_2_models.alloc();
    auto& sum_b = sim.adder_2_models.alloc();
    auto& product = sim.mult_2_models.alloc();
    auto& integrator_a = sim.integrator_models.alloc();
    auto& integrator_b = sim.integrator_models.alloc();
    auto& quantifier_a = sim.quantifier_models.alloc();
    auto& quantifier_b = sim.quantifier_models.alloc();

    integrator_a.default_current_value = 18.0;

    quantifier_a.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_a.default_zero_init_offset = true;
    quantifier_a.default_step_size = 0.01;
    quantifier_a.default_past_length = 3;

    integrator_b.default_current_value = 7.0;

    quantifier_b.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_b.default_zero_init_offset = true;
    quantifier_b.default_step_size = 0.01;
    quantifier_b.default_past_length = 3;

    product.default_input_coeffs[0] = 1.0;
    product.default_input_coeffs[1] = 1.0;
    sum_a.default_input_coeffs[0] = 2.0;
    sum_a.default_input_coeffs[1] = -0.4;
    sum_b.default_input_coeffs[0] = -1.0;
    sum_b.default_input_coeffs[1] = 0.1;

    irt_return_if_bad(
      sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
    irt_return_if_bad(
      sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
    irt_return_if_bad(
      sim.alloc(product, sim.mult_2_models.get_id(product), "prod"));
    irt_return_if_bad(sim.alloc(
      integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
    irt_return_if_bad(sim.alloc(
      integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
    irt_return_if_bad(sim.alloc(
      quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
    irt_return_if_bad(sim.alloc(
      quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));

    irt_return_if_bad(sim.connect(sum_a.y[0], integrator_a.x[1]));
    irt_return_if_bad(sim.connect(sum_b.y[0], integrator_b.x[1]));

    irt_return_if_bad(sim.connect(integrator_a.y[0], sum_a.x[0]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], sum_b.x[0]));

    irt_return_if_bad(sim.connect(integrator_a.y[0], product.x[0]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], product.x[1]));

    irt_return_if_bad(sim.connect(product.y[0], sum_a.x[1]));
    irt_return_if_bad(sim.connect(product.y[0], sum_b.x[1]));

    irt_return_if_bad(sim.connect(quantifier_a.y[0], integrator_a.x[0]));
    irt_return_if_bad(sim.connect(quantifier_b.y[0], integrator_b.x[0]));
    irt_return_if_bad(sim.connect(integrator_a.y[0], quantifier_a.x[0]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], quantifier_b.x[0]));

    top.emplace_back(sum_a.id);
    top.emplace_back(sum_b.id);
    top.emplace_back(product.id);
    top.emplace_back(integrator_a.id);
    top.emplace_back(integrator_b.id);
    top.emplace_back(quantifier_a.id);
    top.emplace_back(quantifier_b.id);

    parent(sum_a.id, undefined<cluster_id>());
    parent(sum_b.id, undefined<cluster_id>());
    parent(product.id, undefined<cluster_id>());
    parent(integrator_a.id, undefined<cluster_id>());
    parent(integrator_b.id, undefined<cluster_id>());
    parent(quantifier_a.id, undefined<cluster_id>());
    parent(quantifier_b.id, undefined<cluster_id>());

    return status::success;
}

status
editor::add_izhikevitch() noexcept
{
    if (!sim.constant_models.can_alloc(3) || !sim.adder_2_models.can_alloc(3) ||
        !sim.adder_4_models.can_alloc(1) || !sim.mult_2_models.can_alloc(1) ||
        !sim.integrator_models.can_alloc(2) ||
        !sim.quantifier_models.can_alloc(2) || !sim.cross_models.can_alloc(2) ||
        !sim.models.can_alloc(14))
        return status::simulation_not_enough_model;

    auto& constant = sim.constant_models.alloc();
    auto& constant2 = sim.constant_models.alloc();
    auto& constant3 = sim.constant_models.alloc();
    auto& sum_a = sim.adder_2_models.alloc();
    auto& sum_b = sim.adder_2_models.alloc();
    auto& sum_c = sim.adder_4_models.alloc();
    auto& sum_d = sim.adder_2_models.alloc();
    auto& product = sim.mult_2_models.alloc();
    auto& integrator_a = sim.integrator_models.alloc();
    auto& integrator_b = sim.integrator_models.alloc();
    auto& quantifier_a = sim.quantifier_models.alloc();
    auto& quantifier_b = sim.quantifier_models.alloc();
    auto& cross = sim.cross_models.alloc();
    auto& cross2 = sim.cross_models.alloc();

    double a = 0.2;
    double b = 2.0;
    double c = -56.0;
    double d = -16.0;
    double I = -99.0;
    double vt = 30.0;

    constant.default_value = 1.0;
    constant2.default_value = c;
    constant3.default_value = I;

    cross.default_threshold = vt;
    cross2.default_threshold = vt;

    integrator_a.default_current_value = 0.0;

    quantifier_a.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_a.default_zero_init_offset = true;
    quantifier_a.default_step_size = 0.01;
    quantifier_a.default_past_length = 3;

    integrator_b.default_current_value = 0.0;

    quantifier_b.default_adapt_state = irt::quantifier::adapt_state::possible;
    quantifier_b.default_zero_init_offset = true;
    quantifier_b.default_step_size = 0.01;
    quantifier_b.default_past_length = 3;

    product.default_input_coeffs[0] = 1.0;
    product.default_input_coeffs[1] = 1.0;

    sum_a.default_input_coeffs[0] = 1.0;
    sum_a.default_input_coeffs[1] = -1.0;
    sum_b.default_input_coeffs[0] = -a;
    sum_b.default_input_coeffs[1] = a * b;
    sum_c.default_input_coeffs[0] = 0.04;
    sum_c.default_input_coeffs[1] = 5.0;
    sum_c.default_input_coeffs[2] = 140.0;
    sum_c.default_input_coeffs[3] = 1.0;
    sum_d.default_input_coeffs[0] = 1.0;
    sum_d.default_input_coeffs[1] = d;

    irt_return_if_bad(
      sim.alloc(constant3, sim.constant_models.get_id(constant3), "tfun"));
    irt_return_if_bad(
      sim.alloc(constant, sim.constant_models.get_id(constant), "1.0"));
    irt_return_if_bad(
      sim.alloc(constant2, sim.constant_models.get_id(constant2), "-56.0"));

    irt_return_if_bad(
      sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
    irt_return_if_bad(
      sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
    irt_return_if_bad(
      sim.alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c"));
    irt_return_if_bad(
      sim.alloc(sum_d, sim.adder_2_models.get_id(sum_d), "sum_d"));

    irt_return_if_bad(
      sim.alloc(product, sim.mult_2_models.get_id(product), "prod"));
    irt_return_if_bad(sim.alloc(
      integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
    irt_return_if_bad(sim.alloc(
      integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
    irt_return_if_bad(sim.alloc(
      quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
    irt_return_if_bad(sim.alloc(
      quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));
    irt_return_if_bad(
      sim.alloc(cross, sim.cross_models.get_id(cross), "cross"));
    irt_return_if_bad(
      sim.alloc(cross2, sim.cross_models.get_id(cross2), "cross2"));

    irt_return_if_bad(sim.connect(integrator_a.y[0], cross.x[0]));
    irt_return_if_bad(sim.connect(constant2.y[0], cross.x[1]));
    irt_return_if_bad(sim.connect(integrator_a.y[0], cross.x[2]));

    irt_return_if_bad(sim.connect(cross.y[0], quantifier_a.x[0]));
    irt_return_if_bad(sim.connect(cross.y[0], product.x[0]));
    irt_return_if_bad(sim.connect(cross.y[0], product.x[1]));
    irt_return_if_bad(sim.connect(product.y[0], sum_c.x[0]));
    irt_return_if_bad(sim.connect(cross.y[0], sum_c.x[1]));
    irt_return_if_bad(sim.connect(cross.y[0], sum_b.x[1]));

    irt_return_if_bad(sim.connect(constant.y[0], sum_c.x[2]));
    irt_return_if_bad(sim.connect(constant3.y[0], sum_c.x[3]));

    irt_return_if_bad(sim.connect(sum_c.y[0], sum_a.x[0]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], sum_a.x[1]));
    irt_return_if_bad(sim.connect(cross2.y[0], sum_a.x[1]));
    irt_return_if_bad(sim.connect(sum_a.y[0], integrator_a.x[1]));
    irt_return_if_bad(sim.connect(cross.y[0], integrator_a.x[2]));
    irt_return_if_bad(sim.connect(quantifier_a.y[0], integrator_a.x[0]));

    irt_return_if_bad(sim.connect(cross2.y[0], quantifier_b.x[0]));
    irt_return_if_bad(sim.connect(cross2.y[0], sum_b.x[0]));
    irt_return_if_bad(sim.connect(quantifier_b.y[0], integrator_b.x[0]));
    irt_return_if_bad(sim.connect(sum_b.y[0], integrator_b.x[1]));

    irt_return_if_bad(sim.connect(cross2.y[0], integrator_b.x[2]));
    irt_return_if_bad(sim.connect(integrator_a.y[0], cross2.x[0]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], cross2.x[2]));
    irt_return_if_bad(sim.connect(sum_d.y[0], cross2.x[1]));
    irt_return_if_bad(sim.connect(integrator_b.y[0], sum_d.x[0]));
    irt_return_if_bad(sim.connect(constant.y[0], sum_d.x[1]));

    top.emplace_back(constant.id);
    top.emplace_back(constant2.id);
    top.emplace_back(constant3.id);
    top.emplace_back(sum_a.id);
    top.emplace_back(sum_b.id);
    top.emplace_back(sum_c.id);
    top.emplace_back(sum_d.id);
    top.emplace_back(product.id);
    top.emplace_back(integrator_a.id);
    top.emplace_back(integrator_b.id);
    top.emplace_back(quantifier_a.id);
    top.emplace_back(quantifier_b.id);
    top.emplace_back(cross.id);
    top.emplace_back(cross2.id);

    parent(constant.id, undefined<cluster_id>());
    parent(constant2.id, undefined<cluster_id>());
    parent(constant3.id, undefined<cluster_id>());
    parent(sum_a.id, undefined<cluster_id>());
    parent(sum_b.id, undefined<cluster_id>());
    parent(sum_c.id, undefined<cluster_id>());
    parent(sum_d.id, undefined<cluster_id>());
    parent(product.id, undefined<cluster_id>());
    parent(integrator_a.id, undefined<cluster_id>());
    parent(integrator_b.id, undefined<cluster_id>());
    parent(quantifier_a.id, undefined<cluster_id>());
    parent(quantifier_b.id, undefined<cluster_id>());
    parent(cross.id, undefined<cluster_id>());
    parent(cross2.id, undefined<cluster_id>());

    return status::success;
}

static int
show_connection(output_port& port,
                output_port_id id,
                data_array<input_port, input_port_id>& input_ports,
                int connection_id)
{
    for_each(input_ports,
             port.connections,
             [&connection_id, id](const auto& /*i_port*/, const auto i_id) {
                 imnodes::Link(
                   connection_id++, editor::get_out(id), editor::get_in(i_id));
             });

    return connection_id;
}

void
editor::show_connections() noexcept
{
    int connection_id = 0;

    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].first.index() == 0) {
            const auto id = std::get<model_id>(top.children[i].first);
            if (const auto* mdl = sim.models.try_to_get(id); mdl) {
                sim.for_all_output_port(
                  *mdl,
                  [this, &connection_id](output_port& port,
                                         output_port_id /*id*/) {
                      connection_id =
                        show_connection(port,
                                        this->sim.output_ports.get_id(port),
                                        this->sim.input_ports,
                                        connection_id);
                  });
            }
        } else {
            const auto id = std::get<cluster_id>(top.children[i].first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                for_each(sim.output_ports,
                         gp->output_ports,
                         [this, &connection_id](output_port& port,
                                                output_port_id /*id*/) {
                             connection_id = show_connection(
                               port,
                               this->sim.output_ports.get_id(port),
                               this->sim.input_ports,
                               connection_id);
                         });
            }
        }
    }
}

void
editor::show_model_cluster(cluster& mdl) noexcept
{
    {
        auto it = mdl.input_ports.begin();
        auto end = mdl.input_ports.end();

        while (it != end) {
            if (auto* port = sim.input_ports.try_to_get(*it); port) {
                imnodes::BeginInputAttribute(get_in(*it));
                ImGui::TextUnformatted(port->name.c_str());
                imnodes::EndAttribute();
                ++it;
            } else {
                it = mdl.input_ports.erase(it);
            }
        }
    }

    {
        auto it = mdl.output_ports.begin();
        auto end = mdl.output_ports.end();

        while (it != end) {
            if (auto* port = sim.output_ports.try_to_get(*it); port) {
                imnodes::BeginOutputAttribute(get_out(*it));
                ImGui::TextUnformatted(port->name.c_str());
                imnodes::EndAttribute();
                ++it;
            } else {
                it = mdl.output_ports.erase(it);
            }
        }
    }
}

static const char* str_error[] = { "Error" };
static const char* str_empty[] = { "" };
static const char* str_integrator[] = { "x-dot", "reset" };
static const char* str_adaptative_integrator[] = { "quanta", "x-dot", "reset" };
static const char* str_in_1[] = { "in" };
static const char* str_in_2[] = { "in-1", "in-2" };
static const char* str_in_3[] = { "in-1", "in-2", "in-3" };
static const char* str_in_4[] = { "in-1", "in-2", "in-3", "in-4" };
static const char* str_value_if_else[] = { "value", "if", "else" };
static const char* str_in_2_nb_2[] = { "in-1", "in-2", "nb-1", "nb-2" };
static const char* str_out_1[] = { "out" };
static const char* str_out_2[] = { "out-1", "out-2" };

template<typename Dynamics>
static constexpr const char**
get_input_port_names()
{
    if constexpr (std::is_same_v<Dynamics, none>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss2_integrator>)
        return str_integrator;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, mult_2>)
        return str_in_2;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, mult_3>)
        return str_in_3;

    if constexpr (std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_4>)
        return str_in_4;

    if constexpr (std::is_same_v<Dynamics, integrator>)
        return str_adaptative_integrator;

    if constexpr (std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, counter>)
        return str_in_1;

    if constexpr (std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, flow>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross> ||
                  std::is_same_v<Dynamics, cross>)
        return str_value_if_else;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_in_2_nb_2;

    return str_error;
}

template<typename Dynamics>
static constexpr const char**
get_output_port_names()
{
    if constexpr (std::is_same_v<Dynamics, none>)
        return str_empty;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator> ||
                  std::is_same_v<Dynamics, qss1_multiplier> ||
                  std::is_same_v<Dynamics, qss1_sum_2> ||
                  std::is_same_v<Dynamics, qss1_sum_3> ||
                  std::is_same_v<Dynamics, qss1_sum_4> ||
                  std::is_same_v<Dynamics, qss1_wsum_2> ||
                  std::is_same_v<Dynamics, qss1_wsum_3> ||
                  std::is_same_v<Dynamics, qss1_wsum_4> ||
                  std::is_same_v<Dynamics, qss2_integrator> ||
                  std::is_same_v<Dynamics, qss2_multiplier> ||
                  std::is_same_v<Dynamics, qss2_sum_2> ||
                  std::is_same_v<Dynamics, qss2_sum_3> ||
                  std::is_same_v<Dynamics, qss2_sum_4> ||
                  std::is_same_v<Dynamics, qss2_wsum_2> ||
                  std::is_same_v<Dynamics, qss2_wsum_3> ||
                  std::is_same_v<Dynamics, qss2_wsum_4> ||
                  std::is_same_v<Dynamics, integrator> ||
                  std::is_same_v<Dynamics, quantifier> ||
                  std::is_same_v<Dynamics, adder_2> ||
                  std::is_same_v<Dynamics, adder_3> ||
                  std::is_same_v<Dynamics, adder_4> ||
                  std::is_same_v<Dynamics, mult_2> ||
                  std::is_same_v<Dynamics, mult_3> ||
                  std::is_same_v<Dynamics, mult_4> ||
                  std::is_same_v<Dynamics, counter> ||
                  std::is_same_v<Dynamics, generator> ||
                  std::is_same_v<Dynamics, constant> ||
                  std::is_same_v<Dynamics, time_func> ||
                  std::is_same_v<Dynamics, flow>)
        return str_out_1;

    if constexpr (std::is_same_v<Dynamics, cross> ||
                  std::is_same_v<Dynamics, qss1_cross> ||
                  std::is_same_v<Dynamics, qss2_cross>)
        return str_out_2;

    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return str_empty;

    return str_error;
}

template<typename Dynamics>
static void
add_input_attribute(editor& ed, const Dynamics& dyn) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
            imnodes::BeginInputAttribute(ed.get_in(dyn.x[i]));
            ImGui::TextUnformatted(names[i]);
            imnodes::EndAttribute();
        }
    }
}

template<typename Dynamics>
static void
add_output_attribute(editor& ed, const Dynamics& dyn) noexcept
{
    if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
        const auto** names = get_output_port_names<Dynamics>();

        for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
            imnodes::BeginOutputAttribute(ed.get_out(dyn.y[i]));
            ImGui::TextUnformatted(names[i]);
            imnodes::EndAttribute();
        }
    }
}

static void
show_dynamics_values(const none& /*dyn*/)
{}

static void
show_dynamics_values(const qss1_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(const qss2_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(const qss3_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(const qss1_sum_2& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(const qss1_sum_3& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
}

static void
show_dynamics_values(const qss1_sum_4& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
    ImGui::Text("%.3f", dyn.values[3]);
}

static void
show_dynamics_values(const qss1_multiplier& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(const qss1_wsum_2& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(const qss1_wsum_3& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
}

static void
show_dynamics_values(const qss1_wsum_4& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
    ImGui::Text("%.3f", dyn.values[3]);
}

static void
show_dynamics_values(const qss2_sum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss2_sum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(const qss2_sum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(const qss2_multiplier& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss2_wsum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss2_wsum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(const qss2_wsum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(const qss3_sum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss3_sum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(const qss3_sum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(const qss3_multiplier& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss3_wsum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(const qss3_wsum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(const qss3_wsum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(const integrator& dyn)
{
    ImGui::Text("value %.3f", dyn.current_value);
}

static void
show_dynamics_values(const quantifier& dyn)
{
    ImGui::Text("up threshold %.3f", dyn.m_upthreshold);
    ImGui::Text("down threshold %.3f", dyn.m_downthreshold);
}

static void
show_dynamics_values(const adder_2& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(const adder_3& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(const adder_4& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(const mult_2& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(const mult_3& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(const mult_4& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(const counter& dyn)
{
    ImGui::Text("number %ld", static_cast<long>(dyn.number));
}

static void
show_dynamics_values(const generator& dyn)
{
    ImGui::Text("next %.3f", dyn.sigma);
}

static void
show_dynamics_values(const constant& dyn)
{
    ImGui::Text("value %.3f", dyn.value);
}

static void
show_dynamics_values(const qss1_cross& dyn)
{
    ImGui::Text("value: %.3f", dyn.value[0]);
    ImGui::Text("if-value: %.3f", dyn.if_value);
    ImGui::Text("else-value: %.3f", dyn.else_value);
}

static void
show_dynamics_values(const qss2_cross& dyn)
{
    ImGui::Text("value: %.3f", dyn.value[0]);
    ImGui::Text("if-value: %.3f", dyn.if_value);
    ImGui::Text("else-value: %.3f", dyn.else_value);
}

static void
show_dynamics_values(const qss3_cross& dyn)
{
    ImGui::Text("value: %.3f", dyn.value[0]);
    ImGui::Text("if-value: %.3f", dyn.if_value);
    ImGui::Text("else-value: %.3f", dyn.else_value);
}

static void
show_dynamics_values(const cross& dyn)
{
    ImGui::Text("value: %.3f", dyn.value);
    ImGui::Text("if-value: %.3f", dyn.if_value);
    ImGui::Text("else-value: %.3f", dyn.else_value);
}

static void
show_dynamics_values(const accumulator_2& dyn)
{
    ImGui::Text("number %.3f", dyn.number);
    ImGui::Text("- 0: %.3f", dyn.numbers[0]);
    ImGui::Text("- 1: %.3f", dyn.numbers[1]);
}

static void
show_dynamics_values(const time_func& dyn)
{
    ImGui::Text("value %.3f", dyn.value);
}

static void
show_dynamics_inputs(none& /*dyn*/)
{}

static void
show_dynamics_values(const flow& dyn)
{
    ImGui::Text("value %.3f", dyn.value);
}

static void
show_dynamics_inputs(qss1_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(qss2_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(qss3_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(qss1_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(qss1_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(qss1_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(qss1_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(qss1_wsum_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(qss1_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(qss1_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(qss2_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(qss2_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(qss2_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(qss2_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(qss2_wsum_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(qss2_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(qss2_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(qss3_multiplier& /*dyn*/)
{}

static void
show_dynamics_inputs(qss3_sum_2& /*dyn*/)
{}

static void
show_dynamics_inputs(qss3_sum_3& /*dyn*/)
{}

static void
show_dynamics_inputs(qss3_sum_4& /*dyn*/)
{}

static void
show_dynamics_inputs(qss3_wsum_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(qss3_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(qss3_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_current_value);
    ImGui::InputDouble("reset", &dyn.default_reset_value);
}

static void
show_dynamics_inputs(quantifier& dyn)
{
    ImGui::InputDouble("quantum", &dyn.default_step_size);
    ImGui::SliderInt("archive length", &dyn.default_past_length, 3, 100);
}

static void
show_dynamics_inputs(adder_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(adder_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(adder_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(mult_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(mult_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(mult_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(counter& /*dyn*/)
{}

static void
show_dynamics_inputs(generator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_value);
    ImGui::InputDouble("period", &dyn.default_period);
    ImGui::InputDouble("offset", &dyn.default_offset);
}

static void
show_dynamics_inputs(constant& dyn)
{
    ImGui::InputDouble("value", &dyn.default_value);
}

static void
show_dynamics_inputs(qss1_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
}

static void
show_dynamics_inputs(qss2_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
}

static void
show_dynamics_inputs(qss3_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
}

static void
show_dynamics_inputs(cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
}

static void
show_dynamics_inputs(accumulator_2& /*dyn*/)
{}

static void
show_dynamics_inputs(flow& dyn)
{
    ImGui::InputDouble("value", &dyn.default_value);
}

static void
show_dynamics_inputs(time_func& dyn)
{
    const char* items[] = { "time", "square" };
    ImGui::PushItemWidth(120.0f);
    int item_current = dyn.default_f == &time_function ? 0 : 1;
    if (ImGui::Combo("function", &item_current, items, IM_ARRAYSIZE(items))) {
        dyn.default_f =
          item_current == 0 ? &time_function : square_time_function;
    }
    ImGui::PopItemWidth();
}

void
editor::show_model_dynamics(model& mdl) noexcept
{
    ImGui::PushItemWidth(100.0f);

    {
        const char* items[] = { "none", "multiplot", "file", "both" };
        int current_item = 0; /* Default show none */
        auto* obs = sim.observers.try_to_get(mdl.obs_id);

        if (obs)
            current_item =
              static_cast<int>(observation_types[get_index(mdl.obs_id)]);

        if (ImGui::Combo(
              "observation", &current_item, items, IM_ARRAYSIZE(items))) {
            if (current_item == 0) {
                if (obs) {
                    observation_types[get_index(mdl.obs_id)] =
                      observation_output::type::none;
                    sim.observers.free(*obs);
                    mdl.obs_id = static_cast<observer_id>(0);
                }
            } else {
                if (!obs) {
                    auto& o =
                      sim.observers.alloc(0.01, mdl.name.c_str(), nullptr);
                    sim.observe(mdl, o);
                }

                observation_types[get_index(mdl.obs_id)] =
                  static_cast<observation_output::type>(current_item);
            }
        }

        if (current_item == 1) {
            if (auto* o = sim.observers.try_to_get(mdl.obs_id); o) {
                float v = static_cast<float>(o->time_step);
                if (ImGui::InputFloat("freq.", &v, 0.001f, 0.1f, "%.3f", 0))
                    o->time_step = static_cast<double>(v);
            }
        }
    }

    ImGui::PopItemWidth();

    if (simulation_show_value &&
        match(st,
              simulation_status::success,
              simulation_status::running_once,
              simulation_status::running_once_need_join,
              simulation_status::running_step)) {

        sim.dispatch(mdl.type, [&](const auto& d_array) {
            const auto& dyn = d_array.get(mdl.id);
            add_input_attribute(*this, dyn);
            ImGui::PushItemWidth(120.0f);
            show_dynamics_values(dyn);
            ImGui::PopItemWidth();
            add_output_attribute(*this, dyn);
            return status::success;
        });
    } else {
        sim.dispatch(mdl.type, [&](auto& d_array) {
            auto& dyn = d_array.get(mdl.id);
            add_input_attribute(*this, dyn);
            ImGui::PushItemWidth(120.0f);
            show_dynamics_inputs(dyn);
            ImGui::PopItemWidth();
            add_output_attribute(*this, dyn);
            return status::success;
        });
    }
}

void
editor::show_top() noexcept
{
    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].first.index() == 0) {
            const auto id = std::get<model_id>(top.children[i].first);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                imnodes::PushColorStyle(
                  imnodes::ColorStyle_TitleBar,
                  ImGui::ColorConvertFloat4ToU32(gui_model_color));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarHovered,
                                        gui_hovered_model_color);
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarSelected,
                                        gui_selected_model_color);

                imnodes::BeginNode(top.children[i].second);
                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(mdl->name.c_str());
                ImGui::OpenPopupOnItemClick("Rename model", 1);

                bool is_rename = true;
                ImGui::SetNextWindowSize(ImVec2(250, 200), ImGuiCond_Always);
                if (ImGui::BeginPopupModal("Rename model", &is_rename)) {
                    ImGui::InputText(
                      "Name##edit-1", mdl->name.begin(), mdl->name.capacity());
                    if (ImGui::Button("Close"))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                imnodes::EndNodeTitleBar();
                show_model_dynamics(*mdl);
                imnodes::EndNode();

                imnodes::PopColorStyle();
                imnodes::PopColorStyle();
            }
        } else {
            const auto id = std::get<cluster_id>(top.children[i].first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                imnodes::PushColorStyle(
                  imnodes::ColorStyle_TitleBar,
                  ImGui::ColorConvertFloat4ToU32(gui_cluster_color));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarHovered,
                                        gui_hovered_cluster_color);
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarSelected,
                                        gui_selected_cluster_color);

                imnodes::BeginNode(top.children[i].second);
                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(gp->name.c_str());
                ImGui::OpenPopupOnItemClick("Rename group", 1);

                bool is_rename = true;
                ImGui::SetNextWindowSize(ImVec2(250, 200), ImGuiCond_Always);
                if (ImGui::BeginPopupModal("Rename group", &is_rename)) {
                    ImGui::InputText(
                      "Name##edit-2", gp->name.begin(), gp->name.capacity());
                    if (ImGui::Button("Close"))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                imnodes::EndNodeTitleBar();
                show_model_cluster(*gp);
                imnodes::EndNode();

                imnodes::PopColorStyle();
                imnodes::PopColorStyle();
            }
        }
    }
}

static const char* dynamics_type_names[] = {
    "none",        "qss1_integrator", "qss1_multiplier",
    "qss1_cross",  "qss1_sum_2",      "qss1_sum_3",
    "qss1_sum_4",  "qss1_wsum_2",     "qss1_wsum_3",
    "qss1_wsum_4", "qss2_integrator", "qss2_multiplier",
    "qss2_cross",  "qss2_sum_2",      "qss2_sum_3",
    "qss2_sum_4",  "qss2_wsum_2",     "qss2_wsum_3",
    "qss2_wsum_4", "integrator",      "quantifier",
    "adder_2",     "adder_3",         "adder_4",
    "mult_2",      "mult_3",          "mult_4",
    "counter",     "generator",       "constant",
    "cross",       "time_func",       "accumulator_2",
    "flow"
};

status
add_popup_menuitem(editor& ed, dynamics_type type, model_id* new_model)
{
    return ed.sim.dispatch(type, [&](auto& d_array) {
        if (ImGui::MenuItem(dynamics_type_names[static_cast<i8>(type)])) {
            if (!ed.sim.models.can_alloc(1) || !d_array.can_alloc(1))
                return status::data_array_not_enough_memory;

            auto& mdl = d_array.alloc();
            ed.sim.alloc(mdl,
                         d_array.get_id(mdl),
                         dynamics_type_names[static_cast<i8>(type)]);
            *new_model = mdl.id;
        }

        return status::success;
    });
}

bool
editor::show_editor() noexcept
{
    imnodes::EditorContextSet(context);
    static bool show_load_file_dialog = false;
    static bool show_save_file_dialog = false;

    ImGuiWindowFlags windows_flags = 0;
    windows_flags |= ImGuiWindowFlags_MenuBar;

    ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);
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
                    auto ret = w(sim);
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
            if (ImGui::MenuItem("Clear"))
                clear();
            if (ImGui::MenuItem("Grid Reorder"))
                compute_grid_layout();
            if (ImGui::MenuItem("Automatic Layout"))
                compute_automatic_layout();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Examples")) {
            if (ImGui::MenuItem("Insert Lotka Volterra model")) {
                if (auto ret = add_lotka_volterra(); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize a Lotka Volterra "
                              "model (%s)\n",
                              status_string(ret));
            }

            if (ImGui::MenuItem("Insert Izhikevitch model")) {
                if (auto ret = add_izhikevitch(); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize an Izhikevitch "
                              "model (%s)\n",
                              status_string(ret));
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (show_load_file_dialog) {
        auto size = ImGui::GetContentRegionMax();
        ImGui::SetNextWindowSize(ImVec2(size.x * 0.7f, size.y * 0.7f));
        ImGui::OpenPopup("Select file path to load");

        if (load_file_dialog(path)) {
            show_load_file_dialog = false;
            log_w.log(
              5, "Load file from %s\n", (const char*)path.u8string().c_str());
            if (auto is = std::ifstream(path); is.is_open()) {
                reader r(is);
                auto ret = r(sim);
                if (is_success(ret))
                    log_w.log(5, "success\n");
                else
                    log_w.log(4, "error writing\n");
            }
        }
    }

    if (show_save_file_dialog) {
        if (sim.models.size()) {
            ImGui::OpenPopup("Select file path to save");

            auto size = ImGui::GetContentRegionMax();
            ImGui::SetNextWindowSize(ImVec2(size.x * 0.7f, size.y * 0.7f));
            if (save_file_dialog(path)) {
                show_save_file_dialog = false;
                log_w.log(
                  5, "Save file to %s\n", (const char*)path.u8string().c_str());

                log_w.log(3,
                          "Write into file %s\n",
                          (const char*)path.u8string().c_str());
                if (auto os = std::ofstream(path); os.is_open()) {
                    writer w(os);
                    auto ret = w(sim);
                    if (is_success(ret))
                        log_w.log(5, "success\n");
                    else
                        log_w.log(4, "error writing\n");
                }
            }
        }
    }

    ImGui::Text("X -- delete selected nodes and/or connections / "
                "D -- duplicate selected nodes / "
                "G -- group model");

    imnodes::BeginNodeEditor();

    show_top();
    show_connections();

    imnodes::EndNodeEditor();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

    if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(1))
        ImGui::OpenPopup("Context menu");

    if (ImGui::BeginPopup("Context menu")) {
        model_id new_model = undefined<model_id>();
        ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        for (size_t i = 0, e = std::size(dynamics_type_names); i != e; ++i) {
            auto ret = add_popup_menuitem(
              *this, static_cast<dynamics_type>(i), &new_model);
            if (is_bad(ret))
                log_w.log(
                  5, "Fail to allocate new dynamics: %s\n", status_string(ret));
        }

        ImGui::EndPopup();

        if (new_model != undefined<model_id>()) {
            parent(new_model, undefined<cluster_id>());
            imnodes::SetNodeScreenSpacePos(top.emplace_back(new_model),
                                           click_pos);
        }
    }

    {
        int start = 0, end = 0;
        if (imnodes::IsLinkCreated(&start, &end)) {
            output_port_id out = get_out(start);
            input_port_id in = get_in(end);

            auto* o_port = sim.output_ports.try_to_get(out);
            auto* i_port = sim.input_ports.try_to_get(in);

            if (i_port && o_port)
                if (auto status = sim.connect(out, in); is_bad(status))
                    log_w.log(6,
                              "Fail to connect these models: %s\n",
                              status_string(status));
        }
    }

    ImGui::PopStyleVar();

    const int num_selected_links = imnodes::NumSelectedLinks();
    const int num_selected_nodes = imnodes::NumSelectedNodes();
    static ImVector<int> selected_nodes;
    static ImVector<int> selected_links;

    if (num_selected_nodes > 0) {
        selected_nodes.resize(num_selected_nodes, -1);

        if (ImGui::IsKeyReleased('X')) {
            imnodes::GetSelectedNodes(selected_nodes.begin());
            log_w.log(7, "%d model(s) to delete\n", num_selected_nodes);
            free_children(selected_nodes);
            imnodes::ClearSelectedNodesAndLinks();
        } else if (ImGui::IsKeyReleased('D')) {
            imnodes::GetSelectedNodes(selected_nodes.begin());
            log_w.log(7, "%d model(s)/group(s) to copy\n", num_selected_nodes);
            copy(selected_nodes);
            imnodes::ClearSelectedNodesAndLinks();
        } else if (ImGui::IsKeyReleased('G')) {
            if (num_selected_nodes > 1) {
                imnodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(7, "%d model(s) to group\n", num_selected_nodes);
                group(selected_nodes);
                imnodes::ClearSelectedNodesAndLinks();
            } else if (num_selected_nodes == 1) {
                imnodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(7, "group to ungroup\n");
                ungroup(selected_nodes[0]);
                imnodes::ClearSelectedNodesAndLinks();
            }
        }
        selected_nodes.resize(0);
    } else if (num_selected_links > 0) {
        selected_links.resize(static_cast<size_t>(num_selected_links));

        if (ImGui::IsKeyReleased('X')) {
            std::fill_n(selected_links.begin(), selected_links.size(), -1);
            imnodes::GetSelectedLinks(selected_links.begin());
            std::sort(
              selected_links.begin(), selected_links.end(), std::less<int>());

            int i = 0;
            int link_id_to_delete = selected_links[0];
            int current_link_id = 0;

            log_w.log(7, "%d connection(s) to delete\n", num_selected_links);

            output_port* o_port = nullptr;
            while (sim.output_ports.next(o_port) && link_id_to_delete != -1) {
                for (const auto dst : o_port->connections) {
                    if (auto* i_port = sim.input_ports.try_to_get(dst);
                        i_port) {
                        if (current_link_id == link_id_to_delete) {
                            sim.disconnect(sim.output_ports.get_id(o_port),
                                           dst);

                            ++i;

                            if (i != selected_links.size())
                                link_id_to_delete = selected_links[i];
                            else
                                link_id_to_delete = -1;
                        }

                        ++current_link_id;
                    }
                }
            }
            imnodes::ClearSelectedNodesAndLinks();
            selected_links.resize(0);
        }
    }

    ImGui::End();

    return true;
}

editor*
editors_new()
{
    if (!editors.can_alloc(1u)) {
        log_w.log(2, "Too many open editor\n");
        return nullptr;
    }

    auto& ed = editors.alloc();
    if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
        editors.free(ed);
        return nullptr;
    }

    log_w.log(5, "Open editor %s\n", ed.name.c_str());
    return &ed;
}

void
editors_free(editor& ed)
{
    log_w.log(5, "Close editor %s\n", ed.name.c_str());
    editors.free(ed);
}

static void
initialize_observation(irt::editor* ed) noexcept
{
    ed->observation_outputs.clear();
    observer* obs = nullptr;
    while (ed->sim.observers.next(obs)) {
        auto* output = ed->observation_outputs.emplace_back(obs->name.c_str());

        const auto type =
          ed->observation_types[get_index(ed->sim.observers.get_id(*obs))];
        const auto diff = ed->simulation_end - ed->simulation_begin;
        const auto freq = diff / obs->time_step;
        const auto length = static_cast<size_t>(freq);
        output->observation_type = type;

        if (match(type,
                  observation_output::type::multiplot,
                  observation_output::type::both)) {
            output->xs.init(length);
            output->ys.init(length);
        }

        if (!obs->name.empty()) {
            const std::filesystem::path obs_file_path =
              ed->observation_directory / obs->name.c_str();

            if (type == observation_output::type::file ||
                type == observation_output::type::both) {
                if (output->ofs.open(obs_file_path); !output->ofs.is_open())
                    log_w.log(4,
                              "Fail to open "
                              "observation file: %s in "
                              "%s\n",
                              obs->name.c_str(),
                              ed->observation_directory.u8string().c_str());
            }
        }

        obs->initialize = &observation_output_initialize;
        obs->observe = &observation_output_observe;
        obs->free = &observation_output_free;
        obs->user_data = static_cast<void*>(output);
    }
}

static editor*
make_combo_editor_name(editor_id& current) noexcept
{
    editor* first = editors.try_to_get(current);
    if (first == nullptr) {
        if (!editors.next(first)) {
            current = undefined<editor_id>();
            return nullptr;
        }
    }

    current = editors.get_id(first);

    if (ImGui::BeginCombo("Name", first->name.c_str())) {
        editor* ed = nullptr;
        while (editors.next(ed)) {
            const bool is_selected = current == editors.get_id(ed);

            if (ImGui::Selectable(ed->name.c_str(), is_selected))
                current = editors.get_id(ed);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return editors.try_to_get(current);
}

void
show_simulation_box(bool* show_simulation)
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Simulation", show_simulation)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        ImGui::InputDouble("Begin", &ed->simulation_begin);
        ImGui::InputDouble("End", &ed->simulation_end);
        ImGui::Checkbox("Show values", &ed->simulation_show_value);

        if (ImGui::CollapsingHeader("Simulation run one")) {
            if (ed->st == simulation_status::running_once_need_join) {
                if (ed->simulation_thread.joinable()) {
                    ed->simulation_thread.join();
                    ed->st = simulation_status::success;
                }
            } else {
                if (ImGui::Button("run")) {
                    initialize_observation(ed);

                    ed->st = simulation_status::running_once;
                    ed->stop = false;

                    ed->simulation_thread =
                      std::thread(&run_simulation,
                                  std::ref(ed->sim),
                                  ed->simulation_begin,
                                  ed->simulation_end,
                                  std::ref(ed->simulation_current),
                                  std::ref(ed->st),
                                  std::cref(ed->stop));
                }
            }

            if (ed->st == simulation_status::running_once) {
                ImGui::SameLine();
                if (ImGui::Button("Force stop"))
                    ed->stop = true;
            }
        }

        if (match(ed->st,
                  simulation_status::success,
                  simulation_status::running_step)) {
            if (ImGui::CollapsingHeader("Simulation step by step")) {
                if (ImGui::Button("init")) {
                    ed->sim.clean();
                    initialize_observation(ed);
                    ed->simulation_current = ed->simulation_begin;
                    if (auto ret = ed->sim.initialize(ed->simulation_current);
                        irt::is_bad(ret)) {
                        log_w.log(3,
                                  "Simulation initialization failure (%s)\n",
                                  irt::status_string(ret));
                        ed->st = simulation_status::success;
                    } else {
                        ed->st = simulation_status::running_step;
                    }
                }

                if (ed->st == simulation_status::running_step) {
                    ImGui::SameLine();
                    if (ImGui::Button("step")) {
                        if (ed->simulation_current < ed->simulation_end) {
                            if (auto ret = ed->sim.run(ed->simulation_current);
                                irt::is_bad(ret)) {
                                log_w.log(3,
                                          "Simulation failure (%s)\n",
                                          irt::status_string(ret));

                                ed->st = simulation_status::success;
                            }
                        }
                    }
                }

                if (ed->st == simulation_status::running_step) {
                    ImGui::SameLine();
                    if (ImGui::Button("step x10")) {
                        for (int i = 0; i < 10; ++i) {
                            if (ed->simulation_current < ed->simulation_end) {
                                if (auto ret =
                                      ed->sim.run(ed->simulation_current);
                                    irt::is_bad(ret)) {
                                    log_w.log(3,
                                              "Simulation failure (%s)\n",
                                              irt::status_string(ret));

                                    ed->st = simulation_status::success;
                                }
                            }
                        }
                    }
                }

                if (ed->st == simulation_status::running_step) {
                    ImGui::SameLine();
                    if (ImGui::Button("step x100")) {
                        for (int i = 0; i < 100; ++i) {
                            if (ed->simulation_current < ed->simulation_end) {
                                if (auto ret =
                                      ed->sim.run(ed->simulation_current);
                                    irt::is_bad(ret)) {
                                    log_w.log(3,
                                              "Simulation failure (%s)\n",
                                              irt::status_string(ret));

                                    ed->st = simulation_status::success;
                                }
                            }
                        }
                    }
                }
            }

            if (ImGui::CollapsingHeader("Simulation until")) {
                if (ImGui::Button("init")) {
                    ed->sim.clean();
                    initialize_observation(ed);
                    ed->simulation_current = ed->simulation_begin;
                    ed->simulation_until =
                      static_cast<float>(ed->simulation_begin);
                    if (auto ret = ed->sim.initialize(ed->simulation_current);
                        irt::is_bad(ret)) {
                        log_w.log(3,
                                  "Simulation initialization failure (%s)\n",
                                  irt::status_string(ret));
                        ed->st = simulation_status::success;
                    } else {
                        ed->st = simulation_status::running_step;
                    }
                }

                if (ed->st == simulation_status::running_step) {
                    ImGui::SliderFloat(
                      "date",
                      &ed->simulation_until,
                      static_cast<float>(ed->simulation_current),
                      static_cast<float>(ed->simulation_end));

                    if (ImGui::Button("run until")) {
                        while (ed->simulation_current <
                               static_cast<double>(ed->simulation_until)) {
                            if (auto ret = ed->sim.run(ed->simulation_current);
                                irt::is_bad(ret)) {
                                log_w.log(3,
                                          "Simulation failure (%s)\n",
                                          irt::status_string(ret));

                                ed->st = simulation_status::success;
                            }
                        }
                    }
                }
            }
        }

        if (match(ed->st,
                  simulation_status::success,
                  simulation_status::running_once,
                  simulation_status::running_once_need_join,
                  simulation_status::running_step)) {
            ImGui::Text("Current: %g", ed->simulation_current);

            const double duration = ed->simulation_end - ed->simulation_begin;
            const double elapsed =
              ed->simulation_current - ed->simulation_begin;
            const double fraction = elapsed / duration;
            ImGui::ProgressBar(static_cast<float>(fraction));
        }
    }

    ImGui::End();
}

static void
show_settings_window(bool* is_open)
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
        compute_color();
    if (ImGui::ColorEdit3(
          "cluster", (float*)&gui_cluster_color, ImGuiColorEditFlags_NoOptions))
        compute_color();

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

void
node_editor_initialize()
{
    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2, "Fail to initialize irritator: %s\n", status_string(ret));
    } else {
        compute_color();
        if (auto* ed = editors_new(); ed)
            ed->context = imnodes::EditorContextCreate();
    }
}

void
show_plot_box(bool* show_plot)
{
    ImGui::SetNextWindowPos(ImVec2(50, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_Once);
    if (!ImGui::Begin("Plot", show_plot)) {
        ImGui::End();
        return;
    }

    static editor_id current = undefined<editor_id>();
    if (auto* ed = make_combo_editor_name(current); ed) {
        if (match(ed->st,
                  simulation_status::success,
                  simulation_status::running_once,
                  simulation_status::running_once_need_join,
                  simulation_status::running_step)) {
            if (ImPlot::BeginPlot("simulation", "t", "s")) {
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.f);
                for (const auto& obs : ed->observation_outputs) {
                    if (obs.observation_type ==
                          observation_output::type::multiplot &&
                        obs.xs.data()) {
                        ImPlot::PlotLine(obs.name.c_str(),
                                         obs.xs.data(),
                                         obs.ys.data(),
                                         static_cast<int>(obs.xs.size()));
                    }
                }
                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        }
    }

    ImGui::End();
}

bool
node_editor_show()
{
    static bool show_log = true;
    static bool show_simulation = true;
    static bool show_demo = false;
    static bool show_plot = true;
    static bool show_settings = false;
    bool ret = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                if (auto* ed = editors_new(); ed)
                    ed->context = imnodes::EditorContextCreate();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
                ret = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            editor* ed = nullptr;
            while (editors.next(ed))
                ImGui::MenuItem(ed->name.c_str(), nullptr, &ed->show);

            ImGui::MenuItem("Simulation", nullptr, &show_simulation);
            ImGui::MenuItem("Plot", nullptr, &show_plot);
            ImGui::MenuItem("Settings", nullptr, &show_settings);
            ImGui::MenuItem("Log", nullptr, &show_log);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    editor* ed = nullptr;
    while (editors.next(ed)) {
        if (ed->show) {
            if (!ed->show_editor()) {
                editor* next = ed;
                editors.next(next);
                editors_free(*ed);
            }
        }
    }

    if (show_log)
        log_w.show(&show_log);

    if (show_simulation)
        show_simulation_box(&show_simulation);

    if (show_plot)
        show_plot_box(&show_plot);

    if (show_demo)
        ImGui::ShowDemoWindow();

    if (show_settings)
        show_settings_window(&show_settings);

    return ret;
}

void
node_editor_shutdown()
{
    editor* ed = nullptr;
    while (editors.next(ed))
        imnodes::EditorContextFree(ed->context);
}

} // namespace irt
