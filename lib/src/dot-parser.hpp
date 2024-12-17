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

    string_buffer(string_buffer&&) noexcept            = default;
    string_buffer& operator=(string_buffer&&) noexcept = default;

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

struct dot_graph {
    enum class node_id : irt::u32;
    enum class edge_id : irt::u32;

    id_array<node_id, default_allocator> nodes;
    id_array<edge_id, default_allocator> edges;

    vector<std::string_view>       node_names;
    vector<int>                    node_ids;
    vector<std::array<float, 2>>   node_positions;
    vector<float>                  node_areas;
    vector<std::array<node_id, 2>> edges_nodes;

    string_buffer buffer;

    std::string main_id;

    bool is_strict  = false;
    bool is_graph   = false;
    bool is_digraph = false;

    /** Build a @c irt::table from node name to node identifier. This function
     * use the @c node_names and @c nodes object, do not change this object
     * after building a toc. */
    table<std::string_view, node_id> make_toc() const noexcept;
};

std::optional<dot_graph> parse_dot_buffer(std::string_view buffer) noexcept;

std::optional<dot_graph> parse_dot_file(
  const std::filesystem::path& p) noexcept;

} // irt

#endif
