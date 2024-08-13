// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/error.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>

#include <boost/ut.hpp>

#include <fmt/format.h>
#include <utility>

template<int length>
static bool get_temp_registred_path(irt::small_string<length>& str) noexcept
{
    std::error_code ec;

    try {
        auto p = std::filesystem::current_path(ec) / "reg-temp";
        str    = p.string();
        return true;
    } catch (...) {
        return false;
    }
}

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    using namespace boost::ut;

    "easy"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = c1.get_or_add_x("in");
        expect(
          !!s1.connect_input(p1_id, ch1, irt::connection::port{ .model = 0 }));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = c2.get_or_add_y("out");
        expect(
          !!s2.connect_output(p2_id, ch2, irt::connection::port{ .model = 0 }));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
        expect(!!s3.connect(mod,
                            ch31,
                            irt::connection::port{ .compo = p2_id },
                            ch32,
                            irt::connection::port{ .compo = p1_id }));

        expect(eq(s1.children.ssize(), 1));
        expect(eq(s2.children.ssize(), 1));
        expect(eq(s3.children.ssize(), 2));
        expect(eq(s1.connections.ssize(), 0));
        expect(eq(s2.connections.ssize(), 0));
        expect(eq(s3.connections.ssize(), 1));

        expect(!!pj.set(mod, sim, c3));
        expect(eq(pj.tree_nodes_size().first, 3));

        expect(eq(sim.models.ssize(), 2));
        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));
        expect(sim.can_connect(1));

        if (m1->type == irt::dynamics_type::counter) {
            expect(!sim.can_connect(*m2, 0, *m1, 0));
        } else {
            expect(!sim.can_connect(*m1, 0, *m2, 0));
        }
    };

    "no-connection"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c1 = mod.alloc_generic_component();
        auto& s1 = mod.generic_components.get(c1.id.generic_id);
        mod.alloc(s1, irt::dynamics_type::counter);

        auto& c2 = mod.alloc_generic_component();
        auto& s2 = mod.generic_components.get(c2.id.generic_id);
        mod.alloc(s2, irt::dynamics_type::time_func);

        auto& c3 = mod.alloc_generic_component();
        auto& s3 = mod.generic_components.get(c3.id.generic_id);
        mod.alloc(s3, mod.components.get_id(c2));
        mod.alloc(s3, mod.components.get_id(c1));

        expect(!!pj.set(mod, sim, c3));
        expect(eq(pj.tree_nodes_size().first, 3));

        expect(eq(sim.models.ssize(), 2));

        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));
    };

    "empty-component"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c1 = mod.alloc_generic_component();
        auto& s1 = mod.generic_components.get(c1.id.generic_id);
        mod.alloc(s1, irt::dynamics_type::counter);
        auto p1_id = c1.get_or_add_x("in");

        auto& c11    = mod.alloc_generic_component();
        auto& s11    = mod.generic_components.get(c11.id.generic_id);
        auto& ch11   = mod.alloc(s11, mod.components.get_id(c1));
        auto  p11_id = c11.get_or_add_x("in");

        expect(!!s11.connect_input(
          p11_id, ch11, irt::connection::port{ .compo = p1_id }));

        auto& c2 = mod.alloc_generic_component();
        auto& s2 = mod.generic_components.get(c2.id.generic_id);
        mod.alloc(s2, irt::dynamics_type::time_func);
        auto p2_id = c2.get_or_add_y("out");

        auto& c22    = mod.alloc_generic_component();
        auto& s22    = mod.generic_components.get(c22.id.generic_id);
        auto& ch22   = mod.alloc(s22, mod.components.get_id(c2));
        auto  p22_id = c22.get_or_add_y("out");

        expect(!!s22.connect_output(
          p22_id, ch22, irt::connection::port{ .compo = p2_id }));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c22));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c11));

        expect(!!s3.connect(mod,
                            ch31,
                            irt::connection::port{ .compo = p22_id },
                            ch32,
                            irt::connection::port{ .compo = p11_id }));

        expect(!!pj.set(mod, sim, c3));
        expect(eq(pj.tree_nodes_size().first, 5));

        expect(eq(sim.models.ssize(), 2));

        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));

        if (m1->type == irt::dynamics_type::counter) {
            expect(sim.can_connect(*m2, 0, *m1, 0));
        } else {
            expect(sim.can_connect(*m1, 0, *m2, 0));
        }
    };

    "graph-small-world"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_graph_component();
        auto& g  = mod.graph_components.get(cg.id.graph_id);
        g.resize(25, mod.components.get_id(c));
        g.param.small = irt::graph_component::small_world_param{};
        g.g_type      = irt::graph_component::graph_type::small_world;

        expect(!!pj.set(mod, sim, cg));
        expect(eq(pj.tree_nodes_size().first, g.children.ssize() + 1));
        expect(eq(sim.models.ssize(), g.children.ssize()));
    };

    "graph-scale-free"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_graph_component();
        auto& g  = mod.graph_components.get(cg.id.graph_id);
        g.resize(25, mod.components.get_id(c));
        g.param.scale = irt::graph_component::scale_free_param{};
        g.g_type      = irt::graph_component::graph_type::scale_free;

        expect(!!pj.set(mod, sim, cg));
        expect(eq(pj.tree_nodes_size().first, g.children.ssize() + 1));
        expect(eq(sim.models.ssize(), g.children.ssize()));
    };

    "grid-3x3-empty-con"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, mod.components.get_id(c));

        expect(!!pj.set(mod, sim, cg));
        expect(eq(pj.tree_nodes_size().first, g.row * g.column + 1));
        expect(eq(sim.models.ssize(), g.row * g.column));
    };

    "grid-3x3-empty-con-middle"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, irt::undefined<irt::component_id>());

        for (int i = 1; i < 4; ++i)
            for (int j = 1; j < 4; ++j)
                g.children[g.pos(i, j)] = mod.components.get_id(c);

        expect(!!pj.set(mod, sim, cg));
        expect(
          eq(pj.tree_nodes_size().first, (g.row - 2) * (g.column - 2) + 1));
        expect(eq(sim.models.ssize(), (g.row - 2) * (g.column - 2)));
    };

    "grid-3x3"_test = [] {
        irt::vector<char>       buffer;
        irt::registred_path_str temp_path;

        expect(get_temp_registred_path(temp_path) == true);

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim(1024 * 1024 * 8);

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto& c3   = mod.alloc_generic_component();
            auto& s3   = mod.generic_components.get(c3.id.generic_id);
            auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
            expect(!!s3.connect(mod,
                                ch31,
                                irt::connection::port{ .compo = p2_id },
                                ch32,
                                irt::connection::port{ .compo = p1_id }));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));

            auto& reg     = mod.alloc_registred("temp", 0);
            auto& dir     = mod.alloc_dir(reg);
            auto& file_c1 = mod.alloc_file(dir);
            auto& file_c2 = mod.alloc_file(dir);
            auto& file_c3 = mod.alloc_file(dir);
            auto& file_cg = mod.alloc_file(dir);
            reg.path      = temp_path;
            dir.path      = "test";

            mod.create_directories(reg);
            mod.create_directories(dir);

            file_c1.path = "c1.irt";
            file_c2.path = "c2.irt";
            file_c3.path = "c3.irt";
            file_cg.path = "cg.irt";

            const auto reg_id = mod.registred_paths.get_id(reg);
            const auto dir_id = mod.dir_paths.get_id(dir);

            expect(mod.registred_paths.try_to_get(reg_id) != nullptr);
            expect(mod.dir_paths.try_to_get(dir_id) != nullptr);

            c1.reg_path = reg_id;
            c1.dir      = dir_id;
            c1.file     = mod.file_paths.get_id(file_c1);

            c2.reg_path = reg_id;
            c2.dir      = dir_id;
            c2.file     = mod.file_paths.get_id(file_c2);

            c3.reg_path = reg_id;
            c3.dir      = dir_id;
            c3.file     = mod.file_paths.get_id(file_c3);

            cg.reg_path = reg_id;
            cg.dir      = dir_id;
            cg.file     = mod.file_paths.get_id(file_cg);

            expect(!!mod.save(c1));

            expect(!!mod.save(c2));

            expect(!!mod.save(c3));
            expect(!!mod.save(cg));

            expect(!!pj.set(mod, sim, cg));
            expect(eq(pj.tree_nodes_size().first, g.row * g.column * 3 + 1));

            expect(eq(sim.models.ssize(), g.row * g.column * 2));

            irt::json_archiver j;
            expect(
              j(pj,
                mod,
                sim,
                buffer,
                irt::json_archiver::print_option::indent_2_one_line_array));
        }

        expect(buffer.size() > 0u);
        fmt::print("Buffer: {:{}}\n", buffer.data(), buffer.size());

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim(1024 * 1024 * 8);

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));

            auto& reg = mod.alloc_registred("temp", 0);
            reg.path  = temp_path;

            mod.create_directories(reg);

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);

            irt::json_dearchiver j;
            expect(j(pj, mod, sim, std::span(buffer.data(), buffer.size())));
        }
    };

    "internal_component_io"_test = [] {
        irt::modeling_initializer mod_init;

        {
            irt::modeling mod;

            expect(!!mod.init(mod_init));
            irt::component_id ids[irt::internal_component_count];

            mod.registred_paths.reserve(8);
            mod.dir_paths.reserve(32);
            mod.file_paths.reserve(256);

            expect(mod.registred_paths.can_alloc(8));
            expect(mod.dir_paths.can_alloc(32));
            expect(mod.file_paths.can_alloc(256));

            expect(mod.components.can_alloc(irt::internal_component_count));

            auto& reg    = mod.alloc_registred("temp", 0);
            auto  reg_id = mod.registred_paths.get_id(reg);
            get_temp_registred_path(reg.path);
            mod.create_directories(reg);

            auto& dir    = mod.alloc_dir(reg);
            auto  dir_id = mod.dir_paths.get_id(dir);
            dir.path     = "dir-temp";
            mod.create_directories(dir);

            for (int i = 0, e = irt::internal_component_count; i != e; ++i) {
                auto& file     = mod.alloc_file(dir);
                auto  file_id  = mod.file_paths.get_id(file);
                file.component = ids[i];
                irt::format(
                  file.path, "{}.irt", irt::internal_component_names[i]);

                auto& c    = mod.components.alloc();
                auto  c_id = mod.components.get_id(c);
                c.reg_path = reg_id;
                c.dir      = dir_id;
                c.file     = file_id;

                expect(
                  !!mod.copy(irt::enum_cast<irt::internal_component>(i), c));
                ids[i] = c_id;
            }

            for (int i = 0, e = irt::internal_component_count; i != e; ++i) {
                auto* c = mod.components.try_to_get(ids[i]);
                expect(c != nullptr);
                expect(!!mod.save(*c));
            }

            expect(mod.components.ssize() == irt::internal_component_count);
        }

        {
            irt::modeling mod;

            expect(!!mod.init(mod_init));

            mod.registred_paths.reserve(8);
            mod.dir_paths.reserve(32);
            mod.file_paths.reserve(256);

            expect(mod.registred_paths.can_alloc(8));
            expect(mod.dir_paths.can_alloc(32));
            expect(mod.file_paths.can_alloc(256));

            expect(mod.components.can_alloc(irt::internal_component_count));

            auto& reg = mod.alloc_registred("temp", 0);
            get_temp_registred_path(reg.path);
            mod.create_directories(reg);

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);
            expect(mod.components.ssize() >= irt::internal_component_count);
        }
    };

    "grid-3x3-constant-model-init-port-all"_test = [] {
        irt::vector<char> buffer;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim(1024 * 1024 * 8);

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));

            auto& c1  = mod.alloc_generic_component();
            auto& s1  = mod.generic_components.get(c1.id.generic_id);
            auto& ch1 = mod.alloc(s1, irt::dynamics_type::counter);

            auto p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(c1));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
            auto  p31_id = c3.get_or_add_x("in");
            auto  p32_id = c3.get_or_add_y("out");

            const auto ch5_id  = s3.children.get_id(ch5);
            const auto ch5_idx = get_index(ch5_id);
            auto&      p       = s3.children_parameters[ch5_idx];
            p.reals[0]         = 0.0;
            p.reals[1]         = 0.0;
            p.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_all);
            p.integers[1] = 0;

            expect(!!s3.connect(mod,
                                ch3,
                                irt::connection::port{ .compo = p2_id },
                                ch4,
                                irt::connection::port{ .compo = p1_id }));
            expect(!!s3.connect_input(
              p31_id, ch4, irt::connection::port{ .compo = p1_id }));
            expect(!!s3.connect_output(
              p32_id, ch3, irt::connection::port{ .compo = p2_id }));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.opts                = irt::grid_component::options::none;
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::four;

            expect(!!pj.set(mod, sim, cg));
            expect(gt(g.cache_connections.ssize(), 0));
            expect(eq(pj.tree_nodes_size().first, g.row * g.column * 3 + 1));

            expect(eq(sim.models.ssize(), g.row * g.column * 3));

            int         nb_constant_model = 0;
            irt::model* cst_mdl           = nullptr;
            while (sim.models.next(cst_mdl)) {
                if (cst_mdl->type == irt::dynamics_type::constant) {
                    ++nb_constant_model;
                    auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                    expect(
                      eq(ordinal(dyn.type),
                         ordinal(
                           irt::constant::init_type::incoming_component_all)));
                    expect(eq(dyn.port, 0u));
                    expect(neq(dyn.default_value, 0.0));
                }
            }

            expect(eq(nb_constant_model, g.row * g.column));
        }
    };

    "grid-3x3-constant-model-init-port-n"_test = [] {
        irt::vector<char> buffer;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim(1024 * 1024 * 8);

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));

            auto& compo_counter = mod.alloc_generic_component();
            auto& gen_counter =
              mod.generic_components.get(compo_counter.id.generic_id);
            auto& child_counter =
              mod.alloc(gen_counter, irt::dynamics_type::counter);
            auto compo_counter_in = compo_counter.get_or_add_x("in");
            expect(
              !!gen_counter.connect_input(compo_counter_in,
                                          child_counter,
                                          irt::connection::port{ .model = 0 }));

            auto& compo_timef = mod.alloc_generic_component();
            auto& gen_timef =
              mod.generic_components.get(compo_timef.id.generic_id);
            auto& child_timef =
              mod.alloc(gen_timef, irt::dynamics_type::time_func);
            auto compo_timef_out = compo_timef.get_or_add_y("out");
            expect(
              !!gen_timef.connect_output(compo_timef_out,
                                         child_timef,
                                         irt::connection::port{ .model = 0 }));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(compo_timef));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(compo_counter));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
            auto  p31_id = c3.get_or_add_x("in");
            auto  p32_id = c3.get_or_add_y("out");

            // auto& mdl          = mod.models.get(ch5.id.mdl_id);
            // auto& dyn          = irt::get_dyn<irt::constant>(mdl);
            // dyn.default_offset = 0;
            // dyn.type           =
            // irt::constant::init_type::incoming_component_n; dyn.port =
            // ordinal(p31_id);

            const auto ch5_id = s3.children.get_id(ch5);
            auto&      p_ch5  = s3.children_parameters[get_index(ch5_id)];
            p_ch5.reals[0]    = 0.0;
            p_ch5.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_n);
            p_ch5.integers[1] = ordinal(p31_id);

            expect(
              !!s3.connect(mod,
                           ch3,
                           irt::connection::port{ .compo = compo_timef_out },
                           ch4,
                           irt::connection::port{ .compo = compo_counter_in }));

            expect(!!s3.connect_input(
              p31_id, ch4, irt::connection::port{ .compo = compo_counter_in }));
            expect(!!s3.connect_output(
              p32_id, ch3, irt::connection::port{ .compo = compo_timef_out }));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;

            expect(!!pj.set(mod, sim, cg));
            expect(eq(pj.tree_nodes_size().first, g.row * g.column * 3 + 1));

            expect(eq(sim.models.ssize(), g.row * g.column * 3));

            int         nb_constant_model = 0;
            irt::model* cst_mdl           = nullptr;
            while (sim.models.next(cst_mdl)) {
                if (cst_mdl->type == irt::dynamics_type::constant) {
                    ++nb_constant_model;
                    auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                    expect(neq(dyn.default_value, 0.0));
                    expect(eq(
                      ordinal(dyn.type),
                      ordinal(irt::constant::init_type::incoming_component_n)));
                }
            }

            expect(eq(nb_constant_model, g.row * g.column));
        }
    };

    "grid-3x3-constant-model-init-port-empty"_test = [] {
        auto old_error_callback =
          std::exchange(irt::on_error_callback, nullptr);

        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim(1024 * 1024 * 8);

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = c1.get_or_add_x("in");
        expect(
          !!s1.connect_input(p1_id, ch1, irt::connection::port{ .model = 0 }));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = c2.get_or_add_y("out");
        expect(
          !!s2.connect_output(p2_id, ch2, irt::connection::port{ .model = 0 }));

        auto& c3  = mod.alloc_generic_component();
        auto& s3  = mod.generic_components.get(c3.id.generic_id);
        auto& ch3 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch4 = mod.alloc(s3, mod.components.get_id(c1));
        auto& ch5 = mod.alloc(s3, irt::dynamics_type::constant);

        const auto ch5_id = s3.children.get_id(ch5);
        auto&      p_ch5  = s3.children_parameters[get_index(ch5_id)];
        p_ch5.reals[0]    = 0.0;
        p_ch5.integers[0] =
          ordinal(irt::constant::init_type::incoming_component_n);
        p_ch5.integers[1] = 17; // Impossible port

        expect(!!s3.connect(mod,
                            ch3,
                            irt::connection::port{ .compo = p2_id },
                            ch4,
                            irt::connection::port{ .compo = p1_id }));

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, mod.components.get_id(c3));

        expect(!pj.set(
          mod,
          sim,
          cg)); /* Fail to build the project since the constant
                   models can not be initialized with dyn.port equals to 17. */

        irt::on_error_callback = old_error_callback;
    };
}
