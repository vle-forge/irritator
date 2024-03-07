// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <utility>

#include <cstdint>

namespace irt {

static auto get_x_or_y_index(const modeling&        mod,
                             const vector<port_id>& ports,
                             std::string_view       name) noexcept -> port_id
{
    for (int i = 0, e = ports.ssize(); i != e; ++i)
        if (auto* port = mod.ports.try_to_get(ports[i]);
            port && name == port->name.sv())
            return mod.ports.get_id(*port);

    return undefined<port_id>();
}

auto modeling::get_x_index(const component& compo,
                           std::string_view name) const noexcept -> port_id
{
    return get_x_or_y_index(*this, compo.x_names, name);
}

auto modeling::get_y_index(const component& compo,
                           std::string_view name) const noexcept -> port_id
{
    return get_x_or_y_index(*this, compo.y_names, name);
}

auto modeling::get_or_add_x_index(component&       compo,
                                  std::string_view name) noexcept -> port_id
{
    auto ret = get_x_or_y_index(*this, compo.x_names, name);

    if (is_defined(ret))
        return ret;

    irt_assert(ports.can_alloc());

    auto& new_p    = ports.alloc(name, components.get_id(compo));
    auto  new_p_id = ports.get_id(new_p);
    compo.x_names.emplace_back(new_p_id);

    return new_p_id;
}

auto modeling::get_or_add_y_index(component&       compo,
                                  std::string_view name) noexcept -> port_id
{
    auto ret = get_x_or_y_index(*this, compo.y_names, name);

    if (is_defined(ret))
        return ret;

    irt_assert(ports.can_alloc());

    auto& new_p    = ports.alloc(name, components.get_id(compo));
    auto  new_p_id = ports.get_id(new_p);
    compo.y_names.emplace_back(new_p_id);

    return new_p_id;
}

modeling::modeling() noexcept
  : log_entries{ 16 }
{}

status modeling::init(modeling_initializer& p) noexcept
{
    descriptions.reserve(p.description_capacity);
    if (not descriptions.can_alloc())
        return new_error(modeling::part::descriptions);

    ports.reserve(p.model_capacity);
    if (not ports.can_alloc())
        return new_error(modeling::part::ports);

    components.reserve(p.component_capacity);
    if (not components.can_alloc())
        return new_error(modeling::part::components);

    grid_components.reserve(p.component_capacity);
    if (not grid_components.can_alloc())
        return new_error(modeling::part::grid_components);

    graph_components.reserve(p.component_capacity);
    if (not graph_components.can_alloc())
        return new_error(modeling::part::graph_components);

    generic_components.reserve(p.component_capacity);
    if (not generic_components.can_alloc())
        return new_error(modeling::part::generic_components);

    hsm_components.reserve(p.component_capacity);
    if (not hsm_components.can_alloc())
        return new_error(modeling::part::hsm_components);

    dir_paths.reserve(p.dir_path_capacity);
    if (not dir_paths.can_alloc())
        return new_error(modeling::part::dir_paths);

    file_paths.reserve(p.file_path_capacity);
    if (not file_paths.can_alloc())
        return new_error(modeling::part::file_paths);

    registred_paths.reserve(p.dir_path_capacity);
    if (not registred_paths.can_alloc())
        return new_error(modeling::part::registred_paths);

    hsms.reserve(p.component_capacity);
    if (not hsms.can_alloc())
        return new_error(modeling::part::hsms);

    children.reserve(p.children_capacity);
    if (not children.can_alloc())
        return new_error(modeling::part::children);

    connections.reserve(p.connection_capacity);
    if (not connections.can_alloc())
        return new_error(modeling::part::connections);

    children_positions.resize(children.capacity());
    children_names.resize(children.capacity());
    children_parameters.resize(children.capacity());
    component_colors.resize(components.capacity());

    return success();
}

static void prepare_component_loading(modeling&             mod,
                                      registred_path&       reg_dir,
                                      dir_path&             dir,
                                      file_path&            file,
                                      std::filesystem::path path) noexcept
{
    namespace fs = std::filesystem;

    auto& compo    = mod.components.alloc();
    compo.reg_path = mod.registred_paths.get_id(reg_dir);
    compo.dir      = mod.dir_paths.get_id(dir);
    compo.file     = mod.file_paths.get_id(file);
    file.component = mod.components.get_id(compo);
    compo.type     = component_type::none;
    compo.state    = component_status::unread;

    try {
        std::filesystem::path desc_file{ path };
        desc_file.replace_extension(".desc");

        std::error_code ec;
        if (fs::exists(desc_file, ec)) {
            debug_logi(6, "found description file {}\n", desc_file.string());
            if (mod.descriptions.can_alloc()) {
                auto& desc  = mod.descriptions.alloc();
                desc.status = description_status::unread;
                compo.desc  = mod.descriptions.get_id(desc);
            } else {
                log_warning(mod, log_level::error);
            }
        }
    } catch (const std::exception& /*e*/) {
        log_warning(mod, log_level::error, reg_dir.path.c_str());
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

    return file_path::file_type::undefined_file;
}

static auto push_back_if_not_exists(modeling&                    mod,
                                    dir_path&                    dir,
                                    const std::filesystem::path& file,
                                    file_path::file_type         type) noexcept
  -> std::optional<file_path_id>
{
    const auto  u8str = file.u8string();
    const auto* cstr  = reinterpret_cast<const char*>(u8str.c_str());

    int i = 0;
    while (i < dir.children.ssize()) {
        if (auto* f = mod.file_paths.try_to_get(dir.children[i]); f) {
            if (f->path.sv() == cstr)
                return std::nullopt;

            ++i;
        } else {
            dir.children.swap_pop_back(i);
        }
    }

    if (mod.file_paths.can_alloc()) {
        auto& fp    = mod.file_paths.alloc();
        auto  fp_id = mod.file_paths.get_id(fp);
        fp.path     = cstr;
        fp.parent   = mod.dir_paths.get_id(dir);
        fp.type     = type;
        return fp_id;
    } else {
        dir.flags.set(dir_path::dir_flags::too_many_file);
        return std::nullopt;
    }
}

auto dir_path::refresh(modeling& mod) noexcept -> vector<file_path_id>
{
    namespace fs = std::filesystem;

    vector<file_path_id> ret;
    std::error_code      ec;

    if (auto* reg = mod.registred_paths.try_to_get(parent); reg) {
        try {
            fs::path dir{ reg->path.sv() };
            dir /= path.sv();

            if (fs::is_directory(dir, ec)) {
                auto it = fs::directory_iterator{ dir, ec };
                auto et = fs::directory_iterator{};

                while (it != et) {
                    if (it->is_regular_file()) {
                        const auto type = detect_file_type(it->path());
                        const auto is_irt_or_dot =
                          any_equal(type,
                                    file_path::file_type::irt_file,
                                    file_path::file_type::dot_file);

                        if (is_irt_or_dot) {
                            const auto id_opt = push_back_if_not_exists(
                              mod, *this, it->path(), type);

                            if (id_opt)
                                ret.emplace_back(*id_opt);
                        }
                    }

                    it = it.increment(ec);
                }
            } else {
                flags.set(dir_path::dir_flags::access_error);
            }
        } catch (const std::exception& /*e*/) {
            flags.set(dir_path::dir_flags::access_error);
        }
    }

    return ret;
}

static void prepare_component_loading(modeling&             mod,
                                      registred_path&       reg_dir,
                                      dir_path&             dir,
                                      std::filesystem::path path) noexcept
{
    namespace fs = std::filesystem;

    try {
        std::error_code ec;

        auto it            = fs::directory_iterator{ path, ec };
        auto et            = fs::directory_iterator{};
        bool too_many_file = false;

        while (it != et) {
            if (it->is_regular_file()) {
                const auto type = detect_file_type(it->path());
                const auto is_irt_or_dot =
                  any_equal(type,
                            file_path::file_type::irt_file,
                            file_path::file_type::dot_file);

                if (is_irt_or_dot) {
                    debug_logi(6, "found file {}\n", it->path().string());

                    if (mod.file_paths.can_alloc() &&
                        mod.components.can_alloc()) {
                        auto  u8str = it->path().filename().u8string();
                        auto* cstr =
                          reinterpret_cast<const char*>(u8str.c_str());
                        auto& file    = mod.file_paths.alloc();
                        auto  file_id = mod.file_paths.get_id(file);
                        file.path     = cstr;
                        file.parent   = mod.dir_paths.get_id(dir);
                        file.type     = type;

                        dir.children.emplace_back(file_id);

                        if (type == file_path::file_type::irt_file)
                            prepare_component_loading(
                              mod, reg_dir, dir, file, it->path());
                    } else {
                        too_many_file = true;
                        break;
                    }
                }
            }

            it = it.increment(ec);
        }

        if (too_many_file) {
            log_warning(mod,
                        log_level::error,
                        "registred path {}, directory {}",
                        reg_dir.path.sv(),
                        dir.path.sv());
        }
    } catch (...) {
        log_warning(
          mod, log_level::error, "registred path {}", reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling&              mod,
                                      registred_path&        reg_dir,
                                      std::filesystem::path& path) noexcept
{
    namespace fs = std::filesystem;

    try {
        std::error_code ec;

        if (fs::is_directory(path, ec)) {
            auto it                 = fs::directory_iterator{ path, ec };
            auto et                 = fs::directory_iterator{};
            bool too_many_directory = false;

            while (it != et) {
                if (it->is_directory()) {
                    if (mod.dir_paths.can_alloc()) {
                        auto u8str = it->path().filename().u8string();
                        auto cstr =
                          reinterpret_cast<const char*>(u8str.c_str());
                        auto& dir    = mod.dir_paths.alloc();
                        auto  dir_id = mod.dir_paths.get_id(dir);
                        dir.path     = cstr;
                        dir.status   = dir_path::state::unread;
                        dir.parent   = mod.registred_paths.get_id(reg_dir);

                        debug_logi(4,
                                   "lookup in subdirectory {}\n",
                                   it->path().string());

                        reg_dir.children.emplace_back(dir_id);
                        prepare_component_loading(
                          mod, reg_dir, dir, it->path());
                    } else {
                        too_many_directory = true;
                        break;
                    }
                }

                it = it.increment(ec);
            }

            if (too_many_directory)
                log_warning(mod,
                            log_level::error,
                            "registred path {}",
                            reg_dir.path.sv());
        } else {
            log_warning(
              mod, log_level::error, "registred path {}", reg_dir.path.sv());
        }
    } catch (...) {
        log_warning(
          mod, log_level::error, "registred path {}", reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling&       mod,
                                      registred_path& reg_dir) noexcept
{
    namespace fs = std::filesystem;

    try {
        debug_logi(2, "lookup in registered path {}\n", reg_dir.path.sv());

        fs::path        p(reg_dir.path.c_str());
        std::error_code ec;

        if (std::filesystem::exists(p, ec)) {
            prepare_component_loading(mod, reg_dir, p);
            reg_dir.status = registred_path::state::read;
        } else {
            debug_logi(4, "registered path does not exists");
            reg_dir.status = registred_path::state::error;

            log_warning(mod,
                        log_level::debug,
                        "registred path does not exist {} ",
                        reg_dir.path.sv());
        }
    } catch (...) {
        log_warning(
          mod, log_level::debug, "registred path: {} ", reg_dir.path.sv());
    }
}

static void prepare_component_loading(modeling& mod) noexcept
{
    int i = 0;
    while (i < mod.component_repertories.ssize()) {
        const auto id      = mod.component_repertories[i];
        auto*      reg_dir = mod.registred_paths.try_to_get(id);

        if (reg_dir) {
            prepare_component_loading(mod, *reg_dir);
            ++i;
        } else {
            auto it = mod.component_repertories.begin() + i;
            mod.component_repertories.erase(it);
        }
    }
}

static status load_component(modeling& mod, component& compo) noexcept
{
    auto* reg  = mod.registred_paths.try_to_get(compo.reg_path);
    auto* dir  = mod.dir_paths.try_to_get(compo.dir);
    auto* file = mod.file_paths.try_to_get(compo.file);

    if (!(reg && dir && file))
        return new_error(modeling::part::components,
                         undefined_error{},
                         e_ulong_id{ ordinal(mod.components.get_id(compo)) });

    try {
        std::filesystem::path p{ reg->path.sv() };
        p /= dir->path.sv();
        p /= file->path.sv();

        std::string str{ p.string() };

        {
            auto f = file::make_file(str.c_str(), open_mode::read);
            if (!f) {
                compo.state = component_status::unreadable;
                return new_error(modeling::part::components,
                                 filesystem_error{},
                                 e_file_name{ str });
            }

            json_archiver j;
            cache_rw      cache; // TODO move into modeling or parameter

            if (auto ret = j.component_load(mod, compo, cache, *f); !ret) {
                compo.state = component_status::unreadable;
                return ret.error();
            }

            compo.state = component_status::unmodified;
        }

        p.replace_extension(".desc");
        str = p.string();

        if (file::exists(str.c_str())) {
            if (auto f = file::make_file(str.c_str(), open_mode::read); f) {
                auto* desc = mod.descriptions.try_to_get(compo.desc);
                if (!desc) {
                    auto& d    = mod.descriptions.alloc();
                    desc       = &d;
                    compo.desc = mod.descriptions.get_id(d);
                }

                if (!f->read(desc->data.data(), desc->data.capacity())) {
                    mod.descriptions.free(*desc);
                    compo.desc = undefined<description_id>();
                }
            }
        }
    } catch (const std::bad_alloc& /*e*/) {
        return new_error(filesystem_error{});
    } catch (...) {
        return new_error(filesystem_error{});
    }

    return success();
}

status modeling::fill_internal_components() noexcept
{
    if (!components.can_alloc(internal_component_count))
        return new_error(part::components, container_full_error{});

    for (int i = 0, e = internal_component_count; i < e; ++i) {
        auto& compo          = components.alloc();
        compo.type           = component_type::internal;
        compo.id.internal_id = enum_cast<internal_component>(i);
    }

    return success();
}

status modeling::fill_components() noexcept
{
    prepare_component_loading(*this);
    bool have_unread_component = components.size() > 0u;

    for_each_data(components, [&](auto& compo) -> void {
        debug_logi(2,
                   "Component type {} id {} name {} location: {} {} {}\n",
                   component_type_names[ordinal(compo.type)],
                   ordinal(components.get_id(compo)),
                   compo.name.sv(),
                   ordinal(compo.reg_path),
                   ordinal(compo.dir),
                   ordinal(compo.file));
    });

    while (have_unread_component) {
        have_unread_component = false;

        for_each_data(components, [&](auto& compo) {
            if (compo.type == component_type::internal)
                return;

            if (auto ret = load_component(*this, compo); !ret) {
                if (compo.state == component_status::unread) {
                    log_warning(*this,
                                log_level::warning,
                                "Need to read dependency for component {} - {}",
                                compo.name.sv(),
                                static_cast<u64>(components.get_id(compo)));

                    have_unread_component = true;
                }

                if (compo.state == component_status::unreadable) {
                    log_warning(*this,
                                log_level::warning,
                                "Fail to read component {} - {}",
                                compo.name.sv(),
                                static_cast<u64>(components.get_id(compo)));
                }
            }
        });
    }

    debug_log("{} components loaded\n", components.size());
    for_each_data(components, [&](auto& compo) noexcept {
        debug_logi(2,
                   "{} {} - ",
                   ordinal(components.get_id(compo)),
                   component_type_names[ordinal(compo.type)]);

        debug_component(*this, compo);
    });

    return success();
}

status modeling::fill_components(registred_path& path) noexcept
{
    for (auto dir_id : path.children)
        if (auto* dir = dir_paths.try_to_get(dir_id); dir)
            free(*dir);

    path.children.clear();

    try {
        std::filesystem::path p(path.path.c_str());
        std::error_code       ec;

        if (std::filesystem::exists(p, ec) &&
            std::filesystem::is_directory(p, ec)) {
            prepare_component_loading(*this, path, p);
        }
    } catch (...) {
    }

    return success();
}

bool modeling::can_alloc_file(i32 number) const noexcept
{
    return file_paths.can_alloc(number);
}

bool modeling::can_alloc_dir(i32 number) const noexcept
{
    return dir_paths.can_alloc(number);
}

bool modeling::can_alloc_registred(i32 number) const noexcept
{
    return registred_paths.can_alloc(number);
}

static bool exist_file(const dir_path& dir, const file_path_id id) noexcept
{
    return std::find(dir.children.begin(), dir.children.end(), id) !=
           dir.children.end();
}

file_path& modeling::alloc_file(dir_path& dir) noexcept
{
    auto& file     = file_paths.alloc();
    auto  id       = file_paths.get_id(file);
    file.component = undefined<component_id>();
    file.parent    = dir_paths.get_id(dir);

    dir.children.emplace_back(id);

    return file;
}

dir_path& modeling::alloc_dir(registred_path& reg) noexcept
{
    auto& dir  = dir_paths.alloc();
    auto  id   = dir_paths.get_id(dir);
    dir.parent = registred_paths.get_id(reg);
    dir.status = dir_path::state::unread;

    reg.children.emplace_back(id);

    return dir;
}

registred_path& modeling::alloc_registred(std::string_view name,
                                          int              priority) noexcept
{
    auto& reg    = registred_paths.alloc();
    auto  reg_id = registred_paths.get_id(reg);

    reg.name = name;
    reg.priority =
      static_cast<i8>(std::clamp<int>(priority, INT8_MIN, INT8_MAX));
    component_repertories.emplace_back(reg_id);

    return reg;
}

bool modeling::exists(const registred_path& reg) noexcept
{
    try {
        std::error_code       ec;
        std::filesystem::path p{ reg.path.sv() };
        return std::filesystem::exists(p, ec);
    } catch (...) {
        return false;
    }
}

bool modeling::create_directories(const registred_path& reg) noexcept
{
    try {
        std::error_code       ec;
        std::filesystem::path p{ reg.path.sv() };
        if (!std::filesystem::exists(p, ec))
            return std::filesystem::create_directories(p, ec);
    } catch (...) {
    }

    return false;
}

bool modeling::exists(const dir_path& dir) noexcept
{
    if (auto* reg = registred_paths.try_to_get(dir.parent); reg) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            if (std::filesystem::exists(p, ec)) {
                p /= dir.path.sv();
                if (!std::filesystem::exists(p, ec))
                    return true;
            }
        } catch (...) {
        }
    }

    return false;
}

bool modeling::create_directories(const dir_path& dir) noexcept
{
    if (auto* reg = registred_paths.try_to_get(dir.parent); reg) {
        try {
            std::error_code       ec;
            std::filesystem::path p{ reg->path.sv() };
            if (!std::filesystem::exists(p, ec))
                if (!std::filesystem::create_directories(p, ec))
                    return false;

            p /= dir.path.sv();
            if (!std::filesystem::exists(p, ec))
                return std::filesystem::create_directories(p, ec);
        } catch (...) {
        }
    }

    return false;
}

void modeling::remove_file(registred_path& reg,
                           dir_path&       dir,
                           file_path&      file) noexcept
{
    try {
        std::filesystem::path p{ reg.path.sv() };
        p /= dir.path.sv();
        p /= file.path.sv();

        std::error_code ec;
        std::filesystem::remove(p, ec);
    } catch (...) {
    }
}

void modeling::move_file(registred_path& /*reg*/,
                         dir_path&  from,
                         dir_path&  to,
                         file_path& file) noexcept
{
    auto id = file_paths.get_id(file);

    if (auto it = std::find(from.children.begin(), from.children.end(), id);
        it != from.children.end())
        from.children.erase(it);

    irt_assert(!exist_file(to, id));

    file.parent = dir_paths.get_id(to);
    to.children.emplace_back(id);
}

bool modeling::can_alloc_grid_component() const noexcept
{
    return components.can_alloc() && grid_components.can_alloc();
}

bool modeling::can_alloc_graph_component() const noexcept
{
    return components.can_alloc() && graph_components.can_alloc();
}

bool modeling::can_alloc_generic_component() const noexcept
{
    return components.can_alloc() && generic_components.can_alloc();
}

component& modeling::alloc_grid_component() noexcept
{
    irt_assert(can_alloc_grid_component());

    auto& new_compo    = components.alloc();
    auto  new_compo_id = components.get_id(new_compo);
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
    irt_assert(can_alloc_graph_component());

    auto& new_compo    = components.alloc();
    auto  new_compo_id = components.get_id(new_compo);
    format(new_compo.name, "graph {}", get_index(new_compo_id));
    new_compo.type  = component_type::graph;
    new_compo.state = component_status::modified;

    auto& graph           = graph_components.alloc();
    new_compo.id.graph_id = graph_components.get_id(graph);

    return new_compo;
}

component& modeling::alloc_generic_component() noexcept
{
    irt_assert(can_alloc_generic_component());

    auto& new_compo    = components.alloc();
    auto  new_compo_id = components.get_id(new_compo);
    format(new_compo.name, "simple {}", get_index(new_compo_id));
    new_compo.type  = component_type::simple;
    new_compo.state = component_status::modified;

    auto& new_s_compo       = generic_components.alloc();
    new_compo.id.generic_id = generic_components.get_id(new_s_compo);

    return new_compo;
}

static bool fill_child(const modeling&       mod,
                       const child&          c,
                       vector<component_id>& out,
                       component_id          search) noexcept
{
    if (c.type == child_type::component) {
        if (c.id.compo_id == search)
            return true;

        if (auto* compo = mod.components.try_to_get(c.id.compo_id); compo)
            out.emplace_back(c.id.compo_id);
    }

    return false;
}

static bool fill_children(const modeling&       mod,
                          const component&      compo,
                          vector<component_id>& out,
                          component_id          search) noexcept
{
    switch (compo.type) {
    case component_type::grid: {
        auto id = compo.id.grid_id;
        if (auto* g = mod.grid_components.try_to_get(id); g) {
            for (const auto& ch : g->children)
                if (fill_child(mod, ch, out, search))
                    return true;
        }
    } break;

    case component_type::graph: {
        auto id = compo.id.graph_id;
        if (auto* g = mod.graph_components.try_to_get(id); g) {
            for (const auto& vertex : g->children)
                if (fill_child(mod, vertex.id, out, search))
                    return true;
        }
    } break;

    case component_type::simple: {
        auto id = compo.id.generic_id;
        if (auto* s = mod.generic_components.try_to_get(id); s) {
            for (auto id : s->children)
                if (auto* ch = mod.children.try_to_get(id); ch)
                    if (fill_child(mod, *ch, out, search))
                        return true;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

bool modeling::can_add(const component& parent,
                       const component& child) const noexcept
{
    vector<component_id> stack;
    component_id         parent_id = components.get_id(parent);
    component_id         child_id  = components.get_id(child);

    if (parent_id == child_id)
        return false;

    if (fill_children(*this, child, stack, parent_id))
        return true;

    while (!stack.empty()) {
        auto id = stack.back();
        stack.pop_back();

        if (auto* compo = components.try_to_get(id); compo) {
            if (fill_children(*this, *compo, stack, parent_id))
                return true;
        }
    }

    return false;
}

void modeling::clean_simulation() noexcept
{
    grid_component* grid = nullptr;
    while (grid_components.next(grid))
        clear_grid_component_cache(*grid);
}

void modeling::clear(child& c) noexcept
{
    c.id.mdl_type = dynamics_type::constant;
    c.type        = child_type::model;
}

void modeling::clear(component& compo) noexcept
{
    for_specified_data(
      ports, compo.x_names, [&](auto& port) { ports.free(port); });
    for_specified_data(
      ports, compo.y_names, [&](auto& port) { ports.free(port); });

    switch (compo.type) {
    case component_type::none:
        break;
    case component_type::internal:
        break;
    case component_type::simple:
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
    }
}

void modeling::free_input_port(component& compo, port& p) noexcept
{
    const auto compo_id = components.get_id(compo);
    const auto port_id  = ports.get_id(p);

    if (compo.type == component_type::simple) {
        auto* gen = generic_components.try_to_get(compo.id.generic_id);
        irt_assert(gen);

        remove_specified_data_if(
          connections, gen->connections, [&](auto& con) -> bool {
              return con.type == connection::connection_type::input &&
                     con.input.index == port_id;
          });
    }

    remove_data_if(connections, [&](auto& con) -> bool {
        if (con.type == connection::connection_type::internal) {
            auto* ch = children.try_to_get(con.internal.dst);

            if (ch)
                return ch->type == child_type::component &&
                       ch->id.compo_id == compo_id;
            else
                return true;
        }
        return false;
    });

    ports.free(p);
}

void modeling::free_output_port(component& compo, port& p) noexcept
{
    const auto compo_id = components.get_id(compo);
    const auto port_id  = ports.get_id(p);

    if (compo.type == component_type::simple) {
        auto* gen = generic_components.try_to_get(compo.id.generic_id);
        irt_assert(gen);

        remove_specified_data_if(
          connections, gen->connections, [&](auto& con) -> bool {
              return con.type == connection::connection_type::output &&
                     con.output.index == port_id;
          });
    }

    remove_data_if(connections, [&](auto& con) -> bool {
        if (con.type == connection::connection_type::internal) {
            auto* ch = children.try_to_get(con.internal.src);

            if (ch)
                return ch->type == child_type::component &&
                       ch->id.compo_id == compo_id;
            else
                return true;
        }
        return false;
    });

    ports.free(p);
}

void modeling::free(child& c) noexcept
{
    clear(c);

    children.free(c);
}

void modeling::free(connection& c) noexcept { connections.free(c); }

void modeling::free(generic_component& gen) noexcept
{
    for (auto child_id : gen.children)
        if (auto* child = children.try_to_get(child_id); child)
            free(*child);

    for (auto connection_id : gen.connections)
        if (auto* con = connections.try_to_get(connection_id); con)
            free(*con);

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
    case component_type::simple:
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

    default:
        break;
    }

    if (auto* desc = descriptions.try_to_get(compo.desc); desc)
        descriptions.free(*desc);

    if (auto* path = file_paths.try_to_get(compo.file); path)
        file_paths.free(*path);

    components.free(compo);
}

child& modeling::alloc(generic_component& parent, component_id id) noexcept
{
    irt_assert(children.can_alloc());

    auto& child       = children.alloc(id);
    auto  child_id    = children.get_id(child);
    child.unique_id   = parent.make_next_unique_id();
    child.type        = child_type::component;
    child.id.compo_id = id;
    parent.children.emplace_back(child_id);

    return child;
}

child& modeling::alloc(generic_component& parent, dynamics_type type) noexcept
{
    irt_assert(children.can_alloc());

    auto&      child = children.alloc(type);
    const auto id    = children.get_id(child);
    const auto index = get_index(id);

    child.unique_id = parent.make_next_unique_id();

    parent.children.emplace_back(id);

    children_names[index].clear();
    children_parameters[index].clear();
    children_positions[index].x = 0.f;
    children_positions[index].y = 0.f;

    return child;
}

status modeling::copy(const component& src, component& dst) noexcept
{
    dst.x_names = src.x_names;
    dst.y_names = src.y_names;
    dst.name    = src.name;
    dst.id      = src.id;
    dst.type    = src.type;
    dst.state   = src.state;

    switch (src.type) {
    case component_type::none:
        break;

    case component_type::simple:
        if (const auto* s_src =
              generic_components.try_to_get(src.id.generic_id);
            s_src) {
            if (!generic_components.can_alloc())
                return new_error(part::generic_components,
                                 container_full_error{});

            auto& s_dst       = generic_components.alloc();
            auto  s_dst_id    = generic_components.get_id(s_dst);
            dst.id.generic_id = s_dst_id;
            dst.type          = component_type::simple;

            if (auto ret = copy(*s_src, s_dst); !ret)
                return ret.error();
        }
        break;

    case component_type::grid:
        if (const auto* s = grid_components.try_to_get(src.id.grid_id); s) {
            if (!generic_components.can_alloc())
                return new_error(part::grid_components, container_full_error{});

            auto& d        = grid_components.alloc();
            auto  d_id     = grid_components.get_id(d);
            dst.id.grid_id = d_id;
            dst.type       = component_type::grid;
            d              = *s;
        }
        break;

    case component_type::graph:
        if (const auto* s = graph_components.try_to_get(src.id.graph_id); s) {
            if (!generic_components.can_alloc())
                return new_error(part::graph_components,
                                 container_full_error{});

            auto& d         = graph_components.alloc();
            auto  d_id      = graph_components.get_id(d);
            dst.id.graph_id = d_id;
            dst.type        = component_type::graph;
            d               = *s;
        }
        break;

    case component_type::hsm:
        if (const auto* s = hsm_components.try_to_get(src.id.hsm_id); s) {
            if (!generic_components.can_alloc())
                return new_error(part::hsms, container_full_error{});

            auto& d       = hsm_components.alloc(*s);
            auto  d_id    = hsm_components.get_id(d);
            dst.id.hsm_id = d_id;
            dst.type      = component_type::hsm;
        }
        break;

    case component_type::internal:
        break;
    }

    return success();
}

void modeling::free(file_path& file) noexcept
{
    if (auto* compo = components.try_to_get(file.component); compo)
        free(*compo);
}

void modeling::free(dir_path& dir) noexcept
{
    for (auto file_id : dir.children)
        if (auto* file = file_paths.try_to_get(file_id); file)
            free(*file);

    dir_paths.free(dir);
}

void modeling::free(registred_path& reg_dir) noexcept
{
    for (auto dir_id : reg_dir.children)
        if (auto* dir = dir_paths.try_to_get(dir_id); dir)
            free(*dir);

    registred_paths.free(reg_dir);
}

status modeling::save(component& c) noexcept
{
    auto* reg  = registred_paths.try_to_get(c.reg_path);
    auto* dir  = dir_paths.try_to_get(c.dir);
    auto* file = file_paths.try_to_get(c.file);

    if (!(reg && dir && file))
        return new_error(part::components,
                         undefined_error{},
                         e_ulong_id{ ordinal(components.get_id(c)) });

    {
        std::filesystem::path p{ reg->path.sv() };
        p /= dir->path.sv();
        p /= file->path.sv();
        p.replace_extension(".irt");

        std::error_code ec;
        json_archiver   j;
        cache_rw        cache;

        if (auto ret = j.component_save(*this, c, cache, p.string().c_str());
            !ret)
            return ret.error();
    }

    if (auto* desc = descriptions.try_to_get(c.desc); desc) {
        std::filesystem::path p{ dir->path.c_str() };
        p /= file->path.c_str();
        p.replace_extension(".desc");
        std::ofstream ofs{ p };
        ofs.write(desc->data.c_str(), desc->data.size());
    }

    c.state = component_status::unmodified;

    return success();
}

} // namespace irt
