// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling-helpers.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>

#include <cstdint>

namespace irt {

component::component(const component& other) noexcept
  : x{ other.x }
  , y{ other.y }
  , input_connection_pack{ other.input_connection_pack }
  , output_connection_pack{ other.output_connection_pack }
  , name{ other.name }
  , id{ other.id }
  , type{ other.type }
  , state{ other.state }
  , srcs{ other.srcs }
{}

component::component(component&& other) noexcept
  : x{ std::exchange(other.x, port_type{}) }
  , y{ std::exchange(other.y, port_type{}) }
  , input_connection_pack{ std::exchange(other.input_connection_pack,
                                         vector<connection_pack>{}) }
  , output_connection_pack{ std::exchange(other.output_connection_pack,
                                          vector<connection_pack>{}) }
  , name{ other.name }
  , id{ other.id }
  , type{ other.type }
  , state{ other.state }
  , srcs{ std::exchange(other.srcs, external_source_definition{}) }
{}

component& component::operator=(const component& other) noexcept
{
    if (this != &other) {
        x                      = other.x;
        y                      = other.y;
        input_connection_pack  = other.input_connection_pack;
        output_connection_pack = other.output_connection_pack;
        name                   = other.name;
        id                     = other.id;
        type                   = other.type;
        state                  = other.state;
        srcs                   = other.srcs;
    }

    return *this;
}

component& component::operator=(component&& other) noexcept
{
    if (this != &other) {
        x = std::exchange(other.x, port_type{});
        y = std::exchange(other.y, port_type{});
        input_connection_pack =
          std::exchange(other.input_connection_pack, vector<connection_pack>{});
        output_connection_pack = std::exchange(other.output_connection_pack,
                                               vector<connection_pack>{});
        name                   = other.name;
        id                     = other.id;
        type                   = other.type;
        state                  = other.state;
        srcs = std::exchange(other.srcs, external_source_definition{});
    }

    return *this;
}

port_id component::get_x(std::string_view str) const noexcept
{
    const auto& vec_name = x.get<port_str>();
    for (const auto elem : x)
        if (str == vec_name[elem].sv())
            return elem;

    return undefined<port_id>();
}

port_id component::get_y(std::string_view str) const noexcept
{
    const auto& vec_name = y.get<port_str>();
    for (const auto elem : y)
        if (str == vec_name[elem].sv())
            return elem;

    return undefined<port_id>();
}

port_id component::get_or_add_x(std::string_view str) noexcept
{
    auto port_id = get_x(str);

    if (is_undefined(port_id)) {
        if (x.can_alloc(1)) {
            port_id                  = x.alloc_id();
            x.get<port_str>(port_id) = str;
            x.get<position>(port_id).reset();
        }
    }

    return port_id;
}

port_id component::get_or_add_y(std::string_view str) noexcept
{
    auto port_id = get_y(str);

    if (is_undefined(port_id)) {
        if (y.can_alloc(1)) {
            port_id                  = y.alloc_id();
            y.get<port_str>(port_id) = str;
            y.get<position>(port_id).reset();
        }
    }

    return port_id;
}

component_access::component_access(
  const modeling_reserve_definition& res) noexcept
  : ids(res.components.value())
  , components(res.components.value())
  , component_file_paths(res.components.value())
  , component_descriptions(res.components.value())
  , generic_components(res.generic_compos.value())
  , grid_components(res.grid_compos.value())
  , graph_components(res.graph_compos.value())
  , hsm_components(res.hsm_compos.value())
  , hsms(res.hsm_compos.value())
  , graphs(res.graph_compos.value())
{}

modeling::modeling(journal_handler&                   jnl,
                   const modeling_reserve_definition& res) noexcept
  : ids(res)
  , files(res.regs.value(), res.dirs.value(), res.files.value())
  , journal(jnl)
{
    files.read([&](const auto& fs, const auto) noexcept {
        ids.read([&](const auto& ids, const auto) noexcept {
            if (ids.generic_components.capacity() == 0 or
                ids.grid_components.capacity() == 0 or
                ids.graph_components.capacity() == 0 or
                ids.hsm_components.capacity() == 0 or
                ids.components.capacity() == 0 or
                fs.registred_paths.capacity() == 0 or
                fs.dir_paths.capacity() == 0 or fs.file_paths.capacity() == 0 or
                ids.hsms.capacity() == 0 or ids.graphs.capacity() == 0 or
                fs.recorded_paths.capacity() == 0)
                journal.push(log_level::error, [&](auto& t, auto& m) noexcept {
                    t = "Modeling initialization error";
                    format(m,
                           "generic-components    {:>8} ({:>8})"
                           "grid-components       {:>8} ({:>8})"
                           "graph-components      {:>8} ({:>8})"
                           "hsm-components        {:>8} ({:>8})"
                           "components            {:>8} ({:>8})"
                           "registred-paths       {:>8} ({:>8})"
                           "dir-paths             {:>8} ({:>8})"
                           "file-paths            {:>8} ({:>8})"
                           "hsms                  {:>8} ({:>8})"
                           "graphs                {:>8} ({:>8})"
                           "component-repertories {:>8} ({:>8})",
                           ids.generic_components.capacity(),
                           res.generic_compos.value(),
                           ids.grid_components.capacity(),
                           res.grid_compos.value(),
                           ids.graph_components.capacity(),
                           res.graph_compos.value(),
                           ids.hsm_components.capacity(),
                           res.hsm_compos.value(),
                           ids.components.capacity(),
                           res.components.value(),
                           fs.registred_paths.capacity(),
                           res.regs.value(),
                           fs.dir_paths.capacity(),
                           res.dirs.value(),
                           fs.file_paths.capacity(),
                           res.files.value(),
                           ids.hsms.capacity(),
                           res.hsm_compos.value(),
                           ids.graphs.capacity(),
                           res.graph_compos.value(),
                           res.components.value(),
                           fs.recorded_paths.capacity(),
                           res.regs.value());
                });
        });
    });
}

dir_path_id registred_path::search(
  const data_array<dir_path, dir_path_id>& data,
  const std::string_view                   dir_name) noexcept
{
    for (const auto& elem : data)
        if (elem.path.sv() == dir_name)
            return data.get_id(elem);

    return undefined<dir_path_id>();
}

bool registred_path::exists(const data_array<dir_path, dir_path_id>& data,
                            const std::string_view dir_name) noexcept
{
    return search(data, dir_name) != undefined<dir_path_id>();
}

static status browse_directory(journal_handler&      jn,
                               file_access&          fs,
                               registred_path&       reg_dir,
                               dir_path&             dir,
                               std::filesystem::path path) noexcept
{
    try {
        std::error_code ec;

        auto it = std::filesystem::directory_iterator{ path, ec };
        auto et = std::filesystem::directory_iterator{};

        for (; it != et; it = it.increment(ec)) {
            if (not it->is_regular_file())
                continue;

            const auto type = get_extension(it->path().filename().string());
            if (type == file_path::file_type::undefined_file)
                continue;

            if (not(fs.file_paths.can_alloc(1) or fs.file_paths.grow<3, 2>()))
                return new_error(modeling_errc::memory_error);

            if (not(dir.children.can_alloc(1) or dir.children.grow<3, 2>()))
                return new_error(modeling_errc::memory_error);

            const auto u8str   = it->path().filename().u8string();
            auto*      cstr    = reinterpret_cast<const char*>(u8str.c_str());
            auto&      file    = fs.file_paths.alloc();
            auto       file_id = fs.file_paths.get_id(file);
            file.path          = cstr;
            file.parent        = fs.dir_paths.get_id(dir);
            file.type          = type;
            dir.children.emplace_back(file_id);
        }
        return success();
    } catch (...) {
        jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
            t = "Modeling initialization error";
            format(m, "Fail to register path {}", reg_dir.path.sv());
        });
    }

    return new_error(modeling_errc::memory_error);
}

