// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/global.hpp>

#include <charconv>
#include <istream>
#include <streambuf>

#include <fmt/format.h>

namespace irt {

struct membuf : std::streambuf {
    membuf(char const* base, size_t size)
    {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
};

struct imemstream
  : virtual membuf
  , std::istream {
    imemstream(char const* base, size_t size)
      : membuf(base, size)
      , std::istream(static_cast<std::streambuf*>(this))
    {}
};

template<typename Iterator, typename T, typename Compare>
constexpr Iterator sorted_vector_find(Iterator begin,
                                      Iterator end,
                                      const T& value,
                                      Compare  comp)
{
    begin = std::lower_bound(begin, end, value, comp);
    return (begin != end && !comp(value, *begin)) ? begin : end;
}

static void do_build_default(variables& v) noexcept
{
    v.rec_paths.ids.reserve(16);
    v.rec_paths.paths.resize(16);
    v.rec_paths.priorities.resize(16);
    v.rec_paths.names.resize(16);

    if (auto sys = get_system_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx              = v.rec_paths.ids.alloc();
            v.rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v.rec_paths.priorities[idx] = 20;
            v.rec_paths.names[idx]      = "system";
        }
    }

    if (auto sys = get_system_prefix_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx              = v.rec_paths.ids.alloc();
            v.rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v.rec_paths.priorities[idx] = 10;
            v.rec_paths.names[idx]      = "p-system";
        }
    }

    if (auto sys = get_default_user_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx              = v.rec_paths.ids.alloc();
            v.rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v.rec_paths.priorities[idx] = 0;
            v.rec_paths.names[idx]      = "user";
        }
    }
}

static std::error_code do_write(const variables& vars,
                                const int        theme,
                                std::FILE*       file) noexcept
{
    fmt::print(file, "[paths]\n");

    for (const auto id : vars.rec_paths.ids) {
        const auto idx = get_index(id);

        if (vars.rec_paths.names[idx].empty() or
            vars.rec_paths.paths[idx].empty())
            continue;

        fmt::print(file,
                   "{} {} {}\n",
                   vars.rec_paths.names[idx].sv(),
                   vars.rec_paths.priorities[idx],
                   vars.rec_paths.paths[idx].sv());
    }

    fmt::print(file, "[themes]\n");
    fmt::print(file, "selected={}\n", themes[theme]);

    return std::ferror(file)
             ? std::make_error_code(std::errc(std::errc::io_error))
             : std::error_code();
}

static std::error_code do_save(std::FILE*       file,
                               const variables& vars,
                               const int        theme) noexcept
{
    fatal::ensure(file);

    fmt::print(file, "# irritator 0.x\n");
    if (std::ferror(file))
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, theme, file);
}

static std::error_code do_save(const std::filesystem::path& filename,
                               const variables&             vars,
                               const int                    theme) noexcept
{
    auto file = file::open(filename, file_mode(file_open_options::write));
    if (file.has_error()) [[unlikely]]
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    return do_save(file->to_file(), vars, theme);
}

enum : u8 { section_colors, section_paths, section_COUNT };

static bool do_read_section(variables& /*vars*/,
                            std::bitset<2>&  current_section,
                            std::string_view section) noexcept
{
    struct map {
        std::string_view name;
        int              section;
    };

    static const map m[] = { { "paths", section_paths },
                             { "themes", section_colors } };

    auto it = sorted_vector_find(
      std::begin(m), std::end(m), section, [](auto a, auto b) noexcept {
          if constexpr (std::is_same_v<decltype(a), std::string_view>)
              return a < b.name;
          else
              return a.name < b;
      });

    if (it == std::end(m))
        return false;

    current_section.reset();
    current_section.set(it->section);
    return true;
}

