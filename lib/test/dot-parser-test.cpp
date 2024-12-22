// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dot-parser.hpp"

#include <boost/ut.hpp>

#include <charconv>

using namespace std::literals;

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
            A--B
            A--C
            A->D
        })";

        auto ret = irt::parse_dot_buffer(buf);
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

        expect(table.get("A") >> fatal);
        expect(table.get("B") >> fatal);
        expect(table.get("C") >> fatal);
        const auto id_A  = *table.get("A");
        const auto id_B  = *table.get("B");
        const auto id_C  = *table.get("C");
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
}
