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

modeling::modeling(journal_handler&                   jnl,
                   const modeling_reserve_definition& res) noexcept
  : descriptions(res.files.value())
  , generic_components(res.generic_compos.value())
  , grid_components(res.grid_compos.value())
  , graph_components(res.graph_compos.value())
  , hsm_components(res.hsm_compos.value())
  , components(res.components.value())
  , hsms(res.hsm_compos.value())
  , graphs(res.graph_compos.value())
  , files(res.regs.value(), res.dirs.value(), res.files.value())
  , journal(jnl)
{
    files.read([&](const auto& fs, const auto /*vers*/) {
        if (descriptions.capacity() == 0 or
            generic_components.capacity() == 0 or
            grid_components.capacity() == 0 or
            graph_components.capacity() == 0 or
            hsm_components.capacity() == 0 or components.capacity() == 0 or
            fs.registred_paths.capacity() == 0 or
            fs.dir_paths.capacity() == 0 or fs.file_paths.capacity() == 0 or
            hsms.capacity() == 0 or graphs.capacity() == 0 or
            fs.component_repertories.capacity() == 0)
            journal.push(log_level::error, [&](auto& t, auto& m) noexcept {
                t = "Modeling initialization error";
                format(m,
                       "descriptions          {:>8} ({:>8})"
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
                       descriptions.capacity(),
                       res.files.value(),
                       generic_components.capacity(),
                       res.generic_compos.value(),
                       grid_components.capacity(),
                       res.grid_compos.value(),
                       graph_components.capacity(),
                       res.graph_compos.value(),
                       hsm_components.capacity(),
                       res.hsm_compos.value(),
                       components.capacity(),
                       res.components.value(),
                       fs.registred_paths.capacity(),
                       res.regs.value(),
                       fs.dir_paths.capacity(),
                       res.dirs.value(),
                       fs.file_paths.capacity(),
                       res.files.value(),
                       hsms.capacity(),
                       res.hsm_compos.value(),
                       graphs.capacity(),
                       res.graph_compos.value(),
                       res.components.value(),
                       fs.component_repertories.capacity(),
                       res.regs.value());
            });
    });
}

