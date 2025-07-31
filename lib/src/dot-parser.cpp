// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/dot-parser.hpp>
#include <irritator/format.hpp>

#include <cctype>
#include <charconv>
#include <fstream>
#include <istream>
#include <random>
#include <streambuf>
#include <string_view>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#ifndef R123_USE_CXX11
#define R123_USE_CXX11 1
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <Random123/philox.h>
#include <Random123/uniform.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace irt {

struct local_rng {
    using rng          = r123::Philox4x64;
    using counter_type = r123::Philox4x64::ctr_type;
    using key_type     = r123::Philox4x64::key_type;
    using result_type  = counter_type::value_type;

    static_assert(counter_type::static_size == 4);
    static_assert(key_type::static_size == 2);

    result_type operator()() noexcept
    {
        if (last_elem == 0) {
            c.incr();

            rng b{};
            rdata = b(c, k);

            n++;
            last_elem = rdata.size();
        }

        return rdata[--last_elem];
    }

    local_rng(std::span<const u64> c0, std::span<const u64> uk) noexcept
      : n(0)
      , last_elem(0)
    {
        std::copy_n(c0.data(), c0.size(), c.data());
        std::copy_n(uk.data(), uk.size(), k.data());
    }

    constexpr static result_type(min)() noexcept
    {
        return (std::numeric_limits<result_type>::min)();
    }

    constexpr static result_type(max)() noexcept
    {
        return (std::numeric_limits<result_type>::max)();
    }

    counter_type c;
    key_type     k;
    counter_type rdata{ 0u, 0u, 0u, 0u };
    u64          n;
    sz           last_elem;
};

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
    missing_reg_path,
    missing_dir_path,
    missing_file_path,
};

static constexpr std::string_view msg_fmt[] = {
    "missing token at line {}\n",
    "missing strict or graph type at line {}\n",
    "missing graph type at line {}\n",
    "unknown graph type `{}' at line {}\n",
    "missing open brace at line {}\n",
    "unknwon attribute `{}' = `{}' at line {}\n",
    "missing comma character in `{}'\n",
    "fail to parse `{}' to read a float\n",
    "undefined `/{}' sequence at line {}\n",
    "missing registered path\n",
    "missing directory path\n",
    "missing file path\n"
};

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

/**
   Return lower case of char @c c iff @c c is upper ascii character.

   @param c The character to convert iff @c c if upper ascii character.
   @return The lower car of the @c c parameter.
 */
inline static auto ascii_tolower(const char c) noexcept -> char
{
    return ((static_cast<unsigned>(c) - 65u) < 26) ? c + 'a' - 'A' : c;
}

/**
   Return true if the lower characters @c a and @c b are equals.

   @param a The character to convert iff @c c if upper ascii character.
   @param b The character to convert iff @c c if upper ascii character.
 */
inline static auto ichar_equals(char a, char b) noexcept -> bool
{
    return ascii_tolower(a) == ascii_tolower(b);
}

/**
   Returns @c true if two strings are equal, using a case-insensitive
   comparison. The case-comparison operation is defined only for low-ASCII
   characters.

   @param lhs The string on the left side of the equality
   @param rhs The string on the right side of the equality
 */
inline static auto iequals(std::string_view lhs, std::string_view rhs) noexcept
  -> bool
{
    auto n = lhs.size();
    if (rhs.size() != n)
        return false;

    auto p1 = lhs.data();
    auto p2 = rhs.data();
    char a, b;

    while (n--) {
        a = *p1++;
        b = *p2++;

        if (a != b)
            goto slow;
    }

    return true;

slow:
    do {
        if (ascii_tolower(a) != ascii_tolower(b))
            return false;
        a = *p1++;
        b = *p2++;
    } while (n--);

    return true;
}

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

    constexpr token() noexcept = default;

    explicit constexpr token(const element_type t) noexcept
      : type{ t }
    {}

    constexpr token(const element_type t, const str_id s) noexcept
      : type{ t }
      , str{ s }
    {}

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
    const auto it  = irt::binary_find(
      beg, end, str, [](const auto l, const auto r) noexcept -> bool {
          for (sz i = 0, e = std::min(l.size(), r.size()); i != e; ++i)
              if (not ichar_equals(l[i], r[i]))
                  return l[i] < r[i];

          return l.size() < r.size();
      });

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
private:
    template<msg_id Index, typename... Args>
    constexpr void warning(Args&&... args) noexcept
    {
        constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);
        static_assert(0 <= idx and idx < std::size(msg_fmt));

        mod.journal.push(
          log_level::warning,
          [](auto& title, auto& msg, auto& format, auto args) {
              title = "Dot parser warning";

              auto ret =
                fmt::vformat_to_n(msg.data(), msg.capacity() - 1, format, args);
              msg.resize(ret.size);
          },
          msg_fmt[idx],
          fmt::make_format_args(args...));
    }

    template<msg_id Index, typename... Args>
    constexpr auto error(Args&&... args) noexcept -> bool
    {
        constexpr auto idx = static_cast<std::underlying_type_t<msg_id>>(Index);
        static_assert(0 <= idx and idx < std::size(msg_fmt));

        mod.journal.push(
          log_level::error,
          [](auto& title, auto& msg, auto& format, auto args) {
              title = "Dot parser error";

              auto ret =
                fmt::vformat_to_n(msg.data(), msg.capacity() - 1, format, args);
              msg.resize(ret.size);
          },
          msg_fmt[idx],
          fmt::make_format_args(args...));

        return false;
    }

