// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

namespace irt {

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
    input_connections.reserve(connection_limit.value());
    output_connections.reserve(connection_limit.value());

    children_positions.resize(child_limit.value());
    children_names.resize(child_limit.value());
    children_parameters.resize(child_limit.value());
}

expected<child_id> generic_component::copy_to(
  const child&       c,
  generic_component& dst) const noexcept
{
    // debug::ensure(children.is_in_array(c));

    const auto src_id  = children.get_id(c);
    const auto src_idx = get_index(src_id);

    if (not dst.children.can_alloc())
        return new_error(modeling_errc::generic_children_container_full);

    auto&      new_c     = dst.children.alloc();
    const auto new_c_id  = dst.children.get_id(new_c);
    const auto new_c_idx = get_index(new_c_id);

    new_c.type = c.type;
    new_c.id   = c.id;

    debug::ensure(std::cmp_less(new_c_idx, dst.children_names.size()));
    debug::ensure(std::cmp_less(new_c_idx, dst.children_positions.size()));
    debug::ensure(std::cmp_less(new_c_idx, dst.children_parameters.size()));

    dst.children_names[new_c_idx] =
      dst.exists_child(children_names[src_idx].sv())
        ? dst.make_unique_name_id(new_c_id)
        : children_names[src_idx];

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
        if (auto* child_src = mapping.get(con.src); child_src) {
            if (auto* child_dst = mapping.get(con.dst); child_dst) {
                dst.connections.alloc(
                  *child_src, con.index_src, *child_dst, con.index_dst);
            }
        }
    }

    return success();
}

bool generic_component::exists_input_connection(
  const port_id          x,
  const child&           dst,
  const connection::port port) const noexcept
{
    for (const auto& con : input_connections)
        if (con.x == x and con.dst == children.get_id(dst) and con.port == port)
            return true;

    return false;
}

bool generic_component::exists_output_connection(
  const port_id          y,
  const child&           src,
  const connection::port port) const noexcept
{
    for (const auto& con : output_connections)
        if (con.y == y and con.src == children.get_id(src) and con.port == port)
            return true;

    return false;
}

bool generic_component::exists(const child&           src,
                               const connection::port p_src,
                               const child&           dst,
                               const connection::port p_dst) const noexcept
{
    const auto src_id = children.get_id(src);
    const auto dst_id = children.get_id(dst);

    for (const auto& con : connections)
        if (con.src == src_id and con.dst == dst_id and
            con.index_src == p_src and con.index_dst == p_dst)
            return true;

    return false;
}

status generic_component::connect([[maybe_unused]] const modeling& mod,
                                  const child&                     src,
                                  const connection::port           p_src,
                                  const child&                     dst,
                                  const connection::port p_dst) noexcept
{
    if (exists(src, p_src, dst, p_dst))
        return new_error(modeling_errc::generic_connection_already_exist);

    if (src.type == child_type::model) {
        if (dst.type == child_type::model) {
            if (not is_ports_compatible(
                  src.id.mdl_type, p_src.model, dst.id.mdl_type, p_dst.model))
                return new_error(
                  modeling_errc::generic_connection_compatibility_error);
        }
    }

    if (not connections.can_alloc(1))
        return new_error(modeling_errc::generic_connection_container_full);

    connections.alloc(children.get_id(src), p_src, children.get_id(dst), p_dst);

    return success();
}

status generic_component::connect_input(const port_id          x,
                                        const child&           dst,
                                        const connection::port port) noexcept
{
    if (not input_connections.can_alloc(1))
        return new_error(
          modeling_errc::generic_input_connection_container_full);

    for (const auto& con : input_connections)
        if (con.x == x and con.dst == children.get_id(dst) and con.port == port)
            return new_error(
              modeling_errc::generic_input_connection_container_already_exist);

    input_connections.alloc(x, children.get_id(dst), port);

    return success();
}

status generic_component::connect_output(const port_id          y,
                                         const child&           src,
                                         const connection::port port) noexcept
{
    if (not output_connections.can_alloc(1))
        return new_error(
          modeling_errc::generic_output_connection_container_full);

    for (const auto& con : output_connections)
        if (con.y == y and con.src == children.get_id(src) and con.port == port)
            return new_error(
              modeling_errc::generic_output_connection_container_already_exist);
    output_connections.alloc(y, children.get_id(src), port);

    return success();
}

status generic_component::import(
  const data_array<child, child_id>&           children,
  const data_array<connection, connection_id>& connections,
  const std::span<position>                    positions,
  const std::span<name_str>                    names,
  const std::span<parameter>                   parameters) noexcept
{
    table<child_id, child_id> src_to_this;

    if (not this->children.can_alloc(children.size())) {
        this->children.reserve(children.size());
        if (not this->children.can_alloc(children.size()))
            return new_error(modeling_errc::generic_children_container_full);
    }

    for (const auto& c : children) {
        auto& new_c = this->children.alloc();
        new_c.type  = c.type;
        new_c.id    = c.id;

        src_to_this.data.emplace_back(children.get_id(c),
                                      this->children.get_id(new_c));
    }

    src_to_this.sort();

    for (const auto& con : connections) {
        if (auto* child_src = src_to_this.get(con.src); child_src) {
            if (auto* child_dst = src_to_this.get(con.dst); child_dst) {
                this->connections.alloc(
                  *child_src, con.index_src, *child_dst, con.index_dst);
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

                children_names[dst_idx] = exists_child(names[src_idx].sv())
                                            ? make_unique_name_id(pair.value)
                                            : names[src_idx];
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

bool generic_component::exists_child(const std::string_view str) const noexcept
{
    for (const auto& c : children)
        if (children_names[children.get_id(c)].sv() == str)
            return true;

    return false;
}

name_str generic_component::make_unique_name_id(
  const child_id from_id) const noexcept
{
    const auto idx = get_index(from_id);

    name_str ret;

    for (auto i = idx; i < std::numeric_limits<decltype(idx)>::max(); ++i) {
        format(ret, "child-{}", i, idx);
        if (not exists_child(ret.sv()))
            break;
    }

    return ret;
}

} // namespace irt
