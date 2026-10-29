// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/global.hpp>
#include <irritator/modeling.hpp>
#include <irritator/simulation-player.hpp>

#include <charconv>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/color.h>
#include <fmt/format.h>

#include <cstdio>

static void show_help() noexcept
{
    std::puts(R"(
irritator-cli [-h][-v][-c][-o path][-t begin[,duration]] project.pirt...
irritator-cli -x file.irb...

Runs each project from the beginning to the end and writes the observations
in `<name>-<id>-<run>.irb` and `<name>-<id>-<run>.manifest.jsonl`.

Options:
  -h,--help                This help message
  -v, --version            The version of irritator
  -c, --csv                Also converts the observations of the next
                           projects into `<name>-<id>-<run>.csv`.
  -x path                  Converts an existing `.irb` file (and the
  --convert path           `.manifest.jsonl` file next to it for the names
                           and the resampling) into a `.csv` file next to it.
  -o path                  The output directory of the observation files.
  --output path            The project directory is used by default, or the
                           current directory.
  -t:begin[,duration]      Define the beginning date of the simulation and
  --time begin[,duration]  optionally the duration. The limits of the project
                           are used by default. The duration is a real
                           greater or equal to 0.0 or `inf` for infinity.

Examples:
$ irritator-cli -t:1,100 first.pirt -t 20,30 second.pirt

        Runs the project `first.pirt` from the date 1.0 to the date 101.0,
        then the project `second.pirt` from the date 20.0 to the date 50.0.

$ irritator-cli -x first-0-0000.irb

        Writes `first-0-0000.csv` with the columns `t,model_id,name,value`.

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

    fmt::print("irritator-cli {}.{}.{}\n\n", VERSION_MAJOR, VERSION_MINOR,
               VERSION_PATCH);
}

struct report_parameter {
    std::string_view str;
    int              arg;
};

enum class ec : irt::u8 {
    arg_missing,
    registred_path_empty,
    unknown_option,
    bad_real,
    bad_dir,
    project_init_error,
    modeling_init_error,
    simulation_init_error,
    unknown_output_path,
    run_error,
    not_irb_file,
    csv_error,
    unknown_error
};

static constexpr report_parameter report_parameters[] = {
    { "argument missing for {}\n", 1 },
    { "not global path\n", 0 },
    { "unknown option {}\n", 1 },
    { "parameter `{}' is not a real\n", 1 },
    { "directory `{}' can not be read\n", 1 },
    { "project init error: {}\n", 1 },
    { "modeling init error: {}\n", 1 },
    { "simulation init error: {}\n", 1 },
    { "unknown output path `{}'\n", 1 },
    { "fail to run `{}': {}\n", 2 },
    { "`{}' is not a .irb file\n", 1 },
    { "fail to convert `{}': {}\n", 2 },
    { "unknown error\n", 0 }
};

template<ec Index, typename... Args>
static constexpr void warning(Args&&... args) noexcept
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    irt::debug::ensure(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(stderr, report_parameters[idx].str,
                fmt::make_format_args(args...));
}

template<ec Index, typename Ret, typename... Args>
static constexpr auto error(Ret&& ret, Args&&... args) noexcept -> Ret
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    irt::debug::ensure(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(stderr, fg(fmt::terminal_color::red),
                report_parameters[idx].str, fmt::make_format_args(args...));

    return ret;
}

enum class option_id : irt::u8 { csv, convert, help, output, time, version };

struct option {
    std::string_view short_opt;
    std::string_view long_opt;
    option_id        id;
    bool             has_value;
};

static inline constexpr option options[] = {
    { "c", "csv", option_id::csv, false },
    { "x", "convert", option_id::convert, true },
    { "h", "help", option_id::help, false },
    { "o", "output", option_id::output, true },
    { "t", "time", option_id::time, true },
    { "v", "version", option_id::version, false },
};

/// The `.csv` and `.manifest.jsonl` files are next to the `.irb` file.
static irt::status convert_to_csv(const irt::path& irb) noexcept
{
    auto manifest = irb;
    manifest.replace_extension(irt::file_type::irt_manifest_file);

    auto csv = irb;
    csv.replace_extension(irt::file_type::data_file);

    return irt::write_observations_csv(irb, manifest, csv);
}

static constexpr const option* find_option(const std::string_view name,
                                           const bool is_long) noexcept
{
    for (const auto& opt : options)
        if ((is_long ? opt.long_opt : opt.short_opt) == name)
            return &opt;

    return nullptr;
}

class main_parameters
{
    irt::task_manager   task_mgr;
    irt::log_history    jn;
    irt::journal_scope  scope;
    irt::config_manager config;

    irt::modeling        mod;
    irt::json_dearchiver json;
    irt::project         pj;

    std::optional<irt::real> begin;
    std::optional<irt::real> duration;
    irt::registred_path_id
         output_dir = irt::undefined<irt::registred_path_id>();
    bool write_csv  = false;

    std::span<const char*> args;
    std::string_view       front;

    bool initialized = false;

public:
    main_parameters(int ac, const char* av[])
      : task_mgr(
          irt::task_manager_options{ .list_count = 1, .worker_count = 0 })
      , args{ av + 1, static_cast<std::size_t>(ac - 1) }
    {
        task_mgr.start();

        mod.files.write([&](auto& fs) noexcept {
            registred_path_add(fs);
            fs.browse_registreds();
        });

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
            initialized = true;
            load_next_token();
        }
    }

    /** Consumes all the arguments of the command line: the options and the
     * projects to run, in order.
     * @return false if an argument is invalid or if a project fails. */
    bool parse() noexcept
    {
        if (not initialized)
            return false;

        while (not front.empty())
            if (not parse_token())
                return false;

        return true;
    }

private:
    irt::status load_project(const std::string_view filename) noexcept
    {
        auto file = irt::file::open(
          irt::path{ filename }, irt::file_mode(irt::file_open_options::read));
        if (not file.has_value())
            return file.error();

        return mod.ids.read([&](const auto& ids, auto) -> irt::status {
            return mod.files.read([&](const auto& fs, auto) -> irt::status {
                return json(pj, fs, ids, filename, *file);
            });
        });
    }

    void apply_options(const std::filesystem::path& filename) noexcept
    {
        if (begin.has_value() or duration.has_value())
            pj.sim.limits.set_duration(
              begin.value_or(pj.sim.limits.begin()),
              duration.value_or(pj.sim.limits.duration()));

        if (irt::is_defined(output_dir))
            pj.observation_dir = output_dir;

        // The name prefixes the observation files.
        if (pj.name.empty())
            pj.name = filename.stem().string();

        pj.flags.set(irt::project::simulation_flag::write_irtb, true);
    }

    // The complete mode of the player: no keyframe, and the samples go
    // straight to the file, so the memory does not grow with the length of the
    // simulation.
    irt::status run() noexcept
    {
        irt::simulation_player player(pj.sim, task_mgr, 0);
        player.set_observation_sink(&pj.observation_file_sink());

        irt_check(player.keep_history(false));
        irt_check(player.start());

        while (not player.finished()) {
            irt_check(player.advance(std::chrono::milliseconds(100)));
            irt_check(pj.flush_observations());
        }

        irt_check(pj.close_observations());

        if (write_csv)
            return convert_to_csv(pj.observation_irb_path());

        return irt::success();
    }

    irt::status prepare_and_run(const std::string_view filename) noexcept
    {
        log(irt::log_level::notice, [&](auto& m) {
            irt::format(m, "Run simulation for file {}\n", filename);
        });

        irt_check(load_project(filename));
        apply_options(std::filesystem::path(filename));
        irt_check(pj.simulation_init(mod));

        return run();
    }

    /** Try to add a new global path in @c modeling. This function only test if
     * the directory exists in the filesystem.
     * @return 1 if the function succeded, 0 otherwise.
     */
    int registred_path_add(const irt::path&       path,
                           const std::string_view name,
                           irt::file_access&      fs) noexcept
    {
        const auto std_path = path.to_std_path();
        auto       ec       = std::error_code{};

        if (std::filesystem::exists(std_path, ec) and ec == std::errc{}) {
            if (std::filesystem::is_directory(std_path, ec) and
                ec == std::errc{}) {
                auto&      dir    = fs.registred_paths.alloc();
                const auto dir_id = fs.registred_paths.get_id(dir);
                dir.name          = name;
                dir.path          = path.sv();
                fs.recorded_paths.emplace_back(dir_id);
                return 1;
            }
        }

        warning<ec::bad_dir>(path.sv());
        return 0;
    }

    /** Try to add generic global paths in @c c modeling: from the system, from
     * the prefix system and from the user.
     *
     * @return @c status_type::status_registred_path_empty if all path does not
     * exists.
     */
    int registred_path_add(irt::file_access& fs) noexcept
    {
        int i = 0;

        if (auto path = irt::get_system_component_dir(); path)
            i += registred_path_add(*path, "System directory", fs);
        if (auto path = irt::get_system_prefix_component_dir(); path)
            i += registred_path_add(*path, "System prefix directory", fs);
        if (auto path = irt::get_default_user_component_dir(); path)
            i += registred_path_add(*path, "User directory", fs);

        if (i == 0)
            warning<ec::registred_path_empty>();

        return i;
    }

    void load_next_token() noexcept
    {
        if (args.empty()) {
            front = std::string_view{};
        } else {
            front = args.front();
            args  = args.subspan(1u);
        }
    }

#if !defined(__APPLE__)
    static bool parse_real(std::string_view& str, irt::real& r) noexcept
    {
        const auto ret = std::from_chars(str.data(), str.data() + str.size(),
                                         r);
        if (ret.ec != std::errc{})
            return false;

        str = str.substr(ret.ptr - str.data());
        return true;
    }
#else
    static bool parse_real(std::string_view& str, irt::real& r) noexcept
    {
        const std::string copy(str.data(), str.size());
        char*             end = nullptr;

        const auto dbl = std::strtod(copy.c_str(), &end);
        if (end == copy.c_str())
            return false;

        str = str.substr(end - copy.c_str());
        r   = dbl;
        return true;
    }
#endif

    static bool parse_real_or_infinity(std::string_view& str,
                                       irt::real&        r) noexcept
    {
        if (str.starts_with("inf")) {
            r   = std::numeric_limits<irt::real>::infinity();
            str = str.substr(3);
            return true;
        }

        return parse_real(str, r);
    }

    /** Parse `begin` or `begin,duration` (`inf` for an infinite duration). */
    bool read_time(std::string_view value) noexcept
    {
        irt::real b = 0, d = 0;

        if (not parse_real(value, b))
            return error<ec::bad_real>(false, value);

        if (value.starts_with(',') or value.starts_with(':')) {
            value = value.substr(1);
            if (not parse_real_or_infinity(value, d) or d < 0)
                return error<ec::bad_real>(false, value);

            duration = d;
        }

        if (not value.empty())
            return error<ec::bad_real>(false, value);

        begin = b;
        return true;
    }

    bool read_output_dir(const std::string_view value) noexcept
    {
        try {
            const auto o  = std::filesystem::path(value);
            auto       ec = std::error_code();

            if (std::filesystem::is_directory(o, ec)) {
                mod.files.write([&](auto& fs) {
                    auto& dir  = fs.registred_paths.alloc();
                    output_dir = fs.registred_paths.get_id(dir);
                    dir.name   = "output-directory";
                    dir.path   = o.string().c_str();
                });
                return true;
            }
        } catch (...) {
        }

        return error<ec::unknown_output_path>(false, value);
    }

    bool read_convert(const std::string_view value) noexcept
    {
        const auto irb = irt::path{ value };

        if (irb.has_extension() != irt::file_type::irb_file)
            return error<ec::not_irb_file>(false, value);

        if (auto ret = convert_to_csv(irb); ret.has_error())
            return error<ec::csv_error>(false, value, ret.error());

        return true;
    }

    bool dispatch(const option& opt, const std::string_view value) noexcept
    {
        switch (opt.id) {
        case option_id::csv:
            write_csv = true;
            return true;

        case option_id::convert:
            return read_convert(value);

        case option_id::help:
            show_help();
            return true;

        case option_id::output:
            return read_output_dir(value);

        case option_id::time:
            return read_time(value);

        case option_id::version:
            show_version();
            return true;
        }

        irt::unreachable();
    }

    /** An option is `-o value`, `-o=value`, `-o:value`, `--output value` or
     * `--output=value`, anything else is a project to run. */
    bool parse_token() noexcept
    {
        if (front.size() < 2 or front[0] != '-')
            return read_argument();

        const auto is_long = front[1] == '-';
        auto       name    = front.substr(is_long ? 2 : 1);
        auto       value   = std::string_view{};

        if (const auto pos = name.find_first_of(is_long ? "=" : "=:");
            pos != std::string_view::npos) {
            value = name.substr(pos + 1);
            name  = name.substr(0, pos);
        }

        const auto* opt = find_option(name, is_long);
        if (not opt)
            return error<ec::unknown_option>(false, front);

        load_next_token();

        if (opt->has_value and value.empty()) {
            if (front.empty())
                return error<ec::arg_missing>(false, opt->long_opt);

            value = front;
            load_next_token();
        }

        return dispatch(*opt, value);
    }

    bool read_argument() noexcept
    {
        const auto filename = front;
        load_next_token();

        if (auto ret = prepare_and_run(filename); ret.has_error())
            return error<ec::run_error>(false, filename, ret.error());

        return true;
    }
};

int main(int argc, const char* argv[])
{
    main_parameters m{ argc, argv };

    return m.parse() ? 0 : 1;
}