static status browse_dirs_registred(journal_handler&             jn,
                                    file_access&                 fs,
                                    registred_path&              reg_dir,
                                    const std::filesystem::path& path) noexcept
{
    try {
        std::error_code ec;

        if (std::filesystem::is_directory(path, ec)) {
            auto it = std::filesystem::directory_iterator{ path, ec };
            auto et = std::filesystem::directory_iterator{};

            while (it != et) {
                if (it->is_directory() and
                    not it->path().filename().string().starts_with('.')) {
                    if (not(fs.dir_paths.can_alloc(1) or
                            fs.dir_paths.grow<3, 2>()))
                        return new_error(modeling_errc::memory_error);

                    if (not(reg_dir.children.can_alloc(1) or
                            reg_dir.children.grow<3, 2>()))
                        return new_error(modeling_errc::memory_error);

                    auto  u8str  = it->path().filename().u8string();
                    auto  cstr   = reinterpret_cast<const char*>(u8str.c_str());
                    auto& dir    = fs.dir_paths.alloc();
                    auto  dir_id = fs.dir_paths.get_id(dir);
                    dir.path     = cstr;
                    dir.status   = dir_path::state::unread;
                    dir.parent   = fs.registred_paths.get_id(reg_dir);

                    reg_dir.children.emplace_back(dir_id);
                    if (auto ret =
                          browse_directory(jn, fs, reg_dir, dir, it->path());
                        ret.has_error()) {
                        jn.push(
                          log_level::error, [&](auto& t, auto& m) noexcept {
                              t = "Modeling initialization error";
                              format(m,
                                     "Too many file in application for "
                                     "registred path {} "
                                     "directory {} (size={}, capacityr={})",
                                     reg_dir.name.sv(),
                                     dir.path.sv(),
                                     fs.file_paths.size(),
                                     fs.file_paths.capacity());
                          });
                    }
                }

                it = it.increment(ec);
            }
        }
    } catch (...) {
        jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
            t = "Modeling initialization error";
            format(m, "File system error in paths {}\n", reg_dir.path.sv());
        });
    }

    return success();
}

int file_access::browse_registred(journal_handler&        jn,
                                  const registred_path_id id) noexcept
{
    const auto old = file_paths.ssize();

    if (auto* r = registred_paths.try_to_get(id)) {
        try {
            std::filesystem::path p(r->path.sv());
            std::error_code       ec;

            if (std::filesystem::exists(p, ec)) {
                if (const auto ret = browse_dirs_registred(jn, *this, *r, p);
                    ret.has_error()) {
                    r->status = registred_path::state::error;
                } else {
                    r->status = registred_path::state::read;
                }
            } else {
                r->status = registred_path::state::error;

                jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
                    t = "Modeling initialization error";
                    format(m,
                           "Registred path {} with value `{}' does not exists",
                           r->name.sv(),
                           r->path.sv());
                });
            }
        } catch (...) {
            jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
                t = "Modeling initialization error";
                format(m,
                       "File system error registred patg {} with value `{}'\n",
                       r->name.sv(),
                       r->path.sv());
            });
        }
    }

    return file_paths.ssize() - old;
}

