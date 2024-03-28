// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/container.hpp"
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
  const irt::generic_component&      gen,
  const irt::connection::internal_t& con) noexcept
{
    for (const auto c : gen.connections) {
        if (c.type != connection::connection_type::internal)
            continue;

        const auto* src = gen.children.try_to_get(c.internal.src);
        const auto* dst = gen.children.try_to_get(c.internal.dst);
        if (!src || !dst)
            continue;

        if (src->type == child_type::component) {
            if (dst->type == child_type::component) {
                if (c.internal.src == con.src && c.internal.dst == con.dst &&
                    c.internal.index_src.compo == con.index_src.compo &&
                    c.internal.index_dst.compo == con.index_dst.compo)
                    return true;
            } else {
                if (c.internal.src == con.src && c.internal.dst == con.dst &&
                    c.internal.index_src.compo == con.index_src.compo &&
                    c.internal.index_dst.model == con.index_dst.model)
                    return true;
            }
        } else {
            if (dst->type == child_type::component) {
                if (c.internal.src == con.src && c.internal.dst == con.dst &&
                    c.internal.index_src.model == con.index_src.model &&
                    c.internal.index_dst.compo == con.index_dst.compo)
                    return true;
            } else {
                if (c.internal.src == con.src && c.internal.dst == con.dst &&
                    c.internal.index_src.model == con.index_src.model &&
                    c.internal.index_dst.model == con.index_dst.model)
                    return true;
            }
        }
    }

    return false;
}

static bool check_connection_already_exists(
  const irt::generic_component&   gen,
  const irt::connection::input_t& con) noexcept
{
    for (const auto& c : gen.connections) {
        if (c.type != connection::connection_type::input)
            continue;

        const auto* dst = gen.children.try_to_get(c.input.dst);
        if (!dst)
            continue;

        if (dst->type == child_type::component) {
            if (con.dst == c.input.dst &&
                con.index_dst.compo == c.input.index_dst.compo &&
                con.index == c.input.index)
                return true;
        } else {
            if (con.dst == c.input.dst &&
                con.index_dst.model == c.input.index_dst.model &&
                con.index == c.input.index)
                return true;
        }
    }

    return false;
}

