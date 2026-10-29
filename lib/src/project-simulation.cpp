// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/binary.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/helpers.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <rapidjson/document.h>

#include <bit>

namespace irt {

static_assert(std::endian::native == std::endian::little,
              "irritator binary format assumes little-endian; "
              "add byteswap support before targeting a big-endian platform");

/// The beginning of the @c .irb file. A reader refuses another version: the
/// version 3 stores raw samples.
struct irb_header {
    u32 magic   = binary_magic('I', 'R', 'T', 'B');
    u32 version = 3;
};

/// A chunk of the @c .irb file: @c n_rows raw samples of the model.
struct chunk_header {
    u64      n_rows;
    real     t_min;
    real     t_max;
    model_id mdl_id;
};

struct irtb_files {
    file json_file;
    file binary_file;
    path json_path;
    path binary_path;
};

static path open_dir_name(const modeling&         mod,
                          const registred_path_id reg_id) noexcept
{
    return mod.files.read([&](auto& fs, auto) noexcept -> path {
        if (auto* r = fs.registred_paths.try_to_get(reg_id))
            return r->path;

        auto        ec       = std::error_code{};
        auto        std_path = std::filesystem::current_path(ec);
        const auto  u8       = std_path.u8string();
        const auto* ptr      = reinterpret_cast<const char*>(u8.data());
        const auto  len      = u8.size();

        return path{ std::string_view(ptr, len) };
    });
}

static expected<irtb_files> open_irtb_file(
  const std::string_view simulation_name,
  const path&            dir_path,
  const u64              simulation_id,
  const u64              simulation_n) noexcept
{
    debug::ensure(all_char_valid(simulation_name));

    try {
        const auto jfilename = format_n<256>("{}-{}-{:04}.manifest.jsonl",
                                             simulation_name, simulation_id,
                                             simulation_n);
        const auto bfilename = format_n<256>("{}-{}-{:04}.irb", simulation_name,
                                             simulation_id, simulation_n);

        auto jfile = path{ dir_path };
        auto bfile = path{ dir_path };

        jfile /= jfilename.sv();
        bfile /= bfilename.sv();

        log(log_level::notice, [&](auto& m) noexcept {
            format(m, "project: using {} and {} to store observations",
                   jfile.sv(), bfile.sv());
        });

        auto j = file::open(jfile, file_mode(file_open_options::write));
        auto b = file::open(bfile, file_mode(file_open_options::write));

        if (j.has_value() and b.has_value())
            return irtb_files{ .json_file   = std::move(*j),
                               .binary_file = std::move(*b),
                               .json_path   = jfile,
                               .binary_path = bfile };

        log(log_level::notice, [&](auto& m) noexcept {
            format(
              m, "project: error opening file {} and {} to store observations",
              jfile.sv(), bfile.sv());
        });

        return make_error(simulation_errc::file_open_error);
    } catch (...) {
        return make_error(simulation_errc::memory_error);
    }
}

static constexpr std::string_view interpolate_type_names[] = { "none", "qss1",
                                                               "qss2", "qss3" };

static_assert(std::size(interpolate_type_names) ==
              ordinal(interpolate_type::qss3) + 1u);

/// The time step and the interpolation let the reader of the raw samples
/// build the curves of the simulation, see write_observations_csv().
static void print_created_json_irtb(const observers_type& observers,
                                    const observer_id     obs_id,
                                    const time            current_time,
                                    file&                 out) noexcept
{
    if (const auto* obs = observers.try_to_get(obs_id)) {
        const auto& r = observers.get<resampler>(obs_id);

        fmt::println(out.to_file(),
                     "{{\"model_id\": {}, \"name\": \"{}\", "
                     "\"created_at\":{}, \"dt\":{}, \"interpolation\": "
                     "\"{}\"}}",
                     ordinal(obs->model()),
                     observers.get<observer_name>(obs_id).sv(), current_time,
                     r.time_step(),
                     interpolate_type_names[ordinal(r.interpolation())]);
    }
}

static status init_json_irtb(const observers_type& observers,
                             const time            current_time,
                             file&                 out) noexcept
{
    debug::ensure(out.to_file() != nullptr);
    debug::ensure(out.get_mode()[file_open_options::write] == true);

    for (const auto& obs : observers)
        print_created_json_irtb(observers, observers.get_id(obs), current_time,
                                out);

    return success();
}

static status new_json_irtb(const observers_type& observers,
                            const observer_id     obs_id,
                            const time            current_time,
                            file&                 out) noexcept
{
    debug::ensure(out.to_file() != nullptr);
    debug::ensure(out.get_mode()[file_open_options::write] == true);
    debug::ensure(observers.exists(obs_id));

    print_created_json_irtb(observers, obs_id, current_time, out);

    return success();
}

static status free_json_irtb(const observers_type& observers,
                             const observer_id     obs_id,
                             const time            current_time,
                             file&                 out) noexcept
{
    debug::ensure(out.to_file() != nullptr);
    debug::ensure(out.get_mode()[file_open_options::write] == true);
    debug::ensure(observers.exists(obs_id));

    const auto& names = observers.get<observer_name>();

    if (const auto* obs = observers.try_to_get(obs_id)) {
        const auto mdl_id = obs->model();

        fmt::println(
          out.to_file(),
          "{{\"model_id\": {}, \"name\": \"{}\", \"destroyed_at\":{}}}",
          ordinal(mdl_id), names[obs_id].sv(), current_time);
    }

    return success();
}

static status finalize_json_irtb(const observers_type& observers,
                                 const time            current_time,
                                 file&                 out) noexcept
{
    debug::ensure(out.to_file() != nullptr);
    debug::ensure(out.get_mode()[file_open_options::write] == true);

    const auto& names = observers.get<observer_name>();

    for (const auto& obs : observers) {
        const auto obs_id = observers.get_id(obs);
        const auto mdl_id = obs.model();

        fmt::println(
          out.to_file(),
          "{{\"model_id\": {}, \"name\": \"{}\", \"destroyed_at\":{}}}",
          ordinal(mdl_id), names[obs_id].sv(), current_time);
    }

    return success();
}

void project::save_simulation_graph(
  const std::string_view absolute_path) noexcept
{
    try {
        auto p = path{ absolute_path };
        p /= "simulation-graph.dot";

        auto f = file::open(p, file_mode{ file_open_options::write });
        if (f.has_value())
            write_dot_graph_simulation(f->to_file(), sim);
    } catch (...) {
    }
}

status project::simulation_init_observation(const modeling& mod) noexcept
{
    for (auto& grid_obs : grid_observers)
        if (auto r = grid_obs.init(*this, mod); r.has_error())
            return r.error();

    for (auto& graph_obs : graph_observers)
        if (auto r = graph_obs.init(*this, mod); r.has_error())
            return r.error();

    for (auto& v_obs : variable_observers)
        if (auto r = v_obs.init(*this); r.has_error())
            return r.error();

    return success();
}

status project::simulation_copy(const modeling& mod) noexcept
{
    simulation_state = simulation_status::initializing;

    return mod.ids
      .read([&](const auto& ids, auto) -> status {
          return mod.files.read([&](const auto& fs, auto) -> status {
              auto guard = make_scope_exit(
                [&] { simulation_state = simulation_status::initialized; });

              return set(ids, fs, head())
                .and_then([&] { return sim.srcs.prepare(); })
                .and_then([&] { return sim.initialize(); });
          });
      })
      .or_else([&](const auto ec) noexcept {
          simulation_state = simulation_status::not_started;

          using namespace std::string_view_literals;

          switch (ec.cat()) {
          case category::project:
              log(log_level::error, "project: error in project copy"sv);
              break;
          case category::modeling:
              log(log_level::error, "project: error in modeling copy"sv);
              break;
          case category::simulation:
              log(log_level::error, "project: error in simulation copy"sv);
              break;
          default:
              log(log_level::error, "project: error in copy error"sv);
              break;
          }

          return status{ ec };
      });
}

status project::simulation_init(const modeling& mod) noexcept
{
    using namespace std::literals;

    bool state = any_equal(simulation_state, simulation_status::initialized,
                           simulation_status::not_started,
                           simulation_status::finished);

    debug::ensure(state);

    simulation_state = simulation_status::initializing;

    if (not tree_nodes.exists(tn_head())) {
        simulation_state = simulation_status::not_started;
        log(log_level::error, "project: the component is empty"sv);
        return make_error(project_errc::empty_project);
    }

    sim.clean();
    sim.observers.clear();

    if (auto r = simulation_init_observation(mod); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error, "project: observation system failed"sv);
        return r.error();
    }