int file_access::browse_registreds(journal_handler& jn) noexcept
{
    const auto old = file_paths.ssize();

    const auto to_del = std::ranges::remove_if(
      recorded_paths, [&](const auto id) noexcept -> bool {
          if (registred_paths.try_to_get(id)) {
              (void)browse_registred(jn, id);
              return false;
          }
          return true;
      });

    recorded_paths.erase(to_del.begin(), to_del.end());

    return file_paths.ssize() - old;
}

status modeling::fill_components() noexcept
{
    return ids.write([&](auto& ids) noexcept -> status {
        const auto file_read_status =
          files.write([&](auto& fs) noexcept -> status {
              fs.browse_registreds(journal);

              for (auto& f : fs.file_paths) {
                  if (f.type == file_path::file_type::dot_file) {
                      if (not(ids.graphs.can_alloc(1) or
                              ids.graphs.template grow<3, 2>()))
                          return new_error(modeling_errc::memory_error);

                      auto&      g        = ids.graphs.alloc();
                      const auto graph_id = ids.graphs.get_id(g);
                      g.file              = fs.file_paths.get_id(f);
                      f.g_id              = graph_id;
                  }
              }

              for (auto& f : fs.file_paths) {
                  if (f.type == file_path::file_type::project_file) {
                  }
              }

              for (auto& f : fs.file_paths) {
                  if (f.type != file_path::file_type::component_file)
                      continue;

                  const auto f_id = fs.file_paths.get_id(f);

                  if (not ids.exists(f.component)) {
                      if (not ids.can_alloc_component(1)) {
                          return new_error(modeling_errc::memory_error);
                      }

                      const auto compo_id = ids.alloc_component();
                      f.component         = compo_id;
                  }

                  ids.component_file_paths[f.component].file = f_id;
              }

              for (auto& g : ids.graphs) {
                  const auto file_id = g.file;

                  if (const auto file = fs.get_fs_path(file_id)) {
                      if (auto ret = parse_dot_file(fs, ids, *file, journal);
                          ret.has_value()) {
                          g      = std::move(*ret);
                          g.file = file_id;
                      } else {
                          journal.push(
                            log_level::warning,
                            [&](auto& t, auto& m, const auto& f) noexcept {
                                t = "Modeling initialization error";
                                format(m,
                                       "Fail to read dot graph `{}' ({}:{})",
                                       reinterpret_cast<const char*>(f.c_str()),
                                       ordinal(ret.error().cat()),
                                       ret.error().value());
                            },
                            *file);
                      }
                  }
              }

              return success();
          });

        if (file_read_status.has_error())
            return file_read_status.error();

        return files.read([&](const auto& fs, auto) noexcept -> status {
            auto have_unread_component = ids.ssize() > 0;

            while (have_unread_component) {
                auto component_read   = 0;
                have_unread_component = false;

                for (auto id : ids) {
                    auto& compo = ids.components[id];
                    if (compo.state == component_status::unmodified)
                        continue;

                    auto& filepath = ids.component_file_paths[id];
                    if (const auto f = make_file(fs, filepath); f.has_value()) {
                        if (const auto ret = load_component(fs, ids, *f, id);
                            ret.has_error()) {
                            switch (compo.state) {
                            case component_status::unread:
                                journal.push(
                                  log_level::warning,
                                  [&](auto& t, auto& m) noexcept {
                                      t = "Modeling initialization error";
                                      format(m,
                                             "Need to read dependency for "
                                             "component {} ({})",
                                             compo.name.sv(),
                                             ordinal(id));
                                  });
                                have_unread_component = true;
                                break;

                            case component_status::modified:
                                have_unread_component = true;
                                break;

                            case component_status::unmodified:
                                have_unread_component = true;
                                break;

                            case component_status::unreadable:
                                journal.push(
                                  log_level::warning,
                                  [&](auto& t, auto& m) noexcept {
                                      t = "Modeling initialization error";
                                      format(m,
                                             "Fail to read component `{}'",
                                             compo.name.sv());
                                  });
                                break;
                            }
                        } else {
                            ++component_read;
                        }
                    } else {
                        have_unread_component = true;
                    }
                }

                if (component_read == 0)
                    return success();
            }

            return success();
        });

        return success();
    });

    return success();
}

void modeling::browse_file_system() noexcept
{
    files.write([&](file_access& fs) { fs.browse_registreds(journal); });
}

