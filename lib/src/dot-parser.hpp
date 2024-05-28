// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER
#define ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#include <forward_list>

namespace irt {

class string_buffer
{
public:
    constexpr static inline std::size_t string_buffer_node_length = 1024 * 1024;

    string_buffer() noexcept = default;

    string_buffer(const string_buffer&) noexcept            = delete;
    string_buffer& operator=(const string_buffer&) noexcept = delete;
    string_buffer(string_buffer&&) noexcept                 = delete;
    string_buffer& operator=(string_buffer&&) noexcept      = delete;

    //! Appends a `std::string_view` into the buffer and returns a new
    //! `std::string_view` to this new chunck of characters. If necessary, a new
    //! `value_type` is allocated to storage large number of strings.
    //!
    //! @param str A `std::string_view` to copy into the buffer. `str` must be
    //! greater than `0` and lower than `string_buffer_node_length`.
    std::string_view append(std::string_view str) noexcept;

    //! Computes and returns the number of `value_type` allocated.
    std::size_t size() const noexcept;

private:
    using value_type     = std::array<char, string_buffer_node_length>;
    using container_type = std::forward_list<value_type>;

    //! Alloc a new `value_type` buffer in front of the last allocated buffer.
    void do_alloc() noexcept;

    container_type m_container;
    std::size_t    m_position = { 0 };
};

class dot_parser
{
public:
    struct memory_error {}; //! Report allocation memory error.
    struct file_error {};   //! Report an error during read the file.
    struct syntax_error {}; //! Report an error in the file format.

    struct read_main_id_error {};
    struct read_graph_or_digraph_error {};
    struct missing_curly_brace_error {};
    struct read_edgeop_error {};
    struct read_id_error {};

    enum class graph { graph, digraph };

    enum class edgeop {
        directed,
        undirected,
    };

    struct node {
        node() noexcept = default;

        node(std::string_view name_, graph_component::vertex_id id_) noexcept
          : name(name_)
          , id(id_)
        {}

        std::string_view           name;
        graph_component::vertex_id id =
          undefined<typename graph_component::vertex_id>();
    };

    struct edge {
        int src;
        int dst;
    };

    string_buffer    buffer;
    std::string_view id;
    graph            type = graph::graph;

    vector<node> nodes;
    vector<edge> edges;

    auto search_node_linear(std::string_view name) const noexcept
      -> std::optional<graph_component::vertex_id>;

    auto search_node(std::string_view name) const noexcept
      -> std::optional<graph_component::vertex_id>;

    void sort() noexcept;
};

status parse_dot_file(graph_component& graph) noexcept;

status parse_dot_buffer(graph_component&       graph,
                        const std::string_view buffer) noexcept;

} // irt

#endif