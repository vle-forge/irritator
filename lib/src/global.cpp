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
    v.rec_paths.write([&](recorded_paths& paths) noexcept {
        paths.ids.reserve(16);
        paths.paths.resize(16);
        paths.priorities.resize(16);
        paths.names.resize(16);

        if (auto sys = get_system_component_dir(); sys.has_value()) {
            std::error_code ec;
            if (std::filesystem::exists(*sys, ec)) {
                const auto idx        = paths.ids.alloc();
                paths.paths[idx]      = (const char*)sys->u8string().c_str();
                paths.priorities[idx] = 20;
                paths.names[idx]      = "system";
            }
        }

        if (auto sys = get_system_prefix_component_dir(); sys.has_value()) {
            std::error_code ec;
            if (std::filesystem::exists(*sys, ec)) {
                const auto idx        = paths.ids.alloc();
                paths.paths[idx]      = (const char*)sys->u8string().c_str();
                paths.priorities[idx] = 10;
                paths.names[idx]      = "p-system";
            }
        }

        if (auto sys = get_default_user_component_dir(); sys.has_value()) {
            std::error_code ec;
            if (std::filesystem::exists(*sys, ec)) {
                const auto idx        = paths.ids.alloc();
                paths.paths[idx]      = (const char*)sys->u8string().c_str();
                paths.priorities[idx] = 0;
                paths.names[idx]      = "user";
            }
        }
    });

    v.enable_notification_windows = true;
    v.theme                       = 0;
}

static std::error_code do_write(const variables& vars, std::FILE* file) noexcept
{
    fmt::print(file, "[paths]\n");

    vars.rec_paths.read(
      [&](const recorded_paths& paths, const auto /*vers*/) noexcept {
          for (const auto id : paths.ids) {
              const auto idx = get_index(id);

              if (paths.names[idx].empty() or paths.paths[idx].empty())
                  continue;

              fmt::print(file,
                         "{} {} {}\n",
                         paths.names[idx].sv(),
                         paths.priorities[idx],
                         paths.paths[idx].sv());
          }
      });

    fmt::print(file, "[options]\n");
    fmt::print(file,
               "notifications={}\n",
               vars.enable_notification_windows ? "true" : "false");

    fmt::print(file, "[themes]\n");
    fmt::print(file, "selected={}\n", themes[vars.theme]);

    return std::ferror(file)
             ? std::make_error_code(std::errc(std::errc::io_error))
             : std::error_code();
}

static std::error_code do_save(std::FILE* file, const variables& vars) noexcept
{
    fatal::ensure(file);

    fmt::print(file, "# irritator 0.x\n");
    if (std::ferror(file))
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, file);
}

static std::error_code do_save(const std::filesystem::path& filename,
                               const variables&             vars) noexcept
{
    auto file = file::open(filename, file_mode(file_open_options::write));
    if (file.has_error()) [[unlikely]]
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    return do_save(file->to_file(), vars);
}

enum : u8 { section_colors, section_options, section_paths, section_COUNT };

