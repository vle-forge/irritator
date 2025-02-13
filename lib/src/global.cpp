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

static gui_theme_id alloc_gui_theme(const std::string_view str,
                                    gui_themes&            g) noexcept
{
    const auto id  = g.ids.alloc();
    const auto idx = get_index(id);

    g.names[idx] = str;

    return id;
}

static gui_theme_id get_theme(const gui_themes&      g,
                              const std::string_view str) noexcept
{
    for (auto id : g.ids)
        if (g.names[get_index(id)].sv() == str)
            return id;

    return undefined<gui_theme_id>();
}

static std::shared_ptr<variables> do_build_default() noexcept
{
    auto v = std::make_shared<variables>();

    v->rec_paths.ids.reserve(16);
    v->rec_paths.paths.resize(16);
    v->rec_paths.priorities.resize(16);
    v->rec_paths.names.resize(16);

    if (auto sys = get_system_component_dir(); sys.has_value()) {
        const auto idx = v->rec_paths.ids.alloc();
        v->rec_paths.paths[get_index(idx)] =
          (const char*)sys->u8string().c_str();
        v->rec_paths.priorities[get_index(idx)] = 20;
        v->rec_paths.names[get_index(idx)]      = "system";
    }

    if (auto sys = get_system_prefix_component_dir(); sys.has_value()) {
        const auto idx = v->rec_paths.ids.alloc();
        v->rec_paths.paths[get_index(idx)] =
          (const char*)sys->u8string().c_str();
        v->rec_paths.priorities[get_index(idx)] = 10;
        v->rec_paths.names[get_index(idx)]      = "local system";
    }

    if (auto sys = get_default_user_component_dir(); sys.has_value()) {
        const auto idx = v->rec_paths.ids.alloc();
        v->rec_paths.paths[get_index(idx)] =
          (const char*)sys->u8string().c_str();
        v->rec_paths.priorities[get_index(idx)] = 0;
        v->rec_paths.names[get_index(idx)]      = "user";
    }

    v->g_themes.ids.reserve(16);
    v->g_themes.names.resize(16);
    v->g_themes.selected = alloc_gui_theme("Modern", v->g_themes);
    alloc_gui_theme("Dark", v->g_themes);
    alloc_gui_theme("Light", v->g_themes);
    alloc_gui_theme("Bess Dark", v->g_themes);
    alloc_gui_theme("Catpuccin Mocha", v->g_themes);
    alloc_gui_theme("Material You", v->g_themes);
    alloc_gui_theme("Fluent UI", v->g_themes);
    alloc_gui_theme("Fluent UI - Light", v->g_themes);

    return v;
}

