// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER
#define ORG_VLEPROJECT_IRRITATOR_2024_SRC_DOT_PARSER

#include <irritator/modeling.hpp>

namespace irt {

expected<graph> parse_dot_buffer(const modeling&        mod,
                                 const std::string_view buffer) noexcept;

expected<graph> parse_dot_file(const modeling&              mod,
                               const std::filesystem::path& p) noexcept;

/**
 * @brief Write the @a graph into a text based file.
 * @param mod Use to get the file and directory for all component.
 * @param graph dot-graph to write.
 * @return @a vector<char> or error_code if error.
 */
expected<void> write_dot_file(const modeling&              mod,
                              const graph&                 graph,
                              const std::filesystem::path& path) noexcept;

/**
 * @brief Write the @a graph into a text based vector.
 * @param mod Use to get the file and directory for all component.
 * @param graph dot-graph to write.
 * @return @a vector<char> or error_code if error.
 */
expected<vector<char>> write_dot_buffer(const modeling& mod,
                                        const graph&    graph) noexcept;

} // irt

#endif
