// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dot-parser.hpp"

#include <boost/ut.hpp>

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
