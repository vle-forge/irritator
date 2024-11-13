// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dot-parser.hpp"

#include <boost/ut.hpp>

enum class node_id;
enum class edge_id;

struct graph {
    irt::id_array<node_id, irt::default_allocator> nodes;
    irt::vector<std::string_view>                  node_names;
    irt::vector<int>                               node_ids;
    irt::vector<float[2]>                          node_positions;
    irt::vector<float>                             node_areas;

    irt::id_array<edge_id, irt::default_allocator> edges;
    irt::vector<node_id[2]>                        edges_nodes;

    irt::string_buffer buffer;

    irt::table<std::string_view, node_id> name_to_node_id;
    bool                                  sort_before_search = false;
};

auto find_or_add = [](graph& g, std::string_view name) noexcept -> node_id {
    if (g.sort_before_search)
        g.name_to_node_id.sort();

    if (auto* found = g.name_to_node_id.get(name); found)
        return *found;

    const auto id  = g.nodes.alloc();
    const auto idx = irt::get_index(id);

    g.node_names[idx]     = g.buffer.append(name);
    g.node_ids[idx]       = 0;
    g.node_positions[idx] = { 0.f, 0.f };
    g.node_areas[idx]     = 0.f;
    g.name_to_node_id.data.emplace_back(g.node_names[idx], id);

    return id;
};

auto to_float(std::string_view str) noexcept -> float
{
    auto f = 0.f;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
        ret.ec != std::errc{})
        return f;

    return 0.f;
}

auto to_2float(std::string_view str) noexcept -> std::pair<float, float>
{
    float f[2] = { 0.f, 0.f };
    auto ret = std::from_chars(str.data(), str.data() + str.size(), f[0]);
    if (ret.ec != std::errc{})
        return { 0.f, 0.f };

    const auto length = ret.ptr - str.data();
    const auto substr = str.substr(length)

    if (auto ret = std::from_chars(ret.ptr, )

    return 0.f;
}

auto attach = [](graph&           g,
                 std::string_view name,
                 std::span<std::pair<std::string_view, std::string_view>>
                   attributes) noexcept -> bool {
    const auto id  = find_or_add(g, name);
    const auto idx = irt::get_index(id);

    for (const auto& att : attributes) {
        if (att.first == "area")
            g.node_areas[idx] = irt::to_float(att.second);
        else if (att.first == "pos")
            g.node_positions[idx] = irt::to_2float(att.second);
        else if (att.first == "id")
            g.node_ids[idx] = irt::to_int(att.second);
    }
};

auto ret = irt::parse_dot_buffer(
  buf,
  [](std::string_view name,
     std::span<std::pair<std::string_view, std::string_view>>
       attributes) noexcept -> bool {
      const auto id = find_or_add(nodes, node_name);

      attachs(id, attributes);

      return true;
  },
  [](std::string_view from,
     std::string_view to,
     std::span<std::pair<std::string_view, std::string_view>>
       attributes) noexcept -> bool {

  });

int main()
{
    using namespace boost::ut;

#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    "small-and-simple-0"_test = [] {
        const std::string_view buf = R"(digraph D {
            A
            B
            C
            A->B
            A--C
            A->D
        })";

        irt::graph_component graph;

        auto ret = irt::parse_dot_buffer(graph, buf);

        expect(!!ret >> fatal);
        expect(eq(graph.children.ssize(), 4));
        expect(eq(graph.edges.ssize(), 4));
    };

    "small-and-simple-with-space-0"_test = [] {
        const std::string_view buf = R"(digraph D {
            A
            B
            C
            A -> B
            A -- C
            A -> D
        })";

        irt::graph_component graph;

        auto ret = irt::parse_dot_buffer(graph, buf);

        expect(!!ret >> fatal);
        expect(eq(graph.children.ssize(), 4));
        expect(eq(graph.edges.ssize(), 4));
    };
}