    obs_files.json_path.clear();
    obs_files.bin_path.clear();

    if (flags[simulation_flag::write_irtb]) {
        const auto dir   = open_dir_name(mod, observation_dir);
        auto       files = open_irtb_file(name.sv(), dir, m_simulation_id,
                                          m_simulation_run++);

        if (files.has_error()) {
            simulation_state = simulation_status::not_started;
            return files.error();
        }

        obs_files.json_irtb = std::move(files->json_file);
        obs_files.bin_irtb  = std::move(files->binary_file);
        obs_files.json_path = files->json_path;
        obs_files.bin_path  = files->binary_path;
        obs_files.written.clear();

        if (const auto header = irb_header{};
            std::fwrite(&header, sizeof(header), 1,
                        obs_files.bin_irtb.to_file()) != 1) {
            simulation_state = simulation_status::not_started;
            return make_error(simulation_errc::file_write_error);
        }

        if (auto r = init_json_irtb(sim.observers, sim.current_time(),
                                    obs_files.json_irtb);
            r.has_error()) {
            simulation_state = simulation_status::not_started;
            return r.error();
        }
    }

    if (auto r = sim.srcs.prepare(); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error, "project: external source system failed"sv);
        return r.error();
    }

    if (auto r = sim.initialize(); r.has_error()) {
        simulation_state = simulation_status::not_started;
        log(log_level::error, "project: simulation system failed"sv);
        return r.error();
    }

    simulation_state = simulation_status::initialized;

    return success();
}

