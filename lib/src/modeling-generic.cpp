// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <optional>
#include <utility>

#include <cstdint>

namespace irt {

#if 0
static bool is_ports_compatible(modeling&          mod,
                                model&             mdl_src,
                                int                port_src,
                                generic_component& compo_dst,
                                port_id            port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_dst.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_dst) {
            auto* sub_child_dst = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_dst);
            irt_assert(sub_child_dst->type == child_type::model);

            auto  sub_model_dst_id = sub_child_dst->id.mdl_id;
            auto* sub_model_dst    = mod.models.try_to_get(sub_model_dst_id);

            if (!is_ports_compatible(mdl_src,
                                     port_src,
                                     *sub_model_dst,
                                     con->output.index_src.model)) {
                is_compatible = false;
                break;
            }
        }
    }

    return is_compatible;
}

static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                port_id            port_src,
                                model&             mdl_dst,
                                int                port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::input &&
            con->input.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->input.dst);
            irt_assert(sub_child_src);
            if (sub_child_src->type == child_type::model) {
                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(*sub_model_src,
                                         con->input.index_dst.model,
                                         mdl_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}
#endif

#if 0
static bool is_ports_compatible(modeling&          mod,
                                generic_component& compo_src,
                                port_id            port_src,
                                generic_component& compo_dst,
                                port_id            port_dst) noexcept
{
    bool is_compatible = true;

    for (auto connection_id : compo_src.connections) {
        connection* con = mod.connections.try_to_get(connection_id);
        if (!con)
            continue;

        if (con->type == connection::connection_type::output &&
            con->output.index == port_src) {
            auto* sub_child_src = mod.children.try_to_get(con->output.src);
            irt_assert(sub_child_src);

            if (sub_child_src->type == child_type::model) {
                auto  sub_model_src_id = sub_child_src->id.mdl_id;
                auto* sub_model_src = mod.models.try_to_get(sub_model_src_id);
                irt_assert(sub_model_src);

                if (!is_ports_compatible(mod,
                                         *sub_model_src,
                                         con->output.index_src.model,
                                         compo_dst,
                                         port_dst)) {
                    is_compatible = false;
                    break;
                }
            } // @TODO Test also component coupling
        }
    }

    return is_compatible;
}
#endif

// static bool is_ports_compatible(modeling& mod,
//                                 generic_component& /*parent*/,
//                                 child_id src,
//                                 i8       port_src,
//                                 child_id dst,
//                                 i8       port_dst) noexcept
//{
//     auto* child_src = mod.children.try_to_get(src);
//     auto* child_dst = mod.children.try_to_get(dst);
//     irt_assert(child_src);
//     irt_assert(child_dst);
//
//     if (child_src->type == child_type::model) {
//         auto  mdl_src_id = child_src->id.mdl_id;
//         auto* mdl_src    = mod.models.try_to_get(mdl_src_id);
//
//         if (child_dst->type == child_type::model) {
//             auto  mdl_dst_id = child_dst->id.mdl_id;
//             auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
//             return is_ports_compatible(*mdl_src, port_src, *mdl_dst,
//             port_dst);
//
//         } else {
//             auto  compo_dst_id = child_dst->id.compo_id;
//             auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
//             irt_assert(compo_dst);
//             auto* s_compo_dst =
//               mod.generic_components.try_to_get(compo_dst->id.generic_id);
//             irt_assert(s_compo_dst);
//
//             return is_ports_compatible(
//               mod, *mdl_src, port_src, *s_compo_dst, port_dst);
//         }
//     } else {
//         auto  compo_src_id = child_src->id.compo_id;
//         auto* compo_src    = mod.components.try_to_get(compo_src_id);
//         irt_assert(compo_src);
//         auto* s_compo_src =
//           mod.generic_components.try_to_get(compo_src->id.generic_id);
//         irt_assert(s_compo_src);
//
//         if (child_dst->type == child_type::model) {
//             auto  mdl_dst_id = child_dst->id.mdl_id;
//             auto* mdl_dst    = mod.models.try_to_get(mdl_dst_id);
//             irt_assert(mdl_dst);
//
//             return is_ports_compatible(
//               mod, *s_compo_src, port_src, *mdl_dst, port_dst);
//         } else {
//             auto  compo_dst_id = child_dst->id.compo_id;
//             auto* compo_dst    = mod.components.try_to_get(compo_dst_id);
//             irt_assert(compo_dst);
//             auto* s_compo_dst =
//               mod.generic_components.try_to_get(compo_dst->id.generic_id);
//             irt_assert(s_compo_dst);
//
//             return is_ports_compatible(
//               mod, *s_compo_src, port_src, *s_compo_dst, port_dst);
//         }
//     }
// }

static bool check_connection_already_exists(
  const irt::modeling&               mod,
  const irt::generic_component&      gen,
  const irt::connection::internal_t& con) noexcept
{
    for (auto id : gen.connections) {
        const auto* c = mod.connections.try_to_get(id);
        if (!c || c->type != connection::connection_type::internal)
            continue;

        const auto* src = mod.children.try_to_get(c->internal.src);
        const auto* dst = mod.children.try_to_get(c->internal.dst);
        if (!src || !dst)
            continue;

        if (src->type == child_type::component) {
            if (dst->type == child_type::component) {
                if (c->internal.src == con.src && c->internal.dst == con.dst &&
                    c->internal.index_src.compo == con.index_src.compo &&
                    c->internal.index_dst.compo == con.index_dst.compo)
                    return true;
            } else {
                if (c->internal.src == con.src && c->internal.dst == con.dst &&
                    c->internal.index_src.compo == con.index_src.compo &&
                    c->internal.index_dst.model == con.index_dst.model)
                    return true;
            }
        } else {
            if (dst->type == child_type::component) {
                if (c->internal.src == con.src && c->internal.dst == con.dst &&
                    c->internal.index_src.model == con.index_src.model &&
                    c->internal.index_dst.compo == con.index_dst.compo)
                    return true;
            } else {
                if (c->internal.src == con.src && c->internal.dst == con.dst &&
                    c->internal.index_src.model == con.index_src.model &&
                    c->internal.index_dst.model == con.index_dst.model)
                    return true;
            }
        }
    }

    return false;
}

static bool check_connection_already_exists(
  const irt::modeling&            mod,
  const irt::generic_component&   gen,
  const irt::connection::input_t& con) noexcept
{
    for (auto id : gen.connections) {
        const auto* c = mod.connections.try_to_get(id);
        if (!c || c->type != connection::connection_type::input)
            continue;

        const auto* dst = mod.children.try_to_get(c->input.dst);
        if (!dst)
            continue;

        if (dst->type == child_type::component) {
            if (con.dst == c->input.dst &&
                con.index_dst.compo == c->input.index_dst.compo &&
                con.index == c->input.index)
                return true;
        } else {
            if (con.dst == c->input.dst &&
                con.index_dst.model == c->input.index_dst.model &&
                con.index == c->input.index)
                return true;
        }
    }

    return false;
}

static bool check_connection_already_exists(
  const irt::modeling&             mod,
  const irt::generic_component&    gen,
  const irt::connection::output_t& con) noexcept
{
    for (auto id : gen.connections) {
        const auto* c = mod.connections.try_to_get(id);
        if (!c || c->type != connection::connection_type::output)
            continue;

        const auto* src = mod.children.try_to_get(c->output.src);
        if (!src)
            continue;

        if (src->type == child_type::component) {
            if (con.src == c->output.src &&
                con.index_src.compo == c->output.index_src.compo &&
                con.index == c->output.index)
                return true;
        } else {
            if (con.src == c->output.src &&
                con.index_src.model == c->output.index_src.model &&
                con.index == c->output.index)
                return true;
        }
    }

    return false;
}

status modeling::connect_input(generic_component& parent,
                               port&              x,
                               child&             c,
                               connection::port   p_c) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    irt_return_if_fail(
      !check_connection_already_exists(
        *this,
        parent,
        connection::input_t{ children.get_id(c), ports.get_id(x), p_c }),
      status::model_connect_already_exist);

    const auto c_id = children.get_id(c);
    const auto x_id = ports.get_id(x);

    if (c.type == child_type::component) {
        irt_assert(ports.try_to_get(p_c.compo) != nullptr);

        auto& con    = connections.alloc(x_id, c_id, p_c.compo);
        auto  con_id = connections.get_id(con);
        parent.connections.emplace_back(con_id);
    } else {
        auto& con    = connections.alloc(x_id, c_id, p_c.model);
        auto  con_id = connections.get_id(con);
        parent.connections.emplace_back(con_id);
    }

    return status::success;
}

