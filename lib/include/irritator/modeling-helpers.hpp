// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_MODELING_HELPERS_HPP

#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/modeling.hpp>

#include <fmt/format.h>

namespace irt {

constexpr inline auto split(const std::string_view str,
                            const char             delim = '_') noexcept
  -> std::pair<std::string_view, std::string_view>
{
    if (const auto pos = str.find_last_of(delim);
        pos != std::string_view::npos) {
        if (std::cmp_less(pos + 1, str.size())) {
            return std::make_pair(str.substr(0, pos),
                                  str.substr(pos + 1, std::string_view::npos));
        } else {
            return std::make_pair(str.substr(0, pos), std::string_view{});
        }
    } else {
        return std::make_pair(str, std::string_view{});
    }
}

static_assert(split("m_0", '_').first == std::string_view("m"));
static_assert(split("m_0", '_').second == std::string_view("0"));
static_assert(split("m_123", '_').first == std::string_view("m"));
static_assert(split("m_123", '_').second == std::string_view("123"));
static_assert(split("m", '_').first == std::string_view("m"));
static_assert(split("m", '_').second == std::string_view{});
static_assert(split("m_", '_').first == std::string_view("m"));
static_assert(split("m_", '_').second == std::string_view{});

inline file_path_id get_file_from_component(const component_access& ids,
                                            const file_access&      fs,
                                            const component_id      compo_id,
                                            const std::string_view str) noexcept
{
    if (ids.exists(compo_id)) {
        const auto& filepath = ids.component_file_paths[compo_id];

        if (const auto* d = fs.dir_paths.try_to_get(filepath.parent)) {
            for (const auto f_id : d->children) {
                if (const auto* f = fs.file_paths.try_to_get(f_id))
                    if (f->path.sv() == str)
                        return f_id;
            }
        }
    }

    return undefined<file_path_id>();
}

/// Check if a @c T with name @c name exists in the @c data_array @c data. Used
/// for @c file_path, @c dir_path and @c reg_path.
template<typename T, typename Identifier>
constexpr bool path_exist(const data_array<T, Identifier>& data,
                          const vector<Identifier>&        container,
                          std::string_view                 name) noexcept
{
    for (const auto id : container)
        if (const auto* item = data.try_to_get(id))
            if (item->path.sv() == name)
                return true;

    return false;
}

/// Adds the extension to the file path string according to @c type extension.
inline void add_extension(
  file_path_str&       file,
  file_path::file_type type = file_path::file_type::component_file) noexcept
{
    const std::decay_t<decltype(file)> tmp(file);

    if (auto dot = tmp.sv().find_last_of('.'); dot == std::string_view::npos) {
        format(file,
               "{}{}",
               tmp.sv().substr(0, dot),
               file_path::file_type_names[ordinal(type)]);
    } else {
        format(
          file, "{}{}", tmp.sv(), file_path::file_type_names[ordinal(type)]);
    }
}

/// Checks if the file path string has the corresponding @c type extension.
constexpr bool has_extension(
  const std::string_view filename,
  file_path::file_type   type = file_path::file_type::component_file) noexcept
{
    if (auto dot = filename.find_last_of('.'); dot != std::string_view::npos) {
        const auto ext = filename.substr(dot);
        return ext == file_path::file_type_names[ordinal(type)];
    }

    return false;
}

/// Detect the type of the file according to the file extension.
///
/// @param filename a string buffer where retrieve extension and compare with
/// irritator known extension.
/// @return The irritator file type known or @c file_type::undefined_file
/// otherwise.
///
constexpr file_path::file_type get_extension(
  const std::string_view filename) noexcept
{
    if (auto dot = filename.find_last_of('.'); dot != std::string_view::npos) {
        const auto ext = filename.substr(dot);

        for (sz i = 0; i < std::size(file_path::file_type_names); ++i)
            if (file_path::file_type_names[i] == ext)
                return enum_cast<file_path::file_type>(i);
    }

    return file_path::file_type::undefined_file;
}

/// Checks if the file path string has an extension equals to any one of
/// irritator files (.irt, .pirt, .data, etc.).
constexpr bool has_irritator_extension(const std::string_view filename) noexcept
{
    if (auto dot = filename.find_last_of('.'); dot != std::string_view::npos) {
        const auto ext = filename.substr(dot);

        for (const auto& valid_extension : file_path::file_type_names)
            if (valid_extension == ext)
                return true;
    }

    return false;
}

/// Checks if the file path string has a valid filename and has the
/// corresponding @c type extension.
constexpr bool is_valid_filename(
  const std::string_view filename,
  file_path::file_type   type = file_path::file_type::component_file) noexcept
{
    return not filename.empty() and filename[0] != '.' and
           filename[0] != '-' and all_char_valid(filename) and
           filename.ends_with(file_path::file_type_names[ordinal(type)]);
}

static_assert(is_valid_filename("file.irt"));
static_assert(is_valid_filename("file.pirt",
                                file_path::file_type::project_file));

inline std::optional<std::filesystem::path> make_file(
  const registred_path& r,
  const dir_path&       d,
  const file_path&      f) noexcept
{
    try {
        std::filesystem::path ret(r.path.sv());
        ret /= d.path.sv();
        ret /= f.path.sv();
        return ret;
    } catch (...) {
        return std::nullopt;
    }
}

struct component_filenames {
    std::filesystem::path component;
    std::filesystem::path description;
};

inline auto make_component_files(const file_access&      fs,
                                 const component_access& ids,
                                 const component_id      id) noexcept
  -> std::optional<component_filenames>
{
    const auto& file_path = ids.component_file_paths[id];

    if (const auto* dir = fs.dir_paths.try_to_get(file_path.parent)) {
        if (const auto* reg = fs.registred_paths.try_to_get(dir->parent)) {
            try {
                std::filesystem::path base(reg->path.sv());
                base /= dir->path.sv();
                base /= file_path.path.sv();
                base.replace_extension(".irt");

                std::filesystem::path desc(base);
                desc.replace_extension(".desc");

                return component_filenames{ .component   = base,
                                            .description = desc };
            } catch (...) {
            }
        }
    }

    return std::nullopt;
}

inline std::optional<std::filesystem::path> make_file(
  const file_access&         fs,
  const component_file_path& c) noexcept
{
    if (auto* dir = fs.dir_paths.try_to_get(c.parent)) {
        if (auto* reg = fs.registred_paths.try_to_get(dir->parent)) {
            std::filesystem::path file(reg->path.sv());
            file /= dir->path.sv();
            file /= c.path.sv();
            return file;
        }
    }

    return std::nullopt;
}

inline std::optional<std::filesystem::path> make_file(
  const modeling&  mod,
  const file_path& f) noexcept
{
    return mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept
                            -> std::optional<std::filesystem::path> {
        if (auto* dir = fs.dir_paths.try_to_get(f.parent))
            if (auto* reg = fs.registred_paths.try_to_get(dir->parent))
                return make_file(*reg, *dir, f);

        return std::nullopt;
    });
}