status project::simulation_new_model(const command::new_model_t& data) noexcept
{
    auto* tn = tree_nodes.try_to_get(data.tn_id);

    if (not tn) [[unlikely]] {
        log(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", ordinal(data.tn_id));
        });

        return make_error(project_errc::memory_error);
    }

    const auto tn_alloc  = tn->children.can_alloc(1) or
                           tn->children.grow<3, 2>(1);
    const auto sim_alloc = sim.can_alloc(1) or sim.grow_models<3, 2>();

    if (not tn_alloc or not sim_alloc) {
        log(log_level::error, [&](auto& m) noexcept {
            format(m,
                   "Fail to allocate new model in tree node {} (capacity: {}",
                   ordinal(data.tn_id), tn->children.capacity());
        });
        return make_error(project_errc::memory_error);
    }

    auto& mdl = sim.alloc(data.type);

    if (auto ret = sim.make_initialize(mdl, sim.current_time());
        ret.has_error()) {
        log(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to initialize new model of type {}",
                   dynamics_type_names[ordinal(data.type)]);
        });

        return ret.error();
    }

    tn->children.push_back(tree_node::child_node{
      .mdl = sim.get_id(mdl), .type = tree_node::child_node::type::model });

    if (flags[simulation_flag::write_irtb] and is_defined(mdl.obs_id)) {
        if (auto r = new_json_irtb(sim.observers, mdl.obs_id,
                                   sim.current_time(), obs_files.json_irtb);
            r.has_error()) {
            log(log_level::error, [&](auto& m) noexcept {
                format(m, "Fail to write new model of type {}",
                       dynamics_type_names[ordinal(data.type)]);
            });
            return r.error();
        }
    }

    return success();
}

