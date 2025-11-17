// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/global.hpp>
#include <irritator/io.hpp>
#include <irritator/observation.hpp>

#include <charconv>
#include <filesystem>
#include <string>
#include <string_view>

#include <fmt/color.h>
#include <fmt/format.h>

#include <cstdio>

static void show_help() noexcept
{
    std::puts(R"(
irritator-cli [-h][-v][-tmin:max]

Options:
  -h,--help                This help message
  -v, --version            The version of irritator
  -o path                  The output path of the simulation result.
  --output path            If path does not exist, current dir is used.
  -t:begin[,duration]      Define the beginning date of the simulation and
  --time begin[,duration]  ptionaly the duration. The begin date default is
                           0.0, the duration is +infiny. Duration can only
                           be a real greater or equal to 0.0 or `inf` for
                           infiny.

Examples:
$ irritator-cli -hvt:1:100 first.irt -t 20:30 second.irt

        Will load and run the simulation `first.irt` from date 1.0 to the
        date 100.0 then load an run the simulation `second.irt` from date
        20.0 to the date 50.0.

)");
}

static void show_version() noexcept
{
#if !defined(VERSION_MAJOR)
#define VERSION_MAJOR = "major version undefined"
#endif
#if !defined(VERSION_MINOR)
#define VERSION_MINOR = "minor version undefined"
#endif
#if !defined(VERSION_PATCH)
#define VERSION_PATCH = "patch version undefined"
#endif
#if !defined(VERSION_TWEAK)
#define VERSION_TWEAK = "tweak version undefined"
#endif

    fmt::print("irritator-cli {}.{}.{}-{}\n\n",
               VERSION_MAJOR,
               VERSION_MINOR,
               VERSION_PATCH,
               VERSION_TWEAK);
}

struct report_parameter {
    std::string_view str;
    int              arg;
};

enum class ec : irt::u8 {
    arg_missing,
    bad_parsing,
    registred_path_empty,
    unknown_option,
    bad_real,
    bad_int,
    bad_dir,
    open_file,
    json_file,
    project_init_error,
    modeling_init_error,
    simulation_init_error,
    unknown_output_path,
    unknown_error
};

static constexpr report_parameter report_parameters[] = {
    { "argument missing for {}", 1 },
    { "fail to parse argument", 0 },
    { "not global path", 0 },
    { "unknwon action {}", 1 },
    { "parameter `{}' is not a real", 1 },
    { "parameter `{}' is not an integer", 1 },
    { "directory `{}' can not be read", 1 },
    { "open file `{}' error: {}", 2 },
    { "json format error in `{}' error: {}", 2 },
    { "project init error: {},", 1 },
    { "modeling init error: {},", 1 },
    { "simulation init error: {}", 1 },
    { "unknown output path `{}'", 1 },
    { "unknown error", 0 }
};

template<ec Index, typename... Args>
static constexpr void warning(Args&&... args) noexcept
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    irt::debug::ensure(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(
      stderr, report_parameters[idx].str, fmt::make_format_args(args...));
}

template<ec Index, typename Ret, typename... Args>
static constexpr auto error(Ret&& ret, Args&&... args) noexcept -> Ret
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    irt::debug::ensure(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(stderr,
                fg(fmt::terminal_color::red),
                report_parameters[idx].str,
                fmt::make_format_args(args...));

    return ret;
}

enum class option_id : irt::u8 { unknown, help, memory, output, time, version };

struct option {
    const std::string_view short_opt;
    const std::string_view long_opt;
    const option_id        id;
    const irt::u8          min_arg;
    const irt::u8          max_arg;
};

static inline constexpr option options[] = {
    { "h", "help", option_id::help, 0, 0 },
    { "m", "memory", option_id::memory, 1, 1 },
    { "o", "output", option_id::output, 1, 1 },
    { "t", "time", option_id::time, 1, 2 },
    { "v", "version", option_id::version, 0, 0 },
};

static constexpr const option* get_from_short(
  std::string_view short_name) noexcept
{
    const auto begin = std::begin(options);
    const auto end   = std::end(options);

    const auto i = irt::binary_find(
      begin,
      end,
      short_name,
      [](const auto& l, const auto& r) noexcept -> bool {
          if constexpr (std::is_same_v<std::decay_t<decltype(l)>,
                                       std::string_view>)
              return l < r.short_opt;
          else
              return l.short_opt < r;
      });

    return i == end ? nullptr : i;
}

