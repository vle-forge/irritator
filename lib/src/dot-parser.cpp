// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>

#include "dot-parser.hpp"

#include <fstream>
#include <istream>
#include <numeric>
#include <streambuf>
#include <string_view>

namespace irt {

class streambuf_view : public std::streambuf
{
private:
    using ios_base = std::ios_base;

protected:
    pos_type seekoff(off_type                            off,
                     ios_base::seekdir                   dir,
                     [[maybe_unused]] ios_base::openmode which =
                       ios_base::in | ios_base::out) override
    {
        if (dir == ios_base::cur)
            gbump(static_cast<int>(off));
        else if (dir == ios_base::end)
            setg(eback(), egptr() + off, egptr());
        else if (dir == ios_base::beg)
            setg(eback(), eback() + off, egptr());
        return gptr() - eback();
    }

    pos_type seekpos(pos_type sp, ios_base::openmode which) override
    {
        return seekoff(sp - pos_type(off_type(0)), ios_base::beg, which);
    }

public:
    streambuf_view(const char* s, std::size_t count) noexcept
    {
        auto p = const_cast<char*>(s);
        setg(p, p, p + count);
    }

    streambuf_view(std::string_view str) noexcept
      : streambuf_view(str.data(), str.size())
    {}
};

class istring_view_stream
  : private virtual streambuf_view
  , public std::istream
{
public:
    istring_view_stream(const std::string_view str)
      : streambuf_view(str)
      , std::istream(static_cast<std::streambuf*>(this))
    {}
};

std::string_view string_buffer::append(std::string_view str) noexcept
{
    debug::ensure(not str.empty());
    debug::ensure(str.size() < string_buffer_node_length);

    if (m_container.empty() ||
        str.size() + m_position > string_buffer_node_length)
        do_alloc();

    std::size_t position = m_position;
    m_position += str.size();

    char* buffer = m_container.front().data() + position;

    std::copy_n(str.data(), str.size(), buffer);

    return std::string_view(buffer, str.size());
}

std::size_t string_buffer::size() const noexcept
{
    return static_cast<std::size_t>(
      std::distance(m_container.cbegin(), m_container.cend()));
}

void string_buffer::do_alloc() noexcept
{
    m_container.emplace_front();
    m_position = 0;
}

//
// dot-parser
//

auto dot_parser::search_node_linear(std::string_view name) const noexcept
  -> std::optional<graph_component::vertex_id>
{
    for (auto i = 0, e = nodes.ssize(); i != e; ++i)
        if (nodes[i].name == name)
            return nodes[i].id;

    return std::nullopt;
}

auto dot_parser::search_node(std::string_view name) const noexcept
  -> std::optional<graph_component::vertex_id>
{
    auto it = binary_find(
      nodes.begin(),
      nodes.end(),
      name,
      [](auto left, auto right) noexcept -> bool {
          if constexpr (std::is_same_v<decltype(left), std::string_view>)
              return left < right.name;
          else
              return left.name < right;
      });

    return it == nodes.end() ? std::nullopt : std::make_optional(it->id);
}

void dot_parser::sort() noexcept
{
    std::sort(nodes.begin(),
              nodes.end(),
              [](const auto& left, const auto& right) noexcept {
                  return left.name < right.name;
              });
}

//
// parsing function
//

static constexpr bool are_equal(const std::string_view lhs,
                                const std::string_view rhs) noexcept
{
    if (rhs.size() != lhs.size())
        return false;

    std::string_view::size_type i = 0;
    std::string_view::size_type e = lhs.size();

    for (; i != e; ++i)
        if (std::tolower(lhs[i]) != std::tolower(rhs[i]))
            return false;

    return true;
}

// static const std::string_view keywords[] = {
//     "digraph", "edge", "graph", "node", "strict", "subgraph",
// };

// static constexpr bool is_keyword(const std::string_view str) noexcept
// {
//     return std::binary_search(
//       std::begin(keywords), std::end(keywords), str, are_equal);
// }

static constexpr bool is_separator(const int c) noexcept
{
    switch (c) {
    case '[':
    case ']':
    case ';':
    case ',':
    case ':':
    case '=':
    case '-':
    case '>':
    case '{':
    case '}':
        return true;

    default:
        return false;
    }
}

static constexpr bool starts_as_id(const int c) noexcept
{
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
           ('\200' <= c and c <= '\377') or (c == '_');
}

static constexpr bool starts_as_id(const std::string_view str) noexcept
{
    return not str.empty() and starts_as_id(str[0]);
}

static constexpr bool next_char_is_id(const int c) noexcept
{
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
           ('\200' <= c and c <= '\377') or ('0' <= c and c <= '9') or
           (c == '_');
}

static constexpr bool next_is_id(const std::string_view str) noexcept
{
    const auto len = str.size();

    if (len == 0)
        return false;

    if (not starts_as_id(str[0]))
        return false;

    for (std::size_t i = 1; i < len; ++i)
        if (not next_char_is_id(str[i]))
            return false;

    return true;
}

static constexpr bool starts_as_number(const int c) noexcept
{
    switch (c) {
    case '-':
    case '.':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;

    default:
        return false;
    }
}

// static constexpr bool starts_as_number(const std::string_view str) noexcept
// {
//     return not str.empty() and starts_as_number(str[0]);
// }

static constexpr bool next_char_is_number(const int c) noexcept
{
    switch (c) {
    case '.':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;

    default:
        return false;
    }
}

// static constexpr bool next_is_number(const std::string_view str) noexcept
// {
//     const auto len = str.size();

//     if (len == 0)
//         return false;

//     if (not starts_as_number(str[0]))
//         return false;

//     for (std::size_t i = 1; i < len; ++i)
//         if (not next_char_is_number(str[i]))
//             return false;

//     return true;
// }

class stream_buffer
{
public:
    constexpr static int stream_buffer_size = 10;