status project::simulation_free_model(
  const command::free_model_t& data) noexcept
{
    auto* tn = tree_nodes.try_to_get(data.tn_id);
    if (not tn) [[unlikely]] {
        log(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find tree node with ID {}", ordinal(data.tn_id));
        });

        return make_error(project_errc::memory_error);
    }

    auto* mdl = sim.models.try_to_get(data.mdl_id);
    if (not mdl) [[unlikely]] {
        log(log_level::error, [&](auto& m) noexcept {
            format(m, "Fail to find model with ID {}", ordinal(data.mdl_id));
        });

        return make_error(project_errc::memory_error);
    }

    if (flags[simulation_flag::write_irtb] and is_defined(mdl->obs_id)) {
        if (auto r = free_json_irtb(sim.observers, mdl->obs_id,
                                    sim.current_time(), obs_files.json_irtb);
            r.has_error()) {
            log(log_level::error, [&](auto& m) noexcept {
                format(m, "Fail to write new model of type {}",
                       dynamics_type_names[ordinal(mdl->type)]);
            });
            return r.error();
        }
    }

    for (sz i = 0, e = tn->children.size(); i < e; ++i) {
        if (tn->children[i].type == tree_node::child_node::type::model and
            tn->children[i].mdl == data.mdl_id) {
            tn->children[i].disable();
            break;
        }
    }

    sim.deallocate(data.mdl_id);

    return success();
}

status project::simulation_copy_model(
  const command::copy_model_t& data) noexcept
{
    auto* src_mdl = sim.models.try_to_get(data.mdl_id);

    if (not src_mdl)
        return success();

    if (not sim.can_alloc(1) and not sim.grow_models<2, 1>()) {
        log(log_level::error, [](auto& m) noexcept {
            m = "Internal error: fail to allocate more models.";
        });

        return make_error(project_errc::memory_error);
    }

    auto& dst_mdl = sim.clone(*src_mdl);

    if (const auto r = sim.make_initialize(dst_mdl, sim.current_time());
        r.has_error()) {
        log(log_level::error, [r](auto& m) noexcept {
            format(m, "Internal error: fail to initialize new model: {})",
                   r.error());
        });

        return r.error();
    }

    dispatch(*src_mdl, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        if constexpr (has_output_port<Dynamics>) {
            for (int i = 0, e = length(dyn.y); i != e; ++i) {
                auto& dst_dyn = get_dyn<Dynamics>(dst_mdl);

                sim.for_each(dyn.y[i], [&](auto& mdl_src, int port_src) {
                    const auto mdl_src_id = sim.get_id(mdl_src);

                    (void)sim.connect(dst_dyn.y[i], mdl_src_id, port_src);
                });
            }
        }
    });

    if (flags[simulation_flag::write_irtb] and is_defined(dst_mdl.obs_id)) {
        if (auto r = new_json_irtb(sim.observers, dst_mdl.obs_id,
                                   sim.current_time(), obs_files.json_irtb);
            r.has_error()) {
            log(log_level::error, [&](auto& m) noexcept {
                format(m, "Fail to write new model of type {}",
                       dynamics_type_names[ordinal(dst_mdl.type)]);
            });
            return r.error();
        }
    }
    return success();
}

status project::simulation_new_connection(
  const command::new_connection_t& data) noexcept
{
    const auto can_connect = sim.can_connect(1) or sim.grow_connections<3, 2>();

    if (not can_connect) {
        log(log_level::error,
            [](auto& m) noexcept { m = "Fail to allocate new connection."; });

        return make_error(project_errc::memory_error);
    }

    auto* src = sim.models.try_to_get(data.mdl_src_id);
    auto* dst = sim.models.try_to_get(data.mdl_dst_id);

    if (not(src and dst))
        return success();

    const auto r = sim.connect(*src, data.port_src, *dst, data.port_dst);
    if (r.has_error()) {
        log(log_level::error, [r](auto& m) noexcept {
            format(m, "fail to allocate new connection ({}) ", r.error());
        });

        return r.error();
    }
    return success();
}

status project::simulation_free_connection(
  const command::free_connection_t& data) noexcept