status modeling::connect_output(generic_component& parent,
                                child&             c,
                                connection::port   p_c,
                                port&              y) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    irt_return_if_fail(
      !check_connection_already_exists(
        *this,
        parent,
        connection::output_t{ children.get_id(c), ports.get_id(y), p_c }),
      status::model_connect_already_exist);

    const auto c_id = children.get_id(c);
    const auto y_id = ports.get_id(y);

    if (c.type == child_type::component) {
        irt_assert(ports.try_to_get(p_c.compo) != nullptr);

        auto& con    = connections.alloc(c_id, p_c.compo, y_id);
        auto  con_id = connections.get_id(con);
        parent.connections.emplace_back(con_id);
    } else {
        auto& con    = connections.alloc(c_id, p_c.model, y_id);
        auto  con_id = connections.get_id(con);
        parent.connections.emplace_back(con_id);
    }

    return status::success;
}

status modeling::connect(generic_component& parent,
                         child&             src,
                         connection::port   y,
                         child&             dst,
                         connection::port   x) noexcept
{
    irt_return_if_fail(connections.can_alloc(),
                       status::simulation_not_enough_connection);

    irt_return_if_fail(!check_connection_already_exists(
                         *this,
                         parent,
                         connection::internal_t{
                           children.get_id(src), children.get_id(dst), y, x }),
                       status::model_connect_already_exist);

    const auto src_id = children.get_id(src);
    const auto dst_id = children.get_id(dst);

    if (src.type == child_type::component) {
        irt_assert(ports.try_to_get(y.compo) != nullptr);

        if (dst.type == child_type::component) {
            irt_assert(ports.try_to_get(x.compo) != nullptr);

            auto& con    = connections.alloc(src_id, y.compo, dst_id, x.compo);
            auto  con_id = connections.get_id(con);
            parent.connections.emplace_back(con_id);
        } else {
            auto& con    = connections.alloc(src_id, y.compo, dst_id, x.model);
            auto  con_id = connections.get_id(con);
            parent.connections.emplace_back(con_id);
        }
    } else {
        irt_assert(ports.try_to_get(y.compo) == nullptr);

        if (dst.type == child_type::component) {
            irt_assert(ports.try_to_get(x.compo) != nullptr);

            auto& con    = connections.alloc(src_id, y.model, dst_id, x.compo);
            auto  con_id = connections.get_id(con);
            parent.connections.emplace_back(con_id);
        } else {
            auto& con    = connections.alloc(src_id, y.model, dst_id, x.model);
            auto  con_id = connections.get_id(con);
            parent.connections.emplace_back(con_id);
        }
    }

    return status::success;
}

