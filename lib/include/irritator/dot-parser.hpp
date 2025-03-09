// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER
#define ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

namespace irt {

struct dot_graph {
    enum class errc : i16 {
        buffer_empty,
        memory_insufficient,
        file_unreachable,
        format_illegible
    };

    id_array<graph_node_id> nodes;
    id_array<graph_edge_id> edges;

    vector<std::string_view>             node_names;
    vector<std::string_view>             node_ids;
    vector<std::array<float, 2>>         node_positions;
    vector<component_id>                 node_components;
    vector<float>                        node_areas;
    vector<std::array<graph_node_id, 2>> edges_nodes;

    string_buffer buffer;

    std::string main_id;

    bool is_strict  = false;
    bool is_graph   = false;
    bool is_digraph = false;

    /**
     * @brief Build a @c irt::table from node name to node identifier.
     * This function use the @c node_names and @c nodes object, do not change
     * this object after building a toc.
     * @return A string-to-node table.
     */
    table<std::string_view, graph_node_id> make_toc() const noexcept;
};

expected<dot_graph> parse_dot_buffer(const modeling&  mod,
                                     std::string_view buffer) noexcept;

expected<dot_graph> parse_dot_buffer(std::string_view buffer) noexcept;

expected<dot_graph> parse_dot_file(const modeling&              mod,
                                   const std::filesystem::path& p) noexcept;

/**
 * @brief Write the @a graph into a text based file.
 * @param mod Use to get the file and directory for all component.
 * @param graph dot-graph to write.
 * @return @a vector<char> or error_code if error.
 */
error_code write_dot_file(const modeling&              mod,
                          const dot_graph&             graph,
                          const std::filesystem::path& path) noexcept;

/**
 * @brief Write the @a graph into a text based vector.
 * @param mod Use to get the file and directory for all component.
 * @param graph dot-graph to write.
 * @return @a vector<char> or error_code if error.
 */
expected<vector<char>> write_dot_buffer(const modeling&  mod,
                                        const dot_graph& graph) noexcept;

} // irt

#endif