    using string_view_array = std::array<std::string_view, stream_buffer_size>;

    stream_buffer(std::istream& is_)
      : is(is_)
    {
        for (int i = 0; i != stream_buffer_size; ++i)
            buffer_ptr[i] = next_token();
    }

    void pop_front()
    {
        auto tmp = buffer_ptr[0];

        buffer_ptr[0] = buffer_ptr[1];
        buffer_ptr[1] = buffer_ptr[2];
        buffer_ptr[2] = buffer_ptr[3];
        buffer_ptr[3] = buffer_ptr[4];
        buffer_ptr[4] = buffer_ptr[5];
        buffer_ptr[5] = buffer_ptr[6];
        buffer_ptr[6] = buffer_ptr[7];
        buffer_ptr[7] = buffer_ptr[8];
        buffer_ptr[8] = buffer_ptr[9];
        buffer_ptr[9] = tmp;

        do_push_back(1);
    }

    void pop_front(int size)
    {
        debug::ensure(size >= 1 && size < stream_buffer_size);

        auto tmp = buffer_ptr[0];

        buffer_ptr[0] = buffer_ptr[(size + 0) % stream_buffer_size];
        buffer_ptr[1] = buffer_ptr[(size + 1) % stream_buffer_size];
        buffer_ptr[2] = buffer_ptr[(size + 2) % stream_buffer_size];
        buffer_ptr[3] = buffer_ptr[(size + 3) % stream_buffer_size];
        buffer_ptr[4] = buffer_ptr[(size + 4) % stream_buffer_size];
        buffer_ptr[5] = buffer_ptr[(size + 5) % stream_buffer_size];
        buffer_ptr[6] = buffer_ptr[(size + 6) % stream_buffer_size];
        buffer_ptr[7] = buffer_ptr[(size + 7) % stream_buffer_size];
        buffer_ptr[8] = buffer_ptr[(size + 8) % stream_buffer_size];
        buffer_ptr[9] = tmp;

        do_push_back(size);
    }