status modeling::load_component(const file_access&           files,
                                component_access&            ids,
                                const std::filesystem::path& filename,
                                const component_id           compo_id) noexcept
{
    if (not ids.exists(compo_id))
        return new_error(modeling_errc::component_load_error);

    auto& compo = ids.components[compo_id];

    try {
        auto f = file::open(filename, file_mode{ file_open_options::read });

        if (f.has_value()) {
            json_dearchiver j;

            if (not j(
                  *this, files, ids, filename.string(), compo_id, compo, *f)) {
                return error_code(modeling_errc::component_load_error);
            }

            compo.state = component_status::unmodified;

            std::filesystem::path descfilename = filename;
            descfilename.replace_extension(".desc");
            auto ec = std::error_code{};

            if (std::filesystem::exists(descfilename, ec) and not ec) {
                auto d = file::open(descfilename,
                                    file_mode{ file_open_options::read });

                if (d.has_value()) {
                    auto& desc = ids.component_descriptions[compo_id];
                    auto  view = std::span<char>(desc.data(), desc.capacity());
                    auto  fileview = d->read_entire_file(view);
                    desc.resize(fileview.size());
                }
            }
        } else {
            compo.state = component_status::unreadable;
            return f.error();
        }
    } catch (const std::bad_alloc& /*e*/) {
        compo.state = component_status::unreadable;
        return new_error(modeling_errc::memory_error);
    } catch (...) {
        compo.state = component_status::unreadable;
        return new_error(modeling_errc::memory_error);
    }

    return success();
}

void file_access::remove(const file_path_id id) noexcept
{
    if (auto* f = file_paths.try_to_get(id)) {
        if (auto* d = dir_paths.try_to_get(f->parent)) {
            if (auto* r = registred_paths.try_to_get(d->parent)) {
                if (const auto opt = make_file(*r, *d, *f); opt.has_value()) {
                    std::error_code ec;
                    std::filesystem::remove(*opt, ec);

                    file_paths.free(*f);

                    auto it = std::ranges::find(d->children, id);
                    if (it != std::ranges::end(d->children))
                        d->children.erase(it);
                }
            }
        }
    }
}

void file_access::refresh(const dir_path_id id) noexcept
{
    if (auto* d = dir_paths.try_to_get(id)) {
        if (auto* r = registred_paths.try_to_get(d->parent)) {
            try {
                std::error_code       ec;
                std::filesystem::path dir{ r->path.sv() };
                dir /= d->path.sv();

                if (std::filesystem::is_directory(dir, ec)) {
                    auto it = std::filesystem::directory_iterator{ dir, ec };
                    auto et = std::filesystem::directory_iterator{};

                    while (it != et) {
                        if (it->is_regular_file()) {
                            const auto type =
                              get_extension(it->path().filename().string());
                            if (type != file_path::file_type::undefined_file) {
                                const auto f = find_file_in_directory(
                                  id, it->path().string());
                                if (is_undefined(f)) {
                                    const auto new_file = alloc_file(
                                      id, it->path().filename().string(), type);

                                    d->children.push_back(new_file);
                                }
                            }
                        }

                        it = it.increment(ec);
                    }
                } else {
                    d->flags.set(dir_path::dir_flags::access_error);
                }
            } catch (const std::exception& /*e*/) {
                d->flags.set(dir_path::dir_flags::access_error);
            }
        }
    }
}

auto file_access::find_file_in_directory(
  const dir_path_id          id,
  const std::string_view     filename,
  const file_path::file_type type) const noexcept -> file_path_id
{
    if (const auto* dir = dir_paths.try_to_get(id))
        for (const auto f_id : dir->children)
            if (const auto* file = file_paths.try_to_get(f_id))
                if (filename == file->path.sv() and
                    (type == file->type or
                     type == file_path::file_type::undefined_file))
                    return f_id;

    return undefined<file_path_id>();
}

auto file_access::find_file(const std::string_view reg,
                            const std::string_view dir,
                            const std::string_view file) const noexcept
  -> file_path_id
{
    const auto reg_id  = find_registred_path_by_name(reg);
    const auto dir_id  = is_defined(reg_id)
                           ? find_directory_in_registry(reg_id, dir)
                           : find_directory(dir);
    const auto file_id = find_file_in_directory(dir_id, file);

    return file_paths.try_to_get(file_id) ? file_id : undefined<file_path_id>();
}

auto file_access::find_directory(std::string_view name) const noexcept
  -> dir_path_id
{
    for (const auto r_id : recorded_paths)
        if (const auto* reg = registred_paths.try_to_get(r_id))
            for (const auto d_id : reg->children)
                if (const auto* dir = dir_paths.try_to_get(d_id))
                    if (dir->path.sv() == name)
                        return d_id;

    return undefined<dir_path_id>();
}

auto file_access::find_registred_path_by_name(
  const std::string_view name) const noexcept -> registred_path_id
{
    for (const auto r_id : recorded_paths)
        if (const auto* reg = registred_paths.try_to_get(r_id))
            if (reg->name.sv() == name)
                return r_id;

    return undefined<registred_path_id>();
}

auto file_access::find_directory_in_registry(
  const registred_path_id id,
  std::string_view        name) const noexcept -> dir_path_id
{
    if (const auto* reg = registred_paths.try_to_get(id))
        for (const auto d_id : reg->children)
            if (const auto* dir = dir_paths.try_to_get(d_id))
                if (dir->path.sv() == name)
                    return d_id;

    return undefined<dir_path_id>();
}

file_path_id file_access::alloc_file(const dir_path_id          id,
                                     const std::string_view     name,
                                     const file_path::file_type type,
                                     const component_id         compo_id,
                                     const project_id project_id) noexcept
{
    if (auto* d = dir_paths.try_to_get(id)) {
        if (file_paths.can_alloc(1) or file_paths.grow<3, 2>()) {
            auto& file     = file_paths.alloc();
            auto  fid      = file_paths.get_id(file);
            file.path      = name;
            file.component = compo_id;
            file.pj_id     = project_id;
            file.parent    = id;
            file.type      = type;
            d->children.push_back(fid);
            return fid;
        }
    }

    return undefined<file_path_id>();
}

