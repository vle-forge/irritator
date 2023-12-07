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
    irt::on_error_callback = irt::on_error_breakpoint;
#endif

    using namespace boost::ut;

    "easy"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = mod.get_or_add_x_index(c1, "in");
        auto* p1    = mod.ports.try_to_get(p1_id);
        expect((p1 != nullptr) >> fatal);
        expect(!!mod.connect_input(s1, *p1, ch1, 0));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = mod.get_or_add_y_index(c2, "out");
        auto* p2    = mod.ports.try_to_get(p2_id);
        expect((p2 != nullptr) >> fatal);
        expect(!!mod.connect_output(s2, ch2, 0, *p2));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
        expect(!!mod.connect(s3, ch31, p2_id, ch32, p1_id));

        expect(eq(mod.children.ssize(), 4));
        expect(eq(mod.connections.ssize(), 3));
        expect(eq(s1.connections.ssize(), 1));
        expect(eq(s2.connections.ssize(), 1));
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
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

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

        expect(eq(mod.children.ssize(), 4));
        expect(eq(mod.connections.ssize(), 0));

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
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = mod.get_or_add_x_index(c1, "in");
        auto* p1    = mod.ports.try_to_get(p1_id);
        expect(!!mod.connect_input(s1, *p1, ch1, 0));

        auto& c11    = mod.alloc_generic_component();
        auto& s11    = mod.generic_components.get(c11.id.generic_id);
        auto& ch11   = mod.alloc(s11, mod.components.get_id(c1));
        auto  p11_id = mod.get_or_add_x_index(c11, "in");
        auto* p11    = mod.ports.try_to_get(p11_id);

        expect(!!mod.connect_input(s11, *p11, ch11, p1_id));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = mod.get_or_add_y_index(c2, "out");
        auto* p2    = mod.ports.try_to_get(p2_id);

        expect(!!mod.connect_output(s2, ch2, 0, *p2));

        auto& c22    = mod.alloc_generic_component();
        auto& s22    = mod.generic_components.get(c22.id.generic_id);
        auto& ch22   = mod.alloc(s22, mod.components.get_id(c2));
        auto  p22_id = mod.get_or_add_y_index(c22, "out");
        auto* p22    = mod.ports.try_to_get(p22_id);

        expect(!!mod.connect_output(s22, ch22, p2_id, *p22));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c22));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c11));

        expect(!!mod.connect(s3, ch31, p22_id, ch32, p11_id));

        expect(eq(mod.children.ssize(), 6));
        expect(eq(mod.connections.ssize(), 5));

        expect(!!pj.set(mod, sim, c3));
        expect(eq(pj.tree_nodes_size().first, 5));

        expect(eq(sim.models.ssize(), 2));

        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));

        if (m1->type == irt::dynamics_type::counter) {
            expect(!sim.can_connect(*m2, 0, *m1, 0));
        } else {
            expect(!sim.can_connect(*m1, 0, *m2, 0));
        }
    };

    "graph-small-world"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

        auto& cg = mod.alloc_graph_component();
        auto& g  = mod.graph_components.get(cg.id.graph_id);
        g.resize(25, mod.components.get_id(c));
        g.param = irt::graph_component::small_world_param{};

        expect(!!pj.set(mod, sim, cg));
        expect(eq(pj.tree_nodes_size().first, g.children.ssize() + 1));
        expect(eq(sim.models.ssize(), g.children.ssize()));
    };

    "graph-scale-free"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

        auto& cg = mod.alloc_graph_component();
        auto& g  = mod.graph_components.get(cg.id.graph_id);
        g.resize(25, mod.components.get_id(c));
        g.param = irt::graph_component::scale_free_param{};

        expect(!!pj.set(mod, sim, cg));
        expect(eq(pj.tree_nodes_size().first, g.children.ssize() + 1));
        expect(eq(sim.models.ssize(), g.children.ssize()));
    };

    "grid-3x3-empty-con"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

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
        irt::simulation           sim;

        expect(!!mod.init(mod_init));
        expect(!!pj.init(mod_init));
        expect(!!sim.init(256, 4096));

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

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
        irt::cache_rw           cache;
        irt::registred_path_str temp_path;

        expect(get_temp_registred_path(temp_path) == true);

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));
            expect(!!sim.init(256, 4096));

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = mod.get_or_add_x_index(c1, "in");
            auto* p1    = mod.ports.try_to_get(p1_id);
            expect(p1 != nullptr);
            expect(!!mod.connect_input(s1, *p1, ch1, 0));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = mod.get_or_add_y_index(c2, "out");
            auto* p2    = mod.ports.try_to_get(p2_id);
            expect(p2 != nullptr);
            expect(!!mod.connect_output(s2, ch2, 0, *p2));

            auto& c3   = mod.alloc_generic_component();
            auto& s3   = mod.generic_components.get(c3.id.generic_id);
            auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
            expect(!!mod.connect(s3, ch31, p2_id, ch32, p1_id));

            expect(eq(mod.children.ssize(), 4));
            expect(eq(mod.connections.ssize(), 3));

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
            expect(!!j.project_save(
              pj,
              mod,
              sim,
              cache,
              buffer,
              irt::json_archiver::print_option::indent_2_one_line_array));
        }

        expect(buffer.size() > 0u);
        fmt::print("Buffer: {:{}}\n", buffer.data(), buffer.size());

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));
            expect(!!sim.init(256, 4096));

            auto& reg = mod.alloc_registred("temp", 0);
            reg.path  = temp_path;

            mod.create_directories(reg);

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);

            irt::json_archiver j;
            expect(!!j.project_load(
              pj, mod, sim, cache, std::span(buffer.data(), buffer.size())));
        }
    };

    "internal_component_io"_test = [] {
        irt::modeling_initializer mod_init;

        {
            irt::modeling mod;

            expect(!!mod.init(mod_init));
            irt::component_id ids[irt::internal_component_count];

            expect(!!mod.registred_paths.init(8));
            expect(!!mod.dir_paths.init(32));
            expect(!!mod.file_paths.init(256));

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

            expect(!!mod.registred_paths.init(8));
            expect(!!mod.dir_paths.init(32));
            expect(!!mod.file_paths.init(256));

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
        irt::cache_rw     cache;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));
            expect(!!sim.init(256, 4096));

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = mod.get_or_add_x_index(c1, "in");
            auto* p1    = mod.ports.try_to_get(p1_id);
            expect(p1 != nullptr);
            expect(!!mod.connect_input(s1, *p1, ch1, 0));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = mod.get_or_add_y_index(c2, "out");
            auto* p2    = mod.ports.try_to_get(p2_id);
            expect(p2 != nullptr);
            expect(!!mod.connect_output(s2, ch2, 0, *p2));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(c1));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
            auto  p31_id = mod.get_or_add_x_index(c3, "in");
            auto* p31    = mod.ports.try_to_get(p31_id);
            expect((p31 != nullptr) >> fatal);
            auto  p32_id = mod.get_or_add_y_index(c3, "out");
            auto* p32    = mod.ports.try_to_get(p32_id);
            expect((p32 != nullptr) >> fatal);

            const auto ch5_id  = mod.children.get_id(ch5);
            const auto ch5_idx = get_index(ch5_id);
            auto&      p       = mod.children_parameters[ch5_idx];
            p.reals[0]         = 0.0;
            p.reals[1]         = 0.0;
            p.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_all);
            p.integers[1] = 0;

            expect(!!mod.connect(s3, ch3, p2_id, ch4, p1_id));
            expect(!!mod.connect_input(s3, *p31, ch4, p1_id));
            expect(!!mod.connect_output(s3, ch3, p2_id, *p32));

            expect(eq(mod.children.ssize(), 5));
            expect(eq(mod.connections.ssize(), 5));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.opts            = irt::grid_component::options::none;
            g.connection_type = irt::grid_component::type::number;
            g.neighbors       = irt::grid_component::neighborhood::four;

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
        irt::cache_rw     cache;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));
            expect(!!sim.init(256, 4096));

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = mod.get_or_add_x_index(c1, "in");
            auto* p1    = mod.ports.try_to_get(p1_id);
            expect(p1 != nullptr);
            expect(!!mod.connect_input(s1, *p1, ch1, 0));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = mod.get_or_add_y_index(c2, "out");
            auto* p2    = mod.ports.try_to_get(p2_id);
            expect(p2 != nullptr);
            expect(!!mod.connect_output(s2, ch2, 0, *p2));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(c1));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
            auto  p31_id = mod.get_or_add_x_index(c3, "in");
            auto* p31    = mod.ports.try_to_get(p31_id);
            expect(p31 != nullptr);
            auto  p32_id = mod.get_or_add_y_index(c3, "out");
            auto* p32    = mod.ports.try_to_get(p32_id);
            expect(p32 != nullptr);
            // auto& mdl          = mod.models.get(ch5.id.mdl_id);
            // auto& dyn          = irt::get_dyn<irt::constant>(mdl);
            // dyn.default_offset = 0;
            // dyn.type           =
            // irt::constant::init_type::incoming_component_n; dyn.port =
            // ordinal(p31_id);

            const auto ch5_id = mod.children.get_id(ch5);
            auto&      p_ch5  = mod.children_parameters[get_index(ch5_id)];
            p_ch5.reals[0]    = 0.0;
            p_ch5.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_n);
            p_ch5.integers[1] = ordinal(p31_id);

            expect(!!mod.connect(s3, ch3, p2_id, ch4, p1_id));
            expect(!!mod.connect_input(s3, *p31, ch4, p1_id));
            expect(!!mod.connect_output(s3, ch3, p2_id, *p32));

            expect(eq(mod.children.ssize(), 5));
            expect(eq(mod.connections.ssize(), 5));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.connection_type = irt::grid_component::type::number;

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
        irt::vector<char> buffer;
        irt::cache_rw     cache;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            expect(!!mod.init(mod_init));
            expect(!!pj.init(mod_init));
            expect(!!sim.init(256, 4096));

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = mod.get_or_add_x_index(c1, "in");
            auto* p1    = mod.ports.try_to_get(p1_id);
            expect(p1 != nullptr);
            expect(!!mod.connect_input(s1, *p1, ch1, 0));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = mod.get_or_add_y_index(c2, "out");
            auto* p2    = mod.ports.try_to_get(p2_id);
            expect(p2 != nullptr);
            expect(!!mod.connect_output(s2, ch2, 0, *p2));

            auto& c3  = mod.alloc_generic_component();
            auto& s3  = mod.generic_components.get(c3.id.generic_id);
            auto& ch3 = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch4 = mod.alloc(s3, mod.components.get_id(c1));
            auto& ch5 = mod.alloc(s3, irt::dynamics_type::constant);
            // auto& mdl          = mod.models.get(ch5.id.mdl_id);
            // auto& dyn          = irt::get_dyn<irt::constant>(mdl);
            // dyn.default_offset = 0;
            // dyn.type           =
            // irt::constant::init_type::incoming_component_n; dyn.port = 17; //
            // Impossible port

            const auto ch5_id = mod.children.get_id(ch5);
            auto&      p_ch5  = mod.children_parameters[get_index(ch5_id)];
            p_ch5.reals[0]    = 0.0;
            p_ch5.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_n);
            p_ch5.integers[1] = 17; // Impossible port

            expect(!!mod.connect(s3, ch3, p2_id, ch4, p1_id));

            expect(eq(mod.children.ssize(), 5));
            expect(eq(mod.connections.ssize(), 3));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));

            expect(!!pj.set(mod, sim, cg));
            expect(eq(pj.tree_nodes_size().first, g.row * g.column * 3 + 1));

            expect(eq(sim.models.ssize(), g.row * g.column * 3));

            int         nb_constant_model = 0;
            irt::model* cst_mdl           = nullptr;
            while (sim.models.next(cst_mdl)) {
                if (cst_mdl->type == irt::dynamics_type::constant) {
                    ++nb_constant_model;
                    auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                    expect(eq(dyn.default_value, 0.0));
                }
            }

            expect(eq(nb_constant_model, g.row * g.column));
        }
    };
}