static void component_loading(modeling&                    mod,
                              modeling::file_access&       fs,
                              registred_path&              reg_dir,
                              dir_path&                    dir,
                              file_path&                   file,
                              const std::filesystem::path& path) noexcept
{
    if (not mod.components.can_alloc(1) and mod.components.grow<3, 2>())
        return;

    auto compo_id = mod.components.alloc_id();

    mod.components.get<component_color>(compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& compo    = mod.components.get<component>(compo_id);
    compo.file     = fs.file_paths.get_id(file);
    file.component = compo_id;
    compo.type     = component_type::none;
    compo.state    = component_status::unread;

    try {
        std::filesystem::path desc_file{ path };
        desc_file.replace_extension(".desc");

        std::error_code ec;
        if (std::filesystem::exists(desc_file, ec)) {
            debug_logi(6, "found description file {}\n", desc_file.string());
            if (mod.descriptions.can_alloc(1)) {
                compo.desc = mod.descriptions.alloc_id();
                mod.descriptions.get<description_str>(compo.desc).clear();
                mod.descriptions.get<description_status>(compo.desc) =
                  description_status::unread;
            } else {
                mod.journal.push(
                  log_level::error, [&](auto& t, auto& m) noexcept {
                      t = "Modeling initialization error";
                      format(m,
                             "Fail to allocate more description ({})",
                             mod.descriptions.size());
                  });
            }
        }
    } catch (const std::exception& /*e*/) {
        mod.journal.push(log_level::error, [&](auto& t, auto& m) noexcept {
            t = "Modeling initialization error";
            format(m,
                   "File system error in {} {} {}",
                   reg_dir.path.sv(),
                   dir.path.sv(),
                   file.path.sv());
        });
    }
}

static auto detect_file_type(const std::filesystem::path& file) noexcept
  -> file_path::file_type
{
    const auto ext = file.extension();

    if (ext == ".irt")
        return file_path::file_type::irt_file;

    if (ext == ".dot")
        return file_path::file_type::dot_file;

    if (ext == ".txt")
        return file_path::file_type::txt_file;

    if (ext == ".data")
        return file_path::file_type::data_file;

    if (ext == ".pirt")
        return file_path::file_type::project_file;

    return file_path::file_type::undefined_file;
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

// static file_path& add_to_dir(modeling::file_access&     fs,
//                              dir_path&                  dir,
//                              const file_path::file_type type,
//                              const std::u8string&       u8str) noexcept
// {
//     auto* cstr    = reinterpret_cast<const char*>(u8str.c_str());
//     auto& file    = fs.file_paths.alloc();
//     auto  file_id = fs.file_paths.get_id(file);
//     file.path     = cstr;
//     file.parent   = fs.dir_paths.get_id(dir);
//     file.type     = type;
//     dir.children.emplace_back(file_id);

//     return file;
// }

// static void prepare_component_loading(journal_handler&       jn,
//                                       modeling::file_access& fs,
//                                       registred_path&        reg_dir,
//                                       dir_path&              dir,
//                                       std::filesystem::path  path) noexcept
// {
//     try {
//         std::error_code ec;

//         auto it            = std::filesystem::directory_iterator{ path, ec };
//         auto et            = std::filesystem::directory_iterator{};
//         bool too_many_file = false;

//         for (; it != et; it = it.increment(ec)) {
//             if (not it->is_regular_file())
//                 continue;

//             const auto type = detect_file_type(it->path());

//             switch (type) {
//             case file_path::file_type::undefined_file:
//                 break;
//             case file_path::file_type::irt_file:
//                 debug_logi(6, "found irt file {}\n", it->path().string());
//                 if (fs.file_paths.can_alloc() && mod.components.can_alloc(1))
//                 {
//                     auto& file = add_to_dir(
//                       fs, dir, type, it->path().filename().u8string());

//                     prepare_component_loading(
//                       mod, fs, reg_dir, dir, file, it->path());
//                 } else {
//                     too_many_file = true;
//                 }
//                 break;
//             case file_path::file_type::dot_file:
//                 debug_logi(6, "found dot file {}\n", it->path().string());
//                 if (fs.file_paths.can_alloc() and mod.graphs.can_alloc()) {
//                     auto& file = add_to_dir(
//                       fs, dir, type, it->path().filename().u8string());

//                     auto& g = mod.graphs.alloc();
//                     g.file  = fs.file_paths.get_id(file);
//                 } else {
//                     too_many_file = true;
//                 }
//                 break;
//             case file_path::file_type::txt_file:
//                 break;
//             case file_path::file_type::data_file:
//                 break;
//             case file_path::file_type::project_file:
//                 debug_logi(
//                   6, "found project irt file {}\n", it->path().string());
//                 if (fs.file_paths.can_alloc() && mod.components.can_alloc(1))
//                 {
//                     (void)add_to_dir(
//                       fs, dir, type, it->path().filename().u8string());
//                     // @TODO load project file?
//                 } else {
//                     too_many_file = true;
//                 }
//                 break;
//             }
//         }

//         if (too_many_file) {
//             jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
//                 t = "Modeling initialization error";
//                 format(m,
//                        "Too many file in application for registred path {} "
//                        "directory {}",
//                        reg_dir.path.sv(),
//                        dir.path.sv());
//             });
//         }
//     } catch (...) {
//         jn.push(log_level::error, [&](auto& t, auto& m) noexcept {
//             t = "Modeling initialization error";
//             format(m, "Fail to register path {}", reg_dir.path.sv());
//         });
//     }
// }

static status browse_directory(journal_handler&       jn,
                               modeling::file_access& fs,
                               registred_path&        reg_dir,
                               dir_path&              dir,
                               std::filesystem::path  path) noexcept
{
    try {
        std::error_code ec;

        auto it = std::filesystem::directory_iterator{ path, ec };
        auto et = std::filesystem::directory_iterator{};

        for (; it != et; it = it.increment(ec)) {
            if (not it->is_regular_file())
                continue;

            const auto type = detect_file_type(it->path());
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
                                    modeling::file_access&       fs,
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

int modeling::file_access::browse_registred(journal_handler&        jn,
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

int modeling::file_access::browse_registreds(journal_handler& jn) noexcept
{
    const auto old = file_paths.ssize();

    const auto to_del = std::ranges::remove_if(
      component_repertories, [&](const auto id) noexcept -> bool {
          if (registred_paths.try_to_get(id)) {
              (void)browse_registred(jn, id);
              return false;
          }
          return true;
      });

    component_repertories.erase(to_del.begin(), to_del.end());

    return file_paths.ssize() - old;
}

status modeling::fill_components() noexcept
{
    struct f_to_c {
        f_to_c(file_path_id f_id_, component_id c_id_) noexcept
          : f_id{ f_id_ }
          , to{ .component = c_id_ }
        {}

        f_to_c(file_path_id f_id_, project_id pj_id_) noexcept
          : f_id{ f_id_ }
          , to{ .project = pj_id_ }
        {}

        f_to_c(file_path_id f_id_, graph_id g_id) noexcept
          : f_id{ f_id_ }
          , to{ .graph = g_id }
        {}

        file_path_id f_id;
        union {
            component_id component{ 0 };
            project_id   project;
            graph_id     graph;
        } to;
    };

    vector<f_to_c> file_id_to_compo_id;

    if (const auto ret =
          files.read([&](const auto& fs, const auto /*vers*/) -> status {
              file_id_to_compo_id.reserve(fs.file_paths.size());

              for (const auto& f : fs.file_paths) {
                  if (f.type == file_path::file_type::dot_file) {
                      if (not(graphs.can_alloc(1) or graphs.grow<3, 2>()))
                          return new_error(modeling_errc::memory_error);

                      auto&      g        = graphs.alloc();
                      const auto graph_id = graphs.get_id(g);
                      const auto file_id  = fs.file_paths.get_id(f);
                      g.file              = fs.file_paths.get_id(f);

                      file_id_to_compo_id.emplace_back(file_id, graph_id);
                  }
              }

              for (const auto& f : fs.file_paths) {
                  if (f.type == file_path::file_type::project_file) {
                  }
              }

              for (const auto& f : fs.file_paths) {
                  if (f.type == file_path::file_type::irt_file) {
                      if (not components.exists(f.component)) {
                          const auto file_id  = fs.file_paths.get_id(f);
                          const auto compo_id = components.alloc_id();

                          components.get<component_color>(
                            compo_id) = { 1.f, 1.f, 1.f, 1.f };
                          auto& compo = components.get<component>(compo_id);
                          compo.file  = file_id;
                          compo.type  = component_type::none;
                          compo.state = component_status::unread;

                          file_id_to_compo_id.emplace_back(file_id, compo_id);
                      }
                  }
              }

              return success();
          });
        ret.has_error())
        return ret.error();

    files.write([&](auto& fs) {
        for (const auto& m : file_id_to_compo_id) {
            if (auto* f = fs.file_paths.try_to_get(m.f_id)) {
                switch (f->type) {
                case file_path::file_type::irt_file:
                    f->component = m.to.component;
                    break;

                case file_path::file_type::project_file:
                    f->pj_id = m.to.project;
                    break;

                case file_path::file_type::dot_file:
                    f->g_id = m.to.graph;
                    break;

                default:
                    break;
                }
            }
        }
    });

    bool have_unread_component = components.size() > 0u;

    debug_log("{} graphs to load\n", graphs.size());
    for (auto& g : graphs) {
        const auto file_id = g.file;

        files.read([&](const auto& fs, const auto /*vers*/) {
            if (const auto file = fs.get_fs_path(file_id)) {
                if (auto ret = parse_dot_file(*this, *file); ret.has_value()) {
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
        });
    }

    auto& compos = components.get<component>();
    while (have_unread_component) {
        have_unread_component = false;

        for (auto id : components) {
            auto& compo = compos[id];
            if (compo.state == component_status::unmodified)
                continue;

            files.read([&](const auto& fs, const auto /*vers*/) {
                if (const auto file = fs.get_fs_path(compo.file)) {
                    if (auto ret = load_component(*file, compo); !ret) {
                        if (compo.state == component_status::unread) {
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
                        }

                        if (compo.state == component_status::unreadable) {
                            journal.push(
                              log_level::warning,
                              [&](auto& t, auto& m) noexcept {
                                  t = "Modeling initialization error";
                                  format(m,
                                         "Fail to read component `{}'",
                                         compo.name.sv());
                              });
                        }
                    }
                }
            });
        }
    }

    return success();
}

status modeling::load_component(const std::filesystem::path& filename,
                                component&                   compo) noexcept
{
    try {
        auto f = file::open(filename, file_mode{ file_open_options::read });

        if (f.has_value()) {
            json_dearchiver j;
            if (not j(*this, compo, filename.string(), *f)) {
                compo.state = component_status::unreadable;
                return error_code(modeling_errc::component_load_error);
            }

            compo.state = component_status::unmodified;

            auto descfilename = filename;
            descfilename.replace_extension(".desc");
            auto ec = std::error_code{};

            if (std::filesystem::exists(descfilename, ec) and not ec) {
                auto d = file::open(descfilename,
                                    file_mode{ file_open_options::read });

                if (d.has_value()) {
                    if (not descriptions.exists(compo.desc))
                        if (descriptions.can_alloc(1))
                            compo.desc = descriptions.alloc_id();
                    descriptions.get<description_str>(compo.desc).clear();
                    descriptions.get<description_status>(compo.desc) =
                      description_status::modified;

                    if (descriptions.exists(compo.desc)) {
                        auto& str = descriptions.get<0>(compo.desc);
                        auto view = std::span<char>(str.data(), str.capacity());

                        const auto newview = d->read_entire_file(view);
                        str.resize(newview.size());
                    }
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

void modeling::file_access::remove(const file_path_id id) noexcept
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

void modeling::file_access::refresh(const dir_path_id id) noexcept
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
                            const auto type = detect_file_type(it->path());
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

auto modeling::file_access::find_file_in_directory(
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

auto modeling::file_access::find_directory(std::string_view name) const noexcept
  -> dir_path_id
{
    for (const auto r_id : component_repertories)
        if (const auto* reg = registred_paths.try_to_get(r_id))
            for (const auto d_id : reg->children)
                if (const auto* dir = dir_paths.try_to_get(d_id))
                    if (dir->path.sv() == name)
                        return d_id;

    return undefined<dir_path_id>();
}

auto modeling::file_access::find_registred_path_by_name(
  const std::string_view name) const noexcept -> registred_path_id
{
    for (const auto r_id : component_repertories)
        if (const auto* reg = registred_paths.try_to_get(r_id))
            if (reg->name.sv() == name)
                return r_id;

    return undefined<registred_path_id>();
}

auto modeling::file_access::find_directory_in_registry(
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

component_id modeling::search_component_by_name(
  std::string_view reg,
  std::string_view dir,
  std::string_view file) const noexcept
{
    const auto file_id = files.read([&](const auto& fs, const auto /*vers*/) {
        const auto r_id = fs.find_registred_path_by_name(reg);
        const auto d_id = is_defined(r_id)
                            ? fs.find_directory_in_registry(r_id, dir)
                            : fs.find_directory(dir);
        const auto f_id = is_defined(d_id)
                            ? fs.find_file_in_directory(d_id, file)
                            : undefined<file_path_id>();

        return f_id;
    });

    return files.read([&](const auto& fs, const auto /*vers*/) {
        if (const auto* f = fs.file_paths.try_to_get(file_id))
            if (auto* c = components.try_to_get<component>(f->component))
                return components.get_id(*c);

        return undefined<component_id>();
    });
}

file_path_id modeling::file_access::alloc_file(
  const dir_path_id          id,
  const std::string_view     name,
  const file_path::file_type type,
  const component_id         compo_id,
  const project_id           project_id) noexcept
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

dir_path_id modeling::file_access::alloc_dir(
  const registred_path_id id,
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

registred_path_id modeling::file_access::alloc_registred(
  const std::string_view name,
  const int              priority) noexcept
{
    if (registred_paths.can_alloc(1) or registred_paths.grow<3, 2>()) {
        auto& reg    = registred_paths.alloc();
        auto  reg_id = registred_paths.get_id(reg);

        reg.name = name;
        reg.priority =
          static_cast<i8>(std::clamp<int>(priority, INT8_MIN, INT8_MAX));
        component_repertories.emplace_back(reg_id);

        auto to_erase =
          std::ranges::remove_if(component_repertories, [&](const auto id) {
              const auto* r = registred_paths.try_to_get(id);
              return r == nullptr;
          });

        if (to_erase.begin() != to_erase.end())
            component_repertories.erase(to_erase.begin(), to_erase.end());

        std::ranges::sort(component_repertories,
                          [&](const auto a, const auto b) {
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

bool modeling::file_access::exists(const registred_path_id id) const noexcept
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

bool modeling::file_access::exists(const dir_path_id id) const noexcept
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

bool modeling::file_access::create_directories(
  const registred_path_id id) const noexcept
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

bool modeling::file_access::create_directories(
  const dir_path_id id) const noexcept
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

void modeling::file_access::remove_files(const dir_path_id id) noexcept
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

void modeling::file_access::remove_file(const file_path_id id) noexcept
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

void modeling::file_access::move_file(const dir_path_id  to,
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

void modeling::file_access::free(const file_path_id file) noexcept
{
    file_paths.free(file);
}

void modeling::file_access::free(const dir_path_id dir) noexcept
{
    if (auto* d = dir_paths.try_to_get(dir))
        for (const auto f_id : d->children)
            if (auto* f = file_paths.try_to_get(f_id))
                f->parent = undefined<dir_path_id>();

    dir_paths.free(dir);
}

void modeling::file_access::free(const registred_path_id reg_dir) noexcept
{
    if (auto* r = registred_paths.try_to_get(reg_dir))
        for (const auto d_id : r->children)
            free(d_id);

    registred_paths.free(reg_dir);
}

expected<std::filesystem::path> modeling::file_access::get_fs_path(
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

expected<std::filesystem::path> modeling::file_access::get_fs_path(
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

expected<std::filesystem::path> modeling::file_access::get_fs_path(
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

bool modeling::can_alloc_grid_component() const noexcept
{
    return components.can_alloc(1) && grid_components.can_alloc();
}

bool modeling::can_alloc_graph_component() const noexcept
{
    return components.can_alloc(1) && graph_components.can_alloc();
}

bool modeling::can_alloc_generic_component() const noexcept
{
    return components.can_alloc(1) && generic_components.can_alloc();
}

bool modeling::can_alloc_hsm_component() const noexcept
{
    return components.can_alloc(1) && hsm_components.can_alloc();
}

bool modeling::can_alloc_sim_component() const noexcept
{
    return components.can_alloc(1) && sim_components.can_alloc();
}

component& modeling::alloc_grid_component() noexcept
{
    debug::ensure(can_alloc_grid_component());

    auto new_compo_id = components.alloc_id();

    components.get<component_color>(new_compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& new_compo = components.get<component>(new_compo_id);
    format(new_compo.name, "grid {}", get_index(new_compo_id));
    new_compo.type  = component_type::grid;
    new_compo.state = component_status::modified;

    auto& grid = grid_components.alloc();
    grid.resize(4, 4, undefined<component_id>());
    new_compo.id.grid_id = grid_components.get_id(grid);

    return new_compo;
}

component& modeling::alloc_graph_component() noexcept
{
    debug::ensure(can_alloc_graph_component());

    auto new_compo_id = components.alloc_id();

    components.get<component_color>(new_compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& new_compo = components.get<component>(new_compo_id);
    format(new_compo.name, "graph {}", get_index(new_compo_id));
    new_compo.type  = component_type::graph;
    new_compo.state = component_status::modified;

    auto& graph           = graph_components.alloc();
    new_compo.id.graph_id = graph_components.get_id(graph);

    return new_compo;
}

component& modeling::alloc_hsm_component() noexcept
{
    debug::ensure(can_alloc_hsm_component());

    auto new_compo_id = components.alloc_id();

    components.get<component_color>(new_compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& new_compo = components.get<component>(new_compo_id);
    format(new_compo.name, "hsm {}", get_index(new_compo_id));
    new_compo.type  = component_type::hsm;
    new_compo.state = component_status::modified;

    auto& h             = hsm_components.alloc();
    new_compo.id.hsm_id = hsm_components.get_id(h);
    h.clear();
    h.machine.states[0].super_id = hierarchical_state_machine::invalid_state_id;
    h.machine.top_state          = 0;

    return new_compo;
}

component& modeling::alloc_sim_component() noexcept
{
    debug::ensure(can_alloc_sim_component());

    auto new_compo_id = components.alloc_id();

    components.get<component_color>(new_compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& new_compo = components.get<component>(new_compo_id);
    format(new_compo.name, "sim {}", get_index(new_compo_id));
    new_compo.type  = component_type::simulation;
    new_compo.state = component_status::modified;

    auto& h             = sim_components.alloc();
    new_compo.id.sim_id = sim_components.get_id(h);

    return new_compo;
}

component& modeling::alloc_generic_component() noexcept
{
    debug::ensure(can_alloc_generic_component());

    auto new_compo_id = components.alloc_id();

    components.get<component_color>(new_compo_id) = { 1.f, 1.f, 1.f, 1.f };
    auto& new_compo = components.get<component>(new_compo_id);
    format(new_compo.name, "simple {}", get_index(new_compo_id));
    new_compo.type  = component_type::generic;
    new_compo.state = component_status::modified;

    auto& new_s_compo       = generic_components.alloc();
    new_compo.id.generic_id = generic_components.get_id(new_s_compo);

    return new_compo;
}

static bool can_add_component(const modeling&       mod,
                              const component_id    c,
                              vector<component_id>& out,
                              component_id          search) noexcept
{
    if (c == search)
        return false;

    if (auto* compo = mod.components.try_to_get<component>(c); compo)
        out.emplace_back(c);

    return true;
}

static bool can_add_component(const modeling&       mod,
                              const component&      compo,
                              vector<component_id>& out,
                              component_id          search) noexcept
{
    switch (compo.type) {
    case component_type::grid: {
        auto id = compo.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(id); g) {
            for (const auto ch : g->children())
                if (not can_add_component(mod, ch, out, search))
                    return false;
        }
    } break;

    case component_type::graph: {
        auto id = compo.id.graph_id;
        if (auto* g = mod.graph_components.try_to_get(id); g) {
            for (const auto edge_id : g->g.nodes)
                if (not can_add_component(
                      mod, g->g.node_components[edge_id], out, search))
                    return false;
        }
    } break;

    case component_type::generic: {
        auto id = compo.id.generic_id;
        if (auto* s = mod.generic_components.try_to_get(id); s) {
            for (const auto& ch : s->children)
                if (ch.type == child_type::component)
                    if (not can_add_component(mod, ch.id.compo_id, out, search))
                        return false;
        }
        break;
    }

    default:
        break;
    }

    return true;
}

bool modeling::can_add(const component& parent,
                       const component& other) const noexcept
{
    vector<component_id> stack;
    component_id         parent_id = components.get_id(parent);
    component_id         other_id  = components.get_id(other);

    if (parent_id == other_id)
        return false;

    if (not can_add_component(*this, other, stack, parent_id))
        return false;

    while (!stack.empty()) {
        auto id = stack.back();
        stack.pop_back();

        if (auto* compo = components.try_to_get<component>(id); compo) {
            if (not can_add_component(*this, *compo, stack, parent_id))
                return false;
        }
    }

    return true;
}

void modeling::clear(component& compo) noexcept
{
    switch (compo.type) {
    case component_type::none:
        break;
    case component_type::generic:
        if_data_exists_do(generic_components,
                          compo.id.generic_id,
                          [&](auto& gen) { free(gen); });
        compo.id.generic_id = undefined<generic_component_id>();
        break;
    case component_type::grid:
        if_data_exists_do(
          grid_components, compo.id.grid_id, [&](auto& gen) { free(gen); });
        compo.id.grid_id = undefined<grid_component_id>();
        break;
    case component_type::graph:
        if_data_exists_do(graph_components,
                          compo.id.graph_id,
                          [&](auto& graph) { free(graph); });
        compo.id.graph_id = undefined<graph_component_id>();
        break;
    case component_type::hsm:
        if_data_exists_do(
          hsm_components, compo.id.hsm_id, [&](auto& h) { free(h); });
        compo.id.hsm_id = undefined<hsm_component_id>();
        break;

    case component_type::simulation:
        if (auto* h = sim_components.try_to_get(compo.id.sim_id)) {
            sim_components.free(*h);
            compo.id.sim_id = undefined<simulation_component_id>();
        }
        break;
    }
}

void modeling::free(generic_component& gen) noexcept
{
    generic_components.free(gen);
}

void modeling::free(grid_component& gen) noexcept { grid_components.free(gen); }

void modeling::free(hsm_component& c) noexcept { hsm_components.free(c); }

void modeling::free(graph_component& gen) noexcept
{
    graph_components.free(gen);
}

void modeling::free(component& compo) noexcept
{
    switch (compo.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if_data_exists_do(
          generic_components, compo.id.generic_id, [&](auto& g) { free(g); });
        break;

    case component_type::grid:
        if_data_exists_do(
          grid_components, compo.id.grid_id, [&](auto& g) { free(g); });
        break;

    case component_type::graph:
        if_data_exists_do(
          graph_components, compo.id.graph_id, [&](auto& g) { free(g); });
        break;

    case component_type::hsm:
        if_data_exists_do(
          hsm_components, compo.id.hsm_id, [&](auto& h) { free(h); });
        break;

    case component_type::simulation:
        if (auto* s = sim_components.try_to_get(compo.id.sim_id))
            sim_components.free(*s);
        break;
    }

    if (descriptions.exists(compo.desc))
        descriptions.free(compo.desc);

    files.write([&](auto& fs) noexcept {
        if (auto* path = fs.file_paths.try_to_get(compo.file); path)
            fs.file_paths.free(*path);
    });

    components.free(components.get_id(compo));
}

generic_component::child& modeling::alloc(generic_component& parent,
                                          component_id       id) noexcept
{
    debug::ensure(parent.children.can_alloc());

    auto&      child    = parent.children.alloc(id);
    const auto child_id = parent.children.get_id(child);
    const auto index    = get_index(child_id);
    child.type          = child_type::component;
    child.id.compo_id   = id;

    parent.children_names[index] = parent.make_unique_name_id(child_id);
    parent.children_parameters[index].clear();
    parent.children_positions[index].x = 0.f;
    parent.children_positions[index].y = 0.f;

    return child;
}

generic_component::child& modeling::alloc(generic_component& parent,
                                          dynamics_type      type) noexcept
{
    debug::ensure(parent.children.can_alloc());

    auto&      child  = parent.children.alloc(type);
    const auto id     = parent.children.get_id(child);
    const auto index  = get_index(id);
    child.type        = child_type::model;
    child.id.mdl_type = type;

    debug::ensure(index < parent.children_names.size());
    debug::ensure(index < parent.children_parameters.size());
    debug::ensure(index < parent.children_positions.size());
    debug::ensure(index < parent.children_positions.size());

    parent.children_names[index] = parent.make_unique_name_id(id);
    parent.children_parameters[index].clear();
    parent.children_positions[index].x = 0.f;
    parent.children_positions[index].y = 0.f;

    return child;
}

status modeling::copy(const component& src, component& dst) noexcept
{
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
    dst.id    = src.id;
    dst.type  = src.type;
    dst.state = src.state;

    switch (src.type) {
    case component_type::none:
        break;

    case component_type::generic:
        if (const auto* s_src =
              generic_components.try_to_get(src.id.generic_id);
            s_src) {
            if (!generic_components.can_alloc())
                return new_error(
                  modeling_errc::generic_children_container_full);

            auto& s_dst       = generic_components.alloc();
            auto  s_dst_id    = generic_components.get_id(s_dst);
            dst.id.generic_id = s_dst_id;
            dst.type          = component_type::generic;

            if (auto ret = copy(*s_src, s_dst); !ret)
                return ret.error();
        }
        break;

    case component_type::grid:
        if (const auto* s = grid_components.try_to_get(src.id.grid_id); s) {
            if (!grid_components.can_alloc())
                return new_error(modeling_errc::grid_children_container_full);

            auto& d        = grid_components.alloc(*s);
            auto  d_id     = grid_components.get_id(d);
            dst.id.grid_id = d_id;
            dst.type       = component_type::grid;
        }
        break;

    case component_type::graph:
        if (const auto* s = graph_components.try_to_get(src.id.graph_id); s) {
            if (!graph_components.can_alloc())
                return new_error(modeling_errc::graph_children_container_full);

            auto& d         = graph_components.alloc(*s);
            auto  d_id      = graph_components.get_id(d);
            dst.id.graph_id = d_id;
            dst.type        = component_type::graph;
        }
        break;

    case component_type::hsm:
        if (const auto* s = hsm_components.try_to_get(src.id.hsm_id); s) {
            if (!hsm_components.can_alloc())
                return new_error(modeling_errc::hsm_children_container_full);

            auto& d       = hsm_components.alloc(*s);
            auto  d_id    = hsm_components.get_id(d);
            dst.id.hsm_id = d_id;
            dst.type      = component_type::hsm;
        }
        break;

    case component_type::simulation:
        if (auto* s = sim_components.try_to_get(src.id.sim_id)) {
            if (not sim_components.can_alloc())
                return new_error(modeling_errc::simulation_container_full);

            auto& d       = sim_components.alloc(*s);
            auto  d_id    = sim_components.get_id(d);
            dst.id.sim_id = d_id;
            dst.type      = component_type::simulation;
        }
        break;
    }

    return success();
}

status modeling::save(component& c) noexcept
{
    const auto filenames = make_component_files(*this, c.file);

    if (not filenames.has_value())
        return new_error(modeling_errc::file_error);

    auto cfile =
      file::open(filenames->component, file_mode{ file_open_options::write });
    if (cfile.has_error()) [[unlikely]]
        return cfile.error();

    json_archiver j;
    if (auto ret = j(*this, c, *cfile); ret.has_error())
        return ret.error();

    if (descriptions.exists(c.desc)) {
        auto dfile = file::open(filenames->description,
                                file_mode{ file_open_options::write });

        if (dfile.has_value()) [[likely]] {
            auto& str = descriptions.get<0>()[c.desc];

            dfile->write(str.sv());
        }
    }

    c.state = component_status::unmodified;

    return success();
}

} // namespace irt
