// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/global.hpp>
#include <irritator/io.hpp>

#include <filesystem>
#include <string_view>

#include <fmt/color.h>
#include <fmt/format.h>

#include <cstdio>

enum class ac { nothing, help, run, version };

struct param {
    std::string_view str;
    int              arg;
};

enum class ec {
    success,
    arg_missing,
    bad_parsing,
    registred_path_empty,
    unknown_action,
    bad_real,
    bad_int,
    bad_dir,
    open_file,
};

static constexpr param report_parameters[] = {
    { "success", 0 },
    { "argument missing for {}", 1 },
    { "fail to parse argument", 0 },
    { "not global path", 0 },
    { "unknwon action {}", 1 },
    { "parameter `{}' is not a real", 1 },
    { "parameter `{}' is not an integer", 1 },
    { "directory `{}' can not be read", 1 },
    { "open file `{}' error: {}", 2 }
};

template<ec Index, typename... Args>
static constexpr void warning(Args&&... args) noexcept
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    assert(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(
      stderr, report_parameters[idx].str, fmt::make_format_args(args...));
}

template<ec Index, typename Ret, typename... Args>
static constexpr auto error(Ret&& ret, Args&&... args) noexcept -> Ret
{
    constexpr auto idx = static_cast<std::underlying_type_t<ec>>(Index);

    assert(sizeof...(args) == report_parameters[idx].arg);

    fmt::vprint(stderr,
                fg(fmt::terminal_color::red),
                report_parameters[idx].str,
                fmt::make_format_args(args...));

    return ret;
}

struct main_action {
    ac               type;
    std::string_view short_string;
    std::string_view long_string;
    int              argument;

    bool operator<(const std::string_view other) const noexcept;
    bool operator==(const std::string_view other) const noexcept;
};

static constexpr main_action actions[] = { { ac::help, "h", "help", 0 },
                                           { ac::run, "r", "run", 0 },
                                           { ac::version, "v", "version", 0 } };

//! Show help message in console.
void show_help() noexcept;

//! Show current version in console.
void show_version() noexcept;

//! Run simulation from simulation file.
void run_simulations(std::span<char*> filenames) noexcept;

struct main_parameters {
    ac  action = ac::nothing;
    int files  = 0;

    static bool parse_real(const char* param, irt::real& out) noexcept;
    static bool parse_integer(const char* param, int& out) noexcept;
    static std::optional<main_parameters> parse(int   argc,
                                                char* argv[]) noexcept;
};

int main(int argc, char* argv[])
{
    if (auto params = main_parameters::parse(argc, argv); params.has_value()) {
        switch (params->action) {
        case ac::nothing:
            break;

        case ac::help:
            show_help();
            break;

        case ac::run:
            run_simulations(
              std::span(argv + params->files, argc - params->files));
            break;

        case ac::version:
            show_version();
            break;
        }
    } else {
        return error<ec::bad_parsing>(0);
    }

    return 0;
}

bool main_action::operator<(const std::string_view other) const noexcept
{
    return long_string < other;
}

bool main_action::operator==(const std::string_view other) const noexcept
{
    return long_string == other;
}

bool main_parameters::parse_real(const char* param, irt::real& out) noexcept
{
    long double result = 0;
    if (auto read = std::sscanf(param, "%Lf", &result); read) {
        out = irt::to_real(result);
        return true;
    }

    return error<ec::bad_real>(false, std::string_view{ param });
}

bool main_parameters::parse_integer(const char* param, int& out) noexcept
{
    int result = 0;
    if (auto read = std::sscanf(param, "%d", &result); read) {
        out = result;
        return true;
    }

    return error<ec::bad_int>(false, std::string_view{ param });
}

std::optional<main_parameters> main_parameters::parse(int   argc,
                                                      char* argv[]) noexcept
{
    ac  action = ac::nothing;
    int files  = 0;

    if (argc == 1) {
        action = ac::help;
    } else {
        std::string_view str{ argv[1] };

        auto it = irt::binary_find(
          std::begin(actions),
          std::end(actions),
          str,
          [](auto left, auto right) noexcept -> bool {
              if constexpr (std::is_same_v<decltype(left), std::string_view>)
                  return left < right.long_string;
              else
                  return left.long_string < right;
          });

        if (it == std::end(actions))
            return error<ec::unknown_action>(std::nullopt);

        action = it->type;
    }

    return main_parameters{ .action = action, .files = files };
}

