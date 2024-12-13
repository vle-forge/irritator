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
    missing_token,
    missing_open_brace,
    unknown_attribute,
    missing_comma,
    parse_real,
    unknown_graph_type,
};

constexpr std::string_view msg_fmt[] = { "missing token at line {}",
                                         "missing open brace at line {}",
                                         "unknwon attribute {}",
                                         "missing comma character in `{}'",
                                         "fail to parse `{}' to read a float",
                                         "unknown graph type at line {}" };

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

    std::string main_id;
    bool        sort_before_search = false;
    bool        is_strict          = false;
    bool        is_graph           = false;
    bool        is_digraph         = false;
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

constexpr static auto find_node(dot_graph&       g,
                                std::string_view name) noexcept -> node_id
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

enum class element_type {
    none,
    opening_brace, // {
    closing_brace, // }
    strict,
    graph,
    digraph,
    subgraph,
    semicolon, // ;
    equals,    // =
    node,
    edge,
    opening_bracket, // [
    closing_bracket, // ]
    directed_edge,   // ->
    undirected_edge, // --
    id,
    real,
};

enum class str_id : irt::u32;
enum class real_id : irt::u32;

struct token {
    element_type type = element_type::none;

    union {
        str_id  str;
        real_id real;
    } id = {};
};

static constexpr bool is_new_line(int c) noexcept { return c == '\n'; }
static constexpr bool is_white_space(int c) noexcept
{
    return c == ' ' or c == '\t';
}

static constexpr bool starts_as_id(int c) noexcept
{
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
           ('\200' <= c and c <= '\377') or (c == '_');
}

class stream_buffer
{
public:
    static constexpr int ring_length = 16;

    irt::id_data_array<str_id, irt::default_allocator, std::string> strings;
    irt::id_data_array<real_id, irt::default_allocator, double>     reals;

    using view_ring = irt::small_ring_buffer<token, ring_length>;

    view_ring     views;
    std::istream& is;
    irt::i64      line = 0;

    stream_buffer(std::istream& is_) noexcept
      : is(is_)
    {
        while (not views.full()) {
            if (const auto ret = next_token(); ret.has_value())
                views.push_tail(*ret);
            else
                break;
        }
    }

private:
    void do_push_back(int size) noexcept
    {
        irt::debug::ensure(size > 0 && size <= ring_length);

        for (int i = ring_length - size; i < ring_length; ++i)
            if (const auto ret = next_token(); ret.has_value())
                views.push_tail(*ret);
    }

    void pop() noexcept
    {
        irt::debug::ensure(not views.empty());
        auto& front = *views.head();
        if (front.type == element_type::id)
            strings.free(front.id.str);
        else if (front.type == element_type::real)
            reals.free(front.id.real);

        views.pop_head();
    }

