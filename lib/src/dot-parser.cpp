// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>

#include "dot-parser.hpp"

#include <charconv>
#include <fstream>
#include <istream>
#include <streambuf>
#include <string_view>

#include <fmt/color.h>
#include <fmt/format.h>

namespace irt {

//
// dot-parser
//

enum class msg_id {
    missing_token,
    missing_strict_or_graph,
    missing_graph_type,
    unknown_graph_type,
    missing_open_brace,
    unknown_attribute,
    missing_comma,
    parse_real,
    undefined_slash_symbol,
};

static constexpr std::string_view msg_fmt[] = {
    "missing token at line {}",
    "missing strict or graph type at line {}",
    "missing graph type at line {}",
    "unknown graph type `{}' at line {}",
    "missing open brace at line {}",
    "unknwon attribute `{}' = `{}' at line {}",
    "missing comma character in `{}'",
    "fail to parse `{}' to read a float",
    "undefined `/{}' sequence at line {}"
};

template<msg_id Index, typename... Args>
static constexpr void warning(Args&&... args) noexcept
{
    constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);
    static_assert(0 <= idx and idx < std::size(msg_fmt));

    fmt::vprint(stderr, msg_fmt[idx], fmt::make_format_args(args...));
}

template<msg_id Index, typename Ret, typename... Args>
static constexpr auto error(Ret&& ret, Args&&... args) noexcept -> Ret
{
    constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);
    static_assert(0 <= idx and idx < std::size(msg_fmt));

    fmt::vprint(stderr,
                fg(fmt::terminal_color::red),
                msg_fmt[idx],
                fmt::make_format_args(args...));

    return ret;
}

#if !defined(__APPLE__)
static auto to_float_str(std::string_view str) noexcept
  -> std::optional<std::pair<float, std::string_view>>
{
    auto f = 0.f;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
        ret.ec == std::errc{})
        return std::make_pair(f, str.substr(ret.ptr - str.data()));

    return std::nullopt;
}

static auto to_float(std::string_view str) noexcept -> float
{
    auto f = 0.f;
    if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
        ret.ec == std::errc{})
        return f;

    return 0.f;
}
#else
static auto to_float_str(std::string_view str) noexcept
  -> std::optional<std::pair<float, std::string_view>>
{
    const std::string copy(str.data(), str.size());
    char*             copy_end = nullptr;

    const auto flt = std::strtof(copy.c_str(), &copy_end);
    if (flt == 0.0 and copy_end == copy.c_str()) {
        return std::nullopt;
    } else {
        return std::make_pair(flt, str.substr(copy_end - copy.c_str()));
    }
}

static auto to_float(std::string_view str) noexcept -> float
{
    const std::string copy(str.data(), str.size());

    return std::strtof(copy.c_str(), nullptr);
}
#endif

