// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/format.hpp>
#include <irritator/global.hpp>

#include <filesystem>
#include <istream>
#include <mutex>
#include <streambuf>

#if defined(_WIN32)
#include <KnownFolders.h>
#include <shlobj.h>
#include <wchar.h>
#include <windows.h>
#endif

namespace irt {

class config_home_manager
{
private:
    bool m_log = false;

    template<typename S, typename... Args>
    constexpr void log(int indent, const S& s, Args&&... args) noexcept
    {
        if (m_log) {
            fmt::print(stderr, "{:{}}", "", indent);
            fmt::vprint(stderr, s, fmt::make_format_args(args...));
        }
    }

public:
    config_home_manager(bool use_log) noexcept
      : m_log{ use_log }
    {
#if defined(VERSION_TWEAK) and (0 - VERSION_TWEAK - 1) != 1
        log(0,
            "irritator-{}.{}.{}-{}\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_PATCH,
            VERSION_TWEAK);

#else
        log(0,
            "irritator-{}.{}.{}\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_PATCH);
#endif
    }

    std::pair<bool, std::string> operator()(const char* dir_name,
                                            const char* subdir_name,
                                            const char* file_name) noexcept
    {
        debug::ensure(dir_name);
        debug::ensure(subdir_name);
        debug::ensure(file_name);

        auto path = std::filesystem::path(dir_name);
        log(0, "- check directory: {}\n", path.string());

        if (not is_directory_and_usable(path)) {
            log(1, "Is not a directory or bad permissions\n");
            return { false, std::string{} };
        }

        path /= subdir_name;
        log(1, "- {}\n", path.string());
        if (not is_directory_and_usable(path)) {
            log(2, "Directory not exists and not usable try to fix\n");
            if (not create_dir(path)) {
                log(3, "Fail to create directory or change permissions\n");
                return { false, std::string{} };
            }
        }

        path /= file_name;
        log(1, "- {}\n", path.string());
        if (not is_file_and_usable(path)) {
            log(2, "Fail to read or create the file. Abort.\n");
            return { false, std::string{} };
        }

        log(1, "- irritator config file configured:\n", path.string());
        return { true, path.string() };
    }

private:
    bool try_change_file_permission(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_read |
                                       std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add,
                                     ec);

        return not ec.value();
    }

    bool try_change_directory_permission(
      const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::add,
                                     ec);

        return ec.value();
    }

    bool is_directory_and_usable(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        auto status = std::filesystem::status(path, ec);
        if (ec)
            return false;

        if (status.type() != std::filesystem::file_type::directory)
            return false;

        auto perms = status.permissions();
        if (std::filesystem::perms::none !=
            (perms & std::filesystem::perms::owner_all))
            return true;

        return try_change_directory_permission(path);
    }

    bool create_dir(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;
        if (not std::filesystem::create_directory(path, ec))
            return false;

        auto perms = std::filesystem::status(path, ec).permissions();
        if (ec)
            return false;

        return std::filesystem::perms::none !=
               (perms & std::filesystem::perms::owner_all);
    }

    bool is_file_and_usable(const std::filesystem::path& path) noexcept
    {
        std::error_code ec;

        auto status = std::filesystem::status(path, ec);
        if (ec)
            return false;

        if (status.type() == std::filesystem::file_type::not_found) {
            std::ofstream ofs(path.string());
            return ofs.is_open();
        }

        if (status.type() != std::filesystem::file_type::regular)
            return false;

        auto perms = status.permissions();
        if (std::filesystem::perms::none !=
            (perms & (std::filesystem::perms::owner_read |
                      std::filesystem::perms::owner_write)))
            return true;

        return try_change_file_permission(path);
    }
};

std::string get_config_home(bool log) noexcept
{
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    config_home_manager m(log);
    small_string<64>    home_dir;

    format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);
    if (auto* xdg = getenv("XDG_CONFIG_HOME"); xdg)
        if (auto ret = m(xdg, home_dir.c_str(), "config.ini"); ret.first)
            return ret.second;

    if (auto* home = getenv("HOME"); home) {
        auto p = std::filesystem::path(home);
        p /= ".config";

        format(home_dir, "irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);
        if (auto ret = m(p.c_str(), home_dir.c_str(), "config.ini"); ret.first)
            return ret.second;

        format(home_dir, ".irritator-{}.{}", VERSION_MAJOR, VERSION_MINOR);
        if (auto ret = m(home, home_dir.c_str(), "config.ini"); ret.first)
            return ret.second;
    }

    std::error_code ec;
    if (auto path = std::filesystem::current_path(ec); ec)
        if (auto ret = m(path.c_str(), home_dir.c_str(), "config.ini");
            ret.first)
            return ret.second;

    if (auto ret = m(".", home_dir.c_str(), "config.ini"); ret.first)
        return ret.second;

    return "config.ini";
#elif defined(_WIN32)
    PWSTR path{ nullptr };

    if (SUCCEEDED(::SHGetKnownFolderPath(
                    FOLDERID_LocalAppData, 0, nullptr, &path) >= 0)) {
        std::filesystem::path ret;
        ret = path;
        ::CoTaskMemFree(path);
        return ret.string();
    } else {
        std::error_code ec;
        if (auto ret = std::filesystem::current_path(ec); !ec)
            if (auto exists = std::filesystem::exists(ret, ec); !ec && exists)
                return ret.string();

        return "config.ini";
    }
#endif
}

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

static float clamp(const float v, const float m, const float M) noexcept
{
    if (v <= m)
        return m;

    if (M <= v)
        return M;

    return v;
}