{
    auto* src = sim.models.try_to_get(data.mdl_src_id);
    auto* dst = sim.models.try_to_get(data.mdl_dst_id);

    if (not(src and dst))
        return success();

    sim.disconnect(*src, data.port_src, *dst, data.port_dst);

    return success();
}

status project::simulation_new_observer(
  const command::new_observer_t& data) noexcept
{
    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
        if (sim.observers.exists(mdl->obs_id))
            return success();

        if (const auto ret = sim.observe(*mdl); ret.has_error()) {
            log(log_level::error, [&](auto& m) noexcept {
                format(m,
                       "Failed to allocate observer. (error: {} capacity:{})",
                       ret.error(), sim.observers.capacity());
            });

            return ret.error();
        }
    } else {
        log(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
        });
    }

    return success();
}

status project::simulation_free_observer(
  const command::free_observer_t& data) noexcept
{
    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
        sim.unobserve(*mdl);
    } else {
        log(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
        });
    }

    return success();
}

status project::simulation_send_message(
  const command::send_message_t& data) noexcept
{
    const auto t = irt::time_domain<time>::is_infinity(sim.current_time())
                     ? sim.last_time()
                     : sim.current_time();

    if (auto* mdl = sim.models.try_to_get(data.mdl_id)) {
        if (mdl->type == dynamics_type::constant) {
            if (mdl->handle == invalid_heap_handle) {
                sim.sched.alloc(*mdl, data.mdl_id, t);
            } else {
                if (sim.sched.is_in_tree(mdl->handle)) {
                    sim.sched.update(*mdl, t);
                } else {
                    sim.sched.reintegrate(*mdl, t);
                }
            }

            mdl->tn = t;
        } else {
            log(log_level::error, [&](auto& msg) noexcept {
                format(msg, "Model ID {} is not a constant model.",
                       ordinal(data.mdl_id));
            });
        }
    } else {
        log(log_level::error, [&](auto& msg) noexcept {
            format(msg, "Model ID {} not found.", ordinal(data.mdl_id));
        });
    }

    return success();
}

status project::simulation_apply_command() noexcept
{
    auto ret = success();

    commands.for_each([&](const command& cmd) noexcept {
        auto r = success();

        switch (cmd.type) {
        case command_type::none:
            break;
        case command_type::new_model:
            r = simulation_new_model(cmd.data.new_model);
            break;
        case command_type::free_model:
            r = simulation_free_model(cmd.data.free_model);
            break;
        case command_type::copy_model:
            r = simulation_copy_model(cmd.data.copy_model);
            break;
        case command_type::new_connection:
            r = simulation_new_connection(cmd.data.new_connection);
            break;
        case command_type::free_connection:
            r = simulation_free_connection(cmd.data.free_connection);
            break;
        case command_type::new_observer:
            r = simulation_new_observer(cmd.data.new_observer);
            break;
        case command_type::free_observer:
            r = simulation_free_observer(cmd.data.free_observer);
            break;
        case command_type::send_message:
            r = simulation_send_message(cmd.data.send_message);
            break;
        }

        if (r.has_error() and ret.has_error())
            ret = r;
    });

    commands.clear();

    return ret;
}

bool project::push(const command& cmd) noexcept
{
    using namespace std::literals;

    if (not commands.push(cmd)) {
        log(log_level::error,
            "simulation: fail to add command order in live simulation"sv);
        return false;
    }

    return true;
}

bool project::empty_commands() const noexcept { return commands.empty(); }