dir_path_id file_access::alloc_dir(const registred_path_id id,
                                   const std::string_view  path) noexcept
{
    if (auto* r = registred_paths.try_to_get(id)) {
        if (dir_paths.can_alloc(1) or dir_paths.grow<3, 2>()) {
            auto& dir    = dir_paths.alloc();
            auto  dir_id = dir_paths.get_id(dir);
            dir.path     = path;
            dir.parent   = id;
            dir.status   = dir_path::state::unread;
            r->children.push_back(dir_id);
            return dir_id;
        }
    }

    return undefined<dir_path_id>();
}

registred_path_id file_access::alloc_registred(const std::string_view name,
                                               const int priority) noexcept
{
    if (registred_paths.can_alloc(1) or registred_paths.grow<3, 2>()) {
        auto& reg    = registred_paths.alloc();
        auto  reg_id = registred_paths.get_id(reg);

        reg.name = name;
        reg.priority =
          static_cast<i8>(std::clamp<int>(priority, INT8_MIN, INT8_MAX));
        recorded_paths.emplace_back(reg_id);

        auto to_erase =
          std::ranges::remove_if(recorded_paths, [&](const auto id) {
              const auto* r = registred_paths.try_to_get(id);
              return r == nullptr;
          });

        if (to_erase.begin() != to_erase.end())
            recorded_paths.erase(to_erase.begin(), to_erase.end());

        std::ranges::sort(recorded_paths, [&](const auto a, const auto b) {
            const auto* ra = registred_paths.try_to_get(a);
            const auto* rb = registred_paths.try_to_get(b);
            if (ra and rb)
                return ra->priority > rb->priority;
            return false;
        });

        return reg_id;
    }

    return undefined<registred_path_id>();
}

bool file_access::exists(const registred_path_id id) const noexcept
{
    if (auto* reg = registred_paths.try_to_get(id)) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            return std::filesystem::exists(p, ec);
        } catch (...) {
        }
    }

    return false;
}

bool file_access::exists(const dir_path_id id) const noexcept
{
    if (const auto* d = dir_paths.try_to_get(id)) {
        if (const auto* r = registred_paths.try_to_get(d->parent)) {
            try {
                std::error_code       ec;
                std::filesystem::path p{ r->path.sv() };
                if (std::filesystem::exists(p, ec)) {
                    p /= d->path.sv();
                    return std::filesystem::exists(p, ec);
                }
            } catch (...) {
            }
        }
    }

    return false;
}

bool file_access::create_directories(const registred_path_id id) const noexcept
{
    if (auto* reg = registred_paths.try_to_get(id)) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            if (std::filesystem::exists(p, ec))
                return false;

            return std::filesystem::create_directories(p, ec);
        } catch (...) {
        }
    }

    return false;
}

bool file_access::create_directories(const dir_path_id id) const noexcept
{
    if (const auto* d = dir_paths.try_to_get(id)) {
        if (const auto* r = registred_paths.try_to_get(d->parent)) {
            try {
                std::error_code       ec;
                std::filesystem::path p{ r->path.sv() };
                p /= d->path.sv();
                if (std::filesystem::exists(p, ec))
                    return false;

                return std::filesystem::create_directories(p, ec);
            } catch (...) {
            }
        }
    }

    return false;
}

void file_access::remove_directory(const dir_path_id id) noexcept
{
    if (auto* d = dir_paths.try_to_get(id)) {
        debug::ensure(not d->path.empty());
        if (d->path.empty())
            return;

        for (const auto f_id : d->children)
            file_paths.free(f_id);

        if (const auto* r = registred_paths.try_to_get(d->parent)) {
            try {
                std::filesystem::path p(r->path.sv());
                p /= d->path.sv();

                std::error_code ec;
                if (std::filesystem::exists(p, ec))
                    std::filesystem::remove_all(p, ec);
            } catch (...) {
            }
        }

        dir_paths.free(id);
    }
}

void file_access::remove_files(const dir_path_id id) noexcept
{
    if (auto* d = dir_paths.try_to_get(id)) {
        if (d->path.empty())
            return;

        if (const auto* r = registred_paths.try_to_get(d->parent)) {
            try {
                std::filesystem::path p(r->path.sv());
                p /= d->path.sv();

                for (const auto f_id : d->children) {
                    if (auto* f = file_paths.try_to_get(f_id)) {
                        const auto file = p / f->path.sv();

                        std::error_code ec;
                        if (std::filesystem::exists(file, ec))
                            std::filesystem::remove(file, ec);
                    }
                }
            } catch (...) {
            }
        }

        d->children.clear();
    }
}