#if !defined(__APPLE__)
    auto to_float_str(std::string_view str) noexcept
      -> std::optional<std::pair<float, std::string_view>>
    {
        auto f = 0.f;
        if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
            ret.ec == std::errc{})
            return std::make_pair(f, str.substr(ret.ptr - str.data()));

        return std::nullopt;
    }

    auto to_float(std::string_view str) noexcept -> float
    {
        auto f = 0.f;

        if (auto ret = std::from_chars(str.data(), str.data() + str.size(), f);
            ret.ec == std::errc{})
            return f;

        warning<msg_id::parse_real>(str);

        return 0.f;
    }
#else
    auto to_float_str(std::string_view str) noexcept
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

    auto to_float(std::string_view str) noexcept -> float
    {
        const std::string copy(str.data(), str.size());

        const auto ret = std::strtof(copy.c_str(), nullptr);
        if (ret == 0.f and errno != 0) {
            warning<msg_id::parse_real>(str);
            return 0.f;
        }
        return ret;
    }
#endif

    auto to_2float(std::string_view str) noexcept -> std::array<float, 2>
    {
        if (const auto first = to_float_str(str); first.has_value()) {
            const auto& [first_float, substr] = *first;

            if (not substr.empty() or substr[0] == ',') {
                const auto second_str =
                  substr.substr(1u, std::string_view::npos);
                return std::array<float, 2>{ first_float,
                                             to_float(second_str) };
            } else {
                warning<msg_id::missing_comma>(substr);
            }
        }

        return std::array<float, 2>{};
    }

public:
    static constexpr int ring_length = 32;

    using token_ring_t = irt::small_ring_buffer<token, ring_length>;

    const modeling& mod;

    irt::id_array<str_id>    strings_ids;
    irt::vector<std::string> strings;

    token_ring_t  tokens;
    std::istream& is;
    irt::i64      line = 0;

    graph g;

    irt::table<std::string_view, graph_node_id> name_to_node_id;

    error_code ec;

    bool sort_before_search = false;

    /** Default is to fill the token ring buffer from the @c std::istream.
     *
     *  @param mod To get component
     *  @param stream The default input stream.
     *  @param start_fill_tokens Start the parsing/tokenizing in constructor.
     */
    explicit input_stream_buffer(const modeling& mod_,
                                 std::istream&   stream,
                                 bool start_fill_tokens = true) noexcept
      : mod{ mod_ }
      , strings_ids(64)
      , strings(64)
      , is(stream)
    {
        if (start_fill_tokens)
            fill_tokens();
    }