status project::observation_files::consume(
  const observation_batch& batch) noexcept
{
    if (not bin_irtb.is_open() or batch.raws.empty())
        return success();

    const auto idx = static_cast<std::size_t>(get_index(batch.id));

    if (idx >= written.size()) {
        const auto n = static_cast<int>(idx + 1u - written.size());

        if (not written.can_alloc(n) and not written.template grow<2, 1>(n))
            return make_error(simulation_errc::memory_error);

        while (written.size() <= idx)
            written.emplace_back();
    }

    auto& done = written[idx];
    if (done.id != batch.id)
        done = written_samples{ .id = batch.id, .count = 0 };

    // A replay after a rewind gives again samples already written.
    const auto first = batch.sampler.raw_output_index();
    const auto skip = std::min<u64>(done.count > first ? done.count - first : 0,
                                    batch.raws.size());
    const auto raws = batch.raws.subspan(static_cast<std::size_t>(skip));

    if (raws.empty())
        return success();

    const auto header = chunk_header{ .n_rows = raws.size(),
                                      .t_min  = raws.front().t,
                                      .t_max  = raws.back().t,
                                      .mdl_id = batch.obs.model() };

    auto* const out = bin_irtb.to_file();
    if (std::fwrite(&header, sizeof(header), 1, out) != 1 or
        std::fwrite(raws.data(), sizeof(raw_sample), raws.size(), out) !=
          raws.size()) {
        log(log_level::error,
            std::string_view("project: fail to write binary data"));
        return make_error(simulation_errc::file_write_error);
    }

    done.count = first + batch.raws.size();

    return success();
}

status project::flush_observations() noexcept
{
    if (obs_files.bin_irtb.is_open() and
        std::fflush(obs_files.bin_irtb.to_file()) != 0)
        return make_error(simulation_errc::file_write_error);

    return success();
}

status project::close_observations() noexcept
{
    if (not obs_files.bin_irtb.is_open() and not obs_files.json_irtb.is_open())
        return success();

    auto guard = make_scope_exit([&]() noexcept {
        obs_files.json_irtb.close();
        obs_files.bin_irtb.close();
    });

    irt_check(flush_observations());

    if (obs_files.json_irtb.is_open())
        return finalize_json_irtb(sim.observers, sim.current_time(),
                                  obs_files.json_irtb);

    return success();
}

/// A model of the manifest. Without time step, the raw samples are written.
struct manifest_model {
    name_str         name;
    real             dt            = zero;
    interpolate_type interpolation = interpolate_type::none;
};

using manifest_models = table<u64, manifest_model>;

static interpolate_type to_interpolate_type(const std::string_view str) noexcept
{
    for (std::size_t i = 0; i != std::size(interpolate_type_names); ++i)
        if (interpolate_type_names[i] == str)
            return enum_cast<interpolate_type>(i);

    return interpolate_type::none;
}

static manifest_models read_manifest(const path& manifest) noexcept
{
    manifest_models models;

    if (manifest.empty())
        return models;

    auto f = file::open(manifest, file_mode{ file_open_options::read });
    if (not f.has_value())
        return models;

    const auto content = f->read_entire_file();
    auto       text    = std::string_view(content.data(), content.size());

    while (not text.empty()) {
        const auto eol  = text.find('\n');
        const auto line = text.substr(0, eol);
        text            = eol == std::string_view::npos ? std::string_view{}
                                                        : text.substr(eol + 1);

        rapidjson::Document doc;
        doc.Parse(line.data(), line.size());

        if (doc.HasParseError() or not doc.IsObject() or
            not doc.HasMember("created_at"))
            continue;

        const auto id   = doc.FindMember("model_id");
        const auto name = doc.FindMember("name");

        if (id == doc.MemberEnd() or not id->value.IsUint64() or
            name == doc.MemberEnd() or not name->value.IsString())
            continue;

        manifest_model m;
        m.name = name_str(std::string_view(name->value.GetString(),
                                           name->value.GetStringLength()));

        const auto dt     = doc.FindMember("dt");
        const auto interp = doc.FindMember("interpolation");

        if (dt != doc.MemberEnd() and dt->value.IsNumber() and
            dt->value.GetDouble() > 0.0)
            m.dt = dt->value.GetDouble();

        if (interp != doc.MemberEnd() and interp->value.IsString())
            m.interpolation = to_interpolate_type(std::string_view(
              interp->value.GetString(), interp->value.GetStringLength()));

        models.data.emplace_back(id->value.GetUint64(), m);
    }

    models.sort();
    return models;
}