void file_access::remove_file(const file_path_id id) noexcept
{
    if (const auto* f = file_paths.try_to_get(id)) {
        if (auto* d = dir_paths.try_to_get(f->parent)) {
            if (const auto* r = registred_paths.try_to_get(d->parent)) {
                debug::ensure(not f->path.empty());
                debug::ensure(not d->path.empty());
                debug::ensure(not r->path.empty());

                if (not(f->path.empty() and d->path.empty() and
                        r->path.empty())) {
                    std::error_code ec;

                    std::filesystem::path p{ r->path.sv() };
                    p /= d->path.sv();
                    p /= f->path.sv();

                    if (std::filesystem::exists(p, ec))
                        std::filesystem::remove(p, ec);

                    const auto to_del = std::ranges::remove_if(
                      d->children, [&](const auto fid) { return id == fid; });

                    if (to_del.begin() != to_del.end())
                        d->children.erase(to_del.begin(), to_del.end());

                    file_paths.free(id);
                }
            }
        }
    }
}

void file_access::move_file(const dir_path_id  to,
                            const file_path_id id) noexcept
{
    if (auto* f = file_paths.try_to_get(id)) {
        if (to != f->parent) {
            const auto old_id = f->parent;

            if (auto* new_d = dir_paths.try_to_get(to)) {
                f->parent = to;
                new_d->children.emplace_back(file_paths.get_id(*f));

                if (auto* old_d = dir_paths.try_to_get(old_id)) {
                    const auto to_del = std::ranges::remove_if(
                      old_d->children,
                      [&](const auto fid) { return id == fid; });

                    if (to_del.begin() != to_del.end())
                        old_d->children.erase(to_del.begin(), to_del.end());
                }
            }
        }
    }
}

void file_access::free(const file_path_id file) noexcept
{
    file_paths.free(file);
}

void file_access::free(const dir_path_id dir) noexcept
{
    if (auto* d = dir_paths.try_to_get(dir))
        for (const auto f_id : d->children)
            if (auto* f = file_paths.try_to_get(f_id))
                f->parent = undefined<dir_path_id>();

    dir_paths.free(dir);
}

void file_access::free(const registred_path_id reg_dir) noexcept
{
    if (auto* r = registred_paths.try_to_get(reg_dir))
        for (const auto d_id : r->children)
            free(d_id);

    registred_paths.free(reg_dir);
}

expected<std::filesystem::path> file_access::get_fs_path(
  const file_path_id id) const noexcept
{
    try {
        if (auto* file = file_paths.try_to_get(id)) {
            if (auto* dir = dir_paths.try_to_get(file->parent)) {
                if (auto* reg = registred_paths.try_to_get(dir->parent)) {
                    std::filesystem::path p{ reg->path.sv() };
                    p /= dir->path.sv();
                    p /= file->path.sv();
                    return p;
                }
                return new_error(modeling_errc::recorded_directory_error);
            }
            return new_error(modeling_errc::directory_error);
        }
        return new_error(modeling_errc::file_error);
    } catch (...) {
        return new_error(modeling_errc::file_error);
    }
}

expected<std::filesystem::path> file_access::get_fs_path(
  const dir_path_id id) const noexcept
{
    try {
        if (auto* dir = dir_paths.try_to_get(id)) {
            if (auto* reg = registred_paths.try_to_get(dir->parent)) {
                std::filesystem::path p{ reg->path.sv() };
                p /= dir->path.sv();
                return p;
            }
            return new_error(modeling_errc::recorded_directory_error);
        }
        return new_error(modeling_errc::directory_error);
    } catch (...) {
        return new_error(modeling_errc::file_error);
    }
}

expected<std::filesystem::path> file_access::get_fs_path(
  const registred_path_id id) const noexcept
{
    try {
        if (auto* reg = registred_paths.try_to_get(id)) {
            return std::filesystem::path{ reg->path.sv() };
        }
        return new_error(modeling_errc::recorded_directory_error);
    } catch (...) {
        return new_error(modeling_errc::file_error);
    }
}