static auto to_2float(std::string_view str) noexcept -> std::array<float, 2>
{
    if (const auto first = to_float_str(str); first.has_value()) {
        const auto& [first_float, substr] = *first;

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

enum class element_type : irt::u16 {
    none,

    digraph,
    edge,
    graph,
    node,
    strict,
    subgraph,

    id,
    integer,
    double_quote,

    opening_brace,   // {
    closing_brace,   // }
    colon,           // :
    comma,           // ,
    semicolon,       // ;
    equals,          // =
    opening_bracket, // [
    closing_bracket, // ]
    directed_edge,   // ->
    undirected_edge, // --
};

static constexpr std::string_view element_type_string[] = {
    "{}",     "digraph",  "edge", "graph",   "node",
    "strict", "subgraph", "id",   "integer", "double_quote",
    "{",      "}",        ":",    ".",       ";",
    "=",      "[",        "]",    "->",      "--",
};

enum class str_id : irt::u32;

struct token {
    element_type type = element_type::none;
    str_id       str  = irt::undefined<str_id>();

    constexpr bool is_string() const noexcept
    {
        return irt::any_equal(type,
                              element_type::id,
                              element_type::integer,
                              element_type::double_quote);
    }

    constexpr bool operator[](const element_type t) const noexcept
    {
        return type == t;
    }
};

static element_type convert_to_element_type(const std::string_view str) noexcept
{
    static constexpr std::string_view strs[] = {
        "digraph", "edge", "graph", "node", "strict", "subgraph",
    };

    static constexpr element_type types[] = {
        element_type::digraph, element_type::edge,   element_type::graph,
        element_type::node,    element_type::strict, element_type::subgraph,
    };

    const auto beg = std::begin(strs);
    const auto end = std::end(strs);
    const auto it  = irt::binary_find(beg, end, str);

    return it != end ? types[std::distance(beg, it)] : element_type::id;
}

static constexpr bool starts_as_id(int c) noexcept
{
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
           ('\200' <= c and c <= '\377') or (c == '_');
}

static constexpr bool starts_as_number(int c) noexcept
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

class input_stream_buffer
{
public:
    static constexpr int ring_length = 32;

    using token_ring_t = irt::small_ring_buffer<token, ring_length>;

    irt::id_array<str_id, irt::default_allocator> strings_ids;
    irt::vector<std::string>                      strings;

    token_ring_t  tokens;
    std::istream& is;
    irt::i64      line = 0;

    irt::id_array<graph_node_id, irt::default_allocator> nodes;
    irt::id_array<graph_edge_id, irt::default_allocator> edges;

    irt::vector<std::string_view>             node_names;
    irt::vector<int>                          node_ids;
    irt::vector<std::array<float, 2>>         node_positions;
    irt::vector<float>                        node_areas;
    irt::vector<std::array<graph_node_id, 2>> edges_nodes;

    irt::table<std::string_view, graph_node_id> name_to_node_id;
    bool                                        sort_before_search = false;

    irt::string_buffer buffer;

    std::string main_id;
    bool        is_strict  = false;
    bool        is_graph   = false;
    bool        is_digraph = false;

    /** Default is to fill the token ring buffer from the @c std::istream.
     *  @param stream The default input stream.
     *  @param start_fill_tokens Start the parsing/tokenizing in constructor.
     */
    explicit input_stream_buffer(std::istream& stream,
                                 bool start_fill_tokens = true) noexcept
      : strings_ids(64)
      , strings(64)
      , is(stream)
    {
        strings.resize(strings_ids.capacity());

        if (start_fill_tokens)
            fill_tokens();
    }

private:
    constexpr auto find_or_add_node(std::string_view name) noexcept
      -> graph_node_id
    {
        if (sort_before_search) {
            name_to_node_id.sort();
            sort_before_search = false;
        }

        if (auto* found = name_to_node_id.get(name); found)
            return *found;

        if (not nodes.can_alloc(1)) {
            const auto c = nodes.capacity() == 0 ? 64 : nodes.capacity() * 2;
            nodes.reserve(c);
            node_names.resize(c);
            node_ids.resize(c);
            node_positions.resize(c);
            node_areas.resize(c);
        }

        const auto id  = nodes.alloc();
        const auto idx = irt::get_index(id);

        node_names[idx]     = buffer.append(name);
        node_ids[idx]       = 0;
        node_positions[idx] = { 0.f, 0.f };
        node_areas[idx]     = 0.f;
        name_to_node_id.data.emplace_back(node_names[idx], id);
        sort_before_search = true;

        return id;
    }

    constexpr auto find_node(std::string_view name) noexcept -> graph_node_id
    {
        if (sort_before_search) {
            name_to_node_id.sort();
            sort_before_search = false;
        }

        const auto* found = name_to_node_id.get(name);
        return found ? *found : irt::undefined<graph_node_id>();
    }

    void grow_strings() noexcept
    {
        const auto capacity = strings_ids.capacity();

        strings_ids.reserve(capacity < 64 ? 64 : capacity * 4);
        strings.resize(strings_ids.capacity());
    }

    token read_negative_integer() noexcept
    {
        if (not strings_ids.can_alloc(1))
            grow_strings();

        const auto id  = strings_ids.alloc();
        auto&      str = strings[irt::get_index(id)];
        char       c;
        str.clear();
        str += '-';

        while (is.get(c)) {
            if (c == '.' or c == '-' or ('0' <= c and c <= '9')) {
                str += c;
            } else {
                is.unget();
                break;
            }
        }

        return token{ .type = element_type::integer, .str = id };
    }

    token read_integer() noexcept
    {
        if (not strings_ids.can_alloc(1))
            grow_strings();

        const auto id  = strings_ids.alloc();
        auto&      str = strings[irt::get_index(id)];
        char       c;
        str.clear();

        while (is.get(c)) {
            if (c == '.' or c == '-' or ('0' <= c and c <= '9')) {
                str += c;
            } else {
                is.unget();
                break;
            }
        }

        return token{ .type = element_type::integer, .str = id };
    }

    token read_id() noexcept
    {
        if (not strings_ids.can_alloc(1))
            grow_strings();

        const auto id  = strings_ids.alloc();
        auto&      str = strings[irt::get_index(id)];
        char       c;
        str.clear();

        while (is.get(c)) {
            if (('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
                ('\200' <= static_cast<int>(c) and
                 static_cast<int>(c) <= '\377') or
                ('0' <= c and c <= '9') or (c == '_')) {
                str += static_cast<char>(c);
            } else {
                is.unget();
                break;
            }
        }

        return token{ .type = element_type::id, .str = id };
    }

    token read_double_quote() noexcept
    {
        if (not strings.can_alloc(1))
            grow_strings();

        const auto id  = strings_ids.alloc();
        auto&      str = strings[irt::get_index(id)];
        char       c;
        str.clear();

        while (is.get(c)) {
            if (c != '\"') {
                str += static_cast<char>(c);
            } else {
                break;
            }
        }

        return token{ .type = element_type::double_quote, .str = id };
    }

    /** Continue to read characters from input stream until the string \*\/ is
     * found. */
    void forget_c_comment() noexcept
    {
        char c;

        while (is.get(c))
            if (c == '*' and is.get(c) and c == '/')
                return;
    }

    /** Continue to read characters from input stream until the end of line
     * is found. */
    void forget_cpp_comment() noexcept
    {
        char c;

        while (is.get(c))
            if (c == '\n')
                return;
    }

    /** Continue to read characters from input stream until the end of line
     * is found. */
    void forget_line() noexcept
    {
        char c;

        while (is.get(c))
            if (c == '\n')
                return;
    }

    /** Returns the element of token ring buffer. If the buffer is empty,
     * read the @c std::istream. If the @c std::istream is empty and token
     * ring buffer is empty,  the @c element_type::none token is returned.
     */
    token pop_token() noexcept
    {
        if (tokens.empty()) // If the ring buffer is empty, fills the ring
            fill_tokens();  // buffer from the stream.

        if (tokens.empty()) // If the ring buffer is empty return none.
            return token{ .type = element_type::none };

        const auto head = *tokens.head();
        tokens.pop_head();

        return head;
    }

    /** Returns true if the next element in the ring buffer is @c type.
     * otherwise returns false. */
    bool next_token_is(const element_type type) noexcept
    {
        if (tokens.empty())
            fill_tokens();

        if (tokens.empty())
            return false;

        return tokens.head()->operator[](type);
    }

    /** Returns true if the next element in the ring buffer is @c type.
     * otherwise returns false. */
    bool next_token_is_string() noexcept
    {
        if (tokens.empty())
            fill_tokens();

        if (tokens.empty())
            return false;

        return tokens.head()->is_string();
    }

    void fill_tokens() noexcept
    {
        char c, c2;

        while (not tokens.full() and is.get(c)) {
            if (c == '\n') {
                line++;
            } else if (c == '#') {
                forget_line();
            } else if (c == '/') {
                if (is.get(c2)) {
                    if (c2 == '*')
                        forget_c_comment();
                    else if (c2 == '/')
                        forget_cpp_comment();
                    else {
                        is.unget();
                        warning<msg_id::undefined_slash_symbol>(c2, line);
                    }
                }
            } else if (c == '\t' or c == ' ') {
            } else if (c == '-') {
                if (is.get(c2)) {
                    if (c2 == '-')
                        tokens.emplace_tail(element_type::undirected_edge);
                    else if (c2 == '>')
                        tokens.emplace_tail(element_type::directed_edge);
                    else {
                        is.unget();
                        tokens.emplace_tail(read_negative_integer());
                    }
                }
            } else if (starts_as_id(c)) {
                is.unget();
                tokens.emplace_tail(read_id());
            } else if (starts_as_number(c)) {
                is.unget();
                tokens.emplace_tail(read_integer());
            } else if (c == '\"') {
                tokens.emplace_tail(read_double_quote());
            } else if (c == '{') {
                tokens.emplace_tail(element_type::opening_brace);
            } else if (c == '}') {
                tokens.emplace_tail(element_type::closing_brace);
            } else if (c == '[') {
                tokens.emplace_tail(element_type::opening_bracket);
            } else if (c == ']') {
                tokens.emplace_tail(element_type::closing_bracket);
            } else if (c == ';') {
                tokens.emplace_tail(element_type::semicolon);
            } else if (c == ':') {
                tokens.emplace_tail(element_type::colon);
            } else if (c == ',') {
                tokens.emplace_tail(element_type::comma);
            } else if (c == '=') {
                tokens.emplace_tail(element_type::equals);
            }
        }
    }

    bool empty() const noexcept
    {
        return tokens.empty() and (is.eof() or is.bad());
    }

    bool check_minimum_tokens(std::integral auto nb) noexcept
    {
        if (std::cmp_greater_equal(tokens.size(), nb))
            return true;

        fill_tokens();

        return std::cmp_greater_equal(tokens.size(), nb);
    }

    void print_ring(const std::string_view title) const noexcept
    {
        fmt::print("{}\n ", title);
        for (auto it = tokens.head(), et = tokens.tail(); it != et; ++it) {
            if (it->is_string())
                fmt::print(" `{}'", strings[irt::get_index(it->str)]);
            else
                fmt::print(" {}",
                           element_type_string[static_cast<int>(it->type)]);
        }
        fmt::print("\n");
    }

    bool next_is_edge() noexcept
    {
        if (not check_minimum_tokens(2))
            return false;

        auto        it     = tokens.head();
        const auto& first  = *it++;
        const auto& second = *it++;

        return first.is_string() and (second[element_type::directed_edge] or
                                      second[element_type::undirected_edge]);
    }

    bool next_is_attributes() noexcept
    {
        if (not check_minimum_tokens(2))
            return false;

        auto        it     = tokens.head();
        const auto& first  = *it++;
        const auto& second = *it++;

        return first[element_type::opening_bracket] and
               (second[element_type::closing_bracket] or second.is_string());
    }

    bool parse_attributes(const graph_node_id id) noexcept
    {
        if (not check_minimum_tokens(1))
            return false;

        auto bracket = pop_token();
        if (not bracket[element_type::opening_bracket])
            return false;

        for (;;) {
            if (not check_minimum_tokens(1))
                return false;

            auto left = pop_token();
            if (left[element_type::closing_bracket])
                return true;

            if (not check_minimum_tokens(3))
                return false;

            if (not left.is_string())
                return false;

            auto middle = pop_token();
            if (not middle[element_type::equals])
                return false;

            auto right = pop_token();
            if (not right.is_string())
                return false;

            const auto left_str  = get_and_free_string(left);
            const auto right_str = get_and_free_string(right);

            if (left_str == "area") {
                node_areas[irt::get_index(id)] = to_float(right_str);
            } else if (left_str == "pos") {
                node_positions[irt::get_index(id)] = to_2float(right_str);
            } else {
                warning<msg_id::unknown_attribute>(left_str, right_str, line);
            }

            auto close_backet_or_comma = pop_token();
            if (close_backet_or_comma[element_type::comma] or
                close_backet_or_comma[element_type::semicolon]) {
                continue;
            } else if (close_backet_or_comma[element_type::closing_bracket]) {
                return true;
            } else {
                return false;
            }
        }
    }

    bool parse_attributes(const graph_edge_id /*id*/) noexcept { return true; }

    bool parse_edge() noexcept
    {
        if (not check_minimum_tokens(3))
            return false;

        const auto from = pop_token();
        if (not from.is_string())
            return false;

        const auto from_str = get_and_free_string(from);
        const auto from_id  = find_or_add_node(from_str);

        const auto type = pop_token();
        if (not(type.type == element_type::directed_edge or
                type.type == element_type::undirected_edge))
            return false;

        const auto to = pop_token();
        if (not to.is_string())
            return false;

        const auto to_str = get_and_free_string(to);
        const auto to_id  = find_or_add_node(to_str);

        if (not edges.can_alloc(1)) {
            const auto c = edges.capacity() == 0 ? 64 : edges.capacity() * 2;
            edges.reserve(c);
            edges_nodes.resize(c);
        }

        const auto new_edge_id                   = edges.alloc();
        edges_nodes[irt::get_index(new_edge_id)] = { from_id, to_id };

        return next_is_attributes() ? parse_attributes(new_edge_id) : true;
    }

    bool parse_node() noexcept
    {
        if (not check_minimum_tokens(1))
            return false;

        const auto node_id = pop_token();
        if (not node_id.is_string())
            return false;

        const auto str = get_and_free_string(node_id);
        const auto id  = find_or_add_node(str);

        return next_is_attributes() ? parse_attributes(id) : true;
    }

    bool parse_stmt_list() noexcept
    {
        if (not check_minimum_tokens(2))
            return false;

        const auto open = pop_token();
        if (not open[element_type::opening_brace])
            return false;

        while (check_minimum_tokens(1)) {
            if (next_token_is(element_type::closing_brace)) {
                pop_token();
                return true;
            }

            const auto success = next_is_edge() ? parse_edge() : parse_node();
            if (not success)
                return false;
        }

        if (not check_minimum_tokens(1))
            return false;

        auto closing = pop_token();
        return closing.type == element_type::closing_brace;
    }

    bool parse_graph_type(const token type) noexcept
    {
        if (not type.is_string())
            return error<msg_id::missing_graph_type>(false, line);

        const auto s = get_and_free_string(type);
        return parse_graph_type(s);
    }

    bool parse_graph_type(const std::string_view type) noexcept
    {
        if (type == "graph")
            is_graph = true;
        else if (type == "digraph")
            is_digraph = true;
        else
            return error<msg_id::unknown_graph_type>(false, type, line);

        return true;
    }

    bool parse_graph() noexcept
    {
        if (not check_minimum_tokens(1))
            return false;

        const auto strict_or_graph = pop_token();
        if (not strict_or_graph.is_string())
            return error<msg_id::missing_strict_or_graph>(false, line);

        const auto s = get_and_free_string(strict_or_graph);

        if (s == "strict") {
            is_strict = true;

            if (not check_minimum_tokens(1))
                return false;

            const auto graph_type = pop_token();
            if (not parse_graph_type(graph_type))
                return false;
        } else {
            if (not parse_graph_type(s))
                return false;
        }

        if (not check_minimum_tokens(1))
            return false;

        if (next_token_is_string()) {
            const auto m_id = pop_token();
            main_id         = get_and_free_string(m_id);
        }

        return check_minimum_tokens(1) ? parse_stmt_list() : true;
    }

    const std::string& get_and_free_string(const token t) noexcept
    {
        irt::debug::ensure(t.is_string());

        const auto idx = irt::get_index(t.str);
        strings_ids.free(t.str);

        return strings[idx];
    }

public:
    std::optional<dot_graph> parse() noexcept
    {
        fill_tokens();

        if (parse_graph())
            return dot_graph{
                .nodes = std::move(nodes),
                .edges = std::move(edges),

                .node_names     = std::move(node_names),
                .node_ids       = std::move(node_ids),
                .node_positions = std::move(node_positions),
                .node_areas     = std::move(node_areas),
                .edges_nodes    = std::move(edges_nodes),

                .buffer = std::move(buffer),

                .main_id = std::move(main_id),

                .is_strict  = is_strict,
                .is_graph   = is_graph,
                .is_digraph = is_digraph,
            };

        return std::nullopt;
    }

    // https://graphviz.org/doc/info/lang.html
    // https://www.ascii-code.com/ASCII
};

std::optional<dot_graph> parse_dot_buffer(
  const std::string_view buffer) noexcept
{
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

        explicit streambuf_view(std::string_view str) noexcept
          : streambuf_view(str.data(), str.size())
        {}
    };

    class istring_view_stream
      : private virtual streambuf_view
      , public std::istream
    {
    public:
        istring_view_stream(const char* str, std::size_t len) noexcept
          : streambuf_view(str, len)
          , std::istream(static_cast<std::streambuf*>(this))
        {}

        explicit istring_view_stream(const std::string_view str) noexcept
          : streambuf_view(str)
          , std::istream(static_cast<std::streambuf*>(this))
        {}
    };

    istring_view_stream isvs{ buffer.data(), buffer.size() };
    input_stream_buffer sb{ isvs };

    return sb.parse();
}

std::optional<dot_graph> parse_dot_file(const std::filesystem::path& p) noexcept
{
    std::ifstream       ifs{ p };
    input_stream_buffer sb{ ifs };

    return sb.parse();
}

irt::table<std::string_view, graph_node_id> dot_graph::make_toc() const noexcept
{
    irt::table<std::string_view, graph_node_id> ret;
    ret.data.reserve(nodes.size());

    for (const auto id : nodes) {
        const auto idx = irt::get_index(id);
        ret.data.emplace_back(node_names[idx], id);
    }

    ret.sort();

    return ret;
}

} // namespace irt