inline std::optional<std::filesystem::path> make_file(
  const modeling&    mod,
  const file_path_id f) noexcept
{
    return mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept
                            -> std::optional<std::filesystem::path> {
        if (auto* file = fs.file_paths.try_to_get(f))
            if (auto* dir = fs.dir_paths.try_to_get(file->parent))
                if (auto* reg = fs.registred_paths.try_to_get(dir->parent))
                    return make_file(*reg, *dir, *file);

        return std::nullopt;
    });
}

inline expected<file> open_file(
  const registred_path& reg_p,
  const dir_path&       dir_p,
  const file_path&      file_p,
  const file_mode       mode = file_mode(file_open_options::read)) noexcept
{
    try {
        std::filesystem::path p = reg_p.path.u8sv();
        p /= dir_p.path.u8sv();
        p /= file_p.path.u8sv();

        return file::open(p, mode);
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

inline expected<file> open_file(
  const modeling&    mod,
  const file_path_id id,
  const file_mode    mode = file_mode(file_open_options::read)) noexcept
{
    try {
        if (const auto filename = make_file(mod, id); filename.has_value()) {
            return file::open(*filename, mode);
        }

        return new_error(file_errc::open_error);
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

/// Checks the type of \c component pointed by the \c tree_node \c.
///
/// \return true in the underlying \c component in the \c tree_node \c is a
/// graph or a grid otherwise return false. If the component does not exists
/// this function returns false.
inline bool component_is_grid_or_graph(const modeling&  mod,
                                       const tree_node& tn) noexcept
{
    return mod.ids.read([&](const auto& ids, auto) noexcept {
        if (ids.exists(tn.id)) {
            return any_equal(ids.components[tn.id].type,
                             component_type::graph,
                             component_type::grid);
        }

        return false;
    });
}

template<typename Function>
void for_each_component(const modeling& mod, Function&& f) noexcept
{
    mod.files.read([&](const auto& fs, const auto /*vers*/) noexcept {
        for (const auto reg_id : fs.recorded_paths) {
            if (const auto* reg = fs.registred_paths.try_to_get(reg_id)) {
                for (const auto dir_id : reg->children) {
                    if (const auto* dir = fs.dir_paths.try_to_get(dir_id)) {
                        for (const auto file_id : dir->children) {
                            if (const auto* file =
                                  fs.file_paths.try_to_get(file_id)) {
                                mod.ids.read(
                                  [&](const auto& ids, auto) noexcept {
                                      if (ids.exists(file->component))
                                          f(*reg,
                                            *dir,
                                            *file,
                                            ids.components[file->component]);
                                  });
                            }
                        }
                    }
                }
            }
        }
    });
}

//! Call \c f for each model found into \c tree_node::nodes_v table.
template<typename Function>
void for_each_model(simulation& sim, tree_node& tn, Function&& f) noexcept
{
    for (sz i = 0, e = tn.unique_id_to_model_id.data.size(); i < e; ++i) {
        if_data_exists_do(
          sim.models, tn.unique_id_to_model_id.data[i].value, [&](auto& mdl) {
              std::invoke(std::forward<Function>(f),
                          tn.unique_id_to_model_id.data[i].id.sv(),
                          mdl);
          });
    }
}

template<typename Function>
void for_each_model(const simulation& sim,
                    const tree_node&  tn,
                    Function&&        f) noexcept
{
    for (sz i = 0, e = tn.unique_id_to_model_id.data.size(); i < e; ++i) {
        const auto mdl_id = tn.unique_id_to_model_id.data[i].value;
        if (const auto* mdl = sim.models.try_to_get(mdl_id); mdl)
            std::invoke(std::forward<Function>(f),
                        tn.unique_id_to_model_id.data[i].id.sv(),
                        *mdl);
    }
}

} // namespace irt

#endif