static bool do_read_affect(variables& /*vars*/,
                           int&                   theme,
                           const std::bitset<2>&  current_section,
                           const std::string_view key,
                           const std::string_view val) noexcept
{
    if (current_section.test(section_colors) and key == "selected") {
        if (val == std::string_view("Modern")) {
            theme = 0;
            return true;
        }
        if (val == std::string_view("Dark")) {
            theme = 1;
            return true;
        }
        if (val == std::string_view("Light")) {
            theme = 2;
            return true;
        }
        if (val == std::string_view("Bess Dark")) {
            theme = 3;
            return true;
        }
        if (val == std::string_view("Catpuccin Mocha")) {
            theme = 4;
            return true;
        }
        if (val == std::string_view("Material You")) {
            theme = 5;
            return true;
        }
        if (val == std::string_view("Fluent UI")) {
            theme = 6;
            return true;
        }
        if (val == std::string_view("Fluent UI - Light")) {
            theme = 7;
            return true;
        } else {
            theme = 0;
            return true;
        }
    }

    return false;
}

static recorded_path_id find_name(const recorded_paths&  rec_paths,
                                  const recorded_path_id search) noexcept
{
    const auto search_idx = get_index(search);

    for (const auto id : rec_paths.ids) {
        if (id != search) {
            const auto idx = get_index(id);
            if (rec_paths.names[idx] == rec_paths.names[search_idx])
                return id;
        }
    }

    return undefined<recorded_path_id>();
}

std::optional<int> do_read_integer(const std::string_view value) noexcept
{
    int i = 0;
    if (const auto ec =
          std::from_chars(value.data(), value.data() + value.size(), i);
        ec.ec == std::errc{}) {
        return i;
    } else {
        return std::nullopt;
    }
}

static bool do_read_elem(variables&             vars,
                         const std::bitset<2>&  current_section,
                         const std::string_view element) noexcept
{
    if (current_section.test(section_paths)) {
        if (not vars.rec_paths.ids.can_alloc(1))
            return false;

        const auto id  = vars.rec_paths.ids.alloc();
        const auto idx = get_index(id);

        vars.rec_paths.priorities[idx] = 0;
        vars.rec_paths.names[idx].clear();
        vars.rec_paths.paths[idx].clear();

        const auto sep_1 = element.find(' ');
        if (sep_1 == std::string_view::npos or sep_1 + 1 > element.size())
            return false;

        const auto name              = element.substr(0, sep_1);
        const auto priority_and_path = element.substr(sep_1 + 1);
        const auto sep_2             = priority_and_path.find(' ');
        if (sep_2 == std::string_view::npos or
            sep_2 + 1 > priority_and_path.size())
            return false;

        const auto priority = priority_and_path.substr(0, sep_2);
        const auto path     = priority_and_path.substr(sep_2 + 1);

        vars.rec_paths.names[idx]      = name;
        vars.rec_paths.paths[idx]      = path;
        vars.rec_paths.priorities[idx] = do_read_integer(priority).value_or(10);

        if (vars.rec_paths.names[idx].empty() or
            vars.rec_paths.paths[idx].empty()) {
            vars.rec_paths.ids.free(id);
        } else if (const auto f_id = find_name(vars.rec_paths, id);
                   is_defined(f_id)) {
            vars.rec_paths.ids.free(f_id);
        }

        return true;
    }

    return false;
}

/**
 * @brief Check if a position is in a range of a buffer.
 * @param buffer the buffer to test.
 * @param pos the position [0, std::string_view::npos].
 * @return True is pos is in range [0, buffer.size()[.
 */
static constexpr bool in_range(const std::string_view      buffer,
                               std::string_view::size_type pos) noexcept
{
    return buffer.empty() ? false : (pos <= buffer.size() - 1);
}

namespace {

/**
 * @brief An anonymous namespace to test the in_range function.
 */
using namespace std::string_view_literals;

static_assert(in_range("totoa"sv, 0));
static_assert(in_range("totoa"sv, 1));
static_assert(in_range("totoa"sv, 2));
static_assert(in_range("totoa"sv, 3));
static_assert(in_range("totoa"sv, 4));
static_assert(not in_range("totoa"sv, -1));
static_assert(not in_range("totoa"sv, 5));

}

