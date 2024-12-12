// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dot-parser.hpp"

#include <boost/ut.hpp>

#include <fmt/color.h>
#include <fmt/format.h>

#include <charconv>

using namespace std::literals;

enum class node_id : irt::u32;
enum class edge_id : irt::u32;

enum class msg_id {
    unknown_attribute,
    missing_comma,
    parse_real,
};

constexpr std::string_view msg_fmt[] = { "unknwon attribute {}",
                                         "missing comma character in `{}'",
                                         "fail to parse `{}' to read a float" };

template<msg_id Index, typename... Args>
static constexpr void warning(Args&&... args) noexcept
{
    constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);

    fmt::vprint(stderr, msg_fmt[idx], fmt::make_format_args(args...));
}

template<msg_id Index, typename Ret, typename... Args>
static constexpr auto error(Ret&& ret, Args&&... args) noexcept -> Ret
{
    constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);

    fmt::vprint(stderr,
                fg(fmt::terminal_color::red),
                msg_fmt[idx],
                fmt::make_format_args(args...));

    return ret;
}

struct dot_graph {
    irt::id_array<node_id, irt::default_allocator> nodes;
    irt::id_array<edge_id, irt::default_allocator> edges;

    irt::vector<std::string_view>     node_names;
    irt::vector<int>                  node_ids;
    irt::vector<std::array<float, 2>> node_positions;
    irt::vector<float>                node_areas;
    irt::vector<node_id[2]>           edges_nodes;

    irt::string_buffer buffer;

    irt::table<std::string_view, node_id> name_to_node_id;

    bool sort_before_search = false;
};

constexpr static auto find_or_add_node(dot_graph&       g,
                                       std::string_view name) noexcept
  -> node_id
{
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
}

constexpr static auto find_node(dot_graph& g, std::string_view name) noexcept
  -> node_id
{
    if (g.sort_before_search)
        g.name_to_node_id.sort();

    const auto* found = g.name_to_node_id.get(name);
    return found ? *found : irt::undefined<node_id>();
}

constexpr static auto to_float_str(std::string_view str) noexcept
  -> std::optional<std::pair<float, std::string_view>>
{
    auto f = 0.f;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
        ret.ec != std::errc{})
        return std::make_pair(f, str.substr(ret.ptr - str.data()));

    return std::nullopt;
}

constexpr static auto to_float(std::string_view str) noexcept -> float
{
    auto f = 0.f;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
        ret.ec != std::errc{})
        return f;

    return 0.f;
}

constexpr static auto to_int(std::string_view str) noexcept -> int
{
    int i = 0;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), i);
        ret.ec != std::errc{})
        return i;

    return 0;
}

constexpr static auto to_2float(std::string_view str) noexcept
  -> std::array<float, 2>
{
    if (const auto first = to_float_str(str); first.has_value()) {
        const auto [first_float, substr] = *first;

        if (not substr.empty() and substr[0] == ',') {
            const auto second = substr.substr(1u, std::string_view::npos);

            if (const auto second_float = to_float(second))
                return std::array<float, 2>{ first_float, second_float };
            else
                warning<msg_id::parse_real>(second);
        } else {
            warning<msg_id::missing_comma>(substr);
        }
    }

    return std::array<float, 2>{};
}

constexpr static auto attach(
  dot_graph&                                               g,
  std::string_view                                         name,
  std::span<std::pair<std::string_view, std::string_view>> attributes) noexcept
  -> bool
{
    const auto id  = find_or_add_node(g, name);
    const auto idx = irt::get_index(id);

    for (const auto& att : attributes) {
        if (att.first == "area")
            g.node_areas[idx] = to_float(att.second);
        else if (att.first == "pos")
            g.node_positions[idx] = to_2float(att.second);
        else if (att.first == "id")
            g.node_ids[idx] = to_int(att.second);
        else
            warning<msg_id::unknown_attribute>(name);
    }

    return true;
}

// auto ret = irt::parse_dot_buffer(
//   buf,
//   [](std::string_view name,
//      std::span<std::pair<std::string_view, std::string_view>>
//        attributes) noexcept -> bool {
//       const auto id = find_or_add(nodes, node_name);

//       attachs(id, attributes);

//       return true;
//   },
//   [](std::string_view from,
//      std::string_view to,
//      std::span<std::pair<std::string_view, std::string_view>>
//        attributes) noexcept -> bool {

//   });

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

    "small-and-simple-with-attributes"_test = [] {
        const std::string_view buf = R"(digraph D {
            A [pos="1,2"]
            B [pos="3,4"]
            C [pos="5,6"]
            A -> B
            A -- C
            A -> D
        })";

        dot_graph g;

        // auto ret = irt::parse_dot_buffer(graph, buf);
        // expect(!!ret >> fatal);

        expect(eq(g.nodes.size(), 3));
        expect(eq(g.edges.size(), 3));
        expect(eq(g.name_to_node_id.size(), 3));

        expect(irt::is_defined(find_node(g, "A")));
        expect(irt::is_defined(find_node(g, "B")));
        expect(irt::is_defined(find_node(g, "C")));
        const auto id_A  = find_node(g, "A");
        const auto id_B  = find_node(g, "B");
        const auto id_C  = find_node(g, "C");
        const auto idx_A = irt::get_index(id_A);
        const auto idx_B = irt::get_index(id_B);
        const auto idx_C = irt::get_index(id_C);

        expect(eq(g.node_names[idx_A], "A"sv));
        expect(eq(g.node_names[idx_B], "B"sv));
        expect(eq(g.node_names[idx_C], "C"sv));
    };
}