static status modeling_connect(modeling&          mod,
                               generic_component& gen,
                               child_id           src,
                               connection::port   p_src,
                               child_id           dst,
                               connection::port   p_dst) noexcept
{
    status ret = status::unknown_dynamics;

    if_data_exists_do(mod.children, src, [&](auto& child_src) noexcept {
        if_data_exists_do(mod.children, dst, [&](auto& child_dst) noexcept {
            if (child_src.type == child_type::component) {
                if (child_dst.type == child_type::component) {
                    ret = mod.connect(
                      gen, child_src, p_src.compo, child_dst, p_dst.compo);
                } else {
                    ret = mod.connect(
                      gen, child_src, p_src.compo, child_dst, p_dst.model);
                }
            } else {
                if (child_dst.type == child_type::component) {
                    ret = mod.connect(
                      gen, child_src, p_src.model, child_dst, p_dst.compo);
                } else {
                    ret = mod.connect(
                      gen, child_src, p_src.model, child_dst, p_src.model);
                }
            }
        });
    });

    return ret;
}

status modeling::copy(const generic_component& src,
                      generic_component&       dst) noexcept
{
    table<child_id, child_id> mapping; // @TODO move this mapping variable into
                                       // the modeling or cache class.

    for_specified_data(children, src.children, [&](auto& c) noexcept {
        const auto src_id  = children.get_id(c);
        const auto src_idx = get_index(src_id);

        if (c.type == child_type::model) {
            auto&      new_child     = alloc(dst, c.id.mdl_type);
            const auto new_child_id  = children.get_id(new_child);
            const auto new_child_idx = get_index(new_child_id);

            children_names[new_child_idx]     = children_names[src_idx];
            children_positions[new_child_idx] = {
                .x = children_positions[src_idx].x,
                .y = children_positions[src_idx].y
            };
            children_parameters[new_child_idx] = children_parameters[src_idx];

            mapping.data.emplace_back(src_id, new_child_id);
        } else {
            const auto compo_id = c.id.compo_id;

            if (auto* compo = components.try_to_get(compo_id); compo) {
                auto& new_child     = alloc(dst, compo_id);
                auto  new_child_id  = children.get_id(new_child);
                auto  new_child_idx = get_index(new_child_id);

                children_names[new_child_idx]     = children_names[src_idx];
                children_positions[new_child_idx] = {
                    .x = children_positions[src_idx].x,
                    .y = children_positions[src_idx].y
                };
                children_parameters[new_child_idx] =
                  children_parameters[src_idx];

                mapping.data.emplace_back(src_id, new_child_id);
            }
        }
    });

    mapping.sort();

    for_specified_data(connections, src.connections, [&](auto& con) noexcept {
        if (con.type == connection::connection_type::internal) {
            if (auto* child_src = mapping.get(con.internal.src); child_src) {
                if (auto* child_dst = mapping.get(con.internal.dst);
                    child_dst) {
                    irt_return_if_bad(modeling_connect(*this,
                                                       dst,
                                                       *child_src,
                                                       con.internal.index_src,
                                                       *child_dst,
                                                       con.internal.index_dst));
                }
            }
        }

        return status::success;
    });

    return status::success;
}

} // namespace irt
