// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/dot-parser.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

using namespace std::literals;

int main()
{
    using namespace boost::ut;
    using namespace std::literals::string_view_literals;

#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    "small-and-simple-0"_test = [] {
        const std::string_view buf = R"(digraph D {
            A
            B
            C
            A--B
            A--C
            A->D
        })";

        irt::journal_handler jn{};
        auto ret = irt::parse_dot_buffer(irt::modeling{ jn }, buf);
        expect(ret.has_value() >> fatal);
        expect(eq(ret->nodes.ssize(), 4));
        expect(eq(ret->edges.ssize(), 3));
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

        auto ret = irt::parse_dot_buffer(buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.ssize(), 4));
        expect(eq(ret->edges.ssize(), 3));
    };

    "small-and-simple-with-attributes"_test = [] {
        const std::string_view buf = R"(digraph D {
            A [pos="1,2";pos="7,8"]
            B [pos="3,4"]
            C [pos="5,6"]
            A -> B
            A -- C
            A -> D
        })";

        auto ret = irt::parse_dot_buffer(buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.size(), 4u));

        const auto table = ret->make_toc();
        expect(eq(table.ssize(), 4));

        expect(table.get("A"sv) >> fatal);
        expect(table.get("B"sv) >> fatal);
        expect(table.get("C"sv) >> fatal);
        const auto id_A  = *table.get("A"sv);
        const auto id_B  = *table.get("B"sv);
        const auto id_C  = *table.get("C"sv);
        const auto idx_A = irt::get_index(id_A);
        const auto idx_B = irt::get_index(id_B);
        const auto idx_C = irt::get_index(id_C);

        expect(eq(ret->node_names[idx_A], "A"sv));
        expect(eq(ret->node_names[idx_B], "B"sv));
        expect(eq(ret->node_names[idx_C], "C"sv));

        expect(eq(ret->node_positions[idx_A][0], 7.0f));
        expect(eq(ret->node_positions[idx_A][1], 8.0f));
        expect(eq(ret->node_positions[idx_B][0], 3.0f));
        expect(eq(ret->node_positions[idx_B][1], 4.0f));
        expect(eq(ret->node_positions[idx_C][0], 5.0f));
        expect(eq(ret->node_positions[idx_C][1], 6.0f));
    };

    "id-area-pos-node-attribute"_test = [] {
        const std::string_view buf = R"(graph Voronoi
        {
            1 [id=1, Area=123, pos="-1,-2"];
            1 -- 2;
            2 [id=2, Area=321, pos="-3,-4"];
        })";

        auto ret = irt::parse_dot_buffer(buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.size(), 2u));
        expect(eq(ret->edges.size(), 1u));

        const auto table = ret->make_toc();
        expect(eq(table.ssize(), 2));

        expect(table.get("1"sv) >> fatal);
        expect(table.get("2"sv) >> fatal);

        const auto id_1  = *table.get("1"sv);
        const auto id_2  = *table.get("2"sv);
        const auto idx_1 = irt::get_index(id_1);
        const auto idx_2 = irt::get_index(id_2);

        expect(eq(ret->node_names[idx_1], "1"sv));
        expect(eq(ret->node_names[idx_2], "2"sv));

        expect(eq(ret->node_positions[idx_1][0], -1.0f));
        expect(eq(ret->node_positions[idx_1][1], -2.0f));
        expect(eq(ret->node_positions[idx_2][0], -3.0f));
        expect(eq(ret->node_positions[idx_2][1], -4.0f));

        expect(eq(ret->node_areas[idx_1], 123.0f));
        expect(eq(ret->node_areas[idx_2], 321.0f));
    };

    "write-load-write-load"_test = [] {
        const std::string_view buf = R"(graph Voronoi
        {
            1 [ID = 1, Area = 123, pos = "-1,-2"] ;
            1 -- 2;
            2 [ID = 2, Area = 321, pos = "-3,-4"];
        })";

        auto check_buffer = [](const irt::graph& g) noexcept {
            expect(eq(g.nodes.size(), 2u));
            expect(eq(g.edges.size(), 1u));

            const auto table = g.make_toc();
            expect(eq(table.ssize(), 2));

            expect(table.get("1"sv) >> fatal);
            expect(table.get("2"sv) >> fatal);

            const auto id_1  = *table.get("1"sv);
            const auto id_2  = *table.get("2"sv);
            const auto idx_1 = irt::get_index(id_1);
            const auto idx_2 = irt::get_index(id_2);

            expect(eq(g.node_names[idx_1], "1"sv));
            expect(eq(g.node_names[idx_2], "2"sv));

            expect(eq(g.node_positions[idx_1][0], -1.0f));
            expect(eq(g.node_positions[idx_1][1], -2.0f));
            expect(eq(g.node_positions[idx_2][0], -3.0f));
            expect(eq(g.node_positions[idx_2][1], -4.0f));

            expect(eq(g.node_areas[idx_1], 123.0f));
            expect(eq(g.node_areas[idx_2], 321.0f));
        };

        auto ret = irt::parse_dot_buffer(buf);
        check_buffer(*ret);
        check_buffer(*ret);

        irt::journal_handler jn{};
        irt::modeling        mod{ jn };

        auto save_buf = irt::write_dot_buffer(mod, *ret);
        expect(save_buf.has_value() >> fatal);

        auto load_buf = irt::parse_dot_buffer(
          std::string_view(save_buf->data(), save_buf->size()));

        expect(load_buf.has_value() >> fatal);
        check_buffer(*load_buf);
    };
}