static constexpr const option* get_from_long(
  std::string_view short_name) noexcept
{
    const auto begin = std::begin(options);
    const auto end   = std::end(options);

    const auto i = irt::binary_find(
      begin,
      end,
      short_name,
      [](const auto& l, const auto& r) noexcept -> bool {
          if constexpr (std::is_same_v<std::decay_t<decltype(l)>,
                                       std::string_view>)
              return l < r.long_opt;
          else
              return l.long_opt < r;
      });

    return i == end ? nullptr : i;
}

class main_parameters
{
    irt::sz memory = 1024 * 1024 * 8;

    irt::journal_handler jn;

    irt::modeling        mod;
    irt::json_dearchiver json;
    irt::project         pj;

    std::span<const char*> args;
    std::string_view       front;

    irt::real r; // Temp variables used during parse operation.
    irt::sz   u;

public:
    main_parameters(int ac, const char* av[])
      : mod(jn)
      , args{ av + 1, static_cast<std::size_t>(ac - 1) }
      , r{ 0.0 }
    {
        registred_path_add();

        if (auto ret = mod.fill_components(); ret.has_error()) {
            switch (ret.error().cat()) {
            case irt::category::modeling:
                warning<ec::modeling_init_error>(ret.error().value());
                break;
            case irt::category::project:
                warning<ec::project_init_error>(ret.error().value());
                break;
            case irt::category::simulation:
                warning<ec::simulation_init_error>(ret.error().value());
                break;
            default:
                warning<ec::unknown_error>();
                break;
            }
        } else {
            load_next_token();
        }
    }

    void observation_initialize() noexcept
    {
        for (auto& o : pj.grid_observers) {
            o.init(pj, mod, pj.sim);
            pj.file_obs.alloc(pj.grid_observers.get_id(o));
        }

        for (auto& o : pj.graph_observers) {
            o.init(pj, mod, pj.sim);
            pj.file_obs.alloc(pj.graph_observers.get_id(o));
        }

        for (auto& o : pj.variable_observers)
            if (auto ret = o.init(pj, pj.sim); !!ret)
                pj.file_obs.alloc(pj.variable_observers.get_id(o));

        pj.file_obs.initialize(pj.sim, pj, pj.get_observation_dir(mod));
    }

    void observation_update() noexcept
    {
        for (int i = 0, e = pj.sim.immediate_observers.ssize(); i != e; ++i) {
            const auto obs_id = pj.sim.immediate_observers[i];
            if (auto* o = pj.sim.observers.try_to_get(obs_id); o)
                if (o->states[irt::observer_flags::buffer_full])
                    write_interpolate_data(*o, o->time_step);
        }

        for (auto& g : pj.grid_observers) {
            const auto g_id = pj.grid_observers.get_id(g);
            if (auto* g = pj.grid_observers.try_to_get(g_id); g)
                if (g->can_update(pj.sim.current_time()))
                    g->update(pj.sim);
        }

        for (auto& g : pj.graph_observers) {
            const auto g_id = pj.graph_observers.get_id(g);
            if (auto* g = pj.graph_observers.try_to_get(g_id); g)
                if (g->can_update(pj.sim.current_time()))
                    g->update(pj.sim);
        }

        if (pj.file_obs.can_update(pj.sim.current_time()))
            pj.file_obs.update(pj.sim, pj);
    }

    void observation_finalize() noexcept
    {
        for (auto& obs : pj.sim.observers)
            flush_interpolate_data(obs, obs.time_step);

        pj.file_obs.finalize();
    }

    irt::expected<void> run() noexcept
    {
        observation_initialize();
        irt_check(pj.sim.srcs.prepare());
        irt_check(pj.sim.initialize());

        fmt::print("grid-observers: {}\n"
                   "graph-observers: {}\n"
                   "plot-observers: {}\n"
                   "file-observers: {}\n",
                   pj.grid_observers.ssize(),
                   pj.graph_observers.ssize(),
                   pj.variable_observers.ssize(),
                   pj.file_obs.ids.ssize());

        do {
            irt_check(pj.sim.run());
            observation_update();
        } while (not pj.sim.current_time_expired());

        irt_check(pj.sim.finalize());
        observation_finalize();

        return irt::success();
    }

