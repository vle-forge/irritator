// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/global.hpp>

#include <istream>
#include <mutex>
#include <streambuf>

#include <fmt/format.h>
#include <fmt/ostream.h>

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

static std::shared_ptr<variables> do_build_default() noexcept
{
    auto v = std::make_shared<variables>();

    v->rec_paths.ids.reserve(16);
    v->rec_paths.paths.resize(16);
    v->rec_paths.priorities.resize(16);
    v->rec_paths.names.resize(16);

    if (auto sys = get_system_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx               = v->rec_paths.ids.alloc();
            v->rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v->rec_paths.priorities[idx] = 20;
            v->rec_paths.names[idx]      = "system";
        }
    }

    if (auto sys = get_system_prefix_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx               = v->rec_paths.ids.alloc();
            v->rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v->rec_paths.priorities[idx] = 10;
            v->rec_paths.names[idx]      = "p-system";
        }
    }

    if (auto sys = get_default_user_component_dir(); sys.has_value()) {
        std::error_code ec;
        if (std::filesystem::exists(*sys, ec)) {
            const auto idx               = v->rec_paths.ids.alloc();
            v->rec_paths.paths[idx]      = (const char*)sys->u8string().c_str();
            v->rec_paths.priorities[idx] = 0;
            v->rec_paths.names[idx]      = "user";
        }
    }

    return v;
}

static std::error_code do_write(const variables& vars,
                                const int        theme,
                                std::ostream&    os) noexcept
{
    if (os << "[paths]\n") {
        for (const auto id : vars.rec_paths.ids) {
            const auto idx = get_index(id);

            if (vars.rec_paths.names[idx].empty() or
                vars.rec_paths.paths[idx].empty())
                continue;

            os << vars.rec_paths.names[idx].sv() << ' '
               << vars.rec_paths.priorities[idx] << ' '
               << vars.rec_paths.paths[idx].sv() << '\n';
        }

        if (os << "[themes]\n") {
            os << "selected=" << themes[theme] << '\n';
        }
    }

    if (os.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return std::error_code();
}

static std::error_code do_save(const char*      filename,
                               const variables& vars,
                               const int        theme) noexcept
{
    auto file = std::ofstream{ filename };
    if (!file.is_open())
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    file << "# irritator 0.x\n";
    if (file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, theme, file);
}

enum { section_colors, section_paths, section_COUNT };

static bool do_read_section(variables& /*vars*/,
                            std::bitset<2>&  current_section,
                            std::string_view section) noexcept
{
    struct map {
        std::string_view name;
        int              section;
    };

    static const map m[] = { { "colors", section_colors },
                             { "paths", section_paths } };

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
    if (current_section.test(section_colors) and key == "themes") {
        if (val == std::string_view("Modern"))
            theme = 0;
        if (val == std::string_view("Dark"))
            theme = 1;
        if (val == std::string_view("Light"))
            theme = 2;
        if (val == std::string_view("Bess Dark"))
            theme = 3;
        if (val == std::string_view("Catpuccin Mocha"))
            theme = 4;
        if (val == std::string_view("Material You"))
            theme = 5;
        if (val == std::string_view("Fluent UI"))
            theme = 6;
        if (val == std::string_view("Fluent UI - Light"))
            theme = 7;
        else
            theme = 0;

        return true;
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

        imemstream in(element.data(), element.size());
        in >> vars.rec_paths.names[idx] >> vars.rec_paths.priorities[idx] >>
          vars.rec_paths.paths[idx];

        if (vars.rec_paths.names[idx].empty() or
            vars.rec_paths.paths[idx].empty()) {
            vars.rec_paths.ids.free(id);
        } else if (const auto f_id = find_name(vars.rec_paths, id);
                   is_defined(f_id)) {
            vars.rec_paths.ids.free(f_id);
        }

        return in.good();
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
            if (not do_read_affect(v,
                                   theme,
                                   s,
                                   buffer.substr(0, pos),
                                   buffer.substr(pos + 1u, new_line)))
                return std::make_error_code(std::errc::argument_out_of_domain);
            if (not in_range(buffer, new_line + 1u))
                return std::error_code();
            buffer = buffer.substr(new_line + 1u);
        } else if (buffer[pos] == '\n') { // Read elemet in list
            if (pos > 0u) {
                if (not do_read_elem(v, s, buffer.substr(0, pos - 1u)))
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

static std::error_code do_load(const char*                 filename,
                               std::shared_ptr<variables>& vars,
                               int&                        theme) noexcept
{
    auto file = std::ifstream{ filename };
    if (not file.is_open())
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    auto latest =
      std::string{ (std::istreambuf_iterator<std::string::value_type>(file)),
                   std::istreambuf_iterator<std::string::value_type>() };

    if (latest.empty() or file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    file.close();

    return do_parse(*vars, theme, latest);
}

vector<recorded_path_id> recorded_paths::sort_by_priorities() const noexcept
{
    vector<recorded_path_id> ret(ids.size(), reserve_tag{});

    if (ids.capacity() >= ids.ssize()) {
        for (auto id : ids)
            ret.emplace_back(id);

        std::sort(
          ret.begin(), ret.end(), [&](const auto a, const auto b) noexcept {
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
  : journal_handler(constrained_value<int, 4, INT_MAX>(32))
{}

journal_handler::journal_handler(
  const constrained_value<int, 4, INT_MAX> len) noexcept
  : m_ring(len.value())
{
    m_logs.reserve(len.value());
}

void journal_handler::clear() noexcept
{
    std::unique_lock lock(m_mutex);
    m_logs.clear();
}

void journal_handler::clear(std::span<entry_id> entries) noexcept
{
    std::unique_lock lock(m_mutex);
    for (const auto id : entries)
        if (m_logs.exists(id))
            m_logs.free(id);
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

void stdfile_journal_consumer::read(journal_handler& lm) noexcept
{
    lm.get(
      [](auto& logs, auto& ring, auto* out) noexcept {
          auto& titles = logs.template get<journal_handler::title>();
          auto& descrs = logs.template get<journal_handler::descr>();
          auto& levels = logs.template get<log_level>();

          for (auto i : ring) {
              if (logs.exists(i)) {
                  const auto idx = get_index(i);

                  fmt::print(out,
                             "- {}: {}\n  {}\n",
                             log_level_names[ordinal(levels[idx])],
                             titles[idx].sv(),
                             descrs[idx].sv());
              }
          }

          logs.clear();
          ring.clear();
      },
      m_fp);
}

config_manager::config_manager() noexcept
  : m_vars{ do_build_default() }
{}

config_manager::config_manager(const std::string config_path) noexcept
  : m_path{ config_path }
  , m_vars{ do_build_default() }
{
    if (do_load(config_path.c_str(), m_vars, theme)) {
        (void)save();
    }
}

std::error_code config_manager::save() const noexcept
{
    std::shared_lock lock(m_mutex);

    return do_save(m_path.c_str(), *m_vars, theme);
}

std::error_code config_manager::load() noexcept
{
    auto ret             = std::make_shared<variables>();
    auto is_load_success = do_load(m_path.c_str(), ret, theme);
    if (not is_load_success)
        return is_load_success;

    std::unique_lock lock(m_mutex);
    std::swap(ret, m_vars);

    return std::error_code();
}

void config_manager::swap(std::shared_ptr<variables>& other) noexcept
{
    std::unique_lock lock(m_mutex);

    std::swap(m_vars, other);
}

std::shared_ptr<variables> config_manager::copy() const noexcept
{
    std::shared_lock lock(m_mutex);

    return std::make_shared<variables>(*m_vars.get());
}

} // namespace irt