static inline rgba_color lerp(const rgba_color& a,
                              const rgba_color& b,
                              const float       t) noexcept
{
    const auto rr = (float)a.r + ((float)b.r - (float)a.r) * t * 255.f;
    const auto rg = (float)a.g + ((float)b.g - (float)a.g) * t * 255.f;
    const auto rb = (float)a.b + ((float)b.b - (float)a.b) * t * 255.f;
    const auto ra = (float)a.a + ((float)b.a - (float)a.a) * t * 255.f;

    return rgba_color{ (u8)clamp(rr, 0.f, 255.f),
                       (u8)clamp(rg, 0.f, 255.f),
                       (u8)clamp(rb, 0.f, 255.f),
                       (u8)clamp(ra, 0.f, 255.f) };
}

template<typename Iterator, typename T, typename Compare>
constexpr Iterator sorted_vector_find(Iterator begin,
                                      Iterator end,
                                      const T& value,
                                      Compare  comp)
{
    begin = std::lower_bound(begin, end, value, comp);
    return (begin != end && !comp(value, *begin)) ? begin : end;
}

//

static std::shared_ptr<const variables> do_build_default() noexcept
{
    auto v = std::make_shared<variables>();

    v->g_themes.ids.reserve(16);
    v->g_themes.colors.resize(16);
    v->g_themes.names.resize(16);

    {
        const auto id           = v->g_themes.ids.alloc();
        const auto idx          = get_index(id);
        v->g_themes.names[idx]  = "Default";
        v->g_themes.colors[idx] = { { { 255u, 255u, 255u, 255u } } };

        v->g_themes.selected = id;
    }

    {
        const auto id           = v->g_themes.ids.alloc();
        const auto idx          = get_index(id);
        v->g_themes.names[idx]  = "Dark";
        v->g_themes.colors[idx] = { { { 255u, 255u, 255u, 255u } } };
    }

    {
        const auto id           = v->g_themes.ids.alloc();
        const auto idx          = get_index(id);
        v->g_themes.names[idx]  = "Light";
        v->g_themes.colors[idx] = { { { 255u, 255u, 255u, 255u } } };
    }

    return std::static_pointer_cast<const variables>(v);
}

static std::error_code do_write(const variables& vars,
                                std::ostream&    os) noexcept
{
    os << "[paths]\n";

    for (const auto id : vars.rec_paths.ids) {
        const auto idx = get_index(id);

        os << vars.rec_paths.names[idx].sv() << ' '
           << vars.rec_paths.priorities[idx] << ' '
           << vars.rec_paths.paths[idx].sv() << '\n';
    }

    os << "[themes]\n";
    for (const auto id : vars.g_themes.ids) {
        const auto idx = get_index(id);

        os << vars.g_themes.names[idx].sv() << ' ';
        for (const auto elem : vars.g_themes.colors[idx])
            os << elem.r << ' ' << elem.g << ' ' << elem.b << ' ' << elem.a
               << ' ';
    }

    if (vars.g_themes.ids.get(vars.g_themes.selected)) {
        const auto idx = get_index(vars.g_themes.selected);
        os << "selected=" << vars.g_themes.names[idx].sv() << '\n';
    }

    os << '\n';

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

    file << "# irritator 0.x\n;\n";
    if (file.bad())
        return std::make_error_code(std::errc(std::errc::io_error));

    return do_write(vars, file);
}

static bool do_read_section(variables& /*vars*/,
                            std::bitset<2>&  current_section,
                            std::string_view section) noexcept
{
    struct map {
        std::string_view name;
        int              section;
    };

    static const map m[] = { { "colors", 0 }, { "paths", 1 } };

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
    if (current_section.test(0) and key == "themes") {
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
    if (current_section.test(0)) {
        if (not vars.g_themes.ids.can_alloc(1))
            return false;

        const auto id  = vars.g_themes.ids.alloc();
        const auto idx = get_index(id);

        std::fill_n(vars.g_themes.colors[idx].data(),
                    vars.g_themes.colors[idx].size(),
                    rgba_color{ 255u, 255u, 255u, 255u });

        vars.g_themes.names[idx].clear();

        imemstream in(element.data(), element.size());

        if (not(in >> vars.g_themes.names[idx]))
            return false;

        for (std::size_t i = 0, e = std::size(vars.g_themes.colors); i != e;
             ++i) {
            if (not(in >> vars.g_themes.colors[idx][i].r >>
                    vars.g_themes.colors[idx][i].g >>
                    vars.g_themes.colors[idx][i].b >>
                    vars.g_themes.colors[idx][i].a))
                return false;
        }

        return true;
    }

    if (current_section.test(1)) {
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

config_manager::config_manager() noexcept
  : m_vars{ do_build_default() }
{}

config_manager::config_manager(const std::string config_path) noexcept
  : m_path{ config_path }
{
    auto ret = std::make_shared<variables>();

    if (not do_load(config_path.c_str(), ret))
        m_vars = do_build_default();
    else
        m_vars = std::static_pointer_cast<const variables>(ret);
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

    auto const_ret = std::static_pointer_cast<const variables>(ret);

    std::unique_lock lock(m_mutex);
    std::swap(const_ret, m_vars);

    return std::error_code();
}

void config_manager::swap(std::shared_ptr<const variables>& other) noexcept
{
    std::unique_lock lock(m_mutex);

    std::swap(m_vars, other);
}

std::shared_ptr<const variables> config_manager::copy() const noexcept
{
    std::shared_lock lock(m_mutex);

    return std::make_shared<const variables>(*m_vars.get());
}

} // namespace irt