static bool do_read_section(variables& /*vars*/,
                            std::bitset<3>&  current_section,
                            std::string_view section) noexcept
{
    struct map {
        std::string_view name;
        int              section;
    };

    static const map m[] = { { "options", section_options },
                             { "paths", section_paths },
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

static bool do_read_affect(variables&             vars,
                           const std::bitset<3>&  current_section,
                           const std::string_view key,
                           const std::string_view val) noexcept
{
    if (current_section.test(section_colors) and key == "selected") {
        if (val == std::string_view("Modern")) {
            vars.theme = 0;
            return true;
        }
        if (val == std::string_view("Dark")) {
            vars.theme = 1;
            return true;
        }
        if (val == std::string_view("Light")) {
            vars.theme = 2;
            return true;
        }
        if (val == std::string_view("Bess Dark")) {
            vars.theme = 3;
            return true;
        }
        if (val == std::string_view("Catpuccin Mocha")) {
            vars.theme = 4;
            return true;
        }
        if (val == std::string_view("Material You")) {
            vars.theme = 5;
            return true;
        }
        if (val == std::string_view("Fluent UI")) {
            vars.theme = 6;
            return true;
        }
        if (val == std::string_view("Fluent UI - Light")) {
            vars.theme = 7;
            return true;
        } else {
            vars.theme = 0;
            return true;
        }
    }

    if (current_section.test(section_options) and key == "notifications") {
        if (val == std::string_view("true") or val == "1") {
            vars.enable_notification_windows = true;
            return true;
        }
        if (val == std::string_view("false") or val == "0") {
            vars.enable_notification_windows = false;
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
                         const std::bitset<3>&  current_section,
                         const std::string_view element) noexcept
{
    return vars.rec_paths.write([&](recorded_paths& rec_paths) noexcept {
        if (current_section.test(section_paths)) {
            if (not rec_paths.ids.can_alloc(1))
                return false;

            const auto id  = rec_paths.ids.alloc();
            const auto idx = get_index(id);

            rec_paths.priorities[idx] = 0;
            rec_paths.names[idx].clear();
            rec_paths.paths[idx].clear();

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

            rec_paths.names[idx]      = name;
            rec_paths.paths[idx]      = path;
            rec_paths.priorities[idx] = do_read_integer(priority).value_or(10);

            if (rec_paths.names[idx].empty() or rec_paths.paths[idx].empty()) {
                rec_paths.ids.free(id);
            } else if (const auto f_id = find_name(rec_paths, id);
                       is_defined(f_id)) {
                rec_paths.ids.free(f_id);
            }

            return true;
        }

        return false;
    });
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

static std::error_code do_parse(variables& v, std::string_view buffer) noexcept
{
    std::bitset<3> s;

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

static std::error_code do_load(std::FILE* file, variables& vars) noexcept
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

    return do_parse(vars, std::string_view{ buffer.data(), buffer.size() });
}

static std::error_code do_load(const std::filesystem::path& filename,
                               variables&                   vars) noexcept
{
    auto file = file::open(filename, file_mode{ file_open_options::read });
    if (file.has_error()) [[unlikely]]
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    return do_load(file->to_file(), vars);
}

vector<recorded_path_id> recorded_paths::sort_by_priorities() const noexcept
{
    vector<recorded_path_id> ret(ids.size(), reserve_tag);

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

static inline u64 get_tick_count_in_milliseconds() noexcept
{
    namespace sc = std::chrono;

    return duration_cast<sc::milliseconds>(
             sc::steady_clock::now().time_since_epoch())
      .count();
}

static bool is_expired(const u64 creation_time, const u64 duration) noexcept
{
    const auto elapsed_time = get_tick_count_in_milliseconds() - creation_time;

    return elapsed_time >= duration;
}

void journal_handler::cleanup_expired(const u64 duration) noexcept
{
    m_logs.write([&](auto& buffer) {
        for (const auto id : buffer.ring)
            if (buffer.ids.exists(id) and
                is_expired(buffer.ids[id].first, duration))
                buffer.ids.free(id);

        while (not buffer.ring.empty()) {
            const auto id = buffer.ring.front();

            if (not buffer.ids.exists(id))
                buffer.ring.dequeue();
            else
                break;
        }
    });
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

config_manager::config_manager() noexcept { do_build_default(vars); }

config_manager::config_manager(const std::string& config_path) noexcept
  : m_path{ config_path }
{
    do_build_default(vars);

    if (do_load(config_path, vars)) {
        if (not save()) {
            std::fprintf(stderr,
                         "Fail to store configuration in %s\n",
                         config_path.c_str());
        }
    }
}

std::error_code config_manager::save() noexcept
{
    return do_save(m_path, vars);
}

std::error_code config_manager::load() noexcept
{
    return do_load(m_path.c_str(), vars);
}

} // namespace irt