private:
    /**
     * Search the identifier of a node called @a name or add it. This function
     * returns a valid identifier or an @a error_code if the container can not
     * be grow to store more nodes.
     * @param name The string to search into node names.
     * @return A valid @a graph_node_id or a @a error_code.
     */
    auto find_or_add_node(std::string_view name) noexcept
      -> expected<graph_node_id>
    {
        if (sort_before_search) {
            name_to_node_id.sort();
            sort_before_search = false;
        }

        if (auto* found = name_to_node_id.get(name); found)
            return *found;

        if (not g.nodes.can_alloc(1)) {
            if (not g.nodes.grow<2, 1>())
                return new_error(modeling_errc::dot_memory_insufficient);

            const auto c = g.nodes.capacity();

            if (not(g.nodes.reserve(c) and g.node_names.resize(c) and
                    g.node_ids.resize(c) and g.node_positions.resize(c) and
                    g.node_areas.resize(c) and g.node_components.resize(c)))
                return new_error(modeling_errc::dot_memory_insufficient);
        }

        const auto id  = g.nodes.alloc();
        const auto idx = irt::get_index(id);

        g.node_names[idx]     = g.buffer.append(name);
        g.node_ids[idx]       = std::string_view{};
        g.node_positions[idx] = { 0.f, 0.f };
        g.node_areas[idx]     = 0.f;
        name_to_node_id.data.emplace_back(g.node_names[idx], id);
        sort_before_search = true;

        return id;
    }

    auto add_port(std::string_view name) noexcept
    {
        return g.buffer.append(name);
    }

    expected<void> grow_strings() noexcept
    {
        if (not strings_ids.can_alloc(1)) {
            if (not strings_ids.grow<2, 1>())
                return new_error(modeling_errc::dot_memory_insufficient);

            if (not strings.resize(strings_ids.capacity()))
                return new_error(modeling_errc::dot_memory_insufficient);
        }

        return {};
    }

    expected<token> read_negative_integer() noexcept
    {
        if (auto ec = grow_strings(); not ec)
            return ec.error();

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

        return token{ element_type::integer, id };
    }

    expected<token> read_integer() noexcept
    {
        if (auto ec = grow_strings(); not ec)
            return ec.error();

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

        return token{ element_type::integer, id };
    }

    expected<token> read_id() noexcept
    {
        if (auto ec = grow_strings(); not ec)
            return ec.error();

        const auto id  = strings_ids.alloc();
        auto&      str = strings[irt::get_index(id)];
        char       c;
        str.clear();

        while (is.get(c)) {
            if (('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or
                (static_cast<int>(c) <= '\377') or ('0' <= c and c <= '9') or
                (c == '_')) {
                str += static_cast<char>(c);
            } else {
                is.unget();
                break;
            }
        }

        return token{ element_type::id, id };
    }

    expected<token> read_double_quote() noexcept
    {
        if (auto ec = grow_strings(); not ec)
            return ec.error();

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

        return token{ element_type::double_quote, id };
    }

    /** Continue to read characters from input stream until the string \*\/ is
     * found. */
    void forget_c_comment() noexcept
    {
        char c;

        while (is.get(c)) {
            if (c == '*') {
                if (is.get(c)) {
                    if (c == '/')
                        return;
                }
            }
        }
    }

    /** Continue to read characters from input stream until the end of
     * line is found. */
    void forget_cpp_comment() noexcept
    {
        char c;

        while (is.get(c))
            if (c == '\n')
                return;
    }

    /** Continue to read characters from input stream until the end of
     * line is found. */
    void forget_line() noexcept
    {
        char c;

        while (is.get(c))
            if (c == '\n')
                return;
    }

    /** Returns the element of token ring buffer. If the buffer is
     * empty, read the @c std::istream. If the @c std::istream is empty
     * and token ring buffer is empty,  the @c element_type::none token
     * is returned.
     */
    token pop_token() noexcept
    {
        debug::ensure(not tokens.empty());

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
            return error<msg_id::missing_token>(line);

        return tokens.head()->operator[](type);
    }

    /** Returns true if the next element in the ring buffer is @c type.
     * otherwise returns false. */
    bool next_token_is_string() noexcept
    {
        if (tokens.empty())
            fill_tokens();

        if (tokens.empty())
            return error<msg_id::missing_token>(line);

        return tokens.head()->is_string();
    }

    expected<void> fill_tokens() noexcept
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
                        if (auto ret = read_negative_integer(); ret.has_error())
                            return ret.error();
                        else
                            tokens.emplace_tail(*ret);
                    }
                }
            } else if (starts_as_id(c)) {
                is.unget();
                if (auto ret = read_id(); ret.has_error())
                    return ret.error();
                else
                    tokens.emplace_tail(*ret);
            } else if (starts_as_number(c)) {
                is.unget();
                if (auto ret = read_integer(); ret.has_error())
                    return ret.error();
                else
                    tokens.emplace_tail(*ret);
            } else if (c == '\"') {
                if (auto ret = read_double_quote(); ret.has_error())
                    return ret.error();
                else
                    tokens.emplace_tail(*ret);
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

        return {};
    }

    bool empty() const noexcept
    {
        return tokens.empty() and (is.eof() or is.bad());
    }

    bool check_minimum_tokens(std::integral auto nb) noexcept
    {
        debug::ensure(std::cmp_less(nb, tokens.capacity()));

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
            return error<msg_id::missing_token>(line);

        auto        it     = tokens.head();
        const auto& first  = *it++;
        const auto& second = *it++;

        if (first.is_string()) {
            if (second[element_type::colon]) {
                if (not check_minimum_tokens(3))
                    return error<msg_id::missing_token>(line);

                const auto& third = *it++;
                if (third.is_string()) {
                    if (not check_minimum_tokens(4))
                        return error<msg_id::missing_token>(line);

                    const auto& four = *it++;

                    return four[element_type::directed_edge] or
                           four[element_type::undirected_edge];
                } else {
                    return error<msg_id::missing_token>(line);
                }
            } else {
                return second[element_type::directed_edge] or
                       second[element_type::undirected_edge];
            }
        }

        return error<msg_id::missing_token>(line);
    }

    bool next_is_attributes() noexcept
    {
        if (not check_minimum_tokens(2))
            return error<msg_id::missing_token>(line);

        auto        it     = tokens.head();
        const auto& first  = *it++;
        const auto& second = *it++;

        return first[element_type::opening_bracket] and
               (second[element_type::closing_bracket] or second.is_string());
    }

    class dot_component
    {
    public:
        static auto make(const std::string_view f) noexcept
        {
            return dot_component(std::string_view{}, std::string_view{}, f);
        }

        static auto make(const std::string_view d,
                         const std::string_view f) noexcept
        {
            return dot_component(std::string_view{}, d, f);
        }

        static auto make(const std::string_view r,
                         const std::string_view d,
                         const std::string_view f) noexcept
        {
            return dot_component(r, d, f);
        }

    private:
        dot_component(std::string_view reg_,
                      std::string_view dir_,
                      std::string_view file_) noexcept
          : reg{ reg_ }
          , dir{ dir_ }
          , file{ file_ }
        {}

    public:
        std::string_view reg;
        std::string_view dir;
        std::string_view file;
    };

    auto split_colon(const std::string_view str) noexcept -> dot_component
    {
        const auto first = str.find(':');

        if (first != std::string_view::npos and first < str.size()) {
            const auto left  = str.substr(0, first);
            const auto right = str.substr(first + 1);

            const auto second = right.find(':');
            if (second != std::string_view::npos and second < right.size()) {
                return dot_component::make(
                  left, right.substr(0, second), right.substr(second + 1));
            } else {
                return dot_component::make(left, right);
            }
        } else {
            return dot_component::make(str);
        }
    }

    component_id search_component(const std::string_view str) noexcept
    {
        const auto dot_compo = split_colon(str);

        return mod.search_component_by_name(
          dot_compo.reg, dot_compo.dir, dot_compo.file);
    }

    bool parse_attributes(const graph_node_id id) noexcept
    {
        using namespace std::string_view_literals;

        if (not check_minimum_tokens(1))
            return error<msg_id::missing_token>(line);

        auto bracket = pop_token();
        if (not bracket[element_type::opening_bracket])
            return error<msg_id::missing_token>(line);

        for (;;) {
            if (not check_minimum_tokens(1))
                return error<msg_id::missing_token>(line);

            auto left = pop_token();
            if (left[element_type::closing_bracket])
                return true;

            if (not check_minimum_tokens(3))
                return error<msg_id::missing_token>(line);

            if (not left.is_string())
                return error<msg_id::missing_token>(line);

            auto middle = pop_token();
            if (not middle[element_type::equals])
                return error<msg_id::missing_token>(line);

            auto right = pop_token();
            if (not right.is_string())
                return error<msg_id::missing_token>(line);

            const auto left_str  = get_and_free_string(left);
            const auto right_str = get_and_free_string(right);

            if (iequals(left_str, "id"sv)) {
                g.node_ids[irt::get_index(id)] = g.buffer.append(right_str);
            } else if (iequals(left_str, "area"sv)) {
                g.node_areas[irt::get_index(id)] = to_float(right_str);
            } else if (iequals(left_str, "component"sv)) {
                g.node_components[irt::get_index(id)] =
                  search_component(right_str);
            } else if (iequals(left_str, "pos"sv)) {
                g.node_positions[irt::get_index(id)] = to_2float(right_str);
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
                return error<msg_id::missing_token>(line);
            }
        }
    }

    bool parse_attributes(const graph_edge_id /*id*/) noexcept { return true; }

    bool parse_edge() noexcept
    {
        if (not check_minimum_tokens(3))
            return error<msg_id::missing_token>(line);

        const auto from = pop_token();
        if (not from.is_string())
            return error<msg_id::missing_token>(line);

        const auto from_str = get_and_free_string(from);
        const auto from_id  = find_or_add_node(from_str);
        if (from_id.has_error()) {
            ec = from_id.error();
            return error<msg_id::missing_token>(line);
        }

        std::string_view port_src;
        std::string_view port_dst;

        if (next_token_is(element_type::colon)) {
            pop_token();
            if (next_token_is(element_type::id)) {
                const auto port     = pop_token();
                const auto port_str = get_and_free_string(port);
                port_src            = add_port(port_str);
            }
        }

        const auto type = pop_token();
        if (not(type.type == element_type::directed_edge or
                type.type == element_type::undirected_edge))
            return error<msg_id::missing_token>(line);

        const auto to = pop_token();
        if (not to.is_string())
            return error<msg_id::missing_token>(line);

        const auto to_str = get_and_free_string(to);
        const auto to_id  = find_or_add_node(to_str);
        if (to_id.has_error()) {
            ec = to_id.error();
            return error<msg_id::missing_token>(line);
        }

        if (next_token_is(element_type::colon)) {
            pop_token();
            if (next_token_is(element_type::id)) {
                const auto port     = pop_token();
                const auto port_str = get_and_free_string(port);
                port_dst            = add_port(port_str);
            }
        }

        if (not g.edges.can_alloc(1)) {
            if (not g.edges.grow<2, 1>()) {
                ec = new_error(modeling_errc::dot_memory_insufficient);
                return error<msg_id::missing_token>(line);
            }

            const auto c = g.edges.capacity();
            if (not g.edges.reserve(c) or not g.edges_nodes.resize(c)) {
                ec = new_error(modeling_errc::dot_memory_insufficient);
                return error<msg_id::missing_token>(line);
            }
        }

        const auto new_edge_id                               = g.edges.alloc();
        g.edges_nodes[irt::get_index(new_edge_id)][0].first  = *from_id;
        g.edges_nodes[irt::get_index(new_edge_id)][0].second = port_src;
        g.edges_nodes[irt::get_index(new_edge_id)][1].first  = *to_id;
        g.edges_nodes[irt::get_index(new_edge_id)][1].second = port_dst;

        return next_is_attributes() ? parse_attributes(new_edge_id) : true;
    }

    bool parse_node() noexcept
    {
        if (not check_minimum_tokens(1))
            return error<msg_id::missing_token>(line);

        const auto node_id = pop_token();
        if (not node_id.is_string())
            return error<msg_id::missing_token>(line);

        const auto str = get_and_free_string(node_id);
        const auto id  = find_or_add_node(str);

        if (id.has_error()) {
            ec = id.error();
            return error<msg_id::missing_token>(line);
        }

        return next_is_attributes() ? parse_attributes(*id) : true;
    }

    bool parse_stmt_list() noexcept
    {
        if (not check_minimum_tokens(2))
            return error<msg_id::missing_token>(line);

        const auto open = pop_token();
        if (not open[element_type::opening_brace])
            return error<msg_id::missing_token>(line);

        while (check_minimum_tokens(1)) {
            if (next_token_is(element_type::closing_brace)) {
                pop_token();
                return true;
            }

            const auto success = next_is_edge() ? parse_edge() : parse_node();
            if (not success)
                return error<msg_id::missing_token>(line);

            if (check_minimum_tokens(1) and
                next_token_is(element_type::semicolon))
                pop_token();
        }

        if (not check_minimum_tokens(1))
            return error<msg_id::missing_token>(line);

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
        switch (convert_to_element_type(type)) {
        case element_type::graph:
            g.flags.reset(graph::option_flags::directed);
            return true;

        case element_type::digraph:
            g.flags.set(graph::option_flags::directed);
            return true;

        default:
            return error<msg_id::unknown_graph_type>(false, type, line);
        }

        irt::unreachable();
    }

    bool parse_graph() noexcept
    {
        if (not check_minimum_tokens(1))
            return error<msg_id::missing_token>(line);

        const auto strict_or_graph = pop_token();
        if (not strict_or_graph.is_string())
            return error<msg_id::missing_strict_or_graph>(false, line);

        const auto s = get_and_free_string(strict_or_graph);

        if (convert_to_element_type(s) == element_type::strict) {
            g.flags.set(graph::option_flags::strict);

            if (not check_minimum_tokens(1))
                return error<msg_id::missing_token>(line);

            const auto graph_type = pop_token();
            if (not parse_graph_type(graph_type))
                return error<msg_id::missing_token>(line);

        } else {
            if (not parse_graph_type(s))
                return error<msg_id::missing_token>(line);
        }

        if (not check_minimum_tokens(1))
            return error<msg_id::missing_token>(line);

        if (next_token_is_string()) {
            const auto m_id = pop_token();
            g.main_id       = g.buffer.append(get_and_free_string(m_id));
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
    expected<graph> parse() noexcept
    {
        fill_tokens();

        if (parse_graph())
            return std::move(g);

        return new_error(modeling_errc::dot_format_illegible);
    }

    // https://graphviz.org/doc/info/lang.html
    // https://www.ascii-code.com/ASCII
};

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

expected<graph> parse_dot_buffer(const modeling&        mod,
                                 const std::string_view buffer) noexcept
{
    if (buffer.empty())
        return new_error(modeling_errc::dot_buffer_empty);

    istring_view_stream isvs{ buffer.data(), buffer.size() };
    input_stream_buffer sb{ mod, isvs };

    return sb.parse();
}

expected<graph> parse_dot_file(const modeling&              mod,
                               const std::filesystem::path& p) noexcept
{
    if (std::ifstream ifs{ p }; ifs) {
        input_stream_buffer sb{ mod, ifs };
        return sb.parse();
    }

    return new_error(modeling_errc::dot_file_unreachable);
}

graph::graph(const graph& other) noexcept
  : nodes(other.nodes)
  , edges(other.edges)
  , node_names(other.node_names)
  , node_ids(other.node_ids)
  , node_positions(other.node_positions)
  , node_components(other.node_components)
  , node_areas(other.node_areas)
  , edges_nodes(other.edges_nodes)
  , main_id(other.main_id)
  , flags(other.flags)
{
    for (const auto id : other.nodes) {
        const auto idx  = get_index(id);
        node_names[idx] = buffer.append(other.node_names[idx]);
        node_ids[idx]   = buffer.append(other.node_ids[idx]);
    }

    main_id = buffer.append(other.main_id);
}

graph::graph(graph&& other) noexcept
  : nodes(std::move(other.nodes))
  , edges(std::move(other.edges))
  , node_names(std::move(other.node_names))
  , node_ids(std::move(other.node_ids))
  , node_positions(std::move(other.node_positions))
  , node_components(std::move(other.node_components))
  , node_areas(std::move(other.node_areas))
  , edges_nodes(std::move(other.edges_nodes))
  , main_id(std::move(other.main_id))
  , buffer(std::move(other.buffer))
  , file(other.file)
  , flags(other.flags)
{}

graph& graph::operator=(const graph& other) noexcept
{
    clear();
    reserve(other.nodes.size(), other.edges.size());

    nodes           = other.nodes;
    edges           = other.edges;
    node_names      = other.node_names;
    node_ids        = other.node_ids;
    node_positions  = other.node_positions;
    node_components = other.node_components;
    node_areas      = other.node_areas;
    edges_nodes     = other.edges_nodes;
    main_id         = other.main_id;
    flags           = other.flags;

    for (const auto id : other.nodes) {
        const auto idx  = get_index(id);
        node_names[idx] = buffer.append(other.node_names[idx]);
        node_ids[idx]   = buffer.append(other.node_ids[idx]);
    }

    main_id = buffer.append(other.main_id);

    return *this;
}

graph& graph::operator=(graph&& other) noexcept
{
    clear();

    nodes           = std::move(other.nodes);
    edges           = std::move(other.edges);
    node_names      = std::move(other.node_names);
    node_ids        = std::move(other.node_ids);
    node_positions  = std::move(other.node_positions);
    node_components = std::move(other.node_components);
    node_areas      = std::move(other.node_areas);
    edges_nodes     = std::move(other.edges_nodes);
    main_id         = std::move(other.main_id);
    buffer          = std::move(other.buffer);
    main_id         = other.main_id;
    flags           = other.flags;

    return *this;
}

expected<void> graph::init_scale_free_graph(double            alpha,
                                            double            beta,
                                            component_id      compo_id,
                                            int               n,
                                            std::span<u64, 4> seed,
                                            std::span<u64, 2> key) noexcept
{
    clear();

    if (const auto r = reserve(n, n * 8); r.has_error())
        return r.error();

    for (auto i = 0; i < n; ++i) {
        const auto id       = nodes.alloc();
        node_names[id]      = std::string_view();
        node_ids[id]        = std::string_view();
        node_positions[id]  = { 0.f, 0.f };
        node_components[id] = compo_id;
        node_areas[id]      = 1.f;
    }

    if (const unsigned n = nodes.max_used(); n > 1) {
        local_rng                               r(seed, key);
        std::uniform_int_distribution<unsigned> d(0u, n - 1);

        auto first = nodes.begin();
        bool stop  = false;

        while (not stop) {
            unsigned xv = d(r);
            unsigned degree =
              (xv == 0 ? 0 : unsigned(beta * std::pow(xv, -alpha)));

            while (degree == 0) {
                ++first;
                if (first == nodes.end()) {
                    stop = true;
                    break;
                }

                xv     = d(r);
                degree = (xv == 0 ? 0 : unsigned(beta * std::pow(xv, -alpha)));
            }

            if (stop)
                break;

            auto second = undefined<graph_node_id>();
            do {
                const auto idx = d(r);
                second         = nodes.get_from_index(idx);
            } while (not is_defined(second) or *first == second or
                     exists_edge(*first, second));
            --degree;

            if (not edges.can_alloc(1)) {
                if (not edges.grow<3, 2>())
                    return new_error(
                      modeling_errc::graph_children_container_full);

                if (not edges_nodes.resize(edges.capacity()))
                    return new_error(
                      modeling_errc::graph_children_container_full);
            }

            auto new_edge_id                  = edges.alloc();
            edges_nodes[new_edge_id][0].first = *first;
            edges_nodes[new_edge_id][1].first = second;
        }
    }

    return expected<void>();
}

expected<void> graph::init_small_world_graph(double            probability,
                                             i32               k,
                                             component_id      compo_id,
                                             int               n,
                                             std::span<u64, 4> seed,
                                             std::span<u64, 2> key) noexcept
{
    clear();
    if (const auto r = reserve(n, n * 8); r.has_error())
        return r.error();

    for (auto i = 0; i < n; ++i) {
        const auto id       = nodes.alloc();
        node_names[id]      = std::string_view();
        node_ids[id]        = std::string_view();
        node_positions[id]  = { 0.f, 0.f };
        node_components[id] = compo_id;
        node_areas[id]      = 1.f;
    }

    if (const auto n = nodes.max_used(); n > 1) {
        local_rng                          r(seed, key);
        std::uniform_real_distribution<>   dr(0.0, 1.0);
        std::uniform_int_distribution<int> di(0, n - 1);

        int first  = 0;
        int second = 1;
        int source = 0;
        int target = 1;

        do {
            target = (target + 1) % n;
            if (target == (source + k / 2 + 1) % n) {
                ++source;
                target = (source + 1) % n;
            }
            first = source;

            double x = dr(r);
            if (x < probability) {
                auto lower = (source + n - k / 2) % n;
                auto upper = (source + k / 2) % n;
                do {
                    second = di(r);
                } while (
                  (second >= lower && second <= upper) ||
                  (upper < lower && (second >= lower || second <= upper)));
            } else {
                second = target;
            }

            const auto vertex_first  = nodes.get_from_index(first);
            const auto vertex_second = nodes.get_from_index(second);

            if (not edges.can_alloc(1)) {
                if (not edges.grow<3, 2>())
                    return new_error(
                      modeling_errc::graph_connection_container_full);

                if (not edges_nodes.resize(edges.capacity()))
                    return new_error(
                      modeling_errc::graph_connection_container_full);
            }

            if (vertex_first == vertex_second or
                exists_edge(vertex_first, vertex_second))
                continue;

            const auto new_edge_id            = edges.alloc();
            edges_nodes[new_edge_id][0].first = vertex_first;
            edges_nodes[new_edge_id][1].first = vertex_second;
        } while (source + 1 < n);
    }

    return expected<void>();
}

expected<graph_node_id> graph::alloc_node() noexcept
{
    if (not nodes.can_alloc(1)) {
        if (not nodes.grow<2, 1>())
            return new_error(modeling_errc::graph_children_container_full);

        const auto c = nodes.capacity();

        if (not(node_names.resize(c) and node_ids.resize(c) and
                node_positions.resize(c) and node_components.resize(c) and
                node_areas.resize(c)))
            return new_error(modeling_errc::graph_children_container_full);
    }

    const auto id        = nodes.alloc();
    const auto idx       = get_index(id);
    node_names[idx]      = std::string_view();
    node_ids[idx]        = std::string_view();
    node_positions[idx]  = { 0.f, 0.f };
    node_components[idx] = undefined<component_id>();
    node_areas[idx]      = 1.f;

    return id;
}

expected<graph_edge_id> graph::alloc_edge(graph_node_id src,
                                          graph_node_id dst) noexcept
{
    for (auto id : edges)
        if (edges_nodes[id][0].first == src and edges_nodes[id][1].first == dst)
            return new_error(modeling_errc::graph_connection_already_exist);

    if (not edges.can_alloc(1)) {
        if (not edges.grow<2, 1>())
            return new_error(modeling_errc::graph_connection_container_full);

        if (not edges_nodes.resize(edges.capacity()))
            return new_error(modeling_errc::graph_connection_container_full);
    }

    const auto id             = edges.alloc();
    edges_nodes[id][0].first  = src;
    edges_nodes[id][1].first  = dst;
    edges_nodes[id][0].second = std::string_view();
    edges_nodes[id][1].second = std::string_view();
    return id;
}

expected<void> graph::reserve(int n, int e) noexcept
{
    if (not(nodes.reserve(n) and node_names.resize(n) and node_ids.resize(n) and
            node_positions.resize(n, std::array<float, 2>{ 0.f, 0.f }) and
            node_components.resize(n, undefined<component_id>()) and
            node_areas.resize(n, 1.f)))
        return new_error(modeling_errc::dot_memory_insufficient);

    if (not(edges.reserve(e) and edges_nodes.resize(e, std::array<edge, 2>())))
        return new_error(modeling_errc::dot_memory_insufficient);

    return success();
}

void graph::swap(graph& g) noexcept
{
    nodes.swap(g.nodes);
    edges.swap(g.edges);
    node_names.swap(g.node_names);
    node_ids.swap(g.node_ids);
    node_positions.swap(g.node_positions);
    node_components.swap(g.node_components);
    node_areas.swap(g.node_areas);
    edges_nodes.swap(g.edges_nodes);
    main_id.swap(g.main_id);

    std::swap(buffer, g.buffer);

    std::swap(flags, g.flags);
}

void graph::clear() noexcept
{
    nodes.clear();
    edges.clear();
    node_names.clear();
    node_ids.clear();
    node_positions.clear();
    node_components.clear();
    node_areas.clear();
    edges_nodes.clear();
    main_id = std::string_view{};

    buffer.clear();

    flags.reset();
}

irt::table<std::string_view, graph_node_id> graph::make_toc() const noexcept
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

struct reg_dir_file {
    std::string_view r, d, f;
};

static std::optional<reg_dir_file> build_component_string(
  const modeling&    mod,
  const component_id id) noexcept
{
    if (const auto* compo = mod.components.try_to_get<component>(id)) {
        auto* reg = mod.registred_paths.try_to_get(compo->reg_path);
        if (not reg or reg->path.empty() or reg->name.empty()) {
            return std::nullopt;
        }

        auto* dir = mod.dir_paths.try_to_get(compo->dir);
        if (not dir or dir->path.empty()) {
            return std::nullopt;
        }

        auto* file = mod.file_paths.try_to_get(compo->file);
        if (not file or file->path.empty()) {
            return std::nullopt;
        }

        return reg_dir_file{ .r = reg->name.sv(),
                             .d = dir->path.sv(),
                             .f = file->path.sv() };
    }

    return std::nullopt;
}

template<typename OutputIterator>
expected<void> write_dot_stream(const modeling& mod,
                                const graph&    g,
                                OutputIterator  out) noexcept
{
    if (g.flags[graph::option_flags::strict])
        out = fmt::format_to(out, "strict ");

    if (g.flags[graph::option_flags::directed])
        out = fmt::format_to(out, "digraph {} {{\n", g.main_id);
    else
        out = fmt::format_to(out, "graph {} {{\n", g.main_id);

    for (const auto id : g.nodes) {
        const auto idx = get_index(id);

        auto compo = build_component_string(mod, g.node_components[idx]);
        if (compo.has_value()) {
            out = fmt::format_to(out,
                                 "  {} [id={}, area={},"
                                 " pos=\"{},{}\","
                                 " component=\"{}:{}:{}\"];\n",
                                 g.node_names[idx],
                                 g.node_ids[idx],
                                 g.node_areas[idx],
                                 g.node_positions[idx][0],
                                 g.node_positions[idx][1],
                                 compo->r,
                                 compo->d,
                                 compo->f);
        } else {
            out = fmt::format_to(out,
                                 "  {} [id={}, area={}, pos=\"{},{}\"];\n",
                                 g.node_names[idx],
                                 g.node_ids[idx],
                                 g.node_areas[idx],
                                 g.node_positions[idx][0],
                                 g.node_positions[idx][1]);
        }
    }

    const auto edge_type = g.flags[graph::option_flags::directed] ? "->" : "--";

    for (const auto id : g.edges) {
        const auto idx = get_index(id);

        if (g.nodes.exists(g.edges_nodes[idx][0].first) and
            g.nodes.exists(g.edges_nodes[idx][1].first)) {
            const auto src = get_index(g.edges_nodes[idx][0].first);
            const auto dst = get_index(g.edges_nodes[idx][1].first);

            out = fmt::format_to(out,
                                 "  {} {} {};\n",
                                 g.node_names[src],
                                 edge_type,
                                 g.node_names[dst]);
        }
    }

    out = fmt::format_to(out, "}}");

    return expected<void>();
}

expected<void> write_dot_file(const modeling&              mod,
                              const graph&                 graph,
                              const std::filesystem::path& path) noexcept
{
    if (std::ofstream ofs(path); ofs) {
        return write_dot_stream(mod, graph, std::ostream_iterator<char>(ofs));
    } else {
        return new_error(modeling_errc::dot_file_unreachable);
    }
}

expected<vector<char>> write_dot_buffer(const modeling& mod,
                                        const graph&    graph) noexcept
{
    vector<char> buffer(4096, reserve_tag);
    if (buffer.capacity() < 4096)
        return new_error(modeling_errc::dot_memory_insufficient);

    if (auto ret =
          write_dot_stream(mod, graph, std::back_insert_iterator(buffer));
        ret.has_value())
        return buffer;
    else
        return ret.error();
}

} // namespace irt
