// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling.hpp>

#include <filesystem>

#include <fmt/format.h>

namespace irt {

static expected<buffered_file> open_buffered_file(
  const std::string_view   output_dir,
  const std::integral auto idx,
  const std::string_view   name) noexcept
{
    try {
        auto ec = std::error_code{};

        auto p = output_dir.empty() ? std::filesystem::current_path(ec)
                                    : std::filesystem::path{ output_dir };
        if (not std::filesystem::exists(p, ec))
            return new_error(file_errc::open_error);

        const auto filename = name.empty()
                                ? fmt::format("{}-empty-observer-name.csv", idx)
                                : fmt::format("{}-{}.csv", idx, name);
        p /= filename;

        return open_buffered_file(
          p, bitflags<buffered_file_mode>(buffered_file_mode::write));
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

static void do_initialize(const variable_observer& vars,
                          std::FILE*               file) noexcept
{
    std::fputs("t,", file);

    const auto names = vars.get_names();
    const auto nb    = vars.ssize();
    auto       i     = 0;

    vars.for_each([&](const auto id) noexcept {
        const auto idx = get_index(id);

        if (i + 1 < nb)
            fmt::print("{}-{},", names[idx].sv(), i++);
        else
            fmt::print("{}-{}\n", names[idx].sv(), i);
    });
}

static void do_update(const simulation&        sim,
                      const variable_observer& vars,
                      std::FILE*               file) noexcept
{
    const auto number = vars.ssize();
    auto       i      = 0;

    if (number > 0)
        fmt::print(file, "{:e},", sim.t);
    else
        fmt::print(file, "{:e}\n", sim.t);

    const auto values = vars.get_values();

    vars.for_each([&](const auto id) noexcept {
        if (i + 1 < number)
            fmt::print(file, "{:e},", values[get_index(id)]);
        else
            fmt::print(file, "{:e}\n", values[get_index(id)]);
    });
}

static void do_initialize(const grid_observer& grid, std::FILE* file) noexcept
{
    std::fputs("t,", file);

    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.cols; ++col) {
            if (row + 1 == grid.rows and col + 1 == grid.cols)
                fmt::print(file, "{}-{}\n", row, col);
            else
                fmt::print(file, "{}-{},", row, col);
        }
    }
}

static void do_update(const simulation&    sim,
                      const grid_observer& grid,
                      std::FILE*           file) noexcept
{
    fmt::print(file, "{},", sim.t);

    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.cols; ++col) {
            const auto pos = col * grid.rows + row;

            if (auto obs = sim.observers.try_to_get(grid.observers[pos]); obs) {
                if (row + 1 == grid.rows and col + 1 == grid.cols)
                    fmt::print(file, "{:e}\n,", grid.values[pos]);
                else
                    fmt::print(file, "{:e},", grid.values[pos]);
            } else {
                if (row + 1 == grid.rows and col + 1 == grid.cols)
                    std::fputs("NA\n,", file);
                else
                    std::fputs("NA,", file);
            }
        }
    }
}

static void do_initialize(const graph_observer& vars, std::FILE* file) noexcept
{
    (void)vars;
    (void)file;
}

static void do_update(const simulation&     sim,
                      const graph_observer& vars,
                      std::FILE*            file) noexcept
{
    (void)sim;
    (void)vars;
    (void)file;
}

void file_observers::grow() noexcept
{
    const auto capacity     = ids.capacity();
    const auto new_capacity = capacity == 0 ? 8 : capacity * 3 / 2;

    ids.reserve(new_capacity);
    files.resize(new_capacity);
    subids.resize(new_capacity);
    types.resize(new_capacity);
    enables.resize(new_capacity);
}

void file_observers::clear() noexcept
{
    for (const auto id : ids)
        files[id].reset();

    ids.clear();
}

void file_observers::initialize(const simulation&      sim,
                                project&               pj,
                                const std::string_view output_dir) noexcept
{
    tn = sim.t + time_step;

    for (const auto file_id : ids) {
        const auto idx = get_index(file_id);

        if (enables[idx] == false)
            continue;

        switch (types[idx]) {
        case type::variables:
            if (auto* v = pj.variable_observers.try_to_get(subids[idx].var);
                v) {
                if (auto f = open_buffered_file(output_dir, idx, v->name.sv());
                    f) {
                    files[idx] = std::move(*f);
                    do_initialize(*v, files[idx].get());
                }
            }
            break;

        case type::grid:
            if (auto* v = pj.grid_observers.try_to_get(subids[idx].grid); v) {
                if (auto f = open_buffered_file(output_dir, idx, v->name.sv());
                    f) {
                    files[idx] = std::move(*f);
                    do_initialize(*v, files[idx].get());
                }
            }
            break;

        case type::graph:
            if (auto* v = pj.graph_observers.try_to_get(subids[idx].graph); v) {
                if (auto f = open_buffered_file(output_dir, idx, v->name.sv());
                    f) {
                    files[idx] = std::move(*f);
                    do_initialize(*v, files[idx].get());
                }
            }
            break;
        }
    }
}

bool file_observers::can_update(const time t) const noexcept { return t > tn; }

void file_observers::update(const simulation& sim, const project& pj) noexcept
{
    tn = sim.t + time_step;

    for (auto id : ids) {
        const auto idx = get_index(id);
        if (enables[idx] == false)
            continue;

        switch (types[idx]) {
        case type::variables:
            if (auto* vars = pj.variable_observers.try_to_get(subids[idx].var);
                vars)
                do_update(sim, *vars, files[idx].get());
            break;

        case type::grid:
            if (auto* vars = pj.grid_observers.try_to_get(subids[idx].grid);
                vars)
                do_update(sim, *vars, files[idx].get());
            break;

        case type::graph:
            if (auto* vars = pj.graph_observers.try_to_get(subids[idx].graph);
                vars)
                do_update(sim, *vars, files[idx].get());
            break;
        }
    }
}

void file_observers::finalize() noexcept
{
    for (auto id : ids) {
        const auto idx = get_index(id);

        if (enables[idx])
            files[idx].reset();
    }
}

} // namespace irt