expected<component_id> component_access::copy(
  const component_id src_id) noexcept
{
    if (not ids.exists(src_id))
        return new_error(modeling_errc::component_not_found);

    if (not can_alloc_component(1))
        return new_error(modeling_errc::component_container_full);

    const auto& src    = components[src_id];
    auto        dst_id = undefined<component_id>();

    switch (src.type) {
    case component_type::none:
        if (not can_alloc_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id = alloc_component();
        break;

    case component_type::generic: {
        if (not can_alloc_generic_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id           = alloc_generic_component();
        auto  sub_dst_id = components[dst_id].id.generic_id;
        auto& sub_dst    = generic_components.get(sub_dst_id);

        sub_dst = generic_components.get(components[src_id].id.generic_id);
    } break;

    case component_type::grid: {
        if (not can_alloc_grid_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id           = alloc_grid_component();
        auto  sub_dst_id = components[dst_id].id.grid_id;
        auto& sub_dst    = grid_components.get(sub_dst_id);

        sub_dst = grid_components.get(components[src_id].id.grid_id);
    } break;

    case component_type::graph: {
        if (not can_alloc_graph_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id           = alloc_graph_component();
        auto  sub_dst_id = components[dst_id].id.graph_id;
        auto& sub_dst    = graph_components.get(sub_dst_id);

        sub_dst = graph_components.get(components[src_id].id.graph_id);
    } break;

    case component_type::hsm: {
        if (not can_alloc_hsm_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id           = alloc_hsm_component();
        auto  sub_dst_id = components[dst_id].id.hsm_id;
        auto& sub_dst    = hsm_components.get(sub_dst_id);

        sub_dst = hsm_components.get(components[src_id].id.hsm_id);
    } break;

    case component_type::simulation: {
        if (not can_alloc_sim_component(1))
            return new_error(modeling_errc::component_container_full);

        dst_id           = alloc_sim_component();
        auto  sub_dst_id = components[dst_id].id.sim_id;
        auto& sub_dst    = sim_components.get(sub_dst_id);

        sub_dst = sim_components.get(components[src_id].id.sim_id);
    } break;
    }

    auto& dst = components[dst_id];

    if (not dst.x.can_alloc(src.x.size()))
        return new_error(modeling_errc::component_input_container_full);

    if (not dst.y.can_alloc(src.y.size()))
        return new_error(modeling_errc::component_output_container_full);

    src.x.for_each([&](auto /*id*/,
                       const auto  type,
                       const auto& name,
                       const auto& pos) noexcept {
        auto new_id                    = dst.x.alloc_id();
        dst.x.get<port_option>(new_id) = type;
        dst.x.get<port_str>(new_id)    = name;
        dst.x.get<position>(new_id)    = pos;
    });

    src.y.for_each([&](auto /*id*/,
                       const auto  type,
                       const auto& name,
                       const auto& pos) noexcept {
        auto new_id                    = dst.y.alloc_id();
        dst.y.get<port_option>(new_id) = type;
        dst.y.get<port_str>(new_id)    = name;
        dst.y.get<position>(new_id)    = pos;
    });

    dst.name  = src.name;
    dst.state = src.state;

    return dst_id;
}

bool component_access::can_alloc_component(int count) noexcept
{
    return (ids.can_alloc(count) or ids.grow<3, 2>()) and
           components.resize(ids.capacity()) and
           component_colors.resize(ids.capacity()) and
           component_file_paths.resize(ids.capacity());
}

bool component_access::can_alloc_grid_component(int count) noexcept
{
    return can_alloc_component(count) and
           (grid_components.can_alloc(count) or grid_components.grow<3, 2>());
}

bool component_access::can_alloc_graph_component(int count) noexcept
{
    return can_alloc_component(count) and
           (graph_components.can_alloc(count) or graph_components.grow<3, 2>());
}

bool component_access::can_alloc_generic_component(int count) noexcept
{
    return can_alloc_component(count) and
           (generic_components.can_alloc(count) or
            generic_components.grow<3, 2>());
}

bool component_access::can_alloc_hsm_component(int count) noexcept
{
    return can_alloc_component(count) and
           (hsm_components.can_alloc(count) or hsm_components.grow<3, 2>());
}

bool component_access::can_alloc_sim_component(int count) noexcept
{
    return can_alloc_component(count) and
           (sim_components.can_alloc(count) or sim_components.grow<3, 2>());
}

component_id component_access::alloc_component() noexcept
{
    if (not can_alloc_component(1))
        return undefined<component_id>();

    auto new_compo_id                    = ids.alloc();
    component_colors[new_compo_id]       = { 1.f, 1.f, 1.f, 1.f };
    component_file_paths[new_compo_id]   = {};
    component_descriptions[new_compo_id] = {};

    auto& new_compo = components[new_compo_id];
    format(new_compo.name, "component-{}", get_index(new_compo_id));
    new_compo.type  = component_type::none;
    new_compo.state = component_status::modified;
    new_compo.x.clear();
    new_compo.y.clear();

    return new_compo_id;
}

component_id component_access::alloc_grid_component() noexcept
{
    if (not can_alloc_grid_component(1))
        return undefined<component_id>();

    const auto new_compo_id = alloc_component();
    auto&      new_compo    = components[new_compo_id];
    format(new_compo.name, "grid {}", get_index(new_compo_id));
    new_compo.type = component_type::grid;

    auto& grid = grid_components.alloc();
    grid.resize(4, 4, undefined<component_id>());
    new_compo.id.grid_id = grid_components.get_id(grid);

    return new_compo_id;
}

component_id component_access::alloc_generic_component() noexcept
{
    if (not can_alloc_generic_component(1))
        return undefined<component_id>();

    const auto new_compo_id = alloc_component();
    auto&      new_compo    = components[new_compo_id];
    format(new_compo.name, "simple {}", get_index(new_compo_id));
    new_compo.type = component_type::generic;

    auto& new_s_compo       = generic_components.alloc();
    new_compo.id.generic_id = generic_components.get_id(new_s_compo);

    return new_compo_id;
}

component_id component_access::alloc_graph_component() noexcept
{
    if (not can_alloc_graph_component(1))
        return undefined<component_id>();

    const auto new_compo_id = alloc_component();
    auto&      new_compo    = components[new_compo_id];
    format(new_compo.name, "graph {}", get_index(new_compo_id));
    new_compo.type = component_type::graph;

    auto& graph           = graph_components.alloc();
    new_compo.id.graph_id = graph_components.get_id(graph);

    return new_compo_id;
}

component_id component_access::alloc_hsm_component() noexcept
{
    if (not can_alloc_hsm_component(1))
        return undefined<component_id>();

    const auto new_compo_id = alloc_component();
    auto&      new_compo    = components[new_compo_id];
    format(new_compo.name, "hsm {}", get_index(new_compo_id));
    new_compo.type = component_type::hsm;

    auto& h             = hsm_components.alloc();
    new_compo.id.hsm_id = hsm_components.get_id(h);
    h.clear();
    h.machine.states[0].super_id = hierarchical_state_machine::invalid_state_id;
    h.machine.top_state          = 0;

    return new_compo_id;
}

component_id component_access::alloc_sim_component() noexcept
{
    if (not can_alloc_sim_component(1))
        return undefined<component_id>();

    const auto new_compo_id = alloc_component();
    auto&      new_compo    = components[new_compo_id];
    format(new_compo.name, "sim {}", get_index(new_compo_id));
    new_compo.type = component_type::simulation;

    auto& h             = sim_components.alloc();
    new_compo.id.sim_id = sim_components.get_id(h);

    return new_compo_id;
}

static bool can_add_component(const component_access& ids,
                              const component_id      c,
                              vector<component_id>&   out,
                              component_id            search) noexcept
{
    if (c == search)
        return false;

    if (ids.exists(c))
        out.emplace_back(c);

    return true;
}

static bool can_add_component(const component_access& ids,
                              const component&        compo,
                              vector<component_id>&   out,
                              component_id            search) noexcept
{
    switch (compo.type) {
    case component_type::grid: {
        auto id = compo.id.grid_id;
        if (auto* g = ids.grid_components.try_to_get(id); g) {
            for (const auto ch : g->children())
                if (not can_add_component(ids, ch, out, search))
                    return false;
        }
    } break;

    case component_type::graph: {
        auto id = compo.id.graph_id;
        if (auto* g = ids.graph_components.try_to_get(id); g) {
            for (const auto edge_id : g->g.nodes)
                if (not can_add_component(
                      ids, g->g.node_components[edge_id], out, search))
                    return false;
        }
    } break;

    case component_type::generic: {
        auto id = compo.id.generic_id;
        if (auto* s = ids.generic_components.try_to_get(id); s) {
            for (const auto& ch : s->children)
                if (ch.type == child_type::component)
                    if (not can_add_component(ids, ch.id.compo_id, out, search))
                        return false;
        }
        break;
    }

    default:
        break;
    }

    return true;
}

bool component_access::can_add(const component_id parent_id,
                               const component_id other_id) const noexcept
{
    vector<component_id> stack;

    if (parent_id == other_id)
        return false;

    if (not can_add_component(*this, other_id, stack, parent_id))
        return false;

    while (!stack.empty()) {
        auto id = stack.back();
        stack.pop_back();

        if (exists(id)) {
            if (not can_add_component(*this, components[id], stack, parent_id))
                return false;
        }
    }

    return true;
}

void component_access::clear(const component_id compo_id) noexcept
{
    if (not ids.exists(compo_id))
        return;

    auto& compo = components[compo_id];

    switch (compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        generic_components.free(compo.id.generic_id);
        break;

    case component_type::grid:
        grid_components.free(compo.id.grid_id);
        break;

    case component_type::graph:
        graph_components.free(compo.id.graph_id);
        break;

    case component_type::hsm:
        hsm_components.free(compo.id.hsm_id);
        break;

    case component_type::simulation:
        sim_components.free(compo.id.sim_id);
        break;
    }

    compo.type  = component_type::none;
    compo.state = component_status::modified;
}

void component_access::free(const component_id compo_id) noexcept
{
    if (not ids.exists(compo_id))
        return;

    clear(compo_id);
    ids.free(compo_id);
}

status modeling::save(const component_access& ids,
                      const file_access&      fs,
                      const component_id      id) noexcept
{
    if (not ids.exists(id))
        return new_error(modeling_errc::component_load_error);

    const auto filenames = make_component_files(fs, ids, id);

    if (not filenames.has_value())
        return new_error(modeling_errc::file_error);

    auto cfile =
      file::open(filenames->component, file_mode{ file_open_options::write });
    if (cfile.has_error()) [[unlikely]]
        return cfile.error();

    json_archiver j;
    if (auto ret = j(*this, fs, ids, id, *cfile); ret.has_error())
        return ret.error();
    return success();

    auto dfile =
      file::open(filenames->description, file_mode{ file_open_options::write });

    if (dfile.has_value()) [[likely]] {
        const auto& str = ids.component_descriptions[id];
        if (not str.empty())
            dfile->write(str.sv());
    }

    return success();
}

status modeling::save(const component_id compo_id) noexcept
{
    return ids.read([&](const auto& ids, auto) -> status {
        if (not ids.exists(compo_id))
            return new_error(modeling_errc::component_load_error);

        const auto filenames = files.read(
          [&](const auto& fs, auto) -> std::optional<component_filenames> {
              return make_component_files(fs, ids, compo_id);
          });

        if (not filenames.has_value())
            return new_error(modeling_errc::file_error);

        auto cfile = file::open(filenames->component,
                                file_mode{ file_open_options::write });
        if (cfile.has_error()) [[unlikely]]
            return cfile.error();

        const auto ret = files.read([&](const auto& files, auto) -> status {
            json_archiver j;
            if (auto ret = j(*this, files, ids, compo_id, *cfile);
                ret.has_error())
                return ret.error();
            return success();
        });

        auto dfile = file::open(filenames->description,
                                file_mode{ file_open_options::write });

        if (dfile.has_value()) [[likely]] {
            const auto& str = ids.component_descriptions[compo_id];
            if (not str.empty())
                dfile->write(str.sv());
        }

        return ret;
    });
}

} // namespace irt