    void print(const std::string_view msg) const
    {
        debug_log(msg);
        for (int i = 0; i < stream_buffer_size; ++i)
            debug_log("[{}: ({})]", i, buffer_ptr[i]);
        debug_log("\n");
    }

    const std::array<std::string_view, stream_buffer_size> array()
      const noexcept
    {
        return buffer_ptr;
    }

    bool empty() const noexcept { return buffer_ptr[0].empty(); }

    constexpr std::string_view first() const noexcept { return buffer_ptr[0]; }

    constexpr std::string_view second() const noexcept { return buffer_ptr[1]; }

    constexpr std::string_view third() const noexcept { return buffer_ptr[2]; }

    constexpr std::string_view fourth() const noexcept { return buffer_ptr[3]; }

private:
    struct stream_token {
        char        buffer[512] = { '\0' };
        std::size_t current     = 0;
    };

    std::array<std::string_view, stream_buffer_size> buffer_ptr;
    std::array<stream_token, stream_buffer_size>     token;
    std::istream&                                    is;
    int                                              current_token_buffer = 0;

    void do_push_back(int size)
    {
        debug::ensure(size > 0 && size <= stream_buffer_size);

        for (int i = stream_buffer_size - size; i < stream_buffer_size; ++i)
            buffer_ptr[i] = next_token();
    }