void show_help() noexcept
{
    fmt::print(
      "irritator-cli action-name action-argument [files...]\n\n"
      "help        This help message\n"
      "version     Version of irritator-cli\n"
      "information Information about simulation files\n"
      "		 (no argument required)\n"
      "run 	 Run simulation files\n"
      "	Need parameters:\n"
      "	- [real] The begin date of the begin date of the simulation\n"
      "	- [real] The duration of the simulation\n"
      "\n\n");
}

void show_version() noexcept
{
    fmt::print("irritator-cli {}.{}.{}-{}\n\n",
               VERSION_MAJOR,
               VERSION_MINOR,
               VERSION_PATCH,
               VERSION_TWEAK);
}

/** Try to add a new global path in @c modeling. This function only test if the
 * directory exists in the filesystem.
 * @return 1 if the function succeded, 0 otherwise.
 */
static int registred_path_add(irt::modeling&               mod,
                              const std::filesystem::path& path,
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

/** Try to add generic global paths in @c c modeling: from the system, from the
 * prefix system and from the user.
 *
 * @return @c status_type::status_registred_path_empty if all path does not
 * exists.
 */
static int registred_path_add(irt::modeling& mod) noexcept
{
    int i = 0;

    if (auto path = irt::get_system_component_dir(); path)
        i += registred_path_add(mod, *path, "System directory");
    if (auto path = irt::get_system_prefix_component_dir(); path)
        i += registred_path_add(mod, *path, "System prefix directory");
    if (auto path = irt::get_default_user_component_dir(); path)
        i += registred_path_add(mod, *path, "User directory");

    if (i == 0)
        warning<ec::registred_path_empty>();

    return i;
}

static void run_simulation(irt::json_dearchiver& json,
                           irt::modeling&        mod,
                           irt::file&            f) noexcept
{
    irt::attempt_all(
      [&]() noexcept -> irt::status {
          irt::simulation_memory_requirement smr{ 1024 * 1024 * 8 };
          irt::simulation                    sim{ smr };
          irt::project                       pj;

          if (json(pj, mod, sim, f)) {
              irt_check(sim.srcs.prepare());
              irt_check(sim.initialize());
              sim.t = 0; // @TODO waiting to stores begin, end or
                         // duration into the irt::project class.
              irt::real end = 100.0;

              fmt::print("file-observers: {}\n", pj.file_obs.ids.ssize());

              do {
                  irt_check(sim.run());
              } while (sim.t < end);

              irt_check(sim.finalize());
          }

          return irt::success();
      },
      [](const irt::project::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize project: {}\n", ordinal(s));
      },

      [](const irt::modeling::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize modeling: {}\n", ordinal(s));
      },

      [](const irt::simulation::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize simulation: {}\n", ordinal(s));
      },

      []() noexcept -> void { fmt::print(stderr, "Unknown error\n"); });
}

void run_simulations(std::span<char*> filenames) noexcept
{
    irt::attempt_all(
      [&]() noexcept -> irt::status {
          irt::modeling_initializer init;
          irt::modeling             mod;
          irt::json_dearchiver      j;

          irt_check(mod.init(init));
          registred_path_add(mod);
          irt_check(mod.fill_components());

          for (auto filename : filenames) {
              irt::project pj;
              irt_check(pj.init(init));
              irt::simulation_memory_requirement smr{ 1024 * 1024 * 8 };
              irt::simulation                    sim{ smr };

              fmt::print("Run simulation for file {}\n", filename);
              if (auto file = irt::file::open(filename, irt::open_mode::read);
                  file.has_value())
                  run_simulation(j, mod, *file);
              else
                  warning<ec::open_file>(filename,
                                         std::string_view{ "unknown" });
          }

          return irt::success();
      },

      [](const irt::project::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize project: {}\n", ordinal(s));
      },

      [](const irt::modeling::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize modeling: {}\n", ordinal(s));
      },

      [](const irt::simulation::part s) noexcept -> void {
          fmt::print(stderr, "Fail to initialize simulation: {}\n", ordinal(s));
      },

      []() noexcept -> void { fmt::print(stderr, "Unknown error\n"); });
}
