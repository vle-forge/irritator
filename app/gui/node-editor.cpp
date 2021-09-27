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

    constexpr irt::u32 mask = ~(15u << 28u);
    irt::u32 index = real_node_id & mask;

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
    ImNodesIO& io = ImNodes::GetIO();
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

    gui_hovered_cluster_color =
      ImGui::ColorConvertFloat4ToU32(gui_cluster_color * 1.25f);
    gui_selected_cluster_color =
      ImGui::ColorConvertFloat4ToU32(gui_cluster_color * 1.5f);
}

void
editor::clear() noexcept
{
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

    auto& new_cluster = clusters.alloc();
    auto new_cluster_id = clusters.get_id(new_cluster);
    format(new_cluster.name, "Group {}", new_cluster_id);
    parent(new_cluster_id, undefined<cluster_id>());

    /* First, move children models and groups from the current cluster into the
       newly allocated cluster. */

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

    for (const auto& child : top.children) {
        if (child.first.index() == 0) {
            const auto child_id = std::get<model_id>(child.first);

            if (auto* model = sim.models.try_to_get(child_id); model) {
                sim.dispatch(
                  *model,
                  [this, &new_cluster]<typename Dynamics>(Dynamics& dyn) {
                      if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                          for (sz i = 0u, e = std::size(dyn.x); i != e; ++i) {
                              for (const auto& elem : dyn.x[i].connections) {
                                  auto* src = sim.models.try_to_get(elem.model);
                                  if (src &&
                                      is_in_hierarchy(new_cluster,
                                                      this->parent(elem.model)))
                                      new_cluster.output_ports.emplace_back(
                                        make_output_node_id(elem.model,
                                                            elem.port_index));
                              }
                          }
                      }

                      if constexpr (is_detected_v<has_output_port_t,
                                                  Dynamics>) {
                          for (sz i = 0u, e = std::size(dyn.y); i != e; ++i) {
                              for (const auto& elem : dyn.y[i].connections) {
                                  auto* src = sim.models.try_to_get(elem.model);
                                  if (src &&
                                      is_in_hierarchy(new_cluster,
                                                      this->parent(elem.model)))
                                      new_cluster.input_ports.emplace_back(
                                        make_input_node_id(elem.model,
                                                           elem.port_index));
                              }
                          }
                      }
                  });
            }
        } else {
            const auto child_id = std::get<cluster_id>(child.first);

            if (auto* group = clusters.try_to_get(child_id); group) {
                for (const auto id : group->input_ports) {
                    const auto model_port = get_in(id);

                    if (model_port.model) {
                        sim.dispatch(
                          *model_port.model,
                          [this, &new_cluster, &model_port]<typename Dynamics>(
                            Dynamics& dyn) {
                              if constexpr (is_detected_v<has_input_port_t,
                                                          Dynamics>) {
                                  for (sz i = 0u, e = std::size(dyn.x); i != e;
                                       ++i) {
                                      for (const auto& elem :
                                           dyn.x[model_port.port_index]
                                             .connections) {
                                          auto* src =
                                            sim.models.try_to_get(elem.model);
                                          if (src &&
                                              is_in_hierarchy(
                                                new_cluster,
                                                this->parent(elem.model)))
                                              new_cluster.output_ports
                                                .emplace_back(
                                                  make_output_node_id(
                                                    elem.model,
                                                    elem.port_index));
                                      }
                                  }
                              }
                          });
                    }
                }

                for (const auto id : group->output_ports) {
                    const auto model_port = get_out(id);

                    if (model_port.model) {
                        sim.dispatch(
                          *model_port.model,
                          [this, &new_cluster, &model_port]<typename Dynamics>(
                            Dynamics& dyn) {
                              if constexpr (is_detected_v<has_output_port_t,
                                                          Dynamics>) {
                                  for (sz i = 0u, e = std::size(dyn.y); i != e;
                                       ++i) {
                                      for (const auto& elem :
                                           dyn.y[model_port.port_index]
                                             .connections) {
                                          auto* dst =
                                            sim.models.try_to_get(elem.model);
                                          if (dst &&
                                              is_in_hierarchy(
                                                new_cluster,
                                                this->parent(elem.model)))
                                              new_cluster.input_ports
                                                .emplace_back(
                                                  make_input_node_id(
                                                    elem.model,
                                                    elem.port_index));
                                      }
                                  }
                              }
                          });
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
                log_w.log(7, "delete model %" PRIu64 "\n", id);
                sim.deallocate(id);

                observation_dispatch(
                  get_index(id),
                  [this](auto& outs, auto id) { outs.free(id); });

                observation_outputs[get_index(id)] = std::monostate{};
            }
        } else {
            auto id = std::get<cluster_id>(child);
            clusters_mapper[get_index(id)] = undefined<cluster_id>();
            if (auto* gp = clusters.try_to_get(id); gp) {
                log_w.log(7, "delete group %" PRIu64 "\n", gp);
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
                log_w.log(7, "delete %" PRIu64 "\n", id);
                parent(id, undefined<cluster_id>());
                sim.deallocate(id);

                observation_dispatch(
                  get_index(id),
                  [this](auto& outs, const auto id) { outs.free(id); });

                observation_outputs[get_index(id)] = std::monostate{};
            }
        } else {
            const auto id = std::get<cluster_id>(child.first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                clusters_mapper[get_index(id)] = undefined<cluster_id>();
                log_w.log(7, "delete group %" PRIu64 "\n", id);
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

        copy_input_port(const int src_, const int dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        int src, dst;
    };

    struct copy_output_port
    {
        copy_output_port() = default;

        copy_output_port(const int src_, const int dst_) noexcept
          : src(src_)
          , dst(dst_)
        {}

        int src, dst;
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
        const typename Container::value_type val{};

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

    int get_input_port(const int src) const noexcept
    {
        return get(c_input_ports, src);
    }

    int get_output_port(const int src) const noexcept
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
              *mdl,
              [this, &sim, mdl, &mdl_id_dst]<typename Dynamics>(
                Dynamics& /*dyn*/) -> status {
                  irt_return_if_fail(sim.models.can_alloc(1),
                                     status::dynamics_not_enough_memory);

                  auto& new_dyn = sim.alloc<Dynamics>();
                  *mdl_id_dst = sim.get_id(new_dyn);

                  if constexpr (is_detected_v<has_input_port_t, Dynamics>)
                      for (sz j = 0u, ej = std::size(new_dyn.x); j != ej; ++j)
                          this->c_input_ports.emplace_back(
                            make_input_node_id(sim.models.get_id(mdl), (int)j),
                            make_input_node_id(*mdl_id_dst, (int)j));

                  if constexpr (is_detected_v<has_output_port_t, Dynamics>)
                      for (sz j = 0, ej = std::size(new_dyn.y); j != ej; ++j)
                          this->c_output_ports.emplace_back(
                            make_output_node_id(sim.models.get_id(mdl), (int)j),
                            make_input_node_id(*mdl_id_dst, (int)j));

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

        // for (size_t i = 0, e = std::size(c_input_ports); i != e; ++i) {
        //    const auto* src =
        //    sim.input_ports.try_to_get(c_input_ports[i].src); auto* dst =
        //    sim.input_ports.try_to_get(c_input_ports[i].dst);

        //    assert(dst->connections.empty());

        //    for (const auto port : src->connections) {
        //        const auto index = get_output_port(port);
        //        dst->connections.emplace_front(c_output_ports[index].dst);
        //    }
        //}

        // for (size_t i = 0, e = std::size(c_output_ports); i != e; ++i) {
        //    const auto* src =
        //      sim.output_ports.try_to_get(c_output_ports[i].src);
        //    auto* dst = sim.output_ports.try_to_get(c_output_ports[i].dst);

        //    assert(dst->connections.empty());

        //    for (const auto port : src->connections) {
        //        const auto index = get_input_port(port);
        //        dst->connections.emplace_front(c_input_ports[index].dst);
        //    }
        //}

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
compute_connection_distance(const child_id src,
                            const child_id dst,
                            editor& ed,
                            const float k)
{
    const auto v = ed.get_top_group_ref(src);
    const auto u = ed.get_top_group_ref(dst);

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
}

static void
compute_connection_distance(const model& mdl,
                            const int port,
                            editor& ed,
                            const float k)
{
    ed.sim.dispatch(
      mdl, [&mdl, port, &ed, k]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              for (auto& dst : dyn.y[port].connections)
                  compute_connection_distance(
                    ed.sim.get_id(mdl), dst.model, ed, k);
          }
      });
}

static void
compute_connection_distance(const model& mdl, editor& ed, const float k)
{
    ed.sim.dispatch(
      mdl, [&mdl, &ed, k]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              for (sz i = 0, e = std::size(dyn.y); i != e; ++i)
                  for (auto& dst : dyn.y[i].connections)
                      compute_connection_distance(
                        ed.sim.get_id(mdl), dst.model, ed, k);
          }
      });
}

void
editor::compute_grid_layout() noexcept
{
    const auto size = length(top.children);

    if (size == 0)
        return;

    const auto tmp = std::sqrt(size);
    const auto column = static_cast<int>(tmp);
    auto line = column;
    auto remaining = size - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const auto panning = ImNodes::EditorContextGetPanning();
    auto new_pos = panning;

    int elem = 0;

    for (int i = 0; i < column; ++i) {
        new_pos.y =
          panning.y + static_cast<float>(i) * settings.grid_layout_y_distance;
        for (int j = 0; j < line; ++j) {
            new_pos.x = panning.x +
                        static_cast<float>(j) * settings.grid_layout_x_distance;
            ImNodes::SetNodeGridSpacePos(top.children[elem].second, new_pos);
            positions[elem].x = new_pos.x;
            positions[elem].y = new_pos.y;
            ++elem;
        }
    }

    new_pos.x = panning.x;
    new_pos.y =
      panning.y + static_cast<float>(column) * settings.grid_layout_y_distance;
    for (int j = 0; j < remaining; ++j) {
        new_pos.x =
          panning.x + static_cast<float>(j) * settings.grid_layout_x_distance;
        ImNodes::SetNodeGridSpacePos(top.children[elem].second, new_pos);
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

    const auto size = length(top.children);
    const auto tmp = std::sqrt(size);
    const auto column = static_cast<int>(tmp);
    auto line = column;
    auto remaining = size - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    const float W =
      static_cast<float>(column) * settings.automatic_layout_x_distance;
    const float L =
      line + (remaining > 0) ? settings.automatic_layout_y_distance : 0.f;
    const float area = W * L;
    const float k_square = area / static_cast<float>(top.children.size());
    const float k = std::sqrt(k_square);

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

        for (size_t i = 0, e = top.children.size(); i != e; ++i) {
            if (top.children[i].first.index() == 0) {
                const auto id = std::get<model_id>(top.children[i].first);
                if (const auto* mdl = sim.models.try_to_get(id); mdl)
                    compute_connection_distance(*mdl, *this, k);
            } else {
                const auto id = std::get<cluster_id>(top.children[i].first);
                if (auto* gp = clusters.try_to_get(id); gp) {
                    for (sz i = 0; i < std::size(gp->output_ports); ++i) {
                        auto model_port = get_out(gp->output_ports[i]);
                        if (model_port.model) {
                            compute_connection_distance(*model_port.model,
                                                        model_port.port_index,
                                                        *this,
                                                        k);
                        }
                    }
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

            ImNodes::SetNodeGridSpacePos(top.children[v].second, positions[v]);
        }
    }

    ImNodes::EditorContextResetPanning(positions[0]);
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
    irt_return_if_bad(sim.init(to_unsigned(settings.kernel_model_cache),
                               to_unsigned(settings.kernel_message_cache)));
    irt_return_if_bad(clusters.init(sim.models.capacity()));
    irt_return_if_bad(top.init(to_unsigned(settings.gui_node_cache)));
    irt_return_if_bad(plot_outs.init(to_unsigned(settings.kernel_model_cache)));
    irt_return_if_bad(file_outs.init(to_unsigned(settings.kernel_model_cache)));
    irt_return_if_bad(
      file_discrete_outs.init(to_unsigned(settings.kernel_model_cache)));

    irt_return_if_bad(srcs.init(50));
    sim.source_dispatch = srcs;

    try {
        observation_outputs.resize(sim.models.capacity());
        models_mapper.resize(sim.models.capacity(), undefined<cluster_id>());
        clusters_mapper.resize(sim.models.capacity(), undefined<cluster_id>());
        models_make_transition.resize(sim.models.capacity(), false);

        positions.resize(sim.models.capacity() + clusters.capacity(),
                         ImVec2{ 0.f, 0.f });
        displacements.resize(sim.models.capacity() + clusters.capacity(),
                             ImVec2{ 0.f, 0.f });

        observation_directory = std::filesystem::current_path();

    } catch (const std::bad_alloc& /*e*/) {
        return status::gui_not_enough_memory;
    }

    use_real_time = false;
    synchronize_timestep = 0.;

    format(name, "Editor {}", id);

    return status::success;
}

status
editor::add_lotka_volterra() noexcept
{
    if (!sim.models.can_alloc(10))
        return status::simulation_not_enough_model;

    auto& sum_a = sim.alloc<adder_2>();
    auto& sum_b = sim.alloc<adder_2>();
    auto& product = sim.alloc<mult_2>();
    auto& integrator_a = sim.alloc<integrator>();
    auto& integrator_b = sim.alloc<integrator>();
    auto& quantifier_a = sim.alloc<quantifier>();
    auto& quantifier_b = sim.alloc<quantifier>();

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

    top.emplace_back(sim.get_id(sum_a));
    top.emplace_back(sim.get_id(sum_b));
    top.emplace_back(sim.get_id(product));
    top.emplace_back(sim.get_id(integrator_a));
    top.emplace_back(sim.get_id(integrator_b));
    top.emplace_back(sim.get_id(quantifier_a));
    top.emplace_back(sim.get_id(quantifier_b));

    parent(sim.get_id(sum_a), undefined<cluster_id>());
    parent(sim.get_id(sum_b), undefined<cluster_id>());
    parent(sim.get_id(product), undefined<cluster_id>());
    parent(sim.get_id(integrator_a), undefined<cluster_id>());
    parent(sim.get_id(integrator_b), undefined<cluster_id>());
    parent(sim.get_id(quantifier_a), undefined<cluster_id>());
    parent(sim.get_id(quantifier_b), undefined<cluster_id>());

    return status::success;
}

status
editor::add_izhikevitch() noexcept
{
    if (!sim.models.can_alloc(14))
        return status::simulation_not_enough_model;

    auto& constant = sim.alloc<irt::constant>();
    auto& constant2 = sim.alloc<irt::constant>();
    auto& constant3 = sim.alloc<irt::constant>();
    auto& sum_a = sim.alloc<irt::adder_2>();
    auto& sum_b = sim.alloc<irt::adder_2>();
    auto& sum_c = sim.alloc<irt::adder_4>();
    auto& sum_d = sim.alloc<irt::adder_2>();
    auto& product = sim.alloc<irt::mult_2>();
    auto& integrator_a = sim.alloc<irt::integrator>();
    auto& integrator_b = sim.alloc<irt::integrator>();
    auto& quantifier_a = sim.alloc<irt::quantifier>();
    auto& quantifier_b = sim.alloc<irt::quantifier>();
    auto& cross = sim.alloc<irt::cross>();
    auto& cross2 = sim.alloc<irt::cross>();

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

    top.emplace_back(sim.get_id(constant));
    top.emplace_back(sim.get_id(constant2));
    top.emplace_back(sim.get_id(constant3));
    top.emplace_back(sim.get_id(sum_a));
    top.emplace_back(sim.get_id(sum_b));
    top.emplace_back(sim.get_id(sum_c));
    top.emplace_back(sim.get_id(sum_d));
    top.emplace_back(sim.get_id(product));
    top.emplace_back(sim.get_id(integrator_a));
    top.emplace_back(sim.get_id(integrator_b));
    top.emplace_back(sim.get_id(quantifier_a));
    top.emplace_back(sim.get_id(quantifier_b));
    top.emplace_back(sim.get_id(cross));
    top.emplace_back(sim.get_id(cross2));

    parent(sim.get_id(constant), undefined<cluster_id>());
    parent(sim.get_id(constant2), undefined<cluster_id>());
    parent(sim.get_id(constant3), undefined<cluster_id>());
    parent(sim.get_id(sum_a), undefined<cluster_id>());
    parent(sim.get_id(sum_b), undefined<cluster_id>());
    parent(sim.get_id(sum_c), undefined<cluster_id>());
    parent(sim.get_id(sum_d), undefined<cluster_id>());
    parent(sim.get_id(product), undefined<cluster_id>());
    parent(sim.get_id(integrator_a), undefined<cluster_id>());
    parent(sim.get_id(integrator_b), undefined<cluster_id>());
    parent(sim.get_id(quantifier_a), undefined<cluster_id>());
    parent(sim.get_id(quantifier_b), undefined<cluster_id>());
    parent(sim.get_id(cross), undefined<cluster_id>());
    parent(sim.get_id(cross2), undefined<cluster_id>());

    return status::success;
}

static int
show_connection(editor& ed, const model& mdl, int port, int connection_id)
{
    ed.sim.dispatch(
      mdl,
      [&ed, &mdl, port, &connection_id]<typename Dynamics>(
        Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              int out = make_output_node_id(ed.sim.get_id(dyn), port);

              for (const auto& c : dyn.y[port].connections) {
                  if (auto* mdl_dst = ed.sim.models.try_to_get(c.model);
                      mdl_dst) {
                      int in = make_input_node_id(c.model, c.port_index);
                      ImNodes::Link(connection_id++, out, in);
                  }
              }
          }
      });

    return connection_id;
}

static int
show_connection(editor& ed, const model& mdl, int connection_id)
{
    ed.sim.dispatch(
      mdl,
      [&ed, &mdl, &connection_id]<typename Dynamics>(Dynamics& dyn) -> void {
          if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
              for (sz i = 0, e = std::size(dyn.y); i != e; ++i) {
                  int out = make_output_node_id(ed.sim.get_id(dyn), (int)i);

                  for (const auto& c : dyn.y[i].connections) {
                      if (auto* mdl_dst = ed.sim.models.try_to_get(c.model);
                          mdl_dst) {
                          int in = make_input_node_id(c.model, c.port_index);
                          ImNodes::Link(connection_id++, out, in);
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

    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].first.index() == 0) {
            const auto id = std::get<model_id>(top.children[i].first);
            if (const auto* mdl = sim.models.try_to_get(id); mdl)
                connection_id = show_connection(*this, *mdl, connection_id);
        } else {
            const auto id = std::get<cluster_id>(top.children[i].first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                for (sz i = 0; i < std::size(gp->output_ports); ++i) {
                    auto model_port = get_out(gp->output_ports[i]);
                    if (model_port.model) {
                        show_connection(*this,
                                        *model_port.model,
                                        model_port.port_index,
                                        connection_id);
                    }
                }
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
            const auto node = get_in(*it);
            if (node.model) {
                ImNodes::BeginInputAttribute(*it,
                                             ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted("");
                ImNodes::EndInputAttribute();
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
            const auto node = get_out(*it);

            if (node.model) {
                ImNodes::BeginOutputAttribute(*it,
                                              ImNodesPinShape_TriangleFilled);
                ImGui::TextUnformatted("");
                ImNodes::EndOutputAttribute();
                ++it;
            } else {
                it = mdl.output_ports.erase(it);
            }
        }
    }
}

template<typename Dynamics>
static void
add_input_attribute(editor& ed, const Dynamics& dyn) noexcept
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        const auto** names = get_input_port_names<Dynamics>();

        for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
            irt_assert(i < 8u);
            const auto& mdl = get_model(dyn);
            const auto mdl_id = ed.sim.models.get_id(mdl);

            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginInputAttribute(make_input_node_id(mdl_id, (int)i),
                                         ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndInputAttribute();
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
            irt_assert(i < 8u);

            const auto& mdl = get_model(dyn);
            const auto mdl_id = ed.sim.models.get_id(mdl);

            assert(ed.sim.models.try_to_get(mdl_id) == &mdl);

            ImNodes::BeginOutputAttribute(make_output_node_id(mdl_id, (int)i),
                                          ImNodesPinShape_TriangleFilled);
            ImGui::TextUnformatted(names[i]);
            ImNodes::EndOutputAttribute();
        }
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const none& /*dyn*/)
{}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_integrator& dyn)
{
    ImGui::Text("X %.3f", dyn.X);
    ImGui::Text("dQ %.3f", dyn.default_dQ);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_2& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_3& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_sum_4& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
    ImGui::Text("%.3f", dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_multiplier& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_2& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_3& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_wsum_4& dyn)
{
    ImGui::Text("%.3f", dyn.values[0]);
    ImGui::Text("%.3f", dyn.values[1]);
    ImGui::Text("%.3f", dyn.values[2]);
    ImGui::Text("%.3f", dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_sum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_multiplier& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_wsum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_sum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_multiplier& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_2& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[2]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_3& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[3]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[5]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_wsum_4& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.values[0], dyn.values[4]);
    ImGui::Text("%.3f %.3f", dyn.values[1], dyn.values[5]);
    ImGui::Text("%.3f %.3f", dyn.values[2], dyn.values[6]);
    ImGui::Text("%.3f %.3f", dyn.values[3], dyn.values[7]);
}

static void
show_dynamics_values(simulation& /*sim*/, const integrator& dyn)
{
    ImGui::Text("value %.3f", dyn.current_value);
}

static void
show_dynamics_values(simulation& /*sim*/, const quantifier& dyn)
{
    ImGui::Text("up threshold %.3f", dyn.m_upthreshold);
    ImGui::Text("down threshold %.3f", dyn.m_downthreshold);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_2& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_3& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const adder_4& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_2& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_3& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const mult_4& dyn)
{
    ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
    ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
    ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
    ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
}

static void
show_dynamics_values(simulation& /*sim*/, const counter& dyn)
{
    ImGui::Text("number %ld", static_cast<long>(dyn.number));
}

static void
show_dynamics_values(simulation& /*sim*/, const queue& dyn)
{
    if (dyn.queue.empty()) {
        ImGui::Text("empty");
    } else {
        ImGui::Text("size %" PRId64, dyn.queue.size());
        ImGui::Text("next ta %.3f", dyn.queue.begin()->real[0]);
        ImGui::Text("next value %.3f", dyn.queue.begin()->real[1]);
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const dynamic_queue& dyn)
{
    if (dyn.queue.empty()) {
        ImGui::Text("empty");
    } else {
        ImGui::Text("size %" PRId64, dyn.queue.size());
        ImGui::Text("next ta %.3f", dyn.queue.begin()->real[0]);
        ImGui::Text("next value %.3f", dyn.queue.begin()->real[1]);
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const priority_queue& dyn)
{
    if (dyn.queue.empty()) {
        ImGui::Text("empty");
    } else {
        ImGui::Text("size %" PRId64, dyn.queue.size());
        ImGui::Text("next ta %.3f", dyn.queue.begin()->real[0]);
        ImGui::Text("next value %.3f", dyn.queue.begin()->real[1]);
    }
}

static void
show_dynamics_values(simulation& /*sim*/, const generator& dyn)
{
    ImGui::Text("next %.3f", dyn.sigma);
}

static void
show_dynamics_values(simulation& /*sim*/, const constant& dyn)
{
    ImGui::Text("next %.3f", dyn.sigma);
    ImGui::Text("value %.3f", dyn.value);
}

template<int QssLevel>
static void
show_dynamics_values(simulation& /*sim*/, const abstract_cross<QssLevel>& dyn)
{
    ImGui::Text("threshold: %.3f", dyn.threshold);
    ImGui::Text("value: %.3f", dyn.value[0]);
    ImGui::Text("if-value: %.3f", dyn.if_value[0]);
    ImGui::Text("else-value: %.3f", dyn.else_value[0]);

    if (dyn.detect_up)
        ImGui::Text("up detection");
    else
        ImGui::Text("down detection");
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_power& dyn)
{
    ImGui::Text("%.3f", dyn.value[0]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_power& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.value[0], dyn.value[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_power& dyn)
{
    ImGui::Text("%.3f %.3f %.3f", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss1_square& dyn)
{
    ImGui::Text("%.3f", dyn.value[0]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss2_square& dyn)
{
    ImGui::Text("%.3f %.3f", dyn.value[0], dyn.value[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const qss3_square& dyn)
{
    ImGui::Text("%.3f %.3f %.3f", dyn.value[0], dyn.value[1], dyn.value[2]);
}

static void
show_dynamics_values(simulation& /*sim*/, const cross& dyn)
{
    ImGui::Text("threshold: %.3f", dyn.threshold);
    ImGui::Text("value: %.3f", dyn.value);
    ImGui::Text("if-value: %.3f", dyn.if_value);
    ImGui::Text("else-value: %.3f", dyn.else_value);
}

static void
show_dynamics_values(simulation& /*sim*/, const accumulator_2& dyn)
{
    ImGui::Text("number %.3f", dyn.number);
    ImGui::Text("- 0: %.3f", dyn.numbers[0]);
    ImGui::Text("- 1: %.3f", dyn.numbers[1]);
}

static void
show_dynamics_values(simulation& /*sim*/, const time_func& dyn)
{
    ImGui::Text("value %.3f", dyn.value);
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
        ImGui::Text("value %.3f", dyn.default_data[dyn.i]);
    else
        ImGui::Text("no data");
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_X);
    ImGui::InputDouble("reset", &dyn.default_dQ);
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
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
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
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
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
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_wsum_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_wsum_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, integrator& dyn)
{
    ImGui::InputDouble("value", &dyn.default_current_value);
    ImGui::InputDouble("reset", &dyn.default_reset_value);
}

static void
show_dynamics_inputs(editor& /*ed*/, quantifier& dyn)
{
    ImGui::InputDouble("quantum", &dyn.default_step_size);
    ImGui::SliderInt("archive length", &dyn.default_past_length, 3, 100);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, adder_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_2& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_3& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
}

static void
show_dynamics_inputs(editor& /*ed*/, mult_4& dyn)
{
    ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
    ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
    ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
    ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
}

static void
show_dynamics_inputs(editor& /*ed*/, counter& /*dyn*/)
{}

static void
show_dynamics_inputs(editor& /*ed*/, queue& dyn)
{
    ImGui::InputDouble("delay", &dyn.default_ta);
    ImGui::SameLine();
    HelpMarker("Delay to resent the first input receives (FIFO queue)");
}

static void
show_external_sources_combo(external_source& srcs,
                            const char* title,
                            source& src)
{
    small_string<63> label("None");

    external_source_type type;
    if (!(external_source_type_cast(src.type, &type))) {
        src.reset();
    } else {
        switch (type) {
        case external_source_type::binary_file: {
            const auto id = enum_cast<binary_file_source_id>(src.id);
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
            const auto id = enum_cast<constant_source_id>(src.id);
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
            const auto id = enum_cast<random_source_id>(src.id);
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
            const auto id = enum_cast<text_file_source_id>(src.id);
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
                const auto id = srcs.constant_sources.get_id(s);
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
                    src.id = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            binary_file_source* s = nullptr;
            while (srcs.binary_file_sources.next(s)) {
                const auto id = srcs.binary_file_sources.get_id(s);
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
                    src.id = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            random_source* s = nullptr;
            while (srcs.random_sources.next(s)) {
                const auto id = srcs.random_sources.get_id(s);
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
                    src.id = ordinal(id);
                    ImGui::EndCombo();
                    return;
                }
            }
        }

        {
            text_file_source* s = nullptr;
            while (srcs.text_file_sources.next(s)) {
                const auto id = srcs.text_file_sources.get_id(s);
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
                    src.id = ordinal(id);
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
    ImGui::InputDouble("offset", &dyn.default_offset);
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
    ImGui::InputDouble("value", &dyn.default_value);
    ImGui::InputDouble("offset", &dyn.default_offset);

    if (ed.is_running()) {
        if (ImGui::Button("Send now")) {
            dyn.value = dyn.default_value;
            dyn.sigma = dyn.default_offset;

            auto& mdl = get_model(dyn);
            mdl.tl = ed.simulation_current;
            mdl.tn = ed.simulation_current + dyn.sigma;
            if (dyn.sigma && mdl.tn == ed.simulation_current)
                mdl.tn = std::nextafter(ed.simulation_current,
                                        ed.simulation_current + 1.);

            ed.sim.sched.update(mdl, mdl.tn);
        }
    }
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_cross& dyn)
{
    ImGui::InputDouble("threshold", &dyn.default_threshold);
    ImGui::Checkbox("up detection", &dyn.default_detect_up);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss1_power& dyn)
{
    ImGui::InputDouble("n", &dyn.default_n);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss2_power& dyn)
{
    ImGui::InputDouble("n", &dyn.default_n);
}

static void
show_dynamics_inputs(editor& /*ed*/, qss3_power& dyn)
{
    ImGui::InputDouble("n", &dyn.default_n);
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
    ImGui::InputDouble("threshold", &dyn.default_threshold);
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
        sim.dispatch(mdl, [&](const auto& dyn) {
            add_input_attribute(*this, dyn);
            ImGui::PushItemWidth(120.0f);
            show_dynamics_values(sim, dyn);
            ImGui::PopItemWidth();
            add_output_attribute(*this, dyn);
        });
    } else {
        sim.dispatch(mdl, [&](auto& dyn) {
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
make_input_tooltip(Dynamics& dyn, std::string& out)
{
    if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
        for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
            if (dyn.x[i].messages.empty())
                continue;

            fmt::format_to(std::back_inserter(out), "x[{}]: ", i);

            for (const auto& msg : dyn.x[i].messages) {
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

        auto ret = ed.sim.dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (is_detected_v<has_input_port_t, Dynamics>)
                return make_input_tooltip(dyn, ed.tooltip);

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
    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].first.index() == 0) {
            const auto id = std::get<model_id>(top.children[i].first);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                if (st != editor_status::editing &&
                    models_make_transition[get_index(id)]) {

                    ImNodes::PushColorStyle(
                      ImNodesCol_TitleBar,
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

                ImNodes::BeginNode(top.children[i].second);
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

                ImGui::Text("%zu\n%s",
                            i,
                            dynamics_type_names[static_cast<int>(mdl->type)]);

                ImNodes::EndNodeTitleBar();
                show_model_dynamics(*mdl);
                ImNodes::EndNode();

                ImNodes::PopColorStyle();
                ImNodes::PopColorStyle();
            }
        } else {
            const auto id = std::get<cluster_id>(top.children[i].first);
            if (auto* gp = clusters.try_to_get(id); gp) {
                ImNodes::PushColorStyle(
                  ImNodesCol_TitleBar,
                  ImGui::ColorConvertFloat4ToU32(settings.gui_cluster_color));
                ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered,
                                        settings.gui_hovered_cluster_color);
                ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected,
                                        settings.gui_selected_cluster_color);

                ImNodes::BeginNode(top.children[i].second);
                ImNodes::BeginNodeTitleBar();
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

                ImNodes::EndNodeTitleBar();
                show_model_cluster(*gp);
                ImNodes::EndNode();

                ImNodes::PopColorStyle();
                ImNodes::PopColorStyle();
            }
        }
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
    if (ImGui::ColorEdit3(
          "cluster", (float*)&gui_cluster_color, ImGuiColorEditFlags_NoOptions))
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
        auto& mdl = ed.sim.alloc(type);
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

        model_id new_model = undefined<model_id>();
        const auto click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
        if (!ImGui::IsAnyItemHovered() && open_popup)
            ImGui::OpenPopup("Context menu");

        if (ImGui::BeginPopup("Context menu")) {

            if (ImGui::BeginMenu("QSS1")) {
                auto i = static_cast<int>(dynamics_type::qss1_integrator);
                const auto e = static_cast<int>(dynamics_type::qss1_wsum_4);
                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS2")) {
                auto i = static_cast<int>(dynamics_type::qss2_integrator);
                const auto e = static_cast<int>(dynamics_type::qss2_wsum_4);

                for (; i != e; ++i)
                    add_popup_menuitem(
                      *this, static_cast<dynamics_type>(i), &new_model);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("QSS3")) {
                auto i = static_cast<int>(dynamics_type::qss3_integrator);
                const auto e = static_cast<int>(dynamics_type::qss3_wsum_4);

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
            parent(new_model, undefined<cluster_id>());
            ImNodes::SetNodeScreenSpacePos(top.emplace_back(new_model),
                                           click_pos);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

        int node_id;
        if (ImNodes::IsNodeHovered(&node_id) && is_running()) {
            const auto index = top.get_index(node_id);
            if (index != not_found || top.children[index].first.index() == 0) {
                const auto id = std::get<model_id>(top.children[index].first);
                if (auto* mdl = sim.models.try_to_get(id); mdl)
                    show_tooltip(*this, *mdl, id);
            }
        } else
            tooltip.clear();

        {
            int start = 0, end = 0;
            if (ImNodes::IsLinkCreated(&start, &end)) {
                const gport out = get_out(start);
                const gport in = get_in(end);

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

        const int num_selected_links = ImNodes::NumSelectedLinks();
        const int num_selected_nodes = ImNodes::NumSelectedNodes();
        static ImVector<int> selected_nodes;
        static ImVector<int> selected_links;

        if (num_selected_nodes > 0) {
            selected_nodes.resize(num_selected_nodes, -1);

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
                ImNodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(7, "%d model(s) to delete\n", num_selected_nodes);
                free_children(selected_nodes);
            } else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('D')) {
                ImNodes::GetSelectedNodes(selected_nodes.begin());
                log_w.log(
                  7, "%d model(s)/group(s) to copy\n", num_selected_nodes);
                copy(selected_nodes);
            } else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('G')) {
                if (num_selected_nodes > 1) {
                    ImNodes::GetSelectedNodes(selected_nodes.begin());
                    log_w.log(7, "%d model(s) to group\n", num_selected_nodes);
                    group(selected_nodes);
                } else if (num_selected_nodes == 1) {
                    ImNodes::GetSelectedNodes(selected_nodes.begin());
                    log_w.log(7, "group to ungroup\n");
                    ungroup(selected_nodes[0]);
                }
            }
            selected_nodes.resize(0);
        } else if (num_selected_links > 0) {
            selected_links.resize(static_cast<size_t>(num_selected_links));

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased('X')) {
                std::fill_n(selected_links.begin(), selected_links.size(), -1);
                ImNodes::GetSelectedLinks(selected_links.begin());
                std::sort(selected_links.begin(),
                          selected_links.end(),
                          std::less<int>());

                int i = 0;
                int link_id_to_delete = selected_links[0];
                int current_link_id = 0;

                log_w.log(
                  7, "%d connection(s) to delete\n", num_selected_links);

                auto selected_links_ptr = selected_links.Data;
                auto selected_links_size = selected_links.Size;

                model* mdl = nullptr;
                while (sim.models.next(mdl) && link_id_to_delete != -1) {
                    sim.dispatch(
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
                              for (sz j = 0, e = std::size(dyn.y); j != e;
                                   ++j) {
                                  for (const auto& elem :
                                       dyn.y[j].connections) {
                                      if (current_link_id ==
                                          link_id_to_delete) {
                                          this->sim.disconnect(
                                            *mdl,
                                            (int)j,
                                            this->sim.models.get(elem.model),
                                            elem.port_index);

                                          ++i;

                                          if (i != selected_links_size)
                                              link_id_to_delete =
                                                selected_links_ptr[i];
                                          else
                                              link_id_to_delete = -1;
                                      }

                                      ++current_link_id;
                                  }
                              }
                          }
                      });
                }
            }

            selected_links.resize(0);
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
                const auto index = top.get_index(selected_nodes[i]);

                if (index == not_found)
                    continue;

                const auto& child = top.children[index];
                if (child.first.index())
                    continue;

                const auto id = std::get<model_id>(child.first);
                model* mdl = sim.models.try_to_get(id);
                if (!mdl)
                    continue;

                if (ImGui::TreeNodeEx(names[i].c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
                    const auto index = get_index(id);
                    auto out = observation_outputs[index];
                    auto old_choose = static_cast<int>(out.index());
                    auto choose = old_choose;

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
                            plot = &tf;
                            tf.ed = this;
                            observation_outputs[index] = plot_outs.get_id(tf);
                            auto& o = sim.observers.alloc(names[i].c_str(), tf);
                            sim.observe(*mdl, o);
                        } else {
                            plot = plot_outs.try_to_get(
                              std::get<plot_output_id>(out));
                            irt_assert(plot);
                        }

                        ImGui::InputText("name##plot",
                                         plot->name.begin(),
                                         plot->name.capacity());
                        ImGui::InputDouble(
                          "dt##plot", &plot->time_step, 0.001, 1.0, "%.8f");
                    } else if (choose == 2) {
                        if (old_choose == 1 || old_choose == 3) {
                            sim.observers.free(mdl->obs_id);
                            observation_outputs_free(index);
                        }

                        file_output* file;
                        if (old_choose != 2) {
                            file_output& tf = file_outs.alloc(names[i].c_str());
                            file = &tf;
                            tf.ed = this;
                            observation_outputs[index] = file_outs.get_id(tf);
                            auto& o = sim.observers.alloc(names[i].c_str(), tf);
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
                            file = &tf;
                            tf.ed = this;
                            observation_outputs[index] =
                              file_discrete_outs.get_id(tf);
                            auto& o = sim.observers.alloc(names[i].c_str(), tf);
                            sim.observe(*mdl, o);
                        } else {
                            file = file_discrete_outs.try_to_get(
                              std::get<file_discrete_output_id>(out));
                            irt_assert(file);
                        }

                        ImGui::InputText("name##filedt",
                                         file->name.begin(),
                                         file->name.capacity());
                        ImGui::InputDouble(
                          "dt##filedt", &file->time_step, 0.001, 1.0, "%.8f");
                    } else if (old_choose != choose) {
                        sim.observers.free(mdl->obs_id);
                        observation_outputs_free(index);
                    }

                    sim.dispatch(*mdl,
                                 [this]<typename Dynamics>(Dynamics& dyn) {
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
                    auto ret = w(sim, srcs);
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

        auto empty_fun = [this](irt::model_id id) {
            this->top.emplace_back(id);
            parent(id, undefined<cluster_id>());
        };

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
        const char* title = "Select file path to load";
        const char8_t* filters[] = { u8".irt", nullptr };

        ImGui::OpenPopup(title);
        if (load_file_dialog(path, title, filters)) {
            show_load_file_dialog = false;
            log_w.log(
              5, "Load file from %s: ", (const char*)path.u8string().c_str());
            if (auto is = std::ifstream(path); is.is_open()) {
                reader r(is);
                auto ret = r(sim, srcs, [&r, this](model_id id) {
                    parent(id, undefined<cluster_id>());

                    const auto index = get_index(id);
                    const auto new_id = top.emplace_back(id);
                    const auto pos = r.get_position(index);

                    ImNodes::SetNodeEditorSpacePos(new_id,
                                                   ImVec2(pos.x, pos.y));
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
            const char* title = "Select file path to save";
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
                          const auto pos = ImNodes::GetNodeEditorSpacePos(
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
