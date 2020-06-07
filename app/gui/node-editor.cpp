// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "node-editor.hpp"
#include "gui.hpp"
#include "imnodes.hpp"

#include <fstream>
#include <string>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/io.hpp>

namespace irt {

template<size_t N, typename... Args>
void
format(small_string<N>& str, const char* fmt, const Args&... args)
{
    auto ret = fmt::format_to_n(str.begin(), N, fmt, args...);
    str.size(ret.size);
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
    if (output->observation_type == observation_output::type::plot ||
        output->observation_type == observation_output::type::both) {
        std::fill_n(output->data.data(), output->data.size(), 0.0f);
        output->tl = t;
        output->min = -1.f;
        output->max = +1.f;
        output->id = 0;
    }

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {

        if (!output->ofs.is_open()) {
            if (output->observation_type == observation_output::type::both)
                output->observation_type = observation_output::type::plot;
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
    const auto value = static_cast<float>(msg.cast_to_real_64(0));

    if (output->observation_type == observation_output::type::plot ||
        output->observation_type == observation_output::type::both) {
        output->min = std::min(output->min, value);
        output->max = std::max(output->max, value);

        for (double to_fill = output->tl; to_fill < t; to_fill += obs.time_step)
            if (static_cast<size_t>(output->id) < output->data.size())
                output->data[output->id++] = value;
    }

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {
        output->ofs << t << ',' << value << '\n';
    }

    output->tl = t;
}

static void
observation_output_free(const irt::observer& obs, const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {
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
                  irt::status_string[static_cast<int>(ret)]);
        st = simulation_status::internal_error;
        return;
    }

    do {
        if (auto ret = sim.run(current); irt::is_bad(ret)) {
            log_w.log(3,
                      "Simulation failure (%s)\n",
                      irt::status_string[static_cast<int>(ret)]);

            st = simulation_status::internal_error;
            return;
        }
    } while (current < end && !stop);

    sim.clean();

    st = simulation_status::success;
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

void
editor::reorder() noexcept
{
    constexpr size_t zero{ 0 };
    const auto view_pos = imnodes::EditorContextGetPanning();

    reorder_subgroup(zero, top.children.size(), view_pos);
}

static bool
is_in_hierarchy(const data_array<cluster, cluster_id>& clusters,
                const cluster& group,
                const cluster_id group_to_search) noexcept
{
    log_w.log(7,
              "Tries is_in_hierarchy %s %s\n",
              group.name.c_str(),
              clusters.get(group_to_search).name.c_str());

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
                if (is_in_hierarchy(clusters, *gp, group_to_search)) {
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
            new_cluster.children.push_back(top.children[index]);
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
        if (child.index() == 0) {
            const auto child_id = std::get<model_id>(child);

            if (auto* model = sim.models.try_to_get(child_id); model) {
                sim.for_all_input_port(
                  *model, [this, &new_cluster](const input_port& port) {
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.output_ports.try_to_get(id);
                              p)
                              if (is_in_hierarchy(this->clusters,
                                                  new_cluster,
                                                  this->parent(p->model)))
                                  new_cluster.output_ports.emplace_back(id);
                      }
                  });

                sim.for_all_output_port(
                  *model, [this, &new_cluster](const output_port& port) {
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.input_ports.try_to_get(id); p)
                              if (is_in_hierarchy(this->clusters,
                                                  new_cluster,
                                                  this->parent(p->model)))
                                  new_cluster.input_ports.emplace_back(id);
                      }
                  });
            }
        } else {
            const auto child_id = std::get<cluster_id>(child);

            if (auto* group = clusters.try_to_get(child_id); group) {
                for (const auto id : group->input_ports) {
                    if (auto* p = sim.input_ports.try_to_get(id); p) {
                        for (const auto d_id : p->connections) {
                            if (auto* d_p = sim.output_ports.try_to_get(d_id);
                                d_p) {
                                if (is_in_hierarchy(this->clusters,
                                                    new_cluster,
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
                                if (is_in_hierarchy(this->clusters,
                                                    new_cluster,
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

    if (top.children[index].index() == 0) {
        log_w.log(5, "node is not a group\n");
        return;
    }

    auto* group =
      clusters.try_to_get(std::get<cluster_id>(top.children[index]));
    if (!group) {
        log_w.log(5, "group does not exist\n");
        return;
    }

    const auto group_id = clusters.get_id(*group);

    /* Remove the group to ungroup from the top cluster using the swap
       idiom and affect the old position. */

    top.pop(index);

    for (size_t i = 0, e = group->children.size(); i != e; ++i) {
        if (group->children[i].index() == 0) {
            const auto id = std::get<model_id>(group->children[i]);
            if (auto* mdl = sim.models.try_to_get(id)) {
                parent(id, undefined<cluster_id>());
                top.emplace_back(group->children[i]);
            }
        } else {
            auto id = std::get<cluster_id>(group->children[i]);
            if (auto* gp = clusters.try_to_get(id)) {
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

        if (child.index() == 0) {
            const auto id = std::get<model_id>(child);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                models_mapper[get_index(id)] = undefined<cluster_id>();
                log_w.log(7, "delete %s\n", mdl->name.c_str());
                parent(id, undefined<cluster_id>());
                sim.deallocate(id);
            }
        } else {
            const auto id = std::get<cluster_id>(child);
            if (auto* gp = clusters.try_to_get(id); gp) {
                clusters_mapper[get_index(id)] = undefined<cluster_id>();
                log_w.log(7, "delete group %s\n", gp->name.c_str());
                free_group(*gp);
            }
        }

        top.pop(index);
    }
}

void
editor::copy_group(const child_id* sources,
                   const size_t size,
                   child_id* destination) noexcept
{
    /* copy all models with the sim.copy() but keep hierarchy... */
}

void
editor::reorder_subgroup(const size_t from,
                         const size_t length,
                         ImVec2 click_pos) noexcept
{
    const auto tmp = std::sqrt(static_cast<double>(length));
    const auto column = static_cast<int>(tmp);
    auto line = column;
    auto remaining = static_cast<int>(length) - (column * line);

    while (remaining > column) {
        ++line;
        remaining -= column;
    }

    log_w.log(7, "reorder with: %d * %d + %d\n", column, line, remaining);

    const auto x_distance = 250.0f;
    const auto y_distance = 250.0f;
    auto elem = static_cast<int>(from);
    ImVec2 new_pos = click_pos;

    for (int i = 0; i < column; ++i) {
        new_pos.y = click_pos.y + i * y_distance;
        for (int j = 0; j < line; ++j) {
            new_pos.x = click_pos.x + j * x_distance;
            imnodes::SetNodeScreenSpacePos(elem++, new_pos);
        }
    }

    new_pos.x = click_pos.x;
    new_pos.y = click_pos.y + column * y_distance;
    for (int j = 0; j < remaining; ++j) {
        new_pos.x = click_pos.x + j * x_distance;
        imnodes::SetNodeScreenSpacePos(elem++, new_pos);
    }
}

status
editor::initialize(u32 id) noexcept
{
    if (is_bad(sim.init(1024u, 32768u)) ||
        is_bad(observation_outputs.init(sim.models.capacity())) ||
        is_bad(observation_types.init(sim.models.capacity())) ||
        is_bad(clusters.init(sim.models.capacity())) ||
        is_bad(models_mapper.init(sim.models.capacity())) ||
        is_bad(clusters_mapper.init(sim.models.capacity())))
        return status::gui_not_enough_memory;

    auto& gp = clusters.alloc();
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

void
editor::show_connections() noexcept
{
    int connection_id = 0;

    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].index() == 0) {
            const auto id = std::get<model_id>(top.children[i]);
            if (const auto* mdl = sim.models.try_to_get(id); mdl) {

                sim.for_all_input_port(
                  *mdl, [this, &connection_id](const input_port& port) {
                      const auto i_port_id = this->sim.input_ports.get_id(port);
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.output_ports.try_to_get(id);
                              p)
                              imnodes::Link(connection_id++,
                                            get_out(id),
                                            get_in(i_port_id));
                      }
                  });

                sim.for_all_output_port(
                  *mdl, [this, &connection_id](const output_port& port) {
                      const auto o_port_id =
                        this->sim.output_ports.get_id(port);
                      for (const auto id : port.connections) {
                          if (auto* p = this->sim.input_ports.try_to_get(id); p)
                              imnodes::Link(connection_id++,
                                            get_out(o_port_id),
                                            get_in(id));
                      }
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

void
editor::show_model_dynamics(model& mdl) noexcept
{
    ImGui::PushItemWidth(100.0f);

    {
        const char* items[] = { "none", "plot", "file", "both" };
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
                  current_item == 1
                    ? observation_output::type::plot
                    : current_item == 2 ? observation_output::type::file
                                        : observation_output::type::both;
            }

            if (auto* o = sim.observers.try_to_get(mdl.obs_id); o) {
                float v = static_cast<float>(o->time_step);
                if (ImGui::InputFloat("freq.", &v, 0.001f, 0.1f, "%.3f", 0))
                    o->time_step = static_cast<double>(v);
            }
        }
    }

    ImGui::PopItemWidth();

    if (simulation_show_value &&
        match(st, simulation_status::success, simulation_status::running)) {
        switch (mdl.type) {
        case dynamics_type::none: /* none does not have input port. */
            break;
        case dynamics_type::integrator: {
            auto& dyn = sim.integrator_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("quanta");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("reset");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("value %.3f", dyn.current_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("x").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("quanta").x - text_width);
            ImGui::TextUnformatted("x");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::quantifier: {
            auto& dyn = sim.quantifier_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("up threshold %.3f", dyn.m_upthreshold);
            ImGui::Text("down threshold %.3f", dyn.m_downthreshold);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            ImGui::TextUnformatted("quanta");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_2: {
            auto& dyn = sim.adder_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_3: {
            auto& dyn = sim.adder_3_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_4: {
            auto& dyn = sim.adder_4_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("x3");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
            ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_2: {
            auto& dyn = sim.mult_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_3: {
            auto& dyn = sim.mult_3_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_4: {
            auto& dyn = sim.mult_4_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("x3");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("%.3f * %.3f", dyn.values[0], dyn.input_coeffs[0]);
            ImGui::Text("%.3f * %.3f", dyn.values[1], dyn.input_coeffs[1]);
            ImGui::Text("%.3f * %.3f", dyn.values[2], dyn.input_coeffs[2]);
            ImGui::Text("%.3f * %.3f", dyn.values[3], dyn.input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::counter: {
            auto& dyn = sim.counter_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("in");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("number %ld", static_cast<long>(dyn.number));
            ImGui::PopItemWidth();

        } break;
        case dynamics_type::generator: {
            auto& dyn = sim.generator_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("next %.3f", dyn.sigma);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::constant: {
            auto& dyn = sim.constant_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("value %.3f", dyn.value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::cross: {
            auto& dyn = sim.cross_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("if_value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("else");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("value: %.3f", dyn.value);
            ImGui::Text("if-value: %.3f", dyn.if_value);
            ImGui::Text("else-value: %.3f", dyn.else_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::accumulator_2: {
            auto& dyn = sim.accumulator_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("number-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("number-2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("acc-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("acc-2");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("number %.3f", dyn.number);
            ImGui::Text("- 0: %.3f", dyn.numbers[0]);
            ImGui::Text("- 1: %.3f", dyn.numbers[1]);
            ImGui::PopItemWidth();
        } break;
        case dynamics_type::time_func: {
            auto& dyn = sim.time_func_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::Text("value %.3f", dyn.value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        }
    } else {
        switch (mdl.type) {
        case dynamics_type::none: /* none does not have input port. */
            break;
        case dynamics_type::integrator: {
            auto& dyn = sim.integrator_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("quanta");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("reset");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_current_value);
            ImGui::InputDouble("reset", &dyn.default_reset_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("x").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("quanta").x - text_width);
            ImGui::TextUnformatted("x");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::quantifier: {
            auto& dyn = sim.quantifier_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("quantum", &dyn.default_step_size);
            ImGui::SliderInt(
              "archive length", &dyn.default_past_length, 3, 100);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            ImGui::TextUnformatted("quanta");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_2: {
            auto& dyn = sim.adder_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_3: {
            auto& dyn = sim.adder_3_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::adder_4: {
            auto& dyn = sim.adder_4_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("x3");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("sum");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_2: {
            auto& dyn = sim.mult_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_3: {
            auto& dyn = sim.mult_3_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::mult_4: {
            auto& dyn = sim.mult_4_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x0");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("x2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("x3");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::counter: {
            auto& dyn = sim.counter_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("in");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::generator: {
            auto& dyn = sim.generator_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_value);
            ImGui::InputDouble("period", &dyn.default_period);
            ImGui::InputDouble("offset", &dyn.default_offset);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::constant: {
            auto& dyn = sim.constant_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::cross: {
            auto& dyn = sim.cross_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("if_value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("else");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("threshold", &dyn.default_threshold);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::accumulator_2: {
            auto& dyn = sim.accumulator_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("number-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("number-2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("acc-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("acc-2");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::time_func: {
            auto& dyn = sim.time_func_models.get(mdl.id);
            const char* items[] = { "time", "square" };
            ImGui::PushItemWidth(120.0f);
            int item_current = dyn.default_f == &time_function ? 0 : 1;
            if (ImGui::Combo(
                  "function", &item_current, items, IM_ARRAYSIZE(items))) {
                dyn.default_f =
                  item_current == 0 ? &time_function : square_time_function;
            }
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        }
    }
}

void
editor::show_top() noexcept
{
    for (size_t i = 0, e = top.children.size(); i != e; ++i) {
        if (top.children[i].index() == 0) {
            const auto id = std::get<model_id>(top.children[i]);
            if (auto* mdl = sim.models.try_to_get(id); mdl) {
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBar,
                                        IM_COL32(70, 70, 140, 255));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarHovered,
                                        IM_COL32(85, 85, 155, 255));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarSelected,
                                        IM_COL32(100, 100, 170, 255));

                imnodes::BeginNode(top.nodes[i]);
                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(mdl->name.c_str());
                imnodes::EndNodeTitleBar();
                show_model_dynamics(*mdl);
                imnodes::EndNode();

                imnodes::PopColorStyle();
                imnodes::PopColorStyle();
            }
        } else {
            const auto id = std::get<cluster_id>(top.children[i]);
            if (auto* gp = clusters.try_to_get(id); gp) {
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBar,
                                        IM_COL32(70, 140, 70, 255));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarHovered,
                                        IM_COL32(85, 155, 85, 255));
                imnodes::PushColorStyle(imnodes::ColorStyle_TitleBarSelected,
                                        IM_COL32(100, 170, 100, 255));

                imnodes::BeginNode(top.nodes[i]);
                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(gp->name.c_str());

                if (ImGui::BeginPopupContextItem("Rename")) {
                    ImGui::Text("Edit name:");
                    ImGui::InputText(
                      "##edit", gp->name.begin(), gp->name.capacity());
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

bool
editor::show_editor() noexcept
{
    imnodes::EditorContextSet(context);
    static bool show_load_file_dialog = false;
    static bool show_save_file_dialog = false;

    ImGuiWindowFlags windows_flags = 0;
    windows_flags |= ImGuiWindowFlags_MenuBar;

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
                reorder();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Examples")) {
            if (ImGui::MenuItem("Insert Lotka Volterra model")) {
                if (auto ret = add_lotka_volterra(); is_bad(ret))
                    log_w.log(
                      3,
                      "Fail to initialize a Lotka Volterra model (%s)\n",
                      status_string[static_cast<int>(ret)]);
            }

            if (ImGui::MenuItem("Insert Izhikevitch model")) {
                if (auto ret = add_izhikevitch(); is_bad(ret))
                    log_w.log(3,
                              "Fail to initialize an Izhikevitch model (%s)\n",
                              status_string[static_cast<int>(ret)]);
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

        if (ImGui::MenuItem("none")) {
            if (sim.none_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.none_models.alloc();
                sim.alloc(mdl, sim.none_models.get_id(mdl), "none");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("integrator")) {
            if (sim.integrator_models.can_alloc(1u) &&
                sim.models.can_alloc(1u)) {
                auto& mdl = sim.integrator_models.alloc();
                sim.alloc(mdl, sim.integrator_models.get_id(mdl), "integrator");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("quantifier")) {
            if (sim.quantifier_models.can_alloc(1u) &&
                sim.models.can_alloc(1u)) {
                auto& mdl = sim.quantifier_models.alloc();
                sim.alloc(mdl, sim.quantifier_models.get_id(mdl), "quantifier");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("adder_2")) {
            if (sim.adder_2_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.adder_2_models.alloc();
                sim.alloc(mdl, sim.adder_2_models.get_id(mdl), "adder");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("adder_3")) {
            if (sim.adder_3_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.adder_3_models.alloc();
                sim.alloc(mdl, sim.adder_3_models.get_id(mdl), "adder");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("adder_4")) {
            if (sim.adder_4_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.adder_4_models.alloc();
                sim.alloc(mdl, sim.adder_4_models.get_id(mdl), "adder");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("mult_2")) {
            if (sim.mult_2_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.mult_2_models.alloc();
                sim.alloc(mdl, sim.mult_2_models.get_id(mdl), "mult");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("mult_3")) {
            if (sim.mult_3_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.mult_3_models.alloc();
                sim.alloc(mdl, sim.mult_3_models.get_id(mdl), "mult");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("mult_4")) {
            if (sim.mult_4_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.mult_4_models.alloc();
                sim.alloc(mdl, sim.mult_4_models.get_id(mdl), "mult");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("counter")) {
            if (sim.counter_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.counter_models.alloc();
                sim.alloc(mdl, sim.counter_models.get_id(mdl), "counter");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("generator")) {
            if (sim.generator_models.can_alloc(1u) &&
                sim.models.can_alloc(1u)) {
                auto& mdl = sim.generator_models.alloc();
                sim.alloc(mdl, sim.generator_models.get_id(mdl), "generator");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("constant")) {
            if (sim.constant_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.constant_models.alloc();
                sim.alloc(mdl, sim.constant_models.get_id(mdl), "constant");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("cross")) {
            if (sim.cross_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                auto& mdl = sim.cross_models.alloc();
                sim.alloc(mdl, sim.cross_models.get_id(mdl), "cross");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("accumulator-2")) {
            if (sim.accumulator_2_models.can_alloc(1u) &&
                sim.models.can_alloc(1u)) {
                auto& mdl = sim.accumulator_2_models.alloc();
                sim.alloc(mdl, sim.accumulator_2_models.get_id(mdl), "acc-2");
                new_model = mdl.id;
            }
        }

        if (ImGui::MenuItem("time_func")) {
            if (sim.time_func_models.can_alloc(1u) &&
                sim.models.can_alloc(1u)) {
                auto& mdl = sim.time_func_models.alloc();
                sim.alloc(mdl, sim.time_func_models.get_id(mdl), "time-func");
                new_model = mdl.id;
            }
        }

        ImGui::EndPopup();
        if (new_model != undefined<model_id>()) {
            parent(new_model, undefined<cluster_id>());
            top.emplace_back(new_model);
            imnodes::SetNodeScreenSpacePos(top.nodes.back(), click_pos);
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
                sim.connect(out, in);
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
            static std::vector<child_id> sources;
            static std::vector<child_id> destinations;

            try {
                //sources.resize(num_selected_nodes);
                //destinations.resize(num_selected_nodes);

                //auto& top = clusters.get(top_group);
                //for (size_t i = 0; i != selected_nodes.size(); ++i)
                //    sources[i] = top.children[selected_nodes[i]];

                //copy_group(sources.data(), sources.size(), destinations.data());
                //imnodes::ClearSelectedNodesAndLinks();
            } catch (const std::bad_alloc&) {
                log_w.log(4,
                          "Fail to allocate copy buffer (%ul)",
                          static_cast<unsigned long>(selected_nodes.size()));
            }
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
        log_w.log(2,
                  "Fail to initialize irritator: %s\n",
                  status_string[static_cast<int>(ret)]);
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

void
show_simulation_box(bool* show_simulation)
{
    static editor_id current_editor_id = static_cast<editor_id>(0);

    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_Always);
    if (!ImGui::Begin("Simulation", show_simulation)) {
        ImGui::End();
        return;
    }

    const char* combo_name = "";
    if (auto* ed = editors.try_to_get(current_editor_id); !ed) {
        ed = nullptr;
        if (editors.next(ed)) {
            current_editor_id = editors.get_id(ed);
            combo_name = ed->name.c_str();
        }
    } else {
        combo_name = ed->name.c_str();
    }

    if (ImGui::BeginCombo("Name", combo_name)) {
        editor* ed = nullptr;
        while (editors.next(ed)) {
            bool is_selected = current_editor_id == editors.get_id(ed);
            if (ImGui::Selectable(ed->name.c_str(), is_selected))
                current_editor_id = editors.get_id(ed);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (auto* ed = editors.try_to_get(current_editor_id); ed) {
        ImGui::InputDouble("Begin", &ed->simulation_begin);
        ImGui::InputDouble("End", &ed->simulation_end);
        ImGui::Checkbox("Show values", &ed->simulation_show_value);

        if (ed->st != simulation_status::running) {
            if (ed->simulation_thread.joinable()) {
                ed->simulation_thread.join();
                ed->st = simulation_status::success;
            }

            if (ImGui::Button("Start")) {
                ed->observation_outputs.clear();
                observer* obs = nullptr;
                while (ed->sim.observers.next(obs)) {
                    auto* output =
                      ed->observation_outputs.emplace_back(obs->name.c_str());

                    const auto type = ed->observation_types[get_index(
                      ed->sim.observers.get_id(*obs))];
                    const auto diff = ed->simulation_end - ed->simulation_begin;
                    const auto freq = diff / obs->time_step;
                    const auto length = static_cast<size_t>(freq);
                    output->observation_type = type;

                    if (type == observation_output::type::plot ||
                        type == observation_output::type::both) {
                        output->data.init(length);
                    }

                    if (!obs->name.empty()) {
                        const std::filesystem::path obs_file_path =
                          ed->observation_directory / obs->name.c_str();

                        if (type == observation_output::type::file ||
                            type == observation_output::type::both) {
                            if (output->ofs.open(obs_file_path);
                                !output->ofs.is_open())
                                log_w.log(
                                  4,
                                  "Fail to open observation file: %s in %s\n",
                                  obs->name.c_str(),
                                  ed->observation_directory.u8string().c_str());
                        }
                    }

                    obs->initialize = &observation_output_initialize;
                    obs->observe = &observation_output_observe;
                    obs->free = &observation_output_free;
                    obs->user_data = static_cast<void*>(output);
                }

                ed->st = simulation_status::running;
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

        if (ed->st == simulation_status::success ||
            ed->st == simulation_status::running) {
            ImGui::Text("Current: %g", ed->simulation_current);

            if (ed->st == simulation_status::running && ImGui::Button("Stop"))
                ed->stop = true;

            const double duration = ed->simulation_end - ed->simulation_begin;
            const double elapsed =
              ed->simulation_current - ed->simulation_begin;
            const double fraction = elapsed / duration;
            ImGui::ProgressBar(static_cast<float>(fraction));

            for (const auto& obs : ed->observation_outputs) {
                if (obs.observation_type == observation_output::type::plot ||
                    obs.observation_type == observation_output::type::both)
                    ImGui::PlotLines(obs.name,
                                     obs.data.data(),
                                     static_cast<int>(obs.data.size()),
                                     0,
                                     nullptr,
                                     obs.min,
                                     obs.max,
                                     ImVec2(0.f, 50.f));

                if (obs.observation_type == observation_output::type::file ||
                    obs.observation_type == observation_output::type::both)
                    ImGui::Text("%s: output file", obs.name);
            }
        }
    }

    ImGui::End();
}

void
node_editor_initialize()
{
    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize irritator: %s\n",
                  irt::status_string[static_cast<int>(ret)]);
    } else {
        if (auto* ed = editors_new(); ed)
            ed->context = imnodes::EditorContextCreate();
    }
}

bool
node_editor_show()
{
    static bool show_log = true;
    static bool show_simulation = true;
    static bool show_demo_window = false;
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
            ImGui::MenuItem("Log", nullptr, &show_log);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo_window);

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

    if (show_demo_window)
        ImGui::ShowDemoWindow();

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