static std::error_code do_write(const variables& vars,
                                std::ostream&    os) noexcept
{
    if (os << "[paths]\n") {
        for (const auto id : vars.rec_paths.ids) {
            const auto idx = get_index(id);

            os << vars.rec_paths.names[idx].sv() << ' '
               << vars.rec_paths.priorities[idx] << ' '
               << vars.rec_paths.paths[idx].sv() << '\n';
        }

        if (os << "[themes]\n") {
            const auto id = vars.g_themes.ids.exists(vars.g_themes.selected)
                              ? vars.g_themes.selected
                              : *vars.g_themes.ids.begin();

            os << "selected=" << vars.g_themes.names[get_index(id)].sv()
               << '\n';
        }
    }

    if (os.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return std::error_code();
}

static std::error_code do_save(const char*      filename,
                               const variables& vars) noexcept
{
    auto file = std::ofstream{ filename };
    if (!file.is_open())
        return std::make_error_code(
          std::errc(std::errc::no_such_file_or_directory));

    file << "# irritator 0.x\n";
    if (file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, file);
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

static bool do_read_affect(variables&       vars,
                           std::bitset<2>&  current_section,
                           std::string_view key,
                           std::string_view val) noexcept
{
    if (current_section.test(section_colors) and key == "themes") {
        for (const auto id : vars.g_themes.ids) {
            const auto idx = get_index(id);

            if (vars.g_themes.names[idx] == val) {
                vars.g_themes.selected = id;
                return true;
            }
        }

        if (vars.g_themes.ids.ssize() != 0)
            vars.g_themes.selected = *(vars.g_themes.ids.begin());
    }

    return false;
}

static bool do_read_elem(variables&       vars,
                         std::bitset<2>&  current_section,
                         std::string_view element) noexcept
{
    if (current_section.test(section_colors)) {
        if (auto id = get_theme(vars.g_themes, element); is_defined(id)) {
            vars.g_themes.selected = id;
            return true;
        }

        return false;
    }

    if (current_section.test(section_paths)) {
        if (not vars.rec_paths.ids.can_alloc(1))
            return false;

        const auto id  = vars.rec_paths.ids.alloc();
        const auto idx = get_index(id);

        vars.rec_paths.priorities[idx] = 0;
        vars.rec_paths.names[idx].clear();
        vars.rec_paths.paths[idx].clear();

        imemstream in(element.data(), element.size());
        in >> vars.rec_paths.priorities[idx] >> vars.rec_paths.names[idx] >>
          vars.rec_paths.paths[idx];

        return in.good();
    }

    return false;
}

static std::error_code do_parse(std::shared_ptr<variables>& v,
                                std::string_view            buffer) noexcept
{
    std::bitset<2> s;

    auto pos = buffer.find_first_of(";#[=\n");
    if (pos != std::string_view::npos) {

        if (buffer[pos] == '#' or buffer[pos] == ';') { // A comment
            if (pos + 1u >= buffer.size()) {
                if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            auto new_line = buffer.find('\n', pos + 1);
            if (new_line == std::string_view::npos or
                new_line + 1u >= buffer.size()) {
                if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                return std::make_error_code(std::errc::argument_out_of_domain);
            buffer = buffer.substr(new_line);
        } else if (buffer[pos] == '[') { // Read a new section
            if (pos + 1u >= buffer.size())
                return std::make_error_code(std::errc::io_error);

            const auto end_section = buffer.find(']', pos + 1u);
            if (end_section == std::string_view::npos)
                return std::make_error_code(std::errc::io_error);

            auto sec = buffer.substr(pos + 1u, end_section - pos + 1u);
            if (not do_read_section(*v, s, sec))
                return std::make_error_code(std::errc::argument_out_of_domain);
            buffer = buffer.substr(end_section + 1u);

        } else if (buffer[pos] == '=') { // Read affectation
            if (pos + 1u >= buffer.size()) {
                if (not do_read_affect(
                      *v, s, buffer.substr(0, pos), std::string_view()))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            auto new_line = buffer.find('\n', pos + 1u);
            if (new_line == std::string_view::npos or
                new_line + 1 >= buffer.size()) {
                if (not do_read_affect(
                      *v, s, buffer.substr(0, pos), buffer.substr(pos + 1u)))
                    return std::make_error_code(
                      std::errc::argument_out_of_domain);
                return std::error_code();
            }

            if (not do_read_affect(*v,
                                   s,
                                   buffer.substr(0, pos),
                                   buffer.substr(pos + 1u, new_line)))
                return std::make_error_code(std::errc::argument_out_of_domain);

            buffer = buffer.substr(new_line + 1u);

        } else if (buffer[pos] == '\n') { // Read elemet in list
            if (not do_read_elem(*v, s, buffer.substr(0, pos)))
                return std::make_error_code(std::errc::argument_out_of_domain);

            if (pos + 1u >= buffer.size())
                return std::error_code();

            buffer = buffer.substr(pos + 1u);
        }
    }

    return std::error_code();
}

static std::error_code do_load(const char*                 filename,
                               std::shared_ptr<variables>& vars) noexcept
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

    return do_parse(vars, latest);
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

config_manager::config_manager() noexcept
  : m_vars{ do_build_default() }
{}

config_manager::config_manager(const std::string config_path) noexcept
  : m_path{ config_path }
  , m_vars{ do_build_default() }
{
    if (do_load(config_path.c_str(), m_vars)) {
        (void)save();
    }
}

std::error_code config_manager::save() const noexcept
{
    std::shared_lock lock(m_mutex);

    return do_save(m_path.c_str(), *m_vars);
}

std::error_code config_manager::load() noexcept
{
    auto ret             = std::make_shared<variables>();
    auto is_load_success = do_load(m_path.c_str(), ret);
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