    irt::expected<void> prepare_and_run() noexcept
    {
        fmt::print("Run simulation for file {}\n", front);
        const std::string str{ front };
        load_next_token();

        if (auto file = irt::file::open(str.c_str(), irt::open_mode::read);
            file.has_value()) {
            if (json(pj, mod, pj.sim, str, *file)) {
                run();
            } else {
                return irt::new_error(irt::json_errc::invalid_project_format);
            }
        } else {
            return file.error();
        }

        return irt::success();
    }

    /** Try to add a new global path in @c modeling. This function only test if
     * the directory exists in the filesystem.
     * @return 1 if the function succeded, 0 otherwise.
     */
    int registred_path_add(const std::filesystem::path& path,
                           const std::string_view       name) noexcept
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) and ec == std::errc{}) {
            auto&      dir    = mod.registred_paths.alloc();
            const auto dir_id = mod.registred_paths.get_id(dir);
            dir.name          = name;
            dir.path          = path.string().c_str();
            mod.component_repertories.emplace_back(dir_id);
            return 1;
        }

        warning<ec::bad_dir>(path.string());
        return 0;
    }

    /** Try to add generic global paths in @c c modeling: from the system, from
     * the prefix system and from the user.
     *
     * @return @c status_type::status_registred_path_empty if all path does not
     * exists.
     */
    int registred_path_add() noexcept
    {
        int i = 0;

        if (auto path = irt::get_system_component_dir(); path)
            i += registred_path_add(*path, "System directory");
        if (auto path = irt::get_system_prefix_component_dir(); path)
            i += registred_path_add(*path, "System prefix directory");
        if (auto path = irt::get_default_user_component_dir(); path)
            i += registred_path_add(*path, "User directory");

        if (i == 0)
            warning<ec::registred_path_empty>();

        return i;
    }

    constexpr void load_next_token() noexcept
    {
        if (args.empty()) {
            front = std::string_view{};
        } else {
            front = args.front();
            if (not args.empty())
                args = args.subspan(1u);
        }
    }

    constexpr void consume_data(const std::size_t nb) noexcept
    {
        if (front.size() <= nb)
            front = front.substr(nb);
        else
            load_next_token();
    }

    constexpr bool have_data() noexcept { return not args.empty(); }

    constexpr bool start_short_option() const noexcept
    {
        irt::debug::ensure(not front.empty());
        return front.size() > 1 and front[0] == '-' and front[1] != '-';
    }

    constexpr bool start_long_option() const noexcept
    {
        irt::debug::ensure(not front.empty());
        return front.size() > 2 and front.substr(0, 2) == "--";
    }

    bool parse_integer() noexcept
    {
        if (const auto ec =
              std::from_chars(front.data(), front.data() + front.size(), u);
            ec.ec == std::errc{}) {
            front = front.substr(ec.ptr - front.data());
        } else {
            warning<ec::bad_int>(front);
        }
        return true;
    }

#if !defined(__APPLE__)
    /** Parse a real from @c front and if it empty, front args. */
    bool parse_real() noexcept
    {
        if (const auto ec =
              std::from_chars(front.data(), front.data() + front.size(), r);
            ec.ec == std::errc{}) {
            front = front.substr(ec.ptr - front.data());
        } else {
            warning<ec::bad_real>(front);
        }

        return true;
    }
#else
    /** Parse a real from @c front and if it empty, front args. */
    bool parse_real() noexcept
    {
        const std::string str(front.data(), front.size());
        char*             str_end = nullptr;

        const auto dbl = std::strtod(str.c_str(), &str_end);
        if (dbl == 0.0 and str_end == str.c_str()) {
            warning<ec::bad_real>(str);
        } else {
            front = front.substr(str_end - str.c_str());
            r     = dbl;
        }

        return true;
    }