static std::error_code do_parse(variables&       v,
                                int&             theme,
                                std::string_view buffer) noexcept
{
    std::bitset<2> s;

    for (auto pos = buffer.find_first_of(";#[=\n");
         pos != std::string_view::npos;
         pos = buffer.find_first_of(";#[=\n")) {

        if (buffer[pos] == '#' or buffer[pos] == ';') { // A comment
            if (not in_range(buffer, pos + 1u))
                return std::error_code();

            const auto new_line = buffer.find('\n', pos + 1u);
            if (not in_range(buffer, new_line + 1u))
                return std::error_code();

            buffer = buffer.substr(new_line + 1u);
        } else if (buffer[pos] == '[') { // Read a new section
            if (not in_range(buffer, pos + 1u))
                return std::make_error_code(std::errc::io_error);

            const auto end_section = buffer.find(']', pos + 1u);
            if (not in_range(buffer, end_section))
                return std::make_error_code(std::errc::io_error);

            auto sec = buffer.substr(pos + 1u, end_section - pos - 1u);
            if (not do_read_section(v, s, sec))
                return std::make_error_code(std::errc::argument_out_of_domain);
            buffer = buffer.substr(end_section + 1u);
        } else if (buffer[pos] == '=') { // Read affectation
            if (not in_range(buffer, pos + 1u))
                return std::make_error_code(std::errc::io_error);

            auto new_line = buffer.find('\n', pos + 1u);
            if (not do_read_affect(
                  v,
                  theme,
                  s,
                  buffer.substr(0, pos),
                  buffer.substr(pos + 1u, new_line - pos - 1u)))
                return std::make_error_code(std::errc::argument_out_of_domain);
            if (not in_range(buffer, new_line + 1u))
                return std::error_code();
            buffer = buffer.substr(new_line + 1u);
        } else if (buffer[pos] == '\n') { // Read elemet in list
            if (pos > 0u) {
                if (not do_read_elem(v, s, buffer.substr(0, pos)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);

                if (not in_range(buffer, pos + 1u))
                    return std::error_code();
            }

            buffer = buffer.substr(pos + 1u);
        }
    }

    return std::error_code();
}

static std::error_code do_load(std::FILE* file,
                               variables& vars,
                               int&       theme) noexcept
{
    std::fseek(file, 0u, SEEK_END);
    auto size = std::ftell(file);

    if (size == -1)
        return std::make_error_code(std::errc(std::errc::io_error));
    else if (size == 0)
        return std::error_code{};

    std::fseek(file, 0u, SEEK_SET);

    vector<char> buffer(size, '\0');

    const auto read = std::fread(buffer.data(), 1u, size, file);
    if (std::cmp_not_equal(read, size))
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_parse(
      vars, theme, std::string_view{ buffer.data(), buffer.size() });
}

static std::error_code do_load(const std::filesystem::path& filename,
                               variables&                   vars,
                               int&                         theme) noexcept
{
    auto file = file::open(filename, file_mode{ file_open_options::read });
    if (file.has_error()) [[unlikely]]
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    return do_load(file->to_file(), vars, theme);
}

vector<recorded_path_id> recorded_paths::sort_by_priorities() const noexcept
{
    vector<recorded_path_id> ret(ids.size(), reserve_tag);

    if (ids.capacity() >= ids.ssize()) {
        for (auto id : ids)
            ret.emplace_back(id);

        const auto view = ret.view();
        std::sort(
          view.begin(), view.end(), [&](const auto a, const auto b) noexcept {
              const auto idx_a = get_index(a);
              const auto idx_b = get_index(b);

              return priorities[idx_a] < priorities[idx_b];
          });
    }

    return ret;
}

stdfile_journal_consumer::~stdfile_journal_consumer() noexcept
{
    debug::ensure(m_fp);

    if (not(m_fp == stdout or m_fp == stderr))
        std::fclose(m_fp);
}

stdfile_journal_consumer::stdfile_journal_consumer(
  const std::filesystem::path& path) noexcept
{
    try {
        std::FILE*  out       = nullptr;
        const auto* unicode_p = path.c_str();
        const auto* p         = reinterpret_cast<const char*>(unicode_p);

#if defined(_WIN32)
        if (::_wfopen_s(&out, unicode_p, L"w") == 0) {
            m_fp = out;
        }
#else
        if (out = std::fopen(path.c_str(), "w"); out) {
            m_fp = out;
        }
#endif
        else {
            fmt::print(stderr, "- logging: fail to open {}\n", p);
        }
    } catch (...) {
        fmt::print(stderr, "- logging: memory error\n");
    }

    fmt::print(m_fp, "irritator start logging\n");
}

journal_handler::journal_handler() noexcept
  : journal_handler(reserve_constraint(32u))
{}

journal_handler::journal_handler(reserve_constraint len) noexcept
{
    m_logs.write(
      [](auto& buffer, const auto cap) noexcept {
          buffer.ring.reserve(cap);
          buffer.ids.reserve(cap);
          buffer.titles.resize(cap);
          buffer.descriptions.resize(cap);
      },
      len.value());
}

void journal_handler::clear() noexcept
{
    m_logs.write([](auto& buffer) noexcept {
        buffer.ring.clear();
        buffer.ids.clear();
    });
}

u64 journal_handler::get_tick_count_in_milliseconds() noexcept
{
    namespace sc = std::chrono;

    return duration_cast<sc::milliseconds>(
             sc::steady_clock::now().time_since_epoch())
      .count();
}

u64 journal_handler::get_elapsed_time(const u64 from) noexcept
{
    return get_tick_count_in_milliseconds() - from;
}

unsigned journal_handler::capacity() const noexcept
{
    auto capacity = 0u;

    m_logs.read([](const auto& buffer,
                   const auto /*version*/,
                   auto& capacity) { capacity = buffer.ring.capacity(); },
                capacity);

    return capacity;
}

unsigned journal_handler::size() const noexcept
{
    auto len = 0u;

    m_logs.read([](const auto& buffer,
                   const auto /*version*/,
                   auto& size) { size = buffer.ring.size(); },
                len);

    return len;
}

int journal_handler::ssize() const noexcept
{
    auto len = 0;

    m_logs.read([](const auto& buffer,
                   const auto /*version*/,
                   auto& size) { size = buffer.ring.ssize(); },
                len);

    return len;
}

void stdfile_journal_consumer::read(journal_handler& lm) noexcept
{
    lm.flush(
      [](auto& ring,
         auto& ids,
         auto& titles,
         auto& descriptions,
         auto* out) noexcept {
          for (auto i : ring) {
              if (ids.exists(i)) {
                  fmt::print(out,
                             "- {}: {}\n  {}\n",
                             log_level_names[ordinal(ids[i].second)],
                             titles[i].sv(),
                             descriptions[i].sv());
              }
          }
      },
      m_fp);
}

config_manager::config_manager() noexcept
{
    m_vars.write([&](auto& buffer) noexcept { do_build_default(buffer); });
}

config_manager::config_manager(const std::string& config_path) noexcept
  : m_path{ config_path }
{
    m_vars.write([&](auto& buffer) noexcept {
        do_build_default(buffer);
        if (do_load(config_path, buffer, theme)) {
            if (not save()) {
                std::fprintf(stderr,
                             "Fail to store configuration in %s\n",
                             config_path.c_str());
            }
        }
    });
}

std::error_code config_manager::save() const noexcept
{
    std::error_code ret;

    m_vars.read(
      [&](const auto& buffer, const auto version, auto& ret) noexcept {
          if (version != m_version)
              ret = do_save(m_path, buffer, theme);
      },
      ret);

    return ret;
}

std::error_code config_manager::load() noexcept
{
    std::error_code ret;

    m_vars.write(
      [&](auto& buffer, auto& ret) noexcept {
          variables temp;

          if (auto is_load_success = do_load(m_path.c_str(), temp, theme);
              not is_load_success) {
              ret = is_load_success;
          } else {
              std::swap(buffer, temp);
          }
      },
      ret);

    return ret;
}

void config_manager::swap(variables& other) noexcept
{
    m_vars.write([&](auto& buffer) noexcept { std::swap(buffer, other); });
}

variables config_manager::copy() const noexcept
{
    variables ret;

    m_vars.read([&](const auto& buffer,
                    const auto /*version*/,
                    auto& ret) noexcept { ret = buffer; },
                ret);

    return ret;
}

} // namespace irt