static bool write_csv_field(std::FILE* out, const std::string_view str) noexcept
{
    if (str.find_first_of(",\"\n\r") == std::string_view::npos)
        return std::fwrite(str.data(), 1, str.size(), out) == str.size();

    if (std::fputc('"', out) == EOF)
        return false;

    for (const auto c : str) {
        if (c == '"' and std::fputc('"', out) == EOF)
            return false;
        if (std::fputc(c, out) == EOF)
            return false;
    }

    return std::fputc('"', out) != EOF;
}

status write_observations_csv(const path& irb,
                              const path& manifest,
                              const path& csv) noexcept
{
    const auto models = read_manifest(manifest);

    // The chunks of the models are interleaved: one resampler for each model
    // keeps its series going from one chunk to the next.
    table<u64, resampler> samplers;
    for (const auto& m : models.data)
        if (m.value.dt > zero)
            samplers.data.emplace_back(
              m.id, resampler(m.value.dt, m.value.interpolation));
    samplers.sort();

    auto in = file::open(irb, file_mode{ file_open_options::read });
    if (not in.has_value())
        return in.error();

    auto* const fin = in->to_file();

    if (irb_header header, expected;
        std::fread(&header, sizeof(header), 1, fin) != 1 or
        header.magic != expected.magic or header.version != expected.version)
        return make_error(simulation_errc::range_error);

    auto out = file::open(csv, file_mode{ file_open_options::write });
    if (not out.has_value())
        return out.error();

    auto* const fout = out->to_file();

    if (std::fputs("t,model_id,name,value\n", fout) == EOF)
        return make_error(simulation_errc::file_write_error);

    const auto write_row = [&](const auto& sample, const u64 mdl,
                               const auto& name) noexcept -> bool {
        fmt::print(fout, "{},{},", sample.t, mdl);
        if (not write_csv_field(fout, name))
            return false;
        fmt::print(fout, ",{}\n", sample.value);
        return true;
    };

    const auto write_points = [&](resampler& r, const u64 mdl,
                                  std::string_view name) noexcept -> bool {
        for (const auto& p : r.output())
            if (not write_row(p, mdl, name))
                return false;

        r.clear_outputs();
        return true;
    };

    constexpr std::size_t block = 4096;
    vector<raw_sample>    samples(block);
    chunk_header          header;

    for (;;) {
        const auto read = std::fread(&header, 1, sizeof(header), fin);
        if (read == 0 and std::feof(fin))
            break;
        if (read != sizeof(header))
            return make_error(simulation_errc::file_eof_error);

        const auto  mdl  = ordinal(header.mdl_id);
        const auto* m    = models.get(mdl);
        const auto  name = m ? m->name.sv() : std::string_view{};
        auto*       r    = samplers.get(mdl);

        for (u64 left = header.n_rows; left > 0;) {
            const auto n = static_cast<std::size_t>(std::min<u64>(left, block));

            if (std::fread(samples.data(), sizeof(raw_sample), n, fin) != n)
                return make_error(simulation_errc::file_eof_error);

            if (r) {
                // A sample is final only when a later date is known: the last
                // one waits for the next chunk or the end of the file.
                r->tick(std::span(samples.data(), n), samples[n - 1].t);
                if (not write_points(*r, mdl, name))
                    return make_error(simulation_errc::file_write_error);
            } else {
                for (std::size_t i = 0; i != n; ++i)
                    if (not write_row(samples[i], mdl, name))
                        return make_error(simulation_errc::file_write_error);
            }

            left -= n;
        }
    }

    for (auto& s : samplers.data) {
        const auto* m = models.get(s.id);

        s.value.tick(std::span<const raw_sample>{},
                     time_domain<time>::infinity);
        if (not write_points(s.value, s.id, m ? m->name.sv() : ""))
            return make_error(simulation_errc::file_write_error);
    }

    if (not out->close())
        return make_error(simulation_errc::file_write_error);

    return success();
}

} // namespace irt