    token read_id() noexcept
    {
        if (not strings.can_alloc(1))
            strings.reserve(strings.capacity() * 2);

        const auto id  = strings.alloc();
        auto&      str = strings.get<std::string>(id);

        for (int c = is.get(); is.good(); c = is.get()) {
            if (('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
                ('\200' <= c and c <= '\377') or ('0' <= c and c <= '9') or
                (c == '_')) {
                str += static_cast<char>(c);
            } else {
                is.unget();
                break;
            }
        }

        return token{ .type = element_type::id, .id = id };
    }

    token read_double_quote() noexcept
    {
        if (not strings.can_alloc(1))
            strings.reserve(strings.capacity() * 2);

        const auto id  = strings.alloc();
        auto&      str = strings.get<std::string>(id);

        int c = is.get();

        irt::debug::ensure(c == '\"');
        irt::debug::ensure(is.good());

        for (int c = is.get(); is.good(); c = is.get()) {
            if (c != '\"') {
                str += static_cast<char>(c);
            } else {
                break;
            }
        }

        return token{ .type = element_type::id, .id = id };
    }

    std::optional<token> read_number() noexcept
    {
        if (not reals.can_alloc(1))
            reals.reserve(reals.capacity() * 2);

        std::string str;

        int c = is.get();
        str.push_back(static_cast<char>(c));

        while (is >> c) {
            if (c == '.' or ('0' << c and c <= '9')) {
                str.push_back(static_cast<char>(c));
            } else {
                is.unget();
                break;
            }
        }

        char* end = nullptr;
        if (auto value = strtod(str.c_str(), &end); value != 0) {
            const auto id         = reals.alloc();
            reals.get<double>(id) = value;
            return token{ .type = element_type::real, .id = id };
        } else {
            return std::nullopt;
        }
    }

    bool starts_as_number(int c) const noexcept
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

    void forget_c_comment() noexcept
    {
        for (auto c = is.get(); is.good(); c = is.get())
            if (c == '*' and is.get() == '/')
                return;
    }

    void forget_cpp_comment() noexcept
    {
        for (auto c = is.get(); is.good(); c = is.get())
            if (c == '\n')
                return;
    }

    void forget_line() noexcept
    {
        for (auto c = is.get(); is.good(); c = is.get())
            if (c == '\n')
                return;
    }

    /** Continue to tokenize the views. If the buffer is empty, append a new
     * buffer */
    std::optional<token> next_token() noexcept
    {
        if (views.empty()) {
            fill_tokens();
        }

        if (views.empty())
            return std::nullopt;
        else {
            const auto head = *views.head();
            views.pop_head();
            return head;
        }
    }

    void fill_tokens() noexcept
    {
        for (auto c = is.get(); is.good() and not views.full(); c = is.get()) {
            if (c == '\n') {
                line++;
            } else if (c == '\t' or c == ' ') {
            } else if (starts_as_id(c)) {
                is.unget();
                views.emplace_tail(read_id());
            } else if (starts_as_number(c)) {
                is.unget();
                views.emplace_tail(read_number());
            } else if (c == '\"') {
                is.unget();
                views.emplace_tail(read_double_quote());
            } else if (c == '#') {
                forget_line();
            } else if (c == '/') {
                const auto c2 = is.get();
                if (c2 == '*')
                    forget_c_comment();
                else if (c2 == '/')
                    forget_cpp_comment();
                // else
                // warning
            }
        }
    }

    bool empty() const noexcept
    {
        return views.empty() and (is.eof() or is.bad());
    }

    std::optional<dot_graph&> parse_stmt_list(dot_graph& g) noexcept {}

    std::optional<dot_graph&> parse_graph(dot_graph& g) noexcept
    {
        const auto strict_or_graph = next_token();
        if (not strict_or_graph.has_value())
            return error<msg_id::missing_token>(std::nullopt, line);

        if (strict_or_graph->type == element_type::strict) {
            g.is_strict           = true;
            const auto graph_type = next_token();
            if (strict_or_graph->type == element_type::graph) {
                g.is_graph = true;
            } else if (strict_or_graph->type == element_type::digraph) {
                g.is_digraph = true;
            } else {
                return error<msg_id::unknown_graph_type>(std::nullopt, line);
            }
        } else if (strict_or_graph->type == element_type::graph) {
            g.is_graph = true;
        } else if (strict_or_graph->type == element_type::digraph) {
            g.is_digraph = true;
        } else {
            return error<msg_id::unknown_graph_type>(std::nullopt, line);
        }

        const auto id_or_brace = next_token();
        if (not id_or_brace.has_value())
            return error<msg_id::missing_token>(std::nullopt, line);

        if (id_or_brace->type == element_type::id) {
            g.main_id = strings.get<std::string>(id_or_brace->id.str);
            const auto open_brace = next_token();
            if (not open_brace.has_value() or
                open_brace->type != element_type::opening_brace)
                return error<msg_id::missing_open_brace>(std::nullopt, line);
        } else if (id_or_brace->type != element_type::opening_brace) {
            return error<msg_id::missing_open_brace>(std::nullopt, line);
        }

        return parse_stmt_list(g);
    }

    std::optional<dot_graph> parse() noexcept
    {
        dot_graph ret;
        fill_tokens();

        return parse_graph(g);
    }

    // https://graphviz.org/doc/info/lang.html
    // https://www.ascii-code.com/ASCII
};

std::optional<dot_graph> parse_dot_file(const std::filesystem::path& p) noexcept
{
    std::ifstream ifs{ p };
    stream_buffer sb{ ifs };

    return sb.parse();
}

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