#endif

    /** Parse a real or the string "inf" from @c front and if it empty,
     * front args. */
    constexpr bool parse_real_or_infinity() noexcept
    {
        if (front.size() >= 3 and front.substr(0, 3) == "inf") {
            r     = std::numeric_limits<irt::real>::infinity();
            front = front.substr(3);
            return true;
        }

        return parse_real();
    }

    /** Parse a real, or a couple real,real or a couple real,inf. */
    bool read_time() noexcept
    {
        auto begin    = pj.sim.limits.begin();
        auto duration = pj.sim.limits.duration();

        if (parse_real()) {
            if (not front.empty() and (front[0] == ',' or front[0] == ':')) {
                front = front.substr(1);
                if (not front.empty() and parse_real_or_infinity()) {
                    pj.sim.limits.set_duration(begin, r);
                    return true;
                }
            } else {
                pj.sim.limits.set_duration(begin, duration);
                return true;
            }
        }

        return error<ec::arg_missing>(false, "time");
    }

    /** Parse a integer and reinitialize the simulation buffers. */
    bool read_memory() noexcept
    {
        if (parse_integer() and u > memory) {
            memory = u;
            return true;
        }

        return error<ec::arg_missing>(false, "memory");
    }

    bool read_output_dir() noexcept
    {
        try {
            auto o  = std::filesystem::path(front);
            auto ec = std::error_code();
            if (std::filesystem::exists(o, ec)) {
                auto&      dir     = mod.registred_paths.alloc();
                const auto dir_id  = mod.registred_paths.get_id(dir);
                dir.name           = "output-directory";
                dir.path           = o.string().c_str();
                pj.observation_dir = dir_id;
                return true;
            }
        } catch (...) {
        }

        return false;
    }

    constexpr bool dispatch(const option& opt) noexcept
    {
        switch (opt.id) {
        case option_id::help:
            show_help();
            return true;

        case option_id::memory:
            if (front[0] == '=' or front[0] == ':')
                consume_data(1u);

            return read_memory();

        case option_id::output:
            if (front[0] == '=' or front[0] == ':')
                consume_data(1u);

            return read_output_dir();

        case option_id::version:
            show_version();
            return true;

        case option_id::time:
            if (front[0] == '=' or front[0] == ':')
                consume_data(1u);

            return read_time();

        case option_id::unknown:
            break;
        }

        return error<ec::unknown_option>(false, front);
    }

    constexpr bool read_short_option() noexcept
    {
        if (const auto ptr = get_from_short(front); ptr) {
            consume_data(1);
            return dispatch(*ptr);
        } else {
            return error<ec::unknown_option>(false, front);
        }
    }

    /** Consume all characters of the @c front token.
     *
     *  @return true If all characters from the front are read
     * successsively.
     */
    constexpr bool read_short_options() noexcept
    {
        while (not front.empty() and read_short_option())
            ;

        return front.empty();
    }

    constexpr bool read_long_option() noexcept
    {
        if (const auto ptr = get_from_long(front); ptr) {
            consume_data(ptr->long_opt.size());
            return dispatch(*ptr);
        } else {
            return error<ec::unknown_option>(false, front);
        }
    }

    bool read_argument() noexcept
    {
        irt::debug::ensure(not front.empty());

        if (auto ret = prepare_and_run(); not ret) {
            switch (ret.error().cat()) {
            case irt::category::json:
                warning<ec::json_file>(front.front(),
                                       std::string_view{ "unknown" });
                return false;
            case irt::category::file:;
                warning<ec::open_file>(front.front(),
                                       std::string_view{ "unknown" });
                return false;
            case irt::category::modeling:
                warning<ec::modeling_init_error>(ret.error().value());
                return false;
            case irt::category::project:
                warning<ec::project_init_error>(ret.error().value());
                return false;
            case irt::category::simulation:
                warning<ec::simulation_init_error>(ret.error().value());
                return false;
            default:
                warning<ec::unknown_error>();
                return false;
            }
        }

        return true;
    }

    bool parse_args()
    {
        return start_short_option()  ? read_short_options()
               : start_long_option() ? read_long_option()
                                     : read_argument();
    }

    /** Consume all argument from the command line interface and return true
     * if parsing are done. */
    constexpr bool parse() noexcept
    {
        while (not front.empty() and parse_args())
            ;

        return front.empty() and args.empty();
    }
};

int main(int argc, const char* argv[])
{
    main_parameters m{ argc, argv };

    return m.parse();
}