static bool check_connection_already_exists(
  const irt::generic_component&    gen,
  const irt::connection::output_t& con) noexcept
{
    for (const auto& c : gen.connections) {
        if (c.type != connection::connection_type::output)
            continue;

        const auto* src = gen.children.try_to_get(c.output.src);
        if (!src)
            continue;

        if (src->type == child_type::component) {
            if (con.src == c.output.src &&
                con.index_src.compo == c.output.index_src.compo &&
                con.index == c.output.index)
                return true;
        } else {
            if (con.src == c.output.src &&
                con.index_src.model == c.output.index_src.model &&
                con.index == c.output.index)
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
    if (not parent.connections.can_alloc())
        return new_error(connection_error{}, container_full_error{});

    if (check_connection_already_exists(
          parent,
          connection::input_t{
            parent.children.get_id(c), ports.get_id(x), p_c }))
        return new_error(connection_error{}, already_exist_error{});

    const auto c_id = parent.children.get_id(c);
    const auto x_id = ports.get_id(x);

    if (c.type == child_type::component) {
        debug::ensure(ports.try_to_get(p_c.compo) != nullptr);
        parent.connections.alloc(x_id, c_id, p_c.compo);
    } else {
        parent.connections.alloc(x_id, c_id, p_c.model);
    }

    return success();
}

status modeling::connect_output(generic_component& parent,
                                child&             c,
                                connection::port   p_c,
                                port&              y) noexcept
{
    if (not parent.connections.can_alloc())
        return new_error(connection_error{}, container_full_error{});

    if (check_connection_already_exists(
          parent,
          connection::output_t{
            parent.children.get_id(c), ports.get_id(y), p_c }))
        return new_error(connection_error{}, already_exist_error{});

    const auto c_id = parent.children.get_id(c);
    const auto y_id = ports.get_id(y);

    if (c.type == child_type::component) {
        debug::ensure(ports.try_to_get(p_c.compo) != nullptr);
        parent.connections.alloc(c_id, p_c.compo, y_id);
    } else {
        parent.connections.alloc(c_id, p_c.model, y_id);
    }

    return success();
}

status modeling::connect(generic_component& parent,
                         child&             src,
                         connection::port   y,
                         child&             dst,
                         connection::port   x) noexcept
{
    if (!parent.connections.can_alloc())
        return new_error(connection_error{}, container_full_error{});

    if (check_connection_already_exists(
          parent,
          connection::internal_t{
            parent.children.get_id(src), parent.children.get_id(dst), y, x }))
        return new_error(connection_error{}, already_exist_error{});

    const auto src_id = parent.children.get_id(src);
    const auto dst_id = parent.children.get_id(dst);

    if (src.type == child_type::component) {
        debug::ensure(ports.try_to_get(y.compo) != nullptr);

        if (dst.type == child_type::component) {
            debug::ensure(ports.try_to_get(x.compo) != nullptr);
            parent.connections.alloc(src_id, y.compo, dst_id, x.compo);
        } else {
            parent.connections.alloc(src_id, y.compo, dst_id, x.model);
        }
    } else {
        if (dst.type == child_type::component) {
            debug::ensure(ports.try_to_get(x.compo) != nullptr);
            parent.connections.alloc(src_id, y.model, dst_id, x.compo);
        } else {
            parent.connections.alloc(src_id, y.model, dst_id, x.model);
        }
    }

    return success();
}

static status modeling_connect(modeling&          mod,
                               generic_component& gen,
                               child_id           src,
                               connection::port   p_src,
                               child_id           dst,
                               connection::port   p_dst) noexcept
{
    if (auto* child_src = gen.children.try_to_get(src); child_src) {
        if (auto* child_dst = gen.children.try_to_get(dst); child_dst) {
            if (child_src->type == child_type::component) {
                if (child_dst->type == child_type::component) {
                    return mod.connect(
                      gen, *child_src, p_src.compo, *child_dst, p_dst.compo);
                } else {
                    return mod.connect(
                      gen, *child_src, p_src.compo, *child_dst, p_dst.model);
                }
            } else {
                if (child_dst->type == child_type::component) {
                    return mod.connect(
                      gen, *child_src, p_src.model, *child_dst, p_dst.compo);
                } else {
                    return mod.connect(
                      gen, *child_src, p_src.model, *child_dst, p_src.model);
                }
            }
        }
    }

    return success();
}

static status modeling_connect(generic_component& parent,
                               child&             src,
                               connection::port   y,
                               child&             dst,
                               connection::port   x) noexcept
{
    if (!parent.connections.can_alloc())
        return new_error(modeling::connection_error{}, container_full_error{});

    if (check_connection_already_exists(
          parent,
          connection::internal_t{
            parent.children.get_id(src), parent.children.get_id(dst), y, x }))
        return new_error(modeling::connection_error{}, already_exist_error{});

    const auto src_id = parent.children.get_id(src);
    const auto dst_id = parent.children.get_id(dst);

    if (src.type == child_type::component) {
        if (dst.type == child_type::component) {
            parent.connections.alloc(src_id, y.compo, dst_id, x.compo);
        } else {
            parent.connections.alloc(src_id, y.compo, dst_id, x.model);
        }
    } else {
        if (dst.type == child_type::component) {
            parent.connections.alloc(src_id, y.model, dst_id, x.compo);
        } else {
            parent.connections.alloc(src_id, y.model, dst_id, x.model);
        }
    }

    return success();
}

static status modeling_connect(generic_component& gen,
                               child_id           src,
                               connection::port   p_src,
                               child_id           dst,
                               connection::port   p_dst) noexcept
{
    if (auto* child_src = gen.children.try_to_get(src); child_src) {
        if (auto* child_dst = gen.children.try_to_get(dst); child_dst) {
            if (child_src->type == child_type::component) {
                if (child_dst->type == child_type::component) {
                    return modeling_connect(
                      gen, *child_src, p_src.compo, *child_dst, p_dst.compo);
                } else {
                    return modeling_connect(
                      gen, *child_src, p_src.compo, *child_dst, p_dst.model);
                }
            } else {
                if (child_dst->type == child_type::component) {
                    return modeling_connect(
                      gen, *child_src, p_src.model, *child_dst, p_dst.compo);
                } else {
                    return modeling_connect(
                      gen, *child_src, p_src.model, *child_dst, p_src.model);
                }
            }
        }
    }

    return success();
}

generic_component::generic_component() noexcept
  : generic_component(child_limiter::lower_bound(),
                      connection_limiter::lower_bound())
{}

generic_component::generic_component(
  const child_limiter      child_limit,
  const connection_limiter connection_limit) noexcept
{
    children.reserve(child_limit.value());
    connections.reserve(connection_limit.value());

    children_positions.resize(child_limit.value());
    children_names.resize(child_limit.value());
    children_parameters.resize(child_limit.value());
}

result<child_id> generic_component::copy_to(
  const child&       c,
  generic_component& dst) const noexcept
{
    // debug::ensure(children.is_in_array(c));

    const auto src_id  = children.get_id(c);
    const auto src_idx = get_index(src_id);

    if (not dst.children.can_alloc())
        return new_error(modeling::part::children);

    auto&      new_c     = dst.children.alloc();
    const auto new_c_id  = dst.children.get_id(new_c);
    const auto new_c_idx = get_index(new_c_id);

    new_c.type = c.type;
    new_c.id   = c.id;
    if (c.unique_id)
        new_c.unique_id = dst.make_next_unique_id();

    debug::ensure(std::cmp_less(new_c_idx, dst.children_names.size()));
    debug::ensure(std::cmp_less(new_c_idx, dst.children_positions.size()));
    debug::ensure(std::cmp_less(new_c_idx, dst.children_parameters.size()));

    dst.children_names[new_c_idx]      = children_names[src_idx];
    dst.children_positions[new_c_idx]  = children_positions[new_c_idx];
    dst.children_parameters[new_c_idx] = children_parameters[src_idx];

    return new_c_id;
}

status modeling::copy(const generic_component& src,
                      generic_component&       dst) noexcept
{
    table<child_id, child_id> mapping; // @TODO move this mapping variable into
                                       // the modeling or cache class.

    for (const auto& c : src.children) {
        if (auto ret = src.copy_to(c, dst); ret.has_value()) {
            const auto c_id = src.children.get_id(c);
            mapping.data.emplace_back(c_id, *ret);
        }
    }

    mapping.sort();

    for (const auto& con : src.connections) {
        if (auto* child_src = mapping.get(con.internal.src); child_src) {
            if (auto* child_dst = mapping.get(con.internal.dst); child_dst) {
                irt_check(modeling_connect(*this,
                                           dst,
                                           *child_src,
                                           con.internal.index_src,
                                           *child_dst,
                                           con.internal.index_dst));
            }
        }
    }

    return success();
}

status generic_component::import(
  const data_array<child, child_id>&           children,
  const data_array<connection, connection_id>& connections,
  const std::span<child_position>              positions,
  const std::span<name_str>                    names,
  const std::span<parameter>                   parameters) noexcept
{
    table<child_id, child_id> src_to_this;

    for (const auto& c : children) {
        if (not this->children.can_alloc())
            return new_error(modeling::part::children);

        auto& new_c     = this->children.alloc();
        new_c.type      = c.type;
        new_c.id        = c.id;
        new_c.unique_id = c.unique_id ? this->make_next_unique_id() : 0;

        src_to_this.data.emplace_back(children.get_id(c),
                                      this->children.get_id(new_c));
    }

    src_to_this.sort();

    for (const auto& con : connections) {
        if (auto* child_src = src_to_this.get(con.internal.src); child_src) {
            if (auto* child_dst = src_to_this.get(con.internal.dst);
                child_dst) {

                irt_check(modeling_connect(*this,
                                           *child_src,
                                           con.internal.index_src,
                                           *child_dst,
                                           con.internal.index_dst));
            }
        }
    }

    if (children_positions.size() <= positions.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = this->children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(this->children.get_id(*dst));

                children_positions[dst_idx] = positions[src_idx];
            }
        }
    }

    if (children_names.size() <= names.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = this->children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(this->children.get_id(*dst));

                children_names[dst_idx] = names[src_idx];
            }
        }
    }

    if (children_parameters.size() < parameters.size()) {
        for (const auto pair : src_to_this.data) {
            const auto* src = children.try_to_get(pair.id);
            const auto* dst = this->children.try_to_get(pair.value);

            if (src and dst) {
                const auto src_idx = get_index(children.get_id(*src));
                const auto dst_idx = get_index(this->children.get_id(*dst));

                children_parameters[dst_idx] = parameters[src_idx];
            }
        }
    }

    return success();
}

} // namespace irt