    std::string_view next_token() noexcept
    {
        if (token[current_token_buffer]
              .buffer[token[current_token_buffer].current] == '\0') {
            current_token_buffer =
              (current_token_buffer + 1) % stream_buffer_size;
            if (!is.good())
                return std::string_view();

            token[current_token_buffer].buffer[0] = '\0';
            while (is >> token[current_token_buffer].buffer) {
                if (token[current_token_buffer].buffer[0] == '\\') {
                    is.ignore(std::numeric_limits<std::streamsize>::max(),
                              '\n');
                    continue;
                }

                if (token[current_token_buffer].buffer[0] == '\0')
                    continue;

                break;
            }

            token[current_token_buffer].current = 0;
            if (token[current_token_buffer].buffer[0] == '\0')
                return std::string_view();
        }

        if (token[current_token_buffer]
              .buffer[token[current_token_buffer].current] == '\0')
            return std::string_view();

        bool starts_with_number =
          starts_as_number(token[current_token_buffer]
                             .buffer[token[current_token_buffer].current]);

        std::size_t start = token[current_token_buffer].current++;

        if (is_separator(token[current_token_buffer].buffer[start]))
            return std::string_view(&token[current_token_buffer].buffer[start],
                                    1);

        if (token[current_token_buffer]
              .buffer[token[current_token_buffer].current] == '\0')
            return std::string_view(&token[current_token_buffer].buffer[start],
                                    1);

        while (token[current_token_buffer]
                 .buffer[token[current_token_buffer].current] != '\0') {
            if (is_separator(token[current_token_buffer]
                               .buffer[token[current_token_buffer].current]))
                break;

            if (starts_with_number &&
                !next_char_is_number(
                  token[current_token_buffer]
                    .buffer[token[current_token_buffer].current]))
                break;

            ++token[current_token_buffer].current;
        }

        return std::string_view(&token[current_token_buffer].buffer[start],
                                token[current_token_buffer].current - start);
    }
};

void try_read_strict(stream_buffer& buf,
                     dot_parser& /*dot*/,
                     graph_component& /*graph*/) noexcept
{
    if (not buf.empty() and are_equal(buf.first(), "strict"))
        buf.pop_front();
}

result<dot_parser::graph> read_graph_or_digraph(
  stream_buffer& buf,
  dot_parser& /*dot*/,
  graph_component& /*graph*/) noexcept
{
    if (not buf.empty()) {
        if (are_equal(buf.first(), "graph")) {
            buf.pop_front();
            return dot_parser::graph::graph;
        }

        if (are_equal(buf.first(), "digraph")) {
            buf.pop_front();
            return dot_parser::graph::digraph;
        }
    }

    return new_error(dot_parser::read_graph_or_digraph_error{});
}

static constexpr bool next_is_edgeop(stream_buffer& buf) noexcept
{
    return buf.first() == "-" and (buf.second() == ">" or buf.second() == "-");
}

static constexpr bool is_next_main_curly_brace_close(
  stream_buffer& buf,
  dot_parser& /*dot*/,
  graph_component& /*graph*/) noexcept
{
    return buf.first() == "}";
}

static status read_main_id(stream_buffer& buf,
                           dot_parser&    dot,
                           graph_component& /*graph*/) noexcept
{
    if (next_is_id(buf.first())) {
        dot.id = dot.buffer.append(buf.first());
        buf.pop_front();
        return success();
    }

    return new_error(dot_parser::read_main_id_error{});
}

static auto read_id(stream_buffer&   buf,
                    dot_parser&      dot,
                    graph_component& graph) noexcept
  -> result<graph_component::vertex_id>
{
    if (starts_as_id(buf.first())) {
        graph_component::vertex_id id;
        if (auto id_opt = dot.search_node_linear(buf.first());
            id_opt.has_value()) {
            id = *id_opt;
        } else {
            auto& n = graph.children.alloc(undefined<component_id>());
            id      = graph.children.get_id(n);
            n.name  = buf.first();
            dot.nodes.emplace_back(dot.buffer.append(buf.first()), id);
        }

        buf.pop_front();
        return id;
    }

    return new_error(dot_parser::read_id_error{});
}

static status read_main_curly_brace_open(stream_buffer& buf,
                                         dot_parser& /*dot*/,
                                         graph_component& /*graph*/) noexcept
{
    if (buf.first() != "{")
        return new_error(dot_parser::missing_curly_brace_error{});

    buf.pop_front();

    return success();
}

static auto read_edgeop(stream_buffer& buf,
                        dot_parser& /*dot*/,
                        graph_component& /*graph*/) noexcept
  -> result<dot_parser::edgeop>
{
    if (buf.first() == "-" and buf.second() == ">") {
        buf.pop_front();
        buf.pop_front();
        return dot_parser::edgeop::directed;
    }

    if (buf.first() == "-" and buf.second() == "-") {
        buf.pop_front();
        buf.pop_front();
        return dot_parser::edgeop::undirected;
    }

    return new_error(dot_parser::read_edgeop_error{});
}

status parse(stream_buffer&   buf,
             dot_parser&      dot,
             graph_component& graph) noexcept
{
    dot_parser dp;

    try_read_strict(buf, dot, graph);

    irt_auto(type, read_graph_or_digraph(buf, dot, graph));
    dp.type = type;

    if (starts_as_id(buf.first()))
        irt_check(read_main_id(buf, dot, graph));

    irt_check(read_main_curly_brace_open(buf, dot, graph));

    while (not is_next_main_curly_brace_close(buf, dot, graph)) {
        irt_auto(id_lhs, read_id(buf, dot, graph));

        if (next_is_edgeop(buf)) {
            irt_auto(op, read_edgeop(buf, dot, graph));
            irt_auto(id_rhs, read_id(buf, dot, graph));
            graph.edges.alloc(id_lhs, id_rhs);
            if (op == dot_parser::edgeop::undirected)
                graph.edges.alloc(id_rhs, id_lhs);
        }
    }

    return success();
}

status parse_dot_file(graph_component& graph) noexcept
{
    std::ifstream ifs{ "toto.dot" };
    stream_buffer sb{ ifs };
    dot_parser    dot;

    return parse(sb, dot, graph);
}

status parse_dot_buffer(graph_component&       graph,
                        const std::string_view buffer) noexcept
{
    istring_view_stream svs{ buffer };
    stream_buffer       sb{ svs };
    dot_parser          dot;

    return parse(sb, dot, graph);
}

} // namespace irt
